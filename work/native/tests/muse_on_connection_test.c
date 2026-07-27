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
  MuseOnConnectionSnapshot snapshot =
      muse_on_classify_connections(partial, 1);
  assert(!muse_on_find_single_complete_controller(partial, 1, &location));
  assert(snapshot.state == MUSE_ON_CONNECTION_DISCONNECTED);
}

static void test_accepts_one_exact_pair_at_one_location(void) {
  MuseOnObservedInterface pair[] = {keyboard(0x110000), joystick(0x110000)};
  uint32_t location = 0;
  MuseOnConnectionSnapshot snapshot =
      muse_on_classify_connections(pair, 2);
  assert(muse_on_find_single_complete_controller(pair, 2, &location));
  assert(snapshot.state == MUSE_ON_CONNECTION_SINGLE);
  assert(snapshot.location_id == 0x110000);
  assert(location == 0x110000);
}

static void test_rejects_mismatched_locations_and_multiple_controllers(void) {
  MuseOnObservedInterface mismatched[] = {keyboard(1), joystick(2)};
  MuseOnObservedInterface multiple[] = {
      keyboard(1), joystick(1), keyboard(2), joystick(2)};
  MuseOnObservedInterface complete_plus_partial[] = {
      keyboard(1), joystick(1), keyboard(2)};
  uint32_t location = 0;
  MuseOnConnectionSnapshot multiple_snapshot =
      muse_on_classify_connections(multiple, 4);
  MuseOnConnectionSnapshot mismatch_snapshot =
      muse_on_classify_connections(mismatched, 2);
  assert(!muse_on_find_single_complete_controller(mismatched, 2, &location));
  assert(!muse_on_find_single_complete_controller(multiple, 4, &location));
  assert(!muse_on_find_single_complete_controller(complete_plus_partial, 3,
                                                   &location));
  assert(multiple_snapshot.state == MUSE_ON_CONNECTION_MULTIPLE);
  assert(mismatch_snapshot.state == MUSE_ON_CONNECTION_DISCONNECTED);
}

static void test_rejects_duplicate_interfaces(void) {
  MuseOnObservedInterface duplicate[] = {
      keyboard(1), keyboard(1), joystick(1)};
  MuseOnObservedInterface unknown[] = {
      keyboard(1), joystick(1), {0, 1, true}};
  MuseOnObservedInterface zero_location[] = {
      keyboard(0), joystick(0)};
  uint32_t location = 0;
  assert(!muse_on_find_single_complete_controller(duplicate, 3, &location));
  assert(muse_on_classify_connections(duplicate, 3).state ==
         MUSE_ON_CONNECTION_DISCONNECTED);
  assert(muse_on_classify_connections(unknown, 3).state ==
         MUSE_ON_CONNECTION_DISCONNECTED);
  assert(muse_on_classify_connections(zero_location, 2).state ==
         MUSE_ON_CONNECTION_DISCONNECTED);
}

int main(void) {
  test_requires_a_paired_keyboard_and_joystick();
  test_accepts_one_exact_pair_at_one_location();
  test_rejects_mismatched_locations_and_multiple_controllers();
  test_rejects_duplicate_interfaces();
  return 0;
}
