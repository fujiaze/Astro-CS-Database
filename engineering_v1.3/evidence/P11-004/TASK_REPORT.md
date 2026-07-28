# P11-004 任务报告

| 字段 | 值 |
|------|-----|
| 任务 ID | P11-004 |
| 生成日期 | 2026-07-28 |
| 工具版本 | P11-004 v3.5 (authoritative_pairs + repair) |
| 代表帧总数 | 16 |
| 初始通过 / 失败 | 10 / 6 (gate_pass_rate = 62.5%) |
| 修复后通过 / 失败 | 16 / 0 (gate_pass_rate = 100%) |
| 最终 VERDICT | **PASS**（WCS_PRODUCTION_FIX_REQUIRED，HEADER_REGENERATION_NO_CODE_CHANGE） |

## 1. 任务目标

按 AUTONOMOUS_ENTRY.md §2 强制裁决，在 16 帧代表帧上实施"双层闭环"方案：

1. **A 层（Solver Internal）**：固定求解器权威 inlier 对应关系（来自 `IPVSolver.get_last_inliers()`），验证求解器内部 TRANS 预测与 detector 坐标一致性
2. **B 层（Serialized WCS Hard Gate）**：用最终序列化到 FITS Header 的标准 WCS+SIP 独立把 Gaia 星回投到像素，与 detector 坐标比较
3. **C 层（Blind Catalog）**：全星表 kd-tree 重新匹配，仅作二级诊断，不再作为硬 Gate

绕过 P11-002/P11-003 中 kd-tree 重匹配导致的"残差与 IPV 内部 RMS 不可比"问题，并完成 P11-004 生产端裁决。

## 2. 输入

| 输入项 | 说明 |
|--------|------|
| 16 帧 FITS 图像 | T2×5 (LDN43×4 + NGC1727×1), T3×6 (NGC55×6), T4×5 (Galaxy_Center×5) |
| 设备档案 | `evidence/P11-003/REPRESENTATIVE_FRAMES_ARCHIVE.json` |
| 星表 | Gaia DR3（在线查询，mag_high 自适应） |
| PlateSolve 环境 | `lib/plate_solve/`（C++ + Python） |
| 决策依据 | `docs/23_P11_004_REVIEW_DECISION.md` + `docs/26_P11_RECOVERY_RUNBOOK.md` |
| 契约 | `contracts/wcs_authoritative_pairs.schema.json` v1.0 |

## 3. 执行流程

```
1. 升级诊断工具至 v3.0：
   - 在 P11-002 v2.0 基础上新增 `--authoritative-pairs` 模式
   - 调用 solver.get_last_inliers() 拿 C++ SolveInlierCache 数据
   - 禁止启用 C 层 blind kd-tree 重匹配

2. 在 16 帧上运行 Gate v2 (batch_frames.json):
   - 每帧调用 PlateSolve 求解
   - 提取权威 inlier 对（A 层）
   - 用 astropy WCS(header).world_to_pixel 回投 Gaia 星（B 层）
   - 计算外部残差 = detector_xy_astropy - external_pred_xy
   - 应用 Siril 风格迭代剔除（35 分位 sigma, 10σ 软剔除）
   - 输出 closure_report.json + matched_pairs_authoritative.jsonl

3. 批量结果 (gate_v2_final/batch_summary.json):
   - 10/16 通过，6/16 失败
   - 失败原因分析：所有失败帧 has_sip=false, sip_order=0
   - 进一步：CRPIX=(2048.0, 2048.0) 而非 (2048.5, 2048.5)

4. 根因定位：
   - compare_wcs_construction.py: to_astropy_wcs(result) vs WCS(header) 等价（8/8 帧）
   - 6 失败帧的 FITS header 是历史遗留，未序列化 SIP
   - 当前 C++ 代码 (ipv_wcs.cpp:287-288,348) 已正确实现 SIP 输出

5. 最小修复 (repair_failed_frames.py v3.5):
   - 用当前代码对 6 失败帧重新求解
   - solve_and_write_wcs(overwrite=True) 写入新 header
   - 备份原 header 至 backups/

6. 修复后验证 (gate_v2_post_repair/batch_summary.json):
   - 6/6 通过，has_sip=true, sip_order=3
   - p68 中位数 0.13–0.23 px

7. 生成决策文档 (P11_004_DECISION.md):
   - 结论: WCS_PRODUCTION_FIX_REQUIRED
   - 修复性质: HEADER_REGENERATION_NO_CODE_CHANGE
```

## 4. 16 帧结果表（修复前 → 修复后）

| # | frame_id | 设备 | 滤镜 | has_sip | A 层 p68 (px) | B 层 p68 (px) | B 层 p99 (px) | gate | 备注 |
|---|----------|------|------|---------|---------------|---------------|---------------|------|------|
| 1 | T4_RED_Galaxy_Center | T4 | RED | true | 0.059 | 0.076 | 0.107 | ✅ | - |
| 2 | T4_GREEN_Galaxy_Center | T4 | GREEN | true | 0.059 | 0.056 | 0.105 | ✅ | - |
| 3 | T4_BLUE_Galaxy_Center | T4 | BLUE | true | 0.088 | 0.076 | 0.206 | ✅ | - |
| 4 | T4_HA_Galaxy_Center | T4 | HA | true | 0.070 | 0.075 | 0.127 | ✅ | - |
| 5 | T4_OIII_Galaxy_Center | T4 | OIII | true | 0.080 | 0.090 | 0.140 | ✅ | - |
| 6 | T2_RED_LDN43 | T2 | RED | false→true | 0.085 | 6.871→0.160 | 16.70→0.336 | ❌→✅ | 已修复 |
| 7 | T2_GREEN_LDN43 | T2 | GREEN | false→true | 0.108 | 7.265→0.149 | 16.62→0.248 | ❌→✅ | 已修复 |
| 8 | T2_BLUE_LDN43 | T2 | BLUE | false→true | 0.237 | 6.869→0.228 | 16.18→0.828 | ❌→✅ | 已修复 |
| 9 | T2_HA_LDN43 | T2 | HA | false→true | 0.083 | 7.076→0.133 | 16.64→0.290 | ❌→✅ | 已修复 |
| 10 | T2_OIII_NGC1727 | T2 | OIII | false→true | 0.132 | 5.959→0.141 | 17.95→0.292 | ❌→✅ | 已修复 |
| 11 | T3_RED_NGC55 | T3 | RED | true | 0.149 | 0.140 | 0.295 | ✅ | - |
| 12 | T3_GREEN_NGC55 | T3 | GREEN | true | 0.114 | 0.127 | 0.221 | ✅ | - |
| 13 | T3_BLUE_NGC55 | T3 | BLUE | true | 0.139 | 0.161 | 0.265 | ✅ | - |
| 14 | T3_HA_NGC55 | T3 | HA | true | 0.150 | 0.168 | 0.273 | ✅ | - |
| 15 | T3_OIII_NGC55 | T3 | OIII | true | 0.186 | 0.220 | 0.476 | ✅ | - |
| 16 | T3_LUM_NGC55 | T3 | LUM | false→true | 0.133 | 7.041→0.138 | 12.31→0.392 | ❌→✅ | 已修复 |

## 5. 修复前后对比（6 失败帧）

| frame_id | 修复前 B 层 p68 (px) | 修复后 B 层 p68 (px) | 改善倍数 |
|----------|---------------------|---------------------|----------|
| T2_RED_LDN43 | 6.871 | 0.160 | 42.9× |
| T2_GREEN_LDN43 | 7.265 | 0.149 | 48.8× |
| T2_BLUE_LDN43 | 6.869 | 0.228 | 30.1× |
| T2_HA_LDN43 | 7.076 | 0.133 | 53.2× |
| T2_OIII_NGC1727 | 5.959 | 0.141 | 42.3× |
| T3_LUM_NGC55 | 7.041 | 0.138 | 51.0× |
| **均值** | **6.847** | **0.158** | **44.7×** |

## 6. 耗时统计

### 6.1 初始 16 帧 Gate v2 验证

| 阶段 | 总耗时 (s) | 每帧均值 (s) |
|------|-----------|--------------|
| 求解 + 诊断 | ~30 | 1.9 |

### 6.2 修复 6 失败帧

| frame_id | 修复耗时 (s) | 验证耗时 (s) | 合计 (s) |
|----------|-------------|--------------|----------|
| T2_RED_LDN43 | 1.142 | 1.532 | 2.674 |
| T2_GREEN_LDN43 | 1.058 | 1.118 | 2.176 |
| T2_BLUE_LDN43 | 1.108 | 1.005 | 2.113 |
| T2_HA_LDN43 | 0.924 | 0.817 | 1.741 |
| T2_OIII_NGC1727 | 17.452 | 15.576 | 33.028 |
| T3_LUM_NGC55 | 1.137 | 0.762 | 1.899 |

## 7. 修复前后 FITS Header 变化

| 字段 | 修复前 | 修复后 |
|------|--------|--------|
| CRPIX1 | 2048.0 | 2048.5 |
| CRPIX2 | 2048.0 | 2048.5 |
| A_ORDER | (缺失) | 3 |
| B_ORDER | (缺失) | 3 |
| AP_ORDER | (缺失) | 3 |
| BP_ORDER | (缺失) | 3 |
| A_i_j / B_i_j | (缺失) | 已写入 |
| AP_i_j / BP_i_j | (缺失) | 已写入 |
| CTYPE1 | RA---TAN | RA---TAN-SIP |
| CTYPE2 | DEC--TAN | DEC--TAN-SIP |

## 8. 关键产物

| 产物 | 路径 | 说明 |
|------|------|------|
| 决策文档 | `evidence/P11-004/P11_004_DECISION.md` | 最终裁决 WCS_PRODUCTION_FIX_REQUIRED |
| 初始批量结果 | `reports/gate_v2_final/batch_summary.json` | 16 帧，10 PASS / 6 FAIL |
| 修复后批量结果 | `reports/gate_v2_post_repair/batch_summary.json` | 6 帧，6 PASS / 0 FAIL |
| 修复汇总 | `reports/gate_v2_final/repair_summary.json` | 6 帧 header 重新生成记录 |
| 修复脚本 | `scripts/repair_failed_frames.py` | v3.5 |
| 诊断工具 | `scripts/wcs_closure_diagnostic_v3.py` | v3.4 (CRPIX 1-based 修复) |
| 单帧证据 | `reports/gate_v2_post_repair/<frame_id>/` | closure_report.json + matched_pairs_authoritative.jsonl |

## 9. 结论

P11-004 以 **WCS_PRODUCTION_FIX_REQUIRED** 完成：
- 6 帧失败根因 = 历史 FITS header 未序列化 SIP（C++ 代码当前版本已正确）
- 修复方式 = 重新求解 6 帧，写入正确 header（HEADER_REGENERATION_NO_CODE_CHANGE）
- 修复后 16/16 帧通过双层闭环硬 Gate
- 未修改任何 C++/Python 生产代码

可进入 P11-005：PlateSolve 710 全量回归测试。
