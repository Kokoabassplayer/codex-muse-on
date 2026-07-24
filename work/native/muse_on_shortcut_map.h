#ifndef MUSE_ON_SHORTCUT_MAP_H
#define MUSE_ON_SHORTCUT_MAP_H

#include <stdbool.h>

#include "muse_on_action_map.h"

typedef enum {
  MUSE_ON_SHORTCUT_KEY_1 = 0,
  MUSE_ON_SHORTCUT_KEY_2,
  MUSE_ON_SHORTCUT_KEY_3,
  MUSE_ON_SHORTCUT_KEY_4,
  MUSE_ON_SHORTCUT_KEY_5,
  MUSE_ON_SHORTCUT_KEY_6,
  MUSE_ON_SHORTCUT_KEY_7,
  MUSE_ON_SHORTCUT_KEY_8,
  MUSE_ON_SHORTCUT_KEY_9,
  MUSE_ON_SHORTCUT_KEY_A,
  MUSE_ON_SHORTCUT_KEY_B,
  MUSE_ON_SHORTCUT_KEY_C,
  MUSE_ON_SHORTCUT_KEY_D,
  MUSE_ON_SHORTCUT_KEY_E,
  MUSE_ON_SHORTCUT_KEY_F,
  MUSE_ON_SHORTCUT_KEY_G
} MuseOnShortcutKey;

typedef enum {
  MUSE_ON_SHORTCUT_TAP = 0,
  MUSE_ON_SHORTCUT_KEY_DOWN,
  MUSE_ON_SHORTCUT_KEY_UP
} MuseOnShortcutOperation;

typedef struct {
  bool command;
  bool option;
  bool control;
  bool shift;
  MuseOnShortcutKey key;
  MuseOnShortcutOperation operation;
} MuseOnShortcutInstruction;

bool muse_on_shortcut_map(const MuseOnActionEvent *action,
                          MuseOnShortcutInstruction *shortcut);
const char *muse_on_shortcut_key_string(MuseOnShortcutKey key);
const char *muse_on_shortcut_operation_string(MuseOnShortcutOperation operation);

#endif
