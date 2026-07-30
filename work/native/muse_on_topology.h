#ifndef MUSE_ON_TOPOLOGY_H
#define MUSE_ON_TOPOLOGY_H

#include <stdbool.h>
#include <stdint.h>

#include "muse_on_connection.h"
#include "muse_on_state_coordinator.h"

/*
 * Small pure adapter for the listener-task-owned topology authority.
 * A new task generation starts Unknown and only its events may replace that
 * snapshot. Invalidation is distinct from a valid Disconnected observation.
 */
typedef struct {
  uint64_t generation;
  MuseOnConnectionSnapshot snapshot;
  bool authoritative;
} MuseOnTopologyAuthority;

void muse_on_topology_init(MuseOnTopologyAuthority *authority);
void muse_on_topology_begin(MuseOnTopologyAuthority *authority,
                            uint64_t generation);
bool muse_on_topology_apply(MuseOnTopologyAuthority *authority,
                            uint64_t generation,
                            MuseOnConnectionSnapshot snapshot);
bool muse_on_topology_invalidate(MuseOnTopologyAuthority *authority,
                                 uint64_t generation);

typedef enum {
  MUSE_ON_TOPOLOGY_EVENT_ACCEPTED = 0,
  MUSE_ON_TOPOLOGY_EVENT_IGNORED_STALE,
  MUSE_ON_TOPOLOGY_EVENT_REJECTED_INVALID,
} MuseOnTopologyEventResult;

/* The production host event state shared by the menu adapter and tests. */
typedef struct {
  MuseOnTopologyAuthority authority;
  bool controller_connected;
  bool multiple_controllers;
  uint32_t controller_location_id;
  bool inputs_released;
  bool permission_granted;
  bool filter_verified;
  bool recovery_filter_verified;
  bool recovery_mode;
  bool recovery_validated;
  bool recovery_failed;
  MuseOnSafetyFailure safety_failure;
} MuseOnTopologyHostState;

typedef struct {
  bool permission_granted;
  bool session_available;
  bool codex_foreground;
  bool cleanup_verified;
} MuseOnTopologyCoordinatorInputs;

void muse_on_topology_host_init(MuseOnTopologyHostState *state);
void muse_on_topology_host_begin(MuseOnTopologyHostState *state,
                                 uint64_t generation, bool recovery_mode);
MuseOnTopologyEventResult muse_on_topology_host_apply_topology(
    MuseOnTopologyHostState *state, uint64_t generation,
    MuseOnConnectionSnapshot snapshot);
MuseOnTopologyEventResult muse_on_topology_host_apply_recovery(
    MuseOnTopologyHostState *state, uint64_t generation,
    MuseOnConnectionSnapshot snapshot, MuseOnRecoveryOutcome outcome,
    bool inputs_released, bool permission_granted, bool filter_verified);
bool muse_on_topology_host_apply_neutral_entry(
    MuseOnTopologyHostState *state, uint64_t generation, bool inputs_released);
bool muse_on_topology_host_apply_filter_verification(
    MuseOnTopologyHostState *state, uint64_t generation, bool filter_verified);
void muse_on_topology_host_require_neutral_entry(
    MuseOnTopologyHostState *state);
bool muse_on_topology_host_invalidate(MuseOnTopologyHostState *state,
                                      uint64_t generation, bool latch,
                                      MuseOnSafetyFailure fallback_failure);
MuseOnSafetyFailure muse_on_topology_host_apply_coordinator(
    const MuseOnTopologyHostState *topology, MuseOnState *coordinator,
    MuseOnCommand command, MuseOnSafetyFailure requested_failure,
    MuseOnTopologyCoordinatorInputs inputs);

#endif
