> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

> 由 `reports/v6/contract-review/tools/gen_freeze.py` 机械渲染，与 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 同源；语义源 = `reports/v6/science-adjudication/adjudications.json` + W3 各规格；基线 HEAD = `ebefe00d3cb9018d61b7b3e8d3d7694191c1f333`。

## 0. 权威与用途

- 文档 ID：`CONTRACT-FREEZE-001-FROZEN-ALG`；把 SCI-ADJ-001 语义冻结的 W3 数值阈值收敛为可实施合同。
- 上游：`docs/algorithms/v6/{phase1,phase2-point,phase2-psfsw,phase2-surface,phase3}/**`、`docs/validation/v6/**`、`reports/v6/qa-design/**`。
- 每条给唯一数值或明确 OPEN+pending owner；未签字项标 PENDING_OWNER_SIGNOFF，不得写成已冻结。
- 本目录不修改任何既有 FZ-* 数值/容差；继承项（ALG-REJ-001/UPM_SOLVER）标 FROZEN(继承)。

## 1. 文件索引

| 文件 | 内容 |
|---|---|
| `01_NUMERIC_THRESHOLD_FREEZE.md` | 全部数值阈值条款（值/单位/状态/owner/source binding/负向 mutation） |
| `02_GATE_AND_MUTATION_FREEZE.md` | 验证门/负向 mutation 冻结索引与 fail-closed 矩阵 |
