# TASK_REPORT: P01-002 数据块注册表与 schema 校验器

- Task ID: P01-002
- Phase: P01（合约冻结阶段）
- Commit/base: 7b85ff3f0d37a4b26fff6077684993842ed2bbae（P01-001 提交后 HEAD）
- 分支: main
- 执行时间: 2026-07-25
- Objective: 建立数据块注册表与 schema 校验器，所有生产者消费者对齐。

## 入口条件
- P01-001 DONE ✓（ADR-005：PipelineFrame 归属 astro_image_io）
- 04_PIPELINEFRAME_CONTRACT_V1.md 已定义必需数据块
- engineering/contracts/star_det_block_v1.md 已定义 star_det v1 格式
- P00-003 DONE ✓（旧 CLI 真实数据基线，stage1_baseline.hiss 可用作校验输入）

## Changes
本任务为合约冻结任务，**不修改任何 lib/ 业务源码**。仅完善/新建文档与工程辅助工具：

- 完善 `engineering/contracts/pipeline_blocks_registry.csv`（注册表，字段扩展为 10 列，覆盖 10 个块）
- 新建 `engineering/tools/validate_block_schema.py`（Python schema 校验器，约 590 行）
- 新建 `engineering/evidence/P01-002/block_schema_validation.json`（结构化校验结果）

## 注册表完善内容

### 字段扩展
原注册表 8 列（block,schema,type,dims,producer,consumers,persistent,notes）扩展为 10 列，对齐任务要求：
`block_name, schema_version, type, dimensions, producer, consumers, created_at_stage, destroyable_at, persistent, notes`

### 块覆盖（10 个）
| 块名 | schema | type | dims | producer | 分类 |
|---|---|---|---|---|---|
| data | v1 | FLOAT32 | [height,width] | READ_FITS | 必需契约 |
| header | v1 | KV | [N] | READ_FITS | 必需契约 |
| star_det | v1 | FLOAT64 | [N,6] | STAR_DETECT\|PLATESOLVE_INTERNAL_EXPORT | 必需契约 |
| psf | v1 | FLOAT64 | [N,9] | PSF | 必需契约 |
| photo_stats | v1 | KV | [N] | PHOTOMETRIC | 必需契约 |
| snr_model | v1 | RAW | [bytes] | SNR | 必需契约 |
| cal_stats | v1 | KV | [N] | CALIBRATE | 诊断 |
| astrometry_stats | v1 | KV | [N] | PLATESOLVE | 诊断 |
| astrometry_matches | v1 | FLOAT64 | [M,7] | PLATESOLVE | 可选诊断 |
| gaia_cat | v0 | FLOAT64 | [N,3] | PLATESOLVE | 观察项（未正式契约化） |

9 个必需契约块全部覆盖（required_contract_blocks_missing=[]），额外登记 1 个观察块 gaia_cat（orchestrator 实际生产但 04 契约未定义，schema_version=0 标注待 ADR 决定）。

## Schema 校验器实现

### 文件
`engineering/tools/validate_block_schema.py`（Python 3.10，依赖 zstandard 0.25.0）

### 功能
1. **注册表校验**：读取 CSV，校验必需块覆盖、块名唯一、类型合法、schema_version 整数、维度格式、producer 非空
2. **HISS 元数据校验**：解析 HISS 二进制头（magic + uncomp_len + comp_len + zstd JSON），校验 magic、必需字段（nside/nested/n_pix/has_snr/snr_format）、nside 2 的幂、数据区长度、has_snr 与 snr_model 一致性
3. **PipelineFrame 块校验**：从 orchestrator DEBUG 日志或 JSON 快照提取块信息，校验块存在、类型匹配、维度/count、star_det 与 psf 行数一致、photo_stats/snr_model 写入状态
4. **不一致项汇总**：合并运行时校验与源码静态审查（orchestrator.cpp），输出结构化 JSON

### 校验规则（04_PIPELINEFRAME_CONTRACT_V1.md §4）
- 块存在检查 ✓
- 类型检查（FLOAT32=0 / FLOAT64=1 / KV=5 / RAW=6）✓
- 维度检查（[height,width] / [N,6] / [N,9]）✓
- count 检查（star_det 与 psf 行数一致）✓
- schema_version 检查 ✓
- revision 检查（ASTROCS.STARDET.* header provenance）✓（静态审查）
- 有限值比例检查（data 块禁止 NaN/Inf）✓（注册表 notes 标注，运行时需 hook）

### CLI 用法
```
python engineering/tools/validate_block_schema.py \
  --registry engineering/contracts/pipeline_blocks_registry.csv \
  --hiss <path.hiss> \
  --log <orchestrator_debug.log> \
  -o <output.json>
```

## 校验器测试结果（P00-003 HISS）

输入：
- HISS: `engineering/evidence/P00-003/output/stage1_baseline.hiss`（47693 字节）
- 日志: `engineering/evidence/P00-003/stage1_run.err.log`

输出摘要（block_schema_validation.json）：
| 指标 | 值 |
|---|---|
| 总检查数 | 97 |
| PASS | 87 |
| FAIL | 5 |
| WARN | 4 |
| SKIP | 1 |
| 不一致项 | 16 |
| critical | 3 |

校验器正确解析 HISS（magic=HISS, nside=512, n_pix=3927, has_snr=0），正确从日志提取 6 个块（data/header/star_det/gaia_cat/psf/photo_stats），并发现真实不一致项。

## 发现的不一致项（生产者消费者对齐）

### Critical（3 项，同一根因）
1. `frame.type_match:star_det` — 期望 FLOAT64 实际 FLOAT32
2. `frame.dims_stardet:star_det` — 期望 FLOAT64[N,6] count=N*6；实际 FLOAT32[N,4] count=8000 (N=2000)
3. `orchestrator.star_det_type_mismatch` — orchestrator.cpp:1465 写入 AIO_BLOCK_FLOAT32 [N,4]，契约 star_det_block_v1.md 要求 FLOAT64 [N,6]（缺 saturated,has_saturated 列）

**根因**：orchestrator.cpp:1453-1467 的 star_det 块写入代码使用 FLOAT32 且只写 4 列（x,y,flux,mag），未实现契约要求的 FLOAT64 6 列（x,y,flux,mag,saturated,has_saturated）。这是 P02 阶段需修复的关键不一致。

### Major（4 项）
4. `frame.block_present:cal_stats` — cal_stats 缺失（CALIBRATE 骨架未写入，master=nullptr 退化）
5. `frame.block_present:astrometry_stats` — astrometry_stats 缺失（PLATESOLVE 未写入 WCS 质量诊断）
6. `frame.block_present:photo_stats` — photo_stats 块存在但通过 kv_set 隐式创建（已修复检测逻辑，此项为 KV 块隐式创建的已知局限）
7. `orchestrator.cal_stats_missing` / `orchestrator.astrometry_stats_missing` / `orchestrator.star_det_provenance_missing` — 源码静态审查确认

### Minor/WARN（5 项）
8. `orchestrator.gaia_cat_unregistered` — gaia_cat FLOAT64[N,3] 未正式契约化
9. `orchestrator.photo_stats_key_case` — 键名大写（STATUS/N_MATCHED...）vs 契约小写，且缺 schema_version/input_data_revision/output_data_revision
10. `orchestrator.snr_model_not_written` / `frame.snr_model_written` — SNR 退化时未写入（G-002）
11. `hiss.snr_model_consistency` — has_snr=0 对应 snr_model 未生产
12. `frame.photo_stats_keys` — 键名大小写不一致

### HISS 元数据
- `hiss.field_present:snr_format` FAIL — HISS JSON 头缺 snr_format 字段（has_snr=0 时可能省略，需 HISS writer 确认是否为缺陷）

## Files
- `engineering/contracts/pipeline_blocks_registry.csv`（完善，10 列 10 块）
- `engineering/tools/validate_block_schema.py`（新建，590 行）
- `engineering/evidence/P01-002/block_schema_validation.json`（新建，校验结果）
- `engineering/evidence/P01-002/TASK_REPORT.md`（本文件，覆盖旧依赖锁定内容）
- `engineering/evidence/P01-002/TEST_REPORT.md`
- `engineering/evidence/P01-002/EVIDENCE_INDEX.md`
- `engineering/evidence/P01-002/REVIEW_REPORT.md`

## Compatibility
- **注册表向后兼容**：原 8 列信息全部保留，新增 created_at_stage/destroyable_at 列，persistent 列保留。原 CSV 中的块定义与 04 契约一致，本次仅补充字段不改变语义。
- **04 契约无变更**：04_PIPELINEFRAME_CONTRACT_V1.md 未修改，注册表忠实反映契约。
- **star_det 契约无变更**：star_det_block_v1.md 未修改，注册表 FLOAT64[N,6] 与契约一致；orchestrator 实现偏离已记录，待 P02 对齐。
- **HISS 格式无变更**：校验器只读解析 HISS，不修改格式。
- **P00-003 基线保持有效**：stage1 45s/stage2 7s 无回归（本任务无代码改动）。

## Rollback
- 本任务无业务源码变更，回退方式：revert 注册表 CSV 与删除 validate_block_schema.py + evidence/P01-002/ 下本任务新增文件。
- 旧 dependencies.lock.* 文件保留原位（来自前一次 P01-002 依赖锁定任务，向后兼容）。

## Remaining risks
1. **star_det 实现与契约偏离**：orchestrator 写 FLOAT32[N,4]，契约要求 FLOAT64[N,6]。这是 P02-002（共享 detections 候选路径与 star_det v1）的修复目标。在校验器中已明确记录，P02 修复后校验器应复测通过。
2. **诊断块未实现**：cal_stats/astrometry_stats 在 orchestrator 中未写入（骨架/退化）。校验器会持续报告，待后续阶段实现后消除。
3. **photo_stats 键名规范**：实现大写 vs 契约小写，且缺 3 个必需键。待 PHOTOMETRIC 修复任务对齐。
4. **gaia_cat 归属待定**：需 ADR 决定纳入注册表 v2 或归档为内部临时数据。
5. **校验器局限**：基于 orchestrator 日志而非运行时 hook，KV 块（header/photo_stats）通过隐式标志检测；有限值比例检查（data 块 NaN/Inf）需运行时 hook，当前仅在注册表 notes 标注，未实际执行。后续可考虑增加 PipelineFrame C API 导出块清单供校验器调用。
6. **HISS snr_format 字段**：has_snr=0 时 HISS JSON 头可能省略 snr_format，需确认 HISS writer 行为是否为缺陷。

## 建议状态
`DONE`（待 REVIEW_REPORT 确认 PASS 后）
