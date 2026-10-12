#!/usr/bin/env bash
# 实验/additive-sky-seamless/code/build_probes.sh
# 编译 SCI-C 仓内实测驱动：只读链接生产 Phase2 静态库（不修改任何生产代码）。
# 产物落 run/SCI-403/（gitignore，不入库）。构建串行化加锁。
#
# 库名与构建目录按**实际仓库布局**解析，不写死绝对路径：
#   · 构建目录：$ACSD_BUILD_DIR > build/linux-control > build
#   · 静态库名：libacsd_*.a（历史名 libastrocs_*.a 亦接受）
#   · 需要触发库构建时用该目录自带的构建入口（Makefile / build.ninja）
#
# **全流程持 flock ${ACSD_BUILD_LOCK:-/tmp/astrocs_build.lock}**：该构建树可能正被
# 其它工作流重编，取不到锁就等待，绝不并发链接——并发链接会取到重建到一半的库，
# 读数不可信。若确实需要触发库构建，也在锁内做。
#
# 退出码：0 = 两个探针都产出；非 0 = 失败（原始报错直接透出，不吞）。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="${1:-$ROOT/run/SCI-403}"
mkdir -p "$OUT"

LOCK="${ACSD_BUILD_LOCK:-/tmp/astrocs_build.lock}"
LOCK_WAIT="${ACSD_LOCK_WAIT:-7200}"

# 探针源目录：默认本目录（code/）。可用 $ACSD_PROBE_SRC_DIR 指向一份**同源副本**
# （例如某次重跑目录下记录在案的驱动副本），用于在不改动 code/** 的前提下构建。
PROBE_SRC="${ACSD_PROBE_SRC_DIR:-$HERE}"

# ---------------------------------------------------------------- 布局解析
resolve_build_dir() {
  if [ -n "${ACSD_BUILD_DIR:-}" ]; then printf '%s\n' "$ACSD_BUILD_DIR"; return 0; fi
  local d
  for d in build/linux-control build; do
    if [ -d "$ROOT/$d" ]; then printf '%s\n' "$ROOT/$d"; return 0; fi
  done
  printf '%s\n' "$ROOT/build"
}
BUILD_DIR="$(resolve_build_dir)"

# lib_of <stem> → 该静态库的绝对路径；找不到则输出空串
lib_of() {
  local stem="$1" pfx f
  for pfx in acsd astrocs; do
    f="$BUILD_DIR/lib${pfx}_${stem}.a"
    if [ -f "$f" ]; then printf '%s\n' "$f"; return 0; fi
  done
  printf '\n'
}

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

# 链接顺序沿用旧版的相对次序；去掉只在 ASTROCS_PROBES=ON 时才存在的探针库——
# 两个探针源都不使用 ACSD_PROBE_* 宏（实测 grep 计数为 0），不需要该库。
LIBS_STEMS=(phase2 identifiability hips aio common cfitsio)

# ---------------------------------------------------------------- 构建
ensure_libs() {
  local stem missing=0
  for stem in "${LIBS_STEMS[@]}"; do
    [ -n "$(lib_of "$stem")" ] || missing=1
  done
  [ "$missing" = "0" ] && return 0
  echo "[build_probes] $BUILD_DIR 缺少部分生产静态库 ⇒ 在锁内构建"
  if [ -f "$BUILD_DIR/Makefile" ]; then
    make -C "$BUILD_DIR" -j"$(nproc)"
  elif [ -f "$BUILD_DIR/build.ninja" ]; then
    ninja -C "$BUILD_DIR"
  else
    echo "[build_probes] $BUILD_DIR 既无 Makefile 也无 build.ninja ⇒ 无法构建" >&2
    return 1
  fi
}

build() {
  local src="$1" out="$2"
  echo "== build $out"
  local libs=() stem p
  for stem in "${LIBS_STEMS[@]}"; do
    p="$(lib_of "$stem")"
    if [ -z "$p" ]; then
      echo "[build_probes] 仍缺 lib{acsd,astrocs}_${stem}.a ⇒ 放弃（不改脚本绕过）" >&2
      return 3
    fi
    libs+=( "$p" )
  done
  echo "[build_probes] 构建树 = $BUILD_DIR"
  echo "[build_probes] 源目录 = $PROBE_SRC"
  echo "[build_probes] 静态库 = ${libs[*]}"
  g++ -O2 -std=c++17 -Wall \
      "${INC[@]}" "$PROBE_SRC/$src" "${libs[@]}" -lpthread -ldl -o "$OUT/$out"
}

# ---------------------------------------------------------------- 入口
exec 9>"$LOCK"
if ! flock -w "$LOCK_WAIT" 9; then
  echo "[build_probes] 等待 $LOCK 超过 ${LOCK_WAIT}s ⇒ 放弃（不并发链接）" >&2
  exit 1
fi
echo "[build_probes] 已取构建锁 $LOCK"

ensure_libs
build upm_probe.cpp upm_probe
build sky_probe.cpp sky_probe
echo "built -> $OUT/{upm_probe,sky_probe}"
