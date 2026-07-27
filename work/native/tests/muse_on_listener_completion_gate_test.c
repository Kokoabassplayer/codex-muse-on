#include <assert.h>

#include "../muse_on_listener_completion_gate.h"

static MuseOnListenerCompletionGate good_gate(void) {
  MuseOnListenerCompletionGate gate;
  muse_on_listener_completion_gate_init(&gate);
  return gate;
}

static MuseOnListenerCompletionResult finalize(
    MuseOnListenerCompletionGate *gate, bool recovery_validated,
    bool stopped, bool cleanup_verified) {
  return muse_on_listener_completion_gate_try_finalize(
      gate, true, recovery_validated, true, stopped, cleanup_verified, false);
}

static void test_termination_first_waits_for_stdout_eof(void) {
  MuseOnListenerCompletionGate gate = good_gate();

  muse_on_listener_completion_gate_mark_termination(
      &gate, MUSE_ON_LISTENER_TERMINATION_EXIT, 0);
  assert(finalize(&gate, true, true, true) ==
         MUSE_ON_LISTENER_COMPLETION_WAITING);
  muse_on_listener_completion_gate_mark_stdout_eof(&gate);
  assert(finalize(&gate, true, true, true) ==
         MUSE_ON_LISTENER_COMPLETION_ACCEPTED);
}

static void test_eof_first_matches_termination_first(void) {
  MuseOnListenerCompletionGate gate = good_gate();

  muse_on_listener_completion_gate_mark_stdout_eof(&gate);
  assert(finalize(&gate, true, true, true) ==
         MUSE_ON_LISTENER_COMPLETION_WAITING);
  muse_on_listener_completion_gate_mark_termination(
      &gate, MUSE_ON_LISTENER_TERMINATION_EXIT, 0);
  assert(finalize(&gate, true, true, true) ==
         MUSE_ON_LISTENER_COMPLETION_ACCEPTED);
}

static void test_invalid_recovery_and_cleanup_fail_closed(void) {
  MuseOnListenerCompletionGate gate = good_gate();

  muse_on_listener_completion_gate_mark_stdout_eof(&gate);
  muse_on_listener_completion_gate_mark_termination(
      &gate, MUSE_ON_LISTENER_TERMINATION_EXIT, 0);
  assert(finalize(&gate, false, true, true) ==
         MUSE_ON_LISTENER_COMPLETION_FAIL_CLOSED);

  gate = good_gate();
  muse_on_listener_completion_gate_mark_stdout_eof(&gate);
  muse_on_listener_completion_gate_mark_termination(
      &gate, MUSE_ON_LISTENER_TERMINATION_EXIT, 0);
  assert(finalize(&gate, true, false, true) ==
         MUSE_ON_LISTENER_COMPLETION_FAIL_CLOSED);

  gate = good_gate();
  muse_on_listener_completion_gate_mark_stdout_eof(&gate);
  muse_on_listener_completion_gate_mark_termination(
      &gate, MUSE_ON_LISTENER_TERMINATION_EXIT, 0);
  assert(muse_on_listener_completion_gate_try_finalize(
             &gate, true, true, false, true, true, false) ==
         MUSE_ON_LISTENER_COMPLETION_FAIL_CLOSED);
}

static void test_failed_termination_fail_closes(void) {
  MuseOnListenerCompletionGate gate = good_gate();

  muse_on_listener_completion_gate_mark_stdout_eof(&gate);
  muse_on_listener_completion_gate_mark_termination(
      &gate, MUSE_ON_LISTENER_TERMINATION_SIGNAL, 0);
  assert(finalize(&gate, true, true, true) ==
         MUSE_ON_LISTENER_COMPLETION_FAIL_CLOSED);
}

static void test_duplicate_signals_finalize_once(void) {
  MuseOnListenerCompletionGate gate = good_gate();

  muse_on_listener_completion_gate_mark_termination(
      &gate, MUSE_ON_LISTENER_TERMINATION_EXIT, 0);
  muse_on_listener_completion_gate_mark_termination(
      &gate, MUSE_ON_LISTENER_TERMINATION_SIGNAL, 1);
  muse_on_listener_completion_gate_mark_stdout_eof(&gate);
  muse_on_listener_completion_gate_mark_stdout_eof(&gate);
  assert(finalize(&gate, true, true, true) ==
         MUSE_ON_LISTENER_COMPLETION_ACCEPTED);
  assert(finalize(&gate, false, false, false) ==
         MUSE_ON_LISTENER_COMPLETION_ACCEPTED);
  assert(gate.termination_status == 0);
  assert(gate.termination_reason == MUSE_ON_LISTENER_TERMINATION_EXIT);
}

int main(void) {
  test_termination_first_waits_for_stdout_eof();
  test_eof_first_matches_termination_first();
  test_invalid_recovery_and_cleanup_fail_closed();
  test_failed_termination_fail_closes();
  test_duplicate_signals_finalize_once();
  return 0;
}
