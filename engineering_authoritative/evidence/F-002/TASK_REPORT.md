# F-002 TASK REPORT — 三片最小总曲面、HCSD 和浏览器检查

- Gate: F
- 状态: **PASS**
- 依赖: E-004 (已完成) + F-001 (已完成) + G-003 (已完成)
- 实现文件: `lib/healpix_db/healpix_stack/python/g_chain/f002_verify.py`
- 日期: 2026-07-30

## 1. 任务目标

用 G-003 生成的 HCSD, 浏览器检查接缝 (三片拼接处), 验证总曲面可求解, 验证三片位置正确。

## 2. 方法

### 2.1 验证 1: HCSD 可读性
- 读回 `T4_RED_GalaxyCenter_fused.hcsd`, 验证 ipix 升序、唯一性、子叶索引、signal 范围
- 检查 meta: filter=Red, n_frames=3

### 2.2 验证 2: 三片位置正确
- 读取三片 HISS, 检查 RA/Dec 范围与 D-001 期望一致
- 检查重叠区连通性: panel1∩panel2, panel2∩panel3 像素数 > 0

### 2.3 验证 3: 接缝连续性
- 读回 HCSD + 调试层 (support_count)
- 比较重叠区 vs 非重叠区 signal median, 相对偏差 < 20% 视为无硬边
- 相邻像素差分分布 (median, p95, max)

### 2.4 验证 4: 总曲面可求解
- 用融合后 signal 重新拟合 2D 多项式曲面 (6 系数, 最小二乘)
- 检查 rank=6 (满秩), 残差有限, 相对残差 < 30%

### 2.5 浏览器可视化
- 生成接缝检查图 (matplotlib): 融合 signal 天球图 + 重叠区标记 + 三片贡献图 + 接缝 signal 分布直方图
- 输出 `seam_check.png` 供浏览器查看

## 3. 结果

### 3.1 验证 1: HCSD 可读性 — PASS
| 检查项 | 结果 |
|--------|------|
| ipix_sorted | PASS |
| ipix_unique | PASS |
| n_pix_positive | PASS |
| filter_correct (Red) | PASS |
| n_frames_correct (3) | PASS |
| nside | 512 |
| n_pix | 9706 |
| n_leaves_with_data | 185 |

### 3.2 验证 2: 三片位置正确 — PASS
| Panel | RA 范围 | Dec 范围 | ra_match | dec_match |
|-------|---------|----------|----------|-----------|
| panel1 | [268.68, 276.94] | [-16.33, -9.90] | PASS | PASS |
| panel2 | [268.59, 277.12] | [-21.46, -15.02] | PASS | PASS |
| panel3 | [268.51, 277.29] | [-26.44, -20.03] | PASS | PASS |

| 重叠区 | 像素数 | 连通 |
|--------|--------|------|
| panel1∩panel2 | 792 | PASS |
| panel2∩panel3 | 879 | PASS |

### 3.3 验证 3: 接缝连续性 — PASS
| 指标 | 值 |
|------|-----|
| 重叠区 signal median | 28338.1 (n=1334) |
| 非重叠区 signal median | 27118.5 (n=8372) |
| 相对偏差 | 0.0430 (< 0.20, 无硬边) |
| 相邻差分 median | 851.0 |
| 相邻差分 p95 | 4080.6 |
| 相邻差分 max | 15849.3 |

### 3.4 验证 4: 总曲面可求解 — PASS
| 指标 | 值 |
|------|-----|
| n_pix | 9706 |
| rank | 6 (满秩) |
| 残差 RMS | 2188.4 |
| 残差 MAD-sigma | 1836.5 |
| signal_range | 23048.5 |
| 相对 RMS | 0.0949 (< 0.30) |
| 收敛 | PASS |
| 可求解 | PASS |

### 3.5 浏览器可视化
- 接缝检查图: `engineering_authoritative/evidence/F-002/seam_check.png`
- 含 4 子图: 融合 signal 天球图 / 重叠区标记 / 三片贡献图 / 接缝 signal 分布

## 4. 禁止捷径合规

| 禁止捷径 | 状态 | 说明 |
|---------|------|------|
| 不得使用不同滤镜 | PASS | 三片均为 Red (meta.filter=Red 验证) |
| 不得只用 panel1 | PASS | 三片全部参与 (n_frames=3, per-panel 贡献验证) |
| 不得使用重复 HISS | PASS | 三片独立 HISS 文件 (panel1/2/3, RA/Dec 范围不同) |

## 5. 证据文件

| 路径 | 说明 |
|------|------|
| `evidence/F-002/f002_result.json` | 结构化验证结果 |
| `evidence/F-002/TASK_REPORT.md` | 本报告 |
| `evidence/F-002/seam_check.png` | 接缝检查图 (浏览器可视化) |
| `lib/healpix_db/healpix_stack/python/g_chain/logs/f002.log` | 运行日志 |

## 6. 运行命令

```
python lib/healpix_db/healpix_stack/python/g_chain/f002_verify.py
```

- 耗时: ~1.5s (含 HISS 加载 + 4 项验证 + 可视化)
- Python 3.10.11, numpy, scipy, astropy_healpix, matplotlib
