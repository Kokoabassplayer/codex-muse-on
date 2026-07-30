#include "muse_on_capture.h"

#include <IOKit/hid/IOHIDManager.h>

uint32_t muse_on_joystick_open_options(bool capture_requested) {
  return capture_requested ? (uint32_t)kIOHIDOptionsTypeSeizeDevice
                           : (uint32_t)kIOHIDOptionsTypeNone;
}

uint32_t muse_on_keyboard_capture_options(void) {
  return (uint32_t)kIOHIDOptionsTypeNone;
}

bool muse_on_capture_is_verified(bool joystick_open,
                                 bool keyboard_filter_verified,
                                 bool controller_connected) {
  return joystick_open && keyboard_filter_verified && controller_connected;
}

bool muse_on_capture_joystick_snapshot_capacity(
    MuseOnProfile profile, size_t capacity, size_t *expected_count) {
  size_t required_count;

  if (!expected_count) return false;
  switch (profile) {
    case MUSE_ON_PROFILE_CONTROLLER_ONLY:
      required_count = 5;
      break;
    case MUSE_ON_PROFILE_PEDAL:
      required_count = 6;
      break;
    default:
      return false;
  }
  if (capacity < required_count) return false;
  *expected_count = required_count;
  return true;
}

static bool joystick_report_hat_is_valid(const uint8_t *report,
                                         size_t report_length) {
  const uint8_t *payload = report_length == 12 ? report + 1 : report;
  uint16_t hat = (uint16_t)payload[7] | ((uint16_t)payload[8] << 8);

  return hat <= 359 || hat == UINT16_MAX;
}

static bool keyboard_boot_report_is_valid(const uint8_t *report,
                                          size_t report_length) {
  size_t index;

  if (report[1] != 0) return false;
  for (index = 2; index < report_length; index++) {
    if (report[index] >= 0x01 && report[index] <= 0x03) return false;
  }
  return true;
}

bool muse_on_capture_observe_report(
    MuseOnDecoder *decoder, MuseOnInterfaceKind interface_kind,
    MuseOnProfile profile, uint8_t report_id, const uint8_t *report,
    size_t report_length, MuseOnInputObservation *observation) {
  bool valid_report;

  if (!decoder || !report || !observation) return false;
  valid_report =
      (interface_kind == MUSE_ON_INTERFACE_JOYSTICK && report_id == 1 &&
       (report_length == 11 ||
        (report_length == 12 && report[0] == report_id))) ||
      (interface_kind == MUSE_ON_INTERFACE_KEYBOARD_BOOT &&
       report_id == 0 && (report_length == 8 || report_length == 32));
  if (!valid_report) return false;
  if ((interface_kind == MUSE_ON_INTERFACE_JOYSTICK &&
       !joystick_report_hat_is_valid(report, report_length)) ||
      (interface_kind == MUSE_ON_INTERFACE_KEYBOARD_BOOT &&
       !keyboard_boot_report_is_valid(report, report_length))) {
    return false;
  }
  observation->event_count = muse_on_decode_report(
      decoder, interface_kind, report_id, report, report_length,
      observation->events, MUSE_ON_MAX_EVENTS_PER_REPORT);
  observation->neutral_entry_state =
      muse_on_selected_profile_neutral_state(decoder, interface_kind, profile);
  return observation->neutral_entry_state != MUSE_ON_NEUTRAL_ENTRY_UNKNOWN;
}

void muse_on_capture_focus_changed(
    MuseOnDecoder *decoder, MuseOnActionRouter *router,
    MuseOnProfile profile, bool codex_foreground,
    bool *neutral_entry_ready) {
  if (!decoder || !router || !neutral_entry_ready) return;
  if (codex_foreground) return;
  muse_on_action_router_init(router, profile);
  *neutral_entry_ready = false;
}

MuseOnNeutralEntryState muse_on_capture_controller_neutral_state(
    const MuseOnDecoder *keyboard_decoder,
    const MuseOnDecoder *joystick_decoder, MuseOnProfile profile) {
  MuseOnNeutralEntryState keyboard_state =
      muse_on_selected_profile_neutral_state(
          keyboard_decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, profile);
  MuseOnNeutralEntryState joystick_state =
      muse_on_selected_profile_neutral_state(
          joystick_decoder, MUSE_ON_INTERFACE_JOYSTICK, profile);

  if (keyboard_state == MUSE_ON_NEUTRAL_ENTRY_HELD ||
      joystick_state == MUSE_ON_NEUTRAL_ENTRY_HELD) {
    return MUSE_ON_NEUTRAL_ENTRY_HELD;
  }
  if (keyboard_state == MUSE_ON_NEUTRAL_ENTRY_UNKNOWN ||
      joystick_state == MUSE_ON_NEUTRAL_ENTRY_UNKNOWN) {
    return MUSE_ON_NEUTRAL_ENTRY_UNKNOWN;
  }
  return MUSE_ON_NEUTRAL_ENTRY_RELEASED;
}

MuseOnNeutralEntryState muse_on_capture_probe_neutral_entry(
    MuseOnDecoder *decoder, MuseOnInterfaceKind interface_kind,
    MuseOnProfile profile, uint8_t report_id, uint8_t *report,
    size_t report_capacity, MuseOnCurrentReportReader reader, void *context) {
  MuseOnInputObservation observation;
  size_t report_length = report_capacity;

  if (interface_kind != MUSE_ON_INTERFACE_KEYBOARD_BOOT ||
      !decoder || !report || report_capacity == 0 || !reader ||
      !reader(context, report_id, report, &report_length) ||
      report_length == 0 || report_length > report_capacity) {
    return MUSE_ON_NEUTRAL_ENTRY_UNKNOWN;
  }
  if (!muse_on_capture_observe_report(
          decoder, interface_kind, profile, report_id, report, report_length,
          &observation)) {
    return MUSE_ON_NEUTRAL_ENTRY_UNKNOWN;
  }
  return observation.neutral_entry_state;
}

MuseOnNeutralEntryState muse_on_capture_probe_joystick_elements(
    MuseOnDecoder *decoder, MuseOnProfile profile,
    MuseOnCurrentJoystickElementReader reader, void *context) {
  MuseOnInputObservation observation;
  uint8_t report[11] = {
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0xff, 0xff, 0x00, 0x00,
  };
  size_t expected_count;
  size_t index;

  if (!muse_on_capture_joystick_snapshot_capacity(
          profile, 6, &expected_count) || !decoder || !reader) {
    return MUSE_ON_NEUTRAL_ENTRY_UNKNOWN;
  }
  for (index = 0; index < expected_count; index++) {
    MuseOnJoystickElementValue value;
    MuseOnJoystickElement element = (MuseOnJoystickElement)index;
    bool valid_value;
    uint32_t expected_usage_page =
        element == MUSE_ON_JOYSTICK_ELEMENT_HAT ? 0x01U : 0x09U;
    uint32_t expected_usage =
        element == MUSE_ON_JOYSTICK_ELEMENT_HAT ? 0x39U : (uint32_t)element;
    int64_t expected_maximum =
        element == MUSE_ON_JOYSTICK_ELEMENT_HAT ? 359 : 1;

    if (!reader(context, element, &value)) {
      return MUSE_ON_NEUTRAL_ENTRY_UNKNOWN;
    }
    valid_value = element == MUSE_ON_JOYSTICK_ELEMENT_HAT
        ? ((value.value >= 0 && value.value <= expected_maximum) ||
           value.value == UINT16_MAX)
        : value.value >= value.logical_minimum &&
              value.value <= value.logical_maximum;

    if (value.element != element ||
        !value.is_absolute_input || value.report_id != 1 ||
        value.usage_page != expected_usage_page || value.usage != expected_usage ||
        value.logical_minimum != 0 || value.logical_maximum != expected_maximum ||
        !valid_value) {
      return MUSE_ON_NEUTRAL_ENTRY_UNKNOWN;
    }
    if (element == MUSE_ON_JOYSTICK_ELEMENT_HAT) {
      report[7] = (uint8_t)((uint16_t)value.value & 0xffU);
      report[8] = (uint8_t)(((uint16_t)value.value >> 8) & 0xffU);
    } else if (value.value != 0) {
      report[9] |= (uint8_t)(1U << (element - 1));
    }
  }

  if (!muse_on_capture_observe_report(
          decoder, MUSE_ON_INTERFACE_JOYSTICK, profile, 1, report,
          sizeof(report), &observation)) {
    return MUSE_ON_NEUTRAL_ENTRY_UNKNOWN;
  }
  return observation.neutral_entry_state;
}
