# OWNER-PACK-001 — V6 控制包负责人审核包（Wave 14 · 汇总与打包）

- 任务：`OWNER-PACK-001`
- 控制包：`AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915`
- write_scope：`reports/v6/owner-package/`、`artifacts/v6/owner-package/`
- 基线 HEAD（本包 `git rev-parse HEAD` 实测）：`6b45995231cb873e6b33a3817353d266633f6d9f`（= `origin/main`）
- 判断依据：根 `ASTROCS_PROJECT_CONSTITUTION.md` §14.5/§17.12/§15.3/§16.1/§13.3/§13.4/§10.5，`AGENTS.md`，控制包 C-001..C-010，`TASK_LEDGER.csv`，各任务交付与 `reports/v6/final-audit/`
- **本包状态：`NOT_READY / NOT_RELEASED`。本包不宣布发布、不提升 `VERSION`/alpha、不替负责人作发布决定（宪章 §17.12/§16.1）。**

---

## 0. 一眼可见（负责人先读这一节）

### 0.1 状态卡

| 项 | 值 |
|---|---|
| 控制包状态 | **NOT_READY**（`NOT_RELEASED`） |
| 是否发布 | **否**（`announce_release=false`；Agent 无权宣布发布） |
| `VERSION` | `0.11.0-alpha.2`（**未提升**） |
| §14.5 六步 | 5 × NOT_MET + 1 × AWAITING（无 PASS） |
| BLOCKER | **5**（B-01..B-05，逐条见 §5） |
| MAJOR / MINOR / INFO | 5 / 5 / 8 |
| 待负责人裁决项 | **12**（结构化见 `artifacts/v6/owner-package/owner_decisions.json`，正文见 §6） |
| 任务交付 | 35 任务（34 已独立集成 commit + 本任务在途）；V6 区间 47 commit |

### 0.2 未就绪事实（必须让负责人先看到）

1. **GitHub CI 未过（B-01）**：本包在基线 `6b459952` 独立复跑，`THREAD-BUDGET` rc=1（`lib/core/src/module_adapters.cpp:245,252` omp_set_num_threads 未登记，属 P36 回退态）、`CTEST-REGISTRATION` rc=1（2 项非 V6 IPV 残项）；Linux CI 整条 V6 线自 `ebefe00d`（Wave 3）起持续红。§14.5 第 2 步 **NOT_MET**。
2. **真实数据终验未执行（B-02）**：§13.3 要求的最终 SHA 全 `testdata`/Gaia 数据流、M42/银心分进程 Phase1→Phase2 马赛克、§13.4 图像初审均未做；`REAL-SCIENCE-001` 的 equal/exposure/ivar 为 **DOCUMENTED_BASELINE（驱动内实现）**，不是全链生产验收。§14.5 第 3/4 步 **NOT_MET**。
3. **Windows 未复验（B-03）**：Fatduck 不可达（ssh rc=255 / tcp rc=124 / ping 100% loss / tailscale offline），Windows CI 无候选，32/32 用例 UNAVAILABLE。§14.5 第 5 步 **AWAITING**。
4. **V6 三 Phase 生产入口零消费者（B-04，AR-037 新缺口）**：`write_phase1_product` / `run_point_information` / `run_surface_gls` / `run_psfsw_robust` / `p3_v6_export` 在 CLI/生产中无消费者；`--mode point_information/psfsw_robust` 实际仍走 **legacy** `run_with_resource_gate` 路径。**生产可达性不成立，不得对外称 V6 口径已可用。**
5. **资源门未达且 fail-open（B-05）**：16 worker 活跃窗口 CPU 均值 **65.09% < 85%**、p50 **87.63% < 90%**、`memory_growth_unbounded@16w`（34.01 MB/s ≥ 32，未定性）、全预算 `alloc_reclaim_missing`；当前按 C-007 记 `record_only_pending_owner_signoff`（`hard_fail=false`），**未获签字、不得计入 PASS**。
6. **治理与覆盖未闭合**：49 条 PENDING 数值阈值已作硬门执行但未签字（M-01）；FROZEN 算法文档 §4.2 单位标签被无签字订正（M-02）；F1 治理提交把 P33/P35/P36 预存回退固化为 V6 基线，删除 9 项 CI 检查登记 + 8 个测试目标 + `cli/frame_admission.h` 且缺授权（M-03）；QA W10 真实数据门未执行且 `case_ledger` 未回写（M-04）；台账 PASS 与 artifact `REVIEW_REQUIRED` 并存（M-05）。
7. **§17.12 发布门**：门 6/7/8/9/10 均未满足（详见 `artifacts/v6/release-review/release_review_verdict.json`）；`AWAITING_EXTERNAL_RELEASE_REVIEW` 亦未达成（连"备齐"都不成立）。

---

## 1. 执行摘要

### 1.1 交付了什么

- **35 个任务**：Wave 0（BASE-OWN-001）→ Wave 14（OWNER-PACK-001）。截至基线 `6b459952`，34 个任务的单任务 commit 已由控制器独立集成并按 §14.5 立即 push `main`；OWNER-PACK-001 即本任务（子代理不 commit，交付后由控制器集成）。
- **V6 区间 47 个 commit**：34 个任务交付 commit + 13 个治理/台账/控制器日志 commit（清单见 §2.2）。无 merge 提交；HEAD == `origin/main`。
- **科学/算法/合同**：SCI-OBS/PSFW/P2/P3 复核 + SCI-ADJ 裁决 + 7 份 W3 规格 + QA-MATRIX + CONTRACT-FREEZE（96 条款）。
- **实现**：9 个 W5 IMPL 模块 + 4 个 W6/W7/W8 集成层 + W9 运行时/CLI 模式门 + W10 真实科学与性能验收 + W11 Windows 验证包 + W12 文档收敛 + W13 独立终审。
- **证据**：`reports/v6/**`（review-audit / science-adjudication / contract-review / qa-design / real-science / performance / windows / release-review / final-audit）与 `artifacts/v6/**`。

### 1.2 科学口径落位

- **三生产模式**：Phase2 正式支持 `point_information` / `surface_gls` / `psfsw_robust`（`route_phase2_mode` / `parse_weight_mode` / phase3 模式解析 / psfsw G20 一致）。
- **`psf_snr_power` 保持 DEFERRED**：本包不解冻，保持 NOT_IMPLEMENTED/unavailable；生产枚举不含它，CLI/路由与 phase3 解析均 REJECT（负向 40/40 覆盖）。
- **基线模式**：`equal` / `pixel_ivar`（文档基线，仅比较；legacy 整数 0 被拒绝，1/2 仅基线）。
- **单位表**：冻结单位表在位（`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json#units_table`）；`FZ-UNIT-VAR-IN`=ADU²、`FZ-UNIT-VAR-SB`=ADU²/px⁴、`FZ-UNIT-IVAR-SB`=px⁴/ADU² 等（部分仍 PENDING_OWNER_SIGNOFF / SO-01）。
- **权重词表单一权威**：`contracts/data/v6_weight_vocabulary_v1.json` 为唯一 canonical（`weight.kind/units/group_normalized/normalization.*/weight_value`）；`relative_dimensionless` / `dimensionless_relative` 仅作 reader 别名（W6 归一，DI-01/DI-07 闭合）。
- **psfsw 无量纲、非 ivar/Fisher**：`compute_psfsw_weights` 产出 `W_psfsw=Wt/median(Wt)`，产品 `weight.units="1"`、`group_normalized=true`、`median_target=1.0`；禁止键（ivar/variance/sigma/fisher/w_info…）由 schema 与 `forbidden_psfsw_product_keys()` 双重守卫；`variance_from_weight=false`。
- **`C_out` 仅 `R C_in Rᵀ` + effective PSF**：final covariance 由实际组合系数 α 传播（`αᵀ C_in α`），不从权重标量反推（`method=propagated_from_composite_coefficients`）；effective PSF 由 `conventional_effective_psf` 输出、FWHM 由剖面测得。
- **禁入权重面的诊断量**：`median(SNR_F)`（`snr_frame_coefficient` 已从工作树删除）、support/coverage/FWHM/residual 等别名均禁止进入权重/方差面。
- **C-004.2/C-004.3 裁决已落实**：帧级 `median(SNR_F)` 系数只作诊断/深度表达登记，未进入权重面；词表差异已由 W6 归一。

### 1.3 独立验证与诚实性要点（详见 §9）

- **已独立验证（本包复核）**：冻结合同 96=39/49/8；基线 HEAD 与 commit 链；B-04 零消费者；CI 红线（THREAD-BUDGET/CTEST-REGISTRATION rc=1）；schema 交叉张力（provenance allOf / signal allOf / quantity.units 自由 string / covariance operator_descriptor 枚举）；AIO `%.12g` 小写 e；legacy CAR/AIT 代码；VERSION 未提升与无 RELEASED 宣称；预存 12 tracked 差异；ci-fix 分支与 worktree。
- **子代理自报、控制器复跑（本包转述但不重复复跑）**：35 任务各自的 Oracle/mutation 通过数、`REAL-SCIENCE-001` 的 486/486 Oracle、`PERF-SCALE-001` 的原始样本复算、`CONTRACT-FREEZE-001` 的 2056 checks、`FINAL-AUDIT-001` 的独立 Oracle 17/17 等——均以各任务证据文件为指针，本包未逐条重跑。
- **UNAVAILABLE / UNKNOWN**：Windows 32/32 用例与候选 sha256（UNAVAILABLE）；Windows MSVC 失败根因（UNKNOWN，需 token 取日志）；Fatduck 复验（AWAITING）。

---

## 2. 交付与 commit 链

### 2.1 35 任务状态与集成 commit

> 状态源：`TASK_LEDGER.csv`（任务状态唯一源）。commit 为控制器集成提交（单任务单 commit）；OWNER-PACK-001 为在途。

| # | task | wave | 状态 | 集成 commit | 关键产物 |
|---|---|---|---|---|---|
| 1 | BASE-OWN-001 | 0 | PASS | `4b508f28` | `run/v6/base/` |
| 2 | SCI-OBS-001 | 1 | PASS | `9d99fd71` | `docs/science/v6/observation/` |
| 3 | SCI-PSFW-001 | 1 | PASS | `24610e01` | `docs/science/v6/psfw/PSFW_FREEZE_RESEARCH.md` |
| 4 | SCI-P2-001 | 1 | PASS | `192fab35` | `docs/science/v6/phase2/` |
| 5 | SCI-P3-001 | 1 | PASS | `eac43135` | `docs/science/v6/phase3/` |
| 6 | AUDIT-REVIEW-001 | 1 | PASS | `a09a81f4` | `reports/v6/review-audit/` |
| 7 | SCI-ADJ-001 | 2 | PASS | `db26eec5` | `reports/v6/science-adjudication/adjudications.json` |
| 8 | DATA-DESIGN-001 | 3 | PASS | `2eab1fc1` | `contracts/proposals/v6/data/` |
| 9 | ALG-P1-001 | 3 | PASS | `29747831` | `docs/algorithms/v6/phase1/` |
| 10 | ALG-P2-POINT-001 | 3 | PASS | `28e0ac6c` | `docs/algorithms/v6/phase2-point/` |
| 11 | ALG-P2-PSFSW-001 | 3 | PASS | `1c4e5f92` | `docs/algorithms/v6/phase2-psfsw/` |
| 12 | ALG-P2-SURF-001 | 3 | PASS | `77ce7299` | `docs/algorithms/v6/phase2-surface/` |
| 13 | ALG-P3-001 | 3 | PASS | `ebefe00d` | `docs/algorithms/v6/phase3/` |
| 14 | QA-MATRIX-001 | 3 | PASS | `67f0456f` | `reports/v6/qa-design/qa_matrix.json` |
| 15 | CONTRACT-FREEZE-001 | 4 | PASS | `7a104485` | `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` |
| 16 | IMPL-P1-CAL-001 | 5 | PASS | `6c17e7d6` | `lib/calibration/src/v6_calibration_covariance.cpp` |
| 17 | IMPL-P1-PSFW-001 | 5 | PASS | `078bde5f` | `lib/photometric_calib/cpp/src/psfsw.cpp` |
| 18 | IMPL-P1-DRZ-001 | 5 | PASS | `684a693e` | `lib/healpix_db/healpix_drizzle/v6_drizzle_science.cpp` |
| 19 | IMPL-P2-UPM-001 | 5 | PASS | `f0502b25` | `lib/phase2/src/upm.cpp` |
| 20 | IMPL-P2-REJ-001 | 5 | PASS | `dfc1f8fc` | `lib/phase2/src/rejection.cpp` |
| 21 | IMPL-P2-SAMP-001 | 5 | PASS | `51d5b645` | `lib/phase2/src/sampler.cpp` |
| 22 | IMPL-P3-PROJ-001 | 5 | PASS | `e476b90e` | `lib/phase3_proj/p3_proj_v6.cpp` |
| 23 | IMPL-P3-RSMP-001 | 5 | PASS | `b5bc6af4` | `lib/phase3_rsmp/p3_rsmp_operator.cpp` |
| 24 | IMPL-AIO-001 | 5 | PASS | `e244b284` | `lib/astro_image_io/v6/src/v6_atomic_publish.cpp` |
| 25 | SCHEMA-INTEGRATE-001 | 6 | PASS | `2ac6b758` | `contracts/schemas/v6/*`、`contracts/data/v6_weight_vocabulary_v1.json` |
| 26 | P1-INTEGRATE-001 | 7 | PASS | `960d6051` | `lib/phase1/v6/src/phase1_product.cpp` |
| 27 | P3-INTEGRATE-001 | 7 | PASS | `a689eff2` | `lib/phase3_session/p3_v6_export.cpp` |
| 28 | P2-INTEGRATE-001 | 8 | PASS | `4c0296e0` | `lib/phase2_int/v6/src/phase2_integrate.cpp` |
| 29 | RUNTIME-CI-001 | 9 | PASS | `0d8e98f1` | `cli/v6_runtime_contract.h`、`runtime/v6_budget.py` |
| 30 | REAL-SCIENCE-001 | 10 | PASS | `815f161f` | `reports/v6/real-science/`、`artifacts/v6/real-science/` |
| 31 | PERF-SCALE-001 | 10 | PASS（附 SO-05 升级 owner） | `f8470f4c` | `artifacts/v6/performance/` |
| 32 | WIN-VERIFY-001 | 11 | WAITING_WINDOWS | `296c012f` | `reports/v6/windows/WIN-VERIFY-001.md` |
| 33 | DOC-CONVERGE-001 | 12 | PASS | `46ca7573` | `reports/v6/release-review/` |
| 34 | FINAL-AUDIT-001 | 13 | PASS（总判定 NOT_READY） | `241e9778` | `reports/v6/final-audit/` |
| 35 | OWNER-PACK-001 | 14 | READY → 本任务在途 | （待控制器集成） | 本包 |

### 2.2 V6 区间治理/台账 commit（13）

`bc166e9d`（C-001..C-005 日志/Wave1 收口）、`125bc099`（Wave2 解锁）、`44e1cb65`（F1 W5 局部裁定）、`ac04289d`（F1 W6）、`95703e63`（F1 W7）、`393db3fb`（F1 W9）、`c7432fa0`（F1 构建面）、`3e7fbc44`（AR-033 清账）、`5eb702bd`（W10 台账）、`b7c4f35e`（AR-034 部分清账）、`8f5ef3e9`（C-008）、`8e1e280e`（C-009）、`6b459952`（C-010）。

### 2.3 提交纪律（审计 i-01/i-02 + 本包复核）

- HEAD == `origin/main`（0/0）；V6 区间无 merge 提交。
- 33/33 任务提交改动集 ⊆ 声明 write_scope，0 越界；无 `git add -A` 迹象；12 项预存 tracked 差异始终未提交。
- 审计基线为 `46ca7573`（FINAL-AUDIT-001 自身提交 `241e9778` 之前）；当前 HEAD `6b459952` 在其上仅新增 2 个提交（终审报告 + C-010 日志），`git diff --stat 46ca7573..HEAD` = 15 文件、+1034/−3，无源码/合同/CI 变更（本包复核）。

### 2.4 控制器日志 C-001..C-010 逐条转述（如实转述，不改写事实）

| 条目 | 摘要（转述） |
|---|---|
| **C-001 基线集成** | `ce5b3a00`：负责人已批准设计（PROJECT_SPEC / 三份 Phase 详细设计 / UNIFIED_SCIENCE_MODEL / PSF_SIGNAL_WEIGHT / 参考文献档案）+ 本包基线（00_READ_FIRST/RULINGS/EXECUTION_GRAPH/TASK_MANIFEST/TASK_LEDGER + 35 任务卡）；ACTIVITY_STATE 唯一 ACTIVE=V6；边界：未触碰在途工作树回退（P33/P35/P36 相关 16 回退 + 10 删除）、审核线文档与 artifacts。 |
| **C-002 Wave 0 集成** | `4b508f28`：BASE-OWN-001 PASS（控制器复跑 build rc=0、verify 19/19 rc=0、mut1/mut2 rc=2、tracked 树零变更）；现场：dirty `-unormal`=173 / `-uall`=868（27 tracked-M + 10 tracked-D + 831 untracked）；其中 16 tracked 回退到 11 个祖先提交 blob，10 tracked 删除等于其引入提交父状态。 |
| **C-003 Wave 1 集成** | 5/5 PASS、单任务单 commit：SCI-P2-001 `192fab35`（Oracle 32/32；17 门 + 3 自我 mutation 全红）、AUDIT-REVIEW-001 `a09a81f4`（19/19；8/8 expected-red）、SCI-OBS-001 `9d99fd71`（15/15；13 mutation rc=2）、SCI-P3-001 `eac43135`（11/11；22 mutation 检出）、SCI-PSFW-001 `24610e01`（76 check + 12/12 CAUGHT；w_info 13/13；doc 10/10）。 |
| **C-004 控制器裁决（6 条）** | ① 本包只实现 `point_information/surface_gls/psfsw_robust`，`psf_snr_power` **不解冻**、不进生产路由；② 帧级 `median(SNR_F)` 系数只作诊断/深度表达，禁止进入任何权重面；③ 词表差异由 W6 归一为单一 schema；④ 构建面 owner 缺口（AR-033/H-1）由控制器在 W5 后以独立集成提交统一处理（归 W9）；⑤ 上位规范写保护（Wave1–4 只写 `docs/**/v6/**`、`contracts/proposals/v6/**`、`reports/v6/**`）；⑥ F1 基线分歧（工作树≠HEAD）须在 W5 派发前裁定。 |
| **C-005 转入 Wave 2** | SCI-ADJ-001 置 READY，输入 = 五个 W1 交付 + C-004 裁决 + F-OBS-01..05 / F3-01..06 / AR-032..036 / S1–S4。 |
| **C-006 Wave 3/4 集成与 F1 局部裁定** | Wave 3 七项全 PASS（ALG-P2-PSFSW `1c4e5f92`、ALG-P2-POINT `28e0ac6c`、DATA-DESIGN `2eab1fc1`、ALG-P2-SURF `77ce7299`、ALG-P1 `29747831`、QA-MATRIX `67f0456f`、ALG-P3 `ebefe00d`）；Wave 4 CONTRACT-FREEZE `7a104485` PASS：96 条款（FROZEN 39 / PENDING 49 / OPEN 8）、Oracle 2056 checks、mutation 23/23、SO-01..07 待签。F1 局部化裁定（取代 C-004.6 的"整体固化"）：工作树 37 tracked 差异先于本包；仅 IMPL-P1-DRZ-001 命中 3 文件，故按波次/按写域局部裁定；本次固化 drizzle 3 文件，其余 34 差异保持不动。 |
| **C-007 Wave 5 集成** | Wave 5 九项全 PASS（IMPL-P1-PSFW `078bde5f`、P1-CAL `6c17e7d6`、P2-REJ `dfc1f8fc`、P3-PROJ `e476b90e`、P1-DRZ `684a693e`、P2-SAMP `51d5b645`、P2-UPM `f0502b25`、AIO `e244b284`、P3-RSMP `b5bc6af4`）；Wave 6 F1 局部裁定（`docs/contracts/DATA_SEMANTICS.md`、`PUBLIC_API.md` 两处回退）；**误报更正留痕**（IMPL-P2-REJ 报"upm.cpp 被改坏"实为缺 `-I` 路径，`upm.cpp` 完好）；F-CAR/F-AIT 高优先科学发现；AR-033 构建面欠账待 W9。 |
| **C-008 Wave 6–10 集成、AR-033 清账与负责人裁决项** | 集成链：W6 `2ac6b758`、W7 P3 `a689eff2`/P1 `960d6051`、W8 P2 `4c0296e0`、W9 `0d8e98f1`、W10 REAL `815f161f`/PERF `f8470f4c`；F1 局部裁定 `ac04289d`/`95703e63`/`393db3fb`/`c7432fa0`；AR-033 清账 `3e7fbc44`（根图注册 4 个 V6 生产库 + 13 个 add_subdirectory；79 个 V6 ctest；AR-033 关闭）；7 项负责人裁决项（SO-05、schema 交叉张力、quantity.units、AIO FITS、F-CAR/F-AIT、Phase2 CLI 真实数据不可达、k_corr 豁免）；诚实性登记（DOCUMENTED_BASELINE、49/8 fail-closed、Fatduck 不可达、余 12 tracked 差异、包 NOT_READY）。 |
| **C-009 Wave 11 结论与 CI 红线** | WIN-VERIFY-001 = AWAITING_WINDOWS_VALIDATION（Fatduck ssh rc=255 / tcp rc=124 / ping 100% loss / tailscale offline；Windows CI run 35012779853 failure 无候选；32/32 UNAVAILABLE；FD-F-003 OPEN）；AR-034 部分清账 `b7c4f35e`（CTEST-REGISTRATION unregistered 3→2）；控制器独立复核 CI：`THREAD-BUDGET` FAIL（`module_adapters.cpp:245,252`，P36 回退态）、`CTEST-REGISTRATION` FAIL（2 项非 V6 IPV）、`AGENTS-GOV`/`VERSION-CONSISTENCY`/`KNOWN-FAILURES-BASELINE-VERIFY` PASS；结论：§14.5「GitHub CI 通过」未满足，包不得宣布完成，最终 NOT_READY。 |
| **C-010 FINAL-AUDIT 受理与包级覆盖缺口** | 独立审计结论：包总判定 **NOT_READY**（BLOCKER 5 / MAJOR 5 / MINOR 5 / INFO 8）；自建根图、自跑 `ctest -R v6_`（79/79、无 skip）、自写 numpy Oracle 17/17、抽查 33 提交 ⊆ write_scope（0 越界）、独立复现 CI 红与 Fatduck 不可达。**B-04 新 BLOCKER**：V6 三 Phase 生产入口零消费者（external_refs=0；CLI 过 mode_gate 仍走 legacy）→ 登记 **AR-037（新）**，建议后续控制包独立任务接线，接线前生产可达性不成立。M-01..M-05 处置；**更正 C-008**：全量根 ctest 实跑 **405/407**，2 项为真实失败（`cpu007_profile_store` 稳定 FAIL（CWD 依赖）、`p1hips_performance` 抖动）。12 项裁决项最终汇总；包状态 **NOT_READY / NOT_RELEASED**；§14.5 六步 = NOT_MET / NOT_MET / NOT_MET / NOT_MET / AWAITING / NOT_MET。 |

---

## 3. 科学口径（三生产模式 / 基线 / DEFERRED / 单位 / 权重 / covariance）

见 §1.2。补充权威锚：

| 口径 | 权威 | 证据 |
|---|---|---|
| `point_information` | `Q=aPᵀC⁻¹d`、`W=a²PᵀC⁻¹P`、`F̂=ΣQ/ΣW`、`Var=1/ΣW` | `lib/snr_estimator/cpp/src/information_weight.cpp`；独立 Oracle 17/17（FINAL-AUDIT-001） |
| `surface_gls` | `(AᵀC⁻¹A)x=AᵀC⁻¹d`、`Cov=(AᵀC⁻¹A)⁻¹` | ALG-P2-SURF-001；`lib/phase2_int/v6/src/phase2_integrate.cpp` |
| `psfsw_robust` | `Wt=C_norm·S^α·Conc^β/(N^γ·B^δ)`、`W_psfsw=Wt/median(Wt)` | `lib/photometric_calib/cpp/src/psfsw.cpp`；`docs/science/v6/psfw/` |
| Drizzle | `S_p=Σ B_j a_jp/Σ a_jp`、`B_j=x_j/A_pixel,j`、`w_SB=a/A_pixel`、`variance_p=Σ v_j w_jp²/D_p²` | ALG-P1-001；`v6_drizzle_science.cpp` |
| `C_out` | 仅 `R C_in Rᵀ`（α 为实际组合系数）+ effective PSF | `propagate_covariance`；REAL-SCIENCE Oracle 486/486 |
| 词表 | 单一 canonical `contracts/data/v6_weight_vocabulary_v1.json` | SCHEMA-INTEGRATE-001（W6） |
| `psf_snr_power` | DEFERRED / NOT_IMPLEMENTED | C-004.1；各路由 REJECT |

**未冻/待签**：`AR-036/SO-05`（资源门记录 vs 判决）、`SO-01..SO-07` 全部保持 PENDING_OWNER_SIGNOFF（49 条款）、8 条款级 OPEN（真实数据/预注册门未执行）。**本包未解冻任何冻结公式/容差/门。**

---

## 4. §14.5 六步逐项判定

> 判定基于基线 `6b459952`（本包实测）与各证据文件；**无一步 PASS**。

| # | §14.5 步骤 | 判定 | 证据 |
|---|---|---|---|
| 1 | 任务提交全部完成 | **NOT_MET** | `TASK_LEDGER.csv`：`WIN-VERIFY-001 = WAITING_WINDOWS`（非 PASS）、`OWNER-PACK-001`（本任务在途）；另有 `reports/v6/final-audit` 指出的状态语义瑕疵（PERF-SCALE-001 PASS vs `REVIEW_REQUIRED`，M-05） |
| 2 | GitHub CI 通过 | **NOT_MET** | 本包 `artifacts/v6/owner-package/logs/10_ci_redline_recheck.log`：`THREAD-BUDGET` rc=1、`CTEST-REGISTRATION` rc=1（`AGENTS-GOV`/`VERSION-CONSISTENCY` rc=0）；Windows CI run 35012779853 failure 无候选；Linux V6 线自 Wave 3 起持续红 |
| 3 | Linux 最终 SHA 真实数据流终验 | **NOT_MET** | `REAL-SCIENCE-001` 为库级五口径（equal/exposure/ivar=DOCUMENTED_BASELINE，仅 W_info/PSFSW 走冻结库函数）；Phase2 CLI obs=0 fail-closed；M42/银心马赛克与 `write_phase1_product→run_*` 未跑；`G-RD-01/02` OPEN |
| 4 | Agent 图像初审 | **NOT_MET** | `reports/v6/` 无 §13.4 预览图（signal/coverage/接缝/rejection）与结构化初审结论；无可疑区坐标+指标 |
| 5 | Windows/Fatduck 复验 | **AWAITING** | `WIN-VERIFY-001` = AWAITING_WINDOWS_VALIDATION；Fatduck 不可达；32/32 UNAVAILABLE；`FD-F-003` OPEN。§15.3 允许标 AWAITING 且不阻塞 Linux，**但不得据此发布** |
| 6 | 汇总和打包 | **NOT_MET** | 本任务（OWNER-PACK-001）产出的是**审核包**且状态 NOT_READY；完整发布包与真实数据/Windows 证据不具备，不满足"汇总和打包"完成条件 |

汇总：**PASS 0 / NOT_MET 5 / AWAITING 1**（与 `artifacts/v6/release-review/release_review_verdict.json#section_14_5_summary` 一致）。

---

## 5. BLOCKER / MAJOR / MINOR 转述（来自 FINAL-AUDIT-001）

> 全部逐条转述自 `reports/v6/final-audit/findings.json`；"控制器复核"列指 C-010 是否已受理/复核。

### 5.1 BLOCKER（5）

| id | 摘述（转述） | V6 归属 | 控制器复核 |
|---|---|---|---|
| **B-01** | GitHub Linux CI 在基线 SHA 为红（§14.5 第 2 步 / §17.12 门 7）：本机复现 `THREAD-BUDGET` FAIL、`CTEST-REGISTRATION` FAIL，整体 rc=1 | RUNTIME-CI-001 / 控制器（P36 回退归属需 owner） | 是（C-009/C-010） |
| **B-02** | §13.3 最终 SHA 真实数据流终验未执行：无全 `testdata`/Gaia 数据流、无 M42/银心 Phase1→Phase2 马赛克、无 §13.4 图像初审（门 8/9 未满足） | REAL-SCIENCE-001（范围缺口） | 是（C-010） |
| **B-03** | Windows/Fatduck 正式复验未完成：Fatduck 不可达（ssh rc=255/tcp rc=124/tailscale offline）；Windows CI 无候选，且证据 SHA=8f5ef3e9 ≠ 审计基线 | WIN-VERIFY-001 | 是（C-009/C-010） |
| **B-04** | V6 三 Phase 生产入口在 CLI/生产中无消费者，`--mode point_information|psfsw_robust` 仍走 legacy 集成路径（§16.2/§14.3） | P1/P2/P3-INTEGRATE-001 + RUNTIME-CI-001（接线缺口 AR-037 新） | 是（C-010 独立复核 external_refs=0） |
| **B-05** | §10.5/§17.6 资源门未达且 fail-open：16w CPU 均值 65.09%（门 85%）、p50 87.6%（门 90%）、内存增长 34.01 MB/s ≥ 32、全预算 `alloc_reclaim_missing`；仅以 `record_only_pending_owner_signoff` 放行 | PERF-SCALE-001 / SO-05 | 是（C-007/C-008/C-010） |

### 5.2 MAJOR（5）摘要

| id | 摘要 | 归属 |
|---|---|---|
| M-01 | 8 条 OPEN 数值门未执行；49 条 PENDING 阈值被生产代码以未签字值硬门执行 | CONTRACT-FREEZE-001 / QA-MATRIX-001 / REAL-SCIENCE-001 |
| M-02 | DOC-CONVERGE-001 在无 owner 签字下修改被登记为 FROZEN 的规范正文（ALG-P1-001 §4.2 concentration 单位） | DOC-CONVERGE-001 |
| M-03 | V6 基线 F1 局部裁定提交固化 P33/P35/P36 回退，删除 9 项 CI 检查登记与 8 个测试目标，缺 owner 授权 | 控制器 / CTRL-F1 |
| M-04 | QA 矩阵 W10 真实数据/预注册门未执行且 `case_ledger` 未更新 | QA-MATRIX-001 + REAL-SCIENCE-001 |
| M-05 | 台账 PERF-SCALE-001 记 PASS，但其交付物 `recommended_status=REVIEW_REQUIRED` 且 SO-05 升级 owner | PERF-SCALE-001 |

### 5.3 MINOR（5）与 INFO（8）指针

- m-01 台账 BASE-OWN-001 证据路径 `21_verify.log` 不存在（实际 `21_verify_inventory.log`）。
- m-02 根全量 ctest 非全绿：`cpu007_profile_store` 稳定 FAIL（CWD 依赖）+ `p1hips_performance` 抖动；**更正 C-008 的"Not Run"表述**。
- m-03 phase2 psfsw 内存路径 `variance_from_weight` 守卫为死代码（磁盘记录路径有效）。
- m-04 PSFW 冻结核研究文档 concentration 单位文本不一致（`docs/science/v6/psfw/PSFW_FREEZE_RESEARCH.md:144`）。
- m-05 预存 `ci-fix` 分支 + worktree（本包实测 44 行含 main；审计称 45 个注册 worktree），违反 §14.1，非本包产物。
- i-01..i-08：提交纪律 / 写域 33-0 越界 / 79 V6 ctest 全过 / 12 预存差异无关 / 发布口径诚实 / 96 计数可复现 / Windows 根因 UNKNOWN / 关键式独立复算 17/17。**全文指针：`reports/v6/final-audit/findings.json`、`reports/v6/final-audit/FINAL-AUDIT-001_REPORT.md` §9。**

**完整性自检**：BLOCKER 5/5、MAJOR 5/5、MINOR 5/5、INFO 8/8 均已转述/登记；合计 23 条与 `findings.json#totals` 一致。

---

## 6. 负责人确认/裁决项（12 项）

> 机读结构化版本：`artifacts/v6/owner-package/owner_decisions.json`（每项含 `facts` / `evidence_pointers` / `decision_needed` / `options` / `default_if_no_decision`）。以下为正文转述，事实以 C-008/C-009/C-010 与各报告为准，未改写。

### OD-01 SO-05 资源门裁决
- **事实**：16w CPU 均值 65.09%（门 85%）、p50 87.63%（门 90%）、`memory_growth_unbounded@16w`（34.01 MB/s ≥ 32，未定性）、全预算 `alloc_reclaim_missing`。当前按 C-007 记 `record_only_pending_owner_signoff`（`hard_fail=false`、`auto_adjudication_withheld_pending_owner_signoff`）。
- **证据指针**：`artifacts/v6/performance/p1_real_ldn43_4k_resource_gate_record.json`、`reports/v6/performance/PERF-SCALE-001.md` §5/§8、`findings.json#B-05`。
- **需要决定**：是否启用自动判决；16w 低利用率是否判失败；内存门如何定性。
- **可选处置及影响**：A 不启用（门保持未满足，包 NOT_READY）；B 启用并判失败（需修复后重跑）；C 按 §10.5 例外修合同（须负责人修改合同）。
- **不裁决默认后果**：保持 record-only；资源门按未满足处理；不得宣称门通过。

### OD-02 AR-037 V6 三 Phase 生产入口无 CLI/生产消费者（最高优先之一）
- **事实**：六个 V6 入口在生产零消费者；`cmd_phase2_run` 过 `mode_gate` 后仍调 `run_with_resource_gate` 走 legacy。TASK_MANIFEST 无任务拥有该接线。
- **证据指针**：`findings.json#B-04`、`CONTROLLER_LOG.md` C-010、`cli/commands.cpp`。
- **需要决定**：是否另立后续任务接线（并定 owner/write_scope/验收）。
- **可选处置及影响**：A 另立后续任务（推荐）；B 声明库层交付并在 product manifest 标三 Phase unavailable；C 暂不处置。
- **不裁决默认后果**：B-04 持续 BLOCKER；生产可达性不成立；不得对外称 V6 口径已可用。

### OD-03 M-01 PENDING 阈值授权
- **事实**：49 条 PENDING（eps 0.05/0.20、corr_ratio 1.05、N_min 3、k_corr 1.4 等）已被生产代码当硬门执行但未签字；8 条 OPEN 未执行。
- **证据指针**：`findings.json#M-01`、冻结合同 `#clauses`、`reports/v6/contract-review/04_OPEN_ITEMS_AND_SIGNOFF.md` §2/§3、`case_ledger.json`。
- **需要决定**：临时生效还是必须先签字。
- **可选处置及影响**：A 逐条签字后生效（推荐）；B 授权临时生效（须注明范围/失效条件）；C 暂不处置。
- **不裁决默认后果**：阈值保持未授权；不得宣称 PENDING 门已获授权。

### OD-04 M-02 FROZEN 文档订正确认（ADU/px → ADU/px²）
- **事实**：DOC-CONVERGE-001 改 ALG-P1-001 §4.2 单位标签，与冻结单位表/schema/代码一致，非公式/容差改动；schema 原文要求签字。
- **证据指针**：`findings.json#M-02`/`#m-04`、`reports/v6/release-review/01_CONVERGENCE_CORRECTIONS.md`、`release_review_verdict.json#concentration_fix`。
- **需要决定**：确认订正；或对 3 文件做单 commit 回退（ALG_P1_001_*.md / alg_p1_001_spec.json / verify_alg_p1_001.py），并把 doc-convergence C4 改为 pending_signoff。
- **可选处置及影响**：A 确认（推荐，需同步 m-04）；B 回退（C4 转红，须改 pending_signoff）；C 暂不处置（M-02 持续 MAJOR）。
- **不裁决默认后果**：M-02 保持 MAJOR；不得声称订正已获授权。

### OD-05 M-03 F1 回退基线逐项授权
- **事实**：F1 治理提交固化 P33/P35/P36 预存回退（删除 9 项 CI 检查登记 + 8 测试目标 + `cli/frame_admission.h`）；内容源自开工前既有工作树，删除清单未获逐项授权；原实现仍在历史可恢复。
- **证据指针**：`findings.json#M-03`、C-006/C-007/C-008、`git show --stat 393db3fb c7432fa0 95703e63 44e1cb65 ac04289d`。
- **需要决定**：逐项确认保持该基线，或指示恢复。
- **可选处置及影响**：A 确认保持（削减转为已授权，需登记）；B 指示恢复（须重跑 CI）；C 部分确认（逐项清单）。
- **不裁决默认后果**：M-03 保持 MAJOR；基线维持现状但不得视为已授权。

### OD-06 schema 交叉张力（FZ-UNIT-FLUX vs FZ-BUNIT-SEMANTICS）
- **事实**：provenance.v1 allOf（bunit=ADU ⇒ surface_brightness ∧ pixel_area_power=−2）与 signal.v1（integrated_flux ⇒ pixel_area_power=0）使**纯积分通量主面不可表达**；AIO `bunit_dimension_decidable` 亦只接受 SB-ADU。
- **证据指针**：`contracts/schemas/v6/astrocs.v6.provenance.v1.schema.json#allOf`、`astrocs.v6.signal.v1.schema.json#allOf`、C-008 §2。
- **需要决定**：FZ-UNIT-FLUX 与 FZ-BUNIT-SEMANTICS 的适用关系（是否修 schema/gate）。
- **可选处置及影响**：A 修订以允许积分通量主面（触 contracts/**，须独立 commit+oracle）；B 维持现状并记为设计约束；C 暂不处置。
- **不裁决默认后果**：维持现状；纯积分通量主面不可表达；不得声称单位面已收敛。

### OD-07 quantity.units 未收敛 + covariance operator_descriptor 枚举缺项
- **事实**：`quantity.units` 在生产 schema 中为自由 string（篡改 `W_info.units` 仍可过 schema，C++ 重开门已补齐）；covariance.v1 对角 representation 强制 `operator_descriptor`，而 kind 枚举无「校准 Jacobian」。
- **证据指针**：`astrocs.v6.point-information.v1.schema.json#/$defs/quantity`、`astrocs.v6.covariance.v1.schema.json#allOf`/`#operator_descriptor`、C-008 §3。
- **需要决定**：是否收紧 units 为 const/enum；是否补「校准 Jacobian」枚举。
- **可选处置及影响**：A 收紧并补枚举（触 contracts/**）；B 维持并记录；C 暂不处置。
- **不裁决默认后果**：维持现状；schema 层未收紧；不得声称 units 已被 schema 约束。

### OD-08 AIO FITS 实数格式（小写 e）
- **事实**：`FitsCard::make_real` 用 `%.12g` 产生小写 e，astropy 判 not FITS standard；P1/P3 在产物层规范为大写 E 绕过；`lib/astro_image_io/v6` 本身待修。
- **证据指针**：`lib/astro_image_io/v6/src/v6_fits.cpp:53`、C-008 §4。
- **需要决定**：是否授权修正及归属任务。
- **可选处置及影响**：A 授权修正（推荐）；B 维持产物层绕过；C 暂不处置。
- **不裁决默认后果**：维持现状；AIO 本身仍待修；不得声称 AIO FITS 输出完全符合作者标准。

### OD-09 F-CAR / F-AIT legacy 投影错误修正授权
- **事实**：legacy `p3_projection.cpp` CAR 赤纬反号（偏差 96°）、AIT 缺 Paper II √2（残差 13.05 px）；V6 层正确；legacy 文件与 `tests/unit/p3_projection_test.cpp` 不属任何 V6 写域。
- **证据指针**：C-007/C-008、`lib/phase3_proj/p3_projection.cpp:190-260`、`reports/v6/release-review/04_RELEASE_REVIEW_SECTION14_5.md` §6.3。
- **需要决定**：是否授权修正 legacy + 同步测试。
- **可选处置及影响**：A 授权修正（独立 commit + WCSLIB 对拍）；B 不改 legacy（须确认 legacy 是否仍在生产路由）；C 暂不处置。
- **不裁决默认后果**：legacy 保持现状；不得声称 legacy 投影与 Paper II 一致。

### OD-10 真实数据终验与 QA 门（B-02 / M-04）
- **事实**：§13.3 最终 SHA 真实数据流终验未执行；`equal/exposure/ivar` 为 DOCUMENTED_BASELINE；Phase2 CLI 真实数据 obs=0 不可达；QA W10 `G-RD-01/02`、`G-BASE-03` executed_cases=0 且 `case_ledger` 未回写。
- **证据指针**：`findings.json#B-02`/`#M-04`、`reports/v6/real-science/REAL-SCIENCE-001_REPORT.md` §3/§9/§13、`reports/v6/qa-design/case_ledger.json`。
- **需要决定**：如何补齐真实数据终验与图像初审/QA 门；或明确降级并登记。
- **可选处置及影响**：A 在最终 SHA 执行全量终验+图像初审并回写台账；B 由负责人明确降级并登记；C 暂不处置。
- **不裁决默认后果**：B-02 保持 BLOCKER、M-04 保持 MAJOR；§14.5 第 3/4 步保持 NOT_MET。

### OD-11 CI 红线归属（B-01）
- **事实**：本包独立复跑 `THREAD-BUDGET` rc=1（`module_adapters.cpp:245,252`，P36 回退态）、`CTEST-REGISTRATION` rc=1（2 项非 V6 IPV）、`AGENTS-GOV`/`VERSION-CONSISTENCY` rc=0；全量 ctest 405/407（1 稳定 FAIL + 1 抖动）；Windows MSVC 失败根因 UNKNOWN。
- **证据指针**：`artifacts/v6/owner-package/logs/10_ci_redline_recheck.log`、`findings.json#B-01`、`reports/v6/final-audit/evidence/ci_repro.log`、`reports/v6/windows/WIN-VERIFY-001.md` §4。
- **需要决定**：是否授权修正 THREAD-BUDGET（涉 M-03 基线）、2 项 IPV 残项归属、Windows 根因取证与修复归属。
- **可选处置及影响**：A 授权修正并指定 owner；B 仅按 R-05/R-13 登记 waiver（verdict 仍 FAIL）；C 暂不处置。
- **不裁决默认后果**：Linux CI 保持红；B-01 保持 BLOCKER；§14.5 第 2 步保持 NOT_MET。

### OD-12 其他 P0/边界
- **事实**：Windows MSVC 根因 UNKNOWN（需 token 取 `win-stage-*.log`）与 `FD-F-003`（Windows 单测门长期 0 用例 PASS）；AR-032/SO-06 非 v6 SCI 正文取代受 C-004.5 写保护未改写；`memory.md` 仍为 DOC-CONV-001 期（327b6c30，BASE=da3c4b4a）；预存 `ci-fix` 分支（69ec459f，2026-09-08）与 worktree 违反 §14.1，非本包产物。
- **证据指针**：`findings.json#i-07`/`#m-05`、`reports/v6/windows/WIN-VERIFY-001.md` §6、`reports/v6/release-review/03_PENDING_AND_OWNER_ITEMS.md` §6、本包 `git branch -a`/`git worktree list` 复核。
- **需要决定**：token 取证与 FD-F-003 修复；AR-032/SO-06 amendment 授权；memory.md 是否纳入本包；是否批准清理分支/worktree。
- **可选处置及影响**：A 逐项处置并指定 owner/后续任务；B 仅登记不处置；C 暂不处置。
- **不裁决默认后果**：各项保持 OPEN；Windows 保持 AWAITING；memory.md 不纳入本包（保持陈旧）；不清理分支/worktree。

---

## 7. 证据索引（关键产物 + sha256）

> sha256 由本包用 `sha256sum` 于基线 `6b459952` 实测计算，见 `artifacts/v6/owner-package/evidence_index.json`。**不得编造：以下值均可一键复核。**

| 路径 | sha256 | 说明 |
|---|---|---|
| `reports/v6/final-audit/FINAL-AUDIT-001_REPORT.md` | `a0ca74094799fdb4a00d759ec27754922a0d4d23380326906967cb434c805b4b` | 终审全文（NOT_READY） |
| `reports/v6/final-audit/findings.json` | `366a643dac35b1951e12d4a99812b5dd9a4b16aeb4d0104ac183f88b3ce81ff7` | 23 条 findings 机读源（B/M/m/i） |
| `reports/v6/final-audit/findings.csv` | `b6d26efe7983684b8715724c1c74a2a5f0e0b9c2194e44b05b3945e85172bf34` | findings 表 |
| `reports/v6/final-audit/evidence/independent_oracle.py` | `a2be5d24e92259daca50822ecf5d17ba7c31efcf4e86478962692384b9b3a96a` | 独立 Oracle（17/17） |
| `reports/v6/release-review/04_RELEASE_REVIEW_SECTION14_5.md` | `dc6fd8b8e7d717b7e880555fdb4a622a94c9cee76a364afe14343c5e6ab51f12` | §14.5 发布复核 |
| `artifacts/v6/release-review/release_review_verdict.json` | `6d13d6d107520a177c38a81cb5830e42a02fb1a1d673bacdad84a21e933f5766` | 机读发布判定 |
| `reports/v6/performance/PERF-SCALE-001.md` | `484e438f5f687a62a1bd42360d76f5bf0490b51312fd56f7046581137e4c1308` | 性能验收（SO-05） |
| `artifacts/v6/performance/p1_real_ldn43_4k_resource_gate_record.json` | `e235fbc0ce066ae4c229a42f93faf002ba07da38aca65de6bb9288078bb9a0ed` | 资源门记录（record_only） |
| `artifacts/v6/performance/PERF-SCALE-001_summary.json` | `3046de8f9471db016e9067f812759c011e826330af14655ca3e999584b7754b6` | 性能汇总（REVIEW_REQUIRED） |
| `reports/v6/real-science/REAL-SCIENCE-001_REPORT.md` | `0d28e4141e09ccef186f6a114ba39c0c6e439e431cd6431024902265a642eda0` | 真实科学验收报告 |
| `artifacts/v6/real-science/five_mode_measurements.json` | `5e4118417d03b22f17e29bd9577899bd1eedb87774f7d8235a291294c5887481` | 五口径实测 |
| `artifacts/v6/real-science/oracle_results.json` | `a7e5dc7394535f2bfdaed9e30fd9bf0244df4f854664c1dd4e74690f7ac58609` | 486/486 Oracle |
| `reports/v6/windows/WIN-VERIFY-001.md` | `bef6a942369d7b0f4b96a3b445fa9afeafc2bb97f488ed3fae43a3735a2712b4` | Windows 验证包（AWAITING） |
| `artifacts/v6/windows/FATDUCK_REACHABILITY.json` | `556312822ddb9d251161b65c03222d62e4784a33c656c68fda9f5c889e034c98` | Fatduck 不可达证据 |
| `artifacts/v6/windows/GITHUB_ACTIONS_EVIDENCE.json` | `ce91c08fa3d088a3742348ab1ca92e75cb128c52b6f214c349d91f34d1299125` | Windows CI failure 证据 |
| `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` | `efc4f258bc407e0054171f95d4bf272611334128badc1caf517c75bf56d3369c` | 96 条款冻结（39/49/8） |
| `reports/v6/contract-review/01_FREEZE_STATUS_MATRIX.md` | `4e0e9c76361a77d66fc3ef460a30b76b44ddf6ea0bef21ea7ec691b225d2262e` | 冻结状态矩阵 |
| `reports/v6/contract-review/04_OPEN_ITEMS_AND_SIGNOFF.md` | `b88fd326cecd12dfa2f6d511bf8b7830fc9f1016cb0b8b2d3d113e3a014be6a6` | 开放项/签字清单 |
| `reports/v6/qa-design/case_ledger.json` | `578d978ff022d501298207b769d2923dc3ebbf041a08b42401c4a82f7418ffed` | QA 台账（W10 门未执行） |
| `reports/v6/qa-design/qa_matrix.json` | `a21f9c2c76b5ce38005ffaa026b1d78d92dc8ff200a1a3f780285afc9f1df1ba` | QA 矩阵 |
| `reports/v6/science-adjudication/adjudications.json` | `bd9f63c0db7e153db291b460ca9a73c9338496688e875ef0c55760a547866867` | 科学裁决机读源 |
| `reports/v6/review-audit/00_导读.md` | `b9455f38ac93f834399506a3caef7815de395efcc45ada688d5df58b9513b503` | 审核导读 |
| `工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/TASK_LEDGER.csv` | `25de4561acad350a6a4abee2a3d84f4ed099c6008369e12cf0670902d54c46ae` | 任务状态唯一源 |
| `工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/CONTROLLER_LOG.md` | `e39d3c665a2571cb06b29886f664995a8287f32699bb0a27f6a69db21322dfe4` | C-001..C-010 |
| `contracts/schemas/v6/astrocs.v6.provenance.v1.schema.json` | `254a983b5cc3c232a6549b04ab4a16b9b6b71d16be0bdcaebcd2f781202d5de3` | allOf 单位张力 |
| `contracts/schemas/v6/astrocs.v6.signal.v1.schema.json` | `23e014f4fa03c81481a425002a670ec7aa9c894f1f0ba14821cab62185d9b73e` | integrated_flux/allOf |
| `contracts/schemas/v6/astrocs.v6.covariance.v1.schema.json` | `b6bad1a2e1fa85615fa13f385670d99782d93f62c389fb3cebc356050e4dd8e9` | operator_descriptor 枚举 |
| `contracts/schemas/v6/astrocs.v6.point-information.v1.schema.json` | `d68091737350e1a0d1cc9d54b3baf15fe52040a7d4d860effa25eea5e3d7e10f` | quantity.units 自由 string |
| `lib/astro_image_io/v6/src/v6_fits.cpp` | `b24ebf4759d02ff64417def9385ae526225da5ee6035067b6432e74af00a66b1` | `%.12g` 小写 e |
| `lib/phase3_proj/p3_projection.cpp` | `6fdcba8d3dfceba4f07881c50bb0c1d19247d7f05f73b88400bdb985f2a25203` | legacy CAR/AIT |
| `VERSION` | `c42c9d58f54f29085af17d9f96fb3caf922a9abdac2f02dd10654174a5309a55` | `0.11.0-alpha.2`（未提升） |

本包自身产出清单与 sha256：`artifacts/v6/owner-package/OWNER_PACKAGE_MANIFEST.json`。

---

## 8. 一键复核命令清单

```bash
cd "/workspace/Astro CS Database"

# 0) 基线
git rev-parse HEAD                    # 6b459952...；git status -sb 应见 main...origin/main

# 1) CI 红线（本包独立复跑；rc=1 为红）
timeout 60  python3 tools/arch/check_thread_budget.py                                   # rc=1
timeout 120 python3 tools/quality/check_ctest_registration.py --output /tmp/ci/ctest_registration.json  # rc=1
timeout 60  python3 tools/check_agents_gov.py                                           # rc=0
timeout 60  python3 ci/check_version.py --expected 0.11.0-alpha.2                       # rc=0

# 2) 冻结合同计数（96 = 39/49/8）
python3 - <<'PY'
import json,collections
d=json.load(open('docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json'))
print(dict(collections.Counter(c['status'] for c in d['clauses'])), len(d['clauses']))
PY

# 3) 发布口径（不得出现正面 RELEASED；VERSION 未提升）
cat VERSION                                            # 0.11.0-alpha.2
grep -rIn "RELEASED" README.md REVIEW.md CHANGELOG.md docs/owner/ 2>/dev/null   # 仅 NOT_RELEASED

# 4) B-04 生产零消费者（排除 tests/run/build 后应无外部引用）
grep -rIn --include=*.cpp --include=*.h -E "write_phase1_product|run_point_information|run_surface_gls|run_psfsw_robust|p3_v6_export" . \
  | grep -v "^./tests/" | grep -v "^./run/" | grep -v "^./build/" | grep -v "^./out/"

# 5) 资源门原始记录（hard_fail=false；findings 全 would_fail_if_signed=true）
python3 -c "import json;d=json.load(open('artifacts/v6/performance/p1_real_ldn43_4k_resource_gate_record.json'));print(d['status'],d['hard_fail'],len(d['findings']))"

# 6) findings 计数（5/5/5/8）
python3 -c "import json;print(json.load(open('reports/v6/final-audit/findings.json'))['totals'])"

# 7) 本包负向检查（篡改 → 必红）
timeout 120 python3 artifacts/v6/owner-package/checks/verify_owner_package.py --self-test

# 8) 关键合成验证（独立子代理/审计脚本）
timeout 120 python3 reports/v6/final-audit/evidence/independent_oracle.py    # TOTAL FAILURES: 0
```

---

## 9. 诚实性声明

### 9.1 本包【已独立验证】（在基线 `6b459952` 实测）

1. 基线 HEAD = `6b45995231cb873e6b33a3817353d266633f6d9f`，等于 `origin/main`；V6 区间 47 commit、无 merge。
2. 冻结合同 96 条款 = FROZEN 39 / PENDING_OWNER_SIGNOFF 49 / OPEN 8（解析 JSON）。
3. CI 红线：`THREAD-BUDGET` rc=1（`module_adapters.cpp:245,252`）、`CTEST-REGISTRATION` rc=1（`ipv_dead_params_lock`/`ipv_dead_params_lock_selfcheck`）、`AGENTS-GOV`/`VERSION-CONSISTENCY` rc=0（日志 `artifacts/v6/owner-package/logs/10_ci_redline_recheck.log`）。
4. B-04：六入口在生产（排除 tests/run/build/out）零消费者；`cmd_phase2_run` 在 `mode_gate` 后调 `run_with_resource_gate`。
5. schema 事实：provenance.v1 allOf、signal.v1 allOf、`quantity.units` 自由 string、covariance 对角强制 `operator_descriptor` 且枚举无校准 Jacobian。
6. `FitsCard::make_real` 用 `%.12g`；legacy `p3_projection.cpp` CAR `Y=−θ`、AIT 缺 √2。
7. `VERSION=0.11.0-alpha.2` 未提升；`RELEASED` 仅出现在 `NOT_RELEASED` 语境。
8. 预存 12 项 tracked 差异与本包写域无交集；`ci-fix` 分支（69ec459f，2026-09-08）与 worktree（`git worktree list` 实测 44 行含 main）早于本包。
9. 全部关键证据 sha256（§7，`evidence_index.json`）。

### 9.2 子代理自报但【控制器已复跑】（本包转述，未逐条重跑）

- 35 任务的 Oracle/mutation/负向门通过数（台账 `review` 列与各任务汇总）。
- `REAL-SCIENCE-001` 486/486 Oracle、40/40 库层负向、15/15 CLI 负向（报告 §6/§8）。
- `PERF-SCALE-001` 原始样本复算（终审 §5 与控制器一致）。
- `CONTRACT-FREEZE-001` 2056 checks / 23 mutation、`FINAL-AUDIT-001` 独立 Oracle 17/17、79/79 V6 ctest。

### 9.3 【UNAVAILABLE / UNKNOWN】

- Windows 32/32 用例与候选 sha256：**UNAVAILABLE**（无候选、Fatduck 不可达）。
- Windows MSVC「Run MSVC tests and package candidate」失败根因：**UNKNOWN**（无 token 取 `win-stage-*.log`/`win-package-summary.json`；`FD-F-003` 为候选但未证实）。
- `AR-034-GAP` 7 项历史 CI 红逐名终态：未逐名验收；`AR-035-GAP` 785 缺陷账本：无销账任务。

### 9.4 本包【未】满足的门（汇总）

- §14.5 第 1/2/3/4/6 步 NOT_MET、第 5 步 AWAITING；**无 PASS**。
- §17.12 门 6（资源）未满足（待裁）、门 7（同 SHA 双平台 CI）未满足、门 8（真实数据+Fatduck）未满足、门 9（图像初审/终审）未满足、门 10（P0/P1 为零）未证实（`FD-F-003` P0 OPEN）。
- `AWAITING_EXTERNAL_RELEASE_REVIEW` 未达成。
- **因此包状态为 `NOT_READY / NOT_RELEASED`；不得宣布发布、不得提升 `VERSION`/alpha。**

### 9.5 本包纪律

- 只写 `reports/v6/owner-package/`、`artifacts/v6/owner-package/`；未改任何其他文件。
- 未 commit / push / add / 分支 / worktree / stash / reset / clean / rebase；未派生子代理；未宣布发布。
- 所有外部命令带 timeout 并落日志；未编造任何数字/sha256/结论；不确定项标 UNKNOWN。

---

## 10. 负向检查（本包汇总器自证可红）

本包附带校验器 `artifacts/v6/owner-package/checks/verify_owner_package.py`，并对自身做三组篡改负向检查（`--self-test`，结果 `artifacts/v6/owner-package/checks/negative_checks.json`）：

1. **把状态写成 RELEASED**（`package_status=RELEASED` / `released_claim=true` / 正文出现正面 RELEASED）→ **必红**。
2. **漏掉任一 BLOCKER**（删除 B-01..B-05 任一条）→ **必红**。
3. **把 SO-05 写成已签字**（`so05.signoff_status=SIGNED`）→ **必红**。

校验器对原始（未篡改）审核包必须 rc=0（绿）；对三种篡改必须 rc=1（红）。详细结果见 `artifacts/v6/owner-package/checks/negative_checks.json`。
