#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../muse_on_action_map.h"

typedef struct {
  const char *identifier;
  MuseOnControlGroup group;
  MuseOnEventName press_event;
  MuseOnActionId controller_action;
  MuseOnActionId pedal_action;
} ExpectedControl;

static const ExpectedControl kExpectedControls[] = {
    {"turntable.clockwise", MUSE_ON_CONTROL_GROUP_TURNTABLE,
     MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED,
     MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT,
     MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT},
    {"turntable.counterclockwise", MUSE_ON_CONTROL_GROUP_TURNTABLE,
     MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED,
     MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT,
     MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT},
    {"white1", MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS, MUSE_ON_EVENT_WHITE1_DOWN,
     MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE,
     MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE},
    {"white3", MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS, MUSE_ON_EVENT_WHITE3_DOWN,
     MUSE_ON_ACTION_APPROVAL_DECLINE, MUSE_ON_ACTION_APPROVAL_DECLINE},
    {"white5", MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS, MUSE_ON_EVENT_WHITE5_DOWN,
     MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN,
     MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN},
    {"white7", MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS, MUSE_ON_EVENT_WHITE7_DOWN,
     MUSE_ON_ACTION_TOGGLE_REVIEW_TAB, MUSE_ON_ACTION_TOGGLE_REVIEW_TAB},
    {"black2", MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS, MUSE_ON_EVENT_BLACK2_DOWN,
     MUSE_ON_ACTION_APPROVAL_APPROVE, MUSE_ON_ACTION_APPROVAL_APPROVE},
    {"black4", MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS, MUSE_ON_EVENT_BLACK4_DOWN,
     MUSE_ON_ACTION_FORK_THREAD, MUSE_ON_ACTION_FORK_THREAD},
    {"black6", MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS, MUSE_ON_EVENT_BLACK6_DOWN,
     MUSE_ON_ACTION_COMPOSER_SUBMIT, MUSE_ON_ACTION_COMPOSER_SUBMIT},
    {"black8", MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS, MUSE_ON_EVENT_BLACK8_DOWN,
     MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
     MUSE_ON_ACTION_ENVIRONMENT_ACTION_1},
    {"leftBall.north", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_EVENT_LEFT_BALL_NORTH_ENGAGED, MUSE_ON_ACTION_PREVIOUS_THREAD,
     MUSE_ON_ACTION_PREVIOUS_THREAD},
    {"leftBall.south", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_EVENT_LEFT_BALL_SOUTH_ENGAGED, MUSE_ON_ACTION_NEXT_THREAD,
     MUSE_ON_ACTION_NEXT_THREAD},
    {"rightBall.vertical", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_ENGAGED,
     MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER,
     MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER},
    {"rightBall.west", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_EVENT_RIGHT_BALL_WEST_ENGAGED, MUSE_ON_ACTION_NAVIGATE_BACK,
     MUSE_ON_ACTION_NAVIGATE_BACK},
    {"rightBall.east", MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
     MUSE_ON_EVENT_RIGHT_BALL_EAST_ENGAGED, MUSE_ON_ACTION_NAVIGATE_FORWARD,
     MUSE_ON_ACTION_NAVIGATE_FORWARD},
    {"pedal", MUSE_ON_CONTROL_GROUP_OPTIONAL_PEDAL, MUSE_ON_EVENT_PEDAL_DOWN,
     MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
     MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD},
};

static void test_semantic_layout_groups(void) {
  const char *const titles[] = {
      "Turntable", "White buttons", "Black buttons", "Directional balls",
      "Optional pedal"};
  const MuseOnControlGroup groups[] = {
      MUSE_ON_CONTROL_GROUP_TURNTABLE,
      MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS,
      MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS,
      MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
      MUSE_ON_CONTROL_GROUP_OPTIONAL_PEDAL};
  const size_t expected_counts[] = {2, 4, 4, 5, 1};
  size_t index;

  assert(muse_on_control_mapping_count() == 16);
  assert(MUSE_ON_CONTROL_GROUP_COUNT == 5);
  for (index = 0; index < MUSE_ON_CONTROL_GROUP_COUNT; index++) {
    assert(strcmp(muse_on_control_group_title(groups[index]), titles[index]) == 0);
    size_t count = 0;
    size_t row;
    for (row = 0; row < muse_on_control_mapping_count(); row++) {
      const MuseOnControlMapping *mapping =
          muse_on_control_mapping_at(row);
      if (mapping->group == groups[index]) count++;
    }
    assert(count == expected_counts[index]);
  }
}

static void test_mapping_completeness_and_dispatch_parity(void) {
  bool seen_actions[MUSE_ON_PROFILE_PEDAL + 1][MUSE_ON_ACTION_COUNT] = {{false}};
  size_t index;

  assert(muse_on_control_map_validate(MUSE_ON_PROFILE_CONTROLLER_ONLY));
  assert(muse_on_control_map_validate(MUSE_ON_PROFILE_PEDAL));
  for (index = 0; index < sizeof(kExpectedControls) / sizeof(kExpectedControls[0]);
       index++) {
    const ExpectedControl *expected = &kExpectedControls[index];
    const MuseOnControlMapping *mapping =
        muse_on_control_mapping_at(index);
    size_t profile_index;

    assert(mapping != NULL);
    assert(strcmp(mapping->identifier, expected->identifier) == 0);
    assert(mapping->group == expected->group);
    assert(mapping->press_event == expected->press_event);
    assert(mapping->physical_label != NULL && mapping->physical_label[0] != '\0');
    assert(mapping->x >= 0.0f && mapping->x <= 1.0f);
    assert(mapping->y >= 0.0f && mapping->y <= 1.0f);
    assert(mapping->width > 0.0f && mapping->x + mapping->width <= 1.0f);
    assert(mapping->height > 0.0f && mapping->y + mapping->height <= 1.0f);
    assert(strcmp(muse_on_control_shape_string(mapping->shape),
                  mapping->shape == MUSE_ON_CONTROL_SHAPE_TURNTABLE
                      ? "turntable"
                      : mapping->shape == MUSE_ON_CONTROL_SHAPE_BUTTON
                            ? "button"
                            : mapping->shape == MUSE_ON_CONTROL_SHAPE_BALL
                                  ? "ball"
                                  : "pedal") == 0);

    for (profile_index = 0; profile_index <= MUSE_ON_PROFILE_PEDAL;
         profile_index++) {
      MuseOnProfile profile = (MuseOnProfile)profile_index;
      const MuseOnControlProfileMapping *profile_mapping =
          muse_on_control_mapping_profile(mapping, profile);
      MuseOnActionEvent action;
      MuseOnActionId expected_action = profile == MUSE_ON_PROFILE_PEDAL
          ? expected->pedal_action : expected->controller_action;

      assert(profile_mapping != NULL);
      if (index == 15 && profile == MUSE_ON_PROFILE_CONTROLLER_ONLY) {
        assert(!profile_mapping->available);
        continue;
      }
      assert(profile_mapping->available);
      assert(profile_mapping->press_action == expected_action);
      assert(!seen_actions[profile_index][profile_mapping->press_action]);
      seen_actions[profile_index][profile_mapping->press_action] = true;
      assert(muse_on_map_event(profile, mapping->press_event, &action));
      assert(action.id == profile_mapping->press_action);
      assert(action.phase == profile_mapping->press_phase);
      assert(action.source == mapping->press_event);
      assert(muse_on_action_display_name(profile_mapping->press_action)[0] != '\0');
      if (profile_mapping->release_mapped) {
        assert(muse_on_map_event(profile, mapping->release_event, &action));
        assert(action.id == profile_mapping->release_action);
        assert(action.phase == profile_mapping->release_phase);
        assert(action.source == mapping->release_event);
      } else {
        assert(!muse_on_map_event(profile, mapping->release_event, &action));
      }
    }
  }
}

static void test_action_display_names_are_user_facing(void) {
  assert(strcmp(muse_on_action_display_name(
                    MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE),
                "Toggle Fast Mode") == 0);
  assert(strcmp(muse_on_action_display_name(
                    MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD),
                "Hold Global Dictation") == 0);
  assert(strcmp(muse_on_action_display_name(
                    MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER),
                "Open Model Picker") == 0);
}

int main(void) {
  test_semantic_layout_groups();
  test_mapping_completeness_and_dispatch_parity();
  test_action_display_names_are_user_facing();
  return 0;
}
