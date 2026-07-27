#include <assert.h>
#include <stdint.h>
#include <string.h>

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

static void test_missing_permission_gates_are_specific(void) {
  assert(muse_on_missing_permission_gates(true, true) ==
         MUSE_ON_PERMISSION_GATE_NONE);
  assert(muse_on_missing_permission_gates(false, true) ==
         MUSE_ON_PERMISSION_GATE_INPUT_MONITORING);
  assert(muse_on_missing_permission_gates(true, false) ==
         MUSE_ON_PERMISSION_GATE_ACCESSIBILITY);
  assert(muse_on_missing_permission_gates(false, false) ==
         (MUSE_ON_PERMISSION_GATE_INPUT_MONITORING |
          MUSE_ON_PERMISSION_GATE_ACCESSIBILITY));

  assert(strcmp(muse_on_permission_guidance(
                   MUSE_ON_PERMISSION_GATE_INPUT_MONITORING),
               "Permission required — enable Input Monitoring.") == 0);
  assert(strcmp(muse_on_permission_guidance(
                   MUSE_ON_PERMISSION_GATE_ACCESSIBILITY),
               "Permission required — enable Accessibility.") == 0);
  assert(strcmp(muse_on_permission_guidance(
                   MUSE_ON_PERMISSION_GATE_INPUT_MONITORING |
                   MUSE_ON_PERMISSION_GATE_ACCESSIBILITY),
               "Permission required — enable Input Monitoring and Accessibility.") == 0);
}

static void test_retry_guides_next_missing_permission_without_requests(void) {
  assert(muse_on_retry_permission_gate(false, false) ==
         MUSE_ON_PERMISSION_GATE_INPUT_MONITORING);
  assert(muse_on_retry_permission_gate(false, true) ==
         MUSE_ON_PERMISSION_GATE_INPUT_MONITORING);
  assert(muse_on_retry_permission_gate(true, false) ==
         MUSE_ON_PERMISSION_GATE_ACCESSIBILITY);
  assert(muse_on_retry_permission_gate(true, true) ==
         MUSE_ON_PERMISSION_GATE_NONE);
}

static void test_permission_guidance_selects_exact_settings_destinations(void) {
  assert(strcmp(muse_on_permission_gate_settings_url(
                   MUSE_ON_PERMISSION_GATE_INPUT_MONITORING),
               "x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent") == 0);
  assert(strcmp(muse_on_permission_gate_settings_url(
                   MUSE_ON_PERMISSION_GATE_ACCESSIBILITY),
               "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility") == 0);
  assert(strcmp(muse_on_permission_gate_settings_url(
                   MUSE_ON_PERMISSION_GATE_NONE),
               muse_on_permission_fallback_settings_url()) == 0);
}

static void test_permission_requests_require_first_enable_and_are_once_only(void) {
  MuseOnPermissionGate gates[] = {
      MUSE_ON_PERMISSION_GATE_INPUT_MONITORING,
      MUSE_ON_PERMISSION_GATE_ACCESSIBILITY,
  };

  for (size_t index = 0; index < sizeof(gates) / sizeof(gates[0]); index++) {
    assert(muse_on_should_request_permission(gates[index], true, true, false));
    assert(!muse_on_should_request_permission(gates[index], false, true, false));
    assert(!muse_on_should_request_permission(gates[index], true, true, true));
    assert(!muse_on_should_request_permission(gates[index], true, false, false));
  }
  assert(!muse_on_should_request_permission(MUSE_ON_PERMISSION_GATE_NONE,
                                            true, true, false));
}

static void test_permission_recovery_requires_fresh_neutral_entry(void) {
  MuseOnState state;
  MuseOnPrerequisites p = {0};

  p.cleanup_verified = true;
  p.permission_granted = true;
  p.controller_connected = true;
  p.session_available = true;
  p.codex_foreground = true;
  p.inputs_released = true;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);

  /* Permission denial is an ordinary gate, not a safety latch. */
  p.permission_granted = false;
  p.inputs_released = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_PERMISSION);
  assert(state.effects.request_dispatch == false);

  /* Permission recovery and listener startup still lack fresh Neutral Entry. */
  p.permission_granted = true;
  muse_on_neutral_entry_require(&p);
  assert(p.inputs_released == false);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(state.effects.request_dispatch == false);

  /* Only the listener's fresh neutral-entry report enables dispatch. */
  p.inputs_released = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.effects.request_dispatch == true);
}

int main(void) {
  test_denied_input_monitoring_is_permission_required_and_blocked();
  test_generic_keyboard_manager_error_remains_safety_latch();
  test_not_permitted_keyboard_manager_error_is_permission_routed();
  test_input_monitoring_request_is_first_enable_only_and_once();
  test_missing_permission_gates_are_specific();
  test_retry_guides_next_missing_permission_without_requests();
  test_permission_guidance_selects_exact_settings_destinations();
  test_permission_requests_require_first_enable_and_are_once_only();
  test_permission_recovery_requires_fresh_neutral_entry();
  return 0;
}
