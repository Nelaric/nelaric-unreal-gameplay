#!/bin/sh
# Copyright (c) 2026 Nelaric
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec sh "$script_dir/../Setup-Unix.sh" Linux
