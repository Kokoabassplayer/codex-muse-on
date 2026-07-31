#ifndef MUSE_ON_ACTION_MAP_H
#define MUSE_ON_ACTION_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "muse_on_decoder.h"

typedef enum {
  MUSE_ON_PROFILE_CONTROLLER_ONLY = 0,
  MUSE_ON_PROFILE_PEDAL
} MuseOnProfile;

typedef enum {
  MUSE_ON_NEUTRAL_ENTRY_UNKNOWN = 0,
  MUSE_ON_NEUTRAL_ENTRY_HELD,
  MUSE_ON_NEUTRAL_ENTRY_RELEASED
} MuseOnNeutralEntryState;

typedef enum {
  MUSE_ON_ACTION_TRIGGER = 0,
  MUSE_ON_ACTION_BEGIN,
  MUSE_ON_ACTION_END
} MuseOnActionPhase;

/*
 * The physical Controller Map is the single source of truth for dispatch and
 * presentation. The row order is also the semantic order used by the native
 * popover: turntable, white buttons, black buttons, directional balls, pedal.
 */
typedef enum {
  MUSE_ON_CONTROL_GROUP_TURNTABLE = 0,
  MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS,
  MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS,
  MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
  MUSE_ON_CONTROL_GROUP_OPTIONAL_PEDAL,
  MUSE_ON_CONTROL_GROUP_COUNT
} MuseOnControlGroup;

typedef enum {
  MUSE_ON_CONTROL_SHAPE_TURNTABLE = 0,
  MUSE_ON_CONTROL_SHAPE_BUTTON,
  MUSE_ON_CONTROL_SHAPE_BALL,
  MUSE_ON_CONTROL_SHAPE_PEDAL
} MuseOnControlShape;

typedef enum {
  MUSE_ON_ACTION_COMPOSER_TOGGLE_FAST_MODE = 0,
  MUSE_ON_ACTION_APPROVAL_APPROVE,
  MUSE_ON_ACTION_APPROVAL_DECLINE,
  MUSE_ON_ACTION_FORK_THREAD,
  MUSE_ON_ACTION_COPY_CONVERSATION_MARKDOWN,
  MUSE_ON_ACTION_COMPOSER_SUBMIT,
  MUSE_ON_ACTION_TOGGLE_REVIEW_TAB,
  MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD,
  MUSE_ON_ACTION_ENVIRONMENT_ACTION_1,
  MUSE_ON_ACTION_COMPOSER_DECREASE_REASONING_EFFORT,
  MUSE_ON_ACTION_COMPOSER_INCREASE_REASONING_EFFORT,
  MUSE_ON_ACTION_PREVIOUS_THREAD,
  MUSE_ON_ACTION_NEXT_THREAD,
  MUSE_ON_ACTION_NAVIGATE_BACK,
  MUSE_ON_ACTION_NAVIGATE_FORWARD,
  MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER
} MuseOnActionId;

enum { MUSE_ON_ACTION_COUNT = MUSE_ON_ACTION_COMPOSER_OPEN_MODEL_PICKER + 1 };

typedef struct {
  MuseOnActionId id;
  MuseOnActionPhase phase;
  MuseOnEventName source;
} MuseOnActionEvent;

typedef struct {
  bool available;
  MuseOnActionId press_action;
  MuseOnActionPhase press_phase;
  bool release_mapped;
  MuseOnActionId release_action;
  MuseOnActionPhase release_phase;
} MuseOnControlProfileMapping;

typedef struct {
  const char *identifier;
  const char *physical_label;
  MuseOnControlGroup group;
  MuseOnControlShape shape;
  float x;
  float y;
  float width;
  float height;
  MuseOnEventName press_event;
  MuseOnEventName release_event;
  uint64_t debounce_ns;
  bool optional;
  MuseOnControlProfileMapping profiles[2];
} MuseOnControlMapping;

/* Require a stable hold edge, and rate-limit Submit to one deliberate press. */
enum { MUSE_ON_HOLD_DEBOUNCE_NS = 80000000ULL };
enum { MUSE_ON_SUBMIT_DEBOUNCE_NS = 250000000ULL };
enum { MUSE_ON_TURNTABLE_INITIAL_REPEAT_NS = 600000000ULL };
enum { MUSE_ON_TURNTABLE_REPEAT_NS = 250000000ULL };

typedef struct {
  MuseOnProfile profile;
  bool trigger_seen[MUSE_ON_ACTION_COUNT];
  uint64_t trigger_last_ns[MUSE_ON_ACTION_COUNT];
  bool hold_active;
  bool hold_pending;
  bool hold_pending_active;
  uint64_t hold_pending_since_ns;
  MuseOnEventName hold_pending_source;
  bool turntable_repeat_active;
  MuseOnActionId turntable_repeat_action;
  MuseOnEventName turntable_repeat_source;
  uint64_t turntable_repeat_next_ns;
} MuseOnActionRouter;

bool muse_on_map_event(MuseOnProfile profile, MuseOnEventName source,
                       MuseOnActionEvent *action);
MuseOnNeutralEntryState muse_on_selected_profile_neutral_state(
    const MuseOnDecoder *decoder, MuseOnInterfaceKind interface_kind,
    MuseOnProfile profile);
void muse_on_action_router_init(MuseOnActionRouter *router, MuseOnProfile profile);
bool muse_on_action_router_route(MuseOnActionRouter *router,
                                 MuseOnEventName source, uint64_t timestamp_ns,
                                 MuseOnActionEvent *action);
bool muse_on_action_router_tick(MuseOnActionRouter *router, uint64_t timestamp_ns,
                                MuseOnActionEvent *action);
const char *muse_on_profile_string(MuseOnProfile profile);
const char *muse_on_action_id_string(MuseOnActionId action);
bool muse_on_action_id_from_string(const char *value, MuseOnActionId *action);
const char *muse_on_action_display_name(MuseOnActionId action);
const char *muse_on_action_phase_string(MuseOnActionPhase phase);
bool muse_on_action_phase_from_string(const char *value,
                                      MuseOnActionPhase *phase);
bool muse_on_action_phase_valid(MuseOnActionId action,
                                MuseOnActionPhase phase);
const char *muse_on_action_phase_prompt(MuseOnActionPhase phase);
const char *muse_on_event_name_string(MuseOnEventName event);

size_t muse_on_control_mapping_count(void);
const MuseOnControlMapping *muse_on_control_mapping_at(size_t index);
const MuseOnControlProfileMapping *muse_on_control_mapping_profile(
    const MuseOnControlMapping *mapping, MuseOnProfile profile);
const char *muse_on_control_group_title(MuseOnControlGroup group);
const char *muse_on_control_shape_string(MuseOnControlShape shape);
bool muse_on_control_map_validate(MuseOnProfile profile);

#endif
