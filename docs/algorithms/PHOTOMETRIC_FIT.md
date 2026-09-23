# Photometric Fit Algorithms (ALG-PHOT)

> 上游：ASTROCS_DESIGN.md §2.1（创新点一）、§4.4（输出合同）

> ID: ALG-PHOT-001  范围: ALG-PHOT-001..002  上游 SCI: SCI-PHOT-001  状态: DERIVED  模块: photometric_calib

## 1 上游 SCI 与输入输出

- 上游: `SCI-PHOT-001` (r=log10(F_instr/F_syn) IRLS Tukey c=4.685, mag_tolerance=3.0)
- 输入: 仪器流量 `F_instr` + 合成流量 `F_syn` (Gaia XP)
- 输出: `PhotometricCalibrationQuality` (sigma_mag, sigma_cal_rel) + scale（无 zero_point 字段：无定义式、结构体无字段）

## 2 离散公式

```text
F1: r_i = log10(F_instr,i / F_syn,i)
F2: S = MAD(r)/0.6744897501960817, init location=median(r)
F3: IRLS Tukey: u=(r−location)/(c·S), c=4.685, w=(1−u²)² if |u|<1 else 0, location=Σw·r/Σw, iter≤50 tol=1e-6
F4: sigma_residual = MAD(r_inliers)/0.6744897501960817, r_inliers={w>0}
F5: sigma_mag = 2.5·sigma_residual, sigma_cal_rel = ln10·sigma_residual
F6: scale = 10^{−location}
F7: 星等一致性预过滤 |Δ−median(Δ)|>3.0 mag reject where Δ=−2.5·log10(F_instr)−G_Gaia
```

来源: `star_matcher.cpp:21,241-248,478-559`

## 3 伪代码

```text
function photometric_fit(F_instr, F_syn, G_Gaia):
  if n < min_ref → NO_DATA
  Δ_i = −2.5·log10(F_instr)−G_Gaia; median_Δ = median(Δ)
  r_consistent = {i | |Δ_i−median_Δ|≤3.0} → r_i=log10(F_instr/F_syn)
  if S=MAD(r)/0.6744897501960817 ==0 → location=median(r) skip IRLS
  else:
    location=median(r); repeat 50×:
      w_i=(1−((r_i−location)/(c·S))²)² if |u|<1 else 0; c=4.685
      new_loc=Σw·r/Σw; if |new−old|<1e-6 break
  sigma_res=MAD({r|w>0})/0.6744897501960817; scale=10^{−location}
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| `F≤0` / log10 非有限 | skip REJECT |
| `MAD==0` | 跳过IRLS取median |
| `S<0` / n<min | NO_DATA |

## 5 确定性与归约

- 排序 median/MAD 确定性；IRLS 按 r 索引固定顺序加权和，无跨样本归约。

### 5.1 到达顺序不变性（正向约束）

本节把「样本序」定为**显式契约**，而非实现细节。样本序有两类来源，约束强度不同：

| 到达序来源 | 是否允许进入浮点路径 | 判据（事前冻结） |
|---|---|---|
| **PSF 星表序**（`psf_cx/cy/flux/status` 的数组序） | 允许，且**它就是样本序本身**：匹配与 IRLS 的归约序由它唯一决定（`star_matcher.cpp` 的 `matchWithKdTree` 按 PSF 下标 `k=0..n-1` 正向查询；`cleanAndScale` 的 `r_consistent` 按同一序累积） | 置换该序 ⇒ 计数/索引/选择结果**精确一致**；IRLS 归约按 `docs/contracts/TEST_MATRIX.md` §2 的 `C·γ_n·Σ\|terms\| + atol`（`C≤4`）判等价（§5c：顺序变化仅影响 <1ulp） |
| **Gaia 参考星表序**（`gaia_ra/dec/mag/fsyn` 的数组序） | **不得**进入任何浮点累加序 | 置换该序 ⇒ **全输出逐位（bitwise）不变**（含 out_pixels / scale / sigma / diag / 逐星 records） |

理由：匹配是**集合谓词**（互为最近邻），与表的枚举序无关；`matches[]` 的输出序由 PSF 序驱动。
故 Gaia 表序只改变索引标号，任何随它变化的结果都说明该序泄漏进了数值路径 —— 那是缺陷，不是容差问题。

**并列（exact tie）是唯一已知的例外，且当前为「未定义」**：当两颗 Gaia 星到同一 PSF 星的距离**精确相等**时，
最近邻选择退化为「KD-tree 遍历序先到者胜」（`findNearestRec` 的 `dist2 < best_dist2` 严格比较），
而遍历序由建树时的 `std::sort` 结果决定 ⇒ **该情形下匹配伙伴随输入序变化**。
规范口径：**并列构型不在本算法的适用域内**；调用方（星检测/星表）必须保证同一 PSF 星在匹配半径内不存在
精确等距的 Gaia 候选。该例外的存在性由 `p1phot_determinism` 的 N1 负例锁住（判据必须能分辨它）。

**可执行判据**：`lib/algorithms/photometry/tests/p1phot/p1phot_tests_determinism.cpp`
（ctest `p1phot_determinism`）—— 线程数扫描 1/2/4/8（I5 的显式补全）+ 两类到达序置换
+ 两个序相关负例（N1 并列 tie-break / N2 在线归约），正负例共用同一 `verdict` 函数。
**注意与 §13.4 I5 的区别**：I5 只扫线程数、每个线程跑**完全相同的样本序**，因此**抓不到**顺序依赖；
两者互补，不可互相替代。

## 6 复杂度

- O(n_ref log n_ref) 排序 + O(n_ref·iter) IRLS

## 7 CPU-only 后端策略（V5）

- 仅 CPU：IRLS 迭代为全样本顺序归约(样本序固定)，天然确定性；规模小(星数级)无并行收益，worker pool 可行但非必需；**禁止硬编码线程数**。

## 5c SIMD 安全与取消点

- `r_i=log10(F_i/F_syn,i)` 逐星独立(SIMD 安全: 数组连续无别名)；IRLS 加权均值 `Σw·r/Σw` 为**固定样本序归约**(FP64, 禁重结合)——顺序变化仅影响 <1ulp, 仍冻结顺序。
- 取消点: IRLS 迭代间检查; 取消时返回未收敛状态不写 location/scale。

## 8 参考实现/Oracle

- 合成注入偏移恢复 PHOT-001..007 (scale 已知→location=log10 k)

## 9 容差来源

- location tol 1e-6 (IRLS 收敛)，预冻结。

## 10 关联 ARC/API/TST

- API: `pc_api.h: pc_calibrate_simple, pc_calibrate_simple_with_gaia`
- TST: `TST-PHOT-*` 合成注入/鲁棒

## 13 实现锚定与测试设计增补

> 本节内容: §13.1 逐符号实现锚定、§13.2 实现事实（仅 ALG 文档事实层，
> 不改根科学公式；SCI-PHOT-001 docs/science/PHOTOMETRY.md FROZEN
> 共享引用不改动）、§13.3 已登记现状缺陷 DISP-PHOT-001..009、§13.4 冻结测试设计
> TEST-PHOT-DESIGN-001、§13.5 非生产通道与待迁移符号。行号为
> lib/algorithms/photometry/cpp/ 的 grep 实测，后续重构以 grep
> 重锚为准。禁止声明 IMPLEMENTED（迁移落码归 P1-PHOT-IMPL）。

### 13.1 逐符号实现锚定

**ALG-PHOT-001 IRLS-Tukey 稳健零点估计**（lib/algorithms/photometry/cpp/src/star_matcher.cpp）:

| 步骤 | 符号/位置 | 锚 |
|---|---|---|
| 常量 | _MAD_SCALE=0.6744897501960817 / _TUKEY_C=4.685 / _IRLS_MAX_ITER=50 / _IRLS_CONVERGE=1e-6 | :21-27 |
| 有效残差 r_i=log10(F_instr/F_syn) | cleanAndScale 内 r 计算 | :396-403 |
| 星等预过滤 | delta−median_delta 阈值 mag_tolerance | :436-450 |
| 一致集空回退 | scale=1.0/fit_used=0 | :462-475 |
| IRLS 初值 | location=median(r), S=MAD/0.6744897501960817; S=0→median 兜底 | :477-490 |
| IRLS 迭代 | Tukey w=(1−u²)², ≤50 iter, 收敛 1e-6 | :494-525 |
| scale | 10^(−location) | :527-529 |
| inliers/sigma_residual | \|u\|<1; MAD(r_inliers)/0.6744897501960817 | :531-560 |
| diag 填充 | r_median/p90/max, rejected_quality | :578-602 |
| 一站式入口 | matchAndClean(2.0,3.0) | :610-632 |

**ALG-PHOT-002 星等一致性匹配（双向最近邻唯一配对）**（star_matcher.cpp）:

| 步骤 | 符号/位置 | 锚 |
|---|---|---|
| 常量/ctor | StarMatcher 构造 | :172-174 |
| Gaia 投影入帧 | matchWithKdTree 内投影与 in-frame 统计 | :207-219 |
| Gaia KD-tree | KdTree2D::build | :103-124/:230 |
| PSF 有效过滤 | status==0 | :232-245 |
| PSF KD-tree | KdTree2D::build | :253-261 |
| 正向最近邻 | PSF→Gaia | :263-282 |
| 反向最近邻 | Gaia→PSF | :284-297 |
| 唯一配对 | 互为最近邻过滤 + match_radius=2.0 | :299-333 |
| 分阶段 diag | spatial_candidates/unique_matches/rejected_* | :335-368 |

**F_syn 合成测光**（spectrum_integrator.cpp; 生产路径=XPSD 官方解码）:

定义式（权威：`docs/science/PHOTOMETRY.md` §2a，claim `PHOT-FSYN-CANON-001`）：

```text
F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ        # W·m⁻²·nm；F_λ 单位 W·m⁻²·nm⁻¹；λ、dλ 单位 nm
```

**不含** `10^(−0.4·G)`；`G`（Gaia G 星等）不进入 `F_syn`。与官方定义（Gaia DR3 文档 §5.4.1 式 5.41：`⟨f_λ⟩ = ∫f_λ S λ dλ / ∫S λ dλ`）只差与星无关的归一化分母 `∫TQλdλ`，该分母被 `location`/`ZP_syn` 吸收。

| 步骤 | 符号/位置 | 锚 |
|---|---|---|
| Akima 子样条 | akima_interpolate（fill=0） | :44-126 |
| Simpson 1/3 复合（尾 3/8） | simpson_integrate | :128-168 |
| **生产符号（XPSD 绝对口径）** | `compute_f_syn_cached_xpsd`（被积函数 `(byte·flux_mul+flux_min)·λTQ`） | :409-454（:441-447） |
| 滤光片/QE 缓存 | prepare_filter_cache（T、Q 用 Akima 重采样到**完整** 343 点谱网格，区间外 0；权重 = Simpson 系数×T×Q×λ） | :283-380 |
| XPSD 解码 | F(λ)=byte·flux_mul+flux_min（W·m⁻²·nm⁻¹，逐星量化参数） | gaia_client.h:62-64 |
| **非生产通道（历史相对口径）** | `compute_f_syn` / `compute_f_syn_cached`：`∫uint8·T·Q·λdλ × 10^(−0.4·magG)`；仅作数值对拍，**不得**用于生产定标 | :170-281 / :366-406 |

**生产路径的实证锚**（真实 M42 产物，`run/SCI-PHOT-FORMULA-01/evidence/d1_zp_sigma_rederive.json`）：
`zero_point_mag = median_i(magG_i + 2.5·log10 F_syn,i)` 由上式**逐位复现** —— T2/M1 落盘 `−15.126346726632235` vs 复算 `−15.126346726631280`（Δ=9.5e−13，n=2338）；T3/M1 落盘 `−15.123241368129857` vs 复算 `−15.123241368086541`（n=2309）。

**生产编排**（pc_api.cpp）: 校验 :784-804; 退化（无 Gaia/无 PSF/无光谱星/滤光片失败 → scale=1.0/rc=0） :72-98/:808-830/:868-890/:911-923; 自适应锥搜 mag_max_arr{12..16}×5 :836-866; F_syn OpenMP schedule(dynamic,64) 逐星 :928-948; 匹配+清洗 :969-976; 逐星 PcMatchRecord :983-1021; f64 内联像素校正 :1023-1028; run_with_gaia_impl<T> :755-1041（_v2 :1048-1082 / _f64_v2 :1084-1118 封装）; make_dr3sp_id :743-752。

**图像校正**: ImageCorrector::correctImage I_cal=I·scale（image_corrector.cpp:63-77，OpenMP static :74-76）。

**aperture 测光非生产符号**（lib/algorithms/photometry/wrapper_phase1/photometer.cpp，§13.5）: 天空环收集 d∈[sky_inner,sky_outer] :31-42; 背景中值 :47-51; 孔径积分 d²≤r² Σ(pixel−background) :53-62; σ_sky=1.482602218505602·MAD :69-70; flux_error=sqrt(max(sum,0)+n_in·σ_sky²) :72-80; snr :81。

### 13.2 实现事实修订（ALG 文档事实层；不改根科学公式）

- 匹配为**双向最近邻互为最近邻唯一配对**（KD-tree），match_radius=2.0px（§13.1 锚）。
- 清洗为**星等预过滤 + IRLS/Tukey 稳健位置估计**；
  scale=10^(−location)（IRLS 直出），median(F_syn/F_instr) 仅为
  computeScale 残留符号（DISP-PHOT-003）。
- F_syn 网格：生产 `prepare_filter_cache` 路径把 T/Q 重采样到**谱网格本身**（343 点 / 2 nm / 336–1020 nm），`compute_f_syn_cached_xpsd` 在该网格上做复合 Simpson；非生产的 `compute_f_syn` 用重叠区 **1.0 nm** 网格（spectrum_integrator.cpp:247-255）。
- **参考通量口径**：`F_syn = ∫F_λ·T·Q·λ dλ`（W·m⁻²·nm，**不含** `10^(−0.4·G)`；XPSD 解码 `F_λ=byte·flux_mul+flux_min` 已是绝对谱辐照度，实测 `median(m_syn−magG)=−0.0037 mag`）。权威 `docs/science/PHOTOMETRY.md` §2a。
- 生产 XPSD 光谱为 uint8 编码 F(λ)=byte·flux_mul+flux_min（:62-64/:409-454），
  非 float 原始光谱。
- 自适应星等锥搜 mag_max_arr={12,13,14,15,16}（pc_api.cpp:836-866）实际
  覆盖 mag_max 入参（DISP-PHOT-005）。
- 构建现状=cpp/Makefile:11 + build.ps1:9（photometric_calib.dll，链接
  gaia_client.dll），未编入根 CMake 主构建（CMake 集成归 P1-PHOT-IMPL）。
- OpenMP：F_syn 逐星 schedule(dynamic,64)（整数 reduction 次序无关）、
  像素校正 schedule(static) 逐元素——科学结果 bitwise 与线程数无关。

### 13.3 已登记现状缺陷（DISP-PHOT-001..009，登记不改码，整改归 P1-PHOT-IMPL/INT）

- **DISP-PHOT-001**: star_matcher.cpp 头注释（:4-6）与 PhotometricDiag
  注释（photometric_calib.h:32-38 "当前无双向过滤/rejected_ambiguous
  保持 0"）失实——实现为双向互最近邻唯一配对，rejected_ambiguous 实际
  统计（:342-346）。
- **DISP-PHOT-002**: `lib/algorithms/photometry/docs/algorithm.md` 的部分
  表述（暴力最近邻 3px/scale=median/MAD 清洗 σ=3/0.1nm 网格/2D 曲面拟合）
  与现行实现不符；现行事实以本文档 §2/§13.1 为准。
- **DISP-PHOT-003**: ImageCorrector::computeScale（image_corrector.cpp:26-57）
  死代码，生产无调用方。
- **DISP-PHOT-004**: 无取消检查点；无 plan/execute/cancel/inspect 生命周期
  （PHASE1_API_V1 §2 为计划语义）。
- **DISP-PHOT-005**: 入参静默失效——mag_max 被自适应 mag_max_arr 覆盖
  （pc_api.cpp:836-866，:758 形参注释 /*mag_max*/）；pc_calibrate_simple
  的 QE 三参数 (void) 丢弃（:103，头注释 :85-88 声明保留）。
- **DISP-PHOT-006**: PhotometricDiag.rejected_quality 为 invalid+mag+irls
  混合计数不可归因（star_matcher.cpp:580-581）；v2 per-star reject_reason
  才可区分。
- **DISP-PHOT-007**: orchestrator 双通道四调用（:2714-2831）与 registry
  descriptor 占位 ID（module_adapters.cpp:535-552）双轨并存，统一归
  P1-PHOT-INT。
- **DISP-PHOT-008**: Photometer 孔径/天空环全图 O(h·w) 扫描 + 背景中值
  整段排序（photometer.cpp:25-62），未优化且未接管线。
- **DISP-PHOT-009**: 帧级 QA 换算（sigma_mag/sigma_cal_rel）落位
  snr_estimator（snr_phot_cal_quality，noise_model.cpp:305），本模块仅
  登记边界。

### 13.4 冻结测试设计 TEST-PHOT-DESIGN-001（容差不得放宽）

- fixture F1: 合成注入已知乘性偏移 k（location=log10 k 恢复，rtol 1e-4，
  SCI-PHOT-001 §11）；F2: 20% 星等离群注入（Δlocation<0.1 dex）；F3:
  双向唯一配对构造帧（含歧义对，断言 unique_matches 与 rejected_ambiguous）；
  F4: XPSD uint8 光谱解码+1.0nm 积分 NumPy 参考复算（rtol 1e-9）；F5:
  退化输入矩阵（无 Gaia/无 PSF/无光谱星/滤光片失败 → scale=1.0、rc=0、
  records reject_reason 显式）；F6: Photometer aperture 已知通量 + 越界/
  空环/空孔径显式失败（对齐 eng/tests/unit/p1_wcs_phot_test 4 组）。
- 不变量 I1: out_pixels=round-trip(I·scale) bitwise（f64 通道）；I2:
  sigma_residual=MAD(r_inliers)/0.6744897501960817 与逐星 records 残差一致；I3:
  Σdiag.rejected_*+fit_used=unique_matches；I4: records[star_id] 与输入
  psf_star_ids 一一对应；I5: 线程数扫描（1/4/N）科学输出 bitwise 不变；
  I6: r 方向恒为 log10(F_instr/F_syn)。
- 负面矩阵: 空指针/h·w=0/n_gaia=0/PSF 全 status≠0/锥搜失败(rc=−3)/
  handle=null(rc=−2)；断言错误码与退化登记齐全。
- 可执行 TEST-P1-PHOT-001 由 P1-PHOT-TEST 落地后更新矩阵 test 层。

### 13.5 非生产通道与待迁移符号（去留归 P1-PHOT-IMPL 登记）

- ABI 兼容通道保留: pc_calibrate_simple/:103、pc_calibrate_simple_f64/:185、
  pc_calibrate_simple_with_gaia/:153、pc_calibrate_simple_with_gaia_f64/:201
  （with-gaia 系为 v2 封装 :1048 前的实现路径 :154-385/:508-727）；生产主
  路径=v2/_f64_v2。
- ImageCorrector::computeScale（image_corrector.cpp:26-57）=待迁移符号
  （median 回退，无调用方，DISP-PHOT-003）。
- astrocs::phase1::Photometer（lib/algorithms/photometry/wrapper_phase1/photometer.{h,cpp}）
  =aperture 测光待迁移符号（静态库 astrocs_phase1_phot，CMakeLists.txt:429-432，
  单测 eng/tests/unit/p1_wcs_phot_test eng/tests/unit/CMakeLists.txt:305-310，
  未接 orchestrator 管线），aperture 合同并入 lib/algorithms/photometry/
  README.md §9。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- Tukey biweight c=4.685：Beaton & Tukey 1974, Technometrics 16, 147（DOI 10.1080/00401706.1974.10489171）；Mosteller & Tukey 1977。
- MAD→σ 0.6744897501960817：标准正态分位恒等式；Rousseeuw & Croux 1993, JASA 88, 1273。
- 最优提取/统计结构：Horne 1986, PASP 98, 609；Naylor 1998, MNRAS 296, 339。
- 误差口径 FLUXERR/MAGERR：Bertin & Arnouts 1996, A&AS 117, 393（SExtractor）。**差异**：本层 sigma_residual 是逐星定标散度（dex），不是单源通量误差。
- Gaia XP/CALSPEC：Gaia Collaboration et al. 2023, A&A 674, A1；Bohlin, Hubeny & Rauch 2020, AJ 159, 246；Bessell & Murphy 2012, PASP 124, 140。
- Akima 子样条：Akima 1970, J. ACM 17, 589（DOI 10.1145/321607.321609）。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

