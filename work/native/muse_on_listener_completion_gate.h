#ifndef MUSE_ON_LISTENER_COMPLETION_GATE_H
#define MUSE_ON_LISTENER_COMPLETION_GATE_H

#include <stdbool.h>

typedef enum {
  MUSE_ON_LISTENER_TERMINATION_EXIT = 0,
  MUSE_ON_LISTENER_TERMINATION_SIGNAL,
  MUSE_ON_LISTENER_TERMINATION_UNKNOWN,
} MuseOnListenerTerminationReason;

typedef enum {
  MUSE_ON_LISTENER_COMPLETION_WAITING = 0,
  MUSE_ON_LISTENER_COMPLETION_ACCEPTED,
  MUSE_ON_LISTENER_COMPLETION_FAIL_CLOSED,
} MuseOnListenerCompletionResult;

typedef struct {
  bool stdout_eof_processed;
  bool termination_observed;
  bool finalized;
  int termination_status;
  MuseOnListenerTerminationReason termination_reason;
  MuseOnListenerCompletionResult result;
} MuseOnListenerCompletionGate;

static inline void muse_on_listener_completion_gate_init(
    MuseOnListenerCompletionGate *gate) {
  if (!gate) return;
  *gate = (MuseOnListenerCompletionGate){
      .termination_reason = MUSE_ON_LISTENER_TERMINATION_UNKNOWN,
      .result = MUSE_ON_LISTENER_COMPLETION_WAITING,
  };
}

static inline void muse_on_listener_completion_gate_mark_stdout_eof(
    MuseOnListenerCompletionGate *gate) {
  if (!gate || gate->finalized) return;
  gate->stdout_eof_processed = true;
}

static inline void muse_on_listener_completion_gate_mark_termination(
    MuseOnListenerCompletionGate *gate,
    MuseOnListenerTerminationReason reason, int status) {
  if (!gate || gate->finalized || gate->termination_observed) return;
  gate->termination_observed = true;
  gate->termination_reason = reason;
  gate->termination_status = status;
}

static inline MuseOnListenerCompletionResult
muse_on_listener_completion_gate_try_finalize(
    MuseOnListenerCompletionGate *gate, bool recovery_required,
    bool recovery_validated, bool filter_restored, bool stopped,
    bool cleanup_verified, bool safety_latched) {
  bool termination_ok;
  bool recovery_ok;
  bool cleanup_ok;

  if (!gate) return MUSE_ON_LISTENER_COMPLETION_FAIL_CLOSED;
  if (gate->finalized) return gate->result;
  if (!gate->stdout_eof_processed || !gate->termination_observed) {
    return MUSE_ON_LISTENER_COMPLETION_WAITING;
  }

  termination_ok = gate->termination_reason == MUSE_ON_LISTENER_TERMINATION_EXIT &&
                   gate->termination_status == 0;
  recovery_ok = !recovery_required || recovery_validated;
  cleanup_ok = filter_restored && stopped && cleanup_verified;
  gate->result = termination_ok && recovery_ok && cleanup_ok &&
                         !safety_latched
                     ? MUSE_ON_LISTENER_COMPLETION_ACCEPTED
                     : MUSE_ON_LISTENER_COMPLETION_FAIL_CLOSED;
  gate->finalized = true;
  return gate->result;
}

#endif
