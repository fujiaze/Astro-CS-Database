# storage

现有LocalStorage负责数组/FITS/产品I/O；本轮不改实现。ArrayBuilder规范封存/撤销、预留容量转交、ArrayBacking设备承接/host物化及版本化迁移是E02/E03/E07目标，见API/OBJECTS/EXECUTION；后续先同步唯一声明，不依赖科学模块私自打开文件补缺口。
