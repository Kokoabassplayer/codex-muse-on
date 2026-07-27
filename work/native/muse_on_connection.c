#include "muse_on_connection.h"

static MuseOnConnectionSnapshot disconnected_snapshot(void) {
  return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
}

const char *muse_on_connection_state_string(MuseOnConnectionState state) {
  switch (state) {
    case MUSE_ON_CONNECTION_UNKNOWN: return "unknown";
    case MUSE_ON_CONNECTION_DISCONNECTED: return "disconnected";
    case MUSE_ON_CONNECTION_SINGLE: return "single";
    case MUSE_ON_CONNECTION_MULTIPLE: return "multiple";
  }
  return "unknown";
}

bool muse_on_connection_snapshot_is_valid(
    MuseOnConnectionSnapshot snapshot) {
  switch (snapshot.state) {
    case MUSE_ON_CONNECTION_UNKNOWN:
      return snapshot.location_id == 0;
    case MUSE_ON_CONNECTION_DISCONNECTED:
    case MUSE_ON_CONNECTION_MULTIPLE:
      return snapshot.location_id == 0;
    case MUSE_ON_CONNECTION_SINGLE:
      return snapshot.location_id != 0;
  }
  return false;
}

static size_t count_at_location(const MuseOnObservedInterface *interfaces,
                                size_t count, uint32_t location_id,
                                MuseOnObservedInterfaceKind kind) {
  size_t index;
  size_t matches = 0;

  for (index = 0; index < count; index++) {
    if (interfaces[index].connected && interfaces[index].location_id == location_id &&
        interfaces[index].kind == kind) {
      matches++;
    }
  }
  return matches;
}

bool muse_on_find_single_complete_controller(
    const MuseOnObservedInterface *interfaces, size_t count,
    uint32_t *location_id) {
  MuseOnConnectionSnapshot snapshot =
      muse_on_classify_connections(interfaces, count);

  if (!interfaces || !location_id) return false;
  if (snapshot.state != MUSE_ON_CONNECTION_SINGLE) return false;
  *location_id = snapshot.location_id;
  return true;
}

MuseOnConnectionSnapshot muse_on_classify_connections(
    const MuseOnObservedInterface *interfaces, size_t count) {
  size_t index;
  size_t complete_count = 0;
  uint32_t candidate = 0;

  if (!interfaces || count == 0) return disconnected_snapshot();
  for (index = 0; index < count; index++) {
    uint32_t location = interfaces[index].location_id;
    size_t keyboard_count;
    size_t joystick_count;
    size_t previous;

    if (!interfaces[index].connected || location == 0 ||
        (interfaces[index].kind != MUSE_ON_OBSERVED_KEYBOARD &&
         interfaces[index].kind != MUSE_ON_OBSERVED_JOYSTICK)) {
      return disconnected_snapshot();
    }
    /* Process each physical location once. */
    for (previous = 0; previous < index; previous++) {
      if (interfaces[previous].location_id == location) break;
    }
    if (previous != index) continue;
    keyboard_count = count_at_location(interfaces, count, location,
                                       MUSE_ON_OBSERVED_KEYBOARD);
    joystick_count = count_at_location(interfaces, count, location,
                                       MUSE_ON_OBSERVED_JOYSTICK);
    /* Every observed location must be exactly one complete controller. */
    if (keyboard_count != 1 || joystick_count != 1) {
      return disconnected_snapshot();
    }
    candidate = location;
    complete_count++;
  }
  if (complete_count == 0) return disconnected_snapshot();
  if (complete_count > 1) {
    return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_MULTIPLE, 0};
  }
  return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_SINGLE, candidate};
}
