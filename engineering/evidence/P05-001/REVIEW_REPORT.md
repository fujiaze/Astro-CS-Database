# P05-001 复核报告

## 复核信息
- **任务 ID**: P05-001
- **任务名称**: 真实参考数据集登记 (v1.1 开发包)
- **阶段**: P05
- **Gate**: G5
- **复核日期**: 2026-07-25
- **复核人**: AI Sub-agent (self-review)
- **Commit base**: f98de03b203996f3c56503c0c73bd4bd044ccd7f
- **VERDICT**: PASS

## 复核检查项

### 1. 任务范围合规性
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 数据集登记任务 | PASS | 仅新增数据集登记文件, 未修改业务源码 |
| lib/ 目录无变更 | PASS | lib/ 目录无任何文件改动 (仅 engineering/ 新增) |
| Canonical 帧数量合规 | PASS | 7 帧 (每目标 1 帧), 达到任务规范下限 7-14 帧 |
| 目标天区全覆盖 | PASS | 7 个目标天区全覆盖 (Galaxy_Center/LDN43/NGC1727/NGC247/NGC55/NGC83_cluster/Victory_Nebula) |

### 2. 数据完整性
| 检查项 | 结果 | 说明 |
|--------|------|------|
| SHA-256 完整性验证 | PASS | 7/7 帧 manifest SHA-256 vs 重算 SHA-256 一致 |
| FITS 元数据采集 | PASS | 7/7 帧成功读取 width/height/bitpix/exposure/filter/ccd_temp/object/date_obs |
| P02-001 manifest 引用 | PASS | manifest_sha256=2A9BE035... 记录在 canonical_dataset.json _meta |
| P02-001 plate solve 结果复用 | PASS | 7/7 帧找到对应 frame_XXXX.json, 指标完整 |

### 3. 预期数值范围合理性
| 检查项 | 预期 | P02-001 实测 | 结果 |
|--------|------|--------------|------|
| PlateSolve success | true | 7/7 true | PASS |
| PlateSolve RMS | < 1.0" | 0.1174" ~ 0.3975" | PASS (远低于阈值) |
| PlateSolve n_pairs | > 10 | 32 ~ 45 | PASS (远高于阈值) |
| PSF 有效参数 | 非 NaN | 声明性 (stage1 历史 1913/2000 stars) | DECLARED (合理) |
| 测光 n_matched | [0, 5000] | 声明性 (G-002 缺口可能 0 或 1606) | DECLARED (合理) |
| SNR has_snr | 0_or_1 | 声明性 (骨架退化 0, P03-004 修复后 1) | DECLARED (合理) |
| HISS 文件大小 | > 10KB | 声明性 (stage1 输出 11.5MB+) | DECLARED (合理) |

**说明**:
- 实测项 (A/B/C/D) 全部 PASS, 远超任务规范阈值
- 声明性项 (E/F/G/H) 基于 stage1 历史数据声明, 范围合理:
  - PSF: 历史 stage1 检测 1913/2000 stars, 参数非 NaN
  - 测光: G-002 缺口 (n_matched 可能为 0 或 1606), 范围 [0, 5000] 涵盖两种情况
  - SNR: 骨架退化时 has_snr=0, P03-004 SIP 修复后 has_snr=1, 范围 0_or_1 涵盖两种状态
  - HISS: stage1 输出文件 11.5MB+, 远大于 10KB 阈值

### 4. Canonical 帧选择规则
| 目标天区 | 选择规则 | manifest index | 实际帧 | 结果 |
|----------|----------|----------------|--------|------|
| Galaxy_Center | panel1 第一帧 Red | 1 | Galaxy_Center_mosaic1_T4_...@061703-180S-Red.fts | PASS |
| LDN43 | 第一帧 Lum | 158 | LDN43_LRGBH_...@031525-600S-Lum.fts | PASS |
| NGC1727 | 第一帧 (Red) | 200 | NGC1727_RGBHO_T2_...@064517-600S-Red.fts | PASS |
| NGC247 | 第一帧 (Lum) | 264 | NGC247_T2_...@033428-600S-Lum.fts | PASS |
| NGC55 | 第一帧 (Red) | 332 | NGC55_T3_...@074114-600S-Red.fts | PASS |
| NGC83_cluster | 第一帧 (Red) | 411 | NGC90_2025wwk_T3_...@020846-600S-Red.fts | PASS |
| Victory_Nebula | mosaic1 第一帧 Lum | 483 | Victory_Nebula_mosaic1_...@035646-180S-Lum.fts | PASS |

**说明**: 选择规则与任务规范完全一致 (用户指定: Galaxy_Center=panel1 第一帧 Red, Victory_Nebula=mosaic1 第一帧 Lum, 其他=第一帧)

### 5. 交付物完整性
| 交付物 | 路径 | 状态 |
|--------|------|------|
| TASK_REPORT.md | engineering/evidence/P05-001/TASK_REPORT.md | PASS (v1.1 模板格式) |
| TEST_REPORT.md | engineering/evidence/P05-001/TEST_REPORT.md | PASS (v1.1 模板格式) |
| EVIDENCE_INDEX.md | engineering/evidence/P05-001/EVIDENCE_INDEX.md | PASS (v1.1 模板格式) |
| REVIEW_REPORT.md | engineering/evidence/P05-001/REVIEW_REPORT.md | PASS (v1.1 模板格式, 本文件) |
| canonical_dataset.json | engineering/evidence/P05-001/canonical_dataset.json | PASS (结构化: _meta + by_target + frames) |
| canonical_dataset_registry.csv | engineering/contracts/canonical_dataset_registry.csv | PASS (17 列, 7 行 + 表头) |

### 6. 脚本质量
| 检查项 | 结果 | 说明 |
|--------|------|------|
| build_canonical_dataset.py | PASS | 模块化设计 (SELECTORS 字典), 路径计算正确 (PROJECT_ROOT 3 级上溯), 完整日志输出 |
| verify_canonical_dataset.py | PASS | 8 项检查 (A-H), 结构化 JSON 输出, 日志记录完整 |
| 错误处理 | PASS | 文件不存在时返回明确错误, 不静默失败 |
| 日志输出 | PASS | 每步骤含进度日志, 便于分析 |
| 路径计算 | PASS | 已修复 PROJECT_ROOT 路径计算 (从 2 级改为 3 级上溯) |

### 7. 工程契约合规性
| 检查项 | 结果 | 说明 |
|--------|------|------|
| canonical_dataset_registry.csv 位置 | PASS | 位于 engineering/contracts/ (工程契约目录) |
| CSV 列完整 | PASS | 17 列 (dataset_id 到 p02_001_n_pairs) |
| CSV 行数 | PASS | 1 表头 + 7 数据行 = 8 行 |
| 引用 P02-001 基线 | PASS | 含 p02_001_rms_arcsec 与 p02_001_n_pairs 列 |

### 8. 兼容性与回滚
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 业务源码无变更 | PASS | lib/ 目录无改动 |
| 工程文件新增 | PASS | 仅 engineering/evidence/P05-001/ + engineering/contracts/canonical_dataset_registry.csv |
| 回滚方案 | PASS | 删除新增文件即可回滚, 无副作用 |
| 残留风险 | PASS | 无 (纯数据集登记, 不影响运行时行为) |

## 风险评估

### 已知限制 (非缺陷)
1. **声明性预期项 (E/F/G/H)**: PSF/测光/SNR/HISS 的预期范围基于 stage1 历史数据声明, 未在 P05-001 任务中实际运行 stage1 全流程验证。这是任务规范允许的: "本任务为数据集登记, 不要求运行 stage1"。后续 P05-002+ 任务将运行 stage1 完整流程, 届时这些声明性预期将转为实测验证。
2. **Victory_Nebula 实际为 NGC1909 (历史命名混淆)**: 帧文件名 Victory_Nebula_mosaic1_flying_dutchman 是用户历史命名, 与天区实际 NGC1909 不同。本任务沿用 testdata 目录命名, 不影响数据集登记的准确性。
3. **NGC83_cluster 实际为 NGC90 (2025wwk 周期彗星)**: 帧文件名 NGC90_2025wwk_T3_flying_dutchman, 目标天区目录为 NGC83_cluster_T3, 这是用户历史命名差异。本任务沿用目录名 NGC83_cluster, 不影响数据集登记的准确性。

### 残留风险
- **无**: P05-001 为数据集登记任务, 不修改业务源码, 不影响运行时行为。所有新增文件均为工程证据/契约文件, 可独立删除回滚。

## 数据来源可信度

| 数据来源 | 文件 | SHA-256 | 可信度 |
|----------|------|---------|--------|
| P02-001 manifest | engineering/evidence/P02-001/testdata_manifest.json | 2A9BE035... | 高 (P02-001 已全量验证) |
| P02-001 plate solve 结果 | engineering/evidence/P02-001/results/frame_XXXX.json | (710 个文件) | 高 (P02-001 已全量运行 batch_platesolve_test.py) |
| testdata FITS 文件 | testdata/<target>/lights/<panel>/ | (与 manifest SHA-256 一致) | 高 (重算 SHA-256 验证通过) |

## 复核结论

P05-001 任务完成质量良好:

1. **范围合规**: 严格遵循"数据集登记不修改业务源码"约束, lib/ 目录零变更
2. **数据完整**: 7 个 canonical 帧覆盖 7 个目标天区, SHA-256 完整性 7/7 通过
3. **预期合理**: 实测项 (PlateSolve success/RMS/n_pairs) 全部远超阈值, 声明性项 (PSF/测光/SNR/HISS) 基于历史数据合理声明
4. **交付齐全**: 6 项交付物全部完成 (TASK_REPORT/TEST_REPORT/EVIDENCE_INDEX/REVIEW_REPORT/canonical_dataset.json/canonical_dataset_registry.csv)
5. **脚本质量**: build/verify 脚本模块化、错误处理完善、日志完整
6. **工程契约**: canonical_dataset_registry.csv 位于 engineering/contracts/, 17 列完整, 可被后续 P05-002+ 任务引用
7. **兼容性**: 完全兼容, 回滚方案清晰 (删除新增文件即可)

**VERDICT: PASS**
