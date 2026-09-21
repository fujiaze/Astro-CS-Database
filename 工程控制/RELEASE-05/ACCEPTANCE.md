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
| ARCH-502 | NOT_RUN | normalize 异步工作流调度器 |
| ARCH-503 | NOT_RUN | mosaic 空间窗口并行 |
| ARCH-504 | NOT_RUN | export 子块流式 |
| ARCH-505 | NOT_RUN | 生产节点改写与 Session/orchestrator 退役 |
| CLEAN-501 | NOT_RUN | |
| GATE-501 | PASS | 25c8227d；L2 真判红（归档回放 9/9 翻红）、监控字段真强制、worker_balance 判别力、230 单元 fail-closed 普查 |
| GATE-502 | RUNNING | 子代理 367515a2（测试体系审计） |
| PERF-501 | NOT_RUN | |
| ACCEPT-501 | NOT_RUN | |
| BLD-501 | NOT_RUN | |
| E2E-501 | NOT_RUN | |
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
- **D-14（integration 档 CHK-FIX208-DISK-GATE 红）**：`--profile integration` 全档跑两次均 1 红（该门 `eng/tools/quality/check_resource_gate_real.py`），**单跑两次均 PASS**（`RESOURCE_GATE_REAL_PASS`，rc=0；serial 注入 rc=10 判红正常）。GATE-501 亦观察到一次同类瞬时红（其回执记 `resource_gate_mode` 为 None）且未定位确定性根因。判定：**非本任务文件域**（resource_gate 属工具/门禁面），如实登记为未定位的全档并发/顺序相关红，**未修、未 waiver**。日志 `run/RELEASE-05/logs/rel05_integration{,2}.log`。
- **D-15（GATE-502 未回执）**：子代理 367515a2（测试体系审计）在本会话结束时仍为 running，其工作区改动（`eng/tests/**`、`eng/packaging/config/filters.json`、`eng/ci/known_failures_baseline.json`、`eng/tools/quality/check_test_discriminative.py`、`eng/tests/cli/test_preflight_matrix.py`）**未提交**，随工作区留存待其回执后由前台提交。
- **D-13（测光 n≥100 适用域未落地）**：PHOTOMETRY.md §16.5 定案阈值在生产代码中未实现（硬门槛仍 kMinFitStars=3）。**未决项**（OPEN_QUESTIONS Q-5）。

## 发布门核对

- [x] 三篇论文式报告交付（SCI-503/504/505）
- [ ] 五一致性独立验收通过（ACCEPT-501 未开始）
- [ ] fast + 全量 + Windows 全绿、零 waiver（fast 见 run/RELEASE-05/logs/rel05_fast2.log；Windows 腿未跑）
- [ ] 两组成品帧 agent 自验通过、负责人目检认可（VIS-501 未开始）
- [x] OPEN_QUESTIONS 已交付
- [ ] FIN（README、0.0.1alpha）经负责人明确授权 —— **未达发布门，不申请发布**

结论：本包**未完成**，不进入 FIN。已完成的确定成果 = 阶段 1 科学闭环（DOC-501/502、SCI-501/502/503/504/505/506）、CONTRACT-501、ARCH-501、GATE-501。
