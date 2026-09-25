# -*- coding: utf-8 -*-
"""AUD-402 / AUD-402 判读队列 A3（配置与合同面收尾）判读装配。

输入：独立审计/批次清单/P402-A-07..11.csv（124 行）
输出：独立审计/证据/AUD-402-判读-A3.csv（列同输入）
纪律：判读结论按 path:line 逐行给出；每 10 行 append 一次并 flush。
本脚本不读仓库、不改仓库；证据全部由前台代理沿代码路径人工核读后写入 V 表。
"""
import csv
import io
import os
import sys

BASE = r"产出/"
INV = os.path.join(BASE, "inventory")
OUT = os.path.join(BASE, "raw", "AUD-402-判读-A3.csv")
QUEUES = ["P402-A-07.csv", "P402-A-08.csv", "P402-A-09.csv",
          "P402-A-10.csv", "P402-A-11.csv"]
COLS = ["符号/键", "位置(路径:行)", "现行值", "单位", "坐标系/归一化", "精度要求",
        "有效有限域", "来源现状", "出处", "处置", "适用域", "备注"]

# 常用短串
NA = "不适用（结构性常数）"
D02 = "lib/infrastructure/cli/resource_gate.h"
D01 = "lib/infrastructure/cli/resource_events.h"
D03 = "lib/infrastructure/cli/resource_recorder.h"
D04 = "lib/infrastructure/cli/v6_runtime_contract.h"
D05 = "lib/infrastructure/cli/session_commands.h"
D06 = "lib/infrastructure/cli/subcommand.h"
D07 = "lib/infrastructure/cli/runtime_client.cpp"

CT = "eng/contracts/resource_gate_v1.json"          # 契约唯一数值源
OB = "docs/plugins/infrastructure/21_observability.md §8"  # 判据语义唯一权威

# 悬空权威串（07 文档面）
DANGL = ("无——注称『07 §1/§3/§4』；跟踪集内唯一 07_*.md 是 "
         "docs/plugins/algorithms_phase1/07_noise_snr.md（噪声/SNR 面），"
         "git log --diff-filter=D 亦无资源面 07 文档")

# V[key] = dict(可选覆盖列 + 判读五列)
V = {}


def v(key, src, anchor, act, domain, note, unit=None, coord=None, prec=None, fin=None):
    V[key] = dict(来源现状=src, 出处=anchor, 处置=act, 适用域=domain, 备注=note)
    if unit is not None:
        V[key]["单位"] = unit
    if coord is not None:
        V[key]["坐标系/归一化"] = coord
    if prec is not None:
        V[key]["精度要求"] = prec
    if fin is not None:
        V[key]["有效有限域"] = fin


# ============ 结构性初值 / 哨兵比较 / 单位换算（共用一句"为什么不进科学语义"） ============
def struct_init(key, unit, fin, note, coord="—"):
    v(key, NA, "—（不进正式链路的观测字段初值：值由调用方在采样后覆盖，"
               "本身不承载判据或科学量）", NA, "仅作为未接线时的占位", note,
      unit=unit, coord=coord, prec="不适用", fin=fin)


def struct_cmp(key, note, fin="哨兵/合法性判别，非阈值"):
    v(key, NA, "—（比较用的哨兵/零值守卫，不是判据阈值）", NA, fin, note,
      unit="无量纲", coord="—", prec="不适用", fin=fin)


def fmt(key, note):
    v(key, NA, "—（fprintf 精度串，非数值常数）", NA, "产物文本表示", note,
      unit="—", coord="—", prec="见备注", fin="输出格式")


# ---------------------------------------------------------------- A-07 (30)
struct_init(D01 + ":84" and "", None, None, "") if False else None

v(D01 + ":84",
  "无出处（引用指针悬空）", DANGL, "④ 待确认",
  "生产可达：commands.cpp:675 is_stage_priority 只传两参 ⇒ 恒取本默认 5.0",
  "『无标注 stage 且 wall>5s 判 P1』整条判据不在契约、也不在 21_observability §8.3 判据表内；"
  "与 resource_gate.h:336/397 同值三写（本簇 3 行：A07#1、A07#28、A08#7）。",
  unit="s", coord="进程 wall 秒", prec="未规定", fin="wall>5.0 且 kind=Unknown")

struct_init(D02 + ":121", "无量纲（哨兵）", "v>-1 记为已采样",
            "命名哨兵，域安全已核：仅用于非负量（util_samples_*、queue_low_run_seconds、alloc_*）；"
            "可负的 rss_slope 另用显式 bool rss_slope_measured（:193），不与 -1 撞域。"
            "出处仅到本仓记录（批次 P p2007 先例）＝C，非数值来源。")

struct_init(D02 + ":160", "等效核", "n>=0",
            "生产由 commands.cpp:913 覆盖；0.0 与『未测』不可分（本文件其它字段用 -1 表未测），"
            "调用方不填则 0.0 参与 LowAvgCores 判红（偏 fail-closed）。")
struct_init(D02 + ":161", "s", "n>=0", "生产由 commands.cpp:914 覆盖；wall<5s 判据见 :336/:363。")
struct_init(D02 + ":164", "分数 0..1+", ">=0 为已测",
            "-1=未测→MemoryBandwidthLow（『须证明』方向正确）；但 commands.cpp:906 kind 恒 Compute "
            "⇒ Memory 分支在 CLI 生产面不可达，本判据无触发路径。")
struct_init(D02 + ":165", "分数 0..1+", "n>=0",
            "注释要求『pre-frozen、由 BENCH-003 写入、不外推』；生产零写入点 ⇒ 恒 0.0（任何实测值都过）。"
            "同上：Memory 分支不可达。")
struct_init(D02 + ":169", "ms", "n>=0",
            "行内注释只写『可得时』，未定口径（进程级 delayacct vs 块层 await 是两个量）；"
            "在 :391 被当作『有无 IO 证据』的第三腿，Io 分支生产不可达。")
struct_init(D02 + ":175", "ns", ">0 才参与", "生产零填充 ⇒ GlobalLockDegradation（:372）不可达。")
struct_init(D02 + ":176", "ns", ">0 才参与", "同 :175：两侧都需 >0，判据在 CLI 生产面恒不触发。")
struct_init(D02 + ":181", "等效核×100（percent_of_one_core）", "n>=0",
            "生产填 s.avg_cpu_percent（:915）。口径问题立案：同文件 :186 明记阈值侧用『已分配容量百分比』，"
            "而 :368 的 20.0 比的是本字段的『100=1 核』刻度 ⇒ 16 核机上 0.2 核即越过 20『低 CPU』线。",
            coord="单核标度（非容量归一）")
struct_init(D02 + ":182", "等效核×100（系统级 iowait）", "n>=0",
            "生产零填充（commands.cpp 无 g.iowait_percent 赋值点）⇒ :326 的 >50 判据恒不触发，"
            "并使 :368 的 iowait<5 腿恒真。")
struct_init(D02 + ":183", "分数/百分比（未定）", ">=0 为已测",
            "生产零填充 ⇒ :369 的 ( <0 || <15 ) 腿恒真；单位在注释里也没定（带宽比例还是百分数）。")
struct_init(D02 + ":185", "活跃线程数（p50）", ">=0 为已采样",
            "契约 compute.min_active_compute_threads_fallback_statistic=\"peak\"、"
            "fallback_domain=\"有效样本 <2\"；C++ 侧无该回落点，回落用的是配置值 selected/max_active"
            "（:345 另把 2 写成字面量）。")
struct_init(D02 + ":190", "已分配容量百分比", ">=0 为已采样",
            "哨兵方向正确（未采样即跳过），但与 :295 的除零回退 0.0 并用：分母未定时写入 0.0 ⇒ "
            "契约 denominator.zero_denominator_effect 要求『利用率类判据不成立、不参与裁决』，"
            "实现参与并判红。")
struct_init(D02 + ":191", "已分配容量百分比", ">=0 为已采样",
            "同 :190。机械层报的第二个值 18.2 是行内注释『§18.2 下限 85%』的条款号 ⇒ 非字面量，"
            "且 §18.2 属已废止宪章（见 A08#14 注）。")
struct_init(D02 + ":194", "MiB/s（1048576 B/s）", "可正可负",
            "commands.cpp:936 按 bytes/(1024*1024) 填 ⇒ 与契约 memory.unit 强制的 MiB/s 一致 ✓"
            "（契约明文禁 MB/s，口径差 4.858%）。负=收缩，故用 :193 显式 bool 而非负哨兵。")
struct_init(D02 + ":200", "s", ">=10 才具代表性；<0=未提供",
            "语义双向已核：-1（未提供）在 gate_window_representative 被判『视为代表性』→ 放行判定，"
            "与 mon001 家族『-1=跳过判定』方向相反；v6_runtime_contract.h:321 同名字段改用 0.0 表未提供"
            "⇒ 同一事实两载体两编码。")

v(D02 + ":207",
  "无出处", "无——契约无 iowait 键、21_observability §8.3 判据表七项亦无『异常 IO 等待』；"
  "lib 内注称『07 §3』文档不在跟踪集（详见 md §5）",
  "④ 待确认", "CLI 生产面不可达（输入 :182 恒 0.0）",
  "多侧同值无源：C++ 结构体默认 50.0（不可配置）↔ 外挂 judge CLI 默认 50.0"
  "（eng/tools/quality/resource_monitor.py:895 --max-io-wait-percent）⇒ 两侧同数但皆未登记；"
  "违反契约 authority_note『实现侧不得再出现字面量阈值』；:204 同一结构体内 memory_growth_limit "
  "已改从契约取，本键漏改。",
  unit="%（iowait 占区间）", coord="系统级 iowait×100（含他进程，21_observability §8.7）",
  prec="未规定", fin="iowait>50 判 IoWaitHigh")

v(D02 + ":211", "不适用（结构性常数）",
  "实际初值 = kMon001NotSampled（契约哨兵）；机械层报的 .1/0.75 是注释文本",
  NA, "逐样本占比输入，0..1",
  "注释『U>=0.75 样本占比』与契约冲突：per_sample_utilization_min_percent=85、"
  "per_sample_pass_fraction_min=0.7，且本文件 :125 自述『旧 D.6 的 75%/50% 已在契约中废止，不得回归』"
  "⇒ 同一文件注释与冻结值打架（与 :529、A07#20 同案）。",
  unit="分数 0..1", coord="逐样本占比", prec="未规定", fin=">=0.70 判绿")

v(D02 + ":212", "不适用（结构性常数）",
  "实际初值 = kMon001NotSampled；机械层报的 0.50 是注释文本",
  NA, "连续低利用窗，>=0",
  "注释『U<0.50』与契约 compute.queue_low_utilization_percent=60 矛盾（同一废止条款的残留）。",
  unit="s", coord="连续窗长", prec="未规定", fin=">=10 s 判 QueueStarvedCpu")

struct_init(D02 + ":225", "线程秒（等效核·秒）", ">=0 为已提供",
            "生产由 commands.cpp:944 无条件覆盖 ⇒ -1 回算分支仅单元/直调路径可达；"
            "下限 10 核·秒本身取自契约 workload_floor_core_seconds ✓。")

struct_cmp(D02 + ":232", "gate_window_representative 的『未提供』比较（<0 放行）。", "0.0 = 哨兵下界")
struct_cmp(D02 + ":240", "work_core_seconds_of 的『已提供』比较。", "0.0 = 哨兵下界")
struct_cmp(D02 + ":241", "选窗回退：active_window>0 否则用 wall；与 :232 的 <0 三态混用（登记）。")
v(D02 + ":295", NA, "—（除零守卫）", "① 登记出处（现值与契约相反，须改）",
  "allocated_capacity_cores==0 时",
  "返回 0.0 把『算不出』伪装成『利用率 0%』，与契约 denominator.zero_denominator_effect"
  "（不成立→记 allocated_capacity_undeclared、不参与裁决）方向相反；应回哨兵 -1。")
struct_cmp(D02 + ":302", "非 Compute 返回阈值 0 ⇒ 使 :365 的 thr>0 短路。", "0 = 无阈值")
v(D02 + ":304", NA, "—（百分比→分数换算；100.0 是『百分』定义的一部分）", NA,
  "Compute 且分母>=1", "换算方向已核：kCpuMeanMinPercent(85)/100 × 已分配核数 ⇒ 单位=核，"
  "与 avg_equivalent_cores 同量纲 ✓。", fin="(kCpuMeanMinPercent/100)*m")
struct_cmp(D02 + ":343", "workers_p50 的哨兵比较（>=0 才用观测值）。", "0.0 = 哨兵下界")
v(D02 + ":336", "无出处（引用指针悬空）", DANGL + "；契约与 §8.3 判据表均无『无标注>5s → P1』",
  "④ 待确认", "CLI 生产可达（kind 恒 Compute，本判据在 kind 分派之前）",
  "CL-5S 第二处（另两行：A07#1 resource_events.h:84 默认参数、A08#7 :397）：同值三写且无唯一数值源登记。",
  unit="s", coord="进程 wall 秒", prec="未规定", fin="wall>5.0 且无 stage 标注")
v(D02 + ":363", "无出处", DANGL + "；契约 applicability 只规定『严格>10 s』并明记"
  "『C++ 侧原有 wall<5s 静默豁免已废止』", "④ 待确认",
  "仅 active_window 未提供（<0）的直调/单元路径可达",
  "第二个判定域下限（5 s）不在唯一数值源内；处置虽已是显式 NotApplicable（非豁免），"
  "但 5 这个数值本身仍是无源字面量，且与契约唯一域界 10 并存。",
  unit="s", coord="wall 秒", prec="未规定", fin="wall<5.0 → NotApplicable")

# ---------------------------------------------------------------- A-08 (30)
v(D02 + ":368", "无出处", DANGL + "；契约/§8.3 均无『CPU/io/mem 皆低』判据", "④ 待确认",
  "Compute 分支、active window>=10 s",
  "20.0 用单核刻度而本文件阈值口径要求容量归一（见 A07#10）；iowait 腿输入恒 0.0（:182 未接线）"
  "⇒ 三腿判据实际退化为『cpu<20』。簇内共 2 行（:368 含 20.0 与 5.0，:369 含 15.0）。",
  unit="%；s", coord="单核标度 / iowait", prec="未规定", fin="cpu<20 && iowait<5")
v(D02 + ":369", "无出处", DANGL, "④ 待确认", "Compute 分支同上",
  "mem_bandwidth_percent 生产恒 -1 ⇒ 本腿恒真；15.0 无源、单位亦未在契约或注释中定义。",
  unit="未定（分数或 %）", coord="内存带宽占用", prec="未规定", fin="mem_bw<15")
struct_cmp(D02 + ":375", "cpu_p50 哨兵比较（未采样即跳过）——方向正确。")
struct_cmp(D02 + ":377", "cpu_mean 哨兵比较——方向正确。")
struct_cmp(D02 + ":382", "achieved 带宽比率的『未测=FAIL』比较；Memory 分支生产不可达。")
struct_cmp(D02 + ":391", "把『await<=0』当作『无 IO 证据』的一腿：0 等待与未测不可分；Io 分支不可达。")
v(D02 + ":397", "无出处（引用指针悬空）", DANGL, "④ 待确认", "Unknown kind 分支",
  "同 :336 的 5 s 判据复写（CL-5S 第 3 处）；Unknown 分支生产不可达（kind 恒 Compute）。",
  unit="s", coord="wall 秒", prec="未规定", fin="wall>5.0")
v(D02 + ":414", NA, "—（utilization_value 的百分→分数换算，定义性）", NA,
  "逐样本 U 计算", "与 :304/:293 同一换算链，口径一致（100%=已分配容量用满）✓。",
  unit="无量纲换算因子", coord="—", prec="不适用", fin="U∈0..1")
struct_cmp(D02 + ":421", "样本数为正的合法性比较（monitoring_effective）。", "0 = 无样本")
struct_cmp(D02 + ":460", "alloc 面『零有效样本=FAIL』的 fail-closed 比较。", "0 = 无样本")
struct_cmp(D02 + ":479", "diag_message 里的哨兵比较（呈现用，不参与判定）。")
v(D02 + ":492", "C 二手（字符串复写契约值）",
  CT + " compute.mean_utilization_min_percent=85（应写成 kCpuMeanMinPercent/100）",
  "② 可由契约导出", "诊断文本呈现",
  "把阈值 0.85 抄进面向用户的失败文案：契约改值即产生假文案。与 :529 同案（L4）。",
  unit="—", coord="—", prec="不适用", fin="字符串常量")
v(D02 + ":529", "无出处（与契约矛盾的陈旧值）",
  "无——契约是 85/0.70，本串写 0.75；:125 已自述 75% 废止",
  "④ 待确认", "诊断文本呈现",
  "非数值常数（fprintf 前的字符串字面量），但内容假：U>=0.75 与实际判据 U>=0.85 不符 ⇒ 立案 L4。",
  unit="—", coord="—", prec="不适用", fin="字符串常量")
v(D02 + ":542", NA, "—（诊断串里把契约百分值换算成分数的定义性因子）", NA,
  "diag_message 文本", "100.0 = 『百分』定义，与 :304/:414 同一换算（簇内 3 行）；"
  "呈现的是 kMon001QueueUtilMinPercent/100=0.60，与契约 queue_low_utilization_percent=60 一致 ✓。",
  unit="无量纲换算因子", coord="—", prec="不适用", fin="百分比→分数")
v(D02 + ":63", NA, "—（枚举行注释里的 0.85 与 §10.5，非字面量）", NA, "注释",
  "机械层『行内浮点字面量』结论作废：本行只有枚举量 UtilizationP75Low。注内 §10.5 属已废止宪章"
  "（docs/DOCUMENT_INDEX.yaml:1065『原引「冻结宪章」已废止』）⇒ 与 :138/:191/:198/:298 同案（L5）。",
  unit="—", coord="—", prec="—", fin="注释")

for k, note in [
    (D03 + ":128", "100.0 = 本仓 cpu_pct 单位定义（1 核满载=100），出处 resource_recorder.h:5-8 与 "
                   "21_observability §8.7；interval_<=0 的 0.0 回退把『间隔非法』伪装成 0% CPU（登记）。"),
    (D03 + ":155", "同 :128（每线程 CPU 峰值刻度）。"),
    (D03 + ":157", "同 :128（每线程 CPU 和刻度）。"),
    (D03 + ":159", "同 :128；本列口径含他进程 iowait（§8.7 已澄清，消费者须写明档位）。"),
]:
    v(k, NA, "—（单位换算/除零守卫）", NA, "逐样本采集", note,
      unit="等效核×100", coord="percent_of_one_core（非容量归一）", prec="不适用", fin="n>=0")

struct_init(D03 + ":208", "分数 0..1", "0..1",
            "注入式进度；0.0 与『进度 0%』不可分，但判定用的是独立 bool progress_stalled（gate:206），"
            "本字段不进判据。")
v(D03 + ":221", NA, "—（空集守卫）", "④ 待确认（估计器定义无登记）", "v 为空",
  "『无样本』与『p50=0』返回同一 0.0 ⇒ 下游 stage_stats 的 cpu_pct_p50=0 会被当已采样参与判红；"
  "契约要求『有效样本<2 回落峰值』在 C++ 侧无实现点（L8）。")
v(D03 + ":225", "C 二手（规格只写 mean/p50/p95/peak 之名）",
  "无——最近秩变体 idx=round(q*(N-1)) 的定义在 21_observability §8.3 与契约均未登记",
  "④ 待确认", "样本数>=1",
  "p50/p95 的**估计器**未登记 ⇒ 同一批样本两实现可给不同分位；q 本身（0.5）取自契约统计量名，"
  "+0.5 是四舍五入秩。簇内 4 行（:225/:259/:260/:263）。")
struct_init(D03 + ":241", "等效核×100 累加", "n>=0", "均值累加器初值（结构性）。")
struct_init(D03 + ":242", "等效核×100 累加", "n>=0", "同上（每线程 CPU 和累加器）。")
v(D03 + ":259", NA, CT + " compute.p50_utilization_min_percent 的统计量名=\"p50\"",
  "② 可由契约导出（等级）／估计器待确认", "送 gate :190 判据⑤",
  "分位等级 0.50 = 契约统计量名，数值本身不是阈值；估计器定义同 :225。")
v(D03 + ":260", NA, "—（报告面统计量）", NA, "仅写 resource_summary.json",
  "已核无判据消费者：契约 §8.3 七项与 evaluate_gate/mon001/mon002 均不读 p95 ⇒ 不进科学语义。")
v(D03 + ":263", NA, CT + " compute.min_active_compute_threads_statistic=\"p50\"",
  "② 可由契约导出（等级）／估计器待确认", "送 gate :185 判据①",
  "同 :259；但契约另要求有效样本<2 回落 peak，此处仍返回 p50 ⇒ 回落缺失点在此（L8）。")
v(D03 + ":265", "无出处", "无——epsilon 未给物理/数值来源（标准 02 §3 要求）", "④ 待确认",
  "st_.wall_seconds < 1 ms 的极短阶段",
  "RSS 斜率分母下限 1 ms ⇒ 斜率被放大最多 1000×；本列产物 rss_slope_bytes_per_s 又与 "
  "memory_report.h 的『整条曲线稳健斜率』并存两套估计器（gate:448 明禁只看端点）。",
  unit="s", coord="首末样本时间差", prec="未规定", fin="max(0.001, Δt)")
v(D03 + ":29", NA, "—（#if defined 特性宏存在性哨兵）", NA, "__has_include 命中时",
  "机械层判为『命名常数（纯数值初始化）』作废：值只用于 #if defined() 判存在，"
  "不进任何算术；效果仅决定 lock_wait_ns 列是否填实测值。",
  unit="—", coord="—", prec="—", fin="预处理")
fmt(D03 + ":295", "timeseries 是曲线唯一载体（resource_events.h:6-7）：cpu_pct 两位小数=0.01 核，"
    "对 85% 判据分辨率充分；elapsed 三位=1 ms ≪ 10 s 域界。")

# ---------------------------------------------------------------- A-09 (30)
fmt(D03 + ":296", ":295 同一 fprintf 的续行格式串 ⇒ 与 :295 同判（共用一条出处，簇内 A08#29+A09#1）。")
fmt(D03 + ":319", "resource_summary.json 的 wall/overhead 三位小数（1 ms）≪ 10 s 域界 ⇒ 充分。")
fmt(D03 + ":327", "阶段块 JSON 的 wall_seconds 精度。")
for k in [":328", ":329", ":330", ":332", ":333", ":335"]:
    fmt(D03 + k, "阶段统计 %.2f（100=1 核刻度下的 0.01 核）⇒ 对 85%/90% 判据分辨率充分。")
v(D03 + ":369", NA, "定义性换算（100=百分），但**定义本身与权威口径同名不同义**",
  "① 登记出处（须改名或写明口径）", "worker_balance.csv 的 utilization_pct 列",
  "此处 util=active/(active+runnable)×100，而 21_observability §8.2 与契约把『利用率』定义为"
  "占已分配容量百分比 ⇒ 同一产物族里 utilization 两义（L12）；denom==0 回退 0.0 同 :295 类问题。",
  unit="%（并行平衡度，非容量利用率）", coord="active+runnable 归一", prec="不适用",
  fin="denom>0")
fmt(D03 + ":370", "worker_balance.csv 的 util 两位小数。")

struct_init(D03 + ":49", "s", "n>=0", "样本单调时刻初值；生产每条样本由 :124 覆盖。")
struct_init(D03 + ":51", "等效核×100", "n>=0", "见 :128（同一刻度定义）。")
struct_init(D03 + ":53", "等效核×100", "n>=0", "系统级 CPU 占用；注释『可得时』未定口径（登记）。")
struct_init(D03 + ":64", "分数 0..1", "0..1",
            "机械层报的 0.1 是注释『(0..1)』的误切 ⇒ 作废；本字段不被任何判据读。")
struct_init(D03 + ":68", "等效核×100", "n>=0", "每线程 CPU 峰值（区间口径，见 :155）。")
struct_init(D03 + ":69", "等效核×100", "n>=0", "每线程 CPU 和（区间口径，见 :157）。")
struct_init(D03 + ":70", "等效核×100", "n>=0",
            "系统级 iowait，含他进程（21_observability §8.7 已澄清）；送 gate 的 iowait 腿未接线。")
for k, u in [(":77", "s"), (":78", "等效核×100"), (":79", "等效核×100"), (":80", "等效核×100"),
             (":81", "等效核×100"), (":82", "线程数"), (":83", "线程数"), (":84", "线程数"),
             (":88", "等效核×100"), (":89", "等效核×100"), (":91", "等效核×100")]:
    struct_init(D03 + k, u, "n>=0",
                "阶段统计零值：与『该阶段零样本』不可分，但同产物带 n_samples 列可由消费者判别。")
v(D03 + ":97", "无出处", "无——既不在契约，也不在 21_observability/defaults.json；"
  "出厂模板与 CLI 骨架亦无该键", "④ 待确认", "采样线程周期",
  "多侧不一致（L7）四档：本头默认 0.25（生产零调用点）/ CLI 生产实取 0.5"
  "（commands.cpp:720，另有 ProcessMonitor(0.5)、period 0.5、queue 累加 +=0.5 四处复写）/ "
  "外挂 judge 1.0（resource_monitor.py:870 --interval）/ 冻结门 0.2"
  "（run_monitored.py:917 --poll-interval）—— 21_observability §8.6 称三面同义，但样本周期差 5×，"
  "直接改变逐样本判据⑥的样本数与②的连续窗量化。",
  unit="s", coord="墙钟采样周期", prec="未规定", fin="interval>0")

# ---------------------------------------------------------------- A-10 (30)
v(D07 + ":118", NA, "—（Pipeline IR 版本标识串）", "④ 待确认（版本面无登记点）",
  "build_pipeline_ir 产出的 ir[\"version\"]",
  "机械层把字符串 \"1.0.0\" 误切成 .0/1.0 ⇒ 作废。实质问题：scheduler 只校验非空"
  "（lib/infrastructure/scheduler/src/pipeline.cpp:89），eng/contracts/schemas/"
  "pipeline_block.schema.json 不含 version 常量 ⇒ 该版本串无合同约束点，属版本治理缺口非科学常数。",
  unit="—", coord="—", prec="—", fin="IR 标识")
v(D05 + ":157", NA, "—（文案里的 §9.68 条款号）", NA, "注释/文案",
  "机械层『现行值 9.68』作废：input_lights 字段说明里引 ASTROCS_DESIGN §9.68，不是数值。",
  unit="—", coord="—", prec="—", fin="文案")
v(D05 + ":181", "A 负责人裁决 + ① 契约侧登记（但本侧取值相反）",
  "唯一数值源 eng/packaging/config/defaults.json:666-667 drizzle.pixfrac=0.8"
  "（authority_status=owner_adjudicated）；docs/plugins/algorithms_phase1/08_drizzle.md:49 亦 0.8 "
  "并指回该唯一源", "① 登记出处（本侧须改 0.8）",
  "生产可达：CLI `--template` 输出面（用户直接抄用）",
  "六侧默认不一致（L1，本队列首要立案）：模板 1.0 / defaults.json 0.8 / 出厂模板 0.8 / "
  "生产代码兜底 1.0 / 编排器兜底 0.8 / 登记册 1.0，逐侧见 md。defaults.json 自述 1.0 ⇒ "
  "『相邻 drop 仅边界相接…出现无覆盖缝隙』⇒ 科学后果已由其自己的 note 写明。",
  unit="1（收缩因子）", coord="drop 边长/源像素边长", prec="未规定", fin="0<pixfrac<=1")
v(D05 + ":187", NA, "—（filter_passband 文案里的 §9.68）", NA, "注释/文案",
  "机械值 9.68 作废，同 :157。", unit="—", coord="—", prec="—", fin="文案")
v(D05 + ":202", NA, "—（star_detection 文案里的 §4.2）", NA, "注释/文案",
  "机械值 4.2 作废。同段文案里的 max_stars 合同域 [20000,50000] 与编译期默认 20000 是"
  "该键的多侧点，属 A1/A2 星检队列范围，此处只交叉登记不判。",
  unit="—", coord="—", prec="—", fin="文案")
v(D05 + ":254", "C 二手（示例占位）", "phase_config_export.schema.json#/$defs/center 无 default 关键字",
  "④ 待确认（示例值与合法值不可分）", "export `--template` 的 center 块",
  "ra_deg/dec_deg=0.0 是一个**真实天球坐标**（赤经 0h/赤纬 0°），模板不标注即可能被当默认提交；"
  "建议占位串或注释同 :256 的 path/to/ 风格。",
  unit="deg", coord="ICRS", prec="未规定", fin="|dec|<=85（TAN 极区排除）")
v(D05 + ":256", NA, "—（值域文案 1..20000 被误切为 .20000）", NA, "export 模板",
  "实际模板值 1024；与 phase_config_export.schema.json:231-235 width_px∈[1,20000] 相容 ⇒ 无冲突。"
  "机械值作废。", unit="px", coord="—", prec="整数", fin="[1,20000]")
v(D05 + ":257", NA, "—（同上，height_px）", NA, "export 模板",
  "实际值 1024，schema :237-241 [1,20000] 相容。机械值作废。",
  unit="px", coord="—", prec="整数", fin="[1,20000]")
v(D05 + ":258", "C 二手（示例占位，无默认主张）",
  "schema 只给 exclusiveMinimum 0（#/$defs/scale_deg_per_px:324-327），无 default；"
  "出厂模板 export.phase_config.json:11 同值 0.001 ⇒ 两侧一致，不构成多侧分歧", "④ 待确认",
  "export 模板", "0.001 deg/px=3.6″/px 为任取示例：既非科学默认也无推导式；"
  "真实值应由输出像素尺度需求给出（属②类，但无登记）。",
  unit="deg/px", coord="输出像元角尺度", prec="未规定", fin=">0")
v(D05 + ":269", "C 二手（文档条款，未进机器可读侧）",
  "docs/plugins/algorithms_phase3/14_projection.md:38（经 schema 描述转引："
  "phase_config_export.schema.json:243-246『默认 0』）", "① 登记出处",
  "export 模板；键生产消费点未落地（eng/ci/ledgers/dead_config_keys.json 已登记）",
  "两侧同值（模板 0.0 / 文档默认 0）✓；缺口 = schema 未写 JSON default 关键字 ⇒ 机器不可读；"
  "0 度是合法值，示例与默认不可分（同 :254 类，但本键文档已明写默认 0，故不立案）。",
  unit="deg", coord="绕 CRVAL 的取向角", prec="未规定", fin="任意实数")
v(D05 + ":272", "② 可由输入几何导出（且已按式算）",
  "导出式 crpix=(1+W)/2（FITS 1-based）配本模板 W=H=1024 ⇒ 512.5；"
  "缺省语义 = 中心，出处 14_projection.md:35 + schema:247-258", "② 可由输入几何导出",
  "export 模板；键生产零读取（死键台账已登记）",
  "值随模板宽高派生（不是独立默认）⇒ 若模板宽高改动须同步；机械层把它当常数读，"
  "实质是示例占位（注释 :265-266 已自证）。",
  unit="px（1-based）", coord="输出像素帧", prec="0.5 px 语义（像中心）", fin=[1] and "[1,W]")
v(D05 + ":302", NA, "—（bitpix 文案里的 §3.3:256）", NA, "注释/文案",
  "机械值 3.3 作废。本行相邻的 bitpix 默认 -32 属另一键（模板刻意不列值，注释已说明）。",
  unit="—", coord="—", prec="—", fin="文案")
v(D05 + ":380", NA, "—（--help 文案里的 §9.68）", NA, "文案",
  "机械值 9.68 作废。", unit="—", coord="—", prec="—", fin="文案")
for k in (":149", ":175", ":177"):
    v(D06 + k, NA, "—（error 文案里的『§3.5』条款号）", NA, "预检文案",
      "机械值 3.5 作废。这里的 §3.5 指 ASTROCS_DESIGN.md §3.5（现行权威，非已废止宪章）✓。",
      unit="—", coord="—", prec="—", fin="文案")
v(D04 + ":115", NA, "—（拒绝消息串里的『ASTROCS_DESIGN.md 3.1』条款号）", NA, "拒绝文案",
  "机械值 3.1 作废：非数值常数。该消息把条款号写成裸文本『3.1』而非『§3.1』，"
  "与仓内其余引用格式不一（低危，登记）。", unit="—", coord="—", prec="—", fin="文案")
v(D04 + ":122", NA, "—（拒绝消息里的控制项编号 C-004.1）", NA, "拒绝文案",
  "机械层把编号切成 -004.1 ⇒ 作废：psf_snr_power 的 DEFERRED 编号，不是数值。",
  unit="—", coord="—", prec="—", fin="文案")
v(D04 + ":250", NA, "—（错误消息里的『§10.4』条款号）", NA, "拒绝文案",
  "机械值 10.4 作废。实质：§10.4 属已废止『冻结宪章』（DOCUMENT_INDEX.yaml:1065），"
  "本头 :4/:17/:22/:291 亦多处引宪章 §10.4/§10.5/§17.6/§18.2 ⇒ 引用悬空（L5）。",
  unit="—", coord="—", prec="—", fin="文案")
for k, u in [(":298", "s"), (":299", "等效核"), (":305", "s"), (":306", "s"),
             (":314", "%"), (":320", "s")]:
    struct_init(D04 + k, u, "n>=0",
                "§10.5 必采字段面的完备性占位初值：本头明记『数值阈值不在此发明』（:22），"
                "完备性判定见 :336-352。")
struct_init(D04 + ":300", "已分配容量百分比", ">=0 为已采样",
            "与 resource_gate.h:190 同名字段同哨兵 ✓（两载体此处一致）。")
struct_init(D04 + ":301", "已分配容量百分比", ">=0 为已采样", "同 :300。")
v(D04 + ":310", "无出处（键名口径与契约冲突）",
  CT + " memory.unit 明记『MiB/s（1048576 B/s）；禁止再使用 MB/s（1e6 B/s，口径差 4.858%）』",
  "④ 待确认", "heavy run 字段面（CLI 事件/Oracle/CI 共读）",
  "字段名 rss_growth_mb_per_s 与 required_metric_keys() 同名对外发布 ⇒ 名称主张 MB/s，"
  "而唯一数值源禁 MB/s（L13）；本头不做换算，真实单位取决于填充方。",
  unit="名写 MB/s，实取未定", coord="RSS 稳健斜率", prec="未规定", fin="n>=0")
v(D04 + ":318", NA, "—（注释里的值域 0..1 被误切为 .1）", "① 登记出处（口径须改名）",
  "heavy run 字段", "机械值 .1 作废。实质同 L12：worker_balance 定义 active/(active+runnable)，"
  "与 §8.2『利用率=占已分配容量百分比』同名族不同义。",
  unit="分数 0..1", coord="active/(active+runnable)", prec="未规定", fin="0..1")
v(D04 + ":321", NA, "—（初值本身结构性）", "④ 待确认", "active 计算窗",
  "默认 0.0 而 GateConfig 同名字段默认 -1.0（未提供）⇒ 两编码；且 :347 用 >=0.0 判完备，"
  "故『零长窗』被记为字段完备（L14）。", unit="s", coord="active 窗", prec="未规定", fin=">=0")

# ---------------------------------------------------------------- A-11 (4)
v(D04 + ":344", NA, "—（完备性谓词：>0 即要求正证据，方向 fail-closed）", NA,
  "heavy_metrics_complete", "0 作『缺字段』用：每线程 CPU 和为 0 视为未采 ⇒ 与 :347 的 >=0 不一致。")
v(D04 + ":345", NA, "—（同上，单线程峰值）", NA, "heavy_metrics_complete",
  "同 :344：谓词本身非阈值；不一致点记在 :347。")
v(D04 + ":346", NA, "—（同上，wall 必须为正）", NA, "heavy_metrics_complete", "同 :344。")
v(D04 + ":347", NA, "—（谓词，但取值方向与他三行相反）", "④ 待确认",
  "heavy_metrics_complete", ">=0.0 使『active 窗未测=0 s』通过完备性检查；而 GateConfig 以 -1 表未提供、"
  "并据 <0 走『视为代表性』放行判定 ⇒ 同一字段两侧语义分叉（L14）。")

# ---------------------------------------------------------------- 装配
def main():
    if os.path.exists(OUT):
        os.remove(OUT)
    total = 0
    judged = 0
    missing = []
    new = not os.path.exists(OUT)
    fh = io.open(OUT, "w", encoding="utf-8-sig", newline="")
    wr = csv.writer(fh)
    wr.writerow(COLS)
    for q in QUEUES:
        with io.open(os.path.join(INV, q), "r", encoding="utf-8-sig", newline="") as f:
            rd = list(csv.DictReader(f))
        for row in rd:
            total += 1
            key = row["位置(路径:行)"].strip()
            d = V.get(key)
            if d is None:
                missing.append("%s | %s | %s" % (q, key, row["现行值"]))
                d = dict(来源现状="未判", 出处="", 处置="", 适用域="", 备注="")
            else:
                judged += 1
            out = []
            for c in COLS:
                if c in d and d[c]:
                    out.append(d[c])
                else:
                    out.append(row.get(c, ""))
            wr.writerow(out)
            if judged % 10 == 0:
                fh.flush()
    fh.close()
    print("rows total=%d judged=%d missing=%d" % (total, judged, len(missing)))
    for m in missing:
        print("MISSING", m)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
