# PLATESOLVE_PATH_DECISION: P02-003 (PlateSolve 路径决策)

- Task ID: P02-003
- 决策时间: 2026-07-25
- 决策结论: **UPSTREAM_SHARED_DETECTIONS** (采用路径 B: callback 导出, 合并到 main)

## 决策依据

### A/B 全量对比 (710 帧)

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

### 关键发现

1. **零退化**: 旧成功->新失败 = 0 帧, 旧失败->新成功 = 0 帧, 双方失败 = 1 帧 (frame 50, Oiii 窄带, 与旧路径同一根因)
2. **RMS 完全一致**: 709 帧成功帧 RMS 差异 min/median/max 均为 0.000000" (算法输出 bit-wise 一致)
3. **WCS 一致性**: 649/709 帧 WCS bit-wise 完全一致; 60 帧存在浮点噪声级差异 (1e-12~1e-14 度, 远低于物理意义, 源于 FITS 文件 I/O vs 内存 buffer 读取路径的浮点累加顺序差异)
4. **重复性完美**: 前 10 帧 × 3 次运行, dRA=0°, dDec=0°, rms_std=0.0000" (确定性保持)
5. **性能**: duration 中位 1.326s (旧 1.302s, +1.8%), duration p99 6.867s (旧 9.682s, -29.1%, 显著改善)
6. **n_pairs 完全一致**: 709 帧 n_pairs 差异均为 0

## 路径 B 改动说明

### 核心改动: orchestrator.cpp (lib/orchestrator/cpp/src/orchestrator.cpp)

路径 B 将 PLATESOLVE 阶段的 star_det 块写入方式从"显式第二次调用 sdet_detect_ex"改为"callback 导出":

- **旧路径 (main)**: `ipv_solve_from_memory` (内部 sdet_detect_ex 1 次) + 显式 `sdet_detect_ex` (第 2 次) → star_det 块
- **路径 B**: `ipv_solve_from_memory_with_callback` (内部 sdet_detect_ex 1 次, callback 同步导出检测结果) → star_det 块

**sdet_detect_ex 调用次数: 2 → 1**, 减少重复计算和内存占用。

### 改动文件

| 文件 | 改动类型 | 说明 |
|---|---|---|
| lib/orchestrator/cpp/src/orchestrator.cpp | 修改 | 新增 PathBCallbackCtx + path_b_detection_callback; 替换 ipv_solve_from_memory → ipv_solve_from_memory_with_callback; 移除第二次 sdet_detect_ex, 改用 callback 结果写 star_det 块 |
| engineering/tools/batch_platesolve_test.py | 扩展 | 新增 --mode path-b; 新增 read_fits_pixels / solve_single_frame_path_b |
| engineering/tools/p02_003_ab_compare.py | 新增 | A/B 对比工具, 加载基线与路径B结果, 应用非退化门限, 输出决策 |

### ipv_solver.dll

- 旧路径基线 DLL: lib/plate_solve/cpp/ipv/ipv_solver.dll (886618 字节, SHA-256 2BBC8EA0...)
- 路径 B DLL: build/artifacts/ipv_solver.dll = lib/plate_solve/cpp/ipv/ipv_solver.dll (984362 字节, SHA-256 804B2F2F...)
- 差异: 路径 B DLL 新增 `ipv_solve_from_memory_with_callback` 导出符号 (P02-002 已完成)

## 兼容性

- `ipv_solve_from_memory_with_callback` 在 callback=NULL 时行为与 `ipv_solve_from_memory` 完全一致 (向后兼容)
- star_det 块格式不变 (FLOAT32 [N,4]: x, y, flux, mag), PSF 阶段消费方无需修改
- WCS 输出格式不变 (CD 矩阵 + CRVAL + CRPIX + SIP), 下游阶段无需修改

## 回滚方案

- 路径 B 代码在 `path-b-callback-export` 分支
- 若生产环境发现问题, 可 `git revert` 合并 commit 或 `git checkout main -- lib/orchestrator/cpp/src/orchestrator.cpp` 回退
- 旧路径 `ipv_solve_from_memory` API 仍在 DLL 中保留, 未被移除

## 残留风险

1. **60 帧 WCS 浮点噪声差异**: 源于 FITS 文件 I/O (uint16→float32 在 C++ 内完成) vs 内存 buffer (numpy float32 转换) 的浮点累加顺序差异。差异量级 1e-12~1e-14 度 (≈1e-7~1e-9 角秒), 远低于星检测 RMS (0.285") 和 Gaia 星表精度, 不影响下游测光/叠加质量。
2. **frame 50 (Oiii 窄带) 失败**: 与旧路径同一根因 (star_detector 窄带灵敏度不足), 非路径 B 引入的问题。
3. **duration 中位 +1.8%**: 路径 B 的 callback 复制检测结果 (2000 星 × 6 字段 × 8 字节 = 96KB/帧) 引入微小开销, 但被 p99 的 -29.1% 改善抵消。
