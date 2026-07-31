> **SUPERSEDED — DO NOT IMPLEMENT**
>
> 本页面已被新标准页面取代，仅保留作历史迁移参考，不得作为实现依据。
> 请参阅 Home.md 中的"Stage1 标准页面（权威来源）"获取最新冻结规范。

# Stage1 待确认事项

本页只记录尚未由用户最终确认的细节。Agent 不得将其自行转为已冻结。

## HISS

1. signal/support 的精确 Drizzle 归一化公式；
2. **support 是否需要存储、以及存储精度**（见下方专题讨论）；
3. HEALPix Tile 层级和目标未压缩块大小；
4. Codec：
   - byte-shuffle + LZ4；
   - byte-shuffle + Zstandard 低等级；
5. Tile checksum 算法；
6. Metadata 物理编码；
7. schema fingerprint 的生成方式。

### support 存储问题（待 ChatGPT 讨论）

#### 背景

Drizzle 内部已计算 `sumWeight`（覆盖面积比例），问题是要不要写入磁盘、用什么精度。

#### 用户观点

- support 不是科学数据，是元数据；
- 按 Wiki 规定"无覆盖区域不写入"，有 ipix 就说明有数据；
- 无论 support 值多小，只要写入了就说明有数据；
- ipix 列表本身已经表达了"有/无数据"；
- 是否可以用 0/1 布尔掩膜，甚至不存 support？

#### Agent 观点

- support 作为连续值（float32）能区分完整覆盖、部分覆盖、边缘擦过；
- 可用于评估边缘像素质量（signal 在低覆盖区可能不准）；
- Drizzle 通量守恒模式下 signal 已乘覆盖权重，可能不需要额外 support；
- Stage2 停止后，support 作为叠加权重的用途暂不需要；
- 建议方案 A（不存 support）或方案 C（float32），取决于是否需要边缘质量评估。

#### 三种方案

| 方案 | 做法 | 优点 | 缺点 |
|------|------|------|------|
| A. 不存 support | HISS 只存 ipix + signal + SNR | 最简单，文件最小 | 无法知道覆盖程度 |
| B. 布尔掩膜 | uint8(0/1) | 简单 | 与 ipix 列表功能重复 |
| C. 连续 float32 | 覆盖面积比例 | 能评估边缘质量 | 复杂，当前可能不需要 |

#### 待确认

此问题需要与 ChatGPT 讨论后决定。

## ~~暗场优化~~（已冻结）

k = t_light / t_dark，从 FITS 头 EXPTIME 读取。详见 Stage1-校准规范 §6。

## Drizzle

1. 输入像素值的最终物理语义；
2. footprint 与 output pixel 的面积归一化；
3. pixfrac 的精确定义；
4. **support 是否存储及精度**（见上方专题讨论）；
5. 守恒量和正式误差门限。

## 说明

其余内容，尤其以下事项，已经冻结：

- Stage1 only；
- 单色输入；
- GUI负责分组和警报；
- 只使用已有 Master；
- 无 Overscan/裁切/CFA/Master 制作；
- 标准公式 \((L-D)/F\)；
- 暗场优化公式 \([L-B-k(D-B)]/F\)；
- 暗场优化系数 k = t_light / t_dark（**已冻结**）；
- signal float32；
- support 存储及精度 **待确认**；
- SNR控制点 float32；
- 自动 NSIDE 1–2×过采样；
- 显式 NSIDE 默认接受；
- Drizzle 不允许低精度近似；
- 功能完成后先做性能分析；
- 710 必须由用户明确批准。
