#ifndef MUSE_ON_ACTIVATION_H
#define MUSE_ON_ACTIVATION_H

#include <stdbool.h>

#include "muse_on_config.h"

bool muse_on_should_filter_keyboard(MuseOnMode mode);
bool muse_on_can_route_actions(MuseOnMode mode, bool codex_frontmost,
                               bool keyboard_captured);
bool muse_on_can_post_actions(MuseOnMode mode, bool codex_frontmost,
                              bool keyboard_captured,
                              bool post_event_access);

#endif
