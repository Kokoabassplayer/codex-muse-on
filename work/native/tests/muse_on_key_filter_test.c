#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "../muse_on_key_filter.h"

static void test_sink_mapping_table_is_exact_and_valid(void) {
  static const uint64_t expected_sources[] = {
      UINT64_C(0x700000027), UINT64_C(0x700000026),
      UINT64_C(0x700000050), UINT64_C(0x70000004f),
      UINT64_C(0x70000003d), UINT64_C(0x70000003c),
      UINT64_C(0x70000001e), UINT64_C(0x70000001f),
      UINT64_C(0x700000020), UINT64_C(0x7000000e1),
  };
  size_t index;

  assert(muse_on_key_filter_mapping_table_is_valid());
  assert(muse_on_key_filter_mapping_count ==
         sizeof(expected_sources) / sizeof(expected_sources[0]));
  for (index = 0; index < muse_on_key_filter_mapping_count; ++index) {
    assert(muse_on_key_filter_mappings[index].source == expected_sources[index]);
    assert(muse_on_key_filter_mappings[index].destination ==
           UINT64_C(0x700000000));
  }
}

static void test_only_the_exact_muse_on_keyboard_service_matches(void) {
  assert(!muse_on_key_filter_service_matches(0x04b4, 0xe106, 0x01, 0x06,
                                             UINT64_C(0x1234), 0));
  assert(!muse_on_key_filter_service_matches(0x04b4, 0xe106, 0x01, 0x06,
                                             0, UINT64_C(0x1234)));
  assert(muse_on_key_filter_service_matches(0x04b4, 0xe106, 0x01, 0x06,
                                            UINT64_C(0x1234),
                                            UINT64_C(0x1234)));
  assert(!muse_on_key_filter_service_matches(
      0x04b4, 0xe106, 0x01, 0x06, UINT64_C(0x100000001),
      UINT64_C(0x200000001)));
  assert(muse_on_key_filter_service_matches(
      0x04b4, 0xe106, 0x01, 0x06, UINT64_C(0x100000001),
      UINT64_C(0x100000001)));
  assert(!muse_on_key_filter_service_matches(0x04b5, 0xe106, 0x01, 0x06,
                                             UINT64_C(0x1234), 0));
  assert(!muse_on_key_filter_service_matches(0x04b4, 0xe107, 0x01, 0x06,
                                             UINT64_C(0x1234), 0));
  assert(!muse_on_key_filter_service_matches(0x04b4, 0xe106, 0x01, 0x04,
                                             UINT64_C(0x1234), 0));
  assert(!muse_on_key_filter_service_matches(0x04b4, 0xe106, 0x01, 0x06,
                                             UINT64_C(0x1234),
                                             UINT64_C(0x4321)));
}

static void test_restore_verification_accepts_only_the_expected_shape(void) {
  assert(muse_on_key_filter_restore_is_verified(true, true, 0));
  assert(muse_on_key_filter_restore_is_verified(true, false, 0));
  assert(!muse_on_key_filter_restore_is_verified(true, false, 1));
  assert(!muse_on_key_filter_restore_is_verified(false, true, 0));
  assert(muse_on_key_filter_restore_is_verified(false, false, 1));
}

static void test_restore_settlement_retries_transient_disconnect_race(void) {
  MuseOnKeyFilterRestorePolicy policy;

  muse_on_key_filter_restore_policy_init(&policy);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy, UINT64_C(100), false, false) ==
         MUSE_ON_KEY_FILTER_RESTORE_RETRY);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy,
             UINT64_C(100) +
                 MUSE_ON_KEY_FILTER_RESTORE_SETTLEMENT_NS -
                 UINT64_C(1),
             false, false) == MUSE_ON_KEY_FILTER_RESTORE_RETRY);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy,
             UINT64_C(100) +
                 MUSE_ON_KEY_FILTER_RESTORE_SETTLEMENT_NS,
             false, false) == MUSE_ON_KEY_FILTER_RESTORE_FAILED);
}

static void test_restore_settlement_clears_after_verified_restore(void) {
  MuseOnKeyFilterRestorePolicy policy;

  muse_on_key_filter_restore_policy_init(&policy);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy, UINT64_C(100), false, false) ==
         MUSE_ON_KEY_FILTER_RESTORE_RETRY);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy, UINT64_C(200), true, false) ==
         MUSE_ON_KEY_FILTER_RESTORE_COMPLETE);
  assert(!policy.waiting);
  assert(policy.started_at_ns == 0);
}

static void test_final_restore_attempt_fails_without_settlement_delay(void) {
  MuseOnKeyFilterRestorePolicy policy;

  muse_on_key_filter_restore_policy_init(&policy);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy, UINT64_C(100), false, true) ==
         MUSE_ON_KEY_FILTER_RESTORE_FAILED);
}

int main(void) {
  test_sink_mapping_table_is_exact_and_valid();
  test_only_the_exact_muse_on_keyboard_service_matches();
  test_restore_verification_accepts_only_the_expected_shape();
  test_restore_settlement_retries_transient_disconnect_race();
  test_restore_settlement_clears_after_verified_restore();
  test_final_restore_attempt_fails_without_settlement_delay();
  return 0;
}
