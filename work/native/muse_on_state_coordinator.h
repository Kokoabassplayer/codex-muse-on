#ifndef MUSE_ON_STATE_COORDINATOR_H
#define MUSE_ON_STATE_COORDINATOR_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Deterministic application state coordinator.
 *
 * Sits between the future native UI and the macOS/HID adapters. It accepts
 * approved user commands and observed platform prerequisites and produces a
 * single visible status plus requested effects. It contains zero AppKit,
 * IOKit, HID, permission, or process calls and is fully testable in plain C.
 *
 * Domain vocabulary follows CONTEXT.md:
 *   Disabled      - persistent user-selected state; nothing dispatches.
 *   Enabled       - persistent user-approved intent to become Active.
 *   Active        - temporary state where shortcut dispatch is permitted.
 *   Inactive      - Enabled but at least one Active prerequisite is unmet;
 *                   the menu shows exactly one reason.
 *   Safety latch  - fail-closed after a safety-critical failure; Retry
 *                   revalidates every prerequisite before dispatch resumes.
 */

typedef enum {
  MUSE_ON_STATUS_DISABLED = 0,
  MUSE_ON_STATUS_INACTIVE,
  MUSE_ON_STATUS_ACTIVE,
  MUSE_ON_STATUS_SAFETY_LATCH
} MuseOnStatus;

/*
 * Inactive Reason priority, highest first (CONTEXT.md "Inactive Reason").
 * The coordinator reports exactly one reason at a time. When the status is
 * not Inactive the reason is MUSE_ON_INACTIVE_REASON_NONE.
 */
typedef enum {
  MUSE_ON_INACTIVE_REASON_NONE = 0,
  MUSE_ON_INACTIVE_REASON_SAFETY_LATCH,    /* 1 */
  MUSE_ON_INACTIVE_REASON_PERMISSION,      /* 2 */
  MUSE_ON_INACTIVE_REASON_MULTIPLE,        /* 3 */
  MUSE_ON_INACTIVE_REASON_DISCONNECTED,    /* 4 */
  MUSE_ON_INACTIVE_REASON_SESSION,         /* 5 */
  MUSE_ON_INACTIVE_REASON_NOT_FOREGROUND,  /* 6 */
  MUSE_ON_INACTIVE_REASON_VERIFYING_CONTROL, /* 7 */
  MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS /* 8 */
} MuseOnInactiveReason;

typedef enum {
  MUSE_ON_SAFETY_FAILURE_NONE = 0,
  MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE,
  MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE,
  MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN,
  MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT
} MuseOnSafetyFailure;

typedef enum {
  MUSE_ON_COMMAND_NONE = 0,
  MUSE_ON_COMMAND_DISABLE,
  MUSE_ON_COMMAND_ENABLE,
  MUSE_ON_COMMAND_RETRY,
  MUSE_ON_COMMAND_QUIT
} MuseOnCommand;

/* Recovery HID settlement is bounded to the existing one-second retry cadence. */
#define MUSE_ON_RECOVERY_SETTLEMENT_NS UINT64_C(1000000000)

typedef enum {
  MUSE_ON_RECOVERY_WAIT = 0,
  MUSE_ON_RECOVERY_SUCCESS,
  MUSE_ON_RECOVERY_FAILURE,
  MUSE_ON_RECOVERY_DONE
} MuseOnRecoveryDecision;

typedef enum {
  MUSE_ON_RECOVERY_OUTCOME_WAIT = 0,
  MUSE_ON_RECOVERY_OUTCOME_NEUTRAL_ENTRY_PENDING,
  MUSE_ON_RECOVERY_OUTCOME_SUCCESS,
  MUSE_ON_RECOVERY_OUTCOME_FAILURE
} MuseOnRecoveryOutcome;

typedef struct {
  bool permission_granted;
  bool controller_connected; /* exactly one paired Muse-On */
  bool multiple_controllers;
  bool codex_foreground;
  bool inputs_released;
  bool filter_verified;
  bool keyboard_open;
  bool error_observed;
} MuseOnRecoveryObservation;

typedef struct {
  uint64_t started_at_ns;
  bool emitted;
} MuseOnRecoveryPolicy;

/*
 * Observed platform prerequisites. All fields are plain values supplied by
 * the adapter layer; the coordinator never queries the OS itself. The
 * safety_latched observation sets the internal latch, but only an explicit
 * MUSE_ON_COMMAND_RETRY with all gates clear can clear it.
 */
typedef struct {
  bool safety_latched;      /* safety-critical failure observed */
  MuseOnSafetyFailure safety_failure; /* named failure, when known */
  bool cleanup_verified;    /* hold release and Pass-through are verified */
  bool permission_granted;  /* required control permission present */
  bool controller_connected;/* exactly one complete Muse-On present */
  bool multiple_controllers;/* more than one complete Muse-On present */
  bool session_available;   /* session awake, unlocked, active */
  bool codex_foreground;    /* com.openai.codex frontmost */
  bool filter_verified;     /* current controller filtering/capture verified */
  bool recovery_filter_verified; /* terminal recovery-generation proof */
  bool inputs_released;     /* Neutral Entry: selected-profile inputs freed */
} MuseOnPrerequisites;

/*
 * Effects the coordinator requests of the adapter layer. The coordinator
 * never performs these; it only asks.
 */
typedef struct {
  bool request_filter;    /* apply per-device raw-button filtering */
  bool request_dispatch;  /* shortcut dispatch permitted */
  bool request_cleanup;   /* release holds and verify Pass-through */
} MuseOnEffects;

typedef struct {
  MuseOnStatus status;
  MuseOnInactiveReason inactive_reason;
  bool enabled_intent;    /* persistent Enabled intent (ADR 0001) */
  bool safety_latched;    /* internal latch: survives until Retry clears it */
  MuseOnSafetyFailure safety_failure; /* specific reason for the latch */
  bool disable_pending;   /* Disable intent awaits verified cleanup */
  bool quit_requested;    /* Safe Quit is awaiting verified cleanup */
  bool quit_allowed;      /* current command may finish process termination */
  MuseOnEffects effects;
} MuseOnState;

/* Initialize a coordinator state in the initial Disabled state. */
void muse_on_state_init(MuseOnState *state);

/*
 * Apply a user command (or MUSE_ON_COMMAND_NONE for a pure observation),
 * then fold in observed prerequisites, producing the new visible status,
 * inactive reason, and requested effects.
 *
 * Safety latch is sticky: once entered, it persists in coordinator state
 * until an explicit MUSE_ON_COMMAND_RETRY with every gate clear. A later
 * ordinary observation (MUSE_ON_COMMAND_NONE) with safety_latched=false
 * does NOT auto-resume.
 */
void muse_on_state_apply(MuseOnState *state, MuseOnCommand command,
                         MuseOnPrerequisites prerequisites);

/* Startup/recovery alone are not evidence; a current-state or release
 * observation from the authoritative listener is required. */
void muse_on_neutral_entry_require(MuseOnPrerequisites *prerequisites);

/* Decide whether recovery is still settling or may emit one terminal snapshot. */
void muse_on_recovery_policy_init(MuseOnRecoveryPolicy *policy,
                                  uint64_t started_at_ns);
MuseOnRecoveryDecision muse_on_recovery_policy_evaluate(
    MuseOnRecoveryPolicy *policy, uint64_t now_ns,
    MuseOnRecoveryObservation observation);
bool muse_on_recovery_non_neutral_gates_valid(
    MuseOnRecoveryObservation observation);
MuseOnRecoveryOutcome muse_on_recovery_outcome_for(
    MuseOnRecoveryDecision decision, MuseOnRecoveryObservation observation);
const char *muse_on_recovery_outcome_string(MuseOnRecoveryOutcome outcome);

/* A missing filter is unsafe only before clean recovery is verified. */
bool muse_on_recovery_filter_restoration_unverified(
    bool recovery_validated, bool cleanup_verified, bool permission_granted,
    bool controller_connected, bool multiple_controllers,
    bool session_available, bool codex_foreground, bool inputs_released,
    bool filter_verified);

const char *muse_on_status_string(MuseOnStatus status);
const char *muse_on_inactive_reason_string(MuseOnInactiveReason reason);
const char *muse_on_safety_failure_string(MuseOnSafetyFailure failure);

#endif
