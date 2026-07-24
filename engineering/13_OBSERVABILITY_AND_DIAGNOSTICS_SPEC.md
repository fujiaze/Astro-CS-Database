# 13 可观测性与诊断 Spec

## 1. 日志

统一结构化字段：

- timestamp、level、module、stage、run_id、frame_id；
- event_code；
- message；
- duration_ms；
- error_code；
- degraded/fallback；
- thread_count；
- input/output summary。

禁止核心错误只 `fprintf(stderr)` 而无结构化记录。

## 2. 指标

每阶段至少记录：

- 耗时；
- 峰值内存；
- 输入/输出元素数；
- 星点数、匹配数、有效 PSF 数；
- scale、sigma residual、SNR 控制点数；
- HISS 像素数与覆盖；
- Stage 2 overlap、样本、拒绝、权重与接缝摘要。

## 3. 退化与回退

任何退化路径必须：

- 有稳定 event_code；
- 写入 run manifest；
- 写入输出元数据；
- 可配置为 warning 或 hard fail；
- 有独立测试。

例如：无主帧、Gaia 不可用、无 SNR、无重叠、梯度拟合失败、Winsorized 回退。

## 4. 检查点

Stage 1/Stage 2 新路径必须真正集成检查点，而不是只保留旧类。检查点应绑定：

- 输入哈希；
- 配置哈希；
- 代码/产物版本；
- 已完成阶段输出哈希。

任一不一致必须拒绝错误恢复。

## 5. 故障注入

- 杀进程；
- 磁盘满/写入失败；
- 缺 DLL；
- 错版本 DLL；
- Gaia 路径缺失；
- 损坏 HISS；
- 无重叠；
- 内存分配失败模拟；
- 超时；
- 输出目标已存在。
