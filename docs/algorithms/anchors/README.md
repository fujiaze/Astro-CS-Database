# anchors

本目录存放文档—代码锚的合同、机读登记与校验脚本：把文档条款锚定到源码符号与行，供机器逐条核验。

## 职责边界

- 放：锚合同正文、锚登记 JSON、待解锚登记与行锚校验脚本。
- 不放：锚所锚定的算法正文（在上级目录）；文档索引登记（在 docs/DOCUMENT_INDEX.yaml）；其他检查器（在 eng/ci 与 eng/tools）。

## 内容

- `ANCHOR_CONTRACT.md` —— 文档—代码符号/行锚合同与机器校验面说明。
- `anchor_contract.json` —— 锚登记的机读形态。
- `unresolved_registry.json` —— 待解锚登记表。
- `check_doc_line_anchors.py` —— 行锚一致性校验脚本。

## 上游

上游：docs/ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）、§4（normalize）、§5（mosaic）、§6（export）。

科学断言以 docs/science/ 为权威，佐证要求见 docs/DOCUMENT_GOVERNANCE.md §2。
