# G-003 TASK REPORT — HCSD 生产层和可开关调试质量层

- Gate: G
- 状态: **PASS**
- 依赖: G-002 (已完成)
- 实现文件: `lib/healpix_db/healpix_stack/python/g_chain/g003_hcsd.py`
- 运行脚本: `lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py`
- 日期: 2026-07-30

## 1. 任务目标

生成 HCSD 文件 (天球数据库), 生产层含融合 signal + support + SNR, 调试质量层 (可开关) 含每帧贡献图、排异图、权重图。HCSD 格式参考 `lib/astro_image_io/docs/HEALPIX_FORMAT_SPEC.md` §3。

## 2. 方法

### 2.1 HCSD 格式 (纯 Python 实现, 不依赖 DLL)
- Magic "HCSD" (4B) + JSON 头 (zstd 压缩) + 子叶索引表 (49152 × 24B) + ipix 数组 (u64) + pixel 数组 (f32)
- 子叶分区: nside=64, 49152 个子叶, shift = 2×(log2(512)-6) = 6
- ipix 按 (leaf_ipix, ipix) 升序排序, 同子叶内连续存储
- 实现: `g_common.hcsd_write` / `g_common.hcsd_read`

### 2.2 生产层 (必写)
- `T4_RED_GalaxyCenter_fused.hcsd`: signal (f32, 融合后)
- `T4_RED_GalaxyCenter_fused.hcsd.snr`: SNR 附属文件 (f32, 同 ipix 顺序, 因 HCSD 格式只存单 pixel 数组)
- meta: filter=Red, n_frames=3, total_exposure_s=540, sigma_clip, stack_stats, source, pipeline

### 2.3 调试质量层 (可开关, 默认 ON)
- `T4_RED_GalaxyCenter_fused.hcsd.debug.npz`: numpy 压缩格式
- 含: ipix, weight_map, support_count, per_panel_contrib (3 帧), rejected_per_panel (3 帧)
- 开关: `HcsdParams.write_debug_layers = True/False`

## 3. 结果

### 3.1 生产层 HCSD
| 指标 | 值 |
|------|-----|
| 文件 | `output/G-F/T4_RED_GalaxyCenter_fused.hcsd` |
| 大小 | 1,296,396 bytes (1.24 MB) |
| nside | 512 |
| nested | true |
| n_pix | 9706 |
| ipix_sorted | true |
| ipix_unique | true |
| n_leaves_with_data | 185 |
| signal median | 27360.1 |
| signal range | [15845.8, 38894.3] |
| filter | Red |
| n_frames | 3 |

### 3.2 生产层 SNR 附属文件
| 指标 | 值 |
|------|-----|
| 文件 | `output/G-F/T4_RED_GalaxyCenter_fused.hcsd.snr` |
| 大小 | 38,824 bytes (9706 × 4B f32) |

### 3.3 调试层
| 指标 | 值 |
|------|-----|
| 文件 | `output/G-F/T4_RED_GalaxyCenter_fused.hcsd.debug.npz` |
| 大小 | 87,195 bytes |
| 内容 | ipix, weight_map, support_count, contrib_panel1/2/3, rejected_panel1/2/3 |

### 3.4 格式合规验证
- ipix 升序: PASS
- ipix 唯一: PASS
- filter=Red: PASS
- n_frames=3: PASS

## 4. 禁止捷径合规

| 禁止捷径 | 状态 | 说明 |
|---------|------|------|
| 调试层开关 | PASS | `HcsdParams.write_debug_layers` 控制, 默认 ON 可关 |
| 不得产生硬边 | PASS | G-002 连续加权, F-002 验证接缝连续性 PASS |
| HCSD 可读 | PASS | F-002 v1 验证读回成功, 格式合规 |

## 5. 证据文件

| 路径 | 说明 |
|------|------|
| `evidence/G-003/g003_result.json` | 结构化结果 |
| `evidence/G-003/g_pipeline_summary.json` | 全 pipeline 汇总 |
| `evidence/G-003/TASK_REPORT.md` | 本报告 |
| `output/G-F/T4_RED_GalaxyCenter_fused.hcsd` | 生产层 HCSD |
| `output/G-F/T4_RED_GalaxyCenter_fused.hcsd.snr` | 生产层 SNR |
| `output/G-F/T4_RED_GalaxyCenter_fused.hcsd.debug.npz` | 调试层 |
| `lib/healpix_db/healpix_stack/python/g_chain/logs/g_pipeline.log` | 运行日志 |

## 6. 运行命令

```
python lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py
```

- 耗时: 0.058s
- Python 3.10.11, numpy, zstandard, astropy_healpix
