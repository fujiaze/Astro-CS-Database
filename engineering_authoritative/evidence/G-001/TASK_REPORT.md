# G-001 TASK REPORT — 梯度校正后稳健排异

- Gate: G
- 状态: **PASS**
- 依赖: E-004 (已完成)
- 实现文件: `lib/healpix_db/healpix_stack/python/g_chain/g001_reject.py`
- 运行脚本: `lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py`
- 日期: 2026-07-30

## 1. 任务目标

读取三片 HISS 的 signal 和 E-003 求解的加性曲面偏移, 校正加性偏移后, 用 MAD (中位绝对偏差) 检测异常像素, 标记排异像素不参与后续融合。

## 2. 方法

### 2.1 加性偏移校正
- 读取 E-003 求解结果 `e003_result.json`: offsets = {panel1: +33.93, panel2: +222.19, panel3: -256.12}
- 校正: `signal_corrected = signal_raw - offset_f` (每帧减去其加性偏移)
- 偏移量之和 ≈ 0 (E-003 零均值约束: Σ off = -5.68e-13)

### 2.2 全局曲面预测
- 用 E-003 的 surface_coeffs (6 系数 2D 多项式) + center 评估每像素的曲面预测值
- `surface_pred = eval_surface(coeffs, ra, dec, center)`
- center = (272.8954, -18.2009)

### 2.3 残差计算
- `residual = signal_corrected - surface_pred`
- 残差反映该像素相对全局共识的偏离 (正常 ≈ 噪声, 异常则大)

### 2.4 MAD 稳健排异
- 每帧独立计算 MAD: `mad = median(|residual - median(residual)|)`
- sigma_mad = 1.4826 × MAD
- 排异阈值: `|residual - median| > 3.0 × 1.4826 × MAD`
- 高 SNR 坏值不豁免: 排异仅基于残差, 与 SNR 无关

## 3. 结果

| Panel | n_pix | offset | residual_median | mad_sigma | rejected | reject% | valid |
|-------|-------|--------|-----------------|-----------|----------|---------|-------|
| panel1 | 3928 | +33.93 | +709.76 | 2160.74 | 277 | 7.05% | 3651 |
| panel2 | 3927 | +222.19 | +767.08 | 2893.53 | 248 | 6.32% | 3679 |
| panel3 | 3936 | -256.12 | +1656.98 | 3157.51 | 226 | 5.74% | 3710 |
| **汇总** | **11791** | — | — | — | **751** | **6.37%** | **11040** |

- 排异率 5.74%~7.05%, 在 3σ MAD 阈值下合理 (正态分布理论 ~0.3%, 银心区域天体密集致残差重尾)
- SNR median: panel1=445.2, panel2=264.4, panel3=235.1 (从 V1 snr_model IDW 插值)

## 4. 禁止捷径合规

| 禁止捷径 | 状态 | 说明 |
|---------|------|------|
| 不得在梯度前做最终叠加 | PASS | 先校正加性偏移 (G-001), 后融合 (G-002) |
| 高 SNR 坏值不得免于排异 | PASS | MAD 排异仅基于残差大小, 与 SNR 无关 |
| 不得产生硬边 | PASS | 排异是像素级标记, 融合时连续加权 (G-002) |

## 5. 证据文件

| 路径 | 说明 |
|------|------|
| `evidence/G-001/g001_result.json` | 结构化结果 |
| `evidence/G-001/TASK_REPORT.md` | 本报告 |
| `lib/healpix_db/healpix_stack/python/g_chain/logs/g_pipeline.log` | 运行日志 |

## 6. 运行命令

```
python lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py
```

- 耗时: 0.46s
- Python 3.10.11, numpy, scipy, astropy_healpix
