#include "muse_on_state_coordinator.h"

void muse_on_state_init(MuseOnState *state) {
  if (!state) return;
  state->status = MUSE_ON_STATUS_DISABLED;
  state->inactive_reason = MUSE_ON_INACTIVE_REASON_NONE;
  state->enabled_intent = false;
  state->safety_latched = false;
  state->safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  state->disable_pending = false;
  state->quit_requested = false;
  state->quit_allowed = false;
  state->effects.request_filter = false;
  state->effects.request_dispatch = false;
  state->effects.request_cleanup = false;
}

/*
 * Determine the single highest-priority Inactive Reason per CONTEXT.md.
 * Returns MUSE_ON_INACTIVE_REASON_NONE when every prerequisite is satisfied.
 * Safety latch is handled by the caller; this function assumes the state is
 * not safety-latched.
 */
static MuseOnInactiveReason evaluate_inactive_reason(
    MuseOnPrerequisites p) {
  if (!p.permission_granted) {
    return MUSE_ON_INACTIVE_REASON_PERMISSION;
  }
  if (p.multiple_controllers) {
    return MUSE_ON_INACTIVE_REASON_MULTIPLE;
  }
  if (!p.controller_connected) {
    return MUSE_ON_INACTIVE_REASON_DISCONNECTED;
  }
  if (!p.session_available) {
    return MUSE_ON_INACTIVE_REASON_SESSION;
  }
  if (!p.codex_foreground) {
    return MUSE_ON_INACTIVE_REASON_NOT_FOREGROUND;
  }
  if (!p.filter_verified) {
    return MUSE_ON_INACTIVE_REASON_VERIFYING_CONTROL;
  }
  if (!p.inputs_released) {
    return MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS;
  }
  return MUSE_ON_INACTIVE_REASON_NONE;
}

static MuseOnSafetyFailure observed_safety_failure(
    MuseOnPrerequisites prerequisites) {
  if (prerequisites.safety_failure != MUSE_ON_SAFETY_FAILURE_NONE) {
    return prerequisites.safety_failure;
  }
  return prerequisites.safety_latched
      ? MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN
      : MUSE_ON_SAFETY_FAILURE_NONE;
}

static bool cleanup_is_verified(MuseOnPrerequisites prerequisites) {
  return prerequisites.cleanup_verified &&
         observed_safety_failure(prerequisites) == MUSE_ON_SAFETY_FAILURE_NONE;
}

static bool recovery_gates_are_clear(
    MuseOnPrerequisites prerequisites) {
  /* Foreground and Neutral Entry are ordinary Active gates. Live filter proof
   * is re-established by the next normal generation after Retry clears. */
  return prerequisites.permission_granted &&
         !prerequisites.multiple_controllers &&
         prerequisites.controller_connected &&
         prerequisites.session_available &&
         prerequisites.recovery_filter_verified;
}

static void set_disabled(MuseOnState *state) {
  state->status = MUSE_ON_STATUS_DISABLED;
  state->inactive_reason = MUSE_ON_INACTIVE_REASON_NONE;
  state->safety_latched = false;
  state->safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  state->disable_pending = false;
  state->quit_requested = false;
  state->quit_allowed = false;
  state->effects.request_filter = false;
  state->effects.request_dispatch = false;
}

static void set_safety_latch(MuseOnState *state,
                             MuseOnSafetyFailure failure,
                             MuseOnPrerequisites prerequisites) {
  state->safety_latched = true;
  if (failure != MUSE_ON_SAFETY_FAILURE_NONE) {
    state->safety_failure = failure;
  } else if (state->safety_failure == MUSE_ON_SAFETY_FAILURE_NONE) {
    state->safety_failure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
  }
  state->status = MUSE_ON_STATUS_SAFETY_LATCH;
  state->inactive_reason = MUSE_ON_INACTIVE_REASON_SAFETY_LATCH;
  /* Enabled + Connected remains Reserved even while dispatch is latched. */
  state->effects.request_filter = state->enabled_intent &&
                                  prerequisites.controller_connected &&
                                  !prerequisites.multiple_controllers;
  state->effects.request_dispatch = false;
}

static void set_active_or_inactive(MuseOnState *state,
                                   MuseOnPrerequisites prerequisites) {
  state->safety_latched = false;
  state->safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  state->inactive_reason = evaluate_inactive_reason(prerequisites);
  if (state->inactive_reason == MUSE_ON_INACTIVE_REASON_NONE) {
    state->status = MUSE_ON_STATUS_ACTIVE;
    state->effects.request_filter = true;
    state->effects.request_dispatch = true;
    return;
  }
  state->status = MUSE_ON_STATUS_INACTIVE;
  state->effects.request_filter = prerequisites.controller_connected &&
                                  !prerequisites.multiple_controllers;
  state->effects.request_dispatch = false;
}

void muse_on_neutral_entry_require(MuseOnPrerequisites *prerequisites) {
  if (!prerequisites) return;
  prerequisites->inputs_released = false;
}

void muse_on_recovery_policy_init(MuseOnRecoveryPolicy *policy,
                                  uint64_t started_at_ns) {
  if (!policy) return;
  policy->started_at_ns = started_at_ns;
  policy->emitted = false;
}

MuseOnRecoveryDecision muse_on_recovery_policy_evaluate(
    MuseOnRecoveryPolicy *policy, uint64_t now_ns,
    MuseOnRecoveryObservation observation) {
  uint64_t elapsed_ns = 0;

  if (!policy || policy->emitted) return MUSE_ON_RECOVERY_DONE;
  if (now_ns >= policy->started_at_ns) {
    elapsed_ns = now_ns - policy->started_at_ns;
  }

  if (muse_on_recovery_non_neutral_gates_valid(observation) &&
      observation.inputs_released) {
    policy->emitted = true;
    return MUSE_ON_RECOVERY_SUCCESS;
  }
  /* Neutral Entry is a user release observation, not HID settlement. */
  if (muse_on_recovery_non_neutral_gates_valid(observation) &&
      !observation.inputs_released) {
    return MUSE_ON_RECOVERY_WAIT;
  }
  if (elapsed_ns >= MUSE_ON_RECOVERY_SETTLEMENT_NS) {
    policy->emitted = true;
    return MUSE_ON_RECOVERY_FAILURE;
  }
  return MUSE_ON_RECOVERY_WAIT;
}

bool muse_on_recovery_non_neutral_gates_valid(
    MuseOnRecoveryObservation observation) {
  /* Foreground is intentionally excluded; dispatch still requires it below. */
  return observation.permission_granted && observation.controller_connected &&
         !observation.multiple_controllers &&
         observation.filter_verified && observation.keyboard_open &&
         !observation.error_observed;
}

MuseOnRecoveryOutcome muse_on_recovery_outcome_for(
    MuseOnRecoveryDecision decision, MuseOnRecoveryObservation observation) {
  if (decision == MUSE_ON_RECOVERY_SUCCESS) {
    return MUSE_ON_RECOVERY_OUTCOME_SUCCESS;
  }
  if (decision == MUSE_ON_RECOVERY_FAILURE) {
    return MUSE_ON_RECOVERY_OUTCOME_FAILURE;
  }
  if (decision == MUSE_ON_RECOVERY_WAIT &&
      muse_on_recovery_non_neutral_gates_valid(observation) &&
      !observation.inputs_released) {
    return MUSE_ON_RECOVERY_OUTCOME_NEUTRAL_ENTRY_PENDING;
  }
  return MUSE_ON_RECOVERY_OUTCOME_WAIT;
}

const char *muse_on_recovery_outcome_string(MuseOnRecoveryOutcome outcome) {
  switch (outcome) {
    case MUSE_ON_RECOVERY_OUTCOME_WAIT: return "wait";
    case MUSE_ON_RECOVERY_OUTCOME_NEUTRAL_ENTRY_PENDING:
      return "neutral_entry_pending";
    case MUSE_ON_RECOVERY_OUTCOME_SUCCESS: return "success";
    case MUSE_ON_RECOVERY_OUTCOME_FAILURE: return "failure";
  }
  return "unknown";
}

bool muse_on_recovery_filter_restoration_unverified(
    bool recovery_validated, bool cleanup_verified, bool permission_granted,
    bool controller_connected, bool multiple_controllers,
    bool session_available, bool codex_foreground, bool inputs_released,
    bool filter_verified) {
  /* Foreground is observed by the host separately from safety proof. */
  (void)codex_foreground;
  return recovery_validated && !cleanup_verified && permission_granted &&
         controller_connected && !multiple_controllers && session_available &&
         inputs_released && !filter_verified;
}

void muse_on_state_apply(MuseOnState *state, MuseOnCommand command,
                         MuseOnPrerequisites prerequisites) {
  MuseOnSafetyFailure observedFailure;

  if (!state) return;
  state->quit_allowed = false;
  state->effects.request_cleanup = false;

  /* --- 1. Fold user command into persistent intent --- */
  switch (command) {
    case MUSE_ON_COMMAND_ENABLE:
      state->enabled_intent = true;
      break;
    case MUSE_ON_COMMAND_DISABLE:
      state->enabled_intent = false;
      break;
    case MUSE_ON_COMMAND_NONE:
    case MUSE_ON_COMMAND_RETRY:
      /* Neither changes the Enabled intent. */
      break;
    case MUSE_ON_COMMAND_QUIT:
      state->quit_requested = true;
      break;
  }

  if (command == MUSE_ON_COMMAND_DISABLE) {
    state->enabled_intent = false;
    state->disable_pending = true;
    state->quit_requested = false;
    state->effects.request_cleanup = true;
    if (cleanup_is_verified(prerequisites)) {
      set_disabled(state);
    } else {
      observedFailure = observed_safety_failure(prerequisites);
      set_safety_latch(state, observedFailure, prerequisites);
    }
    return;
  }

  if (state->disable_pending) {
    if (command == MUSE_ON_COMMAND_RETRY) {
      state->effects.request_cleanup = true;
      if (cleanup_is_verified(prerequisites)) {
        set_disabled(state);
      } else {
        set_safety_latch(state, observed_safety_failure(prerequisites),
                         prerequisites);
      }
    } else {
      set_safety_latch(state, MUSE_ON_SAFETY_FAILURE_NONE, prerequisites);
    }
    return;
  }

  if (command == MUSE_ON_COMMAND_QUIT || state->quit_requested) {
    state->effects.request_cleanup = true;
    if (cleanup_is_verified(prerequisites)) {
      state->quit_allowed = true;
      state->effects.request_filter = false;
      state->effects.request_dispatch = false;
      return;
    }
    set_safety_latch(state, observed_safety_failure(prerequisites),
                     prerequisites);
    return;
  }

  /* --- 2. If not Enabled, the state is Disabled and sends nothing --- */
  if (!state->enabled_intent) {
    set_disabled(state);
    return;
  }

  observedFailure = observed_safety_failure(prerequisites);

  /* --- 3. Update the internal safety latch (ADR 0003) ---
   *
   * The latch is sticky: a fresh observation of safety_latched=true enters
   * it. Only an explicit Retry whose revalidation finds every gate clear
   * can leave it. A plain observation (NONE) reporting safety_latched=false
   * does NOT clear the latch — the human must press Retry.
   */
  if (observedFailure != MUSE_ON_SAFETY_FAILURE_NONE) {
    if (!state->safety_latched ||
        command == MUSE_ON_COMMAND_RETRY) {
      state->safety_failure = observedFailure;
    }
    state->safety_latched = true;
  } else if (state->safety_latched && command == MUSE_ON_COMMAND_RETRY &&
             cleanup_is_verified(prerequisites) &&
             recovery_gates_are_clear(prerequisites)) {
    state->safety_latched = false;
    state->safety_failure = MUSE_ON_SAFETY_FAILURE_NONE;
  }

  /* --- 4. Safety latch has the absolute highest priority --- */
  if (state->safety_latched) {
    state->effects.request_cleanup = command == MUSE_ON_COMMAND_RETRY;
    set_safety_latch(state, MUSE_ON_SAFETY_FAILURE_NONE, prerequisites);
    return;
  }

  /* --- 5. Evaluate Active gates in priority order --- */
  set_active_or_inactive(state, prerequisites);
}

const char *muse_on_status_string(MuseOnStatus status) {
  switch (status) {
    case MUSE_ON_STATUS_DISABLED: return "disabled";
    case MUSE_ON_STATUS_INACTIVE: return "inactive";
    case MUSE_ON_STATUS_ACTIVE: return "active";
    case MUSE_ON_STATUS_SAFETY_LATCH: return "safety_latch";
  }
  return "unknown";
}

const char *muse_on_inactive_reason_string(MuseOnInactiveReason reason) {
  switch (reason) {
    case MUSE_ON_INACTIVE_REASON_NONE: return "none";
    case MUSE_ON_INACTIVE_REASON_SAFETY_LATCH: return "safety_latch";
    case MUSE_ON_INACTIVE_REASON_PERMISSION: return "permission_required";
    case MUSE_ON_INACTIVE_REASON_MULTIPLE: return "multiple_controllers";
    case MUSE_ON_INACTIVE_REASON_DISCONNECTED: return "muse_on_disconnected";
    case MUSE_ON_INACTIVE_REASON_SESSION: return "session_unavailable";
    case MUSE_ON_INACTIVE_REASON_NOT_FOREGROUND: return "codex_not_foreground";
    case MUSE_ON_INACTIVE_REASON_VERIFYING_CONTROL: return "verifying_control";
    case MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS: return "release_controls";
  }
  return "unknown";
}

const char *muse_on_safety_failure_string(MuseOnSafetyFailure failure) {
  switch (failure) {
    case MUSE_ON_SAFETY_FAILURE_NONE: return "none";
    case MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE: return "hold_release_failed";
    case MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE:
      return "pass_through_restoration_failed";
    case MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN:
      return "device_state_uncertain";
    case MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT:
      return "previous_session_ended_unexpectedly";
  }
  return "unknown";
}
