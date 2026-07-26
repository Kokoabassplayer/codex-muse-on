#include <assert.h>
#include <string.h>

#include "../muse_on_state_coordinator.h"

/* Helper: prerequisites with every Active gate satisfied. */
static MuseOnPrerequisites all_clear(void) {
  MuseOnPrerequisites p;
  p.safety_latched = false;
  p.permission_granted = true;
  p.controller_connected = true;
  p.multiple_controllers = false;
  p.session_available = true;
  p.codex_foreground = true;
  p.inputs_released = true;
  return p;
}

/* ---- Disabled initial state ---- */

static void test_initial_state_is_disabled(void) {
  MuseOnState state;

  muse_on_state_init(&state);
  assert(state.status == MUSE_ON_STATUS_DISABLED);
  assert(state.enabled_intent == false);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_NONE);
  assert(state.effects.request_filter == false);
  assert(state.effects.request_dispatch == false);
}

/* Disabled ignores observations: no filter, no dispatch. */
static void test_disabled_ignores_observations(void) {
  MuseOnState state;

  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, all_clear());
  assert(state.status == MUSE_ON_STATUS_DISABLED);
  assert(state.effects.request_filter == false);
  assert(state.effects.request_dispatch == false);
}

/* ---- Persistent Enabled intent (ADR 0001) ---- */

static void test_enable_sets_persistent_intent(void) {
  MuseOnState state;

  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, all_clear());
  assert(state.enabled_intent == true);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.effects.request_filter == true);
  assert(state.effects.request_dispatch == true);
}

/* Enable with unmet prerequisites stays Inactive but keeps intent. */
static void test_enable_keeps_intent_when_inactive(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.controller_connected = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.enabled_intent == true);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_DISCONNECTED);
}

/* Enable with disconnected hardware completes as Inactive (First Enable). */
static void test_first_enable_no_hardware_is_inactive_disconnected(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.controller_connected = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_DISCONNECTED);
}

/* Disable clears Enabled intent. */
static void test_disable_clears_intent(void) {
  MuseOnState state;

  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, all_clear());
  muse_on_state_apply(&state, MUSE_ON_COMMAND_DISABLE, all_clear());
  assert(state.enabled_intent == false);
  assert(state.status == MUSE_ON_STATUS_DISABLED);
  assert(state.effects.request_filter == false);
  assert(state.effects.request_dispatch == false);
}

/* ---- Active gates ---- */

static void test_active_requires_all_gates(void) {
  MuseOnState state;

  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, all_clear());
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
}

static void test_session_unavailable_blocks_active(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.session_available = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
}

static void test_codex_not_foreground_blocks_active(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.codex_foreground = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
}

static void test_permission_missing_blocks_active(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.permission_granted = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_PERMISSION);
}

static void test_inputs_not_released_blocks_active(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.inputs_released = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
}

/* Enable while filter is requested but dispatch is gated stays reserved. */
static void test_enabled_connected_but_not_foreground_still_filters(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.codex_foreground = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.effects.request_filter == true);
  assert(state.effects.request_dispatch == false);
}

/* ---- Inactive Reason priority (CONTEXT.md) ---- */

/* Safety latch beats every other reason. */
static void test_safety_latch_has_highest_priority(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.safety_latched = true;
  p.permission_granted = false;
  p.multiple_controllers = true;
  p.controller_connected = false;
  p.session_available = false;
  p.codex_foreground = false;
  p.inputs_released = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_SAFETY_LATCH);
}

/* Permission beats multiple, disconnected, session, foreground, release. */
static void test_permission_priority(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.permission_granted = false;
  p.multiple_controllers = true;
  p.controller_connected = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_PERMISSION);
}

/* Multiple Controllers beats disconnected. */
static void test_multiple_controllers_priority(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.multiple_controllers = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_MULTIPLE);
}

/* Multiple Controllers is reported even when "connected" is also unmet. */
static void test_multiple_takes_priority_over_disconnected(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.multiple_controllers = true;
  p.controller_connected = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_MULTIPLE);
}

/* Disconnected beats session, foreground, release. */
static void test_disconnected_priority(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.controller_connected = false;
  p.session_available = false;
  p.codex_foreground = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_DISCONNECTED);
}

/* Session unavailable beats foreground and release. */
static void test_session_priority(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.session_available = false;
  p.codex_foreground = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_SESSION);
}

/* Codex not foreground beats release controls. */
static void test_not_foreground_priority(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.codex_foreground = false;
  p.inputs_released = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_NOT_FOREGROUND);
}

/* Release controls is the lowest-priority reason. */
static void test_release_controls_is_lowest_priority(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.inputs_released = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
}

/* ---- Safety latch + Retry (ADR 0003) ---- */

static void test_retry_clears_safety_latch_when_prerequisites_met(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.safety_latched = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  p.safety_latched = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_NONE);
}

static void test_retry_keeps_latch_when_prerequisites_unmet(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.safety_latched = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  p.safety_latched = false;
  p.permission_granted = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_SAFETY_LATCH);
}

/* Disable during safety latch persists intent removal. */
static void test_disable_during_safety_latch(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.safety_latched = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  muse_on_state_apply(&state, MUSE_ON_COMMAND_DISABLE, p);
  assert(state.status == MUSE_ON_STATUS_DISABLED);
  assert(state.enabled_intent == false);
}

/* ---- Sticky safety latch (ADR 0003) ---- */

/* (1) Ordinary observation with safety_latched=false does NOT auto-resume. */
static void test_latch_persists_through_ordinary_observation(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.safety_latched = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  /* Adapter later reports the failure cleared, with every gate green.
     No human Retry — must remain latched. */
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, all_clear());
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_SAFETY_LATCH);
  assert(state.effects.request_dispatch == false);
}

/* (2) Only explicit Retry with all prerequisites clear can leave the latch. */
static void test_only_retry_clears_latch(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.safety_latched = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  /* Even Retry must stay latched while a gate is still unmet. */
  p.safety_latched = false;
  p.controller_connected = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  /* Retry with every gate clear finally clears the latch. */
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, all_clear());
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_NONE);
}

/* (3) Disable wins over the latch and finishes Disabled. */
static void test_disable_wins_over_latch(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  p = all_clear();
  p.safety_latched = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  /* Even a safety observation still present cannot block Disable. */
  muse_on_state_apply(&state, MUSE_ON_COMMAND_DISABLE, p);
  assert(state.status == MUSE_ON_STATUS_DISABLED);
  assert(state.enabled_intent == false);
  assert(state.safety_latched == false);
  assert(state.effects.request_filter == false);
  assert(state.effects.request_dispatch == false);
}

/* ---- Recovery / transitions ---- */

/* Disconnect after Active moves to Inactive; reconnect recovers. */
static void test_disconnect_recovers_on_reconnect(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, all_clear());
  assert(state.status == MUSE_ON_STATUS_ACTIVE);

  p = all_clear();
  p.controller_connected = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_DISCONNECTED);

  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, all_clear());
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
}

/* ---- String helpers ---- */

static void test_status_strings(void) {
  assert(strcmp(muse_on_status_string(MUSE_ON_STATUS_DISABLED),
                "disabled") == 0);
  assert(strcmp(muse_on_status_string(MUSE_ON_STATUS_INACTIVE),
                "inactive") == 0);
  assert(strcmp(muse_on_status_string(MUSE_ON_STATUS_ACTIVE),
                "active") == 0);
  assert(strcmp(muse_on_status_string(MUSE_ON_STATUS_SAFETY_LATCH),
                "safety_latch") == 0);
}

static void test_inactive_reason_strings(void) {
  assert(strcmp(muse_on_inactive_reason_string(MUSE_ON_INACTIVE_REASON_NONE),
                "none") == 0);
  assert(strcmp(muse_on_inactive_reason_string(
                    MUSE_ON_INACTIVE_REASON_SAFETY_LATCH),
                "safety_latch") == 0);
  assert(strcmp(muse_on_inactive_reason_string(
                    MUSE_ON_INACTIVE_REASON_PERMISSION),
                "permission_required") == 0);
  assert(strcmp(muse_on_inactive_reason_string(
                    MUSE_ON_INACTIVE_REASON_MULTIPLE),
                "multiple_controllers") == 0);
  assert(strcmp(muse_on_inactive_reason_string(
                    MUSE_ON_INACTIVE_REASON_DISCONNECTED),
                "muse_on_disconnected") == 0);
  assert(strcmp(muse_on_inactive_reason_string(
                    MUSE_ON_INACTIVE_REASON_SESSION),
                "session_unavailable") == 0);
  assert(strcmp(muse_on_inactive_reason_string(
                    MUSE_ON_INACTIVE_REASON_NOT_FOREGROUND),
                "codex_not_foreground") == 0);
  assert(strcmp(muse_on_inactive_reason_string(
                    MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS),
                "release_controls") == 0);
}

int main(void) {
  test_initial_state_is_disabled();
  test_disabled_ignores_observations();
  test_enable_sets_persistent_intent();
  test_enable_keeps_intent_when_inactive();
  test_first_enable_no_hardware_is_inactive_disconnected();
  test_disable_clears_intent();
  test_active_requires_all_gates();
  test_session_unavailable_blocks_active();
  test_codex_not_foreground_blocks_active();
  test_permission_missing_blocks_active();
  test_inputs_not_released_blocks_active();
  test_enabled_connected_but_not_foreground_still_filters();
  test_safety_latch_has_highest_priority();
  test_permission_priority();
  test_multiple_controllers_priority();
  test_multiple_takes_priority_over_disconnected();
  test_disconnected_priority();
  test_session_priority();
  test_not_foreground_priority();
  test_release_controls_is_lowest_priority();
  test_retry_clears_safety_latch_when_prerequisites_met();
  test_retry_keeps_latch_when_prerequisites_unmet();
  test_disable_during_safety_latch();
  test_latch_persists_through_ordinary_observation();
  test_only_retry_clears_latch();
  test_disable_wins_over_latch();
  test_disconnect_recovers_on_reconnect();
  test_status_strings();
  test_inactive_reason_strings();
  return 0;
}
