# GATES_AND_TOLERANCES — P1 星检测 / PSF / WCS 冻结门表与 SNR 定义（F-2 整改）

> ID: ALG-GATES-001  状态: FROZEN（SCI-FIX-PSF 第 9 项 F-2 整改，2026-09-16）
> 范围: P1 星检测（star_det）、PSF（dpsf）、WCS（plate solve / ipv）三条支路的
> **全部可执行门与容差**（含端到端坐标契约门）。
> 上游（本表**不新设阈值**，每行阈值必须回指到既有 SCI/ALG 条款或本表标注的标定证据）:
> SCI-P1-STAR-001 §1/§4（docs/science/STAR_DETECTION.md）、SCI-PSF-001 §11
> （docs/science/PSF.md）、SCI-WCS-001 §7/§11（docs/science/ASTROMETRY.md）、
> ALG-STARDET-001 §11.4（docs/algorithms/STAR_DETECTION_ALGORITHMS.md）、
> ALG-STARPSF-001 §11.4（docs/algorithms/STAR_PSF_ALGORITHMS.md）、
> ALG-WCS-001 §11.4（docs/algorithms/PLATESOLVE.md）。
> 标定证据面: `reports/PROJECT-GOVERNANCE-01/research/R-3_PSF质心科学门与容差.md`
> 与 `run/PROJECT-GOVERNANCE-01/R-3/**`（探针直链本树 sdet+dpsf 静态库 +
> scipy 独立复算）、`run/PROJECT-GOVERNANCE-01/SCI-FIX-PSF/**`（本任务门红绿证据）。
> 机器校验: `tools/check_gates_and_tolerances.py`（本表即机器可校验事实源）。

## 1 规则（冻结）

- **R1 表外阈值禁止**：任何检查器/门/测试注释不得引用本表之外的容差；新增门必须
  先在本表登记（门ID / 判据式 / 量测域 / 统计量 / SNR 定义 / 阈值 / 来源 / 证据）。
- **R2 证据必需**：`发布门=Y` 的行，证据ID 必须解析到可执行目标（`ctest:<test 名>`
  或仓内存在的测试源路径）；无证据者把 `发布门` 置 `N`，证据ID 写 `UNJUSTIFIED`。
- **R3 量测域独立**：`量测域` 不得与「拟合/生成域」重合（禁止自证门；反例：原
  SCI-WCS-001 §7「7×7 网格上往返 <1e-6 px」既是拟合域又是检验域，已由
  SCI-FIX-PSF 第 8 项废止，见 G-P1-WCS-RT）。
- **R4 统计量显式**：必须是 `max` / `median` / `p95` / `rms` / `bitwise` / `精确` 之一，
  禁止只写「误差 ≤ x」而不说统计量。
- **R5 SNR 统一**：凡门引用 SNR，必须使用 §2 的 `SNR_peak` 定义，并写明所用列
  （检测侧 `A_fit`=star_det flux 列 / PSF 侧 Moffat4 振幅 `A`）。

## 2 SNR 定义（本域唯一，冻结；补 F-2「全文无 SNR 定义」缺口）

| 名称 | 定义式 | 单位 | 计算面（列/来源） |
|---|---|---|---|
| `SNR_peak` | `A_fit / sigma_bg` | 无量纲 | 检测侧: `A_fit` = 椭圆高斯拟合峰值振幅（star_det `flux` 列，DATA-P1-STAR §17.2:703；**不是**解析积分流量）、`sigma_bg` = 背景噪声 RMS = `bgnoise`（行差分 FnNoise1 族，sdet_api.cpp:440-476）。PSF 侧: `A_fit` = Moffat4 振幅 `A`、`sigma_bg` = `mad·1.4826`（star_measurements 列 [4]/[10]） |
| `SNR_phot` | `F / sigma_F`（Horne 1986） | 无量纲 | 测光域（DATA-P1-SNR §13.4），**与本表门无关**，列此仅作区分 |

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
| G-P1-PSF-SCIPY-ORACLE | 位置（px）/ FWHM 相对误差，独立 `scipy.optimize.curve_fit`（第三方优化器 + 旋转主轴投影参数化，与生产二次型不同源） | 合成无噪声 Moffat4(β=4) 块 ≥3 组参数 + `SNR_peak`=100 噪声面（≥200 次蒙特卡洛） | max（无噪声）/ p95（噪声面） | SNR_peak（检测侧） | 0.05 px / 1% | 解析：SCI-PSF-001 §11 Python 参考承诺（docs/science/PSF.md:98-99） | tests/backend/test_psf_moffat_oracle.py | Y |
| G-P1-STAR-RECALL | 召回率 = 检出真星数 / 注入真星数 | 合成星场，域 = `SNR_peak ≥ 10` | 比例（逐场） | SNR_peak（检测侧） | ≥99% | 设计冻结：ALG-STARDET-001 §11.4 F1 | ctest:p1star_units | Y |
| G-P1-STAR-FP | 虚警密度 | 合成纯噪声场（无注入星，与 G-P1-STAR-RECALL 异场） | 计数密度（每千像素） | n/a | ≤0.1 /千像素 | 设计冻结：ALG-STARDET-001 §11.4 F1 | ctest:p1star_units | Y |
| G-P1-STAR-DET | 输出 bitwise 一致（含 mag 全序 + NaN 末尾 + maxStars 保最亮） | 合成星场，同输入、线程数 1/2/4 | bitwise / 精确 | n/a | 完全相等 | 解析：ALG-STARDET-001 §5 全序与串行归约 | ctest:p1star_properties | Y |
| G-P1-WCS-F1 | `rms_arcsec`（″） | **trans 拟合内点集（n_pairs ≥ 12）+ 合成线性场（order=1，已知 CD/CRVAL/CRPIX）**；**不是**产品级天测精度门 | rms | n/a | 0.5″ | 标定：Galaxy_Center 实场锚 0.1431″（ALG-WCS-001 §11.4 注，memory.md 2026-07-12）；量测域由 SCI-FIX-PSF 第 4 项冻结 | UNJUSTIFIED（无可执行 F1 目标，落地归 P1-WCS-TEST） | N |
| G-P1-WCS-CLOSURE | 全帧头域残差 median / p95（独立工具，不导入 to_astropy_wcs、不读 wcs_result.*） | 产品级外部闭环：全帧匹配星（非内点集），真实帧 | median / p95 | n/a | 阈值待标定（UNJUSTIFIED） | UNJUSTIFIED：需另行标定；台账值 0.897 px 系 v1.2 legacy 工具 × 2 帧，量测域与门均不同，**不作为 F1 超标证据**（R-3 §4.3） | UNJUSTIFIED（证据面，非发布门） | N |
| G-P1-WCS-F2 | astropy WCS 前向/逆向 `\|Δ\|`（px） | 合成 SIP 场（order=2，注入已知 A/B），中心 90% 区域 | max | n/a | 1e-4 px | 预冻结（ALG-WCS-001 §8/§11.4 F2 承接，不放宽） | ctest:p1wcs_apbp | Y |
| G-P1-WCS-RT | roundtrip `max‖(x,y) − WCS⁻¹(WCS(x,y))‖`（px） | **独立密集域**：中心 90% + 四边 + 四角 + ≥1000 随机点（**不是**拟合采样网格） | max | n/a | 1e-4 px | 解析+实测：与 ALG-WCS-001 §11.4 F2 同值。原「7×7 网格上 <1e-6 px」为**自证门**（自网格 1.8e-12 px vs 离网格 3.10 px，R-3 §2.7），已废止（SCI-FIX-PSF 第 8 项） | ctest:p1wcs_apbp | Y |
| G-P1-WCS-CRPIX | CRPIX 精确相等 | 任意帧 | 精确 | n/a | 精确 = (w/2+0.5, h/2+0.5)（1-based） | 冻结：SCI-WCS-001 §7 CRPIX 不变量 | ctest:p1wcs_apbp | Y |
| G-P1-WCS-BRIDGE | 九宫格（中心 1 + 四角 4 + 四边中点 4，两 parity 共 18 格）逐像素 roundtrip + 第三方 astropy 交叉 + 负向注入（移除/错置 `+1` 桥接必须 ≥1 px 偏差） | 导出边界（`lib/phase3_session/p3_wcs.cpp`），1024×1024 帧 | max + 注入必败 | n/a | <1e-6 px；注入 ≥1 px 必败 | 标定：STD-F1 实测 3.2e-10 px、astropy 前向 ≤7.7e-14°（SCI-WCS-001 §5a） | ctest:p1wcs_std_f1_bridge_cross | Y |
| G-P1-CENTROID-BRANCH-ORDER | `star_measurements` 写端契约：PSF 支路**恒等**（dpsf 输出即 index-is-center）、fallback 支路 `−0.5`（sdet 连续系）；读端统一 `+0.5` | 契约函数级（`star_coord_contract.h`）+ 端到端（G-P1-CENTROID-1） | 精确 / max | n/a | 精确相等（fallback 输出 − sdet 原始坐标 = 0） | 解析：DATA-P1-STAR §17.2 + dpsf 采样式 dpsf_psf.cpp:295 / sdet 采样式 sdet_api.cpp:127-131（R-3 §3.1/§3.4 实测） | ctest:p1psf_centroid_gate | Y |

## 4 诊断脚本（**不是门**，R1/R2 约束下不得作为发布门）

| 脚本 / 阈值 | 现状 | 处置 |
|---|---|---|
| `lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py` 的 `fwhm_median_le_1pct` / `ell_median_le_0.005` / `flux_median_le_1pct` / `photutils_oracle_centroid_p95_le_0.05px` | Windows 专用脚本，未注册进 `ci/checks.json`（`grep -c gate2 ci/checks.json` = 0），4 个阈值在活动 `docs/**` 零命中（R-3 §3.5） | 降级为**诊断脚本**：不得引用为发布门；如需升格，必须先在 §3 登记门ID/域/统计量/来源并注册 CI 检查（转 CI-003） |

## 5 变更记录

| 日期 | 变更 | 依据 |
|---|---|---|
| 2026-09-16 | 建表（F-2）：登记本域 16 条门 + 1 个 SNR 定义；新增 G-P1-CENTROID-1（端到端绝对位置门）、G-P1-PSF-SCIPY-ORACLE、G-P1-CENTROID-BRANCH-ORDER；补 G-P1-WCS-F1 量测域；废止 G-P1-WCS-RT 的 7×7 自网格口径；gate2 四阈值降级为诊断 | R-3 §4.1/§4.2/§4.3/§4.7/§5.6；SCI-FIX-PSF 第 1/2/3/4/6/8/9 项 |
