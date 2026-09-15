# SCI-PSFW-001 — PSF Signal Weight 冻结研究（W_info / PixInsight-style PSFSW / PSF SNR power / 共同星集与 selection bias）

文档 ID：`SCI-PSFW-001-FREEZE-RESEARCH`
任务：`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/SCI-PSFW-001.md`（wave 1，`depends_on = BASE-OWN-001`）
写域：`docs/science/v6/psfw/`、`run/v6/sci-psfw/`（仅此两处；本任务未写任何其他路径）
建议状态：**PASS**（冻结建议本身通过本任务验收；见 §12。生产实现仍为 `NOT_IMPLEMENTED`，按本包规则不因此判 FAIL）
基线：`HEAD = main = origin/main = 4b508f28bbcada66c417a8a9324aca7eb92869ff`（任务卡口径）——执行期间控制器又提交了 4 个并行 wave-1 兄弟任务的 docs-only 交付（`192fab35`/`a09a81f4`/`9d99fd71`/`eac43135`，见 `run/v6/sci-psfw/logs/22_baseline_probe.log` §1；均不在本任务写域）。本任务一切**生产面判定以已提交 HEAD 为准**，并在 §2 显式标注**基线分歧未裁决（F1）**。

上位权威（本文全部只读）：`docs/owner/PROJECT_SPEC.md`、`docs/design/PHASE1_DETAILED_DESIGN.md`、`docs/design/PHASE2_DETAILED_DESIGN.md`、`docs/design/PHASE3_DETAILED_DESIGN.md`、`docs/science/UNIFIED_SCIENCE_MODEL.md`、`docs/science/PSF_SIGNAL_WEIGHT.md`、`docs/references/SCIENTIFIC_REFERENCES.md`、`ASTROCS_PROJECT_CONSTITUTION.md`（FROZEN）。

> 本文只是**冻结建议**（REVIEW 阶段的可审计输入），不是冻结结果。正式冻结由 wave 4 `CONTRACT-FREEZE-001` 写入 `docs/science/v6/frozen/`；本文中的公式、指数与归一常数最终数值交由 wave 3 `ALG-P2-PSFSW-001` 落定并经 `SCI-ADJ-001` 裁决。本任务不改任何上位规范（`docs/science/*.md`、`docs/owner/**`、`docs/design/**`、`docs/references/**` 全部只读）。

---

## 0. Claim → 条款/文献锚 总表（本文每条科学 claim 的唯一索引）

| ID | Claim（一句话） | 条款锚 | 文献/来源锚 |
|---|---|---|---|
| P1 | 点源最优权重是 `W_info,k = a_k² P_kᵀ C_k⁻¹ P_k = 1/Var(F̂)`，Q/W 为充分统计量 | `SCI-PSFW-001` §2；`UNIFIED` §4；`DESIGN-P1-001` §8.1；`DESIGN-P2-001` §6.2；`PROJECT_SPEC` §5 | Horne 1986 PASP 98, 609，DOI:10.1086/131801；Naylor 1998 MNRAS 296, 339（成像最优 PSF 光度）；Zackay & Ofek 2017 I，arXiv:1512.06872 |
| P2 | 白噪声下 `W_info = a_k²/(σ_pix,k² A_NEA)`，`A_NEA = 1/ΣP_p²` | `SCI-PSFW-001` §2；`DESIGN-P1-001` §6/§8.1（`A_NEA` 定义） | 同上 |
| P3 | `W_info` 只在模型与 covariance 门通过时最优；这是**点源**目标的最优，不适用于扩展源/面亮度 | `SCI-PSFW-001` §4 表；`UNIFIED` §5/§11；`DESIGN-P2-001` §6.2 | Horne 1986；Zackay & Ofek 2017 I §2 |
| P4 | 像素 ivar 加权平均在 PSF 跨多像素时**不**等于 `1/W_info`，且方差严格更大（"像素 ivar 对任意 PSF 都最优"为伪） | `UNIFIED` §5/§11；`DESIGN-P2-001` §6.2「普通像素 ivar coadd 在 PSF 不同时不保证最大点源 SNR，不得宣称等价」 | Horne 1986（matched filter）；Zackay & Ofek 2017 I（先分配滤波再组合） |
| P5 | 独立帧 `SNR_comb² = Σ_k SNR_k²`；相关帧必须用联合 `C`，简单求和过度乐观 | `UNIFIED` §4/§10；`DESIGN-P2-001` §10；`PROJECT_SPEC` §8 | Zackay & Ofek 2017 I §2-3；Fruchter & Hook 2002 PASP 114, 144（相关噪声） |
| P6 | PixInsight PSFSW 是 **3 个输入量（signal、concentration、robust noise×robust background）** 的混合 PSF/孔径质量估计器，归一常数按"组内 median=1"标定；本项目为可审计性把它**分解为四分量分别落产品** | `SCI-PSFW-001` §3（四分量分别写入 + 组内归一）；`DESIGN-P2-001` §6.3/§9 | [PixInsight, *New Image Weighting Algorithms*](https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html) §2.5 Eq.[16]–[17]（"sum of PSF flux estimates…total signal"、"sum of mean PSF flux estimates…signal concentration"、分母 = noise × robust mean background、1000 幅合成图把常数调到单位中位数）；Eq.[11]–[13] robust mean background（MMT 残差）；Eq.[14]–[15] robust noise（MAD / Sn）；Starck & Murtagh 1998 PASP 110, 193（MRS）；Rousseeuw & Croux 1993 JASA 88, 1273，DOI:10.2307/2291267（Sn）；Starck, Murtagh & Fadili 2010，DOI:10.1017/CBO9780511730344（MMT） |
| P7 | 四分量定义（本项目写死为可审计量纲）：`S_k` = 共同星集 PSF 总 flux（通量单位）；`Conc_k` = 共同星集 PSF 平均 flux 或等价集中度（信号/像素单位，**不是** FWHM 或残差）；`N_k` = 稳健噪声（与 signal 同单位）；`B_k` = 稳健背景（同单位，必须 > 0） | `SCI-PSFW-001` §3/§6；`DESIGN-P2-001` §6.3 | 同上 |
| P8 | 复合形式 `Wt_k = C_norm · S_k^α Conc_k^β / (N_k^γ B_k^δ)`，指数与截断版本化；`W_psfsw,k = Wt_k / median_j(Wt_j)` 为**无量纲组内相对**量 | `SCI-PSFW-001` §3「指数、截断、稳健估计器和归一常数版本化」；`PHASE2_DETAILED_DESIGN` §6.3 | 与 `SCI-P2-001_THREE_MODE_REVIEW.md` §4.1 结构一致；方向性（signal↑/Conc↑→W↑，N↑/B↑→W↓）由本任务 Oracle K6/K8 独立复算 |
| P9 | `psfsw_robust_weight` **只能**声明"在指定验收数据上优于指定基线"，不得声明 Fisher 最优；不得写成 ivar/variance/W_info | `SCI-PSFW-001` §3/§4 表/§5/§8；`UNIFIED` §4.1/§11；`PROJECT_SPEC` §5；`DESIGN-P2-001` §6.3；控制包 `RULINGS.md` #3/#4/#5 | PixInsight 原文亦只把它定位为 hybrid PSF/aperture *quality estimator*，未声明 Fisher 最优（URL 同上 §2.5） |
| P10 | 输出 variance/covariance 必须由**实际组合系数**传播 `C_out = R C_in Rᵀ`；`1/W_psfsw`、`ΣW_psfsw` 等反推一律禁止 | `SCI-PSFW-001` §5；`UNIFIED` §7；`DESIGN-P3-001` §4；`RULINGS.md` #5 | Fruchter & Hook 2002（线性重建 + 相关噪声） |
| P11 | 含 seeing/concentration penalty 时必须输出 **effective PSF**（实际组合算子的脉冲响应），并报告对 `W_info`/普通 ivar 基线的 detection power、FWHM、通量偏差、面亮度偏差 | `SCI-PSFW-001` §5；`DESIGN-P2-001` §6.3/§9 | 与 `SCI-P2-001` `COVARIANCE_AND_EFFECTIVE_PSF.md` §3 定义一致 |
| P12 | 共同星集/selection function 是**强制**门：无共同星集、背景非正且变换未定义、有效星不足、选择偏差门失败 → `unavailable`，**不得回退成 median source SNR** | `SCI-PSFW-001` §3 末条/§8；`DESIGN-P1-001` §8.2；`DESIGN-P2-001` §6.3 | `SCI-PSFW-001` §3 |
| P13 | selection bias 是**可测量**的：median(source SNR) 随星表深度显著移动，而 `W_info` 与 ratio-of-powers 量是图像性质、不随深度移动 | `PSF_SIGNAL_WEIGHT.md` §7.3；`DESIGN-P1-001` §11「改变源亮度分布不改变同一图像的 W_psf，但会改变 median source SNR」；`UNIFIED` §10；`PROJECT_SPEC` §4 末段/§8 | 本任务 Oracle K8（§7.2）给出可复算数字 |
| P14 | 共同星集必须**独立于 PSFSW 帧测量本身**（外部参考星表或参考叠加上的单一门限），否则选择函数与待测量耦合 | `PSF_SIGNAL_WEIGHT.md` §3「使用跨帧匹配的共同恒星集合或显式 selection-function 校正」；§7.3 | 本任务 Oracle K8 反例（§7.2）；选择偏差的一般论证见 `docs/references/SCIENTIFIC_REFERENCES.md` G 节引用纪律 |
| P15 | 四分量输入带空间摘要 `p05/p50/p95` 与有效覆盖；显著空间非均匀时拆 region/tile 或拒绝标量 | `SCI-PSFW-001` §6；`UNIFIED` §8；`DESIGN-P1-001` §8.3 | — |
| P16 | `psf_snr_power` 是**独立第三轨**：ratio-of-powers 只追求集成图像经验 SNR，**不能自动等同 Fisher 最优**；本任务建议冻结其生产资格，但须与 PSFSW 分开验收 | `SCI-PSFW-001` §4 表；`00_READ_FIRST.md`（"`psf_snr_power` 只有在 SCI-PSFW 冻结后才可进入生产"）；`RULINGS.md` #3 | PixInsight Eq.[18]–[19]（ratio of powers；常数按 PSF SNR 与标准 SNR 中位数相等标定）；本任务 Oracle K6（两种加权不同） |
| P17 | `median source SNR`、`support`、`coverage`、`FWHM`、`residual` 一律只能作诊断/门，不得单独或组合冒充 PSFSW 或任何科学权重 | `PSF_SIGNAL_WEIGHT.md` §8；`UNIFIED` §3/§11；`PROJECT_SPEC` §4 末段/§7；宪章 §4.1/§6.3 | 与 `SCI-P2-001` 权重来源门 R3 一致（`docs/science/v6/phase2/WEIGHT_PROVENANCE_GATE.md` §2） |
| P18 | `ivar` 是**另一个**合同对象；`psfsw_robust` 产物任何字段不得出现 ivar/variance/Fisher/W_info 语义 | 宪章 §4.1；`UNIFIED` §3；`RULINGS.md` #5 | — |

---

## 1. 复核范围与方法

### 1.1 复核对象

1. `W_info`（严格点源信息权重）——定义、单位、最优性条件、与 pixel-ivar / ratio-of-powers 的区别；
2. PixInsight-style PSFSW——四分量语义、复合、归一、指数/截断版本化；
3. PSF SNR power ratio——ratio-of-powers 的定位与是否可进生产；
4. 共同星集与 selection bias——门、构造规则与可测量后果。

### 1.2 方法（独立 Oracle，不调用生产实现）

四个**独立**复算面，全部纯 NumPy/标准库，不 import、不调用任何 AstroCS 生产代码（HEAD 生产面对 `psfsw`/`W_info`/`A_NEA` 的命中数均为 0，见 `logs/22_baseline_probe.log` §3，所以"用生产实现自证"在本任务下也不可能）：

| 复算面 | 脚本 | 覆盖 |
|---|---|---|
| 数值 Oracle（解析 + Monte Carlo + CRLB） | `run/v6/sci-psfw/tools/w_info_psfsw_oracle.py` | K1–K9（13 check） |
| 合同合法门（含四分量、共同星集、归一、validity、covariance 边界） | `run/v6/sci-psfw/tools/psfsw_oracle.py` | 正向 76 check + 12 项负向 mutation |
| 生产面结构探针 | `logs/22_baseline_probe.log` | 5 组关键词 0 命中；HEAD/工作树删除态；F1 计数 |
| 基线对齐 | `logs/22_baseline_probe.log` §5 与 `run/v6/base/baseline_freeze.json` 逐字段比对 | wave 0 冻结快照 vs 当前 HEAD |
| 文档 claim/锚点审计（含 10 项负向 mutation） | `run/v6/sci-psfw/tools/doc_claim_audit.py` | 18 claim 行 + 9 anchor 文件 + 14 obligation |
| 越界写审计 | `run/v6/sci-psfw/tools/scope_check.py` | docs 侧 git 枚举 + 被 gitignore 的 `run/` 文件系统枚举 |
| 机器汇总 | `run/v6/sci-psfw/tools/gen_summary.py` → `run/v6/sci-psfw/summary.json` | 从日志/Oracle 结果**读取**，不臆造结果 |

---

## 2. 基线状态（强制声明）

- **基线分歧未裁决（F1）**：工作树**不是** HEAD 的干净副本。`run/v6/base/baseline_freeze.json.worktree_vs_head.counts` = `{reverted_tracked_modified: 16, deleted_tracked: 10, new_content_tracked_modified: 11}`；本任务实测 `tracked-M = 27`、`tracked-D = 10`、`untracked(uall) = 841`、`dirty_total(uall) = 878`（`logs/22_baseline_probe.log` §2；随并行 wave-1 任务写入而增长，故以该日志的当次实测为准）。这些**未裁决**回退态**不得**被当作已验证基线。
- 具体命中本任务相邻域的一例：`lib/phase1/noise/snr_frame_coefficient.{cpp,h}` 在 HEAD 中存在（commit `7346f366`，P33），但在**工作树中被删除**。该文件实现的是 `value = median(SNR_F)` 的**帧级单一系数**，其自身注释即声明"帧级度量"且"不得当作局部 SNR 场"。按 `PROJECT_SPEC` §4 末段与 `UNIFIED` §11，它只能作诊断/深度表达，**不得**成为 Phase2 科学权重；若任何消费者把它当权重使用，即为 P17 违规（本任务只读，不改该文件）。
- HEAD 相对任务卡基线**唯一**前进的是 4 个并行 wave-1 兄弟任务的 docs-only 提交（`logs/22_baseline_probe.log` §1）：`192fab35 SCI-P2-001`、`a09a81f4 AUDIT-REVIEW-001`、`9d99fd71 SCI-OBS-001`、`eac43135 SCI-P3-001`，四者写域分别为 `docs/science/v6/phase2|phase3|observation/`、`reports/v6/review-audit/` 与 `run/v6/*`。本任务写域 `docs/science/v6/psfw/`、`run/v6/sci-psfw/` 与它们**两两不相交**，无并发写冲突；已提交的 4 个提交均为文档，未改生产源码，故 §2 的生产面判定不受影响。
- **生产面现状**（HEAD 已提交内容）：`psfsw|PSFSW|PSF Signal Weight`、`psf_information_weight|point_source_information|point_information|W_psf|W_info`、`psfsw_robust_weight`、`A_NEA|noise_equivalent_area`、`signal concentration|signal_concentration` 在 `lib/ cli/ include/ runtime/ contracts/` 命中**全部为 0**（`logs/22_baseline_probe.log` §3）。判定：**NOT_IMPLEMENTED**，与 `run/v6/base/gap_baseline.md` §2.6 一致。

---
## 3. W_info：定义、单位、最优性与适用边界

### 3.1 定义（P1、P2）

对已归一到公共通量尺度的帧（`d_k = a_k F P_k + n_k`，`Cov(n_k)=C_k`）：

```text
Q_k        = a_k P_kᵀ C_k⁻¹ d_k
W_info,k   = a_k² P_kᵀ C_k⁻¹ P_k          = 1 / Var(F̂_k)
F̂          = Σ_k Q_k / Σ_k W_info,k        Var(F̂) = 1 / Σ_k W_info,k
```

白噪声近似（`C_k = σ_pix,k² I`）：

```text
W_info,k = a_k² / (σ_pix,k² · A_NEA,k),   A_NEA,k = 1 / Σ_p P_k,p²
```

**单位**：`[a]²·[P]·[C]⁻¹·[P]`。若 `a` 无量纲（把帧归一到公共通量尺度后 `a` 是相对响应）而 `P` 归一 `ΣP=1`、`d` 为通量单位，则 `W_info` 的单位是 `1/flux²`，即**信息单位**；它与"帧权重"不是同一量纲对象。这是 W_info 与 `psfsw_robust_weight`（无量纲相对权重）不能混名的量纲根据（P9、P18；`SCI-PSFW-001` §2 明写"1/flux² 信息单位"）。

### 3.2 独立验证（K1–K4、K7）

| Check | 实测 | 门 | 结论 |
|---|---|---|---|
| K1 `PᵀC⁻¹P` 白噪声恒等 | 相对差 `0` | `<1e-12` | 两种写法严格等价 |
| K2 Monte Carlo（`N=400 000`，Moffat β=4、FWHM=3 px） | `Var(F̂)_MC = 275.421` vs `1/W_info = 275.064`，相对差 `0.13%` | `<2%` | 解析方差 = 估计量实测散度 |
| K3 Cramér–Rao 下界 | 相对差 `4.1e-16` | `<1e-12` | 达到 CRLB（该线性高斯模型下有效） |
| K7 光度尺度 | `W_info(2a)/W_info(a) = 4.000`，与 `(a'/a)²` 的偏差 `0` | `dev<1e-12` 且比值 ≠1 | `W_info` 随**声明的**光度尺度 `a²` 确定变化 |
| K4 像素 ivar | 无权重孔径加权方差 / matched-filter 方差 = **33.08** | `>1.5` | 像素 ivar 平均远非最优 |

K4 的取值随 `A_NEA` 增长：`A_NEA = 29.05 px²` 时劣势 33×；FWHM→1 px 且 PSF 与像素匹配时该比值→1。所以 P4 的正确表述是**有条件**的："像素 ivar 对任意 PSF 点源目标都最优"为伪，其退化只在 PSF 不跨像素/算子退化到同点采样时成立（`UNIFIED` §5 的措辞）。

### 3.3 最优性边界与失效条件（P3）

`W_info` 的"最小方差/最大点源 SNR"声明成立需全部满足：

1. 模型 `d_k = a_k F P_k + n_k` 正确（单点源、位置与形状已知、`P` 已归一 `ΣP=1`）；
2. `C_k` 正确且可表示（含相关项；只给对角 `C̃` 时必须报告用真实 `C` 复算的 `c̃ᵀ C c̃` 及与 `1/W` 的偏差）；
3. 高斯噪声（或线性估计量的 CRLB 意义下）；
4. 目标为**点源**检测/测光——扩展源/面亮度必须走 `surface_gls`（`UNIFIED` §5）；
5. `a_k`、`P_k` 的空间/系统不确定度按 §6 分层（共享系统项进低秩 covariance，模型偏差进 validity，不得伪装随机 ivar）。

任一条不成立时**不得**声明最优（`PROJECT_SPEC` §3；`SCI-PSFW-001` §2）。

---

## 4. PixInsight-style PSFSW：四分量、复合、归一

### 4.1 外部原方法的结构（P6，逐条来自公开文档）

PixInsight 文档 §2.5 Eq.[16]–[17] 的语义（该文档公式以 SVG 图片排版，以下为对**正文文字**的逐条转写，未臆造公式）：

- 分子两个和项：① PSF 总 flux 估计之和 = **total signal**；② **mean** PSF flux 估计之和 = **signal concentration**（对星像平均尺寸的依赖：和越大分辨率越高）；
- 分母：**noise estimate**（默认 MRS 多分辨率支撑，或 N* 估计器）乘 **robust mean background**（MMT 残差上的稳健均值，用于计入光污染梯度/薄云/透明度/月光）；
- 归一常数：用 1000 幅受控合成线性图（背景均值 0.015、σ=0.001、1500 星、Moffat β=4 FWHM=5 px）把典型值调到 0.01–100，并把该集合的 **median PSFSW 调成 1**；
- §2.6 Eq.[18]–[19] 的 **PSF SNR** 是 ratio-of-powers 实现，常数按"PSF SNR 与标准 SNR 中位数相等"标定，官方定位是"只追求集成图像 SNR"时使用。

**结构差异登记（重要）**：PixInsight 的式子是 **signal × concentration ÷ (noise × background)** 的**三输入**复合；本项目的目标是**四分量分别落产品**（`SCI-PSFW-001` §3、`DESIGN-P2-001` §6.3/§9）。因此：

- 这不是与 PixInsight 的冲突，而是把它的"noise 与 background 相乘"在**产物层**拆成两个独立可审计分量（用户要在合并前独立检查两者）；
- 本项目**不要求**逐字复制其常数（`SCI-PSFW-001` §3 末段）；若将来要精确兼容，必须另名 `pixinsight_psfsw_compat` 并记录所兼容版本。

### 4.2 本项目四分量定义（P7，建议冻结）

在**同一波段**、**同一目标/重叠连通分量**、**光度已归一**的帧组 `G` 内，对共同星集 `S`（§5）逐帧计算：

| 分量 | 建议规范名 | 量纲 | 定义 | 备注 |
|---|---|---|---|---|
| `signal` | `psfsw.signal` | 与帧 signal/flux 同单位 | `S_k = Σ_{s∈S_k} f̂_{k,s}`（PSF 拟合总 flux 之和；`f̂` 为 PSF 测光通量） | 对应 PixInsight "sum of PSF flux estimates"；**不是**帧总信号、不是孔径和 |
| `concentration` | `psfsw.concentration` | signal/像素 | `Conc_k = mean_{s∈S_k} f̂_{k,s} / A_NEA,k`（或文档等价"mean PSF flux"的固定约定） | 对应 "sum of mean PSF flux estimates"；**禁止**用 FWHM 或拟合残差代替（P17） |
| `noise` | `psfsw.noise` | 与 signal 同单位 | `N_k` = 稳健噪声（MAD/Sn 类，须声明估计器与版本） | 对应 Eq.[14]/[15]；**不是**像素 σ、不是 `m_5` |
| `background` | `psfsw.background` | 与 signal 同单位 | `B_k` = 稳健均值背景（须 >0，<0 且变换未定义 → unavailable） | 对应 Eq.[11]–[13]；**不是** support/coverage |

`SCI-PSFW-001` §6 强制：四个输入分量各自带空间摘要 `p05/p50/p95` 与有效覆盖；显著空间非均匀时拆 region/tile 权重或直接拒绝标量模式。本条在机器门中由 `psfsw_oracle.py` 的 `C6–C9` 检查强制（P15）。

### 4.3 复合与归一（P8，建议冻结）

```text
Wt_k        = C_norm(version) · S_k^α · Conc_k^β / (N_k^γ · B_k^δ)      α,β,γ,δ ≥ 0，版本化
W_psfsw,k   = Wt_k / median_{j∈G}(Wt_j)                                 组内 median = 1
α̃_k(p)      = W_psfsw,k · v_k(p) / Σ_{j∈G} W_psfsw,j · v_j(p)          v = validity 门（0/1）
I_out(p)    = Σ_k α̃_k(p) d_k(p)/Σ_k α̃_k(p)                             conventional coadd
```

**归一三条**（建议冻结为不变式）：

1. `C_norm`、指数 `(α,β,γ,δ)`、截断/稳健估计器、归一常数**版本字符串必须落产品**（`SCI-PSFW-001` §3）；
2. 归一是**组内**相对（band × 连通分量 × 光度已归一帧组），跨组比较必须先声明 selection function（`SCI-PSFW-001` §3/§8）；
3. 标定该版本的样本**不得**与最终验收样本相同（`SCI-PSFW-001` §3）。

方向性由 Oracle K8/K6 独立复算（`Wt` 对 `S`、`Conc` 单调增，对 `N`、`B` 单调减，对任意正指数成立）；与 `SCI-P2-001_THREE_MODE_REVIEW.md` §4.1 的示例 `(α,β,γ,δ)=(2,1,2,1)` 结构一致。

### 4.4 最优性边界（P9，严格）

`psfsw_robust_weight`：

- 可以：决定 conventional coadd 中帧的**相对贡献**；在预注册数据上声明"优于指定基线"；
- 不可以：写成 `ivar`/`variance`/`1/W_info`/Fisher information；自动等同 `1/Var(F̂)`；用经验成功替代 Q/W 与 covariance 的科学产品（`SCI-PSFW-001` §8 末条）；
- 若复合权重含 seeing/concentration penalty，它改变分辨率—噪声折衷，必须输出 effective PSF 并报告相对 `W_info`/普通 ivar 基线的 detection power、FWHM、通量偏差、面亮度偏差（P11）。
- **单位/归一最优性边界与 W_info 的区分**（任务正文要求）：`W_info` 是**有量纲信息量**（1/flux²）且其"最优"是有前提的统计声明；`psfsw_robust_weight` 是**无量纲组内相对质量**且只能作经验相对比较。二者唯一的合法耦合是通过 covariance 传播与基线比较，**没有**任何公式把前者写成后者的函数。

---
## 5. 共同星集与 selection bias

### 5.1 强制门（P12）

`SCI-PSFW-001` §3 已把下列条件写成硬约束，本任务建议原样冻结并补上机器判据：

| 条件 | 处置 | 机器判据（本任务门） |
|---|---|---|
| 无共同星集 | `unavailable` | `T2-common_star_set_id` / `T2-selection_function_id` 缺失即 REJECT |
| 背景非正且变换未定义 | `unavailable` | `C5[background]-nonnegative` + validity reason 白名单 |
| 有效星不足 | `unavailable` | `T2-n_common` 缺失；建议阈值 `n_common ≥ 3`（与 `SCI-P2-001` R5 对齐） |
| 选择偏差门失败 | `unavailable` | `T3-selection_correction` ∈ {common_star_set, explicit_selection_function, both} |
| 任何 unavailable | **不得**回退成 median source SNR | `V4-unavailable-no-weight`（`valid=false` 时 `weight_value` 必须为 `null`） |

排除项必须显式列举：`saturated`、`blended`、`trailed`、`moving`、`psf_mismatch`、`edge_truncated`（`SCI-PSFW-001` §3）。

### 5.2 构造规则（P14，建议冻结）

共同星集 `S` 的成员判定必须**独立于待测 PSFSW 帧测量本身**，否则选择函数与目标量耦合，跨帧比较不可解释。建议两条允许路径：

1. **外部参考星表**（Gaia/离线星表，版本化哈希）在帧组天区内的固定子集；
2. 在**参考叠加**（而非逐帧检测）上取单一门限的固定星表。

**禁止**：用逐帧检测阈值分别取星后交集成"共同星集"——这会引入随帧 SNR/seeing 变化的样本，正是本任务 Oracle K8 量化的偏差来源。

### 5.3 selection bias 的可测量后果（P13，独立反例）

Oracle K8 用同一合成星场（6000 颗，`10²..10⁵` 通量，CCD 方程噪声）只改**检测阈值**：

| 检测阈值 (σ) | 选中数 | median(source SNR) | 样本 PSFSW 代理 | ratio-of-powers SNR² | 选中真值信息和 | W_info(真值) |
|---|---|---|---|---|---|---|
| 1.5 | 6000 | 50.81 | 63.29 | 6.90019e7 | 4791 | 0.79845 |
| 4.0 | 6000 | 50.81 | 63.29 | 6.90019e7 | 4791 | 0.79845 |
| 10.0 | 5813 | 53.56 | 61.15 | 6.89852e7 | 4643 | 0.79845 |
| 20.0 | 4620 | 75.43 | 52.06 | 6.87256e7 | 3694 | 0.79845 |
| 40.0 | 3394 | **106.14** | 54.97 | 6.76647e7 | 2715 | 0.79845 |

- `median(source SNR)` 在阈值 1.5→40 σ 之间移动 **108.9%**（实测 `K8.median_snr_bias`）；
- 用同一选中样本构造的 PSFSW 风格代理移动 **13.2%**（`K8.psfsw_selection_bias`）——**方向与幅度都不受控**，证明"样本派生的 PSFSW"对深度敏感；
- 真值 `W_info` 按构造**逐位不变**（`K8.W_info_sample_free`，位移 `0`）——它与检测样本无关；
- ratio-of-powers SNR² 只移动 **1.94%**（`K8.ratio_powers_depth_stable`），说明它和 `W_info` 一样是图像性质量，这也解释了为何 PixInsight 推荐它作纯 SNR 权重。

**结论**：三者对星表深度的响应本质不同——`W_info`/ratio-of-powers 是图像性质，median(source SNR) 与样本派生 PSFSW 是**样本统计量**。因此 `SCI-PSFW-001` §7.3「改变不相关星表深度/检测阈值不得显著改变共同星集 PSFSW」不是形式要求，而是**可量化门**：本任务建议把该门的判据写成"共同星集 PSFSW 在深度扫描下的相对变化 < 5% 且不随深度单调漂移"，并在 wave 3 由 `ALG-P2-PSFSW-001` 固定具体阈值（本任务不擅自改冻结门）。

---

## 6. PSF SNR power ratio（P16）

- 定位：ratio-of-powers 实现（PixInsight Eq.[18]–[19]）是"只追求集成图像经验 SNR"的**第三轨**，官方未声明 Fisher 最优；
- 本项目 `SCI-PSFW-001` §4 表：`psf_snr_power` **不能**自动等同 Fisher 最优；
- `00_READ_FIRST.md` 规定它"只有在 SCI-PSFW 冻结后才可进入生产"——本任务即为是否冻结该资格的输入；
- 独立证据（K6）：在同一 PSF 核 64 个最亮像素上，matched-filter 权重 `f/σ²` 与 ratio-of-powers 权重 `f²/σ²` 的方向余弦 = **0.932**（< 0.999），两种加权**不是**同一算子；
- 冻结建议：**建议有条件允许** `psf_snr_power` 进 Phase2 生产，作为**独立于 `psfsw_robust` 的第三模式**，但必须 (a) 单独登记为 `weight_mode=psf_snr_power`；(b) 其归一常数与截断版本化；(c) 验收只允许"在指定数据上优于指定基线"的声明；(d) 不得借用 PSFSW 的四分量或选择门来**继承**合法性。若 `SCI-ADJ-001`/`CONTRACT-FREEZE-001` 认为证据不足，则保持 `psf_snr_power` 为 `NOT_IMPLEMENTED` 并显式 `unavailable`（**这是本任务唯一要求控制器裁决的冻结项**，见 §11）。

---

## 7. 独立 Oracle 与负向门（可复跑）

### 7.1 命令与实测 rc

| 命令 | 实测 rc | 日志 |
|---|---|---|
| `python3 tools/psfsw_oracle.py --selftest --verbose` | **0**（正向 76 check ACCEPT；12/12 mutation CAUGHT） | `run/v6/sci-psfw/logs/20_oracle_selftest.log` |
| `python3 tools/w_info_psfsw_oracle.py` | **0**（13/13 claim PASS） | `run/v6/sci-psfw/logs/21_oracle_w_info_psfsw.log` |
| `bash tools/run_all.sh`（一键复跑，单一 rc） | **0** | `run/v6/sci-psfw/logs/22_run_all.log` |
| 生产面探针（只读 grep/git） | **0** | `run/v6/sci-psfw/logs/22_baseline_probe.log` |
| 越界写审计（docs + 被 gitignore 的 run/） | **0** | `run/v6/sci-psfw/logs/23_scope_check.log` |
| 文档 claim/锚点审计（含 10 项负向 mutation） | **0** | `run/v6/sci-psfw/logs/24_doc_claim_audit.log` |
| 负向门演示（干净 0 / 注入 ivar 1 / 篡改 Oracle 1） | `0,1,1` | `run/v6/sci-psfw/logs/25_negative_gate.log` |
| 机器可读汇总 | — | `run/v6/sci-psfw/summary.json` |

### 7.2 数值 Oracle 结果（K1–K9，全 PASS）

见 §3.2、§4.3、§5.3 与 `logs/21_oracle_w_info_psfsw.log`。补充：K5 独立帧 `SNR_comb² = Σ SNR_k²` 相对差 `0`；K5b 相关帧（ρ=0.45）下朴素求和的方差 / 联合 `C` 的方差 = `0.697`（乐观低估 30%）；K9 实际组合系数传播 `cᵀ C_in c = 12.7` vs 非法 `1/Σw = 0.25`，相对差 `98.0%`——三者共同把 P5/P10 从条款变成可执行判据。

### 7.3 负向 mutation（12 项，全部 CAUGHT → rc≠0）

| Mutation | 注入内容 | 命中检查 |
|---|---|---|
| M01 | 权重单位写成 `flux^-2` | `Z2-no-inverse-flux-units` |
| M02 | `weight_kind` 写成 `ivar` | `S3-weight_kind` |
| M03 | `concentration` 直接复制 `signal`（四分量塌陷为三） | `C11-four-distinct-components` |
| M04 | 产物中注入 `ivar` 字段 | `Z1-forbidden-keys` |
| M05 | covariance 改成"variance = 1/相对权重" | `X2`/`X3` |
| M06 | 归一域改成 `global`（跨区比较） | `N3-scope` |
| M07 | 归一标定被去掉（数值放大 100 倍） | `N4`/`W4` |
| M08 | 删除共同星集与 selection function | `T2-*` |
| M09 | invalid 时回退写入 median SNR 值 | `V4-unavailable-no-weight` |
| M10 | `valid=true` 同时带失败 reason | `V5-valid-has-no-reason` |
| M11 | 归一常数版本串为空 | `N2-constants_version` |
| M12 | 删除 effective PSF | `X5-effective-psf` |

每一项的"注入即报红"是**先证伪门、再声明门有效**（不是用同一实现自证）：`M03` 首次运行确实 MISSED，随后才补上 `C10/C11` 并复测 CAUGHT——这个过程留在 `logs/20_oracle_selftest.log` 的历史里，正是"负向门能红"的直接证据。

---

## 8. 与相邻任务的一致性（P17 / 接口）

| 相邻产物 | 一致性检查 | 结论 |
|---|---|---|
| `docs/science/v6/phase2/WEIGHT_PROVENANCE_GATE.md`（SCI-P2-001，HEAD `192fab35`） | 其 R3 把 `median_source_snr/support/coverage/fwhm/residual` 列为禁止的权重来源；其 R5 要求 `psfsw_robust units=dimensionless_relative` + `group_normalized=true` + 共同星集 `n_common≥3`；其 R6 要求 `variance_from ∈ {combination_coefficients,…}` | **一致**：本任务 P9/P10/P12/P17 与其同源（`SCI-PSFW-001` §3/§4/§5/§8、`RULINGS.md` #3/#4/#5） |
| 同上，token 词表 | 该门用 `weight.kind` / `weight.units="dimensionless_relative"` / `group_normalized`；本门用 `weight_kind="relative_dimensionless"` / `weight_units="1"` / `normalization.scope="group"` | **接口差异（须 SCHEMA-INTEGRATE-001 归一）**，见 §11-R2 |
| `docs/design/PHASE2_DETAILED_DESIGN.md` §6.3（只读上位） | 要求 `weight_mode=psfsw_robust` 与 `point_information`/`surface_gls` 并列、covariance 从实际组合系数传播、输出 effective PSF、与等权/exposure/pixel-ivar/`W_info` 比较 | 本任务 §4/§5 完全覆盖，无偏差 |
| `SCI-P2-001` open item 4（PSFSW 指数/归一常数版本由 wave 3 冻结） | 本任务 §4.3 只给**结构**，未定数值 | **一致**：本任务不越界定数值 |

---
## 9. 机器可检查约束（"不可写 ivar"）

建议 `CONTRACT-FREEZE-001` 采用如下可直接执行的判据（本任务已给出参考实现 `run/v6/sci-psfw/tools/psfsw_oracle.py`，正向 76 check、负向 12 mutation）：

1. **对象身份**：`weight_kind = "relative_dimensionless"`，`weight_units = "1"`（严格字符串）；任何含 `flux^-2`、`**-2`、`1/flux` 的单位串直接 REJECT；
2. **数值语义**：`weight_value` 必须 finite 且 ≥ 0；`median(source SNR)` 类值不得出现在 `weight_value`（由 `validity`/`star_selection` 分区隔离，禁止同字段复用）；
3. **禁止键**：产物片段内**任何层**不得出现 `ivar`、`variance`、`var`、`sigma`、`sigma2`、`inverse_variance`、`fisher`、`information`、`w_info`、`w_psf`、`snr`、`snr2`、`support`、`coverage`（键名集合命中即 REJECT）；
4. **covariance 来源**：`covariance_model.method = "propagated_from_composite_coefficients"`、`variance_from_weight = false`、`uses_relative_weight_as_ivar = false`、`effective_psf_id` 非空；
5. **归一/星集**：`normalization.scope = "group"`、`median_target = 1.0`、常量版本非空；`star_selection` 含共同星集 id、selection function id、成员哈希、排除旗标、`n_common`；
6. **fail-closed**：`validity.valid=false` 时 `weight_value` 必须为 `null` 且 reason ∈ 白名单（`no_common_star_set`、`background_nonpositive_undefined_transform`、`insufficient_valid_stars`、`selection_bias_gate_failed`、`spatial_nonuniformity_gate_failed`）；
7. **四分量独立**：四个 `measurement_id` 必须互不相同（防 M03 式塌陷）；
8. **空间摘要**：每分量 `p05 ≤ p50 ≤ p95` 且 `valid_area_fraction ∈ [0,1]`。

第 3 条是任务正文"不可写 ivar 的机器可检查约束"的**核心答案**：不靠命名约定，而靠"键集合白名单 + 单位字符串 + covariance 来源字段"三重结构与负向 mutation 证明。

---

## 10. 结论与冻结建议

**建议状态：PASS**（本任务交付通过；下列冻结建议提交 `SCI-ADJ-001` → `CONTRACT-FREEZE-001`）

1. **W_info**：维持 `docs/science/PSF_SIGNAL_WEIGHT.md` §2 定义，不改公式、不改单位；把"最优性前提"5 条（§3.3）写成 fail-closed 门；
2. **psfsw_robust 四分量**：采用 §4.2 的规范名与量纲，四分量分别落产品（`signal/concentration/noise/background` + `measurement_id` + `p05/p50/p95` + 有效覆盖）；
3. **复合与归一**：采用 §4.3 的复合式与"组内 median=1"归一；指数、截断、稳健估计器、常数版本必须落产品；标定样本 ≠ 验收样本；
4. **validity**：采用 §5.1 的 fail-closed 门，`unavailable` 时**禁止**回退成 median source SNR；
5. **与 W_info 的边界**：量纲（1/flux² vs 无量纲）、最优性声明（有条件统计最优 vs 经验相对）、输出 covariance（`C_out = R C_in Rᵀ`）三点必须分别写入合同；`psfsw_robust` 不得写 ivar；
6. **共同星集**：必须独立于帧测量（外部参考星表或参考叠加单一门限），并附 selection function；
7. **`psf_snr_power`**：建议**有条件**解冻进生产（§6 四条件），但需控制器/`SCI-ADJ-001` 裁决（§11-R1）；
8. **机器门**：采用 §9 的 8 条判据，参考实现与 12 项 mutation 已可复跑（rc 见 §7.1）。

---

## 11. 未决风险与需控制器裁决事项

| ID | 事项 | 风险 | 建议裁决 |
|---|---|---|---|
| R1 | `psf_snr_power` 是否解冻进 Phase2 生产 | 若不裁决，wave 3 `ALG-P2-*` 无法确定模式集合；若误当 Fisher 最优会污染科学声明 | 建议**有条件解冻**（§6）；若控制器要求更强证据，保持 `NOT_IMPLEMENTED` + `unavailable` |
| R2 | 与 `SCI-P2-001` 权重来源门的 schema 词表不一致（`weight_kind/weight_units/normalization.scope` vs `weight.kind/weight.units/group_normalized`） | 两套门各自 PASS 但互相不认，集成时漏检 | 由 `SCHEMA-INTEGRATE-001`（W6）统一为**单一** schema；本任务给出双向 token 映射（§8） |
| R3 | F1 基线分歧未裁决（16 回退 + 10 删除） | 本任务生产面判定以 HEAD 为准；若最终基线取工作树，`lib/phase1/noise/snr_frame_coefficient.*` 等删除态会改变"现状" | 控制器先裁决 F1；科学结论（§3–§6）与回退无关，仍然成立 |
| R4 | 帧级 `median(SNR_F)` 系数的现状定位 | HEAD 存在 `value = median(SNR_F)` 的帧级系数；若被任何消费者当权重即违反 P17 | 建议在 `CONTRACT-FREEZE-001` 显式把它登记为**诊断/深度表达**，禁止进入 `weight_mode` 权重面 |
| R5 | PixInsight 式子的"三输入 vs 本项目四分量"结构差异 | 若下游误以为本项目公式必须与 PixInsight 逐字一致，会误删独立分量 | 已在 §4.1 登记；建议 `CONTRACT-FREEZE-001` 原样保留该登记与 `pixinsight_psfsw_compat` 另名规则 |
| R6 | 共同星集深度稳定性阈值（< 5%）与 `n_common` 下限（≥ 3） | 本任务给出的数值是建议，不是冻结门；若被直接当冻结门即属越界 | 交 `ALG-P2-PSFSW-001`（W3）定值，本任务不擅改容差 |
| R7 | 四分量空间非均匀性的"拆 tile"判据 | `SCI-PSFW-001` §6 要求"显著非均匀时拆"，但未给阈值 | 交 `ALG-P2-PSFSW-001`（W3）定值 |
| R8 | 无法在本机读取 PixInsight 公式的 SVG 图像本身 | 本任务只转写文档**正文文字**语义，未复制其符号式；若需逐字比对须有人工读图 | 登记为证据局限；不影响本项目自行推导与冻结（`SCI-PSFW-001` §3 不要求逐字复制） |

---

## 12. 验收对照

| 任务卡验收 | 本任务证据 |
|---|---|
| 任务正文逐项完成且不越界 | §3 W_info、§4 PSFSW、§6 PSF SNR power、§5 共同星集与 selection bias；写域仅 `docs/science/v6/psfw/` + `run/v6/sci-psfw/`（`logs/30_scope_check.log`） |
| 科学/算法/接口/代码/测试在本任务范围内一致 | §8 与 `SCI-P2-001` 门逐条比对；`SCI-PSFW-001`/`UNIFIED`/三 Phase 设计条款逐条锚 |
| 独立 Oracle 或结构证据充分，负向门能红 | §7：两个独立 Oracle（13/13、76+12）、生产面结构探针、rc 全记录；M03 曾 MISSED→修复后 CAUGHT |
| 建议状态只能 PASS/FAIL/REVIEW_REQUIRED | §10 建议 **PASS** |

## 13. 参考

1. Horne, K. 1986, PASP 98, 609, DOI:[10.1086/131801](https://doi.org/10.1086/131801) — 已知 profile 与方差下的最优提取（`PᵀC⁻¹P` 统计结构）。
2. Naylor, T. 1998, MNRAS 296, 339 — 成像最优 PSF 光度。
3. Zackay, B. & Ofek, E. O. 2017, ApJ 836, 187, [arXiv:1512.06872](https://arxiv.org/abs/1512.06872) — 逐帧 matched filter 后组合。
4. Zackay, B. & Ofek, E. O. 2017, ApJ 836, 188, [arXiv:1512.06879](https://arxiv.org/abs/1512.06879) — proper coadd 与信息保持表示。
5. PixInsight, *New Image Weighting Algorithms*, [https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html](https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html) — §2.5 Eq.[16]–[17] PSF Signal Weight；§2.6 Eq.[18]–[19] PSF SNR ratio-of-powers；§2.3 Eq.[11]–[13] robust mean background（MMT）；§2.4 Eq.[14]–[15] robust noise（MAD/Sn）。访问核对 2026-09-15；该文档公式排版为 SVG 图像，本文只转写其正文文字语义。
6. Starck, J.-L. & Murtagh, F. 1998, PASP 110, 193 — MRS 多分辨率支撑噪声估计。
7. Rousseeuw, P. J. & Croux, C. 1993, JASA 88, 1273, DOI:[10.2307/2291267](https://doi.org/10.2307/2291267) — Sn 稳健尺度估计。
8. Starck, J.-L., Murtagh, F. & Fadili, A. 2010, *Sparse Image and Signal Processing*, DOI:[10.1017/CBO9780511730344](https://doi.org/10.1017/CBO9780511730344) — 多尺度中值变换（MMT）。
9. Fruchter, A. S. & Hook, R. N. 2002, PASP 114, 144 — Drizzle 线性重建与相关噪声。
10. 项目内：`docs/science/PSF_SIGNAL_WEIGHT.md`、`docs/science/UNIFIED_SCIENCE_MODEL.md`、`docs/owner/PROJECT_SPEC.md`、`docs/design/PHASE{1,2,3}_DETAILED_DESIGN.md`、`docs/references/SCIENTIFIC_REFERENCES.md`、`ASTROCS_PROJECT_CONSTITUTION.md`、控制包 `00_READ_FIRST.md`/`RULINGS.md`。

## 14. 声明

- 未 commit / push / git add；未建分支或 worktree；未 stash/reset/clean/rebase；git 仅只读使用。
- 未越界写：仅写 `docs/science/v6/psfw/` 与 `run/v6/sci-psfw/`。
- `run/*` 被 `.gitignore:19` 忽略（AGENTS.md 规定 `run/*` 全部 gitignore），故本任务机器可读摘要、Oracle、日志**不进入 git 历史**，仅留在工作区供复现；越界检查因此对 `run/` 目录改用文件系统枚举（`logs/23_scope_check.log`：16 个 run 产物全部在域内，0 越界）。
- 未派生子代理（未调用 subagent/subagent_fork/ralph）。
- 未宣布任何发布；未修改科学公式、容差或冻结门。
