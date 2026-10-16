# 统一数据对象的模块消费

公共对象的物理定义来自统一科学模型[1]；机器形状与兼容关系由工程统一对象页登记[2]。本页只说明 detail 层的消费方式，Normalize 的完整复合对象以数据合同为准[3]。

## 端口与身份

模块签名声明输入和输出的对象类型、单位、坐标、形状、生产者、消费者及有效状态。signal 的方差只能描述同一估计量；source_snr、reference_snr、frame summary、背景控制方差分别使用具名类型。对象内的普通字段结合类型解释，不以无上下文的 weight/mask 代替实际统计量。

运行键绑定 run、输入 occurrence、对象类型、revision 和输入版本。生产者发布后只读；消费者借用视图/模型 lease，完成、失败和取消均由 Runtime 回收。科学 spill/cache 与产品定位通过统一 AIO，开发 snapshot 不供消费者取数。

## 序列化与可用性

阶段一 canonical precision 从 frame_science.json 及其模型读取。Catalogue、规整 sparse_snr_layer 和兼容 frame_snr 保留各自身份；没有完整 reference、单位、支持和模型时不能自动转换。shape 可解析、合法 unavailable、实际数据损坏和可作为 canonical 真源消费分别判定。

阶段二可在内部重建精度、背景和支持，最终只写 signal HiPS；阶段三消费 signal 并写普通 WCS FITS。标准 HiPS 元数据、层级和运行摘要不构成额外科学图像层。实际实现边界由科学实施差异地图维护[4]，本页不增加一份完成状态台账。

## 参考文献

[1] [统一科学模型](../../science/unified/UNIFIED_SCIENCE_MODEL.md)。

[2] [统一数据对象与序列化登记](../../engineering/UNIFIED_OBJECTS.md)。

[3] [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

[4] [科学设计与生产源码差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
