# anchors

本目录存放文档—代码锚的合同、机读登记与校验脚本：把文档条款锚定到源码符号与行，供机器逐条核验。

## 职责边界

- 放：锚合同正文、锚登记 JSON、待解锚登记与行锚校验脚本。
- 不放：锚所锚定的算法正文（在上级目录）；文档索引登记（在 docs/DOCUMENT_INDEX.yaml）；其他检查器（在 eng/ci 与 eng/tools）。

## 内容

- `ANCHOR_CONTRACT.md` —— 文档—代码符号/行锚合同与机器校验面说明。**本目录唯一迁入件**。
- `docs/algorithms/anchors/anchor_contract.json` —— 锚登记的机读形态。**未随本目录迁入**，仍在旧落点（两读法核实：`docs/detail/anchors/` 下无此文件）。
- `docs/algorithms/anchors/unresolved_registry.json` —— 待解锚登记表。**未随本目录迁入**，仍在旧落点。
- `docs/algorithms/anchors/check_doc_line_anchors.py` —— 行锚一致性校验脚本。**未随本目录迁入**，仍在旧落点。

本目录共 2 篇迁入件（本文 + `ANCHOR_CONTRACT.md`）。迁移是否补齐另 3 件属待裁决项，裁决记录登记在集根 README 的待裁决清单（第 6 条）。本目录不反向指回集根，保持「集入口 → 各篇」的单向导航。

## 上游

上游：docs/ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）、§4（normalize）、§5（mosaic）、§6（export）。

科学断言以 docs/science/ 为权威，佐证要求见 docs/engineering/DOCUMENT_GOVERNANCE.md §2。
