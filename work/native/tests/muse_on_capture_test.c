#include <assert.h>
#include <stdint.h>

#include <IOKit/IOReturn.h>

#include "../muse_on_capture.h"

static void test_capture_uses_exclusive_hid_access(void) {
  assert(muse_on_capture_open_options() == UINT32_C(0x01));
}

static void test_capture_requires_both_interfaces_and_one_controller(void) {
  assert(muse_on_capture_is_verified(true, true, true));
  assert(!muse_on_capture_is_verified(false, true, true));
  assert(!muse_on_capture_is_verified(true, false, true));
  assert(!muse_on_capture_is_verified(true, true, false));
}

static void test_capture_release_accepts_only_known_closed_states(void) {
  assert(muse_on_capture_close_result_is_acceptable(kIOReturnSuccess));
  assert(muse_on_capture_close_result_is_acceptable(kIOReturnNotOpen));
  assert(muse_on_capture_close_result_is_acceptable(kIOReturnNoDevice));
  assert(muse_on_capture_close_result_is_acceptable(kIOReturnOffline));
  assert(!muse_on_capture_close_result_is_acceptable(kIOReturnError));
}

int main(void) {
  test_capture_uses_exclusive_hid_access();
  test_capture_requires_both_interfaces_and_one_controller();
  test_capture_release_accepts_only_known_closed_states();
  return 0;
}
