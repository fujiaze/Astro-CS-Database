# EVIDENCE_INDEX: P01-002 数据块注册表与 schema 校验器

- Task ID: P01-002
- Phase: P01（合约冻结阶段）
- Commit/base: 7b85ff3f0d37a4b26fff6077684993842ed2bbae
- 执行时间: 2026-07-25

## 交付物清单

| Evidence | Description | SHA-256 | Size |
|---|---|---|---:|
| `engineering/contracts/pipeline_blocks_registry.csv` | 数据块注册表（完善后，10 列 10 块） | 35B40DE0FF592B1AA77963A7345EBE216D22EC882322F281A3B83ED1FB3CF03C | 2687 |
| `engineering/tools/validate_block_schema.py` | Python schema 校验器（注册表/HISS/PipelineFrame 三类校验） | F843AC330BC62FE2A57D9AB50C0FD9899833F5429984283571152D1B41194BF3 | 33368 |
| `engineering/evidence/P01-002/block_schema_validation.json` | 结构化校验结果（注册表摘要+校验项+不一致项） | B88275A24B8B4203160615BABE9A0658EC6D5BA72B4008AB047DBE71C9028955 | 19374 |
| `engineering/evidence/P01-002/TASK_REPORT.md` | 任务报告（覆盖旧依赖锁定内容） | 3E302290989A0C4B9D14F972252AD1A6F8CAB18F2EA270AA2A28F44268A1FC7C | 9066 |
| `engineering/evidence/P01-002/TEST_REPORT.md` | 测试报告（24 项测试） | 9B947528E8CD2512D2800089180AF303ABDFC3C661D943240AE7730F48938ECD | 6937 |
| `engineering/evidence/P01-002/REVIEW_REPORT.md` | 独立复核报告（VERDICT: PASS） | 7AB843B9C420FD79759ADD155FCB7AA732872E4BE8B1CA35334BF1217AF571D2 | 5592 |
| `engineering/evidence/P01-002/EVIDENCE_INDEX.md` | 证据索引（本文件） | self | - |

## 测试输入（来自 P00-003，只读引用）

| Input | Description | Source |
|---|---|---|
| `engineering/evidence/P00-003/output/stage1_baseline.hiss` | HISS 校验输入（47693 字节，P00-003 基线） | P00-003 |
| `engineering/evidence/P00-003/stage1_run.err.log` | orchestrator DEBUG 日志（块操作提取源） | P00-003 |

## 校验结果摘要

| 指标 | 值 |
|---|---|
| 注册表块总数 | 10（9 必需契约 + 1 观察项 gaia_cat） |
| 必需契约块覆盖 | 9/9（missing=[]） |
| 校验器总检查数 | 97 |
| PASS / FAIL / WARN / SKIP | 87 / 5 / 4 / 1 |
| 不一致项总数 | 16 |
| Critical | 3（star_det 类型/维度不匹配） |
| Major | 4（cal_stats/astrometry_stats/snr_format/provenance） |
| Minor/WARN | 5（gaia_cat/photo_stats 键名/snr_model 退化等） |
| 校验器运行状态 | 成功（exit=2，发现 critical 不一致，符合设计） |
| 任务 VERDICT | PASS |

## 关键发现

### Critical 不一致（P02 修复目标）
- `orchestrator.cpp:1465` 写入 star_det 为 `AIO_BLOCK_FLOAT32 [N,4]`（x,y,flux,mag）
- 契约 `star_det_block_v1.md` 要求 `FLOAT64 [N,6]`（x,y,flux,mag,saturated,has_saturated）
- 修复归属：P02-002（共享 detections 候选路径与 star_det v1）

### 观察项（待 ADR）
- `gaia_cat` FLOAT64[N,3] 由 orchestrator.cpp:1529 生产但 04 契约未定义，注册表登记为 v0 观察项

## 备注
- 旧 `dependencies.lock.*` 文件保留原位（前一次 P01-002 依赖锁定任务产物），本任务不删除，向后兼容。
- 本任务无业务源码改动（lib/ 无改动），仅完善注册表 + 新建校验器 + 生成证据。
- 校验器依赖 `zstandard 0.25.0`（pip 安装），非 Python 标准库。
