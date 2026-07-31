#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

static void test_removal_plan_preserves_production_order_and_ownership(void) {
  const MuseOnKeyFilterOwnership initial = {
      .location_id = UINT64_C(0x1234),
      .registry_id = UINT64_C(0x100012345),
      .restore_pending = true,
      .active = true,
  };
  MuseOnKeyFilterOwnership keyboard_first = initial;
  MuseOnKeyFilterOwnership joystick_first = initial;
  MuseOnKeyFilterOwnership replacement = initial;

  assert(muse_on_key_filter_removal_decide(
             true, &keyboard_first, UINT64_C(0x1234),
             UINT64_C(0x100012345), false, true) ==
         MUSE_ON_KEY_FILTER_REMOVAL_CONFIRM_EXACT_TARGET);
  assert(muse_on_key_filter_ownership_confirm_removed(
      &keyboard_first, UINT64_C(0x1234), UINT64_C(0x100012345)));
  assert(keyboard_first.location_id == 0);
  assert(keyboard_first.registry_id == 0);
  assert(!keyboard_first.restore_pending);
  assert(!keyboard_first.active);

  /* Joystick-first removal restores through the still-present keyboard. */
  assert(muse_on_key_filter_removal_decide(
             false, &joystick_first, UINT64_C(0x1234),
             UINT64_C(0x200012345), false, true) ==
         MUSE_ON_KEY_FILTER_REMOVAL_RESTORE_GENERIC);
  muse_on_key_filter_ownership_clear(&joystick_first);
  assert(muse_on_key_filter_removal_decide(
             true, &joystick_first, UINT64_C(0x1234),
             UINT64_C(0x100012345), false, false) ==
         MUSE_ON_KEY_FILTER_REMOVAL_NONE);

  /* A replacement at the same USB location never owns the old mapping. */
  assert(muse_on_key_filter_removal_decide(
             true, &replacement, UINT64_C(0x1234),
             UINT64_C(0x100067890), true, true) ==
         MUSE_ON_KEY_FILTER_REMOVAL_RESTORE_GENERIC);
  assert(muse_on_key_filter_removal_decide(
             true, &replacement, UINT64_C(0x4321),
             UINT64_C(0x100012345), true, true) ==
         MUSE_ON_KEY_FILTER_REMOVAL_NONE);
  assert(!muse_on_key_filter_ownership_confirm_removed(
      &replacement, UINT64_C(0x1234), UINT64_C(0x100067890)));
  assert(replacement.location_id == initial.location_id);
  assert(replacement.registry_id == initial.registry_id);
  assert(replacement.restore_pending && replacement.active);
  assert(muse_on_key_filter_removal_decide(
             true, &replacement, UINT64_C(0x1234),
             UINT64_C(0x100012345), true, true) ==
         MUSE_ON_KEY_FILTER_REMOVAL_CONFIRM_EXACT_TARGET_AND_REAPPLY);
  assert(muse_on_key_filter_removal_may_reapply(
      MUSE_ON_KEY_FILTER_REMOVAL_CONFIRM_EXACT_TARGET_AND_REAPPLY, true,
      false));
  assert(!muse_on_key_filter_removal_may_reapply(
      MUSE_ON_KEY_FILTER_REMOVAL_CONFIRM_EXACT_TARGET_AND_REAPPLY, false,
      true));
  assert(!muse_on_key_filter_reapply_failed_terminally(false, true, false));
  assert(muse_on_key_filter_reapply_failed_terminally(false, false, false));
  assert(!muse_on_key_filter_reapply_failed_terminally(false, false, true));
  assert(!muse_on_key_filter_reapply_failed_terminally(true, false, false));
}

static void test_event_service_resolves_through_exact_device_ancestor(void) {
  const uint64_t registry_chain[] = {
      UINT64_C(0x10006aade), /* AppleUserHIDEventService */
      UINT64_C(0x10006aad7), /* IOHIDInterface */
      UINT64_C(0x10006aace), /* AppleUserHIDDevice */
      UINT64_C(0x10006aacc), /* USB interface */
  };

  assert(muse_on_key_filter_registry_chain_contains(
      registry_chain, 4, UINT64_C(0x10006aace)));
  assert(!muse_on_key_filter_registry_chain_contains(
      registry_chain, 4, UINT64_C(0x10006ffff)));
  assert(!muse_on_key_filter_registry_chain_contains(
      registry_chain, 4, 0));
  assert(!muse_on_key_filter_registry_chain_contains(
      NULL, 4, UINT64_C(0x10006aace)));
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

static void test_apply_policy_bounds_reconnect_service_refresh(void) {
  MuseOnKeyFilterApplyPolicy policy;

  muse_on_key_filter_apply_policy_init(&policy);
  assert(muse_on_key_filter_apply_policy_evaluate(
             &policy, UINT64_C(100),
             MUSE_ON_KEY_FILTER_APPLY_SERVICE_ABSENT) ==
         MUSE_ON_KEY_FILTER_APPLY_RETRY);
  assert(policy.waiting);
  assert(muse_on_key_filter_apply_policy_evaluate(
             &policy,
             UINT64_C(100) + MUSE_ON_KEY_FILTER_APPLY_SETTLEMENT_NS -
                 UINT64_C(1),
             MUSE_ON_KEY_FILTER_APPLY_SERVICE_UNCERTAIN) ==
         MUSE_ON_KEY_FILTER_APPLY_RETRY);
  assert(muse_on_key_filter_apply_policy_evaluate(
             &policy,
             UINT64_C(100) + MUSE_ON_KEY_FILTER_APPLY_SETTLEMENT_NS,
             MUSE_ON_KEY_FILTER_APPLY_VERIFY_FAILED) ==
         MUSE_ON_KEY_FILTER_APPLY_FAILED);
  assert(!policy.waiting);

  assert(muse_on_key_filter_apply_policy_evaluate(
             &policy, UINT64_C(300),
             MUSE_ON_KEY_FILTER_APPLY_WRITE_FAILED) ==
         MUSE_ON_KEY_FILTER_APPLY_RETRY);
  assert(muse_on_key_filter_apply_policy_evaluate(
             &policy, UINT64_C(400), MUSE_ON_KEY_FILTER_APPLY_APPLIED) ==
         MUSE_ON_KEY_FILTER_APPLY_COMPLETE);
  assert(!policy.waiting);
  assert(muse_on_key_filter_apply_policy_evaluate(
             &policy, UINT64_C(500),
             MUSE_ON_KEY_FILTER_APPLY_ROLLBACK_UNVERIFIED) ==
         MUSE_ON_KEY_FILTER_APPLY_FAILED);
  assert(!policy.waiting);

  assert(strcmp(muse_on_key_filter_apply_result_string(
                    MUSE_ON_KEY_FILTER_APPLY_SERVICE_ABSENT),
                "apply_keyboard_filter_service_absent") == 0);
}

typedef struct {
  void *new_client;
  void *matched_service;
  void *released_client;
  uint64_t expected_location_id;
  uint64_t expected_registry_id;
  size_t create_count;
  size_t find_count;
  size_t release_count;
  bool create_succeeds;
} FakeClientRefresh;

static void *fake_create_client(void *context) {
  FakeClientRefresh *fake = context;
  fake->create_count++;
  return fake->create_succeeds ? fake->new_client : NULL;
}

static void fake_release_client(void *context, void *client) {
  FakeClientRefresh *fake = context;
  fake->release_count++;
  fake->released_client = client;
}

static void *fake_find_service(
    void *context, void *client, uint64_t requested_location_id,
    uint64_t requested_registry_id, MuseOnKeyFilterLookupResult *lookup_result) {
  FakeClientRefresh *fake = context;
  fake->find_count++;
  if (client == fake->new_client &&
      requested_location_id == fake->expected_location_id &&
      requested_registry_id == fake->expected_registry_id) {
    *lookup_result = MUSE_ON_KEY_FILTER_LOOKUP_MATCHED;
    return fake->matched_service;
  }
  *lookup_result = MUSE_ON_KEY_FILTER_LOOKUP_CONFIRMED_ABSENT;
  return NULL;
}

static void test_apply_refreshes_stale_client_before_exact_reconnect_lookup(
    void) {
  int old_client;
  int new_client;
  int replacement_service;
  void *refreshed_client = NULL;
  void *matched_service = NULL;
  MuseOnKeyFilterLookupResult lookup = MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN;
  const MuseOnKeyFilterClientOperations operations = {
      .create_client = fake_create_client,
      .release_client = fake_release_client,
      .find_service = fake_find_service,
  };
  FakeClientRefresh fake = {
      .new_client = &new_client,
      .matched_service = &replacement_service,
      .expected_location_id = UINT64_C(0x1234),
      .expected_registry_id = UINT64_C(0x100067890),
      .create_succeeds = true,
  };

  assert(muse_on_key_filter_refresh_and_find_service(
      &operations, &fake, &old_client, UINT64_C(0x1234),
      UINT64_C(0x100067890), &refreshed_client, &matched_service, &lookup));
  assert(fake.create_count == 1);
  assert(fake.find_count == 1);
  assert(fake.release_count == 1);
  assert(fake.released_client == &old_client);
  assert(refreshed_client == &new_client);
  assert(matched_service == &replacement_service);
  assert(lookup == MUSE_ON_KEY_FILTER_LOOKUP_MATCHED);

  fake = (FakeClientRefresh){
      .new_client = &new_client,
      .expected_location_id = UINT64_C(0x1234),
      .expected_registry_id = UINT64_C(0x100067890),
      .create_succeeds = false,
  };
  refreshed_client = &old_client;
  matched_service = &replacement_service;
  assert(!muse_on_key_filter_refresh_and_find_service(
      &operations, &fake, &old_client, UINT64_C(0x1234),
      UINT64_C(0x100067890), &refreshed_client, &matched_service, &lookup));
  assert(fake.create_count == 1);
  assert(fake.find_count == 0);
  assert(fake.release_count == 0);
  assert(refreshed_client == NULL);
  assert(matched_service == NULL);
}

int main(void) {
  test_sink_mapping_table_is_exact_and_valid();
  test_only_the_exact_muse_on_keyboard_service_matches();
  test_restore_targets_original_registry_entry_not_usb_location();
  test_removal_plan_preserves_production_order_and_ownership();
  test_event_service_resolves_through_exact_device_ancestor();
  test_lookup_selects_exact_registry_not_reused_usb_location();
  test_lookup_is_uncertain_when_identity_cannot_be_read();
  test_lookup_rejects_ambiguous_duplicate_exact_identity();
  test_restore_verification_accepts_only_the_expected_shape();
  test_restore_settlement_retries_transient_disconnect_race();
  test_restore_settlement_clears_after_verified_restore();
  test_restore_waits_without_deadline_when_keyboard_is_absent();
  test_final_restore_attempt_fails_for_present_keyboard();
  test_apply_policy_bounds_reconnect_service_refresh();
  test_apply_refreshes_stale_client_before_exact_reconnect_lookup();
  return 0;
}
