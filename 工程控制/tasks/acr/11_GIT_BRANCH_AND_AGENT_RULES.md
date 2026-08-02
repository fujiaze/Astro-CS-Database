# Git分支、合并与Agent规则

## 1. 唯一分支

继续使用`feature/astrocompute-runtime`。只有不存在时才从最新`main`创建。禁止：

- 新仓库；
- `feature/astrocompute-runtime-v2`；
- 用复制目录重写第二套ACR；
- 为控制包修订创建发布tag或版本号。

在现有代码上增量修改，保留可用实现和历史。

## 2. 开始记录

记录cwd、分支、HEAD、remote、工作区状态、main、base commit、现有Evidence状态和`astro_toolkit.py`自检。用户未提交改动不得清理、stash、覆盖或混入；必要时使用worktree。

## 3. 禁止修改路径

现有算法、Stage1/2、PipelineFrame语义、Orchestrator行为、算法调用点和OpenMP均禁止修改。允许新ACR目录、独立工具/测试/文档及最小feature-gated构建入口。每提交前运行path guard。

## 4. 建议纠正提交

1. `docs(acr): replace fixed-share routing with hardware profiling`
2. `refactor(acr): add task traits and profile-based cost model`
3. `bench(acr): add arithmetic memory reduction and convolution profiles`
4. `feat(acr): connect public API to dispatcher and backends`
5. `feat(acr): add dynamic heterogeneous work queues`
6. `feat(acr): enforce utilization and memory budgets`
7. `test(acr): validate real CPU GPU mixed execution and fallbacks`
8. `docs(acr): regenerate consistent evidence from one head`

不得使用业务算法重构提交。

## 5. 与main同步

开发期间`git fetch origin`后在feature内合并最新`origin/main`。共享分支不强制rebase/force push。冲突只在允许范围内人工解决，禁止批量`ours/theirs`掩盖问题。

## 6. 合并门禁

全部Phase、真实GPU、经典实验、CPU-only、主线回归、许可证、依赖锁、path guard和统一Evidence通过后：

```bash
git switch main
git pull --ff-only
git merge --no-ff feature/astrocompute-runtime   -m "merge: add dormant AstroCompute Runtime foundation"
```

合并后再次完整测试。任何冲突不明、回归、GPU SDK绑死、普通启动副作用、算法修改或证据不一致都不得强行合并。

## 7. 合并后

ACR保持备用；feature不继续业务集成。未来算法改造从最新main另开独立分支。发现底层缺陷使用普通bugfix分支，不创建“新版ACR”平行线。
