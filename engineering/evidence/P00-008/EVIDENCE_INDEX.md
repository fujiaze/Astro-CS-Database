# EVIDENCE_INDEX: P00-008

## 证据目录
`engineering/evidence/P00-008/`

## 证据清单

| 文件 | SHA-256 | 说明 |
|---|---|---|
| generate_manifest.py | 01c48c1f76641fa3914a4d5c6d168de7445ff70ec6b721a86fafe90fd12777b2 | manifest 生成脚本（采集 P00-001~P00-007 证据 SHA-256） |
| baseline_manifest.json | 5b4e07fd7788243e5a1ec432228359fe78356b1e9acfe1440cb246b239f55f3a | 机器可读 G0 证据清单（42 任务证据 + 10 控制文件 + G0 checklist） |
| baseline_manifest.md | 31ca1e3896a164814c18ded90a490008b9cdab163a91dedb5b6205970889ddf6 | 人类可读 G0 报告 |
| TASK_REPORT.md | 7bdb47910b8e10c8699389aee1a11c305012aa050e2414bcc765413450214e31 | 任务执行报告 |
| TEST_REPORT.md | 8e5ead523afbfde044f8c73c2d03853e4bcb2e4d39fa41f70fd8f60e0174d146 | 可重复性测试报告 |
| EVIDENCE_INDEX.md | df12ad025b075c4470d78c0b4e65fad8328b7e0866cfe959343932d9f7384d60 | 本文件 |
| REVIEW_REPORT.md | — | 独立复核报告（待生成） |

## 关键事实证据

### F-001: P00 全阶段 7 个任务全部 DONE
- 证据: MASTER_TASK_REGISTER.csv 中 P00-001 ~ P00-007 状态均为 DONE
- P00-008 本任务完成后也将置为 DONE

### F-002: 42 个证据文件齐全（0 missing）
- 证据: baseline_manifest.json `evidence_files` 字段，42 个文件全部有 SHA-256
- 覆盖: P00-001(7) + P00-002(5) + P00-003(5) + P00-004(6) + P00-005(6) + P00-006(6) + P00-007(6) + bootstrap(1)

### F-003: G0 Checklist 7 项全部 PASS 或 PASS_WITH_CAVEAT
- 5 PASS: 模块源码受控 / 旧审计复核 / 文档冲突登记 / 风险清晰 / baseline tag
- 2 PASS_WITH_CAVEAT: 依赖固定版本（→P01-002）/ 构建证据（→P01-007）
- 0 FAIL

### F-004: 10 项风险已识别
- 证据: RISK_REGISTER.csv 10 项风险全部 OPEN
- G0 仅要求风险已识别，不要求修复
- 10 项风险均有 mitigation_task 映射（P00-002/P00-003/P02-003/P03-002/P03-005/P02-006/P02-001/P05-004/P00-006/P01-007）

### F-005: 4 项 ADR 已登记
- ADR-001 Drizzle/Stack 源码纳管 — PENDING
- ADR-002 PipelineFrame 唯一所有者 — PENDING
- ADR-003 Stage 2 节点模型 — PENDING
- ADR-004 根级构建策略 — PENDING
- G0 不要求 ADR 完成

### F-006: Baseline tag 创建
- Tag 名: `astrocs-baseline-p00`
- 类型: annotated tag
- 指向: P00-008 提交后的 HEAD commit
- 创建命令: `git tag -a astrocs-baseline-p00 -m "AstroCS G0 baseline - P00 complete" <commit>`

## 命令日志
- `python engineering/evidence/P00-008/generate_manifest.py` — 退出码 0，42 证据文件 + 10 控制文件
- `git tag -a astrocs-baseline-p00 -m "..." <commit>` — 待提交后执行
