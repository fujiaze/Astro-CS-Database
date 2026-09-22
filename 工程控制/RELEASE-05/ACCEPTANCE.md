# ACCEPTANCE — RELEASE-05 执行状态登记（执行 agent 维护）

## 任务状态

| ID | 状态 | 提交/证据指针 |
|---|---|---|
| DOC-501 | PASS | 3f5265ff、bf1bc15b、4e8d476d；doc-index/DOC-L0/CHK-STALE-DOC 等 5 门 PASS；六份 cmp 为空 |
| DOC-502 | PASS | b85c79dd（7 条结论落点）+ 1d9609ae / 3f7feb14（按 SCI-503/504/505 独立复核订正目录漂移与 8 处数值/口径） |
| SCI-501 | PASS | cdcc0975；p1snr_science_skysource 双向非退化（zA=1.27 绿 / zB=25.6 红 / legacy 逐位一致）；ctest 全绿 |
| SCI-502 | PASS | d77fd11f（FIX-1 观测尺度相对容差 / FIX-2 四态 0-1-2-3 + stalled / FIX-3 κ 自适应 + provenance）+ 69641b02（**门控 κ 口径实证订正** + 专项测试 12/12）；ctest -R 'p2\|upm\|sky\|UPM' 59/59 + v6_p2_sky_kappa 通过 |
| SCI-503 | PASS | 1efb5c9e；实验/photometric-magnitude/REPORT_paper.md（八段 + 附录 A 34 条索引）；187 项数字对 results 复核无越界 |
| SCI-504 | PASS | dbce6add；实验/absolute-snr/REPORT_paper.md（八段 + 附录 A 40 条索引）；68 项数字复核 0 处不符 |
| SCI-505 | PASS | 2150c162 + ea121e86；实验/additive-sky-seamless/REPORT_paper.md（八段 + 附录 A 41 条 + C 9 条口径冲突 + D 28 条边界） |
| SCI-506 | PASS | 7716e16a；根因=测试期望过时（CLEAN-401 入口 fail-closed），非产品缺陷；新增 NaN fail-closed 负例 |
| CONTRACT-501 | PASS | ff07b958；四份合同 + 五份 schema + 双向校验器（--self-test 4/4 红绿双向）；doc-index PASS |
| ARCH-501 | PASS | b3a1dd27；BlockFrame 生命周期 + DAG 四条非法图判据 + provenance + 取消无泄漏；ctest core_block_frame 全绿且**负例注入实测能红** |
| ARCH-502 | PASS | 53c9c0f0 + 9480f26e；帧内 DAG 流水 + 多帧并发 + 专用预取线程（`prefetch_threads`，默认 1=关）+ 探针 8 类事件；测试 32/32：N=1/2/4/8 逐位一致、预取开启 0.1025s vs 关闭 0.1457s（**1.42×**）、磁盘满路径、探针 JSONL 39 行全 schema 合法；`CHK-ARCH502-NORMALIZE-WF` + `CHK-SCHED-PROBE-SCHEMA` 登记 |
| ARCH-503 | PASS | c107e596；天区窗口划分（`window_tiles` 落 manifest `astrocs.mosaic-window-manifest/v1`）+ 按窗口读路由 + 现场稠密 SNR + 归约顺序冻结；测试 19/19：N=1/2/4/8/16 **逐位一致**且与单窗口参考**逐位相同**（容差 0）、读放大 0.25（393216 B vs 朴素 1572864 B）、峰值驻留 32 B 与总图规模解耦；`CHK-ARCH503-MOSAIC-WIN` 登记 |
| ARCH-504 | PASS | 6811803b + 60689b99；三级有界流水线（读/算/写），背压**只在 reader** 施加（`inflight_count_ < 2*queue_depth`，避免互锁）；WCS 头与 properties 开写前组装（缺则 exit 1）；原子发布（临时文件 → rename）；磁盘满 exit 10 且临时文件不发布。**关键正确性发现**：FITS 是全图行主序而子块是 tile 局部行主序，必须按行定位——原 ofstream 逐行 `seekp` 实现已改为**行带缓冲顺序追加**（内存上界 width×band_rows×8 B，全程无 seek，I/O 仍经 aio）。测试 22/22：与整图参考**逐位相同**（checksum 2882422816247716598）、worker 数无关、在途峰值 1048576 B 在 1×/4× 规模下相同；`CHK-ARCH504-EXPORT-STREAM` 登记。**ACCEPT-501 独立复核抓到并已修**：`inflight_bytes_/inflight_count_` 递减未持锁（reader 递增持锁）⇒ 无符号下溢，约 21% 概率红；修后连跑 **40/40 PASS** |
| ARCH-505 | **BLOCKED**（部分落地） | 557ddd68；块流规格（20 节点/26 块，`eng/contracts/block_flow/stage_block_flow.json`）+ `StageBlockFlow` 执行器（名字级 fail-closed、生命周期归执行器、SHORT 残留=0、与直接顺序计算**逐位一致**，测试 28/28）+ 规格机器门 12/12 + 一致性登记册机器门 10/10。**阻塞**：注册表声明端口与代码真实数据流不一致（原 12 条、经 ACCEPT-501 独立复核补至 **18 条、13 blocker**），按声明图迁移节点只会得到 facade ⇒ 按任务书「不等价就保留并登记阻塞原因，不硬退役」，Session 与文件约定保留，上呈 **OQ-9**。顺带修掉 ARCH-501 `BlockDagValidator` 把空生产者无条件判 UNKNOWN_NODE 的真实缺陷（外部输入无法表达），ctest `core_block_frame` 33/33 |
| CLEAN-501 | NOT_RUN | 依赖 ARCH-505；本轮完成其独立部分：调度器新代码 I/O 全部改经 aio（60689b99，PRODUCTION-RESIDUAL=0）、线程预算/串行硬编码/aio 台账/文档行号锚四门登记补齐 |
| GATE-501 | PASS | 25c8227d；L2 真判红（归档回放 9/9 翻红）、监控字段真强制、worker_balance 判别力、230 单元 fail-closed 普查 |
| GATE-502 | PASS | 77e3097b / 090390b9 / abcc0529 / 091e0755 / 6f217cef；空断言清零（findings=0）+ 空断言静态门（--self-test 2 正 6 负）+ 测试契约对齐 + 路径修复 + 预检 11 用例矩阵；**全量 ctest 478/478 PASS** |
| PERF-501 | NOT_RUN | 依赖 ARCH-505 + CLEAN-501 + GATE-501 |
| ACCEPT-501 | PASS（NO-GO 回执已闭环） | 独立子代理（非实现者）只读复核，报告 `run/RELEASE-05/accept/ACCEPT-501_report.md`（五一致性矩阵 + 14 条问题 + 证据 + 复现命令）。**结论 NO-GO**，最严重 3 条：① export_stream 真实数据竞争（**已修**，见 ARCH-504 行）；② fast 档非全绿（**已修**：deep 构建树重建 + 3 个自测步骤 missing_output 修复 ⇒ 120/120）；③ 块流登记册漏登记（**已补** 6 条 ⇒ 18 条）。其余指出项：空断言门盲区（**已修**：正则补 `CHECK(1,"msg")`、扫描面补 `.c`，findings=0）、E2E docstring 两条判据未实现（**已实现**：FITS 结构自洽 + 目录纪律）、四个新调度器生产路径零引用、`sigma_sky_source` fail-closed 未实现、PHOTOMETRY n≥100 未落地 —— 后三项**未修**，登记为遗留 |
| BLD-501 | NOT_RUN | |
| E2E-501 | PASS | 02bf4704；三命令真实数据全链**首次贯通**（银心 T4 panel1 Red 180s 3 帧，含**真实 Gaia IPV 解算**）：normalize/mosaic/export 全部 rc=0，export 覆盖率 262144/262144、reopen_ok=1、canonical_match=true。manifest 链**独立复算**（Python 复现 `p3n_input_manifest_hash` 公式，与产品自报值逐位一致）。预检矩阵 11 例全绿（GATE-502 产出此前**未注册**，本轮补登记）。`CHK-E2E-CHAIN`/`CHK-E2E-CHAIN-SELFTEST`/`CHK-PREFLIGHT-MATRIX` 登记。排障中确认 5 条真实契约门（dark 约定、母版单位域、gaia_data_dir 层级、hips_paths 须逐帧产品树、导出中心须落在覆盖内）。**M42 数据集未跑通**（OQ-10，根因已定位：60 颗选星全饱和 ⇒ 密度估计污染 ⇒ 首个 Gaia 查询返回 0 即放弃） |
| VIS-501 | NOT_RUN | |
| REPORT-501 | PARTIAL | OPEN_QUESTIONS 已成文（a4b3fe24，8 条）；SUMMARY/审核包未成文 |

## 双线文件域冲突登记

| 线 | 触及共享面 | 前台裁决 |
|---|---|---|
| 前台 | `eng/tools/doccheck/check_doc_index.py`（B 线域） | 控制包登记动作，前台直接改并单独提交（d8495a65） |
| 前台 | `lib/algorithms/noise_snr/tests/p1noise/CMakeLists.txt`、`eng/tests/unit/CMakeLists.txt`、`CMakeLists.txt`、`eng/ci/checks.json`、`eng/ci/id_migration_map.json`、`eng/ci/ledgers/**`（B 线域） | SCI-501/502、ARCH-501 新增 ctest 目标与注册必须同提交登记，按"新增测试同提交登记"规则处理，本表登记 |
| 前台 | `docs/contracts/**`、`eng/contracts/schemas/**` | CONTRACT-501 本体（B 线域，由前台执行以避免双线阻塞） |

## 偏差与开放问题

- **D-1（架构线大部未启动）**：ARCH-502/503/504/505、CLEAN-501、PERF-501 未开始。生产路径**仍是整阶段 Session 装配**，不是最高设计 §8.1 的三独立调度器 + 命名块内存管线；"生产路径无 Session 直接装配"验收门未达成。本包最大未完成面。
- **D-2（收尾线未启动）**：ACCEPT-501 / BLD-501 / E2E-501 / VIS-501 未开始；REPORT-501 仅 OPEN_QUESTIONS 部分。
- **D-3（RELEASE-04 物理出库）**：已按"归档白名单 + 必须自带 SUMMARY.md"保留并保持门禁有牙；**是否物理出库请负责人裁决**（OPEN_QUESTIONS Q-3）。
- **D-4（规范冲突）**：控制包 SCI-502 任务书的 converged 枚举与仓内权威 11_upm.md §4.6 冲突，已按 AGENTS.md §1.1 采用仓内权威（OPEN_QUESTIONS Q-4）。
- **D-5（DOC-502 独立审稿）**：本轮**已由 SCI-503/504/505 三个独立子代理完成复核**并驱动 1d9609ae/3f7feb14 两轮订正（目录漂移 8 份、PSD 峰 k 值、节点间距 2×、κ 双口径、additive_mode、smoothing_lambda、FIX-1 行号、单位歧义）；C6（ACCEPTANCE_SPEC §2.2 措辞）属根文档域未改。**该项关闭**。
- **D-6（SCI-501 对拍）**：生产 C++ ↔ Python 镜像对拍（2.24e-14 量级）未复跑；以 C++ 内置 long-double 参考 + 独立 MC（zA=1.27）替代佐证。仍为未完成项。
- **D-7（SCI-502 真实样本复跑）**：M42 真实样本未用修复后库复跑（κ=3.16e7 取自归档 c7_realdata.json）；c1–c7 亦未复跑。**未完成项**（OPEN_QUESTIONS Q-6 相关）。
- **D-8（新检查器注册）**：`eng/tools/contract_doc_sync.py` 尚未注册进 eng/ci/checks.json（需一个 ctest/检查单元与 ID 迁移映射条目）。GATE-501 未处置，**转 GATE-502 或后续任务**。
- **D-9（artifacts/prerelease_v5/）**：已由 GATE-501 删除并端到端验证（D-14 台账）。**关闭**。
- **D-10（κ 双口径未统一）**：天光面求解器 `kappa_max` 默认 1e8、UPM/GLS 默认 1e6（FZ-AP2S-KAPPA-MAX）。真实 M42 κ=3.16e7 在 1e8 下不触发自适应、在 1e6 下超限。**未决项**（OPEN_QUESTIONS）。
- **D-11（GATE-501 未完成项转派）**：lib 侧 `utilization_pct` 恒 50.0 根因（cli/commands.cpp + resource_recorder.h）未修；生产侧 `record_and_justify` 未翻转；`--self-test` 覆盖 22/78；UT-BACKEND test_05 编译缺 include（改动前即失败）；CHK-SCI-REF 档位作用域问题。
- **D-12（规范严于实现）**：`07_noise_snr.md` §4.2a 的"声明缺失即 fail-closed"落在**调用点层**（C ABI 保留 legacy `UNSPECIFIED=0` 兼容路径且与 SHOT_ONLY 逐位一致）；文档已按实际精确表述，生产调用点已声明。
- **D-14（integration 档 CHK-FIX208-DISK-GATE 红）关闭**：判据断言了 `resource_gate` 事件里不存在的字段名，而该事件冻结的扩展字段集里承载同一事实的是 `enforcement`（值由 `gate_enforcement()` 派生，§9.74 裁决恒 RecordOnly ⇒ `"record_only"`）。`resource_gate` 的冻结扩展字段恰 7 个——`diag / enforcement / strict / enforced / work_core_seconds / workload_floor_core_seconds / workload_floor_reached`——在 `protocol.h`、`jsonl_event_v1.schema.json` 的两处登记与两份读侧对偶实现四面逐字一致；字段名与语义以这四面为正本。判据现断言该冻结字段，并额外要求「违规场景必须真的记录该事件」（`ASTROCS_TEST_PIPELINE_SLEEP_MS=12000` 造违规）；**正向约束：该用例不得在事件未出现时通过**。门连跑 5/5 PASS。证据 `run/RELEASE-05/logs/rel05_integration{,2}.log` 与 `run/RESGATE-EVT-01/`。
- **D-15（GATE-502）关闭**：五个提交（空断言清零 / 契约对齐 / 路径修复 / 预检矩阵 / 门注册）已按目的拆分提交；全量 ctest 478/478 PASS（`run/RELEASE-05/logs/gate502_ctest_full.log`）。其回执的 D3/D4/D5/D6 已由本轮 69641b02 与 7716e16a 修掉（contract-freeze 留痕 / §7 目录 / aio 台账 / CTEST-P1001）。
- **D-16（GATE-502 域外派单，未修）**：① `eng/tools/check_ast_api.py:49` 仍用 `<repo>/include` 且缺 aio include 面 ⇒ AST-API 恒红（检查器缺陷，产品构建正常）；② linux-main 档步骤超时预算不足（CTEST-LINUX-FULL 3600s 预算未到即被外层 1800s 杀掉，212 步只跑 182 步）⇒ 该档全量结论无法完整复现；③ `lib/` 内仍有 8 处恒真断言（aio/orchestrator 测试，A 线域）；④ 2 条 UT-BACKEND 持续红：`test_p2007::test_04` 峰值 RSS 1.45GB 超 512MB 界（RESOURCE/MON 域）、`test_p2003::test_02` 校正后 median 恶化（SCI-C/UPM 域）；⑤ `eng/tests/validation/release02/.../seam_fixed.py:44` 语法错误（RELEASE-02 遗留）；⑥ ISA 证据工件被测试现场重测覆写跟踪路径（GATE-501 D-14 收口方式的副作用）。
- **D-13（测光 n≥100 适用域未落地）**：PHOTOMETRY.md §16.5 定案阈值在生产代码中未实现（硬门槛仍 kMinFitStars=3）。**未决项**（OPEN_QUESTIONS Q-5）。

## 发布门核对

- [x] 三篇论文式报告交付（SCI-503/504/505）
- [x] 五一致性独立验收**已执行**（ACCEPT-501 独立子代理，回执 NO-GO；其 3 条 BLOCKER 已闭环，其余遗留项见 D-17）
- [x] fast 全绿（**120/120**）+ 全量 ctest **482/482**（`run/RELEASE-05/logs/accept_fix_{fast,ctest}.log`）；[ ] Windows 腿未跑、integration 档 1 条未定位红（D-14）
- [ ] 两组成品帧 agent 自验通过、负责人目检认可（VIS-501 进行中：银心 T4 组 32 帧全链已跑通，M42 组被 OQ-10 阻塞）
- [x] OPEN_QUESTIONS 已交付（含 OQ-9 块流结构冲突、OQ-10 M42 解算根因）
- [ ] FIN（README、0.0.1alpha）经负责人明确授权 —— **未达发布门，不申请发布**

## 追加登记

- **D-17（ACCEPT-501 指出、本轮未修）**：① 四个新调度器（ARCH-502/503/504/505）目前**只在库与测试中存在，生产路径零引用**——即「已实现且自证正确」但**尚未接线**，`ARCH-505` 的迁移被 OQ-9 阻塞，故三命令当前仍走原 Session 路径；② `docs/algorithms/07_noise_snr.md` §4.2a 第 4 条的 `sigma_sky_source` 声明缺失 fail-closed 未在 `snr_science.cpp:177-182` 实现（D-12 的同源项，A 线域）；③ `PHOTOMETRY.md` §16.5 的 n≥100 适用域仍未落地（同 D-13）；④ ACCEPT-501 报告指出 `eng/ci/known_failures_baseline.json` 的 `source_commit` 需随提交刷新（本轮已由 b2876888 刷过一次，后续每次提交后需再刷）。
- **D-18（本轮已修，留痕）**：ACCEPT-501 抓到的 export_stream 数据竞争（P-1）与三个自测步骤 missing_output（P-2）**已修并复验**：竞争修后 export_stream 连跑 40/40 PASS；自测步骤补 `--json-out`；`run/ci/build-gcc-release` 重建后 `mosaic_window`/`block_flow` 两个 ctest 目标 PASS。空断言门盲区（正则漏 `CHECK(1,"msg")`、不扫 `.c`）已修，修后立刻抓到 3 处恒真断言并改为**每节点** destroy 实际计数断言（改后该断言具备判别力：循环累积时实测判红）。

结论：本包**未完成**，不进入 FIN。已完成的确定成果 = 阶段 1 科学闭环（DOC-501/502、SCI-501..506）、CONTRACT-501、ARCH-501..504、E2E-501、GATE-501/502，以及 ACCEPT-501 独立复核与其 3 条 BLOCKER 的闭环。**未完成**：ARCH-505 迁移（阻塞于 OQ-9）、CLEAN-501、PERF-501、BLD-501（Windows 腿）、VIS-501（M42 组阻塞于 OQ-10）、REPORT-501。
