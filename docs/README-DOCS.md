# AstroCS 文档体系（L0-L5）

```text
L0 项目入口      README.md / docs/DEVELOPER_GUIDE.md /
                 docs/RELEASE_STATUS.md / docs/KNOWN_LIMITATIONS.md /
                 CHANGELOG.md
L1 科学规范      docs/science/*.md（定义/公式/变量/单位/假设/域/误差/ID）
L2 算法规范      docs/algorithms/*.md（输入/输出/前后置/不变量/伪代码/复杂度/oracle）
L3 工程架构      docs/architecture/*.md（架构/模块/依赖/数据流/所有权/线程/错误/缓存/IO/性能/兼容）
L4 实现标准      docs/standards/*.md（编码/注释/数值/API/C ABI/并发/错误/IO/日志/测试/基准/文档/发布）
L5 模块文档      docs/modules/<module>.md（固定模板）
历史             docs/history/（memory.md、Vxx audit 迁入，不作 current authority）
追溯             docs/TRACEABILITY.csv（唯一矩阵）
```

权威链：Scientific Requirement → Scientific Definition → Algorithm Contract →
Architecture → Module/API/Data Contract → Implementation Standard → Source →
Test → Diagnostics → Release Acceptance。

机器一致性：tools/docs_machine_consistency.py（S8 gate）。
