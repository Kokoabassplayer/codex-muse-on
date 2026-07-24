# Codex Muse-On safety report

Status: pre-publication safety baseline for the local `0.3.1` prototype and
the proposed public-source checkpoint. Reviewable Phase 2 source, mappings,
and tests are prepared for separate approval; app bundles, binaries, logs,
local mapping state, and generated analyzer artifacts remain excluded.

## Current safety behavior

- **Safe default:** launching without arguments selects `dry-run`; real Codex
  actions require an explicit `--mode=active` launch argument.
- **Codex-only dispatch:** captured or active actions route only while the
  frontmost macOS application has bundle ID `com.openai.codex`. Foreground
  status is checked again for each action.
- **App-switch stop:** when Codex loses focus, dispatch is suspended, pending
  push-to-talk release is attempted, and input state is reset. Release failure
  is fatal. The monitor checks focus every 10 ms; this is prompt fail-safe
  handling, not a hard real-time guarantee. The per-device key filter remains
  in place during the helper session so raw controller keys cannot leak into
  another application.
- **Push-to-talk hold:** the pedal path emits debounced begin/end actions and
  is covered by native tests plus live verification. Focus loss, disconnect,
  and cleanup attempt a synthetic hold release; a failed release is fatal.
- **Unplug handling:** a removed controller interface can no longer route actions.
  Synthetic hold release is attempted and the temporary per-device mapping is
  restored or cleared with the disconnected device; release or restoration
  failure is fatal. The helper does not exit after an ordinary unplug, and a
  partial loss of only one HID interface is not yet a guaranteed session-wide
  stop.
- **Fail closed:** active dispatch requires all of: active mode, Codex
  foreground, the exact controller keyboard being captured, and event-posting
  permission. Missing permission, a failed capture, an unknown device
  location, or an event-posting error blocks dispatch. A failed hold release or
  mapping restoration stops the run and returns an error.
- **Narrow device scope:** filtering matches the Muse-On vendor ID, product ID,
  keyboard usage, and nonzero physical location ID. It does not rewrite
  controller firmware and does not intentionally remap other keyboards.

## macOS permissions

- **Input Monitoring** is required to observe the Muse-On HID interfaces.
- **Accessibility / event posting** is required only for `active` mode to send
  the dedicated Codex shortcuts.
- Permission denial or loss must leave action dispatch disabled. Users should
  grant access only to the identified Codex Muse-On app and be able to revoke
  it in **System Settings > Privacy & Security**.
- Event-posting access is rechecked before every active action; Input Monitoring
  state is refreshed once per second. Permission-loss detection is therefore
  fail-closed but not a hard real-time guarantee.
- These macOS grants apply to the app, not to one USB device. The implementation
  narrows input handling to the exact Muse-On identity and gates synthetic
  events to Codex, but users must still trust the granted executable.
- The prototype does not require Full Disk Access, administrator privileges, or
  network access.
- The current local app is ad-hoc signed. A distributed executable needs a
  stable signing identity so permission grants can be scoped to a consistent
  application identity across updates.

## Automatic start and active-mode limitation

Automatic start is not enabled or approved. If login launch is added later, it
must start in `dry-run` and must not silently restore `active` mode. Automatic
restart after a safety or restoration failure must also remain disabled.

The current build has no user-facing mode switch, persistent active-state
indicator, or emergency-stop control. `active` therefore needs explicit
command-line handling and is not suitable for unattended startup or general
distribution yet.

Normal exit and handled termination signals run mapping restoration. A crash or
`SIGKILL` cannot run cleanup; the next launch recognizes its own stale sink
mapping and attempts recovery, but immediate cleanup cannot be guaranteed.

## Required safeguards before publishing executable or controller source

1. Preserve the Codex-foreground, exact-device, capture, and permission gates.
2. Keep app-switch handling able to release holds and stop dispatch. Treat loss
   of either member of the paired HID interfaces as a session-wide stop until
   both interfaces reconnect and are revalidated.
3. Verify temporary mappings are restored on normal exit, handled signals,
   permission loss, and disconnect. Test startup recovery after unclean exit,
   document the `SIGKILL` boundary, and keep restoration failure fail-closed.
4. Add an explicit active-mode control with a continuously visible state and
   an accessible emergency stop before offering automatic start.
5. Keep automatic start in `dry-run` until the user explicitly enables active
   control for that session.
6. Document permission grant and revocation, test on each supported macOS
   release, and publish only reviewed, reproducible artifacts signed with a
   stable release identity.

This report records the present safety boundary; it is not a claim that the
prototype is ready for public installation.
