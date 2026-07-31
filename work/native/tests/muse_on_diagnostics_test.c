#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../muse_on_diagnostics.h"

static void test_diagnostics_are_bounded_and_keep_recent_entries(void) {
  MuseOnDiagnostics diagnostics;
  char output[4096];
  size_t index;

  muse_on_diagnostics_init(&diagnostics);
  for (index = 0; index < MUSE_ON_DIAGNOSTIC_CAPACITY; index++) {
    muse_on_diagnostics_record_error(
        &diagnostics, MUSE_ON_DIAGNOSTIC_ERROR_FILTER_APPLY);
  }
  for (index = 0; index < MUSE_ON_DIAGNOSTIC_CAPACITY; index++) {
    muse_on_diagnostics_record_error(
        &diagnostics, MUSE_ON_DIAGNOSTIC_ERROR_LISTENER_EXIT);
  }
  assert(muse_on_diagnostics_count(&diagnostics) ==
         MUSE_ON_DIAGNOSTIC_CAPACITY);
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) > 0);
  assert(strstr(output, "error listener_exit") != NULL);
  assert(strstr(output, "error filter_apply") == NULL);
}

static void test_diagnostics_contain_only_safe_identifiers(void) {
  MuseOnDiagnostics diagnostics;
  char output[4096];

  muse_on_diagnostics_init(&diagnostics);
  muse_on_diagnostics_record_state(
      &diagnostics, MUSE_ON_STATUS_SAFETY_LATCH,
      MUSE_ON_INACTIVE_REASON_SAFETY_LATCH,
      MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE);
  assert(muse_on_diagnostics_record_action(
      &diagnostics, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
      MUSE_ON_ACTION_BEGIN, MUSE_ON_DIAGNOSTIC_ACTION_DISPATCHED));
  muse_on_diagnostics_record_error(
      &diagnostics, MUSE_ON_DIAGNOSTIC_ERROR_FILTER_RESTORE);
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) > 0);
  assert(strstr(output, "state safety_latch") != NULL);
  assert(strstr(output, "pass_through_restoration_failed") != NULL);
  assert(strstr(output,
                "action globalDictationHold phase=begin outcome=dispatched") !=
         NULL);
  assert(strstr(output, "error filter_restore") != NULL);
  assert(strstr(output, "bytesHex") == NULL);
  assert(strstr(output, "typed") == NULL);
  assert(strstr(output, "chat") == NULL);
  assert(strstr(output, "window") == NULL);
}

static void test_action_diagnostics_distinguish_hold_phase_and_outcome(void) {
  MuseOnDiagnostics diagnostics;
  char output[4096];

  muse_on_diagnostics_init(&diagnostics);
  assert(muse_on_diagnostics_record_action(
      &diagnostics, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
      MUSE_ON_ACTION_BEGIN, MUSE_ON_DIAGNOSTIC_ACTION_DISPATCHED));
  assert(muse_on_diagnostics_record_action(
      &diagnostics, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
      MUSE_ON_ACTION_END, MUSE_ON_DIAGNOSTIC_ACTION_DISPATCHED));
  assert(muse_on_diagnostics_record_action(
      &diagnostics, MUSE_ON_ACTION_COMPOSER_SUBMIT,
      MUSE_ON_ACTION_TRIGGER, MUSE_ON_DIAGNOSTIC_ACTION_BLOCKED));
  assert(!muse_on_diagnostics_record_action(
      &diagnostics, MUSE_ON_ACTION_COMPOSER_SUBMIT,
      MUSE_ON_ACTION_END, MUSE_ON_DIAGNOSTIC_ACTION_DISPATCHED));
  assert(!muse_on_diagnostics_record_action(
      &diagnostics, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
      MUSE_ON_ACTION_TRIGGER, MUSE_ON_DIAGNOSTIC_ACTION_DISPATCHED));
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) > 0);
  assert(strstr(output,
                "action globalDictationHold phase=begin outcome=dispatched") !=
         NULL);
  assert(strstr(output,
                "action globalDictationHold phase=end outcome=dispatched") !=
         NULL);
  assert(strstr(output,
                "action composer.submit phase=trigger outcome=blocked") !=
         NULL);
}

static void test_action_diagnostics_parse_protocol_events_once(void) {
  MuseOnDiagnosticActionOutcome outcome;

  assert(muse_on_diagnostic_action_outcome_from_event(
      "action_dispatched", &outcome));
  assert(outcome == MUSE_ON_DIAGNOSTIC_ACTION_DISPATCHED);
  assert(muse_on_diagnostic_action_outcome_from_event(
      "action_dispatch_failed", &outcome));
  assert(outcome == MUSE_ON_DIAGNOSTIC_ACTION_FAILED);
  assert(muse_on_diagnostic_action_outcome_from_event(
      "action_blocked", &outcome));
  assert(outcome == MUSE_ON_DIAGNOSTIC_ACTION_BLOCKED);
  assert(!muse_on_diagnostic_action_outcome_from_event("action_unknown",
                                                        &outcome));
}

static void test_diagnostics_copy_is_bounded_and_nul_terminated(void) {
  MuseOnDiagnostics diagnostics;
  char output[12];
  size_t written;

  muse_on_diagnostics_init(&diagnostics);
  muse_on_diagnostics_record_state(
      &diagnostics, MUSE_ON_STATUS_ACTIVE, MUSE_ON_INACTIVE_REASON_NONE,
      MUSE_ON_SAFETY_FAILURE_NONE);
  written = muse_on_diagnostics_copy(&diagnostics, output, sizeof(output));
  assert(written < sizeof(output));
  assert(output[written] == '\0');
}

static void test_listener_startup_details_preserve_known_and_unknown_errors(void) {
  MuseOnDiagnostics diagnostics;
  char output[4096];

  muse_on_diagnostics_init(&diagnostics);
  muse_on_diagnostics_record_listener_error(
      &diagnostics, "create_hid_managers", -536870203);
  assert(diagnostics.listener.has_error);
  assert(strcmp(diagnostics.listener.error_operation,
                "create_hid_managers") == 0);
  assert(diagnostics.listener.error_code == -536870203);
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) > 0);
  assert(strstr(output, "listener_error operation=create_hid_managers "
                        "code=-536870203") != NULL);

  muse_on_diagnostics_record_listener_error(
      &diagnostics, "unknown_startup_gate", 42);
  assert(strcmp(diagnostics.listener.error_operation,
                "unknown_startup_gate") == 0);
  assert(diagnostics.listener.error_code == 42);
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) > 0);
  assert(strstr(output, "listener_error operation=unknown_startup_gate "
                        "code=42") != NULL);
}

static void test_listener_error_survives_ready_and_failed_recovery(void) {
  MuseOnDiagnostics diagnostics;
  char output[4096];

  muse_on_diagnostics_init(&diagnostics);
  muse_on_diagnostics_record_listener_error(
      &diagnostics, "apply_keyboard_filter", -536870203);

  muse_on_diagnostics_record_listener_ready(&diagnostics);
  assert(diagnostics.listener.has_error);
  assert(strcmp(diagnostics.listener.error_operation,
                "apply_keyboard_filter") == 0);
  assert(diagnostics.listener.error_code == -536870203);

  muse_on_diagnostics_record_listener_termination(
      &diagnostics, MUSE_ON_DIAGNOSTIC_TERMINATION_EXIT, 1, false, 0);
  muse_on_diagnostics_clear_listener_if_recovered(
      &diagnostics, true, false, false, true,
      MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) > 0);
  assert(strstr(output, "listener_error operation=apply_keyboard_filter "
                        "code=-536870203") != NULL);
  assert(strstr(output, "listener_termination status=1 reason=exit") != NULL);

  muse_on_diagnostics_clear_listener_if_recovered(
      &diagnostics, true, true, true, false, MUSE_ON_SAFETY_FAILURE_NONE);
  assert(!diagnostics.listener.has_error);
  assert(!diagnostics.listener.has_termination);
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) == 0);
}

static void test_listener_details_are_bounded_and_reset_without_losing_history(void) {
  MuseOnDiagnostics diagnostics;
  char output[4096];
  char reason[192];

  muse_on_diagnostics_init(&diagnostics);
  muse_on_diagnostics_record_state(
      &diagnostics, MUSE_ON_STATUS_SAFETY_LATCH,
      MUSE_ON_INACTIVE_REASON_SAFETY_LATCH,
      MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
  assert(muse_on_diagnostics_record_action(
      &diagnostics, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
      MUSE_ON_ACTION_END, MUSE_ON_DIAGNOSTIC_ACTION_FAILED));
  muse_on_diagnostics_record_listener_error(
      &diagnostics, "/private/user/controller", INT64_MAX);
  muse_on_diagnostics_record_listener_termination(
      &diagnostics, MUSE_ON_DIAGNOSTIC_TERMINATION_SIGNAL, INT64_MIN, true,
      INT64_MAX);
  assert(strlen(diagnostics.listener.error_operation) <
         MUSE_ON_DIAGNOSTIC_OPERATION_SIZE);
  assert(strchr(diagnostics.listener.error_operation, '/') == NULL);
  assert(diagnostics.listener.error_code == INT32_MAX);
  assert(diagnostics.listener.termination_status == INT32_MIN);
  assert(diagnostics.listener.termination_signal == INT32_MAX);
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) > 0);
  assert(strstr(output, "listener_termination status=-2147483648 "
                        "reason=signal signal=2147483647") != NULL);
  assert(muse_on_diagnostics_copy_listener_reason(
             &diagnostics, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN, false,
             reason, sizeof(reason)) > 0);
  assert(strstr(reason, "Listener startup failed at _private_user_controller") !=
         NULL);
  assert(strstr(reason, "terminated by signal 2147483647") != NULL);
  assert(muse_on_diagnostics_copy_listener_reason(
             &diagnostics, MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE, false,
             reason, sizeof(reason)) == 0);
  assert(muse_on_diagnostics_copy_listener_reason(
             &diagnostics, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN, true,
             reason, sizeof(reason)) == 0);

  muse_on_diagnostics_reset_listener(&diagnostics);
  assert(!diagnostics.listener.has_error);
  assert(!diagnostics.listener.has_termination);
  assert(muse_on_diagnostics_count(&diagnostics) == 2);
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) > 0);
  assert(strstr(output, "state safety_latch") != NULL);
  assert(strstr(output,
                "action globalDictationHold phase=end outcome=failed") != NULL);
  assert(strstr(output, "listener_error") == NULL);
  assert(strstr(output, "listener_termination") == NULL);
  assert(muse_on_diagnostics_copy_listener_reason(
             &diagnostics, MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN, false,
             reason, sizeof(reason)) == 0);
}

int main(void) {
  test_diagnostics_are_bounded_and_keep_recent_entries();
  test_diagnostics_contain_only_safe_identifiers();
  test_action_diagnostics_distinguish_hold_phase_and_outcome();
  test_action_diagnostics_parse_protocol_events_once();
  test_diagnostics_copy_is_bounded_and_nul_terminated();
  test_listener_startup_details_preserve_known_and_unknown_errors();
  test_listener_error_survives_ready_and_failed_recovery();
  test_listener_details_are_bounded_and_reset_without_losing_history();
  return 0;
}
