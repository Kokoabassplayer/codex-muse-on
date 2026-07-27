#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>
#import <IOKit/IOReturn.h>
#import <IOKit/hidsystem/IOHIDLib.h>

#include <string.h>

#include "muse_on_platform.h"

static const char kMuseOnCodexBundleIdentifier[] = "com.openai.codex";
static const int64_t kMuseOnSyntheticEventTag = 0x4d5553454f4eLL;
static const CGEventFlags kMuseOnHyperFlags =
    kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate |
    kCGEventFlagMaskControl | kCGEventFlagMaskShift;

bool muse_on_bundle_id_is_codex(const char *bundle_id) {
  return bundle_id != NULL &&
         strcmp(bundle_id, kMuseOnCodexBundleIdentifier) == 0;
}

bool muse_on_codex_is_frontmost(void) {
  @autoreleasepool {
    NSRunningApplication *application =
        [NSWorkspace sharedWorkspace].frontmostApplication;
    return muse_on_bundle_id_is_codex(application.bundleIdentifier.UTF8String);
  }
}

bool muse_on_session_is_available(void) {
  CFDictionaryRef session = CGSessionCopyCurrentDictionary();
  CFBooleanRef onConsole;
  CFBooleanRef loginDone;
  bool available;

  if (!session) return false;
  onConsole = (CFBooleanRef)CFDictionaryGetValue(
      session, kCGSessionOnConsoleKey);
  loginDone = (CFBooleanRef)CFDictionaryGetValue(
      session, kCGSessionLoginDoneKey);
  available = (!onConsole || CFBooleanGetValue(onConsole)) &&
              (!loginDone || CFBooleanGetValue(loginDone));
  CFRelease(session);
  return available;
}

bool muse_on_preflight_post_event_access(void) {
  return CGPreflightPostEventAccess();
}

bool muse_on_request_post_event_access(void) {
  return CGRequestPostEventAccess();
}

bool muse_on_input_monitoring_access_granted(void) {
  return IOHIDCheckAccess(kIOHIDRequestTypeListenEvent) ==
         kIOHIDAccessTypeGranted;
}

bool muse_on_input_monitoring_access_unknown(void) {
  return IOHIDCheckAccess(kIOHIDRequestTypeListenEvent) ==
         kIOHIDAccessTypeUnknown;
}

bool muse_on_request_input_monitoring_access(void) {
  return IOHIDRequestAccess(kIOHIDRequestTypeListenEvent);
}

bool muse_on_should_launch_listener_probe(bool enabled, bool safety_latched,
                                          bool listener_running) {
  return enabled && !safety_latched && !listener_running;
}

MuseOnPermissionGate muse_on_listener_missing_permission_gates(
    const char *input_monitoring, bool accessibility_granted) {
  MuseOnPermissionGate gates = MUSE_ON_PERMISSION_GATE_NONE;

  if (input_monitoring == NULL || strcmp(input_monitoring, "granted") != 0) {
    gates = (MuseOnPermissionGate)(gates |
                                   MUSE_ON_PERMISSION_GATE_INPUT_MONITORING);
  }
  if (!accessibility_granted) {
    gates = (MuseOnPermissionGate)(gates |
                                   MUSE_ON_PERMISSION_GATE_ACCESSIBILITY);
  }
  return gates;
}

bool muse_on_should_open_retry_permission_settings(
    bool retry_requested, bool authoritative, MuseOnPermissionGate missing,
    bool destination_already_opened) {
  return retry_requested && authoritative &&
         missing != MUSE_ON_PERMISSION_GATE_NONE &&
         !destination_already_opened;
}

bool muse_on_listener_request_mode_is_explicit(bool first_enable,
                                               bool request_permissions) {
  return first_enable && request_permissions;
}

MuseOnPermissionGate muse_on_missing_permission_gates(
    bool input_monitoring_granted, bool accessibility_granted) {
  MuseOnPermissionGate gates = MUSE_ON_PERMISSION_GATE_NONE;

  if (!input_monitoring_granted) {
    gates = (MuseOnPermissionGate)(gates |
                                   MUSE_ON_PERMISSION_GATE_INPUT_MONITORING);
  }
  if (!accessibility_granted) {
    gates = (MuseOnPermissionGate)(gates |
                                   MUSE_ON_PERMISSION_GATE_ACCESSIBILITY);
  }
  return gates;
}

MuseOnPermissionGate muse_on_retry_permission_gate(
    bool input_monitoring_granted, bool accessibility_granted) {
  if (!input_monitoring_granted) {
    return MUSE_ON_PERMISSION_GATE_INPUT_MONITORING;
  }
  if (!accessibility_granted) {
    return MUSE_ON_PERMISSION_GATE_ACCESSIBILITY;
  }
  return MUSE_ON_PERMISSION_GATE_NONE;
}

const char *muse_on_permission_guidance(MuseOnPermissionGate gates) {
  if (gates == (MUSE_ON_PERMISSION_GATE_INPUT_MONITORING |
                MUSE_ON_PERMISSION_GATE_ACCESSIBILITY)) {
    return "Permission required — enable Input Monitoring and Accessibility.";
  }
  switch (gates) {
    case MUSE_ON_PERMISSION_GATE_INPUT_MONITORING:
      return "Permission required — enable Input Monitoring.";
    case MUSE_ON_PERMISSION_GATE_ACCESSIBILITY:
      return "Permission required — enable Accessibility.";
    case MUSE_ON_PERMISSION_GATE_NONE:
      return "Permission status is current.";
  }
  return "Permission required — check Privacy & Security.";
}

const char *muse_on_permission_gate_settings_url(MuseOnPermissionGate gate) {
  switch (gate) {
    case MUSE_ON_PERMISSION_GATE_INPUT_MONITORING:
      return "x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent";
    case MUSE_ON_PERMISSION_GATE_ACCESSIBILITY:
      return "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility";
    case MUSE_ON_PERMISSION_GATE_NONE:
      return muse_on_permission_fallback_settings_url();
  }
  return muse_on_permission_fallback_settings_url();
}

const char *muse_on_permission_fallback_settings_url(void) {
  return "x-apple.systempreferences:com.apple.preference.security";
}

bool muse_on_should_request_permission(MuseOnPermissionGate gate,
                                       bool first_enable,
                                       bool access_requires_request,
                                       bool request_already_attempted) {
  return (gate == MUSE_ON_PERMISSION_GATE_INPUT_MONITORING ||
          gate == MUSE_ON_PERMISSION_GATE_ACCESSIBILITY) &&
         first_enable && access_requires_request &&
         !request_already_attempted;
}

bool muse_on_should_request_input_monitoring(bool first_enable,
                                             bool access_unknown,
                                             bool request_already_attempted) {
  return muse_on_should_request_permission(
      MUSE_ON_PERMISSION_GATE_INPUT_MONITORING, first_enable, access_unknown,
      request_already_attempted);
}

bool muse_on_listener_error_is_permission_required(const char *operation,
                                                  int32_t code) {
  return operation != NULL &&
         (strcmp(operation, "open_joystick_manager") == 0 ||
          strcmp(operation, "open_keyboard_manager") == 0) &&
         code == kIOReturnNotPermitted;
}

static void muse_on_configure_hyper_key_event(CGEventRef event,
                                              CGEventFlags flags) {
  CGEventSetFlags(event, flags);
  CGEventSetIntegerValueField(event, kCGEventSourceUserData,
                              kMuseOnSyntheticEventTag);
}

size_t muse_on_hyper_key_event_sequence(uint16_t mac_virtual_key,
                                        MuseOnPostHyperKeyPhase phase,
                                        MuseOnHyperKeyEvent *events,
                                        size_t capacity) {
  const MuseOnHyperKeyEvent tap[] = {
      {mac_virtual_key, true, kMuseOnHyperFlags},
      {mac_virtual_key, false, kMuseOnHyperFlags},
  };
  const MuseOnHyperKeyEvent down[] = {
      {kVK_Command, true, kCGEventFlagMaskCommand},
      {kVK_Option, true, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate},
      {kVK_Control, true, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate |
                              kCGEventFlagMaskControl},
      {kVK_Shift, true, kMuseOnHyperFlags},
      {mac_virtual_key, true, kMuseOnHyperFlags},
  };
  const MuseOnHyperKeyEvent up[] = {
      {mac_virtual_key, false, kMuseOnHyperFlags},
      {kVK_Shift, false, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate |
                               kCGEventFlagMaskControl},
      {kVK_Control, false, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate},
      {kVK_Option, false, kCGEventFlagMaskCommand},
      {kVK_Command, false, 0},
  };
  const MuseOnHyperKeyEvent *sequence;
  size_t count;
  size_t index;

  switch (phase) {
    case MUSE_ON_POST_HYPER_KEY_TAP:
      sequence = tap;
      count = sizeof(tap) / sizeof(tap[0]);
      break;
    case MUSE_ON_POST_HYPER_KEY_DOWN:
      sequence = down;
      count = sizeof(down) / sizeof(down[0]);
      break;
    case MUSE_ON_POST_HYPER_KEY_UP:
      sequence = up;
      count = sizeof(up) / sizeof(up[0]);
      break;
    default:
      return 0;
  }
  if (events == NULL || capacity < count) return count;
  for (index = 0; index < count; index++) events[index] = sequence[index];
  return count;
}

bool muse_on_post_hyper_key(uint16_t mac_virtual_key,
                            MuseOnPostHyperKeyPhase phase) {
  MuseOnHyperKeyEvent sequence[5];
  CGEventRef events[5] = {NULL, NULL, NULL, NULL, NULL};
  size_t count;
  size_t index;

  if (!muse_on_preflight_post_event_access()) return false;
  count = muse_on_hyper_key_event_sequence(mac_virtual_key, phase, sequence,
                                           sizeof(sequence) / sizeof(sequence[0]));
  if (count == 0) return false;
  for (index = 0; index < count; index++) {
    events[index] = CGEventCreateKeyboardEvent(
        NULL, (CGKeyCode)sequence[index].mac_virtual_key, sequence[index].key_down);
    if (events[index] == NULL) {
      while (index > 0) CFRelease(events[--index]);
      return false;
    }
    muse_on_configure_hyper_key_event(events[index],
                                      (CGEventFlags)sequence[index].flags);
  }
  for (index = 0; index < count; index++) {
    CGEventPost(kCGHIDEventTap, events[index]);
    CFRelease(events[index]);
  }
  return true;
}

bool muse_on_mac_virtual_key(MuseOnShortcutKey key, uint16_t *mac_virtual_key) {
  uint16_t candidate;

  if (mac_virtual_key == NULL) return false;
  switch (key) {
    case MUSE_ON_SHORTCUT_KEY_1: candidate = kVK_ANSI_1; break;
    case MUSE_ON_SHORTCUT_KEY_2: candidate = kVK_ANSI_2; break;
    case MUSE_ON_SHORTCUT_KEY_3: candidate = kVK_ANSI_3; break;
    case MUSE_ON_SHORTCUT_KEY_4: candidate = kVK_ANSI_4; break;
    case MUSE_ON_SHORTCUT_KEY_5: candidate = kVK_ANSI_5; break;
    case MUSE_ON_SHORTCUT_KEY_6: candidate = kVK_ANSI_6; break;
    case MUSE_ON_SHORTCUT_KEY_7: candidate = kVK_ANSI_7; break;
    case MUSE_ON_SHORTCUT_KEY_8: candidate = kVK_ANSI_8; break;
    case MUSE_ON_SHORTCUT_KEY_9: candidate = kVK_ANSI_9; break;
    case MUSE_ON_SHORTCUT_KEY_A: candidate = kVK_ANSI_A; break;
    case MUSE_ON_SHORTCUT_KEY_B: candidate = kVK_ANSI_B; break;
    case MUSE_ON_SHORTCUT_KEY_C: candidate = kVK_ANSI_C; break;
    case MUSE_ON_SHORTCUT_KEY_D: candidate = kVK_ANSI_D; break;
    case MUSE_ON_SHORTCUT_KEY_E: candidate = kVK_ANSI_E; break;
    case MUSE_ON_SHORTCUT_KEY_F: candidate = kVK_ANSI_F; break;
    case MUSE_ON_SHORTCUT_KEY_G: candidate = kVK_ANSI_G; break;
    default: return false;
  }
  *mac_virtual_key = candidate;
  return true;
}

bool muse_on_post_shortcut(const MuseOnShortcutInstruction *shortcut) {
  MuseOnPostHyperKeyPhase phase;
  uint16_t mac_virtual_key;

  if (shortcut == NULL || !shortcut->command || !shortcut->option ||
      !shortcut->control || !shortcut->shift ||
      !muse_on_mac_virtual_key(shortcut->key, &mac_virtual_key)) {
    return false;
  }
  switch (shortcut->operation) {
    case MUSE_ON_SHORTCUT_TAP:
      phase = MUSE_ON_POST_HYPER_KEY_TAP;
      break;
    case MUSE_ON_SHORTCUT_KEY_DOWN:
      phase = MUSE_ON_POST_HYPER_KEY_DOWN;
      break;
    case MUSE_ON_SHORTCUT_KEY_UP:
      phase = MUSE_ON_POST_HYPER_KEY_UP;
      break;
    default:
      return false;
  }
  return muse_on_post_hyper_key(mac_virtual_key, phase);
}
