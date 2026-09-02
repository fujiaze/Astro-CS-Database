# 主校准帧解析与匹配规范

用户确认全部 Master 已配齐。解析器报告缺失时，先检查别名、大小写、Unicode、文件名模板、Header 和目录层级。

匹配键按可用性组合：设备 ID、传感器尺寸、Bin、Gain/Offset、曝光、温度范围、滤镜规范名、Master 类型。文件名仅为线索，不是唯一真相。

Dark 允许明确的温度/曝光匹配规则；Flat 必须匹配滤镜、Bin 和几何尺寸；Bias 匹配传感器模式。每个 Light 输出唯一解析结果和选择理由，不允许“找到第一份就用”。

输出 `CALIBRATION_MASTER_INVENTORY.csv`、`LIGHT_TO_MASTER_RESOLUTION.csv`、`UNRESOLVED_CALIBRATION_REPORT.md`。最终 unresolved 必须为 0，或由真实数据损坏证据支持的 BLOCKED。
