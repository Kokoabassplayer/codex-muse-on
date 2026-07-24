#include "muse_on_shortcut_map.h"

static bool shortcut_key_for_action(MuseOnActionId action, MuseOnShortcutKey *key) {
  switch (action) {
    case MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE: *key = MUSE_ON_SHORTCUT_KEY_1; return true;
    case MUSE_ON_ACTION_APPROVAL_APPROVE: *key = MUSE_ON_SHORTCUT_KEY_2; return true;
    case MUSE_ON_ACTION_APPROVAL_DECLINE: *key = MUSE_ON_SHORTCUT_KEY_3; return true;
    case MUSE_ON_ACTION_FORK_THREAD: *key = MUSE_ON_SHORTCUT_KEY_4; return true;
    case MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN: *key = MUSE_ON_SHORTCUT_KEY_5; return true;
    case MUSE_ON_ACTION_COMPOSER_SUBMIT: *key = MUSE_ON_SHORTCUT_KEY_6; return true;
    case MUSE_ON_ACTION_TOGGLE_REVIEW_TAB: *key = MUSE_ON_SHORTCUT_KEY_7; return true;
    case MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD: *key = MUSE_ON_SHORTCUT_KEY_8; return true;
    case MUSE_ON_ACTION_ENVIRONMENT_ACTION_1: *key = MUSE_ON_SHORTCUT_KEY_9; return true;
    case MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT: *key = MUSE_ON_SHORTCUT_KEY_A; return true;
    case MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT: *key = MUSE_ON_SHORTCUT_KEY_B; return true;
    case MUSE_ON_ACTION_PREVIOUS_THREAD: *key = MUSE_ON_SHORTCUT_KEY_C; return true;
    case MUSE_ON_ACTION_NEXT_THREAD: *key = MUSE_ON_SHORTCUT_KEY_D; return true;
    case MUSE_ON_ACTION_NAVIGATE_BACK: *key = MUSE_ON_SHORTCUT_KEY_E; return true;
    case MUSE_ON_ACTION_NAVIGATE_FORWARD: *key = MUSE_ON_SHORTCUT_KEY_F; return true;
    case MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER: *key = MUSE_ON_SHORTCUT_KEY_G; return true;
  }
  return false;
}

bool muse_on_shortcut_map(const MuseOnActionEvent *action,
                          MuseOnShortcutInstruction *shortcut) {
  MuseOnShortcutKey key;
  MuseOnShortcutOperation operation;

  if (!action || !shortcut || !shortcut_key_for_action(action->id, &key)) {
    return false;
  }
  if (action->id == MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD) {
    if (action->phase == MUSE_ON_ACTION_BEGIN) {
      operation = MUSE_ON_SHORTCUT_KEY_DOWN;
    } else if (action->phase == MUSE_ON_ACTION_END) {
      operation = MUSE_ON_SHORTCUT_KEY_UP;
    } else {
      return false;
    }
  } else {
    if (action->phase != MUSE_ON_ACTION_TRIGGER) return false;
    operation = MUSE_ON_SHORTCUT_TAP;
  }
  shortcut->command = true;
  shortcut->option = true;
  shortcut->control = true;
  shortcut->shift = true;
  shortcut->key = key;
  shortcut->operation = operation;
  return true;
}

const char *muse_on_shortcut_key_string(MuseOnShortcutKey key) {
  static const char *const names[] = {
      "1", "2", "3", "4", "5", "6", "7", "8", "9",
      "A", "B", "C", "D", "E", "F", "G"};
  if ((unsigned int)key >= sizeof(names) / sizeof(names[0])) return "unknown";
  return names[key];
}

const char *muse_on_shortcut_operation_string(MuseOnShortcutOperation operation) {
  switch (operation) {
    case MUSE_ON_SHORTCUT_TAP: return "tap";
    case MUSE_ON_SHORTCUT_KEY_DOWN: return "key_down";
    case MUSE_ON_SHORTCUT_KEY_UP: return "key_up";
  }
  return "unknown";
}
