#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../muse_on_action_map.h"

typedef struct {
  MuseOnProfile profile;
  MuseOnEventName source;
  MuseOnActionId action;
  MuseOnActionPhase phase;
} MappingCase;

static void test_confirmed_controller_mappings(void) {
  const MappingCase cases[] = {
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_WHITE1_DOWN,
       MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_BLACK2_DOWN,
       MUSE_ON_ACTION_APPROVAL_APPROVE, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_WHITE3_DOWN,
       MUSE_ON_ACTION_APPROVAL_DECLINE, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_BLACK4_DOWN,
       MUSE_ON_ACTION_FORK_THREAD, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_WHITE5_DOWN,
       MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_BLACK6_DOWN,
       MUSE_ON_ACTION_COMPOSER_SUBMIT, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_WHITE7_DOWN,
       MUSE_ON_ACTION_TOGGLE_REVIEW_TAB, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_BLACK8_DOWN,
       MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD, MUSE_ON_ACTION_BEGIN},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_BLACK8_UP,
       MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD, MUSE_ON_ACTION_END},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED,
       MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED,
       MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_LEFT_BALL_NORTH_ENGAGED,
       MUSE_ON_ACTION_PREVIOUS_THREAD, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_LEFT_BALL_SOUTH_ENGAGED,
       MUSE_ON_ACTION_NEXT_THREAD, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_RIGHT_BALL_WEST_ENGAGED,
       MUSE_ON_ACTION_NAVIGATE_BACK, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_RIGHT_BALL_EAST_ENGAGED,
       MUSE_ON_ACTION_NAVIGATE_FORWARD, MUSE_ON_ACTION_TRIGGER},
      {MUSE_ON_PROFILE_CONTROLLER_ONLY, MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_ENGAGED,
       MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER, MUSE_ON_ACTION_TRIGGER},
  };
  size_t index;
  MuseOnActionEvent mapped;

  for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
    assert(muse_on_map_event(cases[index].profile, cases[index].source, &mapped));
    assert(mapped.id == cases[index].action);
    assert(mapped.phase == cases[index].phase);
    assert(mapped.source == cases[index].source);
  }
}

static void test_pedal_profile_and_ignored_events(void) {
  MuseOnActionEvent mapped;

  assert(muse_on_map_event(MUSE_ON_PROFILE_PEDAL, MUSE_ON_EVENT_PEDAL_DOWN, &mapped));
  assert(mapped.id == MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD);
  assert(mapped.phase == MUSE_ON_ACTION_BEGIN);
  assert(muse_on_map_event(MUSE_ON_PROFILE_PEDAL, MUSE_ON_EVENT_PEDAL_UP, &mapped));
  assert(mapped.phase == MUSE_ON_ACTION_END);
  assert(muse_on_map_event(MUSE_ON_PROFILE_PEDAL, MUSE_ON_EVENT_BLACK8_DOWN, &mapped));
  assert(mapped.id == MUSE_ON_ACTION_ENVIRONMENT_ACTION_1);
  assert(mapped.phase == MUSE_ON_ACTION_TRIGGER);

  assert(!muse_on_map_event(MUSE_ON_PROFILE_CONTROLLER_ONLY,
                            MUSE_ON_EVENT_PEDAL_DOWN, &mapped));
  assert(!muse_on_map_event(MUSE_ON_PROFILE_PEDAL, MUSE_ON_EVENT_BLACK8_UP, &mapped));
  assert(!muse_on_map_event(MUSE_ON_PROFILE_PEDAL,
                            MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_RELEASED, &mapped));
  assert(!muse_on_map_event(MUSE_ON_PROFILE_PEDAL, MUSE_ON_EVENT_WHITE1_UP, &mapped));
}

static void test_stable_strings(void) {
  const struct {
    MuseOnActionId id;
    const char *name;
  } actions[] = {
      {MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE, "composer.toggleFastMode"},
      {MUSE_ON_ACTION_APPROVAL_APPROVE, "approval.approve"},
      {MUSE_ON_ACTION_APPROVAL_DECLINE, "approval.decline"},
      {MUSE_ON_ACTION_FORK_THREAD, "forkThread"},
      {MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN, "copyConversationMarkdown"},
      {MUSE_ON_ACTION_COMPOSER_SUBMIT, "composer.submit"},
      {MUSE_ON_ACTION_TOGGLE_REVIEW_TAB, "toggleReviewTab"},
      {MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD, "globalDictationHold"},
      {MUSE_ON_ACTION_ENVIRONMENT_ACTION_1, "environmentAction1"},
      {MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT,
       "composer.decreaseReasoningEffort"},
      {MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT,
       "composer.increaseReasoningEffort"},
      {MUSE_ON_ACTION_PREVIOUS_THREAD, "previousThread"},
      {MUSE_ON_ACTION_NEXT_THREAD, "nextThread"},
      {MUSE_ON_ACTION_NAVIGATE_BACK, "navigateBack"},
      {MUSE_ON_ACTION_NAVIGATE_FORWARD, "navigateForward"},
      {MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER, "composer.openModelPicker"},
  };
  size_t index;

  assert(strcmp(muse_on_profile_string(MUSE_ON_PROFILE_CONTROLLER_ONLY),
                "controller_only") == 0);
  assert(strcmp(muse_on_profile_string(MUSE_ON_PROFILE_PEDAL), "pedal") == 0);
  for (index = 0; index < sizeof(actions) / sizeof(actions[0]); index++) {
    assert(strcmp(muse_on_action_id_string(actions[index].id),
                  actions[index].name) == 0);
  }
  assert(strcmp(muse_on_action_phase_string(MUSE_ON_ACTION_TRIGGER),
                "trigger") == 0);
  assert(strcmp(muse_on_action_phase_string(MUSE_ON_ACTION_BEGIN), "begin") == 0);
  assert(strcmp(muse_on_action_phase_string(MUSE_ON_ACTION_END), "end") == 0);
  assert(strcmp(muse_on_event_name_string(MUSE_ON_EVENT_BLACK6_DOWN),
                "black6.down") == 0);
}

static void test_trigger_bounce_uses_measured_source_thresholds(void) {
  MuseOnActionRouter router;
  MuseOnActionEvent action;

  muse_on_action_router_init(&router, MUSE_ON_PROFILE_CONTROLLER_ONLY);
  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_DOWN,
                                     1000000000ULL, &action));
  assert(action.id == MUSE_ON_ACTION_APPROVAL_DECLINE);
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_UP,
                                      1040000000ULL, &action));
  /* One physical press produced six down edges over about 336 ms. */
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_DOWN,
                                      1056000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_UP,
                                      1096000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_DOWN,
                                      1128000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_DOWN,
                                      1224000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_DOWN,
                                      1280000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_DOWN,
                                      1336000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_DOWN,
                                      1749999999ULL, &action));
  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE3_DOWN,
                                     1750000000ULL, &action));

  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE5_DOWN,
                                     2000000000ULL, &action));
  assert(action.id == MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN);
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE5_DOWN,
                                      2002000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE5_DOWN,
                                      2004000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE5_DOWN,
                                      2006000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE5_DOWN,
                                      2079999999ULL, &action));
  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE5_DOWN,
                                     2080000000ULL, &action));

  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_BLACK2_DOWN,
                                     3000000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_BLACK2_DOWN,
                                      3039999999ULL, &action));
  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_BLACK2_DOWN,
                                     3040000000ULL, &action));
  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE7_DOWN,
                                     4000000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE7_DOWN,
                                      4049999999ULL, &action));
  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE7_DOWN,
                                     4050000000ULL, &action));
}

static void test_white1_measured_press_dispatches_once(void) {
  MuseOnActionRouter router;
  MuseOnActionEvent action;

  muse_on_action_router_init(&router, MUSE_ON_PROFILE_CONTROLLER_ONLY);
  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE1_DOWN,
                                     1000000000ULL, &action));
  assert(action.id == MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE);

  /*
   * Active-mode hardware capture from one physical press:
   * down -> up after about 72 ms -> down again after about 108 ms.
   */
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE1_UP,
                                      1072000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE1_DOWN,
                                      1108000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE1_UP,
                                      1180000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE1_DOWN,
                                      1249999999ULL, &action));
  assert(muse_on_action_router_route(&router, MUSE_ON_EVENT_WHITE1_DOWN,
                                     1250000000ULL, &action));
}

static void test_turntable_direction_repeats_until_released(void) {
  MuseOnActionRouter router;
  MuseOnActionEvent action;

  muse_on_action_router_init(&router, MUSE_ON_PROFILE_CONTROLLER_ONLY);
  assert(muse_on_action_router_route(
      &router, MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED,
      1000000000ULL, &action));
  assert(action.id == MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT);
  assert(!muse_on_action_router_tick(&router, 1599999999ULL, &action));
  assert(muse_on_action_router_tick(&router, 1600000000ULL, &action));
  assert(action.id == MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT);
  assert(action.phase == MUSE_ON_ACTION_TRIGGER);
  assert(action.source == MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED);
  assert(!muse_on_action_router_tick(&router, 1899999999ULL, &action));
  assert(muse_on_action_router_tick(&router, 1900000000ULL, &action));
  assert(!muse_on_action_router_route(
      &router, MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_RELEASED,
      1910000000ULL, &action));
  assert(!muse_on_action_router_tick(&router, 1990000000ULL, &action));

  assert(muse_on_action_router_route(
      &router, MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED,
      2000000000ULL, &action));
  assert(action.id == MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT);
  assert(!muse_on_action_router_tick(&router, 2599999999ULL, &action));
  assert(muse_on_action_router_tick(&router, 2600000000ULL, &action));
  assert(action.id == MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT);
}

static void test_pedal_hold_bounce_requires_stable_begin_and_end(void) {
  MuseOnActionRouter router;
  MuseOnActionEvent action;

  muse_on_action_router_init(&router, MUSE_ON_PROFILE_PEDAL);
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_DOWN,
                                      1000000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_UP,
                                      1010000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_DOWN,
                                      1020000000ULL, &action));
  assert(!muse_on_action_router_tick(&router, 1039999999ULL, &action));
  assert(muse_on_action_router_tick(&router, 1040000000ULL, &action));
  assert(action.id == MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD);
  assert(action.phase == MUSE_ON_ACTION_BEGIN);
  assert(action.source == MUSE_ON_EVENT_PEDAL_DOWN);

  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_UP,
                                      1200000000ULL, &action));
  assert(!muse_on_action_router_tick(&router, 1219999999ULL, &action));
  assert(muse_on_action_router_tick(&router, 1220000000ULL, &action));
  assert(action.phase == MUSE_ON_ACTION_END);
  assert(action.source == MUSE_ON_EVENT_PEDAL_UP);
}

static void test_hold_pending_begin_is_cancelled_by_release(void) {
  MuseOnActionRouter router;
  MuseOnActionEvent action;

  muse_on_action_router_init(&router, MUSE_ON_PROFILE_PEDAL);
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_DOWN,
                                      1000000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_UP,
                                      1010000000ULL, &action));
  assert(!muse_on_action_router_tick(&router, 1100000000ULL, &action));
}

static void test_hold_duplicate_begin_and_repress_cancel_pending_release(void) {
  MuseOnActionRouter router;
  MuseOnActionEvent action;

  muse_on_action_router_init(&router, MUSE_ON_PROFILE_PEDAL);
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_DOWN,
                                      1000000000ULL, &action));
  assert(muse_on_action_router_tick(&router, 1020000000ULL, &action));
  assert(action.phase == MUSE_ON_ACTION_BEGIN);

  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_DOWN,
                                      1030000000ULL, &action));
  assert(!muse_on_action_router_tick(&router, 1050000000ULL, &action));

  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_UP,
                                      1060000000ULL, &action));
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_DOWN,
                                      1070000000ULL, &action));
  assert(!muse_on_action_router_tick(&router, 1100000000ULL, &action));
}

static void test_hold_timestamp_regression_cannot_emit(void) {
  MuseOnActionRouter router;
  MuseOnActionEvent action;

  muse_on_action_router_init(&router, MUSE_ON_PROFILE_PEDAL);
  assert(!muse_on_action_router_route(&router, MUSE_ON_EVENT_PEDAL_DOWN,
                                      1000000000ULL, &action));
  assert(!muse_on_action_router_tick(&router, 999999999ULL, &action));
  assert(muse_on_action_router_tick(&router, 1020000000ULL, &action));
  assert(action.phase == MUSE_ON_ACTION_BEGIN);
}

int main(void) {
  test_confirmed_controller_mappings();
  test_pedal_profile_and_ignored_events();
  test_stable_strings();
  test_trigger_bounce_uses_measured_source_thresholds();
  test_white1_measured_press_dispatches_once();
  test_turntable_direction_repeats_until_released();
  test_pedal_hold_bounce_requires_stable_begin_and_end();
  test_hold_pending_begin_is_cancelled_by_release();
  test_hold_duplicate_begin_and_repress_cancel_pending_release();
  test_hold_timestamp_regression_cannot_emit();
  return 0;
}
