# lib/photometric_calib — 模块合同（P1-PHOT-DOC）

> 状态: **CONTRACT_READY**（P1-PHOT-DOC 冻结，2026-09-07，wave W1；实现已存在，
> 模块化迁移落码归 P1-PHOT-IMPL；禁止声明 IMPLEMENTED）
> 文档版本: r1（本 README 重写取代 v2.0 旧 README——旧文"暴力最近邻 3px /
> scale=median(F_syn/F_instr)/MAD 清洗"等表述与现行源码不符，见 §10 与
> memory.md；合同冻结以本版为准）
> 权威来源: SCI=docs/science/PHOTOMETRY.md（SCI-PHOT-001，FROZEN T103
> 2026-08-23，共享引用不改动）；ALG=docs/algorithms/PHOTOMETRIC_FIT.md
> （ALG-PHOT-001..002 + §13 逐符号源码锚定）；DATA=docs/contracts/
> DATA_SEMANTICS.md §14（DATA-P1-PHOT）；API=docs/contracts/PUBLIC_API.md
> （API-PHOT-001）；矩阵行=docs/traceability/TRACEABILITY_MATRIX.json
> MOD-astrocs-phase1-photometry。
> 唯一权威签名头: lib/photometric_calib/cpp/include/photometric_calib.h
> （271 行；禁止手抄他版）。

## 1. 身份

| 项 | 值 |
|---|---|
| 矩阵行 | P1-PHOT（MODULE_MIGRATION_MATRIX.csv） |
| module_id | `astrocs.p1.photometry` |
| owner | SA-P1-N17 |
| 迁移目标 DLL | `astrocs_p1_photometry.dll`（合同值，尚未建立；现状构建产物 `photometric_calib.dll`，见 §8） |
| depends_on_int | P1-PSF-INT;P1-WCS-INT |
| module_status | CONTRACT_READY |
| entrypoint | **MISSING**（registry 入口未接；lib/core/src/module_adapters.cpp:469 `p1_photometry_descriptor` 持占位 ID，由 P1-PHOT-INT 对齐本合同，不得反向作为冻结依据） |
| legacy_paths | `lib/photometric_calib`（生产实现 C ABI DLL，本合同主落位）+ `lib/phase1/photometry`（aperture 测光旧符号，计划迁移，见 §9） |

落位依据：MODULE_MIGRATION_MATRIX.csv P1-PHOT 行 legacy_paths 第一路径即
lib/photometric_calib/（生产实现所在），本目录三件套
README.md/module.yaml/memory.md 为该模块合同冻结唯一落位；lib/phase1/
photometry/ 为第二 legacy 路径（旧符号 Photometer，CMakeLists.txt:429-432
静态库 astrocs_phase1_phot，未接 orchestrator 管线，仅单测
tests/unit/p1_wcs_phot_test），其合同并入本 README §9，不另立目录。

## 2. 负责范围 / 不负责

**负责**：帧级测光定标——(a) 合成测光 F_syn（Gaia DR3SP uint8 光谱 × 滤光片
T(λ) × CCD QE Q(λ) 的 Akima+Simpson 积分，XPSD 官方解码）；(b) Gaia 参考
星 WCS TAN+SIP 投影与 PSF 星双向最近邻唯一配对（KD-tree）；(c) 星等一致性
预过滤 + IRLS/Tukey 稳健零点估计（r_i=log10(F_instr/F_syn)）→
scale=10^(−location) 与 sigma_residual=MAD(r_inliers)/0.6745；(d) 逐星
PcMatchRecord 残差/拒绝原因回传；(e) 全局乘性校正 I_cal=I·scale；
(f) PhotometricDiag 17 字段分阶段诊断。

**不负责**（边界，禁止越界）：不产生逐像素 ivar（SCI-PHOT-001 边界；帧级
QA 换算 sigma_mag/sigma_cal_rel 现状由 snr_estimator 的 snr_phot_cal_quality
承担，noise_model.cpp:276，见 DISP-PHOT-009）；不做天光/梯度曲面拟合
（v1.0 已封存，禁止复活）；不做 PSF 拟合本身（上游 PSF 模块供 [N,9] 块）；
不做星点检测；不执行单位换算到 mag 空间（仅输出 dex）。

## 3. 输入输出（DATA-P1-PHOT，DATA_SEMANTICS §14 唯一权威）

生产通道（pc_calibrate_simple_with_gaia_v2 / _f64_v2，orchestrator 实际调用）：

| 方向 | 数据 | dtype/shape | 单位/域 |
|---|---|---|---|
| 入 | pixels | float32(v2)/float64(f64_v2) `[h·w]` 行主序 | ADU |
| 入 | psf 块（orchestrator 拆列） | psf_cx/cy/flux double `[n_psf]`、psf_status int32 `[n_psf]` | pixel / ADU / 0=ok |
| 入 | Gaia 锥形搜索结果（DLL 内经 gaia_client） | ra/dec/magG double、光谱 uint8 `[n_gaia·stride]` + flux_min/flux_mul | deg/deg/mag/W·m⁻²·nm⁻¹ 编码 |
| 入 | filter_wl/trans、qe_wl/trans | double `[count]` | nm / [0,1] |
| 入 | spectrum_wl | double `[343]`（336..1020nm step 2nm） | nm |
| 入 | WCS/SIP | crval/crpix/CD4 元 + sip_order(≤2)/a/b/ap/bp `[36]`（i*6+j） | deg/pixel |
| 出 | out_pixels | 同输入 dtype `[h·w]` | ADU（I_cal=I·scale） |
| 出 | out_scale_factor | double 标量 | 无量纲乘性因子 |
| 出 | out_sigma_residual | double 标量 | dex（log10 flux-ratio） |
| 出 | out_n_matched | int32 标量 | 颗 |
| 出 | out_diag（PhotometricDiag） | 17 字段 | 计数/dex/pixel |
| 出 | out_records（PcMatchRecord `[n_psf]`） | star_id/dr3sp_id/reference_flux/residual/status/reject_reason | ADU / dex |

直通通道（pc_calibrate_simple，F_syn 由调用方外部传入 gaia_fsyn；QE 参数
声明但不用——DISP-PHOT-005）。端口编目（psf/sources/fluxes→DATA-P1-PSF/
DATA-P1-SOURCES/DATA-P1-FLUX）为编排层词汇（descriptor），模块合同 DATA
层=DATA-P1-PHOT（§14）。

## 4. 算法（ALG-PHOT-001..002）

权威逐符号锚定=docs/algorithms/PHOTOMETRIC_FIT.md §13.1。摘要：

- **ALG-PHOT-002 星等一致性匹配与 QA**：WCS TAN+SIP 投影 Gaia 星入帧
  （wcs_transform.cpp:207-229）→ Gaia/PSF 两棵 KD-tree（star_matcher.cpp
  :230/:261）→ 双向最近邻互为最近邻唯一配对（正向 :263-282、反向
  :284-297、唯一配对 :299-333）→ 星等预过滤 |delta−median_delta|>3.0 mag
  拒绝（:436-450）→ 分阶段 diag（:335-368/:578-602）。
- **ALG-PHOT-001 IRLS-Tukey 零点估计**：r_i=log10(F_instr/F_syn)
  （:396-403）→ location0=median(r)、S=MAD(r)/0.6745（:477-485；S=0 直接
  median 兜底 :488-490）→ IRLS Tukey biweight c=4.685、w=(1−u²)²、迭代≤50、
  收敛 1e-6（:494-525）→ scale=10^(−location)（:527-529）→
  sigma_residual=MAD(r_inliers)/0.6745（:551-560）。
- **F_syn 合成测光**（生产=XPSD 官方解码）：
  compute_f_syn_cached_xpsd（spectrum_integrator.cpp:409-454，
  F(λ)=byte·flux_mul+flux_min 线性解码）→ 滤光片/QE 缓存重采样
  prepare_filter_cache（:283-380）→ Akima 子样条（fill=0，:44-126）+
  Simpson 1/3 复合（末尾奇数区间 3/8，:128-168）在重叠区 1.0nm 均匀网格
  （:247）积分 F_syn=∫F(λ)·T(λ)·Q(λ)·λdλ。
- **图像校正**：I_cal=I·scale（image_corrector.cpp:63-77；f64 通道内联
  pc_api.cpp:1023-1028）。

冻结语义（禁止改动）：r 方向=F_instr/F_syn；c=4.685/max_iter=50/tol=1e-6；
mag_tolerance=3.0；match_radius=2.0px（DISP-PHOT-002：旧 README 3px 失实）；
S=0→median；SCI-PHOT-001 §10 不可接受变化清单全文适用。

## 5. API（API-PHOT-001）

PUBLIC_API.md「Photometric C API（API-PHOT-001）」节为唯一合同；6 个导出
符号（photometric_calib.h 实测行号）：

| 符号 | 头锚 | 说明 |
|---|---|---|
| `pc_calibrate_simple` | :103-117 | 直通版（F_syn 外传） |
| `pc_calibrate_simple_with_gaia` | :153-183 | DLL 内锥形搜索+积分（ABI 兼容封装） |
| `pc_calibrate_simple_f64` | :185-199 | FP64 直通版 |
| `pc_calibrate_simple_with_gaia_f64` | :201-225 | FP64 with-gaia（封装） |
| `pc_calibrate_simple_with_gaia_v2` | :227-245 | per-star PcMatchRecord（生产主路径，float32） |
| `pc_calibrate_simple_with_gaia_f64_v2` | :247-265 | per-star（float64） |

结构体：PhotometricDiag（:21-45，17 字段）、PcMatchRecord（:47-59，
status 0=unmatched/1=matched+used/2=matched+rejected/3=psf-invalid，
reject_reason 0..6）。返回码：0=成功（含退化恒等校正）；−1=空指针/参数
无效/滤光片或光谱参数非法；−2=gaia_client_handle 为空；−3=锥形搜索失败。
gaia_client_handle 为 opaque borrow（调用方创建/销毁）；out_* 调用方分配；
spec_stars/spectra_buf 内部 malloc 本调用内 free。编排级合同 API-P1-005
（PHASE1_API_V1，descriptor 引用）与本模块 C API 合同并行不互斥。

## 6. 错误语义

- 输入校验失败 → 返回负值（上表）；DLL 内 fprintf(stderr) 诊断 + LOG_INFO。
- 退化路径不失败、显式登记：无 Gaia 星/无 PSF 星/锥形搜索无光谱星/滤光片
  预处理失败 → scale=1.0、n_matched=0、sigma_residual=0、恒等校正、rc=0
  （pc_api.cpp:72-98/:808-830/:868-890/:911-923）；PcMatchRecord 相应行
  status/reject_reason 显式置值（5=unmatched-other、6=psf-invalid）。
- Photometer 旧符号（§9）失败显式：Result::fail(ErrorDomain::DATA) 或
  valid=false+failure_reason（"center out of bounds"/"no sky annulus
  pixels"/"aperture empty"），不留貌似有效结果。

## 7. 并发与确定性

- 现状：F_syn 积分 OpenMP `parallel for schedule(dynamic,64)` 逐星独立
  （pc_api.cpp:931-940）；像素校正 `schedule(static)` 逐元素
  （:1023-1028/image_corrector.cpp:74-76）；匹配与 IRLS 单线程顺序
  （star_matcher.cpp 无 omp）。逐元素与逐星独立 ⇒ 输出 bitwise 与线程数
  无关（n_valid_fsyn 为整数 reduction，次序无关精确）。
- 线程数取 `omp_get_max_threads()`（:54/:175/:415 等），与 ThreadBudget/
  host_executor_lease 无接线——迁移整改点（module.yaml threading_model=
  host_executor_lease 为合同值）。
- 取消：无取消检查点（DISP-PHOT-004；PHASE1_API_V1 §2 plan/execute/
  cancel/inspect 为计划语义，落码归 P1-PHOT-IMPL）。
- determinism=fixed_reduction_order：FP64 全链路（f32 输入升 double），
  禁止重结合；网格/迭代次序固定。

## 8. 构建与生产接线（实测）

- 构建：lib/photometric_calib/cpp/Makefile（:11 TARGET=photometric_calib.dll，
  g++ -shared -fopenmp，链接 ../../gaia_xpsd_client/gaia_client.dll，全静态
  -static -lm）+ cpp/build.ps1（:9 MinGW g++ 通道）；**未编入根 CMake 主
  构建**（根 CMakeLists.txt 无 photometric_calib 目标，grep rc=1）——CMake
  集成归 P1-PHOT-IMPL。
- DLL 装载：lib/orchestrator/cpp/src/dll_loader.cpp:40（ModuleId::PHOTOMETRIC
  → "photometric_calib.dll"）、:54（子目录 lib/photometric_calib/cpp/）；
  预加载依赖 gaia_client.dll（:271-281）。
- 生产调用：orchestrator.cpp:2474 `run_stage_photometric`（PHOTOMETRIC 为
  必需 stage，DLL 未加载即失败退出码 2）；函数指针
  pc_calibrate_simple_with_gaia_f64_v2（:2714）与 _v2（:2790）双通道；读
  psf 块 [N,9]（:2560 起）与 sources/AIO 块，写 photo_stats KV 块
  （STATUS/N_MATCHED/SCALE_FACTOR/SIGMA_RESIDUAL + PhotometricDiag 17 字段，
  :2902-2935）。
- 帧级 QA 下游：sigma_residual(dex) → snr_estimator `snr_phot_cal_quality`
  （snr_estimator.h:47-48，noise_model.cpp:276）换算 sigma_mag/sigma_cal_rel。

## 9. 计划迁移旧符号（lib/phase1/photometry，去留归 P1-PHOT-IMPL 登记）

- `astrocs::phase1::Photometer`（photometer.h:24 类、:26 ctor 默认
  aperture 4.0px/annulus 6.0-10.0px；photometer.cpp:12）：天空环中值背景
  （:31-51）+ 孔径积分 Σ(pixel−background)（:53-62，d²≤r²）+ 简化误差
  flux_error=sqrt(max(sum,0)+n_in·σ_sky²)（:72-80，σ_sky=1.4826·MAD）+
  snr（:81）。失败显式三态（§6）。
- 构建现状：静态库 astrocs_phase1_phot（CMakeLists.txt:429-432），主程序
  链接（:513），单测 tests/unit/p1_wcs_phot_test.cpp（tests/unit/
  CMakeLists.txt:305-310，4 组：已知通量/越界失败/积分回归/显式失败）。
  **未接入 orchestrator 管线**（grep 实测无生产调用方）。
- 单位域：photometer 输入图像 ADU、background ADU/px、flux ADU（孔径内
  和）、flux_error ADU、snr 无量纲——与 DATA-P1-PHOT §14 同域登记。
- 去留：作为 aperture 测光合同并入本模块（albeit 独立 DLL 目标未定），
  接线与消缺（全图扫描 O(h·w)→局部窗口，DISP-PHOT-008）归 P1-PHOT-IMPL。

## 10. 已登记现状缺陷（DISP-PHOT-001..009，登记不改码，整改归 P1-PHOT-IMPL/INT）

| ID | 缺陷 | 锚 |
|---|---|---|
| DISP-PHOT-001 | star_matcher.cpp 头注释与 PhotometricDiag 注释"当前无双向过滤"失实——实现为双向最近邻互最近邻唯一配对，rejected_ambiguous 实际统计 | star_matcher.cpp:4-6/:34-38(头) vs :263-333/:342-346；photometric_calib.h:32-38 |
| DISP-PHOT-002 | 旧 README/docs/algorithm.md 大面积失实（暴力最近邻 3px、scale=median(F_syn/F_instr)、MAD 清洗 σ=3、0.1nm 网格、v1.0 2D 曲面拟合叙述）——本次 README 重写修正，legacy docs 仅存档 | 旧 README §算法原理；docs/algorithm.md:107/:346 |
| DISP-PHOT-003 | ImageCorrector::computeScale（median 回退）为死代码：生产无调用方（scale 由 IRLS 直出） | image_corrector.cpp:26-57（grep 全仓无调用点） |
| DISP-PHOT-004 | 无取消检查点；无 plan/execute/cancel/inspect 生命周期 | pc_api.cpp 全文；PHASE1_API_V1 §2 计划语义 |
| DISP-PHOT-005 | 入参静默失效：mag_max 被自适应 mag_max_arr{12..16}×5 覆盖（pc_api.cpp:836-866）；pc_calibrate_simple 的 QE 三参数 (void) 丢弃（:103，头注释 :85-88 声明保留） | 同左 |
| DISP-PHOT-006 | PhotometricDiag.rejected_quality 为 invalid+mag+irls 混合计数，不可归因（v2 per-star reject_reason 才可区分） | star_matcher.cpp:580-581 |
| DISP-PHOT-007 | orchestrator 双通道四调用（f64_v2/v2，:2714-2831）与 registry descriptor 占位 ID（module_adapters.cpp:469-486）双轨并存，统一归 P1-PHOT-INT | 同左 |
| DISP-PHOT-008 | Photometer 孔径/天空环全图 O(h·w) 扫描 + 背景中位数整段排序，CPU 资源未优化且未接管线 | photometer.cpp:24-62 |
| DISP-PHOT-009 | 帧级 QA 换算（sigma_mag/sigma_cal_rel）落位在 snr_estimator（snr_phot_cal_quality），photometry 合同仅登记边界 | noise_model.cpp:276；API-NOISE-001 范围界定 |

## 11. 测试设计（TEST-PHOT-DESIGN-001，冻结容差）

冻结测试设计=PHOTOMETRIC_FIT.md §13.4（fixture F1-F6/不变量 I1-I6/负面
矩阵/冻结容差 SCI-PHOT-001 §11：合成注入 location≈log10 k rtol 1e-4、
20% 离群 Δlocation<0.1 dex、S=0→median gate、NumPy 参考复算 rtol 1e-9；
不得放宽）。可执行 TEST-P1-PHOT-001 由 P1-PHOT-TEST 落地；现状既有锚：
tests/unit/p1_wcs_phot_test.cpp（Photometer 4 组）与 lib/photometric_calib/
cpp/test/test_photometric_calib.py（Python 通道旧测，P1-PHOT-TEST 对齐
重锚）。

## 12. 模块记忆

追加式：memory.md（P1-PHOT-DOC 段起）。旧 v2.0 进度历史保留原样，其中与
本 README 冲突处（暴力最近邻/median scale）以本 README 与
PHOTOMETRIC_FIT.md §13 为准。
