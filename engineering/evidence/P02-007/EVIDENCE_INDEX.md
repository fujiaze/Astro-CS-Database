# P02-007 EVIDENCE_INDEX - 证据索引

- 任务: P02-007
- 生成日期: 2026-07-25

## 证据清单

### 1. 全量测试结果
- **path_b_results.json** - 710 帧 Path B 全量测试汇总
  - 成功率: 99.86% (709/710)
  - RMS median: 0.2852"
  - 与基线 delta: +0.00% (无退化)
- **results/frame_0001.json ~ frame_0710.json** - 710 帧逐帧结果
- **results/frame_0001_run2.json ~ frame_0010_run3.json** - 前 10 帧重复性结果 (3 次)

### 2. 基线对比
- **对比基线**: `engineering/evidence/P02-001/old_path_baseline.json`
- **基线 commit**: 7b85ff3
- **当前 commit**: f8097df (Path B 已合并)
- **manifest SHA-256**: 2A9BE035D326B735D6E3C751CFB5342ED9183DCC22FE83322E445F97877E7DCF

### 3. 单次检测证据
- **batch_run.log** - 批量测试完整日志
- **关键证据**: 730 次 `sdet_detect_ex start` / 710 帧 = 1.028 次/帧 (含重复)
- **预期**: 730 = 710 + 20 (前 10 帧重复 3 次 × 2 额外)
- **结论**: 每帧恰好 1 次 sdet_detect_ex

### 4. star_det 同源证据
- **stage1 单帧测试日志**: `lib/orchestrator/logs/orchestrator_2026-07-25.log`
- **关键日志**:
  - `[PLATESOLVE] star_det 块已写入 (路径B): 2000 颗星`
  - `[PSF] star_det: 2000 颗星` (完全一致)
- **代码位置**:
  - 生产者: `orchestrator.cpp` L1475 (`fn_add_block("star_det")`)
  - 消费者: `orchestrator.cpp` L1566 (`fn_get_block("star_det")`)

### 5. stage1 HISS provenance
- **HISS 文件**: `stage1_test/frame_0001.hiss` (47693 字节)
- **nside**: 512 (自适应)
- **HEALPix 像素数**: 3927
- **filter**: Red
- **exptime**: 180.0s

### 6. Gate 验证结果
- **gate_verification.json** - 结构化验证结果
- **最终判定**: CONDITIONAL_PASS
  - 非退化门限: PASS (5/5)
  - 单次检测: PASS
  - production path: PASS (Path B callback)
  - star_det homology: PASS
  - PSF f32 API: FAIL (deferred)
  - stage1 pipeline: PASS (7/7)

### 7. 代码审查证据
- **production path**: `orchestrator.cpp` L1337-1370
  - `ipv_solve_from_memory_with_callback` (Path B)
  - `path_b_detection_callback` (callback 导出)
  - `PathBCallbackCtx` (复制检测结果)
- **PSF API**: `orchestrator.cpp` L1509-1669
  - 仍使用 `dpsf_fit_batch` (uint16 API)
  - L1552: 分配全图 uint16 缓冲
  - L1558-1563: 0-65535 clip

### 8. 测试脚本
- `engineering/tools/batch_platesolve_test.py` - 全量批量测试 (P02-001/P02-003/P02-007 共用)
- `engineering/tools/p02_007_gate_check.py` - Gate 验证脚本

## 报告文件

- `TASK_REPORT.md` - 任务执行报告
- `TEST_REPORT.md` - 测试报告
- `EVIDENCE_INDEX.md` - 证据索引 (本文件)
- `REVIEW_REPORT.md` - 独立复核报告
