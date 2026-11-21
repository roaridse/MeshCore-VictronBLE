#!/usr/bin/env bash
set -euo pipefail

readonly project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly source_dir="${1:-${project_dir}/.build/MeshCore}"

if [[ ! -f "${source_dir}/platformio.local.ini" ]]; then
  echo "Prepared MeshCore tree not found: ${source_dir}" >&2
  echo "Run scripts/prepare-meshcore.sh first." >&2
  exit 2
fi

# The prepared MeshCore tree is a generated overlay and can easily become
# stale between builds. Always refresh the project-owned sources and build
# configuration before invoking PlatformIO.
cp -a "${project_dir}/firmware/." "${source_dir}/firmware/"
cp "${project_dir}/config/platformio.local.ini" "${source_dir}/platformio.local.ini"

pio run -d "${source_dir}" -e RAK_4631_victron_sensor
