#!/usr/bin/env bash
# RELEASE-02 P2a 判别力 Oracle 构建/运行（链接真实 upm.cpp；零 ninja/cmake/ctest）
# 用法: bash run/RELEASE-02/fix-p2a/build.sh
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp/astrocs}"
mkdir -p "$TMPDIR"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
INC=(-Ilib/algorithms/coverage/include -Ilib/algorithms/shared
     -Ilib/algorithms/shared/healpix -Ilib/algorithms/shared/crypto
     -Ilib/infrastructure/aio/include
     -Ilib/infrastructure/observability/probes/include
     -Ilib/infrastructure/acr/include -Ithird_party)
g++ -std=c++20 -O2 -ffunction-sections -fdata-sections "${INC[@]}" \
  run/RELEASE-02/fix-p2a/p2a_oracle.cpp \
  lib/algorithms/coverage/src/upm.cpp \
  lib/algorithms/shared/healpix/healpix_core.cpp \
  lib/algorithms/shared/crypto/sha256.cpp \
  -Wl,--gc-sections -o "$TMPDIR/p2a_oracle"
"$TMPDIR/p2a_oracle" | tee run/RELEASE-02/fix-p2a/oracle_out.txt
