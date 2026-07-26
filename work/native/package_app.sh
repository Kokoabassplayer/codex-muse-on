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
  -framework IOKit -framework CoreFoundation \
  -framework AppKit -framework ApplicationServices -framework Carbon \
  -framework ServiceManagement -framework UserNotifications \
  -o "$MACOS_DIR/CodexMuseOn"
cp "$NATIVE_DIR/app/Info.plist" "$CONTENTS_DIR/Info.plist"
codesign --force --sign - "$APP_PATH"

printf '%s\n' "$APP_PATH"
