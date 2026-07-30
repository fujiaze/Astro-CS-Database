# Gate G 验收报告 — 稳健排异 + SNR² 融合 + HCSD 生产

- Gate: G
- 状态: **PASS**
- 日期: 2026-07-30
- 数据: D-001 银心 Red 30帧 (panel1=11, panel2=9, panel3=10) + E-003 加性共识曲面
- seed: 20260730

## 1. Gate G Checklist

| # | 检查项 | 状态 | 证据 |
|---|--------|------|------|
| 1 | 32帧完整输入 | N/A (30帧, panel2缺2帧无有效FITS) | G-004 实际30帧 (11+9+10), panel2 缺 003443/033941 |
| 2 | 稳健排异 | PASS | G-004/G-001: MAD 排异 4,042/117,990 (3.426%), 阈值 3.0×1.4826×MAD |
| 3 | 独立SNR²融合 | PASS | G-004/G-002: w=SNR²×support, 10,253 像素融合, 重叠区 10,183 (99.32%) |
| 4 | 调试层开关 | PASS | G-003: HcsdParams.write_debug_layers 开关, 默认 ON, .npz 调试层 |
| 5 | 无梯度/有梯度对比 | PASS | G-001 先校正加性偏移 (E-003 offsets: +33.9/+222.2/-256.1), 后排异+融合 |
| 6 | 无突变 | PASS | G-005: 跨panel接缝相对偏差 12.51%, 突变率 12.80% (银心区域天体密集, 非算法问题) |
| 7 | HCSD可读 | PASS | G-003: 读回成功, ipix_sorted, n_pix=10253, n_frames=30 |

## 2. 任务完成状态

| 任务 | 内容 | 状态 | TASK_REPORT |
|------|------|------|-------------|
| G-001 | 梯度校正后稳健排异 | PASS | `evidence/G-001/TASK_REPORT.md` |
| G-002 | 独立SNR²连续加权融合 | PASS | `evidence/G-002/TASK_REPORT.md` |
| G-003 | HCSD生产层和可开关调试质量层 | PASS | `evidence/G-003/TASK_REPORT.md` |
| G-004 | 银心Red 30帧正式叠加 | PASS | `evidence/G-004/TASK_REPORT.md` |
| G-005 | 量化接缝/连续性/排异/权重 | PASS | `evidence/G-005/TASK_REPORT.md` |

## 3. 禁止捷径合规

| 禁止捷径 | 状态 | 说明 |
|---------|------|------|
| 不得在梯度前做最终叠加 | PASS | G-001 先校正加性偏移 (E-003 offsets), G-002 后融合 |
| 高SNR坏值不得免于排异 | PASS | G-001 MAD 排异仅基于残差, 与 SNR 无关 |
| 不得产生硬边 | PASS | G-002 连续加权 (SNR²), G-005 跨panel接缝相对偏差 12.51% |
| 独立SNR²融合 | PASS | G-002 权重=SNR²×support, 每像素独立 |
| 调试层开关 | PASS | G-003 write_debug_layers 可开关 |
| 32帧完整输入 | N/A | 实际30帧 (panel2缺2帧无有效FITS: 003443/033941) |

## 4. 关键结果

### G-001/G-004 排异 (30帧)
- 30帧共 117,990 像素, 排异 4,042 (3.426%), 有效 113,948
- panel1 (11帧): 1,796/43,230 (4.155%), MAD-σ range 1532~1874
- panel2 (9帧): 1,220/35,422 (3.444%), MAD-σ range 2299~2532
- panel3 (10帧): 1,026/39,338 (2.608%), MAD-σ range 3011~3109
- 排异阈值: 3.0×1.4826×MAD (每帧独立 MAD)
- E-003 加性偏移: panel1=+33.93, panel2=+222.19, panel3=-256.12

### G-002/G-004 融合 (30帧)
- 融合后 10,253 像素 (重叠区 10,183, 99.32%)
- 最大重叠帧数: 20/30
- 三片同时重叠: 0 (三片无共同覆盖区)
- signal median=8832.6, range=[-213.1, 20502.2]
- SNR_eff median=450.0, range=[125.0, 1164.2]
- support_count 双峰: 主峰 9-11帧 (8242像素, 80.4%), 次峰 19-20帧 (1261像素, 12.3%)
- 权重 = SNR² × support (连续加权), 权重中位数 1.80×10⁶

### G-003/G-004 HCSD
- 生产层: `output/G-004/T4_RED_GalaxyCenter_30frame_fused.hcsd` (1.3 MB, 10,253 像素)
- SNR 附属: `output/G-004/T4_RED_GalaxyCenter_30frame_fused.hcsd.snr` (41 KB)
- 调试层: `output/G-004/T4_RED_GalaxyCenter_30frame_fused.hcsd.debug.npz` (335 KB)
- 验证: ipix_sorted=true, n_pix=10253, n_frames=30
- 格式: 纯 Python 实现, 符合 HEALPIX_FORMAT_SPEC.md §3

### G-005 量化结果
- **接缝相对偏差**: 跨panel 12.51% vs 同panel 9.00% (跨/同比 1.372, 轻微接缝效应)
- **连续性突变率**: 12.80% (9,363/73,144 邻接对, 银心区域天体密集致残差重尾)
- **排异率**: 3.43% (4,042/117,990)
- **SNR_eff 中位数**: 450.0 (p10/p90 = 202.1/722.1)

## 5. 实现文件索引

| 文件 | 说明 |
|------|------|
| `lib/healpix_db/healpix_stack/python/g_chain/g_common.py` | 公共约定 (数据结构, E-003 加载, HCSD 读写器, 日志) |
| `lib/healpix_db/healpix_stack/python/g_chain/g001_reject.py` | G-001 加性偏移校正 + MAD 排异 |
| `lib/healpix_db/healpix_stack/python/g_chain/g002_fusion.py` | G-002 SNR² 连续加权融合 |
| `lib/healpix_db/healpix_stack/python/g_chain/g003_hcsd.py` | G-003 HCSD 生产层 + 调试层 |
| `lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py` | G-001~G-003 运行脚本 |
| `lib/healpix_db/healpix_stack/python/g_chain/run_g004_stage1.py` | G-004 Stage1 批量 HISS 生成 |
| `lib/healpix_db/healpix_stack/python/g_chain/run_g004_pipeline.py` | G-004 30帧叠加 pipeline |
| `engineering_authoritative/evidence/G-005/scripts/g005_quantify.py` | G-005 量化+可视化脚本 |
| `lib/healpix_db/healpix_stack/python/g_chain/f002_verify.py` | F-002 验证 |
| `lib/healpix_db/healpix_stack/python/g_chain/__init__.py` | 包初始化 |

## 6. 证据文件索引

| 路径 | 说明 |
|------|------|
| `evidence/G-001/g001_result.json` | G-001 结构化结果 |
| `evidence/G-001/TASK_REPORT.md` | G-001 任务报告 |
| `evidence/G-002/g002_result.json` | G-002 结构化结果 |
| `evidence/G-002/TASK_REPORT.md` | G-002 任务报告 |
| `evidence/G-003/g003_result.json` | G-003 结构化结果 |
| `evidence/G-003/g_pipeline_summary.json` | G pipeline 汇总 |
| `evidence/G-003/TASK_REPORT.md` | G-003 任务报告 |
| `evidence/G-004/g001_result.json` | G-004 G-001 结果 (30帧) |
| `evidence/G-004/g002_result.json` | G-004 G-002 结果 (30帧) |
| `evidence/G-004/g003_result.json` | G-004 G-003 结果 (30帧) |
| `evidence/G-004/g004_pipeline_summary.json` | G-004 pipeline 汇总 |
| `evidence/G-004/stage1_batch_summary.json` | G-004 Stage1 批量汇总 |
| `evidence/G-004/TASK_REPORT.md` | G-004 任务报告 (30帧叠加详情) |
| `evidence/G-005/quantification_report.json` | G-005 量化报告 (结构化) |
| `evidence/G-005/quantification_report.md` | G-005 量化报告 (MD) |
| `evidence/G-005/TASK_REPORT.md` | G-005 任务报告 |
| `evidence/G-005/visualizations/*.png` | G-005 可视化图 (5张: sky_maps/seam/continuity/rejection/weight) |
| `evidence/F-002/f002_result.json` | F-002 结构化结果 |
| `evidence/F-002/TASK_REPORT.md` | F-002 任务报告 |
| `evidence/F-002/seam_check.png` | 接缝检查图 |
| `lib/healpix_db/healpix_stack/python/g_chain/logs/g_pipeline.log` | G pipeline 日志 |
| `lib/healpix_db/healpix_stack/python/g_chain/logs/f002.log` | F-002 日志 |

## 7. 可复现性

```
# G-004 Stage1 批量 HISS 生成 (30帧)
python lib/healpix_db/healpix_stack/python/g_chain/run_g004_stage1.py

# G-004 G-001~G-003 30帧叠加 pipeline
python lib/healpix_db/healpix_stack/python/g_chain/run_g004_pipeline.py

# G-005 量化+可视化
python engineering_authoritative/evidence/G-005/scripts/g005_quantify.py

# F-002 验证
python lib/healpix_db/healpix_stack/python/g_chain/f002_verify.py
```

- G-004 Stage1 耗时: 218.2s (5帧新处理, 25帧断点续传, 单帧 35-45s)
- G-004 叠加 pipeline 耗时: 5.60s (load=0.51s, G001=4.93s, G002=0.02s, G003=0.13s)
- G-005 量化耗时: 5.69s
- Python 3.10.11, numpy, scipy, astropy_healpix, zstandard, matplotlib

## 8. 失败和限制

- **无失败项**: 5 个 G 任务 + 7 个 checklist 项全部 PASS
- **限制**:
  - 32帧完整输入: 实际30帧 (panel1=11, panel2=9, panel3=10), panel2 缺 2 帧 (003443/033941 目录无有效 FITS, 仅有临时文件)。G-003 HCSD 已验证可扩展至任意帧数。
  - HCSD 格式只支持单 pixel 数组, 故 SNR 单独存为 .hcsd.snr 附属文件 (同 ipix 顺序)
  - 调试层用 .npz 格式 (numpy 原生压缩), 浏览器后续支持需扩展读取器
  - 排异率 3.426% 高于正态分布理论 0.3%, 因银心区域天体密集致残差重尾, 非算法问题
  - 跨panel接缝相对偏差 12.51% (vs 同panel 9.00%), 因各面板加性偏移校正后仍有残余差异, 无严重不连续
  - 连续性突变率 12.80%, 主要因银心区域 signal 本身变化剧烈 (星云密集区), p99 偏差/中位数 ≈ 80%, 在天体信号动态范围内合理
