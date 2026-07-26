#include "muse_on_connection.h"

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
  size_t index;
  uint32_t candidate = 0;
  size_t complete_count = 0;

  if (!interfaces || !location_id) return false;
  for (index = 0; index < count; index++) {
    uint32_t location = interfaces[index].location_id;
    size_t keyboard_count;
    size_t joystick_count;

    size_t previous;
    if (!interfaces[index].connected || location == 0) {
      continue;
    }
    /* Process each connected physical location once. */
    for (previous = 0; previous < index; previous++) {
      if (interfaces[previous].connected &&
          interfaces[previous].location_id == location) break;
    }
    if (previous != index) continue;
    keyboard_count = count_at_location(interfaces, count, location,
                                       MUSE_ON_OBSERVED_KEYBOARD);
    joystick_count = count_at_location(interfaces, count, location,
                                       MUSE_ON_OBSERVED_JOYSTICK);
    /* Any partial or duplicate Muse-On makes selection unsafe. */
    if (keyboard_count != 1 || joystick_count != 1) return false;
    candidate = location;
    complete_count++;
    if (complete_count > 1) return false;
  }
  if (complete_count != 1) return false;
  *location_id = candidate;
  return true;
}
