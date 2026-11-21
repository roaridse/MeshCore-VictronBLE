#!/usr/bin/env bash
set -euo pipefail

readonly project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly output="${TMPDIR:-/tmp}/meshcore-victron-decoder-test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I "${project_dir}/firmware/victron" \
  "${project_dir}/tests/test_decoder.cpp" \
  "${project_dir}/firmware/victron/VictronDecoder.cpp" \
  "${project_dir}/firmware/ruuvi/RuuviDecoder.cpp" \
  -I "${project_dir}/firmware/ruuvi" \
  -o "${output}"
"${output}"
