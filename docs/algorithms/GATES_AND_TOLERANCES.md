# GATES_AND_TOLERANCES — P1 星检测 / PSF / WCS 冻结门表与 SNR 定义

> ID: ALG-GATES-001  状态: FROZEN
> 范围: P1 星检测（star_det）、PSF（dpsf）、WCS（plate solve / ipv）三条支路的
> **全部可执行门与容差**（含端到端坐标契约门）。
> 上游：ASTROCS_DESIGN.md §12.1（科学正确性与三重佐证）、§4.4（输出合同）

> 上游（本表**不新设阈值**，每行阈值必须回指到既有 SCI/ALG 条款或本表标注的标定证据）:
> SCI-P1-STAR-001 §1/§4（docs/science/STAR_DETECTION.md）、SCI-PSF-001 §11
> （docs/science/PSF.md）、SCI-WCS-001 §7/§11（docs/science/ASTROMETRY.md）、
> ALG-STARDET-001 §11.4（docs/algorithms/STAR_DETECTION_ALGORITHMS.md）、
> ALG-STARPSF-001 §11.4（docs/algorithms/STAR_PSF_ALGORITHMS.md）、
> ALG-WCS-001 §11.4（docs/algorithms/PLATESOLVE.md）。
> 标定证据面: 本表各行「来源」列所引 SCI/ALG 条款与对应 `ctest` 目标
> （探针直链本树 sdet+dpsf 静态库 + scipy 独立复算）。
> 机器校验: `eng/tools/check_gates_and_tolerances.py`（本表即机器可校验事实源）。

## 1 规则（冻结）

- **R1 表外阈值禁止**：任何检查器/门/测试注释不得引用本表之外的容差；新增门必须
  先在本表登记（门ID / 判据式 / 量测域 / 统计量 / SNR 定义 / 阈值 / 来源 / 证据）。
- **R2 证据必需**：`发布门=Y` 的行，证据ID 必须解析到可执行目标（`ctest:<test 名>`
  或仓内存在的测试源路径）；无证据者把 `发布门` 置 `N`，证据ID 写 `UNJUSTIFIED`。
- **R3 量测域独立**：`量测域` 不得与「拟合/生成域」重合（禁止自证门；例如
  「7×7 网格上往返 <1e-6 px」既是拟合域又是检验域，不作门，见 G-P1-WCS-RT）。
- **R4 统计量显式**：必须是 `max` / `median` / `p95` / `rms` / `bitwise` / `精确` 之一，
  禁止只写「误差 ≤ x」而不说统计量。
- **R5 SNR 统一**：凡门引用 SNR，必须使用 §2 已登记的定义（`SNR_peak` / `SNR_det`；`SNR_phot` 不属本域），并写明所用列
  （检测侧 `A_fit`=star_det flux 列 / PSF 侧 Moffat4 振幅 `A`）。

## 2 SNR 定义（本域唯一，冻结；补 F-2「全文无 SNR 定义」缺口）

| 名称 | 定义式 | 单位 | 计算面（列/来源） |
|---|---|---|---|
| `SNR_peak` | `A_fit / sigma_bg` | 无量纲 | 检测侧: `A_fit` = 椭圆高斯拟合峰值振幅（star_det `flux` 列，DATA-P1-STAR §17.2:703；**不是**解析积分流量）、`sigma_bg` = 背景噪声 RMS = `bgnoise`（行差分 FnNoise1 族，sdet_api.cpp:440-476）。PSF 侧: `A_fit` = Moffat4 振幅 `A`、`sigma_bg` = `mad·1.482602218505602`（star_measurements 列 [4]/[10]） |
| `SNR_phot` | `F / sigma_F`（Horne 1986） | 无量纲 | 测光域（DATA-P1-SNR §13.4），**与本表门无关**，列此仅作区分 |
| `SNR_det` | `(peak − background) / noise_sigma` | 无量纲 | 检出目录列（`p1_sources.json` 的 `sources[].snr`）：`peak` = **未平滑原图**上检出像素峰值、`background`/`noise_sigma` = 该帧背景与背景 RMS；实现 `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:165`。**与 `SNR_peak` 不同源**：`SNR_peak` 用椭圆高斯拟合振幅 `A_fit`（检测侧 `flux` 列），`SNR_det` 用原始峰值 ⇒ 两列**禁止互换**；凡门写「SNR>x」必须点名用哪一行 |

**SNR_peak 的定义敏感性（必须随门一起读）**：全局检测阈值是
`threshold = median(img) + 5.0·bgnoise`（sdet_api.cpp:1637-1647），作用于
**σ=2 平滑后**的图像 ⇒ 同一合成场在不同「峰值 SNR」下可检出性差异极大：
R-3 §2.9 实测同一 Moffat4 场 `SNR_peak=20` 时 sdet 检出 **0 星**、
`SNR_peak=50` 检出 **36/40**、`SNR_peak=300` 检出 36/40。
⇒ 任何门若写「SNR≥x」，必须同时写 `SNR_peak`（本节定义）与 `x` 的取值域，
否则该门不可判（原 SCI/ALG 文本只写「SNR≥20」而无定义，即 F-2 缺陷本体）。

## 3 冻结门表（机器可校验；列序固定）

| 门ID | 判据式 | 量测域 | 统计量 | SNR/信噪定义 | 阈值 | 阈值来源 | 证据ID | 发布门 |
|---|---|---|---|---|---|---|---|---|
| G-P1-CENTROID-SCI | `\|c_meas − c_truth\|`（px，逐星绝对位置） | 合成高斯星场（已知中心/流量/FWHM），域 = `SNR_peak ≥ 20` 且非饱和非边缘（距边 ≥5 px） | max | SNR_peak（检测侧） | 0.3 px | 解析+标定：R-3 §2.3 CRLB 与蒙特卡洛（SNR_peak=20 ⇒ p95=0.109 px、max=0.137 px，余量 ≥2.2×）；仓内实测 max=0.0535 px（R-3 §3.5） | ctest:p1star_units | Y |
| G-P1-CENTROID-U16 | `\|c_meas − c_truth\|`（px，FP32→uint16 量化通道） | FP32 通道经 uint16 量化（`sdet_detect_ex`），同 G-P1-CENTROID-SCI 的场与域 | max | SNR_peak（检测侧） | 0.5 px | 标定：R-3 §2.6 u16 量化贡献 median 0.0018 / p95 0.0036 / max 0.0056 px ⇒ 余量 ~90×，可达且未超标。**本项只覆盖量化面，不构成端到端位置门**（D-16 重新定性） | ctest:p1star_properties | Y |
| G-P1-CENTROID-1 | `median\|astro_det − truth\| ≤ 0.1 ∧ p95 ≤ 0.3 ∧ \|median_PSF − median_fallback\| ≤ 0.05 ∧ \|median(dpsf_cx − truthIndex)\| ≤ 0.05`（px） | 解析 Moffat4(β=4) 合成场（FWHM 3 px、`SNR_peak`=50/300、256×256×40 星×3 场），**链接 sdet + dpsf + 写读坐标桥真实生产源**；x/y 两轴各断 | median / p95 / median 差 | SNR_peak（检测侧） | 0.1 px / 0.3 px / 0.05 px / 0.05 px | 标定：R-3 §2.9 + 本任务门实测（修复后双支路分离 −0.0000/+0.0001 px、各支路 p95 ≤0.040 px；**修复前分离恒 0.5000 px**，恰卡在 ipv 去重阈值严格 <0.5 之外 ⇒ 同星双份进 ipv） | ctest:p1psf_centroid_gate | Y |
| G-P1-PSF-ORACLE-F64 | `\|Δ中心\|`（px）、`\|ΔA/A\|`、`\|ΔB/B\|` | FP64 通道，独立 Moffat4/Gaussian 复算（不调用生产符号），合成场 | max | n/a（合成无噪声 + 独立 oracle） | 0.05 px / 1e-3 | 解析+设计冻结：ALG-STARDET-001 §11.4 F4 | ctest:p1star_oracle | Y |
| G-P1-PSF-POS-FWHM | `\|Δpos\|`（px）、FWHM 相对误差 | PSF 图像块（合成），域 = 生产初值/收敛初值双构型 | max / 相对 | n/a（合成） | 0.05 px / 1% | 解析：SCI-PSF-001 §11 + ALG-STARPSF-001 §11.4 | ctest:p1psf_oracle | Y |
| G-P1-PSF-SCIPY-ORACLE | 位置（px）/ FWHM 相对误差，独立 `scipy.optimize.curve_fit`（第三方优化器 + 旋转主轴投影参数化，与生产二次型不同源） | 合成无噪声 Moffat4(β=4) 块 ≥3 组参数 + `SNR_peak`=100 噪声面（≥200 次蒙特卡洛） | max（无噪声）/ p95（噪声面） | SNR_peak（检测侧） | 0.05 px / 1% | 解析：SCI-PSF-001 §11 Python 参考承诺（docs/science/PSF.md:98-99） | eng/tests/backend/test_psf_moffat_oracle.py | Y |
| G-P1-STAR-RECALL | 召回率 = 检出真星数 / 注入真星数 | 合成星场，域 = `SNR_peak ≥ 10` | 比例（逐场） | SNR_peak（检测侧） | ≥99% | 设计冻结：ALG-STARDET-001 §11.4 F1 | ctest:p1star_units | Y |
| G-P1-STAR-FP | 虚警密度 | 合成纯噪声场（无注入星，与 G-P1-STAR-RECALL 异场） | 计数密度（每千像素） | n/a | ≤0.1 /千像素 | 设计冻结：ALG-STARDET-001 §11.4 F1 | ctest:p1star_units | Y |
| G-P1-STAR-DET | 输出 bitwise 一致（含 mag 全序 + NaN 末尾 + maxStars 保最亮） | 合成星场，同输入、线程数 1/2/4 | bitwise / 精确 | n/a | 完全相等 | 解析：ALG-STARDET-001 §5 全序与串行归约 | ctest:p1star_properties | Y |
| G-P1-WCS-F1 | `rms_arcsec`（″） | **trans 拟合内点集（n_pairs ≥ 12）+ 合成线性场（order=1，已知 CD/CRVAL/CRPIX）**；**不是**产品级天测精度门 | rms | n/a | 0.5″ | 标定：**无有效标定**——`Galaxy_Center 0.1431″` 在当前版本 T4 帧上不可复现（实测 0.2803–0.3588″），只在 T2/T3 档场复现，为历史值（SCI-WCS-001 §11a）；量测域 = trans 拟合内点集 + 合成线性场 | UNJUSTIFIED（无可执行 F1 目标，落地归 P1-WCS-TEST） | N |
| G-P1-WCS-CLOSURE | `median{d_i : d_i ≤ 1.0″}`（角秒；像素换算 `median_px = median_arcsec/s0`），**必须同报** `n_matched` / `match_rate` / `p95` / `max` | **产品级外部闭环 + 真实帧**：检出星样本 = `x,y` 有限 ∧ `snr>20`（`SNR_det`，超 20000 按 flux 降序截断并记录 `sample_capped`），星表 = 本地 Gaia DR3 XPSD 视场单锥 `G<18`；WCS 口径 solved(CD+SIP) 与 frame_header **分别报告**（`wcs_flavor`），禁止合并比较；独立工具不导入生产代码 | median / p95 / max | `SNR_det`（§2；门槛 20） | 分档阈值 UNJUSTIFIED（实测参考：T2/T3 档 s0≈0.96″/px ⇒ 0.4202–0.5644 px；T4 档 s0≈6.31″/px 见 §4a） | UNJUSTIFIED：口径已冻结（SCI-WCS-001 §11a），**阈值仍未标定**——现有实测值取自 XISF 母版单位未修复的输入，修复后必须整体复跑才可定阈；台账中 0.897 px 一项用 v1.2 工具 × 2 帧、半径/星选/统计量均未冻结（改半径 1″→5″ median 漂 14%），**不可复现，不作门** | UNJUSTIFIED（口径实现 `eng/tools/astrometry/closure_metric.py`） | N |
| G-P1-WCS-CLOSURE-REPRO | 同输入同口径两次运行：`median_px(A) = median_px(B)`（**完全相等**）∧ `n_matched(A) = n_matched(B)`；且每份记录声明的 `n_matched/median/p95/max/match_rate` 必须能由**该记录自带的残差向量 + 声明半径**重新导出，且 `params` 必须逐项等于冻结口径 | **产品级真实帧**（同输入两跑记录）+ 记录面（`params` / 残差向量 / 输入 sha256）；合成场自检同在 ctest 目标内 | median / 精确 | `SNR_det`（§2；样本门槛 20） | 0 px（完全相等）/ `n_matched` 精确相等 | 实测漂移 0：E2E-001 §5.1/§5.3 全链科学面产物逐字节相等、规范哈希全等 ⇒ 同输入同口径指标漂移 = 0；记录内 `1e-9 px` 仅为 JSON 浮点往返护栏，**不是科学容差** | ctest:p1wcs_closure_metric_gate | Y |
| G-P1-WCS-F2 | astropy WCS 前向/逆向 `\|Δ\|`（px） | 合成 SIP 场（order=2，注入已知 A/B），中心 90% 区域 | max | n/a | 1e-4 px | 预冻结（ALG-WCS-001 §8/§11.4 F2 承接，不放宽） | ctest:p1wcs_apbp | Y |
| G-P1-WCS-RT | roundtrip `max‖(x,y) − WCS⁻¹(WCS(x,y))‖`（px） | **独立密集域**：中心 90% + 四边 + 四角 + ≥1000 随机点（**不是**拟合采样网格） | max | n/a | 1e-4 px | 解析+实测：与 ALG-WCS-001 §11.4 F2 同值（SCI/ALG 冻结值，**不放宽**）。「7×7 网格上 <1e-6 px」为**自证门**（自网格 1.8e-12 px vs 离网格 3.10 px），不作门。**分层定位**：本行是**全链冻结门**（判据力弱：实测最坏 2.91e-9 px ⇒ 余量 3.4e4×，与 FP64 地板无关）；迭代反演路径的紧门见 G-P1-WCS-RT-ITER、多项式逆表示门见 G-P1-WCS-RT-APBP（GATE-WCS-01 裁决 2） | ctest:p1wcs_apbp | Y |
| G-P1-WCS-CRPIX | CRPIX 精确相等 | 任意帧 | 精确 | n/a | 精确 = (w/2+0.5, h/2+0.5)（1-based） | 冻结：SCI-WCS-001 §7 CRPIX 不变量 | ctest:p1wcs_apbp | Y |
| G-P1-WCS-BRIDGE | 九宫格（中心 1 + 四角 4 + 四边中点 4，两 parity 共 18 格）逐像素 roundtrip **+ 密集域扫描（全帧 ≤128² 网格 + 四边/四角/中心 + 1024 固定 seed 随机点；9 点采样是密集域子集，dense ≥ nine）** + 第三方 astropy 交叉 + 负向注入（移除/错置 `+1` 桥接必须 ≥1 px 偏差） | 导出边界（`lib/algorithms/projection/p3_wcs.cpp`），1024×1024 帧 | max + 注入必败 | n/a | <1e-8 px（**适用域：scale ≥ min_scale_arcsec = 0.9″/px**；低于该尺度本门不适用，报「超出适用域」而非判红）；注入 ≥1 px 必败 | 推导：TAN 闭式投影截断项**恒等于 0**（逐式证明 G∘F=I；FP80 判别实验误差随 eps 线性缩小 1792–2238× vs 理论 2048×）⇒ 误差 100% 来自 FP64 舍入，主项 `ε ≈ C_env·u·sec²Δ/s_rad`（u=2⁻⁵³，C_env=78 实测 max / 128 设计常数，sec²Δ ≤ 1.03046 @FOV≤20°）；1e-8 px 仅在 s ≥ 0.179″/px（实测常数）/ 0.293″/px（设计常数）时 ≥ 最坏情况包络（sec²Δ≈1；按 FOV=20° 的 sec²Δ=1.03046 取最坏为 0.184/0.302″/px）⇒ **必须配适用域下限 0.9″/px**（覆盖仓内最小真实尺度 0.9586″/px；余量 ≥5.0×，区间包络 ≥2.8×）。合成规则 = 最坏情况包络线性相加（**不用 RSS**：实测包络/RSS = 2.47×）。证据锚 `run/GATE-DERIVE-01/REPORT.md`（27 条自洽断言全 PASS）；历史 FIX-406 全域 880 组几何 max 2.437e-9 px @0.18″/px（`run/FIX-406/SIN_ROUNDTRIP_ORACLE.md`） | ctest:p3_wcs; ctest:p3_projection_registry; ctest:p1wcs_std_f1_bridge_cross | Y |
| G-P1-WCS-BRIDGE-GLOBAL | 同一 STD-F1 桥接量在**全尺度**上的保守门：密集域 roundtrip max < 1e-6 px | 导出边界（`lib/algorithms/projection/p3_wcs.cpp`），任意帧尺度 | max | n/a | <1e-6 px（**全域保守门**，无尺度下限；判据力弱） | 推导：保守性下界 s ≥ 1.79e-3″/px（实测常数）/ 2.93e-3″/px（设计常数，= C_env·u·sec²Δ/s_rad ≤ tol）⇒ 覆盖所有真实仪器（比最细的真实像素尺度还小 1–2 个量级）；代价：0.18″/px 处相对包络余量 101×、相对实测 410× ⇒ 1e-7 量级缺陷会被放过。**与 G-P1-WCS-BRIDGE 是两个不同用途的门，不合并为一个数**（GATE-WCS-01 裁决 4）。来源：SCI-WCS-001 §11 STD-F1（`docs/science/ASTROMETRY.md:163`） | ctest:p3_wcs; ctest:p3_projection_registry | Y |
| G-P1-WCS-RT-ITER | 迭代反演路径（`wcs_sky_to_pixel_iterative`，τ = 1e-9 px）在**独立密集域**（中心 90% + 四边/四角 + 1200 固定 seed 随机点，1024²）上的自洽往返 `max‖(x,y)−F_oracle⁻¹(F_oracle(x,y))‖` | P1 ipv 反演路径（`lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp`）；**独立密集域**（中心 90% + 四边/四角 + 1200 固定 seed 随机点），独立 oracle 前向锚 + 生产反演（不含拟合误差） | max | n/a | 1e-8 px（= κ_iter·τ，κ_iter = 10；τ 写进合同） | 推导：迭代反演地板 = 收敛容差 τ（实测 3.30e-10 / 1.66e-9 / 2.91e-9 px 三档畸变，最坏 = 2.91e-9 px = 2.9·τ ⇒ 余量 3.4×）；τ↓1000× 仅使误差↓4.23× ⇒ 另有 ~4e-10 px 的 FP64/迭代结构地板，故取 κ_iter=10 而非 1。**分层理由**：现行 1e-4 px（G-P1-WCS-RT）对迭代路径过松 4 个量级（GATE-WCS-01 裁决 2）。证据锚 `run/GATE-DERIVE-01/REPORT.md` §5.4/§6.3 | ctest:p1wcs_apbp | Y |
| G-P1-WCS-RT-APBP | APx/BPx 多项式逆**一步直加表示残差** `max‖UV + APx(UV) − u*‖`（u* = 独立 oracle 反演） | P1 SIP 多项式逆路径；**合成畸变场** fixture（FIX-WCS-F 三档，边缘畸变预算 18.4/91.8/183.6 px），中心 90% 网格 | max | n/a | ≤ 10% × 边缘畸变预算（px）；**表示门，不是 FP64 地板门** | 推导：多项式表示残差随畸变预算陡增（实测 low 1.9e-4% / mid 0.036% / high 1.89% ⇒ 最坏档余量 5.3×）；~184 px 畸变下多项式逆**根本达不到 1e-4 px**（实测 3.46 px）⇒ 必须与 τ 门分开登记（GATE-WCS-01 裁决 2；owner 裁决 1 选 B 2026-09-09）。真值无效应⇒归零：零畸变 fixture 残差 2.61e-11 px（比 high 档小 1.3e11×）；等价缺陷（APx 清零）⇒ 202.2 px 必红 | ctest:p1wcs_apbp | Y |
| G-P1-CENTROID-BRANCH-ORDER | `star_measurements` 写端契约：PSF 支路**恒等**（dpsf 输出即 index-is-center）、fallback 支路 `−0.5`（sdet 连续系）；读端统一 `+0.5` | 契约函数级（`star_coord_contract.h`）+ 端到端（G-P1-CENTROID-1） | 精确 / max | n/a | 精确相等（fallback 输出 − sdet 原始坐标 = 0） | 解析：DATA-P1-STAR §17.2 + dpsf 采样式 dpsf_psf.cpp:295 / sdet 采样式 sdet_api.cpp:127-131（R-3 §3.1/§3.4 实测） | ctest:p1psf_centroid_gate | Y |

## 4 诊断脚本（**不是门**，R1/R2 约束下不得作为发布门）

| 脚本 / 阈值 | 现状 | 处置 |
|---|---|---|
| `lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py` 的 `fwhm_median_le_1pct` / `ell_median_le_0.005` / `flux_median_le_1pct` / `photutils_oracle_centroid_p95_le_0.05px` | Windows 专用脚本，未注册进 `eng/ci/checks.json`（`grep -c gate2 eng/ci/checks.json` = 0），4 个阈值在活动 `docs/**` 零命中（R-3 §3.5） | 降级为**诊断脚本**：不得引用为发布门；如需升格，必须先在 §3 登记门ID/域/统计量/来源并注册 CI 检查（转 CI-003） |

## 4a 台账实测值（**XISF 母版单位修复后须整体复跑**）

> 口径 = §3 的 G-P1-WCS-CLOSURE v1（样本 `snr>20`、星表 `G<18`、半径 1″、median，
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
- **采样口径对量值的影响（如实登记）**：同一帧同一 WCS，仅把样本从「flux 前 20000」
  换成 §3 冻结样本 ⇒ T2 solved median 0.4449→0.4202 px（−5.6%）、
  matched 1497→705（−53%）；T3 0.5642→0.5644 px（+0.04%）、matched 874→875；
  T4 0.1075→0.1077 px、matched 5608→6265。探针口径见 §3。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- SNR_det/SNR_peak 与检测阈：Bertin & Arnouts 1996, A&AS 117, 393（SExtractor）；Stetson 1987, PASP 99, 191（DAOPHOT）。
- 天测残差口径与大圆角距：Greisen & Calabretta 2002, A&A 395, 1061（Paper I）；Calabretta & Greisen 2002, A&A 395, 1077（Paper II）；astropy.wcs（BSD-3-Clause）作独立重建 Oracle。
- Gaia G<18 样本与 1″ 匹配：Gaia DR3（Gaia Collaboration et al. 2023, A&A 674, A1）；匹配半径/统计量的冻结依据见本文件 §3 与 ASTROMETRY §11a。
- MAD→σ 常数：Rousseeuw & Croux 1993, JASA 88, 1273。
- 本表阈值均为 Project-defined 冻结门（不得引用表外阈值）；外部文献只提供量测域语义，不提供门值。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。


---

## 接缝与 WCS 基线（回归对照）

- **接缝判据**：在**保留公共天光面 `B_ref`** 的前提下比较帧间一致性与边界跳变，残余阶跃门 = **≤ 2×** 参考水平；把整张背景减掉后再比帧间差是**退化判据**（背景归零时差值天然为零），不得使用（依据 `ASTROCS_DESIGN.md` §5.4 与 `docs/science/PHASE2_UPM.md`）。
- **WCS 真实数据基线值**：绝对零点残差 **0.1028″**（典型）/ **0.3242″**（最坏）。二者是既有实测**基线**，用于回归对照与验收叙事，**不是**新增科学门；门表以上文 §3 为唯一来源。

