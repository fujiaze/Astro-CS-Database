# 任务：ROOT-003 run/ 临时产物保留策略与清运

状态：`NOT_STARTED`
层：L0　依赖：ROOT-001　文件域互斥组：S3-R

## 目标

给 `run/` 建立保留策略并执行清运：区分「当前控制包仍需」与「历史可清」的临时产物，回收磁盘（现 102G / 634993 文件），同时不破坏正在使用的证据。

## 基线状态（编制时实测）

- `run/` 102G、634993 文件；最大子目录：run/perf-fix 59G、run/release-rescue 29G、run/local 2.7G、run/v6 2.3G、run/BASE-UTIL-001-* 约 3G、run/temp 902M、run/ci 768M。
- `run/` 只 tracked 一个 run/.gitkeep（`git ls-files run` 实测），其余全部 untracked 且被 run/* 忽略——**不影响 git 状态，只影响磁盘、可操作性与根目录体面**。
- 磁盘 /workspace 503G 已用 293G（62%）；testdata/ 30G、GaiaDR3/+GaiaDR3SP/ 105G、build/ 6.0G 为其它占用方。
- ENGINEERING_SPEC §7 规定 run/ 是临时产物/日志的家；CONTROL_PACK_SPEC §9 要求执行日志留存到验收完成。

## 权威依据
- ENGINEERING_SPEC.md §7（run/ 定位：临时产物/日志，gitignore）
- CONTROL_PACK_SPEC.md §6.2（命令日志保存）、§9（汇总与归档）
- ASTROCS_DESIGN.md §6.3（运行产物只落 output_dir）

## 改动范围（文件域）
### 允许改
- `run/**` 的清运与本任务新建的策略文件
- 新建 `工程控制/PROJECT-GOVERNANCE-01/RETENTION.md`

### 禁止改
- 删除任何 tracked 文件
- 删除 `run/PROJECT-GOVERNANCE-01/**`（本控制包的日志）
- 删除最近 7 天内修改过的目录（除非证明其为一次性临时数据且无引用）
- 未经负责人确认删除任何 >1G 的目录树

## 步骤
1. 产出保留策略 `RETENTION.md`：按「控制包/任务」而非「目录名」定义保留规则，给出保留期与证据引用关系。
2. 扫描 reports/、evidence/、docs/、ci/、tests/ 中对 run/ 路径的引用，列出「被引用 → 必须保留」清单（这是删错的唯一防线）。
3. 按策略执行清运：先清 >30 天且无引用的小目录，再逐个大目录核对引用后清运；每个被删目录记录大小、文件数、最后修改时间与「无引用」证据。
4. 清运前后记录 df -h 与 du -sh run，给出回收量。

## 验收门（前台独立复跑）
- [ ] `RETENTION.md` 存在且含保留规则、被引用清单、清理条件、例外登记
- [ ] 被引用清单中的路径**全部仍然存在**（逐条 test -e 结果）
- [ ] `git ls-files run` 仍只有 run/.gitkeep（未误删 tracked）
- [ ] 回收量有前后对比数据，剩余体积与策略一致

## 证据命令
```bash
mkdir -p run/PROJECT-GOVERNANCE-01/ROOT-003/logs
df -h /workspace | tail -1 | tee run/PROJECT-GOVERNANCE-01/ROOT-003/logs/df-before.txt
du -sh run | tee run/PROJECT-GOVERNANCE-01/ROOT-003/logs/run-before.txt
# 清运后再取 after，写入 RETENTION.md
```

## 执行规则（每个任务都适用）

- 开工前按 `AGENTS.md §1` 读完本任务「权威依据」列出的全部条款；未读不开工。
- 只改本文件「改动范围」声明的文件域；**不顺手改无关代码**；不改 `docs/science/**` 公式与 `docs/algorithms/**` 推导。
- 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、默认容差**一律只读**（ENGINEERING_SPEC §3）。
- 工作区在本控制包编制前已有大量预存修改与未跟踪文件（见 BASE-001）：**不得** reset/stash/clean/checkout，**不得**把它们混进本任务的改动。
- SubAgent 零 git 写权限：不 commit、不 push、不建分支；改动留在工作区并交自证材料，由前台按任务原子提交。
- 所有外部命令带 `timeout`，日志落 `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/`（`run/` 已 gitignore，不入库）。
- 报告「完成」必须附：命令、退出码、关键输出片段、产物路径。禁止用「环境问题/工具问题」掩盖失败。
- 发现文档冲突、科学歧义或无法证明的状态：登记 `BLOCKED`（控制包内）或 `UNRESOLVED`（GAP_AUDIT 内），不得自行选口径。

## 交付物

1. 限「改动范围」内的改动；
2. `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/` 下的命令日志与机器输出；
3. 自证摘要（字段：任务ID / 改动清单 / 逐条验收命令与退出码 / 未决项）。
