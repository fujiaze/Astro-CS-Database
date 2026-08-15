# AstroCS Test Standard

- 每科学契约 ≥1 test/oracle（property/oracle/MC）。
- 测试必须使用公共生产 API，禁止复制内部实现。
- 确定性：固定 seed；浮点断言用容差 + 说明来源。
- gtest 命名：Suite.Feature；GPU 不可用时 GTEST_SKIP 而非伪通过。
- 覆盖率分母明确：shipping 单元 vs 测试单元不得混算。
- 变更模块必须重跑相关 gate；未变更模块引用 V19 artifact 需 hash 证明。
