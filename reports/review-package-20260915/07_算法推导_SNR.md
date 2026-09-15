# SNR 核心算法推导（审核包章节）

> 文档 ID：`REVIEW-DERIVE-SNR/1`（审核包章节，非 SCI/ALG 权威）
> 范围：帧级 SNR 系数的**估计量推导、选型依据、适用范围与未闭合问题**
> 权威层级（宪章 `ASTROCS_PROJECT_CONSTITUTION.md` §1.1:17-26）：SCI `docs/science/CONTROL_WEIGHT_SNR.md`（SCI-CW-001，FROZEN）> DATA `docs/contracts/DATA_SEMANTICS.md` > 代码/实验报告（仅线索）。
> 纪律：**只提炼与推导，不新增论断**；每条论断附锚（`文件:行` 或合同条款号）；代码结论均由本章作者亲自 grep/read 复核（见附录 A）；不确定处显式写"不确定"。
> 证据等级标注：【合同】=冻结/合同文本；【代码】=本次亲验源码行；【实测】=报告中的真实帧测量；【推断】=报告外推；【不确定】=本章未验证。

---

## 0. 摘要（给负责人）

1. 负责人 2026-09-15 设计变更要求"一帧内 SNR 近似一致、用一个系数写入产品元数据"（P33 REPORT §1 时间线）。选定的估计量是 **`value = median(SNR_F)`**（既有 `median_snr`），依据是与图像侧独立参考在 5 个背景主导帧上一致到 **1.12×**、增量成本 0、不改动既有冻结标量（P33 REPORT §0.1/§3.1/§3.2）。
2. 该系数是**帧级相对质量/深度代理**，**不是**校准科学 SNR；合同（SCI-CW-001 §2a:42-53）明确规定唯一允许的帧级科学基准是 **5σ 点源深度 `m_5`**，并要求 `σ_F(ref)` 显式绑定参考轮廓/孔径/背景。
3. "一帧一个系数"在**帧级**成立（1.12×），在**分区级不成立**：固定通量下 512 px 分区偏离 p50 = 8.3%–26.3%，分区观测中位数偏离 p50 = 12.6%–77.9%，仅 9.4%–59.7% 分区落在 ±10% 内（P33 REPORT §4 表；`results/CO_dispersion.csv` 实测列一致）。稀疏控制点 K=64–256 可把同类误差降到 2.6%–8.7%，是本近似的**反证材料/备选**（P33 REPORT §4.1、附录 A）。
4. **四个未闭合问题**（详见 §4）：(a) `σ_F` 在冻结合同层被登记为"当前实现不产出"，而代码/夹具产物实际写出逐源 `sigma_f_adu`（口径为天空受限且非合同字段）⇒ 现有帧级标量仍不能称校准科学 SNR；(b) 权重语义 legacy `weights=support×snr_v²` 与 `weight_mode=2` 纯逆方差并存，本系数不是权重，当权重用须平方；(c) 生产 phase1 末端只写 signal+support，不写 variance/ivar ⇒ `weight_mode=2` 缺输入、按合同应 fail-closed rc=7，端到端驱动实际默认 `weight_mode=1`（等权）；(d) `median_snr`/`m_5` 锚在交付样本统计量上，限量取样会改变样本成分并使 `Δm_5` 跨帧变号。
5. **P33 的落位尚未进入 main 工作树**（本次亲验：main 无 `lib/phase1/noise/snr_frame_coefficient.*`、`DATA_SEMANTICS.md` 无 `snr_coefficient`；仅存在 `run/perf-fix/P33-snr-model/patches/P33-0{1..5}*.patch`）。因此本系数应读作"已设计/已验证、待落 main"，不得据此认为生产已产出该字段。

---

## 1. 要解决的问题：为什么需要一个帧级 SNR 数，它在下游被谁消费

### 1.1 需求来源

- 负责人 **2026-09-15 设计变更**：放弃稀疏控制点/IDW 重建层，改为"一帧内 SNR 近似一致，用一个系数"，写入产品文件头/元数据；同时删除 18–77 MB/帧的逐源数组（P33 REPORT §1 时间线表、§0.1）。
- 目标是给每个 Phase1 单帧一个**帧级标量**，供消费者在**不做逐源重建**的前提下判断该帧的深度/质量（P33 REPORT §7.1）。

### 1.2 下游消费者（逐条锚）

| 消费者 | 消费字段/入口 | 锚（合同或代码） | 当前状态 |
|---|---|---|---|
| Phase1 交付产物本身 | `p1_snr.json` 帧块 | `module_adapters.cpp::p1_op_noise`（:2629 起；写出 :2837-2844）；schema `DATA-P1-SNR/2`；字段表 DATA_SEMANTICS §13.4:436-450 | 【代码】main 已实现（无 `snr_coefficient` 字段——见 §1.3） |
| Phase1 编排产物登记 | `artifact:p1_snr` | `cli/runtime_client.cpp:163`（`{"snr","artifact:p1_snr"}`）、:271；:265 注释"phase1 内部产物 p1_wcs/p1_snr/p1_hips 无下游 IR 消费者" | 【代码】p1_snr.json 是终态输出 |
| Phase2 控制采样（局部质量权重图） | `local_snr_map[key(frame_id,tile,x/64,y/64)]`；缺失回退整帧 median | SCI-CW-001 §2:32-33、§4:66-80；`stage2.cpp:383-396`、:402-403、:1132-1140 | 【合同+代码】 |
| Phase2 控制采样（整帧回退基准） | `frame_snr_medians(hips)` = 逐帧 HiPS SNR 目录中位数 | `stage2.cpp:73-95`（读 `AIO_HIPS_RD_SNR`；缺目录时 `med=1.0`，:78） | 【代码】 |
| Phase2 `weight_mode=0`（legacy/诊断） | `weights[s] = support×snr_v²`（`snr_v` = `local_snr_map` 命中值或 `frame_snr[i]` 回退） | SCI-CW-001 §4:71；`stage2.cpp:1141`（GPU/chunk 路径）、:1395-1415（CPU 路径） | 【合同+代码】 |
| Phase2 控制点 catalog veto | `thr = 10×frame_snr_med[frame_id]`，半径 0.012°，仅当该帧 SNR 目录非空生效 | `sampler.cpp:847-851`（README 亦记 DISP-P2SMP-004 硬编码：`lib/phase2_samp/README.md:123`） | 【代码】 |
| Drizzle IDW 重建（旧路径） | `SNR(ra,dec) = snr_phot × (IDW_spherical / median_snr)` | `lib/snr_estimator/cpp/include/snr_estimator.h:427-428`、:456、:463-466；序列化 `orchestrator.cpp:4631-4643` | 【代码】 |
| 图像侧独立 QA（非产品主字段） | 孔径测光 SNR 参考 | P33 REPORT §3.3、§7.2 | 【实测】建议只入 QA 报告 |

### 1.3 当前落位与合并状态（必须如实登记）

- P33 报告声明落点为 `p1_snr.json` 的 `frames[].snr_coefficient`（DATA-P1-SNR-COEF/1），HiPS 侧键（`snr/properties` 的 `astrocs_snr_coefficient` 等、`metadata.fits` 的 `SNRCOEF/SNRCOEFN/SNRCOEFE`）为**消费者批次合同**（P33 REPORT §7.1）。
- **本次亲验（main 工作树）**：
  - `lib/phase1/noise/` 仅有 `noise_model.{h,cpp}`、`snr_frame_science.{h,cpp}`、`README.md`；**无** `snr_frame_coefficient.*`；
  - `grep -n "snr_coefficient" lib/core/src/module_adapters.cpp docs/contracts/DATA_SEMANTICS.md` **零命中**；
  - 系数实现仅存在于影子树 `run/perf-fix/P33-snr-model/tree/`（`snr_frame_coefficient.{h,cpp}`）与补丁 `patches/P33-0{1..5}*.patch`。
- 结论：本章把 `snr_coefficient` 作为"**已设计、已用真实帧验证、尚未落 main**"的候选交付处理。任何"生产已产出该字段"的陈述均不成立（【代码】）。

---

## 2. 候选估计量的推导与比较

### 2.1 定义式与语义（四族）

**（A）PSF 族 —— 逐源最优提取（基准定义）**【合同+文献】

```
σ_F^-2 = Σ_i P_i² / σ_i²        (P 为归一化轮廓, ΣP_i = 1)
SNR_F  = F / σ_F                 (Horne 1986 最优提取)
```

- 锚：`snr_frame_science.h:12`（`sigma_F^-2 = sum_i P_i^2/sigma_i^2`）；DATA_SEMANTICS §13.4:438（`snr_phot/median_snr/median_source_snr` 三者同值 = `median(SNR_F)`）；`PHOTOMETRY_LITERATURE_REVIEW.md:590-592`（C.3.1(i)，Horne 1986；相关噪声时 `σ_F² = Σ_ij P_i P_j C_ij`）。

帧级估计量（候选）：

| 估计量 | 定义 | 语义 |
|---|---|---|
| `median(SNR_F)`（选定） | 逐源 `SNR_F` 的中位数 | 帧内所有源共用的分布摘要 |
| trimmed-10% 均值 | 去两端 10% 后均值 | 同上，抗尾 |
| 几何均值 | `exp(mean(ln SNR_F))` | 对数尺度摘要 |
| 通量匹配中位数 | 与独立参考同通量窗内的 `median(SNR_F)` | 消去星等分布差异的比较量 |
| 帧级深度 `m_5` | `F_5 = 5·σ_F(ref)`；`m_5 = ZP − 2.5·log10(F_5)` | 唯一合同许可的帧级**科学基准**（mag） |

- `m_5` 锚：SCI-CW-001 §2a:46-48；DATA_SEMANTICS §13.4:439-440；`snr_frame_science.h:13-14`；文献锚 `PHOTOMETRY_LITERATURE_REVIEW.md:599-601`（C.3.1(iii)，Tonry 2012 / Huang 2017 / Ivezić 2019，见 :909-930 引用表）。
- `m_5` 的参考轮廓当前 = "目录中位 FWHM + 帧 `σ_sky`"（DATA_SEMANTICS:439；`module_adapters.cpp:2803-2808` 的 `snr_reference.profile = median_fwhm_of_catalogue_sky_limited`）。

**（B）测光不确定度族（被否）**【合同+文献+报告】
- 旧定义 `snr_phot = 1/(ln10·σ_logflux_dex)`：语义是**参考星群体定标散度**（跨夜/跨帧系统项），不是逐源探测 SNR；对 F=1e3→1e6 ADU 与真最优提取之比从 0.39 掉到 0.0004（跨 3 个数量级）。
  锚：`PHOTOMETRY_LITERATURE_REVIEW.md:607`（C.3.2 步 1）、:55、:634-652；`lib/snr_estimator/README.md:216-218`（旧 `1/(ln10σ)` 已退休，`snr_phot` 重定义为 `median(SNR_F)`）；P33 REPORT §0.2/§3.3（这 6 帧无测光定标残差 ⇒ **不可评估**）。
- 现码 `snr_estimator.cpp:146` 已是 `const double snr_phot = 1.0;`（旧全帧常数退休），逐源值由 `compute_snr_frame_science` 提供（`snr_estimator.cpp:143` 注释）。

**（C）逐像素背景族（不可作源系数）**【报告实测】
- `1/σ_sky`（逐像素 SNR）：与逐源 SNR 尺度不可比（P33 REPORT §3.1 比值 6e-4），帧内极不均匀（512 px 分区 `σ_sky` 相对散布 CV 0.077–3.47，P33 REPORT §0.3/§4）。只能作星云/簇聚帧的均匀性诊断（P33 REPORT §3.3）。

**（D）孔径测光族（独立参考，不作主系数）**【报告实测】

```
r_ap = 1.5×FWHM；局部背景 = 分区中位数
SNR_ap = F_ap / (σ_region × sqrt(N_ap))     （不含增益/读出 ⇒ 与生产 SNR_F 同处天空受限口径）
```

- 锚：P33 REPORT §2.1、§3.3。参考 `R = median(SNR_ap | |log10 F − median| ≤ 0.1 dex)`（通量匹配消星等分布差异），与生产 `SNR_F` **无共享中间量**（不同噪声模型、不同提取核）。

### 2.2 可用性判据（逐族）

| 候选 | 可用性判据 | 依据 |
|---|---|---|
| `median(SNR_F)` | 上游 `p1_sources.json` 存在且可解析（缺失 → DATA 失败，fail-fast，不写貌似成功的 `p1_snr.json`）；样本 = 全部 `flux>0 ∧ fwhm_px>0` 的源 | `module_adapters.cpp:2635-2655`（fail-fast）、:2726-2745（样本判据）；DATA_SEMANTICS:443 |
| `frame_depth_m5_mag` | 需 `zero_point_mag > 0` 且有限；无 ZP → `null`/NaN | DATA_SEMANTICS:440；`snr_frame_science.h:45`；P25 REPORT §7.5（默认无 `snr` 块 ZP=0 ⇒ m5=null） |
| `sigma_location_se_*` | 需 `σ_logflux_dex>0 ∧ n_matches>0`；否则 `unavailable_no_calibration_residual` | `module_adapters.cpp:2799-2802` |
| 测光不确定度族 | 需跨帧测光定标残差；单帧产物不含 | P33 REPORT §3.3 |
| `1/σ_sky` | 尺度不可比，不可作源系数 | P33 REPORT §0.3 |
| 融合（PSF,孔径）几何均值 | 与参考一致到 1.24×、跨帧 CV 0.64（最稳），但需双路径成本 | P33 REPORT §3.1 |

### 2.3 实测对照（6 真实帧）【实测】

比值 = 估计量 / 独立参考 R（1.0 = 与独立测量一致）。来源：P33 REPORT §3.1 表；本章核对 `results/CO_selection.csv` 实测列一致（`med_snr bias_factor=1.6605`、`cross_frame_cv=1.3128`；`trim10=1.9277`；`geomean=1.7049`；`med_matched=1.5409`；`med_snr_ap=0.8531`；`pix_snr=0.00058`；`combo_geo=1.2413`）。

| 候选 | 参考比（6 帧几何均值） | 参考比（5 帧，排除 M42_Ha） | 逐帧范围（5 帧） | 跨帧 CV | 增量成本 | 判定 |
|---|---|---|---|---|---|---|
| `median(SNR_F)` | 1.66 | **1.12** | 0.66–2.71 | 1.31 | **0** | **采用** |
| trimmed-10% 均值 | 1.93 | — | — | 1.46 | O(N) | 无系统优势 |
| 几何均值 | 1.70 | — | — | 1.36 | O(N) | 无系统优势 |
| 通量匹配中位数 | 1.54 | 1.04 | — | 1.36 | O(N) | 记入 provenance |
| `median(SNR_ap)` | 0.85 | — | — | 0.15 | 40 ms | 参考自身 |
| `1/σ_sky`（逐像素） | 6e-4 | — | — | 1.14 | 0 | **尺度不可比** |
| 几何均值(PSF,孔径) | 1.24 | — | — | **0.64** | 40 ms | 双路径成本，不作主系数 |
| `1/(ln10·σ_logflux_dex)` | 不可评估（无定标残差） | — | — | — | 0 | **否**（语义为定标散度） |

### 2.4 为什么 `median(SNR_F)` 胜出，及其脆弱点

**胜出理由（P33 REPORT §3.2 四条）**：
1. 它就是消费者要的量的帧级摘要：`SNR_F = F/σ_F`（Horne 1986）的帧级中位数（`snr_frame_science.h:64-66`；DATA_SEMANTICS:438）。
2. 与**图像侧独立参考**（孔径测光 SNR，噪声取图像下半分位宽 `p50−p16`，不依赖 PSF 模型/目录方差）在 5 个背景主导帧上一致到 **1.12×**（逐帧 0.66–2.71），说明它不是某一实现的产物（P33 REPORT §0.1/§3.1）。
3. **零增量成本 + 零公式改动**：节点已算 `median_snr`；新字段只是产品化封装（P33 REPORT §3.2.3、§7.3：`snr_phot/median_snr/median_source_snr/frame_depth_*` 逐位不变，ctest `p1snr_frame_parity` 锁）。
4. 样本敏感性可显式落盘：暗/亮四分位中位数比 ≈ **0.157**（背景受限下 SNR 正比于通量，该比值≈四分位通量比），故必须连同**样本定义**落盘（P33 REPORT §3.2.4；`sample_sensitivity.faint_over_bright`，P33 REPORT §7.2）。

**脆弱点（必须与选型结论同时报告）**：
- **T3_M42_Ha 参考自身失效**：强星云中孔径背景被过减（参考值偏低 5–8×），该帧参考失效、被**主动剔除并标注**；若计入，`median(SNR_F)` 的参考比从 1.12 恶化到 1.66（P33 REPORT §3.1 表注/§6.3）。这是"未挑选有利帧"的阴性对照，但也说明 1.12× 建立在 **5/6 帧**上。
- **样本函数性质**：`median_snr` 与 `frame_depth_*` 的参考轮廓都锚在**交付样本自身的统计量**上（`ref_flux = median(样本通量)`、`med_fwhm = median(样本 FWHM)`），本质是样本的函数而非与样本无关的帧灵敏度（P25 REPORT §4.1、§6.1 R6；P25 REPORT §3.2 实测参考通量随截断从 10833 → 97213 ADU（9×））。
- **参考轮廓的存在性依赖**：`m_5` 无 ZP 时为 null（§2.2），即默认配置下**帧级科学基准不可得**（P25 REPORT §7.5）。

---

## 3. "一帧一个系数"成立吗：帧级 vs 分区级

### 3.1 帧级成立【实测】

系数就是帧级度量：5 帧与独立参考一致到 ~1.1×（P33 REPORT §0.4 第 1 条/§3.1）。

### 3.2 分区级不成立【实测】

来源：P33 REPORT §4 表；本章核对 `results/CO_dispersion.csv`（`const_p50`/`resid_p50`/`f_within_10`/`region_sigma_cv` 实测列一致）。

| 帧 | 分区数 | 单系数 vs 分区观测中位数 p50（`const_p50`） | p84 | **固定通量偏离 p50**（`resid_p50`） | 固定在 ±10% 内 | 分区 `σ_sky` CV |
|---|---|---|---|---|---|---|
| T4_GC_Blue | 66 | 0.234 | 0.304 | 0.201 | 21.2% | 0.360 |
| T2_M42_Blue | 64 | 0.626 | 0.739 | 0.212 | 17.2% | 1.183 |
| T3_M42_Ha | 64 | 0.779 | 0.871 | 0.263 | 9.4% | 3.471 |
| T4_GC_Oiii | 72 | 0.126 | 0.251 | 0.083 | 59.7% | 0.077 |
| T3_NGC55 | 64 | 0.430 | 0.620 | 0.220 | 17.2% | 0.278 |
| T2_NGC247 | 64 | 0.426 | 0.539 | 0.179 | 25.0% | 0.190 |

- **固定通量偏离**（消去星等分布后）p50 = **8.3%–26.3%**：这是帧内 SNR 真实空间变化的下界（含估计噪声）。
- **分区观测中位数偏离** p50 = **12.6%–77.9%**：消费者若按分区读 SNR，单系数误差可达数倍；M42_Ha（窄带星云）最差。
- **噪声场均匀性**：分区 `σ_sky` CV 0.077（GC_Oiii，最均匀）到 3.47（M42_Ha）。

### 3.3 反证/备选：稀疏控制点方案【实测，早期实验】

同一批真实 6 帧、同源真值下：

| 方案 | 预测 512 px 分区中位数的 p50 误差 |
|---|---|
| 仅帧级基准（`const`） | **12.9%–339%**（M42_Ha 最差） |
| 稀疏控制点 + IDW，K=64（512 px 格） | 2.4%–26.4% |
| **K=256（256 px 格）** | **2.4%–8.7%** |
| K≈960 | 不再改善（残差被控制值格内中位数抽样噪声底 6%–9% 主导） |

- 布点：均匀网格最稳；密度加权在强簇聚帧劣化（M42_Ha 13.6% vs 5.0%）；`idw_k=4` 最优（k=64 在 M42_Ha 劣化到 42.6%）；`idw_power=2–4` 差异 <1%；全图倾斜项 K≥64 无增益。
- 阴性对照：控制值置换劣化 3–30×；合成光滑场阳性对照 p50 0.35%–2.6%；控制值口径（上游 `sources[].snr`）与 `SNR_F` 对数相关仅 0.51–0.88，直接用作控制值劣化 4–15×。
- 开销/存储：暴力 kNN IDW K=64 2.0 µs/query、K=256 5.6 µs/query（单线程）；KD-tree 3.4–4.6 µs/query；控制点 38–51 B/点，K=256 约 11 KB/帧。
- 锚：P33 REPORT §4.1/附录 A（`results/R_grid.csv`、`S2_scale.csv`、`L_loo.csv`、`R2_placement.csv`、`P_power_knn.csv`、`N_negative.csv`、`PC_positive.csv`、`proxy_check.json`、`curve_convergence_p50.svg`）。

### 3.4 结论

负责人的近似在**帧级**成立且有独立参考支撑；在**分区级**不成立（3–10× 的近似损失）。因此合同中必须写死：该系数是帧级度量，**不得**冒充局部/分区 SNR 场（P33 REPORT §0.4/§4/§7.3）。

---

## 4. 关键未闭合问题（单列）

### 4.1 `σ_F` 不产出 ⇒ 现有帧级标量不能称"校准科学 SNR"

**合同侧（FROZEN）**：
- SCI-CW-001 §2a:44-45：逐源科学 SNR 由 `σ_F` 定义，`SNR_F = F/σ_F`；**"当前实现不产出 σ_F，故本合同不把任何现有标量称为科学 SNR"**。
- SCI-CW-001 §2a:46-48：唯一允许的帧级科学基准是 **`m_5 = ZP − 2.5·log10(5·σ_F(ref))`**（mag），`σ_F(ref)` 必须显式绑定参考轮廓/孔径/背景；空间变化时应给**深度图**而非单标量。
- SCI-CW-001 §7:109：**不可接受变化**——把 `quality_weight`/`local_snr`/`frame_snr` 声明或解释为科学信噪比（`m_5` 或 `SNR_F`）。

**代码侧（本次亲验，与上述冻结文本存在差异，须由负责人裁决）**：
- 现码 `p1_op_noise` **确实写出**逐源 `sigma_f_adu`（`sources[].sigma_f_adu`，`module_adapters.cpp:2817`）与帧级参考 `snr_reference.sigma_f_adu`（:2808）；夹具产物实测含这些键（`run/perf-fix/P14-snr-truth/logs/artifacts/patched/run_unlimited/p1_snr.json`）。
- 但该 `σ_F`：口径为**天空受限**（`gain_e_per_adu<=0` 或未知则不加源泊松项，`snr_frame_science.h:43-45`；`module_adapters.cpp:2628`）；参考轮廓 = 目录中位 FWHM + 帧 `σ_sky`（DATA_SEMANTICS:439），**不是固定物理参考**；且 **DATA_SEMANTICS §13.4 字段表（:436-450）未登记 `sources[]`/`sigma_f_adu`** ——它不是合同冻结的交付字段。
- **P33（未入 main）计划删除 `sources[]`/`local_snr.values[]`**（P33 REPORT §0.7/§7.3），并声明二者在仓内**无机器消费者**（P25 REPORT §6.1 R2；`cli/runtime_client.cpp:265`；P33 REPORT §7.3）。

**⇒ 未闭合结论**：无论 `σ_F` 在代码中是否产出，当前用于交付/消费的**帧级标量 `median_snr` 是逐源 SNR 的分布摘要、锚在样本统计量上**，按 SCI-CW-001 §2a/§7 不能称"校准科学 SNR"。唯一合规的帧级科学基准是 `m_5`（且默认无 ZP 时为 null）。**若负责人认为代码已可产出合同级 `σ_F`，需要按宪章 §1.2 走合同/SCI 变更，而不是由本章改写。**

### 4.2 权重语义：legacy `support×snr_v²` vs `weight_mode=2` 纯逆方差；本系数不是权重

- **legacy（`weight_mode=0`）**：`weights[s] = support[s] × snr_v²`，其中 `snr_v` 命中 `local_snr_map` 否则回退 `frame_snr[i]`；`snr=1.0` 不允许冒充 unknown。
  锚：SCI-CW-001 §4:66-71；`stage2.cpp:1141`（GPU/chunk）、:1395-1415（CPU）。
- **科学默认（`weight_mode=2`）**：`w = ivar_i`（纯逆方差），不乘 `snr²`；`ivar_mosaic = W = Σ ivar_i`，`variance_mosaic = 1/W`。
  锚：DATA_SEMANTICS:987（`2=ivar 逆方差（科学默认）；1=等权；0=support×snr²（legacy/诊断）`）、§20.3:1054-1058、§30.1:2307-2318；`module_adapters.cpp:4260-4270`、:4483-4500、:4540。
- **量纲红线**：`support/coverage` 不能作为 inverse-variance 或 SNR 权重（宪章 §6.3:191；DATA_SEMANTICS §20.3:1045-1048、红线 :1062-1066）；`variance/ivar`（ADU²/ADU⁻²）与 `quality_weight`（无量纲）不混用（SCI-CW-001 §6:101）。
- **本系数的定位**：`snr_coefficient.value` **等于** `median_snr`（P33 REPORT §7.3），它是**无量纲相对质量/深度代理**，**不是权重**。若下游把它当权重用，必须按 legacy 定义式**平方**（`w ∝ SNR²`），且仍不得以 `support` 冒充 `ivar`（DATA_SEMANTICS:1062-1066）。
- 另注：SCI-CW-001 §2a:49-50 要求 `quality_weight` 仅供采样/加权（`support × snr_v²`），**不得**解释为 `m_5` 或 `SNR_F`。

### 4.3 逆方差叠加当前不可用：生产 phase1 入口不写 variance/ivar

**代码链（本次亲验，逐环）**：
1. Phase1 生产 drizzle 节点调用**专用末端 ABI** `hp_drizzle_run_phase1_hips`（`module_adapters.cpp:3190`、:3226、:3305）。
2. 该 ABI 走 `hips_profile=1` → `write_hips_phase1`（`hp_drizzle_hips_api.cpp:43-49`、`hp_drizzle_api.cpp:1156-1162`）。
3. `write_hips_phase1` 的 `aio_hips_product_begin` flags = **`AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT`**，注释明文"不写 drizzle provenance，**不写 variance/snr**"（`astro_sphere_sink.cpp:246-251`）。
4. 头文件同源声明：`hp_drizzle_api.h:77-89`（:79 "仅 signal+support 子产品 … 不写 variance/snr"）。
5. **实测产物**：真实 pilot 单帧 HiPS 目录仅含 `signal/`、`support/`（`run/release-rescue/real-mosaic-e2e/pilot/GALACTIC_CENTER/T4+Blue/p1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@055414-180S-Blue/`），无 `snr/`、`variance/`、`ivar/`。
6. 引擎侧**能力存在但未被 phase1 使用**：`drizzle_engine.cpp:1555-1560` 仅在 `varianceValue > 0` 时累加 `sumVarNum += v·w²`；`varianceData` 可为空（:1909-1912，缺省 `varianceValue=0`）。通用档 `write_hips_direct` 在 `variancePtr` 非空时才写 variance（`hp_drizzle_api.cpp:1163-1168`；`astro_sphere_sink.cpp:63-64,127,136-137`）。

**Phase2 侧后果**：
- `stage2` 工具：`cfg.weight_mode` 默认 2；`weight_mode==2` 时强制打开 `AIO_HIPS_RD_IVAR`，缺帧计数 `ivar_product_missing`；缺 ivar 且 `legacy_allow_weight_fallback=false` → **rc=7 显式科学错误**（`stage2.cpp:555-574`；DATA_SEMANTICS §20.1:1000、§20.4:1107-1109、§20.3 红线:1062-1064）。
- 节点链 `p2_op_integrate`：默认 `weight_mode=2`，且**显式拒绝 0**（"only 1 (equal) or 2 (ivar) are legal"，`module_adapters.cpp:4260-4270`）；ivar 缺失 → fail-closed DATA 错误，除非显式 `legacy_allow_weight_fallback=true`（:4289-4298）。
- `weight_mode=1`（等权）：`uncertainty_available=false`（:4304-4305）；按 DATA-UNC-001 §30.1 unavailable 规则 1，**不写 variance/ivar 子产品**（DATA_SEMANTICS:2320-2324）。
- **端到端实际口径**：`run/release-rescue/real-mosaic-e2e/run_e2e.py:1246` `--weight-mode` **默认 1**（choices=[1,2]；:505 写入配置 `{"weight_mode": int(args.weight_mode)}`）。即当前真实 e2e 驱动默认走**等权**，不是科学默认。

**其它相关事实**：
- DATA-UNC-001 §30.1 为 **FROZEN_TARGET_CONTRACT**，"目标态声明（实现不得先行）"；现状 Phase2 输出仅 signal+support（DISP-P2HIPS-001；DATA_SEMANTICS §20.2:1002-1014、§30 intro:2286-2297）。
- `DATA-HIPS-IVAR-001` 读侧消费在 Stage2（DATA_SEMANTICS:1216）；输入侧 `ivar` 的 0/NaN 双值歧义 **finding F-UNC-001** 未消解（§4a:45-50）。

**⇒ 未闭合结论**：在 phase1 产出 `variance/ivar` 之前，`weight_mode=2` 缺输入、按合同应 fail-closed（rc=7 / DATA 错误）；真实端到端因此实际用 `weight_mode=1`（等权），此时按合同**不存在**方差产品（`uncertainty_available=false`）。帧级系数在该状态下不能作为逆方差叠加的科学权重来源。

### 4.4 帧深度的科学定义：样本分布摘要 vs 固定物理参考

- `median_snr`/`snr_phot`/`frame_depth_*` 的样本由 `snr.max_sources` 决定：默认 0 = **全量测光有效源**（`flux>0 ∧ fwhm_px>0`）；一旦截断 `truncated=true`（`module_adapters.cpp:2747-2768`；DATA_SEMANTICS:443,446-447）。
- **限量即改变样本成分**（P25 REPORT §0.1/§3.1，`results/fidelity_table.csv`）：
  - `median_snr` 被系统性抬高：5000 档 +120%～+1157%；1000 档最高 +3283%；20000 档仍 +37%～+640%。
  - `frame_depth_m5_mag` 偏移**随帧、随上限变号且非单调**：−0.623 mag（T4_GC_Oiii@1000）～ +0.191 mag（T4_GC_Blue@10000）。
  - 6 帧全部未收敛；按 |Δm5|≤0.05 mag 宽松口径，20000 档也只有 2/6 帧达标（P25 REPORT §3.3）。
- **根因是"按亮度选样"规则，不是样本量**：随机子样 K=20000 时 bias<0.2%、std 0.65%–1.06%；K=1000 时 std 3.2%–4.9% 且 bias≈0；而按亮度取顶 K=20000 时 `median_snr` 高估 +241%/+674%，K=1000 时 +5121%/+3352%（P25 REPORT §4，`results/sample_table.json`）。
- **科学定义隐患（P25 §4.1/§6.1 R6，需负责人裁决）**：`median_snr`/`frame_depth` 锚在交付样本统计量上，是**样本的函数**；若目标跨帧/跨滤镜可比，应引入**固定参考**（固定星等 / 固定 S/N 档 / 固定 FWHM 模型）而非样本中位数。P14 把样本从"最亮 ≤5000"改为"全部测光有效源"，只是把锚点从亮端移到全检测分布（P14 REPORT §1.2/§1.3；DATA_SEMANTICS §13.4:452-458 的 parity 锁）。
- **P25-F2 缺陷（已修，随影子树）**：旧版帧匹配用精确文件名字符串，NGC55 上 `.fits` vs `.fts` 导致 SNR 全字段**静默 null**；P33 改为精确名/归一化键唯一命中 + fail-closed（P25 REPORT §6.1 R5；P33 REPORT §8）。main 侧 `module_adapters.cpp:2704-2721` 仍是**精确名匹配 + 未命中静默置 null 且节点返回成功**（:2711-2721），归一化/失败语义未落 main。

**⇒ 未闭合结论**：帧深度目前是**样本分布摘要**；限量取样会跨帧变号地改变摘要。跨帧可比的帧深度需要固定物理参考（`m_5` 已具备参考轮廓绑定，但 ZP 由配置提供、默认 null）。

### 4.5 附：本次核对发现的其它证据缺口（如实登记，不确定处标注）

1. **P33 未落 main**（§1.3，【代码】确证）：`snr_coefficient` 不在 main 源码/合同中。
2. **真实 e2e pilot 产物仍是旧 `p1_snr.json` schema**：`run/release-rescue/real-mosaic-e2e/pilot/.../p1_snr.json`（2026-09-14 20:14）顶层为 `['frames','schema']`，帧块仅 `background/sigma/variance/ivar/valid/reason`，**没有** `snr_phot/median_snr/frame_depth_*/frame_snr`。而 P8/P14 的新 schema `DATA-P1-SNR/2` 只在夹具产物中出现（`run/perf-fix/P14-snr-truth/logs/artifacts/patched/.../p1_snr.json`）。⇒ **不确定**：是否已有真实端到端产物承载新帧深度/系数，本章未找到；需在真实 e2e 产物上另核。
3. **HiPS `snr/` 子产品的来源待核**：`stage2.cpp:73-95` 与 `sampler.cpp:848-851` 消费 `AIO_HIPS_RD_SNR` 的 HiPS SNR 目录；但 phase1 生产末端不写 `snr`（§4.3）。仓内 `snr/` 目录样本见于 hips writer 模块测试（`run/release-rescue/fd-p22b-check/build/tests/unit/run/t_hips_ref/snr` 等），**非** phase1 drizzle 生产产物。⇒ 若真实 phase1 产物无 `snr/`，则 `frame_snr_medians` 逐帧回退 `med=1.0`（`stage2.cpp:78`），`local_snr_map`/catalog veto 亦不生效（`sampler.cpp:848` 的 `!frames[frame_id].snr.empty()` 门）。**不确定**：需在真实 e2e 产物上确认；本章仅代码+样例产物核查。
4. **合同与代码的时序差**：SCI-CW-001 §2a 的 P5-SNR 注记日期为 2026-09-14（:10-14、:24、:53），而 P8/P14 的 `snr_frame_science`/`DATA-P1-SNR/2` 亦在同日/其后落地；FROZEN 文本与 main 代码在"是否产出 σ_F"上不一致（§4.1）。

---

## 5. 结论与建议

### 5.1 本系数在合同里应如何措辞（建议，供负责人裁决）

建议措辞（**不得**称校准科学 SNR）：

> **`snr_coefficient`（帧级质量/深度代理）**：定义为 `value = median(SNR_F)`，其中 `SNR_F = F/σ_F`（PSF 加权最优提取，Horne 1986）。它是**逐源 SNR 分布的帧级摘要**，与图像侧独立孔径测光估计量在背景主导帧上一致到 ~1.1×（5/6 真实帧；1 帧参考自身失效并剔除）。本系数为**无量纲相对质量/深度代理**：既不是逐源科学 SNR，也不是校准科学 SNR（SCI-CW-001 §2a:44-45），**不得**解释为 `m_5` 或 `SNR_F`（SCI-CW-001 §7:109）；仅限**帧级**使用，**不得**冒充局部/分区 SNR 场（§3.2/§3.3）；**不是权重**，若作权重须平方（`w ∝ SNR²`）（SCI-CW-001 §4:71）。须连同样本定义、估计量、`n`、离散度 `{p16,p50,p84}`、样本敏感性 `faint_over_bright` 与噪声尺度 provenance 一并落盘（P33 REPORT §7.2），否则不可复现、跨运行不可比。

字段落点建议维持 P33 §7.1：权威 `p1_snr.json` 的 `frames[].snr_coefficient`（`p1_snr.json` 为准，HiPS 键为副本，禁止反向覆盖）；**但须先落 main**（§1.3）。

### 5.2 要补齐什么才能启用"科学默认叠加"（`weight_mode=2`）

以下为**前置条件清单**（按依赖顺序；每条给当前状态锚）：

1. **Phase1 生产末端必须产出逐帧 `variance/ivar` 子产品**。现状：`hp_drizzle_run_phase1_hips` → `write_hips_phase1` 只写 `SIGNAL|SUPPORT`（`astro_sphere_sink.cpp:246-251`；`hp_drizzle_api.h:79`；真实 pilot 产物实测无 `variance/ivar`）。引擎侧累加与 writer 通道已具备（`drizzle_engine.cpp:1557-1559`；`astro_sphere_sink.cpp:63-64/127/136-137`；`aio_hips_write_variance_tile` 已冻结 `aio_hips.h:140`），属**接线缺口而非能力缺口**（DATA_SEMANTICS §20.2:1012-1014 同判）。
2. **为 drizzle 提供逐像素 `variance` 输入**。现状：`varianceData` 可为空、缺省 `varianceValue=0` ⇒ 分支不累加（`drizzle_engine.cpp:1909-1912`）。需由 noise/SNR 域提供逐像素方差图（SCI-NOISE 域；NOISE_MODEL §9a 的 `variance/ivar` 产出边界：`PHOTOMETRY_LITERATURE_REVIEW.md:807-814` S5 建议措辞）。
3. **消解 `ivar` 写侧 0/NaN 双值歧义（finding F-UNC-001）**：§4a:45-50 与 §12.4:353-358 的预存双值须在合同层给出唯一写侧语义，否则逆方差叠加的无效像素语义不确定。
4. **按 DATA-UNC-001 §30 目标态合同实现 Phase2 合成与产品落盘**：`ivar_mosaic = W = Σ ivar_i`、`variance = 1/W`、invalid policy、`uncertainty_available` 与 fail-closed 规则（`docs/contracts/DATA_SEMANTICS.md` §30.1:2299-2348）；注意 §30 明示"目标态、实现不得先行"（:2286-2297），需按项目治理流程推进。
5. **端到端驱动口径改为科学默认**：现默认 `--weight-mode=1`（`run/release-rescue/real-mosaic-e2e/run_e2e.py:1246`）；在 1–4 完成后应改为 2，并保留显式等权/降级路径（`legacy_allow_weight_fallback`，`stage2.cpp:565-578`；`module_adapters.cpp:4289-4298`）。
6. **帧级系数在新语义下的角色**：改为 **quality/diagnostic**（不进入科学权重式）；科学权重由 `ivar` 承担（DATA_SEMANTICS §20.3:1045-1054）。

### 5.3 建议的负责人裁决项（本章不代裁）

| # | 待裁项 | 依据 |
|---|---|---|
| D-1 | 是否按宪章 §1.2 修订 SCI-CW-001 §2a 的"当前实现不产出 σ_F"，以反映 main 代码已写逐源 `sigma_f_adu`（天空受限、非合同字段） | §4.1 |
| D-2 | `snr_coefficient` 的合同措辞与落位（采纳 §5.1 建议？） | §5.1；P33 REPORT §7.1 |
| D-3 | 帧深度的科学定义：保留样本中位数摘要，还是引入固定物理参考 | §4.4；P25 REPORT §4.1/§6.1 R6 |
| D-4 | 是否为分区级精度保留/重启稀疏控制点层（K=256 可把误差降到 2.4%–8.7%） | §3.3；P33 REPORT §4.1/附录 A |
| D-5 | phase1 `variance/ivar` 产出的立项与优先级（启用科学默认叠加的前置） | §5.2 |
| D-6 | 真实 e2e 产物 schema/`snr/` 子产品缺口（§4.5-2/3）的核对责任归属 | §4.5 |

---

## 附录 A. 亲验记录（本章作者本次实际执行的读取/检索）

**A.1 权威/合同文本**
- `docs/science/CONTROL_WEIGHT_SNR.md`（全文 119 行；重点 §1:16-26、§2:28-40、§2a:42-53、§3:55-61、§4:63-85、§6:97-102、§7:104-110）
- `docs/contracts/DATA_SEMANTICS.md`：§4:26-33、§4a:35-50、§5:52-66、§13.4:428-458、§20.1:972-1000、§20.2:1002-1030、§20.3:1032-1103、§20.4:1105-1109、§30:2284-2348
- `ASTROCS_PROJECT_CONSTITUTION.md` §1.1:15-28、§4.1:96-113、§5.3:154-160、§6.1-6.3:164-194

**A.2 源代码（grep/read 复核）**
- `lib/healpix_db/healpix_drizzle/hp_drizzle_api.h:77-89`（:79 不写 variance/snr）
- `lib/healpix_db/healpix_drizzle/hp_drizzle_hips_api.cpp:34-49`（profile=1）
- `lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp:1155-1178`（profile 分支；:1165/1167 has_variance=variancePtr?1:0）
- `lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp:38-45,63-64,106-158,212-251`（phase1 sink 只写 signal+support）
- `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:1553-1561`（`varianceValue>0` 才累加）、:1896-1913（varianceData 可空）
- `lib/core/src/module_adapters.cpp`：:2613-2629（节点职责）、:2635-2656（fail-fast）、:2658-2662、:2700-2721（精确名匹配 + 静默 null）、:2723-2747（样本判据）、:2747-2768（截断/truncated）、:2770-2808（交付字段 + `sigma_f_adu`）、:2809-2833（`local_snr.values[]`/`sources[]`/`frame_snr` 对象）、:2837-2844（落盘）；:3183-3195（p1 drizzle 调 `hp_drizzle_run_phase1_hips`）；:4233-4310（p2 integrate 的 weight_mode 语义与 ivar fail-closed）；:4434-4503（ivar 读/权重）；:4521-4545（wsum/ivar_mosaic）；:4656/4732（variance 产品写出，仅 `uncertainty_available` 时）
- `lib/phase2/tools/stage2.cpp`：:73-95（`frame_snr_medians`）、:250-253、:402-403、:555-578（rc=7）、:1132-1141（`weights=support×snr_v²`）、:1395-1415（mode 0 CPU 路径）
- `lib/phase2/src/sampler.cpp:847-859`（catalog veto 10×frame_snr_med，0.012°）
- `lib/phase1/noise/snr_frame_science.h:1-85`（公式/单位/禁止项）、`snr_frame_science.cpp:141-148`
- `lib/snr_estimator/cpp/src/snr_estimator.cpp:143-156`（旧全帧常数退休）
- `lib/snr_estimator/cpp/include/snr_estimator.h:427-428,456,463-466`
- `lib/orchestrator/cpp/src/orchestrator.cpp:4528-4529,4598-4643`（snr_phot/median_snr/idw 序列化）
- `cli/runtime_client.cpp:163,265-271`
- 目录/产物实测：`lib/phase1/noise/`（无 `snr_frame_coefficient.*`）；`lib/core/src/module_adapters.cpp` 与 `docs/contracts/DATA_SEMANTICS.md` 中 `snr_coefficient` 零命中；`run/release-rescue/real-mosaic-e2e/pilot/.../`（仅 `signal/`+`support/`；`p1_snr.json` 旧 schema）；`run/perf-fix/P14-snr-truth/logs/artifacts/patched/run_unlimited/p1_snr.json`（新 schema，含 `sigma_f_adu`/`frame_snr`）；`run/release-rescue/real-mosaic-e2e/run_e2e.py:505,1246`

**A.3 实验/文献报告**
- `run/perf-fix/P33-snr-model/REPORT.md`（§0、§1、§2、§3、§4、§5、§6、§7、§8、§9、附录 A；`results/CO_selection.csv`、`CO_dispersion.csv` 实测列核对）
- `run/perf-fix/P25-snr-sample/REPORT.md`（§0、§3.1/§3.2/§3.3、§4、§4.1、§5、§6.1、§7）
- `run/perf-fix/P14-snr-truth/REPORT.md`（§1.1/§1.2/§1.3、§3）
- `run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md`（:55、:463-480、:585-661、:792-814、:866-875、:909-930）

**A.4 本章未做（边界声明）**
- 未做任何 git 操作；未修改主工作树既有文件；未运行构建/ctest/端到端（保持轻负载）；未在真实 e2e 产物上验证 §4.5-2/3 的 schema 与 `snr/` 缺口；未重新计算 P33/P25 的原始 CSV（仅核对报告数字与 CSV 列一致）。
