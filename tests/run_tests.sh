#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

cmake --build build --target challenge_tests -j2 >&2
exec ./build/challenge_tests
