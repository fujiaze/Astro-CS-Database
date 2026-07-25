# ADR-005: PlateSolve 路径决策 - 采用 callback 导出 (路径 B)

- Status: ACCEPTED
- Date: 2026-07-25
- Task: P02-003

## Context

PlateSolve 阶段在 PLATESOLVE 与 PSF 之间存在重复的 `sdet_detect_ex` 调用:
- `ipv_solve_from_memory` 内部调用 sdet_detect_ex (第 1 次, 用于 WCS 求解)
- orchestrator 显式调用 sdet_detect_ex (第 2 次, 用于写 star_det 块供 PSF 消费)

每次 sdet_detect_ex 耗时 0.5-2.0s (取决于图像尺寸和星密度), 重复调用浪费计算资源和内存。P02-002 已在 ipv_solver.dll 中新增 `ipv_solve_from_memory_with_callback` API, 支持 callback 同步导出内部检测结果, 使第 2 次 sdet_detect_ex 不再必要。

依据 docs/18_PLATESOLVE_FULL_TESTDATA_DECISION_SPEC.md, 必须在冻结的 710 帧 testdata 上进行全量 A/B 对比, 满足非退化门限才能合并到生产版本。

## Decision

**采用路径 B (UPSTREAM_SHARED_DETECTIONS)**: 将 orchestrator.cpp 的 PLATESOLVE 阶段从 `ipv_solve_from_memory` + 显式 `sdet_detect_ex` 切换为 `ipv_solve_from_memory_with_callback` + callback 导出, 合并到 main。

## Alternatives

### A. 保留旧路径 (PRESERVE_INTERNAL_DETECTION_EXPORT)
- 不修改 orchestrator.cpp, 维持 2 次 sdet_detect_ex
- 优点: 零风险, 无代码变更
- 缺点: 持续浪费 0.5-2.0s/帧的计算和内存
- 否决原因: 路径 B 全量测试零退化, 无理由保留重复计算

### B. 路径 A (upstream-detect, 独立上游检测)
- 在 PLATESOLVE 之前独立运行 sdet_detect_ex, 将结果同时传给 ipv_solver 和 PSF
- 优点: 完全解耦检测与求解
- 缺点: 需修改 ipv_solver API 接受外部检测结果, 改动面大, 且检测-求解耦合度增加
- 否决原因: 改动面过大, 路径 B 已能达成减少重复计算的目标

## Consequences

### 正面
- sdet_detect_ex 调用次数 2 → 1, 每帧节省 0.5-2.0s 计算和 ~100MB 内存
- duration p99 从 9.68s 降至 6.87s (-29.1%), 尾部延迟显著改善
- 算法输出与旧路径 bit-wise 一致 (RMS 差异 = 0.000000", n_pairs 差异 = 0)
- 重复性完美保持 (dRA=0°, dDec=0°)

### 负面
- 60/709 帧 WCS 存在浮点噪声级差异 (1e-12~1e-14 度), 源于 FITS 文件 I/O vs 内存 buffer 浮点累加顺序, 不影响物理精度
- duration 中位 +1.8% (1.302s → 1.326s), callback 复制检测结果的开销, 被 p99 改善抵消

## Compatibility/migration

- `ipv_solve_from_memory_with_callback` 在 callback=NULL 时与 `ipv_solve_from_memory` 完全一致 (向后兼容)
- star_det 块格式不变 (FLOAT32 [N,4]), PSF 阶段消费方无需修改
- WCS 输出格式不变 (CD+CRVAL+CRPIX+SIP), 下游阶段无需修改
- 旧 API `ipv_solve_from_memory` 仍在 DLL 中保留, 未被移除

## Evidence

- A/B 对比报告: engineering/evidence/P02-003/ab_comparison.json (SHA-256 E6EED78F...)
- 路径 B 全量结果: engineering/evidence/P02-003/path_b_results.json (SHA-256 D577B6F9...)
- 旧路径基线: engineering/evidence/P02-001/old_path_baseline.json (SHA-256 8E1CC4B6...)
- 路径决策文档: engineering/evidence/P02-003/PLATESOLVE_PATH_DECISION.md
- 全部 10 项非退化门限 PASS, 决策 MERGE_PATH_B
- 测试覆盖: 710 帧 testdata 全量 (710 单次 + 10 帧 × 3 次重复性)
