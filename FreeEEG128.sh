#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
task_launch="$task_root/Firmware/tools/launch.sh"
if [[ ! -f "$task_launch" ]]; then task_launch="$task_root/tools/launch.sh"; fi
exec bash "$task_launch" "$@"
