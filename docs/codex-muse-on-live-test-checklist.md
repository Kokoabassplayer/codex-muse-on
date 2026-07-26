# Codex Muse-On v1 live-test checklist

Run only after explicit approval to use the stable local install. These checks
remain outside deterministic repository verification:

- [ ] Paired HID discovery finds exactly one complete Muse-On.
- [ ] Two complete Muse-On controllers produce `Inactive — Multiple Controllers`, dispatch nothing, and recover automatically only when exactly one complete controller remains and the Active prerequisites/Neutral Entry pass.
- [ ] Pedal reports work when `Pedal Enabled` is selected.
- [ ] Per-device filter apply, readback, and restoration succeed.
- [ ] Disconnect is safe in either interface order; no raw-key leakage occurs.
- [ ] Input Monitoring, Accessibility, and notification TCC prompts and revocation behave correctly.
- [ ] Exact `com.openai.codex` dispatch works and other frontmost apps receive nothing.
- [ ] Start Automatically approval and login launch work.
- [ ] Sleep, lock, and wake stop and safely revalidate Control.
- [ ] Safety notification is delivered once per new Safety latch.
- [ ] A second launch preserves single-instance behavior.
- [ ] Rich popover, status icon, and Controller Map are legible.
- [ ] Keyboard navigation and VoiceOver access cover the popover and text-equivalent map.
- [ ] Safe Quit releases holds and restores Pass-through before exit.
- [ ] Abnormal-exit recovery after crash, Force Quit, or power loss remains latched until verified Retry.
