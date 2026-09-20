#!/usr/bin/env bash
# RELEASE-02 A5（SMOOTH-LAMBDA）实验构建：链接真实 upm.cpp。零 ninja/cmake/ctest。
set -euo pipefail
export TMPDIR=${TMPDIR:-/dev/shm/astrocs_lambda}
mkdir -p "$TMPDIR"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
INC=(-Ilib/algorithms/coverage/include -Ilib/algorithms/shared
     -Ilib/algorithms/shared/healpix -Ilib/algorithms/shared/crypto
     -Ilib/infrastructure/aio/include
     -Ilib/infrastructure/observability/probes/include
     -Ilib/infrastructure/acr/include -Ithird_party)
g++ -std=c++20 -O2 -ffunction-sections -fdata-sections "${INC[@]}" \
  reverse_verify/experiments/smooth_lambda/upm_sweep.cpp \
  lib/algorithms/coverage/src/upm.cpp \
  lib/algorithms/shared/healpix/healpix_core.cpp \
  lib/algorithms/shared/crypto/sha256.cpp \
  -Wl,--gc-sections -o "$TMPDIR/upm_sweep"
echo "built: $TMPDIR/upm_sweep"
