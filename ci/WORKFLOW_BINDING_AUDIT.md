# CI-001B｜workflow ↔ 注册表联动核死：逐 step 审计表

> 任务：CI-001B（run Rmtxvlrtfa66eb7 rev14）｜BASE_SHA=778fe98e（执行时最新 main）
> 机器核死入口：python3 ci/validate_workflow_binding.py（exit 0 PASS / 1 FAIL）
> 运行期派发：python3 ci/wf_step.py --step <STEP_ID>（声明唯一事实源：ci/workflow_binding.json）

## 0. 结论

1. **workflow 只调用 ci/run.py（不复制业务命令）**：两份 workflow 的 run: 体只剩四类
   调用——ci/bootstrap.py / ci/select_profile.py（CI 入口）、ci/run.py（注册表执行，
   每份 workflow 恰一条）、ci/wf_step.py（声明步派发：诊断 + 注册表侧门）。原先
   workflow 内联的 cmake 构建树、fixture 准备、候选 zip 校验、内联诊断 python 体
   全部迁入 ci/ 并由注册表检查项执行或由声明派发。
2. **候选缺失必须 FAIL（STD-F8 残留项收口）**：旧的 Test-Path + ::warning:: 跳过路径
   已删除；候选门 = 不可豁免注册表检查项 WIN-CANDIDATE-VALIDATE + 声明侧
   require_outputs=WIN-PACKAGE-CANDIDATE.outputs 的 fail-closed 前置判定。
3. **actions SHA lock 维持并复验**：ci/actions.lock.json 4 条；offline 结构 PASS +
   online tag→SHA 逐条 PASS（ci/verify_actions_lock.py）。

## 1. ci-linux.yml 逐 step 审计

| # | step | 角色 | 注册表/声明绑定 | 业务命令体 |
|---|---|---|---|---|
| 1 | Checkout exact SHA | pinned-action | actions/checkout@3d3c42e5（actions.lock） | 无 |
| 2 | Bootstrap locked toolchain | ci-entrypoint | ci/bootstrap.py（工具链门） | ci/ 入口 |
| 3 | Select profile | ci-entrypoint | ci/select_profile.py；输出 profile ∈ {linux-main, linux-deep} ⊆ 注册表 profile | ci/ 入口 |
| 4 | Install linux-main python deps | host-provisioning | 纯 apt 安装（无仓库路径/构建命令） | 仅外部工具 |
| 5 | Install deep coverage tools | host-provisioning | 纯 apt 安装（llvm-18/pytest/…） | 仅外部工具 |
| 6 | Run registered checks | **registry-runner** | `python3 ci/run.py --profile "${{ steps.profile.outputs.profile }}"`；profile 逐值在注册表内 | 注册表（90+4 项） |
| 7 | Collect bootstrap diagnostics | infra-step | 声明 LINUX-BOOTSTRAP-DIAG → ci/steps/collect_bootstrap_diag.py | ci/ 声明体 |
| 8 | Upload small evidence | artifact-upload | artifacts/ci/（run.py 证据目录），if: always()，if-no-files-found: error | 无 |

原第 6、7 步（fixtures / 构建根图）已删除，收编为注册表检查项（见 §3）。

## 2. ci-windows.yml 逐 step 审计

| # | step | 角色 | 注册表/声明绑定 | 业务命令体 |
|---|---|---|---|---|
| 1 | Checkout exact SHA | pinned-action | actions/checkout@3d3c42e5 | 无 |
| 2 | Provide zlib (vcpkg) and PyYAML | host-provisioning | vcpkg/pip + PATH 注入（无仓库路径/构建命令） | 仅外部工具 |
| 3 | Bootstrap locked toolchain | ci-entrypoint | ci/bootstrap.py | ci/ 入口 |
| 4 | Run MSVC tests and package candidate | **registry-runner** | `python ci/run.py --profile windows-main` | 注册表（63+3 项） |
| 5 | Upload candidate | artifact-upload | artifacts/candidate/，if: success()，if-no-files-found: error | 无 |
| 6 | Collect bootstrap diagnostics | infra-step | 声明 WINDOWS-BOOTSTRAP-DIAG → ci/steps/collect_bootstrap_diag.py | ci/ 声明体 |
| 7 | Upload public CI evidence | artifact-upload | artifacts/ci/ + run/ci/win-*.json，if: always() | 无 |

原「Validate candidate」步（Test-Path + ::warning:: 跳过）已删除，收编为注册表检查项
WIN-CANDIDATE-VALIDATE（见 §3）。Windows concurrency.group 追加 event_name：手动触发
（G-CI 核验）不再与 push run 同组，避免 pending 态被后继 push 取代。

## 3. 新增注册表检查项（ci/checks.json 99 → 104）

| id | profile | 命令 | waivable | 说明 |
|---|---|---|---|---|
| LINUX-MAIN-FIXTURES | linux-main | `python3 ci/wf_step.py --step LINUX-PREPARE-FIXTURES` | false | UT-BACKEND/UT-CLI 前置 fixture（原 workflow 步） |
| LINUX-MAIN-BUILD-TREE | linux-main | `python3 ci/wf_step.py --step LINUX-BUILD-ROOT-GRAPH` | false | UT-BACKEND/UT-CLI 构建树前置（原 workflow 步） |
| WIN-CANDIDATE-VALIDATE | windows-main | `python3 ci/wf_step.py --step WINDOWS-VALIDATE-CANDIDATE` | false | 候选 zip 结构/哈希/manifest 门（原 workflow 步） |
| WORKFLOW-REGISTRY-BINDING | fast/linux-main/windows-main | `python3 ci/validate_workflow_binding.py ...` | false | 本审计的机器核死校验器 |
| CI-BINDING-TESTS | fast/linux-main/windows-main | `python3 -B -m unittest ... -p test_ci001b_*.py` | false | 负向注入回归（33 用例） |

排位约束：LINUX-MAIN-* 两项排在 UT-API 之前（是 UT-BACKEND/UT-CLI 的前置）；
WIN-CANDIDATE-VALIDATE 排在 WIN-PACKAGE-CANDIDATE 之后（消费其 outputs 契约）。

## 4. fail-closed 判定链（候选缺失场景）

1. ci/checks.json WIN-PACKAGE-CANDIDATE（waivable=false）outputs 登记
   `artifacts/candidate/AstroCS-candidate.zip`；run.py 对 exit 0 但产物缺失记
   V_MISSING_OUTPUT（FAIL）。
2. ci/workflow_binding.json 的 WINDOWS-VALIDATE-CANDIDATE 声明
   require_outputs=[同 zip]、binds_check=WIN-PACKAGE-CANDIDATE、fail_closed=true；
   ci/wf_step.py 执行前先核产物存在 → 缺失即 ::error:: + exit 1（绝不 warning 跳过）。
3. 该声明由检查项 WIN-CANDIDATE-VALIDATE（waivable=false）经 ci/run.py 执行；
   校验器要求 command 逐字等于派发调用、outputs 必须含派发记录
   `run/ci/wf_step/<STEP_ID>.json`，漂移即 FAIL。

## 5. Linux 挂起/取代语义核查（前台指定）

机器证据（GitHub API，run/ci/ci001b/runs_probe.json）：
- d3097319 的 Linux run 34674156865 = **cancelled**（在 pending 态被后继 f8778bbb 取代）；
- f8778bbb 的 Linux run 34674377220：created 04:58:23Z，**started 05:11:48Z**（此前一直
  pending，等 71cdc28f 的 run 收尾），in_progress 于 step9「Run registered checks」；
- 830b38c6 的 Linux run 34675402419：created 05:22:03Z 起长期 **pending**（同组串行）。

结论：**不是 job 挂起**，而是「单 concurrency 组 + 30~40 分钟长 job + cancel-in-progress:false」
下的排队/取代语义：GH 对同组只保留 1 running + 1 pending，新 run 入队会取消旧 pending，
因此中等间隔的 push 其 Linux run 可能从未真正执行（结论 cancelled），G-CI「同一 SHA
Linux/Windows 绿」在该 SHA 上不可得。workflow 侧已有：concurrency.group（含 ref+event_name）、
job timeout-minutes=330、run.py 逐检查 timeout_seconds（进程级终止）。

处置（本任务写域内）：①保留 cancel-in-progress:false（不打断进行中的长验证）；②group 必须
同时含 github.ref 与 github.event_name（校验器 B14 强制），使 **workflow_dispatch 手动核验
与 push 分属不同组**——即本任务手动触发双虚拟机核验不会被后继 push 取代；③该排队/取代
语义对本控制包谱系的 G-CI 可达性影响登记为 finding（转前台，见 05 号登记册续写）。
