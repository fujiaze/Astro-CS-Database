# Normalize 实现导航

阶段一科学因果链来自 Normalize 科学正本[1]，生产入口和模块责任由工程架构说明[2]。本页将读者引向实际接口，不复写科学定义或完整实施差异。

## 1. 从用户命令到实现

`normalize` 经 `lib/infrastructure/cli/commands.cpp::run_phase1_block` 和 IR 进入 `lib/infrastructure/scheduler/src/module_adapters.cpp::P1NodeModule`。其八节点是 calibration、cosmetic、WCS、guided sources/PSF、photometry、noise、Drizzle、writer；IPV 自行盲检并求天测，科学源表在 WCS 后生成。

`lib/phase1_session/p1_session.cpp` 的局部校准会话保留兼容身份，不具有完整 WCS→HiPS 产品链。

## 2. 数据和接口定位

| 需要查阅的内容 | 唯一入口 |
|---|---|
| 字段、单位、端口、身份、能力及产品 | Normalize 数据合同[3] |
| C++ 签名、头文件落点与迁移 | 接口实现规格[4] |
| 参考/状态/控制点类型 | `lib/include/acsd/normalize_data.h` |
| 同次 Drizzle 几何到局部窗口的适配 | `lib/infrastructure/scheduler/src/normalize_precision.cpp` |
| 球面参考 GLS、秩、背景及摘要 | `lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp` |
| 模型和 frame_science.json 读写/发布 | `lib/algorithms/integration/normalize_science/src/normalize_science.cpp` |
| 已有接线与未覆盖合同 | 科学实施差异地图[5] |

noise 先建立平面噪声；Drizzle 完成几何后调用同一 noise 库求球面参考精度。当前已有调用和 sidecar staging/publish，跨节点仍有真实 p1_*.json/FITS 读写依赖；完整 Runtime typed 对象迁移不能据端口声明视为已完成。

## 3. 修改与证据边界

先读科学和工程正本，再读对应算法实现与消费端。源表、PSF、photometry、noise、Drizzle 使用同一帧、WCS 和实际标度；参考精度由明确参考、有效域与条件模型定义。canonical sidecar、Catalogue 浏览和兼容 sparse_snr_layer 有不同身份，不能仅凭名字互换。

局部验证、真实帧运行和独立科学支持分别报告；证据归属由收敛说明维护[6]。本页不规定实验批次、不重新选择模型，也不将合法信号的 precision unavailable 写成输入失败。

## 参考文献

[1] [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)。

[2] [Normalize 生产链与模块责任](../../engineering/architecture/NORMALIZE_PIPELINE.md)。

[3] [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

[4] [Normalize 接口实现规格](contracts.md)。

[5] [科学设计与生产源码差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

[6] [Normalize 文档权威与证据归属](../../engineering/governance/NORMALIZE_CONVERGENCE.md)。
