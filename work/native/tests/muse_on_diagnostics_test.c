#include <assert.h>
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
  muse_on_diagnostics_record_action(
      &diagnostics, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD);
  muse_on_diagnostics_record_error(
      &diagnostics, MUSE_ON_DIAGNOSTIC_ERROR_FILTER_RESTORE);
  assert(muse_on_diagnostics_copy(&diagnostics, output, sizeof(output)) > 0);
  assert(strstr(output, "state safety_latch") != NULL);
  assert(strstr(output, "pass_through_restoration_failed") != NULL);
  assert(strstr(output, "action globalDictationHold") != NULL);
  assert(strstr(output, "error filter_restore") != NULL);
  assert(strstr(output, "bytesHex") == NULL);
  assert(strstr(output, "typed") == NULL);
  assert(strstr(output, "chat") == NULL);
  assert(strstr(output, "window") == NULL);
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

int main(void) {
  test_diagnostics_are_bounded_and_keep_recent_entries();
  test_diagnostics_contain_only_safe_identifiers();
  test_diagnostics_copy_is_bounded_and_nul_terminated();
  return 0;
}
