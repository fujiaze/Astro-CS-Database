# SCI-001-S1 帧级 SNR / PSF 信号权重 / 逆方差叠加 专项自审

- 分片：SCI-001-S1（控制包 RELEASE-01）
- 任务书：`docs/research/SNR_WEIGHT_RESEARCH_PACK.md`
- 日期：2026-09-17
- 基线：HEAD = `41b41e2d38076f6600f7de00faf19c733456641c`（未做任何 git 写操作；未改 `lib/`、`tests/`、`contracts/`、`config/`、`ci/`）
- 方法：文档 ↔ 实现 ↔ 独立权威（PixInsight 官方 .pidoc 源文件 + PCL 2.10.4 Doxygen + 一手文献 + photutils 3.0.0 数值对拍）三方比对；每个结论给出文件:行或节/式号。
- 数值证据：`run/RELEASE-01/science/exp_math.py`、`run/RELEASE-01/science/exp_photutils.py`（自包含 numpy/astropy/photutils，不调用生产码）。

> 说明：本文把「PixInsight 官方文章式号」记为 **[N]**（GitLab `Reference-Documentation/docs/ImageWeighting/02-PSF_Flux_Weighting_Algorithms.pidoc`，master 分支，2026-09-17 抓取；HTML 版 `https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html`）。子代理核验已确认该 .pidoc 的 `\equation` 顺序与 HTML 的 `[N]` 编号锚点逐个一致（共 22 式）。

---

## 0 执行摘要（先看结论）

| # | 结论 | 级别 | 依据 |
|---|---|---|---|
| F1 | **`frame_snr` 语义三方冲突**：07 §4.1 / `UNIFIED_MODEL` / `ASTROCS_DESIGN §4.3` 说它是「帧级未加权原始 SNR（对标 PSFSNR）、写入 HiPS 头、Phase2 现场换算 `w=SNR²/F_ref²`」；`CONTROL_WEIGHT_SNR` 与实现（`module_adapters.cpp:3177`）说 `frame_snr` 键承载的是 **5σ 深度对象**，且「本路径不再输出任何整帧 SNR 标量」。 | **P0** | §2.1、§3.1 |
| F2 | **PSFSNR 公式在 07 §4.1 与研究包里写错**：写成 `c3·√(Σ_j f_j²)/(c4·σ_n²)`；官方式[18]是 `c3·(Σ_j f_j)²/(c4·σ_n²)`。`√(Σf²)` 实际是官方保留未用的 `PSFFluxPower` 量。 | **P1（文档）** | §2.2、§3.2 |
| F3 | **AstroCS `psfsw_robust` 与 PixInsight 式(16) 不是同类公式**：实现 `Wt=S²·Conc/(N²·B)`（α=2,γ=2），而官方是 `(Σf)(Σf̄)/(σ_n·M*)`（各一次方）；更关键的是实现的 `N` = **共同星星流通量的 MAD**，不是图像噪声 `σ_n`（`psfsw.cpp:236`、`psfsw.h:137`）。文档只说「信号总量×集中度/（稳健噪声×稳健背景）」，未披露指数与 `N` 的真实定义。 | **P1（文档表述不清 + 科学待裁）** | §2.3、§3.3 |
| F4 | **「帧级 SNR 不随天光漂移」表述为假**：实现 `SNR_F=F/σ_F`，`σ_F` 显含 `σ_sky`；天空受限时 `SNR_F ∝ 1/σ_sky`（数值实验 E4：σ_sky 由 1→40，SNR_F 严格按 1/40 下降）。正确口径是「对**加性背景/梯度**不变，对**天光散粒噪声**敏感」。 | **P1（文档表述不清）** | §2.5、§3.4 |
| F5 | `PSF_SIGNAL_WEIGHT.md §4` 把 `psf_snr_power` 列为可选模式，但该模式在实现里 **DEFERRED、被生产模式门显式拒绝**。 | **P2（文档）** | §3.5 |
| F6 | 常数版本漂移：官方文章 c1=8.0832e-6、c3=1.350e-7；**PCL 2.10.4 头文件 c1=5.326e-6、c3=1.316e-7**。任何引用都必须带版本。AstroCS **没有照抄** c1..c4（`C_norm=1.0` 且组内 median 归一使 C_norm 尺度简并）。 | P2 | §4 |
| F7 | 研究包 §5.1 第 1 条把 **ZOGY（Zackay/Ofek/Gal-Yam 2016, ApJ 830,27）** 与 **How to COAAD Images I（Zackay & Ofek 2017, ApJ 836,187）** 混引。 | P2（文档） | §3.7 |
| F8 | AstroCS 自有的 PSFSW 复合指数（α=2,β=1,γ=2,δ=1）与 `n_common`/非均匀阈值等，在实现注释中全部标 **PENDING_OWNER_SIGNOFF**，没有本项目 L1 合成数据标定记录。 | P1（科学待裁） | §4.3 |
| F9 | 研究包 §4 把 **DeepSkyStacker** 标为 GPL v3 且 URL 错误（404）；实际许可证为 **BSD-3-Clause**、仓库为 `deepskystacker/DSS`。 | P2（文档/合规） | §6 |

---

## 1 七项研究任务逐项结论

### 任务 1 公式重建（PSFSNR / PSFSW / 标准 SNR + w=1/σ² 与 w∝SNR²）

**做了什么**：抓取并逐字精读 PixInsight 官方 .pidoc 源文件与 PCL 2.10.4 Doxygen 头文件；重建式[16]（PSFSW）、式[18]（PSFSNR）、式[20]（标准 SNR）、式[7][8]（PSF flux / mean PSF flux）、式[11]-[13]（MMT→`R*`→`M*`）、式[14][15]（`N*`）；从 Horne 1986 最优提取出发推 `w∝SNR²`；用数值实验复核。

**结论**：
1. 官方式[16]：`w_PSF = c1·(Σ_{j=1}^n f_j)·(Σ_{j=1}^n f̄_j) / (c2·σ_n·M*)`，`c1=8.0832×10^-6, c2=9.0×10^6`（文章版）。`f_j` = 拟合 PSF 的 FWTM 椭圆孔径内 `Σ(v_i−B_j), v_i>B_j`（**不使用拟合振幅 A**，故称 hybrid PSF/aperture photometry，式[7]）；`f̄_j=f_j/(π r_x r_y)`（式[8]，信号集中度）；`M*=median(R*)`（式[13]，MMT 残差中位数，默认尺度 256 px）；`σ_n` 可用 MRS 或 `N*`（文章称默认 MRS）。
2. 官方式[18]：`SNR_PSF = c3·(Σ_j f_j)²/(c4·σ_n²)`，`c3=1.350×10^-7, c4=4.987×10^6`（文章版），由「令中位 PSFSNR = 中位标准 SNR = 3.029」标定。
3. 官方式[20]：`SNR=σ²/σ_n²`，`σ` 为整帧标准差型尺度估计（三次迭代、双侧、轻微剔除）；官方明确说它**受梯度/天光正向影响**，会给亮背景、目标 SNR 低的帧虚高权重。
4. `w=1/σ²` 与 `w∝SNR²`：Horne 1986 最优提取给出 `σ_F^{-2}=Σ_i P_i²/σ_i²`，`SNR_F=F/σ_F`。取公共参考通量 `F_ref`，则 `w_k≡1/σ_{F,k}² = SNR_k(F_ref)²/F_ref²`；`F_ref` 为组内常数时 `w_k∝SNR_k²`。**成立条件**：高斯/白噪声、方差可加、UPM 已把各帧归一到公共通量尺度、`SNR_k` 定义在同一 `F_ref` 上（不是各源自身通量）。
   - 数值复核（`exp_math.py` E3）：两帧（F=1000 ADU，FWHM 3/4 px，σ_sky 5/8 ADU）逐位满足 `w = SNR²/F_ref²`（相对差 0），且 `SNR_comb² = Σ_k SNR_k² = 2392.135`（逆方差合并与逐帧平方和逐位一致）。
   - 注意：`w∝SNR²` **只对「通量型 SNR」成立**。若 `SNR` 取 PixInsight 式[18] 的 ratio-of-powers（分子是 `(Σf)²`、分母是 `σ_n²`，且带经验常数 c3/c4），则 `SNR²/F_ref²` **不再是**任何单源通量的逆方差。当前 AstroCS 文档把「对标 PSFSNR」与「`w=SNR²/F_ref²`」并列，二者不能同时成立（见 F1/§3.1）。

### 任务 2 PSF 差异处理（Zackay & Ofek I 与 point_information）

**做了什么**：核 Zackay & Ofek 2017（Paper I, ApJ 836,187, arXiv:1512.06872）与 Paper II（ApJ 836,188, arXiv:1512.06879）的一手书目与结论；对照 `docs/science/PSF_SIGNAL_WEIGHT.md §2` 的 `W_info=a²PᵀC⁻¹P` 与 `information_weight.cpp`。

**结论**：
- Paper I 的核心结论是「**每帧用其自身 PSF 做 matched filter，再加权求和**才最优；先把 PSF 均质化再叠加、或先叠加再匹配滤波都会损失灵敏度」。AstroCS 的 `point_information` 模式 `Q_k=a_k P_kᵀC_k⁻¹d_k, W_k=a_k²P_kᵀC_k⁻¹P_k, F_hat=ΣQ/ΣW, Var=1/ΣW`（`PSF_SIGNAL_WEIGHT.md:22-26`、`information_weight.cpp:294-316`）**正是逐帧信息加权后合并**，与 Paper I 的结构一致（每帧保留自身 PSF 核，不预先均质化）。**方向正确。**
- 白噪声闭式 `W=a²/(σ_pix²·A_NEA), A_NEA=1/ΣP²`（`information_weight.cpp:199-239`、`PSF_SIGNAL_WEIGHT.md:31-33`）与 Horne 1986 一致（数值 E3 已核）。
- **工程兼容性**：HiPS 球面存储 + 分块按需计算要求信息权重按 tile/leaf 现场求值。`PSF_SIGNAL_WEIGHT.md §6` 已规定 `W_info(x,y)` 默认空间量、通过均匀性/信息损失门才压为帧标量；这与 Paper I「保留每帧 PSF」相容，无需先 PSF 均质化。数值代价 = 每 tile 一次 `PᵀC⁻¹P`（对角闭式 O(m)；dense Cholesky O(m³)；低秩 Woodbury O(mr²)，`information_weight.cpp:116-188`）。
- **偏离点**：Paper II 的 proper coaddition 要求输出像素 i.i.d.、方差归一到常数 1（式[11][13]，arXiv 排版）；AstroCS 的 `C_out=R C_in Rᵀ` 是通用算子形式，不做 proper 归一——这是有意选择（HiPS 存输入产品、Phase2 现场算权重），不是错误，但**不得**把 `point_information` 的合并图像直接宣称为 proper coadd 而不做方差归一说明。

### 任务 3 噪声与背景选型（MRS/N*/MAD/MedDev/SExtractor/Siril bgnoise）

**做了什么**：对照 PixInsight 的 MRS 与 `N*`（式[11]-[15]）、Mad/MedDev（`NOISE_MODEL.md:17,53`）、SExtractor 分块背景、Siril bgnoise；对 `frame_snr` 做天光敏感性数值实验。

**结论**：
- 本项目选定 **空背景 MAD→σ**（`σ_bg=1.482602218505602·MAD`，`NOISE_MODEL.md:53`，`noise_model.cpp:robust_sigma`）+ 8×8 patch 平面场 `var(x,y)=a+b·x+c·y`。MAD 常数是标准正态分位恒等式（`1/Φ⁻¹(3/4)=1.482602218505602`，Rousseeuw & Croux 1993 的 MAD 一致化常数），**不是** PixInsight 私有常数。数值 E1（4×10⁶ 高斯样本）复核 `1/MAD=1.482887`，与冻结值差 1.9×10⁻⁴，在蒙特卡洛标准误内。
- PixInsight 的 `N*_MAD=2.48308·MAD(R*)`、`N*_Sn=2.03636·S_n(R*)`（式[14][15]）**未被 AstroCS 采用**；AstroCS 用的是 MAD→σ 的 1.482602 一致化，两者定义域不同（2.48308/1.482602 ≈ 1.6748，口径不同不可互换）。
- **天光不漂移判据**：数值实验 E4（`exp_math.py`）给出对固定源（F=1000 ADU，FWHM 3 px），天空受限时 `σ_sky` 每翻倍，`SNR_F` 精确减半（1→0.5→0.2→0.1→0.05→0.025）。**所以**：`frame_snr`（若定义为 `F_ref/σ_F`）对**加性背景电平/梯度免疫**（背景被独立扣除，不进入信号），但**对天光散粒噪声敏感**（物理正确：亮天光确实降低真实 SNR）。07 §4.1「不随天光变化漂移」的措辞不成立，须改写为「不受天光**电平/梯度**的**正向虚高**影响；天光**噪声**增大时 `frame_snr` 正确下降」。
- 与标准 SNR 对比：`SNR=σ²/σ_n²` 的分子含天光（`σ²` 随背景变亮而增大），故虚高；AstroCS 拒绝它是**有据**的（PixInsight §2.7/§4.5 逐字批评见 §6 文献）。

### 任务 4 开源对拍

见 §3.6（对照表）与 §6（参考代码库清单）。**8 个开源项目全部抓到源码逐行核验**：Siril、SWarp、DeepSkyStacker、SExtractor、SEP、photutils、properimage、SCAMP；其中 **photutils 3.0.0 完成了数值对拍**（孔径通量/孔径 SNR/Background2D，见 §3.6），其余为源码级公式对拍。关键发现：逐像素 ivar 与 SWarp/SExtractor/SEP/photutils **高度一致**；AstroCS 式 PSFSW 四分量+组内中值归一+无量纲在所列开源实现中**无对应**；DeepSkyStacker 许可证/URL 需更正。

### 任务 5 常数独立标定

见 §4。**结论**：AstroCS 未照抄任何 PixInsight 经验常数；但本项目自己的 PSFSW 复合指数（α=2,β=1,γ=2,δ=1）与阈值在实现里标 `PENDING_OWNER_SIGNOFF`，**尚无本项目 L1 合成数据标定记录**（见 F8、§4.3）；`C_norm=1.0` 因组内 median 归一而尺度简并（`PSFSW_ALGORITHM_SPEC.md §5.2`），无需标定。

### 任务 6 产出与冻结

- 报告：本文件。
- 文档订正：`docs/science/PSF_SIGNAL_WEIGHT.md`、`docs/science/NOISE_MODEL.md`、`docs/references/SCIENTIFIC_REFERENCES.md`（见 §7）。**07 §4.1、`UNIFIED_MODEL`、研究包不在本分片写域**，其订正建议以变更 claim 形式给出（§5）。
- 新增公式进入 L1 Oracle 的建议：`w=SNR²/F_ref²`（TST-SNR-WPROP）、`SNR_comb²=ΣSNR_k²`（TST-SNR-COMB）、`frame_snr 天光不变量`（正例：加性 pedestal 不变；负例：σ_sky×2 ⇒ SNR_F×0.5）。

### 任务 7 合规

- 全程未复制任何 GPL 源码进本仓库；未改 `lib/`、`tests/`、`contracts/`、`config/`、`ci/`；未做 git 写操作。
- 引用的开源项目与许可证见 §6。

---

## 2 逐项证据链

### 2.1 F1：`frame_snr` 三方冲突（P0）

**文档 A（新文档包/最高设计口径）**
- `docs/plugins/algorithms_phase1/07_noise_snr.md:38-39`：「是**唯一帧级参考**，写入 HiPS 文件头；**是未加权的原始信噪比，不是权重**……权重由 Phase2 逆方差叠加时从 SNR 现场计算」。
- `:47`：frame_snr 对标 PSFSNR。
- `:57-64`：`SNR_k(F_ref)=F_ref·sqrt(W_psf,k)=F_ref/σ_F,k`；`w_k=1/σ_F,k²=SNR_k(F_ref)²/F_ref²`。
- `docs/design/UNIFIED_MODEL.md:42`：`frame_snr = 帧级未加权原始信噪比；Phase2 归一后现场换算 w=SNR²/F_ref²`。
- `ASTROCS_DESIGN.md:86,155-158,235`：帧级 SNR 是唯一帧级参考、写入 HiPS 头、`w=1/σ²=SNR²/F_ref²∝SNR²`。

**文档 B（科学权威口径，FROZEN 2026-08-27）**
- `docs/science/CONTROL_WEIGHT_SNR.md:10-14`：「本文件所称 `local_snr`/`frame_snr` 实为**相对质量权重场**……**不是科学信噪比**；科学 SNR 由逐源 `σ_F` 定义」。
- `:30-33`：`frame_snr = 整帧 Phase1 SNR 目录值的**中位数（回退质量基准）（不是科学信噪比）**`。
- `:44-52`：帧级科学基准唯一允许是 `m_5=ZP−2.5log10(5σ_F(ref))`。
- `:120`：禁止把 `frame_snr` 声明/解释为科学信噪比。

**实现 C**
- `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp:23-24`（头注释）：「本路径**不产出**"整帧 SNR 标量"。帧级量只有 5σ 深度（+ 目录中位数 `median(SNR_F)`，其语义是逐源 SNR 的分布摘要，不是新的帧级 SNR 定义）」。
- `lib/infrastructure/scheduler/src/module_adapters.cpp:2964-2973`：同上重述「**不再输出任何"整帧 SNR 标量"**。local_snr/frame_snr 按 SCI-CW-001 §2a/§4 重定义为相对质量权重场 / 5σ 深度（非校准信噪比）」。
- `:3177-3183`：`frame["frame_snr"] = {definition:"5-sigma point-source depth = F_5 [ADU] / m_5 [mag] (SCI-CW-001 2a); NOT a whole-frame scalar SNR", flux5_adu, m5_mag, zero_point_mag}`。
- `contracts/schemas/unified/frame_snr.schema.json:4-5`：`frame_snr = 帧级 SNR（信噪比，非权重）：真实信号/噪声比`——**契约 schema 站在文档 A 一侧**，与实现 C 直接冲突。
- 消费侧：`lib/algorithms/coverage/tools/stage2.cpp:74,86,402` 的 `frame_snr_medians` 取「帧 catalogue SNR 中位数」作回退质量基准，与文档 B 一致。

**裁决**：同一符号 `frame_snr` 至少承载三种含义：(a) 帧级未加权原始 SNR（文档 A + 契约 schema）；(b) 5σ 深度对象（实现 C）；(c) 相对质量权重目录中位数（文档 B + stage2 消费）。**证据不足以判定哪一个是科学上唯一正确的目标态**（三者服务不同目的），但**可以判定现状处于自相矛盾**，且文档 A 与实现 C 不能同真。→ 登记 **UNRESOLVED U1**，按 AGENTS §9 上呈负责人裁决；在裁决前，07 §4.1 不得宣称「frame_snr 写入 HiPS 文件头」为现状。

### 2.2 F2：PSFSNR 公式错误（P1）

- `docs/plugins/algorithms_phase1/07_noise_snr.md:47`：`c3·√(Σ_j f_j²) / (c4·σ_n²)`。
- `docs/research/SNR_WEIGHT_RESEARCH_PACK.md:21`：同一错误公式。
- **权威**：官方 .pidoc 式[18] 逐字为 `SNR_PSF = c3·(Σ_{j=1}^n f_j)² / (c4·σ_n²)`（注意 `(Σ f_j)²`，不是 `√(Σ f_j²)`）。
- 官方 `PSFFluxPower = Σ f_j²` 在元数据表中明确标「**Currently not used, reserved for future extensions**」（子代理 A §5；03-Implementation.pidoc）。错误公式很可能由「把保留量 `PSFFluxPower` 误当分子」产生。
- 影响面：纯文档（实现未采用该式），但会误导后续实现与验收。→ 变更 claim SC-S1-002。

### 2.3 F3：PSFSW 实现与官方式(16) 的差异（P1）

**官方式[16]**：`w_PSF = c1(Σf)(Σf̄)/(c2·σ_n·M*)`。

**AstroCS 实现**（`lib/algorithms/photometry/cpp/src/psfsw.cpp`、`include/astrocs/v6/psfsw.h`）：
- 四分量（`psfsw.h:137-138`、`psfsw.cpp:233-238`）：`S_k=Σ_{s∈S_k} fhat_{k,s}`；`Conc_k=mean_s(fhat)/A_NEA`；`N_k=1.482602218505602·MAD({fhat})`；`B_k=b̄_k·A_ref,k`。
- 复合（`psfsw.h:57-61`、`psfsw.cpp:312-314`）：`Wt=C_norm·S^α·Conc^β/(N^γ·B^δ)`，`α=2, β=1, γ=2, δ=1, C_norm=1.0`，组内 `W=Wt/median(Wt)`。
- 与官方式(16) 的三点差异：
  1. **指数**：官方等效为 `S^1·Conc^1/(σ_n^1·M*^1)`；AstroCS 是 `S²·Conc/(N²·B)`，把「信号/噪声」平方。
  2. **N 的定义**：官方 `σ_n` 是**图像噪声**（MRS 或 `N*`）；AstroCS `N_k` 是**共同星 PSF 通量的星间散度**（MAD）。见数值实验 E6（`exp_math.py`）：固定共同星集（200 颗，真通量 10²–10⁴ ADU），把 σ_sky 从 2→8→32（噪声扩大 16×），`N_k≈1239.6/1215.6/1245.7 ADU` **基本不变**，而真实图像噪声已变 16×。⇒ `N_k` 主要度量星等分布的内在散度，**不是图像噪声**。
  3. **B 的定义**：官方 `M*` 是逐像素稳健平均背景；AstroCS `B_k=b̄_k·A_ref` 是背景×参考面积（量纲被乘了一个面积）。
- 文档侧（`PSF_SIGNAL_WEIGHT.md:39`、研究包 §2.2、`UNIFIED_MODEL.md:44`）只写「信号总量×信号集中度/（稳健噪声×稳健平均背景）」，**未披露 α=2,γ=2 与 N=星间散度**，也未声明「同类/inspired」而非「等价」。→ **文档表述不清 + 科学待裁**：这套复合是否优于（或至少不劣于）式(16) 的比值结构，需本项目 L1 合成/真实数据标定，不能仅凭「受 PixInsight 启发」冻结。→ 变更 claim SC-S1-003、UNRESOLVED U2。

### 2.4 逆方差合并的严格性（支撑任务 1/2）

`F_hat=ΣQ/ΣW`、`Var(F_hat)=1/ΣW` 是线性无偏最小方差估计（高斯噪声）。对多帧：`SNR_comb²=(Σ_k F_k W_k)²/Σ_k W_k ≤ Σ_k F_k² W_k = Σ_k SNR_k²`；当各帧测同一真实通量（`F_k=F`）时取等号。数值 E3 已验证等号与 `ΣSNR_k²` 逐位一致。**成立条件**：帧间独立（协方差对角）、通量已归一、无系统项；相关噪声（Drizzle 后、共享 master）必须走 `C_out=R C_in Rᵀ`，**不能**用 `1/Σw` 简单式（`UNCERTAINTY_AND_COVARIANCE.md:15-19,66-72`）。

### 2.5 F4：天光不变量（P1）

数值 E4 见任务 3。文档需把「不受天光影响」精确为：
- **不变**：加性背景电平/平缓梯度（被独立背景分量扣除，不进入信号）；
- **敏感（且应敏感）**：天光散粒噪声 `σ_sky` 增大 ⇒ `σ_F` 增大 ⇒ `frame_snr` 下降。
- 07 §8 现有 oracle「天光变化不改变帧级 SNR」若按字面执行会**把正确实现判红**，必须改为上述正/负例分解。

### 2.6 F5：`psf_snr_power` 状态（P2）

- `docs/science/PSF_SIGNAL_WEIGHT.md:59` 在 §4「Phase2 支持的选择」表中列 `psf_snr_power | ratio-of-powers 的项目冻结实现`。
- 实现：`lib/infrastructure/cli/v6_runtime_contract.h:92,110-114`：`reject: psf_snr_power(DEFERRED)`，`FZ-MODE-DEFERRED`；`lib/algorithms/coverage/include/astro/phase2/coverage.h:157-165` 生产模式门 allowed={point_information,surface_gls,psfsw_robust}，显式拒绝 `psf_snr_power`。
- `PSFSW_ALGORITHM_SPEC.md §2.1` 亦称其为 `NOT_IMPLEMENTED/unavailable`。→ 文档须标注 DEFERRED。→ 变更 claim SC-S1-005。

### 2.7 F7：研究包文献混引（P2）

- `docs/research/SNR_WEIGHT_RESEARCH_PACK.md §5.1` 第 1 条：「Zackay, B., Ofek, E. O., & Gal-Yam, A. 2016, How to coadd images? I, ApJ 830, 27（arXiv:1512.06872）」。
- Crossref 核验（子代理 C）：**ApJ 830, 27 = Zackay, Ofek & Gal-Yam 2016, "Proper Image Subtraction…"（ZOGY, arXiv:1601.02655, DOI 10.3847/0004-637X/830/1/27）**；**arXiv:1512.06872 = Zackay & Ofek 2017, "How to COAAD Images. I", ApJ 836, 187（DOI 10.3847/1538-4357/836/2/187）**。两者是不同论文，作者也不同（后者无 Gal-Yam）。
- 注：`docs/references/SCIENTIFIC_REFERENCES.md:17-18` 的 ApJ 836,187/188 写法**本身正确**；错误只在研究包。→ 变更 claim SC-S1-007。

---

## 3 对照表

### 3.1 `frame_snr` 定义

| 我方公式/定义 | 权威出处 | 差异分析 | 结论 |
|---|---|---|---|
| 07/UNIFIED_MODEL/设计：frame_snr = 帧级未加权原始 SNR（对标 PSFSNR），写入 HiPS 头；Phase2 换算 `w=SNR²/F_ref²` | 官方式[18] 是 ratio-of-powers（`(Σf)²/σ_n²`）；`w=1/σ_F²` 只在通量型 SNR 下成立（Horne 1986） | 「对标 PSFSNR」若指式[18]，则 `w=SNR²/F_ref²` 不成立；若指 `F_ref/σ_F`，则与 PSFSNR 不是同一量 | **文档表述不清需澄清**（且与实现冲突，见下） |
| 实现：`frame_snr` 键 = `{flux5_adu, m5_mag, zero_point_mag}`（5σ 深度对象，非标量 SNR） | `module_adapters.cpp:3177-3183`；`snr_frame_science.h:23-24`；契约 `frame_snr.schema.json:4-5` 要求 `frame_snr_value` | 契约 schema（文档 A）与实现 C 互斥；实现如实声明自己不是整帧 SNR | **实现不符文档，且文档互斥** → UNRESOLVED U1 |
| CONTROL_WEIGHT_SNR：frame_snr = 帧 catalogue 相对质量权重中位数，非科学 SNR | `stage2.cpp:74,86,402`；`sampler.cpp:633-641` | 与文档 A 的「科学信噪比」直接互斥 | **文档互斥** → UNRESOLVED U1 |

### 3.2 PSFSNR 公式

| 我方 | 权威 | 差异 | 结论 |
|---|---|---|---|
| 07 §4.1 / 研究包：`c3·√(Σ_j f_j²)/(c4·σ_n²)` | 官方式[18]：`c3·(Σ_j f_j)²/(c4·σ_n²)` | 我方分子错（`√(Σf²)` vs `(Σf)²`） | **我方有误需订正**（07/研究包；本分片仅报告） |
| 常量 c3=1.350e-7, c4=4.987e6 | 文章式[19]；PCL 2.10.4 c3=1.316e-7 | 版本漂移 | 引用须带版本；AstroCS 不照抄 |

### 3.3 PSFSW

| 我方 | 权威 | 差异 | 结论 |
|---|---|---|---|
| `Wt=S²·Conc/(N²·B)`（α=2,γ=2），`N=MAD(星流通量)`，`B=b̄·A_ref`，组内 median 归一 | 官方式[16]：`c1·(Σf)(Σf̄)/(c2·σ_n·M*)`；`σ_n`=图像噪声（MRS/N*） | 指数、N 定义、B 定义三处不同 | **文档表述不清**（未披露 α/γ 与 N 语义）；复合优劣**待标定** → 变更 claim + UNRESOLVED U2 |
| `C_norm=1.0`，组内 median=1 | 官方 c1=8.0832e-6, c2=9.0e6（文章版） | AstroCS 用无量纲组内相对量，C_norm 尺度简并 | **开源实现与我方设计不同但合规**（未照抄；spec §5.2 证明 C_norm 对 W 无影响） |
| 文档写「受 PixInsight PSFSW 启发/同类」 | 官方方法学 | 「同类」不等于「等价」；须显式声明差异 | **文档表述不清** |

### 3.4 天光敏感性与噪声

| 我方 | 权威 | 差异 | 结论 |
|---|---|---|---|
| 07：帧级 SNR「不随天光变化漂移」；§8 oracle「天光变化不改变帧级 SNR」 | 实现 `σ_F²` 含 `σ_sky²`（`snr_science.cpp:113-114`）；物理上天光散粒噪声必降低 SNR | 措辞把「不受加性背景虚高」误写成「不受任何天光影响」 | **文档表述不清需澄清**（07，本分片报告） |
| 空背景 MAD→σ，`1.482602218505602` | 标准正态 MAD 分位恒等式；Rousseeuw & Croux 1993 | 一致 | **我方正确** |
| 5σ 裁剪 ≤2 轮、8×8 patch、平面场 | SExtractor 分块背景（Bertin & Arnouts 1996）；PI MMT 背景（式[11]-[13]） | 域不同（PI 用 MMT 非线性多尺度；AstroCS 用 patch 平面场） | **开源实现与我方设计不同但各有据** |

### 3.5 模式与权重面

| 我方 | 权威 | 差异 | 结论 |
|---|---|---|---|
| `PSF_SIGNAL_WEIGHT.md §4` 列 `psf_snr_power` 为可选用 | `v6_runtime_contract.h:110-114` DEFERRED；`coverage.h:157-165` 拒绝 | 文档把 DEFERRED 写成可用 | **我方有误需订正**（可改文件） |
| `W_info=a²PᵀC⁻¹P`，`F_hat=ΣQ/ΣW` | Horne 1986；Zackay & Ofek I（逐帧 PSF 匹配） | 一致 | **我方正确** |
| `C_out=R C_in Rᵀ`，禁止 `Var=1/W_psfsw` | Fruchter & Hook 2002（线性重建、相关噪声）；Paper II | 一致 | **我方正确** |

### 3.6 开源对拍（数值/源码级）

| 项目（许可证） | 对照量 | 权威实现（文件:行，默认分支当前快照） | 与 AstroCS 异同 | 结论 |
|---|---|---|---|---|
| photutils 3.0.0 / astropy（BSD-3-Clause） | 孔径测光通量与误差、Background2D | `photutils/utils/errors.py:91-92`；`_batch_photometry.pyx:273-277`；`photutils/background/core.py:464-531` | 同族：`σ²=Σw_frac²·error²`、`σ_tot²=σ_bkg²+I/g_eff` | **数值对拍通过**（见下） |
| Siril（GPL-3.0） | 帧级权重、IKSS、bgnoise | `src/stacking/median_and_mean.c:1111-1135`：`w_i=1/(pscale_i²·bgnoise_i²)`；wFWHM `:1137-1182`；星数 `:1184-1230` | 同：逆方差型帧权重；异：Siril 直接当叠加系数并按帧均值归一，AstroCS `W_psfsw` 为组内中值归一无量纲量 | 开源实现与我方设计**同族但不同** |
| SWarp（GPL-3.0） | 逐像素 ivar 组合、权重重标定 | `src/coadd.c:1279-1311`：`out=Σx_k/var_k/Σ1/var_k`，`var_out=1/Σ(1/var_k)`；`src/back.c:361-389` RESCALE_WEIGHTS | 同：与 AstroCS 逐像素 ivar 及 `C_out=R C_in Rᵀ` 的对角特例同构 | **开源实现与我方等价** |
| SExtractor（GPL-3.0） | FLUXERR 误差传播 | `src/analyse.c:200-203,304-310`：`Var(F)=Σ(σ_bkg²+F_pix/gain)` | 同：与 AstroCS 孔径 CCD 方程（Howell 1989，`snr_science.cpp:195`）同构 | **开源实现与我方等价** |
| SEP（LGPL-3.0） | 孔径方差 | `src/aperture.c:516-570`：`σ²_sum=Σvar_pix·w²+Σ/gain` | 同 | **开源实现与我方等价** |
| DeepSkyStacker（BSD-3-Clause） | 帧评分、逐像素稳健权重 | `RegisterEngine.cpp:86-118`（quality=圆度加权）；`avx_output.cpp:463-575`（`w=1/(1+(x−µ)²/σ²)`） | 异：quality 与 SNR/FWHM 乘积无关；无 ivar/读噪项 | 开源实现与我方设计**不同**（且研究包许可证/URL 标错） |
| properimage（BSD-3-Clause） | proper coaddition | `properimage/operations.py:457-577`：`R=IFFT(Ŝ/√P̂)`、有效 PSF `P_r` | 同：Zackay & Ofek II 的频域实现，与 `W_info`/协方差传播等价 | **开源实现与我方等价**（信息层） |
| SCAMP（GPL-3.0） | 相对光度定标 | `src/photsolve.c:117-409,437-570,782-785` | 异：核心源文件中未发现像素背景归一；只做相对零点/天体测量 | 开源实现与我方设计**不同**（UPM 背景归一不应引 SCAMP 为依据） |

**数值对拍（photutils 3.0.0，`exp_photutils.py` E5）**：合成 Moffat4 β=4 源（F=5000 ADU，σ=1.5 px，sky=10 ADU，σ_sky=2 ADU），r=3 px 孔径——
- photutils 精确孔径通量 **4778.97 ADU** vs AstroCS 解析 `f_in` 通量 **4814.81 ADU**（比值 **0.9926**，差 0.74%，来自有限孔径离散化/像素化）；
- 孔径 SNR photutils/AstroCS = **449.37/413.30 = 1.087**，与 AstroCS 多算的天空环方差因子 `√(1+n_pix/n_sky)=√1.2=1.095` 一致（Howell 1989 CCD 方程的天空估计项）；
- `Background2D` 在梯度场中心恢复 **21.839** vs 真值 **21.8**。
- 结论：AstroCS 孔径/背景口径与 photutils 数值一致到 <1%，SNR 差异可由显式方差项解释；**无系统性偏差**。

### 3.7 文献引用

| 我方 | 权威 | 差异 | 结论 |
|---|---|---|---|
| 研究包 §5.1-1：Paper I = Zackay/Ofek/Gal-Yam 2016, ApJ 830,27 | Crossref：ApJ 830,27 = ZOGY；Paper I = Zackay & Ofek 2017, ApJ 836,187 | 混引两篇 | **我方有误需订正**（研究包；本分片报告） |
| `SCIENTIFIC_REFERENCES.md:17-18` 的 ApJ 836,187/188 | Crossref 一致 | 一致（arXiv 标题与付印版 "COAAD" 拼写差异可注） | **我方正确**（可补注） |

---

## 4 常数清单（是否照抄 / 是否须独立标定 / 现状证据）

| 常数 | 值 | 出处 | AstroCS 是否照抄 | 是否须独立标定 | 现状证据 |
|---|---|---|---|---|---|
| PSFSW c1 | 8.0832×10⁻⁶（文章式[17]）/ 5.326×10⁻⁶（PCL 2.10.4） | 官方 .pidoc；PCL Doxygen | **否**（无此常数） | 若实现 PixInsight 兼容模式则须，且须锁版本 | grep c1/c3 全仓无实现命中；仅 07:53 提及 |
| PSFSW c2 | 9.0×10⁺⁶ | 同上 | 否 | 同上 | 同上 |
| PSFSNR c3 | 1.350×10⁻⁷（文章）/ 1.316×10⁻⁷（PCL 2.10.4） | 官方式[19]；PCL | **否** | 同上 | 07:53 仅文字引用 |
| PSFSNR c4 | 4.987×10⁺⁶ | 官方式[19]；PCL 同 | 否 | 同上 | 同上 |
| `N*_MAD` 系数 | 2.48308 | 官方式[14] | **否** | 否（AstroCS 用标准 MAD→σ） | `NOISE_MODEL.md:53` 用 1.482602218505602 |
| `N*_Sn` 系数 | 2.03636（PCL 头文件）/ 2.05435（PCL Estimates 注释，冲突） | 官方式[15]；PCL | 否 | 否 | 本分片未复现 2.03636（标准 Sn 的 σ 一致化是 1.1926，见 E1）；标 UNRESOLVED U3 |
| `C_norm` | 1.0（`PSFSW-COMPOSITE-V1`） | `psfsw.h:61` | 否（自有） | **尺度简并，无需标定**；但须登记 | `PSFSW_ALGORITHM_SPEC.md §5.2`；`psfsw.cpp:335-339` 的 cnorm_invariance |
| PSFSW 指数 α,β,γ,δ | 2,1,2,1 | `psfsw.h:57-60` | 否（自有） | **须**（实现注释标 PENDING_OWNER_SIGNOFF，无 L1 标定记录） | `psfsw.h:40`「全部 PENDING_OWNER_SIGNOFF」；spec §13 R1 |
| MAD→σ | 1.482602218505602 | 标准正态分位恒等式（Rousseeuw & Croux 1993） | 否（课本常数） | 已由恒等式闭合；E1 复核 | `NOISE_MODEL.md:53,124` |
| Moffat4 FWHM/σ | 1.230310 | Moffat 1969 轮廓 + β=4 解析（与 PCL `FWHM=2σ√(2^{1/β}−1)` 同物理、σ 约定差 √2） | 否（解析导出） | 解析闭合；E2 复核 1.230308 | `snr_science.cpp:37`；`PSF.md §5` |
| median SE 常数 | 1.253 = √(π/2) | 高斯 median 位置标准误（教材级） | 否 | 解析闭合 | `snr_science.cpp:246` |

**结论**：**未发现任何 PixInsight 经验常数被照抄**；但 AstroCS 自有的 PSFSW 指数缺乏本项目标定记录（F8）。PixInsight 文章版 vs PCL 2.10.4 的 c1/c3 漂移说明：任何「精确兼容」模式都必须**版本化常数**（`pixinsight_psfsw_compat` 已要求记录兼容版本，`PSF_SIGNAL_WEIGHT.md:51`）。

---

## 5 变更 claim 清单

> 每条含：证据、影响面、建议版本递增、一致性回归项。本分片未改实现，未改 07/UNIFIED_MODEL/研究包（不在写域）。

### SC-S1-001（P0）`frame_snr` 语义唯一化
- 证据：§2.1（文档 A 07:38-39/47/57-64、UNIFIED_MODEL:42、ASTROCS_DESIGN:86,155-158,235；文档 B CONTROL_WEIGHT_SNR:10-14,30-33,44-52,120；实现 module_adapters.cpp:2964-2973,3177-3183、snr_frame_science.h:23-24；契约 frame_snr.schema.json:4-5）。
- 影响面：`docs/plugins/algorithms_phase1/07_noise_snr.md`、`docs/design/UNIFIED_MODEL.md`、`docs/science/CONTROL_WEIGHT_SNR.md`、`contracts/schemas/unified/frame_snr.schema.json`、`docs/contracts/UNIFIED_OBJECTS.md`、`docs/contracts/DATA_ARTIFACTS.md`、stage2/sampler 消费、HiPS 写盘。
- 建议版本递增：07 文档 minor；`frame_snr.schema.json` 若改语义则 v1→v2（或明确拆成 `frame_snr_value`（SNR）与 `depth_m5` 两对象）；`DATA-P1-SNR` schema 版本已为 /2，须登记 `DATA_SEMANTICS.md`。
- 一致性回归：`p1snr_*` 全部 ctest；`p1snr_frame_parity_test`；stage2 `weight_runtime_gate.py`（frame_snr_median_fallback）；`test_phase1_inprocess.py` / `test_phase123_pipeline.py` 产物名/n_snr_catalogue。
- 处置：**UNRESOLVED U1**，上呈负责人裁决后再落文档/实现。

### SC-S1-002（P1）PSFSNR 公式订正
- 证据：官方 .pidoc 式[18] vs 07:47 / 研究包:21；PSFFluxPower 保留未用。
- 影响面：`07_noise_snr.md §4.1`、`SNR_WEIGHT_RESEARCH_PACK.md`（研究包回执）。
- 版本递增：07 minor；研究包修订记录。
- 回归：文档一致性 grep（无 `√(Σ` 残留）；L1 Oracle 不含该式，无需代码回归。

### SC-S1-003（P1）PSFSW 复合语义披露与标定
- 证据：官方式[16] vs `psfsw.cpp:233-238,312-314`、`psfsw.h:57-61`、`PSFSW_ALGORITHM_SPEC.md §5.1`；E6 证明 N 不响应图像噪声。
- 影响面：`docs/science/PSF_SIGNAL_WEIGHT.md §3`（可改）、`docs/algorithms/v6/phase2-psfsw/*`、`docs/design/UNIFIED_MODEL.md:44`、`docs/research/SNR_WEIGHT_RESEARCH_PACK.md §2.2`。
- 版本递增：PSF_SIGNAL_WEIGHT minor（补公式与差异声明）；spec 若改指数须走 ALG 变更 + 标定记录。
- 回归：`PSFSW-G14/G15`、`p1psfw` 测试（α/γ 变更须重跑）；新增 L1 合成标定脚本与冻结记录。
- 处置：部分**可改（文档补差异声明）**；指数是否有科学依据 → **UNRESOLVED U2**。

### SC-S1-004（P1）天光不变量口径订正
- 证据：E4（σ_sky×40 ⇒ SNR_F×1/40）；`snr_science.cpp:113-114`。
- 影响面：`07_noise_snr.md §4.1/§8`。
- 版本递增：07 minor。
- 回归：把 §8 oracle 改为「加性 pedestal 不变 + σ_sky 翻倍 SNR_F 减半」正/负例。

### SC-S1-005（P2）`psf_snr_power` DEFERRED 标注
- 证据：`v6_runtime_contract.h:92,110-114`、`coverage.h:157-165`、`PSFSW_ALGORITHM_SPEC.md §2.1`。
- 影响面：`docs/science/PSF_SIGNAL_WEIGHT.md §4`（可改）。
- 版本递增：PSF_SIGNAL_WEIGHT minor。
- 回归：无代码。

### SC-S1-006（P2）常数版本化
- 证据：文章 c1=8.0832e-6/c3=1.350e-7 vs PCL 2.10.4 c1=5.326e-6/c3=1.316e-7（子代理 A）。
- 影响面：`07_noise_snr.md:53`、`SNR_WEIGHT_RESEARCH_PACK.md §2.2/任务5`。
- 版本递增：07 minor。
- 回归：文档。

### SC-S1-007（P2）研究包文献混引订正
- 证据：Crossref（子代理 C）。
- 影响面：`SNR_WEIGHT_RESEARCH_PACK.md §5.1`。
- 版本递增：研究包修订。
- 回归：`SCIENTIFIC_REFERENCES.md` 已正确（本分片补 ZOGY 条目以消歧）。

---

## 6 参考代码库清单

| 项目 | 许可证 | 仓库 URL | 对照文件（默认分支当前快照） |
|---|---|---|---|
| Siril | GPL-3.0 | https://gitlab.com/free-astro/siril | `src/stacking/median_and_mean.c`（帧权重 :1111-1230）、`src/algos/statistics_float.c:150-227`（IKSS）、`src/algos/background_extraction.c`、`src/algos/PSF.c:356-478` |
| SWarp | GPL-3.0 | https://github.com/astromatic/swarp | `src/coadd.c:1279-1443`（ivar 组合）、`src/back.c:361-389`（RESCALE_WEIGHTS）、`src/weight.c:275-360`（权↔方差） |
| DeepSkyStacker（DSS） | **BSD-3-Clause** | https://github.com/deepskystacker/DSS | `DeepSkyStackerKernel/RegisterEngine.cpp:86-118`、`avx_output.cpp:463-575`、`avx_avg.cpp:232-241` |
| SExtractor | GPL-3.0 | https://github.com/astromatic/sextractor | `src/back.c:449-743`、`src/analyse.c:162-310`（FLUXERR）、`src/weight.c:48-135` |
| SEP | **LGPL-3.0** | https://github.com/kbarbary/sep | `src/aperture.c:348-574`（孔径方差）、`src/background.c:277-790`、`src/analyse.c:168-315` |
| photutils / astropy | BSD-3-Clause | https://github.com/astropy/photutils | `photutils/background/core.py:360-690`、`photutils/utils/errors.py:12-138`、`_batch_photometry.pyx:273-300` |
| properimage | **BSD-3-Clause** | https://github.com/quatrope/properimage | `properimage/operations.py:457-577`、`properimage/single_image.py:1259-1404`、`properimage/utils.py:141-243` |
| SCAMP | GPL-3.0 | https://github.com/astromatic/scamp | `src/photsolve.c:117-409,437-570,782-785` |
| PixInsight Reference-Documentation（方法学文档，非代码） | PixInsight 自定义许可（文档公开） | https://gitlab.com/pixinsight/Reference-Documentation | `docs/ImageWeighting/01..05-*.pidoc` |
| PCL（PixInsight Class Library） | PixInsight 自定义 source-available（非 OSI） | https://gitlab.com/pixinsight/PCL | `PSFSignalEstimator.h`（常数/N* 取证，**不复制**） |

> 许可证核验：Siril `LICENSE.md` 首行 GPLv3；SWarp/SExtractor/SCAMP GitHub API 或 `LICENSE`；DSS `LICENSE` 全文 + `README.md:13` BSD-3-Clause；SEP `src/sep.h:9-10` + `licenses/LGPL_LICENSE.txt`；photutils/properimage GitHub API（properimage PyPI `project_urls.Repository` 指向 quatrope/properimage）。GPL/LGPL 代码只作理解与数值行为对照，**未复制进本仓库**。

---

## 7 文档订正 before/after 摘要

> 本分片实际写入的文件只有 3 个（均在本分片允许写域内）+ 本报告；07 §4.1 / UNIFIED_MODEL / 研究包**未改**（不在写域），其订正以 §5 变更 claim 形式给出。
> 注意：`docs/science/PSF_SIGNAL_WEIGHT.md` 与 `docs/science/NOISE_MODEL.md` 在本分片执行期间已被并行分片（SCI-001-S2）追加过参考节（PSF_SIGNAL_WEIGHT §9、NOISE_MODEL §14a）；本分片只做**增量追加**，未删除任何既有条款。

### 7.1 `docs/science/PSF_SIGNAL_WEIGHT.md` §3（新增官方公式与实现披露）

**before**（§3 第一段，原样）：
> PixInsight 将 PSFSW 定义为 hybrid PSF/aperture photometry 的综合图像质量估计器：PSF 总 flux 表示总 signal，mean PSF flux 表示 signal concentration，分母结合稳健 noise 与稳健 mean background；归一常数把典型数值调到实用范围。其 PSF SNR 则采用 ratio-of-powers，并被建议用于只追求集成图像 SNR 的权重。

**after**：保留原段，其后新增引用块，内容为：
1. 官方 PSFSW 式[16] `w_PSF=c1(Σf)(Σf̄)/(c2σ_nM*)`、PSFSNR 式[18] `c3(Σf)²/(c4σ_n²)`、标准 SNR 式[20] `σ²/σ_n²`，逐式标式号；
2. 文章版常数 c1/c2/c3/c4 与标定集（1000×4096²），并标注 **PCL 2.10.4 的 c1/c3 不同**（5.326e-6 / 1.316e-7）；
3. **AstroCS 实现披露**：`Wt=C_norm·S^α·Conc^β/(N^γ·B^δ)`、`α=2,β=1,γ=2,δ=1`、`N=MAD({fhat})`（星间散度，非 σ_n）、`B=b̄·A_ref`，并声明「受启发而非等价」+ PENDING_OWNER_SIGNOFF。
- 出处：PixInsight .pidoc 式[7][8][12][13][16][17][18][19][20]；PCL Doxygen；`lib/algorithms/photometry/cpp/src/psfsw.cpp:233-238,312-314`；`lib/algorithms/photometry/include/astrocs/v6/psfsw.h:42-64,137-138`。

### 7.2 `docs/science/PSF_SIGNAL_WEIGHT.md` §4（DEFERRED 标注）

**before**：`| psf_snr_power | ratio-of-powers 的项目冻结实现 | 只追求集成图像经验 SNR | 不能自动等同 Fisher 最优 |`
**after**：`| psf_snr_power | ratio-of-powers（**DEFERRED，当前未实现**） | 设计占位；生产模式门显式拒绝 | 不适用（不得进入生产路由） |`
并在表下新增证据注：`v6_runtime_contract.h:110-114`、`coverage.h:157-165`、`PSFSW_ALGORITHM_SPEC.md §2.1`。

### 7.3 `docs/science/NOISE_MODEL.md` §14a.1（新增，不改 §5/§5a）

新增子节「14a.1 SCI-001-S1 补充：PixInsight N* 常数与本项目口径」：
- 官方 `N*_MAD=2.48308·MAD`、`N*_Sn=2.03636·Sn` 与 **AstroCS 未采用**（用 1.482602218505602），不得互换；
- PCL 2.10.4 `NStar()` 取 2.03636 与 `Estimates::NStar` 注释 2.05435 的冲突（UNRESOLVED U3），本分片 E1 复核标准 Sn 一致化为 1.1926；
- 逐像素 ivar 开源对照（SWarp/SExtractor/SEP/photutils 文件:行）与 SWarp RESCALE_WEIGHTS 先例。
- 未改动任何既有条款；lint `python3 tools/science_contract_lint.py docs/science/NOISE_MODEL.md` = PASS。

### 7.4 `docs/references/SCIENTIFIC_REFERENCES.md`（订正 + 新增 §N）

**订正 1（§M SEP 行）**
**before**：`- SEP（Source Extraction and Photometry） — **MIT（需网络核验）** [U]（https://github.com/sep-developers/sep，本轮 API 取 LICENSE 失败）……`
**after**：`- SEP（Source Extraction and Photometry） — **LGPL-3.0** [V]（https://github.com/kbarbary/sep；证据 src/sep.h:9-10 与 licenses/LGPL_LICENSE.txt，SCI-001-S1 复核）……`

**新增 §N「帧级 SNR / PSFSW / 逆方差叠加专项」**
- N.1 PixInsight 官方 .pidoc 五节 + PCL 2.10.4 常数（含版本差异）；
- N.2 文献 61–69：Zackay & Ofek I/II（ApJ 836,187/188）、**ZOGY 消歧（ApJ 830,27）**、Starck & Murtagh 1998（PASP 110,193）、Rousseeuw & Croux 1993（JASA 88,1273）、Moffat 1969、Stetson 1987、Bertin & Arnouts 1996、Maples 2018；
- N.3 参考代码库 8 项（Siril/SWarp/DSS/SExtractor/SEP/photutils/properimage/SCAMP）含许可证与对照文件:行；**DSS 更正为 BSD-3-Clause + deepskystacker/DSS**。

### 7.5 未改但必须订正（不在本分片写域）

- `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1/§8：PSFSNR 公式、frame_snr 语义、天光不变量（SC-S1-001/002/004）。
- `docs/design/UNIFIED_MODEL.md` §2 frame_snr 行（SC-S1-001）。
- `docs/research/SNR_WEIGHT_RESEARCH_PACK.md` §2.2/§4/§5.1（SC-S1-002/006/007，DSS 许可证/URL）。

---

## 8 UNRESOLVED（上呈负责人）

| ID | 事项 | 为什么无法在本分片判定 | 需要的裁决/证据 |
|---|---|---|---|
| U1 | `frame_snr` 的规范语义（帧级科学 SNR ↔ 5σ 深度 ↔ 相对质量中位数） | 三方权威互斥（07/UNIFIED_MODEL/设计 vs CONTROL_WEIGHT_SNR vs 实现），证据不足以判定目标态 | 负责人裁决唯一 canonical；同步改契约 schema / 07 / UNIFIED_MODEL / stage2 |
| U2 | AstroCS PSFSW 的 `N=MAD(星流通量)` 与 α=2,γ=2 是否有科学依据 | 无本项目 L1 标定记录；实现标 PENDING_OWNER_SIGNOFF | 用 L1 合成/真实数据对标式(16)，出标定报告后冻结或改指数 |
| U3 | PixInsight `N*_Sn` 常数 2.03636 与 2.05435 冲突 | 标准 Sn 的 σ 一致化是 1.1926；2.03636/2.05435 未见一手定义 | 核 PI 源码/版本或 Rousseeuw-Croux 原文的 S_n 变体 |
| U4 | PixInsight Moffat 式[5]（分母 2σ²）与式[10]/PCL（分母 σ²）差 √2 | 官方文档内部不一致 | 向 PI 确认内部参数化；跨软件 FWHM 比较须注明 |
| U5 | Horne 1986 / Naylor 1998 / Stetson 1987 / Starck & Murtagh 1998 的**付印版式号** | PDF 反爬，仅读到二手片段 | 以付印版 PDF 核式号（本报告不写死式号） |

---

## 9 复跑命令

```bash
cd "/workspace/Astro CS Database"
python3 run/RELEASE-01/science/exp_math.py            # E1-E4,E6
PYTHONPATH=/tmp/pi_venv python3 run/RELEASE-01/science/exp_photutils.py   # E5 (photutils 3.0.0)
```
