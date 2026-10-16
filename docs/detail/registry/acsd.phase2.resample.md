# Mosaic 聚合会话适配描述符

`acsd.phase2.resample` 是 [module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 中 `phase2_descriptor` 保留的聚合 session 适配名，不是正式 Mosaic 七节点中额外的重采样算法。实际阶段职责与交接见 [Mosaic 管线](../mosaic/pipeline.md)。

## 入口与数据边界

注册工厂通过 `SessionModule/P2Api` 委托 [p2_session.h](../../../lib/phase2_session/p2_session.h) 五函数；实际局部会话为 coverage→sample→UPM build→可选 persist，不能据描述符名宣称它执行球面重新采样、排异、集成和 HiPS 发布。CLI 完整路径使用 `P2NodeModule` 的七个 operation。

| 描述符声明 | 实际解释 |
|---|---|
| calibrated / DATA-P1-CAL / ADU / PIXEL | 聚合兼容声明；不是 Mosaic 正式输入的产品身份 |
| resampled / DATA-P2-RES / ADU / PIXEL | 聚合输出名；不能代替 NESTED 马赛克 signal 合同 |
| cpu_heavy、parallel_ok | 调度元数据；不证明本适配器有独立算法或线程池 |

用户输入实际为已发布逐帧 HiPS；阶段间只通过磁盘产品交换。这个聚合描述符的单位/坐标声明不得反向覆盖[最高设计](../../ACSD_DESIGN.md)的球面面亮度与最终单 signal 产品要求。

## 生命周期、错误与验证

handle、host allocator、budget、cancel、JSON 和 manifest 的实际所有权依[局部 session 卡](acsd.phase2.session.md)。本描述符不定义新的 source 数组、WCS、插值核、缓存格式或物理单位，不新增重采样配置。返回码经 session→Result 映射，不能把占位输出存在当完整马赛克成功。

验证应分别检查聚合工厂确实委托会话、CLI 七节点没有误选该兼容描述符，以及产品消费端使用真实 HiPS 合同。没有独立 kernel 就没有本页专属数值通过证据；本页未运行测试。生产差异统一见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
