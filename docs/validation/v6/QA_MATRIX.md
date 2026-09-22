> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# QA-MATRIX-001 科学 QA 矩阵（解析 / Monte Carlo / 注入源 / 真实数据 / 负向 mutation / 基线比较）

> 上游：ASTROCS_DESIGN.md §12（验证体系）

- 文档 ID：`QA-MATRIX-001-QA-MATRIX`
- 任务：`工程控制/旧 V6 控制包（ROOT-007 已删除）/tasks/QA-MATRIX-001.md`（wave 3，depends_on = SCI-ADJ-001）
- write_scope：`docs/validation/v6/`、`reports/v6/qa-design/`（本任务未写任何其他路径）
- 基线：`HEAD = 125bc0999363be1a42a1f2df3254601e0cc7b8fb`（执行期实测，见 `reports/v6/qa-design/evidence/logs/`）
- 机器规格（唯一事实源）：`reports/v6/qa-design/qa_matrix.json`；mutation 目录 `reports/v6/qa-design/data/mutations.json`；基线矩阵 `reports/v6/qa-design/data/baseline_matrix.json`；零用例账本 `reports/v6/qa-design/case_ledger.json`
- 复跑入口：`python3 reports/v6/qa-design/oracle/run_all.py`（单 rc）
- 推荐状态：由任务返回消息给出（PASS / FAIL / REVIEW_REQUIRED）

> 本文件是**验证设计规格**，不是实现，也不是冻结动作。它把 SCI-ADJ-001 的语义冻结展开为**可实施、可验证**的门：
> 每条门给出判据、容差来源、零用例即红的机制、门能红的 mutation 清单、输入输出、单位、适用域与 fail-closed 条件。
> 本文不改任何冻结门/容差、不写生产源码、不 commit/push、不派生子代理。

## 0. 摘要

| 项 | 值 |
|---|---|
| 门总数 | 44（analytic 11 / Monte Carlo 8 / injection 8 / real_data 6 / baseline 5 / P0 6） |
| 解析与结构校验 | `qa_oracle.py run` 35/35 rc=0；`validate_spec.py` 0 违规 rc=0 |
| 人读/机读一致性 | `check_docs.py` rc=0（渲染块逐字一致） |
| 零用例即红 | `case_ledger.json` 逐门登记；executed=0 或 skip-only → rc=2（见 §4） |
| 负向 mutation | `run_mutations.py` 56/56 全部检出（science 39 / spec 14 / doc 3） |
| 基线比较矩阵 | 3 生产模式 × 2 文档基线 + 1 延迟模式；10 个比较单元（见 §8/§9） |

## 1. 权威继承与冻结口径（原样继承，不得矛盾）

本矩阵全部判据以下列上位权威为语义源，**原样继承**、不新增第三条口径：

- 冻结宪章 `ASTROCS_PROJECT_CONSTITUTION-001` §4.1（量不混名）/§5.3（Drizzle 单位与误差传播）/§6.3（support/coverage 非权重）/§14.2（零用例即红）。
- `docs/owner/PROJECT_SPEC.md` §3（任何近似须有适用域与误差门）/§4/§5/§7/§8（科学正确性门）。
- `docs/design/PHASE{1,2,3}_DETAILED_DESIGN.md` 的验收节（P1 §11、P2 §10、P3 §7）。
- `docs/science/UNIFIED_SCIENCE_MODEL.md` §4/§5/§6/§7/§8/§10/§11。
- `docs/science/PSF_SIGNAL_WEIGHT.md` §2/§3/§4/§5/§6/§7/§8。
- `docs/validation/SCIENCE_FREEZE.md` 与 `docs/contracts/DATA_SEMANTICS.md` §31（冻结状态与 `FZ-*` 条款面）。
- 控制器 `CONTROLLER_LOG.md` C-004.1（psf_snr_power 延迟）/C-004.2（帧级 median SNR 仅诊断）/C-004.3（schema 词表归 W6）。

冻结单位表（原样继承，机器校验见 `meta.json.unit_table` 与 G-ANA-06）：
`signal_sb=ADU/px^2`、`pixel_variance_in=ADU^2`、`sb_variance_out=ADU^2/px^4`、`sb_ivar_out=px^4/ADU^2`、
`Q=ADU^-1`、`flux=ADU`、`W_info=ADU^-2`、`psfsw_robust_weight=1`、Phase3 `variance BUNIT=(signal BUNIT)^2`。

冻结 mode（原样继承）：生产 = `point_information | surface_gls | psfsw_robust`；文档基线 = `equal | pixel_ivar`；
延迟 = `psf_snr_power`（DEFERRED/NOT_IMPLEMENTED，不进 V6 生产路由）。

## 2. 门分类法

| family | 含义 | 真值来源 | 执行 Wave |
|---|---|---|---|
| `analytic` | 解析恒等式/量纲代数 | 显式矩阵、解析式 | W3 定义，W5/W7/W8 执行 |
| `monte_carlo` | 定种子 MC 统计一致性 | 解析预言 + MC 散度 | W5 |
| `injection` | 注入源恢复（bias/variance/coverage/fail-closed） | 注入真值 + 解析方差 | W5 |
| `real_data` | 真实数据清单（M42/银心） | 预注册清单 + 外部参考 | W10/W11 |
| `baseline` | 模式比较与声明边界 | 预注册比较协议 | W5/W10 |
| `p0_root_cause` | 历史根因 R1/R2/R4/R5/R10 与 785 合并层账本 | 结构规则 + 真实锚点 | W3（结构） |

## 3. 每条门的强制字段与容差来源纪律

机器规格中每条门必须同时具备：`claim`、`anchors`、`inputs/outputs/units`、`domain`、`criterion`、
`tol_source`、`zero_case_red`、`fail_closed`、`oracle`、`mutations`、`owner/wave/status`。
容差字段 `threshold_status` 只允许两种取值：

- `frozen`：沿用/继承已冻结容差，`criterion.ref` 或 `tol_source` 必须含真实锚（`FZ-*`/`ADJ-*`/`SCI-*`/W1 Oracle/条款号）；本任务**不改数值**。
- `pending_freeze`：度量与门存在性冻结、**数值未冻结**，必须带 `threshold_owner`（ALG-W3 / W4）。本任务不擅自发明数值。

校验器 `validate_spec.py` 对上述逐门强制（规则 V-FIELD/V-CRIT-*/V-ORACLE-*/V-MUT-*/V-FZ-*），`frozen` 无锚或 `pending_freeze` 无 owner 即红。

## 4. 零用例即红机制

- 每个门在 `case_ledger.json` 登记 `required_cases / executed_cases / skipped_cases / pends / counts_as_pass`。
- runner 规则：`executed_cases == 0`（且非 pending）或 `skipped_cases >= executed_cases`（skip-only）→ **rc=2**，不得计入 PASS。
- `pends=true` 的门（真实数据/预注册比较）必须显式声明 `counts_as_pass=false`、`implementation_cases_scheduled >= required_cases`、并带 owner；否则 rc!=0。
- `validate_spec.py` 的 V-LEDGER-* 规则逐条强制；负向 mutation MUT-SPEC-15（executed=0）、MUT-SPEC-16（skip-only）证明该机制能红。

## 5. 独立 Oracle 与禁止同源自证

- 每门 `oracle` 必须声明 `kind`（白名单见 `validate_spec.py`）、独立 `truth` 与 `must_not`（禁止调用的对象）。
- 独立 Oracle `qa_oracle.py` 为纯 NumPy + 标准库，从第一性原理构造参考（显式矩阵、解析恒等、定种子 MC、testdata 索引），
  **不 import / 不 link / 不执行任何 AstroCS 生产实现或生产测试二进制**。
- 真实数据门禁止以生产输出作唯一 expected（PROJECT_SPEC §8；G-RD-04；对应 AR-043 根因）。

## 6. 负向 mutation 总则

三类 mutation，逐条断言 rc!=0：

- `science`（39 条）：向 Oracle 的被测 subject 注入错误（OLS 取代 GLS、系数扰动、漏 D^2、psfsw 当 ivar、effective PSF 用 median FWHM 等），
  断言目标门至少一个 check 变红；清单见 `NEGATIVE_MUTATION_CATALOG.md` 与 `data/mutations.json`。
- `spec`（14 条）：向 `qa_matrix.json` 注入结构/合同错误（空 mutation 列表、psf_snr_power 进生产、诊断量进 weight.sources、
  退休 support×snr² 合法性、frozen 无锚、pending 无 owner、零用例/skip-only 等），断言 `validate_spec.py` 报违规。
- `doc`（3 条）：向人读渲染表注入错误（删行/改容差/插入 psf_snr_power=production），断言 `check_docs.py` 报不一致。

## 7. 机器规格位置与复跑命令

| 产物 | 路径 |
|---|---|
| 机器规格（44 门） | `reports/v6/qa-design/qa_matrix.json` |
| mutation 目录（56 条） | `reports/v6/qa-design/data/mutations.json` |
| 基线矩阵 | `reports/v6/qa-design/data/baseline_matrix.json` |
| 零用例账本 | `reports/v6/qa-design/case_ledger.json` |
| 独立 Oracle / 校验 / 渲染 / 驱动 | `reports/v6/qa-design/oracle/{qa_oracle,validate_spec,render_docs,check_docs,run_mutations,run_all}.py` |
| 证据与 rc | `reports/v6/qa-design/evidence/`、`reports/v6/qa-design/SUMMARY.md` |

复跑（在 `reports/v6/qa-design/oracle/`）：`python3 run_all.py`；单步：`python3 qa_oracle.py run`、
`python3 validate_spec.py`、`python3 render_docs.py --write`、`python3 check_docs.py`、`python3 run_mutations.py`。

## 8. 门矩阵（机读渲染，逐字一致）

<!-- QA-MATRIX-TABLE-BEGIN -->
| Gate | Family | Phase | 判据（criterion） | 容差来源 / 状态 | 零用例即红 | 门能红 mutation | Owner |
|---|---|---|---|---|---|---|---|
| `G-ANA-01` | analytic | phase2 | `max|F_hat - Q/W| / |x_hat| <= 1e-09 rel` | 解析恒等（非测量阈值）：任意正定 C 下两式代数等价；1e-9 为浮点安全界，继承 SCI-P2-001 Oracle C1。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-A01, MUT-A02, MUT-A12 | ALG-P2-POINT-001 / P2-INTEGRATE-001 |
| `G-ANA-02` | analytic | phase2 | `|c^T C c - 1/W| / (1/W) <= 1e-09 rel` | 代数恒等（C 正确时）；OM1 以 c×1.10 证明该门能红（rc=1）。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-A02, MUT-SPEC-05 | ALG-P2-POINT-001 / P2-INTEGRATE-001 |
| `G-ANA-03` | analytic | phase2 | `var_naive/var_joint < 1 ratio` | 构造性不变量（ratio>1 必须检出），非经验阈值；W1 双 Oracle 实测 1.575 / 3.48。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M01, MUT-I01 | ALG-P2-POINT-001 / ALG-P2-SURF-001 |
| `G-ANA-04` | analytic | phase2 | `max|G C G^T - (A^T C^-1 A)^-1| <= 1e-09 abs` | 代数恒等；继承 SCI-P2-001 Oracle C5。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-A04 | ALG-P2-SURF-001 / P2-INTEGRATE-001 |
| `G-ANA-05` | analytic | phase1 | `|a^2/(sigma^2 A_NEA) - a^2 P^T C^-1 P| / (a^2 P^T C^-1 P) <= 1e-12 rel` | 代数恒等；继承 SCI-PSFW-001 K1 与 SCI-OBS-001 C3。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-A05 | ALG-P1-001 / IMPL-P1-PSFW-001 |
| `G-ANA-06` | analytic | all | `dimension_violations == 0 count` | 量纲代数（精确整数指数）已冻结于 FREEZE_LIST §1/§3.2；本任务不改单位表。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-A06, MUT-SPEC-11 | DATA-DESIGN-001 / SCHEMA-INTEGRATE-001 |
| `G-ANA-07` | analytic | phase2 | `|median_j(W_psfsw,j)-1| <= 1e-12 rel` | 组内归一是定义式（median 精确）；单调方向由解析偏导符号证明，对任意正指数成立。 / frozen | min=2；executed=0 或 skip-only -> rc=2 | MUT-A07, MUT-A13 | ALG-P2-PSFSW-001 / IMPL-P1-PSFW-001 |
| `G-ANA-08` | analytic | phase3 | `|W_recompute - W_resampled_input| > 0 abs` | 构造性区分门：两条路径在退化（S=I 且 PSF 为 delta）外必须给出不同 W；不得以相等为通过。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-A08 | ALG-P3-001 / IMPL-P3-RSMP-001 |
| `G-ANA-09` | analytic | phase3 | `|Σ_k c_k^2 u_k - diag(R diag(u) R^T)| <= 1e-12 abs` | 代数恒等（diag 情形）；非退化 Σc^2 != 1 是重力物理而非容差。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-A09 | ALG-P3-001 / IMPL-P3-RSMP-001 |
| `G-ANA-10` | analytic | phase1 | `max|diag(Cov) - Σ_j v_j w_jp^2/D_p^2| <= 1e-11 rel` | 代数恒等；继承 SCI-OBS-001 门 D1（rtol 1e-11）。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-A10, MUT-A14 | ALG-P1-001 / IMPL-P1-DRZ-001 |
| `G-ANA-11` | analytic | phase1 | `max_pixfrac |S_p/B0 - 1| <= 0.001 rel` | 沿用现行常量场门 /S_p/B0-1/<1e-3（ADJ-S3 明令不改数值）；负向三错法必红。 / frozen | min=2；executed=0 或 skip-only -> rc=2 | MUT-A11, MUT-A15, MUT-A16 | ALG-P1-001 / IMPL-P1-DRZ-001 |
| `G-MC-01` | monte_carlo | phase2 | `|Var_MC(F_hat)/(1/W) - 1| <= 0.03 rel` | 统计收敛阈 3%，继承 SCI-P2-001 C4 / SCI-PSFW-001 K2 的 3%/2% 口径；N_mc 与 seed 必须登记以保证可复跑。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M02, MUT-I02 | ALG-P2-POINT-001 / P2-INTEGRATE-001 |
| `G-MC-02` | monte_carlo | phase2 | `|var_mc/var_pred - 1| <= 0.03 rel` | 继承 SCI-P2-001 Oracle C4 的 3% MC 口径；epsilon 上界本门冻结为存在性，数值归 ALG-P2-SURF-001（FZ-GATE-PIXIVAR-APPROX）。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M03 | ALG-P2-SURF-001 / P2-INTEGRATE-001 |
| `G-MC-03` | monte_carlo | phase2 | `var_joint/var_naive > 1 ratio` | 构造性不变量；共享幅度 0.6 sigma 的构造已由 W1 双 Oracle 提供参考值。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M01, MUT-I01 | ALG-P2-SURF-001 / IMPL-P1-CAL-001 |
| `G-MC-04` | monte_carlo | phase2 | `|var_mc/var_coeffs - 1| <= 0.03 rel` | 继承 SCI-P2-001 C7 的 3% MC 口径；proxy 与 coeffs 必须显著不同（>=10%）以证明边界可测。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M04, MUT-A13 | ALG-P2-PSFSW-001 / P2-INTEGRATE-001 |
| `G-MC-05` | monte_carlo | phase2 | `|FWHM(P_eff) - median_k FWHM_k| / FWHM(P_eff) > 0 rel` | 构造性区分门（等式不得作为通过）；只给 FWHM 标量不构成 effective PSF（R7 REJECT）。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M05 | ALG-P2-PSFSW-001 / P2-INTEGRATE-001 |
| `G-MC-06` | monte_carlo | phase2 | `|Var(Q/W) - CRLB|/CRLB <= 1e-12 rel` | CRLB 是解析下界（精确恒等）；loss_ratio>1.5 为构造性区分门，继承 K4 的 A_NEA=29.05 参考。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M06, MUT-B01 | ALG-P2-POINT-001 |
| `G-MC-07` | monte_carlo | phase2 | `(C_stat + J C_theta J^T)/C_stat > 1 ratio` | 构造性不变量；SCI-P2-001 C9 的含项/不含项比 = 2.0。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M07 | ALG-P2-UPM-001 / IMPL-P2-UPM-001 |
| `G-MC-08` | monte_carlo | phase2 | `k_corr > 1 ratio` | 方向不变量（k_corr>1）为门；冻结数值 1.4 仅域内可用（SO-07 数值阈值由负责人确认，本任务不发明）。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M08 | ALG-P2-UPM-001 / IMPL-P2-UPM-001 |
| `G-INJ-01` | injection | phase2 | `|bias|/F_ref <= 0.02 rel` | 门度量（bias、sigma_F vs 1/sqrt(W)）冻结；数值 2% 属设计默认，最终由 ALG-P2-POINT-001 冻结，本任务不宣称冻结数值。 / pending_freeze | min=1；executed=0 或 skip-only -> rc=2 | MUT-I02, MUT-I03, MUT-SPEC-10 | ALG-P2-POINT-001 / P2-INTEGRATE-001 |
| `G-INJ-02` | injection | phase2 | `max(|bias|/B0, |var/var_pred-1|, |flux_tot/flux_true-1|) <= 0.03 rel` | 门度量冻结（三量一致性）；数值 3% 设计默认，最终由 ALG-P2-SURF-001 冻结。 / pending_freeze | min=2；executed=0 或 skip-only -> rc=2 | MUT-A11, MUT-I03 | ALG-P2-SURF-001 / P2-INTEGRATE-001 |
| `G-INJ-03` | injection | phase2 | `max_threshold |W_info - W_info_ref| / W_info_ref <= 0.05 rel` | 门度量冻结；数值 <5% 为 SCI-PSFW-001 建议口径，最终由 ALG-P2-PSFSW-001 冻结（本任务不擅自改冻结门）。 / pending_freeze | min=1；executed=0 或 skip-only -> rc=2 | MUT-I04, MUT-I05 | ALG-P2-PSFSW-001 / IMPL-P1-PSFW-001 |
| `G-INJ-04` | injection | phase2 | `direction_violations == 0 count` | 解析单调性（W=a^2 P^T C^-1 P 对 a 二次增、对 sigma 减）；方向而非幅度。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-I06 | ALG-P2-POINT-001 / ALG-P2-PSFSW-001 |
| `G-INJ-05` | injection | phase2 | `max|P_eff - (Sum alpha_k a_k (P_k ⊗ K_k))/(Sum alpha_k a_k)| <= 1e-09 rel` | 解析组合定义式恒等。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-M05, MUT-SPEC-06 | ALG-P2-PSFSW-001 / P2-INTEGRATE-001 |
| `G-INJ-06` | injection | phase2 | `unavailable_reasons_not_in_whitelist == 0 count` | 白名单为冻结语义（FZ-GATE-PSFSW-FAILCLOSED）；valid=false 时 weight_value=null。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-I07, MUT-I08 | ALG-P2-PSFSW-001 / IMPL-P1-PSFW-001 |
| `G-INJ-07` | injection | phase3 | `|flux_rec/F_inj - 1| <= 0.02 rel` | 门度量冻结（flux bias/astrometry/variance）；数值 2% 设计默认，最终由 ALG-P3-001 冻结。 / pending_freeze | min=2；executed=0 或 skip-only -> rc=2 | MUT-A08, MUT-I09 | ALG-P3-001 / IMPL-P3-RSMP-001 / IMPL-P3-PROJ-001 |
| `G-INJ-08` | injection | phase1 | `aperture_var_diag/aperture_var_true < 1 ratio` | 构造性不变量（低估必须被检出）；D2 参考值 0.400/0.983。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-A10 | ALG-P1-001 / IMPL-P1-DRZ-001 |
| `G-RD-01` | real_data | all | `seam_and_noise_checklist_failures == 0 count` | 门度量与清单冻结（本任务）；数值阈值由 REAL-SCIENCE-001/W10 在预注册后冻结，本任务不发明。 / pending_freeze | min=6；executed=0 或 skip-only -> rc=2 | MUT-R01 | REAL-SCIENCE-001 (W10) |
| `G-RD-02` | real_data | all | `galactic_center_checklist_failures == 0 count` | 门度量与清单冻结（本任务）；数值由 W10 预注册后冻结。 / pending_freeze | min=6；executed=0 或 skip-only -> rc=2 | MUT-R01 | REAL-SCIENCE-001 (W10) |
| `G-RD-03` | real_data | all | `missing_dataset_count == 0 count` | 存在性（结构证据），非测量阈值；索引版本 1.2 已登记。 / frozen | min=8；executed=0 或 skip-only -> rc=2 | MUT-R02 | REAL-SCIENCE-001 (W10) / WIN-VERIFY-001 (W11) |
| `G-RD-04` | real_data | all | `gates_with_only_production_expected == 0 count` | 结构性规则（冻结）。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-SPEC-07 | QA-MATRIX-001 / FINAL-AUDIT-001 |
| `G-RD-05` | real_data | all | `provenance_missing_keys + bunit_violations == 0 count` | 键集合与量纲代数（冻结）。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-R02 | IMPL-AIO-001 / SCHEMA-INTEGRATE-001 |
| `G-RD-06` | real_data | all | `windows_status == 0 enum` | 状态字面量规则（冻结）。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-R03 | WIN-VERIFY-001 (W11) |
| `G-BASE-01` | baseline | phase2 | `missing_cells + illegal_declarations == 0 count` | 结构与枚举规则（冻结）；比较度量集合冻结，效应量阈值由预注册确定。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-SPEC-04, MUT-B02, MUT-B03 | ALG-P2-POINT-001 / ALG-P2-PSFSW-001 / P2-INTEGRATE-001 |
| `G-BASE-02` | baseline | phase2 | `var_pixel_ivar/var_Winfo > 1.5 ratio` | 构造性区分门，继承 SCI-PSFW-001 K4。 / frozen | min=2；executed=0 或 skip-only -> rc=2 | MUT-M06, MUT-B01 | ALG-P2-POINT-001 / ALG-P2-SURF-001 |
| `G-BASE-03` | baseline | phase2 | `effect_size (psfsw vs specified baseline) > 0 rel` | 声明规则冻结（不得宣称 Fisher 最优）；效应量与 CI 由预注册/W10 冻结。 / pending_freeze | min=4；executed=0 或 skip-only -> rc=2 | MUT-B02, MUT-B03 | ALG-P2-PSFSW-001 / REAL-SCIENCE-001 |
| `G-BASE-04` | baseline | phase2 | `equal_optimality_claims == 0 count` | 结构性规则（FZ-MODE-BASELINE）。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-B04 | P2-INTEGRATE-001 |
| `G-BASE-05` | baseline | phase2 | `psf_snr_power_in_production == 0 count` | 控制器裁决 C-004.1（不得推翻）与 FZ-MODE-DEFERRED。 / frozen | min=1；executed=0 或 skip-only -> rc=2 | MUT-SPEC-04 | ALG-P2-PSFSW-001 / CONTRACT-FREEZE-001 / SCHEMA-INTEGRATE-001 |
| `P0-01` | p0_root_cause | all | `gates_without_mutation + undetected_mutations == 0 count` | 结构性规则（冻结）；对应 785 合并层账本中 R1 根因（FD-F-001/M5a-G-005/M5b-G-03）。 / frozen | min=2；executed=0 或 skip-only -> rc=2 | MUT-SPEC-01, MUT-SPEC-15, MUT-SPEC-16 | QA-MATRIX-001 / RUNTIME-CI-001 |
| `P0-02` | p0_root_cause | all | `self_proof_violations == 0 count` | 结构性规则；对应 785 合并层 R2 根因（FD-F-002/M5a-G-002/M5a-C-004）。 / frozen | min=2；executed=0 或 skip-only -> rc=2 | MUT-SPEC-07, MUT-SPEC-12 | QA-MATRIX-001 |
| `P0-03` | p0_root_cause | all | `gates_without_independent_oracle == 0 count` | 结构性规则；对应 785 合并层 R4 根因（M7-G-104/M2a-F-1/M2b-F-01）。 / frozen | min=2；executed=0 或 skip-only -> rc=2 | MUT-SPEC-08 | QA-MATRIX-001 |
| `P0-04` | p0_root_cause | all | `fossilized_retired_tokens == 0 count` | 结构性规则；对应 785 合并层 R10 根因（M1a-F-005/M3b-A-02/M9-F-2）。 / frozen | min=2；executed=0 或 skip-only -> rc=2 | MUT-SPEC-06, MUT-SPEC-13 | QA-MATRIX-001 / CONTRACT-FREEZE-001 |
| `P0-05` | p0_root_cause | all | `ledger_numbers_without_layer + uncovered_p0_root_causes == 0 count` | 口径规则冻结：所有账本数字带 merged_785/leaf_523；覆盖 R1/R2/R4/R5/R10。 / frozen | min=5；executed=0 或 skip-only -> rc=2 | MUT-SPEC-09 | QA-MATRIX-001 / FINAL-AUDIT-001 |
| `P0-06` | p0_root_cause | all | `thresholds_without_real_source == 0 count` | 规则：frozen 必须有 FZ-/ADJ-/W1 实测锚；pending_freeze 必须有 owner；否则 REJECT。 / frozen | min=2；executed=0 或 skip-only -> rc=2 | MUT-SPEC-10, MUT-SPEC-14 | QA-MATRIX-001 / CONTRACT-FREEZE-001 |
<!-- QA-MATRIX-TABLE-END -->

## 9. 门明细（输入输出/单位/适用域/fail-closed/独立 Oracle）

<!-- QA-MATRIX-DETAILS-BEGIN -->
### `G-ANA-01` — Q/W == GLS 点源通量恒等

- **claim**：point_information 的 F_hat=Sum Q_k/Sum W_k 与显式 GLS 解 x_hat=(A^T C^-1 A)^-1 A^T C^-1 d 在任意正定 C 下数值恒等。
- **条款锚**：FZ-FORMULA-Q；FZ-FORMULA-FHAT；ADJ-P2-01；UNIFIED §4；DESIGN-P2 §6.2；SCI-P2-001 Oracle C1
- **文献锚**：Horne 1986 PASP 98,609 DOI:10.1086/131801；Naylor 1998 MNRAS 296,339；Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872
- **输入 -> 输出**：d_k, P_k, a_k, C_k -> F_hat, Q, W
- **单位**：Q=ADU^-1, W=ADU^-2, F_hat=ADU
- **适用域**：C 正定且可表示；P 归一 Sum P=1；独立帧块对角或联合 C。
- **判据**：`max|F_hat - Q/W| / |x_hat| <= 1e-09 rel`
- **容差来源**：解析恒等（非测量阈值）：任意正定 C 下两式代数等价；1e-9 为浮点安全界，继承 SCI-P2-001 Oracle C1。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：C 非正定/不可求逆 -> 该配置整体 REJECT 并记 unavailable，不参与恒等判定。
- **独立 Oracle**：kind=independent_numpy；truth=解析恒等式 + 显式矩阵参考；must_not=astrocs, lib/, cli/, lib/include/
- **门能红 mutation**：MUT-A01, MUT-A02, MUT-A12
- **owner / wave / status**：ALG-P2-POINT-001 / P2-INTEGRATE-001 / W3 / frozen_formula

### `G-ANA-02` — 实际系数方差恒等 c^T C c == 1/W

- **claim**：报告方差必须用实际组合系数 c=C^-1 A/(A^T C^-1 A) 计算：c^T C c == 1/W；权重标量/诊断量反推一律禁止。
- **条款锚**：FZ-FORMULA-COV-PROP；FZ-GATE-PSFSW-COV；ADJ-P2-03；RULINGS #5；COVARIANCE_AND_EFFECTIVE_PSF §1
- **文献锚**：Horne 1986 PASP 98,609 DOI:10.1086/131801；Fruchter & Hook 2002 PASP 114,144
- **输入 -> 输出**：c, C_in -> Var(F_hat), c^T C c
- **单位**：Var=ADU^2, W=ADU^-2
- **适用域**：任意线性组合；C 含相关项时必须用完整 C。
- **判据**：`|c^T C c - 1/W| / (1/W) <= 1e-09 rel`
- **容差来源**：代数恒等（C 正确时）；OM1 以 c×1.10 证明该门能红（rc=1）。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：variance_from 属于 {weight, psfsw_robust_weight, median_source_snr, support, coverage, fwhm, psf_residual} -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=解析恒等式；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A02, MUT-SPEC-05
- **owner / wave / status**：ALG-P2-POINT-001 / P2-INTEGRATE-001 / W3 / frozen_formula

### `G-ANA-03` — 相关帧必须用联合 C（简单求和被拒）

- **claim**：跨帧相关存在时独立帧求和 Var=1/Sum W_k 低估方差；必须用联合 C 求 c^T C c；低估比 > 1 必须可检出。
- **条款锚**：FZ-FORMULA-COV-PROP；ADJ-P2-01；ADJ-OBS-01；UNIFIED §4；UNIFIED §10；SCI-P2-001 Oracle C3
- **文献锚**：Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872；Fruchter & Hook 2002 PASP 114,144
- **输入 -> 输出**：C_joint, C_diag, W_k -> var_joint, var_naive, ratio
- **单位**：var=ADU^2
- **适用域**：跨帧共享模式 / 共同 master / 重采样相关。
- **判据**：`var_naive/var_joint < 1 ratio`
- **容差来源**：构造性不变量（ratio>1 必须检出），非经验阈值；W1 双 Oracle 实测 1.575 / 3.48。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：无法表示联合 C -> variance 面 unavailable 或系统误差预算（FZ-PROV-SHARED-SYSTEMATIC）。
- **独立 Oracle**：kind=independent_numpy；truth=构造已知共享幅度下的解析比；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M01, MUT-I01
- **owner / wave / status**：ALG-P2-POINT-001 / ALG-P2-SURF-001 / W3 / frozen_mechanism

### `G-ANA-04` — GLS 协方差恒等 G C G^T == (A^T C^-1 A)^-1

- **claim**：surface_gls 的 Cov=(A^T C^-1 A)^-1 与 R C_in R^T (R=(A^T C^-1 A)^-1 A^T C^-1) 数值恒等；使用近似 R~ 时必须报告 R~ C_in R~^T 及相对比值。
- **条款锚**：FZ-FORMULA-GLS；FZ-FORMULA-COV-PROP；ADJ-P2-02；UNIFIED §5；SCI-P2-001 Oracle C5
- **文献锚**：Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872
- **输入 -> 输出**：A, C, d -> x_hat, Cov, R C R^T
- **单位**：Cov=BUNIT^2
- **适用域**：A 列满秩；C 正定。
- **判据**：`max|G C G^T - (A^T C^-1 A)^-1| <= 1e-09 abs`
- **容差来源**：代数恒等；继承 SCI-P2-001 Oracle C5。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：A 秩亏 -> gauge/断图处理并 REJECT 该元件。
- **独立 Oracle**：kind=independent_numpy；truth=解析恒等式；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A04
- **owner / wave / status**：ALG-P2-SURF-001 / P2-INTEGRATE-001 / W3 / frozen_formula

### `G-ANA-05` — 白噪声条件恒等 W=a^2/(sigma^2 A_NEA)

- **claim**：仅 C 对角且 sigma_pix 声明时，W_info=a_k^2/(sigma_pix^2 A_NEA)，A_NEA=1/Sum P^2；该近似为条件式，不得无条件宣称。
- **条款锚**：FZ-COND-WHITENOISE；FZ-FORMULA-WINFO；ADJ-P2-01；PSF_SIGNAL_WEIGHT §2；DESIGN-P1 §8.1
- **文献锚**：Horne 1986 PASP 98,609 DOI:10.1086/131801
- **输入 -> 输出**：a, sigma_pix, P -> W_info, A_NEA
- **单位**：W_info=ADU^-2, A_NEA=px^2
- **适用域**：C=sigma^2 I 且 sigma_pix 已声明；P 归一无特别要求（A_NEA 吸收归一）。
- **判据**：`|a^2/(sigma^2 A_NEA) - a^2 P^T C^-1 P| / (a^2 P^T C^-1 P) <= 1e-12 rel`
- **容差来源**：代数恒等；继承 SCI-PSFW-001 K1 与 SCI-OBS-001 C3。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：C 非对角或 sigma_pix 未声明时使用白噪式 -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=解析恒等式；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A05
- **owner / wave / status**：ALG-P1-001 / IMPL-P1-PSFW-001 / W3 / frozen_formula

### `G-ANA-06` — 单位表量纲代数（ADU/PX 指数）

- **claim**：signal_sb=ADU/px^2、pixel_variance_in=ADU^2、sb_variance_out=ADU^2/px^4、sb_ivar=px^4/ADU^2、Q=ADU^-1、flux=ADU、W_info=ADU^-2、psfsw=1；Phase3 variance BUNIT=(signal BUNIT)^2；variance 与 ivar 互为倒数。
- **条款锚**：FZ-UNIT-SIGNAL-SB；FZ-UNIT-VAR-IN；FZ-UNIT-VAR-SB；FZ-UNIT-IVAR-SB；FZ-UNIT-Q；FZ-UNIT-FLUX；FZ-UNIT-WINFO；FZ-UNIT-PSFSW；FZ-P3-BUNIT-QUADRATIC；ADJ-GEN-01
- **文献锚**：Fruchter & Hook 2002 PASP 114,144
- **输入 -> 输出**：unit_table -> dimension_check
- **单位**：basis=ADU, PX
- **适用域**：全部产品 BUNIT/单位声明。
- **判据**：`dimension_violations == 0 count`
- **容差来源**：量纲代数（精确整数指数）已冻结于 FREEZE_LIST §1/§3.2；本任务不改单位表。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：BUNIT 量纲不可判（无显式 px 幂次且 provenance 无 pixel_area_power=-2）-> unavailable/REJECT。
- **独立 Oracle**：kind=independent_stdlib；truth=FREEZE_LIST §1 单位表；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A06, MUT-SPEC-11
- **owner / wave / status**：DATA-DESIGN-001 / SCHEMA-INTEGRATE-001 / W3 / frozen

### `G-ANA-07` — PSFSW 复合与组内归一（median=1、全正、单调方向）

- **claim**：Wt_k=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta) 且 W_psfsw,k=Wt_k/median_j(Wt_j) 组内 median=1、全正；对 S/Conc 单调增、对 N/B 单调减（alpha,beta,gamma,delta>=0）。
- **条款锚**：FZ-FORMULA-PSFSW-COMPOSITE；FZ-FIELD-PSFSW-4COMP；FZ-FIELD-PSFSW-UNIT；ADJ-P2-03；PSF_SIGNAL_WEIGHT §3
- **文献锚**：PixInsight New Image Weighting Algorithms §2.5-2.6；Starck, Murtagh & Fadili 2010 CBO9780511730344 (MMT)；Rousseeuw & Croux 1993 JASA 88,1273 (Sn)
- **输入 -> 输出**：S, Conc, N, B, alpha, beta, gamma, delta, C_norm -> Wt, W_psfsw
- **单位**：W_psfsw=1
- **适用域**：同波段、同连通分量、光度已归一的帧组；四分量 measurement_id 互异。
- **判据**：`|median_j(W_psfsw,j)-1| <= 1e-12 rel`
- **容差来源**：组内归一是定义式（median 精确）；单调方向由解析偏导符号证明，对任意正指数成立。（status=frozen）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：背景非正且变换未定义 / 无共同星集 / 有效星不足 / 选择偏差门失败 -> unavailable，禁回退 median SNR。
- **独立 Oracle**：kind=independent_numpy；truth=定义式 + 解析单调性；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A07, MUT-A13
- **owner / wave / status**：ALG-P2-PSFSW-001 / IMPL-P1-PSFW-001 / W3 / frozen_formula

### `G-ANA-08` — Phase3 Q/W 输出帧重算（pi=S p；禁止重采样输入 Q/W）

- **claim**：输出帧必须重算 Q=a pi^T C_y^-1 f、W=a^2 pi^T C_y^-1 pi，pi=S p；禁止把输入 Q/W 重采样后直接使用。
- **条款锚**：FZ-P3-QW-RECOMPUTE；ADJ-P3-01；C-P3-PROP-14；DESIGN-P3 §4；PHASE3_PROPAGATION_REVIEW §6
- **文献锚**：Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872；Greisen & Calabretta 2002 A&A 395,1061
- **输入 -> 输出**：f, S, p, C_y, a -> Q, W, pi
- **单位**：Q=ADU^-1, W=ADU^-2
- **适用域**：point_source_flux 模式；S 行归一；缺失相关核时须显式声明。
- **判据**：`|W_recompute - W_resampled_input| > 0 abs`
- **容差来源**：构造性区分门：两条路径在退化（S=I 且 PSF 为 delta）外必须给出不同 W；不得以相等为通过。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：缺 PSF / PSF 未归一 / 缺 point_information 且不可重建 / 缺 a / 未出 effective PSF -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=C-P3-PROP-14 定义式；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A08
- **owner / wave / status**：ALG-P3-001 / IMPL-P3-RSMP-001 / W3 / frozen_formula

### `G-ANA-09` — Phase3 重采样方差 Σc^2 u 与行归一 R

- **claim**：行归一 Sum_j R_ij=1 保证常数面亮度不变量；var_out=Σ_k c_k^2 u_k 且非退化位置 Σc_k^2 != 1；ivar_out=1/var_out 有限且 >0。
- **条款锚**：FZ-P3-BUNIT-QUADRATIC；ADJ-P3-01；C-P3-PROP-2；C-P3-PROP-7；C-P3-PROP-8；DATA_SEMANTICS §30.4
- **文献锚**：Greisen & Calabretta 2002 A&A 395,1061；Fernique et al. 2015 A&A 578,A114 (HiPS)
- **输入 -> 输出**：R, c_k, u_k -> var_out, row_sum
- **单位**：var_out=BUNIT^2
- **适用域**：线性采样 y=R x；输入像素独立（否则须给相关核）。
- **判据**：`|Σ_k c_k^2 u_k - diag(R diag(u) R^T)| <= 1e-12 abs`
- **容差来源**：代数恒等（diag 情形）；非退化 Σc^2 != 1 是重力物理而非容差。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：输入 C 已相关仍只出对角且无相关核 -> 拒绝（对角化过度乐观须被检出，M3-05）。
- **独立 Oracle**：kind=independent_numpy；truth=线性传播恒等式；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A09
- **owner / wave / status**：ALG-P3-001 / IMPL-P3-RSMP-001 / W3 / frozen_formula

### `G-ANA-10` — Drizzle 方差传播恒等 + 缩放律

- **claim**：diag(Cov(S_p,S_q))=Σ_j v_j w_jp^2/D_p^2；x->alpha*x 蕴含 var->alpha^2 var、ivar->ivar/alpha^2。
- **条款锚**：FZ-FORMULA-DRIZZLE-VAR；FZ-UNIT-VAR-SB；FZ-UNIT-IVAR-SB；ADJ-F-OBS-01；DRIZZLE §5
- **文献锚**：Fruchter & Hook 2002 PASP 114,144
- **输入 -> 输出**：v_j, w_jp, D_p, x_j -> variance_p, ivar_p
- **单位**：variance_p=ADU^2/px^4, ivar_p=px^4/ADU^2
- **适用域**：线性 Drizzle 算子；w_jp=a_jp/A_drop,j；D_p=Σ_j a_jp。
- **判据**：`max|diag(Cov) - Σ_j v_j w_jp^2/D_p^2| <= 1e-11 rel`
- **容差来源**：代数恒等；继承 SCI-OBS-001 门 D1（rtol 1e-11）。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：非对角协方差被当零用于 aperture/总量误差 -> 拒绝（须给相关核/误差）。
- **独立 Oracle**：kind=independent_numpy；truth=解析恒等式；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A10, MUT-A14
- **owner / wave / status**：ALG-P1-001 / IMPL-P1-DRZ-001 / W3 / frozen_formula

### `G-ANA-11` — Drizzle 面亮度归一 + 常量场 Oracle + 条件通量守恒

- **claim**：S_p=Σ_j B_j a_jp/Σ_j a_jp（B_j=x_j/A_pixel,j）对全部 pixfrac in (0,1] 满足常量面亮度 S_p=B0；通量守恒为条件不变量：pf=1 严格 ΣF=Σx，pf<1 总输出通量=pixfrac^2*Σx 且须记 flux_conservation_factor。
- **条款锚**：FZ-FORMULA-DRIZZLE-SB；FZ-GATE-CONST-SB；FZ-COND-FLUX-CONSERV；ADJ-F-OBS-02；ADJ-S3；DESIGN-P1 §9；UNIFIED §7
- **文献锚**：Fruchter & Hook 2002 PASP 114,144
- **输入 -> 输出**：B0, A_pixel, a_jp, pixfrac, x_j -> S_p, factor
- **单位**：S_p=ADU/px^2
- **适用域**：常量面亮度场按 x_j=B0*A_pixel,j 构造；pixfrac in (0,1]。
- **判据**：`max_pixfrac |S_p/B0 - 1| <= 0.001 rel`
- **容差来源**：沿用现行常量场门 |S_p/B0-1|<1e-3（ADJ-S3 明令不改数值）；负向三错法必红。（status=frozen）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：缺 flux_conservation_factor -> 不可用于绝对通量；BUNIT 缺 px 幂次且无 provenance -> unavailable。
- **独立 Oracle**：kind=independent_numpy；truth=ADJ-S3 真值构造（按 B0）；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A11, MUT-A15, MUT-A16
- **负责人签字（只登记）**：SO-02（面亮度归一 FROZEN 公式变更）/ SO-03（常量场判据取代）— 只登记不签署
- **owner / wave / status**：ALG-P1-001 / IMPL-P1-DRZ-001 / W3 / frozen_mechanism_signoff_SO-02/SO-03

### `G-MC-01` — 点源注入/散度 vs 1/sqrt(W)

- **claim**：对固定 F_ref 的注入点源，实测 F_hat 散度与理论 sigma_F=1/sqrt(W_info) 一致；SNR_combined^2=F_ref^2*W。
- **条款锚**：FZ-FORMULA-FHAT；FZ-FORMULA-WINFO；ADJ-P2-01；PSF_SIGNAL_WEIGHT §7.1；DESIGN-P1 §11；UNIFIED §10
- **文献锚**：Horne 1986 PASP 98,609 DOI:10.1086/131801；Naylor 1998 MNRAS 296,339；Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872
- **输入 -> 输出**：注入 F_ref, P_k, a_k, C_k, seed, N_mc -> Var_MC(F_hat), 1/W, rel_dev, SNR^2
- **单位**：Var_MC=ADU^2, W=ADU^-2
- **适用域**：线性高斯、C 正确、P 归一；独立帧。
- **判据**：`|Var_MC(F_hat)/(1/W) - 1| <= 0.03 rel`
- **容差来源**：统计收敛阈 3%，继承 SCI-P2-001 C4 / SCI-PSFW-001 K2 的 3%/2% 口径；N_mc 与 seed 必须登记以保证可复跑。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：N_mc < 1 或 seed 缺失 -> rc=2（零用例/不可复跑即红）；跨帧相关未用联合 C -> 该 case 拒绝。
- **独立 Oracle**：kind=independent_numpy_mc；truth=解析 1/W + 定种子 MC 散度；must_not=astrocs, lib/, eng/tests/
- **门能红 mutation**：MUT-M02, MUT-I02
- **owner / wave / status**：ALG-P2-POINT-001 / P2-INTEGRATE-001 / W5 / mechanism_frozen_tol_inherited

### `G-MC-02` — surface_gls 协方差 vs Monte Carlo

- **claim**：GLS 预言的 Cov=(A^T C^-1 A)^-1 与 MC 实测方差一致（rel<3%）；忽略 a_k 的像素 ivar 近似方差严格劣化（比值>1）。
- **条款锚**：FZ-FORMULA-GLS；FZ-GATE-PIXIVAR-APPROX；ADJ-P2-02；UNIFIED §5；SCI-P2-001 Oracle C4
- **文献锚**：Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872
- **输入 -> 输出**：A, C, seed, N_mc -> var_pred, var_mc, rel, ratio_approx
- **单位**：var=BUNIT^2
- **适用域**：GLS 假设成立；像素 ivar 近似仅在同点采样+噪声独立+a_k 一致时可用。
- **判据**：`|var_mc/var_pred - 1| <= 0.03 rel`
- **容差来源**：继承 SCI-P2-001 Oracle C4 的 3% MC 口径；epsilon 上界本门冻结为存在性，数值归 ALG-P2-SURF-001（FZ-GATE-PIXIVAR-APPROX）。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：无 Var_approx/Var_GLS <= 1+epsilon 误差门声明 -> REJECT；必须报告 R~ C_in R~^T。
- **独立 Oracle**：kind=independent_numpy_mc；truth=解析 (A^T C^-1 A)^-1 + MC；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M03
- **owner / wave / status**：ALG-P2-SURF-001 / P2-INTEGRATE-001 / W5 / mechanism_frozen_eps_pending

### `G-MC-03` — 共享系统项：联合 vs 朴素求和比值 > 1

- **claim**：存在共同模式时联合 C 的方差严格大于 1/Sum W_k；比值必须 > 1 并被检出（共享项不得按独立项处理）。
- **条款锚**：ADJ-OBS-01；ADJ-F-OBS-04；FZ-PROV-SHARED-SYSTEMATIC；UNIFIED §6；SCI-P2-001 Oracle C3；SCI-OBS-001 C4
- **文献锚**：Fruchter & Hook 2002 PASP 114,144；Padmanabhan et al. 2008 ApJ 674,1217
- **输入 -> 输出**：shared_mode_amp, n_frames, seed -> var_joint, var_naive, ratio
- **单位**：ratio=1
- **适用域**：共同 master / 共同天空 / 重采样相关。
- **判据**：`var_joint/var_naive > 1 ratio`
- **容差来源**：构造性不变量；共享幅度 0.6 sigma 的构造已由 W1 双 Oracle 提供参考值。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：共享项无法表示 -> variance unavailable 或系统误差预算；禁止静默按独立项发布。
- **独立 Oracle**：kind=independent_numpy_mc；truth=构造已知共享项的解析比；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M01, MUT-I01
- **owner / wave / status**：ALG-P2-SURF-001 / IMPL-P1-CAL-001 / W5 / mechanism_frozen

### `G-MC-04` — PSFSW 方差从实际系数传播（禁止 1/W_psfsw）

- **claim**：Var(I_out)=Sum_{k,l} alpha_k alpha_l [C_in]_{kl} 用实际 alpha 计算并与 MC 一致；1/W_psfsw 或 Sum W_psfsw 作为方差必须被拒且可测出差异。
- **条款锚**：FZ-GATE-PSFSW-COV；FZ-FORMULA-COV-PROP；ADJ-P2-03；RULINGS #5；COVARIANCE_AND_EFFECTIVE_PSF §5
- **文献锚**：Fruchter & Hook 2002 PASP 114,144；PixInsight New Image Weighting Algorithms §2.5-2.6
- **输入 -> 输出**：W_psfsw, C_in, seed, N_mc -> var_coeffs, var_proxy, rel, ratio
- **单位**：var=BUNIT^2
- **适用域**：psfsw_robust conventional coadd；帧独立或给定联合 C。
- **判据**：`|var_mc/var_coeffs - 1| <= 0.03 rel`
- **容差来源**：继承 SCI-P2-001 C7 的 3% MC 口径；proxy 与 coeffs 必须显著不同（>=10%）以证明边界可测。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：variance_from_weight=true / uses_relative_weight_as_ivar=true -> REJECT（键集合命中即红）。
- **独立 Oracle**：kind=independent_numpy_mc；truth=C_out=R C_in R^T + MC；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M04, MUT-A13
- **owner / wave / status**：ALG-P2-PSFSW-001 / P2-INTEGRATE-001 / W5 / mechanism_frozen

### `G-MC-05` — effective PSF：FWHM 从实际组合算子测量

- **claim**：FWHM(P_eff) 必须由实际组合算子脉冲响应测量；median(FWHM_k) != FWHM(P_eff)，且同一帧组在不同权重下 FWHM(P_eff) 不同。
- **条款锚**：FZ-GATE-PSFSW-EPSF；ADJ-P2-03；COVARIANCE_AND_EFFECTIVE_PSF §3；DESIGN-P2 §6.3/§9；SCI-P2-001 Oracle C6
- **文献锚**：Zackay & Ofek 2017 II ApJ 836,188 arXiv:1512.06879；Horne 1986 PASP 98,609 DOI:10.1086/131801
- **输入 -> 输出**：P_k, alpha_k, K_k -> P_eff, FWHM(P_eff), median_input_FWHM
- **单位**：FWHM=px
- **适用域**：conventional coadd 与 matched-filter/proper coadd 两族；归一约定须声明（peak/integral）。
- **判据**：`|FWHM(P_eff) - median_k FWHM_k| / FWHM(P_eff) > 0 rel`
- **容差来源**：构造性区分门（等式不得作为通过）；只给 FWHM 标量不构成 effective PSF（R7 REJECT）。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：effective_psf_id 空 / 只有 fwhm 单标量 -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=脉冲响应定义式；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M05
- **owner / wave / status**：ALG-P2-PSFSW-001 / P2-INTEGRATE-001 / W5 / frozen_formula

### `G-MC-06` — CRLB 紧致性与像素 ivar 损失比

- **claim**：线性高斯模型下 Q/W 估计量达到 Cramer-Rao 下界（相对差 <1e-12）；像素 ivar 平均相对 matched-filter 的方差损失比 > 1.5（证明基线不是最优）。
- **条款锚**：FZ-FORMULA-WINFO；ADJ-P2-01；PSF_SIGNAL_WEIGHT §2；UNIFIED §5/§11
- **文献锚**：Horne 1986 PASP 98,609 DOI:10.1086/131801；Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872
- **输入 -> 输出**：P, C, a -> CRLB_rel, loss_ratio
- **单位**：CRLB_rel=rel, loss_ratio=ratio
- **适用域**：线性高斯；PSF 跨多像素（退化到同点采样时 loss_ratio->1，属声明域边界）。
- **判据**：`|Var(Q/W) - CRLB|/CRLB <= 1e-12 rel`
- **容差来源**：CRLB 是解析下界（精确恒等）；loss_ratio>1.5 为构造性区分门，继承 K4 的 A_NEA=29.05 参考。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：声明像素 ivar 对任意 PSF 点源最优 -> REJECT（UNIFIED §11）。
- **独立 Oracle**：kind=independent_numpy；truth=CRLB 解析 + 显式矩阵；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M06, MUT-B01
- **owner / wave / status**：ALG-P2-POINT-001 / W3 / frozen_formula

### `G-MC-07` — UPM 参数不确定度进入 covariance

- **claim**：C_out=C_stat+J C_theta J^T：含 UPM 参数项与不含项的方差比 > 1 且可计算；UPM 项缺失必须被检出。
- **条款锚**：ADJ-P2-01；DESIGN-P2 §4；UNIFIED §6；COVARIANCE_AND_EFFECTIVE_PSF §4；SCI-P2-001 Oracle C9
- **文献锚**：Padmanabhan et al. 2008 ApJ 674,1217；Gruen et al. 2014 PASP 126,158
- **输入 -> 输出**：J, C_theta, C_stat -> ratio
- **单位**：ratio=1
- **适用域**：UPM 参数（光度尺度 g_k、背景 b_k）有不确定度时。
- **判据**：`(C_stat + J C_theta J^T)/C_stat > 1 ratio`
- **容差来源**：构造性不变量；SCI-P2-001 C9 的含项/不含项比 = 2.0。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：UPM 断图/欠定 -> 显式失败/分组件，不得静默发布。
- **独立 Oracle**：kind=independent_numpy；truth=解析传播 C_out=C_stat+J C_theta J^T；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M07
- **owner / wave / status**：ALG-P2-UPM-001 / IMPL-P2-UPM-001 / W5 / mechanism_frozen

### `G-MC-08` — k_corr 可复现口径与适用域

- **claim**：定义 k_corr=Var(median)/[pi sigma_bg^2/(2 N_retained)]；固定种子 MC 必须复现方向 k_corr>1 且脚本可复跑；未复跑前仅允许在已声明域内取 1.4，禁止外推。
- **条款锚**：FZ-PROV-KCORR；ADJ-F-OBS-05；UNCERTAINTY_AND_COVARIANCE §V19R3；SCI-OBS-001 门 D5
- **文献锚**：Starck, Murtagh & Fadili 2010 CBO9780511730344 (MMT)
- **输入 -> 输出**：pixfrac, patch, N_retained, estimator_version, seed -> k_corr, domain
- **单位**：k_corr=1
- **适用域**：显式声明几何/pixfrac/control patch/估计器/是否球面；域外禁用。
- **判据**：`k_corr > 1 ratio`
- **容差来源**：方向不变量（k_corr>1）为门；冻结数值 1.4 仅域内可用（SO-07 数值阈值由负责人确认，本任务不发明）。（status=frozen，owner=ALG-P2-UPM-001）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：k_corr=1（忽略相关）或域外外推且未更新 provenance -> REJECT。
- **独立 Oracle**：kind=independent_numpy_mc；truth=定义式 + 定种子 MC；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M08
- **负责人签字（只登记）**：SO-07（k_corr 标定脚本与数值阈值）— 只登记不签署
- **owner / wave / status**：ALG-P2-UPM-001 / IMPL-P2-UPM-001 / W5 / mechanism_frozen_value_pending_SO-07

### `G-INJ-01` — 点源注入恢复：bias/variance/SNR 组合律

- **claim**：注入已知 F_ref 点源后恢复通量无偏（bias<2%），方差与 1/W 一致；独立帧满足 SNR_combined^2=Sum SNR_k^2。
- **条款锚**：FZ-FORMULA-FHAT；ADJ-P2-01；PROJECT_SPEC §8；DESIGN-P2 §10；UNIFIED §10；PSF_SIGNAL_WEIGHT §7.1
- **文献锚**：Horne 1986 PASP 98,609 DOI:10.1086/131801；Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872
- **输入 -> 输出**：注入目录, P_k, a_k, C_k, seed -> bias, sigma_F, SNR_comb^2
- **单位**：flux=ADU, sigma_F=ADU
- **适用域**：独立帧、模型正确；相关帧场景必须走联合 C 分支。
- **判据**：`|bias|/F_ref <= 0.02 rel`
- **容差来源**：门度量（bias、sigma_F vs 1/sqrt(W)）冻结；数值 2% 属设计默认，最终由 ALG-P2-POINT-001 冻结，本任务不宣称冻结数值。（status=pending_freeze，owner=ALG-P2-POINT-001）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：注入源 sigma_F 与 1/sqrt(W) 偏差超门且无原因 -> REJECT；跨帧相关简单求和 -> REJECT。
- **独立 Oracle**：kind=independent_numpy_mc；truth=注入已知真值 + 解析方差；must_not=astrocs, lib/, eng/tests/
- **门能红 mutation**：MUT-I02, MUT-I03, MUT-SPEC-10
- **owner / wave / status**：ALG-P2-POINT-001 / P2-INTEGRATE-001 / W5 / mechanism_frozen_tol_pending

### `G-INJ-02` — 扩展源/常量场/梯度注入（GLS）

- **claim**：注入常量面亮度、线性梯度与已知总通量的扩展源，GLS 重建在 bias/variance/总通量上无偏；常量面亮度对全部 pixfrac 成立。
- **条款锚**：FZ-FORMULA-GLS；FZ-GATE-CONST-SB；ADJ-P2-02；ADJ-S3；PROJECT_SPEC §8；DESIGN-P2 §10
- **文献锚**：Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872；Fruchter & Hook 2002 PASP 114,144
- **输入 -> 输出**：注入模型, A, C, pixfrac -> bias, var, flux_tot
- **单位**：SB=ADU/px^2
- **适用域**：GLS 假设成立；像素 ivar 近似仅条件成立。
- **判据**：`max(|bias|/B0, |var/var_pred-1|, |flux_tot/flux_true-1|) <= 0.03 rel`
- **容差来源**：门度量冻结（三量一致性）；数值 3% 设计默认，最终由 ALG-P2-SURF-001 冻结。（status=pending_freeze，owner=ALG-P2-SURF-001）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：pf<1 时 flux_conservation_factor 缺失 -> 不得用于绝对通量。
- **独立 Oracle**：kind=independent_numpy；truth=注入真值 + (A^T C^-1 A)^-1；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A11, MUT-I03
- **owner / wave / status**：ALG-P2-SURF-001 / P2-INTEGRATE-001 / W5 / mechanism_frozen_tol_pending

### `G-INJ-03` — 星表深度不变性：W_info 样本无关、median SNR 随深度移动

- **claim**：改变检测阈值/星表深度，W_info（或 ratio-of-powers）逐位不变；median(source SNR) 与样本派生 PSFSW 随之移动；共同星集必须独立于待测帧测量。
- **条款锚**：FZ-GATE-MEDIAN-SNR；ADJ-C004-02；ADJ-P2-03；PSF_SIGNAL_WEIGHT §7.3；DESIGN-P1 §11；PSFW §5.3
- **文献锚**：PixInsight New Image Weighting Algorithms §2.5-2.6
- **输入 -> 输出**：星场, thresholds, 星表 -> W_info, median_snr, shift
- **单位**：W_info=ADU^-2
- **适用域**：共同星集 = 外部参考星表固定子集或参考叠加单一门限；禁止逐帧阈值交集。
- **判据**：`max_threshold |W_info - W_info_ref| / W_info_ref <= 0.05 rel`
- **容差来源**：门度量冻结；数值 <5% 为 SCI-PSFW-001 建议口径，最终由 ALG-P2-PSFSW-001 冻结（本任务不擅自改冻结门）。（status=pending_freeze，owner=ALG-P2-PSFSW-001）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：selection function 缺失 / 样本派生共同星集 -> unavailable。
- **独立 Oracle**：kind=independent_numpy；truth=构造已知：W_info 按构造逐位不变（PSFW K8）；must_not=astrocs, lib/
- **门能红 mutation**：MUT-I04, MUT-I05
- **owner / wave / status**：ALG-P2-PSFSW-001 / IMPL-P1-PSFW-001 / W5 / mechanism_frozen_tol_pending

### `G-INJ-04` — 单变量扫描方向（透明度/seeing/背景/读噪）

- **claim**：W_info 对透明度（a 增大）增大、对 seeing（PSF 展宽）减小、对背景/读噪增大减小；psfsw 四分量方向正确。
- **条款锚**：FZ-FORMULA-WINFO；ADJ-P2-01；PSF_SIGNAL_WEIGHT §7.2；DESIGN-P1 §11
- **文献锚**：Horne 1986 PASP 98,609 DOI:10.1086/131801；Naylor 1998 MNRAS 296,339
- **输入 -> 输出**：扫描变量, P, a, C -> direction_ok
- **单位**：dir=sign
- **适用域**：单变量扫描、其余固定；渐变至退化域须声明。
- **判据**：`direction_violations == 0 count`
- **容差来源**：解析单调性（W=a^2 P^T C^-1 P 对 a 二次增、对 sigma 减）；方向而非幅度。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：任一方向违反 -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=解析单调性；must_not=astrocs, lib/
- **门能红 mutation**：MUT-I06
- **owner / wave / status**：ALG-P2-POINT-001 / ALG-P2-PSFSW-001 / W3 / frozen_mechanism

### `G-INJ-05` — effective PSF 脉冲响应恢复（归一约定声明）

- **claim**：注入单位通量点源后，实际组合算子的输出即 P_eff；peak 与 integral 归一约定按产品族声明；ΣP_eff=1（面亮度产品）或 peak=1（点源统计）。
- **条款锚**：FZ-GATE-PSFSW-EPSF；ADJ-P2-03；COVARIANCE_AND_EFFECTIVE_PSF §3；DESIGN-P2 §9
- **文献锚**：Zackay & Ofek 2017 II ApJ 836,188 arXiv:1512.06879；Horne 1986 PASP 98,609 DOI:10.1086/131801
- **输入 -> 输出**：注入点源, alpha_k, K_k -> P_eff, norm
- **单位**：P_eff=1
- **适用域**：conventional 与 proper-coadd 两族；归一约定必须显式声明。
- **判据**：`max|P_eff - (Sum alpha_k a_k (P_k ⊗ K_k))/(Sum alpha_k a_k)| <= 1e-09 rel`
- **容差来源**：解析组合定义式恒等。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：缺 effective PSF 或只给 FWHM 标量 -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=脉冲响应定义式；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M05, MUT-SPEC-06
- **owner / wave / status**：ALG-P2-PSFSW-001 / P2-INTEGRATE-001 / W5 / frozen_formula

### `G-INJ-06` — psfsw fail-closed 目录（零星/少星/拥挤/梯度/云/拖线/FOV/波段）

- **claim**：无共同星集/背景非正且变换未定义/有效星不足/选择偏差门失败 -> unavailable（weight_value=null），禁止回退 median source SNR；零星、少星、拥挤、严重梯度、云、拖线、不同 FOV 与波段均有 fail-closed 用例。
- **条款锚**：FZ-GATE-PSFSW-FAILCLOSED；ADJ-P2-03；PSF_SIGNAL_WEIGHT §3/§7.6；DESIGN-P2 §6.3
- **文献锚**：PixInsight New Image Weighting Algorithms §2.5-2.6
- **输入 -> 输出**：帧组特征 -> unavailable, reason
- **单位**：1=1
- **适用域**：所有 psfsw_robust 输入；reason ∈ {no_common_star_set, background_nonpositive_undefined_transform, insufficient_valid_stars, selection_bias_gate_failed, spatial_nonuniformity_gate_failed}。
- **判据**：`unavailable_reasons_not_in_whitelist == 0 count`
- **容差来源**：白名单为冻结语义（FZ-GATE-PSFSW-FAILCLOSED）；valid=false 时 weight_value=null。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：valid=false 但 weight_value 非 null / 回退 median SNR -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=FZ-GATE-PSFSW-FAILCLOSED 白名单；must_not=astrocs, lib/
- **门能红 mutation**：MUT-I07, MUT-I08
- **owner / wave / status**：ALG-P2-PSFSW-001 / IMPL-P1-PSFW-001 / W5 / frozen

### `G-INJ-07` — Phase3 注入穿透重采样（通量/天体测量/方差恢复）

- **claim**：注入源经投影+采样后通量、位置与方差恢复一致；Q/W 输出帧重算与注入真值一致；WCS 正反变换与外部实现交叉。
- **条款锚**：FZ-P3-QW-RECOMPUTE；ADJ-P3-01；C-P3-PROP-14；C-P3-PROP-15；DESIGN-P3 §7
- **文献锚**：Greisen & Calabretta 2002 A&A 395,1061；Gorski et al. 2005 HEALPix ApJ 622,759；Fernique et al. 2015 A&A 578,A114 (HiPS)；IVOA HiPS 1.0 Recommendation
- **输入 -> 输出**：注入目录, R, S, C_y -> flux_bias, pos_err, var_ratio
- **单位**：flux=ADU, pos=px
- **适用域**：point_source_flux 与 surface_brightness 模式；TAN/SIN/CAR/AIT。
- **判据**：`|flux_rec/F_inj - 1| <= 0.02 rel`
- **容差来源**：门度量冻结（flux bias/astrometry/variance）；数值 2% 设计默认，最终由 ALG-P3-001 冻结。（status=pending_freeze，owner=ALG-P3-001）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：缺 PSF/PSF 未归一/缺 a/未出 effective PSF/对角无相关核 -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=注入真值 + 外部 WCS 参考；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A08, MUT-I09
- **owner / wave / status**：ALG-P3-001 / IMPL-P3-RSMP-001 / IMPL-P3-PROJ-001 / W5 / mechanism_frozen_tol_pending

### `G-INJ-08` — Drizzle aperture 方差低估检出

- **claim**：Drizzle 输出相邻像素相关，aperture 方差按逐像素方差求和会低估（构造实测 2.46x）；相关性感知 aperture 必须显式加入 Cov 项。
- **条款锚**：ADJ-F-OBS-01；ADJ-AR-02；FZ-FORMULA-COV-PROP；UNIFIED §7；SCI-OBS-001 门 D2
- **文献锚**：Fruchter & Hook 2002 PASP 114,144
- **输入 -> 输出**：footprint, v_j, c_jp -> aperture_var_true, aperture_var_diag, ratio
- **单位**：ratio=1
- **适用域**：Drizzle 后相关像素上的 aperture/总量误差。
- **判据**：`aperture_var_diag/aperture_var_true < 1 ratio`
- **容差来源**：构造性不变量（低估必须被检出）；D2 参考值 0.400/0.983。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：把逐像素方差和当 aperture 精确方差 -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=解析 aperture 二次型；must_not=astrocs, lib/
- **门能红 mutation**：MUT-A10
- **owner / wave / status**：ALG-P1-001 / IMPL-P1-DRZ-001 / W5 / mechanism_frozen

### `G-RD-01` — M42 马赛克真实数据终验

- **claim**：在 testdata/M42_T2T3_mosaic_Flying_dutchman（T2+T3）上检查接缝、背景、星形、排异、黑洞与预测/实测噪声，并落三模式基线比较。
- **条款锚**：PROJECT_SPEC §8；DESIGN-P2 §10；PSF_SIGNAL_WEIGHT §7.4
- **文献锚**：Gruen et al. 2014 PASP 126,158；Padmanabhan et al. 2008 ApJ 674,1217
- **输入 -> 输出**：testdata/M42_T2T3_mosaic_Flying_dutchman/T2, testdata/M42_T2T3_mosaic_Flying_dutchman/T3 -> seam_metric, noise_ratio, rejection_report
- **单位**：noise_ratio=1
- **适用域**：真实数据；执行归 Wave10 REAL-SCIENCE-001；本任务只定义门。
- **判据**：`seam_and_noise_checklist_failures == 0 count`
- **容差来源**：门度量与清单冻结（本任务）；数值阈值由 REAL-SCIENCE-001/W10 在预注册后冻结，本任务不发明。（status=pending_freeze，owner=REAL-SCIENCE-001）
- **零用例即红**：min_cases=6；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：接缝/背景/星形/排异/黑洞/噪声任一未过 -> 不得标 VERIFIED。
- **独立 Oracle**：kind=real_data_checklist；truth=预注册清单 + 外部参考；must_not=solely_production_output
- **门能红 mutation**：MUT-R01
- **owner / wave / status**：REAL-SCIENCE-001 (W10) / W10 / defined_pending_execution

### `G-RD-02` — 银心真实数据终验（拥挤/排异/PSF）

- **claim**：在 testdata/Galaxy_Center_T4 上检查拥挤场 PSF、排异、背景梯度、黑洞与预测/实测噪声，并落模式比较。
- **条款锚**：PROJECT_SPEC §8；DESIGN-P2 §10；PSF_SIGNAL_WEIGHT §7.4
- **文献锚**：Gruen et al. 2014 PASP 126,158；Padmanabhan et al. 2008 ApJ 674,1217
- **输入 -> 输出**：testdata/Galaxy_Center_T4/lights -> crowding_metric, rejection_report, noise_ratio
- **单位**：noise_ratio=1
- **适用域**：真实拥挤场；执行归 Wave10。
- **判据**：`galactic_center_checklist_failures == 0 count`
- **容差来源**：门度量与清单冻结（本任务）；数值由 W10 预注册后冻结。（status=pending_freeze，owner=REAL-SCIENCE-001）
- **零用例即红**：min_cases=6；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：任一清单项未过 -> 不得标 VERIFIED。
- **独立 Oracle**：kind=real_data_checklist；truth=预注册清单；must_not=solely_production_output
- **门能红 mutation**：MUT-R01
- **owner / wave / status**：REAL-SCIENCE-001 (W10) / W10 / defined_pending_execution

### `G-RD-03` — testdata 清单锚定（T2/T3/T4 数据集存在且真实）

- **claim**：真实数据门必须绑定 testdata/index.json 的实际数据集：LDN43_T2素材_flying_dutchman、NGC1727_T2、NGC247_T2、M42_T2T3_mosaic、NGC55_T3、NGC83_cluster_T3、Galaxy_Center_T4、Victory_Nebula_T4。
- **条款锚**：PROJECT_SPEC §8；testdata/index.json v1.2
- **输入 -> 输出**：testdata/index.json -> missing_datasets
- **单位**：count=1
- **适用域**：真实数据门的前置存在性；资产原地保留，禁移动/改名/删除。
- **判据**：`missing_dataset_count == 0 count`
- **容差来源**：存在性（结构证据），非测量阈值；索引版本 1.2 已登记。（status=frozen）
- **零用例即红**：min_cases=8；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：索引与实际目录不一致 -> 真实数据门不成立（不得以缺失数据集判 PASS）。
- **独立 Oracle**：kind=structural；truth=testdata/index.json；must_not=astrocs
- **门能红 mutation**：MUT-R02
- **owner / wave / status**：REAL-SCIENCE-001 (W10) / WIN-VERIFY-001 (W11) / W10 / defined_pending_execution

### `G-RD-04` — 真实数据不得作为唯一 expected

- **claim**：不得以当前程序输出生成唯一 expected；真实数据门必须配解析/MC/外部参考真值。
- **条款锚**：PROJECT_SPEC §8；SCIENTIFIC_REFERENCES §G
- **输入 -> 输出**：gate_manifest -> expected_sources
- **单位**：1=1
- **适用域**：全部门。
- **判据**：`gates_with_only_production_expected == 0 count`
- **容差来源**：结构性规则（冻结）。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：唯一 expected 来自生产输出 -> REJECT（AR-043 根因）。
- **独立 Oracle**：kind=structural；truth=PROJECT_SPEC §8 条款；must_not=astrocs
- **门能红 mutation**：MUT-SPEC-07
- **owner / wave / status**：QA-MATRIX-001 / FINAL-AUDIT-001 / W3 / frozen

### `G-RD-05` — 真实产品层/单位/BUNIT 二次律复检

- **claim**：真实产品从磁盘独立重开后层齐全、BUNIT 量纲可判、variance BUNIT=(signal BUNIT)^2、provenance 最小集完整。
- **条款锚**：FZ-BUNIT-SEMANTICS；FZ-P3-BUNIT-QUADRATIC；FZ-PROV-MINIMAL-SET；ADJ-GEN-03；DESIGN-P3 §5
- **文献锚**：IVOA HiPS 1.0 Recommendation
- **输入 -> 输出**：产品 manifest, HDU 头 -> missing_keys, bunit_ok
- **单位**：1=1
- **适用域**：Phase1/2/3 真实产品写盘。
- **判据**：`provenance_missing_keys + bunit_violations == 0 count`
- **容差来源**：键集合与量纲代数（冻结）。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：缺键/单位不可判/unavailable 无原因 -> REJECT。
- **独立 Oracle**：kind=structural；truth=FZ-PROV-MINIMAL-SET；must_not=astrocs
- **门能红 mutation**：MUT-R02
- **owner / wave / status**：IMPL-AIO-001 / SCHEMA-INTEGRATE-001 / W5 / frozen

### `G-RD-06` — Windows/Fatduck 复验门

- **claim**：Linux 结论不自动等于 Windows VERIFIED；未经 Fatduck 复验只能标 AWAITING_WINDOWS_VALIDATION；Windows 盲区检查不得恒零。
- **条款锚**：PROJECT_SPEC §10；CONTROLLER_LOG C-003/C-004；AGENTS.md 节点映射
- **输入 -> 输出**：Linux 证据, Fatduck 运行记录 -> windows_status
- **单位**：1=1
- **适用域**：发布前平台复验；执行归 WIN-VERIFY-001 (W11)。
- **判据**：`windows_status == 0 enum`
- **容差来源**：状态字面量规则（冻结）。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：未复验却标 VERIFIED -> REJECT。
- **独立 Oracle**：kind=structural；truth=PROJECT_SPEC §10；must_not=astrocs
- **门能红 mutation**：MUT-R03
- **owner / wave / status**：WIN-VERIFY-001 (W11) / W11 / defined_pending_execution

### `G-BASE-01` — 基线比较矩阵完整性 + 声明边界

- **claim**：矩阵行 = {point_information, surface_gls, psfsw_robust} x {equal, pixel_ivar}（+ deferred psf_snr_power 仅登记）；每格必须给出度量、方向、容差来源与声明边界；psfsw 只能声明优于指定基线，不得声明 Fisher 最优/ivar 等价。
- **条款锚**：FZ-MODE-PRODUCTION；FZ-MODE-BASELINE；FZ-MODE-DEFERRED；ADJ-S1；ADJ-P2-03；PSF_SIGNAL_WEIGHT §4/§7.4-7.5；DESIGN-P2 §6.3
- **文献锚**：Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872；PixInsight New Image Weighting Algorithms §2.5-2.6
- **输入 -> 输出**：baseline_matrix.json -> missing_cells, illegal_declarations
- **单位**：1=1
- **适用域**：Phase2 集成模式比较；预注册数据集优先。
- **判据**：`missing_cells + illegal_declarations == 0 count`
- **容差来源**：结构与枚举规则（冻结）；比较度量集合冻结，效应量阈值由预注册确定。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：遗漏基线/将 psfsw 当 ivar/宣称最优 -> REJECT。
- **独立 Oracle**：kind=structural；truth=FZ-MODE-*/ADJ-S1；must_not=astrocs
- **门能红 mutation**：MUT-SPEC-04, MUT-B02, MUT-B03
- **owner / wave / status**：ALG-P2-POINT-001 / ALG-P2-PSFSW-001 / P2-INTEGRATE-001 / W5 / frozen_mechanism

### `G-BASE-02` — pixel_ivar 基线在多像素 PSF 下 != W_info

- **claim**：普通像素 ivar 加权平均在 PSF 跨多像素时不等于 1/W_info 且点源方差严格更大；像素 ivar 不得宣称对任意 PSF 点源最优。
- **条款锚**：ADJ-P2-01；ADJ-P2-02；UNIFIED §5/§11；PSF_SIGNAL_WEIGHT §2；FZ-GATE-PIXIVAR-APPROX
- **文献锚**：Horne 1986 PASP 98,609 DOI:10.1086/131801；Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872
- **输入 -> 输出**：P, C, a -> loss_ratio
- **单位**：loss_ratio=1
- **适用域**：PSF 跨多像素；退化到同点采样时比值->1（声明域边界）。
- **判据**：`var_pixel_ivar/var_Winfo > 1.5 ratio`
- **容差来源**：构造性区分门，继承 SCI-PSFW-001 K4。（status=frozen）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：宣称 pixel_ivar 等价 W_info 或对其任意 PSF 最优 -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=解析方差比较；must_not=astrocs, lib/
- **门能红 mutation**：MUT-M06, MUT-B01
- **owner / wave / status**：ALG-P2-POINT-001 / ALG-P2-SURF-001 / W3 / frozen_formula

### `G-BASE-03` — psfsw_robust vs 指定基线（预注册，只声明优于）

- **claim**：psfsw_robust 在预注册 M42/银心与合成集上，与 equal/exposure/pixel-ivar/W_info 比较：分别报告 detection power、photometric variance、effective PSF、扩展源偏差与伪影；只能声明优于指定基线，不得只以看起来更好通过。
- **条款锚**：ADJ-P2-03；PSF_SIGNAL_WEIGHT §7.4/§7.5；DESIGN-P2 §6.3；FZ-GATE-PSFSW-EPSF
- **文献锚**：PixInsight New Image Weighting Algorithms §2.5-2.6；Zackay & Ofek 2017 I ApJ 836,187 arXiv:1512.06872
- **输入 -> 输出**：预注册数据集, 模式产出 -> effect_size, ci, declaration
- **单位**：effect_size=1
- **适用域**：预先注册的数据与基线；训练/调参样本 != 验收样本。
- **判据**：`effect_size (psfsw vs specified baseline) > 0 rel`
- **容差来源**：声明规则冻结（不得宣称 Fisher 最优）；效应量与 CI 由预注册/W10 冻结。（status=pending_freeze，owner=ALG-P2-PSFSW-001）
- **零用例即红**：min_cases=4；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：未预注册 / 训练样本与验收样本相同 / 宣称 Fisher 最优 -> REJECT。
- **独立 Oracle**：kind=preregistered_comparison；truth=预注册协议 + 外部/解析基线；must_not=astrocs_only
- **门能红 mutation**：MUT-B02, MUT-B03
- **owner / wave / status**：ALG-P2-PSFSW-001 / REAL-SCIENCE-001 / W10 / mechanism_frozen_tol_pending

### `G-BASE-04` — equal 基线：仅文档基线，不得声明科学最优

- **claim**：equal 模式（I_out=mean_k d_k）登记为文档基线；不得声明科学最优；用于证明门非空与对照。
- **条款锚**：FZ-MODE-BASELINE；ADJ-S1；PSF_SIGNAL_WEIGHT §4
- **输入 -> 输出**：frame_group -> I_out
- **单位**：I_out=BUNIT
- **适用域**：仅基线比较。
- **判据**：`equal_optimality_claims == 0 count`
- **容差来源**：结构性规则（FZ-MODE-BASELINE）。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：equal 声明最优 -> REJECT。
- **独立 Oracle**：kind=independent_numpy；truth=定义式；must_not=astrocs
- **门能红 mutation**：MUT-B04
- **owner / wave / status**：P2-INTEGRATE-001 / W5 / frozen

### `G-BASE-05` — psf_snr_power 延迟：不进生产路由

- **claim**：psf_snr_power 保持 DEFERRED/NOT_IMPLEMENTED，不得进入 V6 生产 weight_mode 枚举与路由；生产模式列表出现即 REJECT；必须登记在 deferred_modes。（已按 §9.73 A44 作废：该概念不存在）
- **条款锚**：FZ-MODE-DEFERRED；ADJ-C004-01；C-004.1；00_READ_FIRST；PSF_SIGNAL_WEIGHT §4
- **文献锚**：PixInsight New Image Weighting Algorithms §2.5-2.6
- **输入 -> 输出**：mode_registry -> deferred_ok
- **单位**：1=1
- **适用域**：Phase2 配置/schema/路由。
- **判据**：`psf_snr_power_in_production == 0 count`
- **容差来源**：控制器裁决 C-004.1（不得推翻）与 FZ-MODE-DEFERRED。（status=frozen）
- **零用例即红**：min_cases=1；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：生产枚举含 psf_snr_power/legacy 0/auto/support_x_snr2 -> REJECT。
- **独立 Oracle**：kind=structural；truth=FZ-MODE-* + C-004.1；must_not=astrocs
- **门能红 mutation**：MUT-SPEC-04
- **owner / wave / status**：ALG-P2-PSFSW-001 / CONTRACT-FREEZE-001 / SCHEMA-INTEGRATE-001 / W3 / frozen

### `P0-01` — R1 机器门必须能红（非看着在跑）

- **claim**：每条科学门必须有至少一个已登记 mutation 使其变红；门集合中不允许存在 mutations=[] 的门；mutation 驱动必须实测 rc!=0。
- **条款锚**：AR-041；PROJECT_SPEC §8（mutation 证明门能红）；ADJ-AR-03
- **输入 -> 输出**：qa_matrix.json, mutations.json -> gates_without_mutation, red_results
- **单位**：count=1
- **适用域**：全部门。
- **判据**：`gates_without_mutation + undetected_mutations == 0 count`
- **容差来源**：结构性规则（冻结）；对应 785 合并层账本中 R1 根因（FD-F-001/M5a-G-005/M5b-G-03）。（status=frozen）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：任何门零 mutation -> FAIL；mutation 未检出 -> FAIL。
- **独立 Oracle**：kind=independent_driver；truth=PROJECT_SPEC §8；must_not=astrocs
- **门能红 mutation**：MUT-SPEC-01, MUT-SPEC-15, MUT-SPEC-16
- **账本层**：merged_785；refs=FD-F-001, M5a-G-005, M5b-G-03
- **owner / wave / status**：QA-MATRIX-001 / RUNTIME-CI-001 / W3 / frozen

### `P0-02` — R2 子串断言/同源自证禁止（值断言）

- **claim**：禁止以子串存在性断言或调用被测实现生成期望；每门须声明 oracle.must_not_call 与 independent_truth，且真值来源不得为生产输出。
- **条款锚**：AR-042；PROJECT_SPEC §8；SCIENTIFIC_REFERENCES §G
- **输入 -> 输出**：gate.oracle -> self_proof_violations
- **单位**：count=1
- **适用域**：全部门。
- **判据**：`self_proof_violations == 0 count`
- **容差来源**：结构性规则；对应 785 合并层 R2 根因（FD-F-002/M5a-G-002/M5a-C-004）。（status=frozen）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：真值来源 = 生产输出 / must_not_call 缺失 -> REJECT。
- **独立 Oracle**：kind=structural；truth=PROJECT_SPEC §8；must_not=astrocs
- **门能红 mutation**：MUT-SPEC-07, MUT-SPEC-12
- **账本层**：merged_785；refs=FD-F-002, M5a-G-002, M5a-C-004
- **owner / wave / status**：QA-MATRIX-001 / W3 / frozen

### `P0-03` — R4 独立 Oracle 强制（解析/MC/外部参考/结构）

- **claim**：每门必须有独立真值来源（解析恒等、独立 NumPy、外部 WCS/HEALPix 参考或结构规则）；历史「Oracle 全过」不得直接继承。
- **条款锚**：AR-043；PROJECT_SPEC §8；SCIENTIFIC_REFERENCES §G
- **输入 -> 输出**：gate.oracle.kind -> gates_without_oracle
- **单位**：count=1
- **适用域**：全部门。
- **判据**：`gates_without_independent_oracle == 0 count`
- **容差来源**：结构性规则；对应 785 合并层 R4 根因（M7-G-104/M2a-F-1/M2b-F-01）。（status=frozen）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：无独立真值来源 -> REJECT。
- **独立 Oracle**：kind=structural；truth=PROJECT_SPEC §8；must_not=astrocs
- **门能红 mutation**：MUT-SPEC-08
- **账本层**：merged_785；refs=M7-G-104, M2a-F-1, M2b-F-01
- **owner / wave / status**：QA-MATRIX-001 / W3 / frozen

### `P0-04` — R10 回归锁：不得反向固化已退休 support×snr²

- **claim**：新门/规格不得把已退休的 support×snr²、legacy weight_mode=0、auto、support_x_snr2 钉成合法规格或 expected；support/coverage 只能作门。（已按 §9.73 A44 作废：该概念不存在）
- **条款锚**：AR-049；ADJ-S1；ADJ-AR-01；UNIFIED §11；宪章 §6.3
- **输入 -> 输出**：qa_matrix.json 全文本 -> fossilized_tokens
- **单位**：count=1
- **适用域**：全部门与基线矩阵。
- **判据**：`fossilized_retired_tokens == 0 count`
- **容差来源**：结构性规则；对应 785 合并层 R10 根因（M1a-F-005/M3b-A-02/M9-F-2）。（status=frozen）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：退休 token 出现在合法规格/expected -> REJECT。
- **独立 Oracle**：kind=structural；truth=UNIFIED §11 + ADJ-S1；must_not=astrocs
- **门能红 mutation**：MUT-SPEC-06, MUT-SPEC-13
- **账本层**：merged_785；refs=M1a-F-005, M3b-A-02, M9-F-2
- **owner / wave / status**：QA-MATRIX-001 / CONTRACT-FREEZE-001 / W3 / frozen

### `P0-05` — 785 合并层账本 P0 门族（带层声明）

- **claim**：785 缺陷账本为合并层口径（667+83+29+4+2=785），叶子层 523 个 L-id 结构性不可表达；任何账本数字必须带层号；本门族对 R1/R2/R4/R5/R10 根因逐条给出销账门。
- **条款锚**：AR-035；AR-051；AR-041；AR-042；AR-043；AR-044；AR-049；reports/v6/review-audit/03
- **输入 -> 输出**：DEFECT_LEDGER.json（只读） -> layer_declared, p0_family_cover
- **单位**：count=1
- **适用域**：缺陷账本口径与 P0 门族覆盖；处置权在控制器/负责人（本任务只登记）。
- **判据**：`ledger_numbers_without_layer + uncovered_p0_root_causes == 0 count`
- **容差来源**：口径规则冻结：所有账本数字带 merged_785/leaf_523；覆盖 R1/R2/R4/R5/R10。（status=frozen）
- **零用例即红**：min_cases=5；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：账本数字无层号 / P0 根因无承载门 -> REJECT。
- **独立 Oracle**：kind=structural；truth=reports/v6/review-audit/03 + DEFECT_LEDGER.json；must_not=astrocs
- **门能红 mutation**：MUT-SPEC-09
- **账本层**：merged_785；refs=AR-035, AR-051
- **owner / wave / status**：QA-MATRIX-001 / FINAL-AUDIT-001 / W3 / frozen_registration_only

### `P0-06` — R5 阈值可达/引用真实（不虚构依据）

- **claim**：每个数值容差必须带 threshold_status 与真实来源：frozen（引用 FZ/SCI-ADJ/W1 Oracle 实测）或 pending_freeze（带 owner ALG/W4，任务不得自称冻结）；禁止虚构依据或不可达阈值。
- **条款锚**：AR-044；ADJ-AR-03；FZ-GATE-PIXIVAR-APPROX；SCI-ADJ-001 FREEZE_LIST §7 SO-07
- **输入 -> 输出**：gate.criterion/tol_source -> invented_thresholds
- **单位**：count=1
- **适用域**：全部门的容差字段。
- **判据**：`thresholds_without_real_source == 0 count`
- **容差来源**：规则：frozen 必须有 FZ-/ADJ-/W1 实测锚；pending_freeze 必须有 owner；否则 REJECT。（status=frozen）
- **零用例即红**：min_cases=2；rc=2 if executed_cases==0 or skipped_cases>=executed_cases
- **fail-closed**：自称冻结但无锚 / pending 无 owner -> REJECT。
- **独立 Oracle**：kind=structural；truth=SCI-ADJ-001 FREEZE_LIST 19 required_freeze_ids + ADJ id 集合；must_not=astrocs
- **门能红 mutation**：MUT-SPEC-10, MUT-SPEC-14
- **账本层**：merged_785；refs=AR-044
- **owner / wave / status**：QA-MATRIX-001 / CONTRACT-FREEZE-001 / W3 / frozen

<!-- QA-MATRIX-DETAILS-END -->

## 10. 未决风险与需裁决事项

1. **数值阈值缺口（SO-07）**：surface_gls 的 epsilon、F-OBS-03 的 deficit 阈值、F-OBS-04 的数据面、F-OBS-05 的 k_corr 标定脚本、
   psfsw 深度不变性 <5%、帧级标量降级阈值均为 `pending_freeze`，owner 已登记；本任务只冻结度量与门存在性。
2. **真实数据执行**：G-RD-01/02/06 与 G-BASE-03 在 W10/W11 执行；本任务只定义门并登记 pending（counts_as_pass=false）。
3. **F1 基线分歧（CTRL-F1）**：只登记不裁决；本任务规格锚点在 HEAD 上自洽。
4. **AR-033 根构建面（CTRL-AR033）**：只登记；本矩阵的 Python 校验器不需要根 CMake 注册。
5. **需负责人签字项**：SO-01..SO-07 只登记不签署（详见 FREEZE_LIST §7 与 §10 相关门 `signoff` 字段）。
6. **785 缺陷账本**：合并层口径（667+83+29+4+2=785），叶子层 523 个 L-id 结构不可表达；P0 门族统一带 `merged_785`（见 P0_GATE_FAMILY.md）。
