#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
NATIVE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR" && pwd)
BUILD_DIR="$NATIVE_DIR/build"
APP_PATH="$BUILD_DIR/Codex Muse-On.app"
CONTENTS_DIR="$APP_PATH/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"

rm -rf "$APP_PATH"
mkdir -p "$MACOS_DIR"

clang -std=c11 -Wall -Wextra -Werror -pedantic -fobjc-arc -I"$NATIVE_DIR" \
  "$NATIVE_DIR/muse_on_menu_bar.m" \
  "$NATIVE_DIR/muse_on_platform.m" \
  "$NATIVE_DIR/muse_on_state_coordinator.c" \
  "$NATIVE_DIR/muse_on_setup_state.c" \
  "$NATIVE_DIR/muse_on_connection.c" \
  -framework IOKit -framework CoreFoundation \
  -framework AppKit -framework ApplicationServices -framework Carbon \
  -framework ServiceManagement -framework UserNotifications \
  -o "$MACOS_DIR/CodexMuseOn"
clang -std=c11 -Wall -Wextra -Werror -pedantic -I"$NATIVE_DIR" \
  "$NATIVE_DIR/muse_on_listener.c" \
  "$NATIVE_DIR/muse_on_decoder.c" \
  "$NATIVE_DIR/muse_on_action_map.c" \
  "$NATIVE_DIR/muse_on_config.c" \
  "$NATIVE_DIR/muse_on_activation.c" \
  "$NATIVE_DIR/muse_on_shortcut_map.c" \
  "$NATIVE_DIR/muse_on_key_filter.c" \
  "$NATIVE_DIR/muse_on_platform.m" \
  "$NATIVE_DIR/muse_on_state_coordinator.c" \
  "$NATIVE_DIR/muse_on_connection.c" \
  -framework IOKit -framework CoreFoundation \
  -framework AppKit -framework ApplicationServices -framework Carbon \
  -o "$MACOS_DIR/muse_on_listener"
cp "$NATIVE_DIR/app/Info.plist" "$CONTENTS_DIR/Info.plist"
codesign --force --sign - "$APP_PATH"

printf '%s\n' "$APP_PATH"
