# -*- coding: utf-8 -*-
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emit import flush

ROWS = [
    ("81", "docs/architecture/ERROR_MODEL.md :: R5-57.md::C-57-02",
     "docs/architecture/ERROR_MODEL.md", "R5-57.md::C-57-02", "S1/S2/降级",
     "证据件的 `error_taxonomy_exit_codes` 守护的是 orchestrator 旧码表，而权威已改判 `cli/exit_codes.h` 为唯一源：仓库内现存两套退出码数值表",
     "machine_consistency_before.json:18; docs/architecture/ERROR_MODEL.md:26; lib/infrastructure/cli/exit_codes.h; lib/infrastructure/cli/exit_codes.h:1-19; lib/infrastructure/pipeline/orchestrator/cpp/inc…",
     "docs/architecture/ERROR_MODEL.md:24-45; lib/infrastructure/cli/exit_codes.h:1-21; lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:40/143-166; eng/tools/docs_machine_consistency.py:199-201/334-343; artifacts/evidence/v19r7-quality/machine_consistency_before.json",
     "仍在",
     "权威改判已落文（半收口，但恰是它使两套表并存变得可判）：`ERROR_MODEL.md:24-34` 现读「## 进程退出码（唯一源）> **退出码唯一源 = `lib/infrastructure/cli/exit_codes.h`**（11 码，与最高设计 §6.3 同源）：OK=0 ARGS=2 INPUT=3 SCIENCE=4 BACKEND=5 COMPUTE=6 IO=7 INTEGRITY=8 CANCELLED=9 RESOURCE=10 INTERNAL=70 …凡需要退出码数值处一律引用该头文件；本文档与任何下级文档的数值表只有这一套」+ `:45`「判据 = 与 `lib/infrastructure/cli/exit_codes.h` 的 `astrocs::ExitCode` 枚举逐名逐值一致」。两套数值表现存且互斥：①`lib/infrastructure/cli/exit_codes.h:5-19` `enum ExitCode { OK=0, ARGS=2, …, CANCELLED = 9, RESOURCE = 10, INTERNAL = 70 }`（`:2`「本文件是 11 个退出码在仓库内的唯一定义处;其他文件只 include, 不得重定义数值表」）；②`orchestrator/cpp/include/orchestrator.h:143-166` `namespace AstroCsExitCode { SUCCESS=0 … TIMEOUT = 9; CANCELLED = 10; … STAR_DETECT_FAILED=20 … MODULE_SPECIFIC_BASE=100 }`，且 `:40` 头注释仍自证与 ERROR_MODEL 同源（「错误码与 docs/architecture/ERROR_MODEL.md 全集合一致(AstroCsExitCode 0-10进程码+20-28 numeric_code+100预留, TIMEOUT=9/CANCELLED=10), 由 eng/tools/docs_machine_consistency.py error_taxonomy 全集合校验」）⇒ 唯一源声明所指对象与第二套表在同一仓库互相否定（同一整数 9/10 分别叫 CANCELLED/TIMEOUT 与 TIMEOUT/CANCELLED）。证据件守旧表：门读入面 `docs_machine_consistency.py:199-201` 只挂 `ERROR_MODEL.md`（must 含 `AstroCsExitCode`）与 `orchestrator.h`，判据 `:339-343` 比较 `extract_enum(ERROR_MODEL)` 与 `extract_enum(orchestrator.h, 「namespace AstroCsExitCode」)`，**不含** `exit_codes.h` ⇒ 与 `ERROR_MODEL.md:45` 自己规定的判据（对齐 `astrocs::ExitCode`）不同源；归档读数 `artifacts/evidence/v19r7-quality/machine_consistency_before.json` 同型（ROW 63/69 已现读其 :16-18 的 PASS 明细为 orchestrator 表）。`orchestrator.h` 之外无第二消费者：`git grep -ln 「DllLoader|dll_loader」` 结果同 ROW 80，仅文档/证据件命中。",
     "高",
     "是（S2，与 ROW 69/82 同批一条收口）：门改读 `ERROR_MODEL.md ↔ cli/exit_codes.h` 逐名逐值，orchestrator 的 `AstroCsExitCode` 改为 include 唯一源（或只留 20-28 numeric_code 那一段并注明非进程码），并同步 `orchestrator.h:40` 注释与归档明细"),

    ("82", "docs/architecture/EXECUTION_MODEL.md :: 质量-架构与设计.md::Q-ARCH-23",
     "docs/architecture/EXECUTION_MODEL.md", "质量-架构与设计.md::Q-ARCH-23", "S1/S3/降级",
     "`EXECUTION_MODEL.md` 给出与退出码唯一源**互斥**的第二套码值（`CANCELLED=10`/`TIMEOUT=9`）",
     "docs/architecture/EXECUTION_MODEL.md:50; docs/architecture/ERROR_MODEL.md:30; lib/infrastructure/cli/exit_codes.h:16-17; lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:40; docs/ar…",
     "docs/architecture/EXECUTION_MODEL.md:50/:52; docs/architecture/ERROR_MODEL.md:29-30; lib/infrastructure/cli/exit_codes.h:16-17",
     "仍在",
     "被点名的两行逐字未改（`git log -- docs/architecture/EXECUTION_MODEL.md` 最新 `c4af4136`(09-25 01:16，工单之后的正向化批次) 只改了否定式措辞，未触及码值行）：`:50`「| Orchestrator cancel | atomic flag **`CANCELLED=10`**, 流水线中断检查点 |」、`:52`「| Timeout | stage 配置 timeout_ms, 超时返 **`TIMEOUT=9`** |」。与唯一源互斥现读两侧：正本 `lib/infrastructure/cli/exit_codes.h:16-17`「`CANCELLED     = 9,   // 用户取消或超时`」「`RESOURCE      = 10,  // 磁盘写满 / 写盘失败…`」（11 码表里**根本没有** `TIMEOUT` 这一名），下级文档 `ERROR_MODEL.md:30` 抄录同一套「IO=7  INTEGRITY=8  **CANCELLED=9  RESOURCE=10**  INTERNAL=70」，且 `ERROR_MODEL.md:33-34` 明文「凡需要退出码数值处一律引用该头文件；本文档与任何下级文档的数值表只有这一套（最高设计 §6.3：码值与含义只有一份）」⇒ `EXECUTION_MODEL.md:50/:52` 的 `CANCELLED=10 / TIMEOUT=9` 与「只有一套」的自证直接冲突（10 在正本里是 RESOURCE，9 在正本里是 CANCELLED）。码值并非纯文字：`orchestrator/cpp/src/orchestrator.cpp:492` `result.exit_code = AstroCsExitCode::CANCELLED;`（=10）、`:498` `= AstroCsExitCode::TIMEOUT`（=9）⇒ 运行期真会返回与唯一源含义相反的进程码。",
     "高",
     "是（S3 文档面，须与 ROW 69/81 同一改动收口）：`:50/:52` 去掉码值、改述为引用唯一源具名成员（`ExitCode::CANCELLED` / 超时并入取消或 RESOURCE 语义由正本定），或在正本里给 timeout 定名；两套码值不得并存"),

    ("83", "docs/architecture/MODULE_MAP.md :: 质量-架构与设计.md::Q-ARCH-28",
     "docs/architecture/MODULE_MAP.md", "质量-架构与设计.md::Q-ARCH-28", "S1/S3/降级",
     "`MODULE_MAP.md` 文首宣布「本表不写状态字段」，四张表却每行都写状态字段",
     "docs/architecture/MODULE_MAP.md:7-9; eng/tools/quality/check_module_map.py; RELEASE_STATUS.md; check_module_map.py; REVIEW_FACETS.md",
     "docs/architecture/MODULE_MAP.md:5-11（宣布句 :8）与 :15/:71/:89/:100 四张表头 + 11 行 INSTALLED",
     "仍在",
     "宣布句未改：`:8` 现读「> 本表**不写状态字段**，状态一律由 `eng/tools/quality/check_module_map.py` **现场计算**」，`:9` 括注「（最高设计 §0.2/§12.5：登记表与映射表的**状态字段留空**，防「表内自证绿」）」——`c4af4136`(09-25) 对本文件唯一的改动正是把 :9 从「禁止写状态字段」软化为「状态字段留空」（`git show c4af4136 -- docs/architecture/MODULE_MAP.md` 的 ± 行只含这一句与 p3 projection 行的否定式措辞），宣布与实态的矛盾本身未动。四张表逐行写状态：表头 `:15`（§1 交付面「| 模块 | 路径 | **交付状态** | 产物 | 证据锚 |」）、`:71`（§2 会话与节点执行面，同列）、`:89`（§3 合同/迁移目标目录，同列）、`:100`（§4 非交付面「| 目录 | **状态** | 说明 |」）；§1 各行状态列一律填值（`git grep -c INSTALLED` ⇒ 11 行，如 `:17` conformance「INSTALLED」、`:24` CLI 平台单元「INSTALLED」），§3 的 p3 projection 行更在同一格写「IMPLEMENTED（registry）/ `entrypoint: MISSING` … **未 INSTALLED**」，§4 的 `lib/infrastructure/acr` 行（`:102`）写「DORMANT」⇒ 「不写状态字段」的自我声明与四张表的实际内容互斥，且状态词由表内自证（正是 `:9` 括注要防的「表内自证绿」形态）。",
     "高",
     "是（S3，写法面）：二选一并保持自洽 —— 四张表的状态列留空（状态由 `check_module_map.py` 现场计算并只出现在其输出/`RELEASE_STATUS.md`），或删去 `:8` 的「不写状态字段」宣布并把表内状态词接入 §12.5 唯一口径的门校验"),

    ("84", "docs/architecture/PERFORMANCE_MODEL.md :: 质量-架构与设计.md::Q-ARCH-35",
     "docs/architecture/PERFORMANCE_MODEL.md", "质量-架构与设计.md::Q-ARCH-35", "S1/S2/降级",
     "`PERFORMANCE_MODEL.md` 用本文件自己标注「数据不可用」的一行算出「内核效率衰减」，并把它列为不可改的物理上限",
     "docs/architecture/PERFORMANCE_MODEL.md:178; THREADING_MODEL.md:91-108",
     "docs/architecture/PERFORMANCE_MODEL.md:171（小标题）/:178（作废行）/:181-182（结论行）/:183-186（污染注）",
     "仍在",
     "本批整改未触及该文件（`git show c4af4136 -- docs/architecture/PERFORMANCE_MODEL.md` ⇒ **空 diff**；`git log` 最新 `83ade34c`(09-23，早于工单) ⇒ 无工单后订正）。被点名的推导链原样：`:171` 小标题「**第二条（更硬的）边界：内核并行效率随宽度衰减。**」；表格 `:178` 该行自带否定标注「| ~~`t2_16f_after`~~（**错通带曲线跑，数据不可用于本表**） | 2 | 4 | 8 | **166.0** |」；结论 `:181-182` 却仍以该行为分子分母「4 → 8 条工作线程：每帧 109.2 → **166.0** s，节点级净收益只有 **1.42×（1.46×/2，73% 效率）**，且每帧时间反而变长。⇒ 该内核在 ~4 条并发 stripe 之后进入收益递减区」——即由「数据不可用于本表」的那一格推出效率数并给出「边界」结论；`:183-186` 的注只是把它降级为「受污染的保守估计，需在静默机上复测」，并引 `PERF-MEM-FIX-01 §3` 的 K=2 观测为趋势佐证，未撤结论、未重算，「不可改的物理上限」式措辞（更硬的边界 / 收益递减区）仍在位。",
     "高",
     "是（S2）：作废单元格不得进入任何结论式（要么用可用档位复测后再写趋势，要么把该段改成正向的「当前不可判定，须静默机复测」并删除 73%/1.42× 与「更硬的边界」措辞）"),

    ("85", "docs/architecture/THREAD_BUDGET_ARCH.md :: 质量-架构与设计.md::Q-ARCH-30",
     "docs/architecture/THREAD_BUDGET_ARCH.md", "质量-架构与设计.md::Q-ARCH-30", "S1/S3/降级",
     "`THREAD_BUDGET_ARCH.md`：同一行既称「与线程数无关的确定性」又称「由 budget 快照唯一化」，并规定「全仓禁止硬编码线程数」而执行模型表里写着 16",
     "docs/architecture/THREAD_BUDGET_ARCH.md:34; docs/architecture/EXECUTION_MODEL.md:18; eng/tools/arch/check_thread_budget.py; eng/ci/checks.json; THREADING_MODEL.md; ASTROCS_DESIGN.md; docs/owner/SCIENC…",
     "docs/architecture/THREAD_BUDGET_ARCH.md:34/:41-42; docs/architecture/EXECUTION_MODEL.md:18; lib/algorithms/calibration/src/calibrator.cpp:30; eng/tools/arch/check_thread_budget.py:148/263-264; eng/tools/quality/contracts/check_doc_symbols.py:290",
     "仍在（三点全存，其中「登记表」为按名不存、实存另一套）",
     "①同行两说未改：`THREAD_BUDGET_ARCH.md:34` 现读「- 浮点归约顺序冻结(THREADING_MODEL.md §确定性锚点全部有效: upm.cpp:495/sampler 串行/drizzle_engine.cpp:1662,1751,1834,1843);tile 合并=thread-local 累加后 **t=1..num_threads 固定序串行合并**(与线程数无关的确定性: 结果序列由 budget 快照唯一化)」——同一条括注既断言「与线程数无关」又把唯一化归给「budget 快照」（快照决定并发度 ⇒ 归约序列仍随 num_threads 走），两说并存。文件历史 `git log` 最新 `c4af4136`(09-25) 只做了否定式正向化（`git grep -n 硬编码 -- THREAD_BUDGET_ARCH.md` 现只剩 `:48` 的「BENCH-003(候选不含硬编码 core count)」）。②禁令与实态相反：同文件 `:41-42` 规定静态 checker 合同「`std::thread`/`std::async`/`_beginthread`/`CreateThread` 出现处必须在 `THREAD_BUDGET_EXEMPT` 登记表…**`omp_set_num_threads(`/`num_threads(` 字面量=0 容忍**」，而执行模型表 `EXECUTION_MODEL.md:18` 仍写「| Stage1 calibrate | calibrator thread | per-tile OpenMP | **16** | OpenMP parallel for | tile barrier | `calibrator.cpp: OpenMP 16` |」，其锚点侧 `lib/algorithms/calibration/src/calibrator.cpp:30` 现读「// - 多线程固定 16 线程（开发环境 16 核）」⇒ 文档与注释双双写死 16，与「由 benchmark profile 决定、不硬编码线程数」的口径（AGENTS §6）相斥；注：`calibrator.cpp` 内实际并行指令是 `:94/:126/:134/:170 #pragma omp parallel for schedule(static)`（无 `num_threads(16)`），故该 16 为纯声明、既非门可读的常量也无从验证。③登记表按名不存在：`git grep -n THREAD_BUDGET_EXEMPT` ⇒ 2 命中，仅 `THREAD_BUDGET_ARCH.md:41`（本文件自述）与 `eng/tools/quality/contracts/check_doc_symbols.py:290`（把该串列进「允许出现在文档里的符号」白名单）⇒ 代码侧无同名对象；执法脚本的真实结构是 `eng/tools/arch/check_thread_budget.py:148 EXEMPT = {…}`（按路径登记，条目自带理由，如 `:219`「watchdog 超时守护线程 x2 (:5167 声明 + :5174 启动)…原行级 watchdog 豁免已删除, 改为本路径级登记」）+ `:263-264` 两条正则 `omp_set_num_threads\\s*\\(` 与 `num_threads\\s*\\(\\s*\\d+\\s*\\)` ⇒ 文档点名的登记表名与在册实现不同名，照文档去搜/去登记会落空。",
     "高",
     "是（S3）：`:34` 拆成两句并给出确定性到底随何者（若随 num_threads 则不得称「与线程数无关」）；`:41` 的登记表名改为脚本实名的路径级 `EXEMPT`；`EXECUTION_MODEL.md:18` 的 16 与 `calibrator.cpp:30` 注释改引 budget/profile 口径"),
]

flush(ROWS)
print("batch5 ok:", [r[0] for r in ROWS])
