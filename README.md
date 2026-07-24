# Codex Muse-On

Codex Muse-On is a pre-v1 local macOS command-line prototype for adapting a
GAMMAC MUSE-ON controller to Codex Desktop shortcuts. It is not the completed
native menu-bar app described in [Issue #1](https://github.com/Kokoabassplayer/codex-muse-on/issues/1).

## Current prototype

The prototype has source-defined, tested mappings and starts in dry-run mode.
Active shortcut dispatch is explicit and requires the relevant macOS
permissions. Its current live-verified dictation mapping is:

- **Controller Only:** holding **Black 8** begins the debounced Codex
  `globalDictationHold` action; releasing it ends the hold.
- **Pedal Enabled:** holding the connected pedal begins the same debounced
  `globalDictationHold` action; releasing it ends the hold. In this profile,
  **Black 8** is assigned to a different environment action.

The prototype keybinding configuration is included at
[`work/native/codex-keybindings.json`](work/native/codex-keybindings.json).
Mappings and behavior may change before v1.

## Local development

This source checkpoint is for macOS development and review only. From
`work/native`, run:

```sh
make test
make analyze
make listener
```

See [SAFETY.md](SAFETY.md) before enabling live dispatch.

## Distribution status

There is no public binary, installer, or installation support. This repository
does not yet provide a supported end-user release; generated app bundles,
binaries, logs, local state, and analyzer outputs are intentionally excluded.

## License

[MIT](LICENSE)
