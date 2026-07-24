#include "muse_on_action_map.h"

static bool set_action(MuseOnActionEvent *action, MuseOnActionId id,
                       MuseOnActionPhase phase, MuseOnEventName source) {
  if (!action) return false;
  action->id = id;
  action->phase = phase;
  action->source = source;
  return true;
}

static uint64_t trigger_debounce_ns(MuseOnEventName source) {
  switch (source) {
    case MUSE_ON_EVENT_WHITE1_DOWN: return 250000000ULL;
    case MUSE_ON_EVENT_BLACK2_DOWN: return 40000000ULL;
    case MUSE_ON_EVENT_WHITE3_DOWN: return 750000000ULL;
    case MUSE_ON_EVENT_WHITE5_DOWN: return 80000000ULL;
    case MUSE_ON_EVENT_WHITE7_DOWN: return 50000000ULL;
    default: return 20000000ULL;
  }
}

void muse_on_action_router_init(MuseOnActionRouter *router, MuseOnProfile profile) {
  unsigned int index;

  if (!router) return;
  router->profile = profile;
  for (index = 0; index < MUSE_ON_ACTION_COUNT; index++) {
    router->trigger_seen[index] = false;
    router->trigger_last_ns[index] = 0;
  }
  router->hold_active = false;
  router->hold_pending = false;
  router->hold_pending_active = false;
  router->hold_pending_since_ns = 0;
  router->hold_pending_source = MUSE_ON_EVENT_NONE;
}

bool muse_on_action_router_tick(MuseOnActionRouter *router, uint64_t timestamp_ns,
                                MuseOnActionEvent *action) {
  MuseOnActionPhase phase;

  if (!router || !router->hold_pending ||
      timestamp_ns < router->hold_pending_since_ns ||
      timestamp_ns - router->hold_pending_since_ns < MUSE_ON_HOLD_DEBOUNCE_NS) {
    return false;
  }
  router->hold_active = router->hold_pending_active;
  router->hold_pending = false;
  phase = router->hold_active ? MUSE_ON_ACTION_BEGIN : MUSE_ON_ACTION_END;
  return set_action(action, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD, phase,
                    router->hold_pending_source);
}

bool muse_on_action_router_route(MuseOnActionRouter *router,
                                 MuseOnEventName source, uint64_t timestamp_ns,
                                 MuseOnActionEvent *action) {
  MuseOnActionEvent mapped;
  unsigned int id;

  if (!router || !action || !muse_on_map_event(router->profile, source, &mapped)) {
    return false;
  }
  if (mapped.phase == MUSE_ON_ACTION_TRIGGER) {
    id = (unsigned int)mapped.id;
    if (id >= MUSE_ON_ACTION_COUNT ||
        (router->trigger_seen[id] &&
         (timestamp_ns < router->trigger_last_ns[id] ||
          timestamp_ns - router->trigger_last_ns[id] < trigger_debounce_ns(source)))) {
      return false;
    }
    router->trigger_seen[id] = true;
    router->trigger_last_ns[id] = timestamp_ns;
    *action = mapped;
    return true;
  }

  {
    bool desired_active = mapped.phase == MUSE_ON_ACTION_BEGIN;
    if (desired_active == router->hold_active) {
      if (router->hold_pending && router->hold_pending_active != desired_active) {
        router->hold_pending = false;
      }
      return false;
    }
    if (!router->hold_pending || router->hold_pending_active != desired_active) {
      router->hold_pending = true;
      router->hold_pending_active = desired_active;
      router->hold_pending_since_ns = timestamp_ns;
      router->hold_pending_source = source;
    }
  }
  return false;
}

bool muse_on_map_event(MuseOnProfile profile, MuseOnEventName source,
                       MuseOnActionEvent *action) {
  if (!action) return false;
  switch (source) {
    case MUSE_ON_EVENT_WHITE1_DOWN:
      return set_action(action, MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_BLACK2_DOWN:
      return set_action(action, MUSE_ON_ACTION_APPROVAL_APPROVE,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_WHITE3_DOWN:
      return set_action(action, MUSE_ON_ACTION_APPROVAL_DECLINE,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_BLACK4_DOWN:
      return set_action(action, MUSE_ON_ACTION_FORK_THREAD,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_WHITE5_DOWN:
      return set_action(action, MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_BLACK6_DOWN:
      return set_action(action, MUSE_ON_ACTION_COMPOSER_SUBMIT,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_WHITE7_DOWN:
      return set_action(action, MUSE_ON_ACTION_TOGGLE_REVIEW_TAB,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_BLACK8_DOWN:
      return profile == MUSE_ON_PROFILE_PEDAL
          ? set_action(action, MUSE_ON_ACTION_ENVIRONMENT_ACTION_1,
                       MUSE_ON_ACTION_TRIGGER, source)
          : set_action(action, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
                       MUSE_ON_ACTION_BEGIN, source);
    case MUSE_ON_EVENT_BLACK8_UP:
      return profile == MUSE_ON_PROFILE_CONTROLLER_ONLY &&
          set_action(action, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
                     MUSE_ON_ACTION_END, source);
    case MUSE_ON_EVENT_PEDAL_DOWN:
      return profile == MUSE_ON_PROFILE_PEDAL &&
          set_action(action, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
                     MUSE_ON_ACTION_BEGIN, source);
    case MUSE_ON_EVENT_PEDAL_UP:
      return profile == MUSE_ON_PROFILE_PEDAL &&
          set_action(action, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
                     MUSE_ON_ACTION_END, source);
    case MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED:
      return set_action(action, MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED:
      return set_action(action, MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_LEFT_BALL_NORTH_ENGAGED:
      return set_action(action, MUSE_ON_ACTION_PREVIOUS_THREAD,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_LEFT_BALL_SOUTH_ENGAGED:
      return set_action(action, MUSE_ON_ACTION_NEXT_THREAD,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_RIGHT_BALL_WEST_ENGAGED:
      return set_action(action, MUSE_ON_ACTION_NAVIGATE_BACK,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_RIGHT_BALL_EAST_ENGAGED:
      return set_action(action, MUSE_ON_ACTION_NAVIGATE_FORWARD,
                        MUSE_ON_ACTION_TRIGGER, source);
    case MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_ENGAGED:
      return set_action(action, MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER,
                        MUSE_ON_ACTION_TRIGGER, source);
    default:
      return false;
  }
}

const char *muse_on_profile_string(MuseOnProfile profile) {
  return profile == MUSE_ON_PROFILE_PEDAL ? "pedal" : "controller_only";
}

const char *muse_on_action_phase_string(MuseOnActionPhase phase) {
  switch (phase) {
    case MUSE_ON_ACTION_TRIGGER: return "trigger";
    case MUSE_ON_ACTION_BEGIN: return "begin";
    case MUSE_ON_ACTION_END: return "end";
  }
  return "unknown";
}

const char *muse_on_action_id_string(MuseOnActionId action) {
  static const char *const names[] = {
      "composer.toggleFastMode", "approval.approve", "approval.decline",
      "forkThread", "copyConversationMarkdown", "composer.submit",
      "toggleReviewTab", "globalDictationHold", "environmentAction1",
      "composer.decreaseReasoningEffort", "composer.increaseReasoningEffort",
      "previousThread", "nextThread", "navigateBack", "navigateForward",
      "composer.openModelPicker"};
  if ((unsigned int)action >= sizeof(names) / sizeof(names[0])) return "unknown";
  return names[action];
}

const char *muse_on_event_name_string(MuseOnEventName event) {
  switch (event) {
    case MUSE_ON_EVENT_WHITE1_DOWN: return "white1.down";
    case MUSE_ON_EVENT_WHITE1_UP: return "white1.up";
    case MUSE_ON_EVENT_BLACK2_DOWN: return "black2.down";
    case MUSE_ON_EVENT_BLACK2_UP: return "black2.up";
    case MUSE_ON_EVENT_BLACK6_DOWN: return "black6.down";
    case MUSE_ON_EVENT_BLACK6_UP: return "black6.up";
    case MUSE_ON_EVENT_BLACK8_DOWN: return "black8.down";
    case MUSE_ON_EVENT_BLACK8_UP: return "black8.up";
    case MUSE_ON_EVENT_PEDAL_DOWN: return "pedal.down";
    case MUSE_ON_EVENT_PEDAL_UP: return "pedal.up";
    case MUSE_ON_EVENT_WHITE3_DOWN: return "white3.down";
    case MUSE_ON_EVENT_WHITE3_UP: return "white3.up";
    case MUSE_ON_EVENT_BLACK4_DOWN: return "black4.down";
    case MUSE_ON_EVENT_BLACK4_UP: return "black4.up";
    case MUSE_ON_EVENT_WHITE5_DOWN: return "white5.down";
    case MUSE_ON_EVENT_WHITE5_UP: return "white5.up";
    case MUSE_ON_EVENT_WHITE7_DOWN: return "white7.down";
    case MUSE_ON_EVENT_WHITE7_UP: return "white7.up";
    case MUSE_ON_EVENT_LEFT_BALL_NORTH_ENGAGED: return "leftBall.north.engaged";
    case MUSE_ON_EVENT_LEFT_BALL_NORTH_RELEASED: return "leftBall.north.released";
    case MUSE_ON_EVENT_LEFT_BALL_SOUTH_ENGAGED: return "leftBall.south.engaged";
    case MUSE_ON_EVENT_LEFT_BALL_SOUTH_RELEASED: return "leftBall.south.released";
    case MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_ENGAGED: return "rightBall.vertical.engaged";
    case MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_RELEASED: return "rightBall.vertical.released";
    case MUSE_ON_EVENT_RIGHT_BALL_WEST_ENGAGED: return "rightBall.west.engaged";
    case MUSE_ON_EVENT_RIGHT_BALL_WEST_RELEASED: return "rightBall.west.released";
    case MUSE_ON_EVENT_RIGHT_BALL_EAST_ENGAGED: return "rightBall.east.engaged";
    case MUSE_ON_EVENT_RIGHT_BALL_EAST_RELEASED: return "rightBall.east.released";
    case MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED: return "turntable.clockwise.engaged";
    case MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_RELEASED: return "turntable.clockwise.released";
    case MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED: return "turntable.counterclockwise.engaged";
    case MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_RELEASED: return "turntable.counterclockwise.released";
    default: return "unknown";
  }
}
