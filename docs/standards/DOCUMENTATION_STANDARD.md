# AstroCS Documentation Standard

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

权威链（`ASTROCS_DESIGN.md` §0，唯一）：`ASTROCS_DESIGN.md` → `AGENTS.md` →
`ENGINEERING_SPEC.md` → `CONTROL_PACK_SPEC.md` → `docs/ci/` → `docs/plugins/`；
另立 `docs/science/`（公式权威）、`docs/algorithms/`（推导权威）、
`docs/design/UNIFIED_MODEL.md`（数据对象与三类配置分离）。与其他文档冲突时以
`ASTROCS_DESIGN.md` 为准。

~~~text
L0 最高设计/纪律   ASTROCS_DESIGN.md / AGENTS.md / ENGINEERING_SPEC.md / CONTROL_PACK_SPEC.md
L0 项目入口        README.md / docs/owner/RELEASE_STATUS.md / docs/KNOWN_LIMITATIONS.md /
                   docs/DEVELOPER_GUIDE.md / memory.md
L1 科学规范        docs/science/*.md（定义/公式/变量/单位/假设/域/误差/ID；权威）
L2 算法规范        docs/algorithms/*.md（输入/输出/前后置/不变量/伪代码/复杂度/oracle；权威）
L3 数据与设计      docs/design/UNIFIED_MODEL.md / docs/contracts/*.md / docs/interfaces/** /
                   docs/api/*.md（CLI 协议与 Phase API）/ docs/architecture/*.md
L4 CI 与插件       docs/ci/**（怎么查、哪些是门禁）/ docs/plugins/**（模块规范）
L5 模块文档        docs/modules/**（module 页 + MODULE_MAP.yaml + registry/**）
历史（非权威）      docs/**/v6/**（V6 产品族冻结/设计档案，仍在活动索引）；其余历史由 git 承载
~~~

- 活动/归档边界与机器索引：`docs/DOCUMENT_INDEX.yaml` + `python3 eng/tools/doccheck/check_doc_index.py --strict`。
- 机器一致性检查器以 `eng/ci/checks.json` 为唯一注册表（`docs/ci/01_CHECKS.md`）。

- 每份 science 文档：目的、科学定义、公式、变量/单位、假设、有效域、
  不保证什么、失效条件、系统误差、随机误差、数值精度、参考文献、
  adopted/adapted/not-applicable、SCI/ALG ID。
- 每份 module 文档：职责/非职责、public API、依赖/callers、data contract、
  ownership、threading、errors、config、performance、diagnostics、tests、
  source map。
- 每份 troubleshooting：symptom、likely stage、log/metric/error、
  minimal reproduction、expected invariant、source/doc/test path。
- 正式文档集只保留现行设计；历史材料不入库，也不得作为 current authority。
- 机器一致性：docs_machine_consistency.py 必须 PASS（S8）。
