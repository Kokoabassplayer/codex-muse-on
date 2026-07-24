#ifndef MUSE_ON_ACTION_MAP_H
#define MUSE_ON_ACTION_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "muse_on_decoder.h"

typedef enum {
  MUSE_ON_PROFILE_CONTROLLER_ONLY = 0,
  MUSE_ON_PROFILE_PEDAL
} MuseOnProfile;

typedef enum {
  MUSE_ON_ACTION_TRIGGER = 0,
  MUSE_ON_ACTION_BEGIN,
  MUSE_ON_ACTION_END
} MuseOnActionPhase;

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

enum { MUSE_ON_HOLD_DEBOUNCE_NS = 20000000ULL };

typedef struct {
  MuseOnProfile profile;
  bool trigger_seen[MUSE_ON_ACTION_COUNT];
  uint64_t trigger_last_ns[MUSE_ON_ACTION_COUNT];
  bool hold_active;
  bool hold_pending;
  bool hold_pending_active;
  uint64_t hold_pending_since_ns;
  MuseOnEventName hold_pending_source;
} MuseOnActionRouter;

bool muse_on_map_event(MuseOnProfile profile, MuseOnEventName source,
                       MuseOnActionEvent *action);
void muse_on_action_router_init(MuseOnActionRouter *router, MuseOnProfile profile);
bool muse_on_action_router_route(MuseOnActionRouter *router,
                                 MuseOnEventName source, uint64_t timestamp_ns,
                                 MuseOnActionEvent *action);
bool muse_on_action_router_tick(MuseOnActionRouter *router, uint64_t timestamp_ns,
                                MuseOnActionEvent *action);
const char *muse_on_profile_string(MuseOnProfile profile);
const char *muse_on_action_id_string(MuseOnActionId action);
const char *muse_on_action_phase_string(MuseOnActionPhase phase);
const char *muse_on_event_name_string(MuseOnEventName event);

#endif
