# main-only Git、审阅胶囊与审核包

## 1. 原子提交

- 只用 `main`，开始 Task 前 `fetch` 并确认 `HEAD=main=origin/main`。
- 一个 Task 一个目的；先测试，再只 stage 白名单文件，检查 staged diff，commit，push。
- push 后记录 commit SHA、parent、文件、测试、push 结果到 `COMMITS.csv`。
- 禁止 force push、reset --hard、改写历史、混入外部修改。
- push 冲突时停止该 Task，fetch 后明确归属；不得自动覆盖。

文档、schema、实现、测试若共同构成一个不可分割合同，可以在同一 Task commit；否则按 ledger 拆分。

## 2. 每个 commit 的 review capsule

文件名：`capsules/<task_id>_<commit12>.zip`。必须包含：

- `CAPSULE.json`：task/commit/parent/时间/主机/结论；
- `commit.patch` 和 `changed_files.txt`；
- 所有变更文件的完整最新版本，不只 diff；
- Task 直接相关测试、摘要资源报告、findings；
- `SHA256SUMS`。

SCI/ALG/ARCH/API commit 还必须包含相应层级的全部当前正式文档、traceability 子集、引用清单、直接相关 headers/source/tests。这样外部审核者能重做推导而不依赖 Agent 摘要。

二进制、真实数据、完整 build、HiPS、raw timeseries、大日志禁止进入 capsule；以 `LARGE_ARTIFACTS.csv` 登记路径/hash/保留期。

## 3. 审阅节奏

- Agent push 后生成 capsule，将 Task 标 `REVIEW_PENDING`，继续独立 Task。
- 外部审核发现 P0/P1，后续创建独立修复 Task/commit，不修改旧 capsule。
- 文档/API 审核 100%；核心代码/Oracle/资源调度 100%；其他文件在最终 SHA 上按 `sha256(final_sha + relative_path)` 排序取前 20%。

## 4. 最终审核包白名单

只允许：本控制包副本、最终 Summary/ledger/tables、文档快照、关键源码快照、patch/capsule 索引、小型测试报告、降采样资源数据、HiPS 审核截图/小指标、manifest/hash。

禁止：`.git`、历史包、testdata、FITS/XISF/HISS、完整 HiPS、build/cache/tmp、exe/dll/so/lib/obj/pdb、core dump、单文件 >5MiB、总包 >25MiB。发布二进制单独交付，不塞审核包。

`scripts/package_final.py` 只按白名单复制；`validate_final_package.py` 必须通过。所有大型证据通过路径+hash引用。

## 5. 最终 verdict

只有以下条件全部成立才可生成：`AWAITING_EXTERNAL_RELEASE_REVIEW`：

- ledger 无 FAIL/BLOCKED/NOT_STARTED/IN_PROGRESS/REVIEW_PENDING；
- P0/P1=0，P2 有明确 disposition；
- Linux/Windows build/test/package PASS；
- Windows 32R 当前候选唯一一次成功，资源与接缝门禁 PASS；
- traceability 全闭环；
- 全部 commit 已 push，审核包 hash 校验通过。

