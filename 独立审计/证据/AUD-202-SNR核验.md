# AUD-202 跨帧绝对信噪比链路核验报告（AUDIT-06 只读审查）

- 审查节点：独立只读审查节点（不构建、不跑 ctest、不跑 `run_checks.py`、不跑三命令端到端）
- 冻结基线：`HEAD = c8f64e9a`
- 开工基线 `git -C "F:/Astro dev/Astro CS Normalization Database" status --porcelain`（逐字）：

```text
?? ACSD整治工作包_AUDIT-06.zip
?? site/
```

- 权威链已读：`ASTROCS_DESIGN.md` §2.2 / §5.3 → `docs/science/CONTROL_WEIGHT_SNR.md`（全文）→
  `docs/science/NOISE_MODEL.md`（全文）→ `docs/science/PSF_SIGNAL_WEIGHT.md`（全文）→
  `docs/plugins/algorithms_phase1/07_noise_snr.md`（全文，含 §4.1/§4.2a/§4.5/§8b）→
  `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:99`、`eng/packaging/config/defaults.json`
  SNR 域三键、`docs/algorithms/NOISE_ESTIMATION.md`、`docs/algorithms/PHASE2_COVERAGE.md`。
- 独立复算脚本：`独立审计/复算件/aud202\aud202_recompute.py`
  （纯 numpy，**不 import 仓库任何模块**，自 seed 20260925；输出 `aud202_recompute.json`，
  运行日志 `run1.log`/`run2.log`）。

## 1. 链路口径与公式（核验所得现行口径，非照抄文档）

生产链（scheduler 路径，`lib/infrastructure/scheduler/src/module_adapters.cpp`）：

```text
Phase1  逐源 σ_F,i^-2 = Σ_k P_k²/σ_k,i² ,  σ_k,i² = σ_sky,i² + (RN/g)² [仅 shot_only 声明时] + F_i·P_k/g
        frame_snr  = F_ref / σ_F(ref profile)   ,  F_ref,k = 10^(−0.4(m_ref − ZP_k)), m_ref = 6.0
        HiPS 头写 ASTROCS_FRAME_SNR = snr_reference.snr_f （astro_sphere_sink.cpp:438-460）
Phase2  w_k = actual_snr_k² / F_ref,k² · g_k² ,  actual_snr = frame_snr × intra_snr
        实际 intra_snr ≡ 1.0（`in.sparse = nullptr`），或 w = 逐样本 ivar（Phase1 空背景面）
```

独立复算对**代数本身**的判定：`w = SNR²/F_ref² ≡ 1/σ_F²` 在 SNR 定义为 `F_ref/σ_F` 时按机器精度成立
（R3：最大相对偏差 3.01e-16，四组异源参数）。**该恒等式成立的前提是分子取 F_ref**；
生产实现是否满足这个前提是本报告的核心问题，见 AUD202-003/004/005。

## 2. 证据清单（一手锚点，本次逐字段实测）

| 证据 | 级别 | 出处（本次实际核到的字段） | 支撑主张 | 适用域 |
|---|---|---|---|---|
| Horne 1986 | A 论文 | Crossref `10.1086/131801` → 标题 *An optimal extraction algorithm for CCD spectroscopy*, PASP **98**, 609, 1986-06, 作者仅 `Horne` | 最优提取 `F̂=ΣP d/σ²/ΣP²/σ²`、`Var=1/ΣP²/σ²` | **CCD 光谱一维抽取**，非宽带成像点源测光（外推需声明） |
| Zackay & Ofek I | A 论文 | Crossref `10.3847/1538-4357/836/2/187` → *How to COAAD Images. I. Optimal Source Detection and Photometry of Point Sources Using Ensembles of Images*, ApJ **836**, 187；arXiv **1512.06872** | 逐帧 matched filter 后再加权求和最优 | 点源 |
| Zackay & Ofek II | A 论文 | Crossref `10.3847/1538-4357/836/2/188` → *… II. A Coaddition Image that is Optimal for Any Purpose in the Background-dominated Noise Limit*, ApJ **836**, 188；arXiv **1512.06879** | 合成图与信息保持表示 | **背景主导噪声极限**（标题即限定），不覆盖源受限亮像素 |
| PixInsight | A 开源(文档) | `pixinsight.com/doc/docs/ImageWeighting`（式[16][18][20]）；本仓 `PSF_SIGNAL_WEIGHT.md:41-53` 已逐字核对 | PSFSNR=功率比口径、PSFSW=权重 | 方法学对照，非常数来源 |
| 本仓实验 | B | `实验/absolute-snr/results/b1_sky_scan.json`、`b2_noise_terms.json`、`b4_integration.json`（seed 20260921） | 天光单调性、双计偏差、配对恒等 | 见各 JSON 冻结配置 |

**Zackay & Ofek I/II 与 arXiv 末位序号绑定核查（任务书点名的历史高危形态）：证真为正确。**
逐处核对：`docs/science/PSF_SIGNAL_WEIGHT.md:145-146`、`docs/references/SCIENTIFIC_REFERENCES.md:17-18,164-165`、
`docs/research/SNR_WEIGHT_RESEARCH_PACK.md:65-66,157,159`、`docs/validation/v6/QA_MATRIX.md`（12 处）
全部为 I↔836,187↔1512.06872、II↔836,188↔1512.06879，与 Crossref/arXiv 逐字一致，**无绑反**。
详见 AUD202-002。

## 3. 结论条目

### AUD202-001 生产 Phase2 逐像素权重取 Phase1 **空背景** ivar，不含源光子散粒项

- 对象（路径:行 + 该行内容锚）：
  - `lib/infrastructure/scheduler/src/module_adapters.cpp:11937` → `            w = static_cast<double>(ivar_v[d][static_cast<size_t>(p)]);`
  - 同文件 `:11463` → `  std::string weight_basis = "per_sample_ivar";   // §30.1: w_i = 逐样本 ivar`
  - 该 ivar 的生产者：`module_adapters.cpp:7980-7982` `P1NoiseFrameModel nmc = p1_noise_model_for_frame(...)`
    → `:8156` `frame, "variance", AIO_BLOCK_FLOAT32, var_plane.data(),` 单位串 `"定案2 NoiseWeightModelV1 blank-sky variance (ADU^2, ...)"`
- 权威依据：`ASTROCS_DESIGN.md:171-174`「**加权方差必须含源项（正向约束）**…其中**必须含源的散粒项**（∝ `N_src/g`）。只含空背景项的方差是**背景受限**口径…最优加权的唯一来源是含源项的总方差」；
  同节 `:156-158`「以背景方差倒数定权会使权重不含源光子散粒项、把亮源像素过权」；
  `docs/science/CONTROL_WEIGHT_SNR.md:62-65`（§2a 同一条）；`docs/science/NOISE_MODEL.md` §5 vs §5c 两面的「取值互不代用」。
- 现状 → 应为：现状 = 叠加权重 `w(x,y) = 1/σ_bg²(x,y)`（`σ_bg²` 由星点掩膜后 blank-sky MAD 平面场给出，按构造**不含源项**）
  → 应为 = `w(x,y) = 1/σ_w²(x,y)`，`σ_w² = σ_bg² + S_src(x,y)/g`（NOISE_MODEL §5c 的加权方差面）。
- 改法：把加权方差面（NOISE_MODEL §5c）作为**独立的第二张面**产出并入产品（或在 Phase2 由 UPM 后的源项
  `S_src = Σ F̂_i P_i` 现算叠加），Phase2 集成读该面而非 `ivar`；`ivar`（背景面）继续只服务天光建模与误差报告。
- 证据级别：A论文（Horne 1986 / Z&O I 的权重对象是点源估计量的方差，含源项）+ B本仓实验 + 本次独立推导复算（R4）。
- 置信：**CONFIRMED**（代码写入点 + 单位串 + 权威条款三面对齐）
- 复算命令或推导：
  `PYTHONDONTWRITEBYTECODE=1 python -B "独立审计/复算件/aud202\aud202_recompute.py"` → `R4_production_paths`。
  独立复算给出「中心像素 `1/σ_bg²` 对 `1/σ_w²` 的过权倍数」随源通量的增长：
  F=10 e⁻ → ×1.011；100 → ×1.106；1000 → ×2.057；3×10⁴ → ×32.7；10⁶ → **×1058**。
  即该缺陷对亮源像素的过权可达 3 个数量级，与 `ASTROCS_DESIGN.md:158` 的定性警告同向、量级由本次给出。

### AUD202-002 Zackay & Ofek I/II 归属与 arXiv 末位序号绑定：无绑反（对任务书给定前提的证伪性核查）

- 对象：`docs/science/PSF_SIGNAL_WEIGHT.md:145` → `- **点源信息权重 … Zackay & Ofek 2017, ApJ 836, 187（arXiv:1512.06872）。`
  与 `:146` → `- **proper coadd / 信息保持组合**：Zackay & Ofek 2017, ApJ 836, 188（arXiv:1512.06879）。`
- 权威依据：本次 Crossref 逐条回读（`10.3847/1538-4357/836/2/187` → 标题含 "I. Optimal Source Detection and Photometry…"；
  `10.3847/1538-4357/836/2/188` → 标题含 "II. A Coaddition Image … Background-dominated Noise Limit"）+ arXiv 摘要页逐条回读。
- 现状 → 应为：现状 = 全库 19 处引用（`docs/references/SCIENTIFIC_REFERENCES.md:17-18,164-165`、
  `docs/research/SNR_WEIGHT_RESEARCH_PACK.md:65-66,157,159,165`、`docs/validation/v6/QA_MATRIX.md` 12 处、
  `docs/science/UNIFIED_SCIENCE_MODEL.md:127,129`、`docs/science/INTEGRATION.md:163`、`docs/algorithms/*` 3 处）
  **一致且正确** → 应为：无需订正。**计数命令**：
  `grep -rn "1512.0687\|836, 18\|836,18" --include=*.md --include=*.json --include=*.py --include=*.cpp docs 实验 lib eng`
  → 30 行命中（含 15 条文献登记行），逐行核对后**零处绑反**。
- 但**适用域错配需登记**（同一证据的引用面问题，不是绑定问题）：`PSF_SIGNAL_WEIGHT.md:145` 用 Z&O **I** 支撑
  "点源信息权重 Q/W/Var(F)=1/W" —— 该式的原始出处是 **Horne 1986**（本次核到标题即 "optimal extraction algorithm for CCD spectroscopy"，
  **一维光谱抽取**），Z&O I 是成像点源版本，引用可保留但须写明是「成像点源推广」；
  而 `docs/science/UNCERTAINTY_AND_COVARIANCE.md:142` 与 `docs/algorithms/PHASE2_MOSAIC_WRITE.md:518`
  引 **Z&O II** 支撑 `C_out = R C_in Rᵀ` 的相关噪声传播 —— Z&O II 的标题限定是
  **background-dominated noise limit**，而本仓 §2.2 要求权重含源项（源受限域）。
  → 应为：给这两处补「背景主导极限」的适用域声明，或另找覆盖源受限的锚（Horne/Naylor 的完整协方差形式）。
- 证据级别：A论文（本次实测 DOI/arXiv 双通道）。
- 置信：**CONFIRMED（绑定正确）**；适用域部分 **PARTIAL**
- 复算命令或推导：`curl -s https://api.crossref.org/works/10.3847/1538-4357/836/2/187`（同上 188、10.1086/131801）。

### AUD202-003 帧级 SNR 回退链的权重是**逐帧常数**，生产数据面根本没有逐像素权重可用

- 对象：`lib/infrastructure/scheduler/src/module_adapters.cpp:11534` → `        in.sparse = nullptr;   // 稀疏 SNR 层尚未接入生产数据面`
  下游 `lib/algorithms/integration/v6/src/weight_chain.cpp:714` → `    double intra = 1.0;`（`:715` 的 `if (f.sparse != nullptr && f.sparse->present)` 恒假）
  → `:739` `if (!compose_actual_snr(f.frame_snr, intra, &actual, &cerr))` → `:779` `if (!weight_from_snr(actual, f_ref_k, &w, &werr))`
  → `:783` `w *= g * g;`
- 权威依据：`docs/science/PSF_SIGNAL_WEIGHT.md:82-89`（§4「阶段一产稀疏 SNR 控制点 → 阶段二用每帧的稀疏控制点重建稠密 SNR 面 → 取逆方差定权」= 生产默认组合）；
  `docs/science/CONTROL_WEIGHT_SNR.md:229-237`（§8c 生产默认组合）；`ASTROCS_DESIGN.md:188-191`（帧内空间变化由 `sparse_snr_layer` 承载）。
- 现状 → 应为：现状 = 走 ivar 时是**背景面**逐像素权（AUD202-001），走帧级 SNR 链时是
  `w_k = frame_snr_k²/F_ref,k² = 1/σ_F,k²(ref profile)` —— **一帧一个数，信号维与空间维全为常数**；
  两条路径都不存在「含源项的逐像素权重」。→ 应为 = 默认路径由稀疏绝对 SNR 控制点重建稠密 `SNR(x,y)` 后现场换算，
  或退一步由含源项的加权方差面给出逐像素权。
- 改法：① 把 `sparse_snr_layer` 接入 Phase2 数据面并填 `in.sparse`（该接入点已预留、注释即写"尚未接入"）；
  ② 或实现含源项的加权方差面产品并让它优先于背景 ivar；③ 无论哪条，删除「帧级常量权」作为**唯一**可达路径的现状。
- 证据级别：B本仓实验不可用（本机不跑）→ 本条为纯代码接线事实 + 权威条款比对，A级条款直接引用。
- 置信：**CONFIRMED**
- 复算命令或推导：
  `grep -n "in.sparse = nullptr" "…/module_adapters.cpp"` → 1 处；
  `grep -n "double intra = 1.0" "…/weight_chain.cpp"` → :714。
  代数：`intra≡1 ⇒ w_k = frame_snr_k²/F_ref,k²`，与像素 `(x,y)` 无关 ⇒ 权重在信号维**恒为常数**（这是任务书 (f) 点名的形态，此处由接线而非分母/幂次错误造成）。

### AUD202-004 稀疏控制点存的是**逐源自身通量** SNR，与合同冻结的 `F_ref/σ_F(x,y)` 不同口径

- 对象（生产者）：`lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:65-91` `sourceSnrFromPsfRow` →
  `:80` `    p.flux_adu = F;`（`F = row[2]` 即**该星自己的**通量；`:75` 缺失时用 `2πAσ²/3` 兜底）
  → `:90` `    return res.snr_optimal;`，而 `snr_science.cpp:215` 的 `out->snr_optimal = F / out->sigma_f_optimal_adu;`
  写入点：`snr_estimator.cpp:900` `            pts[i].ra = ra; pts[i].dec = dec; pts[i].snr_psf = valid[(size_t)i].snr;`
  → 落盘 `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:2504` `            double snr_val = snr_model_f64->points[i].snr_psf;`
- 合同/权威（应为）：`eng/contracts/schemas/unified/sparse_snr_layer.schema.json` description →
  「控制点值 = 该点的通量型信噪比 **`F_ref/sigma_F(x,y)`**（与 frame_snr 同物理定义、同逐帧参考通量 `F_ref`）」；
  `docs/plugins/algorithms_phase1/07_noise_snr.md:26`（§3）与 `:179`（§4.5）同文；
  而 `07_noise_snr.md:22`（§3）把 `source_snr = SNR_s = F_hat_s/σ_F,s`（依赖源亮度）列为**另一个**独立对象。
- 现状 → 应为：现状 = 控制点值 = `source_snr`（逐源、随星的亮度变化，视场内跨 2–3 个数量级）
  → 应为 = `F_ref/σ_F(x,y)`（同一参考通量下的逐像素 SNR，随**局部噪声与源项**变化，不随"这颗星多亮"跳变）。
  后果：① 与帧级 SNR **不同口径**，违反 §2.2「二者口径相同（同一物理定义、同一参考通量 F_ref）」；
  ② 若按现设计算 `w = SNR²/F_ref²` 得 `F_i²/(σ_i²F_ref²)`，即把权重乘上 `(F_i/F_ref)²` —— 亮星附近过权、暗星附近欠权；
  ③ 与 `weight_chain.cpp:739` 的 `actual = frame_snr × intra` 复合后成为 **`frame_snr²·(逐源SNR)²/F_ref²`** 量级，
  双重计入。当前 ③ 不可达（AUD202-003 的 `sparse=nullptr`），属**潜伏缺陷**，一旦按 §8c 接入即触发。
- 改法：控制点值改在**参考通量**处求值：`SNR_c = F_ref/σ_F(x_c, ref profile)`（即对每个控制点用 `reference_flux_adu`
  与局部 `σ_sky` 调 `snr_source_snr_f64`，与 `snr_frame_science.cpp:189-201` 的参考轮廓同一构造），或
  显式把 `source_snr` 与 `sparse_snr_layer` 分成两条产物面并禁止后者进入权重链。
- 证据级别：A（合同 schema + 插件文档 + 最高设计三处同一口径）；本条不需要数值实验即可判口径不符。
- 置信：**CONFIRMED**（写入点逐锚核对；"当前不可达"部分为 PARTIAL — 潜伏）
- 复算命令或推导：
  `grep -n "p.flux_adu = F;" "…/snr_estimator.cpp"` → :80；`grep -n "snr_optimal = F /" "…/snr_science.cpp"` → :215。
  代数：`SNR_c^{prod} = F_i/σ_{F,i}`，合同要求 `= F_ref/σ_{F,c}`；两者比值 `= F_i σ_{F,c}/(F_ref σ_{F,i})` ≠ 1（`F_i` 逐星跨数量级）。

### AUD202-005 生产路径的 `sigma_sky_source` 被硬编码为 `EMPIRICAL_TOTAL_RMS`，设计"本意路径"（散粒+(RN/g)²）在生产不可达；增益自估主路径未实现

- 对象：`lib/infrastructure/scheduler/src/module_adapters.cpp:7159` → `      cfg.sigma_sky_source = SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS;`
  入参来源 `:7151` → `      cfg.sigma_sky_adu = src_frame->value("noise_sigma", 0.0);`（`StarDetector::estimate_background` 的整帧 2 轮裁剪 RMS，含读噪）
  增益来源 `:6794` → `    sci_cfg.gain_e_per_adu = sc.value("gain_e_per_adu", 0.0);`（**配置默认 0 ⇒ 未知**；
  `eng/contracts/schemas/phase_config_normalize.schema.json:229` 明文「缺失 = 未知 gain/ZP（天空受限最优提取，m_5 = null）」，
  且 `eng/packaging/config/defaults.json` 与出厂模板均**无** `snr.gain_e_per_adu` 默认值登记）
- 权威依据：`docs/science/NOISE_MODEL.md:223-242`（§5c「增益与读出噪声的来源」：第 1 条**本帧自估 `V = σ0² + S/g` 斜率–截距回归**是
  「本链在正式数据面上的**主路径**」，第 2 条帧头关键字只作交叉校验；`:239-240` 实测 M42 真实帧 124 个关键字中 `GAIN/RDNOISE/EGAIN` 全部缺失）；
  `docs/plugins/algorithms_phase1/07_noise_snr.md:118-122`（§4.2a 决策树：分支 1 = `shot_noise_only` 且 gain>0 才是"设计本意路径"）。
- 现状 → 应为：现状 = 生产只有两条可达臂 —— ① gain 未给（**默认**）⇒ 天空受限臂 `σ_F² = σ_sky²/ΣP_i²`，
  σ_F 里**既无源散粒项也无独立读噪项**；② 用户在 JSON 里手填 gain ⇒ 经验总 rms + 源项（读噪计一次，正确）。
  决策树分支 1（散粒口径 + `(RN/g)²`）从生产**不可达**（声明被写死）。
  → 应为 = 按 §5c 实现自估增益主路径（含杠杆臂可辨识性判据与"不可辨识 ⇒ 显式不可得"），
  并把 `sigma_sky_source` 的择一交回声明面（散粒来源存在时走分支 1），而非由接线层写死。
- 改法：① 实现 `V = σ0² + S/g` 回归（NOISE_MODEL §5c 第 1 条）并把结果连同来源与杠杆臂诊断写入 `snr` 配置面；
  ② 回归不可辨识时保持现状但**必须**按 §4.2a 分支 3 发布 `snr_caliber = upper_bound_no_gain`（已实现，见 `:7130-7136`，此点证真）；
  ③ 在 `07_noise_snr.md` §4.2a 决策树上如实标注"分支 1 生产不可达"或把它接上。
- 证据级别：A（NOISE_MODEL §5c 正向约束条款）+ 代码接线事实。
- 置信：**CONFIRMED**
- 复算命令或推导：`git grep -n "sigma_sky_source" -- lib/infrastructure/scheduler/src/module_adapters.cpp` → 唯一生产赋值 :7159；
  `git grep -n "gain_e_per_adu" -- eng/packaging/config eng/contracts/schemas` → 仅 schema 描述行，无默认值条目（四侧默认缺位形态）。

### AUD202-006 无增益上界的解析式 `√(1+F/σ_bg²)` 量纲不齐且数值偏 1.03–3.72 倍（文档缺陷）

- 对象：`docs/plugins/algorithms_phase1/07_noise_snr.md:121` →
  「…退回**天空受限**口径 `σ_F² = σ_sky²/ΣP_i²`…**SNR 系统性偏高**…（解析式
  `SNR_rep/SNR_true = √(1+F/σ_bg²)`；φ=0.5 偏高 41%、φ=0.95 偏高 347%）」
- 权威依据：由 `σ_F^{-2}=Σ P_i²/σ_i²`、`σ_i² = σ_bg² + F·P_i/g` 直接推导（本次独立推导）：
  `SNR_rep/SNR_true = sqrt( Σ P_i²/σ_bg² · Σ P_i²/(σ_bg²+F P_i/g)⁻¹⁻¹ )`，
  高源极限 `→ sqrt(F·ΣP²/(g σ_bg²))`；**必带 `ΣP²`（或 `P_i`）与 `1/g` 因子**才无量纲。
- 现状 → 应为：现状式 = `√(1 + F/σ_bg²)`：`F`[ADU]/`σ_bg²`[ADU²] = `ADU⁻¹`，**不是无量纲**，
  且缺 `ΣP²`、缺 `1/g`。→ 应为：给无量纲的精确比值式（或按上式给出逐 `ΣP²` 的渐近式），并把 φ 的定义写清（现文中 φ 无定义）。
- 改法：把该行替换为无量纲式并给出适用的 `ΣP²`/`g`；同时用实测倍数替换 41%/347% 两个锚点。
- 证据级别：本仓文档条目 + 本次独立推导复算（A 级公式为出发点）。
- 置信：**CONFIRMED**（量纲部分）；41%/347% 两个数值点 **UNPROVEN**（φ 未定义，无法复算其口径）
- 复算命令或推导：`aud202_recompute.py` → `R6_doc_analytic_formula`（σ_psf=1.5 一列）：
  | F_e | 精确 `SNR_rep/SNR_true` | 文档式 √(1+F/σ_bg²) | 文档式/精确 |
  |---|---|---|---|
  | 10 | 1.0036 | 1.0319 | 1.028 |
  | 100 | 1.0351 | 1.2839 | 1.240 |
  | 1000 | 1.2866 | 2.7357 | **2.126** |
  | 1e4 | 2.5504 | 8.1141 | **3.181** |
  | 1e5 | 7.0604 | 25.4829 | **3.609** |
  | 1e6 | 21.6647 | 80.5282 | **3.717** |

### AUD202-007 误差传播的实现漏协方差项：全信息核 `w_info_dense/low_rank` 存在但不在 SNR 生产链上

- 对象：
  - 生产 σ_F 的唯一求值点 `lib/algorithms/noise_snr/cpp/src/snr_science.cpp:202` → `            var_f += (Pi * Pi) / var_i;`
    （**逐像素对角**，`var_i` 只有一项，无协方差）；`:209` → `        var_f = (sum_p2 > 0.0) ? (sig_sky * sig_sky / sum_p2) : 0.0;`
  - 存在但未被 SNR 链调用的完整形式：`lib/algorithms/noise_snr/cpp/src/information_weight.cpp:128`
    `PointEstimate w_info_dense(const double* p, std::size_t m, const double* c, ...)`（Cholesky 解 `C⁻¹P`，正确）、
    `:140` `w_info_low_rank`（Woodbury，正确）；唯一生产调用点是 `lib/algorithms/integration/v6_phase1/src/phase1_product.cpp:396`
    `  const p1psfw::PointEstimate pe = p1psfw::w_info_diagonal(` —— **对角版**。
- 权威依据：`docs/science/NOISE_MODEL.md:131`（§5b 表"重采样相关"行：`var_out = Σ c_j² v_j`（对角）…
  **协方差非对角不落盘**）；`docs/science/UNCERTAINTY_AND_COVARIANCE.md:142`
  「⇒ 用产品 `variance` 做孔径/测量误差时，**必须显式加入协方差项**；`Σc_k²u_k` 标量式只在 `C_in` 对角时成立」；
  `ASTROCS_DESIGN.md:181` 与 `docs/science/PSF_SIGNAL_WEIGHT.md:28-33`（完整形式 `W = a²PᵀC⁻¹P`，白噪声近似**只在噪声白时**成立）。
- 现状 → 应为：现状 = 帧级 SNR / 稀疏控制点 / 深度 `m_5` 全部走对角（白噪声）形式，
  而 Phase1 产品是 drizzle 重采样后的 HEALPix 叶（相关长度 1–2 px，NOISE_MODEL §5b 实测 ρ₁ 列）
  ⇒ 落入 `07_noise_snr.md:130`「白噪声时为简单形式；**相关噪声时用完整信息核**」的"相关噪声"分支却没有用完整核。
  → 应为 = 相关噪声域用 `w_info_dense`/`w_info_low_rank`（已实现、零接入），并在产品登记所用核的形态。
- 改法：把 `snr_source_snr_f64` 的对角累加改为经 `CovarianceView` 走 `w_info_solve`（同一 TU 已提供
  `WhiteNoiseGate`：`information_weight.cpp:190`，`reason="non_diagonal_covariance"` 即回退完整核）；
  并在 SNR 产物的 provenance 写 `covariance_kind`。
- 适用域附注（同一条）：Horne 1986 原文本次核到标题为 *An optimal extraction algorithm for **CCD spectroscopy***
  （一维光谱抽取），本仓把它用于成像点源 PSF 测光属**推广**；且 `σ_i²` 内用**实测** `F`（`snr_science.cpp:199`
  `const double si = F * Pi;`）使权重依赖数据 ⇒ `Var=1/ΣP²/σ²` 为一阶近似，需在文档写明（现文未写）。
- 证据级别：A论文（Horne 1986 经 Crossref 核到标题/卷页/DOI）+ 代码接线事实。
- 置信：**CONFIRMED**（漏协方差 + 完整核零接入）；适用域外推部分 **PARTIAL**
- 复算命令或推导：`git grep -n "w_info_dense\|w_info_low_rank\|w_info_diagonal" -- '*.cpp' | grep -v information_weight`
  → 仅 `phase1_product.cpp:396`（diagonal）与测试命中；`dense/low_rank` 生产调用数 = 0。

### AUD202-008 「重建误差」= 节点复现残差，对插值型算子恒为 0（恒真判据）；合同要求的预测方差未实现；Δ=64 的密采论证与自身体实验结果相反

- 对象：
  - `docs/plugins/algorithms_phase2/13_integration.md:39` → 「实际生效算子标识与**重建误差**入 manifest
    （`SparseReconstruction.operator_id` / `node_reproduction_max_abs`）」
  - `lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h:167` → `  double node_reproduction_max_abs = 0.0;  /* 控制点自身复现最大绝对残差；应 ~0 */`
    （实现 `weight_chain.cpp:544` `  node_residual_ = max_resid;`）
  - 合同要求：`docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:99` → 「消费时由控制点**直接重建**为稠密 SNR 场
    `SNR(p)`（重建算子显式声明、在控制点处精确复现节点值、并**返回预测方差**）」
  - 间隔论证：`eng/packaging/config/defaults.json:694`（`sparse_snr.spacing_px` constraint）→
    「默认 64 = hips.tile_width / 8 = 512 / 8…实测 SNR 场相关长度 ℓ = 40.4–57.9 px 区间 ⇒ **Δ/ℓ ≈ 1.1–1.6（够密）**」
- 权威依据：`docs/plugins/algorithms_phase1/07_noise_snr.md:193`（§4.5 算子四约束第 ③ 条
  「在控制点处**精确复现**节点值」是**设计约束**）；同文件 `:102`（§4.5 实测：默认算子 Δ\*=32 px，生产 Δ=64 在 HST 类域
  比"退化成帧级"差 2.6 倍）；`docs/science/CONTROL_WEIGHT_SNR.md:215`（§8b「生产 Δ=64 落在前两者的**失效区**内」）。
- 现状 → 应为：
  ① 「重建误差」= 节点处残差，而算子被约束为**必须**在节点精确复现 ⇒ 该量对三个插值算子**恒等于 0**（构造性质，非测量结果）
  ⇒ 它**不携带任何关于节点间插值误差的信息**，把它登记为"重建误差"是恒真判据；
  → 应为 = 落盘真正的**预测方差** `Var[SNR̂(x,y)]`（对插值核算传播 `Var = Σ a_k(x,y)² Var(SNR_k)`，
  其中 `a_k` 是算子的节点权重；样条核的 `Σa_k²` 随点到节点距离变化，可给出逐像素不确定度）—— DATA-002:99 已要求，未实现。
  ② Δ 的"够密"论证不成立：`Δ/ℓ = 1.1–1.6 > 1` 是**欠采**表述（间隔大于相关长度），
  与该仓自身体实测（Δ\* = 32 px 起失效）直接冲突；把 `Δ = tile_width/8` 写成"复用 UPM 控制网格"
  是**几何便利**而非**由 SNR 场自身的空间尺度导出**。→ 应为 = 按输入几何与 SNR 场相关长度导出上界（`Δ ≤ ℓ/2` 一类可陈述的采样判据），
  并把 HST 类域的 32 px 推荐变成按域生效的规则而非建议。
- 改法：① 新增 `prediction_variance` 落盘与门（对无源/平坦控制网格须非零 ⇒ 能红）；
  ② 把 `node_reproduction_max_abs` 从"重建误差"改名为"节点复现自检（构造恒 0，不作精度证据）"；
  ③ 订正 defaults.json 的 Δ 论证文字，按 §8b 把 Δ 与算子/数据来源绑定。
- 证据级别：本仓文档/合同互斥比对 + 独立推导（插值算子节点复现 ⇒ 残差恒 0 是解析结论）。
- 置信：**CONFIRMED**
- 复算命令或推导：`git grep -n "node_reproduction_max_abs" | wc -l` → **9**（其中 **0** 处作为精度门被断言；
  `git grep -n "snr_path_effective" -- lib | wc -l` → **0**，全库 27 处命中全在 docs/ledger/ACCEPTANCE）；
  解析：算子约束 ③ ⇒ `R[v]_at node = v_node` ⇒ `node_residual ≡ 0`（与实现质量无关）。


### AUD202-009 三条 SNR 重建口径在生产不可选、无共同生产者，且被引为定案依据的实验测的是**另一组对象**（σ 场估计器，非 SNR 场）

- 对象：
  - 配置键是死键：`eng/ci/ledgers/dead_config_keys.json` → `{"id": "dead_config_key:snr_path", "kind": "dead_key", ...}`；
    `lib/infrastructure/cli/parser.cpp:353` → `        // 同 snr_path：CLI 只识别并透传，生产消费点未落地 ⇒ 死键台账已登记。`
  - `dense` 口径的 Phase1 稠密 SNR 面生产者：`lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:130`
    `SNR_API int snr_estimate(const float* data, int h, int w,` 与 `:304` `snr_estimate_f64` ——
    **生产调用点 0 处**（`git grep -n "snr_estimate_f64\|snr_estimate(" -- '*.cpp' | grep -v "noise_snr/cpp\|tests/\|/test/"` → 空）；
  - `frame_reconstruct` 与 `sparse_reconstruct` 在权重链里同值（`in.sparse = nullptr`，见 AUD202-003）；
  - 被引为"三口径适用域"定案依据的实验：`实验/absolute-snr/code/b3_domain_map.py:111` →
    `    est_dense = C.sigma_field_fast(img0, P_PATCH)`，`:139` → `        for name, est in [("sparse", est_sparse), ("dense", f32),`；
    `实验/absolute-snr/docs/EXP-04-RECONSTRUCTION.md:96-97` → 「**非算子臂**（对照）：`dense_P32`（稠密口径，P=32 patch 稳健 **σ**，块常数展开）、
    `dense_at_D`…、`frame_median`（帧级标量 = **控制值中位数**…）」
- 权威依据：`ASTROCS_DESIGN.md:398-403`（§5.3：三种口径由 JSON 显式指定…三者都产出同一物理量 `SNR = F_ref/σ_F` 的稠密表示；
  实际生效口径记录在 `snr_path_effective`）；`docs/science/CONTROL_WEIGHT_SNR.md:205-222`（§8b 把该图谱列为"SCI-B 定案；选型依据"）；
  `docs/plugins/algorithms_phase1/07_noise_snr.md:96`（§4.2「适用域由实验判定…完整适用域图谱由 `实验/absolute-snr` 给出」）。
- 现状 → 应为：
  ① 实验三臂 = {patch 稳健 σ 场 / 稀疏控制 σ 场 / 控制值中位数 σ}，度量的是「哪个 σ 场估计器更能恢复真值 σ」；
  被引用的三口径 = {稠密 SNR 面 / 稀疏绝对 SNR 控制点重建 / 帧级 SNR 标量}。**两组对象的分子不同**：
  实验臂全为分母侧 σ 估计器，口径臂含 SNR（分子为源项或 `F_ref`）。特别地 `frame_median`（控制值中位数）
  ≠ 生产 `frame_reconstruct`（`F_ref/σ_F(median-FWHM 参考轮廓)`，`snr_frame_science.cpp:189-201`）。
  ⇒ §8b 表格的"结论"**不覆盖**它被用来支撑的对象，属"判据断言的对象与缺陷所在的对象不是同一个"的形态；
  ② 三条口径**当前无一可选**（键为死键）、`dense` 无生产调用者、`sparse`/`frame` 在权重链里同值 ⇒ 三口径并存只存在于文档面；
  ③ `snr_path_effective`（设计要求记录实际生效口径）在代码与产品里 **0 处实现**：
  `git grep -n "snr_path_effective" | wc -l` → **27，全部落在 docs/ledger/ACCEPTANCE，零落在 lib/**（分域计数见下）。
  且三条口径因 AUD202-004 的分子口径不同（逐源 `F_i` vs `F_ref`）**本就不同物理量**。
- 改法：① 为三臂实验补一组**以 `F_ref` 为分子**的 SNR 场臂，或把 §8b 的适用域结论显式改挂到"σ 场估计器选型"这个真实对象上；
  ② 把 `snr_path` 从死键升为在架键并写 `snr_path_effective`；③ 给 `dense` 路径一个生产者或在文档撤销该口径。
- 证据级别：A（最高设计 §5.3 条款）+ 代码/台账接线事实 + B（实验脚本对象面读码核对，未重跑）。
- 置信：**CONFIRMED**（对象错配与三口径不可达）；三臂数值本身 **未复算**（依赖 C++ 稠密 SNR 面，见需复测清单 R-2）
- 复算命令或推导：`git grep -c "snr_path_effective" -- lib` → 无输出（0 命中）；`git grep -n "snr_path_effective" | cut -d: -f1 | sort -u`
  → 27 命中全在 `ACCEPTANCE_SPEC.md / ASTROCS_DESIGN.md / docs/* / eng/ci/ledgers/*`；
  `git grep -n "snr_estimate" -- '*.cpp' ':!lib/algorithms/noise_snr' ':!*/tests/*' ':!*/test/*'` → 空。对象错配为读码定义比对。

### AUD202-010 「不入库权重」被实质破坏：Phase1 落盘 ivar、Phase2 原样用作叠加权重；两篇权威文档在此直接打架

- 对象：
  - 落盘：`lib/infrastructure/scheduler/src/module_adapters.cpp:8156` → `                frame, "variance", AIO_BLOCK_FLOAT32, var_plane.data(),`
    （随帧 HiPS 发布 `variance`/`ivar` 子产品，见 `:8805` `    if (variance_products_present) { products.push_back("variance"); products.push_back("ivar"); }`）
  - 消费不换算：同文件 `:11937` → `            w = static_cast<double>(ivar_v[d][static_cast<size_t>(p)]);`（**赋值即权重**，无 `SNR²/F_ref²` 换算步骤）
  - 打架条款：`docs/science/NOISE_MODEL.md:328` → 「**权重归一与适用域**：`ivar` 作为 Phase2 逐像素科学权重直接入加权（归一在消费侧 `Σw/Σ`），适用域=空背景随机分量…」
    vs `ASTROCS_DESIGN.md:156-158, 171-174`（权重必须含源项）与 `docs/science/CONTROL_WEIGHT_SNR.md:229-237`（§8c「定权路径（唯一，无分支）：所有重建路径产出的稠密 SNR 场都走同一式取逆方差」）；
    `docs/plugins/algorithms_phase1/07_noise_snr.md:201`（§4.6「权重不落盘：HiPS 里只存帧级 SNR 与稀疏**绝对** SNR，权重由 Phase2 集成时现场派生」）。
- 权威依据：同上（两条互斥条款即为证据）。
- 现状 → 应为：现状 = ①「权重只在 mosaic 现场换算、不入库」的红线在**实质**上被破坏 —— 权重由 Phase1 产出的
  `ivar` 面直接承担（只是换了个名字：`ivar = 1/variance` 就是权重本身，落盘即入库）；
  ② `NOISE_MODEL.md` §9a 明文允许，与最高设计/SCI-CW 冲突 ⇒ 这不是实现偏离，是**规范层自相矛盾**。
  → 应为 = 二选一并回写权威链：(A) 依最高设计 —— Phase1 只入库**含源项的加权方差面**（或 SNR），背景 `ivar` 降为诊断量不入库；
  (B) 依 NOISE_MODEL —— 承认背景 ivar 可作权重，则须撤销 §2.2 的"必须含源项"正向约束并订正 SCI-CW §2a。**此项属 AGENTS §10「两篇权威文档打架」，需上呈负责人裁决。**
- 改法：见 (A)/(B)；无论哪条，`:11937` 的直接赋值处必须写明用的是哪一面并落 provenance 键（现无 `weight_variance_surface` 一类具名登记）。
- 证据级别：A（两份 FROZEN 科学文档条款逐字对照）+ 代码写入/消费点。
- 置信：**CONFIRMED**（冲突存在与消费点）；正确取值方向 **UNPROVEN → 上呈裁决**
- 复算命令或推导：`git grep -n "ivar 作为 Phase2 逐像素科学权重" docs/science/NOISE_MODEL.md` → :328；
  `git grep -n "权重不落盘" docs/plugins/algorithms_phase1/07_noise_snr.md` → :201。两行语义互斥，无中间限定。

### AUD202-011 噪声组成单位与读噪单次计入：独立推导 + 独立 MC 复算成立（证真条目）

- 对象：`lib/algorithms/noise_snr/cpp/src/snr_science.cpp:200-202` →
  `            double var_i = sig_sky * sig_sky + rn_term;` / `            if (si > 0.0) var_i += si / p->gain_e_per_adu;` /
  `            var_f += (Pi * Pi) / var_i;`；`rn_term` 受 `:177` `        const bool rn_in_sky = (p->sigma_sky_source == SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS);` 门控
- 权威依据：Horne 1986 PASP 98 609（本次 Crossref 核到标题/卷页/DOI）；`docs/science/NOISE_MODEL.md:324`（§9a「逐像素噪声组合
  `σ_i² = σ_sky,i² + (RN/g)² + F·P_i/g`，**读噪只出现一次**」）；`docs/plugins/algorithms_phase1/07_noise_snr.md:110`（§4.2a 唯一口径规则）。
- 独立推导（单位）：`σ_sky²`[ADU²]；`(RN/g)²` = (e⁻/(e⁻·ADU⁻¹))² = ADU²；
  Poisson：像素电子数 `N_i = F·P_i·g`[e⁻]、方差同值[e⁻²]、除 `g²` 得 ADU²，即 `F·P_i/g`[ADU²] ✓ 三项同为 ADU²；
  `Σ P_i²/σ_i²` 单位 ADU⁻² ⇒ `σ_F = (ΣP²/σ²)^{-1/2}` [ADU] ✓，`SNR = F/σ_F` 无量纲 ✓。
  归一化约定：`snr_science.cpp:198` `const double Pi = v[k] / sum;` ⇒ `Σ_i P_i = 1` 在**截断网格**上成立（非全空域），
  因而 `σ_F` 是"该网格内通量"的不确定度；`ΣP²` 随 `half` 收敛极快（本次复算 `half=12→60`：`0.09268653→0.09268408`，
  第 5 位起不动）⇒ 截断不构成误差源，且 `P_i` 归一化约定与 Horne 的 `P_i`（轮廓、和为 1）一致。**未发现重复计入。**
  暗电流无独立项：靠"经验总 rms 吸收"（NOISE_MODEL:123-126 表列"隐含"）；若在 `shot_noise_only` 声明下喂入含读噪的经验 rms 即成双计，
  现由写死 `EMPIRICAL_TOTAL_RMS`（AUD202-005）与 `p1snr_science_skysource` 负例共同挡住。
- 现状 → 应为：成立（现状 = 应为）。
- 证据级别：A论文 + B本仓实验 + **本次独立复算**（自有 MC，不 import 仓库代码）。
- 置信：**CONFIRMED（成立）**
- 复算命令或推导：`PYTHONDONTWRITEBYTECODE=1 python -B aud202_recompute.py` → `R1_sigma_f_vs_mc`：
  基准点（F=1000 e⁻, B=100 e⁻/px, D=0.5, RN=10, g=1.3）`σ_F(def)=46.0332` vs 自有 4000 次 MC `45.8528`，**z=+0.35**；
  独立换源点（F=417 e⁻, σ_psf=2.3, B=39, D=7, RN=3.4, g=2.7）`σ_F=16.8812` vs `17.2179`，**z=−1.75**（两组均 ≤3σ，且偏差非零：+0.39% / −1.96%）。
  负例（本代理自己引入的读噪双计）同门报 **z=13.37**（`run1.log`），证明该门能红。
  `R5_degeneracy_selftest`：把天光散粒项冻住（`σ_sky` 不随 B 变）⇒ `SNR(10⁶)/SNR(0) = 1.000000`、log-log 斜率 = 1.0e-17
  （真效应存在时 −0.4931/−0.49996）⇒ 天光单调性判据**有判别力**。
  另：`R2_sky_monotonicity` 亮源 `SNR(10⁶)/SNR(0)=12.76%`、暗源 `1.16%`；传统"信号含天光"口径在暗源上随天光升 **×38.8**，
  与 `CONTROL_WEIGHT_SNR.md:198` 方向一致。生产 `dense` 路径的 IDW 保真性质另见需复测清单 R-1。

### AUD202-012 双计偏差头条数字取自单次 MC 实现值：同一物理点沿两条扫描轴相差 3.1pp，且与解析值偏离至 4.3pp（该偏离被算出却无门约束）

- 对象：
  - `docs/plugins/algorithms_phase1/07_noise_snr.md:123` → 「双计使 `σ_F` 高估 **+12.8%**（基准点 F=1000 e⁻、B=100 e⁻/px、RN=10 e⁻、g=1.3、D=0.5）至 **+34.0%**（RN=50 e⁻ 最坏点）」
  - 同一数字复读：`docs/science/CONTROL_WEIGHT_SNR.md:201`、`docs/science/NOISE_MODEL.md:324`（均 "+12.8%~+34.0%"）、`docs/science/PSF_SIGNAL_WEIGHT.md` §7a
  - 取值来源：`实验/absolute-snr/code/b2_noise_terms.py:184` → `            row["bias_empirical_rn"] = float(row["sigma_f_empirical_rn_adu"][0] / row["sigma_f_mc_adu"] - 1.0)`
    （分母 = **该 seed 的 MC 经验散度**；分子 = 双计臂均值），而解析式在 `:105` `pred_doublecount_bias=float(sigma_f_from(v_emp) / sigma_f_from(v_corr) - 1.0)`
- 权威依据：`standards/02_科学证据分级与三重佐证标准.md` §1「证据记录到…可由独立读者复核」、§3「每个科学量写清…精度要求」。
- 现状 → 应为：
  | 量 | 解析值（本次独立复算逐位吻合） | 归档头条（MC 实现值） | 差 |
  |---|---|---|---|
  | 基准点双计偏差 | **+14.500914%**（`pred=0.14500914320209946`；本次复算 14.501%，`ΣP²=0.09268409` 逐位一致） | +12.791%（`source_flux_e` 轴）／ **+15.922%**（`read_noise_e` 轴，同一物理点） | −1.71pp／+1.42pp |
  | RN=50 最坏点 | +38.252% | +33.954% | −4.30pp |

  即：① 同一物理点沿两条轴给出两个不同"基准点值"（seed 各自独立），文档只取其一；
  ② 全部点的 `max|pred − measured| = 0.0700`（`G4c_pred_vs_measured_mc_max_abs_diff`，7.0pp）**被算出但无门约束**；
  ③ 双计臂自身 `z_empirical_rn_vs_mc` 最坏 **+15.17σ**（n=29）——这本是该臂应红，但同一 MC 分母的抽样噪声未随头条数字一并告示。
  → 应为 = 文档头条改用**解析式**值（+14.5% / +38.3%，seed 无关、可独立复核），或在数字旁标注其 MC 抽样噪声与所取轴；
  并把 `G4c_pred_vs_measured_mc_max_abs_diff` 从"记录量"升为带阈值的门或显式声明其无门。
- 改法：改 4 处文档引用为解析值并注 `pred_doublecount_bias`；`b2_noise_terms.py` 的门补 `pred vs measured` 容差条目。
- 证据级别：B本仓实验（已归档 JSON 逐字段回读）+ 本次独立复算对解析值的逐位吻合。
- 置信：**CONFIRMED**（数字归因面；缺陷方向与量级仍成立，故 AUD202-011 的定性结论不受影响）
- 复算命令或推导：回读 `实验/absolute-snr/results/b2_noise_terms.json`：
  `scans.source_flux_e[F=1000].bias_empirical_rn=0.127913`, `.pred_doublecount_bias=0.145009`;
  `scans.read_noise_e[RN=10].bias_empirical_rn=0.159217`, `.pred_doublecount_bias=0.145009`;
  `scans.read_noise_e[RN=50].bias_empirical_rn=0.339542`, `.pred_doublecount_bias=0.382519`;
  `gates.G4c_pred_vs_measured_mc_max_abs_diff=0.070030`。本代理 `sens.py`（同目录）对 `half=12..60` 复核 `pred` 恒 14.501% ⇒ 纯解析量，与网格无关。


## 4. 判据判别力表（每条问"注入什么偏差它会红"）

| 判据 | 位置 | 注入什么偏差会红 | 结论 |
|---|---|---|---|
| 读噪双计保护 `p1snr_science_skysource` | `lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp:392-396` → `check(std::fabs(zB) > 3.0, "double-count arm REJECTED by MC …")` | 把 `sigma_sky_source` 声明改成与来源不符（总 rms 再加 `(RN/g)²`）⇒ zB>3 红；本代理独立 MC 复现同形偏差 z=13.37 | **有判别力**（真绿真红双向，已本次独立复算） |
| 天光单调性 / log-log 斜率 | `实验/absolute-snr/results/b1_sky_scan.json::gates_bright/gates_faint`；`docs/plugins/algorithms_phase1/07_noise_snr.md:248` | 冻住 `σ_sky`（不随 B 变）⇒ 比值 = 1.000000、斜率 1.0e-17，与 −1/2 判据不符 ⇒ 红 | **有判别力**（`R5_degeneracy_selftest` 实测） |
| 权重换算恒等式 `H1/H5_identity_lt_1e-12` | `实验/absolute-snr/code/b4_integration.py`（`gates.H1_identity_rel_dev=2.22e-16`） | 分母由 `F_ref` 换成逐帧检出中位数、或幂次由 2 改 1 ⇒ 偏差立刻 ≫1e-12 ⇒ 红 | **有判别力但恒真风险低**（本次 R3 用四组异源参数复算，最大 3.01e-16，仍为机器精度 ⇒ 它是代数恒等式，**只保证换算代码不出错，不保证 SNR 分子取的是 F_ref**——见 AUD202-004 的口径漏洞正好从这条门下通过） |
| 三口径"重建误差" `node_reproduction_max_abs ≈ 0` | `lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h:167`；`docs/plugins/algorithms_phase2/13_integration.md:39` | **注入任何节点间插值偏差都不会红**（插值算子按约束 ③ 必在节点精确复现） | **恒真判据 → 立案**（AUD202-008） |
| 稀疏层"绝对语义"复现门 | `docs/plugins/algorithms_phase1/07_noise_snr.md:252`（§8 三态 ①②③） | 把控制值改成相对因子 ⇒ 节点复现仍逐位成立（相对场也是绝对数的插值），**只有 `sparse_snr_semantics` 键声明面能红** | **断言对象错位**：该门测的是"乘没乘帧级标量"，测不到"分子用 `F_i` 还是 `F_ref`"（AUD202-004 恰好通过） |
| 权重链语义门 `kFluxTypeUnweightedSnr` | `lib/algorithms/integration/v6/src/weight_chain.cpp:695` | 把 `kind` 填成相对质量权或 median 诊断 ⇒ 红 | **有判别力**，但 `kind` 由**调用方自填**（`module_adapters.cpp:11531` 硬编码 `in.kind = FrameSnrKind::kFluxTypeUnweightedSnr;`）⇒ 生产恒绿，对象校验缺位（登记为 PARTIAL） |
| SNR 生产控制点等值门 `checkClose(pts[i].snr_psf, e.snr_optimal, 1e-12)` | `p1snr_science_test.cpp:424` | oracle `refCompute` 是**同式 long-double 镜像** ⇒ 换成 `F_ref` 分子才会红；任何"分子口径"错误都不发红 | **测的是自洽性不是正确性**；锁死了 AUD202-004 的错误口径 ⇒ 立案 |
| 帧级 SNR `snr_phot == median(SNR_F)` | 同文件 `:435-437` | 若帧级 SNR 改成参考轮廓口径（`reference_snr_f`）⇒ 该门红 | **恒真风险：把帧级 SNR 钉在逐源中位数上**，与 HiPS 头实际写的 `snr_reference.snr_f`（参考轮廓）**是两个不同数**（`astro_sphere_sink.cpp:453-454`），门测的是 C 函数内部一致性，不覆盖产品面 |

## 5. 三类数据结果（本分片可得的档位）

| 数据类 | 设置 | 关键结果数字 | 判定 |
|---|---|---|---|
| ① 物理仿真成像（电子域 Poisson + 读噪 + 增益） | 本代理自有 4000 次 MC，两组参数（基准点 + 独立换源点），`aud202_recompute.py::R1` | `σ_F` 定义式 vs MC：z=+0.35 / −1.75；偏差 +0.39% / −1.96% | **绿**（定义式本身成立） |
| ② 代数合成（解析恒等式与量纲） | 四组 (g, RN, sky, F) 异源参数，`R3`/`R6`/`sens.py` | 权重恒等式 3.01e-16；`ΣP²` 网格无关（14.501% 稳定）；文档解析式偏 1.03–3.72 倍 | 恒等式**绿**；`07_noise_snr.md:121` 解析式**红**（AUD202-006） |
| ③ 真实测试数据（M42/银心实拍帧） | **本机不可达**：不跑 C++、不跑端到端；`testdata/` 的归约产品需重跑 | — | **未做 → 需复测 R-1** |
| 归档 B 级证据回读 | `b1/b2/b4` JSON 逐字段 | `G1_def_max_abs_z=2.681 (n=29)`；`G4c_bias_at_base=0.12791` vs `pred=0.145009`；`H1_identity_rel_dev=2.22e-16` | 与文档引用一致（除 AUD202-012 的归因面） |

## 覆盖率自报

应做范围与实读计数（计数命令逐条给出，工作树 = `c8f64e9a`，全部按 **git 跟踪** 计数，未跟踪的本机残留不计）：

| 目录 | 跟踪文件数（命令给出） | 本代理通读程度 |
|---|---|---|
| `lib/algorithms/noise_snr/` | 41 | **代码面 8/8 个 .cpp/.h 全文读**：`snr_science.cpp`(282)、`information_weight.cpp`(320)、`snr_estimator.cpp`(932)、`noise_model.cpp`(1482，**分段读，未逐行**)、`snr_frame_science.cpp`(215)、`snr_estimator.h`、`module_entry.cpp`、测试 `p1snr_science_test.cpp`(**:74-95,:165-168,:380-470 读，其余未逐行**)；`test/noise_model_science_test.cpp`、`test/snr_reconcile_test.cpp`、`p1noise_*`（10 文件）、`include/astrocs/*`(4)、`CMakeLists/README/memory.md/module.yaml/def/map`（17 非源码）**未通读** |
| `lib/algorithms/psf/` | 27 | 计数与 SNR/权重消费点扫描已做（`git grep -n "snr\|ivar\|variance\|weight" -- lib/algorithms/psf/src/psf_information.cpp lib/algorithms/psf/include/astrocs/v6/psf_information.h` → 仅 1 命中 = A_NEA 单位声明）⇒ **该模块不消费 SNR/权重，与本分片口径面无交集**；27 文件未逐个通读（主动降级，理由如上） |
| `lib/algorithms/integration/`（消费权重处） | 22 | 全文读：`weight_chain.h`(341)、`weight_chain.cpp:640-819`；`weight_chain_selfcheck.cpp`、`phase2_integrate.cpp`、`variance_propagation.*`、`recon_dump.cpp`、`phase1_product.cpp` 仅**定点读**（调用面/权重面命中行）⇒ 22 文件中 4 全文 |
| `lib/algorithms/rejection/` | **4（全为 .md/.yaml，无 .cpp/.h）** | 真实排异实现不在该目录（在 `lib/algorithms/coverage/src/rejection.cpp`，属 AUD-202 声明范围外）；本分片仅核对 SNR 相关处：`coverage/src/stage2_common.cpp:431-440` 权重枚举禁用（引 `CONTROL_WEIGHT_SNR.md:162-163`）⇒ **未通读排异代码** |
| `实验/absolute-snr/` | 196 | **未逐文件通读**。已读：`code/b2_noise_terms.py`(全文 242 行)、`code/sci_b_common.py`(定点：`moffat4_grid/horne_extract/simulate_stamps/prod_mirror_snr/robust_sigma` 定义)、`code/b3_domain_map.py`(定点 :111/:139)、`results/b1_sky_scan.json`/`b2_noise_terms.json`/`b4_integration.json`(gate 字段与 base-point 回读)、`docs/EXP-04-RECONSTRUCTION.md`(:38,:69,:96-97,:164-165,:227,:378-408,:431,:605)；未读：`README.md`/`REPORT_paper.md`/`B7_*.md`/`code/b5`,`b6`,`b7*`,`exp01-06`,`reverse_verify`,`prod_snr_driver.cpp`,`make_*`，`results/` 其余 30+ JSON，`data/` ⇒ 覆盖率约 **10/196 文件级**，结论层面已覆盖 AUD-202 引用的全部数字锚点（§8a/§8b 引用的 b1/b2/b3/b4/b6 与 EXP-03/04/05） |
| `实验/shared/` | 46 | **未通读**（本分片未用到其数据；`sci_b_common.py` 自带合成器，不依赖 shared） |
| `eng/contracts/` snr/weight/ivar 面 | 21 | 全文读 `unified/sparse_snr_layer.schema.json`、`unified/ivar.schema.json`；`frame_snr`/`source_snr`/`depth_m5`/`point_information`/`variance` schema、`negative/*`、`examples/*`、`product_family_field_constraints` **未逐个通读** |
| 生产接线（超出声明目录，为闭合链路口径必读） | — | `module_adapters.cpp` 定点读 :6643-6815, :6844-6928, :7100-7363, :7960-8090, :11455-11975；`astro_sphere_sink.cpp:430-475`；`parser.cpp:331-360`；`dead_config_keys.json` |

**自报的覆盖缺口（不构成完成声明）**：`noise_model.cpp` 1482 行未逐行（只核 §5d 审计面与 fill 语义相关段）；`实验/absolute-snr` 的文件级覆盖率 ~5%，但 AUD-202 结论所依赖的数字锚点已 100% 回读并交叉复算；`psf/`、`rejection/` 以"与 SNR/权重口径面无交集"的定点扫描替代通读，该判断本身若错会漏掉缺陷。

## 我证伪了任务书的哪个前提

任务书给了 6 条必须逐条核验的前提，其中 **3 条被本次核查证伪或修正**：

1. **证伪："`lib/algorithms/rejection/` 与 SNR 相关处" 作为代码目录存在。**
   命令：`git ls-files -- lib/algorithms/rejection | wc -l` → **4**，且
   `find lib/algorithms/rejection -type f` 显示全部为 `CLASSIFY_V1_PROFILE.md / README.md / memory.md / module.yaml`，**无一个 .cpp/.h**。
   实际排异实现在 `lib/algorithms/coverage/src/rejection.cpp`（+ `coverage/tests/rejection_nonfinite_weights_test.cpp`、
   `coverage/tools/rejection_cli.cpp`），不在本分片声明的代码域内。⇒ 分片的文件域切分与仓库实际布局不符；
   按现范围做"排异与 SNR 相关处"的核验**结构上不可能完成**，需把 `lib/algorithms/coverage/` 的排异文件显式划入或单列。
2. **证伪（部分）："`w = SNR²/F_ref² = 1/σ_F²` 是否在 mosaic 现场换算" 这个提问预设了换算发生在 mosaic。**
   事实是 mosaic 现场**根本没有做这个换算**：逐像素路径直接 `w = ivar`（`module_adapters.cpp:11937`），
   帧级路径做的是 `w = (frame_snr × intra)²/F_ref²`（`weight_chain.cpp:739,779`，且 `intra≡1`）。
   即"现场换算"只在函数签名层存在（`weight_from_snr` 确实算 `SNR²/F_ref²`），**在生产数据面它不是权重的来源**。
   提问预设的"入库=未加权 SNR、权重现场算"两分法在现状下不成立（AUD202-001/003/010）。
3. **修正："`sparse_snr_layer` 与帧级 SNR 是否同口径同 `F_ref`" 提问预设两者至多一个不一致。**
   事实是**两者都不是 `F_ref/σ_F` 的同一实现**：帧级 = `reference_snr_f`（`snr_frame_science.cpp:200`，分子取 `F_ref` ✓ 同口径），
   稀疏层 = 逐源 `F_i/σ_{F,i}`（`snr_estimator.cpp:80,90`，分子取**该星自身通量** ✗）。
   故"不同口径"不是偏差量级问题而是**对象不同**（`source_snr` 冒充 `sparse_snr_layer`），见 AUD202-004。
4. 未证伪的：(a) 噪声组成单位一致性与读噪单次计入 —— 在已核到的两条臂上**成立**（AUD202-011）；
   (g) 天光单调性在实现（公式层）成立且判据有判别力；Zackay & Ofek I/II 绑定**未出现任务书担心的绑反形态**（AUD202-002）。

## 诚实边界（含需复测清单）

**适用域**

- 本报告全部结论落在**跨帧绝对 SNR 链（创新点二）的公式、口径、接线与判据面**。测光零点 `k_photo`/`ZP_syn` 的求取（AUD-201）、
  UPM 平面拟合（AUD-203）、面积交叠（AUD-204）不在本报告判据内，但 `F_ref,k = 10^(−0.4(m_ref−ZP_k))` 的数值正确性**依赖 AUD-201 的 ZP**，本报告只核到"F_ref 逐帧、同帧配对、锚定固定星等"这一**结构**（`module_adapters.cpp:7168-7182`）。
- Horne 1986 的一手适用域是**一维 CCD 光谱最优抽取**（Crossref 标题即 "…for CCD spectroscopy"）；
  本仓把它用于**成像点源 PSF 测光**是推广，且其 `Var=1/ΣP²/σ²` 形式**只在噪声白（C 对角）时成立**；
  本仓在 drizzle 重采样后的相关噪声域仍用对角形式（AUD202-007），这是本报告判为偏离的根据。
- Zackay & Ofek **II** 的标题限定 = background-dominated noise limit；报告把它引作"任意用途最优"的支撑时已属外推（AUD202-002 后半）。

**反例与已知偏差方向**

- AUD202-001 的方向确定：以背景 ivar 定权 ⇒ **亮源像素被系统性过权**（本次独立复算给 ×1.01→×1058 随 F 单调增），
  叠加结果向亮帧的亮源像素偏移，面亮度不受影响、**点源测光与 SNR 产品受影响**。
- AUD202-005/006 的方向确定：无增益 ⇒ `σ_F` 缺源项 ⇒ 上报 SNR **单调偏高**、权重偏高，且偏差随源亮度无界增长（×21.7 @10⁶ e⁻）；
  帧级 `SNR(F_ref)` 用 `F_ref` 而非真实源亮度 ⇒ 该偏差以参考星等档计量，不随帧内具体源变化，方向仍为偏高。
- AUD202-004 若被接入：`w ∝ (frame_snr·F_i/σ_{F,i})²/F_ref²` ⇒ 权重被乘上 `(F_i/F_ref)²`，
  方向 = 亮星邻域过权、暗星邻域欠权，且**逐帧再乘 `frame_snr²`** ⇒ 跨帧权重比被 `(frame_snr_k)²` 污染。
- 本报告未证伪本链的**代数核心**：`σ_F^{-2}=ΣP²/σ_i²` 的实现、`w=SNR²/F_ref²` 恒等、天光单调趋零，三条均经独立复算成立（AUD202-011、R1/R2/R3）。

**未验证部分（不靠重跑 C++ 就无法立证的，全部登记为需复测）**

| 编号 | 缺什么 | 为什么本机不能判 | 建议复测命令/口径 |
|---|---|---|---|
| R-1 | 生产端到端**实际用的是哪条权臂**（背景 ivar / 帧级 SNR 常量 / corrected variance） | 取决于 `ivar` 子产品是否真的在盘上：`07_noise_snr.md:138`（§4.4）声明 scheduler 未挂 variance 块 ⇒ 恒走帧级常量；`module_adapters.cpp:7880-8230` 又显示 variance 块已在 drizzle 节点挂载并 fail-closed 把关。**两条互斥，只能由实跑产品裁决** | 跑一轮 normalize 后 `git`-外的产品面检查：`<frame>/p1_final.json#products` 是否含 `variance`/`ivar`；再跑 mosaic 读 `p2_integrated.json` 的 `weight_basis`/`weight_source`/`uncertainty_unavailable_reason` 三键即判 |
| R-2 | 三条 SNR 重建口径的**互差是否真有实测或推导支撑**（任务书 (e)） | 归档 `b3_domain_map`/EXP-04 测的是 σ 场估计器（AUD202-009 ①），要判"三臂产出同一物理量"必须以生产 SNR 面为臂重跑；本机不跑 C++ | 需 `dense`（当前无生产者，先补）/ `sparse_reconstruct` / `frame_reconstruct` 三臂在同一输入上逐像素对拍，报 `max|SNR_a/SNR_b − 1|`；`实验/absolute-snr/code/b7_*` 已有驱动雏形但对象仍是 σ |
| R-3 | `noise_model.cpp`（1482 行）内 `variance` 面是否在任何分支被写入源项 | 只做了 §5d/§9 相关段与调用点定点读，未逐行 | 全文读 + `git grep -n "gain" lib/algorithms/noise_snr/cpp/src/noise_model.cpp` 逐命中判分支（本代理未完成） |
| R-4 | 归档实验 196 文件中未读的 ~186 个是否藏着与本链结论相反的口径 | 文件级覆盖率 ~5%；已回读全部被文档引用的数字锚点，但 `results/exp05_*`、`reverse_verify/`、`code/b7_*` 未读 | 以 `git grep` 在 `实验/absolute-snr/results/` 全量搜 `F_ref`/`frame_snr` 的分子定义式，与 AUD202-004 对表 |
| R-5 | `p1snr_science_test.cpp` 的 `production` 组是否在任何参数点上能测出"分子错口径" | 其 oracle `refCompute` 是同式镜像（`:74-95`）⇒ 结构上测不到；但要断定"测试面无一处能红"须遍历全部 8 个测试文件的判据 | 需人工通读 `p1noise_tests_core.cpp`/`p1noise_selfcheck.cpp` 等 10 文件（本代理未做） |
| R-6 | 帧级 SNR 的 `m_5` 与 `frame_snr` 在产品头里的实际取值一致性 | 需读落盘 HiPS 头 | `astro_sphere_sink.cpp:430-470` 写 `ASTROCS_FRAME_SNR=snr_reference.snr_f`、`ASTROCS_REFERENCE_FLUX`，与 `frame["frame_snr"]`(=`F_5/m_5`，`:7278-7284`) **同名不同物**：两处键名重叠但内容不同，须在盘上核对消费者读的是哪一个 |

**方法与工具边界**

- 本机 `rg` 不可用（Grep 工具报 `spawn rg ENOENT`），全部检索改走 `git grep` / `grep`；bash heredoc 在本机会吞引号（已在 append AUD202-009..012 时触发一次 `unexpected EOF`），故后续 append 改为"Write 临时文件 + `cat >>`"，临时文件已删除，仓库无残留。
- 全程未 `import` 仓库内 Python 模块、未写任何仓库内文件：`aud202_recompute.py` 与 `sens.py` 均在仓库外，且以 `PYTHONDONTWRITEBYTECODE=1 python -B` 运行；
  读实验代码只读文本。`实验/absolute-snr` 的 results 为**逐字段回读**而非重跑。
- 收工基线核对：见下节原文。

## 开工/收工 git 基线差异（原文）

开工（本报告 §前置）：

```text
?? ACSD整治工作包_AUDIT-06.zip
?? site/
```

收工（同一命令、同一工作树）：

```text
?? ACSD整治工作包_AUDIT-06.zip
?? site/
```

**两次差异 = 空**（无新增、无修改、无删除）⇒ 本分片零写入仓库，取证动作未污染工作树。
核对方式：`git -C "F:/Astro dev/Astro CS Normalization Database" status --porcelain` 开工/收工各跑一次并逐字比对；
`git rev-parse --short HEAD` 两次均为 `c8f64e9a`。
<!-- PROGRESS: 已完成 12 条结论 / 12 条；判据表+三类数据+覆盖率自报+证伪前提+诚实边界(需复测 R-1..R-6) 五节收尾全部完成 -->
