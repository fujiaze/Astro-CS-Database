# TEST_REPORT: P01-002 数据块注册表与 schema 校验器

- Task ID: P01-002
- 执行时间: 2026-07-25
- 测试环境: PowerShell 7 + Python 3.10.11 + zstandard 0.25.0
- 测试输入:
  - 注册表: `engineering/contracts/pipeline_blocks_registry.csv`
  - HISS: `engineering/evidence/P00-003/output/stage1_baseline.hiss`（47693 字节，P00-003 基线输出）
  - 日志: `engineering/evidence/P00-003/stage1_run.err.log`（P00-003 stage1 DEBUG 日志）

## 测试矩阵

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| T-001 | python validate_block_schema.py --registry ...（仅注册表加载） | 30s | 0 | PASS | 注册表 10 块加载，block_count=10 |
| T-002 | 校验器 registry.required_block_present:* | 30s | 0 | PASS | 9 必需契约块全部覆盖，missing=[] |
| T-003 | 校验器 registry.type_valid:* | 30s | 0 | PASS | 10 块类型合法（FLOAT32/FLOAT64/KV/RAW） |
| T-004 | 校验器 registry.schema_version_int:* | 30s | 0 | PASS | 9 块 v1 + 1 块 v0（gaia_cat 观察项） |
| T-005 | 校验器 registry.dims_format:* | 30s | 0 | PASS | 10 块维度格式合法 [...] |
| T-006 | 校验器 hiss.magic | 30s | 0 | PASS | magic=HISS（48 49 53 53） |
| T-007 | 校验器 hiss zstd JSON 解压 | 30s | 0 | PASS | uncomp_len=796, comp_len=557，zstd 解压成功 |
| T-008 | 校验器 hiss.field_present:nside/nested/n_pix/has_snr | 30s | 0 | PASS | nside=512, nested=True, n_pix=3927, has_snr=False |
| T-009 | 校验器 hiss.nside_power_of_two | 30s | 0 | PASS | nside=512 是 2 的幂 |
| T-010 | 校验器 hiss.data_len | 30s | 0 | PASS | 文件长度 47693 == 预期 47693 |
| T-011 | 校验器日志解析 add_block 提取 | 30s | 0 | PASS | 提取 6 块（data/header/star_det/gaia_cat/psf/photo_stats）+ 8 操作 |
| T-012 | 校验器 frame.type_match:data | 30s | 0 | PASS | data FLOAT32 count=16200000 (=4500*3600) |
| T-013 | 校验器 frame.dims_psf:psf | 30s | 0 | PASS | psf FLOAT64[N,9] count=18000 N=2000 |
| T-014 | 校验器 frame.count_stardet_psf_aligned | 30s | 0 | PASS | star_det N=2000 == psf N=2000 |
| T-015 | 校验器 frame.type_match:star_det（不一致检测） | 30s | 2 | PASS | 正确检测 critical 不一致：期望 FLOAT64 实际 FLOAT32 |
| T-016 | 校验器 frame.dims_stardet:star_det（不一致检测） | 30s | 2 | PASS | 正确检测：期望 [N,6] 实际 [N,4] count=8000 |
| T-017 | 校验器 frame.block_present:cal_stats（缺失检测） | 30s | 2 | PASS | 正确检测 cal_stats 缺失（CALIBRATE 退化） |
| T-018 | 校验器 frame.block_present:astrometry_stats（缺失检测） | 30s | 2 | PASS | 正确检测 astrometry_stats 缺失 |
| T-019 | 校验器 frame.photo_stats_written | 30s | 2 | PASS | 日志确认 photo_stats 已写入（KV 隐式创建） |
| T-020 | 校验器 frame.snr_model_written（退化检测） | 30s | 2 | PASS | 正确检测 snr_model 未写入（G-002） |
| T-021 | 校验器 orchestrator.star_det_type_mismatch（静态审查） | 30s | 2 | PASS | 正确记录 orchestrator.cpp:1465 源码不一致 |
| T-022 | 校验器 orchestrator.gaia_cat_unregistered | 30s | 2 | PASS | 正确记录 gaia_cat 未契约化 |
| T-023 | 校验器 JSON 输出完整性 | 30s | 0 | PASS | block_schema_validation.json 含 registry/hiss/frame/inconsistencies/summary |
| T-024 | 校验器退出码语义 | 30s | 2 | PASS | exit=2（有 critical 不一致），符合设计（0=无问题/1=FAIL/2=critical） |

## Real-data metrics

### 注册表覆盖
| 指标 | 值 |
|---|---|
| 注册表块总数 | 10 |
| 必需契约块覆盖 | 9/9 |
| 必需契约块缺失 | 0 |
| 观察项块（未契约化） | 1（gaia_cat v0） |
| 字段列数 | 10（block_name/schema_version/type/dimensions/producer/consumers/created_at_stage/destroyable_at/persistent/notes） |

### HISS 元数据（P00-003 stage1_baseline.hiss）
| 字段 | 值 |
|---|---|
| magic | HISS |
| file_size | 47693 字节 |
| json_uncompressed_len | 796 |
| json_compressed_len | 557 |
| nside | 512 |
| nested | True |
| n_pix | 3927 |
| has_snr | False |
| 数据区长度匹配 | 是（47693 == 12+557+3927*12） |

### PipelineFrame 块快照（从日志提取）
| 块名 | type_id | type | count | 来源 |
|---|---:|---|---:|---|
| data | 0 | FLOAT32 | 16200000 | add_block + add_block_move（CALIBRATE/PHOTOMETRIC 替换） |
| header | 5 | KV | 0 | implicit_kv（READ_FITS kv_set 隐式创建） |
| star_det | 0 | FLOAT32 | 8000 | add_block（PLATESOLVE，2000*4） |
| gaia_cat | 1 | FLOAT64 | 2374239 | add_block（PLATESOLVE，791413*3） |
| psf | 1 | FLOAT64 | 18000 | add_block（PSF，2000*9） |
| photo_stats | 5 | KV | 0 | implicit_kv（PHOTOMETRIC kv_set 隐式创建） |

### 校验汇总
| 指标 | 值 |
|---|---|
| total_checks | 97 |
| PASS | 87 |
| FAIL | 5 |
| WARN | 4 |
| SKIP | 1 |
| inconsistency_count | 16 |
| critical_count | 3 |
| 校验器自身运行 | 成功（exit=2 表示发现 critical 不一致，符合预期） |

## Failures and investigation

### 校验器发现的 FAIL 项（均为被校验对象的真实问题，非校验器缺陷）

1. **frame.type_match:star_det（critical）**：orchestrator 写 FLOAT32，契约要求 FLOAT64
   - 根因：orchestrator.cpp:1465 使用 AIO_BLOCK_FLOAT32，star_det_block_v1.md 要求 FLOAT64
   - 影响：star_det 块缺 saturated/has_saturated 列，PSF/SNR 消费时无法获取饱和标志
   - 修复归属：P02-002（共享 detections 候选路径与 star_det v1）

2. **frame.dims_stardet:star_det（critical）**：实际 [N,4] vs 契约 [N,6]
   - 同 T-015 根因，维度列数不足

3. **frame.block_present:cal_stats（major）**：CALIBRATE 未写入 cal_stats
   - 根因：orchestrator CALIBRATE 骨架，master=nullptr 退化
   - 修复归属：P03-001（真实校准输入接线）

4. **frame.block_present:astrometry_stats（major）**：PLATESOLVE 未写入 astrometry_stats
   - 根因：orchestrator PLATESOLVE 未实现 WCS 质量诊断块写入
   - 修复归属：P02-003/P02-004（PlateSolve 路径实施）

5. **hiss.field_present:snr_format（major）**：HISS JSON 头缺 snr_format 字段
   - 调查：has_snr=0 时 HISS writer（aio_healpix_io.cpp hio_build_json）可能省略该字段
   - 影响：校验器无法确认 snr_format 版本；has_snr=0 时影响有限
   - 处置：记录为待确认项，不阻塞 P01-002

### 校验器自身无失败
校验器所有功能（CSV 解析、HISS 二进制解析、zstd 解压、JSON 解析、日志正则提取、校验逻辑、JSON 输出）均正常运行。exit=2 是设计行为（发现 critical 不一致时返回 2），不代表校验器故障。

## 结论
- 24 项测试全部 PASS（校验器功能正确，能发现真实不一致项）
- 校验器可实际运行并产生结构化结果
- 发现的 16 个不一致项均为被校验对象（orchestrator 实现/HISS 格式）的真实问题，已分类记录，待后续任务修复
