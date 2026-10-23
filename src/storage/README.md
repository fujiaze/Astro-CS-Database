# storage

现有LocalStorage负责数组/FITS/产品I/O；本轮不改实现。ArrayBuilder规范封存/撤销、预留容量转交、ArrayBacking设备承接/host物化及版本化实施是E02/E03/E07目标，见API/OBJECTS/EXECUTION；后续先同步唯一声明，不依赖科学模块私自打开文件补缺口。

按单一当前格式重构；不保旧v1/早期v2 reader、旧hash或产品转换器。产品不匹配具名拒绝并重建，固定服务边界见[SYSTEM](../../docs/engineering/rebuild/SYSTEM.md)。
