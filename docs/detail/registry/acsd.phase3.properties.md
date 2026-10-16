# Export 产品属性读取

properties 节点确认输入 HiPS 的信号、几何、层级与单位，形成下游计划所用的元数据。输入来自已发布单 signal 产品；不要求 Mosaic 提供 variance/ivar/support 图像层。阶段边界见 [Export 管线](../export/pipeline.md)和[HiPS 到 FITS 正本](../../science/projection/HIPS_TO_FITS.md)。

## 生产入口与交接

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_properties` 对应 `acsd.phase3.properties`，通过 `p3n_guard_input_units`、`p3_sampler_open_ex` 及 properties parser 读取真实产品，写 `p3_props.json`。下游 WCS、resample、writer 消费其输入路径、实际最大 order、tile_width、BUNIT 与能力状态。

公开读取入口为 [p3_resample.h](../../../lib/algorithms/resample/p3_resample.h) 的 `p3_sampler_open_ex(product_dir,out,out_order,out_bunit,err)`；底层文本解释由 [hips_properties.h](../../../lib/algorithms/coverage/hips_properties.h) / [hips_properties.cpp](../../../lib/algorithms/coverage/hips_properties.cpp)承担。不是另一个科学像素算法。

## 字段、寿命和错误

| 字段 | 语义 |
|---|---|
| hips_dir、hips_order | 实际读取路径和层级上限，不能只从请求复制 |
| tile_width、ordering、frame | 512、NESTED 和可解释天球坐标；非法数据按 reader 错误上行 |
| bunit、bunit_input | adapter 接受的 canonical 串及实际输入声明；解析结果与实时守卫分叉要报错 |
| pixel_semantics | 球面面亮度；不是每叶积分通量 |
| uncertainty_available/source | 当前单信号路线上 false/none；不探测并不等于证明输入从未有模型 |

sampler 由 open 获取，在元数据读取后 close；输出字符串/JSON 由调用方持有。解析本身无内部线程预算，节点资源和取消由 Runtime 管理，文件通过 AIO。坏 properties、open 失败、非法单位/ordering/尺寸应保留具体错误；不能用猜测默认把坏产品改成兼容。

科学合同要求单位继承真实光度尺度，当前 reader/adapter 的固定 canonical 单位支持不能替代该要求。不得因某个库头含默认 ADU/sr 就给相对标度信号制造物理单位；相邻实现边界见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 验证入口

从 `p3_op_properties`、`p3_sampler_open_ex` 与 parser 进入核对缺必需键、显式非法值、NESTED/阶数、BUNIT 分叉和单信号产品。合法零/负信号无需在属性阶段拒绝；没有输入误差层也不妨碍正式导出。本页未运行测试，属性可解析不能充当 WCS 或图像保真证据。
