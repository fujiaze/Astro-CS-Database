# Statistics / Sky / Coadd 修复报告（task-8）

角色：统计实现者（非独立审查者）。受审缺陷清单：[audit-statistics.md](audit-statistics.md)（受检 `688adeac7`）。
本报告记录修复内容、数学依据、独立探针结果、未修项，以及交给 Lead 接线的前置/后置条件。

- 分支：`rebuild/acsd-v2`；工作树基线 `ca7de6d4d`（`688adeac7` 为其祖先，受审文件此后无他人改动）。
- 写域：`src/sky/*`、`src/coadd/*`、`src/uncertainty/*`、本文件。未改 `src/workflows`、`src/projection`、`src/model`、`src/numerics`、其它 docs；未 commit/push。
- 未改任何测试容差/真值；未新增 CI 门禁。
- 构建：`cmake --build build/science -j16` → 通过（0 error，含 `check_design.py` 的既有设计完整性检查）。
- 独立探针：`/tmp/acsd_probe/probe.cpp`（只用 `include/acsd/contracts.hpp` 与 `validation/stats/harness.hpp`，不 include `src/`，不复用被测 helper；每个期望值在探针内按定义重算）。
  - 修复后：**111 项通过 / 0 失败**（`/tmp/acsd_probe/after.txt`）。
  - 同一探针对 `HEAD` 基线库（`/tmp/acsd-base/build/base`，由 `git archive HEAD` 导出）运行：**23 项失败**（`/tmp/acsd_probe/before.txt`）。
    这 23 条失败就是审计 9 个反例在旧实现上的可执行形式。
- 既有验证：`validation/stats/{sky_test,coadd_test}` 0 失败。`validation/stats/information_test` 有 1 条既有失败（`the cancelled mesh fails with the code cancelled`），在基线库上同样失败，与本任务无关（`src/information` 不在写域）。

---

## 0. 结论摘要

| 项 | 审计裁决 | 本任务状态 |
|---|---|---|
| P0-3 等权分支伪造 `Var=1/K` | 需修 | **已修**：等权方差 = `tᵀΓt`；`Γ` 未知 → `unknown`（不填 0、不填 `1/K`） |
| P0-4 coadd 逐叶对角化 | 需修 | **已修**：发布 `W C Wᵀ`（保留跨叶 shared root），`W` 作为 `CovarianceAction.influence` |
| P0-2 `unknown_terms` 丢失/重标 known | 需修 | **已修**：sky/coadd 联合装配搬 unknown 并保 `state`；blue/QP 检查 Γ 完整性 |
| P0-5 `c` 与 `T` 不同源、`A≠TE`、π chart 静默留零 | 替换 | **已修**：同一 mask 产 `c` 与 `T`；`A=T E`；chart 超界具名拒绝 |
| P0-6 PSD 精确零空间被丢弃 | 需修 | **已修**：先解精确方程 `U0ᵀc=U0ᵀ[H,AG]η`，再在解集内做 range GLS；不一致 → `invalid_model` |
| P1-8 同 `SharedId` 非相邻不合并 | 需修 | **已修**：按身份全量分组；factor/coordinates/unit 身份校验 |
| P1-9 控制协方差失败静默 equal | 需修 | **已修**：malformed 传播；科学性 unknown 才降级，且具名 + `unknown` + issue |
| P1-10 nullspace 修正与发布方差不同估计量 | 需修 | **已修**：`P' = I − M E K T`，信号与方差同一算子 |
| P1-11 `Σt=1` / 选择条件 | 需修 | **部分修**：`Σt=1` 在 solver 与 `combine_sealed` 双向自检；选择条件写进 assessment（`condition` 是下游身份比较字段，不能加 marker）。`effective_response`/revision 身份未修（见 §8） |
| P1-12 `geometry_covariance_bound` 不校验 `E≥0` | 需修 | **已修**：`E` 逐元素非负证书；不可证 → `bound=null` |
| P1-13 选择条件冒充无条件覆盖 | 需修 | **已修**（同 P1-11） |
| P0-1 `apply_sky` 工作流接线 | Lead | **未动**（越界），§7 给出前置/后置条件 |
| P0-7 `export Q C Qᵀ` | Lead | **未动**（越界），§7 给出前置/后置条件 |

---

## 1. P0-3：等权分支不得伪造 `Var = 1/K`

**数学依据**（基础 §1.5 式 (11)）：`B̂ = tᵀz`，`V = tᵀΓt`。等权是 `t=(1/K,…,1/K)` 的**合法但非精度最优**策略，其方差仍是 `(1/K²)1ᵀΓ1`，不是 `1/K`。`Γ` 未知时该值 `unknown`，已知部分只能作诊断。

**修改**：`src/coadd/coadd.cpp:853 solve_weights`
- 每个 leaf 的 `Γ` 块（`covariance_block`，可接受帧子块）用于**所有**策略；方差统一按 `Σ_ab t_a Γ_ab t_b` 计算（`coadd.cpp:1040-1055` 区域）。
- 删除 `variance = 1/K`（旧 `coadd.cpp:912-914`）。
- `Γ` 完整性判定：所有帧有 noise model、`state==known`、无 material `unknown_terms`、联合协方差可构造。
- `Γ` 不完整时：等权/automatic 仍求解但方差 `unknown`；显式 `blue`/`nonnegative_qp` → `Code::unavailable`（不降级）。

**反例（CE-G `Γ=[[1,2],[2,5]]`，2 帧 1 叶，shared root）**
| 量 | 真值 | 修复前 | 修复后 |
|---|---|---|---|
| 等权 `t` | (1/2,1/2) | (1/2,1/2) | (1/2,1/2) |
| 等权 `V` | **5/2** | 1/4（探针测得 0.25） | **2.5** ✓ |
| BLUE `t` / `V` | (3/2,−1/2) / **1/2** | 权重正确 / V=0.25 | V=**0.5** ✓ |
| 非负 QP `t` / `V` | (1,0) / **1** | 权重正确 / V 缺失 | V=**1** ✓ |

模块冻结例 `Γ=diag(1,4)`（两帧独立 innovation，同时验证“帧 2 的 innovation 不落到帧 1 行上”）：等权 `V=5/4`（旧：0.25）、BLUE `t=(4/5,1/5)`、`V=4/5`（旧：BLUE 直接失败）。

`Γ` 未知（帧无 noise model）：旧代码发布 `V=0.25, state=known`；现在 `value=null, state=unknown`，且显式 blue → `unavailable`。

---

## 2. P0-4：coadd 协方差不得逐叶对角化

**数学依据**（基础 §2 式 (6)、§3 式 (5)）：`C_z = P C Pᵀ`；同一 `SharedId` 必须先合并影响再平方，否则丢掉交叉项。逐叶对角化等于把 `Σ_k t_k L_k` 拆成独立根，正是 CE-C/CE-H 的错误。

**修改**：`src/coadd/coadd.cpp:1107-1215`（组合算子 + 协方差发布）
- 构造 `W`（`leaf_count × total_leaves`，`W[ℓ, frame_offsets[f]+row] = t_{ℓf}`）。
- 发布 `CovarianceAction`：`model` 为**联合模型**（根、因子、shared id 全保留），`influence = W`。`apply_covariance` 的定义正是 `C(v) = I C_model Iᵀ`，于是 `C = W C_joint Wᵀ`，跨叶项自动保留，不需要逐根重建。
- 未知情形：`model` 带 material `unknown_term`（`coadd.cpp:1191`），`influence = W`，`state=unknown`；不发布任何伪造对角。

**反例（CE-H，2 叶 1 帧，`C = I₂ + 11ᵀ = [[2,1],[1,2]]`，`t=1`）**
| 量 | 真值（按定义重算） | 逐叶对角化 | 修复前实测 | 修复后 |
|---|---|---|---|---|
| `Var(z₁)` | 2 | 1 | 1 | **2** ✓ |
| `Var(z₁+z₂)` | `(1,1)ᵀC(1,1)=6` | 4 | 2 | **6** ✓ |
| `Var(z₁−z₂)` | `(1,−1)ᵀC(1,−1)=2` | 4 | 2 | **2** ✓ |

**审计报告的算术笔误（已复核）**：审计 CE-H 写 “每叶 V=2；Cov=1；Var(z₁+z₂)=4+4+2·1=10，Var(z₁−z₂)=0”。按其自己给出的矩阵 `C=I₂+11ᵀ=[[2,1],[1,2]]`，对角为 2（不是 4），真值是 `Var(和)=6`、`Var(差)=2`。`10/0` 只对应 `C=2I+11ᵀ` 或 `C=2·11ᵀ`（此时 `Var(和)=8, Var(差)=0`）。**缺陷方向与结论不变**（逐叶对角化给出 4/4），但数字须按矩阵更正；本报告以探针重算值为准。

---

## 3. P0-2：`unknown_terms` 必须传播，不得重标 known

**数学依据**：任何对请求泛函可能非零的未知项使完整方差 `unknown`（基础 §1.1 末段）。已知部分只能作具名下界。联合装配必须搬 unknown，且帧的根影响必须落到**该帧的行偏移**上（否则帧 2 的 innovation 落到帧 1 的行，`Γ` 结构错误）。

**修改**
- `src/sky/sky.cpp:756 build_joint_noise`：每帧影响按 `frame.offset` 用 `shift_rows(..., total_rows)` 放置；`unknown_terms` 逐个搬入并加行偏移；无 noise model 的帧生成**显式** unknown term（行单位阵），而不是静默跳过；`joint->state = unknown`（任一 unknown）并写入 `assessment.noise`。
- `src/coadd/coadd.cpp:224 build_joint_covariance`：同上（本地 `place(...)`/shared root 合并均按 `total_rows` 补齐零行）。
- `src/sky/sky.cpp:1615 fit_relative_sky`：发布前检查 `controls->noise->model->state` 与 material `unknown_terms`；`SkyFit.assessment.noise = unknown`，`science = conditional`，issue 具名，参数协方差的 `quadratic_form` 返回 `unknown`（`parameter_cov` 继承 `unknown_terms`）。
- `src/coadd/coadd.cpp`：`blue`/`nonnegative_qp` 在 `Γ` 不完整时 `unavailable`；`automatic` 降级 equal 并保存原因。
- `src/uncertainty/api.cpp:194 make_covariance`：允许“无已知根但带 material unknown term”的模型（已知部分为空 ≠ 方差为 0）。

**反例结果**
- 帧 noise 带 material unknown term：等权方差 旧 `0.25/known` → 现 `null/unknown`；`automatic` 不再选 BLUE（equal + 原因）；显式 QP → `unavailable`。
- 帧完全没有 noise model（正是当前 `validation/x01` 发布产品的形态：`assessment.noise=unknown, noise=None`）：controls 降级为**具名** equal（issue + `omitted known_control_covariance`），fit 仍给出点估计（offset 2 恢复，误差 4.4e-16），`assessment.noise=unknown`，参数方差 `null/unknown`；coadd `automatic → equal`，联合方差 `unknown`。链路不中断、不伪造已知方差。

---

## 4. P0-5：`c` 与 `T` 同源；`A = T E`；chart 超界具名拒绝

**数学依据**（基础 §4 式 (6)、§1.3）：`c = T y` 必须是**同一个** `T`；`A = T E`（`E` 是同一 basis 在输出样本上的求值，`A` 不能另取插值器/位置）。Gnomonic chart 只在半球内有效，`project_to_chart` 在 `cos ≤ 1e-9` 时失败并返回空行——静默留零等于丢掉常数列。

**修改**：`src/sky/sky.cpp:1072-1335 build_sky_controls`
- 控制行改为“一帧一 patch 一条记录”：先固定 `mask`（组内公共支持 ∩ 本帧 DQ），`c` 由该 mask 求均值，`T` 行由**同一** mask 填 `1/|mask|`（旧代码重建 mask 时漏掉公共支持条件）。
- `A` 改为 `numerics::matmul(T, E)`（旧：在 patch 中心另算 basis）；显式 `T` 路径同样 `A=T E`，并拒绝“一行跨多帧”的显式控制行（component 身份与 per-frame gauge 未定义）。
- chart：半径取控制位置与所有叶中心的实际最大张角；`terms>1` 且 `> 1.4 rad` → `Code::unsupported` 具名拒绝；常数基不依赖 chart（`evaluate_basis` 对常数项短路）；任何 basis 求值不足 `terms` 列 → `invalid_model`（不再静默留零）。
- 控制协方差构造失败分类：`invalid_model/identity_conflict/invalid_request/invalid_covariance` **传播**（输入错误不降级）；`unavailable/unsupported` 或“联合协方差无已知部分”才降级为具名 equal（issue 带原因，`noise=unknown`）。

**反例结果**
| 反例 | 真值 | 修复前 | 修复后 |
|---|---|---|---|
| 帧1 支持 {A,B}、帧2 支持 {B}，`y1=(10,2)` | `c₁=2`，`T y=2` | `c₁=2` 但 `T y=6`（差 4 进差模） | `c₁=2`，`T y=2` ✓ |
| 平面基，mask 只剩 x=+1 的叶 | `A = T E = (1, −0.822, 0.0318)` | `A=(1,0,0)`（patch 中心） | 与 `T E` 逐元素相等 ✓，且 `≠` patch 中心值 ✓ |
| 4 叶跨 4 个 face（张角 > 1.4 rad），平面基 | 拒绝 | 静默零列 | `unsupported` + 具名原因 ✓ |
| 同上但常数基 | 可建 | 可建 | 可建（不需要 chart）✓ |

---

## 5. P0-6：`C_c` 的 PSD 精确零空间

**数学依据**（基础 §1.3 末、§4 式 (7)；`sky.fit.md`“PSD 噪声先处理精确约束”）：PSD 的零方差方向是**精确方程** `U0ᵀc = U0ᵀ[H, A G]η`，不是“无穷不确定”。正确顺序：先用精确方程确定可估方向的约束，再在白噪声（range）子空间做 GLS；不一致 → `invalid_model`。直接只用 `W=UΣ⁻¹Uᵀ` 会丢掉该方程（CE-E：点估计错、已知方差从 0 变成 1）。不得给 `C` 加 epsilon 地板。

**修改**：`src/sky/sky.cpp:2052-2300 fit_relative_sky`
1. `U0` = 因子值域（`rank` 列）在控制空间的正交补（`complete_orthogonal_factorisation`）。
2. `N_s = U0ᵀH`，`P0 = I − N_s N_s⁺`：公共天光 `s` 是自由 nuisance，只能吸收 `range(N_s)` 内的分量。
3. `C = P0·U0ᵀAG`、`d = P0·U0ᵀc`；一致性检查 `‖d − C C⁺d‖∞ ≤ 1e-8·max(1,‖d‖∞)` → 否则 `invalid_model`。
4. `η̂ = η_p + Z α`，`Z = null(C)`，`α` 为 `design·Z` 上的最小二乘 → **约束 GLS**。
5. `K = G[C⁺P0U0ᵀ + Z(DZ)⁺(Q_wᵀW − D C⁺P0U0ᵀ)]`（对 `c` 线性，与 `β=Kc` 一致）。
6. 不可估方向 = `null([D;C])`（`rank` 报告、`nullspace`、`unknown term` 都用它）。
7. **秩判据按问题尺度**（`rank_relative·max(1,‖H‖_F,‖AG‖_F)`），否则 `U0ᵀH` 的舍入噪声（1e-16）会被“相对自身最大奇异值”判成 rank 1，`P0` 归零（这是实现过程中探针抓到的真实陷阱）。

**反例（CE-E 的可连通变体）**：2 帧 × 2 控制，`H` 一列公共天光，`C_c` 在 `u=(1,−1,0,0)/√2` 方向精确零方差，`uᵀH=0`，`uᵀAG=1/2`，`c=(1,0,0,0)` ⇒ `η=√2`，`β=Gη=(1,−1)`，`V=0`。

| 量 | 真值 | 修复前 | 修复后 |
|---|---|---|---|
| `β` | (1, −1) | (1/3, −1/3) | (1, −1) ✓ |
| `Var(β₁)` | 0 | 缺失/不可用 | 2.5e-32 ✓ |
| `Var(β₁−β₂)` | 0 | 缺失 | 4.9e-32 ✓ |
| 精确方程不一致（`A=0`, `uᵀc≠0`） | `invalid_model` | 静默拟合 | `invalid_model` ✓ |

**关于审计 CE-E 的可达性**：审计写“`H=0` 且 `β₁+β₂=0` 一个连通分量”。实现的连通分量来自 `H` 的共享列，`H=0` 时两帧是两个分量、每分量 gauge 自由度 0，`β≡0`——反例的前提在该实现里不可达（结论“点估计错”仍成立，但走的是另一条路径）。本报告用可连通的等价变体（`H` 一列、`u⊥H`）取得同样的判定，且额外覆盖 `P0`（公共天光吸收）分支。

---

## 6. P1 项

### 6.1 P1-8 同 `SharedId` 全量合并（`src/uncertainty/api.cpp:96`）
按身份（`external_namespace/entity/model_revision/component`）对**整个** `shared_factors` 列表分组（不是相邻扫描），每组先合并影响再平方；组内校验 unit、coordinates（size/身份）、factor（CSR/diagonal 载荷逐元素；否则身份或形状）。
- CE-D `[S,T,S]`：旧 3 → 新 **5** ✓；C-ROOT（同根方差 4，影响 +1/−1）：**0** ✓（旧按根相加为 8 的口径）。

### 6.2 P1-12 `E ≥ 0` 证书（`src/uncertainty/api.cpp:464,531`）
`absolute_error_sign`：CSR/diagonal/selection/identity/compose/sum 载荷可判定；无载荷且 `rows·cols ≤ 2^20` 时按域基向量逐列探测；否则 `undecided`。
- `R0=1, E=−5, t=1, C=1`：旧返回 **−35**（负“上界”），现 `bound=null` + 具名原因 ✓。
- 对照 `E=5`：`bound=35`，包含真值 `|36−1|=35` ✓。
- 顺带修：`has_absolute = covariance->absolute_factor_bound != nullptr` 对**空 optional** 为真（`optional != value` 在 disengaged 时为 true），导致所有无绝对因子界的协方差都走绝对分支并报“the absolute factor bound cannot be applied”，谱范数路线不可达。改为 `has_value() && *opt != nullptr`。

### 6.3 P1-9 控制协方差失败不得静默 equal（`src/sky/sky.cpp:1338`）
错误分类见 §4。降级时必须：`assessment.noise=unknown`、`omitted known_control_covariance`、`included equal_weights_unknown_covariance`、issue 带**原始失败原因**。`SkyFit` 发布前检查 `controls->noise` 状态（§3）。

### 6.4 P1-10 nullspace 行：信号与方差同一估计量（`src/sky/sky.cpp:2446 apply_sky`）
`P' = I − M E K T`，`M=diag(m_i)`，不可估位置 `m_i=0`（该行不修正）。信号 `z=P'y` 与协方差 `P' C_y P'ᵀ` 由同一算子产生。
- 反例（`E=K=T=I₂`，null 方向 `(1,0)`，`C_y=diag(1,4)`）：位置 0 不可估 ⇒ `z₀=y₀=7`；旧方差 4（`P C Pᵀ=0` 的实现差）→ 现 **1 = (C_y)₀₀** ✓；位置 1 为 **0** ✓。

### 6.5 P1-11/P1-13 `Σt=1` 与选择条件
- `solve_weights`：每叶权重和 `|Σt−1| ≤ 1e-9`，违反 → `invalid_model`（求解器自检）。
- `combine_sealed`：封存解的有效权和同样检查（`coadd.integrate.md` 的“active sum=1 数值包围须含 1”），违反 → `invalid_model`。
- `Selection`：`assessment` 记录 `conditional_on_frozen_selection_mask`；启用统计排异时 `omitted unconditional_coverage_after_selection` + `science=conditional` + 具名 issue。`WeightSolution`：`included weights_sum_to_one_per_leaf`、`included combined_covariance_keeps_the_shared_roots`；未知方差时 `omitted known_combined_variance`。
- **注意**：`condition` 字段是下游 `validate_inputs` 的**身份比较**对象（`same_condition`），加入 marker 会使 `coadd_with_inputs` 报 `identity_conflict`（实现过程中已实测），所以 marker 放 `assessment`。

---

## 7. 交给 Lead 接线的前置/后置条件（`apply_sky` 与 `export Q C Qᵀ`）

### 7.1 `apply_sky` 边（P0-1，未动）
前置条件（sky 侧已保证）：
1. `fit` 必须由**同一** `SkyControls` 产生；`fit->E` 的行空间 = 被校正信号的叶空间（`apply_sky` 校验 `Eβ` 与 `signal` 长度一致，否则 `invalid_model`）。
2. `signal->noise` 若已带 `CovarianceAction.influence`，`apply_sky` 返回 `unsupported`（组合两个外作用需要 `compose` 的转置，见 §8.1）。工作流应传入 `make_covariance` 直接产出的噪声（`influence == nullptr`）。
3. `signal` 的叶顺序/几何须与 `fit` 的训练帧一致：`SkyFit` 目前**没有** revision/几何身份字段（`include/acsd/model.hpp`，非本任务写域），工作流必须自行保证同一 provider。

后置条件（工作流可直接依赖）：
1. `apply_sky(...)->signal` = `P'y`，`P'` 对不可估行为单位行；`apply_sky(...)->noise` 的 `model` 与输入噪声**同一模型**（根/shared/unknown 不变），`influence = P'`，因此 `quadratic_form(noise, t) = tᵀP'C_yP'ᵀt`。
2. `assessment.science=conditional` 且带 `unavailable` issue，当存在不可估位置。
3. 校正后的 `SignalEstimate` 保留 `estimator`/`support`/`effective_response`（当前为 `nullptr`，来自上游）。
4. 顺序要求：`sky.controls → sky.fit → apply_sky → coadd.select → coadd.weights → coadd.integrate`。`coadd` 侧假定 provider 已绑定校正后的 signal/noise（`coadd.integrate` 不再次 apply_sky）。

### 7.2 `export` 的 `Q C Qᵀ`（P0-7，未动）
前置条件：
1. `estimate->noise` 的 `input_output.domain` 现在是**叶空间**索引（`size = leaf_count`，`kind = leaf`），`influence` 承载 `W`（coadd）或 `P'`（apply_sky）。`export` 若要 `Q C Qᵀ`，必须把 `Q` 与既有 `influence` 组合：`C_out = (Q·I) C_model (Q·I)ᵀ`。
2. `CovarianceAction.influence` 的语义是 `C(v) = I·C_model(Iᵀv)`：`Q C Qᵀ` 不能靠“再挂一个 influence”实现（会丢内层 `I`），正确做法是 `Q∘I` 的组合作用（需要 §8.1 的 `compose` 转置修正，或在 projection 侧直接用 `multiply_csr` 式精确乘积）。
3. 单位：`export` 把信号单位改成 `sr⁻¹`/`sr⁰`，协方差单位仍是叶面亮度方差；`Q C Qᵀ` 必须同时乘上单位幂（`Q` 的系数单位），否则数值与单位不一致。
4. `export` 的 `Q` 行是目标像元的功能泛函；跨叶共享根已由 `model` 保留，`Q C Qᵀ` 会得到正确的跨叶相关（这是 P0-4 修复的直接收益）。

---

## 8. 未修 / 未决（含写域外的跨模块缺陷）

### 8.1 `src/operators/linear_action.cpp:326-333`：`compose` 的转置顺序错误（**P0，写域外**）
`(A B)ᵀ = BᵀAᵀ`，但实现按 `children_[0]ᵀ ∘ children_[1]ᵀ = (B A)ᵀ` 作用。后果：任何通过 `compose` 构造影响/白化的协方差，其 `quadratic_form`（`apply_covariance` 先做 `Iᵀ`）给出错误数值；`control_covariance`（`src/information/information.cpp:585`）与旧的 `apply_sky` 都受影响。
本任务的处理：sky/coadd 侧改为
- 精确稀疏乘积 `multiply_csr`（`src/sky/sky.cpp:463`）用于 `K L_k`；
- `CovarianceAction.influence` 承载 `P'`/`W`（不经过 `compose`）。
建议 Lead 指派 operators 负责人修正该顺序并补独立负例（`A=[[1,2],[0,1]]`、`B=[[1,0],[3,1]]` 下比较 `transpose_apply` 与显式转置）。

### 8.2 `src/operators/api.cpp`：零 CSR 动作丢失列数（**P1，写域外**）
`make_action` 的 csr 分支用 `max(index)+1` 推断列数，全零矩阵得到 `cols=0`，于是 `H=0`、`E=0` 这类合法动作没有域大小（本报告 P1-9 探针因此改用非零公共天光列）。`make_csr_action` 无此问题。建议显式携带 `recipe.spaces` 或在列数不可推断时拒绝。

### 8.3 `src/information/information.cpp:611`：`control_covariance` 的 unknown term 未随 `T` 变换
它把 `source.unknown_terms` 原样复制，而 `influence` 已变成 `T L_k`。当前只影响“materiality”判定（`apply_covariance` 遇 material unknown 直接 `unavailable`），不产生错误数值，但语义上 unknown 项应在控制空间。写域外，记录待办。

### 8.4 `SkyFit`/`Selection`/`WeightSolution` 缺 revision/几何/provider 身份（P1-11 未完成部分）
需要 `include/acsd/model.hpp` 新增字段（`src/model`，写域外），以及 `validate_inputs` 比较。本任务只在 `assessment` 记录选择条件；`effective_response` 仍为 `nullptr`（`coadd.cpp:1297` 区域），需要 `SpatialResponse` 的真实 `t, c, 核组合`，属 `src/model` + `src/projection`/`src/response` 协作。

### 8.5 `quadratic_form` 的负方差判据仍是 `1e-12·max(1,|v|)` 启发式（P2）
未改为区间证书（需要 `numerics` 的 outward 二次型，写域外）。当前行为：`|v|` 小于该尺度才截 0。

### 8.6 `build_supported_triangulation` 的包围盒启发式（P2，写域外）
`src/information/triangulation.cpp` 未动；`validation/stats/information_test` 的既有失败也在该文件（基线同样失败）。

---

## 9. 复现命令

```sh
# 构建
cmake --build build/science -j16

# 既有独立验证（只读使用，未改容差/真值）
g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -I include -I validation/stats \
  validation/stats/sky_test.cpp -o /tmp/statsbase/sky_test \
  -L build/science -lsky -lcoadd -linformation -luncertainty -loperators -lnumerics -lmodel
/tmp/statsbase/sky_test      # 0 failure(s)
# coadd_test / information_test 同法；information_test 的 1 条失败为既有（基线同样失败）

# 本报告的独立探针（不在仓库内，避免新建门禁；源码 /tmp/acsd_probe/probe.cpp）
g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -I include -I validation/stats \
  /tmp/acsd_probe/probe.cpp -o /tmp/acsd_probe/probe \
  -L build/science -lsky -lcoadd -linformation -luncertainty -loperators -lnumerics -lmodel
/tmp/acsd_probe/probe          # 111 ok / 0 failure

# 基线对照（git archive HEAD 导出，独立 build）
cd /tmp/acsd-base && cmake -S . -B build/base -DACSD_BUILD_SCIENCE=ON -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build/base -j16 --target sky coadd information uncertainty operators numerics model
/tmp/acsd_probe/probe_base     # 23 failure（旧实现的 9 个反例族）
```

探针源码约 900 行，位于 `/tmp/acsd_probe/probe.cpp`（同一文件系统，Lead 可直接取用）。若要把它们纳入仓库，规范位置是 `validation/stats/`（`run_validation.sh` 的同一编译/链接方式），但 `validation/` 不在本任务写域。

---

## 10. 证据边界（不夸大）

- 全部结论都是**确定性代数反例**：证明实现与合同定义一致/不一致，不复核真实噪声模型，也不声称 coverage。
- 未运行 `validation/x01` 端到端（它会写 `validation/x01/**`，不在写域且会覆盖他人工作树）。§3 的“帧无 noise model”探针复现了当前 `x01` 产品的形态（`noise=None, assessment.noise=unknown`），据此判断链路不会因本次收紧而中断；这仍是**推理 + 局部探针**，不是端到端实跑。
- `K H=0`、`U0ᵀy` 精确约束、`tᵀΓt` 都是线性/Gaussian 前提下的结论；Huber/median/clip/非 Gaussian 不在其内。
- 本报告的“真值”均由探针按定义重算（含审计 CE-H 数字的更正），不引用生产 solver。
