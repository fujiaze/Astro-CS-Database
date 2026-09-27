# CI 流水线定义（Pipeline）

> 上游：ASTROCS_DESIGN.md §12.4（验证层级与四层验收）

## 1. 触发与范围

范围口径以 `CI_SPEC.md §2` 为正本；本节只写"什么事件跑哪个范围"。

| 事件 | 范围 | 说明 |
|---|---|---|
| 本地 / agent 默认运行 | `changed`（增量） | 无参数即增量：只跑与改动集相交的检查 |
| `push` 到 `main` | profile 全档 | 合并后整档全量：`ci-linux.yml` 跑 `python3 eng/ci/run.py --profile linux-main`、`ci-windows.yml` 跑 `--profile windows-main`，杜绝"增量假绿"进入 main |
| `workflow_dispatch` | 手动 | `ci-linux.yml` 提供 `profile` 选择（`linux-main` / `linux-deep`，默认 `linux-main`）；`ci-windows.yml` 固定 `windows-main` |
| `schedule` | 每日一次全档 | `ci-linux.yml` cron `17 19 * * *`（UTC）复跑整档，兜底无提交日 |
| 提交前（本地 / agent） | `fast` + `integration` 两档 | `fast` 只含秒级一致性门；真起子进程 / 真跑 CLI / 真实测量窗的步骤在 `integration` 档，**必须另跑** |
| 负责人触发 `prerelease` | `--profile prerelease` | 真实数据 E2E / L2 性能 / sanitizer / coverage / nwoker / invariant；带输入指纹缓存，指纹命中即复用归档 |

- 两平台 workflow 的 `on:` 面只含 `push`（`main`）、`workflow_dispatch`、`schedule`，均未配置 `pull_request` 触发；合并前验证由提交前本地 / agent 运行（`fast` + `integration` 两档）承担。

- 增量档 fail-closed 三条（未覆盖路径判红 / 敏感面强制升级全量 / 空选择判红）见 `CI_SPEC.md §2.4`；
- 结果 JSON 顶层 `scope` 必须如实反映本轮范围（`full` / `changed` / `explicit`）；
- 增量档总预算 `--budget-seconds`（默认 120 s）：超出即判红并提示"应拆分"。

## 2. Job 结构

两平台各一个独立 workflow、各一个 job，无 `needs` 依赖链；检查项的编排由
`eng/ci/run.py --profile <profile>` 在 job 内完成（profile → 检查集见 `eng/ci/checks.json`）。

| Workflow | Job（依赖） | 内容 | 证据锚 |
|---|---|---|---|
| `ci-linux.yml` | `linux`（单 job，ubuntu） | run.py --profile linux-main（workflow_dispatch 可选 linux-deep）；失败路径 wf_step.py --step LINUX-BOOTSTRAP-DIAG | `linux-ci-<sha>` ← `artifacts/ci/`（always） |
| `ci-windows.yml` | `windows`（单 job） | run.py --profile windows-main；失败调 wf_step.py --step WINDOWS-BOOTSTRAP-DIAG | `astrocs-windows-candidate-<sha>`、`windows-ci-<sha>` ← artifacts/（success/always） |

增量档不跑整条链：只执行与改动集相交的检查（`CI_SPEC.md §2.3`），构建/测试 target 由构建图反查得出；
任一 fail-closed 条件命中即判红。

## 3. 并行与超时

- `ci-linux.yml` 与 `ci-windows.yml` 是两个独立 workflow，各自触发、互不依赖；
- 每 workflow 单 job，`timeout-minutes: 330`（`ci-linux.yml` / `ci-windows.yml` 一致）；
- **每 step 的 timeout 上界 = max(60, 3 × 最近一次实测墙钟)**，硬上限 3600 s（`prerelease` 重步骤 10800 s）；
  逐项改前/改后数值见 `eng/ci/checks.json` 与 `run/CI-INCREMENTAL/EVIDENCE.md`；
- **增量档总预算**：`--budget-seconds`（默认 120 s），实际耗时超出即判红并提示"应拆分"；
- 日志按 job 留存，可下载。

## 4. 门禁判定

- 汇总 `ci_result.json`（各检查项 rc + 证据路径）；
- 任一 P0 红 → 整体失败，阻塞合并；
- P1 红 → 失败，除非负责人已登记豁免；
- 全部绿 → 生成门禁通过报告，进入打包。

## 5. 真实数据与 Windows 复验

- 真实数据终验/Windows 复验**不是**每提交自动项（成本高），由负责人按阶段触发；
- CI 中的合成测试通过 ≠ VERIFIED；VERIFIED 定义见最高设计 §12.5。

## 6. 失败处理

- 红灯：回退该 commit 或补修提交；提交历史一律前向追加（amend/force push 在处置面之外）；
- 基础设施故障：重跑一次；连续失败负责人介入；
- 所有失败留日志与复现命令。

## 7. 产物（见 04_ARTIFACTS.md）

每次 CI 留存 artifact：`linux-ci-<sha>` 与 `windows-ci-<sha>`（`artifacts/ci/`）、`astrocs-windows-candidate-<sha>`（`artifacts/candidate/`），retention 14 天；留存面明细见 04_ARTIFACTS.md。
