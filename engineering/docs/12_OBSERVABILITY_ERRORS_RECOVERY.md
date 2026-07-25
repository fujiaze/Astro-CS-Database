# 12 可观测性、错误与恢复

## 1. 关联 ID

所有事件、日志、临时文件和报告包含 job_id、frame_id、stage、module_version、input_hash。

## 2. 质量指标

每个 stage 输出结构化指标，不只输出文本：检测数、匹配数、RMS、PSF 成功率、scale、sigma、SNR 点数、n_pix、覆盖与拒绝率。

## 3. 错误传播

模块错误码映射到 AstroCS 稳定错误码，保留原始 module_code 和 message。不得把“无结果”转换为 warning 后继续。

## 4. 资源清理

使用 RAII 或统一 cleanup 保证：DLL 结果、Frame 块、Master 缓存、Gaia 查询结果、临时文件、线程和句柄在成功/失败/取消路径均释放。

## 5. 恢复

任务状态只用于决定重跑，不等价于数据快照。`.aio` 未正式接入前，Stage1 中断从头重跑；Stage2 可从完整 HISS 集合重跑。所有 partial 输出须隔离。
