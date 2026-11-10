# 统计 / Sky / Coadd / Information / Uncertainty 链独立审查

审查角色：独立统计审查者（不参与实现）。只读 `src/`，只新建本文件。
分支：`rebuild/acsd-v2`，受检 HEAD `688adeac7`（工作树另有未提交的 `docs/audit/IMPLEMENTATION-STATUS.md`、`validation/x01/*` 改动，与本审核范围的 `src/` 无关）。
方法：按 `review/13-INDEPENDENT-REVIEW-AND-EXPERIMENTS.md` 第二节，先由 `docs/science/foundation/STATISTICS_SKY_COADD.md` 与十篇 process 合同（`uncertainty.covariance`、`information.reference`、`information.scene`、`sky.controls`、`sky.fit`、`coadd.select`、`coadd.weights`、`coadd.integrate`、`operators.action`、`projection.export`；最后一篇只取其算法/反例节，因为它承载导出的 `Q`/variance 边）**正向重推**，再读 `src/` 逐条对码，最后用精确有理数/闭式小例给出可独立复算的反例。**不以测试数量、不以 `validation/` 既有 PASS 裁决**；`validation/stats` 的证据身份问题见 §7。

受检文件：`src/uncertainty/{api.cpp,covariance.cpp,covariance.hpp}`、`src/operators/{linear_action.cpp,api.cpp}`、`src/information/{information.cpp,triangulation.cpp,internal.cpp}`、`src/sky/sky.cpp`、`src/coadd/{coadd.cpp,weights.cpp}`、`src/projection/export_projection.cpp`，以及 `src/workflows/mosaic.cpp` 的数据边。

---

## 1. 正向推导（不先看生产 C++）

### 1.1 三个方差对象 `V_F`、`V_B`、`V_ctrl`

统一观测模型 `y = R D_α d + ε`（`R` 为同一几何/重采样算子，`D_α` 为已声明线性校准）。随机根 `ξ` 只保留一次：

```
C_d = J_ξ Cov(ξ) J_ξᵀ ,   C_y = R D_α C_d D_αᵀ Rᵀ ,   C_x = C_y        (1)
```

三个量是不同泛函，不能互换：

```
V_F    = 1 / I_{F|β},  I_{F|β} = ‖(I − Π_{Z_w}) h_w‖² ,  h_w = C^{-1/2}h, Z_w = C^{-1/2}Z   (2)
V_B,p  = r_pᵀ C_x r_p ,   r_p = 目标叶的面亮度泛函（可带负背景环）                          (3)
V_ctrl = (T y) 的方差 = t_cᵀ C_y t_c（每个控制行一个 t_c）                                  (4)
```

要点：
- `V_F` 的条件是**固定点源模板 `h`、固定背景列 `Z`、固定 C**；`V_B` 的条件是**实际场景面亮度泛函**。二者即使在同一帧、同一 C 下也一般不等：`1/V_F` 是通量方向的信息，`1/V_B` 是面亮度方向的信息。`SNR_ref²/F_ref² = 1/V_F` 不能改名为场景 IVAR，除非另有 `T = V_B/V_F` 的换算假设并声明同一估计量/响应/噪声条件。
- `V_ctrl` 的 `t_c` 是**控制算子的一行**（mask 面积归一），因此 `C_ctrl = T C_y Tᵀ`；同一像元/同一 root 进入两个控制时其交叉项非零。
- 未知项：任何对请求泛函可能非零的未知项 `U` 使完整方差 `unknown`。已知部分只能作具名下界/诊断，**不填 0**。

### 1.2 `C_out = R C_in Rᵀ` 与 `tᵀCt`

对任意（可带负号的）输出侧泛函 `t`，正确值只能是

```
tᵀ C_out t = (Rᵀt)ᵀ C_in (Rᵀt)                                                             (5)
```

`C_out` 的元素 `(R C Rᵀ)_{pq} = r_pᵀ C r_q` 一般非零。把 `C_in` 的对角当独立方差、或把 `C_out` 的对角当完整协方差，都不是 (5)。方向性：
- 全正孔径忽略非对角**可能**低估（正相关像元）；
- **带负号孔径时误差方向不定**，不能以“更宽=更保守”自我安慰。

数值上 `C = Σ_k L_k F_k F_kᵀ L_kᵀ`，`L_k` 是第 k 个 root 的影响算子（`influence`），`F_k` 是其 factor；同一 `SharedId` 的多次出现是**同一随机变量**，必须先合并 `L = Σ L_i` 再平方：

```
(Σ_i L_i) F Fᵀ (Σ_i L_i)ᵀ = Σ_i L_i F Fᵀ L_iᵀ + Σ_{i≠j} L_i F Fᵀ L_jᵀ                    (6)
```

逐根相加会丢掉 (6) 的交叉项（这正是 `uncertainty.covariance.md` 的 C-ROOT 反例）。

### 1.3 共同天空、背景基与 `K`

控制模型 `c = T y = H s + A β + e`，`A = T E`（`E` 是输出样本上的同一 basis 求值；`A` 必须是 `T` 作用于 `E` 的结果，不能另取一个插值器）。`s` 是任意共同真实天空（不需可辨识），`β = G η` 是 gauge 子空间内的逐帧加性差模。

白化 `W` 满足 `W C_c Wᵀ = I`（在 `C_c` 的 range 上），`Q_w` 为 `W H` 列空间正交补。则

```
η̂ = argmin ‖Q_w W (c − A G η)‖² ,   β̂ = G η̂ = K c ,
K = G (Q_w W A G)⁺ Q_w W                                                                (7)
```

- `K H = G (…)⁺ Q_w W H = 0` 恒成立（`Q_w ⊥ col(W H)`）。这只是**在声明 H 的列空间上**的低频保护：`K (H s) = 0`。真实共同分量若不在 `col(H)`（例如 H 只有常数而真实天光有梯度），`K y ≠ 0`，该分量被整体吸收为帧差模——`K H = 0` 不证明错误 H 下仍受保护。因此正确实现必须同时发布**残差/留出诊断**，否则无法区分“没有差模”和“H 不完整”。
- `C_c` 为 PSD 时，零方差方向是**精确方程** `U0ᵀc = U0ᵀ[H, A G]η`。正确顺序是先用精确方程确定可估方向的约束，再在白噪声子空间做 GLS，并对不一致返回 `invalid_model`；直接把 `C⁺` 当白化或只投影 range 会丢失无噪声信息（`information.reference.md`、`sky.fit.md` 都写明）。

### 1.4 `P = I − E K T` 与 `C_z`

```
z = y − E β̂ = (I − E K T) y = P y                                                   (8)
C_z = P C_y Pᵀ = C_y − E K T C_y − C_y TᵀKᵀEᵀ + E K T C_y TᵀKᵀEᵀ                   (9)
```

(9) 展开有**两个负交叉项**。因为 `P` 是同一数据上的线性算子：参与拟合的像元与留出像元一律用 (9)，不能“先独立算 β 方差，再加到原方差”。等价写法（`V = C_y TᵀKᵀ`，`C_β = K T C_y TᵀKᵀ`）

```
C_z = C_y + [E, V] [[C_β, −I], [−I, 0]] [E, V]ᵀ                                       (10)
```

(10) 的中间块不定，完整 `C_z` 仍 PSD；只加正的低秩项是错的。
若某输出行的 `E` 行在残余 nullspace 上非零，则该行修正不可估：正确做法是**该行不施加修正且不发布 (9) 的该行**（方差回到 `C_y` 行），并具名 `unavailable`。若发布 `z_i = y_i` 却发布 `(P C_y Pᵀ)_{ii}`，则发布值与方差不是同一估计量。

### 1.5 合成权重：`Γ`、BLUE、PSD、QP

同一量 `z_k = B + ε_k`，`Γ = Cov(z)` 已知时

```
t = Γ⁻¹1 / (1ᵀΓ⁻¹1) ,   B̂ = tᵀz ,   V = tᵀΓt = 1/(1ᵀΓ⁻¹1)                              (11)
```

- `Γ` 必须含**共享根与 sky 交叉项**（`P C Pᵀ` 的联合块），不能各帧独立取对角。
- 独立时 (11) 退化为 `IVAR 1/V_B` 加权；**未知 Γ 时不能报 `V = 1/K`**：等权只是“合法但非精度最优”的策略，其方差仍是 `(1/K²)1ᵀΓ1`，Γ 未知时该值 unknown。
- PSD `Γ`：零空间是精确约束。若存在 `u ∈ null(Γ)` 且 `1ᵀu ≠ 0`，则 `t = u/(1ᵀu)` 是零方差归一解（`V = 0`）；否则在 range 内解、`1 ∉ range(Γ)` 时不可解。任何 `ε` 地板都改变了数学问题。
- 非负约束 `min tᵀΓt s.t. 1ᵀt = 1, t ≥ 0` 的解必须用真实 KKT 证书：`2Γt − λ1 − ν = 0`、`ν ≥ 0`、`ν_i t_i = 0`、`1ᵀt = 1`。KKT 成立才算解；非负解**不是**无约束 BLUE，不能沿用后者的 `V`。

### 1.6 选择后 coverage

固定最终 mask 的 `tᵀΓt` 是**条件公式** `Var(B̂ | mask)`。mask 由同一数据经 LOO 阈值产生后，`B̂` 的无条件分布是截尾/混合分布，条件方差**不自动**给出 68/95% 覆盖。因此：
- N=1..3 默认不做统计排异；
- N ≥ 4 单步最多拒一个最大标准化 LOO 残差，阈值 `τ = Φ⁻¹(1 − α/(2N))` 给逐位置 family false-reject ≤ α（`q_k` 不需独立）；
- 发布侧必须把“冻结 mask 条件”标成条件，不能用它冒充无条件覆盖；无条件覆盖只能用包含 fit/selection/sky 的全过程前向 MC 与二项区间主张。

---

## 2. 逐类错误的落点核查

| 错误类 | 是否存在于当前分支 | 证据（file:line） | 说明 |
|---|---|---|---|
| 把参考 Fisher 当 scene IVAR | **未发现**（有防护） | `src/information/information.cpp:672-677`（`PrecisionControls::Kind::actual_scene` 硬检查）、`442-579`（`evaluate_scene_variance` 走帧自身 `FrameDescription.noise` 的 C，不读 V_F）、`429`（`included "reference_source_flux_variance"`） | 残余缺口：`evaluate_scene_variance` 不校验 `description.noise->model->condition` 是否为 actual_scene，`Variance` 也不携带 estimator 身份，所以“参考噪声图被 provider 改名”无法在函数内拒绝（见 §5.2 `evaluate_scene_variance` 行）。 |
| shared root 拆成独立 | **存在**（两处） | ① `src/coadd/coadd.cpp:921-969`：最终 `WeightSolution.covariance` 只保留每叶对角 `sqrt(tᵀΓt)`，每个叶一个独立 innovation root → 叶间共享根相关性被丢弃；② `src/uncertainty/api.cpp:30-90`：同 `SharedId` 只合并**相邻**段，顺序 A,B,A 时两段 A 不合并 → (6) 两个交叉项丢失 | 见 CE-D、CE-H。`sky.cpp:644-724` 与 `coadd.cpp:284-336` 的联合装配本身是按 key 合并的，正确。 |
| unknown 填 0 | **存在**（三处） | ① `src/sky/sky.cpp:644-724`（`build_joint_noise` 只搬 `innovations/shared_factors/influence`，**不搬 `unknown_terms`**，末尾 `joint->state = NoiseState::known`，715 行）；② `src/coadd/coadd.cpp:284-336` 同样丢弃各帧 `unknown_terms`，`382` 行 `joint->state = NoiseState::known`；③ 未知 Γ 时发布 `variance = 1/K` 且 `noise->state = known`（`coadd.cpp:912-914, 957`） | 显式 `blue`/`nonnegative_qp` 只检查 `mixed_observation`（`coadd.cpp:781-820`），不检查 Γ 完整性，所以带 unknown 项的模型会**静默**进入 BLUE/QP。 |
| 固定 mask 条件方差冒充无条件覆盖 | **部分存在**：代码不主张 coverage，但也不记录选择条件 | `coadd.cpp:697-708`（`Selection.condition = request.target.condition`）、`981-992`（`WeightSolution.condition` 同上；`assessment = make_assessment(Code::ok)` 使 `science = bounded`，见 `src/model/units.cpp:265-274`） | 冻结 mask 的条件性只在 `reasons` 数组与 `included "rejected_samples=N"` 里留痕，`WeightSolution` 没有“selection-conditional”标记，`condition` 也没有加入 selection 身份。 |
| β 方差直接相加 | **未发现**（sky 侧正确） | `src/sky/sky.cpp:1949-1985`：`corrected_noise` 由 `compose(P, influence_k)` 重建 → 精确等价于 (9)，保留两个负交叉项；`src/uncertainty/covariance.cpp` 的 `apply_covariance` 对每个 root 做 `L F Fᵀ Lᵀ`，顺序正确 | 但 Coadd 侧存在同构错误：只把对角方差装成独立根（`coadd.cpp:921-969`），等价于“逐叶独立加方差”，见 CE-H。 |
| 内部二次求权 / 二次选择 | **未发现**（链正确） | `src/coadd/coadd.cpp:1154-1328`（`combine_sealed` 只读已封存对象）、`coadd_integrate_process.cpp:6-7,291`；`coadd()` 旧入口在 `1333-1352` 只跑一次 select+weights 后委托同一核心 | 但**缺一整条边**：`apply_sky` 从未被工作流调用（§4.1）。 |
| 跨 `GeometryRevision` 复用旧对象 | **存在（结构性）** | `src/coadd/coadd.cpp:1042-1150`（`validate_inputs` 只比 index/selection/condition）、`SkyFit`/`Selection`/`WeightSolution` 均无 revision 字段（`include/acsd/model.hpp:394-396` 与 `403-407`；`NoiseModel.condition` 也不参与任何身份比对，`model.hpp:209-212`）；`Compute` 侧 `GeometryRevision` 未被 sky/coadd 消费 | 冻结顺序存在，但没有“几何改变使旧对象失效”的机制。 |

---

## 3. 独立最小例（精确可算，附预期数值）

以下 9 例除特别说明外均为**有理数精确值**，可纸面复算；与生产 helper 无关，只作为证伪代数错误/实现错误的最小对象。

### CE-A `2×3 R` 与非对角 `C` 的 `R C Rᵀ`、`tᵀCt`

```
R = [[1,2,0],[0,1,−1]],   C = [[2,1,0],[1,2,1],[0,1,2]]   (SPD, det=4, 顺序主子式 2,3,4)
R C Rᵀ = [[14,3],[3,2]]           （精确）
```

- 输出泛函 `t_out = (1,−1)ᵀ`：正确 `tᵀ C_out t = 14 − 6 + 2 = **10**`；只用 `C_out` 对角得 `14+2 = **16**`（**低估 6**）。
- 输入侧带负号孔径 `t = (1,−2,3)ᵀ`：正确 `tᵀCt = **12**`（`Ct = (0,0,4)`）；按逐像元独立方差 `Σ t_i²C_ii = **28**`（**高估 16**）。这条同时说明“负号孔径下误差方向不定”与“对角化不保守”。
- 一致性检查：`Rᵀt_out = (1,1,1)`，回到输入空间得 `1ᵀC1 = **10**`，与上面 `t_outᵀC_out t_out` 完全一致——这正是 (5) 的要求；逐像元独立 `ΣC_ii = **6**` 与两者都不等。

### CE-B `V_F ≠ V_B`（同一 `V_F`、不同 `V_B`）

两帧、两像元、`C = diag(v,v)`、单叶面亮度泛函 `r=(1,0)`：

| 帧 | `h` | `v` | `V_F = 1/Σh_i²/v` | `V_B = rᵀCr` | IVAR `1/V_B` |
|---|---|---|---|---|---|
| 1 | (1/2, 1/2) | 1/2 | **1** | **1/2** | **2** |
| 2 | (9/10, 1/10) | 41/50 | **1** | **41/50** | **50/41 ≈ 1.219512** |

相同 `V_F = 1`，`V_B` 差 `41/25 = 1.64` 倍。把 `1/V_F = 1` 当两帧场景 IVAR，两帧权重都等于 1 → 归一后 `(1/2,1/2)`，`V = (1/2+41/50)/4 = **33/100**`；正确 BLUE（`Γ` 已知 = `diag(1/2,41/50)`）为 **`(41/66, 25/66)`**、`V = 41/132 ≈ **0.310606**`（差 1.0624 倍）。差异幅度取决于 `V_B/V_F` 的散布，但**权重语义**（“每帧等权”vs“按场景方差加权”）完全不同。结论：`V_F` 只能在有数学桥接时换算成 `V_B`，否则必须保留为参考诊断。生产侧该例的部分断言已由 `information_test` 的 2 与 1.2195 覆盖，但证据身份见 §7。

### CE-C 100 帧共享 floor 不随 K 增大消失

`y_k = μ + s ξ + ε_k`，`Var(ξ)=1`、`Var(ε_k)=1`、`s=1`、`K=100`：

```
Var(mean) = s² + σ²/K = **101/100 = 1.01**
仅独立部分          = σ²/K = **1/100 = 0.01**
```

`K → ∞` 极限是 **1**，不是 0。任何把共享 root 拆成逐帧 innovation 的实现都会把 1.01 报成 0.01（100 倍虚假改进行）；`coadd.cpp:921-969` 的逐叶对角化在**叶间**共享根上正是这种拆法（叶内对角值正确，跨叶归零）。

### CE-D 同 `SharedId` 非相邻时合并失败

模型 `shared_factors = [S(1), T, S(1)]`，influences 均为 `1×1`：`L_S1 = 1, L_T = 1, L_S2 = 1`，`F=1`。

```
正确（同根先合并）：C = (1+1)² + 1² = **5**   ，t=1 时 Var = 5
逐根相加（错误）：  C = 1+1+1 = **3**
当前 api.cpp:30-90（相邻扫描不合并，两段 S 各算一个根）：C = 1+1+1 = **3**
```

（若按 C-ROOT 反例的 4 方差口径：正确 `4·(1−1)²=0`，逐根 `4+4=8`，当前合并逻辑在 A,B,A 顺序下得到 `4+4=8`——即**反例本身在同一顺序下仍然失败**。）

### CE-E sky 的 PSD 精确约束被丢弃

2 控制（每帧一个）、`T=I₂`、`H=0`、`A=I₂`、`basis=constant`、`gauge=weighted_sum` 等权（约束 `β₁+β₂=0`，自由基 `G=(1,−1)/√2`）、`C_c = diag(0,1)`、`c=(1,0)`。

- 当前实现：`W = diag(0,1)` 把第 1 行（零方差精确方程）整行清零（`sky.cpp:1581-1592`），`Q_w=I`，`design = Q_wᵀ W A G = (0,−1/√2)ᵀ`，于是
  `K = [[0,−1],[0,1]]`、`β̂ = K c = **0**`、`K C Kᵀ = [[1,−1],[−1,1]]` ⇒ `Var(β̂₁) = **1**`、`Var(β̂₁−β̂₂) = **4**`。
- 正确（先精确方程再 range GLS）：`U0ᵀc = 1 = (U0ᵀAG)η ⇒ η = √2 ⇒ β = Gη = **(1,−1)**`，`V = **0**`（第二行 `c₂=0` 与 `(AG)₂η=−1` 的差 1 是 1σ 噪声，允许）。

即**点估计错 1（0 vs 1）、已知方差从 0 变成 1（差量方向 4）、且精确方程不一致时不会返回 `invalid_model`**。这与 `sky.fit.md:42,71` 的“PSD 噪声先处理精确约束”直接冲突。

### CE-F `K H = 0` 只在声明子空间

同上模型但 `H=[1,1]ᵀ`（常数共同天光）、`gauge=reference_frame`（`β₁=0`，`G=e₂`）、`C=I`：

```
Q_w = (1,−1)/√2,  design = Q_wᵀAG = −1/√2,  K = [0,0; −1,1]
K H = (0,0)  ✓（恒等式）
```

真实天空是 `1+x`（x=±1，两帧各自 `y=(0,2)`）：`c=(0,2)`，`K c = (0,2)` → `β̂₂ = 2`，把 2 的整体差异全部当作仪器差模。差异 2 与“真差模 0”相差 100%，而 (7) 的 `K H=0` 对此没有任何保护。当前 `SkyFit` 无残差/χ²/留出字段（`include/acsd/model.hpp:394-396`），`fit_relative_sky` 全程不计算残差 → 无法发现 H 不完整。

### CE-G BLUE / 等权 / QP 的已知权

`Γ = [[1,2],[2,5]]`（SPD, det=1）：

```
Γ⁻¹1 = (3,−1), 1ᵀΓ⁻¹1 = 2
BLUE  t = (**3/2, −1/2**),  V = **1/2**
等权  t = (1/2, 1/2),       V = 10/4 = **5/2**      ← 当前代码对 equal 分支发布 1/K = **1/2**
QP  t = (**1,0**),          V = **1**
```

QP 的 KKT：`2Γt=(2,4)`；自由分量 `μ=2`；active 分量 `gradient−μ = 2 ≥ 0` ⇒ 非负乘子成立、互补成立、`1ᵀt=1` ⇒ `(1,0)` 合法，而 `(3/2,−1/2)` 不可行。**注意：等权真实方差 5/2 与代码发布值 1/2 差 5 倍**，而且模块自己的冻结例（`coadd.weights.md` W-BLUE：`Γ=diag(1,4)`，equal 应为 `5/4`）与 `coadd.cpp:912-914` 的 `1/K=1/2` 直接矛盾。

PSD 子例：`Γ = diag(0,1)` → 零空间有 `1ᵀu=1≠0` ⇒ `t = (1,0)`、`V = 0`（代码 `weights.cpp:97-118` 正确）。`Γ = diag(1,4)`：`t = (4/5,1/5)`、`V = **4/5**`（代码正确）。

### CE-H 逐叶对角化丢掉跨叶共享项

2 个叶、单帧、`Γ` 含共享加性根：`C = I₂ + 11ᵀ = [[2,1],[1,2]]`，每叶 `t=1`。

```
真实：每个叶 V = 2；Cov(叶1,叶2) = **1**
Var(z₁+z₂) = 4+4+2·1 = **10**   Var(z₁−z₂) = 0
当前实现（coadd.cpp:921-969）：对角 (2,2)、叶间 0
    → Var(z₁+z₂) = **4**（低估 2.5 倍），Var(z₁−z₂) = **4**（真实 0）
```

这是 (6)/(9) 在叶空间被违反的精确例子，也是“shared root 拆独立”的实际后果。

### CE-I 选择：N=1..3 与单步 LOO

`Γ=I`、`z=(0,0,0,0,20)`、`α=0.001`、`N=5`：`τ = Φ⁻¹(1−α/(2N)) ≈ **3.7190**`；`q₅ = 20/√(1+1/4) = **17.8885**`，其余 `q_k = −5/√1.25 = **−4.4721**`。五者的 `|q|` 全超阈：并行丢弃会拒 5 帧，正确只拒绝最大者（1 帧）。当前 `coadd.cpp:606-633` 只拒一次、只拒最大者，且 `N<4` 走 `kRejectionSkipped`（`537-542`）——**该 op 的选择逻辑本身可保留**。但拒绝后 `Selection`/`WeightSolution` 都不记录“条件是冻结 mask”，见 §2 表。

---

## 4. 数据边与冻结顺序

### 4.1 唯一消费链：缺 `apply_sky` 边（P0）

`docs/workplans/.../08-STATISTICS-SKY-COADD.md` S4 规定的链是
`sky.controls → sky.fit → apply_sky → coadd.select → coadd.weights → coadd.integrate`。

实际 `src/workflows/mosaic.cpp`：`246-247` 建控制、`260-261` 拟合天空、`273-279` 选择、`287-293` 求权、`300-308` 合成。**`apply_sky` 不在其中**（全 `src/` 只在 `runtime/generated/codecs.cpp`、`typed_operations.cpp`、`status.cpp` 里作为已注册 operation 出现，没有任何生产者调用）。`coadd.cpp:1244` 读取的是 provider 的原始 `signal`，于是：

- 发布产品保留了刚拟合出的相对天光偏移；
- `Selection` 的资格与 `weights` 的 `Γ` 都基于**未校正**帧（`build_joint_covariance` 用 `view.noise`，`P C Pᵀ` 从未进入 Γ）；
- `CoaddResult.sky` 携带了一份“看起来已执行”的 `SkyFit`，产品却与之不一致。

这不是“统计近似”，而是链上少一条冻结边，且 `coadd.integrate` 的合同（“provider 已绑定给定 sky/P，不再次 apply_sky”）以既有假设形式把缺口掩盖过去。

### 4.2 其余边检查

| 检查项 | 结论 | 证据 |
|---|---|---|
| 每阶段只执行一次（无二次求权/二次选择） | ✓ | `coadd.cpp:1320-1328`（`coadd_with_inputs`）、`1154-1157`（`combine_sealed` 不调用 select/weights）、`coadd_integrate_process.cpp:291`（typed 入口也只调 `coadd_with_inputs`）；`mosaic.cpp` 各调一次 |
| `Selection`/`WeightSolution` 绑定同一 provider/sky/Γ 身份 | ✗ | 对象里没有 sky/revision 字段（`model.hpp:391-396`、`403-407`；`NoiseModel.condition` 同样未参与身份，`model.hpp:209-212`）；`validate_inputs` 只比 `index`、`selection.identity`、`condition`（`coadd.cpp:1079-1121`）；`Selection.identity` 只由叶数与 order 生成（`coadd.cpp:704-706`），两个不同 sky 的 Selection 可以同 hash |
| 每叶有效权和 = 1 的数值包围 | ✗ | `combine_sealed` 只算 `total` 并在 `total==0` 时跳过（`coadd.cpp:1234-1250`）；`validate_inputs` 不检查 `Σ_k t_k = 1`，`coadd.integrate.md:80` 要求的“active sum=1 的数值包围须含1”未实现 |
| `effective_response`/混合核保存 | ✗ | `coadd.cpp:1297` 硬置 `nullptr`，`coadd.integrate.md:82` 要求保存真实 `t,c,核组合` |
| 跨 `GeometryRevision` 失效 | ✗ | `GeometryRevision`（`model.hpp:347-351`）未被任何 sky/coadd/selection/weights 代码消费；`SkyFit.K/T/E/P` 的 `make_action` 子动作索引为 `Id()`（`sky.cpp:1721-1739`），P 与帧 index 无身份绑定 |
| Export 的 `Q` 与 noise 同算子 | ✗ | `src/projection/export_projection.cpp:438`：`export_estimate->noise = estimate.noise;` 原样复制叶空间协方差，信号却是 `Q·signal`，单位还被改成 `sr⁻¹`/`sr⁰`（`422-428`）；`Q` 只返回到 `result.Q`（`472`） | 违反基础 §2.2“同一 L 同时作用于信号、PSF/response 和 noise”与 `projection.export.md` 的 E-MERGE（`Var=(1/2)²·9=9/4`，而不是原值）。导出的方差既不是 `Q C Qᵀ`，也不再与像元索引/单位对齐。 |
| sky 冻结后屏障 | ✓（工作流层） | `mosaic.cpp` 顺序固定、`fit_relative_sky` 一次性发布；但 `apply_sky` 缺席使“冻结”没有传播到产品 |

---

## 5. 每个 operation 的裁决

裁决口径：**可保留**（数学与实现一致，剩余为其它模块问题）/ **需修**（本 operation 内部有可定位缺陷，修后可用）/ **替换**（估计量或数据结构本身与合同不符，需重写）/ **规格缺口**（实现合理但合同要求的输出/检查缺失，需补合同或补实现）。

### 5.1 `uncertainty.*`

| operation | 裁决 | 证据 | 说明 |
|---|---|---|---|
| `make_covariance` | **需修** | `api.cpp:30-90, 80, 129-141` | `L = ΣL_i` 合并思路正确，但只合并**相邻**同 `SharedId` 段（CE-D）；`NoiseRoot` 取首个成员（`80-82`），只校验 unit 相同（`129-141`），不校验 factor/coordinates 同身份，也不在合并后返回 `identity_conflict`。 |
| `covariance_apply` | **可保留** | `covariance.cpp:96-135` | 每个 root 做 `Lᵀ→Fᵀ→F→L`，等价 (1)/(6)；unknown 项只要 material 即 `unavailable`（`api.cpp:233-245`）——保守且正确。 |
| `quadratic_form` | **需修** | `api.cpp:310-321` | 截断负方差的判据是 `1e-12·max(1,|v|)` 启发式，不是“证明区间含 0”；`-1e-13` 的负方差被静默截成 0，而真正的数值区间可能整体为负。应改为区间证书或直接 `invalid_covariance`。 |
| `geometry_covariance_bound` | **需修** | `api.cpp:325-398` | 公式 (3b) 实现正确（`2δᵀ|Cu₀|+δᵀ|C|δ`），但**不校验 `E` 逐元素非负**，也不取 `|E|`。反例：1D、`R0=1, E=−5, t=1, C=1` ⇒ `u0=1`、`δ=Eᵀ|t|=−5`，代码返回 `bound = 2·δ|Cu0| + δ|Cδ| = 2(−5)(1) + (−5)(5) = **−35**`（负的“上界”）；而同一 box（`|R−R0|≤|E|=5`）的真实最大偏差在 `R=6` 处为 `|36−1| = **35**`。即符号错误的 `E` 会给出既非正、也不包含的伪界。 |
| `control_covariance` | **可保留** | `information.cpp:585-636` | 对每个 influence 组合 `T L_k`，保留 `shared_id` 与未知项（`611`），再走同一个 `make_covariance`。 |

### 5.2 `information.*`

| operation | 裁决 | 证据 | 说明 |
|---|---|---|---|
| `estimate_flux_information` | **可保留** | `information.cpp:151-435` | 白化设计、背景投影、PSD 先解精确约束再 range GLS（`240-291`）、`cᵀ(RᵀR)⁺c`（`393-405`）均与 (2) 一致；`not_identifiable` 分支正确（`315-331, 376-392`）。残余：`component` 投影容差 `1e-8·max(1,cols)`（`314,375`）是绝对阈值，量与 cols 同阶时会在“可估/不可估”边界给出与尺度相关的判决，应改为相对判据。 |
| `evaluate_scene_variance` | **需修** | `information.cpp:442-579` | 泛函与空间判定正确（`kLeaf/kNative`）；缺两条前置校验：未验证 `description.noise` 的条件/估计量是 actual_scene（只信 provider），`Variance` 返回值也不携带 estimator/condition 身份 → “参考噪声图当场景”无法在此拒绝。 |
| `build_supported_triangulation` | **需修（偏规格）** | `triangulation.cpp:708-876` | 支持包含性用**包围盒 + 叶中心重叠**启发式（`780-835`），不是合同要求的“整三角闭域包含/相交”；且 chart 单位下的 `leaf_radius` 与 gnomonic 畸变不匹配时可能漏掉与三角相交的未支持叶。方向上是过度拒绝为主，但不足以称“认证整三角在支持内”。 |
| `reconstruct_scene_variance` | **可保留（有条件）** | `information.cpp:663-919` | `kind=actual_scene` 硬检查（`672-677`）；`δ/M` 只作外部输入；无 M 只给 conditional（`906-912`），与 (4a) 一致。条件：三角网与控点的对应只按位置 1e-12 匹配（`741-767`），未校验 `triangulation` 确由同一 `controls` 生成（响应/hole 身份）；传入另一组控点建的同位置网格不会被拒绝。 |

### 5.3 `sky.*`

| operation | 裁决 | 证据 | 说明 |
|---|---|---|---|
| `build_sky_controls` | **替换（关键路径需重写）** | `sky.cpp:918-956` vs `967-987`；`1052-1090`；`1064`；`1102-1130` | ① **`c` 与 `T` 不同源**：`c` 用“组内公共支持 ∩ DQ”mask 求均值，`T` 行重建时只按本帧 state/DQ，漏掉公共支持条件 → `c ≠ T y`。CE：帧1 支持 {A,B}、帧2 支持 {B}，公共 mask={B}，`y1_A=10, y1_B=2` ⇒ `c₁=2`，而 `T` 行给出 `6`，差 4 直接进入差模。② **`A ≠ T E`**：`E` 在叶中心求值（`1072-1082`），`A` 在 patch 中心求值（`1083-1090`）。CE：平面基 `(1,x)`、mask 只剩 x=+1 的叶 ⇒ `T E = (1,1)`，`A = (1,0)`；真值 `a=0,b=1` 时 `c=1` 被拟合成 `β=1` 的假偏移。③ chart 用 `maximum_radius_rad = π`（`1064`），`project_to_chart` 要求 `cos>1e-9`（`sky.cpp:205-206`），超出者 `evaluate_basis` 返回空，A/E 行被静默留零（包括常数项）→ 退化设计行不报错。④ 控制协方差构造失败一律降级 equal+C=I（`1102-1130`），连 `invalid_model`/`identity_conflict` 都被吞掉。 |
| `fit_relative_sky` | **需修** | `sky.cpp:1571-1674`（PSD）、`1745-1793`（param cov）、`1829-1861`（发布） | 白化 `W = UΣ⁻¹Uᵀ` 正确（`W C Wᵀ = P_range`）；`K H=0` 正确；`null_parameters` 作 `UnknownTerm` 正确（`1766-1778`）。缺陷：① 零方差精确方程整行被 `W` 吞掉，**没有** `U0ᵀc` 一致性检查、也没有 constrained GLS（CE-E：β 错 1、V 从 0 变 2）；② 不检查 `controls->noise->state`/`unknown_terms`，equal-weight 的伪造 C=I 会照常解出 β 并发布 `complete/ok` 的 `SkyFit`（`1844-1846`）；③ 不发布残差/χ²/留出诊断，无法发现 H 不完整（CE-F）。 |
| `apply_sky` | **需修** | `sky.cpp:1900-1934`、`1949-1985`、`2003-2012` | `P C Pᵀ` 用 `compose(P, influence)` 重建，**两个负交叉项正确**（(9)）；`fit->P` 不含重拟合。缺陷：① nullspace 位置把修正按硬阈值 `1e-12` 置 0（`1928`），但同时仍发布 `P C Pᵀ` 的该行 → 发布信号与发布方差不是同一估计量；② 不校验 `signal` 与 `fit` 的训练帧/几何/噪声是否同一对象（无 revision/identity 比较），旧 `SkyFit` 可套到新几何的 signal 上。 |

### 5.4 `coadd.*`

| operation | 裁决 | 证据 | 说明 |
|---|---|---|---|
| `select_samples` | **可保留** | `coadd.cpp:509-645` | DQ 先行、`N<4` skip、单步只拒最大者、阈值 `Φ⁻¹(1−α/(2N))`、并列按 frame ID、跨界 `selection_unresolved`（`617-627`）——与 §1.6 一致；`external_fixed_covariance` 的逐叶/共享域判定正确（`526-529`）。 |
| `solve_weights` | **需修（协方差发布部分需替换）** | `coadd.cpp:781-820`（策略）、`890-911`（BLUE/QP）、`912-914`（1/K）、`921-969`（协方差） | BLUE/QP 调用本身正确（α/Γ 来自联合块）；但：① **equal 分支用 `1/K` 当方差**，与模块自身冻结例（diag(1,4) 应 5/4）矛盾，5 倍低估；② 未知 Γ 时也发 `1/K` 并 `state=known`（`957`）；③ 最终协方差只留每叶对角、叶间共享根归零（CE-H：Var(和) 4 vs 10，Var(差) 4 vs 0）。 |
| `coadd` / `coadd.integrate` | **需修** | `coadd.cpp:1042-1150`（validate）、`1154-1311`（合成） | 只消费封存对象 ✓；叶顺序重算比对 ✓（`1195-1223`）；`unknown C` 时仍发布 signal（合同允许）✓。缺口：`Σ_k t_k=1` 未校验（`1234-1250`）、`SkyFit`/provider 身份未校验、`effective_response=nullptr`（`1297`）、support area 对 `state=0` 的叶也给整叶面积（`1276-1291`）、`getenv("ACSD_COADD_DEBUG")` 调试块留在生产路径（`361-372`）。 |

### 5.5 `operators.*`

| operation | 裁决 | 证据 | 说明 |
|---|---|---|---|
| `make_action` / `apply` / `transpose_apply` / `compose` | **可保留** | `src/operators/linear_action.cpp:240-273`（`Kind::sky`：`I − E(K(Tv))`，域检查 `262`）、`378-401`（转置） | `P = I−EKT` 逐字实现，正/转置一致；`compose` 有子动作声明（历史 P0 已修）。残余：`Kind::sky` 的 `E/K/T` 子动作多以 `Id()` 索引构造（`sky.cpp:1721-1739`），故 P 不会主动拒绝跨版本对象，身份只能靠上层。 |
| `action.dependency_blocks` / `serialize_recipe` / `row_enclosure` | **未在本审核范围取独立证据** | — | 本任务范围是统计链；这三项不影响 §1 的估计量，但 `row_enclosure` 是 `geometry_covariance_bound` 的绝对能力来源，其未认证会连带影响 §5.1 的界。 |

### 5.6 `projection.export`（与统计链直接相连，故纳入）

| operation | 裁决 | 证据 | 说明 |
|---|---|---|---|
| `build_export`（`Q` 与 variance） | **需修** | `src/projection/export_projection.cpp:400-472` | 目标像元信号 `b_qp` 的行 `Q` 构造、支持并集与 `Σb` 正下界检查在有支持时是正确的；但**协方差没有随 `Q` 传播**（`438`）。CE（直接用 `projection.export.md` 的 E-MERGE）：同一 leaf 两个不重叠 support 片各面积 `1/4`、`B=2`、`Var=9`，`supported_flux` 的目标值是 `1`、正确方差 `(1/2)²·9 = **9/4 = 2.25**`；把片当独立得 `9/8`。当前实现把**叶空间协方差原样挂到像元估计**（`438`），既不是 `2.25` 也不是 `1.125`，且索引（叶 vs 像元）与单位都不对齐：信号单位被改成 `sr⁰`（supported_flux）/`sr⁻¹`（supported_mean）（`421-428`），协方差单位仍是叶面亮度方差。再叠加 §5.4 的逐叶对角化（叶间项已是 0），导出层即使补上 `Q C Qᵀ` 也拿不到真实跨叶项。 |

---

## 6. 统计判据的适用域（明确写出，不夸大）

- 本节所有反例都是**确定性代数反例**：证明某条实现/公式违反合同给出的定义，不复核真实噪声模型。
- `V_F` 的 `C` 只在声明的 `known/conditional` 上成立；`V_B` 的稀疏重建（4a）只在外部 `δ/M` 与支持三角内成立；`P C Pᵀ` 的 coverage 声明需全过程 MC（含 sky/selection），本审核**没有**重跑任何 MC。
- `K H=0`、`U0ᵀy` 精确约束、`tᵀΓt` 条件式都是**线性/Gaussian 前提**下的结论；Huber/robust、median/clip、非 Gaussian 噪声不在其内。
- CE-I 的 `α=0.001` 只是研究基线；`τ` 给的是逐位置 family false-reject 界，不是全图误拒率承诺，也不是选择后 coverage。

---

## 7. 当前分支证据状态（重要）

1. `validation/stats/evidence/validation_run.txt` 第 1 行声明 `tested source sha = 785adc34b25e2d95fef75aa0c247c495d0b15e7a`、`branch = rebuild/acsd-v2`，173 检查 0 失败。实测：
   - `git merge-base --is-ancestor 785adc34b HEAD` **失败**（exit 1）；该 commit 只被 `backup/pre-author-clean-20261023` 包含，**不在当前 `rebuild/acsd-v2` 的历史里**。
   - 相对该 commit，受检文件在本分支已有实质改动：`src/coadd/coadd.cpp +332/−33`、`src/sky/sky.cpp +117/−31`、`src/uncertainty/api.cpp +21/−2`、`src/coadd/weights.cpp +9/−1`、`src/operators/linear_action.cpp +16/−6`、`src/information/{information.cpp +7, triangulation.cpp +6}`。例如 `equal_weight_covariance`（`sky.cpp:736-791`）与 `sky.cpp:1102-1130` 的降级分支是**证据之后新增**的，`validation/stats` 的四个 case 完全不覆盖它。
   结论：**该证据是历史证据，不是当前分支证据**；按 `IMPLEMENTATION-STATUS.md:65-68` 自身纪律，它只能记为 HISTORICAL，必须在新 SHA 上重跑才能认证。
2. 我没有重跑任何构建/测试（本任务为只读审查，且用户裁决不以 CI/自动门禁裁决科学正确性）。所以本文所有关于生产行为的判断都来自逐源码推理 + 独立精确小例，未由执行输出背书；反之也不因历史 PASS 而放松。
3. `validation/stats/coadd_loo_test.cpp` 的 184000 次 MC 与 W01 truth 的对比是**好证据**（独立统计实现、二项区间），但它冻结在上面的旧 SHA，且其覆盖的 equal 分支断言是“权重 = 1/2”，**没有**断言 `V = 5/4`，因此 §5.4 的 1/K 缺陷不在其断言集内。
4. 受检 HEAD 上未发现任何针对下列行为的当前证据：PSD 控制协方差下的 sky 拟合、`apply_sky` 的 nullspace 位置、`coadd` 的跨叶共享根协方差、`unknown_terms` 的联合装配传播、`Σt=1` 校验。这些应列为“待新 SHA 的独立负例”，不是 PASS。

---

## 8. 结论与建议修复顺序

**P0（阻断科学正确性，先修）**

1. `src/workflows/mosaic.cpp:246-320`：补 `apply_sky` 边，否则发布产品未被相对天光校正，而 `CoaddResult.sky` 与产品不一致。
2. `src/coadd/coadd.cpp:284-336,382` 与 `src/sky/sky.cpp:644-724,715`：联合装配必须搬 `unknown_terms` 并保持 `state`；未知项不得被重建为 `known`。
3. `src/coadd/coadd.cpp:912-914,957`：删掉 `1/K` 的伪造方差。equal 策略的方差必须 `tᵀΓt`（CE-G：5/2 或文档的 5/4）；Γ 未知时发布 `unknown`。
4. `src/coadd/coadd.cpp:921-969`：最终协方差必须由 `Σ_k t_k L_k` 的同根合并构造（或至少保留叶间交叉块的 action），不能逐叶对角化（CE-H）。
5. `src/sky/sky.cpp:918-956 vs 967-987`：`c` 与 `T` 必须同一 mask 同一算子；`A` 必须等于 `T E`（`1071-1090`）。
6. `src/sky/sky.cpp:1571-1674`：PSD 精确方程先解、再 range GLS、不一致 `invalid_model`（CE-E）。
7. `src/projection/export_projection.cpp:438`：导出方差必须是 `Q C Qᵀ`（含跨叶共享根与单位 `sr` 幂），不能原样复制叶协方差。

**P1（可定位、可修，紧随其后）**

8. `src/uncertainty/api.cpp:30-90,80,129-141`：同 `SharedId` 必须按身份全量分组合并，并校验 factor/coordinates 同身份（CE-D）。
9. `src/sky/sky.cpp:1102-1130`：控制协方差构造失败不得静默降级为 C=I；`SkyFit` 发布前必须检查 `controls->noise->state`。
10. `src/sky/sky.cpp:1900-1934` 与 `1949-1985`：nullspace 行要么同时把修正和该行方差一起处理，要么拒绝发布该行。
11. `src/coadd/coadd.cpp:1042-1150`：`Selection/WeightSolution` 必须携带并验证 sky/provider/几何 revision 与 `Σt=1`；`effective_response` 必须保存。
12. `src/uncertainty/api.cpp:325-398`：`E` 逐元素非负校验或改用 `|E|`。
13. `src/coadd/coadd.cpp:697-708,981-992`：把选择条件写进 `condition`/`assessment`，不要用默认 `science=bounded`。

**P2（规格/工程卫生）**

14. `src/uncertainty/api.cpp:310-321`（区间证书代替 `1e-12`）、`src/coadd/coadd.cpp:361-372`（删除 `ACSD_COADD_DEBUG`）、`src/coadd/weights.cpp:184,299`（`ever_released` 死代码，Bland 未真正实现）、`src/information/triangulation.cpp:780-835`（精确包含）、`src/sky/sky.cpp:1064`（chart 半径 π）、`evaluate_scene_variance` 的条件校验、`coadd.cpp:1297` 的 `effective_response`。

**验证顺序建议**：先修 5/6/7/8/9 并重跑 §3 的 CE-A/C/D/E/F/G/H；再补 `apply_sky` 边（1）与 unknown 传播（2），跑一个含共享 floor、共享 flat 与未知 gain 的端到端合成 MC（前向到产品），二项区间给 coverage；最后处理选择条件与身份（11/13）。**新证据必须在当前 HEAD 的 SHA 上重新生成并记录 `tested_source_sha`**；`785adc34b` 的旧输出只能作为历史对照，不能认证当前实现。

---

## 附：本文未覆盖的边界

- 未审 `src/coordinates`、`src/geometry`、`src/photometry`、`src/response`（本任务范围之外），因此 `R`/PSF/响应的数值正确性以它们的既有审核为准；本文只审 `R`、`C`、`T/H/A/E`、`K/P`、`Γ/t` 的**统计语义与数据边**。
- 未审 `runtime` 的 typed codec/registry 唯一性（另一 review）；本文所引 `runtime/generated/*` 仅用于证明 `apply_sky` 已注册但无生产者。
- 未在真实数据上验证任何适用域；本文所有“应”均为给定合同下的必要条件，不是对真实仪器行为的断言。
