# Export 单信号重采样

本节点将已有 HiPS 信号采样到目标平面，不进行新的光度、背景或多帧权重求解。核的物理含义、缺数政策与二次重采样影响取自[重采样正本](../../science/resample/RESAMPLE.md)和 [Export 管线](../export/pipeline.md)。

## 生产入口与工作载体

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_resample` 对应 `acsd.phase3.resample2`，消费 props、wcs_plan 和实际 HiPS。它读取 `p3_props.json`、`p3_wcs.json`，选择实际 order，经 Runtime executor 按子块采样，用 AIO 位置写产生 `p3_resampled.bin`，并写 `p3_resampled.json`。工作二进制按未裁剪平面连续存 f32 signal、coverage；后者是内部有效域对象，不是最终 FITS 科学层。

`acsd.phase3.resample` 另为聚合 session 适配描述符，不能与本五节点生产 operation 混称。reader、WCS 与写出有各自责任。

## 公开 API 与字段

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

## NaN、支持与资源

nearest 的 tile 缺失返回 NaN、coverage=0，tile 存在但像元非有限可返回 NaN、coverage=1。bilinear 区分缺 tile 与已读邻域的无效样本；后者掩除并按剩余有效权重重归一，零合格样本不制造有限信号。coverage 是可用 tile/支持语义，不自动代表数值有限或精度。

sampler 单实例由一个执行线程使用；worker 可各自 open，再 attach 到同一受控共享 cache。不能继续写成每 worker 必有完全独立缓存或总容量必乘 worker 数。缓存只改变 I/O，不改变 order/核；输入子产品不同不得共享同键 cache。sub_block_px、queue_depth、max_tiles 由实际配置/资源解析控制，不能由接口存在推断任意尺寸的资源实测已满足。

## 模式、错误与科学边界

当前 adapter 要求 output_mode，接受 surface_brightness/visualization，明确拒绝未实现 point_source_flux；这些是实现事实，不把它们改写为最高科学目标。当前正式路径不探测/传播 uncertainty，单信号输入不能反演协方差。

头中 `p3_uncertainty_open/propagate` 等保留库能力，不构成正式 export 已消费误差层。P3ResampleStatus 区分 PARAM、UNSUPPORTED、IO；输入损坏和合法缺数分别处理，取消经 Runtime/AIO 上行。

## 验证入口

[Export 管线测试入口](../../../eng/tests/pipeline/test_stage_export.py)与本头采样函数用于核对生产、正/负缓存、实际读取量及科学值不变；[Export 管线](../export/pipeline.md)定位 production。独立检验常值、梯度、点源孔径/FWHM、RA wrap/极区/face/tile 边界、NaN 重归一和真实空洞。不得以自读自写或平滑外观宣称光通/噪声完整正确；本页未运行测试。
