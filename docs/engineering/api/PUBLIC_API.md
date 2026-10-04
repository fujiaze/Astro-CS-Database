# 公共 API 合同

上游：最高设计的命令树、机器输出与退出码、模块与 ABI 三章；跨动态库边界的类型与并发合同
模板见 `abi/ABI.md`。

每个稳定公共 API 关联一个 `API-*` 合同 ID。本文的分阶段 API 面按 `normalize`、`mosaic`、`export`
组织，公共规则与错误模型统一给出，符号清单以公开头文件为准。

公共 C++ API 只在同一 toolchain 边界内使用；CPU DLL / so 使用 C ABI；exception 不跨 ABI。

## 分层

| 层 | 类型 | 边界 | 说明 |
|---|---|---|---|
| CLI 命令面 | 进程 | stdout JSON / stderr 日志 | 唯一命令树 = `normalize` / `mosaic` / `export` / `help` / `--version` / `doctor` / `benchmark`（`../../ACSD_DESIGN.md` 「命令树」一节）；人类可读合同 = 「分层」一节 |
| Pipeline Runtime API | C++ public | 同一 toolchain | IR / registry / scheduler / services |
| Module API | C++ public | 进程内注册 | `IModule` 描述 / plan / execute |
| CPU Provider ABI | C | DLL / so 边界 | `acsd_cpu_provider_v1` |
| I/O ABI | C | 进程内 | `aio_*` |

## 错误模型

### CLI 退出码

退出码的码值语义唯一声明处 = 「验收」一节
（机器事实源 `lib/infrastructure/cli/exit_codes.h`）。本节只声明接口面归属：

- CLI 收敛面按该表输出 11 码之一，不新增、不改义；
- `ErrorDomain` → 退出码的映射 = 「API 契约 ID」一节；
- 码 10 的适用面是磁盘写满 / 写盘失败；内存 / CPU / 线程不设资源超限门；
- 退出码映射测试覆盖全部 11 码。

### Error domains

- `CONFIG` / `DATA` / `SCIENCE_PRECONDITION` / `IO` / `RESOURCE` / `BACKEND` / `CANCELLED` / `INTERNAL`；
- 模块以 `Result` 报错，`exit` / `abort` 归进程宿主；异常只在内部转 `Result`，不能穿过 C ABI；
- `Result<T>` 支持 nested cause、serialization、cancel、错误码映射；
- 三层语义（success / recoverable science status / hard error）与违规判据 =
 「分层」一节 / 「request」一节。

### 公共 API 规则

- 头文件必须 `extern "C"` + 导出宏（`AC_API` / `AIO_*` / `P2_API` 等）；
- 每个函数的项目等价注释必含五要素：inputs / units、outputs、ownership、
 failure（返回值语义）、thread safety、precision / lifetime；
- 状态取值为稳定 enum / status + 字符串 error（二者可并存）；magic number 不作状态表示；
- 失败时输出参数必须重置或文档化「未写入」；
- public raw pointer 必须注明 borrowed / owned / optional。

### C ABI 规则

- 结构带 `size` / `version`；跨 DLL 传递面 = C ABI 值类型，STL / 异常 / allocator 所有权留在各自侧；
- 固定宽度类型；显式 size / version 校验；坏 provider 拒绝且优雅 fallback；
- 异常保留在 C 边界函数内的 `try` / `catch` 全包裹中（如 `p2_upm_open`），不跨边界逸出；
- 输出在失败时重置：`*out_model = nullptr` 语义由实现保证，调用方的依据是已完整初始化的对象；
- partial allocation 走单出口 cleanup：RAII 或在失败路径 `delete`；
- 句柄类型：`void*` 只允许 opaque handle，ownership 须文档化；
- 字符串输出：传入容量，写 NUL，拷贝按容量带界；
- 整数参数：checked（count / offset 与 buffer 容量匹配）；
- 错误返回：`0` = success，非 `0` = hard error；recoverable science status 经输出状态字段表达。

## 机器输出

- `--json` 输出 schema 固定；stdout 只承载 JSON，日志走 stderr；
- **运行事件流（唯一 schema）**：实现正本 = `lib/infrastructure/cli/protocol.h`（`ValidateEventV1`，发送侧硬闸）+ `lib/infrastructure/cli/jsonl.h`（`JsonlEmitter`）；人类可读合同 = 「生命周期 / 线程安全 / 可重入 / 取消」一节；机器 schema = `eng/contracts/schemas/jsonl_event_v1.schema.json`（**派生件**，字段 / 枚举 / 顺序键以 `protocol.h` + `jsonl.h` 为准）。必含字段 = `schema_version/event_id/run_id/timestamp_utc/sequence/kind/severity/phase/stage/message`，事件枚举键名 = **`kind`**（progress / resource / artifact / backend / final / stage_start / stage_end）。
- **与结构化日志分属两份合同**：`../resources/observability/STRUCTURED_LOGGING.md` 是**结构化日志**合同（人可读摘要 + 机器 JSONL 双通道同源），**不是**运行事件流；其事件键名 `event` 与运行事件流的 `kind` **各自独立**，两份流各用不同工件名。

## 生命周期 / 线程安全 / 可重入 / 取消

- RunContext 只在节点执行期间有效；模块的 context 生命周期到 execute 返回为止；
- 公共对象 thread-safety 在 header 注释声明；所有权（borrow / shared / unique / persisted）
 按统一工程对象正本（当前在本目录根部，归属 detail 层）；
- 取消经 CancellationToken 传播；checkpoint 只在 Artifact 原子提交后写入；
- 跨边界函数的并发合同必填字段模板（`reentrant` / `threadsafe` / `internal_parallel` / `aliasing` +
 内存去向 + 取消点粒度）= 「机器输出」一节。

## API 契约 ID

每个稳定公共 API 关联一个 `API-*` ID。合同清单 =
`PUBLIC_API.md`，追溯登记面 = `governance/TRACEABILITY.md 「模块与源码追溯矩阵」一节`。
已登记的 ID 举例：

| API ID | 覆盖面 |
|---|---|
| `API-AIO-001` | FITS / XISF / HiPS 读写 |
| `API-P2-REJECT-001` | rejection 规划 |
| `API-P2-UPM-001` | UPM build / save / open / calibrate |

`PUBLIC_API.md` 是现有 C ABI（`aio_*` / `p1_*` / `p2_*` / `p3_*`）的合同清单；
本合同冻结其语义，AST 提取出的符号面必须与该清单逐条一致。
新增 Runtime / Module API 的目标签名以 `../../ACSD_DESIGN.md` 「顶层结构」一节（顶层结构）与
（模块与 ABI）为准；目标签名先登记为 expected signature，再由 AST 提取的真实符号锁定。

## 变更纪律

- 接口冻结后改变语义一律走显式变更，不做就地语义漂移；
- 行为变化的推进顺序：Contract first → code → tests → diagnostics → docs。

## 验收

- AST 提取与 API index 零漂移；exception 不跨 C ABI；
- 退出码映射测试覆盖 11 码（码值表见 「CLI 退出码」一节 指向的正本）；
- negative fixture（改名 / 漏符号）必须失败；
- 每个跨边界函数的头注释含 「显式拒绝清单」一节 指向的并发合同必填字段。

## C ABI 声明面与生产调用面（口径条款）

> 上游：docs/ACSD_DESIGN.md 「I/O 与原子产品」一节（块与文件之间的导出/缓存接口属诊断/测试接口，与生产路径区分登记）。

- **声明语义**：`header.P2_API` / `header.AC_API` 等标记声明的符号属于**模块 C ABI 面**，
 供同库其它入口、诊断工具、契约/Oracle 测试与下游消费；声明本身不构成「三个生产命令
 （`cmd_session{1,2,3}_run`）在运行期必然调用该符号」的承诺；生产路径使用 `*_ex` /
 `*_v1` / `*_wcs` 等变体入口时，裸名声明仍在合同面内。
- **零消费者声明的处置**：生产接线台账中的豁免项以「具名在仓消费者 +
 具名退出条件」为门槛，不满足门槛的符号只能接线或撤下声明。
- **消费者名册**：① 本模块诊断/工具面（`lib/algorithms/coverage/tools/stage2.cpp`、
 `rejection_cli.cpp`、`tests/synthetic_gate.cpp`、`tests/sanitize_driver.cpp`）；
 ② 契约与 Oracle 测试（`eng/tests/unit/p2_*`、`eng/tests/validation/release02/`）；
 ③ 生产路径的变体入口（`p2_reject_stack_ex` / `p2_sky_plane_eval_delta` /
 `p2_sample_controls*` / `p2_upm_build_geo` / `p3_sample_*_ex` 等，已接线）。
- **豁免退出条件**（任一即撤下对应豁免）：该符号进入三个命令的生产调用图；或按本合同
 变更流程退役该符号（删声明 + 实现 + 测例迁移）；或被并入已接线的变体入口。
- **接线面依据**：`p2_upm_ma_*`（UPM 矩阵装配族）的生产链走
 `lib/algorithms/coverage/include/astro/phase2/upm.h:386` 冻结的 `p2_upm_build_geo`
 （坐标下降 Huber IRLS）；`p2_reject_stack`（兼容签名）为 COMPAT adapter，生产 Stage2
 不调用；`p3_projection_*` / `p3_sample_*` 的生产 export 走 `acsd_p3_projection_wcs`
 与 `p3_sample_*_ex` / `p3_sampler_open_ex` / `p3_output_*_ex` 变体。

## C ABI（`extern "C"`，不跨边界抛 C++ exception）

- `lib/infrastructure/aio`：`aio_*`（image/HiPS I/O、writer/reader、pipeline）。
- `lib/algorithms/coverage`：`p2_*`（coverage / sampler / upm / integrate / stage2 入口）。
- `p2_upm_build`（obs-only，兼容）与 `p2_upm_build_geo`（全几何节点，
 V13/V14）。
- `p2_sample_controls` / `p2_sample_controls_cached`（后者性能透传 `frame_id_cache` 避免二次 500MB payload 哈希；同数值语义）。
- V16/V17 rejection 接口（typing 单语义，版本化政策）：
 - `p2_reject_plan_resolve`（planning 层把 auto 解析为显式方法 +
 method-specific typed params；profile 默认 `acsd_adaptive_pixel`
 （自研，逐输出像素几何 n）；对照档 `wbpp_2_9_1` 或
 `acsd_adaptive` 独立策略）；
 - `p2_eligibility_filter` / `p2_collect_candidate_stack`（V16 生产 strided
 collector：finite/valid/support/quality → CandidateStack；Stage2 CPU/ACR
 统一入口）；
 - `p2_validate_candidate_weights`（V17：SNR lookup 后统一非 finite/非正
 权重校验，Stage2 的校验入口 = 该函数）；
 - `p2_reject_stack_ex`（explicit plan kernel；per-sample reason +
 stack-level status 分离；V17 契约：仅 OK/UNDERDETERMINED 可继续，其余
 INVALID_*/INTERNAL_ERROR 必须 hard fail）；
 - `p2_large_scale_apply`（V17：acsd.large_scale_rejection.v1，
 per-frame low/high rejection mask 的 connected-component grow）；
 - `p2_integrate_pixel`（V17：唯一 canonical support reducer=max(accepted
 support)；显式状态 OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/
 INVALID_INPUT；非 finite weight/support 绝不返回 OK）；
 - `p2_reject_stack`（兼容签名）为 COMPAT adapter，生产 Stage2 不再调用。
- 返回码：0=OK；非 0 具体语义见各头文件注释；`err` 缓冲只做日志，不承载
 状态机。

## 状态与错误所有权（V14 合同）

- **返回值所有权**：每个 C ABI 函数的返回码由该模块独占定义（各头文件注释
 为唯一权威），调用方只按 0/非 0 与头文件语义分支，错误字符串只作人类可读文本。
- **错误缓冲 `err`**：仅承载人类可读日志文本，不参与状态机；为 `nullptr`
 时函数必须仍能正常执行并返回状态码。缓冲区所有权/容量/生命周期由各头
 文件声明，无隐式全局错误对象。
- **日志与状态分离**：日志落点派生自 `output_dir`（正本见
 `../contracts/LOG_AND_ERROR.md`；不落进程 CWD、源码树与开发/CI 过程产物区），
 返回状态只经返回值传递；模块内部日志级别只作用于日志面，控制流只经返回值。
- **C ABI 不抛异常**：`extern "C"` 边界全部捕获并转换为返回码；`buffer
 ownership/lifetime/nullable/单位` 在头文件逐参数注释。
- **跨阶段**：Phase1 产物语义错误（非法 WCS/负 flux 等）必须在 Phase2 入口
 以非 0 返回码显式拒绝，取值面 = 拒绝码本身。

## C++ API

- `acsd::healpix`（healpix_core：ang2pix/pix2ang/nested_local↔FITS index）。
- `acsd::crypto`（SHA-256）。
- 命名空间建议：`acsd::phase1 / phase2 / hips / acr`（不强制破坏现有
 `p2_*` ABI；C++ 层可逐步包装）。

## 工具/CLI

- 当前生产入口 = `normalize --json <config.json>` 与 `mosaic --json <config.json>`（CLI-001 唯一命令树）。
- `healpix_browser_qt.exe`（HiPS 浏览器；`--hips/--standard-hips/--view/
 --screenshot/--lod/--exit`）。
- `eng/build/toolchain.ps1 check|build|run|review`（统一工程入口）。

## JSON schema（config）

- stage1: `lib/infrastructure/pipeline/orchestrator/configs/stage1_*.json`。
- stage2: `lib/algorithms/coverage/configs/stage2_*.json`（model/integration/output/
 diagnostics 四段；默认值唯一来源见 `../contracts/CONFIG.md`）。

## gaia_client C API（API-GAIA-001）

> SRC: lib/infrastructure/gaia_xpsd_client/src/gaia_client.c；ALG: ALG-GAIA-001；
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
 ALG-GAIA-001 「显式拒绝清单」一节）；`destroy` 的调用面 = 查询全部结束之后；不同 client 互相独立。
- 参数有效域：ra∈[0,360)、dec∈[-90,90]、radius≥0、mag_low≤mag_high、参数
 有限（NaN/Inf 输入不显式校验，行为未定义——前置条件，负面测试覆盖）；
 `query_spectrum_by_coords` 的 `match_radius_arcsec` 单位角秒，其余半径均为度。
- 输出面边界：`GaiaStar.parallax/pmra/pmdec` 不写入（保持调用方缓冲初值）；
 `source_id` 恒 0；空数据目录在 Windows 返回 NULL、在 POSIX 返回
 file_count=0 的空 client；单文件结果上限 200000（超出即截断）；本模块不设
 取消检查点，取消由调用方在查询粒度实现。
- 调用方契约：`sources` 与 `out` 缓冲不得重叠（重叠为未定义行为）。
- plan/execute/cancel/inspect 编排语义见 ALG-GAIA-001 （acsd.catalog.gaia）。

## astro_calibration C API（API-CAL-001）

> SRC: lib/algorithms/calibration/src/（CMake acsd_calibration，4 个 cpp）；
> SCI: SCI-CAL-001；ALG: ALG-CAL-001..004；DATA: DATA-P1-CAL（DATA_SEMANTICS ）。
> 编排级合同（p1_session 五段式）见 API-P1-001；本节冻结现状模块级 C API。

- 导出符号（12 个函数 + 2 工具，全部当前真实存在，`AC_API` 导出）：
 `ac_generate_master_bias`、`ac_generate_master_dark`、`ac_generate_master_flat`、
 `ac_calibrate_frame`、`ac_correct_frame`、`ac_generate_master_bias_f64`、
 `ac_generate_master_dark_f64`、`ac_generate_master_flat_f64`、
 `ac_calibrate_frame_f64`、`ac_correct_frame_f64`、`ac_set_num_threads`、
 `ac_version`。
- 返回码（10 个科学函数）：`AC_OK=0` 成功；`AC_ERR_PARAM=-1` 空指针或
 n_frames/width/height 非正；`AC_ERR_MEMORY=-2`/`AC_ERR_INTERNAL=-3`
 在枚举中定义但当前实现不返回（实现不含异常屏障）。`ac_version` 返回
 静态串，释放责任方 = 库自身；`ac_set_num_threads` 无返回值。
- 单位/dtype/shape：全 ADU；`[h][w]` 行主序 0-based（stack 为
 `[n_frames][h][w]`）；f32 ABI float32、f64 ABI double；掩码 1=坏点。
 NULL 语义：master_bias/dark/flat 可空（条件分支见 DATA_SEMANTICS ）。
- 线程安全：全部函数 reentrant、threadsafe（无共享可变全局；
 API-P1-001 「request」一节 登记一致）；内部 OpenMP 并行（默认 team）。
 **例外**：`ac_set_num_threads` 进程级改写 OpenMP ICV，并发调用竞态且
 影响其他模块的并行度（并行度由 host ThreadLease 统一提供，本函数
 在新代码中的调用面 = 空）。
- FP64 ABI 语义：仅 `ac_calibrate_frame_f64` 真双精度（像素算术 double）；
 4 个 `ac_generate_master_*_f64`/`ac_correct_frame_f64` 内部降级 float32
 执行（统计/mask 路径，头文件声明），除接口 dtype 外不提供额外精度。
- 所有权：全部缓冲调用方分配/释放（模块零 malloc 输出）；无句柄/生命周期
 对象（无 create/destroy）；日志写 stderr（master 生成与 photometry 通道），
 不影响返回码。
- 取消：模块内无取消检查点；取消由调用方（phase1_session）
 在帧粒度实现。
- 行为边界：`generate_master_flat` 对负 median 不做防护；`extern "C"`
 边界不含异常屏障（`bad_alloc` 可穿越）；bilinear 通道按 IDW 计算；NaN 不
 进 cosmetic 统计；`w·h` 的 int31 溢出不做防护；
 `AC_METHOD_BILINEAR`/combine 等越界 enum 值不报错、走实现默认分支。
 完整行为清单见 ALG-CAL 文档 。
- 并行通道（不在本合同）：Makefile 产物 cosmetic_corrector.dll 的
 `cc_correct_median/cc_detect_hot/cc_detect_cold/cc_last_error`
 （window 奇数 3..15，Python ctypes 专用）与 `ac::optimize_dark_k`、
 `calibration::apply_photometry`（**已编译且生产已接线**：Phase1 photometry 节点同一步内施加到像素）。
- plan/execute/cancel/inspect 编排语义见 ALG-CAL-003 文档 （acsd.p1.calibration / acsd_p1_calibration.dll）。

## cosmetic C API（API-COS-001）

> ac_correct_frame :97-103、ac_correct_frame_f64 :142-148）
> SRC: lib/algorithms/calibration/src/cosmetic_corrector.cpp + ac_api.cpp（CMake
> acsd_calibration）；SCI: SCI-CAL-001；ALG: ALG-COS-001..005；
> DATA: DATA-P1-COS（DATA_SEMANTICS ）；MOD: acsd.p1.cosmetic
> （目标 DLL = acsd_p1_cosmetic.dll）。
> 与 API-CAL-001 的关系：两合同共享同一头文件与编译目标，本节只冻结
> cosmetic 路径 3 个符号的语义（模块级合同视角独立）；编排级合同见
> API-P1-002（PUBLIC_API.md 「request」一节，多模块共享）。

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
 在枚举中定义但当前实现不返回（无 extern "C" 异常屏障，bad_alloc 可穿越
 C ABI）。
- 调用时序与所有权：无句柄对象；`out` 由调用方分配（w·h float32）；
 `data/master_dark/master_bias` 只读借用；`out_hot/out_cold` 可 NULL。
 data 与 out 内存重叠为未定义行为（不支持 in-place）。
- 单位/dtype/shape：全 ADU；`[h][w]` 行主序 0-based；f64 ABI 内部降级
 float32 执行（统计/mask/插值全程 f32）；
 method 0=median / 1=IDW（名义 bilinear，按 IDW 计算）；
 hot/cold_sigma<=0 或 master_dark/bias=NULL → 对应检测禁用（ALG-COS-004）。
 数据语义逐字段见 DATA_SEMANTICS （DATA-P1-COS）。
- 线程安全：reentrant、threadsafe（无共享可变全局；OpenMP 默认 team，
 进程级 ICV，并行度由 host ThreadLease 统一提供）；输出 bitwise
 与线程数无关。
- 取消：无取消检查点（API-P1-002 「request」一节 登记"取消点=无"；帧粒度取消由
 phase1_session 层实现）。
- 生产调用方：lib/phase1_session/p1_session.cpp:294-307（cosmetic stage，
 现传 master_dark/master_bias=nullptr → 检测全禁用、恒等 pass，
 不伪造有效覆盖）。
- 行为边界：NaN 不进检测统计（NaN 源帧检测结果全 false）；`w·h` 的
 int31 溢出不做防护；非 0 method 一律按 IDW；小帧按镜像边界语义处理。
 完整行为清单见 ALG-COS 文档 。
- 并行通道（不在本合同）：Makefile 产物 cosmetic_corrector.dll 的
 cc_correct_median/cc_detect_hot/cc_detect_cold/cc_last_error
 （局部窗口修复，公式与 ac_* 通道不同）。
- plan/execute/cancel/inspect 编排语义见 ALG-COS 文档 （acsd.p1.cosmetic / acsd_p1_cosmetic.dll）。

## On-disk 格式

- HiPS：**IVOA HiPS 1.0** 标准 + properties 修订 1.4（产品属性 `hips_version="1.4"`，不是标准号；signal/support/snr 产品，NESTED，512 tile）。
- UPM：`acsd-upm-v2` JSON（sparse）+ dense cache（checksum 校验）。
- Manifest：`manifest.json` / `diagnostics.json` / `controls_accept.json`。

详见 `PUBLIC_API.md`（API 机器单源清单，与 `check_api_contracts` 的
`PUBLIC_API.md` 一致；完整分类清单）。

## drizzle C API（API-DRZ-001）

> 签名只取该头；六导出 :42,62,70,130,139,140）
> SRC: lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp（CMake 静态库
> acsd_drizzle，CMakeLists.txt:701-711）；SCI: SCI-DRZ-001；
> ALG: ALG-DRZ-001；DATA: DATA-P1-DRZ（DATA_SEMANTICS ）；
> MOD: acsd.p1.drizzle（目标 DLL = acsd_p1_drizzle.dll）。编排级合同见 API-P1-007（PUBLIC_API.md，
> 区间 API-P1-001..010 声明，无独立小节）；生产调用方
> orchestrator.cpp:3256-3371 经函数指针调 hp_drizzle_run_hips。

- 导出符号（7 个，全部当前真实存在，`HP_DRIZZLE_API` extern "C"）：
 `hp_drizzle_fits_to_ahpx`（文件通道 FITS→legacy 单文件容器）、
 `hp_drizzle_run`（PipelineFrame 帧通道）、`hp_drizzle_run_hips`
 （帧通道 + HiPS 直写薄封装）、
 `hp_drizzle_run_phase1_hips`（**Phase1 正式末端**：帧通道直写标准 HiPS，
 与既有 writer 节点产物逐字节一致）、
 `hp_drizzle_reverse_run`（Sphere→Plane 反向）、
 `hp_drizzle_reverse_capability`、`hp_drizzle_reverse_version`。
- 签名（hp_drizzle_api.h :42-51,62-66,70-75,78-96,130-134,139-140）：
 `int hp_drizzle_run(PipelineFrame* frame, int nside, int nested,
 double pixfrac, const char* output_path, HpDrizzleResult* result,
 int precision_mode)`；hips 变体增加可选 legacy 容器路径参数
 （:70-75）；`int hp_drizzle_run_phase1_hips(PipelineFrame* frame, int nside,
 int nested, double pixfrac, const char* hips_dir,
 const char* filter_passband, HpDrizzleResult* result, int precision_mode)`
 （:78-96）；reverse: `int hp_drizzle_reverse_run(const
 HpReverseDrizzleInput* in, void* signal_out, void* coverage_out,
 HpReverseDrizzleResult* result)`。
- 返回码：0=成功，非 0=失败；实测语义——文件通道正值 1..12
 （1=null 参数、2=nside 非 2 幂、3=pixfrac 越界、4=读 FITS 失败、
 5=无 WCS、6/7=SNR 读/尺寸、8/9=权重读/尺寸、10=drizzle 失败、
 11=写 legacy 容器失败、12=C 边界内部异常，hp_drizzle_api.cpp:186-396
 `hp_drizzle_fits_to_ahpx`）；帧通道混用负值 -1..-8（参数/
 块校验）、-9=无 WCS（read_wcs_params_from_frame，hp_drizzle_api.cpp:427-447）、
 -12=HiPS dir 空（:1143-1145）、
 -13=直写失败（:1175-1177）——正负两套并存、无集中枚举。reverse 返回 1..6（+7=C 边界内部异常；hp_drizzle_api.cpp:46-163
 `hp_drizzle_reverse_run`）。
 锚唯一取 `lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp`（见本节 SRC 行）。
- 调用时序与所有权：无句柄对象；frame 及其块由调用方拥有（只读
 借用）；result 由调用方分配；reverse 的 signal_out/coverage_out
 由调用方分配（width×height，output_fp64 决定 double/float 视图）；
 HiPS 目录树由模块写入、编排层负责 overwrite 清理
 （orchestrator.cpp:3345-3354）。
- 单位/dtype/shape：data 块 [H][W] 行主序 ADU（f32/f64 二选一）；
 输出 tile 累加量语义见 DATA_SEMANTICS ；precision_mode
 0=FP32（默认）/1=FP64/-1=读 header "PRECISION" KV
 （hp_drizzle_api.h:60）；错误信息经 error_msg[512] 返回。
- 线程安全：单次 run 内 OpenMP 内部并行（config.threads/omp 默认，
 schedule(static)+按线程序合并，1/N 确定性，ALG-DRZ-001 「机器门」一节）；
 同进程多 run 并发经 per-run generation 原子递增隔离缓存
 （drizzle_engine.cpp:1659-1660）；ThreadLease 零命中。
- 取消：无取消检查点（模块内无 cancellation token；编排取消点=
 帧/tile 粒度为编排层合同）。
- stderr 约定：全部诊断/进度日志直写 stderr（[hp_drizzle_api]/
 [drizzle_engine]/[sink] 前缀），不污染 stdout；G4 trace 由 env
 ACSD_DRIZZLE_TRACE 控制（lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:39-330）。
- 接受面与诊断面边界：错误码值像素 NaN 经 `F_p` 传播、不掩膜
 （`docs/science/drizzle/DRIZZLE.md:116`；实现
 `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1899-1902`；回归
 `lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_tests_core.cpp:518-537`
 `p1drz_negative`）；shim 对非法 nside 容忍不抛。完整清单见 ALG-DRZ-001 。
- 并行通道（不在本合同）：模块 Makefile 产物 healpix_drizzle.dll
 （Python ctypes 专用，与 CMake 静态库同源码）。
- plan/execute/cancel/inspect 编排语义见 ALG-DRZ-001 与
 lib/algorithms/drizzle/module.yaml（acsd.p1.drizzle / acsd_p1_drizzle.dll，
 C ABI adapter 面见 lib/algorithms/drizzle/module.yaml）。

## HiPS writer C API（API-HIPS-001）

> 他版；九导出 :104,121,130,135,144,149,152,163,177，AIO_HIPS_EXPORT
> extern "C" :24-30）
> SRC: lib/infrastructure/aio/src/hips/aio_hips_writer.cpp（CMake 静态库
> acsd_hips，CMakeLists.txt:599-618）；SCI: SCI-DRZ-001；ALG:
> ALG-HIPS-001..005；DATA: DATA-P1-HIPS（DATA_SEMANTICS ）；MOD:
> acsd.p1.hips_writer（目标 DLL = acsd_p1_hips_writer.dll）。编排级无独立 hips stage——经 API-P1-007
> hp_drizzle_run_hips 由 astro_sphere_sink（astro_sphere_sink.cpp:97，
> P1-DRZ 链）与 lib/algorithms/coverage/tools/stage2.cpp:592 间接调用；Phase2 读侧
> 为 aio_hips_reader（SCI-P3-001 链，不属本合同）。

- 导出符号（9 个，全部当前真实存在，`AIO_HIPS_EXPORT` extern "C"）：
 `aio_hips_product_begin`（创建产品集句柄）、
 `aio_hips_write_signal_support_tile`、`aio_hips_write_variance_tile`
 （逐叶 tile 流式写，AstroSphereTileView 直供）、`aio_hips_write_snr_points`
 （SNR 控制点累计缓存，finalize 时落 TSV tile）、
 `aio_hips_set_drizzle_provenance`（Phase2 k_corr 选择键）、
 `aio_hips_finalize`（properties/MOC/hierarchy/manifest 收尾+释放）、
 `aio_hips_abort`（释放句柄，不删文件）、
 `aio_hips_write`（legacy 兼容批量入口，legacy 容器中转验证用，support uint8
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
 枚举（负码与正码两套并存）。错误文本经 aio_hips_last_error
 （每次入口 clear，跨调用不可追溯）。
- 调用时序与所有权：begin →（零或多次）write_* / write_snr_points →
 finalize（成功路径内部 delete ps）或 abort（仅 delete ps，**不删除
 已写文件**；`aio_hips.h:151` 的注释与实现不符，残留处置归调用方/
 发布层）。ps 句柄调用方
 持有至 finalize/abort；view 及其数组调用方拥有、调用期间只读借用
 （同步消费，无拷贝）；SNR 点数组调用后即可释放（内部拷贝缓存）。
- 单位/dtype/shape：见 DATA-P1-HIPS（DATA_SEMANTICS ）——
 flux_sum ADU、covered_area sr、[512×512] NESTED local 行主序、
 data_type 0=f32/1=f64 一次固化；products flags 位域
 SIGNAL=1/SUPPORT=2/SNR=4/VARIANCE=8/IVAR=16（ALL=7/ALL_V19=31，
 aio_hips.h:34-43）。
- 线程安全：单句柄非线程安全（成员 scratch 缓冲与 moc_cells/hier/
 leaf_ipix_list 无锁累积）；同进程 CFITSIO 裸调未包装
 aio::cfitsio_mutex（同库 aio_fits/aio_hips_reader 均有包装）；生产链为
 drizzle 合并后单线程串行调用
 （astro_sphere_sink.cpp:97）。threading_model=host_executor_lease 为
 目标 DLL 合同值；ThreadLease 由 host 侧提供。
- 取消：无取消检查点（finalize 长收尾不可中断；模块内无
 cancellation token；编排取消点=帧/tile 粒度为编排层合同）。
- stderr 约定：诊断/六段 profile 计时直写 stderr（[hips] 前缀），
 stdout 不用（writer :368-373,:1030-1076）。
- 写入面边界：abort 只释放句柄、不清理已写文件；发布序为
 remove+create 直写、无原子发布，manifest 无 COMPLETE 标志（对齐边界 =
 DATA_SEMANTICS ）；estsize/fov 为固定值；`moc_order` 越界按静默钳位
 处理；CFITSIO 调用不持进程锁；错误码负码/正码两套并存；hierarchy 以 f32
 累加；`fits_str` 超长即截断。完整清单见 ALG-HIPS-001 。
- plan/execute/cancel/inspect 编排语义见 ALG-HIPS-001 与
 lib/algorithms/drizzle/hips/module.yaml（acsd.p1.hips_writer /
 acsd_p1_hips_writer.dll）。

## SNR/Noise C API（API-NOISE-001）

> 其余来源一律不作签名面；SNR_API extern "C" 导出，_WIN32 下 __declspec(dllexport) :7-11）
> SRC: lib/algorithms/noise_snr/cpp/src/noise_model.cpp（现状构建=cpp/Makefile:5,12
> g++ -shared → snr_estimator.dll + cpp/build.ps1:29，未编入根 CMake 主
> 构建；dll_loader.cpp:59/73 加载名与路径吻合）；SCI: SCI-NOISE-001..015；
> ALG: ALG-NOISE-001..003（NOISE_ESTIMATION 逐符号锚）；DATA:
> DATA-P1-NOISE（DATA_SEMANTICS ）；MOD: acsd.p1.noise-snr（目标 DLL = acsd_p1_noise.dll）。编排级合同见
> API-P1-006（PUBLIC_API.md 「request」一节，多模块共享）；本节只冻结 noise 路径
> 9 个导出符号的语义。
>
> **范围界定**：本节只登记 NoiseWeightModelV1 生产链 + 诊断函数
> （ALG-NOISE-001..003）；同头三层模型其余符号
> snr_phot_cal_quality/snr_psf_fit_quality（测光/PSF 质量，P1-PHOT/PSF
> 合同视角）与乘法 SNR 通道 snr_estimate*/snr_extract_model*（legacy
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
 泄漏**；free 幂等（nullptr 安全）。
- 单位/dtype/shape：data ADU [h·w] 行主序（v1 float32 / f64 float64）；
 source_mask float32 [h·w]（≠0=源）；star 坐标 double[n_stars] 0-based
 pixel；fill 输出 float32 [h·w]；variance ADU²、ivar ADU⁻²、σ ADU、
 gain e⁻/ADU、read_noise e⁻。逐字段语义见 DATA_SEMANTICS （DATA-P1-NOISE）。
- 线程安全：reentrant=yes、threadsafe=yes **以 model 对象隔离为前提**
 （PUBLIC_API.md 「request」一节 口径）——唯一共享可变状态 g_model_floor 为进程级
 无锁 unordered_map，并发 build/free 无保护；fill 只读模型 +
 g_model_floor 查询。输出 bitwise 与线程数无关（实现为单线程，
 noise_model.cpp:137-296,408-470）。
- 取消：模块内无取消检查点（noise_model_impl/fill_impl 无 cancel 回调）；
 调用方按行带粒度设置取消点。
- 生产调用方：lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:4177（stage6 SNR，
 必需 stage）→ dll_loader_ 函数指针 snr_noise_model_v1/_f64/
 _default_config/_fill/_free（orchestrator.cpp:4242-4250）；DLL 装载
 snr_estimator.dll（dll_loader.cpp:59，lib/algorithms/noise_snr/cpp/ :73）。
- 行为边界：模型指针地址可被复用（ABA 式复用），并发 build/free 无保护；
 build 与 fill 对 floor 的处理不一致；`SnrNoiseModelConfig` 的 gain 三字段
 不参与计算；`snr_noise_scale_law` 不做参数校验；掩膜通道互斥；超范围
 参数按静默钳位处理；空 patch 计数与有效 patch 计数混同。完整清单见
 NOISE_ESTIMATION 。
- plan/execute/cancel/inspect 编排语义见 NOISE_ESTIMATION 与
 lib/algorithms/noise_snr/module.yaml（acsd.p1.noise-snr /
 acsd_p1_noise.dll）。

## Photometric C API（API-PHOT-001）

> 权威签名头，PC_API :7-11 `extern "C"` 不抛异常）
> SRC: lib/algorithms/photometry/cpp/src/pc_api.cpp
> SCI: SCI-PHOT-001（docs/science/PHOTOMETRY.md，FROZEN，共享引用不改动）
> ALG: ALG-PHOT-001..002（docs/science/algorithms/PHOTOMETRIC_FIT.md，逐符号锚）
> DATA: DATA-P1-PHOT（DATA_SEMANTICS ）；编排级合同 API-P1-005
> （PUBLIC_API.md，descriptor 引用，与本节并行不互斥）
> MOD: MOD-acsd-phase1-photometry（module.yaml CONTRACT_READY，
> 注册面尚未登记 entrypoint；生产调用 orchestrator.cpp:2474 run_stage_photometric）

### 范围界定

帧级测光定标 C ABI：合成测光 F_syn（XPSD uint8 解码 + Akima/Simpson 1.0nm
积分）、Gaia TAN+SIP 投影、双向最近邻唯一配对（KD-tree，2.0px）、星等预
过滤 + IRLS/Tukey 稳健零点（scale=10^(−location)，sigma_residual=dex）、
逐星 PcMatchRecord、I_cal=I·scale。不做逐像素 ivar；帧级 QA 换算
（sigma_mag/sigma_cal_rel）归 snr_estimator snr_phot_cal_quality
（API-NOISE-001 范围界定）。无取消检查点。

### 导出符号（photometric_calib.h 实测行号锚）

| 符号 | 头锚 | 摘要 |
|---|---|---|
| pc_calibrate_simple | :103-117 | 直通版：F_syn 由调用方传入（gaia_fsyn），QE 三参数不参与计算 |
| pc_calibrate_simple_with_gaia | :153-183 | DLL 内锥形搜索+积分（直通封装） |
| pc_calibrate_simple_f64 | :185-199 | FP64 直通版 |
| pc_calibrate_simple_with_gaia_f64 | :201-225 | FP64 with-gaia 封装 |
| pc_calibrate_simple_with_gaia_v2 | :227-245 | per-star PcMatchRecord（生产主路径，float32 像素） |
| pc_calibrate_simple_with_gaia_f64_v2 | :247-265 | per-star（float64 像素） |

结构体：PhotometricDiag（:21-45，17 字段分阶段诊断）、PcMatchRecord
（:47-59，status 0/1/2/3 + reject_reason 0..6）。

### 签名要点与内存所有权

- gaia_client_handle 为 opaque borrow（调用方经 gaia_client.dll 创建/销毁，
 dll_loader.cpp:289-299 预加载）；out_pixels/out_scale_factor/
 out_sigma_residual/out_n_matched/out_diag/out_records 均调用方分配；
 spec_stars/spectra_buf 为 DLL 内 malloc 的锥搜结果，本调用内 free。
- 所有出参可 NULL 向后兼容（头 :17 注释）；records 需 n_psf≥1 才有意义。

### 返回码（含退化语义）

- 0=成功，**含退化恒等校正**（无 Gaia 星 :72-98 / 无 PSF 星 :808-830 /
 锥搜无光谱星 :868-890 / 滤光片预处理失败 :911-923 → scale=1.0、
 n_matched=0、sigma_residual=0；调用方须以 out_n_matched/out_diag 判据，
 完成定标的判据 = out_n_matched/out_diag（rc=0 只表退化恒等校正）。
- −1=空指针/宽高非正/参数非法（`pc_calibrate_simple` 的 −2/−3 为 dims 校验码，
 退化路径返回 0；头注释与实现的差异见 README 「机器门」一节）。
- −2=gaia_client_handle 为空（with-gaia 系）。
- −3=锥形搜索失败（gaia_client rc≠0，pc_api.cpp:836-866）。

### 单位/dtype/shape

见 DATA_SEMANTICS （唯一权威）：pixels f32(v2)/f64(f64_v2) `[h·w]`
ADU；`out_pixels` 未定标/退化=ADU，已定标（scale≠1 且 n_matched>0）=
模型通带积分辐照度（F_syn 单位，`I_cal=I·scale`）；`scale` 单位 [F_syn 单位]/ADU
（=10^(−location)，`location` 单位 dex(ADU/[F_syn 单位])，F_syn 单位 W·m⁻²·nm；
量纲见 DATA_SEMANTICS ）；sigma_residual dex；records residual=dex；WCS deg/px；
光谱 uint8 编码 F(λ)=byte·flux_mul+flux_min（W·m⁻²·nm⁻¹）。

### 线程安全与确定性

- 无跨调用共享可变状态（模块级单例无）；调用内 OpenMP F_syn
 schedule(dynamic,64) 逐星独立 + 像素 static 逐元素（star_matcher 单线程）
 ——reentrant，并发调用安全；线程数 omp_get_max_threads 未接
 ThreadBudget（module.yaml threading_model=host_executor_lease
 为合同值）。
- determinism=fixed_reduction_order：输出 bitwise 与线程数无关（README 「验收」一节）。

### 生产调用方与编排现状

- orchestrator.cpp:2474 run_stage_photometric（必需 stage，DLL 未加载退出
 码 2）→ 函数指针 pc_calibrate_simple_with_gaia_f64_v2（:2714）/_v2
 （:2790）双通道；写 photo_stats KV 块（:2902-2935，N_MATCHED/
 SCALE_FACTOR/SIGMA_RESIDUAL + diag 17 字段）。
- registry descriptor 占位词汇（module_adapters.cpp:531-547，sci_id=
 SCI-P1-PHOT-001/alg_id=ALG-002/data_id=DATA-P1-FLUX/api_id=API-P1-005/
 test_id=TEST-P1-PHOT-001）不作冻结依据；冻结依据只取上游权威文档。

### 行为边界与编排语义

- 行为边界：`computeScale` 无调用点；入参越界按静默钳位处理；
 `rejected_quality@@ 与有效样本计数混计；直通与 with-gaia 两套入口并存；
 帧级 QA 换算（sigma_mag/sigma_cal_rel）不在本模块内。完整清单见
 PHOTOMETRIC_FIT 。
- plan/execute/cancel/inspect 编排语义见 PHOTOMETRIC_FIT 与
 lib/algorithms/photometry/module.yaml（acsd.p1.photometry /
 acsd_p1_photometry.dll）。

## PSF 拟合 C API（API-PSF-001）

> DPSF_EXPORT extern "C"（_WIN32 下 __declspec(dllexport) :8，否则
> __attribute__((visibility("default"))) :10））
> SRC: lib/algorithms/psf/src/dpsf_psf.cpp（1432 行）
> SCI: SCI-P1-PSF-001；ALG: ALG-STARPSF-001
> （STAR_PSF_ALGORITHMS 逐符号锚）；DATA: DATA-P1-PSF
> （DATA_SEMANTICS ，双 [N,9] 布局权威）；编排级合同 API-P1-003
> （PUBLIC_API.md 「request」一节，descriptor 引用，与本节并行不互斥）
> MOD: MOD-acsd-phase1-star-psf（module.yaml CONTRACT_READY，
> 注册面尚未登记 entrypoint；生产调用 orchestrator.cpp:2067 run_stage_psf）

### 范围界定

Phase1 单帧逐星 Moffat4（β=4 固定）PSF 拟合 C ABI：uint16/float32/float64
三通道图像输入，star_det v1 `FLOAT64[N,6]` 检测坐标消费，7 参数 LM
（B,A,x0,y0,sx,sy,theta），4 状态码失败语义（STAR_PSF_ALGORITHMS ）。
不做星检测（禁重检测，orchestrator.cpp:1754-1756）、不做饱和剔除决策
（star_det v1 [4]/[5] 不消费）、不做 QA 换算（帧级 PSF 质量归
snr_estimator snr_psf_fit_quality，snr_estimator.h:110，API-NOISE-001
范围界定）。无取消检查点。

### 导出符号（dynamic_psf.h 实测行号锚，7 个全部当前真实存在）

| 符号 | 头锚 | 定义锚 | 摘要 |
|---|---|---|---|
| dpsf_fit | :44-47 | dpsf_psf.cpp:428 | uint16 单星拟合，DPSFFitResult 输出 |
| dpsf_fit_batch | :49-52 | dpsf_psf.cpp:482 | uint16 批量（逐星 float patch），DPSFFitResult*[] |
| dpsf_fit_batch_f | :59-63 | dpsf_psf.cpp:580 | float32 图 + (cx[],cy[])，DPSFFitResult*[]（FP32 生产通道） |
| dpsf_free_results | :64 | dpsf_psf.cpp:983 | 释放批量 DPSFFitResult 数组 |
| dpsf_fit_batch_f32 | :122-131 | dpsf_psf.cpp:713 | float32 图 + star_det v1 → out_psf_params[N,9]（compact）+ 可选 out_status[N] |
| dpsf_fit_batch_f64 | :160-169 | dpsf_psf.cpp:968 | float64 图 + star_det v1 → [N,9]（compact；moffat4_fit_d 不降级）+ 可选 out_status[N] |
| dpsf_fit_batch_d | :181-188 | dpsf_psf.cpp:612 | float64 图 + (cx[],cy[])，DPSFFitResult*[]（FP64 生产通道） |

schema 宏：`DPSF_STAR_DET_SCHEMA_V1="star_det_v1:FLOAT64[N,6]"`（:107）、
`DPSF_PSF_PARAMS_SCHEMA="psf_params:FLOAT64[N,9]"`（:108）、
`DPSF_PSF_STATUS_SCHEMA="psf_status:INT32[N]"`（:114，B2-A2 新增）。结构体：
DPSFFitResult 12 字段（:17-31）、DPSFFitParams{fitRadius,maxIter,tolerance}
（:38-42）。错误码 DPSF_FIT_OK/NO_CONVERGENCE/INVALID_PARAMS/ITERATION_LIMIT
=0/1/2/3（:33-36，语义冻结见 STAR_PSF_ALGORITHMS ）。

### 签名要点与内存所有权

- dpsf_fit_batch/_f/_d：`*out_results` 为 DLL 内 malloc 数组，调用方
 `dpsf_free_results` 释放（:599）；失败（rc≠0）调用方仍须对非 NULL
 results 释放（orchestrator.cpp:2350-2352 先 free 再返回）。
- dpsf_fit_batch_f32/_f64：out_psf_params 由调用方预分配（N·9·sizeof(double)），
 out_n_valid 由 DLL 写；params 可 NULL（默认 fitRadius=8/maxIter=200/
 tolerance=1e-8，:717-719/:845-847；maxIter/tolerance 不参与迭代判定）。
- **B2-A2（RESCUE-P0-05）**: out_status 可选（可 NULL），大小 N·sizeof(int)，
 按**检测下标**报告逐星真值（`DPSF_PSF_STATUS_OK`=0 成功；1=拟合失败/未收敛；
 2=空 rect 未拟合；3=patch 分配失败）。out_psf_params 的成功行按检测下标升序
 **compact** 写入 0..n_valid−1；失败星不占参数行。调用方必须用 out_status 做
 星 ID↔行映射，行下标一律取自 out_status 的 `i < n_valid` 前缀（越界行不参与映射）。
 传 NULL 时逐星状态不可得（compact 布局不变），属调用方 ABI 兼容面。
- 全部接口不抛异常（C ABI）；批接口逐星失败**经 out_status 显式报告**、
 不计 valid（逐星状态经 out_status 出批），批级 rc∈{0,−1}。

### 返回码

- 单星 dpsf_fit/moffat4_fit*：0/1/2/3 四码（表：触发锚、输出副作用、
 ITERATION_LIMIT 仍回填当前最优参数 :391-403）。
- 批接口：0=批量完成（逐星成败看 out_status 或 status 列，不要求全成）；
 −1=参数非法（空指针/尺寸非法/计数≤0，:700-707；此时不触碰任何输出缓冲）。

### 单位/dtype/shape

见 DATA_SEMANTICS （唯一权威）：image `[h·w]` ADU 行主序；
cx/cy/fitRadius/sx/sy/fwhm 像素；theta 弧度；B/A/flux/mad ADU
（flux=2π·A·sx·sy/3 Moffat4 解析积分）；eccentricity 无量纲 [0,1)；
布局 A（编排 psf 块 status,B,flux,cx,cy,fwhm,A,mad,eccentricity）与
布局 B（psf_params B,A,cx,cy,sx,sy,theta,fwhm_x,fwhm_y）并存，两者各自具名、按布局 A/B 分派。

### 线程安全与确定性

- 批拟合 OpenMP `parallel for schedule(dynamic) reduction(+:success_count)`
 4 处（dpsf_psf.cpp:528,635,738,876）：逐星独立、输出按索引写、计数
 reduction 与星序无关 → reentrant、并发调用安全、输出 bitwise 与线程数
 无关（determinism=fixed_reduction_order）。线程数未接 ThreadBudget
 （module.yaml threading_model=host_executor_lease 为合同值；ThreadLease 与
 取消检查点由 host 侧提供）。

### 生产调用方与编排现状

- orchestrator.cpp:2067 run_stage_psf（必需 stage，DLL 未加载退出码 2，
 :2071-2075；frame_ 为空=内部错误）→ 函数指针 dpsf_fit_batch_d（:2304，
 FP64 通道）/ dpsf_fit_batch_f（:2327，FP32 通道）+ dpsf_free_results
 （:2290）；编排参数 stage1_cfg psf.fit_radius/max_iterations/tolerance
 （:2277-2286，max_iterations/tolerance 模块侧不生效）。
- 产出：psf 块 FLOAT64 [N,9]（布局 A，:2376-2390）+ star_measurements
 权威块 [N,15]（DATA_SEMANTICS 附属产出）；PHOTOMETRIC 以 psf 为
 必需块消费（:2563-2570，缺失退出码 3）。
- registry descriptor 占位 ID（module_adapters.cpp:492-510，sci_id=
 SCI-P1-PSF-001/alg_id=ALG-002/data_id=DATA-P1-SOURCES/api_id=API-P1-003/
 test_id=TEST-P1-PSF-001）为本合同登记面；冻结依据只取上游
 依据。
- 现状构建 lib/algorithms/psf/Makefile:3-5 → dynamic_psf.dll；dll_loader.cpp:57
 （ModuleId::PSF→dynamic_psf.dll）/:71（lib/algorithms/psf/）；未编入根 CMake
 主构建——目标 DLL = acsd_p1_psf.dll。

### 行为边界与编排语义

- 行为边界：拟合参数序与常数耦合；前向差分求导并对步长硬钳位；
 `maxIter`/`tolerance` 不参与迭代判定；模块内无取消检查点；
 不输出协方差；批处理失败按静默返回处理。完整清单见 STAR_PSF_ALGORITHMS 。
- plan/execute/cancel/inspect 编排语义见 module.yaml（acsd.p1.psf /
 acsd_p1_psf.dll）；验收面设计见 TEST-PSF-DESIGN-001。

## Phase1 装配会话 C API（API-P1-SESSION）

> 权威签名头 lib/phase1_session/p1_session.h:16-37，签名只取该头）。
> 编排级上游合同 API-P1-001（api/PUBLIC_API.md FROZEN）；数据面
> DATA-P1-SESSION（DATA_SEMANTICS ）。registry 关系：五函数经 P1Api
> （lib/infrastructure/scheduler/src/module_adapters.cpp:755-762）被 8 个 Phase1 descriptor
> 工厂委托（:728-735/:755-770）。

### 范围界定

本节只登记装配会话入口符号与调用时序；不定义任何算法（校准/cosmetic
算法委托 API-CAL-001 / API-COS-001 既有符号，其余阶段执行现状见
"生产调用方与编排现状"）。不含 Phase2/3 接口。

### 导出符号（p1_session.cpp 实测行号锚，5 C API + 1 C++ 诊断全部当前真实存在）

| 符号 | 锚 | 语义 |
|---|---|---|
| `p1_session_create` | p1_session.h:16 / p1_session.cpp:159 | host services 单结构注入（struct_size/ABI 校验 :91-93）；opaque handle，owner=创建者 |
| `p1_session_validate` | p1_session.h:19 / p1_session.cpp:172 | 纯读无 IO 幂等；config 键集校验（DATA-P1-SESSION ）；缺必需键/类型错→PARAM，无 silent default |
| `p1_session_run` | p1_session.h:23 / p1_session.cpp:238 | 四段执行 io_read→calibrate→cosmetic→io_write（:168/:195/:283/:319）；async_io_depth∈{0,1,2}（:152） |
| `p1_session_inspect` | p1_session.h:26 / p1_session.cpp:547 | manifest JSON（dump(2)）；out=host alloc，调用方经 host free 释放（:349-357） |
| `p1_session_destroy` | p1_session.h:28 / p1_session.cpp:566 | 唯一释放对（delete SessionState） |
| `acsd::phase1::last_error` | p1_session.h:36 / p1_session.cpp:576 | C++ 诊断：脱敏摘要，handle 空→空串；非科学接口 |

### 签名要点与内存所有权

- handle：`acsd_handle` opaque；生命周期=唯一 create/destroy 对。
- config：`acsd_span_u8`（调用方内存，会话内解析为 JSON，不持久持有）。
- inspect 输出：`acsd_span_u8` host allocator 分配（16 对齐，:351），
 调用方必须 host free；内容 UTF-8 JSON 文本（DATA-P1-SESSION ）。
- host services：`acsd_host_services_v1`（common_abi_v1.h:110-117）
 allocator/logger/cancel/budget 四通道；budget.max_workers 注入
 `ac_set_num_threads`（p1_session.cpp:162-165）。

### 调用时序

create →（可选）validate → run → inspect → destroy；validate 可独立
调用（幂等）；run 前 config 必先 validate（run 内二次解析失败仍 PARAM，
:155-159）；destroy 后 handle 一律失效；inspect 于 run 前调用返回
created 状态 manifest（:342-343）。B 线 registry 通道经 SessionModule
等价执行（execute 内 create→validate→run→inspect 捕获→destroy，
module_adapters.cpp:153-216）。

### 返回码

ACS_OK；ACS_ERR_ABI_MISMATCH（host 结构/ABI :91-93）；ACS_ERR_PARAM
（eng/packaging/config/键集/类型/尺寸匹配 :110-143/:218-221/:236-239/:152）；ACS_ERR_IO
（文件读写 :183-187/:233-235/:261-267/:306-310/:323-327）；ACS_ERR_INTERNAL
（ac_* 委托非 OK :249-253/:301-305）；ACS_ERR_NOMEM（:96/:351）；
ACS_ERR_CANCELLED（文件/帧粒度取消点 :177-181/:228-231/:289）。取消语义：
清理后短路返回，不留伪完整产物（p1_session.cpp:4 注释合同）。

### 单位/dtype/shape

见 DATA-P1-SESSION（DATA_SEMANTICS ）：像素 float32 ADU [h,w]；
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

- 生产调用方 = lib/infrastructure/scheduler/src/module_adapters.cpp。8 个 Phase1
 descriptor 各自委托唯一真实 operation（p1_nodes[]：calibrate→ac_calibrate_frame、
 cosmetic→ac_correct_frame、star-psf→StarDetector、wcs→WcsTan、
 photometry→Photometer、noise-snr→NoiseModel、drizzle→hp_drizzle_run、
 writer→aio_write_fits；子节点不调完整 phase_session_run）；P1Api/SessionModule
 为兼容面。CLI/测试之外无其他直接调用方（生产可达性锚 = 生产可达性判据与
 管线追踪判据）。
- star-psf 走 lib/algorithms/star_detection 生产检测（sdet C 头，StarDetector C++ 类
 为薄包装）+ lib/algorithms/psf `dpsf_fit_batch_f64`（Moffat4 FP64 批量 PSF 拟合，
 DATA-P1-PSF 携 psf_params:FLOAT64[N,9]）。
- wcs 走 lib/algorithms/platesolve ipv 求解链
 `ipv_solve_from_memory_with_callback_d`（sdet + gaia_client 句柄注入）；两平台同源、
 同一组生产 C API（源内无平台 stub）；缺求解参数时按 DATA 拒绝。
- writer：drizzle 节点经 `hp_drizzle_run_phase1_hips` 直写 `AstroSphereTileView` →
 `aio_hips_product_begin/write_signal_support_tile/finalize`（**IVOA HiPS 1.0** 标准、产品属性 `hips_version="1.4"`；512×512
 HiPS）；writer 节点只校验产物并落 p1_final.json。三域模块库 =
 acsd_p1_dpsf/acsd_p1_ipv/acsd_p1_sdet（CMakeLists.txt）。
- 段面：session 现行段序 = 4 段（CAL+COS）；六域真实节点委托由 B 线 registry 通道
 的 p1_nodes[] 承载，完整 7-stage 生产链在 A 线 orchestrator DLL 链（如
 PHOTOMETRIC 生产调用 orchestrator.cpp:2474→:2714/:2790）。

### 行为边界与注册面语义

- 行为边界：cosmetic master 实参为 nullptr 时检测禁用、恒等 pass
 （:295-296）；`dark_scale_factor` 键不经 validate 校验（run 阶段兜底 1.0，
 :225）；`async_io_depth` 预读不按 depth 启用（PUBLIC_API.md 「生命周期」一节 预留值）；
 输出仅 FITS。
- 注册面：assembly 层不设独立 DLL（MODULE_MIGRATION_MATRIX 无 P1-SESSION
 行）；registry descriptor 占位词汇（ALG-002/004/005、TEST-P1-*-001）不作
 冻结依据。验收锚 TEST-P1-SESSION-001 =
 eng/tests/unit/p1_ir_facade_test.cpp（facade/canonical 节点集/委托语义）。

## 星点检测 C API（API-STAR-001）

> P1-STAR，目标 DLL = acsd_p1_star_detection.dll；唯一权威签名头
> lib/algorithms/star_detection/include/star_detector.h:1-99，签名只取该头）。
> 编排级上游合同 API-P1-003（PUBLIC_API.md 「request」一节：一帧只做一次权威检测）；
> 数据面 DATA-P1-STAR（DATA_SEMANTICS ）；算法权威 ALG-STARDET-001
> （STAR_DETECTION_ALGORITHMS 逐符号锚）；SCI-P1-STAR-001。

### 范围界定

本节登记星点检测现行 6 导出符号 + SDetParams 合同
（sdet_detect_guided_ex_f64 为权威路径入口；sdet_detect/sdet_free_coords/
sdet_detect_debug/sdet_free_debug_maps 四条 CC 路径符号不在导出面内）；
不定义任何算法
（ALG-STARDET-001 权威）；不含 Phase2/3 接口。编排级合同 API-P1-003
引用本模块符号，生产调用点在「生产调用方与编排现状」小节。

### 导出符号（6 个 C API 全部真实存在；锚 = star_detector.h SDET_EXPORT 实测）

| 符号 | 锚 | 语义 |
|---|---|---|
| `sdet_create` | star_detector.h:31 / sdet_api.cpp:990 | handle 创建；params=NULL→默认（structureLayers=5/hotPixelFilterRadius=1/iterativeClipSigma=9.0/iterativeMaxRounds=5/medianFilterDetail=1/maxStars=2000/fitRadius=6/fwhmClipSigma=3.0/maxAxisRatio=2.0，:977-989）；生产实参 orchestrator.cpp:1593-1612（fitRadius=0=自动半径） |
| `sdet_destroy` | star_detector.h:32 / sdet_api.cpp:1020 | 唯一释放对 |
| `sdet_detect_ex` | star_detector.h:38-43 / lib/algorithms/star_detection/src/sdet_api.cpp:2305 | 生产 FP32 入口（uint16→float 转换后 impl<float>；10 数组输出 + extras） |
| `sdet_detect_ex_f64` | star_detector.h:47-52 / sdet_api.cpp:2330 | 生产 FP64 入口（全程 double 不降级，PREC-105；out_flux/out_mag 仍 float32 ABI 协议） |
| `sdet_free_detect_ex` | star_detector.h:54-56 / sdet_api.cpp:2344 | 10 数组唯一释放（extras 同组；释放单位 = 整组） |
| `sdet_detect_guided_ex_f64` | star_detector.h:84-93 / sdet_api.cpp:2281 | 星表引导检测权威路径入口（SDetGuidedStats 六计数；n_pred=0 返回 rc=0 + count=0） |

SDetParams 9 字段（star_detector.h:13-24）：maxStars/maxAxisRatio 生产路径
完整消费；fwhmClipSigma 与 structureLayers 等 5 字段不进生产路径；
fitRadius 仅驱动 auto 半径推导（ALG-STARDET-001 ）。

### 签名要点与内存所有权

- handle：opaque `StarDetectorHandle`（star_detector.h:26），owner=创建者；
 handle 级互斥使用（PUBLIC_API.md 「request」一节 表行 handle 级 no/no，无内部锁）。
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
→ sdet_free_detect_ex → sdet_destroy；destroy 后 handle 一律失效。
`sdet_detect`+`sdet_free_coords` 与 `sdet_detect_debug`+`sdet_free_debug_maps`
为独立释放族，与 detect_ex 族各自具名。

### 单位/dtype/shape

见 DATA-P1-STAR（DATA_SEMANTICS ）：x/y double pixel（0-based，像素
中心=索引+0.5）；flux float ADU（正常星=振幅 A）；mag float（NaN=无效）；
saturated/has_saturated int 0/1；图像输入 FP32 通道 uint16（
量化）/ FP64 通道 double；编排序列化 star_det FLOAT64 [N,6]（:2218-2246）。

### 线程安全与确定性

- handle 级互斥（单 handle 单线程）；内部并行=OpenMP（候选拟合 omp for
 dynamic + reduction，:2042-2044；dedup/sort 串行）；输出 bitwise 与线程数
 无关（ALG-STARDET-001 「逐条追溯」一节）；determinism=fixed_reduction_order
 （module.yaml）。ThreadBudget 与取消检查点接线面
 （ALG-STARDET-001 ）。

### 生产调用方与编排现状

- 唯一生产调用方=lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp：run_stage_psf
 （PSF/STAR_MEASURE 阶段权威检测，:2067；函数指针装载 :2149-2165；FP64/
 FP32 通道选择 :2172-2198；star_det 权威块写入 :2237-2246；缓冲释放
 :2466）；PLATESOLVE fallback 读 star_det 块并禁重检测（:1748-1755、
 :1826-1829）；sdet_create 参数构造 :1593-1612。
- API-P1-003（PUBLIC_API.md 「request」一节）表行 `sdet_create/destroy/detect/detect_ex`
 引用本模块符号；descriptor acsd.phase1.star-psf
 （module_adapters.cpp:492-510）为编排层词汇，
 不作冻结依据。

### 行为边界与注册面语义

- 行为边界：线程数取自实现内默认（未接 ThreadBudget），模块内无取消检查
 点。完整清单见 ALG-STARDET-001 。
- 注册面：目标 DLL = acsd_p1_star_detection.dll（adapter 由 P1-STAR 域建立）；
 registry descriptor 占位词汇不作冻结依据。验收面设计见
 TEST-STAR-DESIGN-001（ALG-STARDET-001 ）。

## WCS 求解 C API（API-WCS-001）

> 他版；IPV_API extern "C" 导出宏，238 行）
> SRC: lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp（809 行；内核
> ipv_solver/ipv_select/ipv_triangle/ipv_itertrans/ipv_robust_refine/
> ipv_wcs/ipv_sip 共 13821 行）
> SCI: SCI-WCS-001（docs/science/detection/ASTROMETRY.md，共享引用不改动）；
> ALG: ALG-WCS-001（PLATESOLVE.md 逐符号锚）；DATA: DATA-P1-WCS
> （DATA_SEMANTICS ，单位/dtype/shape/坐标契约唯一权威）；编排级
> 合同 API-P1-004（PUBLIC_API.md 「request」一节，descriptor 引用，与本节并行不互斥）
> MOD: MOD-acsd-phase1-wcs-platesolve（module.yaml CONTRACT_READY，
> 注册面尚未登记 entrypoint；生产调用 orchestrator.cpp:1758 run_stage_platesolve）

### 范围界定

Phase1 单帧天测 WCS 求解 C ABI：星点检测坐标（double [n,6]）+ Gaia DR3SP
参考星 → TAN+SIP 三角形匹配求解，IpvWcsResult POD 输出（CD/CRVAL/CRPIX/
SIP A/B/AP/BP/RMS/CTYPE）。不做星点检测（禁重检测，消费 PSF 产出
star_measurements 权威块，orchestrator.cpp:1748-1755）、不做图像重采样、
不做参考星缓存管理（gaia 句柄由调用方注入）。像素中心双契约（统一契约
index-is-center ↔ IPV 接口契约 center=index+0.5）由调用方显式 +0.5 桥接
（orchestrator.cpp:1867，DATA_SEMANTICS ）。无取消检查点。

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
 success 联合解读**（DATA_SEMANTICS ）。
- 失败-置信度语义（PLATESOLVE.md ）：CD det 退化坍缩（近似 CRPIX 的
 解）必须以 success=0 呈现，呈现面 = 失败；实现面
 wcs_tan.cpp:48-51/wcs_transform.cpp:39-49 同族情形按该呈现面处理。

### 单位/dtype/shape

见 DATA_SEMANTICS （唯一权威）：detections [n,6] double（det_x/det_y
为 +0.5 契约 pixel；flux ADU；mag mag；sat/has_sat 0/1）；ra0/dec0 deg；
focal_length_mm mm；pixel_size_um μm；输出 cd deg/pixel、crval deg
（ICRS/J2000）、crpix 1-based pixel、rms_arcsec/rms_px、SIP 无量纲
（AP[6]−=1、BP[1]−=1 约定，Y-down 符号已折入）。

### 线程安全与确定性

- 句柄级互斥使用（同一 solver 句柄的并发面 = 单线程）；gaia/detector
 句柄生存期由调用方保证。
- OpenMP 并行点：三角形投票（ipv_triangle.cpp:303/:347，线程局部矩阵 +
 整数归并 collapse(2) schedule(static)）、选星（ipv_select.cpp:810/:1123/
 :1412/:1756）——投票与拟合归并为整数/单线程浮点，输出 bitwise 与线程数
 无关（determinism=fixed_reduction_order，ALG-WCS-001 F5 冻结断言）。
 线程数未接 ThreadBudget（module.yaml
 threading_model=host_executor_lease 为合同值；ThreadLease 与取消检查点由
 host 侧提供）。

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
 acsd.phase1.wcs-platesolve、ports sources/wcs、sci_id=SCI-P1-WCS-001/
 alg_id=ALG-002/data_id=DATA-P1-WCS/api_id=API-P1-004/test_id=
 TEST-P1-WCS-001）为本合同登记面；冻结依据只取上游权威文档。
- 现状构建 lib/algorithms/platesolve/cpp/ipv/build.ps1:27 / Makefile:6（g++
 -fopenmp -O3 → ipv_solver.dll，MSYS2/MinGW）；未编入根 CMake 主构建
 （根 CMakeLists.txt 无 ipv 目标）——目标 DLL = acsd_p1_wcs.dll。

### 行为边界与编排语义

- 行为边界：CD 退化坍缩按 success=0 呈现；错误通道按单族返回码承载；
 双 SIP 拟合路径并存；AP/BP 失败按 success=0 呈现；模块内无取消检查点、
 线程数未接 ThreadBudget；三套 TAN 实现并存。完整清单见 PLATESOLVE.md 。
- plan/execute/cancel/inspect 编排语义见 module.yaml（acsd.p1.wcs /
 acsd_p1_wcs.dll）；验收面设计见 TEST-WCS-DESIGN-001。

## Coverage union C API（API-COV-001）

> 签名只取该头；P2_API 导出宏 :16-20 Windows dllexport/POSIX 默认可见）
> SRC: lib/algorithms/coverage/src/coverage.cpp（455 行；生产目标根 CMake
> acsd_phase2 静态库 CMakeLists.txt:338-346，独立 self-build
> lib/algorithms/coverage/CMakeLists.txt:42-46 phase2 STATIC；acsd_p2_coverage.dll
> 为目标 DLL 合同值，capability adapter 面见 module.yaml）
> SCI: SCI-UPM-001/SCI-INT-001/SCI-SCOPE-001（docs/science/ 共享 FROZEN
> 引用不改动，SCI 层声明=PHASE2_COVERAGE.md ）；ALG: ALG-COV-001
> （「权重对象的归属」一节/逐符号锚）；DATA: DATA-COV-001
> （DATA_SEMANTICS ，单位/dtype/shape/坐标契约唯一权威）；编排级
> 合同 API-P2-001（api/PUBLIC_API.md FROZEN 「生命周期」一节 所有权图/
> 「request」一节 并发五字段行 1/「显式拒绝清单」一节 错误码映射，与本节并行不互斥）；MOD:
> MOD-acsd-phase2-coverage（module.yaml CONTRACT_READY，
> 注册面尚未登记其 descriptor；编排消费 lib/phase2_session/p2_session.cpp:119-148）

### 范围界定

Phase2 输入发现/兼容校验/coverage union/target_order C ABI：N 个
Phase1 单帧 HiPS（signal 子目录）→ 逐帧 properties 兼容校验（hips_order/
tile_width=512/hips_version/hips_frame/obs_filter）+ union MOC（NESTED
父单元聚合）+ target_order=min(逐帧 order)。不做：逐帧重校准/PlateSolve/
PSF/Drizzle（coverage.h:6）、像素数据读取（只读 properties+Moc.fits）、
intersection/depth/missing-tiles 产品、任何科学权重
计算（合同红线：coverage 禁作隐式科学权重，PHASE2_COVERAGE.md 「验收」一节）。
无取消检查点（API-P2-001 「request」一节 行 1 取消点=无，阶段级取消由编排 session
阶段边界提供，p2_session.cpp:121）。

### 导出符号（coverage.h/coverage.cpp 实测行号锚，2 个全部当前真实存在）

| 符号 | 头锚 | 定义锚 | 摘要 |
|---|---|---|---|
| p2_coverage_build | coverage.h:52 | coverage.cpp:144 | 发现+校验+union MOC+target_order（两阶段容量协议） |
| p2_coverage_free | coverage.h:56 | coverage.cpp:233 | POD 清零（不释放堆，无所有权转移） |

内部链接符号（非导出，内部锚）: inspect_frame（coverage.cpp:59-140，
匿名 namespace :55-142）、parse_props（coverage.cpp:20-45，文件作用域
static）。

核心结构体: P2MocCell 2 字段（coverage.h:27-29，order/ipix 均uint64）；
P2HipsInputInfo 7 字段（:31-38，hips_path[1024]/frame_id[64]/
max_leaf_order/n_tiles/filter_passband[64]/frame_type[32]）；P2CoverageResult
7 字段（:40-48，n_inputs/inputs/n_union_cells/union_cells/target_order/
status/error[512]）。字段级单位/值域唯一权威=DATA_SEMANTICS 。

### 签名要点与内存所有权

- p2_coverage_build（coverage.h:52-54）: 入参
 `const char* const* hips_paths, uint64 n_inputs, P2CoverageResult* out`；
 返回 int rc（0=成功含 K=0 空 union / 1=失败，error[512] 载因）。
 两阶段协议: 第一次 `out->union_cells=NULL`（及 inputs=NULL）→ rc=0
 得 n_union_cells=K 容量；调用方分配 K 个 P2MocCell（及 n_inputs 个
 P2HipsInputInfo）后第二次调用回填数据（coverage.cpp:219-228；每次
 调用全量重扫，无缓存）。
- P2CoverageResult 及全部数组由调用方分配（coverage.h:42/:44 注释）；
 p2_coverage_free 仅 `memset(out,0,sizeof(*out))`（coverage.cpp:235）
 ——与 PUBLIC_API.md 「生命周期」一节 所有权图 Coverage 行（build 创建/调用方持有/
 p2_coverage_free 释放/下游只读借用，api/PUBLIC_API.md:15）
 一致；无 malloc/无异常跨界。
- 错误通道: rc=1 + out->error[512]（"no inputs"/"empty path at index
 %llu"/"missing hips_order"/"unsupported tile_width"/"missing
 hips_version"/"unsupported hips_frame"/"filter mismatch"/AIO
 last_error 透传 :63-64）；错误码编排映射归 API-P2-001 「显式拒绝清单」一节
 （ACS_ERR_PARAM/ACS_ERR_STATE）。
- "no inputs" 分支 rc=1 而 status=0 不一致（coverage.cpp:154-157；
 memset :151 先行、strncpy :155 后写，error 有效；status 保持 0）——调用方
 以 rc 为准。

### 返回码

- rc: 0=成功（含空 union K=0）；1=失败（「逐条追溯」一节 全部拒绝路径，error 载因）。
- status: 0=ok（:229）；错误路径=1（:168/:177/:190/:200），
 "no inputs" 分支例外（同上，判据由
 TEST-COV-DESIGN-001 F5 固化）。
- 并发合同（API-P2-001 「request」一节 行 1）: reentrant=yes / threadsafe=no
 （独立对象，无内部锁）/ internal_parallel=none（单线程，无
 OpenMP，bitwise 确定）/ 取消点=无 / TST-COV-*（TEST-P2-COV-001，
 设计=PHASE2_COVERAGE.md TEST-COV-DESIGN-001）。
- thread budget: coverage 阶段未单列预算（PUBLIC_API.md 「result」一节 预算分配
 冻结清单不含 coverage——阶段级取消/manifest 登记由 session 编排层
 承担，p2_session.cpp:119-148）；threading_model=host_executor_lease
 为 module.yaml 合同值，ThreadLease 由 host 侧提供
 （与重扫行为同域）。

### 单位/dtype/shape

见 DATA_SEMANTICS （唯一权威）：输入 hips_paths [n_inputs] 字符串
数组；输出 P2MocCell [K]（order/ipix 无量纲整数，NESTED 父单元索引
<12·4^order）、P2HipsInputInfo [n_inputs]（hips_path[1024]/
frame_id[64]/filter_passband[64]/frame_type[32] 字符串 + order/tiles
整数）、target_order int（=min 逐帧 hips_order）；全链路整数运算
bitwise 确定；坐标=HEALPix NESTED equatorial/ICRS（非 PIXEL——
registry descriptor 像素登记面）。

### 生产调用方与编排现状

- 编排 session: lib/phase2_session/p2_session.cpp:119-148 —— coverage
 为 Phase2 DAG 首阶段（阶段边界取消检查 :120，manifest 登记
 n_union_cells/target_order :146-147）；两阶段调用 :125/:138。
- 下游模块消费: sampler p2_sample_controls*（sampler.cpp:473/:1236/:1255/
 :1138）、stage2 正式入口（lib/algorithms/coverage/tools/stage2.cpp:189-200/
 :212-213）、registry
 descriptor acsd.phase2.coverage（module_adapters.cpp:623-638，
 端口 calibrated=DATA-P2-CAL/ADU/PIXEL→coverage=DATA-P2-COV/
 DIMENSIONLESS/PIXEL——出端口坐标登记以本 API/DATA 合同 NESTED 为准）。
- 现状构建: 根 CMakeLists.txt acsd_phase2 STATIC（:338-346，含
 src/coverage.cpp，无独立 DLL target）；lib/algorithms/coverage/CMakeLists.txt
 phase2 STATIC 兼容自测 target（:42-46）+ phase2_synthetic_gate
 （:77-79）；acsd_p2_coverage.dll 为目标 DLL 合同值（CMake 集成面）。
- 既有测试基线: Phase2Coverage.RealHipsUnion（synthetic_gate.cpp:3374）
 / FilterMismatchRejected（:3410）——依赖 Fatduck 本地路径，缺失时
 GTEST_SKIP（:3376/:3413）；合成 fixture 见 TEST-COV-DESIGN-001 F1
 （解除环境依赖）。

### 行为边界与编排语义

- 行为边界：无输入时 status 取值不统一；`frame_id` 取基名，同名不同目录
 存在唯一性风险；空 filter 按静默放行处理；intersection/depth/missing-tiles
 产品不在本面内（覆盖度几何 ≠ 权重因子；`geometric_reliability` 乘数恒 1.0）；
 `extern "C"` include 卫生面 + 两阶段全量重扫；模块内未接 ThreadLease。
 完整清单见 PHASE2_COVERAGE.md 。
- plan/execute/cancel/inspect 编排语义见 module.yaml（acsd.p2.coverage /
 acsd_p2_coverage.dll）；验收面设计见 TEST-COV-DESIGN-001。

## Phase2 mosaic write 公共消费面（API-P2-HIPS-001）

> 定位: Phase2 马赛克写出**当前无独立公共 C API**——生产入口=
> stage2 工具（lib/algorithms/coverage/tools/stage2.cpp main :112）；编排层经
> API-P2-001（PUBLIC_API.md phase session，p2_session）驱动，但
> p2_session 不执行 HiPS 写（hips_paths/output_dir 校验
> p2_session.cpp:81-92，coverage 两阶段调用 :125/:138；HiPS 写入仅
> 发生在 stage2 工具）。本节冻结的是 stage2 配置 schema 的公共消费
> 面 + 进程退出码 + diagnostics.json 键集。
> SRC: lib/algorithms/coverage/tools/stage2.cpp（生产工具 acsd-stage2，唯一
> 写入路径）；配置 schema 唯一权威签名源 lib/algorithms/coverage/include/astro/
> phase2/stage2_common.h:16-99（P2Stage2Config，完整字段集以头文件
> 为准，本节冻结公共关键字段消费面语义）；DATA: DATA-P2-HIPS
> （DATA_SEMANTICS ，单位/dtype/shape/序合同唯一权威）；复用库
> ABI: aio_hips_*（本文件既有 API-HIPS-001 节，P1 冻结面）——P2 作为
> 库消费者经 aio_hips_product_begin（stage2.cpp:592-596）等引用，
> **不重登记、不新增 C ABI**；ALG: ALG-P2-HIPS-001..004
> （docs/science/algorithms/PHASE2_MOSAIC_WRITE.md）；MOD:
> acsd.p2.hips_writer（目标 DLL = acsd_p2_hips_writer.dll）。

### 入口与调用方式（acsd-stage2）

- 用法（stage2.cpp:132）:
 `acsd-stage2 <stage2.json> [--cpu-workers N] [--io-workers N]
 [--gpu-route cpu|auto|cuda] [--deterministic 0|1]`
 （usage 行 :132；CON-002 CLI override 全局 worker 预算，
 覆盖 config execution block，:155-166）。CLI 科学参数的接受面 = 空
 （stage2.cpp:3 头注，唯一参数=stage2.json 路径 + 预算四选项）。
- 编排层关系: p2_session（API-P2-001，lib/phase2_session/
 p2_session.cpp）仅做配置校验（:81-92）与 coverage 阶段
 （:125/:138 两阶段容量协议），**不调用 HiPS 写出**；马赛克写
 属 stage2 工具职责，编排接入点为 descriptor
 acsd.phase2.write 端口表（lib/infrastructure/scheduler/src/module_adapters.cpp
 :677-694，DATA-P2-HIPS 编排层词汇注记）。

### 配置 schema 公共消费面（P2Stage2Config 关键字段，stage2_common.h:16-99）

| 字段 | 默认 | 域 | 语义 |
|---|---|---|---|
| hips | —（:19） | 路径数组 `[n_frames]` | 每帧 Phase1 HiPS 目录（读 signal/support；逆方差权重时加读 ivar） |
| target_order_spec / target_order | "auto" / −1（:20-21） | HEALPix order | auto=cov.target_order；显式值的上界 = 输入最高 order（stage2.cpp:203-205） |
| precision | 0（:50） | 0/1 | 0=float32 / 1=float64 输出（stage2.cpp:528） |
| memory_limit_mb | 24576（:51） | MB | CON-002 内存预算 |
| reject_method / reject_profile / reject_underdetermined_n | P2_REJECT_AUTO / "acsd_adaptive_pixel"（**生产默认**，自研）/ 2（:52-54；**工具链默认仍 "wbpp_2_9_1"（对照档），分歧已登记**） | 无量纲 | planning 层 rejection 解析（profile 版本化；自研档 `underdetermined_n` 默认 3） |
| reject_normalization | "acsd_median_center_v1"（:56） | 无量纲 | 判定工作域归一（mask 应用回原始值） |
| large_scale_enabled（+ min_structure_pixels/low_grow/high_grow） | false / 8 / 2 / 2（:60-63） | 无量纲 | acsd.large_scale_rejection.v1，默认关闭 |
| 键面 | — | — | 不含 `weight_mode` / `legacy_allow_weight_fallback` 键（`../../ACSD_DESIGN.md` 「数据对象」一节（数据对象）：全链没有「权重模式」这一可选概念）；ivar 产品缺失时按 rc=7 失败（stage2.cpp:565-578） |
| acr_route | —（键已删除） | 不是现行键 | 集成执行路由的选择键已退场：出现在 config 中即 fail-closed 拒绝，退役对象的拒绝面必须存活（不得静默忽略或静默取默认值），与 `weight_mode` / `legacy_allow_weight_fallback` 同型，见 `lib/algorithms/coverage/src/stage2_common.cpp` 的退役键拒绝面；集成执行路由唯一 = CPU，无可选路由 |
| out_hips | —（:97） | 路径 | 输出 HiPS 产品集根目录 |
| diagnostics | true（:98） | bool | true → 落 diagnostics.json（本节键集） |

单位/dtype/shape/序合同唯一权威 = DATA_SEMANTICS （DATA-P2-HIPS）；本表仅为消费面副本，冲突以 为准。

### `acsd-stage2` 工具返回值（工具局部，非 `acsd::ExitCode`）

> 进程退出码正本 = `../contracts/LOG_AND_ERROR.md` 「验收」一节（唯一源 =
> `lib/infrastructure/cli/exit_codes.h` 的 `acsd::ExitCode` 枚举）。本表**不是**那份表的
> 接口面副本：`acsd-stage2` 是诊断/工具二进制，未 include `exit_codes.h`、不使用
> `acsd::ExitCode`（`stage2.cpp:11-29` 的 include 面无该头；全文件无 `ExitCode` 引用），
> 其 `main`（`stage2.cpp:120`）以裸整数 `return` 收敛。下列取值逐条取自实现返回点，
> 只对本二进制有效；**不得**按 `../contracts/LOG_AND_ERROR.md` 「验收」一节 的表反查，也不得据此推断
> 三个生产命令的退出码语义。

| 工具返回值 | 域 | 锚（stage2.cpp） |
|---|---|---|
| 0 | 成功 | :2009 |
| 1 | 未捕获异常兜底（`std::exception` / `...`） | :2013/:2017 |
| 2 | config 解析/CLI 参数错误 | :133/:140/:146/:153/:160-165 |
| 3 | coverage 构建 / target_order 校验 | :195/:201/:207 |
| 4 | frame_id / sampler 域 | :227/:283/:288/:292/:298/:303/:310/:315/:319 |
| 5 | UPM 构建/持久化 | :437/:456/:477/:488 |
| 6 | 写路径/集成块（rejection resolve、tile 写、large_scale 等） | :517/:546/:589/:601/:653/:687/:793/:1055 等 |
| 7 | ivar 门（ivar 产品缺失且未显式降级）/ HIPS_VERIFY 回读失败 | :574/:1665 |

**与 `acsd::ExitCode` 的码值重叠**（UNRESOLVED，待负责人裁决）：3–7 五个值在本工具内
指上表所列的工具局部语义，而在 `acsd::ExitCode` 中分别为 `INPUT` / `SCIENCE` /
`BACKEND` / `COMPUTE` / `IO`。同一数值在两个命名空间下语义不同，按任一面判读
`acsd-stage2` 的返回值都会取到另一面的错值。缺陷根因在实现侧（工具以裸整数占用
枚举码值空间），不在文档侧；`acsd-stage2` 未 include `exit_codes.h`，无法在本单范围内修正。

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
 （:1730）/ local_ivar_used（:1732）/
 ivar_product_missing（:1733）/ local_snr_unavailable_controls
 （:1734）/ upm_sigma_floor（:1735）/ upm_support_power（:1736）/
 reject_method（:1739）/ reject_underdetermined_n（:1740）/
 rejection_resolved_methods（:1744）/ rejection_samples_per_pixel
 （:1745）。
- provenance/耗时: model_hash（:1746，UPM 持久层 hash，provenance
 链 DATA-P2-HIPS ）/ runtime_seconds（:1747）。

### 复用库 ABI 与负向条款

- aio_hips_*（API-HIPS-001，P1 冻结面）: P2 为库消费者
 （aio_hips_product_begin/write_signal_support_tile/finalize/last_
 error，stage2.cpp:592/:1629/:1638 路径），语义归 API-HIPS-001 与
 DATA-P1-HIPS ，本节不重登记。
- 负向条款: **不新增、不修改任何公共 C 头/C ABI**——
 p2_* 导出（coverage/sampler/upm/integrate）已由既有节冻结，
 acsd_p2_hips_writer.dll 为目标 DLL 名，其 C ABI adapter 建立后方可
 登记导出符号表。
- 上游：ALG-P2-HIPS-001..004。

## Phase2 integration 公共消费面（API-P2-INT-001）

> 定位: Phase2 逐像素加权积分内核公共 C ABI 消费面——导出符号
> `p2_integrate_pixel` / `p2_validate_candidate_weights`（既有 V17
> 清单行 17-19/24-29 的展开冻结，**不新增、不修改任何 C 头/C
> ABI**）；编排层经 API-P2-001（PUBLIC_API.md phase session）驱动，
> 内核本身无 session 依赖（无状态纯函数）。
> SRC: lib/algorithms/coverage/src/integrate.cpp（89 行，acsd_phase2 静态库
> 成员，根 CMakeLists.txt:652-661/:660）；唯一权威签名头
> lib/algorithms/coverage/include/astro/phase2/integrate.h（83 行: P2PixelStack
> :36-42 / P2IntegrateStatus :45-51 / P2PixelResult :53-63 / 函数
> 声明 :58-66）；DATA: DATA-P2-INT（DATA_SEMANTICS ，单位/dtype/
> shape 唯一权威）；ALG: ALG-P2-INT-001（docs/science/algorithms/
> PHASE2_INTEGRATION.md，逐符号锚与并行 tolerance 合同）；MOD:
> acsd.p2.integration（目标 DLL = acsd_p2_integration.dll；descriptor 占位 module_id=acsd.phase2.
> integrate 为编排层词汇，module_adapters.cpp:719-736。

### 导出符号与签名要点（integrate.h:58-65，冻结）

- `int p2_integrate_pixel(const P2PixelStack*, P2PixelResult*)`
 （:58-59）: rc=1 仅 stack/result null（integrate.cpp:20-21）；
 rc=0 时语义由 `result->status` 承载（五态，:45-51）。线程安全=
 reentrant yes / threadsafe no（无锁无全局态，并发由调用方像素
 划分）；无取消检查点（ThreadLease 由 host 侧提供）。
- `int p2_validate_candidate_weights(const double*, std::uint32_t)`
 （:62-66）: 预检门 → 0 合规 / 1 违规（null→0；任一 !finite 或
 w<0→1；w==0 合规，integrate.cpp:10-17）；调用方权重构造后必经
 （stage2.cpp:1141/:1402）。
- 输入域/输出域: P2PixelStack 四数组可空语义（weights null=等权、
 support null=1.0、accepted null=全接受）与五态触发条件=唯一
 权威 DATA-P2-INT （本节不重复展开）。

### 单位/dtype/shape（消费面副本，唯一权威=DATA_SEMANTICS ）

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
 线程 bitwise（无 epsilon；ALG-P2-INT-001 F7）。
- 调用方对 `pr.support` 的消费面 = 冻结原值本身（stage2.cpp:1525-1526
 注释冻结；ACR :205-208 同型消费）。

### 负向条款与行为边界

- 负向条款: **不新增、不修改任何公共 C 头/C ABI**——既有清单（:17-29）
 与本节为同一 ABI 的展开冻结，定义只有这一份；policy/reducer 分离：本层
 的权重策略面 = 空（weights 数组外置，由 Stage2 现场构造）。
- 行为边界：`sup_max` 的作用域 = 通过资格门（`accepted ∧ values finite ∧
 support finite 且 >0`）的**全部**样本，**含**零权重 accepted 样本（覆盖并集
 保守下界；`integrate.h:26-27` 为冻结文本，`integrate.cpp:44-50` 为唯一实现
 位置，更新位于权重分支之前）；全零权（ZERO_VALID_WEIGHT）仍发布该 max
 （`integrate.cpp:79-80`），ALL_REJECTED 保持 0（`integrate.cpp:21`）；
 Stage2/ACR 直接消费，回归门 = ALG-P2-INT-001 F4/F5 +
 `eng/tests/unit/p2_output_semantics_test.cpp`（语义权威 = SCI-INT-001，FROZEN）。
- 上游：SCI-INT-001（共享 FROZEN）/ ALG-P2-INT-001；验收面设计 =
 ALG-P2-INT-001 。

## Phase2 rejection 公共消费面（API-P2-REJ-001）

> 定位: Phase2 候选栈排异公共 C ABI 消费面——planning 层 +
> eligibility/gather 层 + kernel + large_scale 后处理的导出符号
> 冻结（既有符号的展开冻结，**不新增、不修改任何 C 头/C ABI**）；
> 编排层经 API-P2-001（PUBLIC_API.md phase session）驱动，kernel
> 无 session 依赖（无状态纯函数）。
> SRC: lib/algorithms/coverage/src/rejection.cpp（2965 行，acsd_phase2 静态库
> 成员，根 CMakeLists.txt:656-677/:660）+ 唯一权威签名头
> lib/algorithms/coverage/include/astro/phase2/rejection.h（602 行）；DATA:
> DATA-P2-REJ（DATA_SEMANTICS ，单位/dtype/shape/invalid 唯一
> 权威）；ALG: ALG-P2-REJ-001（docs/science/algorithms/PHASE2_REJECTION.md，
> 逐符号锚与消费链）；MOD: acsd.p2.rejection（目标 DLL = acsd_p2_rejection.dll；descriptor 占位
> module_id=acsd.phase2.reject 为编排层词汇，
> module_adapters.cpp:700-717 p2_reject_descriptor。

### 导出符号与签名要点（rejection.h 实测锚，冻结）

- `int p2_reject_plan_resolve(const P2RejectionPlanRequest*,
 P2RejectionPlan*, char* err, std::size_t err_size)`（:272-274 声明，
 注释 :247-263 路由 + profile 语义）: AUTO 一次解析为
 显式方法（nominal_contributors=u32 几何可贡献数 :234-238；
 kernel 永不接收 AUTO）；**生产默认 profile = `acsd_adaptive_pixel`
 （自研：1≤N≤3 → NONE、N<6 → PERCENTILE、N≥6 → WINSORIZED_SIGMA；
 档界唯一正本 = `docs/detail/mosaic/modules/rejection` 「排异档位表」一节）**；对照档 `wbpp_2_9_1`（n<6 → PERCENTILE、
 6..15 → WINSORIZED、>15 → LINEAR_FIT）与 `acsd_adaptive`；
 非法 profile → 非零 rc。rc=0 OK；rc=1 null
 请求/plan、request 出界或 profile 非法（err 仅日志文本）。线程
 安全=reentrant yes / threadsafe no（无锁无全局态，并发由调用方
 像素划分）；无取消检查点（ThreadLease 由 host 侧提供）。
- `int p2_eligibility_filter(const P2EligibilityInput*,
 P2EligibilityOutput*)`（:222）: 连续版资格层（compat 路径消费，
 与生产 strided gather 同一 policy core）。support_threshold 严格
 大于（:206）；quality_flags_required=0 不要求 quality（:207）。
 rc=1 null in/out 或必要输出缓冲缺失；n==0 → rc=0 空输出。线程
 安全=reentrant yes / threadsafe no；无取消检查点（ThreadLease 由 host
 侧提供）。
- `int p2_collect_candidate_stack(const P2EligibilityGatherInput*,
 P2EligibilityGatherOutput*)`（:263）: 生产 strided gather
 （frame-major f32/f64 → 紧凑 f64 栈，:1164-1179）；输出
 source_indices=权威回映射（PHASE2_IVAR_WIRING 注释 :252-255，
 compact 后的 original slot 映射 = source_indices；stage2.cpp:1098 权重构造方用它
 回映射 ivar slot）。rc=1 null in/out 或必要输出缓冲缺失；n==0 →
 rc=0 空输出。线程安全=reentrant yes / threadsafe no（无锁无全局
 态，并发由调用方像素划分）；无取消检查点（ThreadLease 由 host 侧提供）。
- `int p2_reject_stack_ex(const P2CandidateStack*,
 const P2RejectionPlan*, P2RejectionDecision*)`（:287-289，注释
 :285-286）: 显式 plan 执行（AUTO 返回非法参数）。rc=1 仅
 stack/plan/out null（:1687）或 reasons/values null 且 count>0
 （:1707）；rc=0 时语义全由 status 承载（八态
 P2RejectStatus 0..7，含 INVALID_METHOD/INVALID_INPUT/
 INVALID_CONFIGURATION——"科学状态"而非调用错误；状态机=ALG
 ）。kernel 内 n≤64 固定 scratch（无每像素堆分配）>64 走堆。
 线程安全=reentrant yes / threadsafe no（无锁无全局态，并发由
 调用方像素划分）；无取消检查点（ThreadLease 由 host 侧提供）。
- `const char* p2_rejection_semantic_id(int method)`（:196）: 语义
 id 查询（P2_SEMANTIC_* 常量 :59-70；未知方法返回 "unknown"）。
 线程安全=reentrant yes / threadsafe no（只读映射）；无取消检查
 点（ThreadLease 由 host 侧提供）。
- `int p2_large_scale_apply(std::uint8_t* low, std::uint8_t* high,
 int width, int height, int depth, const P2LargeScaleParams*)`
 （:295-297，注释 :291-294）: frame-major 每帧 width×height 字节
 原地修改（1=rejected）；仅扩张 ≥min_structure_pixels 结构；低/
 高侧独立半径。rc=0 OK（disabled=noop）；rc=1 指针 null ∨
 width/height/depth ≤0 ∨ min_structure_pixels<1 ∨ 负半径。线程
 安全=reentrant yes / threadsafe no（无锁无全局态，并发由调用方
 像素划分）；无取消检查点（ThreadLease 由 host 侧提供）。
- compat 面（冻结两符号，仅测试/兼容调用，:299 冻结注释"生产 Stage2
 不再调用"）: `int p2_reject_stack(const P2SampleStackView*,
 P2RejectionResult*)`（:325，兼容换算 sigma_low/sigma_high/
 max_iterations → typed params，min_samples 兼容门 :1891-1900）；
 P2SampleStackView :301-314 / P2RejectionResult :316-323。rc 语义
 同 kernel 面（科学语义由 status 承载）。线程安全=reentrant yes /
 threadsafe no；无取消检查点（ThreadLease 由 host 侧提供）。

### 单位/dtype/shape（消费面副本，唯一权威=DATA_SEMANTICS ）

| 项 | 值 |
|---|---|
| values | f64，ADU（kernel 工作域输入；gather f32/f64 源 → f64 提升，:1164-1179） |
| weights | f64，1/ADU²（可空=等权；数值域，权重策略外置调用方 ） |
| support | f64，无量纲 [0,1]（仅资格门，不进统计） |
| frame_ids | u64，无量纲稳定帧标识（ESD tie-break/确定性） |
| reasons | u8 0..3（P2RejectReason） |
| status | int 0..7（P2RejectStatus 八态） |
| typed params | 六组结构（rejection.h:99-129）禁跨方法共享字段（:99 注释） |
| 调用粒度 | 单像素栈（kernel）+ strided frame-major gather 批量（生产收集器） |

### 确定性/并发合同（matrix 专项）

- 逐样本独立判定（无跨样本浮点归约）→ per-pixel 决策 **bitwise
 独立于 worker 数**（1..N；ALG-P2-REJ-001 F5 置换不变性门
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

### 负向条款与行为边界

- 负向条款: **不新增、不修改任何公共 C 头/C ABI**——rejection.h 既有声明
 与本节为同一 ABI 的展开冻结，定义只有这一份；policy/reducer 分离：权重
 策略面 = 空（weights 数组外置，由 Stage2 现场构造；RCR 核消费同栈 weights
 数组属官方加权语义，非策略）。
- 行为边界：`rejection.h:118` 的 percentile `low_fraction` 注释写"默认 0.1"、
 `plan_resolve` 默认取 0.2（:1060）；两者以 SCI 权威为准（实现与 SCI 一致）。
 空栈结果 = `MIN_SAMPLES`（:1706），`NO_CANDIDATES` 属积分域
 （`P2IntegrateStatus` integrate.h:46），语义权威 = DATA 。
 SCI/REJECTION_ALGORITHMS 的行号锚以 ALG-P2-REJ-001 「result」一节 实测为准。
 `minmax` 比较器按 value-only 比较，等值样本的次序由实现决定。
- 上游：SCI-REJ-001（共享 FROZEN）/ ALG-P2-REJ-001 / DATA-P2-REJ；
 验收面设计 = ALG-P2-REJ-001 。

## Phase2 sampling 公共消费面（API-P2-SMP-001）

> 定位: Phase2 background-clean 控制点采样公共 C ABI 消费面——
> 配置默认/统计量/帧身份/采样两入口的导出符号冻结（既有符号的
> 展开冻结，**不新增、不修改任何 C 头/C ABI**）；编排层经
> API-P2-001（PUBLIC_API.md phase session）驱动，采样函数无
> session 依赖（数据面经 P2CoverageResult 显式传入）。
> SRC: lib/algorithms/coverage/src/sampler.cpp（1503 行，acsd_phase2 静态库
> 成员，根 CMakeLists.txt:337-346/:342）+ 唯一权威签名头
> lib/algorithms/coverage/include/astro/phase2/sampler.h（273 行）；DATA:
> DATA-P2-SMP（DATA_SEMANTICS ，单位/dtype/shape/invalid 唯一
> 权威）；ALG: ALG-P2-SMP-001（docs/science/algorithms/PHASE2_SAMPLER.md，
> 逐符号锚与消费链）；MOD: acsd.p2.sampling（目标 DLL = acsd_p2_sampling.dll；descriptor 占位
> module_id=acsd.phase2.sample 为编排层词汇，
> module_adapters.cpp:642-653 p2_sample_descriptor。

### 导出符号与签名要点（sampler.h 实测锚，冻结）

- `P2SamplerConfig p2_sampler_default_config(void)`（:60 声明，
 实现 sampler.cpp:294-312）: 配置默认单一来源（15 字段，null cfg
 时使用；显式 cfg 覆盖）。字段表=DATA (3)。线程
 安全=reentrant yes / threadsafe yes（纯值返回）；无取消检查点
 （ThreadLease 由 host 侧提供）。
- `std::uint64_t p2_frame_id(const char* hips_path)`（:93，注释
 :85-92 冻结）: 内容稳定帧标识——truncated-64 canonical SHA-256
 （9 properties + signal tile 像素 + support tile 像素 "S" 前缀 +
 SNR catalogue 内容，:314-438）；路径/重命名/换根不变，任何科学
 payload 变化 → id 变化；取 SHA-256 前 16 hex 大端截断；与输入
 顺序无关；UPM 参考帧=每分量最小 frame_id。**描述口径 = 64 位截断哈希，非 FNV-1a/
 路径派生**（h:92）。失败哨兵=0（调用方 cached 入口 :512-523
 显式拒绝 0）。线程安全=reentrant yes / threadsafe no（AIO 全局
 缓存面）；无取消检查点（ThreadLease 由 host 侧提供）。
- `double p2_stats_median(const double*, std::uint64_t)`（:97）与
 `double p2_stats_mad(const double*, std::uint64_t, double*
 out_median = nullptr)`（:98-99）: 统一统计量（sampler patch
 estimator / MAD / SNR 邻域与 UPM 域共用同一实现）；median 偶数
 n 取上下中位平均、NaN 自动过滤（全 NaN → 0，:440-447）；MAD=
 1.482602218505602×median(|x−med|)（:449-461）。线程安全=reentrant yes /
 threadsafe yes（无共享可变态）；无取消检查点（ThreadLease 由 host 侧
 提供）。
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
 union 合法）/ rc=1 错误 + err 8KB 文本（细分语义=DATA ALG ；bad args :476-479、open failed :536-546、n_union
 >1e6 :634-638、cells>2e8 :644-648/:1071-1077、首 tile 越界
 :656-669、exception 兜底 :1089-1097）；容量不足**不报错**：
 按 capacity 截断拷贝、out_n_* 返回真实需求量（:1098-1117）。
 线程安全=reentrant yes / threadsafe yes（读路径**不存在进程级串行锁**、无进程级共享可变状态；per-worker 独立 AIO
 句柄 :938，每次读各自 open→read→close、句柄线程私有不跨线程转移；
 依据见 「权重对象的归属」一节/「result」一节）；无取消
 检查点（ThreadLease 由 host 侧提供，与重扫行为
 同构）。

### 单位/dtype/shape（消费面副本，唯一权威=DATA_SEMANTICS ）

| 项 | 值 |
|---|---|
| P2ControlObservation | 13 字段（upm.h:31-57）：frame_id/control_id/leaf_ipix u64、ra_deg/dec_deg/value/uncertainty/snr/ivar/control_variance/control_ivar/support f64、snr_available int、quality_flags u32 |
| value/uncertainty | f64 ADU（value 可负 patch median） |
| control_variance/control_ivar | f64 ADU² / 1/ADU²（k_corr×(π/2)×σ_bg²/N_retained 冻结公式；ivar 弃用仅诊断） |
| snr_available | int 0/1（0=回退整帧中位，禁以 1.0 伪装 unknown，upm.h:51-55） |
| P2SampleStats | 10 字段 u64 诊断计数（sampler.h:63-74） |
| P2ControlNode | 7 字段（sampler.h:77-83）；out_n_controls=n_union×G² 含空覆盖占位 |
| cfg | P2SamplerConfig 15 字段（sampler.h:33-57；默认单一来源 :60/:294-312） |
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
 从实现移除（:882-883 注释），lib/algorithms/coverage/CMakeLists.txt:28
 P2_ENABLE_OPENMP option 只影响该 target 的编译面。
- 容差=bitwise（obs 输出无 epsilon 门；clipping 收敛 1e-12 相对阈
 为实现内部冻结常数；坐标面 F4 atol 1e-9 deg、cvar 面 F2/F3/F5
 rtol 1e-12 为 TEST-P2-SMP-DESIGN-001 测试容差，非 ABI 容差）。
- 调用方契约: frame_ids 0=非法（禁静默替换）；out_n_controls
 几何计数与 accepted/overlap 统计计数并列表述禁混用；ivar 弃用
 仅诊断、科学权重一律 control_ivar（upm.h:38-41/）。

### 负向条款与行为边界

- 负向条款: **不新增、不修改任何公共 C 头/C ABI**——sampler.h 既有声明
 与本节为同一 ABI 的展开冻结，定义只有这一份；不引入第二配置面
 （veto 阈值/半径现状硬编码，本节不预设字段）；不改变 `frame_id` 身份
 算法（DATA-FRAME-ID-001 冻结，任何 payload 键变化即违约）。
- 行为边界：配置 `<=0` 按默认值处理，吞掉显式 0（:485-502）；
 `insufficient_retained` 双计数（:1006/:1022），obs 统计面不受影响；
 17 处诊断直写 stderr；veto 阈值/半径硬编码（:849-850）；`m0≈0` 时收敛
 阈值退化、走全迭代（:818），确定性无影响。
- 上游：SCI-UPM-001（共享 FROZEN）/ ALG-P2-SMP-001 / DATA-P2-SMP；
 验收面设计 = ALG-P2-SMP-001 F1-F9。

## Phase2 装配会话 C API（API-P2-SESSION-001）

> 定位: Phase2 进程内装配会话公共 C ABI——coverage → sample →
> upm_build → persist 四段编排的会话生命周期五导出符号冻结（既有
> 符号的展开冻结，**不新增、不修改任何 C 头/C ABI**）；会话本身无
> 科学实现（纯编排 facade，直调 lib/algorithms/coverage 生产符号），编排上游=
> API-P2-001（PUBLIC_API.md phase session，api/PUBLIC_API.md
> FROZEN，引用不改动）。
> SRC: lib/phase2_session/p2_session.cpp（318 行，静态库
> acsd_phase2_session 成员，根 CMakeLists.txt:454-458）+ 唯一
> 权威签名头 lib/phase2_session/p2_session.h（39 行）；DATA:
> DATA-P2-SESSION（DATA_SEMANTICS ，eng/packaging/config/manifest/错误码唯一
> 权威）；ALG: ALG-P2-SESSION-001（docs/science/algorithms/PHASE2_SESSION.md，
> 四段调用序逐源码行号锚）；MOD: acsd.p2.session（目标 DLL = acsd_p2_session.dll；registry
> descriptor 现无 acsd.p2.session 占位词汇；编排委托锚 =
> module_adapters.cpp:23-26 五 C ABI 声明（RT-005），descriptor 词汇不作
> 冻结依据）。

### 导出符号与签名要点（p2_session.h 实测锚，5 C API + 1 C++ 诊断全部当前真实存在，冻结）

- `acsd_status p2_session_create(const acsd_host_services_v1* host,
 acsd_handle* out)`（p2_session.h:17，实现 p2_session.cpp:56-67）:
 会话唯一构造。host 不可空且 struct_size/abi_version 校验
 （:57-59 → ACS_ERR_ABI_MISMATCH）；out null → ACS_ERR_PARAM（:60）；
 SessionState 分配失败 → ACS_ERR_NOMEM（:62）；manifest 初始化
 kind="acsd_phase2_session" + stages=[]（:64）。host 指针由会话
 持有（借用，不拥有——宿主须保证句柄存活期 host 存活）。
- `acsd_status p2_session_validate(acsd_handle h, const acsd_span_u8
 config_json)`（p2_session.h:20，:69-98）: 纯读无 IO，幂等可重复
 （p2_session.h:19 注释）；句柄/config null 或空 → ACS_ERR_PARAM
 （:71）；parse 失败/非 object/缺必需键/类型错 → ACS_ERR_PARAM
 （:76-96）；键集与校验规则唯一权威=DATA （未知键**现状不
 拒绝**；`p2_session.h:19` 的注释与 `:69-98` 实现不同，判据以 DATA 为准）。
- `acsd_status p2_session_run(acsd_handle h, const acsd_span_u8
 config_json)`（p2_session.h:23，:100-248）: 四段编排执行——
 coverage（:119-148，两遍 build 协议）→ sample（:150-178，probe/
 fill）→ upm_build（:180-219，冻结默认+upm 子键覆盖）→ persist
 （:221-240，可选）。取消点=**阶段边界**（p2_session.h:22 注释
 冻结；4 检查点 :120/:152/:181/:223-227，upm 整模型不写半成品）；
 内部并行仅 UPM blocks（cpu_workers=budget.max_workers :155/:195，
 预算驱动禁硬编码）。config 消费口径=DATA ；run 重入
 不清空 manifest（stages 累积，幂等未冻结，登记）。
- `acsd_status p2_session_inspect(acsd_handle h, acsd_span_u8*
 out_manifest_json)`（p2_session.h:25，:250-266）: 只读导出
 manifest JSON（dump(2)，:258）；out null → ACS_ERR_PARAM（:252）；
 status 派生（created/complete/failed :253-256）；输出缓冲经
 host->allocator.alloc 分配（:260），**释放责任=宿主经同一
 allocator**（common_abi_v1.h:74 合同头）；分配失败 →
 ACS_ERR_NOMEM（:261）。字段表唯一权威=DATA 。
- `acsd_status p2_session_destroy(acsd_handle h)`（p2_session.h:27，
 :268-273）: 唯一释放路径；句柄 null → ACS_ERR_PARAM（:270）；destroy
 后句柄不可再用（run 失败路径内部已 p2_upm_close model，:231/:241，
 会话不持 model 句柄）。
- C++ 诊断面（非 C ABI，p2_session.h:32-36）:
 `std::string acsd::phase2::last_error(acsd_handle h)`（:278-281）
 ——最近一次错误脱敏摘要（"what rc=N"，:45）；诊断用，非科学接口，
 不进冻结 ABI 面。

### 参数表（acsd_span_u8/config/manifest 口径，唯一权威=DATA ）

| 参数 | 口径 |
|---|---|
| host | const acsd_host_services_v1*（common_abi_v1.h:110-117，四通道 allocator/logger/cancel/budget；struct_size+abi_version handshake） |
| config_json | acsd_span_u8（UTF-8 JSON 单对象；count 不含 NUL；键集=DATA ） |
| out_manifest_json | acsd_span_u8*（出参；UTF-8 JSON dump(2)；宿主 allocator 释放；字段=DATA ） |
| acsd_handle | 不透明句柄（SessionState*，实例态全隔离） |

### 返回码（唯一权威=DATA 表）

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
 注释锚）——同一 handle 的并发面 = 单线程；不同 handle 并发合法
 （无共享可变全局态）。
- 取消点=**阶段边界**（p2_session.h:22 注释锚；coverage/sample/
 upm_build/persist 段前 4 检查点，p2_session.cpp:121/:152/:181/
 :223-227）；upm 整模型不写半成品（段内无检查点，persist 取消先
 close）；取消返回 ACS_ERR_CANCELLED + 段 status=cancelled。
- 内部并行**仅 UPM blocks**：sample/upm 段 cpu_workers=
 host->budget.max_workers（:155/:195，Runtime lease 唯一来源），
 coverage/persist 串行；预算绑定注释（p2_session.h:4
 "sampler=1"）与实现（N-worker）不同，判据以 DATA 为准。

### 负向条款与行为边界

- 负向条款: **不新增、不修改任何公共 C 头/C ABI**——p2_session.h 既有五
 函数声明与本节为同一 ABI 的展开冻结，定义只有这一份；registry
 descriptor acsd.phase2.session 占位词汇不作冻结依据；编排上游
 API-P2-001（api/PUBLIC_API.md，FROZEN）引用不改动；会话冻结默认
 （upm 参数/tolerance/sigma_floor 等）非 config 覆盖键的部分禁改。
- 行为边界：`p2_session_validate` 不拒绝未知键（p2_session.h:19 与 :69-98
 的注释/实现差异）；`run` 对子键类型错的路径不捕获；h:4 预算绑定注释与
 实现不同。判据以 DATA 为唯一权威。
- 上游：SCI-UPM-001 / SCI-INT-001 / SCI-REJ-001（共享 FROZEN）/
 ALG-P2-SESSION-001 / DATA-P2-SESSION；验收面设计 = ALG-P2-SESSION-001
 TEST-DESIGN 节。

## Phase2 UPM 公共消费面（API-P2-UPM-001）

> 定位: Phase2 UPM fit/persist/apply/reload 公共 C ABI 消费面——
> 既有 16 导出符号的展开冻结（**不新增、不修改任何 C 头/C ABI**；
> upm.h 为唯一权威签名头，184 行）。
> SRC: lib/algorithms/coverage/src/upm.cpp（2981 行，acsd_phase2 静态库成员，
> 根 CMakeLists.txt:337-346/:338）+ 唯一权威签名头
> lib/algorithms/coverage/include/astro/phase2/upm.h（453 行）；DATA:
> DATA-P2-UPM（DATA_SEMANTICS ，fit/persist 域单位/dtype/invalid
> 唯一权威）/ DATA-P2-COR（DATA_SEMANTICS ，apply 域唯一权威）；
> ALG: ALG-P2-UPM-IMPL-001（docs/science/algorithms/PHASE2_UPM_IMPL.md，
> 逐符号锚与消费链）；MOD: acsd.p2.upm（目标 DLL = acsd_p2_upm.dll；descriptor 占位
> module_id=acsd.phase2.upm-fit/upm-apply，
> module_adapters.cpp:661-696（p2_upm_fit_descriptor :599 /
> p2_upm_apply_descriptor :618）。

### 导出符号与签名要点（upm.h 实测锚，冻结）

- `int p2_upm_build(const P2ControlObservation* obs, std::uint64_t
 n_obs, const P2UpmBuildConfig* cfg, void** out_model)`（h:95-97
 声明，实现 upm.cpp:929）: 联合拟合入口（obs 域=DATA-P2-SMP，13
 字段 (1)）；cfg 可空=运行时默认填充（:222-236）+非法值修补
 （:237-245）；target_order=-1（auto）→ rc=1（:246-249，空间 UPM
 必须知 leaf 层级 order=target+9）。rc: 0=ok（out_model 出参，空
 obs 合法→NO_DATA 语义，SCI-UPM-001 「显式拒绝清单」一节）/ 1=参数错误（句柄/cfg
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
 实现 upm.cpp:940）: 模型落盘（acsd-upm-v2 单文件 JSON，
 唯一 AIO aio_upm_write_sparse :1003-1005，aio_upm.cpp:66 原子写
 ENG-IO-001）。rc: 0=ok / 1=model 或 path null（:941）、frame 绑定
 行数不一致拒写（:943-945，ALG-UPM-FRAME-BIND-001）。线程安全=
 生命周期内单会话使用。
- `int p2_upm_open(const char* path, void** out_model)`（h:109，
 实现 upm.cpp:1008）: reload 入口；format 校验失败（非
 acsd-upm-v2，:1027-1028）/ parse 失败 → rc=1；成功保持
 frame_id→θ 映射（SCI-UPM-PERSIST-001）。线程安全=生命周期内
 单会话使用。
- `int p2_upm_info(const void* model, P2ModelInfo* out_info)`
 （h:110，实现 upm.cpp:1233）: 溯源面只读导出（七字段 h:60-68=
 DATA (1) 表）。rc: 0=ok / 1=model 或 out_info null。线程
 安全=纯只读 reentrant yes。
- `int p2_upm_calibrate_block(const void* model, std::uint64_t
 frame_id, const std::uint64_t* leaf_ipix, const double*
 input_signal, double* output_signal, std::uint64_t count)`（h:113-119
 冻结注 :112，实现 upm.cpp:1240）: apply 核心——
 output_signal[i]=input_signal[i]−C(frame_id, leaf_ipix[i])（:1266，
 8×8 cell 内双线性 :1263；DATA ）。rc: 0=ok / 1=句柄或数组
 null（:1245-1248）或**未知 frame_id 显式失败**（:1250-1252 注释
 "frame 0 参数只用于该 frame_id 自身的校准"，禁回退红线）。线程安全=只读模型+独立
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
 controls/邻接，:1350-1362；hash 构成面 = 前四项，权重/quality/support 变化不参与——
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
 :1478；`workers` 注释（h:176）与实现（:1478）取值不同）。rc: 0=ok / 1=model 或
 cache_path null、IO 失败。线程安全=生命周期内单会话使用。
- `int p2_upm_materialize_dense(const void* model, int target_order,
 const char* cache_path)`（h:155-156 与 :174-175 重复声明=
 实现 upm.cpp:1518-1521）: workers=0
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
 模型句柄唯一释放点（DATA (1)）；重复 close/悬垂句柄=调用方
 生命周期违约。线程安全=释放语义（句柄所有权归调用方）。

并行/取消消费面注记: workers 唯一来源=cfg.cpu_workers（Runtime
lease，p2_session.cpp:195 budget.max_workers；禁硬编码，CON-005；
同构）；std::thread 池三段（:513-534/:613-633/:1479-1497，
kChunk=16 :1407），全文件无 #pragma omp；确定性=D1（:513-514，
worker 数无关、同 worker 数下位精确；dense 物化 bit-identical
:1383-1386）。无取消检查点——取消=会话层阶段边界（"取消=整模型
不写半成品"，p2_session.cpp:180）；ThreadLease 由 host 侧提供。

### 负向条款与行为边界

- 负向条款: **不新增、不修改任何公共 C 头/C ABI**——upm.h 既有 16 符号
 声明与本节为同一 ABI 的展开冻结，定义只有这一份；registry descriptor
 占位词汇（module_id=acsd.phase2.upm-fit/upm-apply，
 module_adapters.cpp:661-696，ports samples/upm_model/calibrated_frames/
 corrected）不作冻结依据；上游 SCI-UPM-001（FROZEN）引用不改动。
- 行为边界：`materialize_dense` 在 upm.h:197-198 与 :216-217 各有一次声明；
 upm.h:89-91 注释按 OpenMP 措辞、实现用 std::thread；会话 upm 覆盖键仅
 `max_iterations`/`huber_delta`/`smoothing_lambda` 三个
 （p2_session.cpp:196-202）；材料化 `workers` 注释（h:176）与实现（:1478）
 取值不同。
- 验收面：ALG-P2-UPM-IMPL-001 TEST-DESIGN 节（fit/persist 域与 apply 域）。
- 上游：SCI-UPM-001（FROZEN）/ ALG-P2-UPM-IMPL-001 / DATA-P2-UPM/
 DATA-P2-COR。

## Phase3 FITS 写出公共消费面（API-P3-FITS-001）

> 定位: Phase3 FITS 写出域公共消费面——既有内核符号的展开冻结
> （**不新增、不修改任何 C 头/C ABI**；p3_output.h 为唯一权威签名
> 头，64 行，C++ namespace acsd::phase3；编排面 p3_session.h
> 五段式=API-P3-001 FROZEN 不变，本节仅镜像声明）。
> SRC: lib/algorithms/fits_output/p3_output.cpp（1270 行，acsd_phase3_session
> 静态库成员，根 CMakeLists.txt:460-465）+ 唯一权威签名头
> lib/algorithms/fits_output/p3_output.h（176 行）+ WCS 关键字源 p3_wcs.h
> （50 行）；DATA: DATA-P3-FITS（DATA_SEMANTICS ，单位/dtype/
> invalid 唯一权威）；ALG: ALG-P3-FITS-IMPL-001
> （docs/science/algorithms/PHASE3_FITS_IMPL.md，逐符号锚与消费链）；
> MOD: acsd.p3.fits_writer（目标 DLL = acsd_p3_fits_writer.dll；
> descriptor 占位 module_id=acsd.phase3.writer，
> module_adapters.cpp:445-460 p3_writer_descriptor）。

### 内核符号与签名要点（p3_output.h 实测锚，冻结）

- `P3OutputStatus p3_output_write_atomic(const float* signal, const
 float* coverage, int width, int height, const P3WcsDescriptor* wcs,
 const char* bunit, const char* output_path, const P3Provenance* prov,
 int bitpix, int cancelled_at_row, P3OutputResult* result)`
 （h:45-53 声明，实现 p3_output.cpp:117-335）: 原子写入口——
 signal 主 HDU + COVERAGE 扩展 HDU 合成单文件；tmp → fits_flush_file
 → close → fsync(fd) → rename（R10-C 冻结序，:221-273）；取消
 （cancelled_at_row≥0）/任一步失败 → unlink 不发布（h:41-44）。
 数据域=DATA-P3-FITS ；bitpix∈{-32,-64}（:140-145）；
 cfitsio 进程锁全程（:125，RT-008）。rc: 0=OK（result 出参含
 sha256/coverage_ok/reopen_ok/covered_px/total_px）/ 1=PARAM
 （空指针、W/H<1、bitpix 非法）/ 2=IO（cfitsio/fsync/rename/sha256
 失败，无假文件无假哈希）/ 3=CANCELLED（不落盘）。线程安全=
 进程级互斥内单写（与读路径 aio_fits.cpp:529 同锁），写面无并行、
 输出与 worker 数无关（ALG-P3-FITS-IMPL-001 「验收」一节/）。
- `P3OutputStatus p3_output_verify(const char* output_path, const
 P3WcsDescriptor* wcs, const float* signal, const float* coverage,
 int width, int height, P3OutputResult* result)`
 （h:57-60 声明，实现 p3_output.cpp:310-382）: 独立重开验证
 （READONLY fits_open_file :312）——逐 HDU 尺寸/像素回环
 （NaN==NaN 一致 :327-330）+ coverage 二值门（>0.5f :346）+
 sha256 重算（失败→IO 不带假哈希 :356-366）。wcs 参数现状忽略
 （:305 (void)wcs，WCS 一致性由写路径单点保证，:301-302 注释）。
 rc: 0=OK / 1=PARAM（path/result null、W/H<1）/ 2=IO。线程安全=
 只读 reentrant（cfitsio 锁内）；与写面互斥同锁。
- `struct P3Provenance`（h:15-24）/ `struct P3OutputResult`
 （h:26-32）/ `enum P3OutputStatus`（h:34-39）: 消费面数据结构
 冻结（字段级锚=DATA-P3-FITS ；枚举值 0/1/2/3 冻结）。
- WCS 关键字源符号（p3_wcs.h，ALG-P3-002 承接）:
 `p3_wcs_make`（h:31-34，parity east_left 默认 CD1_1<0 :29）、
 `p3_wcs_pix2world`（h:38-39，0-based 入参 FITS=+1 :36）、
 `p3_wcs_world2pix`（h:42-43）、`p3_wcs_fits_keywords`（h:46）、
 `P3WcsDescriptor`（h:11-20）/`P3WcsStatus`（h:22-27，含
 P3_WCS_HEMISPHERE 半球守卫 :26）。这些符号属本域公共头（同一
 静态库 acsd_phase3_session），随本节一并冻结；重采样/采样
 域符号（p3_resample/p3_sampler）不在本消费面。
- 会话编排面镜像（API-P3-001 FROZEN 不变）: p3_session 五段
 （p3_session.h:16-28 create/validate/run/inspect/destroy）+
 `last_error`（h:33-37 脱敏诊断，非科学接口）。run 内部经
 p3_output_write_atomic（p3_session.cpp:287-292，cancelled_at_row
 恒 -1）落盘，inspect 摘要含 output_fits_path/sha256/
 order_sel_used/sampler_used/coverage_stats/provenance
 （:296-313）——编排消费经 API-P3-001，内核直调面仅限本节符号。

### 交叉引用与消费锚

- 上游：SCI-P3-001（FROZEN，a-11 关键字面）引用不改动；ALG-P3-002/
 ALG-P3-004（PHASE3_RESAMPLE.md 施工规格，公式零改动）；
 ALG-P3-FITS-IMPL-001（实现级合同，定义 T1-T7 验收面）；
 DATA-P3-FITS。
- 写入面硬约束：tmp 命名协议以 `p3_output.h:41-44` 冻结注为准，实现对齐
 点 `p3_output.cpp:81`；测试残留检查按该协议比对。镜像面
 `manifest_hash` 的接线锚 = `p3_session.cpp:270`。
- 现状执行测试：`eng/tests/unit/p3_output_test.cpp`（244 行 4 段）作为
 相邻证据引用不冒认；验收级执行测试见 ALG-P3-FITS-IMPL-001 。
- 上游与镜像：SCI-P3-001（FROZEN）/ ALG-P3-FITS-IMPL-001 /
 DATA-P3-FITS；镜像 API-P3-001（FROZEN 编排面，不因本节改动）。

## Phase3 投影/WCS 公共消费面（API-P3-PROJ-001）

> 定位: Phase3 投影/WCS 域公共消费面——既有内核符号的展开冻结
> （**不新增、不修改任何 C 头/C ABI**；p3_wcs.h 为唯一权威签名头，
> 50 行，C++ namespace acsd::phase3；编排面 p3_session.h 五段式=
> API-P3-001 FROZEN 不变，本节仅镜像声明）。
> SRC: lib/algorithms/projection/p3_wcs.cpp（现为 acsd_p3_projection_wcs
> STATIC 成员，
> 经 acsd_phase3_session/acsd_module_adapters 闭包）+ 唯一权威签名头
> lib/algorithms/projection/p3_wcs.h；DATA: DATA-P3-WCS
> （DATA_SEMANTICS ，单位/dtype/invalid 唯一权威）；ALG:
> ALG-P3-PROJ-IMPL-001（docs/science/algorithms/PHASE3_PROJ_IMPL.md，逐符号
> 锚与 G1/G2 冻结式）；MOD: acsd.p3.projection（目标 DLL = acsd_p3_projection.dll；descriptor 占位
> module_id=acsd.phase3.wcs，module_adapters.cpp:776-793
> p3_wcs_descriptor）。

### 内核符号与签名要点（p3_wcs.h 实测锚，冻结）

- `P3WcsStatus p3_wcs_make(double centre_ra_deg, double
 centre_dec_deg, double scale_deg_per_px, int width_px, int
 height_px, const char* parity, double rotation_pa_deg,
 P3WcsDescriptor* out)`（h:31-34 声明，实现 p3_wcs.cpp:30-123）:
 descriptor 构造入口——G1 冻结式（CRPIX=(W+1)/2、CD=R(−PA)·
 diag(sgn_x·s, sgn_y·s)，PA=0 精确退化对角 diag(−s,+s)/
 diag(+s,−s)，det(CD)=−s²<0 手性冻结，）；参数校验序 parity→
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
 数据结构冻结（字段级锚=DATA-P3-WCS ）。投影方法面
 唯一实现 = TAN，非 TAN 请求一律 UNSUPPORTED。
- 并发语义: 四函数纯函数无状态（0 处 thread/mutex/omp/全局可变
 量，:12-28）——const-only 入口多线程并发安全（descriptor
 parallel_ok=true 与此一致）；确定性 bitwise（ALG-P3-PROJ-IMPL-001
 ）。roundtrip 冻结容差 <1e-8 px（SCI-P3-001 「验收」一节，禁放宽；生产注册表
 实现面 `lib/algorithms/projection/p3_wcs.cpp`）。
 该紧门的**适用域** = `scale ≥ min_scale_arcsec = 0.9″/px`（低于下限时门不适用、报「超出适用域」而非判红，
 退回全域保守门 1e-6 px）；机器可读判定 = `p3_wcs_roundtrip_gate`（GATES 「result」一节 G-P1-WCS-BRIDGE / -GLOBAL）。
- 会话消费锚（编排面 API-P3-001 镜像）: p3_session.cpp:160
 p3_wcs_make（rotation_pa_deg 恒 0.0；PA 面不接线）
 /:163 状态映射（UNSUPPORTED→ACS_ERR_UNSUPPORTED，其余非 OK→
 ACS_ERR_PARAM）/:232 worker 循环逐像素 pix2world（失败 continue，
 半球外像素 NaN）/:247-253 worker 池=budget.max_workers（禁
 hardware_concurrency）。

### 交叉引用与消费锚

- 上游：SCI-P3-001（FROZEN，「逐条追溯」一节 连续定义 + a-4/-6/-12 + 「验收」一节
 roundtrip 容差）引用不改动；ALG-P3-002（「权重对象的归属」一节
 G1/G2 施工规格，公式零改动）；ALG-P3-PROJ-IMPL-001（实现级合同，
 定义 T1-T7 验收面）；DATA-P3-WCS。
- 入口硬约束：会话层的 PA 传参为冻结值 0.0
 （`p3_session.cpp:160`）；边长上限由 `ACSD_P3_MAX_SIDE` 在编译期
 控制（`p3_wcs.cpp:18-22`）。
- 现状执行测试：`lib/algorithms/projection/tests/p3wcs/p3_wcs_test.cpp`（474 行）+
 `eng/tests/backend/test_p1002_gaps.py`（独立解析解回归）+
 `lib/algorithms/projection/tests/p3wcs/p3_wcs_main.cpp`（探针）作为相邻证据引用
 不冒认；验收级（WCSLIB oracle）执行测试见 ALG-P3-PROJ-IMPL-001。
- 上游与镜像：SCI-P3-001（FROZEN）/ ALG-P3-PROJ-IMPL-001 /
 DATA-P3-WCS；域际 API-P3-FITS-001（FITS 写出消费面，wcs 字段
 承载本 descriptor）；镜像 API-P3-001（FROZEN 编排面，不因本节改动）。

## Phase3 HiPS 重采样公共消费面（API-P3-RSMP-001）

> 定位: Phase3 HiPS 重采样域公共消费面——既有内核符号的展开冻结
> （**不新增、不修改任何 C 头/C ABI**；p3_resample.h 为唯一权威签名
> 头，58 行，C++ namespace acsd::phase3；编排面 p3_session.h 五段
> 式=API-P3-001 FROZEN 不变，本节仅镜像声明）。
> SRC: lib/algorithms/resample/p3_resample.cpp（586 行，acsd_phase3_
> session 静态库成员，根 CMakeLists.txt:460-465）+ 唯一权威签名头
> lib/algorithms/resample/p3_resample.h（202 行）；DATA: DATA-P3-RES
> （DATA_SEMANTICS ，单位/dtype/invalid 唯一权威）；ALG:
> ALG-P3-RSMP-IMPL-001（docs/science/algorithms/PHASE3_RSMP_IMPL.md，逐符号
> 锚与 G3/G4 冻结式）；MOD: acsd.p3.resample（目标 DLL = acsd_p3_resample.dll；
> descriptor 占位 module_id=acsd.phase3.resample2，module_adapters.
> cpp:363-377 p3_resample2_descriptor）。

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
 order（int）与 BUNIT（缺省 **canonical `"ADU/sr"`**，**绝不裸 ADU**、**绝不 Jy/beam**——
 SCI a-11；口径来源 science 分册的数据语义卷 a `FZ-UNIT-SIGNAL-SB`；
 实现见 `lib/algorithms/resample/p3_resample.h` 的 BUNIT 来源输入合同与 finalize 实参。
 **注**：`ADU` 是每像素计数、`ADU/sr` 是面亮度，两者差一个立体角、是两个物理量，
 不得互相代入）；
 失败路径 properties 解析/目录不可读→IO、frame≠ICRS→UNSUPPORTED。
- `P3ResampleStatus p3_order_select(int max_order, double
 scale_deg_per_px, int* out_order)`（h:21 声明，实现 :82-93）: G3
 order 选择——最小 k 使 pixel_resolution_arcsec(512<<k)/3600 ≤
 scale_deg_per_px（与 ALG-P3-003 G3 ceil 式数学等价，）；扫描
 完未命中→out_order=max_order（欠采样降级，SCI a-5）；守卫
 out_order 空/max_order∉[0,20]/scale≤0→PARAM。
- `P3ResampleStatus p3_resample_check_mode(const char* input_mode)`
 （h:25 声明，实现 :95-107）: 输入模式守卫——`surface_brightness`
 唯一合法；其余→UNSUPPORTED（SCI a-8/10 显式拒）。会话层不经该入口，
 探针直接消费。
- `void p3_sampler_set_max_tiles(P3Sampler* s, int max_tiles)`
 （h:42 声明，实现 :161-168）: tile 缓存容量设置；≤0 恢复默认 8；
 会话守卫默认 min(1024, ceil(W·H/512²)+16)，请求超默认→
 ACS_ERR_BUDGET（可降不可升，p3_session.cpp:179-194）。
- `void p3_sample_nearest(const P3Sampler* s, const P3WcsDescriptor*
 d, int x, int y, float* value, float* coverage)`（h:46-47 声明，
 实现 :232-239）: NEAREST 采样——输出像素中心→pix2ang→ang2pix
 精确 cell；无插值误差（SCI a-12）；值语义=DATA-P3-RES （tile 内 NaN→值 NaN+coverage=1；tile 缺失→coverage=0）。
- `void p3_sample_bilinear(const P3Sampler* s, const P3WcsDescriptor*
 d, int x, int y, float* value, float* coverage)`（h:51-52 声明，
 实现 :196-230）: BILINEAR 采样——leaf 3×3 邻域四象限最近中心、
 切平面双线性（FP64 权重，Σw=1 精确成立——SCI a-7 不变量）；
 离散化方案以本实现为准（G4 施工规格为其来源）。
- `void p3_sampler_close(P3Sampler* s)`（h:54 声明，实现 :170-180）:
 释放并置空；幂等（可安全重复调用）。
- `enum P3ResampleStatus`（h:12-17）: OK=0/PARAM=1/UNSUPPORTED=2/
 IO=3；`struct P3Sampler`（h:29-31，impl 指针+last_error[256]）:
 消费面数据结构冻结（字段级锚=ALG-P3-RSMP-IMPL-001 「显式拒绝清单」一节）。
- 并发语义: **每 worker 独立 sampler+cache**（自含 TileCache，无
 共享可变状态）；单一 P3Sampler 实例非线程安全（无内部锁），共享面 = 每 worker 各自实例
 （合同红线，ALG-P3-RSMP-IMPL-001 「验收」一节）；确定性
 bitwise（输出与 tile 装载顺序/worker 数/缓存容量无关）。
- 会话消费锚（编排面 API-P3-001 镜像）: p3_session.cpp:167-177
 主 sampler open_ex :171（状态映射 IO→ACS_ERR_IO、UNSUPPORTED→
 ACS_ERR_UNSUPPORTED、PARAM→ACS_ERR_PARAM，:175-176）/:179-194
 max_tiles 会话守卫/:196-199 p3_order_select（max_order=输入实际
 order）/ :217-244 每 worker 独立 open_ex（:222）+逐像素
 nearest/bilinear 分派（:236-237；缺省 bilinear，:116-119 白名单）/
 :258-263 取消收尾/:265-277 provenance
 （order_sel_used/sampler_used 填实际值，:276-277）。

### 交叉引用与登记语义

- 上游: SCI-P3-001（FROZEN，「显式拒绝清单」一节/「逐条追溯」一节/a-1/-5/-7/-8/-10）引用不改动；
 ALG-P3-003（「权重对象的归属」一节 G3/G4 施工规格，公式零改动）；
 ALG-P3-RSMP-IMPL-001（实现级合同）；DATA-P3-RES；
 DATA-P3-WCS（，输出平面几何上游）。
- 边界硬约束：bilinear 离散化方案以 G4 施工规格为准；tile cache
 淘汰策略为 FIFO；`p3_resample_check_mode` 的会话接线面 = 探针
 消费；`provenance.missing_tiles` 冻结为 nullptr（字段不产生）。
 完整分界见 ALG-P3-RSMP-IMPL-001 。
- 现状执行测试：`lib/algorithms/resample/tests/p3rsmp/p3_resample_probe_main.cpp`
 （探针）+ `eng/tests/backend/test_p3_resample.py`（seam/NaN/无静默默认）+
 `eng/tests/unit/p3_interp_test.cpp` / `eng/tests/unit/p3_coverage_test.cpp`（独立
 参考实现）作为相邻证据引用不冒认；验收级执行测试见 ALG-P3-RSMP-IMPL-001。
- 上游与镜像：SCI-P3-001（FROZEN）/ ALG-P3-RSMP-IMPL-001 /
 DATA-P3-RES；域际 API-P3-FITS-001（FITS 写出消费面，resampled
 平面为其输入）；镜像 API-P3-001（FROZEN 编排面，不因本节改动）。

## 消费面权重口径（API-V6-WEIGHTMODE-001）

> 语义权威：`docs/science/unified/DATA_SEMANTICS` ；最高设计
> `../../ACSD_DESIGN.md` 「数据对象」一节（权重的产生链固定为两步）与
> 「边界条款」一节（单一权重口径）。
> 生产 schema：canonical 对象层 `eng/contracts/schemas/unified/*.schema.json` 的 `allOf`；
> 产品族记录层 `eng/contracts/schemas/product_family_field_constraints.schema.json`；
> 词表与映射：`eng/contracts/data/clause_registry.json#weight_vocabulary` / `#migration_map`。
> 本节只定义**消费面语义**，不实现公式、不改既有 API/ABI 布局。

### 权重来源配置面（`weight_mode` 键不存在；`../../ACSD_DESIGN.md` 「数据对象」一节（数据对象）：全链没有「权重模式」这一可选概念）

| 项 | 内容 |
|---|---|
| 有效来源 | `point_information` / `surface_gls`（显式声明，切换只经配置） |
| 拒绝来源 | `psfsw_robust` / `auto` / `support_x_snr2` / `psf_snr_power` / legacy 整数 `0` / `1` / `2` / 未知串：声明即**显式拒绝并返回迁移提示**（迁移到 `point_information` / `surface_gls`），接受集 = 空 |
| 文档基线值 | `equal` / `pixel_ivar`（仅作基线对照，非科学最优声明） |
| 键面 | Phase2 mosaic write 消费面 `P2Stage2Config` 不含 `weight_mode` 字段（`stage2_common.h`；`../../ACSD_DESIGN.md` 「数据对象」一节（数据对象））；`integration.weight_mode` 出现即拒绝（本文「Phase2 mosaic write 公共消费面」；`DATA_SEMANTICS` ） |
| 词表落点 | `eng/contracts/data/clause_registry.json#weight_vocabulary`（`canonical_fields` + `dual_mapping` + `legacy_integer`） |

**legacy 整数处置（reader 规则，唯一）**：**全值域一律拒绝** —— `0=support×snr²`、`1=equal`、`2=pixel_ivar` **三者同等拒绝**。

判据依据：该字段**不存在任何合法取值** ⇒ 出现即 fail-closed 具名拒绝（`../../ACSD_DESIGN.md` 「数据对象」一节（数据对象）「全链没有「权重模式」这一可选概念」；「边界条款」一节「**没有可选择的口径**：不存在口径选择键、口径枚举、口径配置项或口径产物」）。实现事实源：`lib/algorithms/coverage/src/stage2_common.cpp` 与 `lib/infrastructure/scheduler/src/module_adapters.cpp`（键出现即拒绝）；CLI 面 `lib/infrastructure/cli/runtime_contract.h` 的 `route_legacy_weight_mode_int` 是**纯拒绝面**（0/1/2 与任意整数 → `kReject`）。

**登记面与输入路径的分界**：「登记面」= 描述既有数据对象形态的名词；「输入路径」= 决定生产接受什么的动词。冻结适用面 = 后者；`eng/contracts/data/clause_registry.json#weight_modes` 与 `eng/contracts/schemas/product_family_field_constraints.schema.json#/$defs/weight_mode` 的词表登记不构成接受集。判据（唯一可判定式）：**凡出现在「配置读取 / 路由 / 解析」路径上的 legacy 权重域 token（`auto` / `ivar` / `equal` / `support_x_snr2` / 整数 `0`|`1`|`2` / `weight_mode` 键 / `legacy_allow_weight_fallback` 键）一律 fail-closed 具名拒绝**；仅用于描述既有对象形态的枚举与映射不构成输入面，生产可用的判据 = 接受集本身。生产 writer 只写显式字符串来源。

### 权重对象的归属

对象 `psfsw_robust_weight` 不在数据对象集内（当前对象集 = 13 个，权威 =
`../../ACSD_DESIGN.md` 「数据对象」一节 数据对象表）。权重是阶段二按天球像素对应
帧集合现场算出的派生量。

包含此对象字段的词表与迁移映射由 `eng/contracts/data/clause_registry.json`
承载；`weight.kind` / `weight.units` / `weight.group_normalized` /
`weight.normalization.{scope,median_target,constants_version}` / `weight.weight_value` 不进
生产 schema（生产 schema 不出现别名字段）。`point_information` 的 canonical
单位 = `ADU^-2`；`surface_gls` 权威式 = `x_hat=(A^T C^-1 A)^-1 A^T C^-1 d`。

### 消费失败语义（fail-closed，无反例回退）

- 权重来源未知 / legacy 整数 0 / `psf_snr_power` 进生产 → 拒绝（回退面 = 空，任何自动权重均不在回退面）；
- 权重来源含诊断量（`median_source_snr`/`median_snr`/`support`/`coverage`/`fwhm`/`residual`/…）→ 拒绝；
- 诊断面的无量纲相对权重（`Var=1/W_psfsw` 之类）不进权重面；`ivar`/`variance`/`sigma`/`fisher`/`w_info`/`w_psf` 为排除键（四概念分离由 `DATA_SEMANTICS` 承载）；
- 三条生产来源缺 effective PSF（只给 FWHM 标量不算）→ 拒绝；`parameter_effectiveness` 证明参数确实进入组合系数与 effective PSF，否则拒绝；
- provenance 缺最小集键 / 单位不可判 / `unavailable` 无 reason → 拒绝。

### 边界条款

- 帧级 `median(SNR_F)` 只作诊断/深度表达，落点 = 诊断/深度面。
- UPM fit 的 `upm_weight_source`（`DATA_SEMANTICS` ）与 `use_ivar_weight` 是拟合内部诊断开关，**不是** Phase2 集成权重枚举；两者各自具名、互不映射。
- 验证：`eng/tests/contracts/product_family/`（独立 Oracle 对照条款注册表 + 负向 mutation）。

---

## 分阶段 API 面

以下按命令组织。每个公共 API 面给出范围界定、导出符号、签名要点与内存所有权、调用时序、
返回码、单位与数据形状、线程安全与确定性、生产调用方与编排现状、行为边界。符号清单以公开头
文件为签名权威，头文件的行号锚随同提交更新。

### `normalize` 的编排级生命周期合同（命令行处理器直调）

> CLI-001 说明: 本节四段式 C ABI **不变**(内部会话 1 的冻结合同);**用户命令名** = `normalize`
> —— `phase1 run` 不在命令面上并返回 rc=2, 由 `normalize --json <config.json>` 承载
> (ACSD_DESIGN ; phase 仅为内部指代)。

```c
/* 四段式: create→validate→run→inspect;opaque handle, owner=创建者 */
acsd_status p1_session_create(const acsd_host_services_v1* host, acsd_handle* out); /* host services 单结构注入(budget/cancel/logger/allocator) */
acsd_status p1_session_validate(acsd_handle, const acsd_span_u8 config_json); /* 纯读; 无 IO; 幂等 */
acsd_status p1_session_run(acsd_handle, const acsd_span_u8 config_json,
 int async_io_depth); /* async_io_depth∈{0,1,2}(ARCH-004 「request」一节); 取消点=帧粒度 */
acsd_status p1_session_inspect(acsd_handle, acsd_span_u8* out_manifest_json); /* out=host alloc, 调用方释放 */
acsd_status p1_session_destroy(acsd_handle); /* 唯一释放对; 内部 join 后台 IO 线程 */
```

- run 内部阶段序列=stages[](校准→检测/PSF→plate solve→测光定标→SNR→Drizzle→HiPS),与 api/PUBLIC_API.md 的 7 路径一一对应;每 stage 发 stage_start/stage_end+backend 事件(API-002 「显式拒绝清单」一节)。

### 底层模块函数登记（现存头文件为签名权威；此处登记并发合同与测试 ID）

| 函数(头文件) | reentrant | threadsafe | internal_parallel | 取消点 | 直接 test ID |
|---|---|---|---|---|---|
| `ac_generate_master_bias/dark/flat(+_f64)`(astro_calibration.h) | yes | yes(无共享可变) | omp(budget, pixel 域) | 无(短任务) | TST-CAL-001 |
| `ac_calibrate_frame(+_f64)`(同上) | yes | yes | omp(budget, 行带) | 行带 | TST-CAL-001 |
| `ac_correct_frame(+_f64)`(同上, cosmetic) | yes | yes | omp(坏点域) | 无 | TST-CAL-FAIL-001 |
| `ac_set_num_threads(int)`(同上) | yes | yes | — | — | TB-ARCH-004(checker 管控; 由 p1 budget 注入取代, ABI-001 收编) |
| `sdet_create/destroy/detect/detect_ex`(star_detector.h) | handle 级 no | no(单 handle 单线程) | omp(星批) | 星批 | TST-SDET-* |
| `dpsf_fit/batch/batch_f/free_results`(dynamic_psf.h) | yes | yes | omp(星批) | 星批 | TST-DPSF-* |
| `ipv_solve_create/destroy/solve(_from_memory)`(ipv_api.h) | handle 级 no | no | omp(triangle/vote, 帧内) | 帧(星表行块) | TST-IPV-001 |
| `pc_calibrate_simple(_with_gaia)`(photometric_calib.h) | yes | yes | none(IRLS 串行确定性) | 迭代间 | TST-PHOT-001 |
| `snr_noise_model_v1(+_f64/_fill/_free)/snr_noise_gain_variance`(snr_estimator.h) | yes | yes(model 对象隔离, g_model_floor 指针 key) | patch 级 | 行带 | TST-NOISE-001..015 |
| drizzle 引擎(healpix_drizzle) | 帧级 | no(帧序串行) | omp(候选/行带, 固定序归约) | 帧/tile | TST-DRZ-* |

- aliasing: 全部 in/out 不重叠(除标注 in-place 的 normalize_flat);错误码沿用各模块既有枚举(AC_ERR_*/sdet/dpsf/ipv/pc/snr),session 层映射至 acsd_status(表由 CLI-002 落地)。

### 单位与所有权速查（术语见 `../../GLOSSARY.md`，C ABI 见 `abi/ABI.md`）

- 尺寸: w,h(像素), n_frames;曝光: 秒;信号: ADU;方差: ADU²;坐标: 内部 0-based 像素;RA/Dec: deg ICRS。
- 内存: 全部"分配方释放或 host allocator"(函数头注释为准);handle 生命周期=唯一 create/destroy 对;借用(gaia_client_handle 等)不转移所有权。

### 文档符号签名检查项合同（验收）

文档符号签名判据(API-003 建立合同, CLI-002 落地全量, ): 对每个登记函数——① 头文件存在该符号;② 文档表此行存在;③ 签名(参数数)一致;④ 直接 test ID 非空;⑤ 五字段并发合同齐全。任一缺失 FAIL。机器门 = 「request」一节 表 + `eng/tests/api/test_p1_api.py`。

### 阶段流水与所有权图（谁分配、谁持有、谁释放）

```text
coverage ──→ sampler ──→ UPM build ──→ calibrate_block ──→ rejection ──→ integration
(Owned: 调用方 session) (Owned: UPM Model 对象) (borrow model) (borrow stack) (值语义 out)
```

| 对象 | 创建 | 持有 | 释放 | 跨函数传递 |
|---|---|---|---|---|
| `Coverage`(p2_coverage_build) | build | 调用方 | p2_coverage_free | 只读借用 |
| ControlObservation[]/frame_id_cache | 调用方(p2_sample_controls 产出) | 调用方 | 调用方 | 只读借用(cached 版免二次哈希, 同数值语义) |
| UPM Model(void*, p2_upm_build / p2_upm_build_geo / p2_upm_open) | build/open | 调用方 | **p2_upm_close** | calibrate_block/evaluate_c 只读借用;persist 导出副本 |
| CandidateStack(p2_collect_candidate_stack) | collect | 调用方 | 调用方(strided 视图者不 free 底层帧) | rejection/integration 只读借用 |
| P2PixelStack→P2PixelResult | 调用方栈/结果 | 调用方 | 调用方 | p2_integrate_pixel(in const, out 值写) |
| async_io 队列(CON-008) | session | session | destroy | bounded(深度冻结), 无无界缓冲 |

- **无隐藏全局状态**: 唯一模块级资源=g_model_floor(指针 key 注册表, 单线程资源, ARCH-004 已登记)+logger(宿主注入);其余全部经参数/handle 传递。

### 逐函数登记（签名权威为头文件；并发五字段与测试 ID 在此登记）

| 函数 | reentrant | threadsafe | internal_parallel | 取消点 | test ID |
|---|---|---|---|---|---|
| `p2_coverage_build/free` | yes | no(独立对象) | none | 无 | TST-COV-* |
| `p2_sample_controls` / `p2_sample_controls_cached` | yes | no | none(串行=确定性 reference, ARCH-004) | cell 粒度(实验并行) | TEST-P2SAMPLE-* |
| `p2_sampler_default_config` | yes | yes | none | 无 | TEST-P2SAMPLE-* |
| `p2_upm_build` / `p2_upm_build_geo` | yes | no | 块级(worker budget, 固定 control 序) | 整模型(不写半成品) | TEST-UPM-* |
| `p2_upm_calibrate_block`/`p2_upm_evaluate_c` | yes | yes(模型只读借用) | block 内 none | 无(短任务) | TEST-UPM-* |
| `p2_upm_open`/`p2_upm_close`/`p2_upm_info` | yes | no(模型对象) | none(persist IO 串行) | 整模型 | TEST-UPM-* |
| `p2_reject_plan_resolve` | yes | yes | none | 无 | TST-REJ-* |
| `p2_eligibility_filter`/`p2_collect_candidate_stack` | yes | yes | none(收集器 strided) | 无 | TST-REJ-* |
| `p2_validate_candidate_weights` | yes | yes | none | 无 | TST-REJ-* |
| `p2_reject_stack` / `p2_reject_stack_ex` | yes | no(per-sample reason 输出) | 像素行带 | 行带(掩膜帧原子) | TST-REJ-* |
| `p2_integrate_pixel` | yes | yes(纯函数) | none(像素内固定序) | 无(行带由调用方切) | TST-INT-* |
| `p2_large_scale_apply` | yes | yes | 邻域读行带 | 行带 | TST-REJ-* |
| `p2_frame_id` / `p2_stats_median` / `p2_stats_mad` / `p2_rejection_semantic_id` | yes | yes | none | 无 | TEST-P2SAMPLE-*/TST-REJ-* |
| `p2_acr_block_eligible`/`p2_block_plan` | yes | yes | none | 无 | 配置守卫(ACR-IVAR-001; 非 cpu/auto 拒) |

### 线程预算绑定

- `mosaic` 运行预算分配冻结(内部会话 2): sampler=1(串行 reference);upm build=blocks(budget);rejection/integration=行带(budget);async I/O=1;**Σ≤全局 budget**;每 stage_start 事件携带 workers 实际值(API-002 backend 事件)。

### `mosaic` 错误码映射

模块 rc(NO_DATA/INVALID_INPUT/UNDERDETERMINED/rc=2 build fail/OK, SCI-UPM/REJ/INT 冻结)→acsd_status: NO_DATA/INVALID→ACS_ERR_PARAM;rc=2→ACS_ERR_STATE;UNDERDETERMINED→ACS_OK(语义=可继续, final 汇总);映射表由 CLI-002 落地并在 golden 测试断言。

### 与前两阶段同构的检查项

eng/tests/api/test_p2_api.py 机器门: 文档符号↔头文件实跑核对+所有权图完整(七对象)+全局状态显式登记声明+预算绑定引用。

### `export` 会话生命周期合同（与前两阶段会话同构）

```c
acsd_status p3_session_create(const acsd_host_services_v1* host, acsd_handle* out);
acsd_status p3_session_validate(acsd_handle, const acsd_span_u8 request_json); /* 纯校验, 无 IO; 显式拒清单全查 */
acsd_status p3_session_run(acsd_handle, const acsd_span_u8 request_json); /* 取消点=行带; fits 原子写 */
acsd_status p3_session_inspect(acsd_handle, acsd_span_u8* out_result_json); /* host alloc, 调用方释放 */
acsd_status p3_session_destroy(acsd_handle);
```

### `export` 请求字段（`schemas/phase3_request_v1.schema.json`）

| 字段 | 类型/单位 | 约束(ALG-P3/SCI-P3 冻结) |
|---|---|---|
| `source.hips_dir` | UTF-8 path | 必须含合法 properties(ALG-P3-001) |
| `center` | {ra_deg, dec_deg} ICRS | `abs(dec)<=85°`;输出四角同半球 |
| `scale_deg_per_px` | deg/px | >0（**构造下限只有 >0**，与 WCS 往返门无关；但往返**紧门** 1e-8 px 的适用域是 `scale ≥ 0.9″/px`（= 2.5e-4 deg/px），低于该尺度时紧门不适用、报「超出适用域」而非判红，退回全域保守门 1e-6 px —— 见 GATES 「result」一节 G-P1-WCS-BRIDGE / -GLOBAL 与 `p3_wcs_roundtrip_gate`） |
| `width_px`/`height_px` | px | ∈[1,20000] |
| `projection` | 枚举 | **仅 "TAN"**,其他→UNSUPPORTED |
| `sampler` | 枚举 | "nearest"|"bilinear"(默认 bilinear, SCI-P3 a-7) |
| `longitude_parity` | 枚举 | "east_left"(默认, CD1_1<0)|"east_right" |
| `bitpix` | 枚举 | -32|-64 |
| `coverage_output` | 枚举 | "mask"(二值) 单值;通道/weight 模式不存在 |
| `max_tiles` | int | 可降不可升超内存守卫(ARCH-P3 「result」一节) |

### `export` 结果（inspect JSON）

`{run_id, exit_code, output_fits_path, sha256, order_sel_used, sampler_used, provenance{hips_id, manifest_hash, missing_tiles[], software_version}, coverage_stats{covered_px, total_px}, timings}`;输出 FITS 本体=原子产物(S+C 合成或 COV 扩展, 由 CODE-P3 按 API-002 manifest 落实, 二选一在实现中冻结)。

### `export` 显式拒绝清单（输入不明确即确定错误，不做猜测）

| 输入 | 错误 |
|---|---|
| projection≠TAN | ACS_ERR_UNSUPPORTED |
| frame≠ICRS 恒等(galactic/ecliptic) | ACS_ERR_UNSUPPORTED |
| 多通道/RGBA、JPEG/PNG lossy tile、int+BLANK | ACS_ERR_UNSUPPORTED |
| weight/support tile、flux-per-pixel 输入模式 | ACS_ERR_UNSUPPORTED |
| properties 非法/缺键 | ACS_ERR_PARAM |
| abs(dec)>85°(距极点<5°)/输出跨 TAN 半球/W/H 越界 | ACS_ERR_PARAM |
| tile 文件缺失 | **非错误**:coverage=0+provenance.missing(SCI-P3 ) |
| tile 内 NaN | 非错误:S=NaN+C=1 |
| IO/运行失败 | ACS_ERR_IO/安全中止(ARCH-P3 「显式拒绝清单」一节) |

- variance/ivar 子产品输入**不属拒绝项**（SCI-P3 a-10 / DATA-P3-UNC-001）：输入 HiPS 含
 variance/ivar 时必须显式消费传播，输出 `VARIANCE`/`IVAR` 扩展 HDU；两者皆无时显式
 `unavailable`（禁静默）。实现锚：`lib/algorithms/resample/p3_resample.cpp`
 （`p3_uncertainty_open` / `p3_uncertainty_propagate`）、`lib/phase3_session/p3_export.cpp`
 （`VARIANCE` HDU 写出）。失败路径错误码（`p3_uncertainty_open`，`p3_resample.cpp`）：
 参数 NULL → `P3_RS_PARAM`（p3_resample.cpp）；properties 读取失败或 sampler 分配/HiPS 打开失败 →
 `P3_RS_IO`（p3_resample.cpp 三处，fail-closed 不静默跳过）；properties 键集解析失败或 order 与
 signal 覆盖面错位 → `P3_RS_PARAM`（p3_resample.cpp，无 silent default）；variance/ivar 两者皆无 →
 `P3_RS_OK` + `unavailable`（合法非错误，p3_resample.cpp）。

### 逐条追溯

| API 声明 | 锚 |
|---|---|
| request 字段约束 | ALG-P3-002 / ALG-P3-003(G1/G3)+SCI-P3 a-4/5 |
| 拒绝清单 | SCI-P3 a-1/8/10+(逐行同源) |
| sampler 默认 | SCI-P3 a-7(bilinear) |
| result/provenance | ALG-P3-004(G5)+API-002 final 事件 |
| 取消/原子性 | ARCH-P3 「result」一节(行带+整文件原子) |

### 机器检查项

eng/tests/api/test_p3_api.py：生命周期五函数/request 十字段/拒绝清单与 SCI-P3 文本同源交叉核对/锚点齐。

## 参考文献

[1] 内部文档 `../../ACSD_DESIGN.md`，最高设计的命令行、错误传播、模块与 ABI 三章。

[2] 内部文档 `../contracts/LOG_AND_ERROR.md`，日志与错误合同。

[3] 内部文档 `abi/ABI.md`，公共 C ABI 基础层。

[4] 内部文档 `../contracts/RUNTIME.md`，运行时与模块公共合同。

[5] 内部文档 `../architecture/MODULE_MAP.md`，模块与导出符号登记。
