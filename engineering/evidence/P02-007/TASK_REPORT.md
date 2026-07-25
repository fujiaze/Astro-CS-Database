# P02-007 TASK_REPORT - PlateSolve 无退化与单次检测专项 Gate 验证

- 任务: P02-007
- 阶段: P02
- 依赖: P02-004; P02-005; P02-006
- Gate: G2
- 执行日期: 2026-07-25
- 执行者: P02-007 子 Agent

## 目标

证明选定路径 (Path B) 不使 PlateSolve 退化、每帧只检测一次、PlateSolve/PSF 星表同源且 PSF 无全图量化。

## 执行步骤与结果

### 1. 重新运行冻结的全量 PlateSolve TestData (710 帧)

- 工具: `engineering/tools/batch_platesolve_test.py --mode path-b`
- 输入: `engineering/evidence/P02-001/testdata_manifest.json` (710 帧, SHA-256 冻结)
- 输出: `engineering/evidence/P02-007/results/frame_0001.json` ~ `frame_0710.json`
- 汇总: `engineering/evidence/P02-007/path_b_results.json`
- 构建: commit f8097df (Path B 已合并)
- 总耗时: ~20 分钟

### 2. 验证最终 production path 与 P02-003 决策一致

- **代码审查**: `lib/orchestrator/cpp/src/orchestrator.cpp` L1337-1370
- **确认**: 使用 `ipv_solve_from_memory_with_callback` + `path_b_detection_callback` (Path B)
- **日志证据**: `[PLATESOLVE] 调用 ipv_solve_from_memory_with_callback (路径B callback 导出)`
- **结论**: PASS

### 3. 验证 detector 调用次数恰好一次, star_det hash 在消费者间一致

#### 单次检测验证
- **方法**: 统计 `batch_run.log` 中 `sdet_detect_ex start` 出现次数
- **结果**: 730 次调用 / 710 帧 = 1.028 次/帧
- **预期**: 710 + 20 (前 10 帧重复 3 次 × 2 额外) = 730
- **结论**: PASS (730 == 730, 每帧恰好 1 次 sdet_detect_ex)

#### star_det hash 同源验证
- **方法**: stage1 单帧测试 + 代码审查
- **生产者**: PLATESOLVE (`run_stage_platesolve` L1475, `fn_add_block("star_det")`)
- **消费者**: PSF (`run_stage_psf` L1566, `fn_get_block("star_det")`)
- **Schema**: FLOAT32 [N,4]: x, y, flux, mag
- **日志证据**:
  - `[PLATESOLVE] star_det 块已写入 (路径B): 2000 颗星`
  - `[PSF] star_det: 2000 颗星` (完全一致)
- **Hash 机制**: 隐式 (同一 PipelineFrame 内存块, 无独立 hash 字段)
- **结论**: PASS

### 4. 验证 Stage 1 真实数据、HISS provenance、性能和内存无未解释回归

#### stage1 单帧测试
- **测试帧**: `Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts`
- **阶段通过**: 7/7 (READ_FITS, CALIBRATE, PLATESOLVE, PSF, PHOTOMETRIC, SNR, DRIZZLE)
- **总耗时**: 25.58s
- **HISS 输出**: `engineering/evidence/P02-007/stage1_test/frame_0001.hiss` (47693 字节)
- **结论**: PASS

#### 非退化门限检查 (相对 P02-001 基线)
| 指标 | Path B 值 | 基线值 | delta | 门限 | 结果 |
|------|----------|--------|-------|------|------|
| success_rate | 0.9986 | 0.9986 | +0.00% | >=99.0% & <0.5% 退化 | PASS |
| RMS median | 0.2852" | 0.2852" | +0.00% | <=0.30" & <5% 退化 | PASS |
| RMS p99 | 0.8663" | 0.8663" | +0.00% | <=1.00" & <10% 退化 | PASS |
| n_pairs median | 34 | 34 | +0.00% | >=30 & <10% 退化 | PASS |
| duration median | 1.2408s | 1.3024s | -4.73% | <=1.50s & <20% 退化 | PASS (更快) |

**关键观察**: Path B 与旧路径产生**完全一致**的 WCS 结果 (delta +0.00%)，证明 callback 导出不影响求解算法。耗时略快 4.73% (可能因 Path B 避免重复读取 FITS 文件)。

## PSF f32 API 集成状态 (残留风险)

- **预期**: `dpsf_fit_batch_f32` (float32 API, 无全图 uint16 量化)
- **实际**: `dpsf_fit_batch` (uint16 API, 全图 0-65535 clip)
- **状态**: NOT_INTEGRATED
- **原因**: P02-005 添加了 f32 API 但未集成到 orchestrator; SNR 阶段依赖 psf 块的 `mad` 字段 (f32 API 不提供)
- **残留风险**: PSF 阶段仍创建全图 uint16 缓冲 (~33MB for 4096×4096), 违反 "PSF 无全图量化" 要求
- **推迟到**: P02-005 后续集成 (需扩展 f32 API 输出 mad/status 或重构 SNR 模型)

## 最终判定

**CONDITIONAL_PASS**

- Path B 无退化与单次检测已验证 (5/5 门限 PASS, 单次检测 PASS)
- PSF f32 API 集成待 P02-005 完成

## 交付物

- `engineering/evidence/P02-007/gate_verification.json` - 结构化验证结果
- `engineering/evidence/P02-007/path_b_results.json` - 全量测试汇总
- `engineering/evidence/P02-007/results/frame_*.json` - 710 帧逐帧结果
- `engineering/evidence/P02-007/stage1_test/frame_0001.hiss` - 单帧 stage1 HISS 输出
- `engineering/tools/p02_007_gate_check.py` - Gate 验证脚本
- 四份标准报告 (TASK_REPORT, TEST_REPORT, EVIDENCE_INDEX, REVIEW_REPORT)
