# Stage1 HEALPix Drizzle 规范

> 本页面为已冻结规范，源自 `02_FROZEN_STAGE1_HISS_SPEC.md`。Agent 可实现和文档化，不得自行改写科学语义。

## 1. 自动 NSIDE

用户显式给出合法 NSIDE 时，CLI 直接采用，**不警告、不修改、不因性能降低**。

未给出时：

- 依据最终 WCS/SIP 在有效视场内的局部 Jacobian；
- 找到最细局部输入像素尺度；
- 选择最小的 2 次幂 NSIDE，使 HEALPix 线性像素尺度不粗于该最细尺度；
- 结果约为 1～2 倍线性过采样。

目标支持约 0.1 角秒级到 1 度级像素。

## 2. HEALPix ordering

- HISS 内部统一 **NESTED**；
- 所有 ipix 都是 NESTED；
- CLI 不提供 RING 选项；
- 外部 RING 只能在边界适配器转换。

## 3. pixfrac

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

## 4. Drizzle 累计通量

\[
F_p=\sum_j L_j\frac{a_{jp}}{A_{j,drop}}
\]

- `L_j` 是已测光校准的源像素总信号；
- `a_jp` 是真实球面重叠面积；
- `A_j,drop` 是 pixfrac 缩小后 drop 面积；
- drop 未被有效域截断时，单个源像素全部目标贡献之和必须恢复 `L_j`。

内部 WCS/SIP、球面几何、重叠和累加使用 float64；最终 signal 写为 IEEE 754 float32，小端序。**禁止有损量化。**

## 5. support

\[
S_p=\frac{\sum_j a_{jp}}{A_p}
\]

support 是单帧目标 HEALPix 像素被有效 drop 覆盖的纯几何面积比例：

- 与 SNR、曝光、测光比例和后续权重无关；
- 合法范围 0～1；
- 内部 float64；
- 仅浮点误差级超限可钳制；明显超过 1 是几何/WCS 或实现错误；
- HISS 中存 uint8：`round(255*S)`。
