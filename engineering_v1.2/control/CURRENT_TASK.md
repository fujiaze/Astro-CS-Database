# 当前任务

`P10-002`：建立 T1-T4 设备档案。

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

## P09-003 已完成（2026-07-27）

- canonical_dataset_v1.2.json：44 文件 SHA-256 已冻结（7 测光失败帧 + 32 银心 Red 帧 + 4 HCSD 基线 + 1 默认 HISS + 4 浏览器配置）
- 4 个 HCSD 字节一致（SHA-256: 2A9BD12E...），证明 P07-001/P07-002/P08-002 stage2 字节级可重现性
- 浏览器性能基线：HISS leaf_index 构建 15s 瓶颈（nside=65536, 61M 像素, 78 子叶）；HCSD 默认视角 RA=0° 不覆盖数据（设计缺陷）；浏览器无 timing instrumentation（0 个 QElapsedTimer/std::chrono）
- 测光失败基线：7 帧 G-002 缺口（n_matched 0/1，sigma_residual=0.0，SNR 模型未构建，HISS has_snr=0，stage2 等权回退）
- 禁止捷径条款合规：失败样本不可替换
- 12/12 测试 PASS，四件套完整
- 证据：engineering_v1.2/evidence/P09-003/（TASK/TEST/EVIDENCE_INDEX/REVIEW + canonical_dataset + browser_baseline + photometric_failure_baseline + 脚本 + 原始日志）

## P10-001 已完成（2026-07-27）

- 10 个 TestData 子目录发现（7 数据集 + 3 校准目录）
- 7 个说明文档递归读取（素材信息*.txt）
- 49 个 FITS Header 采样 + 27 个 XISF Header 采样
- 4 个交付物：
  - TESTDATA_EQUIPMENT_CATALOG.csv（3 行 T2/T3/T4 设备档案）
  - TESTDATA_DATASET_CATALOG.csv（49 行数据集清单）
  - FILTER_ALIAS_MAP.json（6 规范滤镜名）
  - DOCUMENT_FACT_CONFLICTS.md（1 冲突：OIII 别名不一致）
- 硬门限 PASS（T1-T4 设备 ID + Light 归属）
- 12/12 测试 PASS，四件套完整
- 证据：engineering_v1.2/evidence/P10-001/

## 下一步：P10-002

依据 `tasks/P10-002.md`：

- 建立 T1-T4 设备档案
- 依赖 P10-001（已满足）

