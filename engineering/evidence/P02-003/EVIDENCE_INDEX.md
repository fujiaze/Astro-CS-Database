# EVIDENCE_INDEX: P02-003 (PlateSolve 全量 A/B 与路径决策 v1.1 开发包)

## 任务标识

- Task ID: P02-003
- 任务名: PlateSolve 全量 A/B 与路径决策 (v1.1 开发包)
- Phase / Gate: P02 / G2
- Commit base: ba4f0d3 (path-b-callback-export, "P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API")
- 远端: https://github.com/fujiaze/Astro-CS-Database.git
- 包版本: 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- 生成时间: 2026-07-25 (PSVersion 7.6.3, Windows, Python 3.12)

## 证据目录

`engineering/evidence/P02-003/`

## 范围声明

- 本任务为 A/B 对比与路径决策: 修改 `lib/orchestrator/cpp/src/orchestrator.cpp` (路径 B: callback 导出), 扩展 `engineering/tools/batch_platesolve_test.py` (--mode path-b), 新增 `engineering/tools/p02_003_ab_compare.py` (A/B 对比工具)。
- 路径 B 核心: `ipv_solve_from_memory_with_callback` (内部 sdet_detect_ex 1 次, callback 同步导出) 替代 `ipv_solve_from_memory` + 显式第二次 `sdet_detect_ex`, 将 sdet_detect_ex 调用次数从 2 减至 1。
- 全量 710 帧 testdata A/B 对比, 应用 10 项非退化门限 (冻结自 P02-001), 决策 MERGE_PATH_B。

## 比较门限 (冻结自 P02-001, 不得事后调整)

| 指标 | 旧路径基线值 | 路径 B 实测值 | 门限 | 结果 |
|---|---|---|---|---|
| 总成功率 | 99.86% (709/710) | 99.86% (709/710) | ≥ 99.0% | PASS |
| RMS 中位 | 0.285" | 0.285" | ≤ 0.30" | PASS |
| RMS p99 | 0.866" | 0.866" | ≤ 1.00" | PASS |
| RMS max | 1.491" | 1.491" | ≤ 1.60" | PASS |
| n_pairs 中位 | 34 | 34 | ≥ 30 | PASS |
| n_pairs min | 13 | 13 | ≥ 10 | PASS |
| duration 中位 | 1.302s | 1.326s | ≤ 1.50s | PASS |
| duration p99 | 9.682s | 6.867s | ≤ 12.00s | PASS |
| 重复性 max dRA | 0° | 0° | ≤ 1e-10° | PASS |
| 重复性 max dDec | 8.88e-15° | 0° | ≤ 1e-13° | PASS |
| 失败帧集 | {50} | {50} | ⊆ 旧 ∪ {窄带} | PASS |

## 证据清单 (主要文件, 含 SHA-256)

### 任务核心证据 (8 个)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| path_b_results.json | 476119 | D577B6F93DF20DBF1ECD6CE71E023B6D739EBF8845F02DDD147BC4F21B66EF76 | 路径B 结构化结果 (_meta/overall/by_target/by_filter/repeatability_first_n/fail_frames/all_frames_summary) |
| ab_comparison.json | 444543 | E6EED78FAC0E872012382F8B210E78FE7D7CF260263091AD4ECFADD610E95198 | A/B 对比+门限检查+决策 (per_frame 逐帧对比+WCS 一致性+RMS/n_pairs/duration 差异) |
| results/full_run.log | 12861848 | 6DE54ACDD15B4680302AB2EFC57E8D2B855762DB2B3026D8E5B1EAC3303187A0 | 全量运行 stdout+stderr 日志 (12.9 MB, 含 710 帧 DEBUG 日志) |
| results/frame_0001.json | 3614 | F2C749193451332975E6F8C2C6C66ACEF80CE314527BC2A474807C924EE3FBBE | 首帧单次运行结果 (Galaxy_Center Red, success, RMS=0.333", dur=2.67s, callback_n_detected=2000) |
| results/frame_0050.json | 2829 | 76EDEDDAB967429A2A4419B34D1A02DA9DDC552017BF211FCB5956C5F1FB64C6 | 失败帧 (Galaxy_Center Oiii 600s 窄带, star_detector 无候选, 与旧路径同一根因) |
| results/frame_0710.json | 3595 | 2995AD5043AB2CF5FD648588E740DB9587B4BB8C56A188AAEBCCA77CEEEF8CFF | 末帧单次运行结果 (Victory_Nebula, success, RMS=0.620", 验证全量完成) |
| PLATESOLVE_PATH_DECISION.md | (见 git commit) | (见 git commit) | 路径决策文档 (UPSTREAM_SHARED_DETECTIONS, MERGE_PATH_B) |
| ADR.md | (见 git commit) | (见 git commit) | ADR-005: 采用 callback 导出 (路径 B) |

### 工具与改动 (3 个)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| lib/orchestrator/cpp/src/orchestrator.cpp | 126396 | 41EED936076752C27AE281AAE4590D01E25AE80CE4ADD79E3C5BE2D7687079BC | orchestrator 核心类 (路径 B: 新增 PathBCallbackCtx + callback, 替换 ipv_solve_from_memory → ipv_solve_from_memory_with_callback, 移除第二次 sdet_detect_ex) |
| engineering/tools/batch_platesolve_test.py | 28090 | DFC1CF009A21810522475CB57A3C47C3C12E5CC5619DC322C7B68D279485FC36 | 轻量 plate solving 测试工具 (扩展 --mode path-b + read_fits_pixels + solve_single_frame_path_b) |
| engineering/tools/p02_003_ab_compare.py | 18029 | ADC9F304F629D0345CBAD533928C046BFFD3A0F1660F988B45AC356EDB3D0D4A | A/B 对比工具 (加载基线+路径B, 逐帧对比, 10 项门限检查, 决策输出) |

### 报告 (4 份 v1.1)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| TASK_REPORT.md | (见 git commit) | (见 git commit) | v1.1 任务执行报告 (含详细执行结果 5 节) |
| TEST_REPORT.md | (见 git commit) | (见 git commit) | v1.1 测试报告 (10 项门限 + 重复性 + WCS 一致性 + 失败帧分析) |
| EVIDENCE_INDEX.md | (self) | (self-referential, 见 git commit) | v1.1 证据索引 (本文件, 含 11+ 个文件 SHA-256) |
| REVIEW_REPORT.md | (见 git commit) | (见 git commit) | v1.1 独立复核报告 (VERDICT: PASS) |

### 全量结果文件 (730 个, 摘要)

| 目录 | 文件数 | 总字节 | 说明 |
|---|---:|---:|---|
| results/frame_0001.json ~ frame_0710.json | 710 | ~2,550,000 | 每帧单次运行结果 (success/duration/wcs/n_pairs/rms/callback_n_detected + 元数据) |
| results/frame_0001_run2.json ~ frame_0010_run2.json | 10 | ~36,140 | 前 10 帧第 2 次重复运行 |
| results/frame_0001_run3.json ~ frame_0010_run3.json | 10 | ~36,147 | 前 10 帧第 3 次重复运行 |
| **合计** | **730** | **~2,622,287** | 全量运行结果集 |

### 复用资产 (lib/ 下, 路径 B 修改)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| lib/plate_solve/cpp/ipv/ipv_solver.dll | 984362 | 804B2F2F54C665DCBC796A021E756DE62D37A4531B878483425ACB9A9E506547 | IPV Solver DLL (V4.22 统一求解 + P02-002 新增 ipv_solve_from_memory_with_callback); 与 build/artifacts/ipv_solver.dll 完全一致 |
| lib/plate_solve/python/solve_and_write_wcs.py | (未修改) | (未修改) | 复用 init_environment/read_fits_header/parse_ra_hms/parse_dec_dms |
| lib/plate_solve/python/ipv_solver.py | (未修改) | (未修改) | IPVSolver ctypes 绑定 (复用, 已含 solve_from_memory_with_callback 方法) |

## 关键事实证据

### F-001: 路径 B 全量运行成功 (709/710)

- 命令: `python engineering\tools\batch_platesolve_test.py --mode path-b --manifest engineering\evidence\P02-001\testdata_manifest.json --output-dir engineering\evidence\P02-003\results --repeat-first 10`
- 时间: 2026-07-25 13:14:37 → 13:32:32 (wall 1075.0s = 17.9 min, env init 0.17s)
- 总运行次数: 730 (710 单次 + 20 重复)
- 成功: 709 (99.86%)
- 失败: 1 (frame 50, Galaxy_Center Oiii 600s 窄带, 与旧路径同一根因)
- 输出: 730 个 frame_*.json + path_b_results.json + full_run.log

### F-002: A/B 零退化

- 旧成功->新失败 (退化): 0 帧
- 旧失败->新成功 (改善): 0 帧
- 双方失败: 1 帧 (frame 50, 与旧路径同一根因)
- RMS 差异: min=-1.089e-12", median=0.000000", max=1.327e-13" (浮点噪声级别)
- n_pairs 差异: 全部为 0

### F-003: WCS 一致性

- WCS bit-wise 完全一致: 649/709 (91.5%)
- WCS 浮点噪声差异: 60/709 (8.5%), 量级 1e-12~1e-14 度 (≈1e-7~1e-9 角秒)
- 差异根因: FITS 文件 I/O (uint16→float32 在 C++ 内) vs 内存 buffer (numpy float32 转换) 的浮点累加顺序差异
- 物理影响: 无 (远低于星检测 RMS 0.285" 和 Gaia 星表精度)

### F-004: 重复性完美

- 前 10 帧 × 3 次运行: 10/10 帧 3/3 成功 (30/30)
- WCS 差异: max dRA=0°, max dDec=0° (完美确定性)
- RMS 差异: max rms_std=0.0000" (浮点噪声级别)
- 结论: 路径 B 输出 WCS 完美确定性可重现, 与旧路径一致

### F-005: 性能改善

- duration 中位: 1.326s (旧 1.302s, +1.8%, 在门限 ≤ 1.50s 内)
- duration p99: 6.867s (旧 9.682s, -29.1%, 显著改善)
- duration max: 21.78s (旧 30.55s, -28.8%)
- 改善来源: sdet_detect_ex 调用次数 2→1, 减少重复计算; p99/max 改善来自避免第二次检测的冷缓存开销

### F-006: ipv_solver.dll 一致性

- lib/plate_solve/cpp/ipv/ipv_solver.dll SHA-256: 804B2F2F54C665DCBC796A021E756DE62D37A4531B878483425ACB9A9E506547 (984362 字节)
- build/artifacts/ipv_solver.dll SHA-256: (与上面一致)
- 与 P02-001 旧路径 DLL (2BBC8EA0..., 886618 字节) 不同: 路径 B DLL 新增 `ipv_solve_from_memory_with_callback` 导出符号 (P02-002 已完成)
- 旧 API `ipv_solve_from_memory` 仍在 DLL 中保留, 向后兼容

### F-007: 失败帧 (frame 50, Oiii 窄带)

- 文件: Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii.fts
- 滤镜: Oiii, 曝光: 600S, 尺寸: 4500×3600
- 症状: success=false, duration=1.09s, n_pairs=0, error_msg="" (空)
- 根因: 窄带 OIII 600s 信噪比不足, star_detector 检测阶段无候选星
- 与旧路径对比: 旧路径同一帧同样失败 (frame 50), 同一根因
- 处置: 不修复 (star_detector 优化属独立 spec)

## 复核结论

- VERDICT: PASS (详见 REVIEW_REPORT.md)
- 任务目标"全量 A/B 对比与路径决策"达成, 10/10 非退化门限全部 PASS
- 路径 B 与旧路径在成功率、RMS、n_pairs 上完全一致, 零退化
- 重复性完美 (10/10 帧 3/3 成功, WCS 差异为 0)
- 路径决策: MERGE_PATH_B (UPSTREAM_SHARED_DETECTIONS), 合并到 main
