#!/usr/bin/env sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SOURCE="$SCRIPT_DIR/rtt_cfg.h"
TARGET="$PROJECT_ROOT/rtt_cfg.h"

if [ ! -f "$SOURCE" ]; then
    printf '%s\n' "Error: source file not found: $SOURCE" >&2
    exit 1
fi

if [ -e "$TARGET" ]; then
    if cmp -s "$SOURCE" "$TARGET"; then
        printf '%s\n' "rtt_cfg.h already exists and is up to date: $TARGET"
        exit 0
    fi

    printf '%s\n' "Error: refusing to overwrite existing file: $TARGET" >&2
    printf '%s\n' "Remove it or back it up first, then run this script again." >&2
    exit 1
fi

cp "$SOURCE" "$TARGET"
printf '%s\n' "Copied $SOURCE to $TARGET"
