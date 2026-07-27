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
