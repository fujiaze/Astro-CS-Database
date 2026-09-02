# PlateSolve 单次检测主线状态

当前正式实现是 PlateSolve 内部调用一次检测器，并通过 callback 导出同一份 detections 给 Orchestrator 的 `star_det`，PSF 复用该块。它不是外部先检测再将星表输入 PlateSolve。

建议统一命名为 `INTERNAL_DETECTION_SHARED_EXPORT`，并在 capabilities、日志、provenance 和文档中使用。任务 P09-002 只核对和修正文案/能力声明，不重做算法。

必须继续保证：

- 每帧 `sdet_detect_ex` 只调用一次；
- PlateSolve 和 PSF 使用相同 detection count/hash；
- 710 帧成功率与 WCS 指标无回归；
- fallback 必须显式记录，生产默认不得静默回到重复检测。
