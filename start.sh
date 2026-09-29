#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release >&2
cmake --build build --target server -j2 >&2
./build/server &
PID=$!
trap 'kill -TERM "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true' SIGTERM SIGINT EXIT

get_snap() {
  find src include -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -exec stat -c '%y %n' {} + 2>/dev/null | sort
}

LAST_SNAP=$(get_snap)
while true; do
  sleep 2
  CUR_SNAP=$(get_snap)
  if [[ "$CUR_SNAP" != "$LAST_SNAP" ]]; then
    echo "[Engine] Rebuilding updated C++ files..." >&2
    if cmake --build build --target server -j2 >&2; then
      kill -TERM "$PID" 2>/dev/null || true
      wait "$PID" 2>/dev/null || true
      ./build/server &
      PID=$!
      echo "[Engine] Server restarted." >&2
    fi
    LAST_SNAP=$(get_snap)
  fi
done
