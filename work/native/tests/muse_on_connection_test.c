#include <assert.h>

#include "../muse_on_connection.h"

static MuseOnObservedInterface keyboard(uint32_t location) {
  return (MuseOnObservedInterface){MUSE_ON_OBSERVED_KEYBOARD, location, true};
}

static MuseOnObservedInterface joystick(uint32_t location) {
  return (MuseOnObservedInterface){MUSE_ON_OBSERVED_JOYSTICK, location, true};
}

static void test_requires_a_paired_keyboard_and_joystick(void) {
  MuseOnObservedInterface partial[] = {keyboard(0x110000)};
  uint32_t location = 0;
  assert(!muse_on_find_single_complete_controller(partial, 1, &location));
}

static void test_accepts_one_exact_pair_at_one_location(void) {
  MuseOnObservedInterface pair[] = {keyboard(0x110000), joystick(0x110000)};
  uint32_t location = 0;
  assert(muse_on_find_single_complete_controller(pair, 2, &location));
  assert(location == 0x110000);
}

static void test_rejects_mismatched_locations_and_multiple_controllers(void) {
  MuseOnObservedInterface mismatched[] = {keyboard(1), joystick(2)};
  MuseOnObservedInterface multiple[] = {
      keyboard(1), joystick(1), keyboard(2), joystick(2)};
  MuseOnObservedInterface complete_plus_partial[] = {
      keyboard(1), joystick(1), keyboard(2)};
  uint32_t location = 0;
  assert(!muse_on_find_single_complete_controller(mismatched, 2, &location));
  assert(!muse_on_find_single_complete_controller(multiple, 4, &location));
  assert(!muse_on_find_single_complete_controller(complete_plus_partial, 3,
                                                   &location));
}

static void test_rejects_duplicate_interfaces(void) {
  MuseOnObservedInterface duplicate[] = {
      keyboard(1), keyboard(1), joystick(1)};
  uint32_t location = 0;
  assert(!muse_on_find_single_complete_controller(duplicate, 3, &location));
}

int main(void) {
  test_requires_a_paired_keyboard_and_joystick();
  test_accepts_one_exact_pair_at_one_location();
  test_rejects_mismatched_locations_and_multiple_controllers();
  test_rejects_duplicate_interfaces();
  return 0;
}
