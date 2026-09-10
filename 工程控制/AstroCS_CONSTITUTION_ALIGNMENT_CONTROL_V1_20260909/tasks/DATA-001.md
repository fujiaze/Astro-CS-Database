# DATA-001｜冻结Phase2与Phase3不确定度产品合同

## 目标
负责人冻结 Phase2 variance/ivar/rejection/provenance 与 Phase3 uncertainty 语义、公式、invalid policy；实现不得先行。

## 依赖
`GOV-001`。执行时 BASE_SHA 必须为最新 main，若依赖提交后基线变化须重取。

## 写入白名单
- `docs/science/`
- `docs/algorithms/`
- `docs/contracts/`
- `contracts/`

## 非目标与禁令
- 不顺手修复域外问题；发现后建立 finding。
- 不修改/放宽科学公式、默认容差或负责人裁决。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone。
- SubAgent 不 git add/commit/push，不修改 TASK_LEDGER。

## 必须动作
1. 读取冻结宪章、本任务相关 SCI/ALG/DATA/ARCH/API、模块 README/module.yaml/测试。
2. 先建立最小复现或失败测试，再做最小改动；科学与架构变更分提交。
3. 运行任务特化测试、受影响模块测试、静态追踪与负向故障注入。
4. 保存命令、timeout、cwd、起止时间、rc、stdout/stderr、source SHA；heavy 同 run ID 监控。
5. 返回 TASK_RESULT 与 changed-files 清单；未运行不得写 PASS。

## 验收
- write_scope 零越界，预存 dirty 不被覆盖；所有新增测试能在故障注入时失败。
- 任务目标的正向、边界、错误、确定性、1/N worker（适用时）全部通过。
- 当前 SHA 的文档/接口/符号/测试/证据追踪无断链。
- 前台复跑后才可 PASS；一个任务一个原子 commit 并 push main。

## 返回证据
提交 schema 要求的 scope、acceptance、provenance 三项检查和必要 artifacts；如有外部阻断，给出可复现条件，不得伪造 PASS。
