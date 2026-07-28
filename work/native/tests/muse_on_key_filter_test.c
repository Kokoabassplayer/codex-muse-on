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

static void test_restore_targets_original_registry_entry_not_usb_location(void) {
  assert(muse_on_key_filter_restore_target_matches(UINT64_C(0x100012345),
                                                   UINT64_C(0x100012345)));
  assert(!muse_on_key_filter_restore_target_matches(UINT64_C(0x100012345),
                                                    UINT64_C(0x100067890)));
  assert(!muse_on_key_filter_restore_target_matches(0,
                                                    UINT64_C(0x100012345)));
}

static void test_lookup_selects_exact_registry_not_reused_usb_location(void) {
  const MuseOnKeyFilterServiceObservation observations[] = {
      {
          .registry_id_readable = true,
          .registry_id = UINT64_C(0x100012345),
          .metadata_readable = true,
          .vendor_id = 0x04b4,
          .product_id = 0xe106,
          .usage_page = 0x01,
          .usage = 0x06,
          .location_id = UINT64_C(0x1234),
      },
      {
          .registry_id_readable = true,
          .registry_id = UINT64_C(0x100067890),
          .metadata_readable = true,
          .vendor_id = 0x04b4,
          .product_id = 0xe106,
          .usage_page = 0x01,
          .usage = 0x06,
          .location_id = UINT64_C(0x1234),
      },
  };
  size_t matched_index = SIZE_MAX;

  assert(muse_on_key_filter_select_service(
             observations, 2, UINT64_C(0x100067890), UINT64_C(0x1234),
             &matched_index) == MUSE_ON_KEY_FILTER_LOOKUP_MATCHED);
  assert(matched_index == 1);
}

static void test_lookup_is_uncertain_when_identity_cannot_be_read(void) {
  const MuseOnKeyFilterServiceObservation unreadable_registry[] = {
      {
          .registry_id_readable = false,
      },
  };
  const MuseOnKeyFilterServiceObservation unreadable_metadata[] = {
      {
          .registry_id_readable = true,
          .registry_id = UINT64_C(0x100067890),
          .metadata_readable = false,
      },
  };
  const MuseOnKeyFilterServiceObservation unrelated[] = {
      {
          .registry_id_readable = true,
          .registry_id = UINT64_C(0x100012345),
          .metadata_readable = false,
      },
  };
  size_t matched_index = SIZE_MAX;

  assert(muse_on_key_filter_select_service(
             unreadable_registry, 1, UINT64_C(0x100067890),
             UINT64_C(0x1234), &matched_index) ==
         MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN);
  assert(matched_index == SIZE_MAX);
  assert(muse_on_key_filter_select_service(
             unreadable_metadata, 1, UINT64_C(0x100067890),
             UINT64_C(0x1234), &matched_index) ==
         MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN);
  assert(matched_index == SIZE_MAX);
  assert(muse_on_key_filter_select_service(
             unrelated, 1, UINT64_C(0x100067890), UINT64_C(0x1234),
             &matched_index) == MUSE_ON_KEY_FILTER_LOOKUP_CONFIRMED_ABSENT);
  assert(matched_index == SIZE_MAX);
}

static void test_lookup_rejects_ambiguous_duplicate_exact_identity(void) {
  const MuseOnKeyFilterServiceObservation observations[] = {
      {
          .registry_id_readable = true,
          .registry_id = UINT64_C(0x100067890),
          .metadata_readable = true,
          .vendor_id = 0x04b4,
          .product_id = 0xe106,
          .usage_page = 0x01,
          .usage = 0x06,
          .location_id = UINT64_C(0x1234),
      },
      {
          .registry_id_readable = true,
          .registry_id = UINT64_C(0x100067890),
          .metadata_readable = true,
          .vendor_id = 0x04b4,
          .product_id = 0xe106,
          .usage_page = 0x01,
          .usage = 0x06,
          .location_id = UINT64_C(0x1234),
      },
  };
  size_t matched_index = SIZE_MAX;

  assert(muse_on_key_filter_select_service(
             observations, 2, UINT64_C(0x100067890), UINT64_C(0x1234),
             &matched_index) == MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN);
  assert(matched_index == SIZE_MAX);
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
             &policy, UINT64_C(100), false, true, false) ==
         MUSE_ON_KEY_FILTER_RESTORE_RETRY);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy,
             UINT64_C(100) +
                 MUSE_ON_KEY_FILTER_RESTORE_SETTLEMENT_NS -
                 UINT64_C(1),
             false, true, false) == MUSE_ON_KEY_FILTER_RESTORE_RETRY);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy,
             UINT64_C(100) +
                 MUSE_ON_KEY_FILTER_RESTORE_SETTLEMENT_NS,
             false, true, false) == MUSE_ON_KEY_FILTER_RESTORE_FAILED);
}

static void test_restore_settlement_clears_after_verified_restore(void) {
  MuseOnKeyFilterRestorePolicy policy;

  muse_on_key_filter_restore_policy_init(&policy);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy, UINT64_C(100), false, true, false) ==
         MUSE_ON_KEY_FILTER_RESTORE_RETRY);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy, UINT64_C(200), true, true, false) ==
         MUSE_ON_KEY_FILTER_RESTORE_COMPLETE);
  assert(!policy.waiting);
  assert(policy.started_at_ns == 0);
}

static void test_restore_waits_without_deadline_when_keyboard_is_absent(void) {
  MuseOnKeyFilterRestorePolicy policy;
  uint64_t later =
      UINT64_C(100) + MUSE_ON_KEY_FILTER_RESTORE_SETTLEMENT_NS * UINT64_C(10);

  muse_on_key_filter_restore_policy_init(&policy);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy, UINT64_C(100), false, false, false) ==
         MUSE_ON_KEY_FILTER_RESTORE_RETRY);
  assert(!policy.waiting);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy, later, false, false, false) ==
         MUSE_ON_KEY_FILTER_RESTORE_RETRY);
  assert(!policy.waiting);
}

static void test_final_restore_attempt_fails_for_present_keyboard(void) {
  MuseOnKeyFilterRestorePolicy policy;

  muse_on_key_filter_restore_policy_init(&policy);
  assert(muse_on_key_filter_restore_policy_evaluate(
             &policy, UINT64_C(100), false, true, true) ==
         MUSE_ON_KEY_FILTER_RESTORE_FAILED);
}

int main(void) {
  test_sink_mapping_table_is_exact_and_valid();
  test_only_the_exact_muse_on_keyboard_service_matches();
  test_restore_targets_original_registry_entry_not_usb_location();
  test_lookup_selects_exact_registry_not_reused_usb_location();
  test_lookup_is_uncertain_when_identity_cannot_be_read();
  test_lookup_rejects_ambiguous_duplicate_exact_identity();
  test_restore_verification_accepts_only_the_expected_shape();
  test_restore_settlement_retries_transient_disconnect_race();
  test_restore_settlement_clears_after_verified_restore();
  test_restore_waits_without_deadline_when_keyboard_is_absent();
  test_final_restore_attempt_fails_for_present_keyboard();
  return 0;
}
