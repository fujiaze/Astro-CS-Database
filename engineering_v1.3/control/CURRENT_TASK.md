# 当前任务

P12-004 已完成（CONDITIONAL — 0/16 Gate PASS，需进入 P12-005 修复）。下一任务 P12-005 (修复 SNR 模型与 HISS 持久化)。

## 状态
- 上一任务：P12-004 已完成（2026-07-28，T1-T4 与滤镜类别测光矩阵验证，0/16 Gate PASS）
- 当前 Git HEAD：待提交 P12-004 完成证据
- 下一任务：P12-005（修复 SNR 模型与 HISS 持久化）

## P12-004 完成状态
- ✅ 步骤1: 编写 run_photometric_matrix.py 测试脚本（26166 bytes）
- ✅ 步骤2: 运行 16 帧代表帧测光校准（T2/T3/T4 × LUM/RED/GREEN/BLUE/HA/OIII）
- ✅ 步骤3: 收集 PhotometricDiag 诊断字段（20 字段）
- ✅ 步骤4: 生成 PHOTOMETRY_MATRIX.csv（3420 bytes）
- ✅ 步骤5: 生成 photometric_diag_summary.json（18671 bytes）
- ✅ 步骤6: 生成 failure_classification.json（6726 bytes）
- ✅ 步骤7: 检查 Gate（Broadband fit_used ≥ 20, 窄带 ≥ 8, scale ∈ [0.01, 100.0], sigma > 0）
- ✅ 步骤8: 失败分类（INVALID_SCALE 3 帧 + STAGE1_ERROR 13 帧）
- ✅ 步骤9: 生成 TASK_REPORT.md
- ✅ 步骤10: 生成 TEST_REPORT.md
- ✅ 步骤11: 生成 EVIDENCE_INDEX.md（含 SHA256）
- ✅ 步骤12: 生成 REVIEW_REPORT.md (VERDICT: CONDITIONAL_PASS)
- ✅ 步骤13: 更新 CURRENT_TASK.md
- ✅ 步骤14: 更新 MASTER_TASK_REGISTER.csv
- ✅ 步骤15: 更新 PROJECT_STATE.yaml
- ✅ 步骤16: 更新 DECISION_REGISTER.md

## 测试结果
- 测光矩阵测试: 0/16 Gate PASS（全部失败）
  - T4 RED/GREEN/BLUE (3 帧): INVALID_SCALE — scale_factor ≈ 0.0026-0.0028, valid_fsyn=0
  - T4 HA/OIII (2 帧): STAGE1_ERROR — 加载滤光片曲线失败
  - T2 RED/GREEN/BLUE/HA-LDN43 (4 帧): STAGE1_ERROR — 中文路径 filesystem error
  - T2 OIII-NGC1727 + T3 全部 (7 帧): STAGE1_ERROR — 无 Master 文件

## 关键指标
- 总帧数: 16
- Gate PASS: 0
- Gate FAIL: 16
- Gate 通过率: 0.0%
- Broadband: 0/10 PASS
- Narrowband: 0/6 PASS
- INVALID_SCALE: 3 帧
- STAGE1_ERROR: 13 帧（滤光片曲线 2 + 中文路径 4 + 无 Master 7）

## P12-002 修复有效性间接验证
- T4 RED/GREEN/BLUE 空间匹配工作正常（unique_matches=spatial_candidates, rejected_ambiguous=0）
- fit_used 充足（1231-1670），远超 Broadband Gate 阈值 20
- KD-tree 方向 bug 修复 + 双向最近邻唯一配对工作正常

## 待修复问题（P12-005 范围）
1. scale_factor 异常 (3 帧): valid_fsyn=0 表明光谱合成异常，需调查根因
2. 窄带滤光片缺失 (2 帧): filters.json 缺少 HA/OIII 定义，map_filter_name 未正确映射
3. 中文路径处理 (4 帧): C++ std::filesystem 无法处理非 ASCII 路径
4. 校准文件缺失 (7 帧): T2/T3 缺少 Master 文件，配置未启用 allow_no_calibration

## Gate 状态
- G12 Photometric Diagnostic Gate: P12-004 完成（CONDITIONAL），0/16 Gate PASS
- 下一任务 P12-005 (修复 SNR 模型与 HISS 持久化) — 需修复本任务发现的 4 类问题
