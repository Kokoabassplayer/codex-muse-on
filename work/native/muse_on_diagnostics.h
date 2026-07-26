#ifndef MUSE_ON_DIAGNOSTICS_H
#define MUSE_ON_DIAGNOSTICS_H

#include <stddef.h>

#include "muse_on_action_map.h"
#include "muse_on_state_coordinator.h"

enum {
  MUSE_ON_DIAGNOSTIC_CAPACITY = 32,
  MUSE_ON_DIAGNOSTIC_ENTRY_SIZE = 96
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

typedef struct {
  char entries[MUSE_ON_DIAGNOSTIC_CAPACITY]
             [MUSE_ON_DIAGNOSTIC_ENTRY_SIZE];
  size_t next;
  size_t count;
} MuseOnDiagnostics;

void muse_on_diagnostics_init(MuseOnDiagnostics *diagnostics);
void muse_on_diagnostics_clear(MuseOnDiagnostics *diagnostics);
void muse_on_diagnostics_record_state(MuseOnDiagnostics *diagnostics,
                                       MuseOnStatus status,
                                       MuseOnInactiveReason reason,
                                       MuseOnSafetyFailure failure);
void muse_on_diagnostics_record_action(MuseOnDiagnostics *diagnostics,
                                        MuseOnActionId action);
void muse_on_diagnostics_record_error(MuseOnDiagnostics *diagnostics,
                                       MuseOnDiagnosticErrorCode error);
size_t muse_on_diagnostics_count(const MuseOnDiagnostics *diagnostics);
size_t muse_on_diagnostics_copy(const MuseOnDiagnostics *diagnostics,
                                char *output, size_t capacity);

const char *muse_on_diagnostic_error_string(MuseOnDiagnosticErrorCode error);

#endif
