# Calibration Science

## 目的

去除仪器签名（bias/dark/flat/cosmetic），使信号可比较。

## 科学定义

校准后信号：

```text
cal = (raw − bias − dark·t_expo) / flat_norm
```

flat_norm 为归一化平场（均值或中值=1 约定，模块文档记录具体选择）。

## 变量/单位

- raw/bias/dark/flat：ADU（同曝光时长维度）；t_expo：s；cal：归一化 ADU。

## 假设

- bias 与曝光无关；dark 与曝光线性；flat 对光源谱形状敏感（需滤镜匹配）。

## 有效域

- masterDark 按曝光时长分组；masterFlat 按滤镜；曝光在母版覆盖范围。

## 不保证

- 不校正非线性/电子增益（增益在测光定标层处理）。

## 失效条件

- 母版缺失/滤镜不匹配 → 显式错误（NO_DATA/CONFIG）。
- 除零（flat_norm=0）→ 显式拒绝。

## 系统/随机误差

- 系统：flat 大尺度不均匀残差（低阶）、暗电流温度漂移；
- 随机：bias/dark/flat 母版噪声传播（母版 σ 计入 ivar 的 uncertainty）。

## 数值精度

FP64 母版算术；坏点修复用邻域插值（cosmetic_corrector）。

## 参考文献

标准 CCD 数据归约（Howell 2006）；IRAF/PixInsight 流程语义。

## ID

SCI-CAL-001（母版一致性）、ALG-CAL-001..（S2 注册）。
