#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../muse_on_state_coordinator.h"

/* Helper: prerequisites with every Active gate satisfied. */
static MuseOnPrerequisites all_clear(void) {
  MuseOnPrerequisites p;
  p.safety_latched = false;
  p.safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  p.cleanup_verified = true;
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

/* Background Retry may clear safety, but must remain Inactive and never dispatch. */
static void test_background_retry_clears_latch_without_activation(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  p.safety_latched = true;
  p.codex_foreground = false;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  p.safety_latched = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.safety_latched == false);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_NOT_FOREGROUND);
  assert(state.effects.request_dispatch == false);

  /* Foreground return is not Neutral Entry; the host must observe a fresh release. */
  p.codex_foreground = true;
  p.inputs_released = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(state.effects.request_dispatch == false);

  p.inputs_released = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.effects.request_dispatch == true);
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
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.enabled_intent == false);
  assert(state.disable_pending == true);

  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, all_clear());
  assert(state.status == MUSE_ON_STATUS_DISABLED);
  assert(state.disable_pending == false);
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
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.enabled_intent == false);
  assert(state.safety_latched == true);
  assert(state.disable_pending == true);
  assert(state.effects.request_filter == false);
  assert(state.effects.request_dispatch == false);

  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, all_clear());
  assert(state.status == MUSE_ON_STATUS_DISABLED);
  assert(state.safety_latched == false);
  assert(state.disable_pending == false);
}

/* A safety-critical failure enters a named sticky latch and blocks dispatch. */
static void test_named_safety_failure_is_sticky_and_fail_closed(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.safety_latched == true);
  assert(state.safety_failure == MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE);
  assert(state.effects.request_filter == true);
  assert(state.effects.request_dispatch == false);

  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, all_clear());
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.safety_failure == MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE);
  assert(state.effects.request_dispatch == false);
}

/* Retry needs verified cleanup in addition to every ordinary Active gate. */
static void test_retry_requires_cleanup_and_every_gate(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  p.cleanup_verified = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  p.cleanup_verified = true;
  p.session_available = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, all_clear());
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.safety_latched == false);
}

/* Recovery proof must preserve the listener's released-input observation. */
static void test_unclean_recovery_preserves_proof_then_requires_fresh_entry(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.effects.request_dispatch == false);

  /* Recovery helper reports every prerequisite and verified stopped cleanup. */
  p.safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  p.cleanup_verified = true;
  p.inputs_released = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.safety_latched == false);
  assert(state.effects.request_dispatch == true);

  /* Starting the normal listener requires a new Neutral Entry observation. */
  muse_on_neutral_entry_require(&p);
  assert(p.inputs_released == false);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(state.effects.request_dispatch == false);

  p.inputs_released = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.effects.request_dispatch == true);
}

/* Recovery remains fail-closed when cleanup or any prerequisite is unmet. */
static void test_unclean_recovery_rejects_failed_cleanup_and_gates(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  p.cleanup_verified = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.effects.request_dispatch == false);

  p.cleanup_verified = true;
  p.permission_granted = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.effects.request_dispatch == false);

  p.permission_granted = true;
  p.inputs_released = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(state.safety_latched == false);
  assert(state.effects.request_dispatch == false);

  p.inputs_released = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
}

/* Clean filter restoration is proved by stopped cleanup, not filter absence. */
static void test_recovery_filter_restoration_order(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  /* Recovery state has an active filter; no restoration failure is present. */
  assert(!muse_on_recovery_filter_restoration_unverified(
      true, false, true, true, false, true, false, true, true));

  /* filter_restored clears the active-filter observation while cleanup waits. */
  assert(muse_on_recovery_filter_restoration_unverified(
      true, false, true, true, false, true, false, true, false));

  /* stopped with clean cleanup verifies that restoration; RETRY may clear. */
  assert(!muse_on_recovery_filter_restoration_unverified(
      true, true, true, true, false, true, false, true, false));
  p.safety_failure = MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  p.safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  p.cleanup_verified = true;
  p.inputs_released = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.effects.request_dispatch == true);

  /* Missing filter proof plus failed cleanup remains fail-closed. */
  assert(muse_on_recovery_filter_restoration_unverified(
      true, false, true, true, false, true, false, true, false));
}

/* Disable persists immediately while failed cleanup remains latched. */
static void test_disable_pending_resolves_to_disabled_after_retry(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE;
  p.cleanup_verified = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_DISABLE, p);
  assert(state.enabled_intent == false);
  assert(state.disable_pending == true);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.effects.request_dispatch == false);

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  p.cleanup_verified = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_DISABLED);
  assert(state.disable_pending == false);
  assert(state.enabled_intent == false);
}

/* A prior process without Safe Quit must not resume Active automatically. */
static void test_unclean_prior_exit_latches_next_launch(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.safety_failure == MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT);

  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, all_clear());
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
}

/* Ordinary focus/device/session gates recover without creating a latch. */
static void test_ordinary_gates_auto_recover_without_latch(void) {
  MuseOnState state;
  MuseOnPrerequisites p;

  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, all_clear());

  p = all_clear();
  p.codex_foreground = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.safety_latched == false);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, all_clear());
  assert(state.status == MUSE_ON_STATUS_ACTIVE);

  p = all_clear();
  p.controller_connected = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.safety_latched == false);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, all_clear());
  assert(state.status == MUSE_ON_STATUS_ACTIVE);

  p = all_clear();
  p.session_available = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.safety_latched == false);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, all_clear());
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
}

/* Quit is deferred until cleanup is verified, then becomes explicitly allowed. */
static void test_safe_quit_requires_verified_cleanup(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  p.cleanup_verified = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_QUIT, p);
  assert(state.quit_requested == true);
  assert(state.quit_allowed == false);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);
  assert(state.effects.request_dispatch == false);

  p.cleanup_verified = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.quit_allowed == true);
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

/* Verified cleanup may clear the latch to Release controls without Neutral Entry. */
static void test_retry_enters_release_controls_after_cleanup(void) {
  MuseOnState state;
  MuseOnPrerequisites p = all_clear();

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT;
  muse_on_state_init(&state);
  muse_on_state_apply(&state, MUSE_ON_COMMAND_ENABLE, p);
  assert(state.status == MUSE_ON_STATUS_SAFETY_LATCH);

  p.safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  p.cleanup_verified = true;
  p.inputs_released = false;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_RETRY, p);
  assert(state.status == MUSE_ON_STATUS_INACTIVE);
  assert(state.inactive_reason == MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(state.safety_latched == false);
  assert(state.effects.request_dispatch == false);

  p.inputs_released = true;
  muse_on_state_apply(&state, MUSE_ON_COMMAND_NONE, p);
  assert(state.status == MUSE_ON_STATUS_ACTIVE);
  assert(state.effects.request_dispatch == true);
}

/* Recovery waits through HID enumeration and emits exactly once. */
static void test_recovery_policy_settles_before_emitting(void) {
  MuseOnRecoveryPolicy policy;
  MuseOnRecoveryObservation observation = {
      .permission_granted = true,
      .controller_connected = false,
      .multiple_controllers = false,
      .codex_foreground = true,
      .inputs_released = false,
      .filter_verified = false,
      .keyboard_open = true,
      .error_observed = false,
  };

  muse_on_recovery_policy_init(&policy, 100);
  assert(muse_on_recovery_policy_evaluate(&policy, 100 + 10000000,
                                          observation) ==
         MUSE_ON_RECOVERY_WAIT);

  /* A changing or ambiguous topology remains in the settlement window. */
  observation.controller_connected = true;
  observation.multiple_controllers = true;
  assert(muse_on_recovery_policy_evaluate(&policy, 100 + 500000000,
                                          observation) ==
         MUSE_ON_RECOVERY_WAIT);

  observation.multiple_controllers = false;
  observation.filter_verified = true;
  assert(muse_on_recovery_non_neutral_gates_valid(observation));
  assert(muse_on_recovery_policy_evaluate(&policy, 100 + 900000000,
                                          observation) ==
         MUSE_ON_RECOVERY_WAIT);
  assert(muse_on_recovery_policy_evaluate(
             &policy, 100 + MUSE_ON_RECOVERY_SETTLEMENT_NS + 1,
             observation) == MUSE_ON_RECOVERY_WAIT);

  observation.inputs_released = true;
  assert(muse_on_recovery_policy_evaluate(&policy, 100 + 1600000000,
                                          observation) ==
         MUSE_ON_RECOVERY_SUCCESS);
  assert(muse_on_recovery_policy_evaluate(&policy, 100 + 1700000000,
                                          observation) ==
         MUSE_ON_RECOVERY_DONE);
}

static void test_recovery_policy_accepts_background_safety_proof(void) {
  MuseOnRecoveryPolicy policy;
  MuseOnRecoveryObservation observation = {
      .permission_granted = true,
      .controller_connected = true,
      .multiple_controllers = false,
      .codex_foreground = false,
      .inputs_released = true,
      .filter_verified = true,
      .keyboard_open = true,
      .error_observed = false,
  };

  muse_on_recovery_policy_init(&policy, 100);
  assert(muse_on_recovery_policy_evaluate(&policy, 100 + 10000000,
                                          observation) ==
         MUSE_ON_RECOVERY_SUCCESS);
}

/* A stalled recovery fails once at the bounded deadline and stays fail-closed. */
static void test_recovery_policy_timeout_is_fail_closed(void) {
  MuseOnRecoveryPolicy policy;
  MuseOnRecoveryObservation observation = {.error_observed = true};

  muse_on_recovery_policy_init(&policy, 2000);
  assert(muse_on_recovery_policy_evaluate(
             &policy, 2000 + MUSE_ON_RECOVERY_SETTLEMENT_NS / 2,
             observation) == MUSE_ON_RECOVERY_WAIT);
  assert(muse_on_recovery_policy_evaluate(
             &policy, 2000 + MUSE_ON_RECOVERY_SETTLEMENT_NS, observation) ==
         MUSE_ON_RECOVERY_FAILURE);
  assert(muse_on_recovery_policy_evaluate(
             &policy, 2000 + MUSE_ON_RECOVERY_SETTLEMENT_NS + 1, observation) ==
         MUSE_ON_RECOVERY_DONE);
}

static void test_recovery_outcome_protocol_is_typed_and_truthful(void) {
  MuseOnRecoveryObservation observation = {
      .permission_granted = true,
      .controller_connected = true,
      .multiple_controllers = false,
      .codex_foreground = true,
      .inputs_released = false,
      .filter_verified = true,
      .keyboard_open = true,
      .error_observed = false,
  };

  assert(muse_on_recovery_outcome_for(MUSE_ON_RECOVERY_WAIT, observation) ==
         MUSE_ON_RECOVERY_OUTCOME_NEUTRAL_ENTRY_PENDING);
  assert(muse_on_recovery_outcome_for(MUSE_ON_RECOVERY_FAILURE, observation) ==
         MUSE_ON_RECOVERY_OUTCOME_FAILURE);
  assert(strcmp(muse_on_recovery_outcome_string(
                    MUSE_ON_RECOVERY_OUTCOME_NEUTRAL_ENTRY_PENDING),
                "neutral_entry_pending") == 0);
  assert(strcmp(muse_on_recovery_outcome_string(
                    MUSE_ON_RECOVERY_OUTCOME_FAILURE),
                "failure") == 0);
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

static void test_safety_failure_strings(void) {
  assert(strcmp(muse_on_safety_failure_string(
                    MUSE_ON_SAFETY_FAILURE_NONE),
                "none") == 0);
  assert(strcmp(muse_on_safety_failure_string(
                    MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE),
                "hold_release_failed") == 0);
  assert(strcmp(muse_on_safety_failure_string(
                    MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE),
                "pass_through_restoration_failed") == 0);
  assert(strcmp(muse_on_safety_failure_string(
                    MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN),
                "device_state_uncertain") == 0);
  assert(strcmp(muse_on_safety_failure_string(
                    MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT),
                "previous_session_ended_unexpectedly") == 0);
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
  test_background_retry_clears_latch_without_activation();
  test_retry_keeps_latch_when_prerequisites_unmet();
  test_disable_during_safety_latch();
  test_latch_persists_through_ordinary_observation();
  test_only_retry_clears_latch();
  test_disable_wins_over_latch();
  test_named_safety_failure_is_sticky_and_fail_closed();
  test_retry_requires_cleanup_and_every_gate();
  test_unclean_recovery_preserves_proof_then_requires_fresh_entry();
  test_unclean_recovery_rejects_failed_cleanup_and_gates();
  test_recovery_filter_restoration_order();
  test_disable_pending_resolves_to_disabled_after_retry();
  test_unclean_prior_exit_latches_next_launch();
  test_ordinary_gates_auto_recover_without_latch();
  test_safe_quit_requires_verified_cleanup();
  test_disconnect_recovers_on_reconnect();
  test_retry_enters_release_controls_after_cleanup();
  test_recovery_policy_settles_before_emitting();
  test_recovery_policy_accepts_background_safety_proof();
  test_recovery_policy_timeout_is_fail_closed();
  test_recovery_outcome_protocol_is_typed_and_truthful();
  test_status_strings();
  test_inactive_reason_strings();
  test_safety_failure_strings();
  return 0;
}
