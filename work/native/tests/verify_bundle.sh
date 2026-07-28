#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
NATIVE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
APP_PATH=${1:-"$NATIVE_DIR/build/Codex Muse-On.app"}
CONTENTS_DIR="$APP_PATH/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
PLIST="$CONTENTS_DIR/Info.plist"
LOCAL_SIGN_IDENTITY="Codex Muse-On Local Development"

if [ ! -d "$APP_PATH" ]; then
  echo "bundle not found: $APP_PATH" >&2
  exit 1
fi
if [ ! -f "$PLIST" ]; then
  echo "bundle Info.plist not found: $PLIST" >&2
  exit 1
fi
if [ ! -d "$MACOS_DIR" ]; then
  echo "bundle executable directory not found: $MACOS_DIR" >&2
  exit 1
fi

plutil -lint "$PLIST" >/dev/null

assert_plist_value() {
  key=$1
  expected=$2
  if ! actual=$(plutil -extract "$key" raw -o - "$PLIST"); then
    echo "missing plist key: $key" >&2
    exit 1
  fi
  if [ "$actual" != "$expected" ]; then
    echo "unexpected $key: expected '$expected', got '$actual'" >&2
    exit 1
  fi
}

assert_plist_value CFBundleDisplayName "Codex Muse-On"
assert_plist_value CFBundleName "Codex Muse-On"
assert_plist_value CFBundleIdentifier "com.kokoabassplayer.codex-muse-on"
assert_plist_value CFBundleExecutable "CodexMuseOn"
assert_plist_value LSMinimumSystemVersion "13.0"
assert_plist_value LSUIElement "true"

for executable_name in CodexMuseOn muse_on_listener; do
  executable_path="$MACOS_DIR/$executable_name"
  if [ ! -f "$executable_path" ] || [ ! -x "$executable_path" ]; then
    echo "expected executable missing or not executable: $executable_path" >&2
    exit 1
  fi
done

executable_count=0
for candidate in "$MACOS_DIR"/*; do
  if [ -f "$candidate" ]; then
    executable_count=$((executable_count + 1))
  fi
done
if [ "$executable_count" -ne 2 ]; then
  echo "expected exactly two bundle executables, found $executable_count" >&2
  exit 1
fi

codesign --verify --deep --strict --verbose=2 "$APP_PATH"
codesign --verify --strict --verbose=2 "$MACOS_DIR/CodexMuseOn"
codesign --verify --strict --verbose=2 "$MACOS_DIR/muse_on_listener"

if ! listener_signature_info=$(
  codesign --display --verbose=4 "$MACOS_DIR/muse_on_listener" 2>&1
); then
  echo "$listener_signature_info" >&2
  exit 1
fi
case "$listener_signature_info" in
  *"Identifier=com.kokoabassplayer.codex-muse-on.listener"*) ;;
  *)
    echo "listener does not have its stable code-signing identifier" >&2
    echo "$listener_signature_info" >&2
    exit 1
    ;;
esac

if ! signature_info=$(codesign --display --verbose=4 "$APP_PATH" 2>&1); then
  echo "$signature_info" >&2
  exit 1
fi
if security find-identity -v -p codesigning 2>/dev/null |
    grep -F "\"$LOCAL_SIGN_IDENTITY\"" >/dev/null; then
  case "$signature_info" in
    *"Authority=$LOCAL_SIGN_IDENTITY"*) signature_kind="local-stable" ;;
    *)
      echo "bundle is not signed with the stable local identity" >&2
      echo "$signature_info" >&2
      exit 1
      ;;
  esac
else
  case "$signature_info" in
    *"Signature=adhoc"*) signature_kind="adhoc" ;;
    *)
      echo "bundle is not ad-hoc signed" >&2
      echo "$signature_info" >&2
      exit 1
      ;;
  esac
fi

printf '%s\n' \
  "Verified bundle: $APP_PATH" \
  "CFBundleDisplayName=Codex Muse-On" \
  "CFBundleIdentifier=com.kokoabassplayer.codex-muse-on" \
  "LSMinimumSystemVersion=13.0" \
  "LSUIElement=true" \
  "Executables=CodexMuseOn,muse_on_listener" \
  "Signature=$signature_kind"
