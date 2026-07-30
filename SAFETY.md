# Codex Muse-On safety report

Status: deterministic v1 safety baseline for the owner's local bundle. This
document describes source and local-test behavior; it does not claim live HID,
TCC, accessibility, UI, installation, or public-distribution acceptance.

## Current v1 behavior

- **Persistent intent and menu-bar control:** First Enable records Enabled
  intent; Disabled records the opposite. The menu-bar app is the user control
  surface and presents Active, Inactive, Disabled, or Safety latch. Enabled is
  not Active: dispatch is permitted only while every Active gate is current.
- **Paired controller identity:** Connected means exactly one validated Muse-On
  with its button and joystick interfaces paired at one physical location.
  Loss of either interface, an unknown topology, or more than one complete
  controller blocks dispatch. A reconnect requires a fresh listener topology,
  verified filtering/capture, and Neutral Entry.
- **Fail-closed control verification:** The listener-task generation owns the
  topology. Active and dispatch require current permission/session/Codex
  foreground gates, one paired controller, fresh Neutral Entry, and verified
  filtering/capture for that same generation. Until filtering/capture is
  verified, the menu reports `Inactive — Verifying control`; `filter_restored`,
  topology change, and a new generation clear that verification. Stale events
  cannot reactivate dispatch.
- **Foreground boundary:** Only frontmost bundle ID `com.openai.codex`
  qualifies. Focus or either paired-interface loss immediately blocks dispatch,
  resets Neutral Entry, and attempts release of any synthetic hold. Foreground
  return alone does not dispatch until a fresh release observation. Enabled,
  connected input remains Reserved while dispatch is blocked.
- **Safety and recovery:** Hold release and Pass-through restoration must be
  verified. A safety failure latches fail-closed; only explicit Retry with
  cleanup and current non-neutral safety gates, including verified
  filtering/capture, can clear it. A fresh Neutral Entry remains required
  before dispatch after recovery.
- **Start Automatically:** This independent preference defaults on after First
  Enable and controls whether macOS opens the menu-bar app at login. It does
  not itself activate dispatch; normal startup revalidates all gates quietly.
- **Local bundle:** the repository provides a stable installed local test-bundle
  contract with the menu-bar executable and listener. The stable local signing
  identity is used when available, otherwise the local bundle is ad-hoc signed.

## Permissions, cleanup, and privacy

- **Permissions:** Input Monitoring is required to observe the paired HID
  interfaces. Accessibility is required to post the dedicated Codex shortcuts.
  Denial, loss, or revocation leaves dispatch blocked and directs the owner to
  macOS Settings; grants apply to the Codex Muse-On app identity, not one USB
  device. Neither permission is evidence of current verified filtering/capture.
- **Cleanup boundary:** Normal Disable, Safe Quit, disconnect, and safety
  recovery release holds and require verified Pass-through restoration. An
  unverified restoration remains Safety latched. Crash, Force Quit, power loss,
  or `SIGKILL` cannot prove cleanup; the following launch treats that Unclean
  Exit fail-closed until Retry revalidates it.
- **Privilege and diagnostics:** The local app does not require Full Disk
  Access, administrator privileges, or network access. Diagnostics are a
  bounded in-memory history of state, action identifiers, and error codes; they
  clear on Quit and exclude raw HID data, typed keys, chat content, and
  unrelated app names.

## Known limitation

Neutral Entry uses the best available kernel-maintained state from validated
HID elements. It is not strict freshness proof: a selected joystick control
held before a cold listener start can appear neutral until a new report arrives.
Avoid holding controls while launching or restarting. This limitation does not
relax the verified filtering/capture, foreground, cleanup, or continuous
tracking gates.

## Distribution boundary

Developer ID signing, notarization, and public binary distribution remain
blocked. Do not treat the local bundle or deterministic checks as public or
live-system acceptance.
