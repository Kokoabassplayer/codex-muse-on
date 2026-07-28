#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOReturn.h>
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
#include "muse_on_platform.h"
#include "muse_on_shortcut_map.h"
#include "muse_on_state_coordinator.h"

/*
 * Muse-On adapter for Codex.
 *
 * Dry-run is the default and never posts an event. Capture modes open only the
 * exact Muse-On interfaces with IOHID's exclusive option, so raw controller
 * events do not reach other applications. Closing the managers or terminating
 * the helper releases that ownership in the kernel without mutating system
 * keyboard mappings. Actions still route only while Codex is frontmost.
 */

enum {
  kMuseOnVendorID = 0x04b4,
  kMuseOnProductID = 0xe106,
};

static const CFTimeInterval kActionFlushIntervalSeconds = 0.01;
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
  InterfaceKind kind;
  CFIndex reportCapacity;
  MuseOnDecoder decoder;
  MuseOnActionRouter router;
  bool neutralEntryReady;
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
  bool joystickOpen;
  bool keyboardOpen;
  /* Protocol-compatible name: true means both exact HID managers are seized. */
  bool keyFilterApplied;
  bool syntheticHoldDown;
  bool codexFrontmost;
  bool focusKnown;
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

static bool all_inputs_released(const ListenerState *state) {
  uint32_t locationID;
  const DeviceSlot *slot;

  if (!validated_controller_location(state, &locationID)) return false;
  for (slot = state->slots; slot; slot = slot->next) {
    if (!slot->removed && slot->locationID == locationID &&
        !slot->neutralEntryReady) {
      return false;
    }
  }
  return true;
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
  inputsReleased = controllerConnected && all_inputs_released(state);
  filterVerified = muse_on_capture_is_verified(
      state->keyboardOpen, state->joystickOpen, controllerConnected);
  observation = (MuseOnRecoveryObservation){
      .permission_granted = filter_permissions_ready(state),
      .controller_connected = controllerConnected,
      .multiple_controllers = topology.state == MUSE_ON_CONNECTION_MULTIPLE,
      .codex_foreground = state->codexFrontmost,
      .inputs_released = inputsReleased,
      .filter_verified = filterVerified,
      .keyboard_open = state->keyboardOpen,
      .error_observed = state->recoveryErrorObserved,
  };
  decision = muse_on_recovery_policy_evaluate(
      &state->recoveryPolicy, monotonic_ns(), observation);
  outcome = muse_on_recovery_outcome_for(decision, observation);
  /* HID enumeration and filter proof may settle over several timer ticks.
   * The safety latch already makes routing_is_enabled() return false. */
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
         boolean_string(state->codexFrontmost),
         boolean_string(inputsReleased), boolean_string(filterVerified),
         boolean_string(state->keyboardOpen),
         muse_on_connection_state_string(topology.state), topology.location_id);
  state->recoveryStateEmitted = true;
  stopRequested = 1;
}

static bool slot_matches_captured_controller(const ListenerState *state,
                                             const DeviceSlot *slot) {
  uint32_t controllerLocation;

  if (!slot) return false;
  if (state->config.mode == MUSE_ON_MODE_DRY_RUN) return true;
  if (!state->keyFilterApplied) return false;
  if (!validated_controller_location(state, &controllerLocation) ||
      slot->locationID != controllerLocation) return false;
  return true;
}

static void reset_slots(ListenerState *state, InterfaceKind kind,
                        bool markRemoved) {
  DeviceSlot *slot;

  for (slot = state->slots; slot; slot = slot->next) {
    if (kind != kInterfaceUnknown && slot->kind != kind) continue;
    muse_on_decoder_init(&slot->decoder);
    muse_on_action_router_init(&slot->router, state->config.profile);
    slot->neutralEntryReady = false;
    if (markRemoved) slot->removed = true;
  }
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
  captured = state->keyFilterApplied &&
             slot_matches_captured_controller(state, slot);
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

static bool routing_is_enabled(ListenerState *state) {
  bool frontmost;
  bool captured;
  uint32_t controllerLocation;

  if (state->config.mode == MUSE_ON_MODE_DRY_RUN) return true;
  if (state->config.safety_latched) return false;
  if (state->config.mode == MUSE_ON_MODE_ACTIVE &&
      !filter_permissions_ready(state)) return false;
  if (!validated_controller_location(state, &controllerLocation)) return false;
  for (DeviceSlot *slot = state->slots; slot; slot = slot->next) {
    if (!slot->removed && slot->locationID == controllerLocation &&
        !slot->neutralEntryReady) return false;
  }
  frontmost = muse_on_codex_is_frontmost();
  captured = state->keyFilterApplied &&
             state->keyboardOpen && state->joystickOpen;
  return muse_on_can_route_actions(state->config.mode, frontmost, captured);
}

static void report_received(void *context, IOReturn result, void *sender,
                            IOHIDReportType type, uint32_t reportID,
                            uint8_t *report, CFIndex reportLength,
                            uint64_t timeStamp) {
  ManagerContext *managerContext = context;
  ListenerState *state;
  DeviceSlot *slot;
  MuseOnEvent events[MUSE_ON_MAX_EVENTS_PER_REPORT];
  MuseOnInterfaceKind interfaceKind;
  uint64_t receivedNs;
  size_t eventCount;
  size_t index;

  if (!managerContext || !managerContext->state || !sender) return;
  state = managerContext->state;
  slot = find_slot(state, (IOHIDDeviceRef)sender);
  if (!slot || slot->removed || slot->kind != managerContext->kind) return;
  if (!slot_matches_captured_controller(state, slot)) return;
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
  eventCount = muse_on_decode_report(
      &slot->decoder, interfaceKind, (uint8_t)reportID, report,
      (size_t)reportLength, events, MUSE_ON_MAX_EVENTS_PER_REPORT);
  if (!slot->neutralEntryReady) {
    bool hasHeldControl = false;
    for (index = 0; index < eventCount; index++) {
      switch (events[index].name) {
        case MUSE_ON_EVENT_WHITE1_DOWN:
        case MUSE_ON_EVENT_BLACK2_DOWN:
        case MUSE_ON_EVENT_BLACK6_DOWN:
        case MUSE_ON_EVENT_BLACK8_DOWN:
        case MUSE_ON_EVENT_PEDAL_DOWN:
        case MUSE_ON_EVENT_WHITE3_DOWN:
        case MUSE_ON_EVENT_BLACK4_DOWN:
        case MUSE_ON_EVENT_WHITE5_DOWN:
        case MUSE_ON_EVENT_WHITE7_DOWN:
        case MUSE_ON_EVENT_LEFT_BALL_NORTH_ENGAGED:
        case MUSE_ON_EVENT_LEFT_BALL_SOUTH_ENGAGED:
        case MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_ENGAGED:
        case MUSE_ON_EVENT_RIGHT_BALL_WEST_ENGAGED:
        case MUSE_ON_EVENT_RIGHT_BALL_EAST_ENGAGED:
        case MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED:
        case MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED:
          hasHeldControl = true;
          break;
        default:
          break;
      }
      if (hasHeldControl) break;
    }
    if (hasHeldControl) return;
    slot->neutralEntryReady = true;
    printf("{\"event\":\"neutral_entry\",\"inputsReleased\":%s}\n",
           boolean_string(all_inputs_released(state)));
    return;
  }
  if (!routing_is_enabled(state)) return;
  for (index = 0; index < eventCount; index++) {
    MuseOnActionEvent action;
    if (muse_on_action_router_route(&slot->router, events[index].name,
                                    receivedNs, &action)) {
      handle_action(state, slot, &action);
    }
  }
}

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
                       &maxInputReportSize)) {
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
    slot->reportCapacity = (CFIndex)maxInputReportSize;
    muse_on_decoder_init(&slot->decoder);
    muse_on_action_router_init(&slot->router, state->config.profile);
    slot->neutralEntryReady = false;
  } else {
    slot = calloc(1, sizeof(*slot));
    if (!slot) {
      emit_error(state, "allocate_slot", kIOReturnNoMemory);
      return;
    }
    slot->device = (IOHIDDeviceRef)CFRetain(device);
    slot->locationID = locationID;
    slot->usagePage = usagePage;
    slot->usage = usage;
    slot->kind = kind;
    slot->reportCapacity = (CFIndex)maxInputReportSize;
    muse_on_decoder_init(&slot->decoder);
    muse_on_action_router_init(&slot->router, state->config.profile);
    slot->next = state->slots;
    state->slots = slot;
  }

  reset_slots(state, kInterfaceUnknown, false);
  emit_device_event("device_added", slot);
  emit_current_topology(state);
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
  reset_slots(state, kInterfaceUnknown, false);
  emit_device_event("device_removed", slot);
  emit_current_topology(state);
  if (state->syntheticHoldDown) {
    force_release_synthetic_hold(state, "device_removed");
  }
}

static void stop_signal(int signalNumber) {
  (void)signalNumber;
  stopRequested = 1;
}

static void update_focus_and_filter(ListenerState *state, uint64_t nowNs) {
  bool frontmost;

  if (state->config.mode == MUSE_ON_MODE_DRY_RUN) return;
  refresh_permission_state(state, nowNs);
  frontmost = muse_on_codex_is_frontmost();
  if (!state->focusKnown || frontmost != state->codexFrontmost) {
    state->focusKnown = true;
    state->codexFrontmost = frontmost;
    printf("{\"event\":\"focus_changed\",\"codexFrontmost\":%s}\n",
           boolean_string(frontmost));
    if (!frontmost) {
      force_release_synthetic_hold(state, "codex_focus_lost");
      reset_slots(state, kInterfaceUnknown, false);
    }
  }
}

static void action_timer(CFRunLoopTimerRef timer, void *context) {
  ListenerState *state = context;
  DeviceSlot *slot;
  uint64_t nowNs;

  (void)timer;
  if (!state) return;
  nowNs = monotonic_ns();
  update_focus_and_filter(state, nowNs);
  if (state->config.safety_latched) emit_recovery_state(state);

  if (routing_is_enabled(state)) {
    for (slot = state->slots; slot; slot = slot->next) {
      MuseOnActionEvent action;
      if (!slot->removed &&
          muse_on_action_router_tick(&slot->router, nowNs, &action)) {
        handle_action(state, slot, &action);
      }
    }
  }
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
  IOOptionBits captureOptions;
  IOReturn opened;
  CFRunLoopTimerContext timerContext;
  CFRunLoopTimerRef timer;
  bool captureReleaseVerified = true;
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
  if (!state.joystickManager || !state.keyboardManager) {
    emit_error(&state, "create_hid_managers", kIOReturnNoMemory);
    emit_topology_snapshot(
        (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
    emit_safety_latch(&state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
    unschedule_and_release_manager(&state, state.keyboardManager);
    unschedule_and_release_manager(&state, state.joystickManager);
    emit_stopped(&state);
    return 1;
  }

  captureOptions =
      state.config.mode == MUSE_ON_MODE_DRY_RUN
          ? kIOHIDOptionsTypeNone
          : (IOOptionBits)muse_on_capture_open_options();
  opened = IOHIDManagerOpen(state.joystickManager, captureOptions);
  if (opened != kIOReturnSuccess) {
    emit_topology_snapshot(
        (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0});
    if (muse_on_listener_error_is_permission_required(
            "open_joystick_manager", opened)) {
      emit_permission_required(&state, "input_monitoring");
      goto shutdown;
    }
    emit_error(&state, "open_joystick_manager", opened);
    unschedule_and_release_manager(&state, state.keyboardManager);
    unschedule_and_release_manager(&state, state.joystickManager);
    return 2;
  }
  state.joystickOpen = true;

  opened = IOHIDManagerOpen(state.keyboardManager, captureOptions);
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
  state.keyFilterApplied = state.config.mode != MUSE_ON_MODE_DRY_RUN;
  if (state.keyFilterApplied) {
    emit_capture_state(&state, "filter_applied", "exclusive_hid");
  }
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
  force_release_synthetic_hold(&state, "process_exit");
  if (state.keyboardOpen) {
    IOReturn keyboardClosed =
        IOHIDManagerClose(state.keyboardManager, kIOHIDOptionsTypeNone);
    if (!muse_on_capture_close_result_is_acceptable(keyboardClosed)) {
      emit_error(&state, "close_keyboard_manager", keyboardClosed);
      captureReleaseVerified = false;
    }
    state.keyboardOpen = false;
  }
  if (state.joystickOpen) {
    opened = IOHIDManagerClose(state.joystickManager, kIOHIDOptionsTypeNone);
    if (!muse_on_capture_close_result_is_acceptable(opened)) {
      emit_error(&state, "close_joystick_manager", opened);
      captureReleaseVerified = false;
    }
    state.joystickOpen = false;
  }
  if (!captureReleaseVerified) {
    state.releaseFailed = true;
    emit_safety_latch(
        &state, MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE);
  }
  if (state.keyFilterApplied) {
    state.keyFilterApplied = false;
    if (captureReleaseVerified) {
      emit_capture_state(&state, "filter_restored", "exclusive_hid_closed");
    }
  }

  unschedule_and_release_manager(&state, state.keyboardManager);
  unschedule_and_release_manager(&state, state.joystickManager);
  cleanup_slots(&state);
  if (state.releaseFailed &&
      state.safetyFailure == MUSE_ON_SAFETY_FAILURE_NONE) {
    emit_safety_latch(&state, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
  }
  emit_stopped(&state);
  if (state.releaseFailed) return 3;
  return exitCode;
}
