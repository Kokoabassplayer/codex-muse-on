# Adopt a stable app identity

The native utility uses bundle identifier `com.kokoabassplayer.codex-muse-on` and display name **Codex Muse-On**, replacing the prototype `.monitor` identity before menu-bar permissions or Start Automatically are implemented. macOS privacy grants and login-item registration attach to application identity, so stabilizing it now avoids knowingly forcing users through a later identity migration and reauthorization.
