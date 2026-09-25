# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, r"独立审计/复算件/q402-BD1")
from emit import emit

R = [
# ── 14 kLimit ────────────────────────────────────────────────────────────
["kLimit", "eng/tests/unit/mem_wire_test.cpp:223", "2500",
 "字节（测试内抽象单位；同块 kPerNode=1000 为\"单节点峰值\"）",
 "调度器内存上限（Scheduler(8,8,kLimit)）", "不适用", "0 < limit 时启用受控回压",
 "无（结构性）", "无（行内注释 :223 已给出取值的推导逻辑）",
 "不适用（结构性常数）",
 "回压判据：2×1000 ≤ 2500 < 3×1000 ⇒ 恰好 2 个在途",
 "取值 2500 不是拍的：由同行 kPerNode=1000 与\"允许 2 个在途\"两个设计量导出，注释 :223 写成不等式，等价于 `2·kPerNode < limit < 3·kPerNode`。**该导出未导出**：写成字面量后 kPerNode 若改动判据会静默失效（无断言守护该不等式）；建议改为表达式。出现处数 tot=8（lib 4/eng 4）。注：本文件 :319 的 limit 才是真导出式，见下一行"],

# ── 15 scale_deg_per_px ──────────────────────────────────────────────────
["scale_deg_per_px", "eng/tests/unit/mem_wire_test.cpp:312", "0.001",
 "度/像素", "PlanInputMetadata（规划估算输入）", "不适用（激励值）", ">0；参与 n_side/面积估算",
 "无（测试激励值）", "无",
 "不适用（结构性常数——测试激励）",
 "estimate_plan 的内存峰值估算输入",
 "= 3.6″/px，落在真实数据尺度域内（对照 docs/algorithms/PHASE2_SAMPLER.md:78 的 0.9890″/px 与 GATES 文档的 0.9″/px 下限），但作为估算器激励无口径要求。真正需要出处的量（估算模型自身的系数）在 lib/infrastructure/scheduler 的 estimate_plan 内，不属本符号。出现处数 tot=143（lib 55/eng 70/docs 16/实验 2）——同名键跨规划/度量多处使用，机械层的计数不可当\"该常数的副本数\"读"],

# ── 16 limit ─────────────────────────────────────────────────────────────
["limit", "eng/tests/unit/mem_wire_test.cpp:319", "表达式 per_node + per_node/2（机械层记作 1.5）",
 "字节（同 kLimit 域）", "调度器上限", "不适用", ">0",
 "无（结构性，且已是导出式）",
 "无（行内注释 :319 说明\"同时最多 1 个在途（1.5×）\"，且 per_node 来自估算器 :317，非硬编码）",
 "不适用（结构性常数）",
 "回压判据的\"独立输入\"侧",
 "正例：这是本队列里少数**已按公式写**的量（从 estimate_plan 输出导出，注释 :306 明说\"判据的独立输入，非硬编码\"）。机械层把表达式里的 `1.5`（即 /2 的商）提成\"现行值 1.5\"，制造了一个并不存在的常数。出现处数（模式 `per_node / 2`）tot=1"],

# ── 17 cpu_percent ───────────────────────────────────────────────────────
["cpu_percent", "eng/tests/unit/mon001_gate_test.cpp:34", "95.0",
 "百分比（0-100）", "GateConfig 逐任务观测量", "不适用（激励值）", "[0,100]",
 "无（测试激励值；被测阈值有源）",
 "被测阈值唯一源：eng/contracts/resource_gate_v1.json `compute.per_sample_utilization_min_percent=85`、`p50=90`、`mean=85`（经 CMakeLists.txt:135-154 生成 resource_gate_thresholds_generated.h）；判据语义权威 docs/plugins/infrastructure/21_observability.md §8",
 "不适用（结构性常数——门测试激励）",
 "MON-001 逐样本利用率门（70% 样本 ≥0.85）",
 "取 95 的语义=高于全部三条阈值 ⇒ 绿例激励；同族阈值 record_and_justify 的诚实登记值得引用：JSON compute.enforcement_note「85% 均值门在 16-worker 真负载上实测仅 65.09%，未标定前不得硬失败」⇒ 该族阈值属**③需实验标定且尚未标定**，非本文重点但应进 D4 台账。出现处数 tot=109（lib 15/eng 92/docs 2）"],

# ── 18 cpu_p50_percent ───────────────────────────────────────────────────
["cpu_p50_percent", "eng/tests/unit/mon001_gate_test.cpp:36", "95.0",
 "百分比", "active window 内 p50 统计量", "不适用（激励值）", "[0,100]；仅 active window >10 s 时生效",
 "无（测试激励值）",
 "阈值 90 唯一源 eng/contracts/resource_gate_v1.json `compute.p50_utilization_min_percent`；适用域 10 s 同源 `applicability.min_active_window_seconds_exclusive`",
 "不适用（结构性常数——门测试激励）",
 "MON-002 CpuP50Low 诊断（record_and_justify）",
 "与同行 cpu_percent=95.0 同值：两个统计量取同一个数，使\"p50 与瞬时值分离\"的判别力在本用例中不成立（若生产把 p50 误读成 cpu_percent，此断言仍绿）。出现处数 tot=25（lib 7/eng 15）"],

# ── 19 cpu_mean_percent ──────────────────────────────────────────────────
["cpu_mean_percent", "eng/tests/unit/mon001_gate_test.cpp:37", "90.0",
 "百分比", "active window 内均值", "不适用（激励值）", "[0,100]",
 "无（测试激励值）", "阈值 85 同源 resource_gate_v1.json `mean_utilization_min_percent`",
 "不适用（结构性常数——门测试激励）",
 "MON-002 CpuMeanLow（record_and_justify）",
 "90.0 恰高于均值阈 85、恰等于 p50 阈 90 —— 与 :36 的 95 交叉后无法区分\"均值门\"与\"p50 门\"谁在起作用（两门阈值差 5 个百分点，激励值却落在两者的另一侧边界上）。建议激励值显式偏离任一侧。出现处数 tot=30（lib 11/eng 15）"],

# ── 20 iowait_percent ────────────────────────────────────────────────────
["iowait_percent", "eng/tests/unit/mon002_gate_test.cpp:202", "80.0",
 "百分比", "GateConfig 观测输入", "不适用（激励值）", "[0,100]",
 "无（激励值）；**被测阈值无档**",
 "被测阈值 50.0 的唯一源是代码字面量 lib/infrastructure/cli/resource_gate.h:207（`io_wait_high_percent = 50.0`，GateConfig 成员默认）；eng/contracts/resource_gate_v1.json 内 **iowait/io_wait 零命中**，生成头亦无对应键",
 "④待确认（并案：唯一数值源被旁路）",
 "MON-002 IoWaitHigh 诊断",
 "**立案（本案最有价值的一类）**：resource_gate_v1.json:7 自述\"本文件是该权威下的**唯一数值源**，实现侧不得再出现字面量阈值\"，而 :207 的 50.0、:368 的 20.0/5.0、:369 的 15.0 四条阈值仍以字面量活在实现侧且不在唯一源内 ⇒ 契约自述与实现不一致（不是判据错误，是治理面破口）。行内注释 :198 用 \"80% > 50%\" 说明意图，即 50 是隐式共享前提。出现处数 tot=14（lib 5/eng 9）"],

# ── 21 active_window_seconds ─────────────────────────────────────────────
["active_window_seconds", "eng/tests/unit/mon004_enforcement_test.cpp:46", "12.0",
 "秒", "GateConfig 观测输入（active 窗口长度）", "不适用（激励值）", ">10 s 才具代表性",
 "无（激励值）；被测阈值有源",
 "被测两量皆有唯一源：适用域 10 s = resource_gate_v1.json `applicability.min_active_window_seconds_exclusive`；工作量下限 10 核·秒 = 同文件 `workload_floor_core_seconds`（→ resource_gate.h:147-148 kMon003MinCoreSeconds）",
 "不适用（结构性常数——门测试激励）",
 "work_core_seconds = avg_equivalent_cores × active_window_seconds（本行 2.0×12=24 ≥ 10）",
 "12.0 是\"刚过 10 s 适用域\"的紧激励，设计意图明确；:40 直接断言 kMon003MinCoreSeconds==10.0，把契约数值钉进测试（正向：契约改值即红）。本行无缺陷。出现处数 tot=30（lib 19/eng 9）"],

# ── 22 mem_bandwidth_percent ─────────────────────────────────────────────
["mem_bandwidth_percent", "eng/tests/unit/mon004_enforcement_test.cpp:91", "90.0",
 "百分比（相对峰值带宽）", "GateConfig 观测输入；-1 表示未测", "不适用（激励值）", "≥0；-1=未测（哨兵）",
 "无（激励值）；被测阈值无档",
 "被测阈值 15.0 唯一源＝代码字面量 resource_gate.h:369；契约 JSON 无带宽键",
 "④待确认（并案：唯一数值源被旁路；与本行同族的 15.0 未登记）",
 "MON-004「CPU/io/mem 皆低 ⇒ 禁止以单线程算法解释」的组合前提",
 "本行取 90.0 的**目的**是让\"带宽很高\"从而不触发 :368-369 的三低组合，从而把断言锁定在 LowAvgCores 单一诊断上（:86 注释\"< 0.85*4 = 3.4 → 旧失败案例\"）——测试逻辑自洽；但承载它的 15.0/20.0/5.0 三常数处于唯一源之外。另：成员默认 -1.0（:183）作为\"未测\"哨兵，与 0.0（测得为 0）语义不同，该哨兵纪律有注释、无契约条款。出现处数 tot=8（lib 2/eng 6）"],

# ── 23 max_dev0 ──────────────────────────────────────────────────────────
["max_dev0", "eng/tests/unit/p1001_real_nodes_test.cpp:1516", "0.0",
 "无量纲（支持度偏差绝对值上确界）", "逐像素 max 归约（tile 0）", "不适用", "非负",
 "无（结构性）", "无", "不适用（结构性常数——累加器初值）",
 "HiPS tile support 面回归",
 "同行 max_dev0/max_dev1 成对，覆盖两 tile；判据阈值不在此行（本用例的容差为独立常量）。模式 `max_dev0` tot=3（全在 eng/tests）——机械层把累加器初值登记成\"待确认常数\"是无效工作量，建议在机械层按\"= 0.0/0.0f 且出现在 max/sum 累加语境\"预先归入结构性旁表"],

# ── 24 kPxScaleDeg ───────────────────────────────────────────────────────
["kPxScaleDeg", "eng/tests/unit/p1001_real_nodes_test.cpp:1810", "0.0002777777777777778",
 "度/像素", "WCS CD 矩阵元素（|det CD|^0.5）", "缺（oracle 一致性判据为 worst<1e-9，未登记到该常数本身）", ">0",
 "B（本仓可复现：值由算式导出且在测试内被用作 oracle 输入）",
 "数值可核验为 1″/px：1/3600=0.0002777777777777778；行内注释标为 sqrt|det CD| deg/px。同值在 :1829/:1830 以 CD 对角（-s,+s）出现，另 GATES 文档的 0.9″/px 适用域下限为**不同量**（角秒/像素）",
 "②可由关系导出（应写成 1.0/3600.0 或 3600 分之一，而非 19 位字面量）",
 "SIP oracle 桥接回归的像素尺度",
 "该常数是\"整数量（1 角秒）的单位换算\"，属可导出且零风险，但字面量写法把单位口径埋进小数里：读不出\"1″/px\"这一设计意图，也看不出它是 1/3600 而非 1/3600.0 的截断。出现处数 tot=4（lib 2/eng 2）"],

# ── 25 min_sep_px ────────────────────────────────────────────────────────
["min_sep_px", "eng/tests/unit/p1001_real_nodes_test.cpp:1851", "0.9（断言下限；变量名 min_unbridged_sep_px）",
 "像素", "未桥接（0-based 当 1-based）读法与桥接读法的角距离差", "缺（0.9 的余量依据未登记）",
 "仅当像素尺度 ≥ 适用域（本用例 1″/px）时有意义",
 "C（文档给的是 ≥1 px，不给 0.9）",
 "docs/algorithms/GATES_AND_TOLERANCES.md:67（G-P1-WCS-BRIDGE 负对照条款：\"移除或错置 +1 桥接必须 ≥1 px 偏差\"）；行内推导注释 :1835-1836（\"必须相差 ≥0.9 px，否则本 oracle 无鉴别力(恒真)\"）",
 "②可由关系导出（名义值 = 1 px 错置 ⇒ 1 px 位移；0.9 为其 90% 余量），但余量比例本身待确认",
 "W4-A1 像素原点判别的判别力自证门",
 "**三处需登记**：①文档条款写 ≥1 px、测试执行 ≥0.9 px（判据面 10% 松弛，未在任何文档说明为何允许）；②1 px 是名义值，实际分离度经 SIP 项 A=8.0e-5·dx² / B=-8.0e-5·dy²（:1824-1825）扰动后 ≠ 1 px，故 0.9 是\"扰动后仍应接近 1\"的经验下界，属③需标定而未标；③与本行数字撞车的 `min_scale_arcsec = 0.9″/px`（GATES:67、PHASE3_PROJ_IMPL:255 等，适用域下限，量纲不同）易被误读为同一常数——D4 台账应显式区分。出现处数（模式 min_sep_px|min_unbridged）tot=9（eng 5/实验 4）"],
]
emit(R)
