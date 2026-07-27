# 当前任务

`P09-002`：确认共享检测主线与统一命名。

## P09-001 已完成（2026-07-27）

- v1.1 仓库事实快照已记录（HEAD=ed145a7, 分支=main）
- 旧 G0–G8 证据完整性已核对（31 任务证据目录全部保留）
- v1.2 开发包已解压安装到 `engineering_v1.2/`，并通过 `validate_pack.py`（tasks=50）
- v1.1 过度结论已显式记录 6 项（DECISION_REGISTER.md CORR-001 ~ CORR-006）
- 工具集已扩展（tools/astro_toolkit.py 新增 unzip/move_dir/run_python/file_exists/rmtree）
- 证据：engineering_v1.2/evidence/P09-001/（TASK/TEST/EVIDENCE_INDEX/REVIEW 四件套 + 原始证据）

## 下一步：P09-002

依据 `tasks/P09-002.md`：

- 源码和运行时证明每帧只检测一次
- 确认主线提交（ PlateSolve 内部单次检测 + callback 共享导出）
- 将能力/日志命名改为 `INTERNAL_DETECTION_SHARED_EXPORT`
- 禁止重写 PlateSolve 算法或改回外部预检测
