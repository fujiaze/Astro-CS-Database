# 当前任务

`P09-003`：冻结测光失败帧与浏览器性能基线数据。

## P09-001 已完成（2026-07-27）

- v1.1 仓库事实快照已记录（HEAD=ed145a7, 分支=main）
- 旧 G0–G8 证据完整性已核对（31 任务证据目录全部保留）
- v1.2 开发包已解压安装到 `engineering_v1.2/`，并通过 `validate_pack.py`（tasks=50）
- v1.1 过度结论已显式记录 6 项（DECISION_REGISTER.md CORR-001 ~ CORR-006）
- 工具集已扩展（tools/astro_toolkit.py 新增 unzip/move_dir/run_python/file_exists/rmtree）
- 证据：engineering_v1.2/evidence/P09-001/（TASK/TEST/EVIDENCE_INDEX/REVIEW 四件套 + 原始证据）

## P09-002 已完成（2026-07-27）

- PlateSolve 单次检测主线已确认：每帧 sdet_detect_ex 调用 1.0 次（引用 P02-007 既有证据 730/730）
- 710 帧 A/B 既有证据无回归（引用 P02-003 + P02-007，5/5 非退化门限 PASS）
- INTERNAL_DETECTION_SHARED_EXPORT 命名已统一：
  - capabilities 新增 `internal_detection_shared_export`（cli_command.cpp:1696）
  - 日志/注释统一为 INTERNAL_DETECTION_SHARED_EXPORT（保留 "(历史 P02-00X 路径 B)" 可追溯引用）
  - C ABI 函数名/C++ 类型名/历史 evidence 文件名保持不变（ABI 兼容）
- 编译验证：ipv_solver.dll + orchestrator.exe 均通过（仅既有警告，无新错误）
- 6/6 测试 PASS，四件套完整
- 证据：engineering_v1.2/evidence/P09-002/（BASELINE/TASK/TEST/EVIDENCE_INDEX/REVIEW + 原始日志）

## 下一步：P09-003

依据 `tasks/P09-003.md`：

- 选定 T1–T4 测光代表帧、银心 32 帧、当前 HCSD 和浏览器固定视角
- 记录 canonical_dataset_v1.2.json 与基线性能记录
- 必测项：修改前事实/失败基线、对应测试、真实数据/性能测试、回归、原始日志
- 禁止捷径：不得用随意选择的数据替换失败样本

