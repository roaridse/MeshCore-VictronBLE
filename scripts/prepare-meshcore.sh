#!/usr/bin/env bash
set -euo pipefail

readonly project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly meshcore_commit="75b04ef06def54b348400cb1b138afb6749ffd4f"
readonly destination="${1:-${project_dir}/.build/MeshCore}"

if [[ -e "${destination}" ]]; then
  echo "Destination already exists: ${destination}" >&2
  echo "Choose an empty path so generated files are never overwritten." >&2
  exit 2
fi

mkdir -p "$(dirname "${destination}")"
git clone https://github.com/meshcore-dev/MeshCore.git "${destination}"
git -C "${destination}" checkout --detach "${meshcore_commit}"
git -C "${destination}" apply "${project_dir}/patches/meshcore-sensor-hooks.patch"
cp -R "${project_dir}/firmware" "${destination}/firmware"
cp "${project_dir}/config/platformio.local.ini" "${destination}/platformio.local.ini"

echo "Prepared MeshCore ${meshcore_commit} in ${destination}"
echo "Build with: pio run -d ${destination} -e RAK_4631_victron_sensor"

