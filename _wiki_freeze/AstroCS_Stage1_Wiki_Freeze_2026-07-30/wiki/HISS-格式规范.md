> **SUPERSEDED — DO NOT IMPLEMENT**
>
> 本页面已被新标准页面取代，仅保留作历史迁移参考，不得作为实现依据。
> 请参阅 Home.md 中的"Stage1 标准页面（权威来源）"获取最新冻结规范。

# HISS 格式规范

## 状态

- 科学数据契约：**已冻结**
- support 是否存储及精度：**待确认**（见 Stage1-待确认事项）
- 物理二进制布局、Tile 尺寸和最终 Codec 参数：**当前首要冻结任务**

HISS 是单帧 Stage1 的正式球面数据库，不是调试缓存，也不是 PipelineFrame 快照。

## 1. 必须保存的数据

### 1.1 球面索引

- HEALPix `ipix`；
- 使用 `uint64`；
- 无覆盖区域不写入；
- 不用 signal=0 表示无数据。

### 1.2 主信号

Drizzle 后的主信号固定为：

- IEEE 754 binary32；
- 32-bit float；
- 小端存储；
- 无损压缩；
- 解压后逐位恢复原始 float32；
- 不允许 uint16/uint8 量化；
- 不允许半精度；
- 不允许截断尾数；
- 不允许误差界限有损压缩。

### 1.3 Support

Support 是 Drizzle 的几何覆盖量。**是否存储及精度待确认**（见 Stage1-待确认事项）。

待确认要点：

- 是否需要存储 support（ipix 列表本身已表示"有/无数据"）；
- 若存储，用 float32（连续覆盖比例）还是 uint8（0/1 布尔掩膜）；
- Stage2 停止后，support 作为叠加权重的用途暂不需要。

Drizzle 内部已用 float64 累积 `sumWeight`，问题只是是否持久化到磁盘。

Support 的最终归一化定义必须与 Drizzle 数学定义一起冻结。

### 1.4 SNR 控制点

SNR 控制点数量少，直接使用高可读性的固定结构：

- 精确球面位置，优先绑定 `uint64 ipix`；
- `float32 snr` 或等价质量量；
- 必要的 `float32` 辅助值；
- `uint32 flags`。

不为少量控制点设计复杂有损离散化。

### 1.5 Metadata

至少保存：

- 输入 Light 稳定标识和哈希；
- 校准模式；
- 使用的 Master 标识和哈希；
- Dark Optimization 的 \(k\) 及拟合摘要；
- WCS/SIP；
- PlateSolve 质量；
- 测光定标摘要；
- SNR 模型摘要；
- NSIDE；
- ordering；
- pixfrac；
- Drizzle 定义；
- 软件构建标识；
- 配置哈希；
- 各阶段状态和错误摘要。

## 2. 空间分块

HISS 必须按 HEALPix 空间 Tile 分块，而不是简单每 N 个数组元素切块。

目标：

- Stage2 可按空间顺序读取；
- 浏览器可按当前视野随机读取；
- 每块独立解压；
- 每块独立校验；
- 局部损坏不影响整个文件定位；
- 不需要为了查看局部区域读取完整文件。

## 3. 压缩要求

主数据压缩必须同时满足：

- 完全无损；
- 对 float32 signal/support 有合理压缩率；
- 压缩开销低；
- 解压速度高；
- 单 Tile 解压延迟低；
- 适合后续反复读取。

当前首选候选：

1. byte-shuffle + LZ4；
2. byte-shuffle + Zstandard 低等级；
3. LZ4；
4. 不压缩基线。

正式 Codec 必须使用真实 HISS signal/support 数据做格式决策测试后冻结。测试属于格式冻结，不属于后续性能优化。

测试至少记录：

- 压缩比；
- 压缩吞吐；
- 解压吞吐；
- 单 Tile 延迟；
- 峰值内存；
- 顺序读取；
- 随机读取。

## 4. 完整性

必须包括：

- 固定 Magic；
- 明确字节序；
- schema fingerprint 或等价布局识别；
- Header 长度；
- Tile 目录位置；
- 块长度；
- 块类型；
- 每块校验；
- 原子写入或临时文件完成后替换；
- 截断文件和损坏块的明确错误。

文件内部布局识别不等于维护 HISS v1/v2 产品路线。

## 5. HISS 冻结顺序

1. 冻结 Drizzle signal/support 数学定义；
2. 冻结空间 Tile 层级和索引；
3. 用真实数据比较 Codec；
4. 冻结物理布局；
5. 实现 C++ Writer；
6. 实现 C++ Reader；
7. 实现浏览器局部读取；
8. 做往返、损坏和随机读取验证；
9. 用户审查后才视为正式冻结。
