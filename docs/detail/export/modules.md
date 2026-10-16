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

nearest 的 tile 缺失返回 NaN、coverage=0，tile 存在但像元非有限可返回 NaN、coverage=1。bilinear 区分缺 tile 与已读邻域的无效样本；后者掩除并按剩余有效权重重归一，零合格样本不制造有限信号。coverage 是可用 tile/支持语义，不自动代表数值有限或精度。

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
