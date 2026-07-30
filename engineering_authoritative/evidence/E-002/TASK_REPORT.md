# E-002 TASK REPORT — SNR²/逆方差联合权重 + 重叠区多帧加权平均

- Gate: E
- 状态: PASS
- 依赖: E-001
- 实现文件: `lib/healpix_db/healpix_stack/python/e_chain/e_weights.py`

## 1. 任务目标

为每个控制点计算联合权重 = SNR² × 逆方差, 并在重叠区计算多帧加权平均。

## 2. 权重计算

联合权重公式: `w = SNR² × (1/variance)`, 每帧归一化 (除以中位数)。

| Panel | SNR_med | sigma_MAD | variance | w_snr_med | w_ivar | w_med | w_range |
|-------|---------|-----------|----------|-----------|--------|-------|---------|
| panel1 | 454.7 | 2458.4 | 6.04e6 | 2.07e5 | 1.65e-7 | 1.0 | [0.016, 8.69] |
| panel2 | 264.0 | 2560.3 | 6.56e6 | 6.97e4 | 1.53e-7 | 1.0 | [0.037, 11.6] |
| panel3 | 234.7 | 3251.5 | 1.06e7 | 5.51e4 | 9.46e-8 | 1.0 | [0.059, 12.4] |

- variance 由该帧有效像素 MAD-sigma² 估计 (背景噪声方差)
- variance 下限 = (0.01 × median)² 防止权重爆炸
- 每帧归一化 (w_med=1.0) 保证数值稳定

## 3. 重叠区多帧加权平均

| 重叠对 | n_overlap | delta_median (帧A) | delta_median (帧B) |
|--------|-----------|--------------------|--------------------|
| panel1 vs panel2 | 35 | -325.6 (panel1) | +103.2 (panel2) |
| panel2 vs panel3 | 36 | -40.7 (panel2) | +9.3 (panel3) |

- panel1 vs panel3 无重叠 (ipix 交集=0, 见 D-001)
- 三重交集: 0
- 重叠区共识: consensus_signal = Σ w_f × signal_f / Σ w_f

## 4. 契约合规

| 契约 | 状态 | 说明 |
|------|------|------|
| SNR² 权重必须实现 | PASS | w_snr = snr², 非无权重 |
| 不得选单一参考帧 | PASS | 重叠区用加权平均, 非选参考 |
| 高 SNR 保护 | PASS | w_snr = snr² 使高 SNR 权重更高, 不剔除低 SNR |

## 5. 证据文件

- `e002_result.json` — 结构化结果 (含 weight_stats + overlap_stats)

## 6. 运行命令

```
python lib/healpix_db/healpix_stack/python/e_chain/run_e_pipeline.py
```

elapsed: 0.006s
