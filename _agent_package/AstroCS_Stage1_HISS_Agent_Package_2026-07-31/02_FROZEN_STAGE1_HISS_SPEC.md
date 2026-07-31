# 已冻结的 Stage1 / HISS 规范

> 本文件内容均已由用户确认。Agent 可以实现和文档化，不得自行改写科学语义。

## 1. Stage1 范围

固定流程：

```text
单色 Light
→ 选择一种校准模式
→ PlateSolve
→ 复用同一批星点
→ PSF
→ Gaia 光谱积分 / 测光校准
→ 稀疏 SNR 控制点
→ 显式或自动 NSIDE
→ 高精度 HEALPix Drizzle
→ HISS
```

CLI 只负责明确输入和参数下的确定性计算。以下属于 GUI 或外部工具，不进入 Stage1 CLI：

- 文件分组、Session、日期、设备、滤镜管理；
- Master 自动匹配和 Master 制作；
- CFA/Bayer/Debayer、多通道；
- overscan、裁剪、cosmetic workflow；
- 软警告和交互确认；
- Stage2 调度。

CLI 仍必须检查硬合法性：文件可读、允许格式、单色、尺寸/通道匹配、必需 Master 存在、NSIDE 合法、PlateSolve 成功、输出可写。

## 2. 校准模式

### 2.1 匹配 Master Dark

\[
I_{cal}=(L-D)/F
\]

Master Dark 已包含 Bias，不再单独减 Bias。

### 2.2 曝光时间比例缩放

\[
I_{cal}=[L-B-k_t(D-B)]/F,\quad k_t=t_L/t_D
\]

Bias 不缩放，只减一次。

### 2.3 最优 Dark 系数

估计模型：

\[
L-B=c+k(D-B)
\]

- 使用鲁棒估计、分区抽样和离群抑制；
- 不得为了估计 k 再执行一次完整星点检测；
- 最优估计失败时，必须输出明确结构化诊断，然后自动回退曝光时间比例缩放；
- HISS 记录请求模式、实际模式和最终 k；
- 若曝光时间缺失或回退也失败，整帧硬失败。

建议 FITS 风格字段：

- `DARKREQ`：用户请求模式；
- `DARKMODE`：实际采用模式；
- `DARKSCL`：最终 k；
- `HISTORY`：最优模式失败和回退说明。

## 3. Flat 输入契约

Master Flat 由外部工具提供。CLI 只检查：

- 文件可读；
- 属于允许输入格式；
- 单色；
- 尺寸/通道匹配；
- 数值样本类型受支持。

CLI 不重新归一化，不做 Flat 像素值域、均值、中位数、0/负值/NaN/Inf 等专项预扫描和修复。

## 4. PlateSolve 与星点复用

PlateSolve 内部只做一次全图星点检测。同一检测结果同时用于：

- 天文解算；
- PSF；
- 后续可复用的星点相关计算。

不得重复执行第二次全图检测。

## 5. 自动 NSIDE

用户显式给出合法 NSIDE 时，CLI 直接采用，不警告、不修改、不因性能降低。

未给出时：

- 依据最终 WCS/SIP 在有效视场内的局部 Jacobian；
- 找到最细局部输入像素尺度；
- 选择最小的 2 次幂 NSIDE，使 HEALPix 线性像素尺度不粗于该最细尺度；
- 结果约为 1～2 倍线性过采样。

目标支持约 0.1 角秒级到 1 度级像素。

## 6. HEALPix ordering

- HISS 内部统一 NESTED；
- 所有 ipix 都是 NESTED；
- CLI 不提供 RING 选项；
- 外部 RING 只能在边界适配器转换。

## 7. Gaia 测光与 signal

Gaia 光谱积分校准是 Stage1 正式步骤。测光比例在 Drizzle 前应用：

\[
I_{photo}=k_{photo}I_{cal}
\]

HISS `signal` 保存：

> 已应用 Gaia 光谱积分校准的统一相对测光累计通量。

- 不是原始 ADU；
- 暂不宣称 W/m² 等绝对物理单位；
- 建议 `BUNIT=ASTROCS_RELATIVE_FLUX`；
- `PHOTSCAL` 记录实际应用比例；
- `PHOTAPPL=TRUE`。

## 8. Drizzle 累计通量

源像素 j 对目标像素 p 的贡献：

\[
F_p=\sum_j L_j\frac{a_{jp}}{A_{j,drop}}
\]

- `L_j` 是已测光校准的源像素总信号；
- `a_jp` 是真实球面重叠面积；
- `A_j,drop` 是 pixfrac 缩小后 drop 面积；
- drop 未被有效域截断时，单个源像素全部目标贡献之和必须恢复 `L_j`。

内部 WCS/SIP、球面几何、重叠和累加使用 float64；最终 signal 写为 IEEE 754 float32，小端序。禁止有损量化。

## 9. pixfrac

\[
0<pixfrac\le 1
\]

采用标准 drop 语义：

- 以源像素中心为中心；
- 局部线性尺寸乘 pixfrac；
- 面积乘 pixfrac²；
- 源像素总信号不变；
- drop 内信号面密度乘 1/pixfrac²；
- 之后通过完整 WCS/SIP 映射到球面并做真实面积重叠。

## 10. support

\[
S_p=\frac{\sum_j a_{jp}}{A_p}
\]

support 是单帧目标 HEALPix 像素被有效 drop 覆盖的纯几何面积比例：

- 与 SNR、曝光、测光比例和后续权重无关；
- 合法范围 0～1；
- 内部 float64；
- 仅浮点误差级超限可钳制；明显超过 1 是几何/WCS或实现错误；
- HISS 中存 uint8：`round(255*S)`。

## 11. 自适应空间 Tile

每个 Tile 对应一个 NESTED 父像素。层级差：

\[
d=\min(9,\log_2(NSIDE/16))
\]

\[
NSIDE_{tile}=NSIDE/2^d
\]

同时保证：

- 满 Tile 最多 `4^9=262144` 个叶像素；
- Tile 父级 NSIDE 不低于 16；
- Tile 特征角尺度不超过约 3.7°。

## 12. Tile 占用编码

Writer 支持：

- `FULL`：全部叶像素有效，无占用块；
- `BITMAP`：1 bit/潜在叶像素；
- `SPARSE_LIST`：保存有效局部索引。

用户和 GUI 不配置模式。具体切换阈值尚未冻结，必须用 C++ 实验后汇报。

## 13. 独立子块

每个 Tile 至少包含独立可寻址子块：

- occupancy（FULL 时省略）；
- signal；
- support；
- 可选 SNR controls；
- 未来可选扩展。

不得依赖固定物理顺序解释。未知可选子块可跳过；未知必需子块必须拒绝。

## 14. HISS 容器

参考 XISF 的单体容器思想：

```text
固定签名块
→ 文件前部完整 Header
→ 独立 attachment 子块
```

- Header 是唯一权威目录；
- 不使用 Footer、Checkpoint、断点续写；
- 写入先流式生成临时子块池，再生成最终 Header，组装 `.partial`，flush 后原子重命名；
- Header 使用紧凑可扩展二进制结构，不照搬 XML；
- 当前只维护一个 AstroCS 1.0 目标 HISS 格式。

## 15. 子块目录必需字段

每个子块目录项必须独立记录：

- block type；
- required/optional flags；
- offset；
- compressed size；
- uncompressed size；
- `codec_id`；
- `transform_id`；
- checksum type；
- checksum。

同一 HISS 中允许不同 Tile、不同子块使用不同 codec/transform。必须支持 RAW。文件级默认 codec 只能用于显示或建议，不能替代子块级声明。

## 16. 元数据

采用“传统 FITS 常用头 + HISS/HEALPix 必需参数”的精简方案，不建立大型状态数据库。

### 必需空间/容器信息

- MAGIC / schema / header length / endian / feature flags；
- Tile 与子块目录；
- `PIXTYPE=HEALPIX`；
- `NSIDE`；
- `ORDERING=NESTED`；
- `RADESYS=ICRS`；
- `TILENSID`；
- `PIXFRAC`；
- signal/support 类型和语义；
- Gaia 相对测光系统与比例。

### 传统 FITS 字段

按输入实际存在继承常用字段，如 `OBJECT`、`DATE-OBS`、`EXPTIME`、`FILTER`、`TELESCOP`、`INSTRUME`、`GAIN`、binning 等。

### 校准字段

保留实际必要字段：`CALMODE`、`DARKREQ`、`DARKMODE`、`DARKSCL`、Master 标识和必要 HISTORY。

### 不保存完整 WCS/SIP

HISS 像素已由 NSIDE、NESTED ipix、Tile 父像素和 ICRS 直接定位。原始 WCS/SIP 只用于 Stage1 内部从源图像映射到球面，不写入 HISS。可选保留少量解算质量摘要，但不是定位必需字段。

## 17. SNR 控制点

保持精简。每个控制点仅保存：

- `local_ipix : uint32`；
- `snr : float32`。

估计方法、窗口尺度和数量只在 SNR 子块头保存一次。不得增加大量每点状态量，除非后续用户另行冻结。

## 18. 当前不讨论 Stage2

不得让现有 Stage2 反向限制 Stage1/HISS。Stage2 后续按最终 Stage1 标准修改。
