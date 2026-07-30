# G-002 TASK REPORT — 独立 SNR² 连续加权融合

- Gate: G
- 状态: **PASS**
- 依赖: G-001 (已完成)
- 实现文件: `lib/healpix_db/healpix_stack/python/g_chain/g002_fusion.py`
- 运行脚本: `lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py`
- 日期: 2026-07-30

## 1. 任务目标

对每个球面像素, 计算 SNR² 加权平均, 权重 = SNR² × support (只参与有覆盖的帧), 连续加权 (非硬阈值), 输出融合后的 signal 和 weight map。

## 2. 方法

### 2.1 融合输入
- G-001 输出的三片 CorrectedPanel (signal_corrected + snr + rejected 标记)
- 仅使用非排异像素 (rejected=False) 且 support=1 的像素

### 2.2 权重计算
- 权重: `w_i = SNR_i² × support_i`
- support=1 时有权重, 0 时无 (CorrectedPanel 已保证 support=1)
- SNR 下限: snr_floor=1.0 (避免 SNR=0 导致权重=0)
- 连续加权: 无硬阈值, 即使单帧也用其 SNR² 加权

### 2.3 按 ipix 聚合
- 收集所有非排异像素 (ipix, signal_corrected, snr, panel_id)
- 按 ipix 升序排序, 聚合相同 ipix 的多帧贡献
- 重叠区: 多帧加权平均; 非重叠区: 单帧直通

### 2.4 融合公式
- `signal_fused = Σ(w_i × signal_i) / Σ(w_i)`
- `weight_map = Σ(w_i)`
- `support_count = Σ(贡献帧数)` (1=单帧, 2=重叠)
- `snr_eff = sqrt(Σ(w_i × snr_i²) / Σ(w_i))` (加权合并, 保持 SNR 量纲)

## 3. 结果

| 指标 | 值 |
|------|-----|
| 融合后像素数 | 9706 |
| 重叠区像素数 | 1334 (13.74%) |
| 单帧区像素数 | 8372 (86.26%) |
| signal median | 27360.1 |
| signal range | [15845.8, 38894.3] |
| weight median | 1.12e+05 |
| weight range | [1.69e+03, 1.80e+06] |
| snr_eff median | 318.7 |
| snr_eff range | [41.1, 1340.5] |

### per-panel 贡献
| Panel | 贡献像素数 |
|-------|-----------|
| panel1 | 3651 |
| panel2 | 3679 |
| panel3 | 3710 |

### support_count 分布
| support_count | 像素数 |
|---------------|--------|
| 1 (单帧) | 8372 |
| 2 (重叠) | 1334 |

## 4. 禁止捷径合规

| 禁止捷径 | 状态 | 说明 |
|---------|------|------|
| 不得在梯度前做最终叠加 | PASS | 已由 G-001 校正加性偏移, 后融合 |
| 高 SNR 坏值不得免于排异 | PASS | G-001 已排异 751 像素, 此处只融合非排异像素 |
| 不得产生硬边 | PASS | 连续加权 (SNR²), 非硬阈值, 重叠区/非重叠区相对偏差=4.3% (F-002 验证) |
| 独立 SNR² 融合 | PASS | 权重 = SNR², 每像素独立计算 |

## 5. 证据文件

| 路径 | 说明 |
|------|------|
| `evidence/G-002/g002_result.json` | 结构化结果 |
| `evidence/G-002/TASK_REPORT.md` | 本报告 |
| `lib/healpix_db/healpix_stack/python/g_chain/logs/g_pipeline.log` | 运行日志 |

## 6. 运行命令

```
python lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py
```

- 耗时: 0.168s
- Python 3.10.11, numpy, astropy_healpix
