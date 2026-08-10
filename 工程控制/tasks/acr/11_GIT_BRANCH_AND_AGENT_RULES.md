# Git分支与Agent规则

## 1. 唯一分支

- 使用现有 `feature/astrocompute-runtime`；
- 分支不存在时才从最新 `main` 创建同名分支；
- 禁止新仓库、`-v2`、日期分支和第二套ACR目录；
- 不修改用户其他未提交工作。

## 2. 开始记录

必须记录：remote、main HEAD、feature HEAD、base commit、工作区状态、工具链、GPU/driver和依赖锁。

## 3. 路径边界

建立自动 path guard。除独立构建入口和 ACR目录外，任何算法、Pipeline、Orchestrator或正常CLI路径变化均视为失败。

## 4. 推荐提交序列

1. `docs(acr): freeze profile driven runtime architecture`
2. `refactor(acr): replace kernel routes with hardware profiles`
3. `refactor(acr): connect task descriptors to cost estimator`
4. `bench(acr): expand cpu and gpu hardware profiling`
5. `feat(acr): add profile driven dynamic dispatcher`
6. `feat(acr): add utilization and capacity control`
7. `test(acr): validate real heterogeneous execution and fallbacks`
8. `docs(acr): regenerate evidence from one clean head`

按实际代码拆分原子提交，禁止一个提交同时重构架构、引入依赖和修改测试结论。

## 5. 并行Agent

先冻结公共类型和目录所有权，再并行：

- profile/schema；
- CPU benchmark；
- GPU backend；
- CostEstimator；
- Dispatcher；
- 资源控制；
- 测试/Evidence。

相互依赖模块不得同时修改同一公共头；集成Agent负责合并和统一测试。

## 6. 合并门禁

只有 `CHECKLIST.md` 全部满足、Evidence一致、主线回归和dormant验证通过，才允许合并main。
