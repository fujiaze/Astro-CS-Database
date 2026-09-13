# Photometric Fit Algorithms (ALG-PHOT)

> ID: ALG-PHOT-001  范围: ALG-PHOT-001..002  上游 SCI: SCI-PHOT-001  状态: DERIVED (T203 冻结; V5 ALG-002 重验 2026-08-28)  模块: photometric_calib

## 1 上游 SCI 与输入输出

- 上游: `SCI-PHOT-001` (r=log10(F_instr/F_syn) IRLS Tukey c=4.685, mag_tolerance=3.0)
- 输入: 仪器流量 `F_instr` + 合成流量 `F_syn` (Gaia XP)
- 输出: `PhotometricCalibrationQuality` (sigma_mag, sigma_cal_rel, zero_point) + scale

## 2 离散公式

```text
F1: r_i = log10(F_instr,i / F_syn,i)
F2: S = MAD(r)/0.6745, init location=median(r)
F3: IRLS Tukey: u=(r−location)/(c·S), c=4.685, w=(1−u²)² if |u|<1 else 0, location=Σw·r/Σw, iter≤50 tol=1e-6
F4: sigma_residual = MAD(r_inliers)/0.6745, r_inliers={w>0}
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
  if S=MAD(r)/0.6745 ==0 → location=median(r) skip IRLS
  else:
    location=median(r); repeat 50×:
      w_i=(1−((r_i−location)/(c·S))²)² if |u|<1 else 0; c=4.685
      new_loc=Σw·r/Σw; if |new−old|<1e-6 break
  sigma_res=MAD({r|w>0})/0.6745; scale=10^{−location}
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| `F≤0` / log10 非有限 | skip REJECT |
| `MAD==0` | 跳过IRLS取median |
| `S<0` / n<min | NO_DATA |

## 5 确定性与归约

- 排序 median/MAD 确定性；IRLS 按 r 索引固定顺序加权和，无跨样本归约。

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

## 13 P1-PHOT-DOC 冻结增补（2026-09-07，wave W1，SA-P1-N17）

> 冻结范围: 本节登记 P1-PHOT-DOC 冻结产物——§13.1 逐符号实现锚定、
> §13.2 实现事实修订（仅 ALG 文档事实层，不改根科学公式；SCI-PHOT-001
> docs/science/PHOTOMETRY.md FROZEN T103 2026-08-23 共享引用不改动）、
> §13.3 已登记现状缺陷 DISP-PHOT-001..009、§13.4 冻结测试设计
> TEST-PHOT-DESIGN-001、§13.5 legacy 通道与迁移旧符号。行号均为
> 2026-09-07 grep 实测（lib/photometric_calib/cpp/），后续重构以 grep
> 重锚为准。禁止声明 IMPLEMENTED（迁移落码归 P1-PHOT-IMPL）。

### 13.1 逐符号实现锚定

**ALG-PHOT-001 IRLS-Tukey 稳健零点估计**（lib/photometric_calib/cpp/src/star_matcher.cpp）:

| 步骤 | 符号/位置 | 锚 |
|---|---|---|
| 常量 | _MAD_SCALE=0.6745 / _TUKEY_C=4.685 / _IRLS_MAX_ITER=50 / _IRLS_CONVERGE=1e-6 | :21-27 |
| 有效残差 r_i=log10(F_instr/F_syn) | cleanAndScale 内 r 计算 | :396-403 |
| 星等预过滤 | delta−median_delta 阈值 mag_tolerance | :436-450 |
| 一致集空回退 | scale=1.0/fit_used=0 | :462-475 |
| IRLS 初值 | location=median(r), S=MAD/0.6745; S=0→median 兜底 | :477-490 |
| IRLS 迭代 | Tukey w=(1−u²)², ≤50 iter, 收敛 1e-6 | :494-525 |
| scale | 10^(−location) | :527-529 |
| inliers/sigma_residual | \|u\|<1; MAD(r_inliers)/0.6745 | :531-560 |
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

| 步骤 | 符号/位置 | 锚 |
|---|---|---|
| Akima 子样条 | akima_interpolate（fill=0） | :44-126 |
| Simpson 1/3 复合（尾 3/8） | simpson_integrate | :128-168 |
| F_syn 主实现 | compute_f_syn（重叠区 1.0nm 网格 :247-255; 被积函数 s·t·q·λ :265-271） | :170-281 |
| 滤光片/QE 缓存 | prepare_filter_cache | :283-380 |
| XPSD 解码 | F(λ)=byte·flux_mul+flux_min | :62-64 |
| 生产符号 | compute_f_syn_cached_xpsd | :409-454 |

**生产编排**（pc_api.cpp）: 校验 :784-804; 退化（无 Gaia/无 PSF/无光谱星/滤光片失败 → scale=1.0/rc=0） :72-98/:808-830/:868-890/:911-923; 自适应锥搜 mag_max_arr{12..16}×5 :836-866; F_syn OpenMP schedule(dynamic,64) 逐星 :928-948; 匹配+清洗 :969-976; 逐星 PcMatchRecord :983-1021; f64 内联像素校正 :1023-1028; run_with_gaia_impl<T> :755-1041（_v2 :1048-1082 / _f64_v2 :1084-1118 封装）; make_dr3sp_id :743-752。

**图像校正**: ImageCorrector::correctImage I_cal=I·scale（image_corrector.cpp:63-77，OpenMP static :74-76）。

**aperture 测光旧符号**（lib/phase1/photometry/photometer.cpp，§13.5）: 天空环收集 d∈[sky_inner,sky_outer] :31-42; 背景中值 :47-51; 孔径积分 d²≤r² Σ(pixel−background) :53-62; σ_sky=1.4826·MAD :69-70; flux_error=sqrt(max(sum,0)+n_in·σ_sky²) :72-80; snr :81。

### 13.2 实现事实修订（ALG 文档事实层；不改根科学公式）

- 匹配为**双向最近邻互为最近邻唯一配对**（KD-tree），非 §2/§3 旧稿的
  暴力最近邻；match_radius=2.0px（§13.1 锚）。
- 清洗为**星等预过滤 + IRLS/Tukey 稳健位置估计**，非简单 MAD 3σ 截断；
  scale=10^(−location)（IRLS 直出），median(F_syn/F_instr) 仅为旧符号
  computeScale 残留（DISP-PHOT-003）。
- F_syn 网格为**1.0nm**（spectrum_integrator.cpp:247-255），旧
  lib/photometric_calib/docs/algorithm.md:107/:346 "0.1nm" 失实。
- 生产 XPSD 光谱为 uint8 编码 F(λ)=byte·flux_mul+flux_min（:62-64/:409-454），
  非 float 原始光谱。
- 自适应星等锥搜 mag_max_arr={12,13,14,15,16}（pc_api.cpp:836-866）实际
  覆盖 mag_max 入参（DISP-PHOT-005）。
- 构建现状=cpp/Makefile:11 + build.ps1:9（photometric_calib.dll，链接
  gaia_client.dll），未编入根 CMake 主构建——"编入 CMake"表述如见旧稿
  以本条为准（CMake 集成归 P1-PHOT-IMPL）。
- OpenMP：F_syn 逐星 schedule(dynamic,64)（整数 reduction 次序无关）、
  像素校正 schedule(static) 逐元素——科学结果 bitwise 与线程数无关。

### 13.3 已登记现状缺陷（DISP-PHOT-001..009，登记不改码，整改归 P1-PHOT-IMPL/INT）

- **DISP-PHOT-001**: star_matcher.cpp 头注释（:4-6）与 PhotometricDiag
  注释（photometric_calib.h:32-38 "当前无双向过滤/rejected_ambiguous
  保持 0"）失实——实现为双向互最近邻唯一配对，rejected_ambiguous 实际
  统计（:342-346）。
- **DISP-PHOT-002**: 旧 README v2.0 与 docs/algorithm.md 大面积失实
  （暴力最近邻 3px/scale=median/MAD 清洗 σ=3/0.1nm 网格/v1.0 2D 曲面拟合
  叙述）——README r1 重写修正，legacy docs 仅存档。
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
  descriptor 占位 ID（module_adapters.cpp:531-548）双轨并存，统一归
  P1-PHOT-INT。
- **DISP-PHOT-008**: Photometer 孔径/天空环全图 O(h·w) 扫描 + 背景中值
  整段排序（photometer.cpp:24-62），未优化且未接管线。
- **DISP-PHOT-009**: 帧级 QA 换算（sigma_mag/sigma_cal_rel）落位
  snr_estimator（snr_phot_cal_quality，noise_model.cpp:276），本模块仅
  登记边界。

### 13.4 冻结测试设计 TEST-PHOT-DESIGN-001（容差不得放宽）

- fixture F1: 合成注入已知乘性偏移 k（location=log10 k 恢复，rtol 1e-4，
  SCI-PHOT-001 §11）；F2: 20% 星等离群注入（Δlocation<0.1 dex）；F3:
  双向唯一配对构造帧（含歧义对，断言 unique_matches 与 rejected_ambiguous）；
  F4: XPSD uint8 光谱解码+1.0nm 积分 NumPy 参考复算（rtol 1e-9）；F5:
  退化输入矩阵（无 Gaia/无 PSF/无光谱星/滤光片失败 → scale=1.0、rc=0、
  records reject_reason 显式）；F6: Photometer aperture 已知通量 + 越界/
  空环/空孔径显式失败（对齐 tests/unit/p1_wcs_phot_test 4 组）。
- 不变量 I1: out_pixels=round-trip(I·scale) bitwise（f64 通道）；I2:
  sigma_residual=MAD(r_inliers)/0.6745 与逐星 records 残差一致；I3:
  Σdiag.rejected_*+fit_used=unique_matches；I4: records[star_id] 与输入
  psf_star_ids 一一对应；I5: 线程数扫描（1/4/N）科学输出 bitwise 不变；
  I6: r 方向恒为 log10(F_instr/F_syn)。
- 负面矩阵: 空指针/h·w=0/n_gaia=0/PSF 全 status≠0/锥搜失败(rc=−3)/
  handle=null(rc=−2)；断言错误码与退化登记齐全。
- 可执行 TEST-P1-PHOT-001 由 P1-PHOT-TEST 落地后更新矩阵 test 层。

### 13.5 legacy 通道与迁移旧符号（去留归 P1-PHOT-IMPL 登记）

- 旧 ABI 兼容通道保留: pc_calibrate_simple/:103、pc_calibrate_simple_f64/:185、
  pc_calibrate_simple_with_gaia/:153、pc_calibrate_simple_with_gaia_f64/:201
  （with-gaia 系为 v2 封装 :1048 前旧实现路径 :154-385/:508-727）；生产主
  路径=v2/_f64_v2。
- ImageCorrector::computeScale（image_corrector.cpp:26-57）=迁移旧符号
  （median 回退，无调用方，DISP-PHOT-003）。
- astrocs::phase1::Photometer（lib/phase1/photometry/photometer.{h,cpp}）
  =aperture 测光迁移旧符号（静态库 astrocs_phase1_phot，CMakeLists.txt:429-432，
  单测 tests/unit/p1_wcs_phot_test tests/unit/CMakeLists.txt:305-310，
  未接 orchestrator 管线），aperture 合同并入 lib/photometric_calib/
  README.md §9。
