# 阶段间产品交换

上游为最高设计的三阶段产品和 [DATA_SEMANTICS.md](../../science/unified/DATA_SEMANTICS.md)。跨阶段只经磁盘产品与 manifest 交换，不共享运行时对象。Normalize 的字段和 sidecar 由 [NORMALIZE_DATA.md](../contracts/NORMALIZE_DATA.md) 定义。

## 产品角色

| 角色 | 必需科学图像 | 其它必要对象 | 可选内容 |
|---|---|---|---|
| normalize frame | 逐帧 signal HiPS | frame_science.json、模型引用/能力状态、坐标/单位、完成 manifest | 有来源的 support/variance/ivar、Catalogue 浏览 |
| mosaic | 一个 signal HiPS 科学通道 | HiPS 元数据、manifest、混合响应/运行摘要 | 开发旁路；不得添最终辅助科学图像层 |
| export | 普通二维 WCS signal FITS | WCS/BUNIT/来源和运行摘要 | 不从单信号图伪造置信度 |

HiPS 多个 Norder 是角分辨率层级，不是多个科学通道。阶段一允许必要辅助数据，不因此要求阶段二/三也发布 support/variance。

## 读取责任

输入由对应运行清单枚举，读取实际产品几何、单位、状态和模型 hash，不按文件名猜天区。Normalize 可无原始 WCS，需 IPV 实际解算；已发布球面产品必须有真实可用坐标。缺精度模型与信号损坏分开报告；不同设备/滤镜不构成产品拒绝条件。

新 frame-science reader 验证 schema、frame/run、reference/响应、signal unit、model refs 和实际文件 hash；无新 sidecar 的合法历史 HiPS 保持信号/variance 基线读兼容，canonical precision unavailable。不得自动把旧 frame_snr 或 Catalogue proxy 转成新球面参考精度。阶段二默认权重不在本轮更换。

## 完整性与发布

发布遵循 [ATOMIC_PUBLISH.md](../contracts/ATOMIC_PUBLISH.md)：run-private staging、有界 AIO、所有文件完成后发布 manifest。完成表示磁盘产品完整；科学模型可以明确 unavailable，不能把完成状态当作绝对科学验证。未解帧只在运行清单登记失败原因，其他合法帧可以保留。

模型文件是产品自身组成部分，路径不得逃出根目录；可求值的下游模型不能只指向计算节点原始 FITS。原始帧/母版哈希可以作为血缘，不要求把大原始数据复制进产品或 Git。

## 当前机器合同迁移

既有 phase_product_exchange.schema.json、exchange matrix、artifact registry/validator 的 v1 角色及“signal/support/variance 最小三面”记录当前接口，存在与目标单信号产品不一致的部分。执行者调整实际 reader/writer 时同步版本化机器合同与兼容 adapter，不能修改老产品字节或用新文档冒称已完成接线。旧检查的 PASS 只说明 v1 形状；更新后应接受完整但 precision unavailable 的合法产品，并识别真损坏/悬空模型。形状 schema 与模型物理检验分别进行。
