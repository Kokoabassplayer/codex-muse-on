#ifndef MUSE_ON_CONNECTION_H
#define MUSE_ON_CONNECTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  MUSE_ON_OBSERVED_KEYBOARD = 1,
  MUSE_ON_OBSERVED_JOYSTICK = 2,
} MuseOnObservedInterfaceKind;

typedef struct {
  MuseOnObservedInterfaceKind kind;
  uint32_t location_id;
  bool connected;
} MuseOnObservedInterface;

/*
 * Returns true only when exactly one physical location exposes exactly one
 * connected Muse-On keyboard interface and one connected joystick interface.
 */
bool muse_on_find_single_complete_controller(
    const MuseOnObservedInterface *interfaces, size_t count,
    uint32_t *location_id);

#endif
