#ifndef MUSE_ON_PLATFORM_H
#define MUSE_ON_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "muse_on_shortcut_map.h"

typedef enum {
  MUSE_ON_POST_HYPER_KEY_TAP = 0,
  MUSE_ON_POST_HYPER_KEY_DOWN,
  MUSE_ON_POST_HYPER_KEY_UP
} MuseOnPostHyperKeyPhase;

typedef struct {
  uint16_t mac_virtual_key;
  bool key_down;
  uint64_t flags;
} MuseOnHyperKeyEvent;

bool muse_on_bundle_id_is_codex(const char *bundle_id);
bool muse_on_codex_is_frontmost(void);
bool muse_on_session_is_available(void);
bool muse_on_preflight_post_event_access(void);
bool muse_on_request_post_event_access(void);
size_t muse_on_hyper_key_event_sequence(uint16_t mac_virtual_key,
                                        MuseOnPostHyperKeyPhase phase,
                                        MuseOnHyperKeyEvent *events,
                                        size_t capacity);
bool muse_on_post_hyper_key(uint16_t mac_virtual_key,
                            MuseOnPostHyperKeyPhase phase);
bool muse_on_mac_virtual_key(MuseOnShortcutKey key, uint16_t *mac_virtual_key);
bool muse_on_post_shortcut(const MuseOnShortcutInstruction *shortcut);

#endif
