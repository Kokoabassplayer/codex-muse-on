#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "../muse_on_decoder.h"

static const uint8_t neutral[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x00, 0x00
};

static const uint8_t white1_press[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x01, 0x00
};

static const uint8_t native_full_neutral[] = {
  0x01, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x00, 0x00
};

static const uint8_t native_full_white1_press[] = {
  0x01, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x01, 0x00
};

static const uint8_t native_full_wrong_prefix[] = {
  0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x01, 0x00
};

static const uint8_t black2_press[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x02, 0x00
};

static const uint8_t black6_press[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x04, 0x00
};

static const uint8_t black8_press[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x08, 0x00
};

static const uint8_t white1_black2_chord[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x03, 0x00
};

static const uint8_t pedal_press[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x10, 0x00
};

static const uint8_t axis0_active[] = {
  0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x00, 0x00
};

static const uint8_t hat_010e[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x0e, 0x01, 0x00, 0x00
};

static const uint8_t hat_005a[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x5a, 0x00, 0x00, 0x00
};

static const uint8_t generic_button12_down[] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0x00, 0x08
};

static const uint8_t keyboard_neutral[32] = {0x00};
static const uint8_t keyboard_digit0_down[32] = {0x00, 0x00, 0x27};
static const uint8_t keyboard_modifier_e0_down[32] = {0x01};
static const uint8_t keyboard_boot_neutral[8] = {0x00};
static const uint8_t keyboard_boot_digit0_down[8] = {0x00, 0x00, 0x27};

static const uint8_t mouse_neutral[] = {0x00, 0x00, 0x00, 0x00};
static const uint8_t mouse_button1_down[] = {0x01, 0x00, 0x00, 0x00};
static const uint8_t mouse_padding_bit4_down[] = {0x08, 0x00, 0x00, 0x00};
static const uint8_t mouse_dx_positive[] = {0x00, 0x05, 0x00, 0x00};
static const uint8_t mouse_dy_negative[] = {0x00, 0x00, 0xfe, 0x00};
static const uint8_t mouse_wheel_negative[] = {0x00, 0x00, 0x00, 0xff};

static size_t decode_interface(MuseOnDecoder *decoder, MuseOnInterfaceKind interface_kind,
                               uint8_t report_id, const uint8_t *report, size_t report_size,
                               MuseOnEvent *events, size_t event_capacity) {
  return muse_on_decode_report(decoder, interface_kind, report_id,
                               report, report_size, events, event_capacity);
}

static size_t decode(MuseOnDecoder *decoder, const uint8_t *report, size_t report_size,
                     MuseOnEvent *events, size_t event_capacity) {
  return decode_interface(decoder, MUSE_ON_INTERFACE_JOYSTICK, 1,
                          report, report_size, events, event_capacity);
}

static void test_white1_press_and_release_after_neutral(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];
  MuseOnEvent *event = events;

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 0);

  assert(decode(&decoder, white1_press, sizeof(white1_press), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_WHITE1_DOWN);
  assert(event->interface_kind == MUSE_ON_INTERFACE_JOYSTICK);
  assert(event->report_id == 1);
  assert(event->byte_index == 9);
  assert(event->bit_mask == 0x01);
  assert(event->usage == 0);

  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_WHITE1_UP);
  assert(event->byte_index == 9);
  assert(event->bit_mask == 0x01);
}

static void test_first_active_report_emits_against_known_neutral(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, white1_press, sizeof(white1_press), events, 2) == 1);
  assert(events[0].name == MUSE_ON_EVENT_WHITE1_DOWN);

  muse_on_decoder_init(&decoder);
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_digit0_down, sizeof(keyboard_digit0_down), events, 2) == 1);
  assert(events[0].name == MUSE_ON_EVENT_WHITE3_DOWN);
  assert(events[0].usage == 0x27);

  muse_on_decoder_init(&decoder);
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_MOUSE, 0,
                          mouse_dx_positive, sizeof(mouse_dx_positive), events, 2) == 1);
  assert(events[0].name == MUSE_ON_EVENT_MOUSE_RELATIVE_X);
  assert(events[0].value == 5);
}

static void test_keyboard_boot_current_report_uses_six_usage_slots(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];

  muse_on_decoder_init(&decoder);
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_boot_neutral,
                          sizeof(keyboard_boot_neutral), events, 2) == 0);
  assert(decoder.keyboard_usages_seen);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_boot_digit0_down,
                          sizeof(keyboard_boot_digit0_down), events, 2) == 1);
  assert(events[0].name == MUSE_ON_EVENT_WHITE3_DOWN);
  assert(events[0].usage == 0x27);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_boot_neutral,
                          sizeof(keyboard_boot_neutral), events, 2) == 1);
  assert(events[0].name == MUSE_ON_EVENT_WHITE3_UP);
  assert(events[0].usage == 0x27);
}

static void test_native_full_joystick_report_normalizes_matching_id_prefix(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[MUSE_ON_MAX_EVENTS_PER_REPORT];

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, native_full_neutral, sizeof(native_full_neutral),
                events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 0);
  assert(decode(&decoder, native_full_white1_press, sizeof(native_full_white1_press),
                events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 1);
  assert(events[0].name == MUSE_ON_EVENT_WHITE1_DOWN);

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, native_full_wrong_prefix, sizeof(native_full_wrong_prefix),
                events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 0);
}

static void test_joystick_axes_and_generic_button12(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[MUSE_ON_MAX_EVENTS_PER_REPORT];

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, neutral, sizeof(neutral), events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 0);
  assert(decode(&decoder, axis0_active, sizeof(axis0_active), events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 1);
  assert(events[0].name == MUSE_ON_EVENT_JOYSTICK_AXIS);
  assert(events[0].byte_index == 0);
  assert(events[0].usage == 0x30);
  assert(events[0].value == 0x7f);

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, neutral, sizeof(neutral), events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 0);
  assert(decode(&decoder, generic_button12_down, sizeof(generic_button12_down),
                events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 1);
  assert(events[0].name == MUSE_ON_EVENT_JOYSTICK_BUTTON_DOWN);
  assert(events[0].byte_index == 10);
  assert(events[0].bit_mask == 0x08);
  assert(events[0].usage == 12);
}

static void test_verified_keyboard_usages_have_physical_names(void) {
  struct {
    uint8_t usage;
    MuseOnEventName down;
    MuseOnEventName up;
  } cases[] = {
    {0x27, MUSE_ON_EVENT_WHITE3_DOWN, MUSE_ON_EVENT_WHITE3_UP},
    {0x26, MUSE_ON_EVENT_BLACK4_DOWN, MUSE_ON_EVENT_BLACK4_UP},
    {0x50, MUSE_ON_EVENT_WHITE5_DOWN, MUSE_ON_EVENT_WHITE5_UP},
    {0x4f, MUSE_ON_EVENT_WHITE7_DOWN, MUSE_ON_EVENT_WHITE7_UP},
    {0x3d, MUSE_ON_EVENT_LEFT_BALL_NORTH_ENGAGED, MUSE_ON_EVENT_LEFT_BALL_NORTH_RELEASED},
    {0x3c, MUSE_ON_EVENT_LEFT_BALL_SOUTH_ENGAGED, MUSE_ON_EVENT_LEFT_BALL_SOUTH_RELEASED},
    {0x1e, MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_ENGAGED, MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_RELEASED},
    {0x1f, MUSE_ON_EVENT_RIGHT_BALL_WEST_ENGAGED, MUSE_ON_EVENT_RIGHT_BALL_WEST_RELEASED},
    {0x20, MUSE_ON_EVENT_RIGHT_BALL_EAST_ENGAGED, MUSE_ON_EVENT_RIGHT_BALL_EAST_RELEASED},
  };
  MuseOnDecoder decoder;
  MuseOnEvent events[2];
  uint8_t report[32] = {0};
  size_t index;

  for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
    muse_on_decoder_init(&decoder);
    report[2] = cases[index].usage;
    assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                            report, sizeof(report), events, 2) == 1);
    assert(events[0].name == cases[index].down);
    assert(events[0].usage == cases[index].usage);

    report[2] = 0;
    assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                            report, sizeof(report), events, 2) == 1);
    assert(events[0].name == cases[index].up);
    assert(events[0].usage == cases[index].usage);
  }
}

static void test_turntable_keyboard_duplicate_remains_generic(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];
  uint8_t report[32] = {0};

  muse_on_decoder_init(&decoder);
  report[2] = 0xe1;
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          report, sizeof(report), events, 2) == 1);
  assert(events[0].name == MUSE_ON_EVENT_KEY_USAGE_DOWN);
  assert(events[0].usage == 0xe1);

  report[2] = 0;
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          report, sizeof(report), events, 2) == 1);
  assert(events[0].name == MUSE_ON_EVENT_KEY_USAGE_UP);
  assert(events[0].usage == 0xe1);
}

static void test_turntable_hat_directions_emit_named_lifecycle_events(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 0);
  assert(decode(&decoder, hat_005a, sizeof(hat_005a), events, 2) == 1);
  assert(events[0].name == MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED);
  assert(events[0].usage == 0x39);
  assert(events[0].value == 0x005a);

  assert(decode(&decoder, hat_010e, sizeof(hat_010e), events, 2) == 2);
  assert(events[0].name == MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_RELEASED);
  assert(events[0].value == 0x005a);
  assert(events[1].name == MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED);
  assert(events[1].value == 0x010e);

  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 1);
  assert(events[0].name == MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_RELEASED);
  assert(events[0].value == 0x010e);
}

static void test_black2_press_and_release_after_neutral(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];
  MuseOnEvent *event = events;

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 0);

  assert(decode(&decoder, black2_press, sizeof(black2_press), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_BLACK2_DOWN);
  assert(event->interface_kind == MUSE_ON_INTERFACE_JOYSTICK);
  assert(event->report_id == 1);
  assert(event->byte_index == 9);
  assert(event->bit_mask == 0x02);
  assert(event->usage == 0);

  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_BLACK2_UP);
}

static void test_black6_press_and_release_after_neutral(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];
  MuseOnEvent *event = events;

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 0);

  assert(decode(&decoder, black6_press, sizeof(black6_press), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_BLACK6_DOWN);
  assert(event->interface_kind == MUSE_ON_INTERFACE_JOYSTICK);
  assert(event->report_id == 1);
  assert(event->byte_index == 9);
  assert(event->bit_mask == 0x04);

  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_BLACK6_UP);
}

static void test_black8_press_and_release_after_neutral(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];
  MuseOnEvent *event = events;

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 0);

  assert(decode(&decoder, black8_press, sizeof(black8_press), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_BLACK8_DOWN);
  assert(event->interface_kind == MUSE_ON_INTERFACE_JOYSTICK);
  assert(event->report_id == 1);
  assert(event->byte_index == 9);
  assert(event->bit_mask == 0x08);

  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_BLACK8_UP);
}

static void test_pedal_press_and_release_after_neutral(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];
  MuseOnEvent *event = events;

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 0);

  assert(decode(&decoder, pedal_press, sizeof(pedal_press), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_PEDAL_DOWN);
  assert(event->interface_kind == MUSE_ON_INTERFACE_JOYSTICK);
  assert(event->report_id == 1);
  assert(event->byte_index == 9);
  assert(event->bit_mask == 0x10);
  assert(event->usage == 0);

  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_PEDAL_UP);
}

static void test_chord_emits_each_changed_bit_in_bit_order(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];

  muse_on_decoder_init(&decoder);
  assert(decode(&decoder, neutral, sizeof(neutral), events, 2) == 0);
  assert(decode(&decoder, white1_black2_chord, sizeof(white1_black2_chord), events, 2) == 2);
  assert(events[0].name == MUSE_ON_EVENT_WHITE1_DOWN);
  assert(events[0].bit_mask == 0x01);
  assert(events[1].name == MUSE_ON_EVENT_BLACK2_DOWN);
  assert(events[1].bit_mask == 0x02);
}

static void test_keyboard_boot_usage_down_and_up(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];
  MuseOnEvent *event = events;

  muse_on_decoder_init(&decoder);
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_neutral, sizeof(keyboard_neutral), events, 2) == 0);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_digit0_down, sizeof(keyboard_digit0_down), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_WHITE3_DOWN);
  assert(event->interface_kind == MUSE_ON_INTERFACE_KEYBOARD_BOOT);
  assert(event->report_id == 0);
  assert(event->byte_index == 2);
  assert(event->bit_mask == 0);
  assert(event->usage == 0x27);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_neutral, sizeof(keyboard_neutral), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_WHITE3_UP);
  assert(event->usage == 0x27);
}

static void test_keyboard_modifier_usage_down_and_up(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[MUSE_ON_MAX_EVENTS_PER_REPORT];
  MuseOnEvent *event = events;

  muse_on_decoder_init(&decoder);
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_neutral, sizeof(keyboard_neutral),
                          events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 0);
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_modifier_e0_down, sizeof(keyboard_modifier_e0_down),
                          events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 1);
  assert(event->name == MUSE_ON_EVENT_KEY_USAGE_DOWN);
  assert(event->byte_index == 0);
  assert(event->bit_mask == 0x01);
  assert(event->usage == 0xe0);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0,
                          keyboard_neutral, sizeof(keyboard_neutral),
                          events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 1);
  assert(event->name == MUSE_ON_EVENT_KEY_USAGE_UP);
  assert(event->usage == 0xe0);
}

static void test_mouse_button_and_relative_events(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[2];
  MuseOnEvent *event = events;

  muse_on_decoder_init(&decoder);
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_MOUSE, 0,
                          mouse_neutral, sizeof(mouse_neutral), events, 2) == 0);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_MOUSE, 0,
                          mouse_button1_down, sizeof(mouse_button1_down), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_MOUSE_BUTTON_DOWN);
  assert(event->interface_kind == MUSE_ON_INTERFACE_MOUSE);
  assert(event->report_id == 0);
  assert(event->byte_index == 0);
  assert(event->bit_mask == 0x01);
  assert(event->usage == 0);
  assert(event->value == 0);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_MOUSE, 0,
                          mouse_neutral, sizeof(mouse_neutral), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_MOUSE_BUTTON_UP);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_MOUSE, 0,
                          mouse_dx_positive, sizeof(mouse_dx_positive), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_MOUSE_RELATIVE_X);
  assert(event->byte_index == 1);
  assert(event->value == 5);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_MOUSE, 0,
                          mouse_dy_negative, sizeof(mouse_dy_negative), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_MOUSE_RELATIVE_Y);
  assert(event->byte_index == 2);
  assert(event->value == -2);

  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_MOUSE, 0,
                          mouse_wheel_negative, sizeof(mouse_wheel_negative), events, 2) == 1);
  assert(event->name == MUSE_ON_EVENT_MOUSE_WHEEL);
  assert(event->byte_index == 3);
  assert(event->value == -1);
}

static void test_mouse_ignores_padding_button_bits(void) {
  MuseOnDecoder decoder;
  MuseOnEvent events[MUSE_ON_MAX_EVENTS_PER_REPORT];

  muse_on_decoder_init(&decoder);
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_MOUSE, 0,
                          mouse_neutral, sizeof(mouse_neutral),
                          events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 0);
  assert(decode_interface(&decoder, MUSE_ON_INTERFACE_MOUSE, 0,
                          mouse_padding_bit4_down, sizeof(mouse_padding_bit4_down),
                          events, MUSE_ON_MAX_EVENTS_PER_REPORT) == 0);
}

int main(void) {
  test_keyboard_boot_current_report_uses_six_usage_slots();
  test_white1_press_and_release_after_neutral();
  test_first_active_report_emits_against_known_neutral();
  test_native_full_joystick_report_normalizes_matching_id_prefix();
  test_joystick_axes_and_generic_button12();
  test_black2_press_and_release_after_neutral();
  test_black6_press_and_release_after_neutral();
  test_black8_press_and_release_after_neutral();
  test_pedal_press_and_release_after_neutral();
  test_chord_emits_each_changed_bit_in_bit_order();
  test_keyboard_boot_usage_down_and_up();
  test_verified_keyboard_usages_have_physical_names();
  test_turntable_keyboard_duplicate_remains_generic();
  test_turntable_hat_directions_emit_named_lifecycle_events();
  test_keyboard_modifier_usage_down_and_up();
  test_mouse_button_and_relative_events();
  test_mouse_ignores_padding_button_bits();
  return 0;
}
