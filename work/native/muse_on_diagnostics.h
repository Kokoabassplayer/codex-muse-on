#ifndef MUSE_ON_DIAGNOSTICS_H
#define MUSE_ON_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "muse_on_action_map.h"
#include "muse_on_state_coordinator.h"

enum {
  MUSE_ON_DIAGNOSTIC_CAPACITY = 32,
  MUSE_ON_DIAGNOSTIC_ENTRY_SIZE = 96,
  MUSE_ON_DIAGNOSTIC_OPERATION_SIZE = 48
};

typedef enum {
  MUSE_ON_DIAGNOSTIC_ERROR_NONE = 0,
  MUSE_ON_DIAGNOSTIC_ERROR_LISTENER_EXIT,
  MUSE_ON_DIAGNOSTIC_ERROR_HOLD_RELEASE,
  MUSE_ON_DIAGNOSTIC_ERROR_FILTER_RESTORE,
  MUSE_ON_DIAGNOSTIC_ERROR_FILTER_APPLY,
  MUSE_ON_DIAGNOSTIC_ERROR_DEVICE_UNCERTAIN,
  MUSE_ON_DIAGNOSTIC_ERROR_UNCLEAN_EXIT
} MuseOnDiagnosticErrorCode;

typedef enum {
  MUSE_ON_DIAGNOSTIC_TERMINATION_NONE = 0,
  MUSE_ON_DIAGNOSTIC_TERMINATION_EXIT,
  MUSE_ON_DIAGNOSTIC_TERMINATION_SIGNAL,
  MUSE_ON_DIAGNOSTIC_TERMINATION_UNKNOWN
} MuseOnDiagnosticTerminationReason;

typedef enum {
  MUSE_ON_DIAGNOSTIC_ACTION_DISPATCHED = 0,
  MUSE_ON_DIAGNOSTIC_ACTION_FAILED,
  MUSE_ON_DIAGNOSTIC_ACTION_BLOCKED
} MuseOnDiagnosticActionOutcome;

typedef struct {
  bool has_error;
  char error_operation[MUSE_ON_DIAGNOSTIC_OPERATION_SIZE];
  int32_t error_code;
  bool has_termination;
  MuseOnDiagnosticTerminationReason termination_reason;
  int32_t termination_status;
  bool has_termination_signal;
  int32_t termination_signal;
} MuseOnListenerDiagnostic;

typedef struct {
  char entries[MUSE_ON_DIAGNOSTIC_CAPACITY]
             [MUSE_ON_DIAGNOSTIC_ENTRY_SIZE];
  size_t next;
  size_t count;
  MuseOnListenerDiagnostic listener;
} MuseOnDiagnostics;

void muse_on_diagnostics_init(MuseOnDiagnostics *diagnostics);
void muse_on_diagnostics_clear(MuseOnDiagnostics *diagnostics);
void muse_on_diagnostics_record_state(MuseOnDiagnostics *diagnostics,
                                       MuseOnStatus status,
                                       MuseOnInactiveReason reason,
                                       MuseOnSafetyFailure failure);
bool muse_on_diagnostic_action_outcome_from_event(
    const char *event, MuseOnDiagnosticActionOutcome *outcome);
bool muse_on_diagnostics_record_action(MuseOnDiagnostics *diagnostics,
                                       MuseOnActionId action,
                                       MuseOnActionPhase phase,
                                       MuseOnDiagnosticActionOutcome outcome);
void muse_on_diagnostics_record_error(MuseOnDiagnostics *diagnostics,
                                       MuseOnDiagnosticErrorCode error);
void muse_on_diagnostics_reset_listener(MuseOnDiagnostics *diagnostics);
void muse_on_diagnostics_record_listener_error(MuseOnDiagnostics *diagnostics,
                                                const char *operation,
                                                int64_t code);
void muse_on_diagnostics_record_listener_ready(MuseOnDiagnostics *diagnostics);
void muse_on_diagnostics_record_listener_termination(
    MuseOnDiagnostics *diagnostics,
    MuseOnDiagnosticTerminationReason reason,
    int64_t status,
    bool has_signal,
    int64_t signal);
void muse_on_diagnostics_clear_listener_if_recovered(
    MuseOnDiagnostics *diagnostics,
    bool recovery_validated,
    bool cleanup_verified,
    bool termination_succeeded,
    bool safety_latched,
    MuseOnSafetyFailure safety_failure);
size_t muse_on_diagnostics_count(const MuseOnDiagnostics *diagnostics);
size_t muse_on_diagnostics_copy(const MuseOnDiagnostics *diagnostics,
                                char *output, size_t capacity);
size_t muse_on_diagnostics_copy_listener_reason(
    const MuseOnDiagnostics *diagnostics,
    MuseOnSafetyFailure safety_failure,
    bool disable_pending,
    char *output,
    size_t capacity);

const char *muse_on_diagnostic_error_string(MuseOnDiagnosticErrorCode error);
const char *muse_on_diagnostic_termination_string(
    MuseOnDiagnosticTerminationReason reason);

#endif
