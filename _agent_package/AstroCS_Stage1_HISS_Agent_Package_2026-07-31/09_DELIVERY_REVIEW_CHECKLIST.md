# 最终交付自审清单

## 范围

- [ ] 直接修改旧仓库，没有新建替代仓库。
- [ ] 没有实现或修改Stage2。
- [ ] 没有自动运行710全量回归。
- [ ] 没有用Python原型冒充正式C++实现或性能结论。

## Wiki

- [ ] 已冻结规范全部进入Wiki。
- [ ] 冲突旧页面已删除、归档或标为SUPERSEDED。
- [ ] Wiki说明未决项只做实验，不自动定案。

## Stage1

- [ ] 三种校准模式公式正确。
- [ ] 最优Dark失败后有明确诊断并回退曝光比例。
- [ ] Flat只做允许格式和结构检查。
- [ ] PlateSolve星点只检测一次并复用。
- [ ] Gaia比例已在Drizzle前应用。
- [ ] 自动NSIDE和NESTED正确。
- [ ] pixfrac和球面面积重叠正确。
- [ ] signal/support内部float64，最终float32/uint8。

## HISS

- [ ] 自适应Tile规则正确。
- [ ] FULL/BITMAP/SPARSE均可往返。
- [ ] 独立子块可单独读取。
- [ ] 每子块有codec/transform/size/checksum字段。
- [ ] RAW可用。
- [ ] 未知可选/必需块兼容规则正确。
- [ ] Header前置、attachments后置，无Footer/Checkpoint。
- [ ] 不保存完整WCS/SIP。
- [ ] 元数据保持精简FITS风格。

## 实验

- [ ] 所有压缩与阈值实验由C++完成。
- [ ] 保留CSV/JSON原始结果。
- [ ] 报告运行环境、重复次数和波动。
- [ ] 只给推荐，没有把未决项写成冻结默认值。

## 交付

- [ ] ZIP只包含必要差异文件和报告。
- [ ] 有git patch、删除列表、应用说明和manifest。
- [ ] 没有仓库副本、构建产物、大数据和大型日志。
