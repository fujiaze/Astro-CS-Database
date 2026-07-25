# TASK_REPORT

- Task ID: P02-003（PlateSolve 全量 A/B 与路径决策 v1.1 开发包）
- Commit/base: HEAD = ba4f0d3（path-b-callback-export 分支, "P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API"）；远端 origin = https://github.com/fujiaze/Astro-CS-Database.git；包版本 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- Objective: 对冻结的全部 710 帧 TestData 比较 P02-001 旧路径 (ipv_solve_from_memory + 第二次 sdet_detect_ex) 与 P02-003 路径B (ipv_solve_from_memory_with_callback + callback 导出), 按硬规则选择生产数据路径。
- Changes:
  - 修改 `lib/orchestrator/cpp/src/orchestrator.cpp`：新增 `PathBCallbackCtx` 结构与 `path_b_detection_callback` 函数; 在 `run_stage_platesolve` 中将 `ipv_solve_from_memory` 替换为 `ipv_solve_from_memory_with_callback`; 移除原第二次 `sdet_detect_ex` 调用, 改用 callback 导出的检测结果写 `star_det` 块 (FLOAT32 [N,4])。
  - 扩展 `engineering/tools/batch_platesolve_test.py`：新增 `--mode path-b`; 新增 `read_fits_pixels` (FITS 像素读取为 numpy float32) 和 `solve_single_frame_path_b` (callback 模式求解); 修改主入口按 mode 选择求解函数和 summary 文件名。
  - 新增 `engineering/tools/p02_003_ab_compare.py`：A/B 对比工具, 加载 P02-001 旧路径基线与 P02-003 路径B 结果, 逐帧对比 WCS/RMS/n_pairs/duration, 应用 10 项非退化门限, 检查失败帧集, 输出 `ab_comparison.json` 与路径决策。
  - 新增 `engineering/evidence/P02-003/**`：730 个 frame_*.json + path_b_results.json + full_run.log + ab_comparison.json + 4 份标准报告 + PLATESOLVE_PATH_DECISION.md + ADR.md。
- Files:
  - `lib/orchestrator/cpp/src/orchestrator.cpp`（修改, 126396 字节, SHA-256 41EED936076752C27AE281AAE4590D01E25AE80CE4ADD79E3C5BE2D7687079BC）
  - `engineering/tools/batch_platesolve_test.py`（扩展, 28090 字节, SHA-256 DFC1CF009A21810522475CB57A3C47C3C12E5CC5619DC322C7B68D279485FC36）
  - `engineering/tools/p02_003_ab_compare.py`（新增, 18029 字节, SHA-256 ADC9F304F629D0345CBAD533928C046BFFD3A0F1660F988B45AC356EDB3D0D4A）
  - `engineering/evidence/P02-003/path_b_results.json`（路径B 结构化结果, 476119 字节, SHA-256 D577B6F93DF20DBF1ECD6CE71E023B6D739EBF8845F02DDD147BC4F21B66EF76）
  - `engineering/evidence/P02-003/ab_comparison.json`（A/B 对比+门限检查+决策, 444543 字节, SHA-256 E6EED78FAC0E872012382F8B210E78FE7D7CF260263091AD4ECFADD610E95198）
  - `engineering/evidence/P02-003/results/full_run.log`（全量运行日志, 12861848 字节, SHA-256 6DE54ACDD15B4680302AB2EFC57E8D2B855762DB2B3026D8E5B1EAC3303187A0）
  - `engineering/evidence/P02-003/results/frame_0001.json` ~ `frame_0710.json`（710 单次 + 20 重复性, 共 730 个文件）
  - `engineering/evidence/P02-003/PLATESOLVE_PATH_DECISION.md`（路径决策文档）
  - `engineering/evidence/P02-003/ADR.md`（ADR-005）
  - `engineering/evidence/P02-003/TASK_REPORT.md`（本文件）
  - `engineering/evidence/P02-003/TEST_REPORT.md`（测试报告）
  - `engineering/evidence/P02-003/EVIDENCE_INDEX.md`（证据索引含 SHA-256）
  - `engineering/evidence/P02-003/REVIEW_REPORT.md`（独立复核报告）
- Compatibility:
  - `ipv_solve_from_memory_with_callback` 在 callback=NULL 时与 `ipv_solve_from_memory` 完全一致 (向后兼容)
  - `star_det` 块格式不变 (FLOAT32 [N,4]: x, y, flux, mag), PSF 阶段消费方无需修改
  - WCS 输出格式不变 (CD+CRVAL+CRPIX+SIP), 下游阶段无需修改
  - 旧 API `ipv_solve_from_memory` 仍在 DLL 中保留, 未被移除
- Rollback:
  - 路径 B 代码在 `path-b-callback-export` 分支
  - 若生产环境发现问题, 可 `git revert` 合并 commit 或 `git checkout main -- lib/orchestrator/cpp/src/orchestrator.cpp` 回退
  - 旧路径 API 仍在 DLL 中, 回退不影响其他模块
- Remaining risks:
  - **60 帧 WCS 浮点噪声差异**: 60/709 帧 WCS 存在 1e-12~1e-14 度级差异, 源于 FITS 文件 I/O (uint16→float32 在 C++ 内) vs 内存 buffer (numpy float32 转换) 的浮点累加顺序差异, 远低于物理意义, 不影响下游测光/叠加。
  - **frame 50 (Oiii 窄带) 失败**: 与旧路径同一根因 (star_detector 窄带灵敏度不足, error_msg 为空), 非路径 B 引入的问题。
  - **duration 中位 +1.8%**: callback 复制检测结果 (2000 星 × 6 字段 × 8 字节 = 96KB/帧) 引入微小开销, 被 p99 -29.1% 改善抵消。

## 详细执行结果

### 1. A/B 全量对比 (710 帧)

| 指标 | 旧路径 (P02-001) | 路径 B (P02-003) | 门限 | 结果 |
|---|---|---|---|---|
| 总成功率 | 99.86% (709/710) | 99.86% (709/710) | ≥ 99.0% | PASS |
| RMS 中位 | 0.2852" | 0.2852" | ≤ 0.30" | PASS |
| RMS p99 | 0.8663" | 0.8663" | ≤ 1.00" | PASS |
| RMS max | 1.4907" | 1.4907" | ≤ 1.60" | PASS |
| n_pairs 中位 | 34 | 34 | ≥ 30 | PASS |
| n_pairs min | 13 | 13 | ≥ 10 | PASS |
| duration 中位 | 1.302s | 1.326s | ≤ 1.50s | PASS |
| duration p99 | 9.682s | 6.867s | ≤ 12.00s | PASS |
| 重复性 max dRA | 0° | 0° | ≤ 1e-10° | PASS |
| 重复性 max dDec | 8.88e-15° | 0° | ≤ 1e-13° | PASS |
| 失败帧集 | {50} | {50} | ⊆ 旧 ∪ {窄带} | PASS |

### 2. 逐帧对比统计

| 项 | 值 |
|---|---|
| 总对比帧数 | 710 |
| 双方成功 | 709 |
| 旧成功->新失败 (退化) | 0 |
| 旧失败->新成功 (改善) | 0 |
| 双方失败 | 1 (frame 50, Oiii 窄带) |
| WCS bit-wise 一致 | 649/709 |
| WCS 浮点噪声差异 | 60/709 (1e-12~1e-14 度) |
| RMS 差异 min | -1.089e-12" |
| RMS 差异 median | 0.000000" |
| RMS 差异 max | 1.327e-13" |
| n_pairs 差异 | 全部为 0 |

### 3. 路径 B 按目标天区

| target_name | 帧数 | 成功 | 失败 | 成功率 | median_rms | avg_dur |
|---|---:|---:|---:|---|---|---|
| Galaxy_Center | 157 | 156 | 1 | 99.4% | 0.359" | 1.50s |
| LDN43 | 42 | 42 | 0 | 100.0% | 0.119" | 1.17s |
| NGC1727 | 64 | 64 | 0 | 100.0% | 0.125" | 2.82s |
| NGC247 | 68 | 68 | 0 | 100.0% | 0.179" | 0.80s |
| NGC55 | 79 | 79 | 0 | 100.0% | 0.133" | 0.84s |
| NGC83_cluster | 72 | 72 | 0 | 100.0% | 0.182" | 0.85s |
| Victory_Nebula | 228 | 228 | 0 | 100.0% | 0.450" | 1.73s |

### 4. 重复性 (前 10 帧 × 3 次)

| 帧 | dRA (°) | dDec (°) | rms_std (") | dur_std (s) |
|---|---|---|---|---|
| 1-10 | 0.000000 | 0.000000 | 0.0000 | 0.004-0.581 |

结论: 路径 B 输出 WCS 完美确定性可重现, 与旧路径一致。

### 5. 路径决策

- 决策: **MERGE_PATH_B** (UPSTREAM_SHARED_DETECTIONS)
- 原因: 全部 10 项非退化门限 PASS, 无旧成功->新失败退化, 失败帧集未超出允许范围
- 详见: PLATESOLVE_PATH_DECISION.md + ADR.md
