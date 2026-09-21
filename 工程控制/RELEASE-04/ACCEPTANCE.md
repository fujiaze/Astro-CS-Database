# ACCEPTANCE — RELEASE-04 验收记录

> 执行时填写。PASS 仅由前台独立验证后写入；证据给可复跑命令与路径。
> 前台统一口径（本轮）：`flock /tmp/astrocs_ci.lock python3 eng/ci/run_checks.py --all --profile fast`（entries=56 steps=102）、
> `ninja -C build`、以及各任务头号门的独立复跑；日志落 `run/FRONTEND-VERIFY/logs/` 与 `run/<TASK>/logs/`。

## 任务状态总表

| 任务 | 状态 | 机器门结果 | 证据路径 | 前台结论 |
|---|---|---|---|---|
| DOC-401 | PASS | 五份根文档与包内逐字节一致（cmp 全 OK）；无日期/任务编号/历史叙事 | 提交 344bf67f | 前台复跑一致 |
| DOC-402 | PASS | doc402_gate.py scan findings=0（基线 1829）；selftest 8/8；canon 6/6；check_no_weight_mode.py PASS（files=311） | 提交 d1ccfa1b；run/DOC-402/ | 前台复跑 scan/selftest/canon/no-weight-mode 全一致；10 项跨域项已转派 |
| DOC-403 | PASS | `check_doc_index.py --strict` rc=0（verdict=DOC_INDEX_PASS，v2.0.0；1772 行/319 条目）；`grep -rl '^> 上游：' docs` = 270 份 | run/DOC-403/RECEIPT.md；run/FRONTEND-VERIFY/logs/doc_index.log | 前台独立复跑 rc=0；270 抬头实测与回执一致；期间发现的锚漂移已由整合收口修复 |
| DOC-404 | PASS | check_pack_refs.py rc=0（doi 45/45、arxiv 10/10、url 24/24）；check_pack_oss_anchors.py rc=0（36/36 锚逐字命中） | 提交 dde5c4bd；run/DOC-404/ | 前台独立复跑一致 |
| SCI-401 | PASS | `实验/photometric-magnitude/code/run_all.sh` **rc=0**，gates 8/9 PASS（G6 PARTIAL(5/6) 与归档基线完全一致，N5 为既有已单列结论，未修绿）；11 个结果文件与归档**逐字节相同** | 实验/photometric-magnitude/results/{GATES.md,REVIEW.md}；提交 bb508dea；run/INTEGRATION-EXP-PATHS/EVIDENCE.md | 前台复跑**先受阻于整合引入的脚本路径回归**（rc=127，脚本仍指向旧 `实验/SCI-A`；连带发现 `config/filters.json` 未随整合改到 `eng/packaging/config/`），已由 INTEGRATION-EXP-PATHS 修复后复跑通过 |
| SCI-402 | PASS | `ctest -R 'p1snr\|p1noise_numpy_oracle'` **15/15 Passed rc=0**；`实验/absolute-snr/code/run_all.sh` rc=0 | 实验/absolute-snr/results/{ctest_evidence.md,COMPARISON_TABLES.md,REVIEW.md}；run/FRONTEND-VERIFY/logs/sci402_ctest.log | 前台独立复跑一致。**遗留科学发现 D1/D2 见偏差表（已立 FIX-407）** |
| SCI-403 | PASS | `实验/additive-sky-seamless/code/run_all.sh` rc=0，C1..C7 判据全 PASS；独立对抗审稿 6 项致命问题已修 | 实验/additive-sky-seamless/results/*.json、REVIEW.md；run/FRONTEND-VERIFY/logs/sci403.log | 前台独立复跑 rc=0，且 7 份结果 JSON 中 6 份与归档**逐字节相同**、`c6_sparse_dense.json` 仅 4 个 RSS 数值不同（机器噪声） |
| CLEAN-401 | PASS（域内）+ 5 项 BLOCKED | 退役实现二选一处置完整；`CHK-RETIRED-CODE` + `-SELFTEST` 已入门禁（红绿双向）；全仓 fast 门 98/4 | run/CLEAN-401/{AUDIT.md,RECEIPT.md,retired_code/} | 前台复核审计结论；**5 项删除被跨域注册表阻塞、§8.2 命名块接线判定为架构性改造不在本任务范围** —— 均入偏差表 |
| CLEAN-402 | PASS | 工程控制/ 仅剩 RELEASE-04（find 实测 32 文件）；check_doc_index.py --strict PASS；check_doc_symbols.py 0 findings | 提交 ae0f1e63；run/CLEAN-402/ | 前台复核删除清单；12 组保留候选已裁决保留（入偏差表） |
| CLEAN-403 | PASS | PRODUCTION-RESIDUAL 45 文件/397 命中 → **0**；修复旧扫描根被 `os.path.isdir` 静默跳过；fail-closed 锚 + 14 条自测负例 | run/CLEAN-403/RECEIPT.md、logs/ | 前台复核 5 条验收门全绿；aio 唯一 I/O 棘轮收口成立 |
| FIX-401 | PASS | 原子发布 + 磁盘满退出码 10（A1–A6 + M1/M2/M3 变异测试）；`CHK-FIX208-DISK-GATE` 由红转绿 | run/FIX-401/{README.md,mutations/} | 前台复核：磁盘满由 fail-open 改为失败瞬间分类（ENOSPC/EDQUOT） |
| FIX-402 | PASS | 7/7 门；生产 phase3 节点链语义守卫 + `SURFACE_BRIGHTNESS` 写端口 + 4 份生产产物核对 | run/FIX-402/RECEIPT.md | 前台复核通过 |
| FIX-403 | PASS | HiPS 层级累加去 f32 轨、恒 f64；O7 组独立 f64 对拍 + 逐层通量守恒 + 负例；1/2/4 线程树摘要位级一致 | run/FIX-403/REPORT.md、verify/ | 前台复核通过 |
| FIX-404 | PASS | MODULE_MAP 假路径 **0**（exists=95 / registered_gap=66 / fake=0）；lib/ 悬空注释 54→0；dangling_ledger 104→50；probe 15/15、`--selftest` 21 例 | run/FIX-404/RECEIPT.md | 前台复核通过；**66 条路径缺口 + 72 条能力缺口无归属任务**，入偏差表 |
| FIX-405 | PASS | 7/7 项：drizzle n_rejected_nonfinite、variance_floor fail-closed、DISP-P2SMP-002 双计数、10 类事件 kind、FITS 严格字节判据、`doctor --run-manifest`、weight_mode 移除、cpu_workers 掩码 | run/FIX-405/RECEIPT.md、evidence/ | 前台复核通过 |
| FIX-406 | PASS（含已登记偏差） | SIN 往返 2.5e-5 px 归因=灾难性消去（astropy/WCSLIB 8.4 = 2.83e-10 px）；TAN 容差按 Oracle 由 1e-6 → **1e-8 px**；`sin_deviation_gate.py` 必红复现门 | run/FIX-406/{SIN_ROUNDTRIP_ORACLE.md,TAN_ROUNDTRIP_TOLERANCE.md,WINDOWS_CANCEL_PLATFORM_LIMITS.md} | 前台复核结论成立：GAP_AUDIT G3-6「偏紧 25x」前提被表证伪；不放宽容差、不改 v6 内核，交 CLEAN-401 处置 |
| PERF-401 | 部分 PASS（未定稿） | 撤 cfitsio 进程级锁 + 计数式 `CfitsioLockGuard`（锁等待可观测）；执行合同 `EXEC-NO-AIO-SERIAL` 按变更流程换成 6 条更强判据，`--self-test` 7 例（1 正 + 6 负）全判红；行锚改行数中性编辑（1536→1536 行，行号零漂移） | run/PERF-401/{RECEIPT.md,EVIDENCE.md}；artifacts/acceptance/l2_performance/ | 代码与合同变更已落地且自证有牙；**但 L2 性能门本身是恒真门**（D-10），且实测**证伪** GAP_AUDIT G3-14 的锁归因（D-11）。最终测量未定稿 ⇒ 本行不转 PASS |
| BLD-401 | 部分 PASS | 四条基线红**全部转绿**（R1 清单与事实不符 → 订正 + 生成器加 `PRODUCTION_UNREACHABLE` 登记表并幂等再生；R2 `check_agents_gov.py` 改按标题定位硬禁令节 + 权威声明语义判定，自测 6 正 10 负；R3 `check_version_consistency.py` 加第三方工具版本豁免（三重收窄），自测 10 例；R4 `check_no_weight_mode.py` 按字段行收窄豁免，自测 8 例）；全量 ctest **6 红 → 1 红**（5 条整合回归，含 1 处 `io_adapter` **空断言恒真**已按 fail-closed 修好） | run/BLD-401/{RECEIPT.md,EVIDENCE.md,WINDOWS_HANDOFF.md} | `exemptions.json=[]`、零 waiver。**未达成**：Windows/MSVC 腿 NOT_VERIFIED（平台受限，不伪造绿灯）；`linux-main` 15 条内容面红未处置；`p1001_real_nodes` 产品行为红转 FIX-408 |
| E2E-401 | NOT_RUN（负责人裁决） | — | — | **负责人 2026-09-21 决定本轮不跑 E2E**，直接提交并转入下一个工程包 |
| VIS-401 | NOT_RUN（负责人裁决） | — | — | 同上；输入已核实可用（M42 49 帧 Red、银心 32 帧 Red，磁盘余量 177 GiB），未执行 |
| FIN-401 | NOT_RUN（负责人裁决） | — | — | 同上；版本号与发布决定仍只属负责人 |

## 发布门（ACCEPTANCE_SPEC §8）核对

| # | 门 | 结果 | 证据 |
|---|---|---|---|
| 1 | SCI-A/B/C 全部成立并审稿通过 | 部分 | 三单元独立审稿记录齐备（实验/*/results/REVIEW.md）；SCI-C 对抗审稿 6 项致命问题已修并复跑；**SCI-B D1/D2 遗留**（偏差表 D-4） |
| 2 | L1 科学性全绿 | 待 BLD-401 | run/BLD-401/ |
| 3 | L2 性能门通过 | 待 PERF-401 | run/PERF-401/、artifacts/acceptance/l2_performance/ |
| 4 | L3 端到端跑通 | NOT_RUN（负责人裁决） | 负责人决定本轮不跑 E2E |
| 5 | L4 两组视觉验收负责人确认 | NOT_RUN（负责人裁决） | 同上 |
| 6 | P0 机器门全绿、零 waiver | 部分 | fast 档 4 条基线红已转绿；`linux-main` 15 条内容面红未处置（D-8）；`exemptions.json=[]`、零 waiver |
| 7 | 仓库整洁（清理完成） | 部分 | CLEAN-401/402/403 均 PASS；5 项删除阻塞 + 12 组保留候选入偏差表 |
| 8 | 版本纪律（0.0.1alpha） | 待 FIN-401 | 版本号属负责人决定 |

## 已知偏差与遗留（不阻塞本轮交付，但负责人必须知悉）

| # | 偏差 | 性质 | 证据 | 推荐处置 |
|---|---|---|---|---|
| D-1 | ASTROCS_DESIGN §8.2「阶段内命名块内存管线」未在生产路径执行：20 个生产节点仅 1 个触命名块 API（Phase1 drizzle，节点内自建自毁），**没有任何一对相邻节点通过帧传数据**；`RunContext` 无帧成员 | 最高设计未落地（架构性缺口，非本清理任务范围） | run/CLEAN-401/AUDIT.md:29-32；docs/architecture/PIPELINE.md:45-47 | 立独立架构任务（`context.h` + `runtime.cpp` + 3 个 execute + 20 节点改写 + 数值等价验证）；本轮不声称 §8.2 已达成 |
| D-2 | CLEAN-401 五项删除被跨域注册表阻塞：B1 orchestrator 目录（50 文件/18411 行）、B2 `p3_v6_export.*` + v6_p3 集成测试、B3 v1 `p3_projection.*`、B4 healpix_drizzle shim、B5 psfsw/weight_mode 残留面 | 处置选择=「保留 + 统一注释块」（ENGINEERING_SPEC §2 第 2 种），非「先注释留着」 | run/CLEAN-401/RECEIPT.md §4 | B2/B3 的阻塞随 BLD-401 订正 PRODUCTION_EXECUTION_INVENTORY.csv 后解除；B5 需 DOC-402/contracts 域退役 `forbidden.weight_source_tokens`/`weight_modes` 两段 |
| D-3 | CLEAN-402 12 组保留候选裁决为保留（未被任何生产 target 引用但被注册表/输出登记锁定） | 同上 | run/CLEAN-402/RETAIN_CANDIDATES.md | 随注册表退役分批删除 |
| D-4 | **SCI-B D1**：σ_sky 在 `module_adapters.cpp:4258` 重复计入读出噪声 ⇒ σ_F 高估 +12.8%（基线）~ +36.6%（最坏）；**D2**：见 SCI-402 审稿记录 | 科学实现偏差（跨帧绝对 SNR 是三个创新点之一） | 实验/absolute-snr/results/REVIEW.md | **已立 FIX-407**，前台排期执行；FIX-407 完成前 SCI-B 的 σ_F 绝对定标不得作为发布依据 |
| D-5 | Windows `CTRL_CLOSE_EVENT` 无法保证退出码 9（平台限制）；FIX-406 的写窗口取消可能掩盖失败原因 | 平台/实现限制 | run/FIX-406/WINDOWS_CANCEL_PLATFORM_LIMITS.md | 随 Windows CI 轮次复核 |
| D-6 | FIX-404：66 条路径/目标缺口 + 72 条能力缺口在 RELEASE-04 内**无归属任务** | 控制包覆盖面缺口 | run/FIX-404/RECEIPT.md | 立落地任务或在下一控制包显式声明「不在本包范围」 |
| D-7 | 4 条整合前既有红门（UT-VERSION / AGENTS-GOV / CHK-SPEC-NAMED-IMPL-ON-PROD-PATH / CHK-NO-WEIGHT-MODE） | 门禁口径错 3 条 + 真数据缺陷 1 条 | 本文件任务表 BLD-401 行 | BLD-401 处理中；修口径必须配 `--self-test` 正负例，禁止放宽 |
| D-8 | `eng/tests/api` 5 条断言 CLI_PROTOCOL_V1.md 中从未存在的措辞（整合前后一致）；`eng/tests/config` CFG002-04 依赖 RELEASE-04 换版后已不存在的 `bader r` 反例 | 测试/文档内容缺口 | run/INTEGRATION-PATHS/EVIDENCE.md §4 | 属既有内容缺口，需改 `docs/api/` 或最高设计；上呈负责人 |
| D-9 | Windows/MSVC 平台面在本 Linux 节点不可真跑 | 环境不可达 | run/BLD-401/WINDOWS_HANDOFF.md | 不伪造绿灯；发布前在 Windows 节点补跑 |
| D-10 | **L2 性能门是恒真门**：`artifacts/acceptance/l2_performance/gates/*_gate.json` 的 `frozen_gate.verdict="pass"`、`violations=[]`，但 `recorded` 里四条冻结判据**全部违规**（平均利用率 0.245<0.85、p50 0.039<0.90、达标样本占比 0.069<0.70、连续 142 s 低利用窗），原因是 `enforcement=record_and_justify` —— 违规只记录不判红，**该门不可能判红** | 判据退化（违反 AGENTS.md §6「不用 waiver 盖红灯」、§9「恒真门没有证据资格」） | artifacts/acceptance/l2_performance/gates/real16_w16_gate.json | 把 `record_and_justify` 改为真判红 + 修 `worker_balance/*.csv` 的 `utilization_pct`（11 个 run、680 行**恒为 50.00%**，零判别力） |
| D-11 | **GAP_AUDIT G3-14 的锁归因被实测证伪**：撤掉 cfitsio 进程级锁 339.5 s vs 把锁注回 333.5 s（噪声内）；1→16 worker 只有 **2.37×**（923.4 s→389.3 s），平均 CPU 392%（**3.92/16 核 = 24.5%**）、p50 仅 62%（0.62 核），且存在**连续 142 s「利用率<60% 且无就绪线程积压」**窗口（`queued_work=false`）。真实瓶颈是串行段，不是锁 | 缺口归因错误（新事实） | artifacts/acceptance/l2_performance/{gates,worker_balance}/、run/PERF-401/ | 按"阶段串行段 + I/O 串行段"重新定位；顺带查 16 帧 mosaic **读 62.7 GB（≈3.9 GB/帧读放大）**、峰值 RSS 6.7 GB |
| D-12 | **`requires_monitor` 字段大面积不生效**：`eng/ci/run.py:109-119` 的 `monitor_gate_requested()` 只在**命令的监控参数区**出现 `--gate-required`/`--gate-workers` 时才算"请求判定"；8 个声明 `requires_monitor=True` 的检查里，CHK-UNIT/CHK-BUILD-LINUX/CHK-CONTRACT-TEST/CHK-SANITIZER/CHK-COVERAGE/CHK-REALDATA-E2E 的命令**都没有**这些参数 ⇒ 落到 `run.py:962 else: verdict=PASS`，`monitor_gate_evidence_gap()`（要求 outputs 含 `cpu_samples`+`frozen_gate`）对它们**根本不执行**。字段唯一机器效果是静态锚（要求 `resource_monitor.py` 存在）+ `validate_registry` R7 结构约束 | 登记字段与执行语义不一致（同 D-10 病根） | eng/ci/run.py:109-119/631/789/949/962；eng/ci/validate_registry.py:312-314 | 二选一：让 `requires_monitor=True` 真正强制监控证据（fail-closed），或把它降级为纯静态声明并改名为 `monitor_capable`。**注意 `mutates_workspace=True` 同理**：`run.py:789` 显示它只是跳过执行前后的 `git status` 对比（自我豁免），不是"写共享树" |
| D-14 | **ISA 测量工件写到陈旧路径**：`eng/tests/backend/test_isa_variants.py:79` 写 `artifacts/prerelease_v5/ISA-001/MEASUREMENTS.csv`（下划线、根级），而文档与已跟踪证据用的是 `artifacts/evidence/prerelease-v5/ISA-001/MEASUREMENTS.csv`（连字符、evidence 下）—— `reports/` → `artifacts/evidence/**` 整合时该输出路径未同步。后果：每次跑 UT-BACKEND 都在**无人读取的路径**重造测量工件，已跟踪的 ISA 证据永不刷新（本轮实测两套数值不同：ISA-001 hips +37.5% vs 归档 +37.8%、calibration +9.7% vs +24.4%，决策列一致但百分比是共享机器上的测量噪声）。该文件未入库（前台未提交游离目录），登记待修 | 整合遗留（输出路径未同步） | eng/tests/backend/test_isa_variants.py:79/96；`?? artifacts/prerelease_v5/` | 把输出路径改为 `artifacts/evidence/prerelease-v5/`，或明确该测量为临时产物应落 `run/`；顺带核 `eng/tools/pack_audit_package.py:30` 的 `OUT = artifacts/prerelease_v5` 是否同源陈旧 |
| D-13 | **CI 结构性问题（本轮已部分处置）**：`docs/ci/CI_SPEC.md:31` 原写「每个提交全量构建 + 全量检查（不做"只测改动"的默认跳过）」；`changed_paths` 在 71 项上全部声明但 **runner 从未读取**（死字段）；**无任何编译器缓存**（ccache/sccache/distcc 全缺）；`fast` 档 `timeout_seconds` 之和 **78780 s（21.9 h）**、单项 `CHK-UNIT` 21420 s；仓库里 5 棵独立 build 树各建各的。实测 `fast` 307 s（空载）/373 s（CI 内），其中 **6 步占 71%**，且第二梯队 15 个只读扫描是被**串行排队**（非各自慢） | 工程效率缺陷 | run/CI-INCREMENTAL/、run/BLD-401/logs/ci_fast.json | 本轮已完成：fast 重分档（56 项/103 步 → 52 项/99 步，重活移入新 `integration` 档）+ UT-RUNTIME 拆分（47+25 用例，不靠 skip）+ 规范面订正 + 最小版 `--changed`；**未完**：并行化 runner 等价性验证、两处现场 g++ 预编译、SECRET-HYGIENE 并发读、RESOURCE-GATE 合并窗口、构建图反查完整映射、指纹缓存 |

## BLOCKED / 上呈记录

| 任务 | 原因 | 上呈时间 | 负责人裁决 |
|---|---|---|---|
| CLEAN-401 B1–B5 | 删除动作被跨域注册表（`eng/ci/checks.json`、`eng/ci/spec_named_impls.json`、v6 合同冻结面）硬阻塞 | 2026-09-21 | 待裁决（BLD-401 已解开 B2/B3 的清单侧阻塞） |
| CLEAN-401 §8.2 | 命名块管线接线为架构性改造，超出清理任务范围 | 2026-09-21 | 待裁决（见 D-1） |
| FIX-406 | SIN 容差不上调、不改 v6 内核的处置 | 2026-09-21 | 前台已按证据裁决；v6 家族交 CLEAN-401 |
| SCI-402 D1/D2 | σ_sky 重复计入读出噪声 | 2026-09-21 | 前台已立 FIX-407 |
| 版本号 / 发布 | `0.0.1alpha` 与是否发布 | — | **仅负责人** |
