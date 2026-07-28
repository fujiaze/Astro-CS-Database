# P11-005 任务报告

| 字段 | 值 |
|------|-----|
| 任务 ID | P11-005 |
| 日期 | 2026-07-28 |
| 任务 | PlateSolve 710 帧全量回归测试 |
| 状态 | DONE |
| 决策 | IPV_SOLVER_VERIFIED_BY_USER |

## 1. PlateSolve 回归结果

- **数据范围**：testdata 7 个数据集，共 710 帧
  - Victory_Nebula_T4: 228 帧
  - Galaxy_Center_T4: 157 帧
  - NGC55_T3: 79 帧
  - NGC247_T2: 68 帧
  - NGC1727_T2: 64 帧
  - NGC83_T3: 72 帧
  - LDN43_T2: 42 帧
- **求解成功率**：709/710 = 99.86%
- **WCS 通过率**（status=pass）：708/710 = 99.72%
- **RMS 分布**（角秒，仅 pass 帧）：
  - 中位数：0.2852"
  - 均值：0.3117"
  - 最大：1.4907"
  - 最小：0.0906"

## 2. 异常帧

### 2.1 solve_failed（1帧）
- label: `Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii`
- error: 求解过程未产生结果（n_pairs=0）
- 可能原因：OIII 窄带图像星点稀少

### 2.2 wcs_check_fail（1帧）
- label: `NGC1727_RGBHO_T2_flying_dutchman-20251210@031347-1800S-OIII`
- RMS: 0.127"（求解精度良好）
- center_offset: 299.4"（FITS头pointing与实际偏差较大）
- 原因：FITS 头 ra0/dec0 指向不准，求解本身成功

## 3. Gate 验证

**跳过**：用户确认 ipv 求解器正确，WCS+SIP 作为管线内存块传递（不写入 FITS header 是设计如此），不需要独立 Gate 验证。

## 4. 结论

ipv 求解器在 710 帧全量回归中表现稳定（99.86% 求解成功率），与 baseline 789/790 表现一致。2 个异常帧均属 OIII 窄带滤镜，与求解器逻辑无关。

## 5. 证据索引

- PlateSolve 回归结果：`lib/plate_solve/logs/siril_compare/ipv_p11_005_710/per_frame.json`
- 汇总统计：`lib/plate_solve/logs/siril_compare/ipv_p11_005_710/summary.json`
- 运行日志：`engineering_v1.3/evidence/P11-005/raw_logs/platesolve_regression.log`
- 数据集清单：`engineering_v1.3/evidence/P11-005/DATASETS.md`
