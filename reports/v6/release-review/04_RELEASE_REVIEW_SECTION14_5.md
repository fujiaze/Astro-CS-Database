# §14.5 发布复核：完成顺序逐项判定（DOC-CONVERGE-001 / V6 并行包 Wave 12）

- 任务：DOC-CONVERGE-001（write_scope：`docs/`、`README.md`、`REVIEW.md`、`CHANGELOG.md`、`reports/v6/release-review/`、`artifacts/v6/release-review/`）
- 基线 HEAD：`8e1e280e8db498aa879abd52ed8d18fa9f0eabd2`（只读 `git rev-parse HEAD` 复核）
- 判定依据：冻结宪章 §14.5（控制包完成顺序）、§17.12（发布门禁，尤其门 7/门 8）、§15.3（Fatduck）、
  控制包 `CONTROLLER_LOG.md` C-004..C-009 与 `TASK_LEDGER.csv`
- 机器可读判定：`artifacts/v6/release-review/release_review_verdict.json`
- **本任务不得宣布发布；本文件只是复核包，不是发布决定。** 最终发布决定权只属项目负责人（宪章 §17.12）。

## 0. 结论（一句话）

按宪章 §14.5 冻结顺序，六步中 **5 步未满足、1 步等待（AWAITING）**；
因此控制包状态为 **NOT_READY（NOT_RELEASED）**，
**不得**宣布发布、**不得**提升根 `VERSION`/`alpha` 编号、**不得**写成 `RELEASED`。

## 1. 六步逐项判定

| # | §14.5 步骤 | 判定 | 依据（文件锚 / 命令 / rc） |
|---|---|---|---|
| 1 | 任务提交全部完成 | **NOT_MET** | `TASK_LEDGER.csv`：`WIN-VERIFY-001 = WAITING_WINDOWS`（非 PASS）、`DOC-CONVERGE-001 = READY`（本任务）、`FINAL-AUDIT-001 = BLOCKED`、`OWNER-PACK-001 = BLOCKED`；Wave 0–12 之外仍有未完成/未提交任务 |
| 2 | GitHub CI 通过 | **NOT_MET** | Linux CI 整条 V6 线自 `ebefe00d`（Wave 3）起持续红。基线 `8e1e280e` 本地复现：`tools/arch/check_thread_budget.py` **rc=1**（`lib/core/src/module_adapters.cpp:245,252` omp_set_num_threads 未登记，**属 P36 回退态**）；`tools/quality/check_ctest_registration.py` **rc=1**（余 2 项**非 V6** IPV 残项 `ipv_dead_params_lock`/`ipv_dead_params_lock_selfcheck`）。Windows CI run `35012779853` = **failure**（step 5 exit 1，无候选）。旁证 PASS：`AGENTS-GOV` rc=0、`ci/check_version.py` rc=0、`check_version_namespaces.py` rc=0（日志 `artifacts/v6/release-review/logs/ci_redline_recheck.log`） |
| 3 | Linux 最终 SHA 真实数据流终验 | **NOT_MET** | `REAL-SCIENCE-001` 在 9 数据集 × 5 帧 × 8 共同星上做**库级五口径**比较（Oracle 486/486、负向 40+15 rc=0），但 `equal/exposure/ivar` 三口径为 **DOCUMENTED_BASELINE（驱动内实现）**，非全链生产路径；**Phase2 CLI 真实数据不可达**（`obs=0`/`overlap_controls=0` → control ivar 缺失 fail-closed）；M42/银心 Phase1→Phase2 **马赛克**终验与 `write_phase1_product → run_point_information/run_psfsw_robust` 未跑；`G-RD-01`/`G-RD-02` 仍 OPEN |
| 4 | Agent 图像初审 | **NOT_MET** | §13.4 要求对 M42/银心马赛克以固定显示参数导出预览并由 Agent 做结构化初审；`reports/v6/` 全量检索 `图像/预览/preview/初审` **零命中**，无 signal/coverage/接缝/rejection 预览与可疑区坐标结论 |
| 5 | Windows/Fatduck 复验 | **AWAITING** | `WIN-VERIFY-001` = **AWAITING_WINDOWS_VALIDATION**：Fatduck（100.104.10.71:22）不可达（ssh rc=255 / tcp rc=124 / ping 100% loss / tailscale offline）；Windows CI 无候选；32/32 Windows 用例 UNAVAILABLE，sha256 清单留空；`FD-F-003`（Windows C++ 单测门长期 0 用例）仍 OPEN。宪章 §15.3 允许标 `AWAITING_WINDOWS_VALIDATION` 且不阻塞 Linux，但**不得据此发布** |
| 6 | 汇总和打包 | **NOT_MET** | `OWNER-PACK-001 = BLOCKED`（depends_on FINAL-AUDIT-001），审核包/发布包尚未生成；本 W12 交付的是**发布复核包**（`reports/v6/release-review/`），不是 owner 打包 |

## 2. §17.12 发布门禁（Agent 侧可核项）

| 门 | 状态 | 事实 |
|---|---|---|
| 门 1 本文及 SCI/ALG/DATA/API 冻结且无冲突 | **部分** | V6 96 条款冻结合同在位（FROZEN 39 / PENDING_OWNER_SIGNOFF 49 / OPEN 8）；非 v6 SCI 正文冲突由 W4 登记为取代项，**正式 amendment 待负责人签字**（`AR-032`/`SO-06`） |
| 门 2 文档—模块—源码—测试追踪无断链 | 依各任务证据；本任务闭合 `OI-04` 文档索引覆盖红 | `check_doc_index.py` rc=0（覆盖 282 文件；`artifacts/v6/release-review/logs/doc_index_after.json`） |
| 门 4 三 Phase 声明与实际可用状态一致 | **部分** | 三生产模式 + 三 Phase 集成实现；Phase2 CLI 真实数据不可达、Phase3 `healpix_interp4`/`流式 FITS` NOT_IMPLEMENTED |
| 门 6 heavy 无硬编码线程/无持续低利用率/无界内存增长 | **未满足（待裁决）** | `THREAD-BUDGET` 红（P36 回退态线程预算旁路）；16w CPU mean 65.09% < 85%、p50 87.63% < 90%；`memory_growth_unbounded@16w` 未定性（`SO-05`） |
| 门 7 同 SHA Linux/Windows CI 通过 | **未满足** | Linux CI 红 + Windows CI failure |
| 门 8 Linux testdata/Gaia 全量 + M42/银心终验 + Fatduck Windows 复验 | **未满足** | 见第 1 节第 3、5 步 |
| 门 9 接缝/黑洞/条纹/排异/HiPS 结构量化证据 + Agent 初审 + Owner 终审 | **未满足** | 无图像初审证据 |
| 门 10 P0/P1 问题为零 | **未证实** | `FD-F-003`（P0 OPEN）；`AR-035` 785 合并层缺陷账本无销账任务 |
| 门 12 只有负责人可作最终发布决定 | **遵守** | 本任务未宣布发布、未提升版本 |

## 3. 包状态建议

- 控制包状态：**NOT_READY（NOT_RELEASED）**。未满足 §14.5 第 1/2/3/4/6 步，第 5 步 AWAITING。
- `docs/archive/review/RELEASE_STATUS.md` 的 `AWAITING_EXTERNAL_RELEASE_REVIEW` 也**未达成**（该字面量表示"已备齐、待外部审阅"）；本包连"备齐"都不成立。
- 建议控制器：按 §15.3 不因 Fatduck 离线停工，但 `FINAL-AUDIT-001`/`OWNER-PACK-001` 必须**如实携带**本轮 CI 红线与未满足门，**不得以 waiver 掩盖**（裁决 R-05/R-13）。

## 4. 本任务纪律与边界

- 未 commit/push/add/分支/worktree/stash/reset/clean/rebase；未派生子代理。
- 只写 `docs/`、`README.md`、`REVIEW.md`、`CHANGELOG.md`、`reports/v6/release-review/`、`artifacts/v6/release-review/`。
- 未改 `contracts/**`、`ci/**`、`.github/**`、`lib/**`、`cli/**`、`runtime/**`、`tests/**`、`CMakeLists`、`VERSION`；未改冻结公式/容差/门；未用 waiver 掩盖 CI 红线；未宣布发布。
