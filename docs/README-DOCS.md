# AstroCS 文档体系（文档集分层 + 机器索引）

> 上游：ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）、§0.3（文档写法）

机器索引（活动/归档边界与校验）：`docs/DOCUMENT_INDEX.yaml` +
`python3 eng/tools/doccheck/check_doc_index.py --strict`。

权威链（`ASTROCS_DESIGN.md` §0，唯一）：`ASTROCS_DESIGN.md` → `AGENTS.md` →
`ENGINEERING_SPEC.md` → `CONTROL_PACK_SPEC.md` → `docs/ci/` → `docs/plugins/`（23 篇）；
另立 `docs/science/`（公式权威）、`docs/algorithms/`（推导权威）、
`docs/design/UNIFIED_MODEL.md`（数据对象与三类配置分离）。与其他文档冲突时以
`ASTROCS_DESIGN.md` 为准。

```text
L0 最高设计/纪律   ASTROCS_DESIGN.md / AGENTS.md / ENGINEERING_SPEC.md / CONTROL_PACK_SPEC.md
L0 项目入口        README.md / docs/RELEASE_STATUS.md / docs/KNOWN_LIMITATIONS.md /
                   docs/DEVELOPER_GUIDE.md / HANDOVER.md / memory.md
L1 科学规范        docs/science/*.md（定义/公式/变量/单位/假设/域/误差/ID；权威）
L2 算法规范        docs/algorithms/*.md（输入/输出/前后置/不变量/伪代码/复杂度/oracle；权威）
L3 数据与设计      docs/design/UNIFIED_MODEL.md / docs/contracts/*.md / docs/interfaces/** /
                   docs/api/*.md（CLI 协议与 Phase API）/ docs/architecture/*.md
L4 CI 与插件       docs/ci/**（怎么查、哪些是门禁）/ docs/plugins/**（23 篇模块规范）
L5 模块文档        docs/modules/**（module 页 + MODULE_MAP.yaml + registry/**）
历史（非权威）      docs/**/v6/**（V6 产品族冻结/设计档案，仍在活动索引）、
                   docs/API_REFERENCE.md、docs/ARCHITECTURE.md
                   （标 ARCHIVED_NON_NORMATIVE）；原归档树与旧轮次评审副本
                   已由 CLEAN-402 删除，历史由 git 承载
```

- 任务状态与发布结论只写在 `工程控制/PROJECT-GOVERNANCE-01/`（控制包）与
  `docs/owner/RELEASE_STATUS.md`；状态词唯一口径 = `ASTROCS_DESIGN.md` §12.5（状态阶梯）。
- 已删除且不得再作现状引用的旧体系：根 `ASTROCS_PROJECT_CONSTITUTION.md`、
  旧工程约束、旧 `CHANGELOG.md`/`REVIEW.md`/`HANDOVER.md`、`设计大纲/`、
`evidence/**`、旧历史控制包归档树与旧 `工程控制/AstroCS_*` 控制包（均已删除）
  （ROOT-007）。
- 机器一致性：`eng/tools/docs_machine_consistency.py` 等检查器以 `eng/ci/checks.json` 为唯一注册表
  （`docs/ci/01_CHECKS.md`）。
