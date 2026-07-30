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
  p.filter_verified = true;
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

static void test_safety_failure_retains_priority_over_permission_state(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  p.permission_granted = false;
  p.safety_failure = MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_SAFETY_LATCH);
  assert(state.safety_failure == MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE);
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

static void test_listener_probe_remains_runtime_authority(void) {
  /* Responsible-app consent prepares TCC, but only the listener's runtime
   * probe decides whether capture may proceed. */
  assert(muse_on_should_launch_listener_probe(true, false, false));
  assert(!muse_on_should_launch_listener_probe(true, false, true));
  assert(!muse_on_should_launch_listener_probe(false, false, false));
  assert(!muse_on_should_launch_listener_probe(true, true, false));
}

static void test_responsible_app_requests_missing_permissions_only_on_consent(void) {
  const MuseOnPermissionGate both =
      MUSE_ON_PERMISSION_GATE_INPUT_MONITORING |
      MUSE_ON_PERMISSION_GATE_ACCESSIBILITY;
  MuseOnResponsiblePermissionPlan plan;

  plan = muse_on_responsible_permission_plan(
      MUSE_ON_PERMISSION_REQUEST_PASSIVE, true, false, false, false);
  assert(plan.request_gates == MUSE_ON_PERMISSION_GATE_NONE);
  assert(!plan.mark_handled);

  plan = muse_on_responsible_permission_plan(
      MUSE_ON_PERMISSION_REQUEST_FIRST_ENABLE, true, false, false, false);
  assert(plan.request_gates == both);
  assert(plan.mark_handled);

  plan = muse_on_responsible_permission_plan(
      MUSE_ON_PERMISSION_REQUEST_RETRY, true, false, false, false);
  assert(plan.request_gates == both);
  assert(plan.mark_handled);

  plan = muse_on_responsible_permission_plan(
      MUSE_ON_PERMISSION_REQUEST_RETRY, false, false, false, false);
  assert(plan.request_gates == MUSE_ON_PERMISSION_GATE_NONE);
  assert(!plan.mark_handled);

  plan = muse_on_responsible_permission_plan(
      MUSE_ON_PERMISSION_REQUEST_RETRY, true, true, false, false);
  assert(plan.request_gates == MUSE_ON_PERMISSION_GATE_NONE);
  assert(!plan.mark_handled);

  plan = muse_on_responsible_permission_plan(
      MUSE_ON_PERMISSION_REQUEST_RETRY, true, false, true, false);
  assert(plan.request_gates == MUSE_ON_PERMISSION_GATE_ACCESSIBILITY);
  assert(plan.mark_handled);

  plan = muse_on_responsible_permission_plan(
      MUSE_ON_PERMISSION_REQUEST_RETRY, true, false, false, true);
  assert(plan.request_gates == MUSE_ON_PERMISSION_GATE_INPUT_MONITORING);
  assert(plan.mark_handled);

  /* First Enable consumes the one request opportunity even when permissions
   * were already granted, so later revocation never causes a recurring prompt. */
  plan = muse_on_responsible_permission_plan(
      MUSE_ON_PERMISSION_REQUEST_FIRST_ENABLE, true, false, true, true);
  assert(plan.request_gates == MUSE_ON_PERMISSION_GATE_NONE);
  assert(plan.mark_handled);
}

static void test_retry_routes_once_after_authoritative_listener_event(void) {
  MuseOnPermissionGate missing = muse_on_listener_missing_permission_gates(
      "granted", false);

  assert(missing == MUSE_ON_PERMISSION_GATE_ACCESSIBILITY);
  assert(muse_on_should_open_retry_permission_settings(
      true, true, missing, false));
  assert(!muse_on_should_open_retry_permission_settings(
      true, true, missing, true));
  assert(!muse_on_should_open_retry_permission_settings(
      true, false, missing, false));

  missing = muse_on_listener_missing_permission_gates("denied", false);
  assert(missing == (MUSE_ON_PERMISSION_GATE_INPUT_MONITORING |
                     MUSE_ON_PERMISSION_GATE_ACCESSIBILITY));
  assert(muse_on_retry_permission_gate(
             (missing & MUSE_ON_PERMISSION_GATE_INPUT_MONITORING) == 0,
             (missing & MUSE_ON_PERMISSION_GATE_ACCESSIBILITY) == 0) ==
         MUSE_ON_PERMISSION_GATE_INPUT_MONITORING);
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

static void test_retry_guides_next_missing_permission(void) {
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

static void test_permission_recovery_requires_fresh_neutral_entry(void) {
  MuseOnState state;
  MuseOnPrerequisites p = {0};

  p.cleanup_verified = true;
  p.permission_granted = true;
  p.controller_connected = true;
  p.session_available = true;
  p.codex_foreground = true;
  p.filter_verified = true;
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
  test_safety_failure_retains_priority_over_permission_state();
  test_not_permitted_keyboard_manager_error_is_permission_routed();
  test_listener_probe_remains_runtime_authority();
  test_responsible_app_requests_missing_permissions_only_on_consent();
  test_retry_routes_once_after_authoritative_listener_event();
  test_missing_permission_gates_are_specific();
  test_retry_guides_next_missing_permission();
  test_permission_guidance_selects_exact_settings_destinations();
  test_permission_recovery_requires_fresh_neutral_entry();
  return 0;
}
