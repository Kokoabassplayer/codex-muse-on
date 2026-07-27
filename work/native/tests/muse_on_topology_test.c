#include "../muse_on_topology.h"

#include <assert.h>
#include <string.h>

#include "../muse_on_state_coordinator.h"

static MuseOnConnectionSnapshot snapshot(MuseOnConnectionState state,
                                          uint32_t location_id) {
  return (MuseOnConnectionSnapshot){state, location_id};
}

static void apply_topology_to_state(MuseOnState *state,
                                    MuseOnConnectionSnapshot topology,
                                    MuseOnCommand command) {
  MuseOnPrerequisites prerequisites = {
      .cleanup_verified = true,
      .permission_granted = true,
      .controller_connected = topology.state == MUSE_ON_CONNECTION_SINGLE,
      .multiple_controllers = topology.state == MUSE_ON_CONNECTION_MULTIPLE,
      .session_available = true,
      .codex_foreground = true,
      .inputs_released = true,
  };

  muse_on_state_apply(state, command, prerequisites);
}

static void test_authoritative_stream_and_coordinator_source(void) {
  MuseOnTopologyAuthority authority;
  MuseOnState state;

  muse_on_topology_init(&authority);
  muse_on_topology_begin(&authority, 1);
  assert(authority.snapshot.state == MUSE_ON_CONNECTION_UNKNOWN);

  assert(muse_on_topology_apply(
      &authority, 1,
      snapshot(MUSE_ON_CONNECTION_DISCONNECTED, 0)));
  assert(authority.snapshot.state == MUSE_ON_CONNECTION_DISCONNECTED);

  assert(muse_on_topology_apply(
      &authority, 1,
      snapshot(MUSE_ON_CONNECTION_SINGLE, 0x110000)));
  assert(authority.snapshot.location_id == 0x110000);

  assert(muse_on_topology_apply(
      &authority, 1,
      snapshot(MUSE_ON_CONNECTION_MULTIPLE, 0)));
  assert(authority.snapshot.state == MUSE_ON_CONNECTION_MULTIPLE);

  assert(muse_on_topology_apply(
      &authority, 1,
      snapshot(MUSE_ON_CONNECTION_DISCONNECTED, 0)));
  assert(muse_on_topology_apply(
      &authority, 1,
      snapshot(MUSE_ON_CONNECTION_SINGLE, 0x220000)));
  assert(authority.snapshot.location_id == 0x220000);

  muse_on_state_init(&state);
  apply_topology_to_state(&state, authority.snapshot, MUSE_ON_COMMAND_ENABLE);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.effects.request_dispatch);
}

static void test_recovery_single_neutral_pending_clears_latch_without_equality(void) {
  MuseOnTopologyAuthority authority;
  MuseOnState state;

  muse_on_topology_init(&authority);
  muse_on_topology_begin(&authority, 7);
  assert(muse_on_topology_apply(
      &authority, 7,
      snapshot(MUSE_ON_CONNECTION_SINGLE, 0x330000)));

  muse_on_state_init(&state);
  apply_topology_to_state(&state, authority.snapshot, MUSE_ON_COMMAND_ENABLE);
  state.safety_latched = true;
  state.safety_failure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
  state.status = MUSE_ON_STATUS_SAFETY_LATCH;
  apply_topology_to_state(&state, authority.snapshot, MUSE_ON_COMMAND_RETRY);
  assert(!state.safety_latched);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);

  /* The helper's typed neutral-entry-pending outcome is still fail-closed. */
  MuseOnPrerequisites pending = {
      .permission_granted = true,
      .controller_connected = true,
      .session_available = true,
      .codex_foreground = true,
      .inputs_released = false,
  };
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, pending);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
}

static void test_removal_and_multiple_block_dispatch(void) {
  MuseOnState state;

  muse_on_state_init(&state);
  apply_topology_to_state(
      &state, snapshot(MUSE_ON_CONNECTION_SINGLE, 0x440000),
      MUSE_ON_COMMAND_ENABLE);
  assert(state.effects.request_dispatch);

  apply_topology_to_state(
      &state, snapshot(MUSE_ON_CONNECTION_DISCONNECTED, 0),
      MUSE_ON_COMMAND_NONE);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_DISCONNECTED);
  assert(!state.effects.request_dispatch);
  assert(state.effects.request_cleanup == false);

  apply_topology_to_state(
      &state, snapshot(MUSE_ON_CONNECTION_MULTIPLE, 0), MUSE_ON_COMMAND_NONE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_MULTIPLE);
  assert(!state.effects.request_dispatch);
}

static void test_invalidation_latches_and_stale_generation_is_ignored(void) {
  MuseOnTopologyAuthority authority;
  MuseOnState state;

  muse_on_topology_init(&authority);
  muse_on_topology_begin(&authority, 10);
  assert(muse_on_topology_apply(
      &authority, 10,
      snapshot(MUSE_ON_CONNECTION_SINGLE, 0x550000)));
  muse_on_topology_begin(&authority, 11);
  assert(authority.snapshot.state == MUSE_ON_CONNECTION_UNKNOWN);
  assert(!muse_on_topology_apply(
      &authority, 10,
      snapshot(MUSE_ON_CONNECTION_MULTIPLE, 0)));
  assert(authority.snapshot.state == MUSE_ON_CONNECTION_UNKNOWN);
  assert(muse_on_topology_invalidate(&authority, 11));
  assert(!authority.authoritative);
  assert(authority.snapshot.state == MUSE_ON_CONNECTION_UNKNOWN);

  muse_on_state_init(&state);
  state.enabled_intent = true;
  muse_on_state_apply(
      &state, MUSE_ON_COMMAND_NONE,
      (MuseOnPrerequisites){
          .safety_latched = true,
          .safety_failure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN,
          .permission_granted = true,
          .session_available = true,
      });
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.safety_failure == MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN);
}

int main(void) {
  test_authoritative_stream_and_coordinator_source();
  test_recovery_single_neutral_pending_clears_latch_without_equality();
  test_removal_and_multiple_block_dispatch();
  test_invalidation_latches_and_stale_generation_is_ignored();
  assert(strcmp(muse_on_connection_state_string(MUSE_ON_CONNECTION_UNKNOWN),
                "unknown") == 0);
  assert(!muse_on_connection_snapshot_is_valid(
      snapshot(MUSE_ON_CONNECTION_SINGLE, 0)));
  return 0;
}
