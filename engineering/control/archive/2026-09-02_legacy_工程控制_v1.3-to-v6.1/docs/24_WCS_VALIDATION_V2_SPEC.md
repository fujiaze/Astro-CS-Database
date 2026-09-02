# WCS 验证架构 v2

## 目标

同时验证三件不同的事：求解器拟合质量、标准 WCS 序列化质量、盲目录匹配健康度。三者不得混成单一残差。

## A. Solver Fit 层

来源：IPV 最终 RANSAC/Umeyama inliers。

报告：`n_inliers`、`solver_rms_px`、内部残差分布、TRANS/SIP 阶数。该层是参考基线，但不能单独证明 Header WCS 正确。

## B. Serialized WCS 层（P11 硬 Gate）

固定 A 层的对应关系，仅使用最终 Header/PipelineFrame WCS/SIP回投 Gaia 星。

至少输出：

- `external_to_detector`: WCS预测像素 vs detector质心；
- `external_to_internal_prediction`: WCS预测像素 vs 求解器内部预测；
- median/p68/p90/p99/RMS/max；
- X/Y mean、median、MAD；
- 四象限和边缘分布；
- 每个 pair 的稳定 ID。

### Gate

1. 星对数与求解器最终 inlier 数一致；若剔除非有限值，必须逐项列出原因；
2. `external_to_internal_prediction`：median ≤ 0.05 px，p99 ≤ 0.20 px；若旧 API 暂不能导出内部预测，可临时使用差分 RMS Gate，但必须在 P11-006 补齐接口；
3. `external_to_detector_rms ≤ max(0.35 px, 2 × solver_rms_px + 0.05 px)`；
4. `external_to_detector` median ≤ 0.50 px、p90 ≤ 1.00 px、p99 ≤ 2.00 px；
5. |X mean|、|Y mean| ≤ 0.25 px，且无象限翻转、尺度漂移、90/180°旋转；
6. WCS对象数值闭环 median ≤ 1e-6 px；
7. 代表帧全部通过后才进入 710 回归。

门限若被真实权威星对证明不合理，必须用分布和 ADR调整，禁止为了当前帧临时放宽。

## C. Blind Catalog 层（二级诊断）

用途：发现检测质量、拥挤场、饱和、proper motion、星等选择和 Photometric 匹配问题，不作为 P11 WCS 硬 Gate。

要求：

- 排除饱和、边缘、严重 blend；
- Gaia proper motion 可用时传播到观测历元；
- 使用一对一匹配，不允许多个 Gaia 指向同一 detection；
- 候选半径由 B 层误差与 PSF FWHM决定；
- 优先使用 mutual nearest neighbor，必要时最小代价分配；
- 分星等/FWHM/饱和/边缘报告。

若 C 失败而 B 通过，应进入 Photometric 匹配任务，不得回头改 WCS。
