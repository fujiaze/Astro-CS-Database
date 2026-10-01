#!/usr/bin/env bash
# 实验/additive-sky-seamless/code/run_all.sh —— P5 一键复跑（固定 seed，无网络，零 git 写）
#
#   bash 实验/additive-sky-seamless/code/run_all.sh            # 全量
#   SKIP_BUILD=1 bash 实验/additive-sky-seamless/code/run_all.sh   # 跳过探针重建
#   SKIP_SELFTEST=1 bash 实验/additive-sky-seamless/code/run_all.sh # 跳过物理链自检（不推荐）
#
# 路径一律从脚本自身位置推导（BASH_SOURCE），命令全部从**仓库根**执行。
# 产物：run/SCI-403/{logs,*.bin,*.json}（gitignore）。
#
# 本入口是**非破坏性**的：跑之前把已入库的固化读数备份到
# run/SCI-403/results_prior/，跑之后把本次新读数另存到 run/SCI-403/results_head/，
# 再把固化读数原样放回 results/。重跑永远不会改写仓库内的证据面，
# 新旧读数的差异由比对者自己在 run/ 下做。
#
# 重计算纪律（AGENTS.md §9）：每个 python 步骤经 mem_guard 看门狗执行，超时用
# mem_guard 的 --timeout，**不套外层 timeout(1)**（外层只能杀看门狗本身，
# 子进程会逃逸成孤儿）。本仓根路径含空格（"Astro CS Database"），
# 故看门狗用函数而不是字符串变量——未加引号的展开会被词分割成
# /workspace/Astro + CS + Database/...，起不动且报怪错。
#
# 退出码：任一步失败 ⇒ 1。c1–c7 各腿的退出码 = 该腿的判据失败数是否为 0。
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNIT="$(cd "$HERE/.." && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
cd "$ROOT"
mkdir -p run/SCI-403/logs run/SCI-403/results_prior run/SCI-403/results_head "$UNIT/results/figs"

MAX_RSS_GB="${MAX_RSS_GB:-8}"      # 单进程探针档：正常峰值 < 1 GB ⇒ 8 GB 上限
STEP_TIMEOUT="${STEP_TIMEOUT:-3600}"

mg() { python3 "$ROOT/eng/tools/monitoring/mem_guard.py" \
         --max-rss-gb "$MAX_RSS_GB" --timeout "$STEP_TIMEOUT" "$@"; }

rc=0
note() { echo "[run_all] $*"; }

# --------------------------------------------------------------------------
# 0. 公共前置步：合成数据物理链自检（四个组件全跑）
#
#    本单元是共享物理链的第一个真实消费者，物理链的任何漂移都会直接落到
#    本单元的读数上，所以它是本入口的前置门而不是可选步骤。
#
#    **不得只调 noise_selftest.py 的 A/B 系列**：A/B 用独立 RNG 原语重实现物理链，
#    验的是「方法」（分布口径），不经过生产代码。生产面的门禁责任在
#    m16_scene --selftest、m16_sampling --selftest，以及 noise_selftest 的
#    **C 系列**（C1–C6 直接调 NM.expose / NM.sky_surface_e_per_s / 解析一阶矩）。
#    A2 的判词已拆成 verdict_independent（验方法）与 verdict_production（验代码）
#    两项，两者不可互相替代、必须同时成立。
#    G08-04 整改 T1/T2：旧版 A2 判词只取独立臂、C 系列与 m16_sampling V10 尚不存在。
#    **订正（G08-04 整改 R2）**：旧注释写「注入 7 类物理缺陷后四个组件全部仍判绿」——
#    该表述过头。修前实测只对**表内 6 个注入**成立；对**天光侧平场**（m16_sampling
#    V3 判红）与**删泊松**（m16_scene V4 + m16_sampling V1/V7 判红）修前**能**判红。
#    准确说法：修前对读出噪声/量化/饱和/源侧平场/天空梯度这五个环节无生产面覆盖。
#    注：V6 与 V10 读真实模板 testdata/HST_M16/，该目录被 .gitignore 排除；模板缺失时
#    V6/V10 记红（不静默跳过、不设 waiver 开关）—— 需先备齐数据再跑本入口。
# --------------------------------------------------------------------------
if [ "${SKIP_SELFTEST:-0}" != "1" ]; then
  note "物理链自检（公共前置步）"
  if mg bash "$ROOT/实验/shared/synthetic/run_selftests.sh" \
       > run/SCI-403/logs/selftests.log 2>&1; then
    note "物理链自检全绿（详见 run/SCI-403/logs/selftests.log）"
  else
    note "物理链自检未通过 ⇒ 本次读数不得作为证据"
    tail -n 8 run/SCI-403/logs/selftests.log | sed 's/^/   /'
    rc=1
  fi
fi

# --------------------------------------------------------------------------
# 备份固化读数（跑完再原样放回，见文件头「非破坏性」）
# --------------------------------------------------------------------------
cp -f "$UNIT"/results/*.json run/SCI-403/results_prior/ 2>/dev/null || true
note "固化读数已备份 -> run/SCI-403/results_prior/ ($(ls run/SCI-403/results_prior 2>/dev/null | wc -l) 个文件)"

if [ "${SKIP_BUILD:-0}" != "1" ]; then
  note "构建探针（只读链接生产静态库）"
  if ! mg bash "$HERE/build_probes.sh" > run/SCI-403/logs/build_probes.log 2>&1; then
    note "探针构建失败 ⇒ 依赖探针的各腿不可跑（详见 run/SCI-403/logs/build_probes.log）"
    tail -n 8 run/SCI-403/logs/build_probes.log | sed 's/^/   /'
    rc=1
  fi
fi

for step in c1_additive c2_multiplicative c3_public_plane c4_seam_criterion \
            c5_weights c6_sparse_dense c7_realdata; do
  echo "== $step"
  if mg python3 "$HERE/$step.py" > "run/SCI-403/logs/${step%%_*}.log" 2>&1; then
    echo "   OK"
  else
    echo "   FAILED (see run/SCI-403/logs/${step%%_*}.log)"; rc=1
  fi
  tail -n 20 "run/SCI-403/logs/${step%%_*}.log" | sed 's/^/   /'
done

echo "== 归零负例（接缝度量 + 天光平面生产链路）"
if mg python3 "$HERE/sky_plane_zero_negative.py" > run/SCI-403/logs/zero_negative.log 2>&1; then
  echo "   OK"
else
  echo "   FAILED (see run/SCI-403/logs/zero_negative.log)"; rc=1
fi
tail -n 12 run/SCI-403/logs/zero_negative.log | sed 's/^/   /'

echo "== 冻结记录自检（生产 49 帧读数的可核对面）"
if mg python3 "$HERE/production_e2e_record_check.py" > run/SCI-403/logs/record_check.log 2>&1; then
  echo "   OK"
else
  echo "   FAILED (see run/SCI-403/logs/record_check.log)"; rc=1
fi
tail -n 4 run/SCI-403/logs/record_check.log | sed 's/^/   /'

if mg python3 "$HERE/make_figures.py" > run/SCI-403/logs/figures.log 2>&1; then
  echo "== figures OK"
else
  echo "== figures FAILED (see run/SCI-403/logs/figures.log)"; rc=1
fi

# --------------------------------------------------------------------------
# 非破坏性收口：本次新读数留档，固化读数原样放回
# --------------------------------------------------------------------------
for f in c1_additive c2_multiplicative c3_public_plane c4_seam_criterion \
         c5_weights c6_sparse_dense c7_realdata sky_plane_zero_negative; do
  [ -f "$UNIT/results/$f.json" ] && cp -f "$UNIT/results/$f.json" run/SCI-403/results_head/ || true
done
cp -f run/SCI-403/results_prior/*.json "$UNIT/results/" 2>/dev/null || true
note "本次新读数 -> run/SCI-403/results_head/；results/ 已恢复为固化读数"

echo "== P5 done rc=$rc"
exit $rc
