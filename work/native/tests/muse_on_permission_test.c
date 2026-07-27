#include <assert.h>
#include <stdint.h>

#include <IOKit/IOReturn.h>

#include "../muse_on_platform.h"
#include "../muse_on_state_coordinator.h"

static MuseOnPrerequisites all_clear(void) {
  MuseOnPrerequisites p = {0};
  p.cleanup_verified = true;
  p.permission_granted = true;
  p.controller_connected = true;
  p.session_available = true;
  p.codex_foreground = true;
  p.inputs_released = true;
  return p;
}

static void test_denied_input_monitoring_is_permission_required_and_blocked(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  p.permission_granted = false;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_PERMISSION);
  assert(state.enabled_intent == true);
  assert(state.effects.request_dispatch == false);
}

static void test_generic_keyboard_manager_error_remains_safety_latch(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  assert(!muse_on_listener_error_is_permission_required(
      "open_keyboard_manager", kIOReturnError));
  p.safety_failure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.safety_failure == MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
  assert(state.effects.request_dispatch == false);
}

static void test_not_permitted_keyboard_manager_error_is_permission_routed(void) {
  assert(muse_on_listener_error_is_permission_required(
      "open_keyboard_manager", kIOReturnNotPermitted));
  assert(muse_on_listener_error_is_permission_required(
      "open_joystick_manager", kIOReturnNotPermitted));
  assert(!muse_on_listener_error_is_permission_required(
      "open_keyboard_manager", kIOReturnError));
  assert(!muse_on_listener_error_is_permission_required(
      "input_report", kIOReturnNotPermitted));
}

static void test_input_monitoring_request_is_first_enable_only_and_once(void) {
  assert(muse_on_should_request_input_monitoring(true, true, false));
  assert(!muse_on_should_request_input_monitoring(true, true, true));
  assert(!muse_on_should_request_input_monitoring(false, true, false));
  assert(!muse_on_should_request_input_monitoring(false, false, false));
  /* Retry supplies checks only; it never enters the First Enable request path. */
  assert(!muse_on_should_request_input_monitoring(false, true, false));
}

int main(void) {
  test_denied_input_monitoring_is_permission_required_and_blocked();
  test_generic_keyboard_manager_error_remains_safety_latch();
  test_not_permitted_keyboard_manager_error_is_permission_routed();
  test_input_monitoring_request_is_first_enable_only_and_once();
  return 0;
}
