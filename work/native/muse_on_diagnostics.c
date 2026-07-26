#include "muse_on_diagnostics.h"

#include <stdio.h>
#include <string.h>

static void record_line(MuseOnDiagnostics *diagnostics, const char *format,
                        const char *first, const char *second,
                        const char *third) {
  size_t slot;

  if (!diagnostics || !format || !first) return;
  slot = diagnostics->next;
  (void)snprintf(diagnostics->entries[slot], MUSE_ON_DIAGNOSTIC_ENTRY_SIZE,
                 format, first, second ? second : "", third ? third : "");
  diagnostics->entries[slot][MUSE_ON_DIAGNOSTIC_ENTRY_SIZE - 1] = '\0';
  diagnostics->next = (diagnostics->next + 1) % MUSE_ON_DIAGNOSTIC_CAPACITY;
  if (diagnostics->count < MUSE_ON_DIAGNOSTIC_CAPACITY) {
    diagnostics->count++;
  }
}

void muse_on_diagnostics_init(MuseOnDiagnostics *diagnostics) {
  if (!diagnostics) return;
  memset(diagnostics, 0, sizeof(*diagnostics));
}

void muse_on_diagnostics_clear(MuseOnDiagnostics *diagnostics) {
  muse_on_diagnostics_init(diagnostics);
}

void muse_on_diagnostics_record_state(MuseOnDiagnostics *diagnostics,
                                       MuseOnStatus status,
                                       MuseOnInactiveReason reason,
                                       MuseOnSafetyFailure failure) {
  record_line(diagnostics, "state %s reason=%s safety=%s",
              muse_on_status_string(status),
              muse_on_inactive_reason_string(reason),
              muse_on_safety_failure_string(failure));
}

void muse_on_diagnostics_record_action(MuseOnDiagnostics *diagnostics,
                                        MuseOnActionId action) {
  record_line(diagnostics, "action %s", muse_on_action_id_string(action),
              NULL, NULL);
}

void muse_on_diagnostics_record_error(MuseOnDiagnostics *diagnostics,
                                       MuseOnDiagnosticErrorCode error) {
  record_line(diagnostics, "error %s",
              muse_on_diagnostic_error_string(error), NULL, NULL);
}

size_t muse_on_diagnostics_count(const MuseOnDiagnostics *diagnostics) {
  return diagnostics ? diagnostics->count : 0;
}

size_t muse_on_diagnostics_copy(const MuseOnDiagnostics *diagnostics,
                                char *output, size_t capacity) {
  size_t written = 0;
  size_t index;
  size_t oldest;

  if (!output || capacity == 0) return 0;
  output[0] = '\0';
  if (!diagnostics || diagnostics->count == 0) return 0;

  oldest = (diagnostics->next + MUSE_ON_DIAGNOSTIC_CAPACITY -
            diagnostics->count) % MUSE_ON_DIAGNOSTIC_CAPACITY;
  for (index = 0; index < diagnostics->count; index++) {
    const char *entry = diagnostics->entries[
        (oldest + index) % MUSE_ON_DIAGNOSTIC_CAPACITY];
    size_t entryLength = strlen(entry);
    size_t available = capacity - 1 - written;
    size_t copyLength = entryLength < available ? entryLength : available;

    if (copyLength > 0) {
      memcpy(output + written, entry, copyLength);
      written += copyLength;
    }
    if (index + 1 < diagnostics->count && written < capacity - 1) {
      output[written++] = '\n';
    }
    if (written == capacity - 1) break;
  }
  output[written] = '\0';
  return written;
}

const char *muse_on_diagnostic_error_string(MuseOnDiagnosticErrorCode error) {
  switch (error) {
    case MUSE_ON_DIAGNOSTIC_ERROR_NONE: return "none";
    case MUSE_ON_DIAGNOSTIC_ERROR_LISTENER_EXIT: return "listener_exit";
    case MUSE_ON_DIAGNOSTIC_ERROR_HOLD_RELEASE: return "hold_release";
    case MUSE_ON_DIAGNOSTIC_ERROR_FILTER_RESTORE: return "filter_restore";
    case MUSE_ON_DIAGNOSTIC_ERROR_FILTER_APPLY: return "filter_apply";
    case MUSE_ON_DIAGNOSTIC_ERROR_DEVICE_UNCERTAIN:
      return "device_uncertain";
    case MUSE_ON_DIAGNOSTIC_ERROR_UNCLEAN_EXIT: return "unclean_exit";
  }
  return "unknown";
}
