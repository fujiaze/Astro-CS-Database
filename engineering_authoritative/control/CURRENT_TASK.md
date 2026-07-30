# 当前任务

`A-004`：实现 Light 到 Bias/Dark/Flat 唯一解析与严格模式。

## 已完成
- A-001: 安装权威 README，迁移报告已生成
- A-002: T1-T4 设备档案 + 5 个产物（设备/滤镜/Master清单/Light解析/未解决报告）
- A-003: 滤镜别名映射 + 主校准帧清单（A-002 已覆盖）

## 未解决问题（A-004 需处理）
- T2 Lum Flat 缺失（影响 LDN43_T2, NGC247_T2 的 Lum 通道）
- T4 Lum Flat 缺失（影响 Victory_T4 的 Lum 通道）
- T2/T4 无 Lum Flat → 解析器需明确标记为 UNRESOLVED，不可静默降级
