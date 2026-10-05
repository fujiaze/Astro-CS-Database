# GATES_AND_TOLERANCES — P1 星检测 / PSF / WCS 冻结门表与 SNR 定义

> 上游：ACSD_DESIGN.md §12.1（科学正确性与三重佐证）、§4.4（输出合同）
>
> 范围：P1 星检测（star_det）、PSF（dpsf）、WCS（plate solve / ipv）三条支路的
> **全部可执行门与容差**（含端到端坐标契约门）。本文件条款为冻结定义，变更走变更流程。

> 上游（本表**不新设阈值**，每行阈值必须回指到既有 SCI/ALG 条款或本表标注的标定证据）:
> SCI-P1-STAR-001 §1/§4（docs/science/detection/STAR_DETECTION.md）、SCI-PSF-001 §11
> （docs/science/psf/PSF.md）、SCI-WCS-001 §7/§11（docs/science/detection/ASTROMETRY.md）、
> ALG-STARDET-001 §11.4（docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md）、
> ALG-STARPSF-001 §11.4（docs/science/algorithms/STAR_PSF_ALGORITHMS.md）、
> ALG-WCS-001 §11.4（docs/science/algorithms/PLATESOLVE.md）。
> 标定证据面: 本表各行「阈值来源」列所引 SCI/ALG 条款与实验单元读数
> （独立复算直链本树 sdet+dpsf 生产符号 + scipy 第三方复算）。
> 校验面: 本表即判据与阈值的唯一事实源，由人读对抗审核逐行核对
> ——正确实现下判据通过、注入等价缺陷时判红，且两臂判定必须互不相同。

## 1 规则（冻结）

- **R1 表内唯一来源**：任何检查器/门/判据注释的容差一律取自本表；新增门必须
  先在本表登记（门ID / 判据式 / 量测域 / 统计量 / SNR 定义 / 阈值 / 来源 / 证据）。
- **R2 证据必需**：`发布门=Y` 的行，「阈值来源」列必须解析到**可复算的读数出处**
  （SCI/ALG 条款的解析推导，或实验单元中带复现命令与产物路径的实测读数）；
  无证据者把 `发布门` 置 `N`，「阈值来源」列写 `UNJUSTIFIED` 并写明缺什么读数。
- **R3 量测域独立**：`量测域` 与「拟合/生成域」各自独立（自证门不计入门表；例如
  「7×7 网格上往返 <1e-6 px」既是拟合域又是检验域，不作门，见 G-P1-WCS-RT）。
- **R4 统计量显式**：必须是 `max` / `median` / `p95` / `rms` / `bitwise` / `精确` 之一，
  阈值一律与统计量同写（「误差 ≤ x」的写法不含统计量）。
- **R5 SNR 统一**：凡门引用 SNR，必须使用 §2 已登记的定义（`SNR_peak` / `SNR_det`；`SNR_phot` 不属本域），并写明所用列
  （检测侧 `A_fit`=star_det flux 列 / PSF 侧 Moffat4 振幅 `A`）。

## 2 SNR 定义（本域唯一，冻结；补 F-2「全文无 SNR 定义」缺口）

| 名称 | 定义式 | 单位 | 计算面（列/来源） |
|---|---|---|---|
| `SNR_peak` | `A_fit / sigma_bg` | 无量纲 | 检测侧: `A_fit` = 椭圆高斯拟合峰值振幅（star_det `flux` 列，DATA-P1-STAR §17.2；**不是**解析积分流量）、`sigma_bg` = 背景噪声 RMS = `bgnoise`（行差分 FnNoise1 族，`sdet_compute_bgnoise()`，实现见 `lib/algorithms/star_detection/src/sdet_api.cpp`）。**⚠ 两条限定必须随本行引用同写**（正文见 `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md`）：① **`sigma_bg` 属于哪幅图必须声明** —— 它是**未平滑原图**噪声的倍数，不是阈值作用图上的显著性（该文 :55-56）；② 行差分族的估计量只在 **`rho_adj = 0`（相邻像素噪声独立）时无偏**，一般情形为 `Var(d) = 2·sigma_n²·(1−rho_adj)`、`sigma_hat = sigma_n·sqrt(1−rho_adj)`，`rho_adj` 须按该文 :37-47 的 AR(1) 标定表取值（:39-47）。**不声明这两条，「5σ」在真 sigma 下有 4 种读法（5.00σ / 3.33σ / 13.0σ / 8.7σ）。**PSF 侧: `A_fit` = Moffat4 振幅 `A`、`sigma_bg` = `mad/0.7316727929211932`（star_measurements 列 [4]/[10]；**列名 `mad` 是 10–90% 截尾均值 \|残差\|（`residual_scale`）的历史别名，不是中位绝对偏差**，故用截尾均值→σ 因子而非 MAD 因子；实现锚 `lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp`（`sigma_sky_adu = residual_scale/0.7316727929211932`）、`lib/algorithms/noise_snr/cpp/src/noise_model.cpp`、统计量定义 `lib/algorithms/psf/src/dpsf_psf.cpp`；本行原读法 `mad·1.482602218505602` **已废止（历史读法，R6-02）**：MAD 因子只作用于真 MAD，互斥条款见 `docs/science/noise_snr/NOISE_SNR.md` §3.1 稳健尺度节） |
| `SNR_phot` | `F / sigma_F`（Horne 1986） | 无量纲 | 测光域（DATA-P1-SNR §13.4），**与本表门无关**，列此仅作区分 |
| `SNR_det` | `(peak − background) / noise_sigma` | 无量纲 | 检出目录列（`p1_sources.json` 的 `sources[].snr`）：`peak` = **未平滑原图**上检出像素峰值、`background`/`noise_sigma` = 该帧背景与背景 RMS；实现 `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp`（`s.snr = (peak − cat.background) / cat.noise_sigma`）。**与 `SNR_peak` 不同源**：`SNR_peak` 用椭圆高斯拟合振幅 `A_fit`（检测侧 `flux` 列），`SNR_det` 用原始峰值 ⇒ 两列**各自具名**；凡门写「SNR>x」必须点名用哪一行 |

**SNR_peak 的定义敏感性（必须随门一起读）**：全局检测阈值是
`threshold = median(img) + 5.0·bgnoise`（阈组装在 `sdet_prepare_field()` 内，实现见 `lib/algorithms/star_detection/src/sdet_api.cpp`），作用于
**σ=2 平滑后**的图像（`sdet_gaussian_blur_yvv(..., 2.0)`，见 `lib/algorithms/star_detection/src/sdet_api.cpp`；同文件 bgnoise 计算面 = `sdet_compute_bgnoise()`，由 `sdet_prepare_field()` 调用）。
**⚠ 「5σ」在本行的读法限定（正文见 `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md` :55-56）**：`5.0` 乘的是 `bgnoise`，
即**未平滑原图**的噪声倍数；阈值实际作用在 **σ=2 平滑后**的图上 ⇒ **这不是「平滑图上的 5σ 显著性」**。
`STAR_DETECTION_ALGORITHMS.md` 逐字：「`threshold_sigma` 是未平滑原图噪声的倍数，不是阈值实际作用图像上的显著性；引用「5σ」时**必须**声明 σ 属于哪幅图」。
⇒ 同一合成场在不同「峰值 SNR」下可检出性差异极大：
R-3 §2.9 实测同一 Moffat4 场 `SNR_peak=20` 时 sdet 检出 **0 星**、
`SNR_peak=50` 检出 **36/40**、`SNR_peak=300` 检出 36/40。
⇒ 任何门若写「SNR≥x」，必须同时写 `SNR_peak`（本节定义）与 `x` 的取值域，
否则该门不可判（**禁用**只写「SNR≥x」而不定义 `SNR_peak` 与 `x` 的取值域——
该门不可判，负例判据）。

## 3 冻结门表（判据与阈值唯一事实源；列序固定）

| 门ID | 判据式 | 量测域 | 统计量 | SNR/信噪定义 | 阈值 | 阈值来源 | 发布门 |
|---|---|---|---|---|---|---|---|
| G-P1-CENTROID-SCI | `\|c_meas − c_truth\|`（px，逐星绝对位置） | 合成高斯星场（已知中心/流量/FWHM），域 = `SNR_peak ≥ 20` 且非饱和非边缘（距边 ≥5 px） | max | SNR_peak（检测侧） | 0.3 px | 解析+标定：R-3 §2.3 CRLB 与蒙特卡洛（SNR_peak=20 ⇒ p95=0.109 px、max=0.137 px，余量 ≥2.2×）；仓内实测 max=0.0535 px（R-3 §3.5） | Y |
| G-P1-CENTROID-U16 | `\|c_meas − c_truth\|`（px，FP32→uint16 量化通道） | FP32 通道经 uint16 量化（`sdet_detect_ex`），同 G-P1-CENTROID-SCI 的场与域 | max | SNR_peak（检测侧） | 0.5 px | 标定：R-3 §2.6 u16 量化贡献 median 0.0018 / p95 0.0036 / max 0.0056 px ⇒ 余量 ~90×，可达且未超标。**本项只覆盖量化面，不构成端到端位置门** | Y |
| G-P1-CENTROID-1 | `median\|astro_det − truth\| ≤ 0.1 ∧ p95 ≤ 0.3 ∧ \|median_PSF − median_fallback\| ≤ 0.05 ∧ \|median(dpsf_cx − truthIndex)\| ≤ 0.05`（px） | 解析 Moffat4(β=4) 合成场（FWHM 3 px、`SNR_peak`=50/300、256×256×40 星×3 场），**链接 sdet + dpsf + 写读坐标桥真实生产源**；x/y 两轴各断 | median / p95 / median 差 | SNR_peak（检测侧） | 0.1 px / 0.3 px / 0.05 px / 0.05 px | 标定：R-3 §2.9 + 门实测（双支路分离 −0.0000/+0.0001 px、各支路 p95 ≤0.040 px）；**禁用**双支路分离 ≥0.5 px 的实现——分离恰卡在 ipv 去重阈值严格 `<0.5` 之外时同星双份进 ipv（负例判据） | Y |
| G-P1-PSF-ORACLE-F64 | `\|Δ中心\|`（px）、`\|ΔA/A\|`、`\|ΔB/B\|` | FP64 通道，独立 Moffat4/Gaussian 复算（不调用生产符号），合成场 | max | n/a（合成无噪声 + 独立 oracle） | 0.05 px / 1e-3 | 解析+设计冻结：ALG-STARDET-001 §11.4 F4 | Y |
| G-P1-PSF-POS-FWHM | `\|Δpos\|`（px）、FWHM 相对误差 | PSF 图像块（合成），域 = 生产初值/收敛初值双构型 | max / 相对 | n/a（合成） | 0.05 px / 1% | 解析：SCI-PSF-001 §11 + ALG-STARPSF-001 §11.4 | Y |
| G-P1-PSF-SCIPY-ORACLE | 位置（px）/ FWHM 相对误差，独立 `scipy.optimize.curve_fit`（第三方优化器 + 旋转主轴投影参数化，与生产二次型不同源） | 合成无噪声 Moffat4(β=4) 块 ≥3 组参数 + `SNR_peak`=100 噪声面（≥200 次蒙特卡洛） | max（无噪声）/ p95（噪声面） | SNR_peak（检测侧） | 0.05 px / 1% | 解析：SCI-PSF-001 §11 Python 参考承诺（`docs/science/psf/PSF.md`） | Y |
| G-P1-STAR-RECALL | 召回率 = 检出真星数 / 注入真星数 | 合成星场；**域 = 按 σ_psf 分档的逐档 99% 召回阈表**：`SNR_peak ≥` **∈[52, 65]** / 24.0 / 20.0 / 16.0 / 12.0 / 10.0（对应 σ_psf = 1.0 / 1.27 / 1.5 / 2.0 / 2.5 / 3.0 px，峰值对齐像素中心；1000 次/档标定；σ_psf = 1.0 px 档零失败性质非单调 ⇒ **区间标定，引用须同报区间**）；档间取相邻两档较严者；**阈下真星不计入召回率**，其召回率作为过渡带负例单独报告 | 比例（逐场） | SNR_peak（检测侧） | ≥99% | 实测冻结：ALG-STARDET-001 §11.4 F1 逐档 99% 召回阈表（该节同时冻结判据非退化要求：召回场必须在每一档都有域内真星，且含过渡带真星 ≥8 颗、覆盖 ≥4 档，并报告过渡带负例召回率） | Y |
| G-P1-STAR-FP | 虚警密度 | 合成纯噪声场（无注入星，与 G-P1-STAR-RECALL 异场） | 计数密度（每千像素） | n/a | ≤0.1 /千像素（口径正本 = `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md`） | 设计冻结兼发布门 Y：ALG-STARDET-001 §11.4 F1 | Y |
| G-P1-STAR-DET | 输出 bitwise 一致（含 mag 全序 + NaN 末尾 + maxStars 保最亮） | 合成星场，同输入、线程数 1/2/4 | bitwise / 精确 | n/a | 完全相等 | 解析：ALG-STARDET-001 §5 全序与串行归约 | Y |
| G-P1-WCS-F1 | `rms_arcsec`（″） | **trans 拟合内点集（n_pairs ≥ 12）+ 合成线性场（order=1，已知 CD/CRVAL/CRPIX）**；**不是**产品级天测精度门 | rms | n/a | 0.5″ | 标定：**无有效标定**——`Galaxy_Center 0.1431″` 在当前版本 T4 帧上不可复现（实测 0.2803–0.3588″），只在 T2/T3 档场复现，**禁用**该值作标定依据（SCI-WCS-001 §11a）；量测域 = trans 拟合内点集 + 合成线性场；`UNJUSTIFIED`——本行缺一段可复算的标定读数 | N |
| G-P1-WCS-CLOSURE | `median{d_i : d_i ≤ 1.0″}`（角秒；像素换算 `median_px = median_arcsec/s0`），**必须同报** `n_matched` / `match_rate` / `p95` / `max` | **产品级外部闭环 + 真实帧**：检出星样本 = `x,y` 有限 ∧ `snr>20`（`SNR_det`，超 20000 按 flux 降序截断并记录 `sample_capped`），星表 = 本地 Gaia DR3 XPSD 视场单锥 `G<18`；WCS 口径 solved(CD+SIP) 与 frame_header **分别报告**（`wcs_flavor`），各自独立成项；独立工具不导入生产代码 | median / p95 / max | `SNR_det`（§2；门槛 20） | 分档阈值 UNJUSTIFIED（实测参考：T2/T3 档 s0≈0.96″/px ⇒ 0.4202–0.5644 px；T4 档 s0≈6.31″/px 见 §4a） | 口径已冻结（SCI-WCS-001 §11a），口径实现 = `eng/tools/astrometry/closure_metric.py`，**阈值仍未标定**——现有读数取自 XISF 母版单位未修复的输入，修复后必须整体复跑才可定阈；台账中 0.897 px 一项的工具、帧数、半径/星选/统计量均未冻结（半径 1″→5″ 时 median 漂 14%），**不可复现，不作门** | N |
| G-P1-WCS-CLOSURE-REPRO | 同输入同口径两次运行：`median_px(A) = median_px(B)`（**完全相等**）∧ `n_matched(A) = n_matched(B)`；且每份记录声明的 `n_matched/median/p95/max/match_rate` 必须能由**该记录自带的残差向量 + 声明半径**重新导出，且 `params` 必须逐项等于冻结口径 | **产品级真实帧**（同输入两跑记录）+ 记录面（`params` / 残差向量 / 输入 sha256）；合成场自检与产品级两跑读数同属本行量测域 | median / 精确 | `SNR_det`（§2；样本门槛 20） | 0 px（完全相等）/ `n_matched` 精确相等 | 实测漂移 0：E2E-001 §5.1/§5.3 全链科学面产物逐字节相等、规范哈希全等 ⇒ 同输入同口径指标漂移 = 0；记录内 `1e-9 px` 仅为 JSON 浮点往返护栏，**不是科学容差** | Y |
| G-P1-WCS-F2 | astropy WCS 前向/逆向 `\|Δ\|`（px） | 合成 SIP 场（order=2，注入已知 A/B），中心 90% 区域 | max | n/a | 1e-4 px | 预冻结（ALG-WCS-001 §8/§11.4 F2 承接，不放宽） | Y |
| G-P1-WCS-RT | roundtrip `max‖(x,y) − WCS⁻¹(WCS(x,y))‖`（px） | **独立密集域**：中心 90% + 四边 + 四角 + ≥1000 随机点（**不是**拟合采样网格） | max | n/a | 1e-4 px | 解析+实测：与 ALG-WCS-001 §11.4 F2 同值（SCI/ALG 冻结值，**不放宽**）。「7×7 网格上 <1e-6 px」为**自证门**（自网格 1.8e-12 px vs 离网格 3.10 px），不作门。**分层定位**：本行是**全链冻结门**（判据力弱：实测最坏 2.91e-9 px ⇒ 余量 3.4e4×，与 FP64 地板无关）；迭代反演路径的紧门见 G-P1-WCS-RT-ITER、多项式逆表示门见 G-P1-WCS-RT-APBP | Y |
| G-P1-WCS-CRPIX | CRPIX 精确相等 | 任意帧 | 精确 | n/a | 精确 = (w/2+0.5, h/2+0.5)（1-based） | 冻结：SCI-WCS-001 §7 CRPIX 不变量 | Y |
| G-P1-WCS-BRIDGE | 九宫格（中心 1 + 四角 4 + 四边中点 4，两 parity 共 18 格）逐像素 roundtrip **+ 密集域扫描（全帧 ≤128² 网格 + 四边/四角/中心 + 1024 固定 seed 随机点；9 点采样是密集域子集，dense ≥ nine）** + 第三方 astropy 交叉 + 负向注入（移除/错置 `+1` 桥接必须 ≥1 px 偏差） | 导出边界（`lib/algorithms/projection/p3_wcs.cpp`），1024×1024 帧 | max + 注入必败 | n/a | <1e-8 px（**适用域：scale ≥ min_scale_arcsec = 0.9″/px**；低于该尺度本门不适用，报「超出适用域」而非判红）；注入 ≥1 px 必败 | 推导：TAN 闭式投影截断项**恒等于 0**（逐式证明 G∘F=I；FP80 判别实验误差随 eps 线性缩小 1792–2238× vs 理论 2048×）⇒ 误差 100% 来自 FP64 舍入，主项 `ε ≈ C_env·u·sec²Δ/s_rad`（u=2⁻⁵³，C_env=78 实测 max / 128 设计常数，sec²Δ ≤ 1.03046 @FOV≤20°）；1e-8 px 仅在 s ≥ 0.179″/px（实测常数）/ 0.293″/px（设计常数）时 ≥ 最坏情况包络（sec²Δ≈1；按 FOV=20° 的 sec²Δ=1.03046 取最坏为 0.184/0.302″/px）⇒ **必须配适用域下限 0.9″/px**（覆盖仓内最小真实尺度 0.9586″/px；余量 ≥5.0×，区间包络 ≥2.8×）。合成规则 = 最坏情况包络线性相加（**不用 RSS**：实测包络/RSS = 2.47×）。**测法**：导出边界 `lib/algorithms/projection/p3_wcs.cpp` 上取九宫格 18 格 + 全帧 ≤128² 密集网格 + 四边/四角/中心 + 1024 固定 seed 随机点，与第三方 astropy 交叉；逐点算 roundtrip 偏差取 max。**负对照**：移除或错置 `+1` 桥接必须产生 ≥1 px 偏差（注入必败）；FP80 判别实验中误差随 eps 线性缩小 1792–2238×（理论 2048×）⇒ 误差确为 FP64 舍入而非投影截断。**全域对照**：历史全域 880 组几何 max 2.437e-9 px @0.18″/px，与本门在 0.9″/px 下限处的包络一致 ⇒ 下限不是拍的。 | Y |
| G-P1-WCS-BRIDGE-GLOBAL | 同一 STD-F1 桥接量在**全尺度**上的保守门：密集域 roundtrip max < 1e-6 px | 导出边界（`lib/algorithms/projection/p3_wcs.cpp`），任意帧尺度 | max | n/a | <1e-6 px（**全域保守门**，无尺度下限；判据力弱） | 推导：保守性下界 s ≥ 1.79e-3″/px（实测常数）/ 2.93e-3″/px（设计常数，= C_env·u·sec²Δ/s_rad ≤ tol）⇒ 覆盖所有真实仪器（比最细的真实像素尺度还小 1–2 个量级）；代价：0.18″/px 处相对包络余量 101×、相对实测 410× ⇒ 1e-7 量级缺陷会被放过。**与 G-P1-WCS-BRIDGE 是两个不同用途的门，不合并为一个数**。来源：SCI-WCS-001 §11 STD-F1（`docs/science/detection/ASTROMETRY.md`） | Y |
| G-P1-WCS-RT-ITER | 迭代反演路径（`wcs_sky_to_pixel_iterative`，τ = 1e-9 px）在**独立密集域**（中心 90% + 四边/四角 + 1200 固定 seed 随机点，1024²）上的自洽往返 `max‖(x,y)−F_oracle⁻¹(F_oracle(x,y))‖` | P1 ipv 反演路径（`lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp`）；**独立密集域**（中心 90% + 四边/四角 + 1200 固定 seed 随机点），独立 oracle 前向锚 + 生产反演（不含拟合误差） | max | n/a | 1e-8 px（= κ_iter·τ，κ_iter = 10；τ 写进合同） | 推导：迭代反演地板 = 收敛容差 τ（实测 3.30e-10 / 1.66e-9 / 2.91e-9 px 三档畸变，最坏 = 2.91e-9 px = 2.9·τ ⇒ 余量 3.4×）；τ↓1000× 仅使误差↓4.23× ⇒ 另有 ~4e-10 px 的 FP64/迭代结构地板，故取 κ_iter=10 而非 1。**分层理由**：全链门 1e-4 px（G-P1-WCS-RT）对迭代路径过松 4 个量级 ⇒ 迭代路径必须另设紧门（本行）。**测法**：独立 oracle 前向锚 + 生产反演（不含拟合误差），在中心 90% + 四边/四角 + 1200 固定 seed 随机点的 1024² 密集域上逐点算自洽往返取 max；三档畸变各跑一遍取最坏。**负对照**：τ↓1000× 只使误差↓4.23× ⇒ 存在 ~4e-10 px 的 FP64/迭代结构地板，故地板**不是**由 τ 单独决定，κ_iter=10 而非 1 由此实测推出。 | Y |
| G-P1-WCS-RT-APBP | APx/BPx 多项式逆**一步直加表示残差** `max‖UV + APx(UV) − u*‖`（u* = 独立 oracle 反演） | P1 SIP 多项式逆路径；**合成畸变场** fixture（FIX-WCS-F 三档，边缘畸变预算 18.4/91.8/183.6 px），中心 90% 网格 | max | n/a | ≤ 10% × 边缘畸变预算（px）；**表示门，不是 FP64 地板门** | 推导：多项式表示残差随畸变预算陡增（实测 low 1.9e-4% / mid 0.036% / high 1.89% ⇒ 最坏档余量 5.3×）；~184 px 畸变下多项式逆**根本达不到 1e-4 px**（实测 3.46 px）⇒ 必须与 τ 门分开登记。真值无效应⇒归零：零畸变 fixture 残差 2.61e-11 px（比 high 档小 1.3e11×）；等价缺陷（APx 清零）⇒ 202.2 px 必红 | Y |
| G-P1-CENTROID-BRANCH-ORDER | `star_measurements` 写端契约：PSF 支路**恒等**（dpsf 输出即 index-is-center）、fallback 支路 `−0.5`（sdet 连续系）；读端统一 `+0.5` | 契约函数级（`star_coord_contract.h`）+ 端到端（G-P1-CENTROID-1） | 精确 / max | n/a | 精确相等（fallback 输出 − sdet 原始坐标 = 0） | 解析：DATA-P1-STAR §17.2 + dpsf 采样式 `lib/algorithms/psf/src/dpsf_psf.cpp` / sdet 采样式 `lib/algorithms/star_detection/src/sdet_api.cpp`（R-3 §3.1/§3.4 实测） | Y |

## 4 诊断脚本（**不是门**，R1/R2 约束下只作诊断）

| 脚本 / 阈值 | 现状 | 处置 |
|---|---|---|
| `gate2_psf_oracle.py` 的 `fwhm_median_le_1pct` / `ell_median_le_0.005` / `flux_median_le_1pct` / `photutils_oracle_centroid_p95_le_0.05px` | Windows 专用诊断脚本，**脚本与其原址在本仓均不存在**；4 个阈值在活动 `docs/**` 零命中（R-3 §3.5） | 四个阈值**不是门**：引用面限于诊断，任何数值陈述不得以它们作合格判据；如需升格，必须先在 §3 按 R1 登记门ID/域/统计量/阈值/来源，并按 R2 给出可复算的读数出处（含负对照），否则 `发布门` 置 `N` |

## 4a 台账实测值（**XISF 母版单位修复后须整体复跑**）

> 口径 = §3 的 G-P1-WCS-CLOSURE（样本 `snr>20`、星表 `G<18`、半径 1″、median，
> 同报匹配率；solved 与 header 分别报告）。
> **输入单位缺陷（XISF 母版按 [0,1] 归一化消费）未修复 ⇒ 下表绝对量值是方法学
> 重锚，不是可定阈的科学结论**（相对量：solved↔header 比较、
> 两跑一致性、容差敏感性不受影响）。

| 帧（真实数据） | s0（″/px） | 内部解 rms_px / rms_arcsec（n_pairs） | 外部闭环 **solved** median（″ / px；matched） | 外部闭环 **header** median（″ / px；matched） | solved 匹配率 |
|---|---:|---|---:|---:|---:|
| T3 NGC55 Lum 600s | 0.9586 | 0.1650 / 0.1584（43） | 0.5410 / **0.5644**（875） | 0.4841 / 0.5051（867） | 4.86% |
| T2 LDN43 Hα 1200s | 0.9669 | 0.1521 / 0.1472（48） | 0.4062 / **0.4202**（705） | 0.3507 / 0.3627（750） | 3.52% |
| T4 Galaxy_Center panel1 Red 180s | 6.3076 | 0.0580 / 0.3588（41） | 0.6791 / **0.1077**（6265） | 0.6793 / 0.1077（4460） | 31.32% |

- **内部解（T4）六帧范围**：0.2803–0.3588″（n_pairs 39–46，median 0.3231″）；
  台账单值 `0.1431″` 只与 T2/T3 档场（0.1472/0.1584″）同量级，见 SCI-WCS-001 §11a。
- **两跑一致性（发布门 G-P1-WCS-CLOSURE-REPRO，容差 0 px）**：T4 用 `det2/n1` 与
  `det3/n1`（同一帧的两次独立全链路运行、同一星表）⇒ 两份记录逐位相同（median
  0.107658 px、n_matched 6265），门 PASS。
- **采样口径对量值的影响**：样本口径必须与 §3 冻结样本一致——「flux 前 20000」
  与 §3 冻结样本属两个口径，混用会使 `median` 与 `n_matched` 的读数不可比；两种
  口径下的读数正本 = 实验/engineering-evidence/（results）。探针口径见 §3。

## 参考文献与参考代码库（含许可证）

- `SNR_det`/`SNR_peak`：**本仓 Project-defined 量**（两条定义式见本文件 §2 表，均非外部文献给出，引用时必须随定义同写）。外部文献只提供检测阈的**其它**口径：SExtractor（Bertin & Arnouts 1996, A&AS 117, 393）给出的是 ±3σ 背景裁剪（§2）、去混叠通量比 δ=5·10⁻³（§4.1）与 Kron 半径 k=2/2.5（§6），全文不含 SNR 型定义或阈值公式；DAOPHOT 见 Stetson 1987, PASP 99, 191（Bertin & Arnouts §2 只以一句提及 "in Stetson's DAOPHOT program"，该文本身未著录 Stetson 1987，两条引用各自独立、不得相邻解读为同一来源）。
- 天测残差口径与大圆角距：Greisen & Calabretta 2002, A&A 395, 1061（Paper I）；Calabretta & Greisen 2002, A&A 395, 1077（Paper II）；astropy.wcs（BSD-3-Clause）作独立重建 Oracle。
- Gaia G<18 样本与 1″ 匹配：Gaia DR3（Gaia Collaboration et al. 2023, A&A 674, A1）；匹配半径/统计量的冻结依据见本文件 §3 与 ASTROMETRY §11a。
- MAD→σ 常数：Rousseeuw & Croux 1993, JASA 88, 1273。
- 本表阈值均为 Project-defined 冻结门（阈值来源只此一表）；外部文献只提供量测域语义，不提供门值。



---

## 接缝与 WCS 基线（回归对照）

- **接缝判据**：在**保留公共天光面 `B_ref`** 的前提下比较帧间一致性与边界跳变，残余阶跃门 = **≤ 2×** 参考水平；把整张背景减掉后再比帧间差是**退化判据**（背景归零时差值天然为零），判据只用保留 `B_ref` 的写法（依据 `ACSD_DESIGN.md` §5.4 与 `docs/science/PHASE2_UPM.md`）。
- **WCS 真实数据回归对照**：绝对零点残差的取值与读数正本 = 实验/engineering-evidence/（results）；回归对照不是新增科学门，门表以上文 §3 为唯一来源。

