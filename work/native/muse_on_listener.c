#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <mach/mach_time.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "muse_on_activation.h"
#include "muse_on_action_map.h"
#include "muse_on_capture.h"
#include "muse_on_config.h"
#include "muse_on_connection.h"
#include "muse_on_key_filter.h"
#include "muse_on_listener_lifecycle.h"
#include "muse_on_platform.h"
#include "muse_on_shortcut_map.h"
#include "muse_on_state_coordinator.h"

/*
 * Muse-On adapter for Codex.
 *
 * Dry-run is the default and never changes device mappings or posts an event.
 * Capture modes seize the Muse-On joystick interface and apply an exact
 * Muse-On keyboard UserKeyMapping. This avoids macOS's privileged keyboard
 * seizure gate while keeping pointer input exclusive. Actions route only while
 * Codex is frontmost. Disconnecting or exiting restores the keyboard mapping.
 */

enum {
  kMuseOnVendorID = 0x04b4,
  kMuseOnProductID = 0xe106,
};

static const CFTimeInterval kActionFlushIntervalSeconds = 0.01;
static const uint64_t kFilterRetryIntervalNs = 1000000000ULL;
static const uint64_t kPermissionCheckIntervalNs = 1000000000ULL;

typedef enum {
  kInterfaceUnknown = 0,
  kInterfaceKeyboard,
  kInterfaceJoystick,
} InterfaceKind;

typedef struct ListenerState ListenerState;

typedef struct DeviceSlot {
  IOHIDDeviceRef device;
  uint32_t locationID;
  uint32_t usagePage;
  uint32_t usage;
  uint64_t registryID;
  InterfaceKind kind;
  CFIndex reportCapacity;
  bool removed;
  struct DeviceSlot *next;
} DeviceSlot;

typedef struct {
  ListenerState *state;
  InterfaceKind kind;
} ManagerContext;

struct ListenerState {
  DeviceSlot *slots;
  CFRunLoopRef runLoop;
  IOHIDManagerRef joystickManager;
  IOHIDManagerRef keyboardManager;
  ManagerContext joystickContext;
  ManagerContext keyboardContext;
  MuseOnConfig config;
  MuseOnKeyFilter *keyFilter;
  bool joystickOpen;
  bool keyboardOpen;
  bool keyFilterApplied;
  bool syntheticHoldDown;
  MuseOnListenerLifecycle lifecycle;
  bool inputMonitoringAccess;
  bool postEventAccess;
  bool permissionsKnown;
  bool releaseFailed;
  MuseOnSafetyFailure safetyFailure;
  bool safetyLatchEmitted;
  bool recoveryStateEmitted;
  bool recoveryErrorObserved;
  bool topologyInitialSettled;
  bool topologyClassificationFailed;
  MuseOnRecoveryPolicy recoveryPolicy;
  MuseOnKeyFilterRestorePolicy filterRestorePolicy;
  uint64_t filterRetryAfterNs;
  uint64_t permissionCheckAfterNs;
};

static volatile sig_atomic_t stopRequested = 0;

static uint64_t monotonic_ns(void) {
  static mach_timebase_info_data_t timebase;
  uint64_t ticks = mach_continuous_time();

  if (timebase.denom == 0) mach_timebase_info(&timebase);
  return ticks * timebase.numer / timebase.denom;
}

static const char *boolean_string(bool value) {
  return value ? "true" : "false";
}

static void emit_safety_latch(ListenerState *state,
                              MuseOnSafetyFailure failure) {
  if (!state) return;
  if (failure == MUSE_ON_SAFETY_FAILURE_NONE) {
    failure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
  }
  if (state->safetyFailure == MUSE_ON_SAFETY_FAILURE_NONE) {
    state->safetyFailure = failure;
  }
  if (state->safetyLatchEmitted) return;
  state->safetyLatchEmitted = true;
  printf("{\"event\":\"safety_latch\",\"reason\":\"%s\"}\n",
         muse_on_safety_failure_string(state->safetyFailure));
}

static void emit_permission_required(ListenerState *state, const char *gate) {
  if (!state) return;
  printf("{\"event\":\"topology\",\"state\":\"unknown\","
         "\"locationID\":0}\n");
  if (gate && strcmp(gate, "input_monitoring") == 0) {
    state->inputMonitoringAccess = false;
  } else if (gate && strcmp(gate, "accessibility") == 0) {
    state->postEventAccess = false;
  }
  printf("{\"event\":\"permission_required\","
         "\"reason\":\"permission_required\",\"gate\":\"%s\","
         "\"inputMonitoring\":%s,\"accessibility\":%s,"
         "\"dispatchAllowed\":false}\n",
         gate ? gate : "unknown", boolean_string(state->inputMonitoringAccess),
         boolean_string(state->postEventAccess));
}

static void emit_stopped(const ListenerState *state) {
  printf("{\"event\":\"stopped\",\"releaseFailed\":%s,"
         "\"safetyReason\":\"%s\"}\n",
         boolean_string(state && state->releaseFailed),
         muse_on_safety_failure_string(
             state ? state->safetyFailure : MUSE_ON_SAFETY_FAILURE_NONE));
}

static void emit_topology_snapshot(MuseOnConnectionSnapshot snapshot) {
  printf("{\"event\":\"topology\",\"state\":\"%s\","
         "\"locationID\":%u}\n",
         muse_on_connection_state_string(snapshot.state),
         snapshot.location_id);
}

static void emit_current_topology(const ListenerState *state);
static void fail_topology_classification(ListenerState *state);

static const char *interface_name(InterfaceKind kind) {
  switch (kind) {
    case kInterfaceKeyboard: return "keyboard";
    case kInterfaceJoystick: return "joystick";
    default: return "unknown";
  }
}

static const char *access_name(IOHIDAccessType access) {
  switch (access) {
    case kIOHIDAccessTypeGranted: return "granted";
    case kIOHIDAccessTypeDenied: return "denied";
    case kIOHIDAccessTypeUnknown: return "unknown";
    default: return "unknown";
  }
}

static bool filter_permissions_ready(const ListenerState *state) {
  return state->inputMonitoringAccess &&
         (state->config.mode != MUSE_ON_MODE_ACTIVE ||
          state->postEventAccess);
}

static void refresh_permission_state(ListenerState *state, uint64_t nowNs) {
  bool inputMonitoringAccess;
  bool postEventAccess;
  bool changed;

  if (nowNs < state->permissionCheckAfterNs) return;
  inputMonitoringAccess =
      IOHIDCheckAccess(kIOHIDRequestTypeListenEvent) ==
      kIOHIDAccessTypeGranted;
  postEventAccess = state->config.mode == MUSE_ON_MODE_ACTIVE
      ? muse_on_preflight_post_event_access()
      : state->postEventAccess;
  changed = !state->permissionsKnown ||
            inputMonitoringAccess != state->inputMonitoringAccess ||
            postEventAccess != state->postEventAccess;
  state->inputMonitoringAccess = inputMonitoringAccess;
  state->postEventAccess = postEventAccess;
  state->permissionsKnown = true;
  state->permissionCheckAfterNs = nowNs + kPermissionCheckIntervalNs;
  if (changed) {
    printf("{\"event\":\"permissions_changed\","
           "\"inputMonitoring\":%s,\"accessibility\":%s}\n",
           boolean_string(state->inputMonitoringAccess),
           boolean_string(state->postEventAccess));
    if (!filter_permissions_ready(state)) {
      emit_topology_snapshot(
          (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
    } else {
      emit_current_topology(state);
    }
  }
}

static void emit_error(ListenerState *state, const char *operation,
                       IOReturn code) {
  if (state && state->config.safety_latched) {
    state->recoveryErrorObserved = true;
  }
  printf("{\"event\":\"error\",\"operation\":\"%s\",\"code\":%d}\n",
         operation, (int)code);
}

static void fail_topology_classification(ListenerState *state) {
  if (!state || state->topologyClassificationFailed) return;
  state->topologyClassificationFailed = true;
  (void)muse_on_listener_lifecycle_unbind(&state->lifecycle);
  emit_error(state, "classify_topology", kIOReturnNoMemory);
  emit_topology_snapshot(
      (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
  emit_safety_latch(state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
  stopRequested = 1;
}

static void emit_capture_state(const ListenerState *state, const char *event,
                               const char *reason) {
  printf("{\"event\":\"%s\",\"reason\":\"%s\","
         "\"keyboardOpen\":%s,\"keyFilterApplied\":%s}\n",
         event, reason, boolean_string(state->keyboardOpen),
         boolean_string(state->keyFilterApplied));
}

static bool number_property(IOHIDDeviceRef device, CFStringRef key,
                            uint32_t *value) {
  CFTypeRef property = IOHIDDeviceGetProperty(device, key);
  int64_t raw = 0;

  if (!property || CFGetTypeID(property) != CFNumberGetTypeID()) return false;
  if (!CFNumberGetValue((CFNumberRef)property, kCFNumberSInt64Type, &raw) ||
      raw < 0 || (uint64_t)raw > UINT32_MAX) {
    return false;
  }
  *value = (uint32_t)raw;
  return true;
}

static bool device_registry_id(IOHIDDeviceRef device, uint64_t *registryID) {
  io_service_t service;

  if (!device || !registryID) return false;
  service = IOHIDDeviceGetService(device);
  return service != IO_OBJECT_NULL &&
         IORegistryEntryGetRegistryEntryID(service, registryID) ==
             KERN_SUCCESS &&
         *registryID != 0;
}

static InterfaceKind classify_interface(uint32_t usagePage, uint32_t usage) {
  if (usagePage != kHIDPage_GenericDesktop) return kInterfaceUnknown;
  if (usage == kHIDUsage_GD_Keyboard) return kInterfaceKeyboard;
  if (usage == kHIDUsage_GD_Joystick) return kInterfaceJoystick;
  return kInterfaceUnknown;
}

static MuseOnInterfaceKind decoder_interface_kind(InterfaceKind kind) {
  switch (kind) {
    case kInterfaceKeyboard: return MUSE_ON_INTERFACE_KEYBOARD_BOOT;
    case kInterfaceJoystick: return MUSE_ON_INTERFACE_JOYSTICK;
    default: return 0;
  }
}

static DeviceSlot *find_slot(ListenerState *state, IOHIDDeviceRef device) {
  DeviceSlot *slot;

  for (slot = state->slots; slot; slot = slot->next) {
    if (slot->device == device) return slot;
  }
  return NULL;
}

static bool current_connection_snapshot(
    const ListenerState *state, MuseOnConnectionSnapshot *snapshot) {
  const DeviceSlot *slot;
  MuseOnObservedInterface *interfaces;
  size_t count = 0;
  size_t index = 0;

  if (!state || !snapshot) return false;
  for (slot = state->slots; slot; slot = slot->next) {
    if (!slot->removed && (slot->kind == kInterfaceKeyboard ||
                           slot->kind == kInterfaceJoystick)) {
      count++;
    }
  }
  if (count == 0) {
    *snapshot = (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
    return true;
  }
  interfaces = calloc(count, sizeof(*interfaces));
  if (!interfaces) return false;
  for (slot = state->slots; slot; slot = slot->next) {
    if (slot->removed) continue;
    if (slot->kind == kInterfaceKeyboard) {
      interfaces[index++] = (MuseOnObservedInterface){
          MUSE_ON_OBSERVED_KEYBOARD, slot->locationID, true};
    } else if (slot->kind == kInterfaceJoystick) {
      interfaces[index++] = (MuseOnObservedInterface){
          MUSE_ON_OBSERVED_JOYSTICK, slot->locationID, true};
    }
  }
  *snapshot = muse_on_classify_connections(interfaces, index);
  free(interfaces);
  return true;
}

static void emit_current_topology(const ListenerState *state) {
  MuseOnConnectionSnapshot snapshot;

  if (!state || !state->topologyInitialSettled) return;
  if (!current_connection_snapshot(state, &snapshot)) {
    fail_topology_classification((ListenerState *)state);
    return;
  }
  emit_topology_snapshot(snapshot);
}

static bool validated_controller_location(const ListenerState *state,
                                          uint32_t *locationID) {
  MuseOnConnectionSnapshot snapshot;

  if (!current_connection_snapshot(state, &snapshot)) {
    fail_topology_classification((ListenerState *)state);
    return false;
  }
  if (!locationID || snapshot.state != MUSE_ON_CONNECTION_SINGLE) return false;
  *locationID = snapshot.location_id;
  return true;
}

static bool read_keyboard_current_report(void *context, uint8_t reportID,
                                         uint8_t *report,
                                         size_t *reportLength) {
  DeviceSlot *slot = context;
  CFIndex length;
  IOReturn result;

  if (!slot || !slot->device || !report || !reportLength ||
      *reportLength == 0 || *reportLength > (size_t)LONG_MAX) {
    return false;
  }
  length = (CFIndex)*reportLength;
  result = IOHIDDeviceGetReport(slot->device, kIOHIDReportTypeInput,
                                (CFIndex)reportID, report, &length);
  if (result != kIOReturnSuccess || length <= 0) return false;
  *reportLength = (size_t)length;
  return true;
}

typedef struct {
  IOHIDDeviceRef device;
  CFArrayRef elements;
  MuseOnProfile profile;
} JoystickElementReaderContext;

static bool is_input_element(IOHIDElementType type) {
  return type == kIOHIDElementTypeInput_Misc ||
         type == kIOHIDElementTypeInput_Button ||
         type == kIOHIDElementTypeInput_Axis ||
         type == kIOHIDElementTypeInput_ScanCodes;
}

static bool joystick_element_for_usage(uint32_t usagePage, uint32_t usage,
                                       MuseOnProfile profile,
                                       MuseOnJoystickElement *element) {
  if (!element) return false;
  if (usagePage == kHIDPage_GenericDesktop &&
      usage == kHIDUsage_GD_Hatswitch) {
    *element = MUSE_ON_JOYSTICK_ELEMENT_HAT;
    return true;
  }
  if (usagePage != kHIDPage_Button || usage < 1 || usage > 5 ||
      (profile != MUSE_ON_PROFILE_PEDAL && usage == 5)) {
    return false;
  }
  *element = (MuseOnJoystickElement)usage;
  return true;
}

static bool read_joystick_current_element(
    void *context, MuseOnJoystickElement requested,
    MuseOnJoystickElementValue *snapshot) {
  JoystickElementReaderContext *reader = context;
  CFIndex count;
  CFIndex index;
  IOHIDElementRef selected = NULL;
  IOHIDValueRef value = NULL;

  if (!reader || !snapshot) return false;
  if (!reader->device || !reader->elements) return false;

  count = CFArrayGetCount(reader->elements);
  for (index = 0; index < count; index++) {
    IOHIDElementRef candidate =
        (IOHIDElementRef)CFArrayGetValueAtIndex(reader->elements, index);
    MuseOnJoystickElement element;

    if (candidate && joystick_element_for_usage(
                         IOHIDElementGetUsagePage(candidate),
                         IOHIDElementGetUsage(candidate),
                         reader->profile, &element)) {
      if (element != requested) continue;
      if (selected) return false;
      selected = candidate;
    }
  }
  if (!selected ||
      IOHIDDeviceGetValue(reader->device, selected, &value) != kIOReturnSuccess ||
      !value || IOHIDValueGetElement(value) != selected) {
    return false;
  }
  *snapshot = (MuseOnJoystickElementValue){
      .element = requested,
      .is_absolute_input = is_input_element(IOHIDElementGetType(selected)) &&
                           !IOHIDElementIsRelative(selected),
      .report_id = IOHIDElementGetReportID(selected),
      .usage_page = IOHIDElementGetUsagePage(selected),
      .usage = IOHIDElementGetUsage(selected),
      .logical_minimum = IOHIDElementGetLogicalMin(selected),
      .logical_maximum = IOHIDElementGetLogicalMax(selected),
      .value = IOHIDValueGetIntegerValue(value),
  };
  return true;
}

static const DeviceSlot *active_keyboard_slot_at_location(
    const ListenerState *state, uint32_t locationID);
static const DeviceSlot *active_joystick_slot_at_location(
    const ListenerState *state, uint32_t locationID);

static bool active_filter_matches_current_keyboard(
    const ListenerState *state, uint32_t controllerLocation) {
  const DeviceSlot *keyboardSlot;
  uint64_t filterLocation;
  uint64_t filterRegistryID;

  if (!state || !state->keyFilter) return false;
  keyboardSlot =
      active_keyboard_slot_at_location(state, controllerLocation);
  filterLocation = muse_on_key_filter_location_id(state->keyFilter);
  filterRegistryID = muse_on_key_filter_registry_id(state->keyFilter);
  return keyboardSlot && filterLocation == controllerLocation &&
         filterRegistryID != 0 &&
         keyboardSlot->registryID == filterRegistryID;
}

static void emit_recovery_state(ListenerState *state) {
  MuseOnConnectionSnapshot topology;
  uint32_t controllerLocation = 0;
  bool controllerConnected;
  bool filterVerified;
  bool inputsReleased;
  MuseOnRecoveryDecision decision;
  MuseOnRecoveryOutcome outcome;
  MuseOnRecoveryObservation observation;

  if (!state || state->recoveryStateEmitted) return;
  if (!current_connection_snapshot(state, &topology)) {
    fail_topology_classification(state);
    return;
  }
  controllerConnected = topology.state == MUSE_ON_CONNECTION_SINGLE &&
                        validated_controller_location(state, &controllerLocation);
  inputsReleased =
      controllerConnected &&
      muse_on_listener_lifecycle_inputs_released(&state->lifecycle);
  filterVerified = muse_on_capture_is_verified(
      state->joystickOpen,
      controllerConnected && state->keyFilterApplied &&
          muse_on_key_filter_is_active(state->keyFilter) &&
          active_filter_matches_current_keyboard(state, controllerLocation),
      controllerConnected);
  observation = (MuseOnRecoveryObservation){
      .permission_granted = filter_permissions_ready(state),
      .controller_connected = controllerConnected,
      .multiple_controllers = topology.state == MUSE_ON_CONNECTION_MULTIPLE,
      .codex_foreground =
          muse_on_listener_lifecycle_is_foreground(&state->lifecycle),
      .inputs_released = inputsReleased,
      .filter_verified = filterVerified,
      .keyboard_open = state->keyboardOpen,
      .error_observed = state->recoveryErrorObserved,
  };
  decision = muse_on_recovery_policy_evaluate(
      &state->recoveryPolicy, monotonic_ns(), observation);
  outcome = muse_on_recovery_outcome_for(decision, observation);
  /* HID enumeration and filter proof may settle over several timer ticks.
   * The safety latch already makes external_route_gate() return false. */
  if (outcome == MUSE_ON_RECOVERY_OUTCOME_WAIT ||
      decision == MUSE_ON_RECOVERY_DONE) {
    return;
  }
  printf("{\"event\":\"recovery_state\","
         "\"recoveryOutcome\":\"%s\","
         "\"recoveryFailure\":\"%s\","
         "\"permissionGranted\":%s,\"controllerConnected\":%s,"
         "\"multipleControllers\":%s,\"codexForeground\":%s,"
         "\"inputsReleased\":%s,\"filterVerified\":%s,"
         "\"keyboardOpen\":%s,\"topology\":\"%s\","
         "\"locationID\":%u}\n",
         muse_on_recovery_outcome_string(outcome),
         muse_on_safety_failure_string(
             outcome == MUSE_ON_RECOVERY_OUTCOME_FAILURE
                 ? MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN
                 : MUSE_ON_SAFETY_FAILURE_NONE),
         boolean_string(observation.permission_granted),
         boolean_string(observation.controller_connected),
         boolean_string(observation.multiple_controllers),
         boolean_string(
             muse_on_listener_lifecycle_is_foreground(&state->lifecycle)),
         boolean_string(inputsReleased), boolean_string(filterVerified),
         boolean_string(state->keyboardOpen),
         muse_on_connection_state_string(topology.state), topology.location_id);
  state->recoveryStateEmitted = true;
  stopRequested = 1;
}

static bool slot_matches_active_filter(const ListenerState *state,
                                       const DeviceSlot *slot) {
  uint64_t locationID;
  uint64_t registryID;
  uint32_t controllerLocation;
  const DeviceSlot *keyboardSlot;

  if (!slot) return false;
  if (state->config.mode == MUSE_ON_MODE_DRY_RUN) return true;
  if (!validated_controller_location(state, &controllerLocation) ||
      slot->locationID != controllerLocation) return false;
  locationID = muse_on_key_filter_location_id(state->keyFilter);
  registryID = muse_on_key_filter_registry_id(state->keyFilter);
  keyboardSlot = active_keyboard_slot_at_location(state, controllerLocation);
  return locationID != 0 && locationID <= UINT32_MAX &&
         registryID != 0 && keyboardSlot &&
         keyboardSlot->registryID == registryID &&
         slot->locationID == (uint32_t)locationID &&
         (slot->kind != kInterfaceKeyboard || slot->registryID == registryID);
}

static void emit_device_event(const char *event, const DeviceSlot *slot) {
  printf("{\"event\":\"%s\",\"interface\":\"%s\","
         "\"usagePage\":%u,\"usage\":%u,\"locationID\":%u,"
         "\"maxInputReportSize\":%ld}\n",
         event, interface_name(slot->kind), slot->usagePage, slot->usage,
         slot->locationID, (long)slot->reportCapacity);
}

static void emit_action(const char *event, const ListenerState *state,
                        const DeviceSlot *slot,
                        const MuseOnActionEvent *action) {
  printf("{\"event\":\"%s\",\"mode\":\"%s\",\"profile\":\"%s\","
         "\"action\":\"%s\",\"phase\":\"%s\",\"source\":\"%s\","
         "\"interface\":\"%s\"}\n",
         event, muse_on_mode_string(state->config.mode),
         muse_on_profile_string(state->config.profile),
         muse_on_action_id_string(action->id),
         muse_on_action_phase_string(action->phase),
         muse_on_event_name_string(action->source),
         slot ? interface_name(slot->kind) : "safety");
}

static bool force_release_synthetic_hold(ListenerState *state,
                                         const char *reason) {
  MuseOnActionEvent releaseAction = {
      MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
      MUSE_ON_ACTION_END,
      MUSE_ON_EVENT_NONE,
  };
  MuseOnShortcutInstruction shortcut;
  bool posted;

  if (!state->syntheticHoldDown) return true;
  posted = muse_on_shortcut_map(&releaseAction, &shortcut) &&
           muse_on_post_shortcut(&shortcut);
  printf("{\"event\":\"forced_hold_release\",\"reason\":\"%s\","
         "\"posted\":%s}\n",
         reason, boolean_string(posted));
  if (posted) {
    state->syntheticHoldDown = false;
  } else {
    state->releaseFailed = true;
    emit_safety_latch(state, MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE);
  }
  return posted;
}

static void handle_action(ListenerState *state, const DeviceSlot *slot,
                          const MuseOnActionEvent *action) {
  bool frontmost;
  bool captured;
  bool postAccess;
  MuseOnShortcutInstruction shortcut;
  bool posted;

  if (state->config.mode == MUSE_ON_MODE_ACTIVE &&
      !filter_permissions_ready(state)) {
    return;
  }

  frontmost = state->config.mode == MUSE_ON_MODE_DRY_RUN
      ? false
      : muse_on_codex_is_frontmost();
  captured = state->joystickOpen && state->keyFilterApplied &&
             muse_on_key_filter_is_active(state->keyFilter) &&
             active_filter_matches_current_keyboard(
                 state, slot ? slot->locationID : 0) &&
             slot_matches_active_filter(state, slot);
  if (!muse_on_can_route_actions(state->config.mode, frontmost, captured)) {
    return;
  }

  if (state->config.mode != MUSE_ON_MODE_ACTIVE) {
    emit_action(state->config.mode == MUSE_ON_MODE_CAPTURE_DRY_RUN
                    ? "capture_dry_run_action"
                    : "dry_run_action",
                state, slot, action);
    return;
  }

  postAccess = muse_on_preflight_post_event_access();
  state->postEventAccess = postAccess;
  if (!muse_on_can_post_actions(state->config.mode, frontmost, captured,
                                postAccess)) {
    emit_action("action_blocked", state, slot, action);
    if (action->id == MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD &&
        action->phase == MUSE_ON_ACTION_END &&
        state->syntheticHoldDown) {
      state->releaseFailed = true;
      emit_safety_latch(state, MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE);
      stopRequested = 1;
    }
    return;
  }
  if (!muse_on_shortcut_map(action, &shortcut)) {
    emit_action("shortcut_map_failed", state, slot, action);
    return;
  }
  if (action->id == MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD) {
    if (action->phase == MUSE_ON_ACTION_BEGIN && state->syntheticHoldDown) {
      emit_action("duplicate_hold_ignored", state, slot, action);
      return;
    }
    if (action->phase == MUSE_ON_ACTION_END && !state->syntheticHoldDown) {
      emit_action("orphan_hold_release_ignored", state, slot, action);
      return;
    }
  }

  posted = muse_on_post_shortcut(&shortcut);
  emit_action(posted ? "action_dispatched" : "action_dispatch_failed",
              state, slot, action);
  if (!posted) {
    if (action->id == MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD &&
        action->phase == MUSE_ON_ACTION_END &&
        state->syntheticHoldDown) {
      state->releaseFailed = true;
      emit_safety_latch(state, MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE);
      stopRequested = 1;
    }
    return;
  }
  if (action->id == MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD) {
    state->syntheticHoldDown =
        action->phase == MUSE_ON_ACTION_BEGIN;
  }
}

static void lifecycle_event(
    void *context, const MuseOnListenerLifecycleEvent *event) {
  ListenerState *state = context;
  const DeviceSlot *slot = NULL;

  if (!state || !event) return;
  if (event->type == MUSE_ON_LISTENER_LIFECYCLE_EVENT_FOCUS_CHANGED) {
    printf("{\"event\":\"focus_changed\",\"codexFrontmost\":%s}\n",
           boolean_string(event->foreground));
    return;
  }
  if (event->type == MUSE_ON_LISTENER_LIFECYCLE_EVENT_NEUTRAL_ENTRY) {
    printf("{\"event\":\"neutral_entry\",\"inputsReleased\":%s}\n",
           boolean_string(event->inputs_released));
    return;
  }
  if (event->type != MUSE_ON_LISTENER_LIFECYCLE_EVENT_ACTION) return;
  if (event->interface_kind == MUSE_ON_INTERFACE_KEYBOARD_BOOT) {
    slot = active_keyboard_slot_at_location(
        state, event->controller_location_id);
  } else if (event->interface_kind == MUSE_ON_INTERFACE_JOYSTICK) {
    slot = active_joystick_slot_at_location(
        state, event->controller_location_id);
  }
  if (slot) handle_action(state, slot, &event->action);
}

static bool external_route_gate(ListenerState *state) {
  bool frontmost;
  bool captured;
  uint32_t controllerLocation;

  if (state->config.mode == MUSE_ON_MODE_DRY_RUN) return true;
  if (state->config.safety_latched) return false;
  if (state->config.mode == MUSE_ON_MODE_ACTIVE &&
      !filter_permissions_ready(state)) return false;
  if (!validated_controller_location(state, &controllerLocation)) return false;
  frontmost = muse_on_codex_is_frontmost();
  captured = state->joystickOpen && state->keyFilterApplied &&
             muse_on_key_filter_is_active(state->keyFilter) &&
             active_filter_matches_current_keyboard(state,
                                                    controllerLocation);
  return muse_on_can_route_actions(state->config.mode, frontmost, captured);
}

static void report_received(void *context, IOReturn result, void *sender,
                            IOHIDReportType type, uint32_t reportID,
                            uint8_t *report, CFIndex reportLength,
                            uint64_t timeStamp) {
  ManagerContext *managerContext = context;
  ListenerState *state;
  DeviceSlot *slot;
  MuseOnInterfaceKind interfaceKind;
  uint64_t receivedNs;

  if (!managerContext || !managerContext->state || !sender) return;
  state = managerContext->state;
  slot = find_slot(state, (IOHIDDeviceRef)sender);
  if (!slot || slot->removed || slot->kind != managerContext->kind) return;
  if (!slot_matches_active_filter(state, slot)) return;
  if (result != kIOReturnSuccess) {
    emit_error(state, "input_report", result);
    emit_topology_snapshot(
        (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
    emit_safety_latch(state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
    stopRequested = 1;
    return;
  }
  if (type != kIOHIDReportTypeInput || !report || reportLength < 0 ||
      reportLength > slot->reportCapacity) {
    emit_error(state, "invalid_input_report", kIOReturnBadArgument);
    emit_topology_snapshot(
        (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
    emit_safety_latch(state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
    stopRequested = 1;
    return;
  }

  printf("{\"event\":\"report\",\"interface\":\"%s\","
         "\"usagePage\":%u,\"usage\":%u,\"locationID\":%u,"
         "\"timestamp\":%llu,\"reportID\":%u,\"reportLength\":%ld,"
         "\"bytesHex\":\"",
         interface_name(slot->kind), slot->usagePage, slot->usage,
         slot->locationID, (unsigned long long)timeStamp, reportID,
         (long)reportLength);
  for (CFIndex byteIndex = 0; byteIndex < reportLength; byteIndex++) {
    printf("%02x", report[byteIndex]);
  }
  puts("\"}");

  interfaceKind = decoder_interface_kind(slot->kind);
  receivedNs = monotonic_ns();
  (void)muse_on_listener_lifecycle_observe_report(
      &state->lifecycle, interfaceKind, (uint8_t)reportID, report,
      (size_t)reportLength, receivedNs, external_route_gate(state));
}

static const DeviceSlot *active_keyboard_slot_at_location(
    const ListenerState *state, uint32_t locationID) {
  const DeviceSlot *slot;

  for (slot = state->slots; slot; slot = slot->next) {
    if (!slot->removed && slot->kind == kInterfaceKeyboard &&
        slot->locationID == locationID) return slot;
  }
  return NULL;
}

static const DeviceSlot *active_joystick_slot_at_location(
    const ListenerState *state, uint32_t locationID) {
  const DeviceSlot *slot;

  for (slot = state->slots; slot; slot = slot->next) {
    if (!slot->removed && slot->kind == kInterfaceJoystick &&
        slot->locationID == locationID) return slot;
  }
  return NULL;
}

static void seed_lifecycle_current_reports(
    ListenerState *state, const DeviceSlot *keyboardSlot,
    const DeviceSlot *joystickSlot) {
  uint8_t joystickReport[11];
  uint8_t *keyboardReport;
  size_t keyboardReportLength;
  JoystickElementReaderContext reader;

  if (!state || !keyboardSlot || !joystickSlot ||
      keyboardSlot->reportCapacity <= 0) {
    return;
  }
  keyboardReport =
      calloc((size_t)keyboardSlot->reportCapacity, sizeof(*keyboardReport));
  if (keyboardReport) {
    keyboardReportLength = (size_t)keyboardSlot->reportCapacity;
    if (read_keyboard_current_report(
            (void *)keyboardSlot, 0, keyboardReport, &keyboardReportLength)) {
      (void)muse_on_listener_lifecycle_observe_report(
          &state->lifecycle, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
          keyboardReport, keyboardReportLength, monotonic_ns(), false);
    }
    free(keyboardReport);
  }
  reader = (JoystickElementReaderContext){
      .device = joystickSlot->device,
      .elements = IOHIDDeviceCopyMatchingElements(
          joystickSlot->device, NULL, kIOHIDOptionsTypeNone),
      .profile = state->config.profile,
  };
  if (reader.elements &&
      muse_on_capture_read_joystick_snapshot(
          state->config.profile, read_joystick_current_element, &reader,
          joystickReport)) {
    (void)muse_on_listener_lifecycle_observe_report(
        &state->lifecycle, MUSE_ON_INTERFACE_JOYSTICK, 1, joystickReport,
        sizeof(joystickReport), monotonic_ns(), false);
  }
  if (reader.elements) CFRelease(reader.elements);
}

static void sync_lifecycle_binding(ListenerState *state) {
  const DeviceSlot *keyboardSlot;
  const DeviceSlot *joystickSlot;
  uint32_t controllerLocation;
  bool alreadyBound;

  if (!state ||
      !validated_controller_location(state, &controllerLocation)) {
    if (state) {
      (void)muse_on_listener_lifecycle_unbind(&state->lifecycle);
    }
    return;
  }
  keyboardSlot =
      active_keyboard_slot_at_location(state, controllerLocation);
  joystickSlot =
      active_joystick_slot_at_location(state, controllerLocation);
  if (!keyboardSlot || !joystickSlot ||
      (state->config.mode != MUSE_ON_MODE_DRY_RUN &&
       (!state->keyFilterApplied ||
        !muse_on_key_filter_is_active(state->keyFilter) ||
        !active_filter_matches_current_keyboard(
            state, controllerLocation)))) {
    (void)muse_on_listener_lifecycle_unbind(&state->lifecycle);
    return;
  }
  alreadyBound = muse_on_listener_lifecycle_is_bound_to(
      &state->lifecycle, state->config.profile, controllerLocation);
  if (!muse_on_listener_lifecycle_bind(
          &state->lifecycle, state->config.profile, controllerLocation,
          true, true) ||
      alreadyBound) {
    return;
  }
  seed_lifecycle_current_reports(state, keyboardSlot, joystickSlot);
}

static void activate_keyboard_filter(ListenerState *state, uint64_t nowNs,
                                     const char *reason);
static bool restore_keyboard_filter(ListenerState *state, uint64_t nowNs,
                                    const char *reason, bool finalAttempt);

static void device_added(void *context, IOReturn result, void *sender,
                         IOHIDDeviceRef device) {
  ManagerContext *managerContext = context;
  ListenerState *state;
  DeviceSlot *slot;
  uint32_t vendorID = 0;
  uint32_t productID = 0;
  uint32_t usagePage = 0;
  uint32_t usage = 0;
  uint32_t locationID = 0;
  uint32_t maxInputReportSize = 0;
  uint64_t registryID = 0;
  InterfaceKind kind;

  (void)sender;
  if (result != kIOReturnSuccess || !managerContext ||
      !managerContext->state || !device) {
    emit_error(managerContext ? managerContext->state : NULL,
               "device_added", result);
    if (managerContext && managerContext->state) {
      emit_topology_snapshot(
          (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
      emit_safety_latch(managerContext->state,
                        MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
      stopRequested = 1;
    }
    return;
  }
  state = managerContext->state;
  if (!number_property(device, CFSTR(kIOHIDVendorIDKey), &vendorID) ||
      !number_property(device, CFSTR(kIOHIDProductIDKey), &productID) ||
      !number_property(device, CFSTR(kIOHIDPrimaryUsagePageKey), &usagePage) ||
      !number_property(device, CFSTR(kIOHIDPrimaryUsageKey), &usage) ||
      !number_property(device, CFSTR(kIOHIDLocationIDKey), &locationID) ||
      !number_property(device, CFSTR(kIOHIDMaxInputReportSizeKey),
                       &maxInputReportSize) ||
      !device_registry_id(device, &registryID)) {
    emit_error(state, "device_properties", kIOReturnBadArgument);
    emit_topology_snapshot(
        (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
    emit_safety_latch(state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
    stopRequested = 1;
    return;
  }
  kind = classify_interface(usagePage, usage);
  if (vendorID != kMuseOnVendorID || productID != kMuseOnProductID ||
      kind != managerContext->kind || maxInputReportSize == 0) {
    return;
  }

  slot = find_slot(state, device);
  if (slot) {
    if (!slot->removed) return;
    slot->removed = false;
    slot->locationID = locationID;
    slot->registryID = registryID;
    slot->reportCapacity = (CFIndex)maxInputReportSize;
  } else {
    slot = calloc(1, sizeof(*slot));
    if (!slot) {
      emit_error(state, "allocate_slot", kIOReturnNoMemory);
      return;
    }
    slot->device = (IOHIDDeviceRef)CFRetain(device);
    slot->locationID = locationID;
    slot->registryID = registryID;
    slot->usagePage = usagePage;
    slot->usage = usage;
    slot->kind = kind;
    slot->reportCapacity = (CFIndex)maxInputReportSize;
    slot->next = state->slots;
    state->slots = slot;
  }

  emit_device_event("device_added", slot);
  emit_current_topology(state);
  if (state->config.mode != MUSE_ON_MODE_DRY_RUN) {
    uint32_t controllerLocation;
    if (state->keyFilterApplied &&
        !validated_controller_location(state, &controllerLocation)) {
      restore_keyboard_filter(state, monotonic_ns(),
                              "controller_topology_changed", false);
    } else if (!state->keyFilterApplied) {
      activate_keyboard_filter(state, monotonic_ns(), "controller_connected");
    }
  }
  sync_lifecycle_binding(state);
}

static void device_removed(void *context, IOReturn result, void *sender,
                           IOHIDDeviceRef device) {
  ManagerContext *managerContext = context;
  ListenerState *state;
  DeviceSlot *slot;

  (void)sender;
  if (result != kIOReturnSuccess || !managerContext ||
      !managerContext->state || !device) {
    emit_error(managerContext ? managerContext->state : NULL,
               "device_removed", result);
    if (managerContext && managerContext->state) {
      emit_topology_snapshot(
          (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
      emit_safety_latch(managerContext->state,
                        MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
      stopRequested = 1;
    }
    return;
  }
  state = managerContext->state;
  slot = find_slot(state, device);
  if (!slot || slot->removed || slot->kind != managerContext->kind) return;

  slot->removed = true;
  sync_lifecycle_binding(state);
  emit_device_event("device_removed", slot);
  emit_current_topology(state);
  if (state->config.mode != MUSE_ON_MODE_DRY_RUN && state->keyFilterApplied) {
    uint32_t controllerLocation;
    if (!validated_controller_location(state, &controllerLocation)) {
      restore_keyboard_filter(state, monotonic_ns(), "controller_disconnected",
                              false);
      return;
    }
  }
  if (slot->kind == kInterfaceKeyboard &&
      slot->locationID == muse_on_key_filter_location_id(state->keyFilter) &&
      state->config.mode != MUSE_ON_MODE_DRY_RUN) {
    restore_keyboard_filter(state, monotonic_ns(), "keyboard_disconnected",
                            false);
  } else if (state->syntheticHoldDown) {
    force_release_synthetic_hold(state, "device_removed");
  }
}

static void stop_signal(int signalNumber) {
  (void)signalNumber;
  stopRequested = 1;
}

static bool restore_keyboard_filter(ListenerState *state, uint64_t nowNs,
                                    const char *reason, bool finalAttempt) {
  MuseOnKeyFilterRestoreDecision decision;
  bool firstAttempt;
  bool keyboardPresent;
  bool restoreNeeded;
  bool restored;
  uint64_t filterLocation;

  firstAttempt = !state->filterRestorePolicy.waiting &&
                 state->filterRetryAfterNs == 0;
  state->keyFilterApplied = false;
  if (firstAttempt) {
    force_release_synthetic_hold(state, reason);
    (void)muse_on_listener_lifecycle_unbind(&state->lifecycle);
    state->filterRetryAfterNs = 0;
  }
  restoreNeeded = muse_on_key_filter_needs_restore(state->keyFilter);
  if (!restoreNeeded) {
    muse_on_key_filter_restore_policy_init(&state->filterRestorePolicy);
    state->filterRetryAfterNs = 0;
    return true;
  }
  filterLocation = muse_on_key_filter_location_id(state->keyFilter);
  keyboardPresent =
      filterLocation != 0 && filterLocation <= UINT32_MAX &&
      active_keyboard_slot_at_location(state, (uint32_t)filterLocation) != NULL;
  if (!finalAttempt && state->filterRestorePolicy.waiting &&
      keyboardPresent &&
      nowNs < state->filterRetryAfterNs) {
    return false;
  }

  restored = muse_on_key_filter_restore(state->keyFilter);
  decision = muse_on_key_filter_restore_policy_evaluate(
      &state->filterRestorePolicy, nowNs, restored, keyboardPresent,
      finalAttempt);
  if (decision == MUSE_ON_KEY_FILTER_RESTORE_COMPLETE) {
    state->filterRetryAfterNs = 0;
    emit_capture_state(state, "filter_restored", reason);
    return true;
  }
  if (decision == MUSE_ON_KEY_FILTER_RESTORE_RETRY) {
    state->filterRetryAfterNs = nowNs + kFilterRetryIntervalNs;
    if (firstAttempt) {
      emit_capture_state(state, "filter_waiting", "restore_settling");
    }
    return false;
  }

  emit_error(state, "restore_keyboard_filter", kIOReturnError);
  state->releaseFailed = true;
  emit_safety_latch(state, MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE);
  stopRequested = 1;
  return false;
}

static void activate_keyboard_filter(ListenerState *state,
                                     uint64_t nowNs,
                                     const char *reason) {
  const DeviceSlot *keyboardSlot;
  uint32_t controllerLocation;

  if (state->config.mode == MUSE_ON_MODE_DRY_RUN ||
      state->keyFilterApplied || muse_on_key_filter_is_active(state->keyFilter)) {
    return;
  }
  if (muse_on_key_filter_needs_restore(state->keyFilter)) {
    if (!restore_keyboard_filter(state, nowNs, "recover_keyboard_filter",
                                 false)) {
      return;
    }
  }
  if (!validated_controller_location(state, &controllerLocation)) {
    emit_capture_state(state, "filter_waiting", "controller_incomplete");
    state->filterRetryAfterNs = nowNs + kFilterRetryIntervalNs;
    return;
  }
  keyboardSlot = active_keyboard_slot_at_location(state, controllerLocation);
  if (!keyboardSlot) {
    emit_capture_state(state, "filter_waiting", "keyboard_unavailable");
    state->filterRetryAfterNs = nowNs + kFilterRetryIntervalNs;
    return;
  }
  if (!muse_on_key_filter_apply(state->keyFilter, keyboardSlot->locationID,
                                keyboardSlot->registryID)) {
    emit_error(state, "apply_keyboard_filter", kIOReturnError);
    if (muse_on_key_filter_needs_restore(state->keyFilter) &&
        !restore_keyboard_filter(state, nowNs, "rollback_keyboard_filter",
                                 false)) {
      return;
    }
    state->filterRetryAfterNs = nowNs + kFilterRetryIntervalNs;
    return;
  }
  state->keyFilterApplied = muse_on_key_filter_is_active(state->keyFilter);
  state->filterRetryAfterNs = 0;
  sync_lifecycle_binding(state);
  emit_capture_state(state, state->keyFilterApplied ? "filter_applied"
                                                    : "filter_waiting",
                     reason);
}

static void update_focus_and_filter(ListenerState *state, uint64_t nowNs) {
  bool frontmost;
  bool wantFilter;
  uint32_t controllerLocation;

  if (state->config.mode == MUSE_ON_MODE_DRY_RUN) return;
  refresh_permission_state(state, nowNs);
  frontmost = muse_on_codex_is_frontmost();
  if (muse_on_listener_lifecycle_focus_changed(
          &state->lifecycle, frontmost) &&
      !frontmost) {
    force_release_synthetic_hold(state, "codex_focus_lost");
  }

  wantFilter = muse_on_should_filter_keyboard(state->config.mode) &&
               filter_permissions_ready(state) &&
               validated_controller_location(state, &controllerLocation);
  if (!wantFilter && (state->keyFilterApplied ||
                      muse_on_key_filter_needs_restore(state->keyFilter)) &&
      nowNs >= state->filterRetryAfterNs) {
    restore_keyboard_filter(state, nowNs, "filter_not_requested", false);
  } else if (wantFilter && !state->keyFilterApplied &&
             nowNs >= state->filterRetryAfterNs) {
    activate_keyboard_filter(state, nowNs, "session_filter");
  }
}

static void action_timer(CFRunLoopTimerRef timer, void *context) {
  ListenerState *state = context;
  uint64_t nowNs;

  (void)timer;
  if (!state) return;
  nowNs = monotonic_ns();
  update_focus_and_filter(state, nowNs);
  if (state->config.safety_latched) emit_recovery_state(state);

  muse_on_listener_lifecycle_tick(
      &state->lifecycle, nowNs, external_route_gate(state));
  if ((stopRequested || state->releaseFailed) && state->runLoop) {
    CFRunLoopStop(state->runLoop);
  }
}

static void cleanup_slots(ListenerState *state) {
  DeviceSlot *slot = state->slots;

  while (slot) {
    DeviceSlot *next = slot->next;
    CFRelease(slot->device);
    free(slot);
    slot = next;
  }
  state->slots = NULL;
}

static CFDictionaryRef create_muse_on_match(InterfaceKind kind) {
  int vendorID = kMuseOnVendorID;
  int productID = kMuseOnProductID;
  int usagePage = kHIDPage_GenericDesktop;
  int usage;
  CFNumberRef vendor;
  CFNumberRef product;
  CFNumberRef page;
  CFNumberRef primaryUsage;
  const void *keys[] = {
      CFSTR(kIOHIDVendorIDKey),
      CFSTR(kIOHIDProductIDKey),
      CFSTR(kIOHIDPrimaryUsagePageKey),
      CFSTR(kIOHIDPrimaryUsageKey),
  };
  const void *values[4];
  CFDictionaryRef match;

  if (kind == kInterfaceKeyboard) {
    usage = kHIDUsage_GD_Keyboard;
  } else if (kind == kInterfaceJoystick) {
    usage = kHIDUsage_GD_Joystick;
  } else {
    return NULL;
  }
  vendor = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendorID);
  product = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &productID);
  page = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usagePage);
  primaryUsage =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
  if (!vendor || !product || !page || !primaryUsage) {
    if (vendor) CFRelease(vendor);
    if (product) CFRelease(product);
    if (page) CFRelease(page);
    if (primaryUsage) CFRelease(primaryUsage);
    return NULL;
  }

  values[0] = vendor;
  values[1] = product;
  values[2] = page;
  values[3] = primaryUsage;
  match = CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 4,
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFRelease(vendor);
  CFRelease(product);
  CFRelease(page);
  CFRelease(primaryUsage);
  return match;
}

static IOHIDManagerRef create_manager(ListenerState *state,
                                      ManagerContext *managerContext) {
  IOHIDManagerRef manager;
  CFDictionaryRef match;

  manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDManagerOptionNone);
  match = create_muse_on_match(managerContext->kind);
  if (!manager || !match) {
    if (manager) CFRelease(manager);
    if (match) CFRelease(match);
    return NULL;
  }

  IOHIDManagerSetDeviceMatching(manager, match);
  IOHIDManagerRegisterDeviceMatchingCallback(
      manager, device_added, managerContext);
  IOHIDManagerRegisterDeviceRemovalCallback(
      manager, device_removed, managerContext);
  IOHIDManagerRegisterInputReportWithTimeStampCallback(
      manager, report_received, managerContext);
  IOHIDManagerScheduleWithRunLoop(
      manager, state->runLoop, kCFRunLoopDefaultMode);
  CFRelease(match);
  return manager;
}

static void unschedule_and_release_manager(ListenerState *state,
                                           IOHIDManagerRef manager) {
  if (!manager) return;
  IOHIDManagerUnscheduleFromRunLoop(
      manager, state->runLoop, kCFRunLoopDefaultMode);
  CFRelease(manager);
}

int main(int argc, char *argv[]) {
  ListenerState state = {0};
  IOHIDAccessType inputAccess;
  IOReturn opened;
  CFRunLoopTimerContext timerContext;
  CFRunLoopTimerRef timer;
  int exitCode = 0;

  if (!muse_on_config_parse(argc, (const char *const *)argv,
                            &state.config)) {
    fprintf(stderr,
            "usage: %s "
            "[--profile=controller-only|--profile=pedal] "
            "[--mode=dry-run|--mode=capture-dry-run|--mode=active] "
            "[--safety-latched]\n",
            argv[0]);
    return 64;
  }
  muse_on_listener_lifecycle_init(&state.lifecycle, lifecycle_event, &state);
  muse_on_key_filter_restore_policy_init(&state.filterRestorePolicy);

  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  signal(SIGINT, stop_signal);
  signal(SIGTERM, stop_signal);
  emit_topology_snapshot(
      (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});

  inputAccess = IOHIDCheckAccess(kIOHIDRequestTypeListenEvent);
  if (state.config.request_permissions &&
      inputAccess != kIOHIDAccessTypeGranted) {
    (void)IOHIDRequestAccess(kIOHIDRequestTypeListenEvent);
    inputAccess = IOHIDCheckAccess(kIOHIDRequestTypeListenEvent);
  }
  if (inputAccess != kIOHIDAccessTypeGranted) {
    printf("{\"event\":\"warning\",\"operation\":\"input_monitoring\","
           "\"action\":\"grant_in_settings_then_retry\"}\n");
  }
  state.inputMonitoringAccess = inputAccess == kIOHIDAccessTypeGranted;

  if (state.config.mode == MUSE_ON_MODE_ACTIVE) {
    state.postEventAccess = muse_on_preflight_post_event_access();
    if (state.config.request_permissions && !state.postEventAccess) {
      (void)muse_on_request_post_event_access();
      state.postEventAccess = muse_on_preflight_post_event_access();
    }
  }
  state.permissionsKnown = true;

  printf("{\"event\":\"tcc_status\",\"inputMonitoring\":\"%s\","
         "\"accessibility\":%s}\n",
         access_name(inputAccess), boolean_string(state.postEventAccess));

  if (state.config.mode == MUSE_ON_MODE_ACTIVE &&
      !filter_permissions_ready(&state)) {
    emit_permission_required(&state,
                             !state.inputMonitoringAccess
                                 ? "input_monitoring"
                                 : "accessibility");
    emit_stopped(&state);
    return 0;
  }

  state.runLoop = CFRunLoopGetCurrent();
  state.joystickContext.state = &state;
  state.joystickContext.kind = kInterfaceJoystick;
  state.keyboardContext.state = &state;
  state.keyboardContext.kind = kInterfaceKeyboard;
  state.joystickManager = create_manager(&state, &state.joystickContext);
  state.keyboardManager = create_manager(&state, &state.keyboardContext);
  state.keyFilter = muse_on_key_filter_create();
  if (!state.joystickManager || !state.keyboardManager || !state.keyFilter) {
    emit_error(&state, "create_hid_managers", kIOReturnNoMemory);
    emit_topology_snapshot(
        (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
    emit_safety_latch(&state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
    muse_on_key_filter_destroy(state.keyFilter);
    unschedule_and_release_manager(&state, state.keyboardManager);
    unschedule_and_release_manager(&state, state.joystickManager);
    emit_stopped(&state);
    return 1;
  }

  opened = IOHIDManagerOpen(
      state.joystickManager,
      (IOOptionBits)muse_on_joystick_open_options(
          state.config.mode != MUSE_ON_MODE_DRY_RUN));
  if (opened != kIOReturnSuccess) {
    emit_topology_snapshot(
        (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
    if (muse_on_listener_error_is_permission_required(
            "open_joystick_manager", opened)) {
      emit_permission_required(&state, "input_monitoring");
      goto shutdown;
    }
    emit_error(&state, "open_joystick_manager", opened);
    muse_on_key_filter_destroy(state.keyFilter);
    unschedule_and_release_manager(&state, state.keyboardManager);
    unschedule_and_release_manager(&state, state.joystickManager);
    return 2;
  }
  state.joystickOpen = true;

  opened = IOHIDManagerOpen(
      state.keyboardManager,
      (IOOptionBits)muse_on_keyboard_capture_options());
  if (opened != kIOReturnSuccess) {
    emit_topology_snapshot(
        (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
    if (muse_on_listener_error_is_permission_required(
            "open_keyboard_manager", opened)) {
      emit_permission_required(&state, "input_monitoring");
      goto shutdown;
    }
    emit_error(&state, "open_keyboard_manager", opened);
    exitCode = 2;
    goto shutdown;
  }
  state.keyboardOpen = true;
  state.topologyInitialSettled = true;
  emit_current_topology(&state);
  if (state.config.mode != MUSE_ON_MODE_DRY_RUN) {
    update_focus_and_filter(&state, monotonic_ns());
  }
  if (state.config.safety_latched) {
    muse_on_recovery_policy_init(&state.recoveryPolicy, monotonic_ns());
  }

  timerContext = (CFRunLoopTimerContext){0, &state, NULL, NULL, NULL};
  timer = CFRunLoopTimerCreate(
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent() + kActionFlushIntervalSeconds,
      kActionFlushIntervalSeconds, 0, 0, action_timer, &timerContext);
  if (!timer) {
    emit_error(&state, "create_action_timer", kIOReturnNoMemory);
    exitCode = 1;
  } else {
    CFRunLoopAddTimer(state.runLoop, timer, kCFRunLoopDefaultMode);
    printf("{\"event\":\"ready\",\"mode\":\"%s\",\"profile\":\"%s\","
           "\"inputMonitoring\":\"%s\",\"accessibility\":%s,"
           "\"keyboardOpen\":%s,\"keyFilterApplied\":%s}\n",
           muse_on_mode_string(state.config.mode),
           muse_on_profile_string(state.config.profile),
           access_name(inputAccess), boolean_string(state.postEventAccess),
           boolean_string(state.keyboardOpen),
           boolean_string(state.keyFilterApplied));
    CFRunLoopRun();
    CFRunLoopRemoveTimer(state.runLoop, timer, kCFRunLoopDefaultMode);
    CFRelease(timer);
  }

shutdown:
  emit_topology_snapshot(
      (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
  restore_keyboard_filter(&state, monotonic_ns(), "process_exit", true);
  if (state.keyboardOpen) {
    IOReturn keyboardClosed =
        IOHIDManagerClose(state.keyboardManager, kIOHIDOptionsTypeNone);
    if (keyboardClosed != kIOReturnSuccess &&
        keyboardClosed != kIOReturnNotOpen &&
        keyboardClosed != kIOReturnNoDevice &&
        keyboardClosed != kIOReturnOffline) {
      emit_error(&state, "close_keyboard_manager", keyboardClosed);
      state.releaseFailed = true;
      emit_safety_latch(&state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
    } else {
      state.keyboardOpen = false;
    }
  }
  if (state.joystickOpen) {
    opened = IOHIDManagerClose(state.joystickManager, kIOHIDOptionsTypeNone);
    if (opened != kIOReturnSuccess && opened != kIOReturnNotOpen &&
        opened != kIOReturnNoDevice && opened != kIOReturnOffline) {
      emit_error(&state, "close_joystick_manager", opened);
      state.releaseFailed = true;
      emit_safety_latch(&state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
    } else {
      state.joystickOpen = false;
    }
  }

  unschedule_and_release_manager(&state, state.keyboardManager);
  unschedule_and_release_manager(&state, state.joystickManager);
  muse_on_key_filter_destroy(state.keyFilter);
  cleanup_slots(&state);
  if (state.releaseFailed &&
      state.safetyFailure == MUSE_ON_SAFETY_FAILURE_NONE) {
    emit_safety_latch(&state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
  }
  emit_stopped(&state);
  if (state.releaseFailed) return 3;
  return exitCode;
}
