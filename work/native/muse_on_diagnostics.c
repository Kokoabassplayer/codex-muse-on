#include "muse_on_diagnostics.h"

#include <stdio.h>
#include <string.h>

static int32_t clamp_int64_to_int32(int64_t value) {
  if (value > INT32_MAX) return INT32_MAX;
  if (value < INT32_MIN) return INT32_MIN;
  return (int32_t)value;
}

static void copy_safe_operation(char *destination, size_t capacity,
                                const char *source) {
  size_t index;

  if (!destination || capacity == 0) return;
  if (!source || source[0] == '\0') source = "unknown";
  for (index = 0; index + 1 < capacity && source[index] != '\0'; index++) {
    char value = source[index];
    if (!((value >= 'a' && value <= 'z') ||
          (value >= 'A' && value <= 'Z') ||
          (value >= '0' && value <= '9') || value == '_' || value == '-')) {
      value = '_';
    }
    destination[index] = value;
  }
  destination[index] = '\0';
}

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

void muse_on_diagnostics_reset_listener(MuseOnDiagnostics *diagnostics) {
  if (!diagnostics) return;
  memset(&diagnostics->listener, 0, sizeof(diagnostics->listener));
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

void muse_on_diagnostics_record_listener_error(MuseOnDiagnostics *diagnostics,
                                                const char *operation,
                                                int64_t code) {
  if (!diagnostics) return;
  diagnostics->listener.has_error = true;
  copy_safe_operation(diagnostics->listener.error_operation,
                      sizeof(diagnostics->listener.error_operation),
                      operation);
  diagnostics->listener.error_code = clamp_int64_to_int32(code);
}

void muse_on_diagnostics_record_listener_termination(
    MuseOnDiagnostics *diagnostics,
    MuseOnDiagnosticTerminationReason reason,
    int64_t status,
    bool has_signal,
    int64_t signal) {
  if (!diagnostics) return;
  diagnostics->listener.has_termination = true;
  diagnostics->listener.termination_reason = reason;
  diagnostics->listener.termination_status = clamp_int64_to_int32(status);
  diagnostics->listener.has_termination_signal = has_signal;
  diagnostics->listener.termination_signal = clamp_int64_to_int32(signal);
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
  if (!diagnostics || (diagnostics->count == 0 &&
                       !diagnostics->listener.has_error &&
                       !diagnostics->listener.has_termination)) return 0;

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
  if (diagnostics->listener.has_error) {
    char detail[MUSE_ON_DIAGNOSTIC_ENTRY_SIZE];
    (void)snprintf(detail, sizeof(detail),
                   "listener_error operation=%s code=%d",
                   diagnostics->listener.error_operation,
                   diagnostics->listener.error_code);
    if (written > 0 && written < capacity - 1) output[written++] = '\n';
    if (written < capacity - 1) {
      size_t available = capacity - 1 - written;
      size_t length = strlen(detail);
      size_t copyLength = length < available ? length : available;
      memcpy(output + written, detail, copyLength);
      written += copyLength;
    }
  }
  if (diagnostics->listener.has_termination && written < capacity - 1) {
    char detail[MUSE_ON_DIAGNOSTIC_ENTRY_SIZE];
    const char *reason =
        muse_on_diagnostic_termination_string(
            diagnostics->listener.termination_reason);
    int detailLength;

    if (diagnostics->listener.has_termination_signal) {
      detailLength = snprintf(detail, sizeof(detail),
                              "listener_termination status=%d reason=%s "
                              "signal=%d",
                              diagnostics->listener.termination_status, reason,
                              diagnostics->listener.termination_signal);
    } else {
      detailLength = snprintf(detail, sizeof(detail),
                              "listener_termination status=%d reason=%s",
                              diagnostics->listener.termination_status, reason);
    }
    (void)detailLength;
    if (written > 0 && written < capacity - 1) output[written++] = '\n';
    if (written < capacity - 1) {
      size_t available = capacity - 1 - written;
      size_t length = strlen(detail);
      size_t copyLength = length < available ? length : available;
      memcpy(output + written, detail, copyLength);
      written += copyLength;
    }
  }
  output[written] = '\0';
  return written;
}

size_t muse_on_diagnostics_copy_listener_reason(
    const MuseOnDiagnostics *diagnostics,
    MuseOnSafetyFailure safety_failure,
    bool disable_pending,
    char *output,
    size_t capacity) {
  char detail[192];
  size_t length;
  int detailLength;

  if (!output || capacity == 0) return 0;
  output[0] = '\0';
  if (!diagnostics || disable_pending ||
      safety_failure != MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN ||
      (!diagnostics->listener.has_error &&
       !diagnostics->listener.has_termination)) {
    return 0;
  }

  if (diagnostics->listener.has_error) {
    detailLength = snprintf(detail, sizeof(detail),
                            "Listener startup failed at %s (IOReturn %d)",
                            diagnostics->listener.error_operation,
                            diagnostics->listener.error_code);
  } else {
    detailLength = snprintf(detail, sizeof(detail), "Listener exited");
  }
  if (diagnostics->listener.has_termination) {
    size_t offset = detailLength > 0 && (size_t)detailLength < sizeof(detail)
                        ? (size_t)detailLength
                        : sizeof(detail) - 1;
    if (diagnostics->listener.has_termination_signal) {
      (void)snprintf(detail + offset, sizeof(detail) - offset,
                     "; terminated by signal %d (status %d)",
                     diagnostics->listener.termination_signal,
                     diagnostics->listener.termination_status);
    } else {
      (void)snprintf(detail + offset, sizeof(detail) - offset,
                     "; exited status %d",
                     diagnostics->listener.termination_status);
    }
  }
  length = strlen(detail);
  if (length >= capacity) length = capacity - 1;
  memcpy(output, detail, length);
  output[length] = '\0';
  return length;
}

const char *muse_on_diagnostic_termination_string(
    MuseOnDiagnosticTerminationReason reason) {
  switch (reason) {
    case MUSE_ON_DIAGNOSTIC_TERMINATION_NONE: return "none";
    case MUSE_ON_DIAGNOSTIC_TERMINATION_EXIT: return "exit";
    case MUSE_ON_DIAGNOSTIC_TERMINATION_SIGNAL: return "signal";
    case MUSE_ON_DIAGNOSTIC_TERMINATION_UNKNOWN: return "unknown";
  }
  return "unknown";
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
