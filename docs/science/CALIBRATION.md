# Calibration Science

## 目的

去除仪器签名（bias/dark/flat/cosmetic），使信号可比较。

## 科学定义

校准后信号（双分支，与 `lib/calibration/src/calibrator.cpp:104-136` 一致）：

```text
dark_opt=0 (默认): cal = (raw − dark) / flat_norm          # Dark 已含 Bias
dark_opt=1:          cal = (raw − bias − K·(dark − bias)) / flat_norm  # K = t_light / t_dark
```

flat_norm 为归一化平场，冻结为 median=1.0，且逐像素 clamp：flat_norm = max(median_flat, 0.1)（见 `normalize_flat` median→1.0, <0.1→0.1，`calibrator.cpp:78-93,120,130,164,173`）。

## 变量/单位

- raw/bias/dark/flat：ADU（同曝光时长维度）；t_expo：s；K：无量纲（曝光时间比 t_light/t_dark）；cal：归一化 ADU；flat_norm：无量纲（median=1.0, floor 0.1）。

## 假设

- bias 与曝光无关；dark 与曝光线性（dark_opt=1 时通过 K 线性缩放）；flat 对光源谱形状敏感（需滤镜匹配）。

## 有效域

- masterDark/masterFlat 分组（按曝光时长/滤镜）由 orchestrator 层保证，calibration 层仅接收已分组母版；曝光在母版覆盖范围。

## 不保证

- 不校正非线性/电子增益（增益在测光定标层处理）。

## 失效条件

- 母版缺失/滤镜不匹配 → 显式错误（NO_DATA/CONFIG，由 orchestrator 抛出）。
- 平场缺失/损坏/异常小值 → 不显式拒绝：flat_norm = max(flat, 0.1) 静默 clamp 至 0.1 继续流水，记录警告；仅 FITS 读写失败等 IO 错误才显式拒绝（与 `calibrator.cpp:90,120,164` `max(flat,0.1f)` 一致）。

## 系统/随机误差

- 系统：flat 大尺度不均匀残差（低阶）、暗电流温度漂移；
- 随机：bias/dark/flat 母版噪声（不由 calibration 层传播至 ivar；ivar 由 snr_estimator 独立估计，不含母版方差项）。

## 数值精度

FP64 母版算术；Flat 归一化 median=1.0 + floor 0.1；坏点修复用邻域插值（cosmetic_corrector）。

## 参考文献

标准 CCD 数据归约（Howell 2006）；IRAF/PixInsight 流程语义。

## ID

SCI-CAL-001（母版一致性）、ALG-CAL-001..（S2 注册）。
