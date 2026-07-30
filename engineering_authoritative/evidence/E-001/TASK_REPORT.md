# E-001 TASK REPORT — 星点/饱和/异常掩膜 + 稀疏控制点采样

- Gate: E
- 状态: PASS
- 依赖: D-001 (三片 Red HISS)
- 实现文件: `lib/healpix_db/healpix_stack/python/e_chain/e_masks_sampling.py`
- 公共约定: `lib/healpix_db/healpix_stack/python/e_chain/e_common.py`

## 1. 任务目标

读取三片 V1 HISS (D-001 银心 Red), 计算:
- 星点掩膜 (median + 5×MAD-sigma)
- 饱和掩膜 (p99.9)
- 异常掩膜 (NaN/Inf/≤0)
- 稀疏控制点采样 (分层随机, 按 dec 分箱, 每片 800 点)
- SNR IDW 插值 (power=2.0, k=8)

## 2. 输入

| Panel | nside | n_pix | signal_median | HISS 文件 |
|-------|-------|-------|---------------|-----------|
| panel1 | 512 | 3928 | 25272.5 | T4_RED_GalaxyCenter_panel1.hiss |
| panel2 | 512 | 3927 | 27836.5 | T4_RED_GalaxyCenter_panel2.hiss |
| panel3 | 512 | 3936 | 28860.4 | T4_RED_GalaxyCenter_panel3.hiss |

seed=20260730, target_cp_per_panel=800, n_strata=8

## 3. 掩膜结果

| Panel | median | mad_sigma | star_thresh | sat_thresh | n_star | n_sat | n_anom | n_valid | valid% |
|-------|--------|-----------|-------------|------------|--------|-------|--------|---------|--------|
| panel1 | 25272.5 | 2469.2 | 37618.5 | 42706.6 | 9 | 4 | 0 | 3919 | 99.77% |
| panel2 | 27836.5 | 2565.8 | 40665.6 | 41063.6 | 5 | 4 | 0 | 3922 | 99.87% |
| panel3 | 28860.4 | 3265.3 | 45187.0 | 59987.4 | 14 | 4 | 0 | 3922 | 99.64% |

- 总有效像素: 11763 / 11791 (99.76%)
- 无异常像素 (NaN/Inf/≤0): 0 (V1 HISS 数据质量良好)

## 4. 控制点采样

| Panel | n_ctrl_pts | RA range | Dec range | SNR median |
|-------|-----------|----------|-----------|------------|
| panel1 | 800 | [268.68, 276.94] | [-16.33, -9.90] | 454.7 |
| panel2 | 800 | [268.68, 277.12] | [-21.46, -15.02] | 264.0 |
| panel3 | 800 | [268.51, 277.29] | [-26.44, -20.03] | 234.7 |

- 总控制点: 2400 (每片 800, 分层随机按 dec 8 箱均匀采样)
- 所有控制点 SNR 有限 (IDW 插值覆盖率 100%)

## 5. 契约合规

| 契约 | 状态 | 说明 |
|------|------|------|
| 只采有效覆盖 (support=1) | PASS | V1 HISS 所有像素 support=1, 掩膜后仅采 valid 区域 |
| 星点掩膜 | PASS | median + 5×MAD-sigma, 检出 9-14 个亮源/片 |
| 饱和掩膜 | PASS | p99.9, 检出 4 个/片 |
| 异常掩膜 | PASS | NaN/Inf/≤0, 检出 0 个 (数据质量良好) |

## 6. 证据文件

- `e001_result.json` — 结构化结果
- `logs/e_pipeline.log` — 运行日志

## 7. 运行命令

```
python lib/healpix_db/healpix_stack/python/e_chain/run_e_pipeline.py
```

elapsed: 0.100s
