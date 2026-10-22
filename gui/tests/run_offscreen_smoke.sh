#!/usr/bin/env bash
# ACSD v2 GUI —— 离屏冒烟运行器
#
# 做三件事：
#   1) 设置 QT_QPA_PLATFORM=offscreen（无 X11 环境）；
#   2) 补齐运行期需要的 libGL.so.1 / libEGL.so.1（libQt6Gui.so 的 DT_NEEDED；
#      没有 OpenGL 开发包的最小环境里这两个 so 不在默认搜索路径里）；
#   3) 依次跑 GUI 冒烟（--smoke）与协议级冒烟（protocol_smoke.py）。
#
# 用法：
#   gui/tests/run_offscreen_smoke.sh [构建目录] [证据目录]
#
# 环境变量：
#   ACSD_GUI_BUILD_DIR       构建目录（默认 <repo>/build/gui）
#   ACSD_GUI_EVIDENCE_DIR    证据输出目录（默认 <repo>/gui/evidence）
#   ACSD_GUI_OPENGL_LIB_DIR  额外搜索 libGL.so.1/libEGL.so.1 的目录
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
gui_root="$(dirname "$here")"
repo_root="$(dirname "$gui_root")"

build_dir="${1:-${ACSD_GUI_BUILD_DIR:-$repo_root/build/gui}}"
evidence_dir="${2:-${ACSD_GUI_EVIDENCE_DIR:-$gui_root/evidence}}"
mkdir -p "$evidence_dir"

export QT_QPA_PLATFORM=offscreen

# ---- 运行期 GL 库 ----
gl_missing=0
for soname in libGL.so.1 libEGL.so.1; do
  if ! ldconfig -p 2>/dev/null | grep -q "$soname" && [ ! -e "/usr/lib/x86_64-linux-gnu/$soname" ]; then
    gl_missing=1
  fi
done
if [ "$gl_missing" = "1" ]; then
  candidates=(
    "${ACSD_GUI_OPENGL_LIB_DIR:-}"
    "$HOME/.cache/wklibs/usr/lib/x86_64-linux-gnu"
    "/usr/lib/x86_64-linux-gnu"
    "/usr/lib64"
    "/usr/local/lib"
  )
  for dir in "${candidates[@]}"; do
    [ -n "$dir" ] || continue
    if [ -e "$dir/libGL.so.1" ] || [ -e "$dir/libEGL.so.1" ]; then
      export LD_LIBRARY_PATH="$dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
      break
    fi
  done
fi

echo "== 环境 =="
echo "QT_QPA_PLATFORM=$QT_QPA_PLATFORM"
echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-（未设置）}"
echo "构建目录=$build_dir"

echo
echo "== GUI 离屏冒烟（acsd-gui --smoke） =="
set +e
"$build_dir/acsd-gui" --smoke \
  --config "$here/sample-config.json" \
  --cli "$here/fake_acsd.py" \
  --evidence "$evidence_dir/gui_smoke.json" \
  2>&1 | tee "$evidence_dir/gui_smoke.log"
gui_status=${PIPESTATUS[0]}
set -e

echo
echo "== 协议级冒烟（不依赖 Qt，protocol_smoke.py） =="
set +e
python3 "$here/protocol_smoke.py" \
  --fake "$here/fake_acsd.py" \
  --config "$here/sample-config.json" \
  --check "$build_dir/acsd-protocol-check" \
  --out "$evidence_dir/protocol_smoke.json" \
  2>&1 | tee "$evidence_dir/protocol_smoke.log"
protocol_status=${PIPESTATUS[0]}
set -e

echo
echo "== 汇总 =="
echo "GUI 冒烟退出码=$gui_status"
echo "协议冒烟退出码=$protocol_status"
if [ "$gui_status" = "0" ] && [ "$protocol_status" = "0" ]; then
  echo "全部通过"
  exit 0
fi
echo "存在失败项"
exit 1
