# Issue #80 · P5 E0 矩阵可辨识性取证报告

**状态**：`REPRODUCED`（当前 main 同一冻结输入仍失败）+ **根因分层认定完成**
**软件身份**：main `58ee3fe03fe650ae4fce6e0aea6c2fd65dd6bcfd`（= `origin/main`），
`build/linux-sci-p5/acsd`（`code/` 与构建戳 `build_stamp_manifest.json:head_sha` 一致），
`OMP_NUM_THREADS=1`（单线程装配，排除并行归约差异）。
**范围**：只做诊断与实验；**未**加任意 ridge；**未**修改 `sky_plane.cpp` 生产求解算法；
**未**宣布 P5 科学闭合。main-only，无分支、无 PR。

---

## 0. 结论速览（TL;DR）

冻结的真实 3 帧 `2×H-alpha(600 s) + 1×Red(300 s)`（M42_M1）在**当前 main 上原样复现**
`reduced normal matrix not SPD`。独立矩阵 oracle 与生产读数逐项一致，判定结果**同时**是：

| 判读 | 是否成立 | 决定性证据 |
| --- | --- | --- |
| `no longer reproduced` | ✗ | §2：`abc_real` exit=2、rc=6、h=0.553742、coarsenings=0 |
| `rank-deficient valid`（真实秩亏/规范自由度） | **✗** | §5：投影秩 `rank(Q_B P)=6/6`、归一化后 `rank(Q_P A)=25/25`、`rank([A\|P])=31/31`；无额外零模 |
| `numeric defect`（浮点数值失稳） | **✓（机制）** | §4/§5：跨帧权重中位跨度 **1.405e32**；`H_red` 出现 λ_min=−2.27e-3 的**显著负**特征值；同一数学问题的分帧/正交/归一化算法分别得 PSD |
| `model invalid across filters/units` | **✓（主因，跨帧身份）** | §3：两代 P1 产品的 `photappl/photscal` 相差 **5.702e-17**，却都声明 `BUNIT=ADU/sr` |

**关键结论**：这不是"某三帧在当前物理模型下不可解"的数学不可辨识，也不是矩阵装配公式写错。
它是**输入的光度身份不一致**（一帧已施加 P1 光度尺度、两帧未施加，声明单位却相同）
所产生的 ~1e32 跨帧权重跨度，超过了 double 的 16 位有效数字，使**逐帧 Schur 消元的
约化正规矩阵在双精度下根本不可表示**；失败与否还取决于由**元数据哈希**决定的
规范锚帧（§4.3），因此同一份数值数据仅改名义 `obs_filter` 就能从 FAIL 变成 PASS。

同时必须明确：即便数值问题解决，**`Hα` 与 `Baader R` 两个真实通带共用单个 `B_ref`
仍缺共同物理响应/SED 模型**——本轮不据此宣称该联合解具有"共同天空 S"的科学含义（§7）。

---

## 1. 冻结身份

### 1.1 帧集（真本 path，只读）

| 键 | 真本 | 物理身份 | `obs_filter`(声明) | `photappl` | `photscal` | 曝光 |
| --- | --- | --- | --- | --- | --- | --- |
| A | `run/science-v2-impl/e2e/out/ab_off/M42_M1_...20251212_014241-600S-H-alpha` | Hα 600 s | `Baader 7nm H-alpha` | **0** | **1.0** | 600 s |
| B | `run/science-v2-impl/e2e/out/ab_off/M42_M1_...20251216_031003-600S-H-alpha` | Hα 600 s | `Baader 7nm H-alpha` | **0** | **1.0** | 600 s |
| C | `run/PHOT-FIX/p1_wcs_full/M42_M1_...20251212_012404-300S-Red` | Red 300 s | `Baader R` | **1** | **5.70245780684985e-17** | 300 s |

- A/B 与历史异常输入的关系：历史 `anom_abc` 目录与真本**逐文件对照只有
  `signal/properties:obs_filter` 行不同**（746/754 个 signal/support/variance/ivar
  tile 的 sha256 全部一致），即历史失败输入的**科学载荷与真本完全相同**，
  只差一个不参与天空面数值的名义标签。该对照由 §2 的 label-swap 臂独立验证。
- 正对照：D/E/F = `run/PHOT-FIX/p1_wcs_full/M42_M2_*_Red`（同仪器同滤镜三个真实 Red 帧）。
- 逐文件 sha256 + 帧级身份 + 二进制 sha256：`evidence/input_index.json`
  （含 `tree.aggregate_sha256`，例如 A=`341d27456af9e307…`、C=`2e590bc54e39e8a8…`）。

### 1.2 几何 / 采样 / 模型参数（生产自报，全部落盘）

| 量 | 值 | 来源 |
| --- | --- | --- |
| union cells / observations / overlap controls | 116 / 17237 / 5807 | `stage sample ok` |
| `h_upper_deg`（失败臂起点） | 0.5537417834846583 | `adaptive` |
| `h_lower_deg`（数据分辨率极限） | 0.0143966 | 同上 |
| `constraining_scale_deg` | 1.107484 | 同上 |
| `spline_degree d` / `frame_gradient_order p` / `m` | 3 / 1 / 3 | `p2_sky_plane.bin` + 代码 |
| `rank_rtol` | 1e-10 | 同上 |
| `weight_mode` | 0 ⇒ `W = 1/variance`，`variance = observations[].control_variance` | `weight_basis="per_sample_ivar"` |
| gauge | 逐连通分量**最小 frame_id** 为锚帧，`δ_anchor ≡ 0`，锚帧不进 Schur | `sky_plane.cpp:640,724-728,2056-2057` |
| 连通分量 | **1 个**（5623 个 3 帧控制 + 184 个 2 帧控制） | `evidence/matrix_oracle.json` |

### 1.3 权重与光度尺度的实测跨度

每帧 `control_variance` 中位数 ⇒ `W` 中位数：

| 帧 | n_obs | value 中位 | control_variance 中位 | `W = 1/var` 中位 |
| --- | --- | --- | --- | --- |
| A(Hα) | 5776 | 2.6469e+12 | 7.751e+20 | 1.290e-21 |
| B(Hα) | 5759 | 2.9133e+12 | 7.933e+20 | 1.261e-21 |
| C(Red) | 5702 | 5.1209e-04 | 5.648e-12 | 1.771e+11 |

⇒ **跨帧权重中位跨度 = 1.405e32**（≈10^32.1），远超 double 的 16 位十进制有效数字。

---

## 2. 复现与受控臂（全部真实 CLI，`acsd mosaic --json cfg -y`）

| 臂 | 帧与标签 | exit | `[sky_plane]` | n_used | 锚帧 | 跨帧权重跨度 | 独立 oracle λ_min(H_red) | Cholesky |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `abc_real` | A(Hα) B(Hα) C(Red) | **2** | **FAIL rc=6** `not SPD` | 17237 | A(Hα) | 1.405e32 | **−2.266e-03** | **FAIL**（最小主元 −1.57e-02） |
| `ab_2halpha` | A(Hα) B(Hα) | 0 | ok n_used=11456 | 11456 | A(Hα) | 1.025 | +2.117e-25 | OK |
| `abc_label_swap_all_halpha` | A B C 全标 Hα | **2** | **FAIL rc=6** | 17237 | A | 1.405e32 | −2.266e-03 | **FAIL** |
| `abc_label_swap_all_red` | A B C 全标 R | 0 | ok n_used=17237 | 17237 | **C(Red)** | 1.405e32 | **+1.665e+07** | OK |
| `def_samefilter` | D E F 全为真实同滤镜 Red | 0 | ok n_used=17266 | 17266 | F 系 | 1.058 | +3.966e+07 | OK |
| `abc_photscal_harmonized` | A B C，仅把 A/B 换算到 C 的光度单位 | 0 | **ok n_used=17237** | 17237 | — | **2.241** | **+2.685e-25** | **OK** |

要点：

1. **原样复现**：`abc_real` 的 stderr 与历史证据
   `实验/issue11-passband-identity/evidence/25-anomaly-3frame-mixed-response.stderr:21`
   逐字同构：`rc=6 at h=0.553742 (upper=0.553742 lower=0.0143966 coarsenings=0, last: reduced normal matrix not SPD)`。
2. **标签不改数值却改结论**：`abc_real`(Hα,Hα,R) 与 `abc_label_swap_all_halpha`(Hα,Hα,Hα)
   都 FAIL，`abc_label_swap_all_red`(R,R,R) 却 exit=0 —— 三者的 `n_obs=17237`、
   `overlap_controls=5807`、数值载荷完全相同，只有 `signal/properties` 一行不同。
   机制见 §4.3（锚帧 = 分量内最小 `frame_id`，而 `frame_id` 是帧元数据的哈希）。
3. **同滤镜同尺度正对照**：`ab_2halpha` 与 `def_samefilter` 的跨帧权重跨度 ≈1，
   独立 oracle 与生产都判 OK。
4. **因果验证**：`abc_photscal_harmonized` 只把 A/B 的 signal×c、variance×c²、ivar÷c²
   （c = C 自己的 `photscal=5.70245780684985e-17`），即让它与 C 同单位；
   模型自由度、参数基、样本集、gauge 全不变，**CLI 端到端 exit=0**，
   独立 oracle 的 λ_min 由 −2.27e-3 变为 **+2.685e-25**，且三个候选锚帧**全部 SPD**。
   该臂是**单位换算，不是 ridge/正则化**，见 `code/make_photscal_overlay.py` 与 §5.3。

生产成功的 `abc_photscal_harmonized` / `abc_label_swap_all_red` / `def_samefilter`
模型身份：`rank = n_params = 25`、`identifiable=1`、`κ(H_red)`≈1.2e4–2.6e4
（细节：`evidence/sky_plane_model_identity.json`）。
**注意**：`abc_real` 失败时 `ACSD_SKY_COMPONENT_DUMP` **不落盘**（payload 在
`sky_plane.cpp:2713` 才构造，而失败在 `:2275` 返回），故失败臂没有生产侧分量读数——
这正是 §6 要登记的诊断缺口。

---

## 3. 根因第一层（主因）：跨帧光度身份不一致

一手证据（各帧 `p1_stack.json`，真本读出，非推断）：

```
A (H-alpha, ab_off)      : photappl=0   photscal=1.0
B (H-alpha, ab_off)      : photappl=0   photscal=1.0
C (Red, PHOT-FIX)        : photappl=1   photscal=5.70245780684985e-17
```

- 两代 P1 产品把**同一个** `bunit="ADU/sr"`、`pixel_semantics="surface_brightness"`、
  `units{variance_bunit, ivar_bunit}` 原样写进 manifest 与 `p1_final.json`，
  但**只有一代施加了光度尺度**。
- 直接后果（HiPS tile 实测中位值）：
  `ab_off` 产品 ≈2.8e12、`PHOT-FIX` 产品 ≈5.4e-4 ⇒ 数值尺度相差 ≈5.2e15；
  换算回共同单位后，C 的原始量级 ≈8.98e12，与 A 的 2.65e12 同量级——
  即**两帧的原始数据本身是自洽的，差异全部来自未施加的 photscal**。
- 声明身份**无法**暴露该不一致：`obs_filter`（`Baader 7nm H-alpha` vs `Baader R`）
  在 #11 之后已明确**不是跨帧准入条件**（`coverage.cpp:9,227-229`），
  `BUNIT/pixel_semantics` 两者也完全相同。因此**任何基于声明字符串的闸门都拦不住它**，
  而这恰是 #80 的现象在"合法混通带输入"下出现的原因。

> 这是本批的**主要科学结论**：`Hα ×2 + R ×1` 的失败首先是一个
> **P1 光度身份 / 跨帧单位合同**缺陷，而不是 P5 求解器的数学范畴问题。

---

## 4. 根因第二层：数值机制（为何 double 下必然失稳）

### 4.1 量级分析

`H_red = Σ_k [A_kᵀW_k A_k − S_k M_k⁻¹ S_kᵀ]`（生产在帧循环里逐帧相减，`sky_plane.cpp:2051-2120`）。
以 Red 帧为主量级：`H_data` 对角 ≈ `n_Red × W_Red` ≈ 5702×1.77e11 ≈ **1.0e15**；
Hα 两帧贡献 ≈ 5776×1.29e-21 ≈ **7.5e-18**。二者相差 **1.3e32**。
Schur 减法中 `S M⁻¹ Sᵀ` 与被减项同量级（≈1e15），
一次双精度消减的绝对舍入 ≈ `1e15 × 1e-16 = 1e-1`，
而真实的 Hα 曲率只有 `7.5e-18` ⇒ **约化矩阵的"信号"被舍入噪声彻底淹没**，
其符号随机 ⇒ Cholesky 遇到负主元。

独立 oracle 实测（`evidence/matrix_oracle.json`，`abc_real`）：
`λ_max(H_red)=1.31e13`、`λ_min=−2.266e-03`、`effective_rank=22/25`、1 个负特征值、
对称残差 `‖N−Nᵀ‖/‖N‖` ~1e-16 量级（说明**不是**装配不对称/写错索引）。

### 4.2 同一数学问题的三种独立算法

| 算法 | 数学含义 | abc_real 结果（λ_min of `H_red`） |
| --- | --- | --- |
| 生产同序（`H_data` 一次性累加后逐帧减） | 与生产逐行同构 | **−2.265592e-03**（FAIL，rank 22/25） |
| 分帧 Schur（每帧独立 Schur 后相加） | **数学恒等**（`M` 因帧行互斥而块对角） | **−3.015114e-03**（FAIL，rank 22/25） |
| 白化 QR 残差 Gram `A_wᵀ(I−QQᵀ)A_w` | 构造性 PSD | **−4.901351e-04**（FAIL，rank 22/25） |
| 归一化分帧 Schur（§5.3） | 合法单位换算后的同一物理模型 | **+2.685301e-25**（OK，rank 25/25） |

`λ_max` 三者一致 = 1.311852e+13。前三种在 float64 下**都**给出负特征值，且两两之差
~1e-15 相对（`|H_joint−H_perframe|max=1.758e-02`，`rel=1.595e-15`；
对称残差 `sym_err_joint=9.411e-16`）。这证明：**不是某一种装配顺序写错**，而是该约化矩阵在 double 下
根本不可表示——任何 double 排列都救不回来。归一化（把数据换算到同一单位）后同一个
数学量立刻变成 PSD 且条件数正常。

### 4.3 规范锚帧由元数据哈希决定 ⇒ 同一数据两种结论

`sky_plane.cpp:640,724-728`：每个连通分量的锚帧 = **分量内最小 `frame_id`**；
锚帧 `δ≡0` 且 **被 `continue` 跳过 Schur 消元**（`:2056-2057`）。
`frame_id` 是帧元数据（含 `signal/properties`）的哈希 ⇒ **改 `obs_filter` 一行就会换锚帧**。

对同一份数值数据的三个候选锚帧（独立 oracle，`anchor_study`）：

| 锚帧 | 身份 | 权重档 | `H_red` λ_min | Cholesky | 生产对应臂 |
| --- | --- | --- | --- | --- | --- |
| 8477435787220171510 | **Hα** | 1.3e-21 | **−2.265592e-03** | **FAIL** | `abc_real` / 全 Hα |
| 17937401722011391352 | **Hα** | 1.3e-21 | **−2.263018e-03** | **FAIL** | — |
| 13095713314048628290 | **Red** | 1.8e+11 | **+1.665050e+07** | **OK** | 全标 R |

预测与实测**逐臂一致**（§2 表）。当锚帧是低权重帧时，高权重 Red 帧必须进入 Schur，
产生 1e32 量级的相消；当锚帧恰是 Red 帧时它被排除在 Schur 之外，剩下的两帧同尺度，
于是"成功"——但这个"成功"的科学含义是**公共面几乎完全由 Red 一帧的高权重决定**
（`rms_w=2.35e-05`，即 Red 的量级），Hα 帧几乎不参与。**即成功臂也不代表跨通带联合解成立。**

### 4.4 自适应放粗在 h=upper 处结构性不可用

`sky_plane.cpp:3179`：`can_coarsen = (h < ns.upper_deg) && …`。
首次尝试即 `h = upper_deg` ⇒ `can_coarsen=false` ⇒ 立刻 `finalize()` 并返回 rc=6
（`coarsenings=0`）。报错文本"adaptive node spacing exhausted"在此路径上**名不副实**：
它并没有尝试任何其他节点间距，唯一旋钮在起点就被禁止移动。

---

## 5. 独立 oracle 的构造与读数

`code/p5_matrix_oracle.py`（纯 NumPy，只读 `p2_samples.json`，不读 C++ 内部状态）。
逐行复刻：gnomonic 投影与切平面中心（`:1630-1645`）、均匀张量 B 样条 `d=3`
（`:197-222,1666-1695`）、自由节点门 `Σb²>1e-9·max`（`:1760-1774`）、
`δ` 归一化多项式 `order=1,m=3`（`:224-237`）、`W=1/control_variance`（`:1726`）、
锚帧规则与 Schur（`:640,2056-2057`）、`λ_eff=rank_rtol·mean(diag(H_red))`（`:2129-2132`）、
二阶差分惩罚与 Cholesky 主元记录（`:2137-2165,250-336`）。

**复刻一致性**（独立算得的 `n_full, n_free` 与生产自报 `info.n_nodes/n_params` 逐例相同）：

| 臂 | 独立 `nx×ny` / `n_full` | 独立 `n_free` | 生产 `n_nodes` / `n_params` | 结论一致 |
| --- | --- | --- | --- | --- |
| abc_real | 5×6 / 30 | 25 | （失败，无 bin） | ✓ rc=6 |
| abc_all_red | 5×6 / 30 | 25 | 30 / 25 | ✓ OK |
| ab_2halpha | 5×5 / 25 | 25 | 25 / 25 | ✓ OK |
| def_samefilter | 5×6 / 30 | 25 | 30 / 25 | ✓ OK |

### 5.1 投影 GLS 可辨识性（白化 SVD/QR）

`rank(Q_B P)`：消去公共场后逐帧加性场是否满列秩（`q=(K−1)m=6`）；
`rank(Q_P A)`：消去 δ 后公共场是否满列秩（`n_free=25`）；`rank([A|P])` 期望 `=31`。

| 臂 | 权重 | `rank(Q_B P)` | `rank(Q_P A)` | `rank([A\|P])` | 增广 σ_min | 增广 κ |
| --- | --- | --- | --- | --- | --- | --- |
| abc_real | 原始 | 6/6 | **22/25** | **25/31** | 6.31e-10 | **5.33e16** |
| abc_real | 归一 | 6/6 | **25/25** | **31/31** | 5.18e-13 | **5.58e03** |
| abc_all_red | 原始 | 6/6 | 25/25 | **25/31** | 1.45e-09 | **7.56e15** |
| abc_all_red | 归一 | 6/6 | 25/25 | **31/31** | 5.18e-13 | 5.87e03 |
| ab_2halpha | 原始=归一 | 3/3 | 25/25 | 28/28 | 4.60e-13 | 6.24e03 |
| def_samefilter | 原始=归一 | 6/6 | 25/25 | 31/31 | 6.30e+03 | 5.67e03 |

读数：
- **没有真实不可辨识方向**：`rank(Q_B P)` 在所有臂都满秩；归一化后 `rank(Q_P A)`、
  `rank([A|P])` 全部满秩。唯一被"去掉"的 3 维正是 `δ_anchor≡0` 规范（不是缺陷）。
- 原始权重下 `rank(Q_P A)` 掉 3、增广秩掉 6、κ 达 5.3e16——**纯数值退化**。
- `abc_all_red`（生产中"成功"的臂）在增广设计上 κ 仍达 7.6e15：**成功臂同样是数值退化的**。

### 5.2 谱 / 对称 / 主元

失败臂：对称残差 `sym_err_joint=9.411e-16`（装配对称性正常）；`H_red` 有 **1 个显著负特征值**
（λ_min=−2.265592e-03，相对 λ_max=1.311852e+13 是 −1.7e-16 相对量级，但绝对量级远大于
Hα 真实曲率 7.5e-18）；Cholesky 最小主元 **−1.5667e-02**（失败列 j=24）⇒ 直接失败；
`λ_eff = 1e-10·mean(diag(H_red))` 取的是被污染的对角 ⇒ 惩罚量级不可信，
且当 `mean_diag ≤ 0` 时 `lam=0` ⇒ `H_solve==H_red`，`alpha=0` ⇒ 生产连
deflation 回退分支都不会进入（`sky_plane.cpp:2182-2190`）。

### 5.3 单位归一臂（合法换算，非 ridge）

`W_k ← W_k · s_k²`（`s_k = photscal_k`，未施加记 1）等价于 `ỹ=y/s_k, ṽ=v/s_k²`，
设计矩阵 `A/P` 不变，`B_ref` 与 `δ_k` 同步换算。这是**同一物理模型的单位换算**，
不改变任何自由度/基函数/判据，也不改变可辨识性。

结果（abc_real，同一批数值）：跨帧权重跨度 1.4046e32 → **2.2407**；
`λ_min(H_red)` −2.265592e-03 → **+2.685301e-25**；`rank([A|P])` 25/31 → **31/31**；
增广 κ 5.3322e16 → **5.5829e03**；三个候选锚帧**全部 SPD**。
CLI 端到端（真实数据，仅换算单位）**exit=0**，`rank=n_params=25`、`identifiable=1`。

---

## 6. 三种严格判读的裁定

1. **真实不可辨识（秩亏/gauge）**：**否定**。连通分量只有 1 个（5623 个 3 帧控制）；
   投影秩 `rank(Q_B P)=6/6`；归一化后 `rank(Q_P A)=25/25`、`rank([A|P])=31/31`；
   规范自由度只有设计内的 `δ_anchor≡0`（3 维），与失败无关。
2. **矩阵装配错误**：**否定**（公式层面）。生产同序、数学恒等的分帧 Schur、
   白化 QR 残差 Gram 三种独立算法在 float64 下给出同一结论且两两一致到 1e-15 相对；
   对称残差 ~1e-16。**"装配错误"不成立，成立的是"该量在 double 下不可表示"。**
3. **浮点数值不稳定**：**成立且是直接机制**。跨帧权重跨度 1.405e32（10^32.1 ≫ 10^16）；
   三种 double 算法都产生负特征值，归一化后立刻 PSD。
4. **跨通带/跨代模型无效**：**成立且是主因**。§3 光度身份不一致是权重跨度的来源；
   并且 `Hα` 与 `Baader R` 之间没有共同响应/SED 变换，单个 `B_ref` 不能被解释为
   "共同天空 S"。

---

## 7. 明确未证与未决条件（不得误读）

- **未**证明"合法混通带输入应当跨帧共享一个 `B_ref`"。本轮只证明当前冻结输入的失败
  可由单位不一致解释；**跨真实通带仍需 #14 / #76 的响应或 SED 模型**，
  在此之前不得把归一化后的联合解当作有物理含义的共同天空面。
- **未**证明这是 P5 求解器的一般性算法失败。同尺度组（`ab_2halpha`、`def_samefilter`）
  全部通过；`abc_label_swap_all_red` 在生产中也通过（尽管增广设计 κ 仍达 7.6e15）。
- **未**给出生产修复方案。本批按要求**不加**任意 ridge、**不改** `sky_plane.cpp` 生产算法。
- 失败的 IRLS 迭代号、失败主元位置、回退分支是否被尝试：**生产侧当前不可观测**
  （`chol_spd` 只返回 bool；`p2_sky_plane.bin` 仅成功时写；`ACSD_SKY_COMPONENT_DUMP`
  在失败路径不构造；`retain_normal_matrices` 无任何调用方）。本轮以独立 oracle 补齐，
  若要在生产侧直接取证需另立只读诊断任务。
- `def_samefilter` 正对照的 `chi2_red=8643`（远大于失败臂本底）与 M42_M2 三帧含亮区有关，
  未在本批判定为问题；它只用作"同尺度可解"的对照。

### 具名的待决条件

| # | 待决 | 归属 |
| --- | --- | --- |
| C1 | P1 跨代产品的 `photappl/photscal` 必须一致，且须有**可校验的数值单位**（而非仅声明 `BUNIT` 字符串）合同 | P1 光度身份（新登记） |
| C2 | 跨帧准入是否需要"数值尺度一致性"检查？（声明字符串已不足以拦截） | #11 / #78 口径 |
| C3 | 不同真实通带共用 `B_ref` 所需的共同响应/SED 模型 | #14 / #76 |
| C4 | 规范锚帧是否应改为**确定性且科学中立**的规则（当前 = 元数据哈希最小值，使标签影响结论） | P5 设计（研究项） |
| C5 | `h == upper_deg` 时自适应无放粗退路；失败文本与行为不一致 | 工程健壮性 |
| C6 | 生产失败路径缺矩阵/主元/分量读数（诊断缺口） | 只读诊断任务 |

---

## 8. 产物与复跑

**代码**（`实验/issue80-p5-nonspd-e0/code/`）

| 脚本 | 作用 |
| --- | --- |
| `freeze_inputs.py` | 建冻结硬链接树 + 受控标签 overlay（不改真本），逐文件 sha256 → `evidence/input_index.json` |
| `run_repro.py` | 跑真实 CLI 各臂，记录 exit/stderr/关键标志/小产物哈希 → `evidence/replay_cases.json` |
| `run_component_diag.py` | 生产只读分量诊断（`ACSD_SKY_COMPONENT_DUMP`）→ `evidence/diag_normal.json` |
| `p5_matrix_oracle.py` | 独立白化 SVD/QR + 投影 GLS + Schur 复刻、锚帧置换、单位归一臂 → `evidence/matrix_oracle.json` |
| `make_photscal_overlay.py` | 受控**单位换算** overlay（signal×c、variance×c²、ivar÷c²）→ `evidence/photscal_overlay.json` |

**复跑**

```bash
# 0) 身份
git -C . rev-parse HEAD            # 58ee3fe03fe650ae4fce6e0aea6c2fd65dd6bcfd
ninja -C build/linux-sci-p5        # acsd = build/linux-sci-p5/acsd

# 1) 冻结输入（真本只读；scratch 下建硬链接树与标签/单位 overlay）
python3 实验/issue80-p5-nonspd-e0/code/freeze_inputs.py
python3 实验/issue80-p5-nonspd-e0/code/make_photscal_overlay.py

# 2) 真实 CLI 六臂（含复现失败臂与单位归一臂）
python3 实验/issue80-p5-nonspd-e0/code/run_repro.py \
  --arms abc_real,ab_2halpha,abc_label_swap_all_halpha,abc_label_swap_all_red,def_samefilter_red_3frame,abc_photscal_harmonized

# 3) 生产只读分量诊断
python3 实验/issue80-p5-nonspd-e0/code/run_component_diag.py --arms abc_real,abc_label_swap_all_red,ab_2halpha

# 4) 独立矩阵 oracle（含锚帧置换与单位归一臂）
python3 实验/issue80-p5-nonspd-e0/code/p5_matrix_oracle.py \
  --samples abc_real=<runs>/abc_real/p2_samples.json,... \
  --h-map "abc_real=0.5537417834846583,abc_all_red=0.5537417834846583,ab_2halpha=0.553735639305885,def_samefilter=0.5524731164276467" \
  --photscal "13095713314048628290=5.70245780684985e-17" \
  --json-out 实验/issue80-p5-nonspd-e0/evidence/matrix_oracle.json
```

**证据文件**（`实验/issue80-p5-nonspd-e0/evidence/`，仅轻量结果入库；大 HiPS/FITS 留在
`/workspace/issue80-scratch`，不上传 Git）
`input_index.json`、`replay_cases.json`、`matrix_oracle.json`、`sky_plane_model_identity.json`、
`diag_normal.json`、`photscal_overlay.json`、`cli_logs/`、`diag_logs/`。

---

## 9. 交付边界

- 生产源码**零修改**；未改 `sky_plane.cpp` 求解算法；未加任何 ridge/正则化；
  未创建分支/PR；未宣布 P5 科学闭合。
- 新增实验单元仅 `实验/issue80-p5-nonspd-e0/{code,evidence}`。
- 真实观测 FITS 只在 scratch 下以硬链接/受控拷贝存在，本单元只入库 sha256 索引。
- 状态：`REPRODUCED` + `ROOT_CAUSE_DETERMINED`（分层：主因跨代光度身份不一致；
  机制为 1e32 权重跨度下的 Schur 数值不可表示；放大条件为锚帧由元数据哈希决定），
  并保留 §7 的 C1–C6 未决条件。**P5 科学闭合仍未完成。**
