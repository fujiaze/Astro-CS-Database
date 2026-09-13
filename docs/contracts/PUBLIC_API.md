# AstroCS Public / Internal API

## C ABI（`extern "C"`，不跨边界抛 C++ exception）

- `lib/astro_image_io`：`aio_*`（image/HiPS I/O、writer/reader、pipeline）。
- `lib/phase2`：`p2_*`（coverage / sampler / upm / integrate / stage2 入口）。
- `p2_upm_build`（obs-only，兼容）与 `p2_upm_build_geo`（全几何节点，
  V13/V14）。
- `p2_sample_controls` / `p2_sample_controls_cached`（后者性能透传 `frame_id_cache` 避免二次 500MB payload 哈希；同数值语义）。
- V16/V17 rejection 接口（typing 单语义，版本化政策）：
  - `p2_reject_plan_resolve`（planning 层把 auto 解析为显式方法 +
    method-specific typed params；profile=wbpp_2_9_1 冻结版本或
    astrocs_adaptive 独立策略）；
  - `p2_eligibility_filter` / `p2_collect_candidate_stack`（V16 生产 strided
    collector：finite/valid/support/quality → CandidateStack；Stage2 CPU/ACR
    统一入口）；
  - `p2_validate_candidate_weights`（V17：SNR lookup 后统一非 finite/非正
    权重校验，禁止 Stage2 漏检）；
  - `p2_reject_stack_ex`（explicit plan kernel；per-sample reason +
    stack-level status 分离；V17 契约：仅 OK/UNDERDETERMINED 可继续，其余
    INVALID_*/INTERNAL_ERROR 必须 hard fail）；
  - `p2_large_scale_apply`（V17：astrocs.large_scale_rejection.v1，
    per-frame low/high rejection mask 的 connected-component grow）；
  - `p2_integrate_pixel`（V17：唯一 canonical support reducer=max(accepted
    support)；显式状态 OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/
    INVALID_INPUT；非 finite weight/support 绝不返回 OK）；
  - `p2_reject_stack`（旧签名）为 COMPAT adapter，生产 Stage2 不再调用。
- 返回码：0=OK；非 0 具体语义见各头文件注释；`err` 缓冲只做日志，不承载
  状态机。

## 状态与错误所有权（V14 合同）

- **返回值所有权**：每个 C ABI 函数的返回码由该模块独占定义（各头文件注释
  为唯一权威），调用方只按 0/非 0 与头文件语义分支，禁止解析错误字符串。
- **错误缓冲 `err`**：仅承载人类可读日志文本，不参与状态机；为 `nullptr`
  时函数必须仍能正常执行并返回状态码。缓冲区所有权/容量/生命周期由各头
  文件声明，无隐式全局错误对象。
- **日志与状态分离**：日志写 `run/logs/<module>/`，返回状态只经返回值传递；
  模块内部日志级别不得影响控制流。
- **C ABI 不抛异常**：`extern "C"` 边界全部捕获并转换为返回码；`buffer
  ownership/lifetime/nullable/单位` 在头文件逐参数注释。
- **跨阶段**：Phase1 产物语义错误（非法 WCS/负 flux 等）必须在 Phase2 入口
  以非 0 返回码显式拒绝，禁止静默用默认值替代。

## C++ API

- `astrocs::healpix`（healpix_core：ang2pix/pix2ang/nested_local↔FITS index）。
- `astrocs::crypto`（SHA-256）。
- 命名空间建议：`astrocs::phase1 / phase2 / hips / acr`（不强制破坏现有
  `p2_*` ABI；C++ 层可逐步包装）。

## 工具/CLI

- `astrocs-stage2.exe <config.json>`（**V5 遗留, LEG-004 已退出生产**；当前生产入口 = `astrocs phase2 run` / `astrocs run --phases` preset）。
- `orchestrator.exe <stage1.json>`（**V5 遗留, LEG-002 已退出生产**；当前生产入口 = `astrocs phase1 run` / `astrocs run --phases` preset）。
- `healpix_browser_qt.exe`（HiPS 浏览器；`--hips/--standard-hips/--view/
  --screenshot/--lod/--exit`）。
- `toolchain.ps1 check|build|run|review`（统一工程入口）。

## JSON schema（config）

- stage1: `lib/orchestrator/configs/stage1_*.json`。
- stage2: `lib/phase2/configs/stage2_*.json`（model/integration/output/
  diagnostics 四段；默认值唯一来源见 `CONFIG_SCHEMA.md`）。

## gaia_client C API（API-GAIA-001）

> ID: API-GAIA-001  状态: CONTRACT_READY（CAT-GAIA-DOC 冻结，2026-09-05）
> 头: lib/gaia_xpsd_client/src/gaia_client.h（唯一权威签名源，禁止手抄他版）
> SRC: lib/gaia_xpsd_client/src/gaia_client.c；ALG: ALG-GAIA-001；
> DATA: DATA-GAIA-001。纯 C（无 C++ 边界），`GAIA_EXPORT` 导出。

- 导出符号（12 个，全部当前真实存在）：`gaia_client_create`、
  `gaia_client_create_ex`、`gaia_client_destroy`、`gaia_client_cone_search`、
  `gaia_client_cone_search_for_solver`、`gaia_client_get_db_type`、
  `gaia_client_get_file_count`、`gaia_client_get_total_sources`、
  `gaia_client_cone_search_with_spectrum`、`gaia_client_query_spectrum_by_coords`、
  `gaia_client_cone_search_with_photometry`、`gaia_client_get_spectrum_params`。
- 返回码：搜索/查询族 `0`=成功（含 0 结果）、`-1`=参数错误/分配失败/内部错误；
  `get_spectrum_params` 返回 `1`=有光谱 / `0`=无；`get_db_type` 返回
  GaiaDbType（0/1/2，NULL 句柄返回 0）；`create/create_ex` 失败返回 NULL。
- 所有权：`client` 由 create 分配、destroy 释放；全部 `out_*` 数组由模块
  malloc、调用方 free（用同一 C 运行时 free）；`out_stars=NULL`/`out_count=0`
  表示空结果，不需要 free。
- 线程安全：同一 client 并发查询安全（文件级 OpenMP 并行 + 缓存互斥，
  ALG-GAIA-001 §4）；`destroy` 不得与在途查询并发；不同 client 互相独立。
- 参数有效域：ra∈[0,360)、dec∈[-90,90]、radius≥0、mag_low≤mag_high、参数
  有限（NaN/Inf 输入不显式校验，行为未定义——前置条件，负面测试覆盖）；
  `query_spectrum_by_coords` 的 `match_radius_arcsec` 单位角秒，其余半径均为度。
- 已登记现状缺陷（不得静默使用，CAT-GAIA-IMPL 处理）：
  `GaiaStar.parallax/pmra/pmdec` 输出未初始化；`source_id` 恒 0；空数据目录在
  Windows 返回 NULL 而 POSIX 返回 file_count=0 的空 client（平台差异）；
  单文件结果上限 200000 静默截断；无取消检查点。
- plan/execute/cancel/inspect 迁移语义见 ALG-GAIA-001 §3.1（astrocs.catalog.gaia，
  C ABI adapter 由 CAT-GAIA-IMPL 建立；本节描述现状 C API，不声明 DLL 化完成）。

## astro_calibration C API（API-CAL-001）

> ID: API-CAL-001  状态: CONTRACT_READY（P1-CAL-DOC 冻结，2026-09-07）
> 头: lib/calibration/include/astro_calibration.h（唯一权威签名源，禁止手抄他版）
> SRC: lib/calibration/src/（CMake astrocs_calibration，4 个 cpp）；
> SCI: SCI-CAL-001；ALG: ALG-CAL-001..004；DATA: DATA-P1-CAL（DATA_SEMANTICS §9）。
> 编排级合同（p1_session 五段式）见 API-P1-001；本节冻结现状模块级 C API。

- 导出符号（12 个函数 + 2 工具，全部当前真实存在，`AC_API` 导出）：
  `ac_generate_master_bias`、`ac_generate_master_dark`、`ac_generate_master_flat`、
  `ac_calibrate_frame`、`ac_correct_frame`、`ac_generate_master_bias_f64`、
  `ac_generate_master_dark_f64`、`ac_generate_master_flat_f64`、
  `ac_calibrate_frame_f64`、`ac_correct_frame_f64`、`ac_set_num_threads`、
  `ac_version`。
- 返回码（10 个科学函数）：`AC_OK=0` 成功；`AC_ERR_PARAM=-1` 空指针或
  n_frames/width/height 非正；`AC_ERR_MEMORY=-2`/`AC_ERR_INTERNAL=-3`
  定义但**从未返回**（现状无异常屏障，DISP-CAL-001）。`ac_version` 返回
  静态串，调用方不得 free；`ac_set_num_threads` 无返回值。
- 单位/dtype/shape：全 ADU；`[h][w]` 行主序 0-based（stack 为
  `[n_frames][h][w]`）；f32 ABI float32、f64 ABI double；掩码 1=坏点。
  NULL 语义：master_bias/dark/flat 可空（条件分支见 DATA_SEMANTICS §9.1）。
- 线程安全：全部函数 reentrant、threadsafe（无共享可变全局；
  API-P1-001 §2 登记一致）；内部 OpenMP 并行（默认 team）。
  **例外**：`ac_set_num_threads` 进程级改写 OpenMP ICV，并发调用竞态且
  影响其他模块的并行度（DISP-CAL-002，迁移后由 host ThreadLease 取代，
  新代码禁止调用）。
- FP64 ABI 语义：仅 `ac_calibrate_frame_f64` 真双精度（像素算术 double）；
  4 个 `ac_generate_master_*_f64`/`ac_correct_frame_f64` 内部降级 float32
  执行（统计/mask 路径，头文件声明），除接口 dtype 外不提供额外精度。
- 所有权：全部缓冲调用方分配/释放（模块零 malloc 输出）；无句柄/生命周期
  对象（无 create/destroy）；日志写 stderr（master 生成与 photometry 通道），
  不影响返回码。
- 取消：模块内无取消检查点（DISP-CAL-008）；取消由调用方（phase1_session）
  在帧粒度实现。
- 已登记现状缺陷（不得静默使用，P1-CAL-IMPL/INT 处理）：
  `generate_master_flat` 负 median 未防护；extern "C" 无异常屏障
  （bad_alloc 可穿越）；bilinear 实为 IDW；NaN 未在 cosmetic 统计过滤；
  w·h int31 溢出无防护；AC_METHOD_BILINEAR/combine 等 enum 越界值不报错
  （按实现默认分支执行）。完整清单见 ALG-CAL 文档 §10（DISP-CAL-001..011）。
- 遗留通道（不在本合同）：Makefile 产物 cosmetic_corrector.dll 的
  `cc_correct_median/cc_detect_hot/cc_detect_cold/cc_last_error`
  （window 奇数 3..15，Python ctypes 专用）与 `ac::optimize_dark_k`、
  `calibration::apply_photometry`（未编译未接线）——迁移去留由
  P1-CAL-IMPL 决定（ALG-CAL §4.1）。
- plan/execute/cancel/inspect 迁移语义见 ALG-CAL-003 文档 §8
  （astrocs.p1.calibration / astrocs_p1_calibration.dll，C ABI adapter 由
  P1-CAL-IMPL 建立；本节描述现状 API，不声明 DLL 化完成）。

## cosmetic C API（API-COS-001）

> ID: API-COS-001  状态: CONTRACT_READY（P1-COS-DOC 冻结，2026-09-07）
> 头: lib/calibration/include/astro_calibration.h（唯一权威签名源，禁止手抄他版；
> ac_correct_frame :97-103、ac_correct_frame_f64 :142-148）
> SRC: lib/calibration/src/cosmetic_corrector.cpp + ac_api.cpp（CMake
> astrocs_calibration）；SCI: SCI-CAL-001；ALG: ALG-COS-001..005；
> DATA: DATA-P1-COS（DATA_SEMANTICS §10）；MOD: astrocs.p1.cosmetic
> （迁移目标 astrocs_p1_cosmetic.dll，落码由 P1-COS-IMPL 建立）。
> 与 API-CAL-001 的关系：两合同共享同一头文件与编译目标，本节只冻结
> cosmetic 路径 3 个符号的语义（模块级合同视角独立）；编排级合同见
> API-P1-002（PHASE1_API_V1 §2，多模块共享）。

- 导出符号（3 个，全部当前真实存在，`AC_API` 导出）：
  `ac_correct_frame`（f32）、`ac_correct_frame_f64`、`ac_set_num_threads`
  （与 API-CAL-001 共享；本合同引用其 ICV 副作用登记，不重复冻结）。
- 签名（astro_calibration.h :97-103，f64 变体 :142-148，double 参数）：
  `int ac_correct_frame(const float* data, int width, int height,
  const float* master_dark, const float* master_bias, float* out,
  float hot_sigma, float cold_sigma, int method,
  int max_structure_size, int* out_hot, int* out_cold)`。
- 返回码：`AC_OK=0`；`AC_ERR_PARAM=-1`（data/out 空指针、width/height
  非正，ac_api.cpp:108-122 校验）；`AC_ERR_MEMORY=-2`/`AC_ERR_INTERNAL=-3`
  定义但**从未返回**（无 extern "C" 异常屏障，bad_alloc 可穿越 C ABI，
  DISP-COS-001）。
- 调用时序与所有权：无句柄对象；`out` 由调用方分配（w·h float32）；
  `data/master_dark/master_bias` 只读借用；`out_hot/out_cold` 可 NULL。
  data 与 out 内存重叠行为未定义（in-place 未登记，DISP-COS-010）。
- 单位/dtype/shape：全 ADU；`[h][w]` 行主序 0-based；f64 ABI 内部降级
  float32 执行（统计/mask/插值全程 f32，DISP-COS-004）；
  method 0=median / 1=IDW（名义 bilinear，DISP-COS-003）；
  hot/cold_sigma<=0 或 master_dark/bias=NULL → 对应检测禁用（ALG-COS-004）。
  数据语义逐字段见 DATA_SEMANTICS §10（DATA-P1-COS）。
- 线程安全：reentrant、threadsafe（无共享可变全局；OpenMP 默认 team，
  进程级 ICV——ThreadLease 迁移整改点 DISP-COS-008）；输出 bitwise
  与线程数无关。
- 取消：无取消检查点（API-P1-002 §2 登记"取消点=无"；帧粒度取消由
  phase1_session 层实现，DISP-COS-007）。
- 生产调用方：lib/phase1_session/p1_session.cpp:294-307（cosmetic stage，
  现传 master_dark/master_bias=nullptr → 检测全禁用、恒等 pass，
  DISP-COS-009；no fabrication of valid coverage）。
- 已登记现状缺陷（不得静默使用，P1-COS-IMPL/INT 处理）：
  NaN 未在检测统计过滤（NaN 源帧检测静默全 false）；w·h int31 溢出
  无防护；非 0 method 一律按 IDW；镜像边界小帧语义。完整清单见
  ALG-COS 文档 §10（DISP-COS-001..011）。
- 遗留通道（不在本合同）：Makefile 产物 cosmetic_corrector.dll 的
  cc_correct_median/cc_detect_hot/cc_detect_cold/cc_last_error
  （局部窗口修复，公式与 ac_* 通道不同）——计划迁移旧符号，去留由
  P1-COS-IMPL 决定。
- plan/execute/cancel/inspect 迁移语义见 ALG-COS 文档 §0/§8
  （astrocs.p1.cosmetic / astrocs_p1_cosmetic.dll，C ABI adapter 由
  P1-COS-IMPL 建立；本节描述现状 API，不声明 DLL 化完成）。

## On-disk 格式

- HiPS：IVOA 1.4（signal/support/snr 产品，NESTED，512 tile）。
- UPM：`astrocs-upm-v2` JSON（sparse）+ dense cache（checksum 校验）。
- Manifest：`manifest.json` / `diagnostics.json` / `controls_accept.json`。

详见 `docs/architecture/api_inventory.csv`（API 机器单源清单，与 `check_api_contracts` 的
`API_CONTRACTS.csv` 一致；完整分类清单）。

## drizzle C API（API-DRZ-001）

> ID: API-DRZ-001  状态: CONTRACT_READY（P1-DRZ-DOC 冻结，2026-09-07）
> 头: lib/healpix_db/healpix_drizzle/hp_drizzle_api.h（唯一权威签名源，
> 禁止手抄他版；六导出 :42,62,70,130,139,140）
> SRC: lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp（CMake 静态库
> astrocs_drizzle，CMakeLists.txt:356-366）；SCI: SCI-DRZ-001；
> ALG: ALG-DRZ-001；DATA: DATA-P1-DRZ（DATA_SEMANTICS §11）；
> MOD: astrocs.p1.drizzle（迁移目标 astrocs_p1_drizzle.dll，落码由
> P1-DRZ-IMPL 建立）。编排级合同见 API-P1-007（PHASE1_API_V1，
> 区间 API-P1-001..010 声明，无独立小节）；生产调用方
> orchestrator.cpp:3256-3371 经函数指针调 hp_drizzle_run_hips。

- 导出符号（6 个，全部当前真实存在，`HP_DRIZZLE_API` extern "C"）：
  `hp_drizzle_fits_to_ahpx`（文件通道 FITS→.hiss）、
  `hp_drizzle_run`（PipelineFrame 帧通道）、`hp_drizzle_run_hips`
  （帧通道 + HiPS 直写薄封装，Phase1 正式末端）、
  `hp_drizzle_reverse_run`（Sphere→Plane 反向）、
  `hp_drizzle_reverse_capability`、`hp_drizzle_reverse_version`。
- 签名（hp_drizzle_api.h :42-51,62-66,70-75,130-134,139-140）：
  `int hp_drizzle_run(PipelineFrame* frame, int nside, int nested,
  double pixfrac, const char* output_path, HpDrizzleResult* result,
  int precision_mode)`；hips 变体增加 `const char* legacy_hiss_path`
  （:70-75）；reverse: `int hp_drizzle_reverse_run(const
  HpReverseDrizzleInput* in, void* signal_out, void* coverage_out,
  HpReverseDrizzleResult* result)`。
- 返回码：0=成功，非 0=失败；实测语义——文件通道正值 1..11
  （1=null 参数、2=nside 非 2 幂、3=pixfrac 越界、4=读 FITS 失败、
  5=无 WCS、6/7=SNR 读/尺寸、8/9=权重读/尺寸、10=drizzle 失败、
  11=写 HISS 失败，api.cpp:168-352）；帧通道混用负值 -1..-8（参数/
  块校验）、-9=无 WCS（:541-545）、-12=HiPS dir 空（:1044-1048）、
  -13=直写失败（:1066-1070）——正负两套并存无集中枚举（登记缺陷，
  迁移整改点）。reverse 返回 1..6（api.cpp:39-143）。
- 调用时序与所有权：无句柄对象；frame 及其块由调用方拥有（只读
  借用）；result 由调用方分配；reverse 的 signal_out/coverage_out
  由调用方分配（width×height，output_fp64 决定 double/float 视图）；
  HiPS 目录树由模块写入、编排层负责 overwrite 清理
  （orchestrator.cpp:3345-3354）。
- 单位/dtype/shape：data 块 [H][W] 行主序 ADU（f32/f64 二选一）；
  输出 tile 累加量语义见 DATA_SEMANTICS §11.2；precision_mode
  0=FP32（默认）/1=FP64/-1=读 header "PRECISION" KV
  （hp_drizzle_api.h:60）；错误信息经 error_msg[512] 返回。
- 线程安全：单次 run 内 OpenMP 内部并行（config.threads/omp 默认，
  schedule(static)+按线程序合并，1/N 确定性，ALG-DRZ-001 §6）；
  同进程多 run 并发经 per-run generation 原子递增隔离缓存
  （drizzle_engine.cpp:1659-1660）；ThreadLease 零命中——迁移整改点。
- 取消：无取消检查点（模块内无 cancellation token；编排取消点=
  帧/tile 粒度为编排层合同）。
- stderr 约定：全部诊断/进度日志直写 stderr（[hp_drizzle_api]/
  [drizzle_engine]/[sink] 前缀），不污染 stdout；G4 trace 由 env
  ASTROCS_DRIZZLE_TRACE 控制（drizzle_engine.cpp:39-330）。
- 已登记现状缺陷（不得静默使用，P1-DRZ-IMPL/INT 处理）：错误码
  正负两套混用；文件通道接受 pixfrac=0.0 而引擎层拒绝（DISP-DRZ-003）；
  值像素 NaN 静默跳过无计数（DISP-DRZ-004）；shim 对非法 nside 容忍
  不抛。完整清单见 ALG-DRZ-001 §10（DISP-DRZ-001..008）。
- 遗留通道（不在本合同）：模块 Makefile 产物 healpix_drizzle.dll
  （Python ctypes 专用，与 CMake 静态库同源码）——迁移去留由
  P1-DRZ-IMPL 决定。
- plan/execute/cancel/inspect 迁移语义见 ALG-DRZ-001 §0 与
  lib/drizzle/module.yaml（astrocs.p1.drizzle / astrocs_p1_drizzle.dll，
  C ABI adapter 由 P1-DRZ-IMPL 建立；本节描述现状 API，不声明 DLL 化
  完成）。

## HiPS writer C API（API-HIPS-001）

> ID: API-HIPS-001  状态: CONTRACT_READY（P1-HIPS-DOC 冻结，2026-09-07）
> 头: lib/astro_image_io/include/aio_hips.h（唯一权威签名源，禁止手抄
> 他版；九导出 :104,121,130,135,144,149,152,163,177，AIO_HIPS_EXPORT
> extern "C" :24-30）
> SRC: lib/astro_image_io/src/hips/aio_hips_writer.cpp（CMake 静态库
> astrocs_hips，CMakeLists.txt:298-309）；SCI: SCI-DRZ-001；ALG:
> ALG-HIPS-001..005；DATA: DATA-P1-HIPS（DATA_SEMANTICS §12）；MOD:
> astrocs.p1.hips_writer（迁移目标 astrocs_p1_hips_writer.dll，落码由
> P1-HIPS-IMPL 建立）。编排级无独立 hips stage——经 API-P1-007
> hp_drizzle_run_hips 由 astro_sphere_sink（astro_sphere_sink.cpp:97，
> P1-DRZ 链）与 lib/phase2/tools/stage2.cpp:592 间接调用；Phase2 读侧
> 为 aio_hips_reader（SCI-P3-001 链，不属本合同）。

- 导出符号（9 个，全部当前真实存在，`AIO_HIPS_EXPORT` extern "C"）：
  `aio_hips_product_begin`（创建产品集句柄）、
  `aio_hips_write_signal_support_tile`、`aio_hips_write_variance_tile`
  （逐叶 tile 流式写，AstroSphereTileView 直供）、`aio_hips_write_snr_points`
  （SNR 控制点累计缓存，finalize 时落 TSV tile）、
  `aio_hips_set_drizzle_provenance`（Phase2 k_corr 选择键）、
  `aio_hips_finalize`（properties/MOC/hierarchy/manifest 收尾+释放）、
  `aio_hips_abort`（释放句柄，不删文件——见缺陷登记）、
  `aio_hips_write`（legacy 兼容批量入口，HISS 中转验证用，support uint8
  0..255→covered_area=su/255·A_cell、flux_sum=signal·su/255，flags 固定
  ALL）、`aio_hips_last_error`（thread_local 文本）。
- 签名（aio_hips.h :104-118,121-123,130-132,135-138,144-145,149,152,
  163-173,177）：`AioHipsProductSet* aio_hips_product_begin(const char*
  out_dir, uint32_t nside, uint32_t tile_width, int32_t data_type, int
  flags, const char* creator_did, const char* obs_title, const char*
  obs_filter, double exposure_s, const char* obs_date, uint32_t
  moc_order)`；`int aio_hips_write_signal_support_tile(AioHipsProductSet*
  ps, const AstroSphereTileView* view)`；variance 变体同型（:130）；
  `int aio_hips_write_snr_points(AioHipsProductSet* ps, const
  AioHipsSnrPoint* pts, int n)`；`int aio_hips_set_drizzle_provenance(
  AioHipsProductSet* ps, double pixfrac, double scale_arcsec)`。
- 返回码：begin 失败返回 NULL + last_error（nside<512/tile_width≠512/
  dtype∉{0,1}/flags 越位，writer :399-404）；write/finalize 负码
  −1 null、−2 参数/视图不匹配或重复 finalize、−3 parent_ipix 越界或
  signal 子产品失败、−4 support/FITS 失败、−5 全无效 variance tile 或
  hierarchy 失败、−6 snr 失败、−7 variance FITS、−8 ivar FITS
  （:513,:521,:632,:648,:658,:1036-1072）；provenance 正码 1=null、
  2=值域（pixfrac∈(0,1]、scale≥0，:1007-1016）——负正两套并存无集中
  枚举（登记缺陷 DISP-HIPS-007）。错误文本经 aio_hips_last_error
  （每次入口 clear，跨调用不可追溯）。
- 调用时序与所有权：begin →（零或多次）write_* / write_snr_points →
  finalize（成功路径内部 delete ps）或 abort（仅 delete ps，**不删除
  已写文件**；aio_hips.h:151 注释"清理已写部分(尽力)"与实现不符，
  DISP-HIPS-001——残留处置归调用方/IO-003 发布层）。ps 句柄调用方
  持有至 finalize/abort；view 及其数组调用方拥有、调用期间只读借用
  （同步消费，无拷贝）；SNR 点数组调用后即可释放（内部拷贝缓存）。
- 单位/dtype/shape：见 DATA-P1-HIPS（DATA_SEMANTICS §12.1/§12.2）——
  flux_sum ADU、covered_area sr、[512×512] NESTED local 行主序、
  data_type 0=f32/1=f64 一次固化；products flags 位域
  SIGNAL=1/SUPPORT=2/SNR=4/VARIANCE=8/IVAR=16（ALL=7/ALL_V19=31，
  aio_hips.h:34-43）。
- 线程安全：单句柄非线程安全（成员 scratch 缓冲与 moc_cells/hier/
  leaf_ipix_list 无锁累积）；同进程 CFITSIO 裸调未包装
  aio::cfitsio_mutex（同库 aio_fits/aio_hips_reader 均有包装——
  DISP-HIPS-006）；现状生产链为 drizzle 合并后单线程串行调用
  （astro_sphere_sink.cpp:97）。threading_model=host_executor_lease 为
  迁移合同值，ThreadLease 接线由 P1-HIPS-IMPL 建立（迁移整改点）。
- 取消：无取消检查点（finalize 长收尾不可中断；模块内无
  cancellation token；编排取消点=帧/tile 粒度为编排层合同）。
- stderr 约定：诊断/六段 profile 计时直写 stderr（[hips] 前缀），
  stdout 不用（writer :368-373,:1030-1076）。
- 已登记现状缺陷（不得静默使用，P1-HIPS-IMPL/INT 处理）：abort 无
  清理（DISP-HIPS-001）；无原子发布/remove+create 直写/manifest 无
  COMPLETE（DISP-HIPS-004，对齐边界=DATA_SEMANTICS §12.5）；
  estsize/fov 硬编码（DISP-HIPS-002）；moc_order 静默钳位（DISP-HIPS-005）；
  CFITSIO 无锁（DISP-HIPS-006）；错误码混用（DISP-HIPS-007）；f32
  hierarchy 累加（DISP-HIPS-009）；fits_str 截断（DISP-HIPS-010）。完整
  清单见 ALG-HIPS-001 §10（DISP-HIPS-001..012）。
- plan/execute/cancel/inspect 迁移语义见 ALG-HIPS-001 §0 与
  lib/hips/module.yaml（astrocs.p1.hips_writer / astrocs_p1_hips_writer.dll，
  entrypoint=MISSING——registry 无 descriptor；C ABI adapter 由
  P1-HIPS-IMPL 建立；本节描述现状 API，不声明 DLL 化完成）。

## SNR/Noise C API（API-NOISE-001）

> ID: API-NOISE-001  状态: CONTRACT_READY（P1-NOISE-DOC 冻结，2026-09-07）
> 头: lib/snr_estimator/cpp/include/snr_estimator.h（唯一权威签名源，禁止
> 手抄他版；SNR_API extern "C" 导出，_WIN32 下 __declspec(dllexport) :7-11）
> SRC: lib/snr_estimator/cpp/src/noise_model.cpp（现状构建=cpp/Makefile:5,12
> g++ -shared → snr_estimator.dll + cpp/build.ps1:29，未编入根 CMake 主
> 构建；dll_loader.cpp:41/55 加载名与路径吻合）；SCI: SCI-NOISE-001..015；
> ALG: ALG-NOISE-001..003（NOISE_ESTIMATION §13.1 逐符号锚）；DATA:
> DATA-P1-NOISE（DATA_SEMANTICS §13）；MOD: astrocs.p1.noise-snr（迁移目标
> astrocs_p1_noise.dll，落码由 P1-NOISE-IMPL 建立）。编排级合同见
> API-P1-006（PHASE1_API_V1 §2，多模块共享）；本节只冻结 noise 路径
> 9 个导出符号的语义。
>
> **范围界定**：本节只登记 NoiseWeightModelV1 生产链 + 诊断函数
> （ALG-NOISE-001..003）；同头三层模型其余符号
> snr_phot_cal_quality/snr_psf_fit_quality（测光/PSF 质量，P1-PHOT/PSF
> 合同视角）与旧乘法 SNR 通道 snr_estimate*/snr_extract_model*（legacy
> diagnostic，头注释 :19-20 降级声明）不属本合同，仅登记边界。

- 导出符号（noise 路径 9 个，全部当前真实存在）：`snr_noise_model_v1`
  （:143-149）、`snr_noise_model_v1_f64`（:151-157）、
  `snr_noise_model_v1_default_config`（:115）、
  `snr_noise_model_v1_fill`（:162-166）、`snr_noise_model_v1_free`
  （:167-168）、`snr_noise_scale_law`（:173-175）、
  `snr_noise_gain_variance`（:178-180）——7 个导出 + `snr_estimate`
  （:200-203）/`snr_estimate_f64`（:215-218）2 个 legacy 诊断导出
  （范围外登记）。
- 签名（snr_estimator.h 权威）：
  `int snr_noise_model_v1(const float* data, int h, int w, const float*
  source_mask, const double* star_x, const double* star_y, int n_stars,
  const SnrNoiseModelConfig* cfg, NoiseWeightModelV1* out_model)`
  （:143-149；f64 变体 data 为 double :151-157）；
  `int snr_noise_model_v1_fill(const NoiseWeightModelV1* model, int h,
  int w, float* out_variance, float* out_ivar)`（:162-166）；
  `void snr_noise_model_v1_free(NoiseWeightModelV1* model)`（:167-168）；
  `void snr_noise_scale_law(double alpha, double* variance, double* ivar)`
  （:173-175）；`double snr_noise_gain_variance(double signal, double
  gain_e_per_adu, double read_noise_e)`（:178-180）。
- 返回码（build/fill 一致，noise_model.cpp 实测）：`0`=成功（含
  degenerate=1 全局兜底成功，:267）；`1`=完全退化（ivar_bg_global=0.0，
  调用方拒绝加权，:225-233）；`3`=nullptr/尺寸非法/内部异常（C ABI
  try/catch 屏障 :348-364；malloc 失败 :244-254）。
- 调用时序与所有权：`snr_noise_model_v1[_f64]`（build，写
  g_model_floor[out_model] :126）→ `snr_noise_model_v1_fill`（读模型 +
  指针 key 查 floor，:402-405）→ `snr_noise_model_v1_free`（free ctrl
  数组 + 擦除注册表条目 :431-445）。out_model 由调用方分配/持有，
  ctrl_* 内部数组由实现 malloc/free；**未 free 即丢弃指针 = 注册表
  泄漏**（DISP-NOISE-009）；free 幂等（nullptr 安全）。
- 单位/dtype/shape：data ADU [h·w] 行主序（v1 float32 / f64 float64）；
  source_mask float32 [h·w]（≠0=源）；star 坐标 double[n_stars] 0-based
  pixel；fill 输出 float32 [h·w]；variance ADU²、ivar ADU⁻²、σ ADU、
  gain e⁻/ADU、read_noise e⁻。逐字段语义见 DATA_SEMANTICS §13
  （DATA-P1-NOISE）。
- 线程安全：reentrant=yes、threadsafe=yes **以 model 对象隔离为前提**
  （PHASE1_API_V1 §2 口径）——唯一共享可变状态 g_model_floor 为进程级
  无锁 unordered_map，并发 build/free 无保护（DISP-NOISE-001）；fill
  只读模型 + g_model_floor 查询。输出 bitwise 与线程数无关（现状
  单线程实现，noise_model.cpp:118-267,371-429）。
- 取消：无取消检查点（noise_model_impl/fill_impl 无 cancel 回调，
  DISP-NOISE-004；PHASE1_API_V1 §2 "取消点=行带"为计划语义）。
- 生产调用方：lib/orchestrator/src/orchestrator.cpp:4177（stage6 SNR，
  必需 stage）→ dll_loader_ 函数指针 snr_noise_model_v1/_f64/
  _default_config/_fill/_free（orchestrator.cpp:4242-4251）；DLL 装载
  snr_estimator.dll（dll_loader.cpp:41，lib/snr_estimator/cpp/ :55）。
- 已登记现状缺陷（不得静默使用，P1-NOISE-IMPL/INT 处理）：ABA 复用与
  并发无锁（DISP-NOISE-001）、build/fill floor 语义不一致（002）、
  gain 三字段无效（003）、无取消点（004）、scale_law 无校验（005）、
  掩膜通道互斥（006）、参数静默钳位（007）、空 patch 计数混同（008）、
  注册表泄漏路径（009）。完整清单见 NOISE_ESTIMATION §13.3。
- plan/execute/cancel/inspect 迁移语义见 NOISE_ESTIMATION §13.5 与
  lib/snr_estimator/module.yaml（astrocs.p1.noise-snr /
  astrocs_p1_noise.dll，C ABI adapter 由 P1-NOISE-IMPL 建立；本节描述
  现状 API，不声明 DLL 化完成）。

## Photometric C API（API-PHOT-001）

> ID: API-PHOT-001  状态: CONTRACT_READY（P1-PHOT-DOC 冻结，2026-09-07）
> 头: lib/photometric_calib/cpp/include/photometric_calib.h（271 行，唯一
> 权威签名头，PC_API :7-11 `extern "C"` 不抛异常）
> SRC: lib/photometric_calib/cpp/src/pc_api.cpp
> SCI: SCI-PHOT-001（docs/science/PHOTOMETRY.md，FROZEN，共享引用不改动）
> ALG: ALG-PHOT-001..002（docs/algorithms/PHOTOMETRIC_FIT.md，§13 逐符号锚）
> DATA: DATA-P1-PHOT（DATA_SEMANTICS §14）；编排级合同 API-P1-005
> （PHASE1_API_V1，descriptor 引用，与本节并行不互斥）
> MOD: MOD-astrocs-phase1-photometry（module.yaml CONTRACT_READY，
> entrypoint=MISSING；生产调用 orchestrator.cpp:2474 run_stage_photometric）


### 范围界定

帧级测光定标 C ABI：合成测光 F_syn（XPSD uint8 解码 + Akima/Simpson 1.0nm
积分）、Gaia TAN+SIP 投影、双向最近邻唯一配对（KD-tree，2.0px）、星等预
过滤 + IRLS/Tukey 稳健零点（scale=10^(−location)，sigma_residual=dex）、
逐星 PcMatchRecord、I_cal=I·scale。不做逐像素 ivar；帧级 QA 换算
（sigma_mag/sigma_cal_rel）归 snr_estimator snr_phot_cal_quality
（API-NOISE-001 范围界定，DISP-PHOT-009）。无取消检查点（DISP-PHOT-004）。


### 导出符号（photometric_calib.h 实测行号锚）

| 符号 | 头锚 | 摘要 |
|---|---|---|
| pc_calibrate_simple | :103-117 | 直通版：F_syn 由调用方传入（gaia_fsyn），QE 三参数保留不用（DISP-PHOT-005） |
| pc_calibrate_simple_with_gaia | :153-183 | DLL 内锥形搜索+积分（v2 前旧实现路径封装） |
| pc_calibrate_simple_f64 | :185-199 | FP64 直通版 |
| pc_calibrate_simple_with_gaia_f64 | :201-225 | FP64 with-gaia 封装 |
| pc_calibrate_simple_with_gaia_v2 | :227-245 | per-star PcMatchRecord（生产主路径，float32 像素） |
| pc_calibrate_simple_with_gaia_f64_v2 | :247-265 | per-star（float64 像素） |

结构体：PhotometricDiag（:21-45，17 字段分阶段诊断）、PcMatchRecord
（:47-59，status 0/1/2/3 + reject_reason 0..6）。


### 签名要点与内存所有权

- gaia_client_handle 为 opaque borrow（调用方经 gaia_client.dll 创建/销毁，
  dll_loader.cpp:271-281 预加载）；out_pixels/out_scale_factor/
  out_sigma_residual/out_n_matched/out_diag/out_records 均调用方分配；
  spec_stars/spectra_buf 为 DLL 内 malloc 的锥搜结果，本调用内 free。
- 所有出参可 NULL 向后兼容（头 :17 注释）；records 需 n_psf≥1 才有意义。


### 返回码（如实登记，含退化语义）

- 0=成功，**含退化恒等校正**（无 Gaia 星 :72-98 / 无 PSF 星 :808-830 /
  锥搜无光谱星 :868-890 / 滤光片预处理失败 :911-923 → scale=1.0、
  n_matched=0、sigma_residual=0；调用方须以 out_n_matched/out_diag 判据，
  不得以 rc=0 推断完成定标）。
- −1=空指针/宽高非正/参数非法（pc_calibrate_simple 的 −2/−3 为 dims 校验，
  实际退化路径返回 0——头注释与实现的差异如实登记，README §6）。
- −2=gaia_client_handle 为空（with-gaia 系）。
- −3=锥形搜索失败（gaia_client rc≠0，pc_api.cpp:836-866）。


### 单位/dtype/shape

见 DATA_SEMANTICS §14（唯一权威）：pixels f32(v2)/f64(f64_v2) `[h·w]`
ADU；scale 无量纲；sigma_residual dex；records residual=dex；WCS deg/px；
光谱 uint8 编码 F(λ)=byte·flux_mul+flux_min（W·m⁻²·nm⁻¹）。


### 线程安全与确定性

- 无跨调用共享可变状态（模块级单例无）；调用内 OpenMP F_syn
  schedule(dynamic,64) 逐星独立 + 像素 static 逐元素（star_matcher 单线程）
  ——reentrant，并发调用安全；线程数 omp_get_max_threads() 未接
  ThreadBudget（迁移整改点，module.yaml threading_model=host_executor_lease
  为合同值）。
- determinism=fixed_reduction_order：输出 bitwise 与线程数无关（README §7）。


### 生产调用方与编排现状

- orchestrator.cpp:2474 run_stage_photometric（必需 stage，DLL 未加载退出
  码 2）→ 函数指针 pc_calibrate_simple_with_gaia_f64_v2（:2714）/_v2
  （:2790）双通道；写 photo_stats KV 块（:2902-2935，N_MATCHED/
  SCALE_FACTOR/SIGMA_RESIDUAL + diag 17 字段）。
- registry descriptor 占位 ID（module_adapters.cpp:531-548，sci_id=
  SCI-P1-PHOT-001/alg_id=ALG-002/data_id=DATA-P1-FLUX/api_id=API-P1-005/
  test_id=TEST-P1-PHOT-001）由 P1-PHOT-INT 对齐本合同，不得反向作为冻结
  依据（DISP-PHOT-007）。


### 已登记现状缺陷与迁移语义

- DISP-PHOT-001..009 全清单见 PHOTOMETRIC_FIT §13.3（001 注释失实/
  002 旧文档失实/003 computeScale 死代码/004 无取消点/005 入参静默失效/
  006 rejected_quality 混计/007 双轨并存/008 Photometer 未优化/009 QA 换算
  边界）。
- plan/execute/cancel/inspect 迁移语义见 PHOTOMETRIC_FIT §13.5 与
  lib/photometric_calib/module.yaml（astrocs.p1.photometry /
  astrocs_p1_photometry.dll，C ABI adapter 由 P1-PHOT-IMPL 建立；本节描述
  现状 API，不声明 DLL 化完成）。

## PSF 拟合 C API（API-PSF-001）

> ID: API-PSF-001  状态: CONTRACT_READY（P1-PSF-DOC 冻结，2026-09-07）
> 头: lib/dynamic_psf/include/dynamic_psf.h（唯一权威签名源，禁止手抄他版；
> DPSF_EXPORT extern "C"（_WIN32 下 __declspec(dllexport) :8，否则
> __attribute__((visibility("default"))) :10））
> SRC: lib/dynamic_psf/src/dpsf_psf.cpp（934 行）
> SCI: SCI-P1-PSF-001（本任务冻结层）；ALG: ALG-STARPSF-001
> （STAR_PSF_ALGORITHMS §11 逐符号锚）；DATA: DATA-P1-PSF
> （DATA_SEMANTICS §15，双 [N,9] 布局权威）；编排级合同 API-P1-003
> （PHASE1_API_V1 §2，descriptor 引用，与本节并行不互斥）
> MOD: MOD-astrocs-phase1-star-psf（module.yaml CONTRACT_READY，
> entrypoint=MISSING；生产调用 orchestrator.cpp:2067 run_stage_psf）


### 范围界定

Phase1 单帧逐星 Moffat4（β=4 固定）PSF 拟合 C ABI：uint16/float32/float64
三通道图像输入，star_det v1 `FLOAT64[N,6]` 检测坐标消费，7 参数 LM
（B,A,x0,y0,sx,sy,theta），4 状态码失败语义（STAR_PSF_ALGORITHMS §11.2）。
不做星检测（禁重检测，orchestrator.cpp:1754-1756）、不做饱和剔除决策
（star_det v1 [4]/[5] 不消费）、不做 QA 换算（帧级 PSF 质量归
snr_estimator snr_psf_fit_quality，snr_estimator.h:79，API-NOISE-001
范围界定）。无取消检查点（DISP-PSF-004）。


### 导出符号（dynamic_psf.h 实测行号锚，7 个全部当前真实存在）

| 符号 | 头锚 | 定义锚 | 摘要 |
|---|---|---|---|
| dpsf_fit | :44-47 | dpsf_psf.cpp:427 | uint16 单星拟合，DPSFFitResult 输出 |
| dpsf_fit_batch | :49-52 | dpsf_psf.cpp:482 | uint16 批量（逐星 float patch），DPSFFitResult*[] |
| dpsf_fit_batch_f | :59-63 | dpsf_psf.cpp:580 | float32 图 + (cx[],cy[])，DPSFFitResult*[]（FP32 生产通道） |
| dpsf_free_results | :64 | dpsf_psf.cpp:599 | 释放批量 DPSFFitResult 数组 |
| dpsf_fit_batch_f32 | :122-131 | dpsf_psf.cpp:713 | float32 图 + star_det v1 → out_psf_params[N,9]（compact）+ 可选 out_status[N] |
| dpsf_fit_batch_f64 | :160-169 | dpsf_psf.cpp:968 | float64 图 + star_det v1 → [N,9]（compact；moffat4_fit_d 不降级）+ 可选 out_status[N] |
| dpsf_fit_batch_d | :181-188 | dpsf_psf.cpp:612 | float64 图 + (cx[],cy[])，DPSFFitResult*[]（FP64 生产通道） |

schema 宏：`DPSF_STAR_DET_SCHEMA_V1="star_det_v1:FLOAT64[N,6]"`（:107）、
`DPSF_PSF_PARAMS_SCHEMA="psf_params:FLOAT64[N,9]"`（:108）、
`DPSF_PSF_STATUS_SCHEMA="psf_status:INT32[N]"`（:114，B2-A2 新增）。结构体：
DPSFFitResult 12 字段（:17-31）、DPSFFitParams{fitRadius,maxIter,tolerance}
（:38-42）。错误码 DPSF_FIT_OK/NO_CONVERGENCE/INVALID_PARAMS/ITERATION_LIMIT
=0/1/2/3（:33-36，语义冻结见 STAR_PSF_ALGORITHMS §11.2）。


### 签名要点与内存所有权

- dpsf_fit_batch/_f/_d：`*out_results` 为 DLL 内 malloc 数组，调用方
  `dpsf_free_results` 释放（:599）；失败（rc≠0）调用方仍须对非 NULL
  results 释放（orchestrator.cpp:2350-2353 先 free 再返回）。
- dpsf_fit_batch_f32/_f64：out_psf_params 由调用方预分配（N·9·sizeof(double)），
  out_n_valid 由 DLL 写；params 可 NULL（默认 fitRadius=8/maxIter=200/
  tolerance=1e-8，:717-719/:845-847；maxIter/tolerance 为死参数 DISP-PSF-003）。
- **B2-A2（RESCUE-P0-05）**: out_status 可选（可 NULL），大小 N·sizeof(int)，
  按**检测下标**报告逐星真值（`DPSF_PSF_STATUS_OK`=0 成功；1=拟合失败/未收敛；
  2=空 rect 未拟合；3=patch 分配失败）。out_psf_params 的成功行按检测下标升序
  **compact** 写入 0..n_valid−1；失败星不占参数行。调用方必须用 out_status 做
  星 ID↔行映射，禁止 `i < n_valid` 前缀截断（该错位曾把 NaN 贴真实 star_id）。
  传 NULL 时逐星状态不可得（compact 布局不变），仅为旧调用方 ABI 兼容面。
- 全部接口不抛异常（C ABI）；批接口逐星失败**经 out_status 显式报告**、
  不计 valid（B2-A2 关闭 DISP-PSF-006 的"per-star 状态不出批"缺口），批级 rc∈{0,−1}。


### 返回码

- 单星 dpsf_fit/moffat4_fit*：0/1/2/3 四码（§11.2 表：触发锚、输出副作用、
  ITERATION_LIMIT 仍回填当前最优参数 :391-403）。
- 批接口：0=批量完成（逐星成败看 out_status 或 status 列，不要求全成）；
  −1=参数非法（空指针/尺寸非法/计数≤0，:700-707；此时不触碰任何输出缓冲）。


### 单位/dtype/shape

见 DATA_SEMANTICS §15（唯一权威）：image `[h·w]` ADU 行主序；
cx/cy/fitRadius/sx/sy/fwhm 像素；theta 弧度；B/A/flux/mad ADU
（flux=2π·A·sx·sy/3 Moffat4 解析积分）；eccentricity 无量纲 [0,1)；
布局 A（编排 psf 块 status,B,flux,cx,cy,fwhm,A,mad,eccentricity）与
布局 B（psf_params B,A,cx,cy,sx,sy,theta,fwhm_x,fwhm_y）并存，禁止混用。


### 线程安全与确定性

- 批拟合 OpenMP `parallel for schedule(dynamic) reduction(+:success_count)`
  4 处（dpsf_psf.cpp:528,635,738,876）：逐星独立、输出按索引写、计数
  reduction 与星序无关 → reentrant、并发调用安全、输出 bitwise 与线程数
  无关（determinism=fixed_reduction_order）。线程数未接 ThreadBudget
  （迁移整改点，module.yaml threading_model=host_executor_lease 为合同值，
  ThreadLease/取消检查点归 P1-PSF-IMPL，DISP-PSF-004）。


### 生产调用方与编排现状

- orchestrator.cpp:2067 run_stage_psf（必需 stage，DLL 未加载退出码 2，
  :2071-2075；frame_ 为空=内部错误）→ 函数指针 dpsf_fit_batch_d（:2304，
  FP64 通道）/ dpsf_fit_batch_f（:2327，FP32 通道）+ dpsf_free_results
  （:2290）；编排参数 stage1_cfg psf.fit_radius/max_iterations/tolerance
  （:2277-2286，max_iterations/tolerance 模块侧不生效）。
- 产出：psf 块 FLOAT64 [N,9]（布局 A，:2376-2390）+ star_measurements
  权威块 [N,15]（DATA_SEMANTICS §15.2 附属产出）；PHOTOMETRIC 以 psf 为
  必需块消费（:2563-2570，缺失退出码 3）。
- registry descriptor 占位 ID（module_adapters.cpp:492-510，sci_id=
  SCI-P1-PSF-001/alg_id=ALG-002/data_id=DATA-P1-SOURCES/api_id=API-P1-003/
  test_id=TEST-P1-PSF-001）由 P1-PSF-INT 对齐本合同，不得反向作为冻结
  依据（DISP-PSF-001 附注）。
- 现状构建 lib/dynamic_psf/Makefile:3-5 → dynamic_psf.dll；dll_loader.cpp:39
  （ModuleId::PSF→dynamic_psf.dll）/:53（lib/dynamic_psf/）；未编入根 CMake
  主构建——astrocs_p1_psf.dll 迁移由 P1-PSF-IMPL 建立。


### 已登记现状缺陷与迁移语义

- DISP-PSF-001..006 全清单见 STAR_PSF_ALGORITHMS §11.3（001 参数序/常数
  耦合/002 前向差分+硬钳位/003 maxIter+tolerance 死参数/004 无取消点/
  005 无协方差输出/006 批退败静默）。
- plan/execute/cancel/inspect 迁移语义见 module.yaml（astrocs.p1.psf /
  astrocs_p1_psf.dll，C ABI adapter 由 P1-PSF-IMPL 建立；本节描述现状 API，
  不声明 DLL 化完成）；测试设计 TEST-PSF-DESIGN-001（§11.4）由 P1-PSF-TEST
  执行落 TEST-P1-PSF-001 + EVIDENCE。

## Phase1 装配会话 C API（API-P1-SESSION）

> ID: API-P1-SESSION  状态: CONTRACT_READY（P1-SESSION-DOC 冻结，2026-09-07）
> 模块: lib/phase1_session（MOD-astrocs-phase1-session；assembly 层，唯一
> 权威签名头 lib/phase1_session/p1_session.h:16-37，禁止手抄他版）。
> 编排级上游合同 API-P1-001（docs/api/PHASE1_API_V1.md FROZEN）；数据面
> DATA-P1-SESSION（DATA_SEMANTICS §16）。registry 关系：五函数经 P1Api
> （lib/core/src/module_adapters.cpp:755-762）被 8 个 Phase1 descriptor
> 工厂委托（:728-735/:755-770）。

### 范围界定

本节只登记装配会话入口符号与调用时序；不定义任何算法（校准/cosmetic
算法委托 API-CAL-001 / API-COS-001 既有符号，其余阶段执行现状见
"生产调用方与编排现状"）。不含 Phase2/3 接口。

### 导出符号（p1_session.cpp 实测行号锚，5 C API + 1 C++ 诊断全部当前真实存在）

| 符号 | 锚 | 语义 |
|---|---|---|
| `p1_session_create` | p1_session.h:16 / p1_session.cpp:90 | host services 单结构注入（struct_size/ABI 校验 :91-93）；opaque handle，owner=创建者 |
| `p1_session_validate` | p1_session.h:19 / :103 | 纯读无 IO 幂等；config 键集校验（DATA-P1-SESSION §16.1）；缺必需键/类型错→PARAM，无 silent default |
| `p1_session_run` | p1_session.h:23 / :149 | 四段执行 io_read→calibrate→cosmetic→io_write（:168/:195/:283/:319）；async_io_depth∈{0,1,2}（:152） |
| `p1_session_inspect` | p1_session.h:26 / :339 | manifest JSON（dump(2)）；out=host alloc，调用方经 host free 释放（:349-357） |
| `p1_session_destroy` | p1_session.h:28 / :358 | 唯一释放对（delete SessionState） |
| `astrocs::phase1::last_error` | p1_session.h:36 / :368 | C++ 诊断：脱敏摘要，handle 空→空串；非科学接口 |

### 签名要点与内存所有权

- handle：`acs_handle` opaque；生命周期=唯一 create/destroy 对。
- config：`acs_span_u8`（调用方内存，会话内解析为 JSON，不持久持有）。
- inspect 输出：`acs_span_u8` host allocator 分配（16 对齐，:351），
  调用方必须 host free；内容 UTF-8 JSON 文本（DATA-P1-SESSION §16.3）。
- host services：`astrocs_host_services_v1`（common_abi_v1.h:110-117）
  allocator/logger/cancel/budget 四通道；budget.max_workers 注入
  `ac_set_num_threads`（p1_session.cpp:162-165，迁移整改点）。

### 调用时序

create →（可选）validate → run → inspect → destroy；validate 可独立
调用（幂等）；run 前 config 必先 validate（run 内二次解析失败仍 PARAM，
:155-159）；destroy 后 handle 一律失效；inspect 于 run 前调用返回
created 状态 manifest（:342-343）。B 线 registry 通道经 SessionModule
等价执行（execute 内 create→validate→run→inspect 捕获→destroy，
module_adapters.cpp:153-216）。

### 返回码

ACS_OK；ACS_ERR_ABI_MISMATCH（host 结构/ABI :91-93）；ACS_ERR_PARAM
（config/键集/类型/尺寸匹配 :110-143/:218-221/:236-239/:152）；ACS_ERR_IO
（文件读写 :183-187/:233-235/:261-267/:306-310/:323-327）；ACS_ERR_INTERNAL
（ac_* 委托非 OK :249-253/:301-305）；ACS_ERR_NOMEM（:96/:351）；
ACS_ERR_CANCELLED（文件/帧粒度取消点 :177-181/:228-231/:289）。取消语义：
清理后短路返回，不留伪完整产物（p1_session.cpp:4 注释合同）。

### 单位/dtype/shape

见 DATA-P1-SESSION（DATA_SEMANTICS §16）：像素 float32 ADU [h,w]；
manifest 字段 dtype 逐项登记；坐标/单位词汇沿用 GLOSSARY（ADU/0-based
像素），本节不新增科学单位。

### 线程安全与确定性

- p1_session.h:15 并发合同：reentrant:yes；threadsafe:no（handle 级——
  单 handle 单线程）；内部并行=omp，worker 数=host budget.max_workers
  注入（:162-165），禁硬编码核数。
- B 线 registry 通道：ThreadLease 租借+RAII 归还（module_adapters.cpp:
  156-162），预算耗尽→1 worker 串行；determinism=fixed_reduction_order
  （module.yaml；阶段序固定 :168-330，逐段短路返回）。

### 生产调用方与编排现状

- 生产调用方=lib/core/src/module_adapters.cpp。**P1-001（2026-09-10）
  真实节点化后**：8 个 Phase1 descriptor 各自委托唯一真实 operation
  （p1_nodes[]：calibrate→ac_calibrate_frame、cosmetic→ac_correct_frame、
  star-psf→StarDetector、wcs→WcsTan、photometry→Photometer、noise-snr→
  NoiseModel、drizzle→hp_drizzle_run、writer→aio_write_fits；子节点不调
  完整 phase_session_run，ARCH-P0-001 整改）；P1Api/SessionModule 保留
  兼容面。无 CLI/测试外的其他直接调用方（生产可达性由
  tools/quality/check_prod_reachability.py:42 与
  tools/check_pipeline_trace.py:16 登记锚）。
- **P1-001 attempt 2 三域真实化（2026-09-10）**：star-psf→lib/star_detector
  生产检测（sdet C 头，StarDetector C++ 类仅薄包装）+lib/dynamic_psf
  `dpsf_fit_batch_f64`（Moffat4 FP64 批量 PSF 拟合，DATA-P1-PSF 携
  psf_params:FLOAT64[N,9]）；wcs→lib/plate_solve ipv 真实求解链
  `ipv_solve_from_memory_with_callback_d`（sdet+gaia_client 句柄注入；
  非 Windows 平台为生产源内建 stub，节点 fail-closed 如实报平台限制，
  Windows 侧真实求解；缺求解参数 DATA 拒绝）；writer→lib/astro_image_io
  aio_hips 写链（p1_stack.hiss→`aio_hiss_inspect/read_tile_*`→
  `AstroSphereTileView`→`aio_hips_product_begin/write_signal_support_tile/
  finalize`，NESTED 聚合 IVOA 1.4 标准 512×512 HiPS）。生产源零 diff
  （只调用不修改）；三域模块库构建接线 astrocs_p1_dpsf/astrocs_p1_ipv/
  astrocs_p1_sdet（CMakeLists.txt）。
- 如实差距（P1-001 后）：API-P1-001 冻结 7-stage 序列 vs session 现行
  4 段（CAL+COS）——六域已在 B 线 registry 通道由 P1-001 真实节点委托
  承载（p1_nodes[]），session 内 7-stage 段补齐归 P1-SESSION-IMPL；
  完整 7-stage 生产链现状=A 线 orchestrator DLL 链（production_call_paths_stage1.csv，
  如 PHOTOMETRIC 生产调用 orchestrator.cpp:2474→:2714/:2790）。补齐归
  P1-SESSION-IMPL，本节不宣称 session 已完成 7-stage。

### 已登记现状缺陷与迁移语义

- cosmetic master 实参 nullptr→检测禁用恒等 pass（:295-296，DISP-COS-009，
  整改归 P1-COS-INT）；dark_scale_factor 键 validate 不验（:225 run 兜底
  1.0）；async_io_depth 预读未按 depth 启用（PHASE1_API_V1 §1 预留）；
  输出仅 FITS。整改归 P1-SESSION-IMPL，登记不改码。
- 迁移：MODULE_MIGRATION_MATRIX 无 P1-SESSION 行（assembly 层不设独立
  DLL）；registry descriptor 占位词汇（ALG-002/004/005、TEST-P1-*-001）
  不作冻结依据（P1-PSF-DOC 先例）。测试锚 TEST-P1-SESSION-001=
  tests/unit/p1_ir_facade_test.cpp（facade/canonical 节点集/委托语义）。

## 星点检测 C API（API-STAR-001）

> ID: API-STAR-001  状态: CONTRACT_READY（P1-STAR-DOC 冻结，2026-09-07）
> 模块: lib/star_detector;lib/phase1/stars（MOD-astrocs-phase1-star，matrix
> P1-STAR，迁移目标 astrocs_p1_star_detection.dll；唯一权威签名头
> lib/star_detector/include/star_detector.h:1-73，禁止手抄他版）。
> 编排级上游合同 API-P1-003（PHASE1_API_V1 §2：一帧只做一次权威检测）；
> 数据面 DATA-P1-STAR（DATA_SEMANTICS §17）；算法权威 ALG-STARDET-001
> （STAR_DETECTION_ALGORITHMS §11 逐符号锚）；SCI-P1-STAR-001（本任务
> 冻结层）。

### 范围界定

本节只登记星点检测 9 导出符号 + SDetParams 合同；不定义任何算法
（ALG-STARDET-001 权威）；不含 Phase2/3 接口。编排级合同 API-P1-003
引用本模块符号，生产调用点在「生产调用方与编排现状」小节。

### 导出符号（9 C API 全部当前真实存在）

| 符号 | 锚 | 语义 |
|---|---|---|
| `sdet_create` | star_detector.h:33 / sdet_api.cpp:954 | handle 创建；params=NULL→默认（structureLayers=5/hotPixelFilterRadius=1/iterativeClipSigma=9.0/iterativeMaxRounds=5/medianFilterDetail=1/maxStars=2000/fitRadius=6/fwhmClipSigma=3.0/maxAxisRatio=2.0，:963-975）；生产实参 orchestrator.cpp:1593-1612（fitRadius=0=自动半径） |
| `sdet_destroy` | star_detector.h:34 / :984 | 唯一释放对 |
| `sdet_detect` | star_detector.h:36-40 / :992 | 旧 uint16 入口（仅 x/y；内部旧 CC 路径，非生产，DISP-STAR-005） |
| `sdet_free_coords` | star_detector.h:42 / :1276 | sdet_detect x/y 专用释放 |
| `sdet_detect_debug` | star_detector.h:44-50 / :1281 | 诊断入口（CC 路径 + 平滑图/detail/binary 导出 + extras） |
| `sdet_free_debug_maps` | star_detector.h:52 / :1595 | debug 输出图专用释放 |
| `sdet_detect_ex` | star_detector.h:54-62 / :2318 | 生产 FP32 入口（uint16→float 转换后 impl<float>；10 数组输出 + extras） |
| `sdet_detect_ex_f64` | star_detector.h:67-75 / :2343 | 生产 FP64 入口（全程 double 不降级，PREC-105；out_flux/out_mag 仍 float32 ABI 协议） |
| `sdet_free_detect_ex` | star_detector.h:77-79 / :2357 | 10 数组唯一释放（extras 同组；禁止逐数组 free） |

SDetParams 9 字段（star_detector.h:13-24）：maxStars/maxAxisRatio 生产路径
完整消费；fitRadius 仅驱动 auto 半径推导（ALG-STARDET-001 §11.1）；fwhmClipSigma
仅 debug 入口消费；structureLayers/hotPixelFilterRadius/iterativeClipSigma/
iterativeMaxRounds/medianFilterDetail 仅旧 CC 路径消费——消费面缺口=DISP-STAR-003。

### 签名要点与内存所有权

- handle：opaque `StarDetectorHandle`（star_detector.h:26），owner=创建者；
  handle 级互斥使用（PHASE1_API_V1 §2 表行 handle 级 no/no，无内部锁）。
- 输出数组：模块 malloc，调用方只经 `sdet_free_detect_ex` /
  `sdet_free_coords` / `sdet_free_debug_maps` 释放；n=0 空场 → 输出指针全
  NULL 且 count=0（合法 rc=0）。
- extras：`out_extras` 为 float* 逐列指针数组，`sdet_free_detect_ex` 同组
  释放（:2357-2373）。
- 返回码：0=成功（含 0 星空场）；−1=参数无效/句柄 NULL/分配失败
  （sdet_api.cpp:1612、:2264-2270）。错误所有权按 V14 合同：本模块返回码
  独占定义，调用方只按 0/非 0 分支。

### 调用时序

sdet_create →（多次）sdet_detect_ex / sdet_detect_ex_f64（同 handle 互斥）
→ sdet_free_detect_ex → sdet_destroy；destroy 后 handle 一律失效。旧
`sdet_detect`+`sdet_free_coords` 与 `sdet_detect_debug`+`sdet_free_debug_maps`
为独立释放族，禁止与 detect_ex 族混用。

### 单位/dtype/shape

见 DATA-P1-STAR（DATA_SEMANTICS §17）：x/y double pixel（0-based，像素
中心=索引+0.5）；flux float ADU（正常星=振幅 A）；mag float（NaN=无效）；
saturated/has_saturated int 0/1；图像输入 FP32 通道 uint16（DISP-STAR-001
量化）/ FP64 通道 double；编排序列化 star_det FLOAT64 [N,6]（:2218-2246）。

### 线程安全与确定性

- handle 级互斥（单 handle 单线程）；内部并行=OpenMP（候选拟合 omp for
  dynamic + reduction，:2042-2044；dedup/sort 串行）；输出 bitwise 与线程数
  无关（ALG-STARDET-001 §5）；determinism=fixed_reduction_order
  （module.yaml）。ThreadBudget 接线与取消检查点缺失已登记
  （ALG-STARDET-001 §11.3），整改归 P1-STAR-IMPL。

### 生产调用方与编排现状

- 唯一生产调用方=lib/orchestrator/cpp/src/orchestrator.cpp：run_stage_psf
  （PSF/STAR_MEASURE 阶段权威检测，:2067；函数指针装载 :2149-2165；FP64/
  FP32 通道选择 :2172-2198；star_det 权威块写入 :2237-2246；缓冲释放
  :2466）；PLATESOLVE fallback 读 star_det 块并禁重检测（:1748-1755、
  :1826-1829）；sdet_create 参数构造 :1593-1612。
- API-P1-003（PHASE1_API_V1 §2）表行 `sdet_create/destroy/detect/detect_ex`
  引用本模块符号；descriptor astrocs.phase1.star-psf
  （module_adapters.cpp:492-510）为编排层词汇，由 P1-PSF-INT 对齐，
  不作冻结依据。

### 已登记现状缺陷与迁移语义

- DISP-STAR-001..005 与线程数/取消登记均不改码（ALG-STARDET-001 §11.3），
  整改归 P1-STAR-IMPL/P1-STAR-INT；本节不宣称缺陷已修复。
- 迁移：矩阵行 P1-STAR（owner=SA-P1-S15，legacy_paths=
  lib/star_detector;lib/phase1/stars，迁移目标 astrocs_p1_star_detection.dll；
  C ABI adapter 由 P1-STAR-IMPL 建立；本节描述现状 API，不声明 DLL 化
  完成）。测试锚 TEST-STAR-DESIGN-001（ALG-STARDET-001 §11.4）由
  P1-STAR-TEST 执行落 TEST-P1-STAR-001 + EVIDENCE；registry descriptor
  占位词汇不作冻结依据（P1-PSF-DOC 先例）。

## WCS 求解 C API（API-WCS-001）

> ID: API-WCS-001  状态: CONTRACT_READY（P1-WCS-DOC 冻结，2026-09-07）
> 头: lib/plate_solve/cpp/ipv/include/ipv_api.h（唯一权威签名源，禁止手抄
> 他版；IPV_API extern "C" 导出宏，238 行）
> SRC: lib/plate_solve/cpp/ipv/src/ipv_entry.cpp（649 行；内核
> ipv_solver/ipv_select/ipv_triangle/ipv_itertrans/ipv_robust_refine/
> ipv_wcs/ipv_sip 共 13821 行）
> SCI: SCI-WCS-001（docs/science/ASTROMETRY.md，共享引用不改动）；
> ALG: ALG-WCS-001（PLATESOLVE.md §11 逐符号锚）；DATA: DATA-P1-WCS
> （DATA_SEMANTICS §18，单位/dtype/shape/坐标契约唯一权威）；编排级
> 合同 API-P1-004（PHASE1_API_V1 §2，descriptor 引用，与本节并行不互斥）
> MOD: MOD-astrocs-phase1-wcs-platesolve（module.yaml CONTRACT_READY，
> entrypoint=MISSING；生产调用 orchestrator.cpp:1758 run_stage_platesolve）

### 范围界定

Phase1 单帧天测 WCS 求解 C ABI：星点检测坐标（double [n,6]）+ Gaia DR3SP
参考星 → TAN+SIP 三角形匹配求解，IpvWcsResult POD 输出（CD/CRVAL/CRPIX/
SIP A/B/AP/BP/RMS/CTYPE）。不做星点检测（禁重检测，消费 PSF 产出
star_measurements 权威块，orchestrator.cpp:1748-1755）、不做图像重采样、
不做参考星缓存管理（gaia 句柄由调用方注入）。像素中心双契约（统一契约
index-is-center ↔ IPV 接口契约 center=index+0.5）由调用方显式 +0.5 桥接
（orchestrator.cpp:1867，DATA_SEMANTICS §18.1）。无取消检查点
（DISP-WCS-005）。

### 导出符号（ipv_api.h/ipv_entry.cpp 实测行号锚，12 个全部当前真实存在）

| 符号 | 头锚 | 定义锚 | 摘要 |
|---|---|---|---|
| ipv_solve_create | ipv_api.h:84 | ipv_entry.cpp:237 | 创建 IPVSolver 句柄 |
| ipv_solve_destroy | :87 | :249 | 释放句柄（整句柄唯一释放入口） |
| ipv_set_gaia_handle | :90 | :260 | 注入 Gaia DR3SP 客户端句柄（intptr_t） |
| ipv_set_detector_handle | :93 | :273 | 注入 sdet 句柄（intptr_t） |
| ipv_get_default_params | :198 | :286 | IpvParams 默认值（log_dir 空=无日志文件） |
| ipv_get_last_inlier_count | :224 | :314 | 最近一次求解 inlier 计数 |
| ipv_get_last_inliers | :232 | :328 | inlier 9 列缓冲拷出（ipv_api.h:203-221） |
| ipv_solve | :97 | :345 | 文件路径入口（legacy，非生产） |
| ipv_solve_from_memory | :110 | :377 | PipelineFrame 内存入口 |
| **ipv_solve_from_detections_v1** | :146 | :524 | **生产入口**（检测坐标数组直入，orchestrator 实调） |
| ipv_solve_from_memory_with_callback | :165 | :566 | 回调进度变体 |
| ipv_solve_from_memory_with_callback_d | :182 | :610 | 回调变体 FP64 |

核心结构体：IpvWcsResult 16 字段（:39-61，cd[4]/crval[2]/crpix[2]
1-based/sip_a·b·ap·bp[36]/rms_px/rms_arcsec/n_pairs/success/
trans_order/ctype1·2[16]/error_msg[256]）；IpvParams（:64-80，log_dir[256]
等）。inlier 缓冲 9 列（:203-221）=det_x,det_y,gaia_ra,gaia_dec,pred_x,
pred_y,residual_x,residual_y,residual_dist。

### 签名要点与内存所有权

- ipv_solve_from_detections_v1（:146-163）：入参 solver/detections [n,6]/
  n_detections/image_width/height/ra0/dec0/focal_length_mm/pixel_size_um/
  IpvWcsResult*；返回 0=失败/1=成功；NULL 参数 → ret=0（:524 入口校验）。
- IpvWcsResult 为调用方栈/堆分配 POD（sizeof 固定），DLL 仅写不 malloc；
  无配套 free 函数（与 dpsf 先例不同，无堆所有权转移）。
- ipv_get_last_inliers（:232）：调用方预分配 9·max_count double 缓冲，
  返回实际拷贝数；数据来自求解器内部缓存 cache_last_inliers_
  （ipv_solver.cpp:744-752，WCS Gate v2 双层闭环）。
- 全部接口不抛异常（C ABI，try/catch → set_error_msg，ipv_entry.cpp:141、
  :181-187/:218-224）。

### 返回码

- ret：0=失败（error_msg 载因：NULL 参数/求解失败/C++ 异常）、1=成功
  （success=1 且 trans_order∈{1,2,3}）。
- result->success 与 ret 同步；失败时 trans_order=0、n_pairs=0、rms=0
  （fail_result，ipv_solver.cpp:396-398）——**rms=0 不代表完美解，必须与
  success 联合解读**（DATA_SEMANTICS §18.2）。
- 失败-置信度语义（DISP-WCS-001，PLATESOLVE.md §11.3）：CD det 退化坍缩
  （近似 CRPIX 的解）必须以 success=0 呈现，禁止冒充成功解；现状
  wcs_tan.cpp:48-51/wcs_transform.cpp:39-49 同族缺陷已登记不改码，整改归
  P1-WCS-IMPL/P1-PHOT-IMPL。

### 单位/dtype/shape

见 DATA_SEMANTICS §18（唯一权威）：detections [n,6] double（det_x/det_y
为 +0.5 契约 pixel；flux ADU；mag mag；sat/has_sat 0/1）；ra0/dec0 deg；
focal_length_mm mm；pixel_size_um μm；输出 cd deg/pixel、crval deg
（ICRS/J2000）、crpix 1-based pixel、rms_arcsec/rms_px、SIP 无量纲
（AP[6]−=1、BP[1]−=1 约定，Y-down 符号已折入）。

### 线程安全与确定性

- 句柄级互斥使用（同一 solver 句柄禁止跨线程并发求解）；gaia/detector
  句柄生存期由调用方保证。
- OpenMP 并行点：三角形投票（ipv_triangle.cpp:302/:347，线程局部矩阵 +
  整数归并 collapse(2) schedule(static)）、选星（ipv_select.cpp:810/:1123/
  :1412/:1756）——投票与拟合归并为整数/单线程浮点，输出 bitwise 与线程数
  无关（determinism=fixed_reduction_order，ALG-WCS-001 §11.4 F5 冻结断言）。
  线程数未接 ThreadBudget（迁移整改点，module.yaml
  threading_model=host_executor_lease 为合同值，ThreadLease/取消检查点归
  P1-WCS-IMPL，DISP-WCS-005）。

### 生产调用方与编排现状

- orchestrator.cpp:1758 run_stage_platesolve（必需 stage，DLL 未加载
  :1763-1764 退出码 2）→ init 段 ipv_solve_create/ipv_set_gaia_handle/
  ipv_set_detector_handle（:1621-1643）→ 消费 star_measurements [N,≥15]
  权威块（过滤 status∈{0,3}/sat/fwhm∈[0.5,20]/边缘 5px，+0.5 转换 :1867）
  + star_det fallback（DETECTOR_FALLBACK，<0.5px 去重）→ 调用
  ipv_solve_from_detections_v1（:1967）→ 失败 → PLATESOLVE_FAILED
  （:1980）→ WCS 头写回 CTYPE/CRVAL/CRPIX/CD + RADESYS=ICRS/EQUINOX=2000
  （:2003-2010）+ SIP A/B/AP/BP 写回（:2017-2049）。
- star_det/star_det_psf_compat/star_measurements 均由上游 PSF/STAR_MEASURE
  产出，本阶段不重写（:2057）。
- registry descriptor 占位 ID（module_adapters.cpp:512-526，module_id=
  astrocs.phase1.wcs-platesolve、ports sources/wcs、sci_id=SCI-P1-WCS-001/
  alg_id=ALG-002/data_id=DATA-P1-WCS/api_id=API-P1-004/test_id=
  TEST-P1-WCS-001）由 P1-WCS-INT 对齐本合同，不得反向作为冻结依据。
- 现状构建 lib/plate_solve/cpp/ipv/build.ps1:27 / Makefile:6（g++
  -fopenmp -O3 → ipv_solver.dll，MSYS2/MinGW）；未编入根 CMake 主构建
  （根 CMakeLists.txt 无 ipv 目标）——astrocs_p1_wcs.dll 迁移由
  P1-WCS-IMPL 建立。

### 已登记现状缺陷与迁移语义

- DISP-WCS-001..006 全清单见 PLATESOLVE.md §11.3（001 CD 退化静默坍缩
  R1/002 错误通道缺失族/003 双 SIP 拟合路径并存/004 AP/BP 半静默/
  005 取消点+ThreadBudget 缺失/006 三套 TAN 实现并存）。
- plan/execute/cancel/inspect 迁移语义见 module.yaml（astrocs.p1.wcs /
  astrocs_p1_wcs.dll，C ABI adapter 由 P1-WCS-IMPL 建立；本节描述现状
  API，不声明 DLL 化完成）；测试设计 TEST-WCS-DESIGN-001（§11.4）由
  P1-WCS-TEST 执行落 TEST-P1-WCS-001 + EVIDENCE。

## Coverage union C API（API-COV-001）

> ID: API-COV-001  状态: CONTRACT_READY（P2-COV-DOC 冻结，2026-09-07）
> 头: lib/phase2/include/astro/phase2/coverage.h（唯一权威签名源，59 行，
> 禁止手抄他版；P2_API 导出宏 :16-20 Windows dllexport/POSIX 默认可见）
> SRC: lib/phase2/src/coverage.cpp（239 行；生产目标根 CMake
> astrocs_phase2 静态库 CMakeLists.txt:338-346，独立 self-build
> lib/phase2/CMakeLists.txt:42-46 phase2 STATIC；astrocs_p2_coverage.dll
> 为迁移目标合同值，尚未存在，由 P2-COV-IMPL 建立）
> SCI: SCI-UPM-001/SCI-INT-001/SCI-SCOPE-001（docs/science/ 共享 FROZEN
> 引用不改动，SCI 层声明=PHASE2_COVERAGE.md §11.5）；ALG: ALG-COV-001
> （PHASE2_COVERAGE.md §2/§11 逐符号锚）；DATA: DATA-COV-001
> （DATA_SEMANTICS §19，单位/dtype/shape/坐标契约唯一权威）；编排级
> 合同 API-P2-001（docs/api/PHASE2_API_V1.md FROZEN §1 所有权图/
> §2 并发五字段行 1/§4 错误码映射，与本节并行不互斥）；MOD:
> MOD-astrocs-phase2-coverage（module.yaml CONTRACT_READY，
> entrypoint=MISSING；编排消费 lib/phase2_session/p2_session.cpp:119-148）

### 范围界定

Phase2 输入发现/兼容校验/coverage union/target_order C ABI：N 个
Phase1 单帧 HiPS（signal 子目录）→ 逐帧 properties 兼容校验（hips_order/
tile_width=512/hips_version/hips_frame/obs_filter）+ union MOC（NESTED
父单元聚合）+ target_order=min(逐帧 order)。不做：逐帧重校准/PlateSolve/
PSF/Drizzle（coverage.h:6）、像素数据读取（只读 properties+Moc.fits）、
intersection/depth/missing-tiles 产品（DISP-COV-004）、任何科学权重
计算（合同红线：coverage 禁作隐式科学权重，PHASE2_COVERAGE.md §7）。
无取消检查点（API-P2-001 §2 行 1 取消点=无，阶段级取消由编排 session
阶段边界提供，p2_session.cpp:120）。

### 导出符号（coverage.h/coverage.cpp 实测行号锚，2 个全部当前真实存在）

| 符号 | 头锚 | 定义锚 | 摘要 |
|---|---|---|---|
| p2_coverage_build | coverage.h:52 | coverage.cpp:144 | 发现+校验+union MOC+target_order（两阶段容量协议） |
| p2_coverage_free | coverage.h:56 | coverage.cpp:233 | POD 清零（不释放堆，无所有权转移） |

内部链接符号（非导出，登记备查）: inspect_frame（coverage.cpp:59-140，
匿名 namespace :55-142）、parse_props（coverage.cpp:20-45，文件作用域
static）。

核心结构体: P2MocCell 2 字段（coverage.h:26-29，order/ipix 均uint64）；
P2HipsInputInfo 7 字段（:31-38，hips_path[1024]/frame_id[64]/
max_leaf_order/n_tiles/filter_passband[64]/frame_type[32]）；P2CoverageResult
7 字段（:40-48，n_inputs/inputs/n_union_cells/union_cells/target_order/
status/error[512]）。字段级单位/值域唯一权威=DATA_SEMANTICS §19.2。

### 签名要点与内存所有权

- p2_coverage_build（coverage.h:52-54）: 入参
  `const char* const* hips_paths, uint64 n_inputs, P2CoverageResult* out`；
  返回 int rc（0=成功含 K=0 空 union / 1=失败，error[512] 载因）。
  两阶段协议: 第一次 `out->union_cells=NULL`（及 inputs=NULL）→ rc=0
  得 n_union_cells=K 容量；调用方分配 K 个 P2MocCell（及 n_inputs 个
  P2HipsInputInfo）后第二次调用回填数据（coverage.cpp:219-228；每次
  调用全量重扫，无缓存，DISP-COV-005）。
- P2CoverageResult 及全部数组由调用方分配（coverage.h:42/:44 注释）；
  p2_coverage_free 仅 `memset(out,0,sizeof(*out))`（coverage.cpp:235）
  ——与 PHASE2_API_V1 §1 所有权图 Coverage 行（build 创建/调用方持有/
  p2_coverage_free 释放/下游只读借用，docs/api/PHASE2_API_V1.md:15）
  一致；无 malloc/无异常跨界。
- 错误通道: rc=1 + out->error[512]（"no inputs"/"empty path at index
  %llu"/"missing hips_order"/"unsupported tile_width"/"missing
  hips_version"/"unsupported hips_frame"/"filter mismatch"/AIO
  last_error 透传 :63-64）；错误码编排映射归 API-P2-001 §4
  （ACS_ERR_PARAM/ACS_ERR_STATE）。
- "no inputs" 分支 rc=1 而 status=0 不一致（coverage.cpp:154-157；
  memset :151 先行、strncpy :155 后写，error 有效；status 保持 0，
  DISP-COV-001）——调用方以 rc 为准，status 语义整改归 P2-COV-IMPL。

### 返回码

- rc: 0=成功（含空 union K=0）；1=失败（§5 全部拒绝路径，error 载因）。
- status: 0=ok（:229）；错误路径=1（:168/:177/:190/:200），
  "no inputs" 分支例外=DISP-COV-001（同上，整改门由
  TEST-COV-DESIGN-001 F5 固化）。
- 并发合同（API-P2-001 §2 行 1）: reentrant=yes / threadsafe=no
  （独立对象，无内部锁）/ internal_parallel=none（单线程，无
  OpenMP，bitwise 确定）/ 取消点=无 / TST-COV-*（TEST-P2-COV-001
  由 P2-COV-TEST 落地，设计=PHASE2_COVERAGE.md §11.4
  TEST-COV-DESIGN-001）。
- thread budget: coverage 阶段未单列预算（PHASE2_API_V1 §3 预算分配
  冻结清单不含 coverage——阶段级取消/manifest 登记由 session 编排层
  承担，p2_session.cpp:119-148）；threading_model=host_executor_lease
  为 module.yaml 合同值，ThreadLease 接线归 P2-COV-IMPL
  （DISP-COV-005 同族整改域）。

### 单位/dtype/shape

见 DATA_SEMANTICS §19（唯一权威）：输入 hips_paths [n_inputs] 字符串
数组；输出 P2MocCell [K]（order/ipix 无量纲整数，NESTED 父单元索引
<12·4^order）、P2HipsInputInfo [n_inputs]（hips_path[1024]/
frame_id[64]/filter_passband[64]/frame_type[32] 字符串 + order/tiles
整数）、target_order int（=min 逐帧 hips_order）；全链路整数运算
bitwise 确定；坐标=HEALPix NESTED equatorial/ICRS（非 PIXEL——
registry descriptor 像素登记由 P2-COV-INT 修订）。

### 生产调用方与编排现状

- 编排 session: lib/phase2_session/p2_session.cpp:119-148 —— coverage
  为 Phase2 DAG 首阶段（阶段边界取消检查 :120，manifest 登记
  n_union_cells/target_order :146-147）；两阶段调用 :125/:138。
- 下游模块消费: sampler p2_sample_controls*（sampler.cpp:464/:504/:632/
  :1138）、stage2 正式入口（lib/phase2/tools/stage2.cpp:189-200/
  :212-213）、registry
  descriptor astrocs.phase2.coverage（module_adapters.cpp:623-638，
  端口 calibrated=DATA-P2-CAL/ADU/PIXEL→coverage=DATA-P2-COV/
  DIMENSIONLESS/PIXEL——出端口坐标登记以本 API/DATA 合同 NESTED 为准
  修订，P2-COV-INT 对齐）。
- 现状构建: 根 CMakeLists.txt astrocs_phase2 STATIC（:338-346，含
  src/coverage.cpp，无独立 DLL target）；lib/phase2/CMakeLists.txt
  phase2 STATIC 兼容自测 target（:42-46）+ phase2_synthetic_gate
  （:77-79）；astrocs_p2_coverage.dll 为迁移目标合同值（尚未存在），
  CMake 集成归 P2-COV-IMPL。
- 既有测试基线: Phase2Coverage.RealHipsUnion（synthetic_gate.cpp:3374）
  / FilterMismatchRejected（:3410）——依赖 Fatduck 本地路径，缺失时
  GTEST_SKIP（:3376/:3413）；合成 fixture 由 P2-COV-TEST 建立
  （TEST-COV-DESIGN-001 F1，解除环境依赖）。

### 已登记现状缺陷与迁移语义

- DISP-COV-001..005 全清单见 PHASE2_COVERAGE.md §11.3（001 "no inputs"
  status 不一致 / 002 frame_id 基名截断唯一性风险 / 003 空 filter
  静默放行 / 004 intersection/depth/missing-tiles 产品缺失（合同范围
  缺口，覆盖度几何≠权重因子，关联 R3-A geometric_reliability 乘数
  恒 1.0=DISP-UPM-003，修正归 P2-UPM 域）/ 005 extern "C" include
  卫生+两阶段全量重扫+ThreadLease 未接线）。
- plan/execute/cancel/inspect 迁移语义见 module.yaml（astrocs.p2.coverage
  / astrocs_p2_coverage.dll，C ABI adapter 由 P2-COV-IMPL 建立；本节
  描述现状 API，不声明 DLL 化完成）；测试设计 TEST-COV-DESIGN-001
  （§11.4）由 P2-COV-TEST 执行落 TEST-P2-COV-001 + EVIDENCE。

## Phase2 mosaic write 公共消费面（API-P2-HIPS-001）

> ID: API-P2-HIPS-001  状态: CONTRACT_READY（P2-HIPS-DOC 冻结，
> 2026-09-07）
> 定位: Phase2 马赛克写出**当前无独立公共 C API**——生产入口=
> stage2 工具（lib/phase2/tools/stage2.cpp main :112）；编排层经
> API-P2-001（PHASE2_API_V1 phase session，p2_session）驱动，但
> p2_session 不执行 HiPS 写（hips_paths/output_dir 校验
> p2_session.cpp:81-92，coverage 两阶段调用 :125/:138；HiPS 写入仅
> 发生在 stage2 工具）。本节冻结的是 stage2 配置 schema 的公共消费
> 面 + 进程退出码 + diagnostics.json 键集。
> SRC: lib/phase2/tools/stage2.cpp（生产工具 astrocs-stage2，唯一
> 写入路径）；配置 schema 唯一权威签名源 lib/phase2/include/astro/
> phase2/stage2_common.h:16-99（P2Stage2Config，完整字段集以头文件
> 为准，本节冻结公共关键字段消费面语义）；DATA: DATA-P2-HIPS
> （DATA_SEMANTICS §20，单位/dtype/shape/序合同唯一权威）；复用库
> ABI: aio_hips_*（本文件既有 API-HIPS-001 节，P1 冻结面）——P2 作为
> 库消费者经 aio_hips_product_begin（stage2.cpp:592-596）等引用，
> **不重登记、不新增 C ABI**；ALG: ALG-P2-HIPS-001..004
> （docs/algorithms/PHASE2_MOSAIC_WRITE.md）；MOD:
> astrocs.p2.hips_writer（迁移目标 astrocs_p2_hips_writer.dll 为
> 矩阵合同值，尚未存在——MISSING 语义，由 P2-HIPS-IMPL 建立，本节
> 不声明 IMPLEMENTED）。

### 入口与调用方式（astrocs-stage2）

- 用法（stage2.cpp:132）:
  `astrocs-stage2 <stage2.json> [--cpu-workers N] [--io-workers N]
  [--gpu-route cpu|auto|cuda] [--deterministic 0|1]`
  （usage 行 :132；CON-002 CLI override 全局 worker 预算，
  覆盖 config execution block，:155-166）。CLI 科学参数禁止
  （stage2.cpp:3 头注，唯一参数=stage2.json 路径 + 预算四选项）。
- 编排层关系: p2_session（API-P2-001，lib/phase2_session/
  p2_session.cpp）仅做配置校验（:81-92）与 coverage 阶段
  （:125/:138 两阶段容量协议），**不调用 HiPS 写出**；马赛克写
  属 stage2 工具职责，编排接入点为 descriptor
  astrocs.phase2.write 端口表（lib/core/src/module_adapters.cpp
  :677-694，DATA-P2-HIPS §20.3 编排层词汇注记）。

### 配置 schema 公共消费面（P2Stage2Config 关键字段，stage2_common.h:16-99）

| 字段 | 默认 | 域 | 语义 |
|---|---|---|---|
| hips | —（:19） | 路径数组 `[n_frames]` | 每帧 Phase1 HiPS 目录（读 signal/support，weight_mode=2 加读 ivar） |
| target_order_spec / target_order | "auto" / −1（:20-21） | HEALPix order | auto=cov.target_order；显式值不得高于输入最高 order（stage2.cpp:203-205） |
| precision | 0（:50） | 0/1 | 0=float32 / 1=float64 输出（stage2.cpp:528） |
| memory_limit_mb | 24576（:51） | MB | CON-002 内存预算 |
| reject_method / reject_profile / reject_underdetermined_n | P2_REJECT_AUTO / "wbpp_2_9_1" / 2（:52-54） | 无量纲 | planning 层 rejection 解析（profile 版本化冻结） |
| reject_normalization | "astrocs_median_center_v1"（:56） | 无量纲 | 判定工作域归一（mask 应用回原始值） |
| large_scale_enabled（+ min_structure_pixels/low_grow/high_grow） | false / 8 / 2 / 2（:60-63） | 无量纲 | astrocs.large_scale_rejection.v1，默认关闭 |
| weight_mode | 2（:90） | 0/1/2 | 2=ivar（科学默认）/1=等权/0=support×snr²（legacy/诊断） |
| legacy_allow_weight_fallback | false（:94） | bool | ivar 产品缺失默认 rc=7；true 才允许降级 support 标红（stage2.cpp:565-578） |
| acr_route | "auto"（:95） | cpu/auto/cuda 族 | 集成执行路由 |
| out_hips | —（:97） | 路径 | 输出 HiPS 产品集根目录 |
| diagnostics | true（:98） | bool | true → 落 diagnostics.json（本节键集） |

单位/dtype/shape/序合同唯一权威 = DATA_SEMANTICS §20
（DATA-P2-HIPS）；本表仅为消费面副本，冲突以 §20 为准。

### 退出码表（stage2 进程级）

| 退出码 | 域 | 锚（stage2.cpp） |
|---|---|---|
| 2 | config 解析/CLI 参数错误 | :133/:140/:146/:153/:160-165 |
| 3 | coverage 构建 / target_order 校验 | :195/:201/:207 |
| 4 | frame_id / sampler 域 | :227/:283/:288/:292/:298/:303/:310/:315/:319 |
| 5 | UPM 构建/持久化 | :437/:456/:477/:488 |
| 6 | 写路径/集成块（rejection resolve、tile 写、large_scale 等） | :517/:546/:589/:601/:653/:687/:793/:1055 等 |
| 7 | ivar 门（ivar 产品缺失且未显式降级）/ HIPS_VERIFY 回读失败 | :574/:1665 |

### diagnostics.json 键集（stage2.cpp:1697-1749，diagnostics=true 时落 `<out_hips>/diagnostics.json`）

- 版本/路由: stage2_version（:1697）/ acr_requested_route（:1698）/
  acr_effective_route（:1699）/ acr_workers（:1700）/
  acr_fallback_reason（:1701）。
- 输入/UPM 规模: input_frames（:1702）/ component_count（:1703）/
  observations（:1704）/ unique_controls（:1705）/
  controls_with_depth_1（:1706）/ controls_with_depth_ge_2（:1707）。
- 产出统计: tiles_written（:1708）/ output_pixels（:1709）/
  rejected_samples（:1710）/ fallback_pixels（:1711）/
  pixels_depth_0/1/ge_2（:1712-1714）/ integrated_pixels（:1727）/
  zero_coverage_pixels（:1737）/ underdetermined_pixels（:1738）。
- rejection 明细: reject_normalization（:1715）/ reject_profile
  （:1716）/ reject_group_level（:1717）/ reject_group_method
  （:1718）/ large_scale_enabled（:1719）/
  large_scale_min_structure_pixels（:1720）/
  large_scale_low_grow_radius_pixels（:1722）/
  large_scale_high_grow_radius_pixels（:1724）/
  large_scale_grown_samples（:1726）/ quality_fallback_unknown
  （:1728）/ local_snr_used（:1729）/ frame_snr_median_fallback
  （:1730）/ weight_mode（:1731）/ local_ivar_used（:1732）/
  ivar_product_missing（:1733）/ local_snr_unavailable_controls
  （:1734）/ upm_sigma_floor（:1735）/ upm_support_power（:1736）/
  reject_method（:1739）/ reject_underdetermined_n（:1740）/
  rejection_resolved_methods（:1744）/ rejection_samples_per_pixel
  （:1745）。
- provenance/耗时: model_hash（:1746，UPM 持久层 hash，provenance
  链 DATA-P2-HIPS §20.3）/ runtime_seconds（:1747）。

### 复用库 ABI 与负向条款

- aio_hips_*（API-HIPS-001，P1 冻结面）: P2 为库消费者
  （aio_hips_product_begin/write_signal_support_tile/finalize/last_
  error，stage2.cpp:592/:1629/:1638 路径），语义归 API-HIPS-001 与
  DATA-P1-HIPS §12，本节不重登记。
- 负向条款: **P2-HIPS-DOC 不新增、不修改任何公共 C 头/C ABI**——
  p2_* 导出（coverage/sampler/upm/integrate）已由既有节冻结，
  astrocs_p2_hips_writer.dll 为迁移目标合同值（MISSING），C ABI
  adapter 由 P2-HIPS-IMPL 建立后方可登记导出符号表。
- 下游: TEST-P2-HIPS-001（MISSING，P2-HIPS-DOC 登记，由
  P2-HIPS-TEST 落地 + EVIDENCE）；上游 ALG-P2-HIPS-001..004。

## Phase2 integration 公共消费面（API-P2-INT-001）

> ID: API-P2-INT-001  状态: CONTRACT_READY（P2-INT-DOC 冻结，
> 2026-09-09）
> 定位: Phase2 逐像素加权积分内核公共 C ABI 消费面——导出符号
> `p2_integrate_pixel` / `p2_validate_candidate_weights`（既有 V17
> 清单行 17-19/24-29 的展开冻结，**不新增、不修改任何 C 头/C
> ABI**）；编排层经 API-P2-001（PHASE2_API_V1 phase session）驱动，
> 内核本身无 session 依赖（无状态纯函数）。
> SRC: lib/phase2/src/integrate.cpp（76 行，astrocs_phase2 静态库
> 成员，根 CMakeLists.txt:336-346/:344）；唯一权威签名头
> lib/phase2/include/astro/phase2/integrate.h（74 行: P2PixelStack
> :36-42 / P2IntegrateStatus :45-51 / P2PixelResult :53-63 / 函数
> 声明 :58-66）；DATA: DATA-P2-INT（DATA_SEMANTICS §21，单位/dtype/
> shape 唯一权威）；ALG: ALG-P2-INT-001（docs/algorithms/
> PHASE2_INTEGRATION.md，逐符号锚与并行 tolerance 合同）；MOD:
> astrocs.p2.integration（迁移目标 astrocs_p2_integration.dll 为
> 矩阵合同值，尚未存在——MISSING 语义，由 P2-INT-IMPL 建立，本节
> 不声明 IMPLEMENTED；descriptor 占位 module_id=astrocs.phase2.
> integrate 为编排层词汇，module_adapters.cpp:719-737，由 P2-XX-INT
> 对齐）。

### 导出符号与签名要点（integrate.h:58-66，冻结）

- `int p2_integrate_pixel(const P2PixelStack*, P2PixelResult*)`
  （:58-59）: rc=1 仅 stack/result null（integrate.cpp:20-21）；
  rc=0 时语义由 `result->status` 承载（五态，:45-51）。线程安全=
  reentrant yes / threadsafe no（无锁无全局态，并发由调用方像素
  划分）；无取消检查点（ThreadLease 接线归 P2-INT-IMPL）。
- `int p2_validate_candidate_weights(const double*, std::uint32_t)`
  （:62-66）: 预检门 → 0 合规 / 1 违规（null→0；任一 !finite 或
  w<0→1；w==0 合规，integrate.cpp:10-17）；调用方权重构造后必经
  （stage2.cpp:1141/:1402）。
- 输入域/输出域: P2PixelStack 四数组可空语义（weights null=等权、
  support null=1.0、accepted null=全接受）与五态触发条件=唯一
  权威 DATA-P2-INT §21.1/§21.3（本节不重复展开）。

### 单位/dtype/shape（消费面副本，唯一权威=DATA_SEMANTICS §21）

| 项 | 值 |
|---|---|
| values/signal | f64，ADU（f32/f64 写盘转换在 Stage2 precision） |
| weights/wsum | f64，1/ADU²（数值权重，本层无 ivar 语义） |
| support（in/out） | f64，无量纲 [0,1] |
| accepted | u8 0/1；count/计数器 u32 |
| 调用粒度 | 单像素栈（count 候选）；frame-major 批量由调用方逐像素切出（stage2.cpp:1213-1223/:1515-1527、acr_kernels.cpp:189-195） |

### 确定性/并发合同（matrix 专项 parallel reduction tolerance）

- 候选索引 i=0..count-1 固定序单栈归约（integrate.cpp:30-57）；
  vs/wsum 双累加器（:52-53）+ 单除法（:70）→ 同输入 bitwise 确定
  且**与 worker 数无关**（像素内无并行归约；像素间并行在调用方:
  stage2.cpp:1288/:1298 schedule(static)、acr_kernels.cpp:218/:228
  schedule(static)；per-thread 统计 thread id 定序归并
  stage2.cpp:1305-1313；large_scale 激活强制串行）。容差=1..N
  线程 bitwise（无 epsilon；ALG-P2-INT-001 §11.4 F7）。
- 调用方禁止对 `pr.support` 二次 max/mean（stage2.cpp:1525-1526
  注释冻结；ACR :205-208 同型消费）。

### 负向条款与缺陷迁移语义

- 负向条款: **P2-INT-DOC 不新增、不修改任何公共 C 头/C ABI**——
  既有 V17 清单（:17-29）与本节为同一 ABI 的展开冻结，禁止第二套
  定义；policy/reducer 分离: 本层禁止引入 ivar/SNR 权重策略
  （weights 数组外置，构造在 Stage2 weight_mode）。
- 缺陷迁移语义（登记不改码）: DISP-P2INT-001（sup_max 漏计零权重
  accepted 样本——输出 support 保守方向偏低，偏离 integrate.h:17
  冻结文本；Stage2/ACR 直接消费，整改归 P2-INT-IMPL，回归门=
  ALG §11.4 F5）；DISP-P2INT-002（INTEGRATION.md:58 vs
  integrate.h:17 表述矛盾，文档级，SCI FROZEN 禁改）。
- 下游: TEST-P2-INT-001（MISSING，P2-INT-DOC 登记，由 P2-INT-TEST
  落地 + EVIDENCE；设计冻结面=ALG-P2-INT-001 §11.4）；上游
  SCI-INT-001（共享 FROZEN）/ ALG-P2-INT-001。

## Phase2 rejection 公共消费面（API-P2-REJ-001）

> ID: API-P2-REJ-001  状态: CONTRACT_READY（P2-REJ-DOC 冻结，
> 2026-09-09）
> 定位: Phase2 候选栈排异公共 C ABI 消费面——planning 层 +
> eligibility/gather 层 + kernel + large_scale 后处理的导出符号
> 冻结（既有符号的展开冻结，**不新增、不修改任何 C 头/C ABI**）；
> 编排层经 API-P2-001（PHASE2_API_V1 phase session）驱动，kernel
> 无 session 依赖（无状态纯函数）。
> SRC: lib/phase2/src/rejection.cpp（2076 行，astrocs_phase2 静态库
> 成员，根 CMakeLists.txt:336-346/:340）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/rejection.h（329 行）；DATA:
> DATA-P2-REJ（DATA_SEMANTICS §22，单位/dtype/shape/invalid 唯一
> 权威）；ALG: ALG-P2-REJ-001（docs/algorithms/PHASE2_REJECTION.md，
> 逐符号锚与消费链）；MOD: astrocs.p2.rejection（迁移目标
> astrocs_p2_rejection.dll 为矩阵合同值，尚未存在——MISSING 语义，
> 由 P2-REJ-IMPL 建立，本节不声明 IMPLEMENTED；descriptor 占位
> module_id=astrocs.phase2.reject 为编排层词汇，
> module_adapters.cpp:700-717 p2_reject_descriptor，由 P2-XX-INT
> 对齐）。

### 导出符号与签名要点（rejection.h 实测锚，冻结）

- `int p2_reject_plan_resolve(const P2RejectionPlanRequest*,
  P2RejectionPlan*, char* err, std::size_t err_size)`（:191-193 声明，
  注释 :182-190 WBPP 2.9.1 路由 + profile 语义）: AUTO 一次解析为
  显式方法（nominal n<6 → PERCENTILE、6..15 → WINSORIZED、>15 →
  LINEAR_FIT；nominal_contributors=u32 几何可贡献数 :174-177；
  kernel 永不接收 AUTO）；profile 版本化（wbpp_2_9_1 /
  astrocs_adaptive）；非法 profile → 非零 rc。rc=0 OK；rc=1 null
  请求/plan、request 出界或 profile 非法（err 仅日志文本）。线程
  安全=reentrant yes / threadsafe no（无锁无全局态，并发由调用方
  像素划分）；无取消检查点（ThreadLease 接线归 P2-REJ-IMPL）。
- `int p2_eligibility_filter(const P2EligibilityInput*,
  P2EligibilityOutput*)`（:222）: 连续版资格层（compat 路径消费，
  与生产 strided gather 同一 policy core）。support_threshold 严格
  大于（:206）；quality_flags_required=0 不要求 quality（:207）。
  rc=1 null in/out 或必要输出缓冲缺失；n==0 → rc=0 空输出。线程
  安全=reentrant yes / threadsafe no；无取消检查点（ThreadLease
  接线归 P2-REJ-IMPL）。
- `int p2_collect_candidate_stack(const P2EligibilityGatherInput*,
  P2EligibilityGatherOutput*)`（:263）: 生产 strided gather
  （frame-major f32/f64 → 紧凑 f64 栈，:1164-1179）；输出
  source_indices=权威回映射（PHASE2_IVAR_WIRING 注释 :252-255，
  compact 后禁止猜 original slot；stage2.cpp:1098 权重构造方用它
  回映射 ivar slot）。rc=1 null in/out 或必要输出缓冲缺失；n==0 →
  rc=0 空输出。线程安全=reentrant yes / threadsafe no（无锁无全局
  态，并发由调用方像素划分）；无取消检查点（ThreadLease 接线归
  P2-REJ-IMPL）。
- `int p2_reject_stack_ex(const P2CandidateStack*,
  const P2RejectionPlan*, P2RejectionDecision*)`（:287-289，注释
  :285-286）: 显式 plan 执行（AUTO 返回非法参数）。rc=1 仅
  stack/plan/out null（:1687）或 reasons/values null 且 count>0
  （:1707）；rc=0 时语义全由 status 承载（八态
  P2RejectStatus 0..7，含 INVALID_METHOD/INVALID_INPUT/
  INVALID_CONFIGURATION——"科学状态"而非调用错误；状态机=ALG
  §4.1）。kernel 内 n≤64 固定 scratch（无每像素堆分配）>64 走堆。
  线程安全=reentrant yes / threadsafe no（无锁无全局态，并发由
  调用方像素划分）；无取消检查点（ThreadLease 接线归
  P2-REJ-IMPL）。
- `const char* p2_rejection_semantic_id(int method)`（:196）: 语义
  id 查询（P2_SEMANTIC_* 常量 :59-70；未知方法返回 "unknown"）。
  线程安全=reentrant yes / threadsafe no（只读映射）；无取消检查
  点（ThreadLease 接线归 P2-REJ-IMPL）。
- `int p2_large_scale_apply(std::uint8_t* low, std::uint8_t* high,
  int width, int height, int depth, const P2LargeScaleParams*)`
  （:295-297，注释 :291-294）: frame-major 每帧 width×height 字节
  原地修改（1=rejected）；仅扩张 ≥min_structure_pixels 结构；低/
  高侧独立半径。rc=0 OK（disabled=noop）；rc=1 指针 null ∨
  width/height/depth ≤0 ∨ min_structure_pixels<1 ∨ 负半径。线程
  安全=reentrant yes / threadsafe no（无锁无全局态，并发由调用方
  像素划分）；无取消检查点（ThreadLease 接线归 P2-REJ-IMPL）。
- compat 面（冻结两符号，仅测试/旧调用，:299 冻结注释"生产 Stage2
  不再调用"）: `int p2_reject_stack(const P2SampleStackView*,
  P2RejectionResult*)`（:325，兼容换算 sigma_low/sigma_high/
  max_iterations → typed params，min_samples 兼容门 :1891-1900）；
  P2SampleStackView :301-314 / P2RejectionResult :316-323。rc 语义
  同 kernel 面（科学语义由 status 承载）。线程安全=reentrant yes /
  threadsafe no；无取消检查点（ThreadLease 接线归 P2-REJ-IMPL）。

### 单位/dtype/shape（消费面副本，唯一权威=DATA_SEMANTICS §22）

| 项 | 值 |
|---|---|
| values | f64，ADU（kernel 工作域输入；gather f32/f64 源 → f64 提升，:1164-1179） |
| weights | f64，1/ADU²（可空=等权；数值域，权重策略外置调用方 weight_mode） |
| support | f64，无量纲 [0,1]（仅资格门，不进统计） |
| frame_ids | u64，无量纲稳定帧标识（ESD tie-break/确定性） |
| reasons | u8 0..3（P2RejectReason） |
| status | int 0..7（P2RejectStatus 八态） |
| typed params | 六组结构（rejection.h:99-130）禁跨方法共享字段（:99 注释） |
| 调用粒度 | 单像素栈（kernel）+ strided frame-major gather 批量（生产收集器） |

### 确定性/并发合同（matrix 专项）

- 逐样本独立判定（无跨样本浮点归约）→ per-pixel 决策 **bitwise
  独立于 worker 数**（1..N；ALG-P2-REJ-001 §11.4 F5 置换不变性门
  G6PermutationInvariance :2863 + V15ExPermutationInvarianceTyped
  :4443）。同输入同 plan 同 fid → decision bitwise 确定（ESD
  tie-break=frame_id；linear_fit 排序 (value, orig_index) 字典序；
  median 值级置换不变）。
- 像素间并行在调用方（stage2.cpp:1288/:1298 schedule(static)、
  acr_kernels.cpp:218/:228 schedule(static)）；per-thread 统计
  thread id 定序归并（stage2.cpp:1305-1313）；large_scale 激活强制
  串行（stage2.cpp:1280 条件）。
- 容差=bitwise（无 epsilon 门；ESD tie 1e-15 / RCR isEqual rel
  1e-8 / winsor 收敛 5e-4·σ 为实现内部冻结常数，非门容差）。
- 调用方契约: source_indices 禁猜 slot（ivar/quality/variance/
  metadata 一律经权威映射）；mask 应用回原始 calibrated 值（归一化
  仅判定工作域，h:15-17 冻结注释）；AUTO 只进 plan_resolve 不进
  kernel（h:285 注释）。

### 负向条款与缺陷迁移语义

- 负向条款: **P2-REJ-DOC 不新增、不修改任何公共 C 头/C ABI**——
  rejection.h 既有声明与本节为同一 ABI 的展开冻结，禁止第二套
  定义；policy/reducer 分离: 禁止引入 ivar/SNR 权重策略（weights
  数组外置，构造在 Stage2 weight_mode；RCR 核消费同栈 weights 数组
  属官方加权语义，非策略）。
- 缺陷迁移语义（登记不改码）: DISP-P2REJ-001（rejection.h:118
  percentile low_fraction 注释"默认 0.1" vs 实现 plan_resolve 默认
  0.2 :1060 与 SCI 权威——实现与 SCI 一致，header 注释漂移，整改=
  P2-REJ-IMPL 注释对齐）；DISP-P2REJ-002（SCI §8 "NO_CANDIDATES"
  vs 实现空栈=MIN_SAMPLES :1706，NO_CANDIDATES 属积分域
  P2IntegrateStatus integrate.h:46——SCI FROZEN 禁改，语义权威=
  DATA §22.3）；DISP-P2REJ-003（SCI/REJECTION_ALGORITHMS 行号锚
  漂移，行号权威=ALG §3 实测）；DISP-P2REJ-004（minmax value-only
  比较器 tie-break 未显式冻结，整改候选=P2-REJ-IMPL 显式 index
  tie-break + P2-REJ-TEST 等值门）。
- 下游: TEST-P2-REJ-001（MISSING，P2-REJ-DOC 登记，由 P2-REJ-TEST
  落地 + EVIDENCE；设计冻结面=ALG-P2-REJ-001 §11.4 + registry 页
  §独立 synthetic 验证节）；上游 SCI-REJ-001（共享 FROZEN）/
  ALG-P2-REJ-001 / DATA-P2-REJ。

## Phase2 sampling 公共消费面（API-P2-SMP-001）

> ID: API-P2-SMP-001  状态: CONTRACT_READY（P2-SAMP-DOC 冻结，
> 2026-09-09）
> 定位: Phase2 background-clean 控制点采样公共 C ABI 消费面——
> 配置默认/统计量/帧身份/采样两入口的导出符号冻结（既有符号的
> 展开冻结，**不新增、不修改任何 C 头/C ABI**）；编排层经
> API-P2-001（PHASE2_API_V1 phase session）驱动，采样函数无
> session 依赖（数据面经 P2CoverageResult 显式传入）。
> SRC: lib/phase2/src/sampler.cpp（1156 行，astrocs_phase2 静态库
> 成员，根 CMakeLists.txt:337-346/:342）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/sampler.h（136 行）；DATA:
> DATA-P2-SMP（DATA_SEMANTICS §23，单位/dtype/shape/invalid 唯一
> 权威）；ALG: ALG-P2-SMP-001（docs/algorithms/PHASE2_SAMPLER.md，
> 逐符号锚与消费链）；MOD: astrocs.p2.sampling（迁移目标
> astrocs_p2_sampling.dll 为矩阵合同值，尚未存在——MISSING 语义，
> 由 P2-SAMP-IMPL 建立，本节不声明 IMPLEMENTED；descriptor 占位
> module_id=astrocs.phase2.sample 为编排层词汇，
> module_adapters.cpp:642-654 p2_sample_descriptor，由 P2-XX-INT
> 对齐）。

### 导出符号与签名要点（sampler.h 实测锚，冻结）

- `P2SamplerConfig p2_sampler_default_config(void)`（:60 声明，
  实现 sampler.cpp:294-312）: 配置默认单一来源（15 字段，null cfg
  时使用；显式 cfg 覆盖）。字段表=DATA §23.1(3)。线程
  安全=reentrant yes / threadsafe yes（纯值返回）；无取消检查点
  （ThreadLease 接线归 P2-SAMP-IMPL）。
- `std::uint64_t p2_frame_id(const char* hips_path)`（:93，注释
  :85-92 冻结）: 内容稳定帧标识——truncated-64 canonical SHA-256
  （9 properties + signal tile 像素 + support tile 像素 "S" 前缀 +
  SNR catalogue 内容，:314-438）；路径/重命名/换根不变，任何科学
  payload 变化 → id 变化；取 SHA-256 前 16 hex 大端截断；与输入
  顺序无关；UPM 参考帧=每分量最小 frame_id。**禁止描述为 FNV-1a/
  路径派生**（h:92）。失败哨兵=0（调用方 cached 入口 :512-523
  显式拒绝 0）。线程安全=reentrant yes / threadsafe no（AIO 全局
  缓存面）；无取消检查点（ThreadLease 接线归 P2-SAMP-IMPL）。
- `double p2_stats_median(const double*, std::uint64_t)`（:97）与
  `double p2_stats_mad(const double*, std::uint64_t, double*
  out_median = nullptr)`（:98-99）: 统一统计量（sampler patch
  estimator / MAD / SNR 邻域与 UPM 域共用同一实现）；median 偶数
  n 取上下中位平均、NaN 自动过滤（全 NaN → 0，:440-447）；MAD=
  1.4826×median(|x−med|)（:449-461）。线程安全=reentrant yes /
  threadsafe yes（无共享可变态）；无取消检查点（ThreadLease 接线
  归 P2-SAMP-IMPL）。
- `int p2_sample_controls(const P2CoverageResult*, const char*
  const* hips_paths, const P2SamplerConfig*, P2ControlObservation*
  out_obs, std::uint64_t out_capacity, std::uint64_t* out_n_obs,
  std::uint64_t* out_n_controls, P2SampleStats* out_stats,
  P2ControlNode* out_controls, std::uint64_t ctrl_capacity, char*
  err, std::size_t err_size)`（:103-114，probe/fill 冻结注
  :101-102）: 基础入口（内部逐帧 p2_frame_id）。
- `int p2_sample_controls_cached(..., const std::uint64_t*
  frame_ids, ...)`（:120-132，h:116-119 冻结注）: 生产入口
  （stage2 已算 frame_id 时透传，避免二次 500MB payload 哈希，
  stage2.cpp:279-311 probe/fill）；frame_ids 可空=内部重算，0=
  非法哨兵 → rc=1（:512-523）；out_n_controls=n_union×G² 全几何
  含空覆盖占位（:118-119，与 stats.accepted/overlap_controls
  区分、日志并列表述）。两入口 rc=0 成功（含空 obs——空覆盖
  union 合法）/ rc=1 错误 + err 8KB 文本（细分语义=DATA §23.5/
  ALG §11.1；bad args :476-479、open failed :536-546、n_union
  >1e6 :634-638、cells>2e8 :644-648/:1071-1077、首 tile 越界
  :656-669、exception 兜底 :1089-1097）；容量不足**不报错**：
  按 capacity 截断拷贝、out_n_* 返回真实需求量（:1098-1117）。
  线程安全=reentrant yes / threadsafe no（g_aio_mu :161/:166 串行
  路径锁 + per-worker 独立 AIO 句柄；无共享可变全局态）；无取消
  检查点（ThreadLease 接线归 P2-SAMP-IMPL，与 DISP-COV-005
  同构）。

### 单位/dtype/shape（消费面副本，唯一权威=DATA_SEMANTICS §23）

| 项 | 值 |
|---|---|
| P2ControlObservation | 13 字段（upm.h:31-57）：frame_id/control_id/leaf_ipix u64、ra_deg/dec_deg/value/uncertainty/snr/ivar/control_variance/control_ivar/support f64、snr_available int、quality_flags u32 |
| value/uncertainty | f64 ADU（value 可负 patch median） |
| control_variance/control_ivar | f64 ADU² / 1/ADU²（k_corr×(π/2)×σ_bg²/N_retained 冻结公式；ivar 弃用仅诊断） |
| snr_available | int 0/1（0=回退整帧中位，禁以 1.0 伪装 unknown，upm.h:51-55） |
| P2SampleStats | 10 字段 u64 诊断计数（sampler.h:63-74） |
| P2ControlNode | 7 字段（sampler.h:77-83）；out_n_controls=n_union×G² 含空覆盖占位 |
| cfg | P2SamplerConfig 15 字段（sampler.h:32-57；默认单一来源 :60/:294-312） |
| 调用粒度 | 全帧批量（一次调用产出全 union 控制网格观测，非逐像素） |

### 确定性/并发合同（matrix 专项）

- 输出 obs 序列 **bitwise 与 worker 数无关**（1/N 等价）：固定槽位
  写回 cells[idx]（:870-872）+ 第三遍单线程顺序扫描 +
  frame_snr_med 排序 median；验证门=F8
  sampler_parallel_consistency_test.cpp:29
  TEST(Phase2SamplerParallel, OneTvsTwoTDeterminism)。
- worker 数唯一来源=Runtime lease（cfg.cpu_workers，stage2.cpp:
  273-274 由 ExecutionOptions 透传；模块无 hardware_concurrency
  自行开线程 :880-883）；>1 走 std::thread 池（:886-913，per-worker
  独立 AIO 句柄 :894），=1 串行 reference（:916-933）；OpenMP 已
  从实现移除（:882-883 注释），lib/phase2/CMakeLists.txt:28
  P2_ENABLE_OPENMP option 保留仅影响旧 target 编译面。
- 容差=bitwise（obs 输出无 epsilon 门；clipping 收敛 1e-12 相对阈
  为实现内部冻结常数；坐标面 F4 atol 1e-9 deg、cvar 面 F2/F3/F5
  rtol 1e-12 为 TEST-P2-SMP-DESIGN-001 测试容差，非 ABI 容差）。
- 调用方契约: frame_ids 0=非法（禁静默替换）；out_n_controls
  几何计数与 accepted/overlap 统计计数并列表述禁混用；ivar 弃用
  仅诊断、科学权重一律 control_ivar（upm.h:38-41/§23.4）。

### 负向条款与缺陷迁移语义

- 负向条款: **P2-SAMP-DOC 不新增、不修改任何公共 C 头/C ABI**——
  sampler.h 既有声明与本节为同一 ABI 的展开冻结，禁止第二套定义；
  不引入第二配置面（veto 阈值/半径现状硬编码 = DISP-P2SMP-004，
  schema 扩展归 P2-SAMP-IMPL，本节不预设字段）；不改变 frame_id
  身份算法（DATA-FRAME-ID-001 冻结，任何 payload 键变化即违约）。
- 缺陷迁移语义（登记不改码）: DISP-P2SMP-001（cfg `<=0→默认` 吞
  显式 0 :485-502，整改=P2-SAMP-IMPL 语义区分 0 与未设置）；
  DISP-P2SMP-002（insufficient_retained 双计数 :1006+:1022，统计
  面偏差 obs 不受影响，整改=P2-SAMP-IMPL 去重 + P2-SAMP-TEST F9
  现状口径守恒门）；DISP-P2SMP-003（17 处 stderr 诊断直写，整改=
  P2-SAMP-IMPL 结构化日志通道）；DISP-P2SMP-004（veto 阈值/半径
  硬编码 :849-850，整改候选=P2-SAMP-IMPL 配置面扩展）；
  DISP-P2SMP-005（m0≈0 收敛阈值退化全迭代 :818，确定性无影响，
  整改候选=P2-SAMP-IMPL 性能观察级）。
- 下游: TEST-P2-SMP-001（MISSING，P2-SAMP-DOC 登记，由 P2-SAMP-TEST
  落地 + EVIDENCE；设计冻结面=TEST-P2-SMP-DESIGN-001 ALG §11.3
  F1-F9 + registry 页 §独立 synthetic 验证节）；上游 SCI-UPM-001
  （共享 FROZEN）/ ALG-P2-SMP-001 / DATA-P2-SMP。

## Phase2 装配会话 C API（API-P2-SESSION-001）

> ID: API-P2-SESSION-001  状态: CONTRACT_READY（P2-SESSION-DOC 冻结，
> 2026-09-10，SA-P2-X24）
> 定位: Phase2 进程内装配会话公共 C ABI——coverage → sample →
> upm_build → persist 四段编排的会话生命周期五导出符号冻结（既有
> 符号的展开冻结，**不新增、不修改任何 C 头/C ABI**）；会话本身无
> 科学实现（纯编排 facade，直调 lib/phase2 生产符号），编排上游=
> API-P2-001（PHASE2_API_V1 phase session，docs/api/PHASE2_API_V1.md
> FROZEN，引用不改动）。
> SRC: lib/phase2_session/p2_session.cpp（282 行，静态库
> astrocs_phase2_session 成员，根 CMakeLists.txt:454-458）+ 唯一
> 权威签名头 lib/phase2_session/p2_session.h（39 行）；DATA:
> DATA-P2-SESSION（DATA_SEMANTICS §24，config/manifest/错误码唯一
> 权威）；ALG: ALG-P2-SESSION-001（docs/algorithms/PHASE2_SESSION.md，
> 四段调用序逐源码行号锚）；MOD: astrocs.p2.session（迁移目标
> astrocs_p2_session.dll 为矩阵合同值，尚未存在——MISSING 如实
> 登记，由 P2-SESSION-IMPL 建立，本节不声明 IMPLEMENTED；registry
> descriptor 现无 astrocs.p2.session 占位词汇，编排委托=
> module_adapters.cpp:23-26 五 C ABI 声明（RT-005），词汇对齐归
> P2-XX-INT，不作冻结依据）。

### 导出符号与签名要点（p2_session.h 实测锚，5 C API + 1 C++ 诊断全部当前真实存在，冻结）

- `acs_status p2_session_create(const astrocs_host_services_v1* host,
  acs_handle* out)`（p2_session.h:17，实现 p2_session.cpp:56-67）:
  会话唯一构造。host 不可空且 struct_size/abi_version 校验
  （:57-59 → ACS_ERR_ABI_MISMATCH）；out null → ACS_ERR_PARAM（:60）；
  SessionState 分配失败 → ACS_ERR_NOMEM（:62）；manifest 初始化
  kind="astrocs_phase2_session" + stages=[]（:64）。host 指针由会话
  持有（借用，不拥有——宿主须保证句柄存活期 host 存活）。
- `acs_status p2_session_validate(acs_handle h, const acs_span_u8
  config_json)`（p2_session.h:20，:69-98）: 纯读无 IO，幂等可重复
  （p2_session.h:19 注释）；句柄/config null 或空 → ACS_ERR_PARAM
  （:71）；parse 失败/非 object/缺必需键/类型错 → ACS_ERR_PARAM
  （:76-96）；键集与校验规则唯一权威=DATA §24.1（未知键**现状不
  拒绝**，p2_session.h:19 注释与实现漂移，§24.1 如实登记）。
- `acs_status p2_session_run(acs_handle h, const acs_span_u8
  config_json)`（p2_session.h:23，:100-248）: 四段编排执行——
  coverage（:119-148，两遍 build 协议）→ sample（:150-178，probe/
  fill）→ upm_build（:180-219，冻结默认+upm 子键覆盖）→ persist
  （:221-240，可选）。取消点=**阶段边界**（p2_session.h:22 注释
  冻结；4 检查点 :120/:152/:181/:223-227，upm 整模型不写半成品）；
  内部并行仅 UPM blocks（cpu_workers=budget.max_workers :155/:195，
  预算驱动禁硬编码）。config 消费口径=DATA §24.1/§24.4；run 重入
  不清空 manifest（stages 累积，幂等未冻结，§24.2 登记）。
- `acs_status p2_session_inspect(acs_handle h, acs_span_u8*
  out_manifest_json)`（p2_session.h:25，:250-266）: 只读导出
  manifest JSON（dump(2)，:258）；out null → ACS_ERR_PARAM（:252）；
  status 派生（created/complete/failed :253-256）；输出缓冲经
  host->allocator.alloc 分配（:260），**释放责任=宿主经同一
  allocator**（common_abi_v1.h:74 合同头）；分配失败 →
  ACS_ERR_NOMEM（:261）。字段表唯一权威=DATA §24.2。
- `acs_status p2_session_destroy(acs_handle h)`（p2_session.h:27，
  :268-273）: 唯一释放路径；句柄 null → ACS_ERR_PARAM（:270）；destroy
  后句柄不可再用（run 失败路径内部已 p2_upm_close model，:231/:241，
  会话不持 model 句柄）。
- C++ 诊断面（非 C ABI，p2_session.h:32-36）:
  `std::string astrocs::phase2::last_error(acs_handle h)`（:278-281）
  ——最近一次错误脱敏摘要（"what rc=N"，:45）；诊断用，非科学接口，
  不进冻结 ABI 面。

### 参数表（acs_span_u8/config/manifest 口径，唯一权威=DATA §24）

| 参数 | 口径 |
|---|---|
| host | const astrocs_host_services_v1*（common_abi_v1.h:110-117，四通道 allocator/logger/cancel/budget；struct_size+abi_version handshake） |
| config_json | acs_span_u8（UTF-8 JSON 单对象；count 不含 NUL；键集=DATA §24.1） |
| out_manifest_json | acs_span_u8*（出参；UTF-8 JSON dump(2)；宿主 allocator 释放；字段=DATA §24.2） |
| acs_handle | 不透明句柄（SessionState*，实例态全隔离） |

### 返回码（唯一权威=DATA §24.3 表）

ACS_OK / ACS_ERR_ABI_MISMATCH（create host handshake）/
ACS_ERR_PARAM（句柄、span null/空、parse、非 object、缺必需键、
类型错）/ ACS_ERR_STATE（生产 rc=2，build fail 显式缺 ivar 等）/
ACS_ERR_IO（persist p2_upm_save 失败，error_kind=output）/
ACS_ERR_INTERNAL（生产其余 rc 兜底）/ ACS_ERR_NOMEM（SessionState、
manifest 分配）/ ACS_ERR_CANCELLED（阶段边界检查命中）。
映射实现=map_rc（p2_session.cpp:43-50，rc=0/1/2/其余四档）。

### 调用时序与句柄生命周期

create（唯一构造）→ [validate*（幂等，推荐先于 run）] → run（成功
或失败均可重入，manifest 累积）→ inspect*（任意时刻只读）→
destroy（唯一释放）。句柄不可复制/二次 destroy；宿主保证 host 存活
覆盖句柄生命周期。

### 线程安全与取消合同（matrix 专项）

- **reentrant: yes / threadsafe: no（handle 级）**（p2_session.h:16
  注释锚）——同一 handle 并发调用禁止；不同 handle 并发合法
  （无共享可变全局态）。
- 取消点=**阶段边界**（p2_session.h:22 注释锚；coverage/sample/
  upm_build/persist 段前 4 检查点，p2_session.cpp:120/:152/:181/
  :223-227）；upm 整模型不写半成品（段内无检查点，persist 取消先
  close）；取消返回 ACS_ERR_CANCELLED + 段 status=cancelled。
- 内部并行**仅 UPM blocks**：sample/upm 段 cpu_workers=
  host->budget.max_workers（:155/:195，Runtime lease 唯一来源），
  coverage/persist 串行；预算绑定注释漂移（p2_session.h:4
  "sampler=1" vs 实现 N-worker）登记于 DATA §24.5。

### 负向条款与缺陷迁移语义

- 负向条款: **P2-SESSION-DOC 不新增、不修改任何公共 C 头/C ABI**——
  p2_session.h 既有五函数声明与本节为同一 ABI 的展开冻结，禁止第二
  套定义；registry descriptor astrocs.phase2.session 占位词汇由
  P2-XX-INT 对齐，**不作冻结依据**；编排上游 API-P2-001
  （docs/api/PHASE2_API_V1.md，FROZEN）引用不改动；会话冻结默认
  （upm 参数/tolerance/sigma_floor 等）非 config 覆盖键的部分禁改，
  扩展归 P2-SESSION-IMPL schema 修订。
- 缺陷迁移语义（登记不改码）: 未知键拒绝缺失（p2_session.h:19 vs
  :69-98）、run 子键类型错未捕获路径、h:4 预算绑定注释漂移——编号
  见 ALG-P2-SESSION-001 DISP 清单。
- 下游: TEST-P2-SESSION-001（MISSING，P2-SESSION-DOC 登记，DORMANT，
  由 P2-SESSION-TEST 落地 + EVIDENCE；设计冻结面=ALG-P2-SESSION-001
  TEST-DESIGN 节）；上游 SCI-UPM-001 / SCI-INT-001 / SCI-REJ-001
  （共享 FROZEN）/ ALG-P2-SESSION-001 / DATA-P2-SESSION。

## Phase2 UPM 公共消费面（API-P2-UPM-001）

> ID: API-P2-UPM-001  状态: CONTRACT_READY（P2-UPM-DOC 冻结，
> 2026-09-10）
> 定位: Phase2 UPM fit/persist/apply/reload 公共 C ABI 消费面——
> 既有 16 导出符号的展开冻结（**不新增、不修改任何 C 头/C ABI**；
> upm.h 为唯一权威签名头，184 行）。
> SRC: lib/phase2/src/upm.cpp（1565 行，astrocs_phase2 静态库成员，
> 根 CMakeLists.txt:337-346/:338）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/upm.h（184 行）；DATA:
> DATA-P2-UPM（DATA_SEMANTICS §25，fit/persist 域单位/dtype/invalid
> 唯一权威）/ DATA-P2-COR（DATA_SEMANTICS §26，apply 域唯一权威）；
> ALG: ALG-P2-UPM-IMPL-001（docs/algorithms/PHASE2_UPM_IMPL.md，
> 逐符号锚与消费链）；MOD: astrocs.p2.upm（迁移目标
> astrocs_p2_upm.dll 为矩阵合同值，尚未存在——MISSING 语义，由
> P2-UPM-IMPL 建立，本节不声明 IMPLEMENTED；descriptor 占位
> module_id=astrocs.phase2.upm-fit/upm-apply，
> module_adapters.cpp:661-696（p2_upm_fit_descriptor :599 /
> p2_upm_apply_descriptor :618），由 P2-XX-INT 对齐）。

### 导出符号与签名要点（upm.h 实测锚，冻结）

- `int p2_upm_build(const P2ControlObservation* obs, std::uint64_t
  n_obs, const P2UpmBuildConfig* cfg, void** out_model)`（h:95-97
  声明，实现 upm.cpp:929）: 联合拟合入口（obs 域=DATA-P2-SMP，13
  字段 §23.2(1)）；cfg 可空=运行时默认填充（:222-236）+非法值修补
  （:237-245）；target_order=-1（auto）→ rc=1（:246-249，空间 UPM
  必须知 leaf 层级 order=target+9）。rc: 0=ok（out_model 出参，空
  obs 合法→NO_DATA 语义，SCI-UPM-001 §4）/ 1=参数错误（句柄/cfg
  校验）/ 2=production 缺 control_ivar（经 p2_upm_raw_weight :1311
  传播，h:126-133）。线程安全=生命周期内单会话使用。
- `int p2_upm_build_geo(const P2ControlObservation* obs, std::uint64_t
  n_obs, const P2ControlNode* nodes, std::uint64_t n_nodes, const
  P2UpmBuildConfig* cfg, void** out_model)`（h:103-106，实现
  upm.cpp:934）: 全几何节点入口（nodes 覆盖 coverage union 全部
  control cell 含单帧区，obs 只含 ≥2 clean 帧观测；单帧区由全局
  平滑/Laplacian 延拓得 C——harmonic continuation，h:99-101 冻结）；
  stage2 生产调用 :432-434。rc 语义同 p2_upm_build。线程安全=
  生命周期内单会话使用。
- `int p2_upm_save(const void* model, const char* path)`（h:108，
  实现 upm.cpp:940）: 模型落盘（astrocs-upm-v2 单文件 JSON，
  唯一 AIO aio_upm_write_sparse :1003-1005，aio_upm.cpp:66 原子写
  ENG-IO-001）。rc: 0=ok / 1=model 或 path null（:941）、frame 绑定
  行数不一致拒写（:943-945，ALG-UPM-FRAME-BIND-001）。线程安全=
  生命周期内单会话使用。
- `int p2_upm_open(const char* path, void** out_model)`（h:109，
  实现 upm.cpp:1008）: reload 入口；format 校验失败（非
  astrocs-upm-v2，:1027-1028）/ parse 失败 → rc=1；成功保持
  frame_id→θ 映射（SCI-UPM-PERSIST-001）。线程安全=生命周期内
  单会话使用。
- `int p2_upm_info(const void* model, P2ModelInfo* out_info)`
  （h:110，实现 upm.cpp:1233）: 溯源面只读导出（七字段 h:60-68=
  DATA §25.2(1) 表）。rc: 0=ok / 1=model 或 out_info null。线程
  安全=纯只读 reentrant yes。
- `int p2_upm_calibrate_block(const void* model, std::uint64_t
  frame_id, const std::uint64_t* leaf_ipix, const double*
  input_signal, double* output_signal, std::uint64_t count)`（h:113-119
  冻结注 :112，实现 upm.cpp:1240）: apply 核心——
  output_signal[i]=input_signal[i]−C(frame_id, leaf_ipix[i])（:1266，
  8×8 cell 内双线性 :1263；DATA §26.2）。rc: 0=ok / 1=句柄或数组
  null（:1245-1248）或**未知 frame_id 显式失败**（:1250-1252 注释
  "禁止回退 frame 0 参数"，禁回退红线）。线程安全=只读模型+独立
  输出缓冲，reentrant yes。
- `double p2_upm_evaluate_c(const void* model, std::uint64_t
  frame_id, std::uint64_t leaf_ipix)`（h:122-123，实现
  upm.cpp:1271）: 单点校正 C_f(leaf)（sparse/dense 同一科学语义，
  h:121）。失败语义（非 rc 型）: model null→0.0（:1273）；未知
  frame_id→quiet NaN（:1276-1279，显式不可用，禁 0.0 伪装）。
  线程安全=纯只读 reentrant yes。
- `int p2_upm_raw_weight(const P2ControlObservation* obs, const
  P2UpmBuildConfig* cfg, double* out_raw)`（h:134-136，实现
  upm.cpp:1292）: 观测 raw weight 单一实现（production
  use_ivar_weight!=0: raw_w=quality_factor×control_ivar，:1306-1312，
  无 star-SNR/support^p/单像素 ivar 因子；legacy 仅 ablation/
  诊断 SNR-015: support^support_power×snr²/(1+snr²)/max(unc²,
  sigma_floor²)，:1315-1323）。rc: 0=ok / 1=obs 或 out_raw null
  （:1294）/ 2=production control_ivar≤0/非有限（:1311，h:126-133
  显式 INVALID 禁静默回退）。线程安全=纯只读 reentrant yes。
- `int p2_upm_normalized_weights(const P2ControlObservation* obs,
  std::uint64_t n_obs, const P2UpmBuildConfig* cfg, double*
  out_norm)`（h:139-142，实现 upm.cpp:1325）: per-control 归一化
  raw/Σraw×control_reliability（h:138；:1330-1342，Σ=0 归 0 而非
  rc 失败）。rc: 0=ok / 1=obs 或 out null、n_obs=0（:1329）或任一
  raw_weight 失败（:1333，含 rc=2 传播）。线程安全=纯只读 reentrant yes。
- `int p2_upm_geometry_hash(const void* model, char* out, int
  buf_size)`（h:146，实现 upm.cpp:1346）: geometry/topology hash
  （SHA-256 hex；payload 仅 geometry/coverage 拓扑 order/grid/cell/
  controls/邻接，:1350-1362；权重/quality/support 变化不得改变——
  h:144-145 冻结）。rc: 0=ok / 1=model 或 out null、buf_size≤0
  （:1347）。线程安全=纯只读 reentrant yes。
- `int p2_upm_component_gauges(const void* model, std::uint64_t*
  out_component_count, std::uint64_t* out_ref_frame_ids)`（h:150-152，
  实现 upm.cpp:1369）: 每连通分量 gauge frame id（分量内最小
  frame_id，构建与重开后一致，h:148-149；out 可 NULL 只取数量）。
  rc: 0=ok / 1=model null（:1372）/ 2=分量缓冲不足（:1376）。线程
  安全=纯只读 reentrant yes。
- `int p2_upm_materialize_dense_n(const void* model, int
  target_order, const char* cache_path, int workers)`（h:177-178，
  实现 upm.cpp:1390）: dense cache 物化（CON-010；fp64 缓存 :1402；
  target_order<0 取模型 info.target_order 兜底 :1394；workers≤0→1
  :1478，h:176 注释漂移如实登记不改码）。rc: 0=ok / 1=model 或
  cache_path null、IO 失败。线程安全=生命周期内单会话使用。
- `int p2_upm_materialize_dense(const void* model, int target_order,
  const char* cache_path)`（h:155-156 与 :174-175 重复声明=
  DISP-P2UPM-001 登记不改码，实现 upm.cpp:1518-1521）: workers=0
  委托 p2_upm_materialize_dense_n。rc 语义同 dense_n。线程安全=
  生命周期内单会话使用。
- `int p2_upm_dense_info(const void* model, const char* cache_path,
  int* out_target_order, std::uint64_t* out_pixels, char*
  out_source_hash, std::size_t hash_buf_size)`（h:159-162，实现
  upm.cpp:1523）: dense cache 信息读取（稀疏=稠密 Gate 用）。rc:
  0=ok / 1=model/path null、cache 打开或 parse 失败。线程安全=
  生命周期内单会话使用（cache 文件只读，模型只读）。
- `int p2_upm_dense_read_block(const void* model, const char*
  cache_path, std::uint64_t frame_id, const std::uint64_t*
  leaf_ipix, const double* input_signal, double* output_signal,
  std::uint64_t count)`（h:166-172 冻结注 :164-165，实现
  upm.cpp:1542）: dense 块校准（与 sparse calibrate_block 数值
  等价，h:164；委托 aio_upm_read_dense_block :1554-1558）。rc:
  0=ok / 1=句柄或数组 null（:1547-1550）、未知 frame_id（:1553）/
  2=stale-cache（source hash 不匹配，aio_upm.cpp:469，禁陈旧缓存
  静默出数）。线程安全=纯只读 reentrant yes（cache 文件只读）。
- `int p2_upm_close(void* model)`（h:180，实现 upm.cpp:1559）:
  模型句柄唯一释放点（DATA §25.2(1)）；重复 close/悬垂句柄=调用方
  生命周期违约。线程安全=释放语义（句柄所有权归调用方）。

并行/取消消费面注记: workers 唯一来源=cfg.cpu_workers（Runtime
lease，p2_session.cpp:195 budget.max_workers；禁硬编码，CON-005；
§24.5 同构）；std::thread 池三段（:513-534/:613-633/:1479-1497，
kChunk=16 :1407），全文件无 #pragma omp；确定性=D1（:513-514，
worker 数无关、同 worker 数下位精确；dense 物化 bit-identical
:1383-1386）。无取消检查点——取消=会话层阶段边界（"取消=整模型
不写半成品"，p2_session.cpp:180）；ThreadLease 接线归 P2-UPM-IMPL。

### 负向条款

- 负向条款: **P2-UPM-DOC 不新增、不修改任何公共 C 头/C ABI**——
  upm.h 既有 16 符号声明与本节为同一 ABI 的展开冻结，禁止第二套
  定义；registry descriptor 占位词汇（module_id=
  astrocs.phase2.upm-fit/upm-apply，module_adapters.cpp:661-696，
  ports samples/upm_model/calibrated_frames/corrected）由 P2-XX-INT
  对齐 astrocs.p2.upm-fit/upm-apply，**不作冻结依据**
  （DISP-P2UPM-004 占位语义）；上游 SCI-UPM-001（FROZEN）引用
  不改动。
- 缺陷迁移语义（登记不改码）: upm.h:154-156 与 :173-175
  materialize_dense 重复声明=DISP-P2UPM-001；upm.h:89-91 注释
  OpenMP 措辞 vs std::thread 实现漂移=DISP-P2UPM-002；会话 upm
  覆盖键仅 max_iterations/huber_delta/smoothing_lambda 三个
  （p2_session.cpp:196-202）=DISP-P2UPM-003 归 P2-SESSION-IMPL；
  材料化 workers 注释漂移（h:176 vs :1478）如实登记。
- 测试语义: TEST-P2-UPM-001（fit/persist 域）/TEST-P2-UPM-002
  （apply 域）MISSING——设计冻结面=ALG-P2-UPM-IMPL-001
  TEST-DESIGN 节，执行测试归 P2-UPM-TEST 落地 + EVIDENCE；INDEX
  登记 status: DORMANT（照 TEST-P2-INT-001 先例）。
- 下游: TEST-P2-UPM-001/002（MISSING，P2-UPM-DOC 登记）；上游
  SCI-UPM-001（FROZEN）/ ALG-P2-UPM-IMPL-001 / DATA-P2-UPM（§25）/
  DATA-P2-COR（§26）。

## Phase3 FITS 写出公共消费面（API-P3-FITS-001）

> ID: API-P3-FITS-001  状态: CONTRACT_READY（P3-FITS-DOC 冻结，
> 2026-09-08）
> 定位: Phase3 FITS 写出域公共消费面——既有内核符号的展开冻结
> （**不新增、不修改任何 C 头/C ABI**；p3_output.h 为唯一权威签名
> 头，64 行，C++ namespace astrocs::phase3；编排面 p3_session.h
> 五段式=API-P3-001 FROZEN 不变，本节仅镜像声明）。
> SRC: lib/phase3_session/p3_output.cpp（370 行，astrocs_phase3_session
> 静态库成员，根 CMakeLists.txt:460-465）+ 唯一权威签名头
> lib/phase3_session/p3_output.h（64 行）+ WCS 关键字源 p3_wcs.h
> （50 行）；DATA: DATA-P3-FITS（DATA_SEMANTICS §27，单位/dtype/
> invalid 唯一权威）；ALG: ALG-P3-FITS-IMPL-001
> （docs/algorithms/PHASE3_FITS_IMPL.md，逐符号锚与消费链）；
> MOD: astrocs.p3.fits_writer（迁移目标 astrocs_p3_fits_writer.dll
> 为矩阵合同值，尚未存在——MISSING 语义，由 P3-FITS-IMPL 建立，
> 本节不声明 IMPLEMENTED；descriptor 占位 module_id=
> astrocs.phase3.writer，module_adapters.cpp:445-460
> p3_writer_descriptor，由 P3-FITS-INT 对齐）。

### 内核符号与签名要点（p3_output.h 实测锚，冻结）

- `P3OutputStatus p3_output_write_atomic(const float* signal, const
  float* coverage, int width, int height, const P3WcsDescriptor* wcs,
  const char* bunit, const char* output_path, const P3Provenance* prov,
  int bitpix, int cancelled_at_row, P3OutputResult* result)`
  （h:45-53 声明，实现 p3_output.cpp:117-321）: 原子写入口——
  signal 主 HDU + COVERAGE 扩展 HDU 合成单文件；tmp → fits_flush_file
  → close → fsync(fd) → rename（R10-C 冻结序，:221-273）；取消
  （cancelled_at_row≥0）/任一步失败 → unlink 不发布（h:41-44）。
  数据域=DATA-P3-FITS §27.1；bitpix∈{-32,-64}（:140-145）；
  cfitsio 进程锁全程（:125，RT-008）。rc: 0=OK（result 出参含
  sha256/coverage_ok/reopen_ok/covered_px/total_px）/ 1=PARAM
  （空指针、W/H<1、bitpix 非法）/ 2=IO（cfitsio/fsync/rename/sha256
  失败，无假文件无假哈希）/ 3=CANCELLED（不落盘）。线程安全=
  进程级互斥内单写（与读路径 aio_fits.cpp:529 同锁），写面无并行、
  输出与 worker 数无关（ALG-P3-FITS-IMPL-001 §7/§8）。
- `P3OutputStatus p3_output_verify(const char* output_path, const
  P3WcsDescriptor* wcs, const float* signal, const float* coverage,
  int width, int height, P3OutputResult* result)`
  （h:57-60 声明，实现 p3_output.cpp:296-368）: 独立重开验证
  （READONLY fits_open_file :312）——逐 HDU 尺寸/像素回环
  （NaN==NaN 一致 :327-330）+ coverage 二值门（>0.5f :346）+
  sha256 重算（失败→IO 不带假哈希 :356-366）。wcs 参数现状忽略
  （:305 (void)wcs，WCS 一致性由写路径单点保证，:301-302 注释）。
  rc: 0=OK / 1=PARAM（path/result null、W/H<1）/ 2=IO。线程安全=
  只读 reentrant（cfitsio 锁内）；与写面互斥同锁。
- `struct P3Provenance`（h:15-24）/ `struct P3OutputResult`
  （h:26-32）/ `enum P3OutputStatus`（h:34-39）: 消费面数据结构
  冻结（字段级锚=DATA-P3-FITS §27.1/§27.2；枚举值 0/1/2/3 冻结）。
- WCS 关键字源符号（p3_wcs.h，ALG-P3-002 承接）:
  `p3_wcs_make`（h:31-34，parity east_left 默认 CD1_1<0 :29）、
  `p3_wcs_pix2world`（h:38-39，0-based 入参 FITS=+1 :36）、
  `p3_wcs_world2pix`（h:42-43）、`p3_wcs_fits_keywords`（h:46）、
  `P3WcsDescriptor`（h:11-20）/`P3WcsStatus`（h:22-27，含
  P3_WCS_HEMISPHERE 半球守卫 :26）。这些符号属本域公共头（同一
  静态库 astrocs_phase3_session），随本节一并冻结；重采样/采样
  域符号（p3_resample/p3_sampler）不在本消费面。
- 会话编排面镜像（API-P3-001 FROZEN 不变）: p3_session 五段
  （p3_session.h:16-28 create/validate/run/inspect/destroy）+
  `last_error`（h:33-37 脱敏诊断，非科学接口）。run 内部经
  p3_output_write_atomic（p3_session.cpp:287-292，cancelled_at_row
  恒 -1）落盘，inspect 摘要含 output_fits_path/sha256/
  order_sel_used/sampler_used/coverage_stats/provenance
  （:296-313）——编排消费经 API-P3-001，内核直调面仅限本节符号。

### 交叉引用与登记语义

- 上游: SCI-P3-001（FROZEN，§96 关键字面）引用不改动；ALG-P3-002/
  ALG-P3-004（PHASE3_RESAMPLE.md 施工规格，公式零改动）；
  ALG-P3-FITS-IMPL-001（实现级合同）；DATA-P3-FITS（§27）。
- 缺陷迁移语义（登记不改码）: DISP-P3FITS-001（AIO README cfitsio
  依赖表述矛盾）/ DISP-P3FITS-002（tmp 命名 h:41-44 协议注 vs
  p3_output.cpp:81 实现，测试残留检查弱匹配）归 P3-FITS-IMPL；
  manifest_hash 恒 nullptr（p3_session.cpp:270）接线归
  P3-FITS-IMPL。
- 测试语义: TEST-P3-WR-001 MISSING——登记面=TEST-P3-WR-DESIGN-001
  设计冻结 VERIFIED（ALG-P3-FITS-IMPL-001 §12 T1-T7 + registry
  手写页 docs/modules/registry/astrocs.phase3.writer.md §独立
  synthetic 验证节，双重陈述）；现状执行测试 tests/unit/
  p3_output_test.cpp（116 行 4 段）=相邻证据引用不冒认；可执行
  归 P3-FITS-TEST 落地 + EVIDENCE；INDEX 登记 status: DORMANT
  （照 TEST-P2-INT-001 先例）。
- 下游: TEST-P3-WR-001（MISSING，P3-FITS-DOC 登记）；上游
  SCI-P3-001（FROZEN）/ ALG-P3-FITS-IMPL-001 / DATA-P3-FITS（§27）；
  镜像: API-P3-001（FROZEN 编排面，不因本节改动）。

## Phase3 投影/WCS 公共消费面（API-P3-PROJ-001）

> ID: API-P3-PROJ-001  状态: CONTRACT_READY（P3-PROJ-DOC 冻结，
> 2026-09-11）
> 定位: Phase3 投影/WCS 域公共消费面——既有内核符号的展开冻结
> （**不新增、不修改任何 C 头/C ABI**；p3_wcs.h 为唯一权威签名头，
> 50 行，C++ namespace astrocs::phase3；编排面 p3_session.h 五段式=
> API-P3-001 FROZEN 不变，本节仅镜像声明）。
> SRC: lib/phase3_session/p3_wcs.cpp（165 行，astrocs_phase3_session
> 静态库成员，根 CMakeLists.txt:460-465）+ 唯一权威签名头
> lib/phase3_session/p3_wcs.h（50 行）；DATA: DATA-P3-WCS
> （DATA_SEMANTICS §28，单位/dtype/invalid 唯一权威）；ALG:
> ALG-P3-PROJ-IMPL-001（docs/algorithms/PHASE3_PROJ_IMPL.md，逐符号
> 锚与 G1/G2 冻结式）；MOD: astrocs.p3.projection（迁移目标
> astrocs_p3_projection.dll 为矩阵合同值，尚未存在——MISSING 语义，
> 由 P3-PROJ-IMPL 建立，本节不声明 IMPLEMENTED；descriptor 占位
> module_id=astrocs.phase3.wcs，module_adapters.cpp:406-423
> p3_wcs_descriptor，由 P3-PROJ-INT 对齐）。

### 内核符号与签名要点（p3_wcs.h 实测锚，冻结）

- `P3WcsStatus p3_wcs_make(double centre_ra_deg, double
  centre_dec_deg, double scale_deg_per_px, int width_px, int
  height_px, const char* parity, double rotation_pa_deg,
  P3WcsDescriptor* out)`（h:31-34 声明，实现 p3_wcs.cpp:30-90）:
  descriptor 构造入口——G1 冻结式（CRPIX=(W+1)/2、CD=R(−PA)·
  diag(sgn_x·s, sgn_y·s)，PA=0 精确退化对角 diag(−s,+s)/
  diag(+s,−s)，det(CD)=−s²<0 手性冻结，§6.2）；参数校验序 parity→
  |dec|≤85°→scale>0→W,H∈[1,20000]（:39-43）；四角同半球守卫
  （:80-88，失败码透传）。rc: 0=OK / 1=PARAM（parity 非法、
  |dec|>85°、scale≤0、尺寸越界、out 空）/ 3=HEMISPHERE（视场超
  TAN 半球）；out 失败时保持零初始化态（:35）。
- `P3WcsStatus p3_wcs_pix2world(const P3WcsDescriptor* d, double x,
  double y, double* ra_deg, double* dec_deg)`（h:38-39 声明，实现
  :93-118）: 像素→天球（G2 正向）——x,y **0-based**（FITS
  1-based=+1，:97-98）；中间坐标 ξ,η=CD·(pix−CRPIX)（deg→rad）；
  r≥π/2 →HEMISPHERE（:104）；gnomonic θ=atan(1/r)、φ=atan2(−ξ,η)
  →球面角（Calabretta & Greisen 2002 形式，:111-112）；RA 归一
  [0,360)（:113-114）。rc: 0=OK / 1=PARAM（空指针）/ 3=HEMISPHERE。
- `P3WcsStatus p3_wcs_world2pix(const P3WcsDescriptor* d, double
  ra_deg, double dec_deg, double* x, double* y)`（h:42-43 声明，
  实现 :120-143）: 天球→像素（G2 反向）——|dec|>85° →PARAM
  （:123）；背面 denom≤0 →HEMISPHERE（:130）；gnomonic (ξ,η)
  （:131-133）→线性解 CD·δ=(ξ,η)（det 奇异 |det|<1e-300 →PARAM
  :137）→0-based 像素输出（:140-141）。rc: 0=OK / 1=PARAM / 3=
  HEMISPHERE。
- `std::string p3_wcs_fits_keywords(const P3WcsDescriptor* d)`
  （h:46 声明，实现 :145-163）: descriptor→FITS 关键词文本
  （CTYPE1/2=RA---TAN/DEC--TAN、CUNIT1/2=deg、CRPIX1/2、CRVAL1/2、
  CD1_1..CD2_2，每行 ≤80 字节）；nullptr → 空串。调试/探针面；
  生产 FITS 头写路径在 p3_output 域（API-P3-FITS-001）。
- `struct P3WcsDescriptor`（h:11-20）/ `enum class P3WcsStatus`
  （h:22-27，OK=0/PARAM=1/UNSUPPORTED=2/HEMISPHERE=3）: 消费面
  数据结构冻结（字段级锚=DATA-P3-WCS §28.1/§28.2；UNSUPPORTED
  现无产生点——projection 硬编码 "TAN"，SIN/ZEA/CAR/AIT 扩展
  TODO，如实登记）。
- 并发语义: 四函数纯函数无状态（0 处 thread/mutex/omp/全局可变
  量，:12-28）——const-only 入口多线程并发安全（descriptor
  parallel_ok=true 与此一致）；确定性 bitwise（ALG-P3-PROJ-IMPL-001
  §10）。roundtrip 冻结容差 <1e-6 px（SCI-P3-001 §7，禁放宽）。
- 会话消费锚（编排面 API-P3-001 镜像）: p3_session.cpp:160
  p3_wcs_make（rotation_pa_deg 恒 0.0——PA 未接线整改项，不改码）
  /:163 状态映射（UNSUPPORTED→ACS_ERR_UNSUPPORTED，其余非 OK→
  ACS_ERR_PARAM）/:232 worker 循环逐像素 pix2world（失败 continue，
  半球外像素 NaN）/:247-253 worker 池=budget.max_workers（禁
  hardware_concurrency）。

### 交叉引用与登记语义

- 上游: SCI-P3-001（FROZEN，§5 连续定义 + §9a-4/-6/-12 + §7
  roundtrip 容差）引用不改动；ALG-P3-002（PHASE3_RESAMPLE.md §2
  G1/G2 施工规格，公式零改动）；ALG-P3-PROJ-IMPL-001（实现级
  合同）；DATA-P3-WCS（§28）。
- 整改迁移语义（登记不改码）: PA 未接线（p3_session.cpp:160 恒
  0.0）归 P3-PROJ-IMPL/INT；kMaxSide ASTROCS_P3_MAX_SIDE 编译期
  覆盖（p3_wcs.cpp:18-22）如实冻结；dll/入口未建归 P3-PROJ-IMPL。
  本域无 DISP 缺陷登记（P0 bughunt_p0_wcs 修复已合并，:65-68）。
- 测试语义: TEST-P3-WCS-001——登记面=TEST-P3-WCS-DESIGN-001 设计
  冻结 VERIFIED（ALG-P3-PROJ-IMPL-001 §12 T1-T7 + registry 手写页
  docs/modules/registry/astrocs.phase3.wcs.md §9，双重陈述）；
  现状执行测试 tests/unit/p3_wcs_test.cpp（90 行）+
  tests/backend/test_p1002_gaps.py（独立解析解回归）+
  tests/backend/p3_wcs_main.cpp（探针）=相邻证据引用不冒认；
  验收级（WCSLIB oracle）归 P3-PROJ-TEST 落地 + EVIDENCE。
- 下游: TEST-P3-WCS-001（登记面如上）；上游 SCI-P3-001（FROZEN）/
  ALG-P3-PROJ-IMPL-001 / DATA-P3-WCS（§28）；域际: API-P3-FITS-001
  （FITS 写出消费面，wcs 字段承载本 descriptor）；镜像: API-P3-001
  （FROZEN 编排面，不因本节改动）。

## Phase3 HiPS 重采样公共消费面（API-P3-RSMP-001）

> ID: API-P3-RSMP-001  状态: CONTRACT_READY（P3-RSMP-DOC 冻结，
> 2026-09-12）
> 定位: Phase3 HiPS 重采样域公共消费面——既有内核符号的展开冻结
> （**不新增、不修改任何 C 头/C ABI**；p3_resample.h 为唯一权威签名
> 头，58 行，C++ namespace astrocs::phase3；编排面 p3_session.h 五段
> 式=API-P3-001 FROZEN 不变，本节仅镜像声明）。
> SRC: lib/phase3_session/p3_resample.cpp（239 行，astrocs_phase3_
> session 静态库成员，根 CMakeLists.txt:460-465）+ 唯一权威签名头
> lib/phase3_session/p3_resample.h（58 行）；DATA: DATA-P3-RES
> （DATA_SEMANTICS §29，单位/dtype/invalid 唯一权威）；ALG:
> ALG-P3-RSMP-IMPL-001（docs/algorithms/PHASE3_RSMP_IMPL.md，逐符号
> 锚与 G3/G4 冻结式）；MOD: astrocs.p3.resample（迁移目标
> astrocs_p3_resample.dll 为矩阵合同值，尚未存在——MISSING 语义
> （DISP-P3RSMP-005），由 P3-RSMP-IMPL 建立，本节不声明 IMPLEMENTED；
> descriptor 占位 module_id=astrocs.phase3.resample2，module_adapters.
> cpp:363-377 p3_resample2_descriptor，由 P3-RSMP-INT 对齐）。

### 内核符号与签名要点（p3_resample.h 实测锚，冻结）

- `P3ResampleStatus p3_sampler_open(const char* hips_dir, P3Sampler*
  out, char* err)`（h:32-33 声明，实现 p3_resample.cpp:109+）:
  sampler 构造入口（open_ex 缺参薄封装）——hips_properties_parse
  严格校验（必需 keys hips_order/hips_tile_width/hips_frame/
  dataproduct_type；order∈[0,20]；tile_width 必须 512；NESTED 唯一）；
  失败必须经 err 缓冲带原因（禁静默默认，test_06 冻结）。rc 见
  P3ResampleStatus 表。
- `P3ResampleStatus p3_sampler_open_ex(const char* hips_dir,
  P3Sampler* out, int* out_order, char* out_bunit, char* err)`
  （h:36-38 声明，实现 :130-159）: 同上校验 + 回填 survey 实际
  order（int）与 BUNIT（缺省 'ADU'，**绝不 Jy/beam**——SCI §9a-11）；
  失败路径 properties 解析/目录不可读→IO、frame≠ICRS→UNSUPPORTED。
- `P3ResampleStatus p3_order_select(int max_order, double
  scale_deg_per_px, int* out_order)`（h:21 声明，实现 :82-93）: G3
  order 选择——最小 k 使 pixel_resolution_arcsec(512<<k)/3600 ≤
  scale_deg_per_px（与 ALG-P3-003 G3 ceil 式数学等价，§6.1）；扫描
  完未命中→out_order=max_order（欠采样降级，SCI §9a-5）；守卫
  out_order 空/max_order∉[0,20]/scale≤0→PARAM。
- `P3ResampleStatus p3_resample_check_mode(const char* input_mode)`
  （h:25 声明，实现 :95-107）: 输入模式守卫——`surface_brightness`
  唯一合法；其余→UNSUPPORTED（SCI §9a-8/10 显式拒）。实测: 会话层
  未接线（DISP-P3RSMP-003），探针消费。
- `void p3_sampler_set_max_tiles(P3Sampler* s, int max_tiles)`
  （h:42 声明，实现 :161-168）: tile 缓存容量设置；≤0 恢复默认 8；
  会话守卫默认 min(1024, ceil(W·H/512²)+16)，请求超默认→
  ACS_ERR_BUDGET（可降不可升，p3_session.cpp:179-194）。
- `void p3_sample_nearest(const P3Sampler* s, const P3WcsDescriptor*
  d, int x, int y, float* value, float* coverage)`（h:46-47 声明，
  实现 :232-239）: NEAREST 采样——输出像素中心→pix2ang→ang2pix
  精确 cell；无插值误差（SCI §9a-12）；值语义=DATA-P3-RES §29.2
  （tile 内 NaN→值 NaN+coverage=1；tile 缺失→coverage=0）。
- `void p3_sample_bilinear(const P3Sampler* s, const P3WcsDescriptor*
  d, int x, int y, float* value, float* coverage)`（h:51-52 声明，
  实现 :196-230）: BILINEAR 采样——leaf 3×3 邻域四象限最近中心、
  切平面双线性（FP64 权重，Σw=1 精确成立——SCI §9a-7 不变量）；
  离散化方案与 G4 施工规格差异=DISP-P3RSMP-001 如实登记。
- `void p3_sampler_close(P3Sampler* s)`（h:54 声明，实现 :170-180）:
  释放并置空；幂等（可安全重复调用）。
- `enum P3ResampleStatus`（h:12-17）: OK=0/PARAM=1/UNSUPPORTED=2/
  IO=3；`struct P3Sampler`（h:29-31，impl 指针+last_error[256]）:
  消费面数据结构冻结（字段级锚=ALG-P3-RSMP-IMPL-001 §4）。
- 并发语义: **每 worker 独立 sampler+cache**（自含 TileCache，无
  共享可变状态）；单一 P3Sampler 实例非线程安全（无内部锁），禁止
  跨线程共享——合同禁止项（ALG-P3-RSMP-IMPL-001 §7）；确定性
  bitwise（输出与 tile 装载顺序/worker 数/缓存容量无关）。
- 会话消费锚（编排面 API-P3-001 镜像）: p3_session.cpp:167-178
  主 sampler open_ex :171（状态映射 IO→ACS_ERR_IO、UNSUPPORTED→
  ACS_ERR_UNSUPPORTED、PARAM→ACS_ERR_PARAM，:175-176）/:179-194
  max_tiles 会话守卫/:196-199 p3_order_select（max_order=输入实际
  order）/ :217-244 每 worker 独立 open_ex（:222）+逐像素
  nearest/bilinear 分派（:236-237；缺省 bilinear，:116-119 白名单）/
  :258-263 取消收尾/:265-277 provenance
  （order_sel_used/sampler_used 填实际值，:276-277）。

### 交叉引用与登记语义

- 上游: SCI-P3-001（FROZEN，§4/§5/§9a-1/-5/-7/-8/-10）引用不改动；
  ALG-P3-003（PHASE3_RESAMPLE.md §2 G3/G4 施工规格，公式零改动）；
  ALG-P3-RSMP-IMPL-001（实现级合同）；DATA-P3-RES（§29）；
  DATA-P3-WCS（§28，输出平面几何上游）。
- 整改迁移语义（登记不改码）: DISP 台账五项
  （DISP-P3RSMP-001 bilinear 离散化方案与 G4 表述差异 /
  002 cache FIFO vs LRU 表述 / 003 check_mode 会话未接线 /
  004 provenance.missing_tiles 恒 nullptr / 005 dll 未建）归
  P3-RSMP-IMPL/INT（ALG-P3-RSMP-IMPL-001 §11）。
- 测试语义: TEST-P3-RES-001——登记面=TEST-P3-RSMP-DESIGN-001 设计
  冻结 VERIFIED（ALG-P3-RSMP-IMPL-001 §12 + registry 手写页
  docs/modules/registry/astrocs.phase3.resample2.md §9，双重陈述）；
  现状执行测试 tests/backend/p3_resample_probe_main.cpp（探针）+
  tests/backend/test_p3_resample.py（seam/NaN/无静默默认）+
  tests/unit/p3_interp_test.cpp / p3_coverage_test.cpp（独立参考
  实现）=相邻证据引用不冒认；可执行面升级归 P3-RSMP-TEST + EVIDENCE。
- 下游: TEST-P3-RES-001（登记面如上）；上游 SCI-P3-001（FROZEN）/
  ALG-P3-RSMP-IMPL-001 / DATA-P3-RES（§29）；域际: API-P3-FITS-001
  （FITS 写出消费面，resampled 平面为其输入）；镜像: API-P3-001
  （FROZEN 编排面，不因本节改动）。
