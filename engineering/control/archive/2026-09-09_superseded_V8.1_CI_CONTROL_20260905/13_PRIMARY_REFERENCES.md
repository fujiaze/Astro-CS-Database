# 13｜权威参考

## 项目内部优先级

1. Owner 冻结约束与 `AstroCS_ENGINEERING_CONSTRAINTS.md`
2. 当前活动 SCI 合同
3. 由 SCI 推导并已审核的 ALG 合同
4. DATA/API/ABI 合同
5. 源码和测试
6. 历史文档与报告（仅线索，不是权威）

发生冲突时生成追踪 finding 并修正较低层；不得让实现反向篡改科学定义。

## CI / 工具官方资料

- GitHub Actions workflow syntax: https://docs.github.com/actions/using-workflows/workflow-syntax-for-github-actions
- Add self-hosted runners: https://docs.github.com/actions/hosting-your-own-runners/adding-self-hosted-runners
- Self-hosted runner groups/access: https://docs.github.com/actions/hosting-your-own-runners/managing-self-hosted-runners/managing-access-to-self-hosted-runners-using-groups
- Secure use reference: https://docs.github.com/actions/reference/security/secure-use
- Self-hosted runner queue/update limits: https://docs.github.com/actions/reference/runners/self-hosted-runners
- GitHub-hosted runner specifications: https://docs.github.com/actions/reference/runners/github-hosted-runners
- GitHub Actions public-repository billing/usage: https://docs.github.com/actions/concepts/billing-and-usage
- GitHub Actions limits: https://docs.github.com/actions/reference/limits
- Artifact storage/retention: https://docs.github.com/actions/tutorials/store-and-share-data
- Dependency caching: https://docs.github.com/actions/reference/workflows-and-actions/dependency-caching
- CMake presets: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
- CTest: https://cmake.org/cmake/help/latest/manual/ctest.1.html
- MSVC binary compatibility: https://learn.microsoft.com/cpp/porting/binary-compat-2015-2017

引用论文或标准必须进入项目 references lock，包含 DOI/版本/访问日期及其支撑的 SCI/ALG ID；CI 接入任务不得临时扩张科学内容。
