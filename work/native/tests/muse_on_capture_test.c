#include "../muse_on_capture.h"

#include <assert.h>
#include <IOKit/hid/IOHIDManager.h>

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
  return 0;
}
