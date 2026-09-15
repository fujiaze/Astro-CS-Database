#!/usr/bin/env bash
# SCHEMA-INTEGRATE-001 / W6 一键验证（单一 rc）。用法: bash tests/contracts/v6/run_all.sh
set -uo pipefail
cd "$(dirname "$0")/../../.." || exit 2
python3 tests/contracts/v6/run_all.py
rc=$?
echo "run_all.sh rc=$rc"
exit $rc
