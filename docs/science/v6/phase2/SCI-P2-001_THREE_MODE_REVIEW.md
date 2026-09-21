> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# SCI-P2-001 主正文 — point_information / surface_gls / psfsw_robust 独立复核

> 上游：ASTROCS_DESIGN.md §2（核心科学方法）、§3（数据对象与配置）

文档 ID：`SCI-P2-001-REVIEW`
任务：SCI-P2-001（wave 1，depends_on BASE-OWN-001）
基线：`HEAD = main = 4b508f28bbcada66c417a8a9324aca7eb92869ff`
性质：**独立复核（review-only）**。不改科学公式/容差/冻结门，不写生产源码，不 commit/push。
建议状态：**PASS**

> 复核对象是 Phase2 三个**冻结模式**的科学契约（目标函数、输出、最优性边界、验证），
> 不是要求本任务实现它们。实现属 Wave 5 `IMPL-P2-{UPM,REJ,SAMP}` 与 Wave 8 `P2-INTEGRATE-001`
> （`EXECUTION_GRAPH.md`、`TASK_MANIFEST.json`）；算法冻结属 Wave 3 `ALG-P2-{POINT,PSFSW,SURF}-001`。

## 0. 上位权威与复核口径

- 冻结宪章 `ASTROCS-CONSTITUTION-001` §4.1（量不混名）、§6.3（integration 必须明确不确定度传播）、§1.1（权威分层：冲突不得选方便的一层）。
- 目标规范：`ASTROCS-PROJECT-SPEC-002` §3（统一观测模型）、§5（Phase2 三模式与输出族）、§7（数据对象不可混淆）、§8（科学正确性门）。
- 设计：`DESIGN-P2-001`（TARGET_NORMATIVE）§6.1/§6.2/§6.3/§7/§9/§10；`DESIGN-P1-001` §8.1/§8.2/§8.3/§11；`DESIGN-P3-001` §4。
- 统一科学：`ASTROCS-SCIENCE-MODEL-001` §2/§3/§4/§4.1/§5/§6/§7/§10/§11；`SCI-PSFW-001` §2/§3/§4/§5/§6/§7/§8。
- 控制包裁决 `RULINGS.md` #3（双轨）、#4（四分量/共同星集/归一/validity 可审计）、#5（psfsw 不写 ivar、covariance 从实际组合系数传播）。
- 文献：Horne 1986 PASP 98,609 DOI:10.1086/131801；Naylor 1998 MNRAS 296,339；Zackay & Ofek 2017 ApJ 836,187（arXiv:1512.06872）与 ApJ 836,188（arXiv:1512.06879）；PixInsight ImageWeighting §2.5/§2.6。
- 口径：**符合** = 契约自洽且有可复跑独立证据；**偏差** = 与上位条款不一致；**NOT_IMPLEMENTED** = HEAD 生产面零命中（按 `run/v6/base/gap_baseline.md` 术语）。

---

## 1. 三模式总表

| mode | 目标函数 | 权重对象与单位 | 可作的最优性声明 | 主要输出 | 最优性失效条件（fail-closed/gate） |
|---|---|---|---|---|---|
| `point_information` | 最小化 `Var(F_hat)`／最大化点源检测 SNR | `W_info = a²PᵀC⁻¹P`，单位 `1/flux²` | 模型与 covariance 门通过时：BLUE / 最大点源 SNR / 最小通量方差 | Q、W map、`Q/W` flux、detection statistic、effective/proper PSF、`1/W` variance、逐帧 Q_k/W_k | PSF/光度响应/协方差未知或近似未经误差门；存在跨帧相关却用 Σ 求和 |
| `surface_gls` | 最小化面亮度估计方差 | `AᵀC⁻¹A` 正规矩阵，单位 `1/(surface_brightness²)` | GLS 假设成立时：Gauss–Markov BLUE | signal、variance、correlation 描述、effective PSF、support、coverage、rejection | 像素 ivar 近似在 A 非退化/噪声相关时未加误差门；C 未含 drizzle 相关/共同 master/UPM 参数 |
| `psfsw_robust` | 兼顾 common-star PSF signal、concentration、稳健 noise、稳健 background 的 conventional integration 质量 | `psfsw_robust_weight`，**无量纲、组内相对** | **只能**声明"在指定验收数据上优于指定基线"；不得声明 Fisher 最优 | 四分量、相对权重、conventional coadd、variance/correlation、effective PSF、基线比较 | 无共同星集/selection function、背景非正、有效星不足、选择偏差门失败 → `unavailable`，不得回退 median source SNR |

三模式的**默认模式选择**必须由配置显式给出，不得由"检测到多少颗星"等偶然因素自动切换（`SCI-PSFW-001` §4 末段）。
`psf_snr_power` 只有 `SCI-PSFW` 冻结后才可进入生产（`00_READ_FIRST.md`），本复核**不**为其背书。

---

## 2. point_information

### 2.1 目标函数

模型 `d_k = a_k F P_k + n_k`，`Cov(n_k)=C_k`（`PROJECT_SPEC` §3；`DESIGN-P2-001` §6.2）。充分统计量：

```text
Q_k       = a_k P_kᵀ C_k⁻¹ d_k
W_info,k  = a_k² P_kᵀ C_k⁻¹ P_k
Q         = Σ_k Q_k          (独立帧)
W         = Σ_k W_info,k     (独立帧)
F_hat     = Q / W
Var(F_hat)= 1 / W
```

锚：`DESIGN-P2-001` §6.2；`ASTROCS-SCIENCE-MODEL-001` §4；`SCI-PSFW-001` §2；`DESIGN-P1-001` §8.1。统计结构源自 Horne 1986（已知轮廓与方差下的最优提取）与 Naylor 1998（成像最优 PSF 光度）；多帧逐帧 matched filter 后组合见 Zackay & Ofek 2017 I。

白噪声近似（`SCI-PSFW-001` §2、`DESIGN-P1-001` §8.1）：

```text
W_info,k = a_k² / (sigma_pix,k² · A_NEA,k),    A_NEA,k = 1 / Σ_p P_k,p²
```

### 2.2 输出

按 `DESIGN-P2-001` §9 的 `point_source` 产品族：Q、W、flux(`Q/W`)、detection statistic / score map、point-source information map `W`、
effective/proper coadd PSF，外加 `support`/`coverage`/`validity`/`rejection`、UPM 与 provenance。
不得只写"一张 signal + 一个语义不明的 weight"（`DESIGN-P2-001` §9 末段；`PROJECT_SPEC` §5 末段）。

### 2.3 最优性边界

1. `point_information` 就是**单参数** `F` 在参数化设计向量 `A = [a_k P_k]` 下的 GLS 解：`F_hat = (AᵀC⁻¹A)⁻¹AᵀC⁻¹d`，`Var = (AᵀC⁻¹A)⁻¹`。Oracle C1 数值证明二者恒等（差 < 1e-9）。
2. `Q=ΣQ_k, W=ΣW_k` **只在帧间噪声独立**时成立（`UNIFIED` §4）。存在跨帧相关（共享 master、共同天光、UPM 参数）时必须用联合 `C`，否则方差被低估（Oracle C3 给出过度乐观比值）。
3. 最优性前提：`P_k`、`a_k`、`C_k` 已知或近似经误差门；高斯噪声；单点源、固定位置与形状。
4. 对扩展源/面亮度**不**最优（应走 `surface_gls`）；对 PSF 不同帧的普通像素 ivar coadd 不保证最大点源 SNR，不得宣称等价（`DESIGN-P2-001` §6.2；`UNIFIED` §11）。
5. `W_psf` 是空间量，压成帧级标量须过均匀性/信息损失门并输出 p05/p50/p95、覆盖与模型误差（`DESIGN-P1-001` §8.3；`UNIFIED` §8）。

### 2.4 验证（`DESIGN-P2-001` §10；`DESIGN-P1-001` §11）

- 独立高精度矩阵/NumPy Oracle 验证 Q/W 与 covariance（本任务 Oracle C1/C2/C3）。
- 注入点源：理论 `sigma_F = 1/sqrt(W_info)` 与实测 flux dispersion 一致（`PROJECT_SPEC` §8）。
- 独立帧 `SNR_combined² = Σ SNR_k²`；相关帧时简单求和必须被拒（`UNIFIED` §10）。
- 改变星表亮度分布不改变同一图像的 `W_info`，但改变 median source SNR（Oracle C8）。
- PSF 集中度（`A_NEA`）、透明度/seeing/背景/read noise 单变量扫描方向正确。

---

## 3. surface_gls

### 3.1 目标函数

对同一输出 sky element（扩展源/面亮度）估计 `x`（`DESIGN-P2-001` §6.1；`UNIFIED` §5）：

```text
x_hat      = (Aᵀ C⁻¹ A)⁻¹ Aᵀ C⁻¹ d
Cov(x_hat) = (Aᵀ C⁻¹ A)⁻¹
```

工程近似为独立样本、且线性算子退化为同点采样时才退化为像素 ivar 加权平均；该退化**不是**点源 PSF-aware 最优的替代（`UNIFIED` §5）。
`C` 必须纳入 drizzle 相关噪声、共同 master、UPM 参数与重采样导致的协方差，或用 correlation kernel/低秩项近似（`DESIGN-P2-001` §6.1）。

### 3.2 输出

`surface_brightness` 产品族：signal、variance、correlation 描述、effective PSF、support、coverage、rejection（`DESIGN-P2-001` §9）。
`support/coverage` 不得作为 inverse-variance 或 SNR 权重（宪章 §6.3；`UNIFIED` §3 术语表）。

### 3.3 最优性边界

1. 在 `Cov(n)=C` 正确时是 Gauss–Markov BLUE。
2. 近似（像素 ivar、对角 C）必须给出适用域与误差门（`PROJECT_SPEC` §3：任何标量化/独立噪声/对角 covariance 假设都必须有适用域和误差门）。
3. 当各帧光度响应 `a_k` 不同或噪声相关时，忽略它们的 ivar 近似方差严格劣化（Oracle C4.3 给出比值）。
4. `A` 非退化（重采样算子不同）时同点采样假设失效，`x_hat` 与简单平均不同（Oracle C5）。

### 3.4 验证

独立高精度矩阵 Oracle 验证 GLS 与 covariance（Oracle C4/C5）；扩展源常量场、梯度、总通量与方差无偏（`DESIGN-P2-001` §10）；相关噪声失配能红。

---

## 4. psfsw_robust

### 4.1 目标函数（复合相对权重，非 Fisher）

对同一波段、同一目标/重叠连通分量、光度已归一的帧组，取共同恒星集合 `S`（或显式 selection-function 校正），
每帧计算四分量（`SCI-PSFW-001` §3；`DESIGN-P2-001` §6.3）：

- `S_k`：PSF signal（PSF 总 flux，代表总 signal）；
- `Conc_k`：signal concentration（mean PSF flux / `A_NEA` 类集中度）；
- `N_k`：稳健 noise（MAD 类）；
- `B_k`：稳健 background（正值，否则变换未定义）。

复合与组内归一（指数/截断/归一常数**版本化**；具体数值由 `SCI-PSFW`/`ALG-P2-PSFSW-001` 冻结，本复核只给可容许结构）：

```text
Wt_k      = C_norm · S_k^α · Conc_k^β / (N_k^γ · B_k^δ)      (α,β,γ,δ ≥ 0，版本化)
W_psfsw,k = Wt_k / median_j(Wt_j)                           (组内 median=1，无量纲、相对)
```

Oracle C7 用示例参数 `(α,β,γ,δ)=(2,1,2,1)` 验证：组内 `median=1`、全正；单变量扫描方向正确
（signal↑→W↑、concentration↑→W↑、noise↑→W↓、background↑→W↓）；这些性质对任意正指数成立。

### 4.2 conventional coadd 与输出

```text
I_out(p) = Σ_k α_k(p) d_k(p) / Σ_k α_k(p),   α_k(p) = W_psfsw,k · v_k(p)
```

其中 `v_k` 是 validity/coverage 门（`support/coverage` 只作门，不作权重）。
输出族 `psfsw_integration`（`DESIGN-P2-001` §9）：四分量、相对权重、conventional coadd、variance/correlation、
effective PSF 与对等权/exposure/pixel-ivar/`W_info` 基线的比较。

### 4.3 最优性边界（严格）

1. `psfsw_robust_weight` **无量纲、组内相对、不得写成 ivar 或 Fisher information**（`SCI-PSFW-001` §3/§4 表；`ASTROCS-SCIENCE-MODEL-001` §3/§4.1/§11；`RULINGS.md` #5）。
2. 它**可以**决定 conventional coadd 中帧的相对贡献，但**不能反向定义输出 pixel variance**；最终 variance/covariance 必须从实际组合系数与输入 covariance 传播（`SCI-PSFW-001` §5；见 `COVARIANCE_AND_EFFECTIVE_PSF.md`）。
3. 只能声明"在验收数据上优于指定基线"，不得自动等同 `1/Var(F_hat)`（`SCI-PSFW-001` §4 表；§8）。
4. 若复合权重含 seeing/concentration penalty，会改变分辨率—噪声折衷；必须输出 effective PSF 并报告相对基线的 detection power、FWHM、通量偏差与面亮度偏差（`SCI-PSFW-001` §5）。
5. `median source SNR` 不得命名为 PSFSW；PSF 拟合 residual、FWHM、背景任一项不得单独冒充完整 PSF signal weight（`SCI-PSFW-001` §8）。
6. fail-closed：无共同星集、背景非正且变换未定义、有效星不足或选择偏差门失败 → `unavailable`，**不得回退成 median source SNR**（`SCI-PSFW-001` §3；Oracle C7.6）。

### 4.4 验证（`SCI-PSFW-001` §7 七项）

注入源验证 `1/sqrt(W_info)`；四类单变量扫描；改变不相关星表深度/检测阈值不改共同星集 PSFSW；
与等权/exposure/pixel-ivar/`W_info` 基线比较；报告 detection power / photometric variance / effective PSF / 扩展源偏差 / 伪影；
零星/少星/拥挤/严重梯度/云/拖线/不同 FOV/不同波段的 fail-closed；权重分量与最终权重的负向 mutation 必须使门变红。

---

## 5. 与 HEAD 生产面的差距（结构证据，非猜测）

基线：`HEAD = 4b508f28`（**工作树含未裁决回退，见 `VERIFICATION_AND_EVIDENCE.md` §1**）。逐项为 HEAD 已提交内容的机器探针结果：

| 观察 | 证据（HEAD） | 判定 |
|---|---|---|
| `point_information`/`surface_gls`/`psfsw_robust` 字面量在生产面 0 命中 | `oracle/probe_production_surface.py`（16/16 PASS，rc=0） | **NOT_IMPLEMENTED** |
| `P2PixelResult` 仅 `signal`/`support`/`n_used`/计数，无 variance/covariance/effective PSF | `git show HEAD:lib/algorithms/coverage/include/astro/phase2/integrate.h` | 偏差（§9 输出族未达成） |
| `p2_integrate_pixel` 只做加权均值 `Σwv/Σw`，无 `AᵀC⁻¹A`、无 Q/W | `git show HEAD:lib/algorithms/coverage/src/integrate.cpp` | 偏差 |
| `weight_mode` 只接受整数 {1=equal, 2=ivar}，显式拒绝 0（legacy SNR） | `git show HEAD:lib/infrastructure/scheduler/src/module_adapters.cpp`（HEAD `module_adapters.cpp:4368-4379`） | 与三模式集不相交 |（已按 §9.73 A44 作废：该概念不存在）
| 跨 Phase 交换矩阵无 `point_information`/`psfsw`/`effective_psf` 字段 | `git show HEAD:eng/contracts/data/phase_product_exchange_matrix.json` | 偏差（`UNIFIED` §9 最小合同未达成） |

结论与 `run/v6/base/gap_baseline.md` §1 一致：三模式在 HEAD **全部不可达**，是 Wave 3→4→5→8 全链新建的科学能力，
不是"接一下开关"。本任务**不**因此判 FAIL——复核任务正确报告基线差距即为完成；差距的闭合责任在后续 wave。

---

## 6. 需控制器 / SCI-ADJ-001 裁决的契约开放项

1. **F1 基线分歧（阻塞性）**：工作树 16 回退 + 10 删除未裁决。W5/W8 触碰 `lib/algorithms/coverage/`、`lib/infrastructure/scheduler/`
   （RUNTIME-CI-001 域）前必须裁定"基线 = HEAD 还是工作树"。
2. **`point_source` 产品的规范输出形态**：`DESIGN-P2-001` §6.2 允许 detection statistic / W map / flux map /
   effective PSF / proper coadd 多种表示。需冻结哪一种是"必须"，以及 map 形态下的 covariance 报告方式
   （本任务 `COVARIANCE_AND_EFFECTIVE_PSF.md` 同时给出标量与 map 两种公式）。
3. **surface_gls 像素 ivar 近似的误差门指标**：需冻结"何时允许退化"的判据（建议用方差比值
   `Var_approx / Var_GLS` 上界，Oracle C4.3 已给出该量可计算）。
4. **PSFSW 指数/截断/归一常数版本**：`SCI-PSFW-001` §3 要求版本化且训练/验收样本不同；本复核只验证结构性质，
   具体数值须由 `ALG-P2-PSFSW-001` 冻结并登记。控制器须确认该任务承担。
5. **effective PSF 归一约定（peak vs integral）**：必须按产品族声明，否则 FWHM/EE 有歧义
   （`COVARIANCE_AND_EFFECTIVE_PSF.md` §3）。
6. **跨帧 covariance 的可表示形式**：完整联合 `C` 不可行，需冻结低秩/相关核表示及其"独立帧求和失效"检测门
   （`UNIFIED` §6/§7；Oracle C3 给出检测量）。
