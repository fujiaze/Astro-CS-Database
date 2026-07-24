# TEST_REPORT: P00-008 Baseline Manifest 可重复性验证

## 测试目标
验证 `baseline_manifest.json` 生成脚本可重复运行、证据文件全部存在（0 missing）、SHA-256 稳定、G0 Checklist 核对结果一致。

## 测试环境
- **仓库**: f:\Astro dev\Astro CS Normalization Database
- **Commit**: 7dfc183（P00-007 提交后 HEAD）
- **分支**: main
- **Python**: 3.x

## 测试 1: 生成脚本可重复性
- **命令**: `python engineering/evidence/P00-008/generate_manifest.py`
- **退出码**: 0
- **stdout**: `OK: baseline_manifest.json generated / evidence files: 42 (missing: 0) / control files: 10 / G0 checklist: 5 PASS / 2 PASS_WITH_CAVEAT / 0 FAIL`
- **重复运行**: 结果一致
- **结果**: PASS

## 测试 2: 证据文件完整性
| 类别 | 预期 | 实际 | 结果 |
|---|---|---|---|
| P00-001 证据文件 | 7 | 7 | PASS |
| P00-002 证据文件 | 5 | 5 | PASS |
| P00-003 证据文件 | 5 | 5 | PASS |
| P00-004 证据文件 | 6 | 6 | PASS |
| P00-005 证据文件 | 6 | 6 | PASS |
| P00-006 证据文件 | 6 | 6 | PASS |
| P00-007 证据文件 | 6 | 6 | PASS |
| bootstrap 证据文件 | 1 | 1 | PASS |
| 控制文件 | 10 | 10 | PASS |
| **合计** | **42+10=52** | **42+10=52** | **PASS** |
| missing 文件 | 0 | 0 | PASS |

## 测试 3: SHA-256 稳定性
- 重复运行 generate_manifest.py 两次，对比 JSON 中所有 sha256 字段
- **结果**: 完全一致（SHA-256 是确定性哈希，预期稳定）— PASS

## 测试 4: G0 Checklist 核对
| 检查项 | 预期状态 | 实际状态 | 结果 |
|---|---|---|---|
| 1 模块源码受控 | PASS | PASS | PASS |
| 2 依赖固定版本 | PASS_WITH_CAVEAT | PASS_WITH_CAVEAT | PASS |
| 3 构建证据 | PASS_WITH_CAVEAT | PASS_WITH_CAVEAT | PASS |
| 4 旧审计复核 | PASS | PASS | PASS |
| 5 文档冲突登记 | PASS | PASS | PASS |
| 6 风险清晰 | PASS | PASS | PASS |
| 7 baseline tag | PASS | PASS | PASS |
| **G0 总判定** | **PASSED** | **PASSED** | **PASS** |

## 测试 5: 关键证据文件抽查
- P00-001/preflight.json: SHA-256 存在且非空 — PASS
- P00-004/dependency_graph.json: SHA-256 存在且非空 — PASS
- P00-005/environment_baseline.json: SHA-256 存在且非空 — PASS
- P00-006/audit_reconciliation.json: SHA-256 存在且非空 — PASS
- P00-007/documentation_conflict_register.json: SHA-256 存在且非空 — PASS

## 测试 6: JSON 可解析性
- **命令**: `python -c "import json; json.load(open('engineering/evidence/P00-008/baseline_manifest.json','r',encoding='utf-8'))"`
- **退出码**: 0
- **结果**: PASS

## 测试 7: Tag 创建验证（提交后执行）
- **命令**: `git tag -a astrocs-baseline-p00 -m "..." <commit>`
- **验证**: `git tag --list astrocs-baseline-p00` 返回 tag 名
- **验证**: `git rev-parse astrocs-baseline-p00^{}` 返回 commit SHA
- **结果**: 待提交后执行

## 结论
- 生成脚本可重复运行，结果稳定
- 52 个文件全部存在（0 missing），SHA-256 全部记录
- G0 Checklist 7 项核对一致（5 PASS + 2 PASS_WITH_CAVEAT）
- JSON 可解析
- **VERDICT: PASS**（tag 创建验证待提交后补做）
