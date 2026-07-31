# Stage1 测光与 SNR

> 本页面为已冻结规范，源自 `02_FROZEN_STAGE1_HISS_SPEC.md`。Agent 可实现和文档化，不得自行改写科学语义。

## 1. Gaia 光谱积分校准

Gaia 光谱积分校准是 Stage1 正式步骤。测光比例在 Drizzle 前应用：

\[
I_{photo}=k_{photo}I_{cal}
\]

## 2. HISS signal 语义

HISS `signal` 保存：

> 已应用 Gaia 光谱积分校准的统一相对测光累计通量。

- **不是**原始 ADU；
- 暂不宣称 W/m² 等绝对物理单位；
- 建议 `BUNIT=ASTROCS_RELATIVE_FLUX`；
- `PHOTSCAL` 记录实际应用比例；
- `PHOTAPPL=TRUE`。

## 3. Drizzle 累计通量

源像素 j 对目标像素 p 的贡献：

\[
F_p=\sum_j L_j\frac{a_{jp}}{A_{j,drop}}
\]

- `L_j` 是已测光校准的源像素总信号；
- `a_jp` 是真实球面重叠面积；
- `A_j,drop` 是 pixfrac 缩小后 drop 面积；
- drop 未被有效域截断时，单个源像素全部目标贡献之和必须恢复 `L_j`。

## 4. 内部精度

内部 WCS/SIP、球面几何、重叠和累加使用 float64；最终 signal 写为 IEEE 754 float32，小端序。**禁止有损量化。**

## 5. SNR 控制点

保持精简。每个控制点仅保存：

- `local_ipix : uint32`；
- `snr : float32`。

估计方法、窗口尺度和数量只在 SNR 子块头保存一次。不得增加大量每点状态量，除非后续用户另行冻结。
