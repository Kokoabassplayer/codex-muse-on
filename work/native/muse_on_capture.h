#ifndef MUSE_ON_CAPTURE_H
#define MUSE_ON_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "muse_on_action_map.h"

typedef bool (*MuseOnCurrentReportReader)(void *context, uint8_t report_id,
                                          uint8_t *report,
                                          size_t *report_length);

typedef enum {
  MUSE_ON_JOYSTICK_ELEMENT_HAT = 0,
  MUSE_ON_JOYSTICK_ELEMENT_BUTTON1,
  MUSE_ON_JOYSTICK_ELEMENT_BUTTON2,
  MUSE_ON_JOYSTICK_ELEMENT_BUTTON3,
  MUSE_ON_JOYSTICK_ELEMENT_BUTTON4,
  MUSE_ON_JOYSTICK_ELEMENT_BUTTON5
} MuseOnJoystickElement;

typedef struct {
  MuseOnJoystickElement element;
  bool is_absolute_input;
  uint32_t report_id;
  uint32_t usage_page;
  uint32_t usage;
  int64_t logical_minimum;
  int64_t logical_maximum;
  int64_t value;
} MuseOnJoystickElementValue;

typedef bool (*MuseOnCurrentJoystickElementReader)(
    void *context, MuseOnJoystickElement element,
    MuseOnJoystickElementValue *value);

typedef struct {
  MuseOnEvent events[MUSE_ON_MAX_EVENTS_PER_REPORT];
  size_t event_count;
  MuseOnNeutralEntryState neutral_entry_state;
} MuseOnInputObservation;

uint32_t muse_on_joystick_open_options(bool capture_requested);
uint32_t muse_on_keyboard_capture_options(void);
bool muse_on_capture_is_verified(bool joystick_open,
                                 bool keyboard_filter_verified,
                                 bool controller_connected);
bool muse_on_capture_joystick_snapshot_capacity(
    MuseOnProfile profile, size_t capacity, size_t *expected_count);
bool muse_on_capture_observe_report(
    MuseOnDecoder *decoder, MuseOnInterfaceKind interface_kind,
    MuseOnProfile profile, uint8_t report_id, const uint8_t *report,
    size_t report_length, MuseOnInputObservation *observation);
MuseOnNeutralEntryState muse_on_capture_probe_neutral_entry(
    MuseOnDecoder *decoder, MuseOnInterfaceKind interface_kind,
    MuseOnProfile profile, uint8_t report_id, uint8_t *report,
    size_t report_capacity, MuseOnCurrentReportReader reader, void *context);
MuseOnNeutralEntryState muse_on_capture_probe_joystick_elements(
    MuseOnDecoder *decoder, MuseOnProfile profile,
    MuseOnCurrentJoystickElementReader reader, void *context);
bool muse_on_capture_read_joystick_snapshot(
    MuseOnProfile profile, MuseOnCurrentJoystickElementReader reader,
    void *context, uint8_t report[11]);

#endif
