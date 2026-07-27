#ifndef MUSE_ON_TOPOLOGY_H
#define MUSE_ON_TOPOLOGY_H

#include <stdbool.h>
#include <stdint.h>

#include "muse_on_connection.h"

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

#endif
