#include <assert.h>
#include <stddef.h>

#import <Carbon/Carbon.h>

#include "../muse_on_platform.h"

static void test_hold_shortcut_emits_modifier_transitions(void) {
  const MuseOnHyperKeyEvent expected_begin[] = {
      {kVK_Command, true, kCGEventFlagMaskCommand},
      {kVK_Option, true, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate},
      {kVK_Control, true, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate |
                               kCGEventFlagMaskControl},
      {kVK_Shift, true, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate |
                             kCGEventFlagMaskControl | kCGEventFlagMaskShift},
      {kVK_ANSI_8, true, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate |
                              kCGEventFlagMaskControl | kCGEventFlagMaskShift},
  };
  const MuseOnHyperKeyEvent expected_end[] = {
      {kVK_ANSI_8, false, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate |
                               kCGEventFlagMaskControl | kCGEventFlagMaskShift},
      {kVK_Shift, false, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate |
                              kCGEventFlagMaskControl},
      {kVK_Control, false, kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate},
      {kVK_Option, false, kCGEventFlagMaskCommand},
      {kVK_Command, false, 0},
  };
  MuseOnHyperKeyEvent actual[5];
  size_t index;

  assert(muse_on_hyper_key_event_sequence(
      kVK_ANSI_8, MUSE_ON_POST_HYPER_KEY_DOWN, actual, 5) == 5);
  for (index = 0; index < 5; index++) {
    assert(actual[index].mac_virtual_key == expected_begin[index].mac_virtual_key);
    assert(actual[index].key_down == expected_begin[index].key_down);
    assert(actual[index].flags == expected_begin[index].flags);
  }

  assert(muse_on_hyper_key_event_sequence(
      kVK_ANSI_8, MUSE_ON_POST_HYPER_KEY_UP, actual, 5) == 5);
  for (index = 0; index < 5; index++) {
    assert(actual[index].mac_virtual_key == expected_end[index].mac_virtual_key);
    assert(actual[index].key_down == expected_end[index].key_down);
    assert(actual[index].flags == expected_end[index].flags);
  }
}

static void test_only_the_codex_bundle_identifier_matches(void) {
  assert(muse_on_bundle_id_is_codex("com.openai.codex"));
  assert(!muse_on_bundle_id_is_codex(NULL));
  assert(!muse_on_bundle_id_is_codex("com.openai.chatgpt"));
  assert(!muse_on_bundle_id_is_codex("com.openai.codex.helper"));
  assert(!muse_on_bundle_id_is_codex("COM.OPENAI.CODEX"));
}

static void test_shortcut_keys_map_to_physical_macos_keys(void) {
  const struct {
    MuseOnShortcutKey key;
    uint16_t expected;
  } cases[] = {
      {MUSE_ON_SHORTCUT_KEY_1, kVK_ANSI_1},
      {MUSE_ON_SHORTCUT_KEY_2, kVK_ANSI_2},
      {MUSE_ON_SHORTCUT_KEY_3, kVK_ANSI_3},
      {MUSE_ON_SHORTCUT_KEY_4, kVK_ANSI_4},
      {MUSE_ON_SHORTCUT_KEY_5, kVK_ANSI_5},
      {MUSE_ON_SHORTCUT_KEY_6, kVK_ANSI_6},
      {MUSE_ON_SHORTCUT_KEY_7, kVK_ANSI_7},
      {MUSE_ON_SHORTCUT_KEY_8, kVK_ANSI_8},
      {MUSE_ON_SHORTCUT_KEY_9, kVK_ANSI_9},
      {MUSE_ON_SHORTCUT_KEY_A, kVK_ANSI_A},
      {MUSE_ON_SHORTCUT_KEY_B, kVK_ANSI_B},
      {MUSE_ON_SHORTCUT_KEY_C, kVK_ANSI_C},
      {MUSE_ON_SHORTCUT_KEY_D, kVK_ANSI_D},
      {MUSE_ON_SHORTCUT_KEY_E, kVK_ANSI_E},
      {MUSE_ON_SHORTCUT_KEY_F, kVK_ANSI_F},
      {MUSE_ON_SHORTCUT_KEY_G, kVK_ANSI_G},
  };
  size_t index;

  for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
    uint16_t actual = UINT16_MAX;
    assert(muse_on_mac_virtual_key(cases[index].key, &actual));
    assert(actual == cases[index].expected);
  }

  {
    uint16_t unchanged = UINT16_MAX;
    assert(!muse_on_mac_virtual_key((MuseOnShortcutKey)999, &unchanged));
    assert(unchanged == UINT16_MAX);
    assert(!muse_on_mac_virtual_key(MUSE_ON_SHORTCUT_KEY_1, NULL));
  }
}

int main(void) {
  test_only_the_codex_bundle_identifier_matches();
  test_shortcut_keys_map_to_physical_macos_keys();
  test_hold_shortcut_emits_modifier_transitions();
  return 0;
}
