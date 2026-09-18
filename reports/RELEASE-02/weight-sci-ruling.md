# RELEASE-02 帧级 SNR 权重链科学裁决（WEIGHT-SCI）

> 裁决者：帧级 SNR 权重链科学裁决 SubAgent（WEIGHT-SCI）。工作目录：/workspace/Astro CS Database。
> 时点：2026-09-18。**零 git 写；未改任何代码/文档；未跑 ninja/cmake/ctest。**
> 只读面：ASTROCS_DESIGN.md、docs/science/**、docs/design/**、docs/plugins/**、
> docs/algorithms/**、lib/algorithms/{noise_snr,integration,coverage}/**、
> lib/infrastructure/{scheduler,aio,drizzle}/**、工程控制/RELEASE-02/**、
> reports/RELEASE-02/**、run/RELEASE-02/L4-rebuild/**（实测产物）。
> 产物：reports/RELEASE-02/weight-sci-ruling.md（本文件）；
> 复算：run/RELEASE-02/weight-sci/analyze.py、run/RELEASE-02/weight-sci/convention_compare.json。

---

## 0 结论摘要（先行）

| 问题 | 裁决 |
|---|---|
| **w_f = SNR_f²/F_ref² 里的 F_ref 用逐帧还是组内公共？** | **组内公共（common）**。且存入 HiPS 头的 ASTROCS_FRAME_SNR 必须是**同一个公共参考通量下的 SNR**——二者是**配对量**，分母必须等于定义该 SNR 时所用的参考通量。 |
| 根因 | Phase1 写的是**逐帧自己的 F_ref**（本帧检出源通量中位数，snr_frame_science.cpp:167-170），并在该逐帧通量下定义 SNR；Phase2 闸门/权重链要求**组内公共**。**约定不一致**，必然 fail-closed。 |
| UPM 是否在权重链前归一到公共通量标度？ | **顺序满足，内容不满足**。节点链 coverage→sample→upm_fit→upm_apply→reject→integrate→write（module_adapters.cpp:7581-7589）确实让 UPM 先于权重链；但**现行冻结 UPM 是纯加性**（calibrated = raw − C_f，upm.cpp:1300；SCI-UPM §1/§6/§10「乘性尺度差已撤销」），**不做乘性归一**。「公共通量标度」这一前置条件目前只由**冻结假设**「帧间无乘性尺度差」兜底，不是由 UPM 的动作产生。 |
| 闸门（:5379-5380）与单 F_ref 权重链 API 是否要改？ | **不改（判为正确）**。它们是设计侧的正确实现；要改的是**写侧（Phase1）**。 |
| 是否需改文档？ | 需要（命名消歧 + 前置条件澄清），走变更 claim；**不改科学公式**。见 §7.2。 |

**一句话**：F_ref 是**帧组的公共参考通量**；现行实现把它做成了「帧自己的检出通量中位数」，是写侧缺陷。修 Phase1，不修权重链。

---

## 1 问题、实测事实与复算

### 1.1 前台实测事实（本轮独立复算确认）

L4 49 帧真实产物 run/RELEASE-02/L4-rebuild/norm/**/signal/properties 逐帧读回：

- ASTROCS_FRAME_SNR 与 ASTROCS_REFERENCE_FLUX 两键**确实存在且成对**（49/49）；
- 全局 ASTROCS_REFERENCE_FLUX ∈ [1728.85, 11882.99]（比值 **6.87×**）；
- **组内**（每个 <t?_m?_red> 目录 = 一个配置）F_ref 离散度：最小 2.36%，最大 **114.05%**（t3_m4_red，含一个 1728.85 的离群帧），见 convention_compare.json。

复算脚本：run/RELEASE-02/weight-sci/analyze.py（纯 stdlib，可复跑）。

### 1.2 Phase2 的 fail-closed 行为（读侧）

lib/infrastructure/scheduler/src/module_adapters.cpp:5377-5381：

    if (has_ref) {
      if (!ref_flux_set) { ref_flux = fref; ref_flux_set = true; }
      else if (std::fabs(fref - ref_flux) > 1e-9 * std::fabs(ref_flux))
        ref_err = "ASTROCS_REFERENCE_FLUX 逐帧不一致（组内公共通量标度要求）";
    }

组内 F_ref 只要相对差 > 1e-9 即报 unclosed_invalid_reference_flux。按实测（组内 2.4%~114%），**任何 n>1 的组都必然失败**——闸门行为本身是正确的（fail-closed），失败的是写侧没写出公共值。

---

## 2 核心推导：分母必须是定义 SNR 所用的那个参考通量

### 2.1 观测模型与符号

帧 f 观测真通量 F 的源：

    s_f = a_f · F + n_f ,      Var(n_f) = σ_f²   [ADU²]

- a_f：帧 f 的**线性响应**（ADU / 单位真通量；含通带、曝光、增益、以及任何逐帧光度标度差）；
- σ_f：该样本的噪声（ADU）；**允许依赖信号水平**（源泊松项），σ_f = σ_f(s_f)；
- 现行实现默认 gain<=0 走天空受限分支（snr_science.cpp:175-178，σ_F² = σ_sky²/ΣP²，**与通量无关**）；gain>0 时含源泊松项（:145-174，**与通量有关**）。

### 2.2 最小方差无偏组合（Gauss–Markov / Aitken）

要把不同标度的帧组合成对 F 的估计，先各自归一到公共标度：y_f = s_f/a_f，Var(y_f) = σ_f²/a_f²。独立样本的 BLUE：

    F̂ = Σ_f ( y_f / Var(y_f) ) / Σ_f ( 1 / Var(y_f) )
      = Σ_f ( a_f·s_f / σ_f² ) / Σ_f ( a_f² / σ_f² )

因此作用在**原始样本 s_f** 上的相对权重为

    w_f = a_f² / σ_f² .                                        (★)

**这就是「逆方差加权」在帧间存在标度差时的正确形式**：反演方差必须连同**标度因子平方**一起进入权重。
若 a_f ≡ const（帧已在公共通量标度上），(★) 退化为 w_f ∝ 1/σ_f²。

### 2.3 用存头的 SNR 表达 (★)：配对性定理

定义帧级 SNR（通量型口径，DESIGN §3.4 / 07_noise_snr §4.1）：

    SNR_f = (真通量为 F_ref 的源在本帧的观测信号) / σ_f = a_f·F_ref / σ_f .

于是

    SNR_f² / F_ref² = a_f² / σ_f² = w_f .                      (★★)

**(★★) 成立当且仅当分母 F_ref 与定义 SNR_f 时所用的 F_ref 是同一个参考通量。** 这就是本裁决的「配对性定理」。由此：

- **公共参考（Convention A）**：F_ref = F₀（组内公共的**物理**参考通量）。
  则 SNR_f = a_f F₀/σ_f，SNR_f²/F₀² = a_f²/σ_f² = w_f。
  → **一般成立，含标度因子 a_f²**；且 w_f ∝ SNR_f²（因 F₀ 是常数）。
- **逐帧参考（Convention B）**：F_ref,f = a_f·(本帧参考真通量)。
  则 SNR_f = F_ref,f/σ_f（数值与 A 相同），但 SNR_f²/F_ref,f² = 1/σ_f²。
  → **丢掉 a_f²**；仅当 a_f ≡ const 时才等于 (★)。w ∝ SNR² 也不再成立。

### 2.4 为什么设计坚持公共 F_ref（三条独立理由）

1. **一般性**：只有 Convention A 携带 a_f²（帧间乘性标度）。Convention B 把标度因子除掉了。
2. **可比较性**：SNR_f = a_f F₀/σ_f 是「**同一个参考源**在各帧的 SNR」——只反映噪声与 PSF；而 Convention B 的 SNR_f = F_ref,f/σ_f 把「本帧检出源有多亮」混进了 SNR。DESIGN §3.4:163-167 要求帧级 SNR「**可靠且独立**……作为**唯一帧级参考**」，只有 A 满足。
3. **评价点一致**：σ_f 依赖通量（gain>0 时）。Convention A 下所有帧在同一物理参考通量处评价；Convention B 下各帧在各自的检出通量处评价，使权重带上与天区/深度相关的伪依赖（且当 gain>0 时权重随信号变化）。文献原则：coadd 权重应**与信号无关**（arXiv:2209.09253, PSFs of coadded images, OJA：线性 coadd 只有在「各帧 PSF 相同」或「权重与信号无关」时才有良定义 PSF）。

### 2.5 本裁决不是「公式之争」，而是「约定一致性之争」

关键：**(★★) 对任何 F_ref 都成立，只要分子分母配对。** 所以：

- 若 Phase1 用逐帧参考定义 SNR，则**唯一自洽**的换算是逐帧 F_ref，得 1/σ_f²；
- 若 Phase1 用公共参考定义 SNR，则唯一自洽的换算是公共 F_ref，得 a_f²/σ_f²；
- **两者混用必然错**（多出 (F_ref,f/F_common)² 因子）。

现状正是混用：写侧用 B，读侧（闸门 + compute_inverse_variance_weights 单 F_ref）按 A。**闸门报错是正确行为**。

### 2.6 实测数值：混用会造成多大偏差

convention_compare.json（组内公共取该组 F_ref 中位数）：

| 组 | n | F_ref 离散度 | 公共/逐帧 权重比范围 |
|---|---|---|---|
| t2_m1_red | 2 | 3.75% | 0.964 ~ 1.037 |
| t2_m2_red | 4 | 2.36% | 0.974 ~ 1.021 |
| t3_m1_red | 6 | 13.73% | 0.965 ~ 1.249 |
| t3_m2_red | 4 | 28.65% | 0.962 ~ **1.592** |
| t3_m4_red | 6 | **114.05%** | **0.283** ~ 1.297 |

即：在真实数据上，混用约定可造成**同一帧权重 3.5 倍的错误**（t3_m4_red 离群帧）。这不是数值噪声，是科学错误。

> 注意（澄清一个易混点）：实测 F_ref 的组内离散**不是**「帧未归一到公共标度」的证据。帧间乘性标度差会让信号与噪声同比例缩放，**SNR 不变**；而实测 SNR 在组内也有 1.3× 变化（t3_m4_red：21.36 vs 27.62）。所以逐帧 F_ref 的变化主要来自**检出星群/深度差异**，它既不是纯标度差，也不是公共参考。这正说明「帧自己的检出通量中位数」不是一个合格的参考通量。

---

## 3 文档证据（逐条锚定）

### 3.1 支持「组内公共 F_ref」的条款

| 条款 | 原文要点 | 位置 |
|---|---|---|
| DESIGN §4.3 | 「UPM 归一到公共通量尺度后 Phase2 现场换算 w = 1/σ² = SNR²/F_ref² ∝ SNR²」 | ASTROCS_DESIGN.md:256 |
| DESIGN §3.4 | 帧级 SNR = 未加权原始通量型 F_ref/σ_F，写 HiPS 头（唯一承载）；「可靠且独立……唯一帧级参考」 | ASTROCS_DESIGN.md:163-167 |
| noise_snr §4.1 | SNR_k(F_ref)=F_ref/σ_F；w_k = SNR_k²/F_ref² = 1/σ_F,k²，**「⇒ F_ref 为组内公共常数，w_k ∝ SNR_k²」** | docs/plugins/algorithms_phase1/07_noise_snr.md:59-63 |
| noise_snr §5 | reference_flux：**固定参考通量**（m5/SNR 定义必需） | 07_noise_snr.md:89 |
| UNIFIED_MODEL §2 | frame_snr 行：「Phase2 **归一后**现场换算 w=SNR²/F_ref²=1/σ_F²」 | docs/design/UNIFIED_MODEL.md:42 |
| PSF_SIGNAL_WEIGHT §2 | 点源信息权重「**对已归一到公共通量尺度的帧**」给出 W_info,k = a_k²PᵀC⁻¹P | docs/science/PSF_SIGNAL_WEIGHT.md:19-35 |
| ALG-P2-POINT-001 | SNR_k = F_ref·sqrt(W_info,k)，**「固定参考通量 F_ref」** | docs/algorithms/v6/phase2-point/ALG-P2-POINT-001_SPEC.md:121,130 |
| integration §4.0 | 由各自 SNR 现场换算逆方差权重 | docs/plugins/algorithms_phase2/13_integration.md:32-38 |
| aio_hips.h | ASTROCS_REFERENCE_FLUX = **组内公共参考通量** F_ref | lib/infrastructure/aio/include/aio_hips.h:302 |
| SD-15（验收） | 冻结键名；Phase1 必须写入 | 工程控制/RELEASE-02/ACCEPTANCE.md:113 |
| HUB-B 报告 | 键语义表：F_ref = 组内公共参考通量（w=SNR²/F_ref²=1/σ_F² 的公共标度） | reports/RELEASE-02/hub-b-report.md:98-101 |
| weight_chain.h | 前置条件：调用前帧已由 UPM 归一到公共通量尺度，F_ref 为组内公共常数 | lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h:34-35 |
| 研究包 §6.1 | 要求证明 w ∝ SNR² 并写明成立条件（高斯噪声、背景主导、**已归一化**） | docs/research/SNR_WEIGHT_RESEARCH_PACK.md:91 |

**文档面高度一致：F_ref 是组内公共参考通量。** 唯一的分歧不在「逐帧 vs 公共」，而在 §3.3 的命名冲突。

### 3.2 UPM 目标模型 vs 冻结实现（影响前置条件的真假）

| 条款 | 内容 | 位置 |
|---|---|---|
| DESIGN §4.2 | UPM 联合相对模型 **g·s+b**（乘性响应 + 加性背景） | ASTROCS_DESIGN.md:234 |
| plugin 11_upm §4.1 | y_k(x) = g_k·s(x) + b_k(x)；g_k 乘性、b_k 加性，**不得互相代替**；输出「施加归一化后的产品」 | docs/plugins/algorithms_phase2/11_upm.md:5,25-31 |
| SCI-UPM §1/§5/§6/§10 | 现行**冻结**模型为**纯加性** calibrated = raw − C_f；「**乘性尺度差已撤销**」；§10 明文「**在 calibrate_block 中引入乘性尺度**」为不可接受变化 | docs/science/PHASE2_UPM.md:7,8,48-50,81,125,157 |
| SCI-UPM §14a | 已登记 UNRESOLVED：设计/插件文档的 g_k 与冻结纯加性模型**不可同时为真** | docs/science/PHASE2_UPM.md:182 |
| SCI-AUDIT VERDICT | 判定设计侧（g·s+b）成立，PHASE2_UPM 文档落后于设计；「UPM 相对定标（g 已实现但未接线/文档未对齐）」 | reports/RELEASE-02/SCI-AUDIT/VERDICT.md:26,40,60 |

**代码核实结论**：lib/algorithms/coverage/src/upm.cpp 全文 **grep gain/g_k/multipl/乘性 零命中**；p2_upm_calibrate_block（upm.cpp:1274-1303）逐点执行 output = input − C_f（:1300）；upm-apply 调用点注释同样写 corrected = (raw − C_f − b_k)（module_adapters.cpp:4737-4748）。即：**现行生产 UPM 无乘性归一**。SCI-AUDIT 的「g 已实现」表述与代码不符（应记为「g 未实现」）；这不影响本裁决方向，但应订正登记。

### 3.3 CONTROL_WEIGHT_SNR.md 与 07_noise_snr.md 的互斥：**是命名冲突，不是公式冲突**

- CONTROL_WEIGHT_SNR.md 的 frame_snr = 「**整帧 Phase1 SNR 目录值的中位数（回退质量基准）**」，属 stage2 控制采样的**相对质量权重场**，且明确「本文件不定义 mode 2 的权重语义」，weight_mode=0 为 legacy/诊断（docs/science/CONTROL_WEIGHT_SNR.md:32-40,66-77,102-106）。
- 07_noise_snr.md / UNIFIED_MODEL.md / DESIGN 的 frame_snr = **写进 HiPS 头的帧级通量型参考 SNR**，是权重链输入。
- 二者**是两个不同的量，共用了同一个标识符 frame_snr**。文档已自述 UNRESOLVED（CONTROL_WEIGHT_SNR.md:140、PSF_SIGNAL_WEIGHT.md:117）。
- 验收已裁决（SD-10）：**以最高设计为准（通量型未加权 SNR）**，CONTROL_WEIGHT_SNR.md 走变更 claim（ACCEPTANCE.md:105）。

**本裁决确认 SD-10 并给出更精确的措辞**：不是「CONTROL_WEIGHT_SNR 错了」，而是它的 frame_snr 是**另一个对象**，必须改名消歧（见 §7.2）。权重链只认 HiPS 头 ASTROCS_FRAME_SNR；FrameSnrKind::kRelativeQualityWeight 已 fail-closed 拒绝质量权重冒充（weight_chain.cpp:298-305），处置正确。

---

## 4 代码证据：实际执行顺序与现状

### 4.1 顺序（问题 2 的「顺序」部分：满足）

- Phase2 canonical 节点链：coverage→sample→upm_fit→upm_apply→reject→integrate→write
  （lib/infrastructure/scheduler/src/module_adapters.cpp:7581-7589；头部注释 :897,7575）。
- 权重链在 p2_op_integrate（:5271）内，且它**消费 upm-apply 产物** p2_corrected.json（:5274-5278），
  p2_corrected.json 由 p2_op_upm_apply（:4593）写出。
  ⇒ **UPM apply 严格先于权重链**。顺序前置条件成立。

### 4.2 内容（问题 2 的「归一」部分：不满足）

- p2_op_upm_apply 对每帧调用 p2_upm_calibrate_block(model, fid, leaves, in_v, out_v, n_valid)（:4735-4736），
  该函数只做 output = input − C_f（upm.cpp:1300）——**加性**。
- 无任何乘性增益施加。因此 UPM **没有**把帧归一到公共通量标度。
- 「公共通量标度」目前只由 SCI-UPM §6 的**假设**「帧间无乘性尺度差」保证。

### 4.3 写侧现状（缺陷点）

- lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp:167-170：

      const double ref_flux =
          (std::isfinite(cfg.reference_flux_adu) && cfg.reference_flux_adu > 0.0)
              ? cfg.reference_flux_adu
              : median_of(used_flux);          // ← 逐帧检出通量中位数（缺陷）

  随后 :184-187 输出 reference_flux_adu = ref_flux、reference_snr_f = ref_res.snr_optimal（在 ref_flux 下评价）。
- 调用侧 module_adapters.cpp:3160：sci_cfg.reference_flux_adu = sc.value("reference_flux_adu", 0.0);
  缺省 0.0 ⇒ 落入逐帧中位数回退。:3256-3257 逐帧调用 compute_snr_frame_science；
  :3289-3294 写 snr_reference.{flux_adu,snr_f}；:3323-3327 落 p1_snr.json。
- 写头侧 lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:344-402：读同目录 p1_snr.json
  的 snr_reference.{snr_f,flux_adu}，逐帧原样写入 ASTROCS_FRAME_SNR/ASTROCS_REFERENCE_FLUX
  （aio_hips_writer.cpp:1522-1541，键名与语义见 aio_hips.h:296-303）。
  ⇒ **写侧成对自洽，但用的是逐帧参考**。

### 4.4 读侧现状（正确，保持）

- 闸门 module_adapters.cpp:5377-5381（要求组内公共，容差 1e-9 相对）。
- compute_inverse_variance_weights(winputs, ref_flux)（:5383-5385）只接受**单个**组级 F_ref；
  weight_from_snr 实现 w = (SNR/F_ref)²（weight_chain.cpp:78-96）；
  头文件把「调用前帧已归一到公共通量尺度、F_ref 为组内公共常数」写成**前置条件**（weight_chain.h:34-35）。
- 判为**正确**：它与设计一致；缺的是写侧没按约定产出公共值。

---

## 5 文献证据

1. **Horne 1986**（PASP 98, 609；DOI 10.1086/131801）最优提取：σ_F⁻² = Σ_i P_i²/σ_i²，SNR_F = F/σ_F——逆方差加权的经典源头；本仓 docs/science/CONTROL_WEIGHT_SNR.md:137、docs/research/SNR_WEIGHT_RESEARCH_PACK.md:64 均引。**Naylor 1998**（MNRAS 296, 339）成像测光对应版。
2. **Gauss–Markov / Aitken 加权最小二乘（教科书级）**：对独立观测 y_i = a_i μ + ε_i，Var(ε_i)=σ_i²，BLUE 为 μ̂ = Σ(a_i y_i/σ_i²)/Σ(a_i²/σ_i²)，即权重 w_i = a_i²/σ_i²。这正是 §2.2 的 (★)。**结论：权重必须携带帧间标度因子平方 a_f²。**
3. **Siril（GPL-3.0，参考实现对照）**：帧权重 w_i = 1/(pscale_i²·bgnoise_i²)（src/stacking/median_and_mean.c:1111-1230）——**光度标度平方进入权重**，与 (★) 同构。来源：docs/research/SNR_WEIGHT_RESEARCH_PACK.md:47（研究包已登记的对照结论）。
4. **SWarp（GPL-3.0）**：逐像素 ivar 组合 out = Σ(x_k/var_k)/Σ(1/var_k)、var_out = 1/Σ(1/var_k)（src/coadd.c:1279-1311）——**仅在帧已同标度时直接可用**，否则需 RESCALE_WEIGHTS 重标定。来源：研究包 §4:48。
5. **arXiv:2209.09253**（PSFs of coadded images, Open Journal of Astrophysics）：线性 coadd 只有「各帧 PSF 相同」或「**权重与信号无关**」时才有良定义 PSF；建议权重不与信号相关。→ 反对把「本帧检出通量」放进帧权重的评价点（Convention B/C 的固有缺陷）。
6. **PixInsight 公开方法学**（docs/science/PSF_SIGNAL_WEIGHT.md:41-48）：只把信号/噪声/背景分量写入元数据，集成时才计算权重；与本仓「数据库存客观 SNR、Phase2 现场换算」一致，且**权重不在入库量里**。

**文献结论**：逆方差加权的正确权重是 (★) a_f²/σ_f²。要在元数据只有 (SNR_f, F_ref) 的情况下恢复它，分母必须取**公共参考通量**（Convention A）；逐帧参考（B）丢掉 a_f²。文献支持公共 F_ref。

---

## 6 明确裁决

**裁决（WEIGHT-SCI-001）：w_f = SNR_f²/F_ref² 中的 F_ref 必须是「组内公共参考通量」；存入 HiPS 头的 ASTROCS_FRAME_SNR 必须是该公共参考通量下的 SNR（配对）。**

具体条款：

1. **写侧**：Phase1 必须为**整个帧组**选定**一个**参考通量 F₀，用它定义每帧的 ASTROCS_FRAME_SNR，并把**同一个** F₀ 写入每帧的 ASTROCS_REFERENCE_FLUX。**禁止**用本帧检出源通量中位数作为逐帧参考（现行 snr_frame_science.cpp:170）。
2. **读侧**：闸门 module_adapters.cpp:5379-5380 与 compute_inverse_variance_weights(..., single F_ref) **保持**；不得放宽为逐帧 F_ref。
3. **理由优先级**：配对性定理（§2.3）+ 最小方差一般式 (★)（§2.2）+ 可比较性（DESIGN §3.4）+ 设计/文档一致（§3.1）+ 文献（§5）。
4. **成立条件（诚实登记）**：在**现行冻结管线**下（UPM 纯加性、SNR 不含光度响应 a_k、默认 gain<=0 天空受限），(★) 退化为 1/σ_f²，Convention A 与 B **数值等价**。因此本裁决的**直接收益是「约定自洽 + 存头 SNR 可比较」**，而 a_f² 因子的恢复还依赖两项未闭合：①UPM 的乘性归一 g_k 接线；②SNR 计算引入光度响应 a_k（设计侧 W_psf,k = a_k²PᵀC⁻¹P，snr_science.cpp 现无此入参）。
5. **备选方案（不推荐）**：若负责人决定不动 Phase1，则唯一自洽的替代是把权重链与闸门改成**逐帧 F_ref**（w=SNR²/F_ref,f²=1/σ_f²）。代价：与 DESIGN §4.3:256、07_noise_snr §4.1:63、UNIFIED_MODEL §2:42、aio_hips.h:302、SD-15 直接冲突，须走**最高设计变更**；且丢失存头 SNR 的可比较性与 w ∝ SNR²。**仅在冻结假设「帧间无乘性尺度差」永久成立时才可接受。**

---

## 7 需改动的精确位置（本轮不改；前台统一改）

### 7.1 代码（推荐路径：改写侧，保持读侧）

| # | 文件:行 | 现状 | 改法 |
|---|---|---|---|
| C1 | lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp:167-170 | ref_flux = cfg.reference_flux_adu>0 ? cfg.reference_flux_adu : median_of(used_flux) | **删除逐帧中位数回退**。reference_flux_adu 缺失/非有限/≤0 时：out.reason="reference_flux_adu required (group-common F_ref; per-frame median fallback removed)"; out.valid=false; return out;（fail-closed）。保留 :184-187 的成对输出。 |
| C2 | lib/infrastructure/scheduler/src/module_adapters.cpp:3149-3163（p1_op_noise） | 逐帧 sci_cfg.reference_flux_adu = sc.value("reference_flux_adu", 0.0) | **为整个 input_lights 数据块算/取一个公共 F₀**，并对所有帧使用同一值。二选一：<br>（a）**首选**：要求 snr.reference_flux_adu 显式给出（缺失/≤0 → ErrorDomain::DATA fail-closed）。全局固定值 ⇒ 闸门恒过。<br>（b）**次选**：两遍法——先收集各帧检出通量中位数，取块级中位数作 F₀，再逐帧以该 F₀ 调 compute_snr_frame_science。仅当一次 Phase1 run 的帧即 Phase2 组时闸门才过。 |
| C3 | lib/infrastructure/scheduler/src/module_adapters.cpp:3289-3294 | 写 snr_reference.{profile,flux_adu,fwhm_px,snr_f,sigma_f_adu} | 增写 provenance：snr_reference.scope="group"、reference_flux_source="config"|"group_median"；保证同组同值（%.17g 已由写头侧 round-trip）。 |
| C4 | lib/infrastructure/scheduler/src/module_adapters.cpp:5377-5381 | 闸门只报「逐帧不一致」 | **保持 fail-closed**；建议错误串补 expected=<ref_flux> actual=<fref> frame=<id>，便于定位。**不得放宽容差、不得删除。** |
| C5 | lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h:34-35,103-105 | 前置条件文字 | 澄清：reference_flux 必须**等于定义 frame_snr 时所用的参考通量**；配对性定理（无功能改动）。 |
| C6 | lib/algorithms/integration/v6/src/weight_chain.cpp:78-96,271-282 | w=(SNR/F_ref)² | **不改**（正确）。可选：在 weight_from_snr 注释补配对性说明。 |
| C7 | lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1522-1541、astro_sphere_sink.cpp:344-402 | 成对写入，已 fail-closed | **不改**；若 C2 改两遍法，sink 侧无需变（仍读 p1_snr.json）。 |

**不推荐路径（若改读侧为逐帧 F_ref）**：weight_chain.h:164-166 签名改为逐帧 reference_flux[]；weight_chain.cpp:271-282,348-354 改为逐帧；module_adapters.cpp:5377-5381 删除等值检查并逐帧传参；同时必须改 DESIGN §4.3:256、07_noise_snr.md:63、UNIFIED_MODEL.md:42、aio_hips.h:302、SD-15（**最高设计变更**）。

### 7.2 文档（变更 claim 草案，按 ENGINEERING_SPEC §3）

> 说明：ENGINEERING_SPEC §3 要求「独立证据证明文档与标准/事实不符时，订正文档是义务：走变更 claim（证据、影响面、版本递增）+ 一致性回归」。以下为草案，**本轮不落盘修改**。

**CLAIM-DOC-WEIGHT-SCI-001 — CONTROL_WEIGHT_SNR.md 的 frame_snr 命名消歧**

- **目标文档/条款**：docs/science/CONTROL_WEIGHT_SNR.md §2 符号表 frame_snr 行、§2a、§4、§7、§9 UNRESOLVED。
- **证据**：
  - 该文件 frame_snr = 「整帧 Phase1 SNR 目录值的中位数（回退质量基准）」，用于 stage2 控制采样的 quality_weight（weight_mode=0 legacy）；
  - HiPS 头 ASTROCS_FRAME_SNR = 帧级通量型参考 SNR（aio_hips.h:296-303、07_noise_snr.md:55-64、UNIFIED_MODEL.md:42）；
  - 二者是**不同对象**，仅标识符同名。验收 SD-10 已裁决以最高设计为准（ACCEPTANCE.md:105）。
- **改法**：把 stage2 控制采样的字段 frame_snr 更名为 frame_quality_weight（或其别名），并在 §2/§9 增加一句：「本字段**不是** HiPS 头 ASTROCS_FRAME_SNR（帧级通量型参考 SNR）；权重链只消费后者」。删除 §9 的 UNRESOLVED 登记（改为「已消歧」）。
- **影响面**：docs/science/CONTROL_WEIGHT_SNR.md（文档）；其引用的实现锚 stage2.cpp 的 frame_snr_medians/frame_snr_by_id 若在文档中作为规范名出现，需同步为别名说明（**不改实现语义**，只改文档命名）。
- **版本递增**：SCI-CW-001..008 状态由 FROZEN 递增为 FROZEN + 命名消歧注记（P5-SNR 式原位注记）。
- **一致性回归**：tools/science_contract_lint.py；grep -rn "frame_snr" docs/science 确认不再指向两个对象。

**CLAIM-DOC-WEIGHT-SCI-002 — 07_noise_snr.md §4.1 补前置条件（固定/公共 F_ref）**

- **目标文档/条款**：docs/plugins/algorithms_phase1/07_noise_snr.md §4.1:55-64、§5:89。
- **证据**：§4.1 已写「F_ref 为组内公共常数」，但未写「**禁止**逐帧检出通量中位数回退」；实现 snr_frame_science.cpp:170 正是该回退，是 RELEASE-02 权重链 fail-closed 的直接根因。
- **改法**：在 §4.1 公式块后加一句：「F_ref 必须是**帧组固定/公共**参考通量；每帧的 frame_snr 必须在该参考通量下定义，并与写头的 ASTROCS_REFERENCE_FLUX 配对。**禁止**用本帧检出源通量中位数作逐帧参考（否则 w=SNR²/F_ref² 与 1/σ_F² 不再配对）。」
- **影响面**：仅文档；实现由 C1/C2 落地。
- **版本递增**：插件文档原位注记（不改公式）。
- **一致性回归**：docs/ 交叉引用检查；07_noise_snr.md 与 weight_chain.h:34-35 措辞一致。

**CLAIM-DOC-WEIGHT-SCI-003 — PHASE2_UPM.md/11_upm.md 的 g_k 状态与权重链前置条件绑定**

- **目标文档/条款**：docs/science/PHASE2_UPM.md §1/§6/§14a；docs/plugins/algorithms_phase2/11_upm.md §4.1；docs/algorithms/UPM_SOLVER.md §2 F4。
- **证据**：设计/插件文档要求 g·s+b（ASTROCS_DESIGN.md:234、11_upm.md:25-31），冻结实现为纯加性（upm.cpp:1300，全文无 g_k），SCI-UPM §14a 已登记 UNRESOLVED；SCI-AUDIT 的「g 已实现」与代码不符。
- **改法**：把 UNRESOLVED 与权重链前置条件显式绑定：「在 g_k 接线前，weight_chain.h:34-35 的『UPM 归一到公共通量尺度』前置条件**仅由 SCI-UPM §6『帧间无乘性尺度差』假设兜底**；一旦 g_k 生效，须同步重审 w=SNR²/F_ref² 是否携带 g_f²。」并把 SCI-AUDIT 的「g 已实现」订正为「g 未实现（代码零命中）」。
- **影响面**：文档 + SCI-AUDIT 台账；实现由 UPM 后续任务落地。
- **版本递增**：SCI-UPM-001 原位注记。
- **一致性回归**：grep -rn "g_k" lib/algorithms/coverage 应为 0（未接线前），与文档状态一致。

**CLAIM-DOC-WEIGHT-SCI-004（可选）— DESIGN §4.3:256 显式前置条件**

- 在 ASTROCS_DESIGN.md:256 的「UPM 归一到公共通量尺度后」后补「（F_ref 为组内公共常数；写入 HiPS 头的 frame_snr 必须定义在该参考通量下）」。属澄清，不改公式。

---

## 8 证据索引与复现

| 证据 | 路径 |
|---|---|
| 本裁决报告 | reports/RELEASE-02/weight-sci-ruling.md |
| 约定对比复算脚本（纯 stdlib） | run/RELEASE-02/weight-sci/analyze.py |
| 约定对比结果（12 组 / 49 帧） | run/RELEASE-02/weight-sci/convention_compare.json |
| L4 实测 properties（49 帧） | run/RELEASE-02/L4-rebuild/norm/**/signal/properties |
| 闸门 | lib/infrastructure/scheduler/src/module_adapters.cpp:5377-5381 |
| 节点顺序 | lib/infrastructure/scheduler/src/module_adapters.cpp:7581-7589 |
| UPM 加性校正 | lib/algorithms/coverage/src/upm.cpp:1274-1303（:1300） |
| Phase1 逐帧参考回退（缺陷） | lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp:167-170 |
| 写头 | lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:344-402；aio_hips_writer.cpp:1522-1541 |
| 权重链 | lib/algorithms/integration/v6/src/weight_chain.cpp:78-96,271-363；.../include/astrocs/v6/weight_chain.h:34-35 |
| 研究包（文献/开源对照） | docs/research/SNR_WEIGHT_RESEARCH_PACK.md |
| 审计裁决（SD-10/SD-15） | 工程控制/RELEASE-02/ACCEPTANCE.md:105,113；reports/RELEASE-02/SCI-AUDIT/VERDICT.md |

复现：

    export TMPDIR=/dev/shm/astrocs_wtsci
    python3 -B run/RELEASE-02/weight-sci/analyze.py > run/RELEASE-02/weight-sci/convention_compare.json

---

## 9 未闭合项（上呈）

1. **g_k 乘性归一未接线**：设计/插件要求 g·s+b，实现纯加性。在接线前，本裁决第 6.4 条的等价性成立；接线后必须重审 w=SNR²/F_ref² 是否携带 g_f²（本裁决 §2.2 的 (★) 要求携带）。
2. **SNR 不含光度响应 a_k**：设计 W_psf,k=a_k²PᵀC⁻¹P（PSF_SIGNAL_WEIGHT.md:23），实现 snr_source_snr_f64 无 a_k 入参。这决定 a_f² 因子能否从存头量恢复；属 Phase1 科学面后续任务。
3. **reference_flux 取固定配置值还是块级统计值**：本裁决只定「必须组内公共」；C2(a)/(b) 的取舍（全局固定 vs 块级）属负责人治理决定。建议 (a)（全局固定，闸门恒过）。
4. **两遍法 vs 单遍法**：C2(b) 需一次预扫描；若选 (b) 须登记性能与确定性影响（不得改科学语义）。
