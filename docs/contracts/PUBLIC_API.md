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
