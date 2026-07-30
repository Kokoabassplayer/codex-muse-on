#include "../muse_on_topology.h"

#include <assert.h>
#include <string.h>

static MuseOnConnectionSnapshot snapshot(MuseOnConnectionState state,
                                          uint32_t location_id) {
  return (MuseOnConnectionSnapshot){state, location_id};
}

static MuseOnTopologyCoordinatorInputs normal_gates(void) {
  return (MuseOnTopologyCoordinatorInputs){
      .permission_granted = true,
      .session_available = true,
      .codex_foreground = true,
      .cleanup_verified = true,
  };
}

static void apply_coordinator(MuseOnTopologyHostState *host,
                              MuseOnState *coordinator,
                              MuseOnCommand command,
                              MuseOnSafetyFailure failure) {
  (void)muse_on_topology_host_apply_coordinator(
      host, coordinator, command, failure, normal_gates());
}

static void test_host_event_stream_drives_reader_state(void) {
  MuseOnTopologyHostState host;
  MuseOnState coordinator;

  muse_on_topology_host_init(&host);
  muse_on_state_init(&coordinator);
  muse_on_topology_host_begin(&host, 1, false);
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_ENABLE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(host.authority.snapshot.state == MUSE_ON_CONNECTION_UNKNOWN);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(coordinator.inactive_reason == MUSE_ON_INACTIVE_REASON_DISCONNECTED);
  assert(!coordinator.effects.request_dispatch);

  assert(muse_on_topology_host_apply_topology(
             &host, 1, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x110000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(host.controller_connected);
  assert(host.controller_location_id == 0x110000);
  assert(coordinator.inactive_reason ==
         MUSE_ON_INACTIVE_REASON_VERIFYING_CONTROL);
  assert(muse_on_topology_host_apply_filter_verification(&host, 1, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(!coordinator.effects.request_dispatch);

  assert(muse_on_topology_host_apply_neutral_entry(&host, 1, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);
  assert(coordinator.effects.request_dispatch);

  assert(muse_on_topology_host_apply_topology(
             &host, 1, snapshot(MUSE_ON_CONNECTION_MULTIPLE, 0)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.inactive_reason == MUSE_ON_INACTIVE_REASON_MULTIPLE);
  assert(!coordinator.effects.request_dispatch);

  assert(muse_on_topology_host_apply_topology(
             &host, 1, snapshot(MUSE_ON_CONNECTION_DISCONNECTED, 0)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.inactive_reason == MUSE_ON_INACTIVE_REASON_DISCONNECTED);
}

static void test_active_requires_current_generation_filter_verification(void) {
  MuseOnTopologyHostState host;
  MuseOnState coordinator;

  muse_on_topology_host_init(&host);
  muse_on_state_init(&coordinator);
  muse_on_topology_host_begin(&host, 8, false);
  assert(muse_on_topology_host_apply_topology(
             &host, 8, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x880000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_neutral_entry(&host, 8, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_ENABLE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(coordinator.inactive_reason ==
         MUSE_ON_INACTIVE_REASON_VERIFYING_CONTROL);
  assert(!coordinator.effects.request_dispatch);

  assert(muse_on_topology_host_apply_filter_verification(&host, 8, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);
  assert(coordinator.effects.request_dispatch);
}

static void test_filter_proof_requires_current_single_topology(void) {
  MuseOnTopologyHostState host;
  MuseOnState coordinator;

  muse_on_topology_host_init(&host);
  muse_on_state_init(&coordinator);
  muse_on_topology_host_begin(&host, 11, false);
  assert(!muse_on_topology_host_apply_filter_verification(&host, 11, true));
  assert(!host.filter_verified);
  assert(muse_on_topology_host_apply_topology(
             &host, 11, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x111100)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_filter_verification(&host, 11, true));
  assert(muse_on_topology_host_apply_neutral_entry(&host, 11, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_ENABLE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);
  assert(!muse_on_topology_host_apply_filter_verification(&host, 10, true));
}

static void test_filter_restoration_and_stale_events_cannot_reactivate(void) {
  MuseOnTopologyHostState host;
  MuseOnState coordinator;

  muse_on_topology_host_init(&host);
  muse_on_state_init(&coordinator);
  muse_on_topology_host_begin(&host, 9, false);
  assert(muse_on_topology_host_apply_topology(
             &host, 9, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x990000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_neutral_entry(&host, 9, true));
  assert(muse_on_topology_host_apply_filter_verification(&host, 9, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_ENABLE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);

  assert(muse_on_topology_host_apply_filter_verification(&host, 9, false));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.inactive_reason ==
         MUSE_ON_INACTIVE_REASON_VERIFYING_CONTROL);
  assert(!coordinator.effects.request_dispatch);

  muse_on_topology_host_begin(&host, 10, false);
  assert(!muse_on_topology_host_apply_filter_verification(&host, 9, true));
  assert(!host.filter_verified);
  assert(muse_on_topology_host_apply_topology(
             &host, 10, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x990000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_neutral_entry(&host, 10, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.inactive_reason ==
         MUSE_ON_INACTIVE_REASON_VERIFYING_CONTROL);
  assert(!coordinator.effects.request_dispatch);
}

static void test_stale_generation_and_unexpected_exit_fail_closed(void) {
  MuseOnTopologyHostState host;
  MuseOnState coordinator;

  muse_on_topology_host_init(&host);
  muse_on_state_init(&coordinator);
  muse_on_topology_host_begin(&host, 2, false);
  assert(muse_on_topology_host_apply_topology(
             &host, 2, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x220000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  muse_on_topology_host_begin(&host, 3, false);
  assert(muse_on_topology_host_apply_topology(
             &host, 2, snapshot(MUSE_ON_CONNECTION_MULTIPLE, 0)) ==
         MUSE_ON_TOPOLOGY_EVENT_IGNORED_STALE);
  assert(host.authority.snapshot.state == MUSE_ON_CONNECTION_UNKNOWN);
  assert(!host.multiple_controllers);

  assert(muse_on_topology_host_apply_topology(
             &host, 3, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x330000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_filter_verification(&host, 3, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_ENABLE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(muse_on_topology_host_apply_neutral_entry(&host, 3, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);

  assert(muse_on_topology_host_invalidate(
      &host, 3, true, MUSE_ON_SAFETY_FAILURE_NONE));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    host.safety_failure);
  assert(host.authority.snapshot.state == MUSE_ON_CONNECTION_UNKNOWN);
  assert(!host.authority.authoritative);
  assert(coordinator.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(coordinator.safety_failure ==
         MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
  assert(!coordinator.effects.request_dispatch);

  /* A second termination observation remains invalidated and fail-closed. */
  assert(muse_on_topology_host_invalidate(
      &host, 3, true, MUSE_ON_SAFETY_FAILURE_NONE));
  assert(host.authority.snapshot.state == MUSE_ON_CONNECTION_UNKNOWN);
}

static void test_typed_recovery_pending_requires_fresh_neutral_entry(void) {
  MuseOnTopologyHostState host;
  MuseOnState coordinator;

  muse_on_topology_host_init(&host);
  muse_on_state_init(&coordinator);
  muse_on_topology_host_begin(&host, 4, true);
  coordinator.enabled_intent = true;
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
  assert(coordinator.status == MUSE_ON_STATUS_SAFETY_LATCH);

  assert(muse_on_topology_host_apply_recovery(
             &host, 4, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x440000),
             MUSE_ON_RECOVERY_OUTCOME_NEUTRAL_ENTRY_PENDING, false, true,
             true) == MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(host.recovery_validated);
  assert(!host.inputs_released);

  /* Expected EOF/termination invalidates the authority but preserves the
   * typed recovery observation long enough for the clean Retry transition. */
  assert(muse_on_topology_host_invalidate(
      &host, 4, false, MUSE_ON_SAFETY_FAILURE_NONE));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_RETRY,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(!coordinator.safety_latched);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(coordinator.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(!coordinator.effects.request_dispatch);

  /* Only a fresh listener generation's neutral-entry observation can make
   * this Active. */
  muse_on_topology_host_begin(&host, 5, false);
  assert(muse_on_topology_host_apply_topology(
             &host, 5, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x440000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_filter_verification(&host, 5, true));
  assert(muse_on_topology_host_apply_neutral_entry(&host, 5, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);
  assert(coordinator.effects.request_dispatch);
}

static void test_focus_loss_requires_fresh_neutral_entry(void) {
  MuseOnTopologyHostState host;
  MuseOnState coordinator;

  muse_on_topology_host_init(&host);
  muse_on_state_init(&coordinator);
  muse_on_topology_host_begin(&host, 6, false);
  assert(muse_on_topology_host_apply_topology(
             &host, 6, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x660000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_filter_verification(&host, 6, true));
  assert(muse_on_topology_host_apply_neutral_entry(&host, 6, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_ENABLE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);
  assert(coordinator.effects.request_dispatch);

  /* Focus loss resets the host's release prerequisite. Returning focus does
   * not release it; only a fresh listener Neutral Entry may do so. */
  muse_on_topology_host_require_neutral_entry(&host);
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(coordinator.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(!coordinator.effects.request_dispatch);

  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(!coordinator.effects.request_dispatch);

  assert(muse_on_topology_host_apply_neutral_entry(&host, 6, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);
  assert(coordinator.effects.request_dispatch);
}

static void test_multiple_to_single_requires_fresh_neutral_entry(void) {
  MuseOnTopologyHostState host;
  MuseOnState coordinator;

  muse_on_topology_host_init(&host);
  muse_on_state_init(&coordinator);
  muse_on_topology_host_begin(&host, 7, false);
  assert(muse_on_topology_host_apply_topology(
             &host, 7, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x770000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_filter_verification(&host, 7, true));
  assert(muse_on_topology_host_apply_neutral_entry(&host, 7, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_ENABLE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);

  assert(muse_on_topology_host_apply_topology(
             &host, 7, snapshot(MUSE_ON_CONNECTION_MULTIPLE, 0)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(!host.inputs_released);
  assert(muse_on_topology_host_apply_topology(
             &host, 7, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x770000)) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(!host.inputs_released);
  assert(!host.filter_verified);
  assert(muse_on_topology_host_apply_filter_verification(&host, 7, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE,
                    MUSE_ON_SAFETY_FAILURE_NONE);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(coordinator.inactive_reason ==
         MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(!coordinator.effects.request_dispatch);
}

static void test_invalid_snapshot_is_rejected_not_disconnected(void) {
  MuseOnTopologyHostState host;

  muse_on_topology_host_init(&host);
  muse_on_topology_host_begin(&host, 5, false);
  assert(muse_on_topology_host_apply_topology(
             &host, 5, snapshot(MUSE_ON_CONNECTION_SINGLE, 0)) ==
         MUSE_ON_TOPOLOGY_EVENT_REJECTED_INVALID);
  assert(host.authority.snapshot.state == MUSE_ON_CONNECTION_UNKNOWN);
  assert(!host.controller_connected);
}

int main(void) {
  test_host_event_stream_drives_reader_state();
  test_active_requires_current_generation_filter_verification();
  test_filter_proof_requires_current_single_topology();
  test_filter_restoration_and_stale_events_cannot_reactivate();
  test_stale_generation_and_unexpected_exit_fail_closed();
  test_typed_recovery_pending_requires_fresh_neutral_entry();
  test_focus_loss_requires_fresh_neutral_entry();
  test_multiple_to_single_requires_fresh_neutral_entry();
  test_invalid_snapshot_is_rejected_not_disconnected();
  assert(strcmp(muse_on_connection_state_string(MUSE_ON_CONNECTION_UNKNOWN),
                "unknown") == 0);
  return 0;
}
