#include "../muse_on_capture.h"

#include <assert.h>
#include <IOKit/hid/IOHIDManager.h>
#include <string.h>

typedef struct {
  const uint8_t *bytes;
  size_t length;
  bool succeeds;
  uint8_t expected_report_id;
} FakeCurrentReport;

typedef struct {
  MuseOnJoystickElementValue values[6];
  bool available[6];
  bool read_succeeds;
  unsigned int reads;
  MuseOnJoystickElement read_elements[6];
} FakeJoystickElements;

static bool read_current_report(void *context, uint8_t report_id,
                                uint8_t *report, size_t *report_length) {
  FakeCurrentReport *fake = context;

  assert(report_id == fake->expected_report_id);
  if (!fake->succeeds || !report || !report_length ||
      *report_length < fake->length) {
    return false;
  }
  memcpy(report, fake->bytes, fake->length);
  *report_length = fake->length;
  return true;
}

static bool read_joystick_element(void *context,
                                  MuseOnJoystickElement element,
                                  MuseOnJoystickElementValue *value) {
  FakeJoystickElements *fake = context;

  if (!value || element < MUSE_ON_JOYSTICK_ELEMENT_HAT ||
      element > MUSE_ON_JOYSTICK_ELEMENT_BUTTON5 ||
      !fake->read_succeeds || !fake->available[element]) {
    return false;
  }
  assert(fake->reads < sizeof(fake->read_elements) / sizeof(fake->read_elements[0]));
  fake->read_elements[fake->reads++] = element;
  *value = fake->values[element];
  return true;
}

static FakeJoystickElements neutral_joystick_elements(bool pedal) {
  FakeJoystickElements fake = {.read_succeeds = true};
  MuseOnJoystickElement last = pedal ? MUSE_ON_JOYSTICK_ELEMENT_BUTTON5
                                     : MUSE_ON_JOYSTICK_ELEMENT_BUTTON4;
  MuseOnJoystickElement element;

  for (element = MUSE_ON_JOYSTICK_ELEMENT_HAT; element <= last; element++) {
    MuseOnJoystickElementValue *value = &fake.values[element];
    fake.available[element] = true;
    value->element = element;
    value->is_absolute_input = true;
    value->report_id = 1;
    value->usage_page = element == MUSE_ON_JOYSTICK_ELEMENT_HAT ? 0x01 : 0x09;
    value->usage = element == MUSE_ON_JOYSTICK_ELEMENT_HAT ? 0x39 : element;
    value->logical_minimum = 0;
    value->logical_maximum = element == MUSE_ON_JOYSTICK_ELEMENT_HAT ? 359 : 1;
    value->value = element == MUSE_ON_JOYSTICK_ELEMENT_HAT ? 0xffff : 0;
  }
  return fake;
}

static void test_profile_count_contract(void) {
  size_t expected_count = 0;

  assert(muse_on_capture_joystick_snapshot_capacity(
      MUSE_ON_PROFILE_CONTROLLER_ONLY, 6, &expected_count));
  assert(expected_count == 5);
  assert(muse_on_capture_joystick_snapshot_capacity(
      MUSE_ON_PROFILE_PEDAL, 6, &expected_count));
  assert(expected_count == 6);
  assert(!muse_on_capture_joystick_snapshot_capacity(
      MUSE_ON_PROFILE_CONTROLLER_ONLY, 4, &expected_count));
  assert(!muse_on_capture_joystick_snapshot_capacity(
      MUSE_ON_PROFILE_PEDAL, 5, &expected_count));
}

static void test_keyboard_current_report_preserves_boot_sizes(void) {
  static const uint8_t neutral_keyboard_boot[8] = {0};
  static const uint8_t neutral_keyboard_extended[32] = {0};
  uint8_t report[32] = {0};
  MuseOnDecoder decoder;
  FakeCurrentReport fake = {
      .bytes = neutral_keyboard_boot,
      .length = sizeof(neutral_keyboard_boot),
      .succeeds = true,
      .expected_report_id = 0,
  };

  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_probe_neutral_entry(
             &decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT,
             MUSE_ON_PROFILE_CONTROLLER_ONLY, 0, report, sizeof(report),
             read_current_report, &fake) == MUSE_ON_NEUTRAL_ENTRY_RELEASED);
  fake.bytes = neutral_keyboard_extended;
  fake.length = sizeof(neutral_keyboard_extended);
  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_probe_neutral_entry(
             &decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT,
             MUSE_ON_PROFILE_CONTROLLER_ONLY, 0, report, sizeof(report),
             read_current_report, &fake) == MUSE_ON_NEUTRAL_ENTRY_RELEASED);
}

static void assert_observation_rejected_without_decoder_mutation(
    MuseOnInterfaceKind interface_kind, uint8_t report_id,
    const uint8_t *report, size_t report_length) {
  MuseOnDecoder decoder;
  MuseOnDecoder before;
  MuseOnInputObservation observation;

  muse_on_decoder_init(&decoder);
  before = decoder;
  assert(!muse_on_capture_observe_report(
      &decoder, interface_kind, MUSE_ON_PROFILE_CONTROLLER_ONLY, report_id,
      report, report_length, &observation));
  assert(memcmp(&decoder, &before, sizeof(decoder)) == 0);
}

static void test_joystick_report_hat_domain_is_validated_before_decode(void) {
  uint8_t report[11] = {
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0xff, 0xff, 0x00, 0x00,
  };
  uint8_t prefixed[12] = {1};
  MuseOnDecoder decoder;
  MuseOnInputObservation observation;

  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_observe_report(
      &decoder, MUSE_ON_INTERFACE_JOYSTICK,
      MUSE_ON_PROFILE_CONTROLLER_ONLY, 1, report, sizeof(report),
      &observation));
  assert(observation.neutral_entry_state == MUSE_ON_NEUTRAL_ENTRY_RELEASED);

  report[7] = 90;
  report[8] = 0;
  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_observe_report(
      &decoder, MUSE_ON_INTERFACE_JOYSTICK,
      MUSE_ON_PROFILE_CONTROLLER_ONLY, 1, report, sizeof(report),
      &observation));
  assert(observation.neutral_entry_state == MUSE_ON_NEUTRAL_ENTRY_HELD);

  report[7] = 0x0e;
  report[8] = 0x01;
  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_observe_report(
      &decoder, MUSE_ON_INTERFACE_JOYSTICK,
      MUSE_ON_PROFILE_CONTROLLER_ONLY, 1, report, sizeof(report),
      &observation));
  assert(observation.neutral_entry_state == MUSE_ON_NEUTRAL_ENTRY_HELD);

  report[7] = 0x68;
  report[8] = 0x01;
  assert_observation_rejected_without_decoder_mutation(
      MUSE_ON_INTERFACE_JOYSTICK, 1, report, sizeof(report));
  memcpy(prefixed + 1, report, sizeof(report));
  assert_observation_rejected_without_decoder_mutation(
      MUSE_ON_INTERFACE_JOYSTICK, 1, prefixed, sizeof(prefixed));
  assert_observation_rejected_without_decoder_mutation(
      MUSE_ON_INTERFACE_JOYSTICK, 1, report, sizeof(report) - 1);
  prefixed[0] = 2;
  assert_observation_rejected_without_decoder_mutation(
      MUSE_ON_INTERFACE_JOYSTICK, 1, prefixed, sizeof(prefixed));
}

static void test_keyboard_boot_errors_are_validated_before_decode(void) {
  uint8_t boot[8] = {0};
  uint8_t extended[32] = {0};
  MuseOnDecoder decoder;
  MuseOnInputObservation observation;

  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_observe_report(
      &decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT,
      MUSE_ON_PROFILE_CONTROLLER_ONLY, 0, boot, sizeof(boot),
      &observation));
  assert(observation.neutral_entry_state == MUSE_ON_NEUTRAL_ENTRY_RELEASED);
  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_observe_report(
      &decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT,
      MUSE_ON_PROFILE_CONTROLLER_ONLY, 0, extended, sizeof(extended),
      &observation));
  assert(observation.neutral_entry_state == MUSE_ON_NEUTRAL_ENTRY_RELEASED);

  for (uint8_t usage = 0x01; usage <= 0x03; usage++) {
    for (size_t index = 2; index < sizeof(boot); index++) {
      boot[index] = usage;
      assert_observation_rejected_without_decoder_mutation(
          MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0, boot, sizeof(boot));
      boot[index] = 0;
    }
    for (size_t index = 2; index < sizeof(extended); index++) {
      extended[index] = usage;
      assert_observation_rejected_without_decoder_mutation(
          MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0, extended, sizeof(extended));
      extended[index] = 0;
    }
  }
  boot[1] = 1;
  assert_observation_rejected_without_decoder_mutation(
      MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0, boot, sizeof(boot));
  extended[1] = 1;
  assert_observation_rejected_without_decoder_mutation(
      MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0, extended, sizeof(extended));
}

static void test_ordinary_joystick_reader_proves_neutral(void) {
  FakeJoystickElements fake = neutral_joystick_elements(false);
  MuseOnDecoder decoder;

  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_probe_joystick_elements(
             &decoder, MUSE_ON_PROFILE_CONTROLLER_ONLY,
             read_joystick_element, &fake) == MUSE_ON_NEUTRAL_ENTRY_RELEASED);
  assert(fake.reads == 5);
  for (unsigned int index = 0; index < fake.reads; index++) {
    assert(fake.read_elements[index] == (MuseOnJoystickElement)index);
  }
}

static void test_current_value_neutral_is_valid(void) {
  FakeJoystickElements fake = neutral_joystick_elements(false);
  MuseOnDecoder decoder;

  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_probe_joystick_elements(
             &decoder, MUSE_ON_PROFILE_CONTROLLER_ONLY,
             read_joystick_element, &fake) == MUSE_ON_NEUTRAL_ENTRY_RELEASED);
}

static void test_held_and_partial_release_block_neutral_entry(void) {
  FakeJoystickElements fake = neutral_joystick_elements(false);
  MuseOnDecoder decoder;

  fake.values[MUSE_ON_JOYSTICK_ELEMENT_HAT].value = 0x005a;
  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_probe_joystick_elements(
             &decoder, MUSE_ON_PROFILE_CONTROLLER_ONLY,
             read_joystick_element, &fake) == MUSE_ON_NEUTRAL_ENTRY_HELD);

  fake = neutral_joystick_elements(false);
  fake.values[MUSE_ON_JOYSTICK_ELEMENT_HAT].value = 0x010e;
  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_probe_joystick_elements(
             &decoder, MUSE_ON_PROFILE_CONTROLLER_ONLY,
             read_joystick_element, &fake) == MUSE_ON_NEUTRAL_ENTRY_HELD);

  fake = neutral_joystick_elements(false);
  fake.values[MUSE_ON_JOYSTICK_ELEMENT_BUTTON4].value = 1;
  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_probe_joystick_elements(
             &decoder, MUSE_ON_PROFILE_CONTROLLER_ONLY,
             read_joystick_element, &fake) == MUSE_ON_NEUTRAL_ENTRY_HELD);

  fake = neutral_joystick_elements(true);
  fake.values[MUSE_ON_JOYSTICK_ELEMENT_BUTTON5].value = 1;
  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_probe_joystick_elements(
             &decoder, MUSE_ON_PROFILE_PEDAL,
             read_joystick_element, &fake) == MUSE_ON_NEUTRAL_ENTRY_HELD);

  fake = neutral_joystick_elements(false);
  fake.values[MUSE_ON_JOYSTICK_ELEMENT_BUTTON1].value = 1;
  muse_on_decoder_init(&decoder);
  assert(muse_on_capture_probe_joystick_elements(
             &decoder, MUSE_ON_PROFILE_CONTROLLER_ONLY,
             read_joystick_element, &fake) == MUSE_ON_NEUTRAL_ENTRY_HELD);
}

static void test_invalid_or_unavailable_elements_fail_closed(void) {
  FakeJoystickElements fake;
  MuseOnDecoder decoder;

#define ASSERT_UNKNOWN(mutator) \
  do { \
    fake = neutral_joystick_elements(false); \
    mutator; \
    muse_on_decoder_init(&decoder); \
    assert(muse_on_capture_probe_joystick_elements( \
               &decoder, MUSE_ON_PROFILE_CONTROLLER_ONLY, \
               read_joystick_element, &fake) == MUSE_ON_NEUTRAL_ENTRY_UNKNOWN); \
  } while (0)

  ASSERT_UNKNOWN(fake.available[MUSE_ON_JOYSTICK_ELEMENT_BUTTON4] = false);
  ASSERT_UNKNOWN(fake.values[MUSE_ON_JOYSTICK_ELEMENT_BUTTON4].element =
                     MUSE_ON_JOYSTICK_ELEMENT_BUTTON3);
  ASSERT_UNKNOWN(fake.values[MUSE_ON_JOYSTICK_ELEMENT_BUTTON1].is_absolute_input = false);
  ASSERT_UNKNOWN(fake.values[MUSE_ON_JOYSTICK_ELEMENT_BUTTON1].report_id = 2);
  ASSERT_UNKNOWN(fake.values[MUSE_ON_JOYSTICK_ELEMENT_BUTTON1].usage = 99);
  ASSERT_UNKNOWN(fake.values[MUSE_ON_JOYSTICK_ELEMENT_BUTTON1].logical_maximum = 2);
  ASSERT_UNKNOWN(fake.values[MUSE_ON_JOYSTICK_ELEMENT_BUTTON1].value = 2);
  ASSERT_UNKNOWN(fake.read_succeeds = false);
  ASSERT_UNKNOWN(fake.values[MUSE_ON_JOYSTICK_ELEMENT_HAT].value = 360);
  ASSERT_UNKNOWN(fake.values[MUSE_ON_JOYSTICK_ELEMENT_HAT].logical_maximum = 65535);

#undef ASSERT_UNKNOWN
}

static void test_hybrid_capture_options_are_interface_specific(void) {
  assert(muse_on_joystick_open_options(true) ==
         (uint32_t)kIOHIDOptionsTypeSeizeDevice);
  assert(muse_on_joystick_open_options(false) ==
         (uint32_t)kIOHIDOptionsTypeNone);
  assert(muse_on_keyboard_capture_options() ==
         (uint32_t)kIOHIDOptionsTypeNone);
}

static void test_hybrid_capture_requires_both_controls(void) {
  assert(muse_on_capture_is_verified(true, true, true));
  assert(!muse_on_capture_is_verified(false, true, true));
  assert(!muse_on_capture_is_verified(true, false, true));
  assert(!muse_on_capture_is_verified(true, true, false));
}

int main(void) {
  test_hybrid_capture_options_are_interface_specific();
  test_hybrid_capture_requires_both_controls();
  test_profile_count_contract();
  test_keyboard_current_report_preserves_boot_sizes();
  test_joystick_report_hat_domain_is_validated_before_decode();
  test_keyboard_boot_errors_are_validated_before_decode();
  test_ordinary_joystick_reader_proves_neutral();
  test_current_value_neutral_is_valid();
  test_held_and_partial_release_block_neutral_entry();
  test_invalid_or_unavailable_elements_fail_closed();
  return 0;
}
