# Codex Muse-On

Codex Muse-On is a macOS controller integration that turns a GAMMAC Muse-On into a safe command surface for Codex.

## Language

**Enabled**:
A persistent, user-approved state that allows the integration to become Active automatically across launches.
_Avoid_: Activated, armed, ready

**Disabled**:
A persistent user-selected state in which the menu-bar app may keep running but cannot capture, filter, or dispatch controller input.
_Avoid_: Inactive, quit

**Disable Pending**:
A persisted user request to become Disabled while safety cleanup is not yet verified. Control remains in Safety latch and sends nothing until Retry succeeds, then finishes Disabled and cannot resume without a new Enable.
_Avoid_: Disabled, automatic resume

**Active**:
The temporary state in which shortcut dispatch is permitted because the exact Muse-On is connected, required permissions are valid, verified controller filtering/capture is current, Neutral Entry is satisfied, and Codex is foreground.
_Avoid_: Enabled, ready

**Foreground Only**:
A control boundary under which Muse-On input never launches, activates, or switches focus to Codex. When Codex is not foreground, the controller remains Reserved and dispatches nothing until the user foregrounds Codex.
_Avoid_: Auto-focus, launch Codex, switch to Codex

**Codex Foreground**:
The state in which the exact frontmost macOS application has bundle ID `com.openai.codex`. Legacy ChatGPT (`com.openai.chat`), display-name matches, executable-name matches, and bundle-ID prefix matches do not qualify.
_Avoid_: ChatGPT foreground, Codex-named app, approximate match

**Neutral Entry**:
A prerequisite for Active requiring every input in the selected Control Profile to appear released after Enable or recovery. Normal automatic Neutral Entry uses the best available kernel-maintained state from each exact, unique, absolute HID element with validated report, usage, descriptor, and value; malformed metadata or any read failure blocks it. This ordinary state is not strict freshness proof: a selected joystick control held before a cold listener start can be indistinguishable from neutral until a new report arrives. Users should avoid holding controller inputs while launching or restarting; the app is intended to remain running at login, with foreground-only dispatch, filtering/cleanup, and live continuous tracking unchanged. Inputs held across a state transition cannot trigger actions; a fresh press is required.
_Avoid_: Resume held input, activate while held

**Release controls**:
An Inactive reason shown when every other Active prerequisite, including verified controller filtering/capture, is satisfied but Neutral Entry cannot yet determine that selected-profile inputs appear released. It clears automatically after a best-available current-state or release observation, without Retry.
_Avoid_: Retry required, stuck input

**Verifying control**:
An Inactive reason shown after all higher-priority ordinary gates pass while the current listener generation has not verified controller filtering/capture. It clears only after that generation reports verified filtering/capture; filter restoration, topology change, or a new generation clears the verification.
_Avoid_: Active before capture, Release controls before filtering

**Inactive**:
An Enabled state in which shortcut dispatch is blocked because at least one Active prerequisite is unmet. The menu presents one specific reason.
_Avoid_: Disabled, ready

**Inactive Reason**:
The single highest-priority explanation for Inactive: a specific Safety latch failure, Permission Required, Multiple Controllers, Muse-On Disconnected, Session unavailable, Codex not foreground, Verifying control, then Release controls.
_Avoid_: Multiple simultaneous reasons, generic unavailable

**Safety latch**:
A fail-closed state entered after a safety-critical failure; dispatch remains blocked until Retry or process restart revalidates every prerequisite. Entry attempts hold release and Pass-through restoration, but an unverified restoration remains latched with a specific error.
_Avoid_: Automatic recovery, generic error

**Safe Quit**:
A Quit request that ends the process only after held actions are released and Pass-through restoration is verified. Failed cleanup keeps the app running in Safety latch until Retry succeeds.
_Avoid_: Force quit, best-effort quit

**Quit Anyway**:
An exceptional, one-confirmation escape from Safety latch or Disable Pending when cleanup cannot be verified. It keeps dispatch blocked, does not claim Safe Quit, and requires released controls, Retry, every gate, and Neutral Entry on the next launch.
_Avoid_: normal quit, verified cleanup

**Unclean Exit**:
A prior process end that did not verify Safe Quit, including a crash, Force Quit, or power loss. The next launch remains Enabled but enters Safety latch until controls are released and Retry verifies cleanup and every Active prerequisite.
_Avoid_: Normal Quit, automatic resume

**Retry**:
An explicit request to revalidate device, permission, filter, and hold safety after a Safety latch. Retry may make the installation's one missing-permission request from the responsible Codex Muse-On app identity if First Enable did not already make it; later retries only recheck and guide to Settings. Login, relaunch, and routine gate changes never prompt.
_Avoid_: Enable, reconnect, automatic permission prompt

**Permission Required**:
An Inactive reason indicating that a required control permission is absent. Denial during First Enable preserves Enabled without recurring or automatic prompts; the app sends nothing and waits for Open Settings or an explicit Retry, while explicit Disable cancels the intent.
_Avoid_: Permission error, recurring prompt, login prompt

**Connected**:
The state in which the Muse-On button interface and joystick interface from the same physical controller are both present and validated. An external Mac keyboard is never required.
_Avoid_: Detected, partially connected

**Disconnected**:
Any state in which the paired Muse-On interfaces are absent, incomplete, or cannot be validated; shortcut dispatch must be Inactive.
_Avoid_: Partially connected

**Multiple Controllers**:
An Inactive reason indicating that more than one complete Muse-On is connected. Dispatch remains blocked until exactly one controller remains.
_Avoid_: Automatically selected controller, first device

**Muse-On button interface**:
The Muse-On controller's own HID surface for its physical play buttons. It is not a Mac keyboard and does not depend on one.
_Avoid_: Keyboard interface, Mac keyboard

**Muse-On joystick interface**:
The Muse-On controller's HID surface for its non-button controls.
_Avoid_: External joystick, Mac input

**Start Automatically**:
A preference, enabled by default after first Enable, controlling whether macOS opens the menu-bar app at login. It remains independent of whether the integration is Enabled or Disabled.
_Avoid_: Launch at Login, login item, automatic activation

**Startup Approval Required**:
A Start Automatically state in which macOS has not approved startup. The current app remains Enabled, shows Open Login Items and Retry, and never repeatedly prompts or enters a Safety latch.
_Avoid_: Permission Required, Disabled

**First Enable**:
The one-time explicit consent that changes a new installation from Disabled to Enabled, begins permission setup, and turns on Start Automatically without requiring connected hardware. Cancelling its setup alert is a strict no-op; completing it without Muse-On present finishes as `Inactive — Muse-On Disconnected` and waits quietly for connection.
_Avoid_: Activate, onboarding wizard

**Control Map**:
The source-defined, tested mapping from physical Muse-On inputs to Codex actions for the selected profile. The first release presents it read-only.
_Avoid_: Editable keybindings, remapping

**Control Profile**:
A persisted manual selection between Controller Only and Pedal Enabled. A profile change releases holds, becomes Inactive, resets input state, and revalidates before dispatch can resume.
_Avoid_: Automatically detected profile, remapping

**Controller Only**:
The default Control Profile, which never depends on the optional pedal.
_Avoid_: No-pedal detection

**Pedal Enabled**:
The optional Control Profile selected when the user wants the pedal mapping. Selection is manual because an idle and unplugged pedal are indistinguishable.
_Avoid_: Auto-detected pedal

**Primary Instance**:
The single running Codex Muse-On process permitted to open controller interfaces or dispatch shortcuts. Any later launch defers to it and exits safely.
_Avoid_: Active instance, duplicate listener

**Reserved**:
An Enabled and Connected state in which raw Muse-On input is filtered even when Codex is not foreground. Only an Active integration may translate that input into actions.
_Avoid_: Active, seized

**Pass-through**:
The Muse-On's original host behavior after its temporary filtering has been removed and restoration verified.
_Avoid_: Disabled, assumed restored

**Session Available**:
The macOS user session is awake, unlocked, and active. An unavailable session forces Control Inactive and requires revalidation before automatic resume.
_Avoid_: Codex foreground, app running

**Status Icon**:
The static monochrome menu-bar indicator with distinct variants for Active, Inactive, Disabled, and Safety latch. Menu text is the authoritative status signal.
_Avoid_: Animation-only status, activity indicator

**Status Menu**:
The fixed menu-bar interface ordered from status and safety controls through profile, Control Map, automatic startup, recovery, support, and Quit.
_Avoid_: Main window, settings window

**Quiet by Default**:
The native menu-bar experience in which routine connection, focus, session, and recovery changes happen automatically without dialogs or notifications. User attention is reserved for First Enable, explicit preferences, macOS-required approval, or a genuine Safety latch.
_Avoid_: Operational dashboard, repeated prompt, confirmation-heavy flow

**Safety Notification**:
One non-repeating macOS notification emitted for each new genuine Safety latch when the user granted optional authorization during First Enable. Denial never blocks Active or causes another prompt; menu status remains the fallback safety signal.
_Avoid_: Routine notification, repeated alert, modal warning

**Diagnostics**:
A bounded in-memory history of state transitions, action identifiers, and error codes that clears on Quit. It excludes raw HID data, typed keys, chat content, and unrelated app names.
_Avoid_: Telemetry, persistent logs, raw input capture

**Report a Problem**:
A user-initiated menu action that opens this repository's GitHub New Issue form with only a safe title or template. It stores no authentication token, never uploads or submits Diagnostics, and requires the user to review and explicitly submit.
_Avoid_: Automatic report, telemetry upload
