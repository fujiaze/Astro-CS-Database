> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# 三模式 covariance 传播与 effective PSF 定义（SCI-P2-001 强制交付项）

> 上游：ASTROCS_DESIGN.md §2（核心科学方法）、§3（数据对象与配置）

文档 ID：`SCI-P2-001-COVARIANCE-EPSF`
上位：`DESIGN-P3-001` §4（C_y = R C_x Rᵀ）；`ASTROCS-SCIENCE-MODEL-001` §6/§7；`SCI-PSFW-001` §5；
     `DESIGN-P2-001` §4/§6.1/§6.2/§6.3/§9；冻结宪章 §6.3/§4.1。
文献：Fruchter & Hook 2002（Drizzle 线性重建与相关噪声）；Zackay & Ofek 2017 II（arXiv:1512.06879，proper coadd/信息保持）；
     Horne 1986 DOI:10.1086/131801（PᵀC⁻¹P 统计结构）。

## 0. 统一公式：从**实际组合系数**传播

三模式共用一个不变量：**报告出来的 variance/covariance 必须用实现真正使用的线性组合系数与输入 covariance 计算**，
不得用理想 GLS 公式反推，也不得由任何权重标量反推。

```text
C_out = R C_in Rᵀ                                   (线性算子/矩阵形式)
Var(out) = cᵀ C_in c = Σ_{i,j} c_i c_j [C_in]_{ij}  (单输出标量形式)
```

- `R`（或向量 `c`）= 实际组合算子：含 UPM 光度尺度、重采样/插值核、rejection/validity 掩码、归一化与逐帧权重；
- `C_in` = 输入 covariance：随机项 + 可表示的相关项（共享 master、共同天光、drizzle 相关）；不能只存对角 variance 而不给相关核/低秩项（`UNIFIED` §7）；
- 锚：`DESIGN-P3-001` §4；`ASTROCS-SCIENCE-MODEL-001` §6/§7；`SCI-PSFW-001` §5；
  `DESIGN-P2-001` §6.3：「传播实际线性组合的 covariance」；控制包 `RULINGS.md` #5。

machine 可执行性：门规则 R6 要求记录同时给出 `covariance.propagation`、非空
`covariance.combination_coefficients` 与 `covariance.variance_from ∈ {combination_coefficients,...}`；
把 `variance_from` 写成 `weight`/`psfsw_robust_weight`/`median_source_snr`/`support`/`coverage`/`fwhm`/`psf_residual` 一律 REJECT
（见 `WEIGHT_PROVENANCE_GATE.md` §2）。

---

## 1. point_information 的 covariance

标量参数估计 `F_hat`（`DESIGN-P2-001` §6.2；`SCI-PSFW-001` §2）：

```text
A            = stack_k( a_k P_k )                       (设计向量)
c            = C_in⁻¹ A / (Aᵀ C_in⁻¹ A)                 (实际组合系数)
F_hat        = cᵀ d
W            = Aᵀ C_in⁻¹ A
Var(F_hat)   = cᵀ C_in c = 1 / W                        (C 正确时恒等)
```

逐帧显式形式（独立帧、块对角 `C_in`）：

```text
c_k = a_k C_k⁻¹ P_k / W,   W = Σ_k a_k² P_kᵀ C_k⁻¹ P_k
Var(F_hat) = Σ_k c_kᵀ C_k c_k = 1 / W
```

- **空间/地图形式**：当 `P_k = P_k(x,y)` 与 `a_k = a_k(x,y)` 为空间量时，「BT»A`、`c`、`W` 都是位置的函数；
  输出 point-source information map `W(x,y)` 时，每个位置报告 `Var(F_hat;x,y) = 1/W(x,y)`，并随产品保存系数来源
  （设计向量、输入 covariance、近似）。压成帧级标量须过均匀性/信息损失门（`DESIGN-P1-001` §8.3；`UNIFIED` §8）。
- **相关帧**：必须用联合 `C_in`（跨帧块非零），`c = C_in⁻¹A (AᵀC_in⁻¹A)⁻¹`；简单 `Σ_k` 求和会低估方差
  （Oracle C3；`UNIFIED` §4/§10）。
- **近似**：若实现用对角 `C̃` 得 `c̃`，则报告 `c̃ᵀ C c̃`（用真实 `C`）并给出与 `1/W` 的偏差比，
  不得直接报告理想 `1/W`（`PROJECT_SPEC` §3：近似必须有适用域与误差门）。
- Oracle C1.3 证明 `cᵀ C c == 1/W`（`< 1e-9`）；OM1 把 `c` 扰动 10% 后该 check 报红（rc=1）。

---

## 2. surface_gls 的 covariance

```text
R        = (Aᵀ C⁻¹ A)⁻¹ Aᵀ C⁻¹
x_hat    = R d
C_out    = R C_in Rᵀ = (Aᵀ C⁻¹ A)⁻¹                (C 正确时恒等)
```

- 恒等式 `R C_in Rᵀ = (Aᵀ C⁻¹A)⁻¹` 既是解析结果也是独立 Oracle 的不变量（Oracle C5，误差 `< 1e-12`）。
- **实际系数版本**：若实现使用近似 `R̃`（例：忽略 `a_k` 的像素 ivar），必须报告 `R̃ C_in R̃ᵀ` 及其相对
  `(AᵀC⁻¹A)⁻¹` 的比值；Oracle C4.3 给出该比值严格 > 1（近似劣化），C4.4 证明报告值随实际系数变化。
- **相关核**：若只落盘对角 variance，必须另存相关核 `ρ_ij = [C_out]_ij / sqrt([C_out]_ii [C_out]_jj)` 或可重建算子摘要
  （`UNIFIED` §7；`DESIGN-P2-001` §6.1）。
- `C_in` 必须纳入 drizzle 相关噪声、共同 master、UPM 参数与重叠重采样（`DESIGN-P2-001` §6.1）。

---

## 3. effective PSF 定义

**定义（操作式、可注入验证）**：effective PSF 是**实际组合算子**对单位通量点源的脉冲响应，
在输出位置 `x_o` 按声明约定归一：

```text
P_eff(x ; x_o) = [ Σ_k α_k(x_o) · a_k · (P_k ⊗ K_k)(x − x_o) ]
               / [ Σ_k α_k(x_o) · a_k · (P_k ⊗ K_k)(0) ]        (peak 归一)
```

其中 `α_k` 是实际组合系数（含 UPM 尺度/validity/归一），`K_k` 是重采样/插值核，`⊗` 为卷积。

- **积分归一约定**：面亮度产品要求 `Σ_x P_eff = 1`（PSF 归一 `ΣP=1`，`DESIGN-P1-001` §6）；peak 归一用于点源/detection statistic。
  归一约定**必须按产品族显式声明**，否则 FWHM/EE 有歧义。
- **conventional coadd**（`surface_gls`、`psfsw_robust`）：
  ```text
  P_eff = Σ_k α_k a_k (P_k ⊗ K_k) / Σ_k α_k a_k
  ```
- **matched-filter / proper coadd**（`point_information` 的输出图像面）：
  ```text
  q_k = C_k⁻¹ P_k;   P_eff ∝ Σ_k (q_k ⊗ P_k);   白噪声 q_k = P_k/σ_k² ⇒ P_eff ∝ Σ_k (P_k ⊗ P_k)/σ_k²
  ```
  锚：Horne 1986（matched filter）；Zackay & Ofek 2017 I/II（逐帧匹配滤波后组合、proper coadd 的 PSF 即其自卷积）。
- **FWHM/encircled energy 必须从 `P_eff` 测量**，不得由逐帧 FWHM 摘要（median/平均）代替：Oracle C6 证明
  `median(FWHM_k) ≠ FWHM(P_eff)`，且同一帧组在 equal 与 pixel-ivar 权重下 `FWHM(P_eff)` 不同。
- **可复现性**：P_eff 必须与 `K_k`（或 transfer 描述）和归一约定一起保存；只给 FWHM 标量不构成 effective PSF
  （门规则 R7 直接 REJECT）。
- Oracle C6 以「注入单位点源 → 用实际系数组合 → 与解析组合比对」验证定义，误差 `< 1e-12`。

---

## 4. UPM 参数不确定度进入 covariance

```text
C_out = C_stat + J C_θ Jᵀ,     J = ∂(output) / ∂θ|_{θ=UPM 参数}
```

UPM 光度尺度/背景参数的协方差 `C_θ` 必须传播到最终 covariance；共享系统项进低秩项/provenance，模型偏差进 validity，
不得伪装随机 ivar（`DESIGN-P2-001` §4；`UNIFIED` §6）。Oracle C9 给出含/不含 UPM 项的方差比 > 1 的可计算判据。

---

## 5. psfsw_robust 的 covariance（严格边界）

```text
α_k(p) = W_psfsw,k · v_k(p) / Σ_j W_psfsw,j · v_j(p)
I_out(p) = Σ_k α_k(p) d_k(p)
Var(I_out(p)) = Σ_{k,l} α_k(p) α_l(p) [C_in]_{kl}(p)
              = Σ_k α_k(p)² σ_k(p)²                     (帧独立、对角 C_in)
```

- `α_k` 必须是**实际**使用的系数（含组内归一 `W_psfsw`、validity 门 `v_k`、UPM 尺度）；
  `W_psfsw` 只决定相对贡献，**不能**定义 variance（`SCI-PSFW-001` §5；`RULINGS.md` #5）。
- 禁止：`Var = 1/W_psfsw`、`Var = Σ_k W_psfsw,k`、或把任何诊断量当方差面 → 门 R6 REJECT（mutation M04/M13）。
- Oracle C7.4/C7.5：实际系数方差与"复合权重方差代理"显著不同，且实际系数方差与蒙特卡洛一致（相对误差 < 3%）。
- effective PSF 用 §3 conventional 公式计算并输出，用于报告与 `W_info`/pixel-ivar 基线的 detection power、
  FWHM、通量偏差、面亮度偏差（`SCI-PSFW-001` §5）。
