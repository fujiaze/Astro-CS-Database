#!/usr/bin/env bash
# 实验/additive-sky-seamless/code/build_probes.sh
# 编译 SCI-C 仓内实测驱动：只读链接生产 Phase2 静态库（不修改任何生产代码）。
# 产物落 run/SCI-403/（gitignore，不入库）。构建串行化加锁。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="${1:-$ROOT/run/SCI-403}"
mkdir -p "$OUT"

INC=(
  -I "$ROOT/lib/algorithms/coverage/include"
  -I "$ROOT/lib/infrastructure/aio/include"
  -I "$ROOT/lib/infrastructure/aio/src"
  -I "$ROOT/lib/infrastructure/aio/third_party/cfitsio"
  -I "$ROOT/lib/algorithms/shared"
  -I "$ROOT/include"
  -I "$ROOT/lib/algorithms/shared/include"
  -I "$ROOT/lib/algorithms/shared/crypto"
  -I "$ROOT/lib/algorithms/shared/healpix"
  -I "$ROOT/third_party"
)
LIBS=(
  "$ROOT/build/libastrocs_phase2.a"
  "$ROOT/build/libastrocs_hips.a"
  "$ROOT/build/libastrocs_aio.a"
  "$ROOT/build/libastrocs_common.a"
  "$ROOT/build/libastrocs_probes.a"
  "$ROOT/build/libastrocs_cfitsio.a"
)

build() {
  local src="$1" out="$2"
  echo "== build $out"
  # 干净检出：先确保生产静态库存在（只读使用，不改源码）
if [ ! -f build/libastrocs_phase2.a ]; then
  echo "[build_probes] build/libastrocs_phase2.a 不存在 ⇒ 先 ninja -C build"
  flock /tmp/astrocs_build.lock ninja -C build || exit 1
fi

flock /tmp/astrocs_build.lock g++ -O2 -std=c++17 -Wall \
    "${INC[@]}" "$HERE/$src" "${LIBS[@]}" -lpthread -ldl -o "$OUT/$out"
}
build upm_probe.cpp upm_probe
build sky_probe.cpp sky_probe
echo "built -> $OUT/{upm_probe,sky_probe}"
