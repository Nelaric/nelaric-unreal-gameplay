#!/bin/sh
# Copyright (c) 2026 Nelaric
set -eu

platform=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$script_dir/.." && pwd)
plugin=$repo/NelaricGameplay/Plugins/Puerts
backend=$plugin/ThirdParty/v8_9.4.146.24
cache=$repo/.tools/puerts
archive=$cache/puerts_v8_94-Unreal_v1.0.9.tgz
expected_hash=0ab80673f6bf59cf7f3f80ff17f254975f811813c4637ba2d85d6455fd1e4e24
url=https://github.com/Tencent/puerts/releases/download/Unreal_v1.0.9/puerts_v8_94.tgz
stage=
partial=

cleanup() {
    if [ -n "$partial" ] && [ -f "$partial" ]; then
        rm -f "$partial"
    fi
    if [ -n "$stage" ] && [ -d "$stage" ]; then
        case "$stage" in
            "$cache"/extract.*) rm -rf "$stage" ;;
            *) echo 'Unexpected extraction path; refusing to clean it up.' >&2 ;;
        esac
    fi
}
trap cleanup 0

for command in node npm curl tar mktemp; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "Required command is missing: $command" >&2
        exit 1
    fi
done

case "$platform" in
    Linux)
        [ "$(uname -s)" = Linux ] || { echo 'Run the Linux Setup script on Linux.' >&2; exit 1; }
        [ "$(uname -m)" = x86_64 ] || { echo 'The bundled Linux V8 backend requires x86_64.' >&2; exit 1; }
        command -v sha256sum >/dev/null 2>&1 || { echo 'sha256sum is required.' >&2; exit 1; }
        library=Lib/Linux/libwee8.a
        ;;
    macOS)
        [ "$(uname -s)" = Darwin ] || { echo 'Run the macOS Setup script on macOS.' >&2; exit 1; }
        case "$(uname -m)" in
            x86_64|arm64) ;;
            *) echo 'The bundled macOS V8 backend requires x86_64 or arm64.' >&2; exit 1 ;;
        esac
        command -v shasum >/dev/null 2>&1 || { echo 'shasum is required.' >&2; exit 1; }
        library=Lib/macOS/libwee8.a
        ;;
    *) echo 'Unknown platform.' >&2; exit 1 ;;
esac

hash_archive() {
    if [ "$platform" = macOS ]; then
        shasum -a 256 "$1" | awk '{ print $1 }'
    else
        sha256sum "$1" | awk '{ print $1 }'
    fi
}

validate_backend() {
    [ -f "$1/Inc/v8.h" ] && [ -f "$1/$library" ] || return 1
    if [ "$platform" = macOS ]; then
        [ -f "$1/Lib/macOS_arm64/libwee8.a" ] &&
            [ -f "$1/Lib/macOSdylib/libv8.dylib" ] &&
            [ -f "$1/Lib/macOSdylib_arm64/libv8.dylib" ] || return 1
    fi
}

[ -f "$plugin/Puerts.uplugin" ] || { echo 'PuerTS source is missing.' >&2; exit 1; }
mkdir -p "$cache"

if [ -e "$backend" ]; then
    validate_backend "$backend" || { echo 'Existing V8 backend is incomplete.' >&2; exit 1; }
    echo 'V8 backend is already installed.'
else
    if [ ! -f "$archive" ] || [ "$(hash_archive "$archive")" != "$expected_hash" ]; then
        partial=$(mktemp "$cache/download.XXXXXX")
        echo 'Downloading the official PuerTS V8 9.4 package...'
        curl --fail --location --retry 3 --silent --show-error --output "$partial" "$url"
        [ "$(hash_archive "$partial")" = "$expected_hash" ] || {
            echo 'V8 archive SHA-256 mismatch.' >&2
            exit 1
        }
        mv -f "$partial" "$archive"
        partial=
    fi

    stage=$(mktemp -d "$cache/extract.XXXXXX")
    echo 'Extracting the V8 backend...'
    tar -xzf "$archive" -C "$stage" 'Puerts/ThirdParty/v8_9.4.146.24'
    extracted=$stage/Puerts/ThirdParty/v8_9.4.146.24
    validate_backend "$extracted" || { echo 'V8 package is incomplete.' >&2; exit 1; }
    mv "$extracted" "$backend"
fi

cp "$repo/Setup/V8-LICENSE" "$backend/LICENSE"
echo 'Configuring PuerTS TypeScript tooling...'
node "$repo/Setup/PrepareEditor.js"
node "$plugin/enable_puerts_module.js"
node "$repo/Setup/EnablePlugin.js"
echo 'PuerTS setup is complete.'
