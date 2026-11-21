#!/usr/bin/env bash
set -euo pipefail

readonly project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly source_dir="${1:-${project_dir}/.build/MeshCore}"

if [[ ! -f "${source_dir}/platformio.local.ini" ]]; then
  echo "Prepared MeshCore tree not found: ${source_dir}" >&2
  echo "Run scripts/prepare-meshcore.sh first." >&2
  exit 2
fi

pio run -d "${source_dir}" -e RAK_4631_victron_sensor

