#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../muse_on_shortcut_map.h"

typedef struct {
  MuseOnActionId action;
  MuseOnActionPhase phase;
  MuseOnShortcutKey key;
  MuseOnShortcutOperation operation;
} ShortcutCase;

static void test_all_confirmed_shortcuts_use_hyper_chord(void) {
  const ShortcutCase cases[] = {
      {MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_1, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_APPROVAL_APPROVE, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_2, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_APPROVAL_DECLINE, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_3, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_FORK_THREAD, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_4, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_5, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_COMPOSER_SUBMIT, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_6, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_TOGGLE_REVIEW_TAB, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_7, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD, MUSE_ON_ACTION_BEGIN,
       MUSE_ON_SHORTCUT_KEY_8, MUSE_ON_SHORTCUT_KEY_DOWN},
      {MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD, MUSE_ON_ACTION_END,
       MUSE_ON_SHORTCUT_KEY_8, MUSE_ON_SHORTCUT_KEY_UP},
      {MUSE_ON_ACTION_ENVIRONMENT_ACTION_1, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_9, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_A, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_B, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_PREVIOUS_THREAD, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_C, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_NEXT_THREAD, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_D, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_NAVIGATE_BACK, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_E, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_NAVIGATE_FORWARD, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_F, MUSE_ON_SHORTCUT_TAP},
      {MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER, MUSE_ON_ACTION_TRIGGER,
       MUSE_ON_SHORTCUT_KEY_G, MUSE_ON_SHORTCUT_TAP},
  };
  size_t index;

  for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
    MuseOnActionEvent action = {cases[index].action, cases[index].phase,
                                MUSE_ON_EVENT_NONE};
    MuseOnShortcutInstruction shortcut;
    assert(muse_on_shortcut_map(&action, &shortcut));
    assert(shortcut.command && shortcut.option && shortcut.control && shortcut.shift);
    assert(shortcut.key == cases[index].key);
    assert(shortcut.operation == cases[index].operation);
  }
}

static void test_invalid_action_phase_pairs_fail_closed(void) {
  MuseOnActionEvent action = {MUSE_ON_ACTION_COMPOSER_SUBMIT,
                              MUSE_ON_ACTION_BEGIN, MUSE_ON_EVENT_NONE};
  MuseOnShortcutInstruction shortcut = {
      false, false, false, false,
      MUSE_ON_SHORTCUT_KEY_G, MUSE_ON_SHORTCUT_KEY_UP};

  assert(!muse_on_shortcut_map(&action, &shortcut));
  assert(!shortcut.command && !shortcut.option && !shortcut.control && !shortcut.shift);
  assert(shortcut.key == MUSE_ON_SHORTCUT_KEY_G);
  assert(shortcut.operation == MUSE_ON_SHORTCUT_KEY_UP);
  action.id = MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD;
  action.phase = MUSE_ON_ACTION_TRIGGER;
  assert(!muse_on_shortcut_map(&action, &shortcut));
  action.id = (MuseOnActionId)999;
  assert(!muse_on_shortcut_map(&action, &shortcut));
}

static void test_stable_display_strings(void) {
  assert(strcmp(muse_on_shortcut_key_string(MUSE_ON_SHORTCUT_KEY_1), "1") == 0);
  assert(strcmp(muse_on_shortcut_key_string(MUSE_ON_SHORTCUT_KEY_G), "G") == 0);
  assert(strcmp(muse_on_shortcut_operation_string(MUSE_ON_SHORTCUT_TAP), "tap") == 0);
  assert(strcmp(muse_on_shortcut_operation_string(MUSE_ON_SHORTCUT_KEY_DOWN),
                "key_down") == 0);
  assert(strcmp(muse_on_shortcut_operation_string(MUSE_ON_SHORTCUT_KEY_UP),
                "key_up") == 0);
}

static void test_hold_dispatch_guard_is_production_backed(void) {
  MuseOnActionEvent begin = {MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
                             MUSE_ON_ACTION_BEGIN, MUSE_ON_EVENT_BLACK8_DOWN};
  MuseOnActionEvent end = {MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
                           MUSE_ON_ACTION_END, MUSE_ON_EVENT_BLACK8_UP};
  MuseOnActionEvent submit = {MUSE_ON_ACTION_COMPOSER_SUBMIT,
                              MUSE_ON_ACTION_TRIGGER,
                              MUSE_ON_EVENT_BLACK6_DOWN};

  assert(muse_on_shortcut_dispatch_decide(&begin, false) ==
         MUSE_ON_SHORTCUT_DISPATCH_POST);
  assert(muse_on_shortcut_dispatch_decide(&begin, true) ==
         MUSE_ON_SHORTCUT_DISPATCH_IGNORE_DUPLICATE_HOLD_BEGIN);
  assert(muse_on_shortcut_dispatch_decide(&end, false) ==
         MUSE_ON_SHORTCUT_DISPATCH_IGNORE_ORPHAN_HOLD_END);
  assert(muse_on_shortcut_dispatch_decide(&end, true) ==
         MUSE_ON_SHORTCUT_DISPATCH_POST);
  assert(muse_on_shortcut_dispatch_decide(&submit, false) ==
         MUSE_ON_SHORTCUT_DISPATCH_POST);
}

int main(void) {
  test_all_confirmed_shortcuts_use_hyper_chord();
  test_invalid_action_phase_pairs_fail_closed();
  test_stable_display_strings();
  test_hold_dispatch_guard_is_production_backed();
  return 0;
}
