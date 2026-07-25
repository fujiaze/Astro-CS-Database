# TEST_REPORT

- Task ID: P02-003（PlateSolve 全量 A/B 与路径决策 v1.1 开发包）
- 测试时间: 2026-07-25 13:14:37 → 13:32:32 (wall 1075.0s = 17.9 min, env init 0.17s)
- 测试环境: Windows, Python 3.12, ipv_solver.dll (SHA-256 804B2F2F..., 984362 字节)
- 测试分支: path-b-callback-export (commit ba4f0d3)

## 测试范围

### 全量 testdata (710 帧)

- 命令: `python engineering\tools\batch_platesolve_test.py --mode path-b --manifest engineering\evidence\P02-001\testdata_manifest.json --output-dir engineering\evidence\P02-003\results --repeat-first 10`
- 总运行次数: 730 (710 单次 + 20 重复)
- 测试帧覆盖: Galaxy_Center 157 + LDN43 42 + Victory_Nebula 228 + NGC55 79 + NGC247 68 + NGC83_cluster 72 + NGC1727 64 = 710
- 滤镜覆盖: Red 132 + Green 137 + Blue 132 + Lum 160 + H-alpha 77 + OIII 25 + Oiii 47 = 710

### A/B 对比

- 命令: `python engineering\tools\p02_003_ab_compare.py`
- 对比对象: P02-001 旧路径基线 (old_path_baseline.json) vs P02-003 路径B (path_b_results.json)
- 对比帧数: 710 (全量)

## 测试结果

### 1. 全量运行结果

| 项 | 值 |
|---|---|
| 总帧数 | 710 |
| 成功 | 709 (99.86%) |
| 失败 | 1 (frame 50, Oiii 窄带, 与旧路径同一根因) |
| 总耗时 | 1075.0s (17.9 min) |
| 平均耗时 | 1.47s/帧 |
| RMS 角秒 | min=0.091, median=0.285, mean=0.312, p90=0.546, p99=0.866, max=1.491 |
| n_pairs | min=13, median=34, mean=34.93, p90=42, p99=50, max=56 |
| duration | min=0.662s, median=1.326s, mean=1.467s, p90=1.979s, p99=6.867s, max=21.78s |

### 2. 非退化门限检查 (10/10 PASS)

| 门限 | 旧路径基线 | 路径 B | 门限值 | 结果 |
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

### 3. 失败帧集检查

- 旧路径失败帧: {50}
- 路径 B 失败帧: {50}
- 新失败帧超出允许范围: {} (空)
- 结果: PASS (路径 B 失败帧集 ⊆ 旧路径失败帧集 ∪ {窄带})

### 4. 重复性测试 (前 10 帧 × 3 次)

| 帧 | target | dRA (°) | dDec (°) | rms_std (") | dur_std (s) |
|---|---|---|---|---|---|
| 1 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.581 |
| 2 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.034 |
| 3 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.060 |
| 4 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.048 |
| 5 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.072 |
| 6 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.004 |
| 7 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.034 |
| 8 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.057 |
| 9 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.066 |
| 10 | Galaxy_Center | 0.000000 | 0.000000 | 0.0000 | 0.073 |

结论: 10/10 帧 3/3 运行成功 (30/30), WCS 差异为 0 (完美确定性)。

### 5. WCS bit-wise 一致性

| 项 | 值 |
|---|---|
| 对比帧数 (双方成功) | 709 |
| WCS bit-wise 完全一致 | 649 (91.5%) |
| WCS 浮点噪声差异 | 60 (8.5%) |
| 差异量级 | 1e-12~1e-14 度 (≈1e-7~1e-9 角秒) |
| 差异根因 | FITS 文件 I/O (uint16→float32 在 C++ 内) vs 内存 buffer (numpy float32 转换) 的浮点累加顺序差异 |
| 物理影响 | 无 (远低于星检测 RMS 0.285" 和 Gaia 星表精度) |

### 6. 失败帧分析

- frame 50: Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii.fts
- 滤镜: Oiii, 曝光: 600s
- 症状: success=false, duration=1.09s, n_pairs=0, error_msg="" (空)
- 根因: 窄带 OIII 600s 信噪比不足, star_detector 检测阶段无候选星
- 与旧路径对比: 旧路径同一帧同样失败 (frame 50), 同一根因
- 处置: 不修复 (star_detector 优化属独立 spec, 超出 plate_solve 范围)

## 测试结论

- 全量 710 帧 A/B 测试完成, 路径 B 与旧路径在成功率、RMS、n_pairs 上完全一致
- 10/10 非退化门限全部 PASS
- 重复性完美 (10/10 帧 3/3 成功, WCS 差异为 0)
- 失败帧集未超出允许范围 (frame 50 与旧路径同一根因)
- 路径决策: MERGE_PATH_B
- 详见: ab_comparison.json + PLATESOLVE_PATH_DECISION.md + ADR.md
