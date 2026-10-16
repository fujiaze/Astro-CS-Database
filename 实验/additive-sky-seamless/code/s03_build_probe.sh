#!/usr/bin/env bash
# run/ACSDSDOC-02/S03/code/build_probe_s03.sh
#
# S03 探针构建：静态链接 **S03 独立构建目录** build/linux-sci-p5（不与
# build/linux-sci 并发写）。用法：
#   build_probe_s03.sh [tag]      # tag 默认 pre；产物 p2_sky_joint_probe_s03_<tag>
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
BUILD_DIR="${S03_BUILD_DIR:-$ROOT/build/linux-sci-p5}"
TAG="${1:-pre}"
OUT="$HERE/p2_sky_joint_probe_s03_${TAG}"

INC=(
  -I "$ROOT/lib/algorithms/coverage/include"
  -I "$ROOT/lib/infrastructure/aio/include"
  -I "$ROOT/lib/infrastructure/aio/src"
  -I "$ROOT/lib/infrastructure/aio/third_party/cfitsio"
  -I "$ROOT/lib/algorithms/shared"
  -I "$ROOT/lib/algorithms/shared/include"
  -I "$ROOT/lib/algorithms/shared/crypto"
  -I "$ROOT/lib/algorithms/shared/healpix"
  -I "$ROOT/lib/third_party"
)
LIBS=(
  "$BUILD_DIR/libacsd_phase2.a"
  "$BUILD_DIR/libacsd_identifiability.a"
  "$BUILD_DIR/libacsd_hips.a"
  "$BUILD_DIR/libacsd_aio.a"
  "$BUILD_DIR/libacsd_common.a"
  "$BUILD_DIR/libacsd_cfitsio.a"
)
for p in "${LIBS[@]}"; do [ -f "$p" ] || { echo "missing $p" >&2; exit 3; }; done

# 产物哈希（构建 SHA 证据）：libacsd_phase2.a 的 SHA256 + 关键源码 SHA256
echo "libacsd_phase2.a sha256: $(sha256sum "$BUILD_DIR/libacsd_phase2.a" | cut -d' ' -f1)"
echo "sky_plane.cpp     sha256: $(sha256sum "$ROOT/lib/algorithms/coverage/src/sky_plane.cpp" | cut -d' ' -f1)"

g++ -O2 -std=c++17 -Wall "${INC[@]}" "$HERE/p2_sky_joint_probe_s03.cpp" "${LIBS[@]}" \
    -lpthread -ldl -o "$OUT"
echo "built -> $OUT"
sha256sum "$OUT"
