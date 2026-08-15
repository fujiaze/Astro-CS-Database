# AstroCS Documentation Standard

权威文档分层（L0-L5）见 docs/README-DOCS.md。

- 每份 science 文档：目的、科学定义、公式、变量/单位、假设、有效域、
  不保证什么、失效条件、系统误差、随机误差、数值精度、参考文献、
  adopted/adapted/not-applicable、SCI/ALG ID。
- 每份 module 文档：职责/非职责、public API、依赖/callers、data contract、
  ownership、threading、errors、config、performance、diagnostics、tests、
  source map。
- 每份 troubleshooting：symptom、likely stage、log/metric/error、
  minimal reproduction、expected invariant、source/doc/test path。
- 历史材料迁 docs/history/，不得作为 current authority。
- 机器一致性：docs_machine_consistency.py 必须 PASS（S8）。
