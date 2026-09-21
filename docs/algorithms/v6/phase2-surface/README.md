> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# Phase2 扩展源（surface_gls）V6 算法规格 — 索引与冻结常量

> 上游：ASTROCS_DESIGN.md §4（normalize）、§5（mosaic）、§6（export）

- 文档 ID：`ALG-P2-SURF-001`（本目录整体）
- 任务：`工程控制/旧 V6 控制包（ROOT-007 已删除）/tasks/ALG-P2-SURF-001.md`
- wave：3；depends_on：`SCI-ADJ-001`
- 基线：`HEAD = main = 125bc0999363be1a42a1f2df3254601e0cc7b8fb`（本机 `git rev-parse` 实测；工作树含未裁决回退，见 §基线分歧）
- 写域：`docs/algorithms/v6/phase2-surface/`（人读规格）+ `run/v6/alg-p2-surf/`（机器规格/证据/日志，任务卡 `00_READ_FIRST` 与产出要求指定）
- 性质：**算法规格冻结（W3）**。不改科学公式/冻结门/容差，不写生产源码，不 commit/push，不派生子代理。
- 机器规格伴生：`run/v6/alg-p2-surf/spec/alg_p2_surf_spec.json`（`astrocs.alg-p2-surf/spec/v1`）

## 1 范围与上位权威

本任务把 `DESIGN-P2-001 §4/§5/§6.1/§7/§9` 与 `UNIFIED §5/§6/§7`、`SCI-P2-001-COVARIANCE-EPSF` 展开为**可实施、可验证**的 `surface_gls` 算法规格，覆盖六项：

1. GLS 扩展源解与设计矩阵（`ALG-P2-SURF-GLS.md`）；
2. pixel-ivar 近似门与 **ε 数值冻结**（`ALG-P2-SURF-PIXIVAR-GATE.md`）；
3. UPM 乘法/加性分离 + gauge/秩/条件数/参数协方差（`ALG-P2-SURF-UPM.md`）；
4. rejection reason/probability（`ALG-P2-SURF-REJECTION.md`）；
5. covariance 传播（`ALG-P2-SURF-COVARIANCE-EPSF.md`）；
6. effective PSF 规格（同上）。

权威分层（`SCI-ADJ-001_CONFLICT_MATRIX.md §0`）：宪章 `ASTROCS-CONSTITUTION-001` §4.1/§4.3/§6.3 > `PROJECT_SPEC §3/§5/§7/§8` > `DESIGN-P2-001` > `UNIFIED/SCI-PSFW` > 专项 SCI/ALG/DATA/API > 代码/测试。冲突时以高层为准，本目录不得反向改写上位规范。

条款锚约定：继承条款用 `SC-ADJ-*`/`FZ-*`（`SCI-ADJ-001`）；本任务新增条款用 `ALG-P2S-<域>.<n>`；本任务新增数值冻结用 `FZ-AP2S-*`（W3 冻结，W4 `CONTRACT-FREEZE-001` 正式化）。

## 2 冻结常量表（机器可解析；与 spec JSON `frozen_constants` 逐值一致）

| freeze_id | value | unit | subject | gate |
|---|---|---|---|---|
| `FZ-AP2S-EPS-PIXIVAR` | 0.05 | 1 | pixel-ivar 近似 p95 方差膨胀门 ε | rho_p95 <= 1+eps；无声明即 REJECT |
| `FZ-AP2S-EPS-PIXIVAR-SUP` | 0.20 | 1 | pixel-ivar 逐元素硬上限（4ε） | rho(p) > 1+sup 的元素 fail-closed |
| `FZ-AP2S-MC-RELTOL` | 0.03 | 1 | 协方差 vs Monte Carlo 相对容差 | 超限即失败 |
| `FZ-AP2S-IDENT-RTOL` | 1e-09 | 1 | G C Gᵀ == (AᵀC⁻¹A)⁻¹ 恒等容差 | 超限即失败 |
| `FZ-AP2S-EPSF-RTOL` | 1e-12 | 1 | effective PSF 注入 vs 解析组合容差 | 超限即失败 |
| `FZ-AP2S-KAPPA-MAX` | 1000000.0 | 1 | UPM 列均衡加权正规矩阵条件数上限 | 超限 -> 分组件/unavailable |
| `FZ-AP2S-RANK-RTOL` | 1e-10 | 1 | UPM Jacobian 奇异值相对秩判据 | rank < n_params-2*n_components 即 REJECT |
| `FZ-AP2S-UPM-MINFRAMES` | 2 | 1 | 乘法尺度可辨识所需每分量最少帧数 | 单帧区禁拟 g，须显式 additive-only 降级或 unavailable |
| `FZ-AP2S-REJ-CALIB-BINMIN` | 50 | 1 | rejection probability 校准分箱最少样本 | 分箱样本不足 -> 该箱不参与但须登记覆盖 |
| `FZ-AP2S-REJ-CALIB-ABS` | 0.10 | 1 | rejection probability 可靠性绝对容差 | 观测拒绝率与 mean(p) 偏差超限即失败 |
| `FZ-AP2S-REJ-BSS-MIN` | 0.10 | 1 | rejection probability Brier 技巧分下限 | BSS <= 下限即失败 |

**单位表（原样继承 `ADJ-GEN-01`/FREEZE_LIST §1）**：`signal_sb=ADU/px^2`、`pixel_variance_in=ADU^2`、`sb_variance_out=ADU^2/px^4`、`sb_ivar_out=px^4/ADU^2`、`W_info=ADU^-2`、`psfsw_robust_weight=1`、`Q=ADU^-1`、`flux=ADU`；二次律 `variance=signal^2`、`ivar=1/variance`。本任务**不重新定义**这些单位。

## 3 继承的冻结条目（不得矛盾）

| 冻结条目 | 本任务落点 |
|---|---|
| `FZ-FORMULA-GLS` | `ALG-P2-SURF-GLS.md` §2（权威式原样） |
| `FZ-GATE-PIXIVAR-APPROX` | `ALG-P2-SURF-PIXIVAR-GATE.md`（ε 数值由本任务冻结） |
| `FZ-FORMULA-COV-PROP` | `ALG-P2-SURF-COVARIANCE-EPSF.md` §1 |
| `FZ-GATE-PSFSW-COV` / `FZ-GATE-PSFSW-EPSF` | `ALG-P2-SURF-COVARIANCE-EPSF.md` §1/§3（surface_gls 侧同构，psfsw 严格边界引用） |
| `FZ-GATE-MEDIAN-SNR` / `FZ-GATE-SUPPORT-COVERAGE` | 所有文件 "禁止项" 节 |
| `FZ-MODE-PRODUCTION` / `FZ-MODE-DEFERRED` | `ALG-P2-SURF-GLS.md` §1（`psf_snr_power` 不得进生产，C-004.1） |
| `FZ-PROV-KCORR` | `ALG-P2-SURF-UPM.md` §5（k_corr 定义/适用域继承） |
| `FZ-PROV-MINIMAL-SET` / `FZ-BUNIT-SEMANTICS` | 各文件 provenance 节 |
| `FZ-DEGRADE-SCALAR` | `ALG-P2-SURF-GLS.md` §6 |
| `FZ-COND-WHITENOISE` | `ALG-P2-SURF-GLS.md` §2（条件式） |

## 4 文件

| 文件 | 内容 |
|---|---|
| `README.md` | 本索引、冻结常量表、继承条目、基线分歧 |
| `ALG-P2-SURF-GLS.md` | surface_gls 目标函数/设计矩阵/输入输出/单位/适用域/fail-closed/验证门 |
| `ALG-P2-SURF-PIXIVAR-GATE.md` | pixel-ivar 近似门：结构性条件 + ε=0.05/p95 + 0.20 硬上限 + 报告要求 + 负向判据 |
| `ALG-P2-SURF-UPM.md` | UPM `y=g s+b` 乘加分离/gauge/秩/κ/参数协方差/k_corr |
| `ALG-P2-SURF-REJECTION.md` | reason 分类 + probability 定义 + 校准门 + fail-closed + 小样本/移动源 |
| `ALG-P2-SURF-COVARIANCE-EPSF.md` | C_out=R C_in Rᵀ（含 UPM 项）+ 相关核 + effective PSF 定义/归一/验证 |
| `ALG-P2-SURF-VERIFICATION.md` | 验证命令/rc/Oracle/结构一致性/负向 mutation 实测 |

## 5 验证与证据（`run/v6/alg-p2-surf/`）

| 产物 | 说明 |
|---|---|
| `run/v6/alg-p2-surf/spec/alg_p2_surf_spec.json` | 机器可读规格（本目录冻结常量的语义源再镜像） |
| `oracle/surf_oracle.py` | 独立 NumPy Oracle（GLS/pixel-ivar/UPM/rejection/covariance/effective PSF） |
| `oracle/check_spec.py` | 结构 + 文档/JSON 一致 + 冻结继承一致性检查门 |
| `oracle/run_gate_and_mutations.py` | 正向控制 + 负向 mutation + Oracle 自 mutation |
| `oracle/run_all.sh` | 一键复跑（单一 rc） |
| `summary.json` | 机器汇总（基线 SHA、计数、rc、建议状态） |
| `logs/` | 命令日志（含 rc 行） |
| `mutations/` | 每个被注入错误的记录 |

## 6 基线分歧声明（强制）

工作树**不是** HEAD 的干净副本（`CTRL-F1`：16 tracked 回退 + 10 tracked 删除，`CONTROLLER_LOG C-004.6`，**未裁决**）。
本任务一切**生产面判定以已提交 HEAD 为准**（`git show HEAD:<path>` 读取），不把回退态当已验证基线。
F1 与 AR-033 为控制器级事项，本任务**只登记不裁决**（见 `ALG-P2-SURF-VERIFICATION.md §6`）。

## 7 明确不做

- 不改 `docs/science/*`、`docs/owner/**`、`docs/design/**`、`docs/references/**`、`eng/contracts/**`、生产源码、根/公共 CMake；
- 不写入 schema 实现（词表归一归 `SCHEMA-INTEGRATE-001` W6，C-004.3）；
- 不实现算法（`IMPL-P2-UPM-001`/`IMPL-P2-REJ-001`/`IMPL-P2-SAMP-001` W5、`P2-INTEGRATE-001` W8）；
- 不放行 `psf_snr_power`（C-004.1）；
- 不 commit/push、不派生子代理。
