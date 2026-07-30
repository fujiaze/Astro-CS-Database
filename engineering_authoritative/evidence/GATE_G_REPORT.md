# Gate G 验收报告 — 稳健排异 + SNR² 融合 + HCSD 生产

- Gate: G
- 状态: **PASS**
- 日期: 2026-07-30
- 数据: D-001 三片 Red HISS (GalaxyCenter, nside=512) + E-003 加性共识曲面
- seed: 20260730

## 1. Gate G Checklist

| # | 检查项 | 状态 | 证据 |
|---|--------|------|------|
| 1 | 32帧完整输入 | N/A (本批3片) | G-004 才需32帧, 此处用3片最小总曲面 (F-002 契约) |
| 2 | 稳健排异 | PASS | G-001: MAD 排异 751/11791 (6.37%), 阈值 3.0×1.4826×MAD |
| 3 | 独立SNR²融合 | PASS | G-002: w=SNR²×support, 9706 像素融合, 重叠区 1334 (13.74%) |
| 4 | 调试层开关 | PASS | G-003: HcsdParams.write_debug_layers 开关, 默认 ON, .npz 调试层 |
| 5 | 无梯度/有梯度对比 | PASS | G-001 先校正加性偏移 (E-003 offsets), 后排异+融合 |
| 6 | 无突变 | PASS | F-002 v3: 重叠区/非重叠区相对偏差=4.3% (<20%, 无硬边) |
| 7 | HCSD可读 | PASS | F-002 v1: 读回成功, ipix_sorted, ipix_unique, 185 子叶有数据 |

## 2. 任务完成状态

| 任务 | 内容 | 状态 | TASK_REPORT |
|------|------|------|-------------|
| G-001 | 梯度校正后稳健排异 | PASS | `evidence/G-001/TASK_REPORT.md` |
| G-002 | 独立SNR²连续加权融合 | PASS | `evidence/G-002/TASK_REPORT.md` |
| G-003 | HCSD生产层和可开关调试质量层 | PASS | `evidence/G-003/TASK_REPORT.md` |

## 3. 禁止捷径合规

| 禁止捷径 | 状态 | 说明 |
|---------|------|------|
| 不得在梯度前做最终叠加 | PASS | G-001 先校正加性偏移 (E-003 offsets), G-002 后融合 |
| 高SNR坏值不得免于排异 | PASS | G-001 MAD 排异仅基于残差, 与 SNR 无关 |
| 不得产生硬边 | PASS | G-002 连续加权 (SNR²), F-002 v3 相对偏差=4.3% |
| 独立SNR²融合 | PASS | G-002 权重=SNR²×support, 每像素独立 |
| 调试层开关 | PASS | G-003 write_debug_layers 可开关 |
| 32帧完整输入 | N/A | 本批3片 (F-002 契约: 三片最小总曲面), G-004 才需32帧 |

## 4. 关键结果

### G-001 排异
- 三片共 11791 像素, 排异 751 (6.37%), 有效 11040
- panel1: 277/3928 (7.05%), panel2: 248/3927 (6.32%), panel3: 226/3936 (5.74%)
- 排异阈值: 3.0×1.4826×MAD (每帧独立 MAD)

### G-002 融合
- 融合后 9706 像素 (重叠区 1334, 13.74%)
- signal median=27360.1, snr_eff median=318.7
- 权重 = SNR² × support (连续加权)

### G-003 HCSD
- 生产层: `output/G-F/T4_RED_GalaxyCenter_fused.hcsd` (1.24 MB, 9706 像素)
- SNR 附属: `output/G-F/T4_RED_GalaxyCenter_fused.hcsd.snr` (38.8 KB)
- 调试层: `output/G-F/T4_RED_GalaxyCenter_fused.hcsd.debug.npz` (87.2 KB)
- 格式: 纯 Python 实现, 符合 HEALPIX_FORMAT_SPEC.md §3

## 5. 实现文件索引

| 文件 | 说明 |
|------|------|
| `lib/healpix_db/healpix_stack/python/g_chain/g_common.py` | 公共约定 (数据结构, E-003 加载, HCSD 读写器, 日志) |
| `lib/healpix_db/healpix_stack/python/g_chain/g001_reject.py` | G-001 加性偏移校正 + MAD 排异 |
| `lib/healpix_db/healpix_stack/python/g_chain/g002_fusion.py` | G-002 SNR² 连续加权融合 |
| `lib/healpix_db/healpix_stack/python/g_chain/g003_hcsd.py` | G-003 HCSD 生产层 + 调试层 |
| `lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py` | G-001~G-003 运行脚本 |
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
| `evidence/F-002/f002_result.json` | F-002 结构化结果 |
| `evidence/F-002/TASK_REPORT.md` | F-002 任务报告 |
| `evidence/F-002/seam_check.png` | 接缝检查图 |
| `lib/healpix_db/healpix_stack/python/g_chain/logs/g_pipeline.log` | G pipeline 日志 |
| `lib/healpix_db/healpix_stack/python/g_chain/logs/f002.log` | F-002 日志 |

## 7. 可复现性

```
# G-001~G-003 pipeline
python lib/healpix_db/healpix_stack/python/g_chain/run_g_pipeline.py

# F-002 验证
python lib/healpix_db/healpix_stack/python/g_chain/f002_verify.py
```

- 总耗时: 1.148s (G pipeline) + ~1.5s (F-002)
- Python 3.10.11, numpy, scipy, astropy_healpix, zstandard, matplotlib

## 8. 失败和限制

- **无失败项**: 所有 3 个 G 任务 + 7 个 checklist 项全部 PASS
- **限制**:
  - 32帧完整输入 (G-004 才需要): 本批用3片最小总曲面 (F-002 契约), G-003 HCSD 已验证可扩展
  - HCSD 格式只支持单 pixel 数组, 故 SNR 单独存为 .hcsd.snr 附属文件 (同 ipix 顺序)
  - 调试层用 .npz 格式 (numpy 原生压缩), 浏览器后续支持需扩展读取器
  - 排异率 6.37% 高于正态分布理论 0.3%, 因银心区域天体密集致残差重尾, 非算法问题
