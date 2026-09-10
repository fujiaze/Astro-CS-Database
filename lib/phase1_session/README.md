# lib/phase1_session — Phase1 装配层合同（API-P1-SESSION / DATA-P1-SESSION）

> P1-SESSION-DOC 冻结（2026-09-07，SA-P1-R19）：本 README 为 Phase1 进程内
> 装配会话（`lib/phase1_session/p1_session.cpp/.h`）的模块合同，只描述**装配、
> 编排顺序、端口与 artifact schema**。科学公式、校准/星点/WCS/噪声/drizzle
> 算法定义不在本层——全部委托既有冻结合同（SCI/ALG 见 §2 节点表与 §8 链接），
> 本文档不含任何算法公式定义。单位/dtype/shape 以源码实测行号锚登记；
> descriptor 占位词汇不作为冻结依据。迁移矩阵无 P1-SESSION 行：SESSION 是
> 装配层，不设独立 DLL 迁移目标（§9）。

## 1 身份与范围

- MOD ID：`MOD-astrocs-phase1-session`；模块词汇 `astrocs.phase1.session`
  （归档映射表 docs/archive/refactor/P1_SYMBOL_MAP.md:14 既有词汇，现行
  registry descriptor 无此 module_id——P1-001 真实节点化（2026-09-10）
  前 p1_session 函数族经 `P1Api` 被 8 个 Phase1 descriptor 工厂委托；
  现行 8 descriptor 各自委托唯一真实 operation（§2，ARCH-P0-001 整改），
  p1_session 保留为 CLI 兼容装配会话）。
- owner：AstroCS（P1-SESSION-DOC, SA-P1-R19）；语言 c++17；ABI v1
  （`ACS_ABI_VERSION_V1`，p1_session.cpp:92 校验）。
- 构建锚：根 `CMakeLists.txt:448` `add_library(astrocs_phase1_session STATIC
  lib/phase1_session/p1_session.cpp)`；`:449-452` include 目录与 PUBLIC 链接
  `astrocs_contracts astrocs_calibration astrocs_aio`；`:496/:514/:530` 编入
  astrocs 主可执行链接列表。现状为**静态库**，无独立 DLL（迁移矩阵无
  P1-SESSION 行，`dll_name=MISSING` 见 module.yaml）。
- 生产调用方（登记）：`lib/core/src/module_adapters.cpp`
  - **现行（P1-001 后）**：8 个 Phase1 descriptor 注册 + `P1NodeModule`
    工厂唯一真实 operation 委托（p1_nodes[] 表；子节点不调
    phase_session_run，ARCH-P0-001 整改）；
  - 历史（P1-001 前）：`P1Api` 五函数指针委托 + `make_session_module<P1Api>`
    8 工厂同一委托（`P1Api`/SessionModule 现保留为兼容面）。
- 会话头文件注释合同（p1_session.h:1-3,15-28）：四段式
  create→validate→run→inspect(+destroy)；opaque handle，owner=创建者；
  不 shell-out；cancel/线程预算/监控经 host services 注入。

## 2 DAG 节点表（每节点：module ID / entry / TEST / 端口 / artifact schema 摘要）

**B 线（modular registry 装配，本库所属）**：`register_phase_modules`
注册 8 个 Phase1 descriptor。**P1-001（2026-09-10）真实节点化后**：全部
工厂改为 `P1NodeModule` 唯一真实 operation 委托（module_adapters.cpp
p1_nodes[] 表，operation/entry 名与
runtime/pipeline/module_ports.registry.json 冻结绑定表一致）——calibrate→
`ac_calibrate_frame`、cosmetic→`ac_correct_frame`、star-psf→
`StarDetector::detect`+`dpsf_fit_batch_f64`（lib/star_detector 检测 →
star_det v1 [N,6] → lib/dynamic_psf Moffat4 FP64 批量 PSF 拟合，
DATA-P1-PSF 携 psf_params:FLOAT64[N,9]；P1-001 attempt 2 口径更新），
wcs-platesolve→`ipv_solve_from_memory_with_callback_d`（lib/plate_solve
ipv 真实求解器链：sdet+gaia_client 句柄注入 → FP64 解算 → CD/CRVAL/
CRPIX/RMS → WcsTan roundtrip 自检；求解参数 ra0/dec0/focal_length_mm/
pixel_size_um/gaia_data_dir 缺失即 DATA 拒绝（禁 silent default）；
**ipv 非 Windows 平台为生产源内建 stub → Linux 节点 fail-closed 如实
报平台限制，Windows 侧真实求解**；生产源零 diff）、photometry→
`Photometer::measure`、noise-snr→`NoiseModel::estimate`、drizzle→
`hp_drizzle_run`、writer→`aio_hiss_inspect/read_tile_*`→
`AstroSphereTileView`→`aio_hips_product_begin/write_signal_support_tile/
finalize`（消费上游 p1_stack.hiss，NESTED 聚合展开为 IVOA 1.4 标准
512×512 HiPS：signal/+support/+properties/MOC；covered_area=
support>0?A_cell:0 单帧语义，p1_final.json 登记
covered_area_model="support_x_A_cell"；IVOA nside>=512 合同，上游
drizzle nside<512 节点 DATA 拒绝）；**子节点禁止调用完整
phase_session_run**（ARCH-P0-001 整改，P1Api 仅保留兼容面）。descriptor 端口/DATA/单位/坐标为
module_adapters.cpp 实测；`ALG-002/ALG-004/ALG-005` 为 descriptor 占位
kernel 词汇（各域冻结 ALG 以"冻结合同"列为准，由各 INT 任务对齐，
与 P1-PSF-DOC 先例一致）：

| # | 节点 module ID（descriptor 行） | 端口 in → out（DATA/单位/坐标） | descriptor 占位 SCI/ALG/API/TEST | 冻结合同（权威指针） | entry 与算法委托 | 冻结 TEST 设计 |
|---|---|---|---|---|---|---|
| 1 | `astrocs.phase1.calibration`（:254） | `frames`(DATA-P1-FRAME/ADU/PIXEL) → `calibrated`(DATA-P1-CAL/ADU/PIXEL) | SCI-P1-CAL-001 / ALG-P1-CAL-001 / API-P1-001 / TEST-P1-CAL-001 | SCI-CAL-001（docs/science/CALIBRATION.md）；ALG-CAL-001..006（docs/algorithms/CALIBRATION_ALGORITHMS.md）；DATA-P1-CAL（DATA_SEMANTICS §9）；API-CAL-001（PUBLIC_API） | P1-001 后=ac_calibrate_frame（p1_nodes[] 直调；p1_session 四段编排见 §3） | TEST-CAL-DESIGN-001（CALIBRATION_ALGORITHMS §9） |
| 2 | `astrocs.phase1.cosmetic`（:411） | `calibrated`(DATA-P1-CAL) → `cleaned`(DATA-P1-COSMETIC/ADU/PIXEL) | SCI-P1-COS-001 / ALG-P1-COS-001 / API-P1-002 / TEST-P1-COS-001 | SCI-CAL-001 共享；ALG-COS-001..005（docs/algorithms/COSMETIC_ALGORITHMS.md）；DATA-P1-COS（DATA_SEMANTICS §10）；API-COS-001（PUBLIC_API） | P1-001 后=ac_correct_frame（p1_nodes[] 直调，in-place 覆写校准帧；p1_session 编排同款 :294；master nullptr 恒等=DISP-COS-009 语义） | TEST-COS-DESIGN-001（COSMETIC_ALGORITHMS） |
| 3 | `astrocs.phase1.star-psf`（:430） | `cleaned`(DATA-P1-COSMETIC) → `sources`(DATA-P1-SOURCES/DIMENSIONLESS/ICRS)+`psf`(DATA-P1-PSF/DIMENSIONLESS/PIXEL) | SCI-P1-PSF-001 / ALG-002（占位）/ API-P1-003 / TEST-P1-PSF-001 | SCI-P1-PSF-001+ALG-STARPSF-001（STAR_PSF_ALGORITHMS §11）；DATA-P1-PSF（DATA_SEMANTICS §15）；API-PSF-001（PUBLIC_API） | P1-001 attempt 2 后=sdet 检测（lib/star_detector 生产源）+`dpsf_fit_batch_f64`（lib/dynamic_psf Moffat4 FP64 批量拟合；DATA-P1-PSF 携 psf_params:FLOAT64[N,9]+detection_schema=star_det_v1:FLOAT64[N,6]；N>0 全失败 DATA 拒绝） | TEST-PSF-DESIGN-001（STAR_PSF_ALGORITHMS §11.4） |
| 4 | `astrocs.phase1.wcs-platesolve`（:450） | `sources`(DATA-P1-SOURCES) → `wcs`(DATA-P1-WCS/DIMENSIONLESS/ICRS) | SCI-P1-WCS-001 / ALG-002（占位）/ API-P1-004 / TEST-P1-WCS-001 | 矩阵行显式 MISSING（MOD-astrocs-phase1-wcs-platesolve） | P1-001 attempt 2 后=`ipv_solve_from_memory_with_callback_d`（ipv 真实求解链；Linux=源内 stub fail-closed 报平台限制、Windows=真实求解；缺求解参数 DATA 拒绝；p1001 平台化单测）| 无（TEST-P1-WCS-001 词汇，待迁移任务） |
| 5 | `astrocs.phase1.photometry`（:469） | `psf`(DATA-P1-PSF)+`sources`(DATA-P1-SOURCES) → `fluxes`(DATA-P1-FLUX/ELECTRON/ICRS) | SCI-P1-PHOT-001 / ALG-002（占位）/ API-P1-005 / TEST-P1-PHOT-001 | SCI-P1-PHOT-001；ALG-PHOT-001..002（PHOTOMETRIC_FIT.md）；DATA-P1-PHOT（DATA_SEMANTICS §14）；API-PHOT-001（PUBLIC_API） | P1-001 后=Photometer::measure（p1_nodes[]）；A 线生产调用锚 orchestrator.cpp:2474 → :2714（FP64 `pc_calibrate_simple_with_gaia_f64_v2`）/:2790（FP32 `pc_calibrate_simple_with_gaia_v2`） | TEST-PHOT-DESIGN-001（PHOTOMETRIC_FIT.md） |
| 6 | `astrocs.phase1.noise-snr`（:489） | `fluxes`(DATA-P1-FLUX) → `snr`(DATA-P1-SNR/DIMENSIONLESS/ICRS) | SCI-P1-SNR-001 / ALG-004（占位）/ API-P1-006 / TEST-P1-SNR-001 | ALG-NOISE-001..003（NOISE_ESTIMATION.md）；DATA-P1-NOISE（DATA_SEMANTICS §13）；API-NOISE-001（PUBLIC_API） | P1-001 后=NoiseModel::estimate（p1_nodes[]）| TEST-NOISE-DESIGN-001（NOISE_ESTIMATION.md） |
| 7 | `astrocs.phase1.drizzle`（:508） | `calibrated`(DATA-P1-CAL) → `stacked`(DATA-P1-STACK/ADU/ICRS) | SCI-P1-DRIZ-001 / ALG-005（占位）/ API-P1-007 / TEST-P1-DRIZ-001 | ALG-DRZ-001（DRIZZLE_GEOMETRY.md）；DATA-P1-DRZ（DATA_SEMANTICS §11）；API-DRZ-001（PUBLIC_API） | P1-001 后=hp_drizzle_run（p1_nodes[] 直调；IVOA nside>=512 合同由下游 writer fail-closed 校验）| TEST-DRZ-DESIGN-001（DRIZZLE_GEOMETRY.md） |
| 8 | `astrocs.phase1.writer`（:527） | `stacked`(DATA-P1-STACK) → `fits`(DATA-P1-FITS/ADU/ICRS) | SCI-P1-WR-001 / ALG-P1-WR-001 / API-P1-008 / TEST-P1-WR-001 | HiPS 写出域：ALG-HIPS-001..005（HIPS_WRITER.md）；DATA-P1-HIPS（DATA_SEMANTICS §12）；API-HIPS-001（PUBLIC_API）；writer/hips registry 无独立 descriptor（hips 页 docs/modules/registry/astrocs.phase1.hips-writer.md 为手写合同页） | P1-001 attempt 2 后=aio_hiss_inspect/read_tile_* → AstroSphereTileView → `aio_hips_product_begin/write_signal_support_tile/finalize`（消费 p1_stack.hiss；NESTED 聚合 → IVOA 1.4 标准 512×512 HiPS signal/+support/+properties/MOC；covered_area_model=support_x_A_cell 单帧语义） | TEST-HIPS-DESIGN-001（HIPS_WRITER.md §9） |

**A 线（CLI 生产编排，现行唯一 7-stage 全链）**：
docs/architecture/production_call_paths_stage1.csv 登记 7 条生产调用路径
（calibrate→platesolve→photometric→drizzle→snr→HiPS 写出，唯一入口
`Orchestrator::run`，DIAGNOSTIC_FIELD/退出码/test ID 逐行登记）。A 线经
DLL 显式加载（dll_loader），与本库（B 线静态库）并存；两条装配线的算法
委托不产生第二套调度顺序——B 线 session 对 3..8 号节点只提供 registry
注册与兼容 adapter，不提供算法实现（§3 差距声明）。

**p1_session 内部节点（canonical 4 段，tests/unit/p1_ir_facade_test.cpp:33-40
静态断言与源码注释一致）**：`io_read` → `calibrate` → `cosmetic` →
`io_write`（p1_session.cpp:168/195/283/319 段首行号）。

## 3 装配顺序与数据流（p1_session_run 实测）

`p1_session_run`（p1_session.cpp:149）在 `async_io_depth∈{0,1,2}` 校验
（:152）与 config 复检（:156-159）后按固定顺序执行：

1. **io_read**（:168-192）：读 master（bias/dark/flat，全可空）+ input_lights
   逐帧；`aio_read` 自动探测格式（:71-74 注释+调用：`.fts/.fits`→FITS，
   `.xisf` 校准母版→XISF）；取消点=文件粒度（:177-181）；manifest stage
   条目记 `files`（:191）。
2. **calibrate**（:195-280）：逐帧 `ac_calibrate_frame`（:243，dark_scale
   因子 `k_fixed=doc.value("dark_scale_factor",1.0f)` :225）；master/光帧
   尺寸不匹配→PARAM（:218-221/:236-239）；输出写
   `output_dir/calibrated_<base>`（:258-262）：复用读结构
   `aio_read_fits`（:256）覆写像素（:258）后 `aio_write_fits`（:262，仅
   FITS）；每帧 artifacts 登记与 `++frames_ok`（:270/:274-275）；取消
   点=帧粒度（:228-231）；stage 记 `frames`/`per_frame`（:277-279）。
3. **cosmetic**（:283-317）：`cosmetic.enabled=false` 跳过（键集见 §4，
   开关 :284）；逐校准帧 `ac_correct_frame`（:294-297，master_dark/
   master_bias 实参传 nullptr）in-place 覆写后写回（:307-309）；
   取消点=帧粒度（:289）。
4. **io_write**（:319-330）：校验每个 artifacts 路径存在
   （:323-327，filesystem 存在性检查）；成功后 `manifest["frames"]` 与
   `manifest["status"]="complete"`（:333-334）。

数据流为严格线性（无分支/无循环依赖）；失败即短路返回（§5），
不留伪完整产物（头注释 p1_session.cpp:4）。

**如实差距声明（不宣称 session 完成 7-stage）**：docs/api/PHASE1_API_V1.md
（API-P1-001，FROZEN）§1 声明 run 内部阶段序列=校准→检测/PSF→plate
solve→测光定标→SNR→Drizzle→HiPS；**现行实现仅落 CAL+COS 域 4 段**，
photometry/psf/wcs/noise/drizzle/hips 在 B 线 session 内无执行段（其
registry 端口存在、P1-001 后工厂真实 operation 委托（p1_nodes[]），执行语义由 B 线节点或
后续 P1-SESSION-IMPL 整改补齐，见 §9）。本 README 不以此差距反推算法
行为，不修改 SCI/ALG 冻结文档。

## 4 外部输入

**config JSON（validate 键集，p1_session.cpp:115-143；纯读无 IO 幂等，
:103-147）**：

| 键 | 必/可 | 类型 | 默认 | 消费段 |
|---|---|---|---|---|
| `input_lights` | 必 | 非空 string 数组 | 无（缺失/类型错→PARAM，:116-126） | io_read |
| `output_dir` | 必 | string | 无（:116-119） | calibrate/io_write |
| `master_bias` / `master_dark` / `master_flat` | 可 | string 或 null | null（:127-131） | io_read/calibrate |
| `cosmetic` | 可 | object（值 numeric/bool） | `{}`（:132-140） | cosmetic |
| `cosmetic.enabled` | 可 | bool | true（run 开关 p1_session.cpp:284 `value("enabled", true)`；facade 测试断言默认开） | cosmetic |
| `dark_optimization` | 可 | bool | false（:141-143） | calibrate |
| `dark_scale_factor` | **validate 不验** | float | 1.0（run :225 `value()` 兜底） | calibrate |

**host services（include/astrocs/common_abi_v1.h:110-117
`astrocs_host_services_v1`）**：`allocator`/`logger`/`cancel`/`budget`
（thread budget :100-108：`available_cpus`=affinity∩cgroup∩Job Object、
`max_workers`=本次 worker 上限、原子 acquire/release；cancel :92-97 单向
置位只读轮询）。create 校验 `struct_size`/`abi_version`（:91-93）。

**文件输入**：光帧/母版经 `aio_read`（FITS/XISF 自动探测）；校准输出仅
FITS（`aio_write_fits`，:262）；图像释放必经 canonical deleter
`aio_free_image_data`（:59-61，IO-002 释放合同）。

## 5 错误与取消传播

返回码=ACS_ERR_*（ACS_OK/PARAM/ABI_MISMATCH/NOMEM/IO/UNSUPPORTED/
CANCELLED/STATE/BUDGET/INTERNAL；SessionModule 侧映射
module_adapters.cpp:61-91）：

| 语义 | 触发锚 |
|---|---|
| ABI_MISMATCH | create host 结构/ABI 校验失败（:91-93） |
| PARAM | config 解析失败/缺必需键/类型错（:110-143）；async_io_depth 越界（:152）；master/光帧尺寸不匹配（:218-221/:236-239） |
| IO | 文件不可读（:183-187/:233-235）；写失败（:261-267/:306-310）；artifact 落盘后缺失（:323-327） |
| INTERNAL | `ac_calibrate_frame`/`ac_correct_frame` 非 OK（:249-253/:301-305，经 `ac_err_name` :43-49） |
| NOMEM | session/inspect 缓冲分配失败（:96/:351-353） |
| CANCELLED | io_read 文件粒度（:177-181）/calibrate 帧粒度（:228-231）/cosmetic 帧粒度（:289） |

- 错误细节：`last_error` 脱敏摘要（SessionState.last_error :30；C++
  `astrocs::phase1::last_error` :368-371）；失败路径先填 `last_error` 再
  返回；manifest 同步记 `error_kind`（"input" :185/:204-212 等）。
- manifest 状态机：`created`（inspect 于未 run 且无错时 :342-343）→
  `failed`+`error`（:344-346）→ `complete`（:334）。
- inspect 输出=host alloc（:349-357），调用方经 host free 释放（:25 注释）。

## 6 并发与资源

- 并发合同（p1_session.h:15 注释）：`reentrant:yes`；
  `threadsafe:no(handle 级——单 handle 单线程使用)`；内部并行=omp，
  由 host 预算驱动。
- 线程注入：`ac_set_num_threads(host->budget.max_workers)`（:162-165，
  budget 注入迁移整改点，禁硬编码核数——头注释 :3 与
  PHASE1_API_V1 §2 TB-ARCH-004 一致）。
- ThreadLease 接线（B 线 registry 通道）：SessionModule.execute 先原子
  预算租借（module_adapters.cpp:156-162，RAII 归还；预算耗尽→1 worker
  串行），再以授权数初始化 host services；provider 真实观测
  `ctx.set_provider("baseline")` + PROVIDER_ENTER trace（:170-183）。
- 执行序：execute 内先 validate 后 run（:193-201，RT-008），destroy 前
  捕获 session manifest（:202-209）；`validate_config` 同样走
  create→validate→destroy（:121-141）。
- 内存：图像 unique_ptr 管理（:55-61）；manifest JSON 随 handle 生存；
  inspect 缓冲 host 分配调用方释放。
- 取消检查点：文件/帧粒度（§5），无更细粒度 checkpoint（无断点续跑）。

## 7 验证

- 生命周期五函数登记一致性：`tests/api/test_p1_api.py`（API-003
  doc-symbol-signature 合同；`test_02_lifecycle_five_functions` :50-55
  校验 PHASE1_API_V1.md 含五函数与四段式表述）。
- facade 语义与 canonical 节点集：`tests/unit/p1_ir_facade_test.cpp`
  （:33-40 四节点声明断言；:49-57 只作 facade——委托
  `ac_calibrate_frame`/`ac_correct_frame`，禁内联校准公式；:60-72
  委托层可调用验证）。
- 符号映射完整性：`tools/check_p1_symbol_map.py`（p1_session.h 导出 API
  ↔ docs/archive/refactor/P1_SYMBOL_MAP.md 映射表）。
- 生产可达性：`tools/quality/check_prod_reachability.py:42` 与
  `tools/check_pipeline_trace.py:16` 将 lib/phase1_session/p1_session.cpp
  列为 p1 生产锚。
- 本合同验收：DATA-P1-SESSION=docs/contracts/DATA_SEMANTICS.md §16；
  API-P1-SESSION=docs/contracts/PUBLIC_API.md「Phase1 装配会话」节；
  矩阵行=MOD-astrocs-phase1-session（docs/traceability/）。
  TEST-P1-SESSION-001 锚 tests/unit/p1_ir_facade_test.cpp（可执行测试
  已接线登记）。
- complete 门 fail-closed（P1-001, 2026-09-10）：run 成功路径 manifest
  `status="partial"`（不写 complete）+ `availability` 8 域如实报告
  （calibration/cosmetic=available，star_psf/wcs/photometry/noise_snr/
  drizzle/writer=unavailable）；PROD-P0-001 Phase1 侧整改锚
  `tests/unit/p1001_real_nodes_test.cpp::test_complete_gate_fail_closed`。
  既有 u1/u1b/u7/w1/p2 断言同步迁移 partial（run 链不完整期间 complete
  语义冻结，链完整迁移完成后按控制包门禁恢复）。

## 8 已知限制（登记不改码）

1. **7-stage 差距**：API-P1-001 冻结序列（校准→检测/PSF→plate solve→
   测光→SNR→Drizzle→HiPS）与 4 段现状的差距见 §3；补齐归后续
   P1-SESSION-IMPL/INT（矩阵无 P1-SESSION 迁移行，任务拆分由控制包定）。
2. **descriptor 占位词汇**：star-psf/wcs/photometry 的 `ALG-002`、noise 的
   `ALG-004`、drizzle 的 `ALG-005` 为 module_adapters.cpp 占位 kernel
   注释（:442/:484/:503），冻结 ALG 以 §2 节点表"冻结合同"列为权威，
   由各域 INT 任务对齐（P1-PSF-DOC 同先例）；`TEST-P1-*-001` registry
   词汇对应可执行测试待各 TEST 任务落地。
3. **cosmetic 恒等现状**：master_dark/master_bias 传 nullptr（:295-296
   调用实参）→检测全禁用，即 DISP-COS-009（lib/cosmetic/README.md 登记）；
   接通 cosmetic master 属 P1-COS-INT 整改。
4. **dark_scale_factor 未验**：validate 不校验该键类型，run `value()`
   兜底 1.0（:225）——键集与验面的不一致如实登记，整改归
   P1-SESSION-IMPL。
5. **async_io_depth 现状语义**：参数域 {0,1,2} 强校验（:152），当前
   实现未按 depth 启动预读 worker（PHASE1_API_V1 §1 预留语义），
   如实登记不改码。
6. **写格式**：校准输出仅 FITS（aio_write_fits）；XISF 仅作读侧探测。

## 9 迁移

- SESSION 为装配层：MODULE_MIGRATION_MATRIX 无 P1-SESSION 行，不设
  独立 DLL（`dll_name=MISSING`，静态库 astrocs_phase1_session）；模块化
  迁移边界由 §2 B 线 8 descriptor 承载，各算法域迁移见各模块 matrix 行
  （P1-CAL/COS/PSF/PHOT/NOISE/DRZ/HIPS）与其 module.yaml。
- 入口符号已真实导出（entrypoint=p1_session_run）；后续整改
  （7-stage 段补齐、取消粒度细化、async 预读）为 P1-SESSION-IMPL
  范围，均不得改动本合同端口/DATA/单位登记。

## 10 参考

- 头文件：`p1_session.h`（五 C API :16-28；并发合同 :15；inspect
  所有权 :25；C++ last_error :33-37）。
- 编排合同：docs/api/PHASE1_API_V1.md（API-P1-001..010，FROZEN）。
- 数据语义：docs/contracts/DATA_SEMANTICS.md §16（DATA-P1-SESSION）。
- 公共 API：docs/contracts/PUBLIC_API.md（API-P1-SESSION 节）。
- 架构：docs/contracts/ARCH-001.md；生产路径表
  docs/architecture/production_call_paths_stage1.csv。
- 旧映射（历史）：docs/archive/refactor/P1_SYMBOL_MAP.md。
