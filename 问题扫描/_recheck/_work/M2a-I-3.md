## M2a-I-3 数据接口合同把 ARCHIVED_NON_NORMATIVE 的归档约束文件列为现行上游权威

- 类别: I_DOC_HYGIENE
- 优先级: P2
- 来源: L10-023
- 位置: docs/interfaces/data/DATA-003_*::约束来源行；docs/interfaces/data/DATA-002_*、DATA-004_*（同族引用）；AGENTS.md::顶部约束表；ASTROCS_PROJECT_CONSTITUTION.md::§1.1
- 证据摘录（逐字，复核时点现文）:
  > （DATA-003 头部）约束来源: AstroCS_ENGINEERING_CONSTRAINTS.md A.3/A.4/A.6 …
  > （AGENTS.md）历史约束（ARCHIVED_NON_NORMATIVE）：根 AstroCS_ENGINEERING_CONSTRAINTS.md —— 已被宪章替代，仅作历史追溯；与宪章冲突处一律以宪章为准
- 权威依据: 宪章 §1.1（权威分层，归档件非规范）、§12.3-2（禁以历史文档作为当前依据）；AGENTS.md 约束表
- 问题说明: 三份现行接口合同仍以归档件的附录条款作为「约束来源」引用，等于把非规范文档当规范上游；若该归档件与宪章或现行 DATA 合同冲突，读者会按错的层级取证。
- 影响: 权威分层被合同自身削弱；不影响当前数值。
- 建议处置: 三份合同的「约束来源」改指 ASTROCS_PROJECT_CONSTITUTION.md 相应条款 + DATA_SEMANTICS/DATA_ARTIFACTS 现行节；确需保留历史出处的，显式标注 ARCHIVED_NON_NORMATIVE 与「仅溯源」。
- 置信度: 高
- related: M2a-E-1、M2a-B-2
