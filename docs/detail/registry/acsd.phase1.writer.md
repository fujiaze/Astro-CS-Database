# Normalize 帧产品发布

本节点检查 Drizzle 已写的逐帧 HiPS，发布科学模型与唯一 `frame_science.json`，登记帧产品。它属于 [Normalize 八节点链](../../engineering/architecture/NORMALIZE_PIPELINE.md)，并非把 stacked 另导出一张平面 FITS。

## 实际入口与数据交接

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_writer` 对应 `acsd.phase1.writer`，在 Drizzle 后执行，消费逐帧 `p1_stack.json`、已完成 HiPS 和 precision staging，产生帧摘要/产品引用。descriptor 目前仍以 `stacked → fits` 命名，不能据此改写真实球面产品身份。

目标 writer 输入为同帧同版本的 WCS、photprov、noise、stacked、Drizzle 模型和参考 controls；Runtime 类型、occurrence、状态和发布范围见 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)及[接口规格](../normalize/contracts.md)。

## 统一 serializer/reader 与发布 API

[normalize_science.h](../../../lib/algorithms/integration/normalize_science/include/acsd/normalize_science.h)和[实现](../../../lib/algorithms/integration/normalize_science/src/normalize_science.cpp)提供：

| 入口 | 输入、输出与边界 |
|---|---|
| `write_local_model_file(root,payload,out)` | 序列化局部模型及 hash；bool 和 ModelWriteResult |
| `write_frame_science_document(root,doc,out)` | 唯一 sidecar serializer；未知值 null，稀疏 FP64 round-trip |
| `read_frame_science_document(path)` | FrameScienceReadResult：ok/error、present、supported、doc、findings |
| `publish_staged_frame_science(staging,root,out)` | 核对/复制模型 hash，原子写 sidecar 并回读；由 writer 决定调用时机 |
| `load_local_model_payload` / `reconstruct_factor_covariance` | 读取模型，核对因子/CSR结构并在预算内重建 C；不同于 sidecar 解码 |
| `verify_frame_science_factor_models` | 对模型引用组织因子核对；当前通用 reader 尚未调用它，不能据函数存在宣称发布回读已执行全部模型校验 |

生产 `p1_op_writer` 有实际发布调用。sidecar 指向 signal、WCS、reference、模型、controls、summary 和状态；model refs 使用产品内相对路径/hash。sky 为 ICRS degree、CRPIX 为 FITS 一基、叶号按 NESTED 实际叶阶；F_ref/V_F/SNR 与真实单位、模板/协方差配对。精度缺项不允许用零值填满。

## 所有权、错误与实现边界

writer 是 I/O 节点，单帧产品只有一个最终发布者。输入图像/模型由 Runtime/AIO 或 staging 持有；writer 不重新估计 PSF、噪声或光度，不创建另一份 canonical 真源。JSON/FITS 工作文件仍是实际依赖，开发诊断不能供 writer 补科学数据。

合法 signal 且 precision unavailable 是完整但能力受限的产品；损坏、reader findings 与发布失败是不同状态。运行清单应枚举全部输入 occurrence 和失败/取消原因，不能仅枚举成功目录。当前逐文件 hash/原子写和回读不等于 signal+全部模型+sidecar+完成 manifest 已形成整帧事务；唯一现行差异正文见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

取消与 AIO 错误按 Runtime/统一 I/O 上行；写出后存在文件不足以说明可作为完成产品消费。模型能力状态与磁盘完整性也不能互相代替。

## 验证入口

[sidecar schema 检验](../../../eng/tests/conformance/normalize_frame_science_schema_test.py)与[生产阶段一测试](../../../eng/tests/pipeline/test_stage_normalize.py)分别检查形状和实际节点。独立验证还需模型 hash/shape/引用、读写语义、applied application ID、未知 schema、缺 sidecar 兼容，以及发布中途失败/取消后不出现伪完成产品。本页未运行测试，不以 serializer/reader 存在宣称这些合同已全部满足。
