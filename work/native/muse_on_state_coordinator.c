#include "muse_on_state_coordinator.h"

void muse_on_state_init(MuseOnState *state) {
  state->status = MUSE_ON_STATUS_DISABLED;
  state->inactive_reason = MUSE_ON_INACTIVE_REASON_NONE;
  state->enabled_intent = false;
  state->safety_latched = false;
  state->effects.request_filter = false;
  state->effects.request_dispatch = false;
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
  if (!p.inputs_released) {
    return MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS;
  }
  return MUSE_ON_INACTIVE_REASON_NONE;
}

void muse_on_state_apply(MuseOnState *state, MuseOnCommand command,
                         MuseOnPrerequisites prerequisites) {
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
  }

  /* --- 2. If not Enabled, the state is Disabled and sends nothing --- */
  if (!state->enabled_intent) {
    state->status = MUSE_ON_STATUS_DISABLED;
    state->inactive_reason = MUSE_ON_INACTIVE_REASON_NONE;
    state->safety_latched = false;
    state->effects.request_filter = false;
    state->effects.request_dispatch = false;
    return;
  }

  /* --- 3. Update the internal safety latch (ADR 0003) ---
   *
   * The latch is sticky: a fresh observation of safety_latched=true enters
   * it. Only an explicit Retry whose revalidation finds every gate clear
   * can leave it. A plain observation (NONE) reporting safety_latched=false
   * does NOT clear the latch — the human must press Retry.
   */
  if (prerequisites.safety_latched) {
    state->safety_latched = true;
  } else if (command == MUSE_ON_COMMAND_RETRY &&
             evaluate_inactive_reason(prerequisites) ==
                 MUSE_ON_INACTIVE_REASON_NONE) {
    state->safety_latched = false;
  }

  /* --- 4. Safety latch has the absolute highest priority --- */
  if (state->safety_latched) {
    state->status = MUSE_ON_STATUS_SAFETY_LATCH;
    state->inactive_reason = MUSE_ON_INACTIVE_REASON_SAFETY_LATCH;
    /* Reserved: filtering may remain applied (ADR 0006), but no dispatch. */
    state->effects.request_filter = prerequisites.controller_connected &&
                                    !prerequisites.multiple_controllers;
    state->effects.request_dispatch = false;
    return;
  }

  /* --- 5. Evaluate Active gates in priority order --- */
  state->inactive_reason = evaluate_inactive_reason(prerequisites);

  if (state->inactive_reason == MUSE_ON_INACTIVE_REASON_NONE) {
    state->status = MUSE_ON_STATUS_ACTIVE;
    state->effects.request_filter = true;
    state->effects.request_dispatch = true;
    return;
  }

  /* --- 6. Inactive: filter while connected (Reserved, ADR 0006) --- */
  state->status = MUSE_ON_STATUS_INACTIVE;
  state->effects.request_filter = prerequisites.controller_connected &&
                                  !prerequisites.multiple_controllers;
  state->effects.request_dispatch = false;
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
    case MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS: return "release_controls";
  }
  return "unknown";
}
