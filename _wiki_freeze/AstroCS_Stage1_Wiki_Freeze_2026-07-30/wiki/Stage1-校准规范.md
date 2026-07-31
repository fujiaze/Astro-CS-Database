> **SUPERSEDED — DO NOT IMPLEMENT**
>
> 本页面已被新标准页面取代，仅保留作历史迁移参考，不得作为实现依据。
> 请参阅 Home.md 中的"Stage1 标准页面（权威来源）"获取最新冻结规范。

# Stage1 校准规范

## 状态

- 校准范围与公式：**已冻结**
- 暗场优化系数 k：**已冻结**（k = t_light / t_dark，曝光时间比例）

## 1. 输入定义

记：

- \(L(x,y)\)：单色 Light；
- \(B(x,y)\)：Master Bias；
- \(D(x,y)\)：包含 Bias/Offset 成分的 Master Dark；
- \(F(x,y)\)：已校准并归一化的 Master Flat；
- \(k\)：暗场优化系数。

Master Dark 的定义是：

\[
D(x,y)=B(x,y)+D_{\mathrm{thermal}}(x,y)
\]

Master Flat 必须在进入 Stage1 前完成校准和归一化。Stage1 不制作、不校准、不重新归一化 Flat。

## 2. 模式 A：标准主暗场校准

适用于曝光、温度、Gain、Offset、读出模式等正确匹配的 CMOS Master Dark。

\[
I_{\mathrm{cal}}(x,y)=
\frac{L(x,y)-D(x,y)}{F(x,y)}
\]

规则：

- Master Dark 已包含 Bias，因此不能再次减去 Master Bias；
- 标准模式不使用 Dark Scaling；
- Master Bias 在此模式中不是必需输入；
- 不裁切负值；
- 不自动增加 Pedestal；
- 不自动把负值夹紧到零；
- 不执行 Overscan 或裁切。

## 3. 模式 B：暗场优化校准

暗场优化只能缩放去除 Bias 后的纯暗信号分量：

\[
D_{\mathrm{thermal}}(x,y)=D(x,y)-B(x,y)
\]

正式校准公式：

\[
I_{\mathrm{cal}}(x,y)=
\frac{L(x,y)-B(x,y)-k[D(x,y)-B(x,y)]}{F(x,y)}
\]

硬规则：

- Bias 只减一次；
- Bias 绝不参与缩放；
- 只有 \(D-B\) 乘以 \(k\)；
- 不允许同时拟合额外二维背景；
- 不允许在校准阶段修改测光尺度；
- 不允许把梯度校正混入暗场优化。

暗场优化模式必须同时提供：

- Light；
- Master Bias；
- Master Dark；
- Master Flat。

## 4. WBPP 吸收边界

AstroCS 只参考 WBPP/PixInsight 中以下语义：

- 使用现成 Master 校准 Light；
- 标准匹配 Master Dark 的校准路径；
- Dark Optimization 对纯暗信号分量进行缩放的原则。

不吸收：

- 文件分组；
- session/date；
- 自动匹配；
- Overscan；
- 裁切；
- Master 生成；
- CFA；
- Cosmetic Correction 工作流；
- 注册；
- Local Normalization；
- Integration；
- Subframe Weighting。

## 5. 数值和错误规则

- 输入科学数据统一转为 float32 内存图像；
- 校准计算不得降低到整数；
- Flat 非有限或绝对值低于冻结阈值时，该像素不可参与正常除法；
- 尺寸不一致、通道数不为 1、必需 Master 缺失时返回硬错误；
- CLI 不负责曝光、温度、Gain、Offset 匹配警报，这些由 GUI 负责；
- CLI 仍需返回稳定错误码和机器可读错误信息。

## 6. 暗场优化系数 k（已冻结）

### 6.1 计算方法

k 通过曝光时间比例直接计算，不使用优化搜索或稳健回归：

\[
k = \frac{t_{\mathrm{light}}}{t_{\mathrm{dark}}}
\]

- \(t_{\mathrm{light}}\)：Light 帧曝光时间，从 FITS 头 `EXPTIME` 关键字读取；
- \(t_{\mathrm{dark}}\)：Master Dark 帧曝光时间，从 FITS 头 `EXPTIME` 关键字读取。

### 6.2 规则

- 曝光时间相同时 k = 1.0，等同于标准模式；
- FITS 头无 `EXPTIME` 关键字时返回硬错误（不回退到优化搜索）；
- k 不做范围限制（由实际曝光时间决定）；
- 不使用黄金分割搜索、MAD 最小化或其他优化方法；
- 不需要 Reference 数据和误差门限。
