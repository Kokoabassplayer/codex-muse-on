#include "muse_on_topology.h"

static MuseOnConnectionSnapshot unknown_snapshot(void) {
  return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_UNKNOWN, 0};
}

void muse_on_topology_init(MuseOnTopologyAuthority *authority) {
  if (!authority) return;
  authority->generation = 0;
  authority->snapshot = unknown_snapshot();
  authority->authoritative = false;
}

void muse_on_topology_begin(MuseOnTopologyAuthority *authority,
                            uint64_t generation) {
  if (!authority || generation == 0) return;
  authority->generation = generation;
  authority->snapshot = unknown_snapshot();
  authority->authoritative = true;
}

bool muse_on_topology_apply(MuseOnTopologyAuthority *authority,
                            uint64_t generation,
                            MuseOnConnectionSnapshot snapshot) {
  if (!authority || !authority->authoritative ||
      generation != authority->generation ||
      !muse_on_connection_snapshot_is_valid(snapshot)) {
    return false;
  }
  authority->snapshot = snapshot;
  return true;
}

bool muse_on_topology_invalidate(MuseOnTopologyAuthority *authority,
                                 uint64_t generation) {
  if (!authority || !authority->authoritative ||
      generation != authority->generation) {
    return false;
  }
  authority->snapshot = unknown_snapshot();
  authority->authoritative = false;
  return true;
}

static void clear_host_observation(MuseOnTopologyHostState *state) {
  state->controller_connected = false;
  state->multiple_controllers = false;
  state->controller_location_id = 0;
  state->inputs_released = false;
  state->filter_verified = false;
  state->recovery_filter_verified = false;
}

void muse_on_topology_host_init(MuseOnTopologyHostState *state) {
  if (!state) return;
  muse_on_topology_init(&state->authority);
  clear_host_observation(state);
  state->permission_granted = false;
  state->recovery_mode = false;
  state->recovery_validated = false;
  state->recovery_failed = false;
  state->safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
}

void muse_on_topology_host_begin(MuseOnTopologyHostState *state,
                                 uint64_t generation, bool recovery_mode) {
  if (!state) return;
  muse_on_topology_begin(&state->authority, generation);
  clear_host_observation(state);
  state->recovery_mode = recovery_mode;
  state->recovery_validated = false;
  state->recovery_failed = false;
  state->safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
}

static MuseOnTopologyEventResult validate_host_event(
    const MuseOnTopologyHostState *state, uint64_t generation,
    MuseOnConnectionSnapshot snapshot) {
  if (!state || state->authority.generation != generation ||
      !state->authority.authoritative) {
    return MUSE_ON_TOPOLOGY_EVENT_IGNORED_STALE;
  }
  if (!muse_on_connection_snapshot_is_valid(snapshot)) {
    return MUSE_ON_TOPOLOGY_EVENT_REJECTED_INVALID;
  }
  return MUSE_ON_TOPOLOGY_EVENT_ACCEPTED;
}

MuseOnTopologyEventResult muse_on_topology_host_apply_topology(
    MuseOnTopologyHostState *state, uint64_t generation,
    MuseOnConnectionSnapshot snapshot) {
  MuseOnTopologyEventResult result;
  bool topology_changed;

  result = validate_host_event(state, generation, snapshot);
  if (result != MUSE_ON_TOPOLOGY_EVENT_ACCEPTED) return result;
  (void)muse_on_topology_apply(&state->authority, generation, snapshot);
  if (snapshot.state == MUSE_ON_CONNECTION_UNKNOWN &&
      state->recovery_mode && state->recovery_validated) {
    return MUSE_ON_TOPOLOGY_EVENT_ACCEPTED;
  }
  topology_changed =
      state->controller_connected !=
          (snapshot.state == MUSE_ON_CONNECTION_SINGLE) ||
      state->multiple_controllers !=
          (snapshot.state == MUSE_ON_CONNECTION_MULTIPLE) ||
      state->controller_location_id != snapshot.location_id;
  state->controller_connected = snapshot.state == MUSE_ON_CONNECTION_SINGLE;
  state->multiple_controllers = snapshot.state == MUSE_ON_CONNECTION_MULTIPLE;
  state->controller_location_id = snapshot.location_id;
  if (topology_changed || snapshot.state == MUSE_ON_CONNECTION_UNKNOWN) {
    state->inputs_released = false;
    state->filter_verified = false;
  }
  return MUSE_ON_TOPOLOGY_EVENT_ACCEPTED;
}

MuseOnTopologyEventResult muse_on_topology_host_apply_recovery(
    MuseOnTopologyHostState *state, uint64_t generation,
    MuseOnConnectionSnapshot snapshot, MuseOnRecoveryOutcome outcome,
    bool inputs_released, bool permission_granted, bool filter_verified) {
  MuseOnTopologyEventResult result =
      validate_host_event(state, generation, snapshot);
  bool single = snapshot.state == MUSE_ON_CONNECTION_SINGLE;

  if (result != MUSE_ON_TOPOLOGY_EVENT_ACCEPTED) return result;
  if (!state->recovery_mode) {
    return MUSE_ON_TOPOLOGY_EVENT_REJECTED_INVALID;
  }
  if (outcome != MUSE_ON_RECOVERY_OUTCOME_SUCCESS &&
      outcome != MUSE_ON_RECOVERY_OUTCOME_NEUTRAL_ENTRY_PENDING &&
      outcome != MUSE_ON_RECOVERY_OUTCOME_FAILURE) {
    return MUSE_ON_TOPOLOGY_EVENT_REJECTED_INVALID;
  }
  if ((outcome == MUSE_ON_RECOVERY_OUTCOME_SUCCESS &&
       (!single || !permission_granted || !filter_verified ||
        !inputs_released)) ||
      (outcome == MUSE_ON_RECOVERY_OUTCOME_NEUTRAL_ENTRY_PENDING &&
       (!single || !permission_granted || !filter_verified ||
        inputs_released))) {
    return MUSE_ON_TOPOLOGY_EVENT_REJECTED_INVALID;
  }
  result = muse_on_topology_host_apply_topology(state, generation, snapshot);
  if (result != MUSE_ON_TOPOLOGY_EVENT_ACCEPTED) return result;
  state->recovery_validated = outcome != MUSE_ON_RECOVERY_OUTCOME_FAILURE;
  state->recovery_failed = outcome == MUSE_ON_RECOVERY_OUTCOME_FAILURE;
  state->inputs_released = inputs_released;
  state->permission_granted = permission_granted;
  state->filter_verified = false;
  state->recovery_filter_verified =
      state->recovery_validated && filter_verified;
  return MUSE_ON_TOPOLOGY_EVENT_ACCEPTED;
}

bool muse_on_topology_host_apply_neutral_entry(
    MuseOnTopologyHostState *state, uint64_t generation, bool inputs_released) {
  if (!state || state->authority.generation != generation ||
      !state->authority.authoritative) {
    return false;
  }
  state->inputs_released = inputs_released;
  return true;
}

bool muse_on_topology_host_apply_focus_changed(
    MuseOnTopologyHostState *state, uint64_t generation, bool foreground) {
  if (!state || state->authority.generation != generation ||
      !state->authority.authoritative) {
    return false;
  }
  if (!foreground) state->inputs_released = false;
  return true;
}

bool muse_on_topology_host_apply_filter_verification(
    MuseOnTopologyHostState *state, uint64_t generation, bool filter_verified) {
  if (!state || state->authority.generation != generation ||
      !state->authority.authoritative) {
    return false;
  }
  if (filter_verified &&
      (state->authority.snapshot.state != MUSE_ON_CONNECTION_SINGLE ||
       !state->controller_connected || state->multiple_controllers)) {
    return false;
  }
  if (filter_verified && state->recovery_mode) return true;
  state->filter_verified = filter_verified;
  return true;
}

void muse_on_topology_host_require_neutral_entry(
    MuseOnTopologyHostState *state) {
  if (!state) return;
  state->inputs_released = false;
}

bool muse_on_topology_host_invalidate(MuseOnTopologyHostState *state,
                                      uint64_t generation, bool latch,
                                      MuseOnSafetyFailure fallback_failure) {
  bool preserve_recovery_topology;

  if (!state || state->authority.generation != generation) return false;
  preserve_recovery_topology =
      state->recovery_mode && state->recovery_validated;
  state->authority.snapshot = unknown_snapshot();
  state->authority.authoritative = false;
  if (!preserve_recovery_topology) clear_host_observation(state);
  if (latch) {
    state->safety_failure = fallback_failure == MUSE_ON_SAFETY_FAILURE_NONE
        ? MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN : fallback_failure;
  }
  return true;
}

MuseOnSafetyFailure muse_on_topology_host_apply_coordinator(
    const MuseOnTopologyHostState *topology, MuseOnState *coordinator,
    MuseOnCommand command, MuseOnSafetyFailure requested_failure,
    MuseOnTopologyCoordinatorInputs inputs) {
  MuseOnSafetyFailure effective_failure = requested_failure;
  MuseOnPrerequisites prerequisites;

  if (!topology || !coordinator) return effective_failure;
  if (command == MUSE_ON_COMMAND_RETRY &&
      effective_failure == MUSE_ON_SAFETY_FAILURE_NONE &&
      muse_on_recovery_filter_restoration_unverified(
          topology->recovery_validated, inputs.cleanup_verified,
          inputs.permission_granted, topology->controller_connected,
          topology->multiple_controllers, inputs.session_available,
          inputs.codex_foreground, topology->inputs_released,
          topology->filter_verified)) {
    effective_failure = MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE;
  }
  prerequisites = (MuseOnPrerequisites){
      .safety_latched = effective_failure != MUSE_ON_SAFETY_FAILURE_NONE,
      .safety_failure = effective_failure,
      .cleanup_verified = inputs.cleanup_verified,
      .permission_granted = inputs.permission_granted,
      .controller_connected = topology->controller_connected,
      .multiple_controllers = topology->multiple_controllers,
      .session_available = inputs.session_available,
      .codex_foreground = inputs.codex_foreground,
      .filter_verified = topology->filter_verified,
      .recovery_filter_verified = topology->recovery_filter_verified,
      .inputs_released = topology->inputs_released,
  };
  muse_on_state_apply(coordinator, command, prerequisites);
  return effective_failure;
}
