#include "muse_on_activation.h"

bool muse_on_should_filter_keyboard(MuseOnMode mode) {
  switch (mode) {
    case MUSE_ON_MODE_DRY_RUN:
      return false;
    case MUSE_ON_MODE_CAPTURE_DRY_RUN:
    case MUSE_ON_MODE_ACTIVE:
      return true;
  }
  return false;
}

bool muse_on_can_route_actions(MuseOnMode mode, bool codex_frontmost,
                               bool keyboard_captured) {
  switch (mode) {
    case MUSE_ON_MODE_DRY_RUN:
      return true;
    case MUSE_ON_MODE_CAPTURE_DRY_RUN:
    case MUSE_ON_MODE_ACTIVE:
      return codex_frontmost && keyboard_captured;
  }
  return false;
}

bool muse_on_can_post_actions(MuseOnMode mode, bool codex_frontmost,
                              bool keyboard_captured,
                              bool post_event_access) {
  return mode == MUSE_ON_MODE_ACTIVE && codex_frontmost &&
         keyboard_captured && post_event_access;
}
