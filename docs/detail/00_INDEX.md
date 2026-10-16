# 细节文档阅读索引

细节文档将科学定义和工程合同落实为模块调用、对象交接、所有权及错误处理。其权威低于最高设计与一级正本；全文档文件清单由 DOCUMENT_INDEX 统一维护 [1,2]，本页提供阅读路径。

## 按问题阅读

| 问题 | 细节入口 | 上层依据 |
|---|---|---|
| 科学量、单位与产品如何对应 | [统一模型](common/unified_model.md)、[对象引用](common/unified_objects.md) | 统一科学模型、数据语义、工程对象合同 |
| Normalize 的节点如何交接 | [管线](normalize/pipeline.md)、[接口规格](normalize/contracts.md) | Normalize 科学正本、数据合同与工程管线 |
| Gaia 引导检测如何实现 | [星点检测](normalize/modules/star_detection.md) | 检测、PSF 与天体测量科学分册 |
| 球面帧如何合成为马赛克 | [Mosaic 管线](mosaic/pipeline.md) | 噪声、背景、排异和集成分册 |
| HiPS 如何导出平面文件 | [Export 管线](export/pipeline.md) | 投影与重采样分册 |
| 运行时如何调度并回收对象 | [Runtime](infrastructure/runtime.md) | 架构、命名块、调度器和所有权合同 |
| 产品如何读写与发布 | [统一 I/O](infrastructure/aio.md)、[存储形态](infrastructure/storage_form.md) | 产品交换、I/O 与原子发布合同 |
| 如何查看事件及排查失败 | [可观测性](infrastructure/observability.md)、[日志错误](infrastructure/log_and_error_system.md)、[排障](infrastructure/troubleshooting.md) | 日志、事件和错误合同 |
| 模块有哪些入口及消费者 | [Normalize](normalize/modules.md)、[Mosaic](mosaic/modules.md)、[Export](export/modules.md)、[HiPS writer](infrastructure/hips_writer.md) | 工程模块地图、公开接口及端口注册表 |

## 阶段与模块说明的分工

阶段管线页描述节点顺序、输入输出和交接责任。接口规格描述字段、单位、寿命及调用约定。各阶段模块正文集中说明具体入口、依赖和实现限制，引用上层定义，不重新建立另一套科学对象或产品政策。

同一模块可能存在独立工具、兼容 facade 与调度入口；文档需要标明当前调用者，不能把工具 main、descriptor 名称和真实生产路径视为同一件事。接口设计、源码静态存在、实际运行及独立科学验证分别说明。实施差异统一在工程实施地图维护 [3]。

## 对象和生命周期

每个阶段只消费其输入产品，阶段内由 Runtime 管理有名对象及消费者寿命。元数据和大数组分别声明所有权，异步视图持有显式租约；必要缓存有输入身份、版本与失效规则。开发快照是旁路观察，不能成为下游取数依赖。

Normalize 的正式球面参考对象遵循数据合同 [4]。Mosaic 内部精度、背景和支持对象不因此成为最终科学图像层；Export 从最终信号生成平面 FITS，不能由缺失的噪声模型制造绝对误差。

## 参考文献

[1] [ACSD 最高设计](../ACSD_DESIGN.md)。

[2] [全文档索引](../DOCUMENT_INDEX.yaml)。

[3] [科学设计与生产实现差异](../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

[4] [Normalize 数据合同](../engineering/contracts/NORMALIZE_DATA.md)。
