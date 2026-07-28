#include "muse_on_action_map.h"

typedef struct {
  const char *identifier;
  const char *display_name;
} MuseOnActionInfo;

static const MuseOnActionInfo kActionInfo[MUSE_ON_ACTION_COUNT] = {
    {"composer.toggleFastMode", "Toggle Fast Mode"},
    {"approval.approve", "Approve"},
    {"approval.decline", "Decline"},
    {"forkThread", "Fork Thread"},
    {"copyConversationMarkdown", "Copy Conversation as Markdown"},
    {"composer.submit", "Submit"},
    {"toggleReviewTab", "Toggle Review Tab"},
    {"globalDictationHold", "Hold Global Dictation"},
    {"environmentAction1", "Environment Action 1"},
    {"composer.decreaseReasoningEffort", "Decrease Reasoning Effort"},
    {"composer.increaseReasoningEffort", "Increase Reasoning Effort"},
    {"previousThread", "Previous Thread"},
    {"nextThread", "Next Thread"},
    {"navigateBack", "Navigate Back"},
    {"navigateForward", "Navigate Forward"},
    {"composer.openModelPicker", "Open Model Picker"},
};

#define TRIGGER_PROFILE(action) \
  {true, action, MUSE_ON_ACTION_TRIGGER, false, action, MUSE_ON_ACTION_TRIGGER}
#define HOLD_PROFILE(action) \
  {true, action, MUSE_ON_ACTION_BEGIN, true, action, MUSE_ON_ACTION_END}
#define UNAVAILABLE_PROFILE \
  {false, MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE, MUSE_ON_ACTION_TRIGGER, \
   false, MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE, MUSE_ON_ACTION_TRIGGER}

static const MuseOnControlMapping kControlMap[] = {
    {"turntable.clockwise", "Turntable clockwise",
     MUSE_ON_CONTROL_GROUP_TURNTABLE, MUSE_ON_CONTROL_SHAPE_TURNTABLE,
     0.066667f, 0.126126f, 0.24f, 0.486486f,
     MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED,
     MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_RELEASED, 20000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT),
      TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT)}},
    {"turntable.counterclockwise", "Turntable counterclockwise",
     MUSE_ON_CONTROL_GROUP_TURNTABLE, MUSE_ON_CONTROL_SHAPE_TURNTABLE,
     0.066667f, 0.126126f, 0.24f, 0.486486f,
     MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED,
     MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_RELEASED, 20000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT),
      TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT)}},
    {"white1", "White button 1", MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS,
     MUSE_ON_CONTROL_SHAPE_BUTTON, 0.431111f, 0.409910f, 0.086667f, 0.139640f,
     MUSE_ON_EVENT_WHITE1_DOWN, MUSE_ON_EVENT_WHITE1_UP, 250000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE),
      TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE)}},
    {"white3", "White button 3", MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS,
     MUSE_ON_CONTROL_SHAPE_BUTTON, 0.533333f, 0.409910f, 0.086667f, 0.139640f,
     MUSE_ON_EVENT_WHITE3_DOWN, MUSE_ON_EVENT_WHITE3_UP, 750000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_APPROVAL_DECLINE),
      TRIGGER_PROFILE(MUSE_ON_ACTION_APPROVAL_DECLINE)}},
    {"white5", "White button 5", MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS,
     MUSE_ON_CONTROL_SHAPE_BUTTON, 0.635556f, 0.409910f, 0.086667f, 0.139640f,
     MUSE_ON_EVENT_WHITE5_DOWN, MUSE_ON_EVENT_WHITE5_UP, 80000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN),
      TRIGGER_PROFILE(MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN)}},
    {"white7", "White button 7", MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS,
     MUSE_ON_CONTROL_SHAPE_BUTTON, 0.737778f, 0.409910f, 0.086667f, 0.139640f,
     MUSE_ON_EVENT_WHITE7_DOWN, MUSE_ON_EVENT_WHITE7_UP, 50000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_TOGGLE_REVIEW_TAB),
      TRIGGER_PROFILE(MUSE_ON_ACTION_TOGGLE_REVIEW_TAB)}},
    {"black2", "Black button 2", MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS,
     MUSE_ON_CONTROL_SHAPE_BUTTON, 0.482222f, 0.193694f, 0.086667f, 0.139640f,
     MUSE_ON_EVENT_BLACK2_DOWN, MUSE_ON_EVENT_BLACK2_UP, 40000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_APPROVAL_APPROVE),
      TRIGGER_PROFILE(MUSE_ON_ACTION_APPROVAL_APPROVE)}},
    {"black4", "Black button 4", MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS,
     MUSE_ON_CONTROL_SHAPE_BUTTON, 0.584444f, 0.193694f, 0.086667f, 0.139640f,
     MUSE_ON_EVENT_BLACK4_DOWN, MUSE_ON_EVENT_BLACK4_UP, 20000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_FORK_THREAD),
      TRIGGER_PROFILE(MUSE_ON_ACTION_FORK_THREAD)}},
    {"black6", "Black button 6", MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS,
     MUSE_ON_CONTROL_SHAPE_BUTTON, 0.686667f, 0.193694f, 0.086667f, 0.139640f,
     MUSE_ON_EVENT_BLACK6_DOWN, MUSE_ON_EVENT_BLACK6_UP, 20000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_SUBMIT),
      TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_SUBMIT)}},
    {"black8", "Black button 8", MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS,
     MUSE_ON_CONTROL_SHAPE_BUTTON, 0.788889f, 0.193694f, 0.086667f, 0.139640f,
     MUSE_ON_EVENT_BLACK8_DOWN, MUSE_ON_EVENT_BLACK8_UP, 20000000ULL, false,
     {HOLD_PROFILE(MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD),
      TRIGGER_PROFILE(MUSE_ON_ACTION_ENVIRONMENT_ACTION_1)}},
    {"leftBall.north", "Left ball north", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_CONTROL_SHAPE_BALL, 0.366667f, 0.193694f, 0.066667f, 0.067568f,
     MUSE_ON_EVENT_LEFT_BALL_NORTH_ENGAGED,
     MUSE_ON_EVENT_LEFT_BALL_NORTH_RELEASED, 20000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_PREVIOUS_THREAD),
      TRIGGER_PROFILE(MUSE_ON_ACTION_PREVIOUS_THREAD)}},
    {"leftBall.south", "Left ball south", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_CONTROL_SHAPE_BALL, 0.366667f, 0.261261f, 0.066667f, 0.067568f,
     MUSE_ON_EVENT_LEFT_BALL_SOUTH_ENGAGED,
     MUSE_ON_EVENT_LEFT_BALL_SOUTH_RELEASED, 20000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_NEXT_THREAD),
      TRIGGER_PROFILE(MUSE_ON_ACTION_NEXT_THREAD)}},
    {"rightBall.vertical", "Right ball vertical", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_CONTROL_SHAPE_BALL, 0.882222f, 0.414414f, 0.035556f, 0.067568f,
     MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_ENGAGED,
     MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_RELEASED, 20000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER),
      TRIGGER_PROFILE(MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER)}},
    {"rightBall.west", "Right ball west", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_CONTROL_SHAPE_BALL, 0.866667f, 0.482000f, 0.033333f, 0.067568f,
     MUSE_ON_EVENT_RIGHT_BALL_WEST_ENGAGED,
     MUSE_ON_EVENT_RIGHT_BALL_WEST_RELEASED, 20000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_NAVIGATE_BACK),
      TRIGGER_PROFILE(MUSE_ON_ACTION_NAVIGATE_BACK)}},
    {"rightBall.east", "Right ball east", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_CONTROL_SHAPE_BALL, 0.900000f, 0.482000f, 0.033333f, 0.067568f,
     MUSE_ON_EVENT_RIGHT_BALL_EAST_ENGAGED,
     MUSE_ON_EVENT_RIGHT_BALL_EAST_RELEASED, 20000000ULL, false,
     {TRIGGER_PROFILE(MUSE_ON_ACTION_NAVIGATE_FORWARD),
      TRIGGER_PROFILE(MUSE_ON_ACTION_NAVIGATE_FORWARD)}},
    {"pedal", "Optional pedal", MUSE_ON_CONTROL_GROUP_OPTIONAL_PEDAL,
     MUSE_ON_CONTROL_SHAPE_PEDAL, 0.391111f, 0.806306f, 0.217778f, 0.175676f,
     MUSE_ON_EVENT_PEDAL_DOWN, MUSE_ON_EVENT_PEDAL_UP, 20000000ULL, true,
     {UNAVAILABLE_PROFILE, HOLD_PROFILE(MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD)}},
};

static size_t control_mapping_count(void) {
  return sizeof(kControlMap) / sizeof(kControlMap[0]);
}

static bool set_action(MuseOnActionEvent *action, MuseOnActionId id,
                       MuseOnActionPhase phase, MuseOnEventName source) {
  if (!action) return false;
  action->id = id;
  action->phase = phase;
  action->source = source;
  return true;
}

static uint64_t trigger_debounce_ns(MuseOnEventName source) {
  size_t index;

  for (index = 0; index < control_mapping_count(); index++) {
    if (kControlMap[index].press_event == source) {
      return kControlMap[index].debounce_ns;
    }
  }
  return 20000000ULL;
}

static MuseOnEventName turntable_engaged_event_for_release(
    MuseOnEventName source) {
  switch (source) {
    case MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_RELEASED:
      return MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED;
    case MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_RELEASED:
      return MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED;
    default:
      return MUSE_ON_EVENT_NONE;
  }
}

static bool is_turntable_engaged_event(MuseOnEventName source) {
  return source == MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED ||
         source == MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED;
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
  router->turntable_repeat_active = false;
  router->turntable_repeat_action =
      MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT;
  router->turntable_repeat_source = MUSE_ON_EVENT_NONE;
  router->turntable_repeat_next_ns = 0;
}

bool muse_on_action_router_tick(MuseOnActionRouter *router, uint64_t timestamp_ns,
                                MuseOnActionEvent *action) {
  MuseOnActionPhase phase;

  if (!router || !action) return false;
  if (router->hold_pending &&
      timestamp_ns >= router->hold_pending_since_ns &&
      timestamp_ns - router->hold_pending_since_ns >=
          MUSE_ON_HOLD_DEBOUNCE_NS) {
    router->hold_active = router->hold_pending_active;
    router->hold_pending = false;
    phase = router->hold_active ? MUSE_ON_ACTION_BEGIN : MUSE_ON_ACTION_END;
    return set_action(action, MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD, phase,
                      router->hold_pending_source);
  }
  if (!router->turntable_repeat_active ||
      timestamp_ns < router->turntable_repeat_next_ns) {
    return false;
  }
  router->turntable_repeat_next_ns =
      timestamp_ns > UINT64_MAX - MUSE_ON_TURNTABLE_REPEAT_NS
          ? UINT64_MAX
          : timestamp_ns + MUSE_ON_TURNTABLE_REPEAT_NS;
  router->trigger_seen[router->turntable_repeat_action] = true;
  router->trigger_last_ns[router->turntable_repeat_action] = timestamp_ns;
  return set_action(action, router->turntable_repeat_action,
                    MUSE_ON_ACTION_TRIGGER,
                    router->turntable_repeat_source);
}

bool muse_on_action_router_route(MuseOnActionRouter *router,
                                 MuseOnEventName source, uint64_t timestamp_ns,
                                 MuseOnActionEvent *action) {
  MuseOnActionEvent mapped;
  MuseOnEventName released_engaged_event;
  unsigned int id;

  if (!router || !action) return false;
  released_engaged_event = turntable_engaged_event_for_release(source);
  if (released_engaged_event != MUSE_ON_EVENT_NONE) {
    if (router->turntable_repeat_active &&
        router->turntable_repeat_source == released_engaged_event) {
      router->turntable_repeat_active = false;
      router->turntable_repeat_source = MUSE_ON_EVENT_NONE;
      router->turntable_repeat_next_ns = 0;
    }
    return false;
  }
  if (!muse_on_map_event(router->profile, source, &mapped)) {
    return false;
  }
  if (mapped.phase == MUSE_ON_ACTION_TRIGGER) {
    id = (unsigned int)mapped.id;
    if (id >= MUSE_ON_ACTION_COUNT) return false;
    if (is_turntable_engaged_event(source)) {
      router->turntable_repeat_active = true;
      router->turntable_repeat_action = mapped.id;
      router->turntable_repeat_source = source;
      router->turntable_repeat_next_ns =
          timestamp_ns > UINT64_MAX - MUSE_ON_TURNTABLE_REPEAT_NS
              ? UINT64_MAX
              : timestamp_ns + MUSE_ON_TURNTABLE_REPEAT_NS;
    }
    if (router->trigger_seen[id] &&
        (timestamp_ns < router->trigger_last_ns[id] ||
         timestamp_ns - router->trigger_last_ns[id] <
             trigger_debounce_ns(source))) {
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
  size_t index;
  const MuseOnControlProfileMapping *profile_mapping;

  if (!action || (profile != MUSE_ON_PROFILE_CONTROLLER_ONLY &&
                  profile != MUSE_ON_PROFILE_PEDAL)) {
    return false;
  }
  for (index = 0; index < control_mapping_count(); index++) {
    profile_mapping = &kControlMap[index].profiles[profile];
    if (!profile_mapping->available) continue;
    if (source == kControlMap[index].press_event) {
      return set_action(action, profile_mapping->press_action,
                        profile_mapping->press_phase, source);
    }
    if (profile_mapping->release_mapped &&
        source == kControlMap[index].release_event) {
      return set_action(action, profile_mapping->release_action,
                        profile_mapping->release_phase, source);
    }
  }
  return false;
}

size_t muse_on_control_mapping_count(void) {
  return control_mapping_count();
}

const MuseOnControlMapping *muse_on_control_mapping_at(size_t index) {
  return index < control_mapping_count() ? &kControlMap[index] : NULL;
}

const MuseOnControlProfileMapping *muse_on_control_mapping_profile(
    const MuseOnControlMapping *mapping, MuseOnProfile profile) {
  if (!mapping || (profile != MUSE_ON_PROFILE_CONTROLLER_ONLY &&
                  profile != MUSE_ON_PROFILE_PEDAL)) {
    return NULL;
  }
  return &mapping->profiles[profile];
}

const char *muse_on_control_group_title(MuseOnControlGroup group) {
  switch (group) {
    case MUSE_ON_CONTROL_GROUP_TURNTABLE: return "Turntable";
    case MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS: return "White buttons";
    case MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS: return "Black buttons";
    case MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS: return "Directional balls";
    case MUSE_ON_CONTROL_GROUP_OPTIONAL_PEDAL: return "Optional pedal";
    case MUSE_ON_CONTROL_GROUP_COUNT: break;
  }
  return "Unknown group";
}

const char *muse_on_control_shape_string(MuseOnControlShape shape) {
  switch (shape) {
    case MUSE_ON_CONTROL_SHAPE_TURNTABLE: return "turntable";
    case MUSE_ON_CONTROL_SHAPE_BUTTON: return "button";
    case MUSE_ON_CONTROL_SHAPE_BALL: return "ball";
    case MUSE_ON_CONTROL_SHAPE_PEDAL: return "pedal";
  }
  return "unknown";
}

bool muse_on_control_map_validate(MuseOnProfile profile) {
  bool seen_actions[MUSE_ON_ACTION_COUNT] = {false};
  size_t mapped_count = 0;
  size_t index;

  if (profile != MUSE_ON_PROFILE_CONTROLLER_ONLY &&
      profile != MUSE_ON_PROFILE_PEDAL) {
    return false;
  }
  for (index = 0; index < control_mapping_count(); index++) {
    const MuseOnControlMapping *mapping = &kControlMap[index];
    const MuseOnControlProfileMapping *profile_mapping =
        &mapping->profiles[profile];
    MuseOnActionEvent action;

    if (!mapping->identifier || !mapping->physical_label ||
        mapping->press_event == MUSE_ON_EVENT_NONE ||
        mapping->group >= MUSE_ON_CONTROL_GROUP_COUNT ||
        mapping->x < 0.0f || mapping->y < 0.0f || mapping->width <= 0.0f ||
        mapping->height <= 0.0f || mapping->x + mapping->width > 1.0f ||
        mapping->y + mapping->height > 1.0f) {
      return false;
    }
    if (!profile_mapping->available) {
      if (!mapping->optional || profile == MUSE_ON_PROFILE_PEDAL) return false;
      continue;
    }
    mapped_count++;
    if ((unsigned int)profile_mapping->press_action >= MUSE_ON_ACTION_COUNT ||
        seen_actions[profile_mapping->press_action] ||
        !muse_on_map_event(profile, mapping->press_event, &action) ||
        action.id != profile_mapping->press_action ||
        action.phase != profile_mapping->press_phase) {
      return false;
    }
    seen_actions[profile_mapping->press_action] = true;
    if (profile_mapping->release_mapped) {
      if (mapping->release_event == MUSE_ON_EVENT_NONE ||
          !muse_on_map_event(profile, mapping->release_event, &action) ||
          action.id != profile_mapping->release_action ||
          action.phase != profile_mapping->release_phase) {
        return false;
      }
    } else if (muse_on_map_event(profile, mapping->release_event, &action)) {
      return false;
    }
  }
  return mapped_count == (profile == MUSE_ON_PROFILE_PEDAL ? 16 : 15);
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

const char *muse_on_action_phase_prompt(MuseOnActionPhase phase) {
  switch (phase) {
    case MUSE_ON_ACTION_TRIGGER: return "Press to trigger";
    case MUSE_ON_ACTION_BEGIN: return "Press and hold; release to stop";
    case MUSE_ON_ACTION_END: return "Release to stop";
  }
  return "Interact with";
}

const char *muse_on_action_id_string(MuseOnActionId action) {
  if ((unsigned int)action >= MUSE_ON_ACTION_COUNT) return "unknown";
  return kActionInfo[action].identifier;
}

const char *muse_on_action_display_name(MuseOnActionId action) {
  if ((unsigned int)action >= MUSE_ON_ACTION_COUNT) return "Unknown action";
  return kActionInfo[action].display_name;
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
