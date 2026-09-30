# Astro Celestial Sphere Database（ACSD） Documentation Standard

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

权威链（`ASTROCS_DESIGN.md` §0，唯一）：`ASTROCS_DESIGN.md` → `AGENTS.md` →
`docs/engineering/` → `docs/ASTROCS_DESIGN.md` → `docs/engineering/` → `docs/detail/`；
另立 `docs/science/`（公式权威）、`docs/science/algorithms/`（推导权威）、
`docs/detail/UNIFIED_MODEL.md`（数据对象与三类配置分离）。与其他文档冲突时以
`ASTROCS_DESIGN.md` 为准。

~~~text
L0 最高设计/纪律   docs/ASTROCS_DESIGN.md / AGENTS.md / docs/ASTROCS_DESIGN.md
L0 项目入口        README.md / docs/engineering/RELEASE_STATUS.md / docs/KNOWN_LIMITATIONS.md /
                   docs/engineering/DEVELOPER_GUIDE.md 
L1 科学规范        docs/science/*.md（定义/公式/变量/单位/假设/域/误差/ID；权威）
L2 算法规范        docs/science/algorithms/*.md（输入/输出/前后置/不变量/伪代码/复杂度/oracle；权威）
L3 数据与设计      docs/detail/**（详细设计正本）/ docs/contracts/*.md / docs/architecture/*.md
L4 CI 与插件       docs/engineering/**（怎么查、哪些是门禁、正本与模块规范）
L5 模块文档        docs/detail/registry/**（module 页 + MODULE_MAP.yaml + registry/**）
历史（非权威）      docs/**/v6/**（V6 产品族冻结/设计档案，仍在活动索引）；其余历史由 git 承载
~~~

- 活动/归档边界与机器索引：`docs/DOCUMENT_INDEX.yaml` + `python3 eng/tools/doccheck/check_doc_index.py --strict`。
- 机器一致性检查器以 门禁注册面（G08-10 重建） 为唯一注册表（`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md §12`）。

- 每份 science 文档：目的、科学定义、公式、变量/单位、假设、有效域、
  不保证什么、失效条件、系统误差、随机误差、数值精度、参考文献、
  adopted/adapted/not-applicable、SCI/ALG ID。
- 每份 module 文档：职责/非职责、public API、依赖/callers、data contract、
  ownership、threading、errors、config、performance、diagnostics、tests、
  source map。
- 每份 troubleshooting：symptom、likely stage、log/metric/error、
  minimal reproduction、expected invariant、source/doc/test path。
- 正式文档集只保留现行设计；current authority 只取正式文档集内的现行条目。
- 机器一致性：docs_machine_consistency.py 必须 PASS（S8）。

## 判据的名词口径与上位冻结语句的同向性

下层正本与验收判据引用上位冻结语句里的名词时，只取该冻结语句之义，**不复述、不改写、不换词**。

同向性规则：

1. **口径只在一处**：名词的定义在上位冻结语句出现处；下层正文引用它时给条款号，不复述定义；
2. **反义即缺陷**：下层把该名词取成反义含义（把表达层量当数据形态、把绝对量当相对量、把「不可辨识」读成「不许出现数值」等）时，以上位冻结语句为准并立即订正下层；
3. **逐条比对**：验收判据与上位冻结语句的名词口径逐条比对，出现反义即判红；比对方向固定为「下层判据 → 上位冻结语句」，不自建上位口径；
4. **执行面**：该比对属文档域，按 `DOCUMENT_GOVERNANCE.md` §6 的口径由对抗性审查逐条覆盖；文档域不设机器门（`VALIDATION_EVIDENCE_STANDARD.md` §12.5），若同一比对在别面具备实质判据则按该面登记；自检必须含「注入反义表述 ⇒ 判红」的负例，只有正例自检不足以证明判据有牙；
5. **澄清用正向陈述**：需要消歧时直接写现行口径的正向句子，不写成对另一种说法的纠正（写法纪律见 `DOCUMENT_GOVERNANCE.md` §5）。
