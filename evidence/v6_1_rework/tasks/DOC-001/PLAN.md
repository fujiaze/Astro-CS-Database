# DOC-001: 重建合同 front matter 与完整图

任务 ID: DOC-001
Gate: G7
依赖: P1-004; P2-007; P3-006
平台: Linux
变更类别: documentation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` DOC-001：

> 每份 L1 文档有 front matter：id/version/status/owner/upstream/downstream/
> source_commit。每个 production MOD 必须存在 SCI→ALG→DATA→ARCH→API→MOD→
> source→TEST 完整可达链；双向引用一致；ACTIVE 不能依赖 DRAFT/OBSOLETE/
> CONFLICT。禁止索引存在但文件不打包。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 每份 L1 合同 front matter | docs/contracts/*.md 均有 状态/版本/owner/上游/下游(front matter 结构) | c01 |
| SCI→ALG→DATA→ARCH→API→MOD→source→TEST 完整链 | check_contract_graph PASS(36 合同, 双向一致) + check_prod_reachability PASS(18 compile entries, acr=0) | c01/c02 |
| ACTIVE 不依赖 DRAFT/OBSOLETE/CONFLICT | 合同图状态机检查通过 | c01 |
| 禁止索引存在但文件不打包 | gen_source_index_v61 PASS(79 targets, missing=0, vendored=239) | c03 |
| 注释卫生(无审计故事/任务号) | check_comment_hygiene PASS(501 扫描, 0 违规) | c04 |

## 检查命令

- c01: check_contract_graph.py → CONTRACT_GRAPH_PASS contracts=36
- c02: check_prod_reachability.py → REACH_PASS (18 entries, acr=0)
- c03: gen_source_index_v61.py → SOURCE_INDEX_PASS targets=79 missing=0
- c04: check_comment_hygiene.py → 501 scanned, 0 violations

## 测试结果

- 4 项检查全 PASS(合同图/可达性/源索引/注释卫生)

## 说明

- 文档基础在 R0 系列已建立; 本任务复验当前 SHA(含 P3-001..006 全部 G6 变更)的一致性。
- 无生产代码变更, 仅证据收集与台账登记。
