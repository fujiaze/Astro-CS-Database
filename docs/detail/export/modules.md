# Export 模块与接口

本页按生产依赖组织节点接口，依据[阶段管线](pipeline.md)和所引科学、工程合同。每节说明输入输出、公开头、所有权、错误及实现边界；库能力、局部会话与 CLI 生产节点分别标识。

| 模块 | 调用职责 |
|---|---|
| [properties](#properties) | Export 产品属性读取 |
| [wcs](#wcs) | Export WCS 与裁剪几何 |
| [resample2](#resample2) | Export 单信号重采样 |
| [writer](#writer) | Export 平面 FITS 写出 |
| [verify](#verify) | Export FITS 读回复核 |

## properties

**Export 产品属性读取。**

properties 节点确认输入 HiPS 的信号、几何、层级与单位，形成下游计划所用的元数据。输入来自已发布单 signal 产品；不要求 Mosaic 提供 variance/ivar/support 图像层。阶段边界见 [Export 管线](pipeline.md)和[HiPS 到 FITS 正本](../../science/projection/HIPS_TO_FITS.md)。

### 生产入口与交接

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_properties` 对应 `acsd.phase3.properties`，通过 `p3n_guard_input_units`、`p3_sampler_open_ex` 及 properties parser 读取真实产品，写 `p3_props.json`。下游 WCS、resample、writer 消费其输入路径、实际最大 order、tile_width、BUNIT 与能力状态。

公开读取入口为 [p3_resample.h](../../../lib/algorithms/resample/p3_resample.h) 的 `p3_sampler_open_ex(product_dir,out,out_order,out_bunit,err)`；底层文本解释由 [hips_properties.h](../../../lib/algorithms/coverage/hips_properties.h) / [hips_properties.cpp](../../../lib/algorithms/coverage/hips_properties.cpp)承担。不是另一个科学像素算法。

单位身份的判定本体只有一处：[p3_input_unit.cpp](../../../lib/algorithms/resample/p3_input_unit.cpp) 的 `p3_input_unit_verdict`（Issue #91）。`scheduler 入口`（`p3_op_properties`/`p3_op_resample` 经 `p3n_guard_input_units`）与`正式 session 入口`（`p3_session_run` 经 `p3_sampler_open_ex`）都经它判定，同一输入文件、同一身份条件下 verdict 必须相同；`p3_sampler_open_ex` 只回填裁决给出的 canonical 串，裁决未通过时不回填任何单位。未通过时：缺失/空串/重复（含大小写别名）/声明面冲突/表外语汇 → 输入错误（CLI 3）；可解析但非面亮度/与 `ACSD_PIXEL_SEMANTICS`、`ACSD_PIXEL_AREA_POWER` 矛盾 → 科学语义错误（CLI 4）；两者都不产出 FITS。

### 字段、寿命和错误

| 字段 | 语义 |
|---|---|
| hips_dir、hips_order | 实际读取路径和层级上限，不能只从请求复制 |
| tile_width、ordering、frame | 512、NESTED 和可解释天球坐标；非法数据按 reader 错误上行 |
| bunit、bunit_input | adapter 接受的 canonical 串及实际输入声明；解析结果与实时守卫分叉要报错 |
| pixel_semantics | 球面面亮度；不是每叶积分通量 |
| uncertainty_available/source | 当前单信号路线上 false/none；不探测并不等于证明输入从未有模型 |

sampler 由 open 获取，在元数据读取后 close；输出字符串/JSON 由调用方持有。解析本身无内部线程预算，节点资源和取消由 Runtime 管理，文件通过 AIO。坏 properties、open 失败、非法单位/ordering/尺寸应保留具体错误；不能用猜测默认把坏产品改成兼容。

科学合同要求单位继承真实光度尺度，当前 reader/adapter 的固定 canonical 单位支持不能替代该要求。不得因某个库头含默认 ADU/sr 就给相对标度信号制造物理单位；相邻实现边界见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

### 验证入口

从 `p3_op_properties`、`p3_sampler_open_ex` 与 parser 进入核对缺必需键、显式非法值、NESTED/阶数、BUNIT 分叉和单信号产品。合法零/负信号无需在属性阶段拒绝；没有输入误差层也不妨碍正式导出。本页未运行测试，属性可解析不能充当 WCS 或图像保真证据。
## wcs

**Export WCS 与裁剪几何。**

本节点从输出中心、尺寸、角尺度、方向与投影请求生成平面 WCS，供采样、writer 和 verify 共用。数学与输出被估量依 [HiPS 到 FITS 正本](../../science/projection/HIPS_TO_FITS.md)及 [Export 管线](pipeline.md)。

### 实际入口和公开 API

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_wcs` 对应 `acsd.phase3.wcs`，读取 `p3_props.json`，调用 `p3_wcs_make` 并解析裁剪，保存 `p3_wcs.json`。生产 adapter 当前传 rotation_pa_deg=0；库有旋转参数不表示 CLI 已实际使用用户旋转。

公开头 [p3_wcs.h](../../../lib/algorithms/projection/p3_wcs.h)，实现 [p3_wcs.cpp](../../../lib/algorithms/projection/p3_wcs.cpp)。核心入口：

| 接口 | 作用 |
|---|---|
| `p3_wcs_validate_request` | 校验 projection/frame/coverage_output 的实际支持 |
| `p3_wcs_make` | centre、scale_deg_per_px、width/height、parity、PA → P3WcsDescriptor |
| `p3_wcs_pix2world/world2pix` | 零基像元中心 ↔ ICRS degree，返回具名状态 |
| `p3_wcs_fits_keywords` | 生成 CTYPE/CRPIX/CRVAL/CD/CUNIT 文本 |
| `p3_wcs_applicability`、`_check_applicability`、`_roundtrip_gate` | 当前实现范围与数值诊断，不能当投影理论的通用禁域 |
| `p3_crop_window_from_sky`、`p3_crop_apply_wcs` 等 | 唯一裁剪几何；不增加一次插值 |

投影注册在 [p3_projection_registry.h](../../../lib/algorithms/projection/p3_projection_registry.h)及其实现，当前生产仅 TAN。配置枚举中的 SIN/CAR/AIT 等不能列成已实现；不支持请求明确返回 UNSUPPORTED。

### 坐标、裁剪和单位

`P3WcsDescriptor` 的 CRVAL 为 ICRS degree，CD 为 degree/pixel，CRPIX 为 FITS 一基连续参考像元，width/height 为 px。变换入参/返回像元为零基中心；不重复加一，不再引入半像元。RA 归一与手性必须独立核对。

`P3CropWindow` 内部为零基半开区间 [x0,x1)×[y0,y1)。像素配置采用正式配置声明的一基闭区间，经唯一 adapter 转换；sky crop 由已定义 WCS 转整数窗口。两种形式互斥，越界/空窗明确报错。

子窗 CRVAL/CD 保持，CRPIX 减整数窗口原点，尺寸改为子窗尺寸。writer/verify 读同一 plan，裁剪不再重采样；因此外部合法 CRPIX 不必位于子窗中心。`p3_crop_window_from_sky` 采用有限密度边界采样，不能据此宣称对连续曲线极值已有严格包络保证。

### 所有权、错误和验证入口

descriptor/窗口由调用方持有，函数同步、无内部线程或文件 I/O，元数据 FP64；Runtime 管理节点与并行调用。状态为 OK、PARAM、UNSUPPORTED、HEMISPHERE，crop 有独立状态，不能直接冒作 CLI 退出整数。

当前 |Dec|、画幅/FOV 等检查是软件能力限制；TAN 奇点取决于距切点角距离。内部往返诊断不足以发现两向同错，必须对独立 WCS reader 作绝对坐标比较。

验证从本头 make/变换/crop 及 [WCS 单元入口](../../../eng/tests/unit/test_wcs_plausibility.py)进入：非零中心赤纬、RA wrap、旋转/手性、非对称裁剪、CRPIX 原点和未支持投影。已有函数/门限不等于完成外部 Oracle 或所有天区验证；本页未运行测试。

## resample2

**Export 单信号重采样。**

本节点将已有 HiPS 信号采样到目标平面，不进行新的光度、背景或多帧权重求解。核的物理含义、缺数政策与二次重采样影响取自[重采样正本](../../science/resample/RESAMPLE.md)和 [Export 管线](pipeline.md)。

### 生产入口与工作载体

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_resample` 对应 `acsd.phase3.resample2`，消费 props、wcs_plan 和实际 HiPS。它读取 `p3_props.json`、`p3_wcs.json`，选择实际 order，经 Runtime executor 按子块采样，用 AIO 位置写产生 `p3_resampled.bin`，并写 `p3_resampled.json`。工作二进制按未裁剪平面连续存 f32 signal、coverage；后者是内部有效域对象，不是最终 FITS 科学层。

`acsd.phase3.resample` 另为聚合 session 适配描述符，不能与本五节点生产 operation 混称。reader、WCS 与写出有各自责任。

**子块 work unit 的失败语义（Issue #91/E1-R）**：并发 worker 各自打开自己的 sampler（独立数据集句柄，共享同一有界 tile 缓存）。任一 worker 的 `p3_sampler_open_ex` 失败时，失败**不得**被计入正常完成：节点以线程安全状态保留首错的机器码（`P3-RESAMPLE-WORKER-OPEN-INPUT` / `-IO`）、打开目标与输入产品身份，在全部 worker 退出后显式判红；已写入临时对象的 `p3_resampled.bin` 被丢弃删除，不执行原子 rename，下游 writer/verify 因 DAG 依赖不运行 ⇒ 无 FITS、无 complete manifest。成功 run 的子块计数随 `p3_resampled.json` 的 `worker_bands`（planned/executed/active_peak/sub_blocks）落盘，供独立核对「任务数与产品一致」。故障注入仅经既有 `ACSD_RT001_FAULT` 测试钩子（`resample_worker_open_fail[:k]`），生产不设该变量即不可达。

### 公开 API 与字段

[p3_resample.h](../../../lib/algorithms/resample/p3_resample.h)和 [p3_resample.cpp](../../../lib/algorithms/resample/p3_resample.cpp)提供：

| 接口 | 消费合同 |
|---|---|
| `p3_sampler_open/open_ex`、`_close` | 产品→sampler；open_ex 返回实际 order/BUNIT，非成功带原因 |
| `p3_order_select` | 从现有层级与输出 degree/pixel 选 order；放大不增加真实分辨率 |
| `p3_sample_nearest` / `_nearest_ex` | 取包含 sky 的叶；ex 暴露 leaf ipix |
| `p3_sample_bilinear` / `_bilinear_ex` | 局部切平面四象限叶中心插值，ex 暴露有效权重/叶号 |
| `p3_sample_bilinear_nanmask_ex` | 同一数值路径，另返回逐类剔除/合格计数 |
| `p3_sampler_set_max_tiles`、`_attach_cache`、`_cache_stats` | 共享有界只读 tile/缺失缓存及命中读数 |

sky 为 ICRS degree；叶索引为 NESTED，tile=512，索引/FITS 手性复用共享 HEALPix。signal 沿实际球面面亮度单位，不按单叶通量解释；有限零与负值保留。局部 bilinear 不等于真实球面面积交叠，当前生产不能把 area-overlap 列为已接入核。

### NaN、支持与资源

nearest 的合法无覆盖（`OUTSIDE_DECLARED_FOOTPRINT`，已证实）返回 NaN、coverage=0，tile 存在但像元非有限可返回 NaN、coverage=1。Issue #87 起，声明内缺失（`DECLARED_MISSING`）、FITS 损坏（`INVALID_TILE`）、读取 I/O（`IO_ERROR`）、校验失败（`CHECKSUM_FAILED`）与覆盖身份不足（`FOOTPRINT_UNDETERMINED`）不再折叠成 NaN/coverage=0：采样返回 `P3_RS_INTEGRITY`/`P3_RS_IO`，`p3_op_resample`/`p3_session_run` 在写出前 fail-closed。Issue #87/E2-R 起，无可靠覆盖来源的缺失（`UNKNOWN_LEGACY_COVERAGE`）与已证实无覆盖分开：仍返回 NaN/coverage=0 的 best-effort 可视图，但 `coverage_verified=false`/`input_completeness=UNVERIFIED_LEGACY` 必须随 resampled/writer/verify/manifest/FITS 头（`COVSTAT`/`COVVERIF`）发布；verify 节点对三处身份同源对拍。bilinear 区分缺 tile 与已读邻域的无效样本；后者掩除并按剩余有效权重重归一，零合格样本不制造有限信号。coverage 是可用 tile/支持语义，不自动代表数值有限或精度。

sampler 单实例由一个执行线程使用；worker 可各自 open，再 attach 到同一受控共享 cache。不能继续写成每 worker 必有完全独立缓存或总容量必乘 worker 数。缓存只改变 I/O，不改变 order/核；输入子产品不同不得共享同键 cache。sub_block_px、queue_depth、max_tiles 由实际配置/资源解析控制，不能由接口存在推断任意尺寸的资源实测已满足。

### 模式、错误与科学边界

当前 adapter 要求 output_mode，接受 surface_brightness/visualization，明确拒绝未实现 point_source_flux；这些是实现事实，不把它们改写为最高科学目标。当前正式路径不探测/传播 uncertainty，单信号输入不能反演协方差。

头中 `p3_uncertainty_open/propagate` 等保留库能力，不构成正式 export 已消费误差层。P3ResampleStatus 区分 PARAM、UNSUPPORTED、IO；输入损坏和合法缺数分别处理，取消经 Runtime/AIO 上行。

### 验证入口

[Export 管线测试入口](../../../eng/tests/pipeline/test_stage_export.py)与本头采样函数用于核对生产、正/负缓存、实际读取量及科学值不变；[Export 管线](pipeline.md)定位 production。独立检验常值、梯度、点源孔径/FWHM、RA wrap/极区/face/tile 边界、NaN 重归一和真实空洞。不得以自读自写或平滑外观宣称光通/噪声完整正确；本页未运行测试。

## writer

**Export 平面 FITS 写出。**

writer 把重采样信号写为普通二维 WCS FITS。当前生产仅 PRIMARY signal，一个 HDU；不要求输入马赛克具有 variance/ivar，不从单信号制造不确定度。产品和单位要求见[HiPS 到 FITS 正本](../../science/projection/HIPS_TO_FITS.md)与 [Export 管线](pipeline.md)。

### 实际入口与发布路径

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_writer` 对应 `acsd.phase3.writer`。输入 `p3_resampled.json/.bin`、`p3_wcs.json` 和 run_context，按同一 crop plan 取子窗。生产使用 `P3FitsStream` 和有界子块 scheduler 写 `output_phase3.fits`，并写 `p3_writer.json`；`unc=false`、`nplanes=1` 明确单信号。整幅 API 仍是库/故障对照能力，不能替代生产子块路径描述。

公开头 [p3_output.h](../../../lib/algorithms/fits_output/p3_output.h)，实现 [p3_output.cpp](../../../lib/algorithms/fits_output/p3_output.cpp)。WCS/裁剪复用 [p3_wcs.h](../../../lib/algorithms/projection/p3_wcs.h)。

### API 与文件合同

| 接口/对象 | 语义 |
|---|---|
| `P3FitsStream.open` | output_path、WCS、W/H、BITPIX、BUNIT、provenance → 同目录私有临时文件 |
| `begin_hdu/write_block/end_hdu` | plane=0 的信号子块；x0/y0 零基、w×h 行主序 f32 |
| `publish` / `abort` | flush/close/fsync、校验/hash、原子改名，或清理临时对象 |
| `p3_output_write_atomic` / `_ex` | 整幅缓冲入口；ex 的误差参数存在不表示本命令发布误差扩展 |
| `P3OutputResult` | sha256、reopen_ok、coverage_ok、covered_px、total_px |
| `P3Provenance` | 输入 hash、软件/运行身份、order、sampler 和可用性说明 |

最终图像 BITPIX 为 -32 或 -64，由真实配置传入；WCS 的 CRVAL/CD 为 degree/degree-per-pixel，CRPIX 为 FITS 一基。裁剪用相同源子窗，CRPIX 减原点，不能重新拟合 WCS或额外插值。无有效贡献 NaN，有限零与负信号合法。

科学要求 BUNIT 继承实际光度尺度；当前 adapter 仍检查固定面亮度 canonical 字符串。该检查的存在不证明 response-calibrated/relative 输入单位链已闭合，相邻限制依[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

### 所有权、并行与错误

调用方拥有源子块，写调用期间保持有效；stream 拥有临时文件和内部状态，析构/abort 按实现清理。FITS 写句柄串行，读取/搬运可经 Runtime 预算并行；队列与在途块受配置限制。内部 coverage 可用于计数，最终不建 COVERAGE 扩展。

P3OutputStatus 为 OK、PARAM、IO、CANCELLED，`p3_output_last_error` 仅诊断，不作状态真源。缺 run/hash、工作数组截断、非法几何、I/O/hash/重开失败要上行；单 signal 的 uncertainty unavailable 是能力声明。原子文件发布依[发布合同](../../engineering/contracts/ATOMIC_PUBLISH.md)，不能由最终路径存在推断全部运行成功。

### 验证入口

从 `P3FitsStream`、`P3FitsVerifyStream` 及[生产 verify 卡](modules.md#verify)进入；独立 reader 核对单 HDU、尺寸/BITPIX/WCS、真实单位、子窗像素、checksum/hash、取消/故障残留与大图资源。代码含流式路径不等于本页执行了内存/性能验证，也不等于完成科学保真或发布验收。

## verify

**Export FITS 读回复核。**

verify 重开已发布 FITS，核对声明、结构、像元与完整性；它不重做采样，也不把内部自洽称为独立天文真值。产品与坐标要求见 [Export 管线](pipeline.md)和[投影正本](../../science/projection/HIPS_TO_FITS.md)。

### 生产入口与交接

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_verify` 对应 `acsd.phase3.verify`，读取 `p3_resampled.json/.bin`、`p3_wcs.json`、`p3_writer.json`，从 writer 记录定位实际 FITS。它用同一裁剪 plan 生成预期窗口，使用 `P3FitsVerifyStream` 分块比对，写 `p3_verify.json`。生产 `unc=false` 且只校验 PRIMARY signal，不能要求不存在的附加 HDU。

公开头 [p3_output.h](../../../lib/algorithms/fits_output/p3_output.h)及[实现](../../../lib/algorithms/fits_output/p3_output.cpp)还提供整幅 `p3_output_verify` / `_verify_ex`。各入口的能力与生产调用分开，不将头注释当运行证据。

### 校验对象、坐标与所有权

| 检查/接口 | 合同 |
|---|---|
| `P3FitsVerifyStream.open` | 只读重开，预期尺寸、HDU 面和 WCS，has_uncertainty=false |
| `check_block(plane,x0,y0,w,h,expected)` | plane=0，零基子块，对实际源窗口逐像元比对 |
| `close(result)` | 汇总 reopen/coverage 结果并求实际文件 sha256 |
| 元数据核对 | resampled/writer 的 BUNIT、output_mode、measurement_capable 一致 |
| 产品 hash | verify 独立重算 canonical hash，再与 writer 自述比较，不直接采信写侧值 |

WCS 使用 FITS 一基 CRPIX、ICRS degree 和 degree/pixel CD；crop 后按实际输出尺寸校验，不能强制 CRPIX 仍居中。NaN 按约定同态比对，真实零/负值作为正常信号。缺 tile 或内孔可产生合法 NaN；是否异常丢失必须与输入有效域/核支持比较，不能仅按洞的位置判断。

stream 持有读句柄，调用者持有期望子块并复用有界缓冲；校验串行读，不自建线程预算。输入工作文件须来自同次运行/plan，不能读到陈旧目录后把恰好相同的尺寸当同一产品。

### 错误与验证边界

重开、尺寸、HDU、WCS/单位、像素、checksum/hash 或摘要分叉通过非 OK/Result 错误上行，最终状态不写成 verified。局部状态码依 P3OutputStatus，不直接冒作 CLI 退出码。取消与临时对象清理由 Runtime/统一 I/O 管理，不把读后发现错误等同此前从未发布文件。

本节点证明生产写读相符与文件完整性的实际范围；相同 WCS 公式两端同错仍可能通过。绝对 sky/手性、独立源通量、有效 PSF、低频星云与噪声统计须用外部 WCS/HEALPix reader、已知天空模拟或真实观测另验。

验证入口为本页 stream/整幅接口、[WCS 单元入口](../../../eng/tests/unit/test_wcs_plausibility.py)与生产五节点调用。应注入头/像元/hash 篡改、截断、非对称 crop、工作文件版本分叉及合法空洞。本页未运行验证，不继承无复现载体的通过结论。


## D5｜P2→P3 单 Signal 响应与 provenance 正式施工合同（Issue #79）

本节补全[Phase2 Writer 算法](../../science/algorithms/mosaic/PHASE2_MOSAIC_WRITE.md)所定义的跨阶段身份。**生产已核事实**：`p2_op_write` 在 `p2_final.json` 保存 `obs_filter/response_distribution`、`signal/properties` 保存名义 `obs_filter`；`p3_op_properties`（`module_adapters.cpp` 16055 起）目前只从 `p3n_guard_input_units/p3_sampler_open_ex` 获取 BUNIT/order，不读取前述响应；`p3_op_writer`（16614 起）从 resampled/WCS/run_context 构建 FITS，未绑定 P2 response。#79 的真实 `acsd export` exit0 后 `p3_props.json` 与 `output_phase3.fits` 均缺名义通带/响应身份。以下是**TARGET/NOT_RUN**，不宣称新的机器 Schema 或 C++ 已实现。

| 函数/生产者 | 明确选择的输入、输出与消费 | 错误策略 |
|---|---|---|
| `p2_op_write` | 当前 sole `signal/` 和 `p2_final.json.obs_filter/response_distribution`；目标为每帧 `frame_id,input_occurrence,source_sha256,nominal_filter,filter_curve_sha256,qe_curve_sha256,photometric_application_id,sky_model_hash` 与 signal revision 同步绑定，及可用支持域、缺项 reason | 若有名义标签却缺真实曲线，明确 `nominal_only`，不冒称具物理可求值 `R_eff` |
| `p3_op_properties` | 必须先解析已完成 P2 manifest、`signal/properties`、`p2_final.json` 并验证同一 revision/hash/BUNIT/order；新增 `p3_props.json.obs_filter,response_distribution,response_identity_refs,response_provenance_sha256,response_status` | P2 宣称有字段而 P3 不读取/冲突为 DATA_CONFLICT；文件/哈希错误为 DATA_CORRUPT；独立外部合规非 ACSD HiPS 来源写 `unknown_origin`，不伪造 P2 |
| `p3_op_wcs` 与 `p3_op_resample` | 原 WCS、crop 与 signal 重采样保持数值定义不变；真实 BUNIT 与响应/输入身份透传到 `p3_resampled.json` 及运行 manifest | 传递身份不等于做新的滤镜/增益转换；同一 run/response 不一致时拒绝 |
| `p3_op_writer` / `P3FitsStream` | 仅一个 PRIMARY signal FITS HDU；BUNIT 为真实 signal 单位；`FILTER` **只在全部源帧非空同一名义标签**时写；新增目标 FITS 关键字 `ACSDRESP=SINGLE|UNKNOWN|MIXED` 与 `RSPHASH`（配套完整响应 JSON 的 64 位 hex SHA-256） | 不把 `mixed:m_passbands` 填进 FILTER；不得用短 hash 冒充完整；FITS 8 字符关键字/quoted hash card 需在真实 CFITSIO 写读验证 |
| `p3_op_verify` 与独立读侧 | 重开真实 FITS、核对 WCS/crop/BUNIT/FILTER/ACSDRESP/RSPHASH 与 sidecar；验证本次 P2→P3 revision、科学数值误差范围和运行 ID | 不能把 FITS 自读成功当空间有效响应或 P5 无接缝的科学证明 |

### D5.1 名义通带响应三态的完整定义

`labels` 是全部真实参与的每帧声明多重集，包括**已声明空值**。`counts` 用确定性有序 key，`obs_filter` 仅当所有相等时取原单值（可以是空），否则取 `mixed:<distinct-key-count>_passbands`，其中 distinct 数包含空 key。目标 Export 表达：全部 Halpha→`ACSDRESP=SINGLE,FILTER=Halpha`；全部空→`UNKNOWN` 且不写 FILTER；`["",Halpha]`、`[Halpha,""]`、`[Red,Blue]`→`MIXED` 且不写单 FILTER。完整 `response_distribution` 及曲线身份必须存可通过 `RSPHASH` 定位和验证的配套 JSON；若 only nominal data，明确 `response_status=nominal_only`，不是 `physical_response_validated`。真实 \(R_{\mathrm{eff}}(\lambda;\omega)\) 还需要每帧响应、位置依赖权重、PSF/Drizzle、rejection 集合及标度状态，不可从 label/count 推导；该研究归 #14/#76，不限制用户混合不同滤镜。

### D5.2 真实生产数据与独立 oracle

以 #11 已通过的标签-only 矩阵为基线复跑：全同 Halpha、全 unknown、unknown+known 顺序互换、Red+Blue、unknown+Red+Blue 六种排列。逐条执行当前 CLI Mosaic→Export，再以外部 CFITSIO/astropy 检查 P2 `p2_final`、P3 `p3_props` 和 FITS FILTER/ACSDRESP/RSPHASH/侧车 hash 的同一性；**仅修改 FILTER 名义标签不应改变 science DATA**（除 Export 自身固定算法的声明舍入），而 FITS Header 必须跟随身份变化。负例：删 `obs_filter` 属性键（与有空值不同）、换 run/model/source SHA、损坏完整 sidecar、跨 revision 混拼、BUNIT/leaf_order 冲突，明确拒绝。真实不同光谱响应的 M42/星云结构无偏性由 #14/P5 独立合成与真实 E2 证明，不用标签不变性自证。此 D5 只冻结文档，**不修改 C++、不更新机器 Schema、不声称已运行或已关闭 #79**。


## D7-A｜P3 WCS / crop / Phase1 Drizzle 交叉引用下的当前 ABI

**当前 P3 生产有完整 DAG 五段**：module_adapters.cpp::p3_op_properties → p3_op_wcs → p3_op_resample → p3_op_writer → p3_op_verify，P3 session 旧兼容入口不能替代 DAG 的真实资源说明。真实 p3_wcs.h/cpp 的 C++ 命名空间为 acsd::phase3；主要 API 签名如下（完整声明以头为准）：

~~~cpp
P3WcsStatus p3_wcs_validate_request(
    const char* projection, const char* frame,
    const char* coverage_output, std::string* why);
P3WcsStatus p3_wcs_make(
    double centre_ra_deg, double centre_dec_deg, double scale_deg_per_px,
    int width_px, int height_px, const char* parity, double rotation_pa_deg,
    P3WcsDescriptor* out, const char* projection="TAN");
P3WcsStatus p3_wcs_pix2world(
    const P3WcsDescriptor* d, double x, double y,
    double* ra_deg, double* dec_deg);
P3WcsStatus p3_wcs_world2pix(
    const P3WcsDescriptor* d, double ra_deg, double dec_deg,
    double* x, double* y);
std::string p3_wcs_fits_keywords(const P3WcsDescriptor* d);
const P3WcsApplicability* p3_wcs_applicability(const char* projection);
P3WcsRoundtripGate p3_wcs_roundtrip_gate(const P3WcsDescriptor* d);
P3WcsStatus p3_wcs_check_applicability(
    const P3WcsDescriptor* d, std::string* why);
~~~

p3_wcs.h 还公开 p3_wcs_fov_deg、p3_wcs_scale_arcsec_per_px、p3_wcs_roundtrip_envelope_px、p3_wcs_roundtrip_max_error_px、p3_wcs_roundtrip_dense_max_error_px 和 header-inline p3_crop_window_from_fits/from_sky/apply_wcs。描述符字段为 double crval_ra_deg/crval_dec_deg/crpix_x/crpix_y、double cd[2][2]、int width_px/height_px、const char* projection；projection 为指向静态注册代码字面量的借用指针，不归调用者释放。状态 P3_WCS_OK=0、PARAM=1、UNSUPPORTED=2、HEMISPHERE=3；crop 状态 OK/PARAM/HEMISPHERE 分离。p3_wcs_make 在投影已可声明时零初始化 out，通过 CD/FOV/往返/corner gates 后返回；不要把错误状态当成输出 WCS 有效。当前 adapter PA=0, projection=TAN-only，frame=ICRS, coverage_output=mask；crop 内部0-based半开窗口，来自 FITS 的像素输入为1-based闭区间，输出 CRPIX 减整数原点，CD/CRVAL 原样；sky crop 每边257采样点是实现规格而非连续严格边界证书。

**P1 Drizzle 真实内核与 P3 API 属不同库**：[Drizzle 球面长文](../../science/algorithms/export/DRIZZLE_GEOMETRY.md)指向 lib/algorithms/drizzle/healpix_drizzle/{hp_drizzle_api.h,drizzle_engine.cpp,spherical_overlap.cpp}；其中生产 nside>=256 leaf 求交使用 boundary4/cached TargetGeomCache，故 #15 保持 OPEN。P3 p3_resample.h 是 HEALPix leaf 中心/邻域的 nearest、bilinear；它们不是输出像元足迹面积求交，未来的 area-overlap 是 TARGET。库 #88/#89 的重复 leaf 合并和 masked-corner weight 初始化修复已在当前 main；当前 CLI 的 p3_op_resample/ writer 仍只有 Signal，不发布 variance/ivar。#81/#84 的 P2 large-scale/zero-candidate 源码已修复，不再以旧 D6 描述作为当前缺陷。

**资源与真值分离**：P1 球面交叠每 drop 候选查询与角点/真曲边精细求积成本随 nside/边界复杂度变化，TargetGeomCache bounded/thread-local；P3 阶段行带/固定子块写盘随 W×H 和 tile cache/queue-depth，Runtime 租约预算控制 worker，stream 按 block bounded 驻留，crop 不新算整张值。两者计算成本不能合并估为一次 bilinear O(1)。外部 Astropy/WCSLIB、独立 HEALPix chart/membership、P2 Signal→FITS 的 BUNIT/response/PSF/方差 oracle 均为 NOT_RUN。真正的 RESPONSE/FILTER/ACSDRESP/RSPHASH 仍按本页 D5 的 #79 TARGET 定义而非宣称当前支持。


## D7-B｜P3 Resample / FITS Writer / Verify 实际 API、状态与所有权补遗

完整函数数学合同与原始长篇有效实验/伪代码见 [Resample 正式实施](../../science/algorithms/export/PHASE3_RSMP_IMPL.md) 和 [FITS 正式实施](../../science/algorithms/export/PHASE3_FITS_IMPL.md)。本页是真实 module adapter 与公共接口、资源及错误消费位置的连接，不把p3_session兼容入口与真正五阶段 CLI 混为一谈。

**Resample API（命名空间 acsd::phase3）**：`p3_sampler_open(const char* product_dir,P3Sampler* out,std::string* err)`、`p3_sampler_open_ex(const char*,P3Sampler*,int* out_order,std::string* out_bunit,std::string* err)`、`p3_sampler_close(P3Sampler*)`；`p3_order_select(int,double,int*)`、`p3_resample_check_mode(const char*)`；`p3_sample_nearest` / `p3_sample_nearest_ex`（P3Sampler*、ICRS RA/Dec、float* value、int* coverage；_ex 另有 uint64_t* leaf_ipix）；`p3_sample_bilinear_ex(P3Sampler*,double,double,float*,int*,double weights[4],uint64_t leaf_ipix[4])`，`p3_sample_bilinear_nanmask_ex(...,P3SampleRejection* rejection)`；缓存`set_max_tiles/attach_cache/cache_stats/set_absent_cache`；Issue #87 故障身份`p3_sampler_set_worker_id/tile_failures/tile_failure_count/last_tile_failure/footprint_identity`；独立可调用误差接口`p3_uncertainty_open/close/propagate`，这些后者不在当前CLI调用链。 `P3Sampler`只拥有opaque `impl*`，调用方负责close；每worker独立sampler+只读共享LRU，不能在signal与uncertainty不同子产品间共享tile内容。C++ `std::string*` 是库内部签名，**不是跨DLL版本化C ABI**；跨模块边界须另按公共对象/host ABI设计，不得把该C++头误称纯POD C导出。P3ResampleStatus OK0/PARAM1/UNSUPPORTED2/IO3/INTEGRITY4；合法无覆盖仍 OK+NaN/C0，真实完整性故障返回 INTEGRITY/IO，具体 code/tile/CFITSIO 通过 `P3TileFailure` 暴露，不能只以 code0 断言输入无损。

**生产 `p3_op_resample`**：读取 `p3_props.json`/`p3_wcs.json`、真实HiPS signal，通过Runtime并发sub-block生成 `p3_resampled.bin` 的f32 signal、coverage双平面，取实际order；`p3_resampled.json`声明output_mode、BUNIT、uncertainty_available=false。订单 `K` 来源实际properties而不是额外影子metadata；配置 `max_tiles`可降不可升，`sub_block_px`和`queue_depth`是Runtime内存/背压参数。512² f32每tile约1MiB，成功tile LRU容量可控；`SharedTileCache::absent` 合法负缓存不共享LRU cap，Issue #87 的完整性错误表是独立 `std::map`（按tile升序、上限4096），hot-slot和并发首次miss仍会影响峰值与重复open次数。AIO reader真实 `NorderK/Dir{floor(ipix/10000)*10000}/Npix{ipix}.fits` 标准布局优先，缺标准路径才用旧式只读fallback；读侧readonly CFITSIO不持writer的整个进程CfitsioLockGuard。worker 采样返回 `P3_RS_INTEGRITY`/`P3_RS_IO` 时节点继续扫描以汇总输出网格触及的全部坏 tile（错误缓存去重），wait_all 后按 tile_ipix 升序判红，临时 bin 丢弃，并写 `p3_tile_failures.json`；writer 侧再防御性拒绝 `tile_error_count>0` 或 `worker_bands` 不完整的 artifact。

**Writer对象接口**：`P3FitsStream::open(path,wcs,width,height,bitpix,bunit,prov)`→`begin_hdu(0)`→多次`write_block(x0,y0,w,h,float* data)`→`end_hdu()`→`publish(P3OutputResult*)`，失败用`abort()`；`P3FitsVerifyStream::open(path,wcs,width,height,false)`→`check_block(0,x0,y0,w,h,expected)`→`close(result)`。stream拥有文件句柄、锁和临时路径，调用方拥有输入row-major子块与P3Provenance借用的字符串；写入期间须保持有效。 `P3OutputStatus` = OK0/PARAM1/IO2/CANCELLED3，`P3OutputResult` 包含sha256[65]、reopen_ok、coverage_ok、covered_px、total_px。CURRENT只能plane0 PRIMARY；整幅 `p3_output_write_atomic[_ex]` 与`p3_output_verify[_ex]`公共库函数存在但并非production默认执行，variance/ivar非空被明确拒绝。资源/发布顺序：裁剪后每个子块读内部磁盘work file→fits_write_subset→写PRIMARY DATASUM/CHECKSUM→flush/close→fsync(tmp)→独立verify_tmp_chksums→SHA256(tmp)→atomic_replace→发布后流式独立读回；writer记录完整hash和规范hash、run ID、输入manifest hash，二者不是同一算法。

**状态消费需修复的生产 gap（SOURCE_CONFIRMED，未新跑故障注入）**：`P3FitsVerifyStream::close`能返回`P3_OUT_OK`但`result.reopen_ok=0`；`p3_op_writer`和`p3_op_verify`消费此结构却未统一显式因`reopen_ok=0`失败，且`p3_op_verify`记录`canonical_match=false`仍return success。以同run身份、真实源窗口、WCS、FITS BUNIT/FILTER/response、完整结构/校验和/identity hash以及两个布尔均为真作为正式verify成功条件，须对篡改和工作文件版本分叉做正反例；以上为TARGET，本文不动生产C++。当前流式`covered_px`仅计非NaN，无法区分C=1但值NaN与C=0缺tile，且Inf会计入；不要用其冒充coverage平面或实际天空面积。外部 FITS/sky真值与 #79、#9 仍另验。
