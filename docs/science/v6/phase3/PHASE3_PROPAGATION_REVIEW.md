> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# SCI-P3-001 主正文 — Phase3 投影/采样下 signal、variance/correlation、effective PSF、Q/W 传播独立复核

文档 ID：`SCI-P3-001-REVIEW`
任务：SCI-P3-001（wave 1，depends_on BASE-OWN-001）
write_scope：`docs/science/v6/phase3/`、`run/v6/sci-p3/`
基线：任务开工时 `HEAD = main = 4b508f28bbcada66c417a8a9324aca7eb92869ff`（父提交 `ce5b3a00`）；
证据采集时 HEAD 已由控制器推进到 `192fab3546e710163e5dc276a9cd76309d650ff2`（parallel Wave-1 `SCI-P2-001` 集成，仅新增 `docs/science/v6/phase2/**` 与台账 1 行，未触及任何 phase3 路径）。本复核结论以 4b508f28 为科学基线，对 192fab35 同样成立（见 §0 与 `run/v6/sci-p3/logs/00_env_and_structure.log`）。
性质：**独立科学复核（review-only）**。不改科学公式/容差/冻结门，不写生产源码，不 commit/push。
建议状态：**PASS**（复核结论；实现属 W3/W5/W7，见 §7）

> 复核对象是 Phase3 在**投影与采样**下的传播科学契约（signal / variance+correlation /
> effective PSF / Q/W）与三输出模式的适用域、fail-closed 条件；不是要求本任务实现它们。
> 实现属 Wave 5 `IMPL-P3-PROJ-001`/`IMPL-P3-RSMP-001`，算法冻结属 Wave 3 `ALG-P3-001`，
> 产品链集成属 Wave 7 `P3-INTEGRATE-001`（`EXECUTION_GRAPH.md`、`TASK_MANIFEST.json`）。

## 0. 上位权威、复核口径与基线分歧

- 冻结宪章 `ASTROCS-CONSTITUTION-001`：§4.1（量不得混名）、§7.1（Phase3 输入/输出）、
  §7.3（投影/反向映射/浮点/variance 传播/显式拒绝）、§13.1（独立 Oracle）、§14.4（fail-fast）。
- 目标规范：`ASTROCS-PROJECT-SPEC-002` §3（统一模型 d_k=A_k x+n_k）、§6（Phase3 三模式与传播）、
  §7（数据对象不可混淆）、§8（科学正确性门）。
- 设计：`DESIGN-P3-001`（TARGET_NORMATIVE）§1（三模式）、§2（WCS/order/FOV）、§3（反向映射与采样核语义）、
  §4（`C_y=R C_x Rᵀ`、相关核、effective PSF、Q/W）、§5（FITS 产品）、§7（验收）。
- 统一科学：`ASTROCS-SCIENCE-MODEL-001` §2/§4/§5/§7/§8/§9/§10/§11；`SCI-PSFW-001` §2/§4/§5；
  `UNCERTAINTY_AND_COVARIANCE.md`（Phase3 节, DATA-P3-UNC-001）；`SCI-P3-001` §5/§7/§9a（含 DATA-UNC-001 更新块）。
- 文献：Calabretta & Greisen 2002, A&A 395,1077（FITS WCS Paper II，投影与面积元）；
  Greisen & Calabretta 2002, A&A 395,1061（Paper I，CD/CRPIX）；Fruchter & Hook 2002, PASP 114,144
  （Drizzle 线性重建与相关噪声）；Horne 1986, PASP 98,609, DOI:10.1086/131801 与 Naylor 1998, MNRAS 296,339
  （PᵀC⁻¹P 最优提取/成像最优 PSF）；Zackay & Ofek 2017, ApJ 836,187/188（arXiv:1512.06872/1512.06879，
  逐帧 matched filter 组合、proper coadd 信息保持）；Górski et al. 2005, ApJ 622,759（HEALPix）。
- 口径：**符合** = 契约自洽且有可复跑独立证据；**偏差** = 与上位条款不一致；
  **NOT_IMPLEMENTED** = HEAD 生产面零命中（术语按 `run/v6/base/gap_baseline.md`）。
- 跨阶段一致性：Phase3 的 `R` 是 HiPS→输出平面的重采样算子，`C_in` 是 Phase2 输出 covariance；
  「报告 variance/covariance 必须由**实际组合系数**传播」这一不变量与
  `docs/science/v6/phase2/COVARIANCE_AND_EFFECTIVE_PSF.md` §0 完全一致，本复核沿用不重定义。

> **基线分歧未裁决（必须在控制器裁决前如实标注）**：`run/v6/base/baseline_freeze.json` 记录 HEAD 工作树对
> 16 个 tracked 文件回退到祖先 blob、删除 10 个 tracked 文件；其中 `lib/phase3_session/p3_resample.{cpp,h}`
> 属回退命中（P30 blob）。经本任务只读 git 复核：两者**科学相关行逐字一致**——
> `p3_resample_check_mode` 的 `surface_brightness`/`weight`/`flux-per-pixel` 分支与
> `p3_uncertainty_propagate` 的 `acc += w * w * u_k` 在 worktree 与 HEAD 完全相同；
> 差异仅为 4 insertions / 102 deletions 的性能/缓存代码（`git diff --stat HEAD`）。因此本复核的科学结构结论
> 对 HEAD 与工作树同时成立；但**本复核不裁决 F1，也不把回退态当作已验证基线**。
>
> 施工中 HEAD 由控制器由 4b508f28 推进到 192fab35（仅集成 Phase2 姊妹复核，`git diff --name-only 4b508f28 192fab35`
> 未含任何 phase3 路径），故 §7 结构证据与全部 Oracle 数值对该两 SHA 均有效。

---

## 1. 符号与统一重采样模型

输入 HiPS 像素 `j`：值 `x_j`（面亮度语义，SB）；像素立体角 `Omega_j`（sr）；`[C_x]_{jk}` 为输入 covariance。
输出像素 `i`：立体角 `Omega'_i`；角尺度 `s_out`。

```text
投影 Jacobian:  Omega'_i = |det( d(sky)/d(pixel) )|_i          # TAN/SIN/CAR/AIT 各自由 Paper II 定义
重采样（线性算子）:            y = R x
SB 算子（行归一）:   R_ij = |Omega_j ∩ Omega'_i| / Omega'_i ,  Σ_j R_ij = 1
通量算子（列归一）:  S_ij = |Omega_j ∩ Omega'_i| / Omega_j ,  Σ_i S_ij = 1 ,  S_ij = R_ij · Omega'_i/Omega_j
输出数据（每像素通量）: f = S d ,  d_j = x_j · Omega_j
```

**C-P3-PROP-1（统一模型）** Phase3 的投影+采样是一个线性算子 `R`（SB 语义）及其等价的通量算子 `S`；
模式差异决定用 `R` 还是 `S`，以及是否携带 `Omega'_i`。
锚：`DESIGN-P3-001` §3/§4；`ASTROCS-SCIENCE-MODEL-001` §2/§7；`ASTROCS-PROJECT-SPEC-002` §3/§6；
Fruchter & Hook 2002（线性重建）。

**C-P3-PROP-2（SB 不含权、通量含面积）** 行归一 `Σ_j R_ij=1` 保证常数面亮度场不变量；
列归一 `Σ_i S_ij=1` 保证点源总通量守恒 `Σ_i f_i = Σ_j d_j`。二者在 `Omega'_i != Omega_j` 时数值不同，
**不得互相替代**。Oracle P3-O1（行和误差 0，`max|R·1−1|=1.1e-16`）与 P3-O2
（通量相对误差 `5.1e-12`，有效 PSF `Σq=1−5.1e-12`）验证。
锚：`ASTROCS-CONSTITUTION-001` §4.1/§7.3（面亮度与每像素通量分列）；`DESIGN-P3-001` §3；
`SCI-P3-001` §7 常数场不变量。

**C-P3-PROP-3（投影面积元必须逐像素）** `Omega'_i` 随天区变化：
CAR 在 CRVAL2=0 视场 ±60° 上 `Omega = s_rad·(sinδ_hi−sinδ_lo)`，`max/min = 2.0000`；
TAN/SIN 在各自合法 FOV 内分别 1.0202/1.0068；**AIT 等积**，`max/min = 1.00000006`（Ω≈const）。
外部 WCSLIB（astropy）与 Paper II 解析 CAR 面积元交叉一致到 `1.27e-6`（Oracle P3-O7）。
锚：`ASTROCS-CONSTITUTION-001` §7.3（每种投影声明适用天区/合法 FOV）；`DESIGN-P3-001` §2；
Calabretta & Greisen 2002 §5/§6（各投影面积元与 Aitoff 等积性）。

---

## 2. signal 传播

**C-P3-PROP-4（输出模式决定 signal 语义与 BUNIT）**
- `surface_brightness`：输出 `y = R x`，BUNIT 为面亮度单位；常数场不变量要求行归一。
- `point_source_flux`：输出为通量/统计量族（Q、W、flux、detection statistic）；每像素值若为 SB，
  必须同时给 `Omega'_i` 才能换算总通量 `Σ y_i Omega'_i`。
- `visualization`：显示型降级，不得声明测量语义（§6）。
锚：`DESIGN-P3-001` §1/§5；`ASTROCS-PROJECT-SPEC-002` §6；`ASTROCS-CONSTITUTION-001` §4.1/§7.1。

**C-P3-PROP-5（插值不制造信息）** 当 `order_sel = min(hips_order, order_needed) < order_needed`
（输入原分辨率粗于输出）时，输出被过采样，`R` 只能插值，**不得**据 `Σ c_k²u_k < u` 宣称信息增加：
逐像素方差下降来自平滑与相邻相关，点源信息 `W` 被输入 PSF/order 上限封顶。
Oracle P3-O10 量化有效 PSF 采样损失：固定输出逐像素方差约定下，`h_out/σ = 1,1.5,2,3` 对应
`W = 13.63, 8.12, 4.80, 4.04`，即 `h_out=3σ` 时信息降至 `h_out=1σ` 的 0.30 倍。
锚：`DESIGN-P3-001` §2（Nyquist/信息损失约束选 order）/§3（采样核是产品语义）；
`SCI-P3-001` §9a-5（order 截断按原生分辨率记录）；Naylor 1998（成像最优 PSF 的采样依赖）。
> 诚实边界：P3-O10 的 `W` 是**固定输出逐像素噪声约定下**的匹配滤波信息，绝对归一依赖噪声/面积约定，
> 故只作适用域标记，**不冒充** `W_info` 的绝对标度。

**C-P3-PROP-6（缺失/非有限不得零填充）** `coverage=0` 或输入 NaN 时输出 NaN 并记录 provenance，
不得以 0 填充（否则污染 flux 积分与匹配滤波）。
锚：`DESIGN-P3-001` §3/§5；`SCI-P3-001` §5/§8；`UNCERTAINTY_AND_COVARIANCE.md`（Phase3 节, invalid 表）。

---

## 3. variance / correlation 传播

**C-P3-PROP-7（一般式）** 线性算子下：

```text
C_y = R C_x Rᵀ
Var(y_i) = Σ_j R_ij² [C_x]_jj + 2 Σ_{j<k} R_ij R_ik [C_x]_jk
rho_ij   = [C_y]_ij / sqrt([C_y]_ii [C_y]_jj)
```

锚：`DESIGN-P3-001` §4；`ASTROCS-SCIENCE-MODEL-001` §7；与 Phase2
`COVARIANCE_AND_EFFECTIVE_PSF.md` §0 同一不变量。

**C-P3-PROP-8（现行生产公式的适用域 = 对角输入）** 现行 `p3_uncertainty_propagate` 计算
`var_out = Σ_k c_k² u_k`（源码 `acc += w*w*u_k`），这正是 `diag(R C_x Rᵀ)` 在 `C_x` **对角**时的严格结果
（Oracle P3-O3 误差 0）。但当输入 HiPS 由 Drizzle 产生、存在相邻相关
（`UNCERTAINTY_AND_COVARIANCE.md` 记录 nside=512 `mean|ρ|≈0.19`, `max|ρ|≈0.57`，SNR-012）时，
该公式省略交叉项 `2Σ_{j<k} c_j c_k [C_x]_jk`，**系统性低估**方差：Oracle P3-O4 以 `ρ=0.19` 实测
对角省略的相对亏损中位数 **23.3%**（`median_diag_deficit = 0.2331`），
完整式与 Monte Carlo 一致（中位相对误差 0.39%）。当前代码/合同**未声明**该适用域。
锚：`DESIGN-P3-001` §4（只出对角须给相关核/近似误差）；`UNCERTAINTY_AND_COVARIANCE.md`（协方差节与 Phase3 节）；
`ASTROCS-PROJECT-SPEC-002` §3（简化必须有适用域与误差门）。

**C-P3-PROP-9（相关核是强制输出，不是可选）** 即使输入 `C_x=σ²I`，重采样也使
`C_y = σ² S Sᵀ` 非对角；只存对角 variance 时，任何下游匹配滤波/孔径求和都必须显式使用相关核
`rho_ij` 或可重建算子摘要，并给出近似误差；`coverage` 不得代替 variance。
Oracle P3-O5：以完整 `C_y` 的 `W=qᵀC_y⁻¹q` 与 MC 严格一致（rel 0.0），
而对角化后声明的 `1/W_diag` 比同一估计量的真实方差**低 28.7%**（claimed 21.71 vs actual 30.45）——即过度乐观。
锚：`DESIGN-P3-001` §4；`ASTROCS-SCIENCE-MODEL-001` §7/§10；Phase2 `COVARIANCE_AND_EFFECTIVE_PSF.md` §2；
Fruchter & Hook 2002（Drizzle 相关噪声）。

**C-P3-PROP-10（BUNIT 二次律）** 输出 variance 的 BUNIT 必须是 signal BUNIT 的平方，
ivar 为其倒数；语义择一且一致。
锚：`DESIGN-P3-001` §5；`SCI-P3-001` §9a-11 + DATA-UNC-001 更新块；`DATA_SEMANTICS.md` §30.4。

---

## 4. effective PSF 传播

**C-P3-PROP-11（定义）** Phase3 输出的 effective PSF 是**实际采样算子对单位通量点源的脉冲响应**，
即对输入 effective PSF 施加同一重采样核：

```text
flux 归一（面亮度产品）:  pi_i = (S p)_i ,  Σ_i pi_i = 1        # p 为输入像素积分归一 PSF
peak 归一（点源/检测）:   使用 Phase2 §3 的操作式定义并按产品族显式声明归一约定
```

Oracle P3-O2/P3-O10：`Σq=1` 在全部采样下成立（误差 ≤5.1e-12）；对齐网格上
`q_res = S p` 与直接在输出网格积分的参考一致（L1 差 0）；非对齐网格（`h_out=1.5σ`）
L1 差 0.16，即插值核误差必须显式声明。
锚：`DESIGN-P3-001` §3/§4；`ASTROCS-SCIENCE-MODEL-001` §9（Phase2→Phase3 需 effective PSF）；
`docs/science/v6/phase2/COVARIANCE_AND_EFFECTIVE_PSF.md` §3；Horne 1986；Naylor 1998。

**C-P3-PROP-12（不得把 signal 插值规则机械套用于 PSF/Q/W）** 点源匹配滤波必须使用输出有效 PSF `pi`；
把输出 PSF 当单像素 δ（或沿用输入帧 PSF）会产生大偏差：Oracle P3-O6 实测 δ 近似的通量偏差 **94.8%**。
锚：`DESIGN-P3-001` §3 明文（"point-source Q、W、PSF 参数不得把普通 signal 插值规则机械套用"）/§4；
Zackay & Ofek 2017 I（普通先叠加后滤波损失灵敏度）。

**C-P3-PROP-13（PSF 缺失即 fail-closed）** 输入无 PSF 层或 PSF 归一化非法（`ΣP≠1`）时，
`point_source_flux` 必须拒绝或输出显式 unavailable，**不得**静默退化为面亮度模式。
锚：`DESIGN-P3-001` §1（缺少所选模式所需信息时拒绝或显式 unavailable）/§4。

---

## 5. Q / W 传播

**C-P3-PROP-14（输出帧重算，不是重采样输入 Q/W）** 输出帧：

```text
a = 光度响应尺度（归一到公共通量尺度后）
Q = a · piᵀ C_y⁻¹ f ,   W = a² · piᵀ C_y⁻¹ pi ,   F_hat = Q/W ,   Var(F_hat) = 1/W
```

`W` 是 `(pi, C_y)` 的二次型；`R` 与匹配滤波不可交换，故 `W_out != Σ_i (重采样核) W_in,i`。
必须在输出帧以**输出有效 PSF `pi=S p`** 与**完整（或带相关核的）`C_y`** 重算。
Oracle P3-O5：`qᵀC_y⁻¹q` 与 MC 方差严格一致（rel 0.0）；M3-05 证明对角化使声明方差
过度乐观（`claimed_var 21.71 < actual_var 30.45`，rc=1）。
锚：`ASTROCS-SCIENCE-MODEL-001` §4/§4.1/§9；`DESIGN-P3-001` §3/§4；
`docs/science/v6/phase2/COVARIANCE_AND_EFFECTIVE_PSF.md` §1；Horne 1986；Zackay & Ofek 2017 I。

**C-P3-PROP-15（上游权重语义边界）** Phase3 消费上游 `point_information`/`W_info`，**不重算、不替换**：
`psfsw_robust_weight` 等无量纲组内相对复合权重**不得**写入 ivar/variance，也不得冒充 `W_info`；
covariance 只能从实际组合系数与输入 covariance 传播。三模式规则门 `GLOBAL` 对所有模式生效
（Oracle 门 mutation `gate_pf_weight_as_ivar` rc=1）。
锚：`SCI-PSFW-001` §5/§8；`ASTROCS-SCIENCE-MODEL-001` §11；`RULINGS.md` #5；
`ASTROCS-PROJECT-SPEC-002` §6。

---

## 6. 三输出模式：适用域与 fail-closed 矩阵

| 层 / 规则 | `surface_brightness` | `point_source_flux` | `visualization` |
|---|---|---|---|
| signal 语义 | SB（行归一） | Q/W/flux/detection | 显示拉伸 |
| per-pixel flux（`f=S d`） | 仅当显式带 `Omega` 换算 | 必需（或由 SB+`Omega` 导出） | 禁止冒充测量 |
| 逐像素立体角 `Omega_i` | 若做 flux 换算则必需 | 必需 | 不需要 |
| variance / ivar | 声明测量则必需，且 BUNIT 二次律 | 必需（`C_in`） | 可选，不得充当测量 |
| correlation 核/近似误差 | 只输出对角时必需 | 必需 | 不适用 |
| coverage / validity | 必需 | 必需 | 可选 |
| PSF / effective PSF | 可选 | **必需**（否则 fail-closed） | 不适用 |
| point information W | 不适用 | 必需（或可由逐帧 PSF+cov 重建） | 不适用 |
| 光度响应尺度 `a` | 不适用 | 必需 | 不适用 |
| measurement 标记 | true | true | **必须 false** |

**C-P3-PROP-16（逐模式 fail-closed 条件）**
- `surface_brightness`：(i) `flux_per_pixel` 输入而无逐像素立体角映射 → 拒绝；
  (ii) 声明测量却 uncertainty unavailable → 拒绝（应显式 unavailable，禁静默）；
  (iii) variance BUNIT ≠ signal BUNIT² → 拒绝；(iv) 只出对角 variance 而无相关核/近似误差 → 拒绝。
- `point_source_flux`：缺 PSF / PSF 未归一 / 缺 `point_information` 且不可由逐帧 PSF+covariance 重建 /
  协方差仅对角且无相关核 / 缺光度尺度 / 未输出 effective PSF → 任一命中即拒绝。
- `visualization`：`measurement_capable=true` 或写 VARIANCE/IVAR/POINT_INFORMATION 作测量层 → 拒绝；
  允许缺 variance/PSF，但 manifest 必须记录降级与拉伸参数。
- 全局：任何模式把上游相对复合权重写成 ivar/variance → 拒绝。
全部 12 个门 mutation 均使门变红（`mutate-all` rc=0，逐条 rc=1）。
锚：`DESIGN-P3-001` §1/§4/§5；`ASTROCS-CONSTITUTION-001` §7.1/§7.3/§14.4；
`ASTROCS-PROJECT-SPEC-002` §6/§7；`SCI-PSFW-001` §5。

---

## 7. 现状结构证据（HEAD=4b508f28 生产面）

| 结论 | 证据锚 | 判定 |
|---|---|---|
| 仅 `surface_brightness` 输入模式可接受；`weight`/`flux-per-pixel` UNSUPPORTED，variance/ivar 转 uncertainty 子产品 | `p3_resample.cpp` `p3_resample_check_mode`；`p3_resample.h` §23-28 | 符合（alpha 面） |
| 无 `point_source_flux`/`visualization` 模式 | 结构扫描 `lib/phase3_session`+`lib/algorithms/projection`：0 命中（Oracle P3-O11） | **NOT_IMPLEMENTED** |
| 无 effective PSF / Q / W / POINT_INFORMATION / W_info 传播 | 同上；`p3_output` 仅 SIGNAL+COVERAGE(+VARIANCE/IVAR) | **NOT_IMPLEMENTED** |
| variance 传播仅对角 `Σc²u`，无相关核输出 | `p3_resample.cpp` `acc += w*w*u_k`；`p3_output.h` EXTNAME 集合 | **偏差**（缺 DESIGN-P3 §4 相关核） |
| 无逐像素立体角/面积元换算 | `grep -riE 'jacobian|solid.?angle|pixel.?area|omega'` 于 phase3 源码 0 命中 | **NOT_IMPLEMENTED** |
| 四投影 registry（TAN/SIN/CAR/AIT，max_fov 20/60/180/360°）源码 IMPLEMENTED，但会话仍仅 TAN | `p3_projection.cpp:267-273`；`p3_session.cpp` `projection must be TAN`；`module.yaml entrypoint: MISSING` | 源码符合，安装未达成 |

现状生产路径（`p3_session`）对 `projection != "TAN"` 返回 `ACS_ERR_UNSUPPORTED`，
即目标态四投影与三模式在**生产会话不可达**；这与 `gap_baseline.md` §2/§2.7、
`memory.md` §5-4 一致。锚：`ASTROCS-CONSTITUTION-001` §7.3/§18.1；`DESIGN-P3-001` §1/§2。

---

## 8. 独立 Oracle 与负向 mutation

- Oracle：`run/v6/sci-p3/tools/p3_propagation_oracle.py`（Python/numpy；**不 import/不调用任何生产实现**；
  投影面积元以外部 WCSLIB/astropy 交叉并以 Paper II 解析式二次校验）。
- 正向：`python3 run/v6/sci-p3/tools/p3_propagation_oracle.py run …` → `rc=0`（11/11 检查通过）。
- 负向：`… mutate-all …` → `rc=0` 且 `all_detected=true`（22 个 mutation，逐条单跑 `rc=1`）。
- 关键量化（详见 `run/v6/sci-p3/data/`）：`rho=0.19` 对角省略亏损 23.3%；
  对角化输出信息过度乐观 28.7%；δ-PSF 通量偏差 94.8%；常数 Ω 通量偏差 97.5%；
  `W` 随 `h_out/σ` 由 1→3 降至 0.30 倍；CAR `Omega` 极差 2.0000，AIT 等积 1.00000006。
- 结构断言亦带 mutation：向 `p3_resample.cpp` 影子文本注入 `point_source_flux` 分支 → 扫描器报红 rc=1。

---

## 9. Findings / 移交 SCI-ADJ-001 → ALG-P3-001

| ID | 内容 | 处置所有者 |
|---|---|---|
| F3-01 | DESIGN-P3 §4 要求"只输出对角 variance 时给相关核/近似误差"，现行 Phase3 权威（`UNCERTAINTY_AND_COVARIANCE.md` Phase3 节）与代码只出对角、无相关核；Phase2 已具备该门（`COVARIANCE_AND_EFFECTIVE_PSF.md` §2），Phase3 应对齐 | SCI-ADJ-001 → ALG-P3-001 |
| F3-02 | `Σc²u` 的适用域"输入 covariance 对角"未在 SCI/ALG/代码声明；Drizzle 输入 `mean|ρ|≈0.19` 下低估 23.3% | ALG-P3-001 / DATA-DESIGN-001 |
| F3-03 | Phase3 无逐像素立体角/面积元；`每像素通量`（宪章 §7.3）无对应合同对象 | DATA-DESIGN-001 / ALG-P3-001 |
| F3-04 | `point_source_flux`/`visualization`/effective PSF/Q-W 传播 NOT_IMPLEMENTED | IMPL-P3-RSMP-001 / P3-INTEGRATE-001（W5/W7） |
| F3-05 | FROZEN `SCI-P3-001` §1/§9a-10 仍含"variance 输入拒绝 / SIN·CAR 拒绝"字样，仅 §9a-10 由 DATA-UNC-001 更新块改写；投影部分未同步宪章 §18.1 | SCI-ADJ-001 / DOC-CONVERGE-001（不得改冻结正文，须走正式 amendment） |
| F3-06 | `p3_resample.{cpp,h}` 命中 F1 工作树回退；科学相关行与 HEAD 一致，仅性能代码差异 | 控制器裁决（F1） |

---

## 10. 复核结论

1. Phase3 在投影/采样下的传播科学可用统一线性模型 `y=R x`、`C_y=R C_x Rᵀ`、`pi=S p`、
   `Q/W=a^{(1,2)} piᵀC_y^{(-1,0)}·(f,pi)` 完备表述，且与上位规范、Phase2 姊妹文档一致。
2. 三输出模式的适用域与 fail-closed 条件已冻结为可执行规则门（§6），22 个负向 mutation 全部能红。
3. 现状生产面仅 `surface_brightness`，方差传播仅对角且未声明适用域；相关核、逐像素立体角、
   effective PSF、Q/W、`point_source_flux`/`visualization` 均为 **NOT_IMPLEMENTED/偏差**，已登记 F3-01..F3-06。
4. 本复核**不改**任何科学公式、容差、冻结门，**不**宣布发布；实现与合同冻结由 W3/W5/W7 承担。

---

## 11. Oracle 命令与证据索引

```text
run/v6/sci-p3/tools/p3_propagation_oracle.py     # 独立 Oracle（正向 + mutation）
run/v6/sci-p3/tools/verify_review.py             # 本文档结构/锚/数值一致性验证 + mutation
run/v6/sci-p3/data/oracle_results.json           # 正向 11 检查
run/v6/sci-p3/data/oracle_mutations.json         # 22 mutation
run/v6/sci-p3/data/review_consistency.json       # 文档一致性验证
run/v6/sci-p3/logs/                             # 全部命令 stdout/stderr 与 rc
```
