# M5a 合并验证报告（域：运行时 / 统一线程预算 / CPU 后端 / ACR 休眠合规）

- 代理：M5a（第二层合并验证）；输入：`_cache/L11.md`（21 条：P0:6 / P1:13 / P2:2）+ 基准 `_cache/F00_FRONT_SPOTCHECKS.md` F00-03 + 协调令（L11-002 与 L17-004 必须并成一条）
- 纪律执行情况：全程只用 read / grep / glob 取证；仅写 `问题扫描/findings/**` 与本文件；未修改任何既有仓库文件；未执行构建/测试/脚本/git。
- 唯一一次写外操作：按前台协调令把「一条 finding 一个文件」的散件并入「类别×优先级」合并件（`findings/<类别>/p<N>/M5a_L11.md`，11 个），并用 `rm -f` 显式列出**我本人刚创建的 21 个散件**逐个删除（不含通配、不含 README.md、不触碰他人产出）。
- 复核时点：定稿前对全部 4 条 P0 的证据锚按当下树重新 grep（`0.80 * static_cast<double>(m)`、`kCpuMeanMinPercent = 85.0`、`CHECK(t1 == 0.80 * 2.0)`、`(s.d_cpu_seconds / interval_) * 100.0`、`SCAN_ROOTS = [os.path.join(REPO, "lib")]`、`ci/checks.json` 内 `gate-required` 零命中等 12 处）全部复现 → 四条 P0 均为「复核时点成立」。

## 1. 逐条处置表（四态）

| 来源 | L11 级别/标题要点 | 四态 | M5a 定稿 | 处置理由（要点） |
|---|---|---|---|---|
| L11-001 | P0 worker 均衡/单活跃线程用计划值 | **部分修复** | M5a-G-004（P1，残余事实） | `set_workers(budget,budget)` 已被并发提交 B2-A18 换成真实租约观测（含回归测试）；残余：active=runnable 同值、queue/progress 零注入、peak 恒等使 p50 失义、未观测回退仍用配置值 |
| L11-002 | P0 §10.5 冻结阈值未实现 | **仍成立** | M5a-G-001（P0，含 L17-004 并档） | 80/75/50 三值恰为 §18.2 点名废止的旧 D.6 值；85/60 在 `cli/**` 无实现；两个门常量零引用；单测固化 0.80 |
| L11-003 | P0 利用率归一口径 | **仍成立** | M5a-G-002（P0） | 前台要求的唯一结论已给出：**分母 = 1 核**；四处注释与自身公式互斥；`tests/backend/test_p2007_joint_gate.py:33-34` 的 16 核≈107% 实测注记直接证明该门在 6.7% 容量下仍达标 |
| L11-004 | P0 合规门零调用点 | **仍成立（措辞订正）** | M5a-G-003（P0） | 叶子「只在合成字典上跑过」不准确：9 处注册项经 `ci/resource_monitor.py` 真实调用 run_monitored，但零判定旗标；新增事实 `ci/run.py:75-76` 指向的 REAL-001 在 checks.json 中不存在 |
| L11-005 | P0 THREAD-BUDGET 空转 | **仍成立（引文/数字订正）** | M5a-G-005（P0） | 静态重放复算：`std::vector<std::thread>` 生产源 12 处不可见（含 Runtime 自身 executor.cpp:39/:217）；`#pragma omp parallel` 99 处、带数字 num_threads 者 0；检查器自身正则仅命中 5 行且全被文件名/`watchdog` 行豁免；并新增「实现低于 ARCH-THREAD-001 §5-1 条款文字」 |
| L11-006 | P1 第二线程预算源 | **仍成立（子事实剔除）** | M5a-G-006（P1） | `hardware_concurrency` 作默认预算与 ARCH-THREAD-001 §1（affinity∩cgroup∩Job）直接冲突；剔除 parser.cpp 误归属 |
| L11-007 | P1 Windows 不启用 OpenMP | **仍成立** | M5a-G-007（P1） | `if(UNIX) find_package(OpenMP QUIET)`；三 target `-fopenmp` 仅 UNIX；同图 dpsf/sdet 无条件链接该 imported target（其存在依赖 tests 子图）|
| L11-008 | P1 --cpu-profile 不消费 | **仍成立** | M5a-C-001（P1） | 唯一消费点在 `cmd_show_effective`；三个 run 命令均不读；新增 15 处 `set_provider("baseline")` 字面量证据 |
| L11-009 | P1 ACR 四道门可绕 | **仍成立（措辞修正）** | M5a-G-008（P1） | 新证：CI 构建落点为 `build/linux-control`，`build/root-cmake/astrocs` 不存在 → 二进制判据在 CI 恒跳过；`register_phase2_acr_kernels` 定义恰在被排除文件；「魔法字符串旁路」当下未触发，改述为潜在旁路 |
| L11-010 | P1 ISA-LEAK 只跑 selftest | **部分成立** | M5a-G-009（P1） | CI 仅 `--selftest`、docstring 的 nm/link map 无实现、provider 侧缺参静默跳过 —— 成立；「缺 `--binary` 静默跳过」不成立（:158-160 明确 FAIL），「EVEX 系统性盲区」改为覆盖面不全 + 未登记偏差 |
| L11-011 | P0 provider float32 + 1e-6f | **仍成立（降级）** | M5a-H-001（P1） | 代码与头文件合同事实全部逐字复现；但 `ACS_CPU_KIDX_DRIZZLE_ACCUMULATE/INTEGRATION_ACCUM` 在 `lib/ cli/ runtime/ modules/` 零调用点 → 无数值后果证据，按规程 §2 不得留 P0；与 L04-001 互为两半 |
| L11-012 | P1 ACR 容差推导 | **仍成立（加强）** | M5a-A-001（P1） | 自身算术即自毁：1e-7×32=3.2e-6 > 宣称包络 1e-6，再×714 差三个数量级；量纲混用（独立像素数不放大 max_abs） |
| L11-013 | P1 两份同名 ACR 文档分叉 | **仍成立** | M5a-I-001（P1） | ALG 内 F2「逐位等价」与 F4「1e-6 容差」同块互斥；SCI FROZEN/ALG DERIVED 无路径级互引 |
| L11-014 | P1 TST-ACR 不存在 | **仍成立（加强）** | M5a-F-001（P1） | 新增：八层权威矩阵内 `SCI-ACR|ACR-EQUIV` 零命中（旧表还给它标 VERIFIED）；最接近的两个测试因 `skipUnless(build/linux-openmp-on/libphase2.a)` 与 CI 构建树不符而静默跳过；`test_iso_acr_gpu_isolation.py::test_04` 体为 continue/pass |
| L11-015 | P1 EXECUTION_MODEL 与代码相反 | **仍成立** | M5a-C-002（P1） | OpenMP 16 证据指针不可定位；sampler 现由 std::thread 池并行而 ARCH 仍写「OpenMP or serial/串行」；异步行与 ASYNC_IO_CONTRACT「待接入点」互斥；新增 pc_api 三处「OpenMP 16 线程」注释 |
| L11-016 | P2 锚点批量漂移 | **仍成立（升级）** | M5a-E-001（P1） | 规程 §2 明列「锚点漂移或悬空引用」为 P1 典型；且 `execution_options_contract.md:40` 指向的路径在真源不存在（glob 复算）；新增「锚点门 DOC-LINE-ANCHORS 的 doc_globs 只含 science/algorithms，不覆盖 architecture/owner」 |
| L11-017 | P1 公共头 cpu_workers 语义相反 | **仍成立（补登记状态）** | M5a-D-001（P1） | 实现一律 0/负→1；upm.h:89-91 已登记 DISP-P2UPM-002 未整改，而 sampler.h:54-56 与 upm.h:176 不在任何偏差 ID 覆盖内 |
| L11-018 | P1 provider 无 executor 静默串行 | **仍成立（补可达性界定）** | M5a-G-010（P1） | 兜底写进公共头 :42 成为对外合同，与同仓 `conservative_route`「≥2 不退 1」方向相反；可达性同 L11-011 界定 |
| L11-019 | P1 PSS/iowait/每线程 CPU 未采 | **仍成立（加强）** | M5a-C-003（P1） | `cli/` 内无 smaps_rollup 读取（memory_report.h 读的是 Private）；六个 GateConfig 字段无喂值；新增：宪章要的是 `resource-timeseries.csv` 与 `resource-summary.json`（连字符），实现是 `resource_samples.csv`/`resource_summary.json`，两处命名差异均无偏差登记，且 `resource-timeseries` 全仓无产出点 |
| L11-020 | P1 ISA 变体等价口径与独立性 | **拆分：部分成立** | M5a-C-004（P1）+ M5a-I-002（P2） | 保留「文档称与 Python 参考比对，实为两生产 provider 互比、无误差合成」与「ISA-004 平台口径自相矛盾」；剔除 ①2e-4 无来源 ③逐位/容差不区分 ④AVX512 缺对照 |
| L11-021 | P2 注释堆积任务号 | **仍成立** | M5a-D-002（P2） | §12.2 违例面复现（含 THREADING_MODEL.md:21 的提交哈希与「79 PASS identical」）；`module_adapters.cpp:364` 仍描述已被 B2-A18 取代的 `set_workers(budget,budget)` |

### 基准并档（前台指令执行结果）
- F00-03（线程门对裸 omp 不可见 + 测光自证未接线 + `pc_api.cpp:331` 注释错）：门侧全部并入 **M5a-G-005**；`pc_api.cpp` 注释事实并入 **M5a-C-002 ⑤**（该处为定稿点，M3a/M6 请 related 指过来）。
- L11-002 × L17-004：已并为一条 = **M5a-G-001**，由 M5a 主落，**M5b 不得重复登记 L17-004**。
- L11-003 的前台要求（给出归一分母的唯一结论）：结论 = **1 核**，见 M5a-G-002。
- L11-004 的零调用点：成立并加强，见 M5a-G-003。
- L11-005 × L11-006 × L13（三源同证）：门侧在本域一处定稿（M5a-G-005）。
- L11-011 × L04-001：各落本域一半并互挂 related，不合并。

## 2. 已被修复（并发提交期间）——不进 findings/

| 原判据（L11-001 内） | 当下树事实 | 回归保护 |
|---|---|---|
| `cli/commands.cpp` 采样线程写 `recorder.set_workers(budget, budget)`（把配置 budget 当实测 worker 数）；叶子引用 `:789-791` 与注释「active 阶段注入实际 worker 租约数」 | 该调用已被并发提交 **B2-A18** 替换为：先读 `astrocs::core::granted_worker_observation().peak_active`（`include/astrocs/core/context.h:93-107` + `lib/core/src/context.cpp:121-149` 的真实 acquire/release 原子累计），仅在哨兵 0（未观测）时回退 `min(budget, cli_affinity_cpu_count())`；`GateConfig.granted_workers` 同源于 :863-864。合同面亦已订正（`docs/contracts/DATA_SEMANTICS.md:603`） | **有**：`tests/unit/mon001_gate_test.cpp` 用例 18a/18b（:237-279）断言 U 分母取真实观测、观测不被 available 封顶、真实 `ThreadBudget::acquire` 后 `peak_active ≥ 4` 且 `acquired_total` 递增；注册为 ctest `mon001_gate`（`tests/unit/CMakeLists.txt:314-319`）并被 CI 的 `ctest-full` 面覆盖（`ci/checks.json:2452` 附近）；覆盖面另含 `tests/unit/rt001_unique_executor_test.cpp`（ctest `rt001_unique_executor`，`tests/unit/CMakeLists.txt:1241-1250`）、三个模块集成测试对 trace `granted_workers` 字段的解析（`calibration_integration_test.cpp:945`、`cosmetic_integration_test.cpp:1049`、`gaia_integration_test.c:812`）、`tests/unit/cpu008_worker_advisor_test.cpp:324-340` 与 Python 合同面 `tests/monitoring/test_monitor_contract.py:281-361` → 属「改过且有人守」，**不另立 F_TEST_GAP** |
| L11-001 的派生判据「`workers_p50` 恒等于预算值」 | 采样值不再等于配置 budget（当有租约时） | 同上；残余形态（峰值恒等、未观测回退）已在 M5a-G-004 定稿，不重复登记 |
| L17-004 侧「`GateConfig` 从未被生产赋值」 | `cli/commands.cpp:858-943` 大量赋值（前台已注记，checks.json 已删该项自检谎称） | 由 M5b 按其自检条目口径处理，本域不登记 |

> 说明：本域其余 20 条在复核时点均可在当前树复现，无「锚已消失」型失效。

## 3. 关键复算与原文对照（可复算要点）

1. **§18.2 与被废止值的对照（宪章原文）**：§18.2 第 2 项末句「本裁决替代旧 `AstroCS_ENGINEERING_CONSTRAINTS.md` D.6 的 **80%/75%/50%** 表述」（:656）；代码三处：`compute_cores_threshold` = `0.80*min(...)`（resource_gate.h:185）、`kMon001UtilSampleMinPercent=75.0`（:104）、`kMon001QueueUtilMinPercent=50.0`（:107）。三个数字与废止值逐一对应 → 判「沿用被废止旧值」而非「数值巧合」。
2. **利用率归一分母（前台要求给唯一结论）**：`record()` 式 `cpu_pct=(d_cpu_seconds/interval)*100` 中 `d_cpu_seconds` 为**进程** user+sys CPU 秒差（N 核满载 → N×100），`monitor.h:318` 的 `avg_cpu_percent = avg_equivalent_cores * 100.0` 自证同一标度 → 采集口径 = **100% 为 1 核**。故 `evaluate_gate:246-249` 的 90/85 是 0.90/0.85 核下限。仓库自身实测注记（`tests/backend/test_p2007_joint_gate.py:33-34`）：16 核全核语境 `cpu_p50≈114% ≥ 90、cpu_mean≈107% ≥ 85` → 按容量算约 **6.7%（1.07/16）** 仍判达标，是该结论的直接实证。`utilization_value()` 则把 cpu_pct 除以 `100·m`（容量标度），同一文件内两套标度并存。
3. **THREAD-BUDGET 重放计数**（只读复刻 `scan()` 的文件/目录/注释行过滤规则，未执行脚本）：`lib/` 生产源中 `std::vector<std::thread>` = **12**（11 代码行 + 1 注释行）、检查器自身 `std::thread` 正则 = **5**（4 代码行 + 1 注释行），4 代码行分别命中文件名豁免（`resource_monitor.h`、`orchestrator.h`）与行级 `"watchdog" in line` 豁免（`orchestrator.cpp:5167/:5174`）→ 「未登记线程创建=0」为真但由豁免与漏匹配共同造成。`#pragma omp parallel` = **99**（叶子报 85，差异来自过滤口径：本条只计 .c/.cpp/.h/.hpp 且排除 `tests/`、`archive/`、`third_party/`）；其中带数字 `num_threads(\d)` 子句 = **0**。
4. **§10.5 门的两个「存在性」**：合规实现存在（`run_monitored.py:451-456` 的 0.85/0.60/2）但无应用点；不合规实现存在应用点（`commands.cpp:887-943`）但阈值与口径均非宪章值。二者不重叠 → 本域前两条 P0 的分工依据。
5. **CI 监控面清单（复算）**：`requires_monitor: true` 共 **6** 项（DEEP-CLANG-BUILD:1966 / DEEP-SAN-ASAN:2010 / DEEP-SAN-TSAN:2054 / DEEP-COV-CPP:2101 / DEEP-COV-PY:2142 等），`ci/resource_monitor.py` 作为命令出现的注册项 **9** 处（1893/1936/1980/2024/2068/2116/2183/2229/2278），全部无 `--gate-required/--gate-workers`；`ci/checks.json` 内不存在 `REAL-001` 注册项。
6. **ACR 休眠四道的当下状态**：CMake 默认 OFF 与「ACR 源不编入生产 target」为真（这是 §10.1 的正向事实，已在 M5a-G-008 内如实承认）；CI 构建落点为 `build/linux-control`（`ci/steps/linux_build_root_graph.sh:13-14`），而 `tools/check_legacy_exit.py:19/:67` 找的是 `build/root-cmake/astrocs` → 二进制判据永不执行。
7. **provider 可达性界定（用于降级 L11-011/018）**：`ACS_CPU_KIDX_DRIZZLE_ACCUMULATE`/`ACS_CPU_KIDX_INTEGRATION_ACCUM`/`acs_cpu_baseline_run` 在 `lib/`、`cli/`、`runtime/`、`modules/` 内 **0 调用点**，仅 `tests/cpu/baseline|avx2|avx512` 以 dlopen 驱动；`cmake/install_layout.cmake:63-64` 自述该 DSO 为「Linux 技术预览 DSO（legacy backend ABI；正式 provider ABI … 由 CPU-002 交付）」。
8. **锚点门的真实作用域**：`docs/algorithms/anchors/check_doc_line_anchors.py:5` + `anchor_contract.json:5-8` 的 `doc_globs` 只有 `docs/science/*.md`、`docs/algorithms/*.md`；而本域漂移锚全在 `docs/architecture/**`、`docs/owner/**` → 判为「机器门未覆盖该文档层」，非「检查器失效」（回答叶子 §6 待复核第 2 项）。
9. **宪章 §11 工件名原文**：清单为 `resource-timeseries.csv` / `resource-summary.json`（:396-397，连字符）；实现与 `docs/api/CLI_PROTOCOL_V1.md:61` 为 `resource_samples.csv` / `resource_summary.json`（下划线）；全仓 `resource-timeseries` 检索仅命中宪章与 `工程控制/`，**无产出点**。
10. **ACR 容差推导的算术复算**：`1e-7 × 32 = 3.2e-6`（已 > 宣称包络 `1e-6` 的 3.2 倍）；再 `× 714 = 2.28e-3`（比包络大 3 个数量级）→ 「推导」与其自身结论矛盾，不依赖外部标准即可定档。

## 4. 剔除与降级显式清单（禁止静默丢弃）

| 来源 | 原级别 | 处置 | 理由 |
|---|---|---|---|
| L11-001 | P0 | **降级 P1 + 改标题（部分修复）** | 主判据（配置 budget 冒充观测）已被 B2-A18 修掉且有回归测试；只保留三处残余事实（同值、恒 0 列、峰值恒等/回退），标题按残余写实 |
| L11-011 | P0 | **降级 P1（H_NUMERIC）** | 代码/合同不合规为真，但本轮静态复算无法证明该 provider 位于生产重计算路径（0 调用点 + install 侧自述技术预览）；规程 §2 要求 P0 有「改变输出数值/产品不合规」后果，可达性未证前不留 P0；可达性证据需求已写入影响段与移交 |
| L11-016 | P2 | **升级 P1（E_TRACE_BREAK）** | 规程 §2 把「锚点漂移或悬空引用」列为 P1 典型；且 FROZEN 文档（THREAD_BUDGET_ARCH.md:32）主动断言这批锚「全部有效」，另有活动文档指向不存在路径 |
| L11-020 | P1 | **拆 1 条 P1 + 1 条 P2，剔除其中 3 项** | 见下行明细 |
| L11-020 ① | （子项） | **剔除** | 「2e-4 无任何来源」不成立：`docs/architecture/cpu/CPU_003_AVX2_PROVIDER.md:112` 给出判据式 `\|a−b\|/max(1,\|b\|) ≤ 2e-4` 并注「与 CPU-002 同规」、:117-118 说明冻结时序；层级偏低 ≠ 无来源 |
| L11-020 ③ | （子项） | **剔除** | 「同文档既承诺逐位又允许 2e-4，未区分两判据」不成立：:83 讲的是 budget 1 vs 4 的跨 worker **确定性**、:73 讲的是跨 ISA **等价性**，文档已分列 §4 与 §2，非矛盾 |
| L11-020 ④ | （子项） | **剔除（叶子读断）** | 「AVX-512 侧无 baseline 对照容差项」错误：`tests/cpu/avx512/run_provider_avx512_checks.py:347-350` 的通过串完整为「baseline/avx2/avx512 oracle rel≤2e-4; determinism budget1vs4 bitwise; …」，:348 即 baseline 对照项 |
| L11-010 子项 | — | **措辞修正** | 「真实扫描缺输入即静默跳过」只对 provider 侧（:101/:110）成立；`--binary` 缺省时 `:158-160` 明确 `ISA_LEAK_FAIL` 并 return 1 → 叶子把二者混为一条，本域只登记 provider 侧 fail-open |
| L11-010 子项 | — | **措辞修正** | 「显式跳过 EVEX 0x62 → AVX-512 系统性盲区」夸大：`scan_avx512` 有 18 个 EVEX 专属助记符 + `%zmm` 兜底，真实缺口是「EVEX 编码而操作数为 xmm/ymm 的 AVX512VL/掩码类指令」不命中，且 :71-72 已给出规避理由（该理由未登记为偏差） |
| L11-009 子项 | — | **措辞修正（潜在≠既成）** | 「只要任一文件含 `DORMANT_NOT_IN_PRODUCTION` 就整体放弃判定」是设计缺陷，但复算全仓该字面量**只出现在检查器自身**（:12/:83/:96），cli/lib 源码内 0 命中 → 当下未触发，登记为潜在旁路 |
| L11-006 子项 | — | **剔除** | 「`cli/parser.cpp:273` 亦直接取 `hardware_concurrency()`（第二预算源）」误归属：该函数现为 `local_cpu_signature()`（:275-280），注释自述「profile stale 判定；非调度线程数」，属机器指纹而非预算源 |
| L11-002 / L17-004 的 fail-open 表述 | — | **不采纳** | `resource_gate.h:233-234` 的「active window 未提供 → 视为代表性」（`gate_window_representative` 返回 true，不 return Ok）与「wall<5s → Ok」不构成生产 fail-open；且 §10.5 自身以「计算区间超过 10 秒」为前提，<10 s 跳过与宪章相容 |
| L11-001 引文 | — | **引文更正** | 叶子引用的 `commands.cpp:789-791` 与其注释「active 阶段注入实际 worker 租约数(cli_affinity 分配核; 禁硬编码)」在当下树中已不存在（被 B2-A18 改写），本域改引 :752-762 现文；同时保留 F00-03 的注释错事实但改指 `pc_api.cpp:331/695/969`（三处，非一处） |
| L11-005 引文 | — | **引文更正** | 叶子写 `SCAN_ROOTS = [REPO / "lib"]` 与 `r"(?<!\w)std::thread\s*\("`，实际为 `[os.path.join(REPO, "lib")]` 与 `r"std::thread\s*\(|std::thread\s+\w+"`（无 lookbehind、含第二分支）→ 定稿件按原文重抄；核心漏匹配结论不受影响（`>` 阻断 `\s+\w+`） |
| L11-004 措辞 | — | **收紧** | 「只在 `tests/monitoring/test_frozen_gate.py` 的合成字典上被跑过」不准确：CI 有 9 处真实调用 run_monitored（无判定旗标）。定稿改为「`evaluate_frozen_gate()` 不在任何注册面上被请求」 |

## 5. 移交清单（跨域）

| 目标 | 事项 | 依据/证据 |
|---|---|---|
| **M5b** | `cmake/install_layout.cmake:63-64` 自述 `astrocs_cpu_baseline` 为「Linux 技术预览 DSO（legacy backend ABI）」，而 `packaging/astrocs.product.json:17` 与 `packaging/install-tree.contract.json:17` 把 `PROV-CPU-BASELINE` 列为随产品交付的 provider 单元；宪章 §16.2 要求 manifest 与已验收状态一致 | 交付状态口径矛盾属构建/打包面（L12-012/L12-013 域）；本域只提供 provider 侧证据（M5a-G-010/H-001 的可达性界定） |
| **M5b** | `NO-SERIAL-HEAVY`（ci/checks.json:113-122）与三个 `*-SELFTEST`（:623/:646/:669）的注册项重叠与「只跑 selftest」问题；`check_serial_heavy.py` 覆盖面 | 本域 M5a-G-008/G-009 只登记「ACR/ISA 两面」的直接证据，注册表整体由 M5b 落 |
| **M2a** | 真实 FP64 承诺 ↔ 实际累加域：`docs/science/ACR_EQUIVALENCE.md:84` 自述「FP32 信号域与 **FP64 累积 `vs/wsum`**」，与本域 H-001（provider f32 累加）、L04-001（HP_DRIZZLE API 无累加精度参数 / `module_adapters` 恒投 FLOAT32）构成同一主题三处证据；请由前台并主题 | §5.3/§7.3、STD-NUMERIC L15 |
| **M3a** | `lib/photometric_calib/cpp/src/pc_api.cpp:331/695/969` 三处「OpenMP 16 线程」注释（同文件 :62/:192/:441 打印 `omp_get_max_threads()`）→ 定稿点在 **M5a-C-002 ⑤**，M3a 侧勿重复登记 | §12.2 / STD-CONC:19 |
| **M4** | `cli/monitor.h:17`「Windows 侧为 MON-004 待办（PDH/ETW 未接入，当前 Linux /proc 实现）」与 L09-016 为同一事实，本域不重复登记，仅作为 M5a-G-007 的平台侧背景 | §3.1/§15.3 |
| **M6** | ①`docs/TRACEABILITY.csv:68` 对 SCI-ACR-EQUIV-001 标 `VERIFIED` 且 TEST 列填 `TST-ACR-001`（无实体），而八层矩阵无该 SCI 行 → 归 F00-01「矩阵双头」根因，本域 M5a-F-001 与之 related 互挂；②`docs/standards/CONCURRENCY_STANDARD.md:24` 与 `docs/architecture/THREADING_MODEL.md:26` 引用的 `ENG-THREAD-001..003` 只在旧矩阵定义（非八层合同成员）→ 属「引用非权威矩阵」实例，并入 F00-01 处置 | §12.3-1/-9 |
| **M6a（监控合同 / Python 面）** | 观测修复在 C++ 与 Python 两侧对同名字段给出不同语义：C++ 侧 `include/astrocs/core/context.h:99-103` 的 `peak_active` = **并发授予租约 token 之和**（higher-water，只增不减），而 `runtime/monitoring/monitor.py:57` 与 `runtime/monitoring/trace_feed.py:22` 把 `granted_workers` 定义为「授予租约上限 / 单次 max grant」；资源门用它作 U 分母（`cli/commands.cpp:863-864` → `resource_gate.h::utilization_value`）。字段语义归一属监控合同域，本域只登记门侧用法（M5a-G-004），合同侧请 M6a 定稿 |
| **负责人裁决请求** | ①§10.5 的 85%/60% 是否要求「有效 CPU<2 或区间≤10 s」时仍有兜底判据（现两套实现均直接放行）；②`lib/acr/scheduler/dispatcher.cpp:476` 的 `std::min<std::size_t>(16, hardware_concurrency())` 是否按 §10.4 无硬编码口径处理（休眠域是否受约束，存在解释空间，叶子亦列待复核）；③`run_monitored.py`（Python 合规门）与 `cli/resource_gate.h`（C++ 产品门）是否应合一 | 宪章 §10.4/§10.5/§18.2；规程 §2「权威解释仅 §1.2」 |

## 6. 行号漂移处置

- 规则：不以「行号不匹配」为剔除理由；一律按符号名或语句关键片段 grep 重定位，定稿位置写 `path::符号`，行号仅作复核时点参考。
- 本域实测漂移（叶子引用 → 当下树）：
  - `cli/resource_gate.h`：`kCpuP50MinPercent` 等常量 :92-94 → **:93-95**；MON-001 常量 :102-105 → **:104-107**；「阈值(85%/60%…)不动」注释 :148 → **:153**；`compute_cores_threshold` :176-181 → **:181-186**；CPU p50/mean 判定 :241-244 → **:246-249**；wall<5s 分支 :228-229 → **:233-234**（该文件在并发中新增了 `B2-A18`/`granted_workers` 字段，整体后移 3-5 行）。
  - `cli/commands.cpp`：`set_workers` :789-791 → **:761**（且语义已变）；`GateConfig` 赋值段 :855-865 → **:858-868**；`iowait_percent` 赋值「不存在」复核为真（全仓仅 :141 定义与 :205/:239/:377/:403 读取）。
  - `tests/unit/p2_workers_test.cpp:85`、`mon002_gate_test.cpp:23-31`、`resource_recorder.h:101/:243` 未漂移。
  - `lib/core/src/module_adapters.cpp`（并发高危文件，5541 行）：叶子/F00 引用的 :680-682、:3777-3793、:5016 等均已不可靠；本域定稿只使用符号锚（`p1_photometry_descriptor`、`photometry → Photometer::measure` 段、行带 work unit 注记）。
  - `lib/photometric_calib/cpp/src/pc_api.cpp`：F00-03 列 14 处裸 omp → 当下 **19 处**（新增 :867/:925/:958/:973/:1067）；「16 线程」注释由 1 处变 3 处。
  - `docs/TRACEABILITY.csv` 行号未漂移；`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp` 锚点相对文档所列 +11/+1 偏移（1673-1674 vs 1662、1862 vs 1834/1843）。
- 处置结果：0 条因行号漂移被剔除；1 条（M5a-E-001）的对象本身就是锚点漂移。

## 7. 定稿统计

| 优先级 | 条数 | 编号 |
|---|---|---|
| P0 | 4 | M5a-G-001、M5a-G-002、M5a-G-003、M5a-G-005 |
| P1 | 16 | M5a-G-004、G-006、G-007、G-008、G-009、G-010；M5a-C-001、C-002、C-003、C-004；M5a-D-001；M5a-E-001；M5a-F-001；M5a-H-001；M5a-I-001；M5a-A-001 |
| P2 | 2 | M5a-D-002、M5a-I-002 |
| 合计 | **22** | 来源 21 条 L11（1 条拆 2、1 条并 L17-004、1 条转「已修复+残余」） |

**输入 21 条的四态小结**：仍成立 17（含 L11-016 升级 P2→P1、L11-011 降级 P0→P1、L11-010 部分成立并两处措辞修正）｜部分修复 1（L11-001，修复面有回归保护）｜整条已被修复 0｜拆分 1（L11-020 → C-004 + I-002）｜无法判定（转 §8，需执行权限）5 项子问题。合计定稿 22 条。

- 落盘文件（每「类别×优先级」一个，按前台协调令把散件并入合并件，编号不变、内容不删）：`findings/G_GOV_GATE/p0/M5a_L11_L17.md`（含 L17-004 并档条目）、`G_GOV_GATE/p1/M5a_L11.md`、`C_DOC_CODE_GAP/p1/M5a_L11.md`、`D_COMMENT/p1/M5a_L11.md`、`D_COMMENT/p2/M5a_L11.md`、`E_TRACE_BREAK/p1/M5a_L11.md`、`F_TEST_GAP/p1/M5a_L11.md`、`H_NUMERIC/p1/M5a_L11.md`、`I_DOC_HYGIENE/p1/M5a_L11.md`、`I_DOC_HYGIENE/p2/M5a_L11.md`、`A_SCI_DEF/p1/M5a_L11.md`。

## 8. 无法判定（转 §6 待复核，需执行权限）

1. `check_thread_budget.py` 在真实树上的实际输出（本域以只读复刻其过滤规则得出 5/12/99 三个数，未执行脚本）→ 需在有执行权限的工作区跑一次并核对 `SCAN_ROOTS` 文件数。
2. baseline provider 是否被任何真实 run 经 dlopen 调用（影响 M5a-H-001 / M5a-G-010 是否升 P0）→ 需 `astrocs doctor --json`、`selftest --provider baseline` 与 run trace 的 `provider` 字段。
3. 真实 heavy run 产物是否确缺 `resource-timeseries.csv`（M5a-C-003 的 §11 条款核对）→ `run/**`/`evidence/**` 属免报区，不作为 finding 证据，需当前 SHA 实测。
4. `ISA-004/005 属 Windows 域` 表述是否已由后续任务订正（M5a-I-002 的置信度上限）→ 需 ISA 域任务台账。
5. `2e-4` 是否在控制包内另有 SCI/ALG 级出处（本域已在 CPU_003 找到 ARCH 级判据式，故不再依赖该项）→ 免报路径不作证据。

## 9. 产出粒度与清理记录（响应前台协调令）

- 本域定稿原先以「一条一文件」写入，现全部并入 11 个「类别×优先级」合并件（清单见 §7），条目编号与正文未删改；仅对 `G_GOV_GATE/p0` 因含 L17-004 并档条目改名 `M5a_L11_L17.md`。
- 删除的散件：21 个本代理本人创建的单条文件（含 1 个落错层级者，见下条）（`M5a-G-001/002/003/005.md`、`M5a-G-004/006/007/008/009/010.md`、`M5a-C-001..004.md`、`M5a_D-001/002.md`、`M5a_E-001.md`、`M5a_F-001.md`、`M5a_H-001.md`、`M5a_I-001/002.md`、`M5a_A-001.md`）。未触碰任何 README.md 与他人产出。
- 路径纠正（前台第二次指令）：M5a-E-001 的单条件实际落在了**错误层级** `问题扫描/E_TRACE_BREAK/p1/M5a_E-001.md`（不在 `findings/` 下）；已确认其内容已完整并入 `问题扫描/findings/E_TRACE_BREAK/p1/M5a_L11.md`（含后补的「锚点门作用域」一节，合并件为该条唯一版本且信息更多），随后用显式路径 `rm -f` 删除该散件并 `rmdir` 清掉我创建的两个空目录（`问题扫描/E_TRACE_BREAK/p1`、`问题扫描/E_TRACE_BREAK`）。未触碰 `findings/**/README.md` 与他人产出。
- 删除与改名各用一次 `rm -f -- <显式路径列表>` 与一次 `mv -f -- <显式路径>`（前台协调令要求删除散件，工具面无删除能力）；均为对 `问题扫描/findings/**` 内本代理自产文件的操作，未执行构建/测试/git 任何子命令。
