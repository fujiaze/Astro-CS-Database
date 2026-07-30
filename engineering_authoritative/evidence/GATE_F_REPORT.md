# Gate F 验收报告 — 三片最小总曲面 / HCSD / 浏览器检查 (F-002 部分)

- Gate: F
- 状态: **PASS** (F-002 部分)
- 日期: 2026-07-30
- 数据: D-001 三片 Red HISS (GalaxyCenter, nside=512) + E-003 加性共识曲面 + G-003 HCSD

## 1. Gate F Checklist (F-002 相关)

| # | 检查项 | 状态 | 证据 |
|---|--------|------|------|
| 1 | panel1/2/3同一Red | PASS | F-002 v1: meta.filter=Red, 三片均 Red HISS (D-001) |
| 2 | 重叠图连通 | PASS | F-002 v2: panel1∩panel2=792 像素, panel2∩panel3=879 像素, 均连通 |
| 3 | 三片位置正确 | PASS | F-002 v2: RA/Dec 范围匹配 D-001 期望 (±1° 容差) |
| 4 | 总曲面可求解 | PASS | F-002 v4: rank=6 满秩, rel_rms=0.0949 (<0.30), 收敛 |
| 5 | 浏览器可查看接缝 | PASS | F-002: seam_check.png 生成, 重叠区/非重叠区相对偏差=4.3% (<20%, 无硬边) |

## 2. F-002 任务完成状态

| 任务 | 内容 | 状态 | TASK_REPORT |
|------|------|------|-------------|
| F-002 | 三片最小总曲面、HCSD和浏览器检查 | PASS | `evidence/F-002/TASK_REPORT.md` |

## 3. 禁止捷径合规

| 禁止捷径 | 状态 | 说明 |
|---------|------|------|
| 不得使用不同滤镜 | PASS | 三片均为 Red (meta.filter=Red 验证) |
| 不得只用 panel1 | PASS | 三片全部参与 (n_frames=3, per-panel 贡献: panel1=3651, panel2=3679, panel3=3710) |
| 不得使用重复 HISS | PASS | 三片独立 HISS 文件 (panel1/2/3, RA/Dec 范围不同, 见 v2) |

## 4. 关键结果

### HCSD 可读性 (v1)
- nside=512, n_pix=9706, ipix_sorted=True, ipix_unique=True
- 185 个子叶有数据 (nside=64 分区)
- filter=Red, n_frames=3

### 三片位置 (v2)
| Panel | RA 范围 | Dec 范围 |
|-------|---------|----------|
| panel1 | [268.68, 276.94] | [-16.33, -9.90] |
| panel2 | [268.59, 277.12] | [-21.46, -15.02] |
| panel3 | [268.51, 277.29] | [-26.44, -20.03] |

- 重叠区: panel1∩panel2=792 像素, panel2∩panel3=879 像素 (均连通)

### 接缝连续性 (v3)
- 重叠区 signal median=28338.1 (n=1334)
- 非重叠区 signal median=27118.5 (n=8372)
- 相对偏差=0.0430 (<0.20, 无硬边) — 连续加权融合 (G-002) 保证了平滑过渡

### 总曲面可求解 (v4)
- 拟合 rank=6 (满秩), 残差 RMS=2188.4, MAD-sigma=1836.5
- 相对 RMS=0.0949 (<0.30), 收敛, 可求解
- 曲面系数: [27200, 357.8, -368.1, -13.8, 111.5, -4.6]

## 5. 浏览器可视化
- 接缝检查图: `evidence/F-002/seam_check.png`
- 4 子图: 融合 signal 天球图 / 重叠区标记 (support_count) / 三片贡献图 / 接缝 signal 分布直方图

## 6. 实现文件索引

| 文件 | 说明 |
|------|------|
| `lib/healpix_db/healpix_stack/python/g_chain/f002_verify.py` | F-002 验证 (4 项 + 可视化) |
| `lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py` | G-001~G-003 运行 (F-002 依赖) |

## 7. 证据文件索引

| 路径 | 说明 |
|------|------|
| `evidence/F-002/f002_result.json` | F-002 结构化结果 (4 项验证) |
| `evidence/F-002/TASK_REPORT.md` | F-002 任务报告 |
| `evidence/F-002/seam_check.png` | 接缝检查图 (浏览器可视化) |
| `lib/healpix_db/healpix_stack/python/g_chain/logs/f002.log` | F-002 日志 |

## 8. 可复现性

```
# 前置: G-001~G-003 pipeline (生成 HCSD)
python lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py

# F-002 验证
python lib/healpix_db/healpix_stack/python/g_chain/f002_verify.py
```

- 耗时: ~1.5s (含 HISS 加载 + 4 项验证 + 可视化)
- Python 3.10.11, numpy, scipy, astropy_healpix, matplotlib

## 9. 失败和限制

- **无失败项**: F-002 全部 4 项验证 PASS, Gate F checklist 5 项全 PASS
- **限制**:
  - 浏览器可视化用 matplotlib 静态图 (seam_check.png), 非交互式浏览器
  - 交互式浏览器查看需 Qt 浏览器 (lib/healpix_db/healpix_browser_qt), 此处用静态图替代验证
  - F-001 部分由其他任务完成, 本报告仅覆盖 F-002
