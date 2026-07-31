# 实现要求

## 1. 总体要求

- 直接修改旧仓库。
- 正式实现使用 C++，接入现有构建系统、CLI、内存管道和Browser。
- 删除无用原型和重复路径；保留迁移参考时必须隔离。
- 所有外部进程、网络、硬件等待或可能阻塞的 Python 辅助脚本必须设置明确超时。

## 2. 先统一的数据接口

建议建立或整理以下概念，不强制文件名：

- `Stage1FrameContract`：单色输入、校准和测光状态；
- `HealpixGridSpec`：NSIDE、NESTED、ICRS、Tile NSIDE；
- `DrizzleTileAccumulator`：float64 signal/support累加；
- `HissTile`：父ipix、占用模式、子块集合；
- `HissSubblockDescriptor`：类型、flags、offset、size、codec、transform、checksum；
- `HissWriter` / `HissReader`；
- `CodecRegistry` / `TransformRegistry`；
- `HissMetadata`：精简FITS风格字段；
- `Stage1Diagnostics`：结构化错误和非致命回退记录。

## 3. Stage1修改

### 校准

- 实现三种模式及正确Bias/Dark公式。
- 最优模式失败：输出诊断，自动回退曝光比例法。
- 不得静默改变实际模式。
- Flat只做允许格式、单色、尺寸/通道和样本类型检查。

### PlateSolve / PSF

- 星点检测只做一次并复用。
- 清除重复检测路径或让其不再进入正式流水线。

### Gaia测光

- 比例在Drizzle前真正乘入信号。
- HISS signal标记为统一相对测光累计通量。

### Drizzle

- 完整WCS/SIP映射；
- 球面真实重叠；
- float64内部；
- 标准pixfrac；
- 通量守恒检查；
- support范围检查。

## 4. HISS Writer

- 支持自适应Tile规则。
- 支持FULL/BITMAP/SPARSE结构，但切换阈值不得最终定案。
- 每Tile独立子块。
- Header前置、attachments后置。
- 使用临时子块池和最终 `.partial` 原子提交。
- 不实现Checkpoint、Footer和断点恢复。
- RAW必须可用，其他codec通过注册接口接入。
- 未冻结的默认codec不得硬编码为正式规范。

## 5. HISS Reader

- 按目录读取，不依赖子块物理顺序。
- 支持只读occupancy、signal、support或SNR。
- 未知可选块跳过；未知必需块报不兼容。
- 检查offset/size越界、解压长度和checksum。
- 用NSIDE/NESTED/ICRS定位，不依赖WCS。

## 6. Browser

Stage1 Browser至少应能：

- 打开HISS并查看元数据；
- 按天空区域读取Tile；
- 切换signal/support/SNR控制点；
- 显示无数据区域、覆盖边缘和Tile边界；
- 查询某位置的ipix、signal、support；
- 不要求加载整文件；
- 对未知可选子块保持兼容。

Browser只针对Stage1 HISS，不实现Stage2功能。

## 7. 未决参数的实现方式

- codec、transform、FULL/BITMAP/SPARSE阈值、checksum算法、块对齐等应放入可替换策略或实验配置。
- 正式功能测试可用RAW或显式实验参数运行。
- 不得把某次benchmark冠军直接改为正式默认值。
