# REVIEW_REPORT

- Task ID: P02-003（PlateSolve 全量 A/B 与路径决策 v1.1 开发包）
- 复核时间: 2026-07-25
- 复核人: 独立复核 Agent
- 复核对象: engineering/evidence/P02-003/**

## 复核范围

1. 路径 B 代码改动 (lib/orchestrator/cpp/src/orchestrator.cpp)
2. 测试工具扩展 (engineering/tools/batch_platesolve_test.py)
3. A/B 对比工具 (engineering/tools/p02_003_ab_compare.py)
4. 全量 710 帧 A/B 测试结果 (path_b_results.json + ab_comparison.json)
5. 非退化门限检查 (10 项)
6. 路径决策 (PLATESOLVE_PATH_DECISION.md + ADR.md)
7. 4 份标准报告 (TASK_REPORT/TEST_REPORT/EVIDENCE_INDEX/REVIEW_REPORT)

## 复核检查清单

### 1. 范围完整性

| 检查项 | 结果 | 说明 |
|---|---|---|
| 全量 710 帧 testdata 运行 | PASS | 710 单次 + 20 重复 = 730 次运行全部完成 |
| 覆盖所有目标天区 | PASS | 7 个目标天区全部覆盖 (Galaxy_Center/LDN43/Victory_Nebula/NGC55/NGC247/NGC83_cluster/NGC1727) |
| 覆盖所有滤镜 | PASS | 7 种滤镜全部覆盖 (Red/Green/Blue/Lum/H-alpha/OIII/Oiii) |
| A/B 对比帧数 | PASS | 710 帧全量对比 (非抽样) |
| 重复性测试 | PASS | 前 10 帧 × 3 次 = 30 次运行 |

### 2. 代码改动正确性

| 检查项 | 结果 | 说明 |
|---|---|---|
| PathBCallbackCtx 结构 | PASS | POD-like 结构, 含 detections_buf/n_detected/copied, 通过 user_data 传递 |
| path_b_detection_callback 签名 | PASS | 匹配 IpvDetectionCallback 签名 (const double*, int, void*) |
| callback 内立即复制 | PASS | 使用 assign 复制到本地缓冲区 (callback 返回后源指针失效) |
| API 替换 | PASS | ipv_solve_from_memory → ipv_solve_from_memory_with_callback, 参数正确传递 callback+user_data |
| star_det 块写入 | PASS | 使用 callback 结果 (FLOAT64 [N,6] → FLOAT32 [N,4]: x,y,flux,mag), 格式与 PSF 期望一致 |
| 移除第二次 sdet_detect_ex | PASS | 原路径的显式 sdet_detect_ex + float→uint16 转换已移除, sdet_detect_ex 调用次数 2→1 |
| 错误处理 | PASS | callback 未复制 (copied=false) 或 n_detected=0 时不写 star_det 块, 不影响 WCS 求解 |

### 3. 非退化门限

| 检查项 | 旧路径基线 | 路径 B | 门限 | 结果 |
|---|---|---|---|---|
| total_success_rate | 0.9986 | 0.9986 | ≥ 0.99 | PASS |
| rms_arcsec_median | 0.285" | 0.285" | ≤ 0.30" | PASS |
| rms_arcsec_p99 | 0.866" | 0.866" | ≤ 1.00" | PASS |
| rms_arcsec_max | 1.491" | 1.491" | ≤ 1.60" | PASS |
| n_pairs_median | 34 | 34 | ≥ 30 | PASS |
| n_pairs_min | 13 | 13 | ≥ 10 | PASS |
| duration_median | 1.302s | 1.326s | ≤ 1.50s | PASS |
| duration_p99 | 9.682s | 6.867s | ≤ 12.00s | PASS |
| repeat_max_dRA_deg | 0° | 0° | ≤ 1e-10° | PASS |
| repeat_max_dDec_deg | 8.88e-15° | 0° | ≤ 1e-13° | PASS |

**10/10 PASS**

### 4. 失败帧集检查

| 检查项 | 结果 | 说明 |
|---|---|---|
| 旧路径失败帧 | {50} | frame 50, Oiii 窄带 |
| 路径 B 失败帧 | {50} | 同一帧, 同一根因 |
| 新失败帧超出允许范围 | {} (空) | 路径 B 失败帧集 ⊆ 旧路径失败帧集 ∪ {窄带} |
| 结果 | PASS | 无新增非窄带失败 |

### 5. WCS 一致性

| 检查项 | 结果 | 说明 |
|---|---|---|
| bit-wise 完全一致 | 649/709 (91.5%) | 浮点累加顺序相同的帧 |
| 浮点噪声差异 | 60/709 (8.5%) | 1e-12~1e-14 度 (≈1e-7~1e-9 角秒) |
| 差异根因 | FITS 文件 I/O vs 内存 buffer 浮点累加顺序 | 非算法差异, 物理无影响 |
| RMS 差异 | min=-1.089e-12", median=0, max=1.327e-13" | 浮点噪声级别 |
| n_pairs 差异 | 全部为 0 | 完全一致 |
| 结果 | PASS | 差异远低于物理意义, 不影响下游 |

### 6. 重复性

| 检查项 | 结果 | 说明 |
|---|---|---|
| 前 10 帧 × 3 次 | 10/10 帧 3/3 成功 (30/30) | 全部成功 |
| max dRA | 0° | 完美确定性 |
| max dDec | 0° | 完美确定性 |
| max rms_std | 0.0000" | 浮点噪声级别 |
| 结果 | PASS | 与旧路径重复性一致 |

### 7. 路径决策

| 检查项 | 结果 | 说明 |
|---|---|---|
| 决策逻辑 | PASS | 全部门限 PASS + 无旧成功->新失败退化 → MERGE_PATH_B |
| 决策文档 | PASS | PLATESOLVE_PATH_DECISION.md + ADR.md 完整 |
| 兼容性说明 | PASS | callback=NULL 向后兼容, star_det/WCS 格式不变 |
| 回滚方案 | PASS | git revert / git checkout main, 旧 API 仍在 DLL 中 |

### 8. 证据完整性

| 检查项 | 结果 | 说明 |
|---|---|---|
| 730 个 frame_*.json | PASS | 710 单次 + 20 重复, 全部存在 |
| path_b_results.json | PASS | 结构化结果, SHA-256 D577B6F9... |
| ab_comparison.json | PASS | A/B 对比+门限+决策, SHA-256 E6EED78F... |
| full_run.log | PASS | 全量运行日志, SHA-256 6DE54ACD... |
| ipv_solver.dll 一致性 | PASS | lib/ = build/artifacts/ (SHA-256 804B2F2F...) |

## 复核发现

### 无阻塞问题

### 轻微问题 (不阻塞合并)

1. **60 帧 WCS 浮点噪声差异**: 源于 FITS 文件 I/O vs 内存 buffer 的浮点累加顺序差异, 量级 1e-12~1e-14 度, 远低于物理意义。已在 TASK_REPORT 和 PLATESOLVE_PATH_DECISION.md 中记录, 不影响下游测光/叠加质量。

2. **duration 中位 +1.8%**: callback 复制检测结果 (96KB/帧) 的微小开销, 被 p99 -29.1% 改善抵消。在门限 ≤ 1.50s 内。

3. **frame 50 (Oiii 窄带) 失败**: 与旧路径同一根因 (star_detector 窄带灵敏度不足), 非路径 B 引入的问题。

## 复核结论

**VERDICT: PASS**

- 路径 B 全量 710 帧 A/B 测试完成, 10/10 非退化门限全部 PASS
- 零退化 (旧成功->新失败 = 0), 重复性完美 (WCS 差异 = 0)
- 路径决策 MERGE_PATH_B 有充分证据支持
- 代码改动正确 (callback 立即复制, API 替换正确, sdet_detect_ex 调用次数 2→1)
- 兼容性良好 (向后兼容, 格式不变, 回滚可行)
- 证据完整 (730 个结果文件 + 2 个汇总 JSON + 全量日志 + 4 份报告 + 决策文档 + ADR)
