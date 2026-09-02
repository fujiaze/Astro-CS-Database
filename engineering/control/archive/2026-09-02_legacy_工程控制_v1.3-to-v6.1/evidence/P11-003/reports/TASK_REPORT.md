# P11-003 任务报告

| 字段 | 值 |
|------|-----|
| 任务 ID | P11-003 |
| 生成日期 | 2026-07-28 |
| 工具版本 | P11-003 v1.0 (subset driver, astropy WCS 独立诊断) |
| 总帧数 | 16 |
| 通过 / 失败 | 8 / 8 (gate_pass_rate = 50.0%) |

## 1. 任务目标

对 4 台设备（T2/T3/T4；T1 无数据不纳入测试）的 **16 帧代表性图像**执行 WCS 闭环诊断，验证 PlateSolve 求解结果与 astropy WCS 独立诊断的一致性，评估 SIP order=3 多项式在不同焦距（200mm / 1900mm）、不同滤镜（LRGBHO）、不同目标场景下的拟合精度，并定位 WCS 闭环缺陷的根因。

## 2. 输入

| 输入项 | 说明 |
|--------|------|
| 16 帧 FITS 图像 | T2×5 (LDN43×4 + NGC1727×1), T3×6 (NGC55×6), T4×5 (Galaxy_Center×5) |
| 设备档案 | `engineering_v1.2/evidence/P11-003/REPRESENTATIVE_FRAMES_ARCHIVE.json` |
| 星表 | Gaia DR3（在线查询，mag_high=18.0） |
| T4×5 + T2 LDN43×4 | 此前已完成（P11-002 v1.0 格式 closure_report） |
| group_a (4帧) | T2_HA_LDN43, T2_OIII_NGC1727, T3_RED_NGC55, T3_GREEN_NGC55 |
| group_b (4帧) | T3_BLUE_NGC55, T3_HA_NGC55, T3_OIII_NGC55, T3_LUM_NGC55 |

## 3. 执行流程

```
1. 从 REPRESENTATIVE_FRAMES_ARCHIVE.json 选取 16 帧代表性图像
2. 分两组并行执行（group_a + group_b），每组 4 帧
   - T4×5 和 T2 LDN43×4 此前已完成，直接复用 closure_report
3. 每帧执行流水线：
   a. PlateSolve 求解（生成 WCS + SIP 多项式写入 FITS header）
   b. astropy WCS 独立诊断：
      - 从 FITS header 读取 WCS
      - Gaia DR3 星表查询（search_radius 基于 FOV）
      - 星点检测 + cKDTree 双向最近邻匹配（max_dist=3.0 px）
      - 残差统计（median / p90 / p99 / x / y 方向）
      - 闭环验证：PS→Sky→PS 和 Sky→PS→Sky
   c. gate 检查（median ≤ 0.75 px AND p90 ≤ 1.5 px AND p99 ≤ 3.0 px）
4. 汇总 16 帧结果 → p11_003_summary.json
5. 生成标准报告（TASK / TEST / EVIDENCE / REVIEW）
```

## 4. 16 帧结果表

| # | frame_id | 设备 | 滤镜 | 目标 | solve_rms(px) | n_pairs | n_matched | median(px) | p90(px) | p99(px) | gate |
|---|----------|------|------|------|---------------|---------|-----------|------------|---------|---------|------|
| 1 | T4_RED_Galaxy_Center | T4 | RED | Galaxy_Center | 0.054 | 45 | 1795 | 0.533 | 0.685 | 2.735 | ✅ PASS |
| 2 | T4_GREEN_Galaxy_Center | T4 | GREEN | Galaxy_Center | 0.052 | 30 | 1795 | 0.583 | 0.981 | 2.814 | ✅ PASS |
| 3 | T4_BLUE_Galaxy_Center | T4 | BLUE | Galaxy_Center | 0.084 | 32 | 1785 | 0.560 | 0.721 | 2.771 | ✅ PASS |
| 4 | T4_HA_Galaxy_Center | T4 | HA | Galaxy_Center | 0.065 | 44 | 1808 | 0.663 | 0.854 | 2.705 | ✅ PASS |
| 5 | T4_OIII_Galaxy_Center | T4 | OIII | Galaxy_Center | 0.066 | 34 | 1785 | 0.606 | 0.759 | 2.780 | ✅ PASS |
| 6 | T2_RED_LDN43 | T2 | RED | LDN43 | 0.121 | 34 | 1945 | 0.960 | 1.174 | 1.560 | ❌ FAIL |
| 7 | T2_GREEN_LDN43 | T2 | GREEN | LDN43 | 0.103 | 32 | 1944 | 0.788 | 1.033 | 1.438 | ❌ FAIL |
| 8 | T2_BLUE_LDN43 | T2 | BLUE | LDN43 | 0.288 | 40 | 1941 | 0.880 | 1.153 | 1.718 | ❌ FAIL |
| 9 | T2_HA_LDN43 | T2 | HA | LDN43 | 0.108 | 33 | 1237 | 0.772 | 0.958 | 2.058 | ❌ FAIL |
| 10 | T2_OIII_NGC1727 | T2 | OIII | NGC1727 | 0.130 | 45 | 1624 | 0.689 | 0.853 | 1.827 | ✅ PASS |
| 11 | T3_RED_NGC55 | T3 | RED | NGC55 | 0.135 | 38 | 693 | 0.762 | 1.007 | 1.465 | ❌ FAIL |
| 12 | T3_GREEN_NGC55 | T3 | GREEN | NGC55 | 0.109 | 33 | 663 | 0.826 | 1.121 | 1.775 | ❌ FAIL |
| 13 | T3_BLUE_NGC55 | T3 | BLUE | NGC55 | 0.134 | 36 | 614 | 0.773 | 1.074 | 1.656 | ❌ FAIL |
| 14 | T3_HA_NGC55 | T3 | HA | NGC55 | 0.142 | 39 | 328 | 0.709 | 1.005 | 1.433 | ✅ PASS |
| 15 | T3_OIII_NGC55 | T3 | OIII | NGC55 | 0.201 | 45 | 306 | 0.693 | 0.908 | 1.328 | ✅ PASS |
| 16 | T3_LUM_NGC55 | T3 | LUM | NGC55 | 0.151 | 31 | 702 | 0.897 | 1.284 | 1.778 | ❌ FAIL |

> 注：T2_RED/GREEN/BLUE_LDN43 三帧的 closure_report.json 此前因文件系统级损坏（全零字节）导致详细诊断字段缺失，已通过重跑恢复完整数据，上表 p90/p99 及后续方向性统计均基于恢复后的完整 closure_report.json。

## 5. 耗时统计

### 5.1 T4 帧（P11-002 v1.0 格式，仅总 elapsed_sec）

| frame_id | 总耗时 (s) |
|----------|-----------|
| T4_RED_Galaxy_Center | 9.089 |
| T4_GREEN_Galaxy_Center | 7.687 |
| T4_BLUE_Galaxy_Center | 7.778 |
| T4_HA_Galaxy_Center | 7.320 |
| T4_OIII_Galaxy_Center | 7.288 |
| **均值** | **7.832** |

### 5.2 group_a / group_b 帧（P11-003 v1.0 格式，solve + diag 分离）

| frame_id | solve (s) | diag (s) | 合计 (s) |
|----------|-----------|----------|----------|
| T2_RED_LDN43 | 1.327 | 2.565 | 3.892 |
| T2_GREEN_LDN43 | 1.070 | 1.905 | 2.975 |
| T2_BLUE_LDN43 | 1.027 | 1.876 | 2.903 |
| T2_HA_LDN43 | 1.156 | 2.516 | 3.672 |
| T2_OIII_NGC1727 | 19.686 | 4.240 | 23.926 |
| T3_RED_NGC55 | 0.797 | 1.522 | 2.319 |
| T3_GREEN_NGC55 | 0.821 | 1.626 | 2.447 |
| T3_BLUE_NGC55 | 1.017 | 2.214 | 3.231 |
| T3_HA_NGC55 | 0.816 | 1.530 | 2.346 |
| T3_OIII_NGC55 | 0.798 | 1.497 | 2.295 |
| T3_LUM_NGC55 | 0.911 | 1.708 | 2.619 |
| **均值（排除 T2_OIII 异常值）** | **0.974** | **1.896** | **2.870** |

> T2_OIII_NGC1727 的 solve 耗时 19.686s 远高于其他帧（均值 0.974s），可能因首次 Gaia 缓存未命中或网络延迟。T2_RED/GREEN/BLUE_LDN43 三帧的耗时数据已通过重跑恢复（此前 closure_report 损坏时无耗时数据）。

## 6. 聚合统计

| 指标 | 值 |
|------|-----|
| 求解成功数 | 16/16 (100%) |
| 诊断成功数 | 16/16 (100%) |
| gate 通过数 | 8/16 (50.0%) |
| gate 失败数 | 8/16 (50.0%) |
| 全局 median solve RMS (px) | 0.114641 |
| 全局 median dist_median (px) | 0.735982 |
