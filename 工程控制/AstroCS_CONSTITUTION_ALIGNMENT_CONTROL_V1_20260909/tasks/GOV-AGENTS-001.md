# GOV-AGENTS-001｜AGENTS.md 宪章要素映射对齐（AGENTS-GOV 转绿）

## 目标
修复 `AGENTS-GOV`（三个 profile 全红，非豁免门）：`tools/check_agents_gov.py` 断言的 10 项 V5 治理要素当前 **10/10 全部 MISS**。

必须做的是"**映射式恢复**"而非复制长文：在 `AGENTS.md` 中以「治理要素 → 冻结宪章条款」映射表逐项显式承载下列 10 项要素，每项写出宪章条款号与一句话要点（不得臆造条款号，须逐条核到 `ASTROCS_PROJECT_CONSTITUTION.md` 实际内容）：

| 检查器要素键 | 语义 | 宪章去向（执行时逐条核实后填写） |
|---|---|---|
| `main-only` | 只在 main 原子提交并立即 push、禁止分支及破坏性 Git | §14.5 Agent 与提交 |
| `amd64` | 仅支持 amd64 | §3.1 用户入口 / §3.3 当前非目标 |
| `节点` | 控制节点与 Fatduck 验证节点分工 | §15.1 / §15.3 |
| `cpu-only` | ACR 暂不接入、纯 CPU 自适应 backend | §3.3 / §10.1 / §10.2 |
| `单入口` | 仅一个 astrocs CLI，Phase1/2/3 由 CLI 调用 | §3.1 / §8.1 |
| `资源门禁` | 重计算自动监控，低利用率或异常内存增长为失败 | §10.4 / §10.5 |
| `无硬编码` | ISA、workers、block 由逐内核 benchmark 选择，禁止硬编码 | §10.4 |
| `alpha/发布` | 未经最终外部审核不得宣称发布 | §16.1 / §17.12 |
| `状态机` | NOT_STARTED → IN_PROGRESS → REVIEW_PENDING → waiver | §14.5 控制包完成顺序 |
| `不停工` | 不设等待外部批准的停止点；Fatduck 离线不中止 Linux 任务 | §15.3 |

同时：在文件顶部显著位置声明「`ASTROCS_PROJECT_CONSTITUTION.md` 是项目最高守则，**前台 Agent 与全部 SubAgent 必读**」，并保留/强化既有"具体规则一律以冻结宪章为准，AGENTS.md 不复制长文"的定性。

**要素文本措辞必须同时满足检查器的字面断言**（`tools/check_agents_gov.py` 的 `REQUIRED` 列表逐项 `in text`），但语义一律以宪章为准；若某条检查器断言与宪章表述冲突，**登记 finding 并如实上报**，不得为过检查而写入与宪章相悖的规则。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `AGENTS.md`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-07**：既不放宽 `tools/check_agents_gov.py`、也不删检查项，以映射式承载使机器门与宪章**同向**。
- 负责人 2026-09-12 指令原文口径：「宪章是项目最高守则，记录到 agent.md，前台 agent 必读」。
- 宪章 §12.3 机器一致性检查；§1.1 权威分层（宪章为最高源）。

## 非目标与禁令
- 不顺手修复域外问题；发现后登记 finding（05 号登记册续写）。
- 不修改/放宽科学公式、默认容差、冻结门或负责人裁决。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。
- **严禁以"放宽检查规则 / 加入 known-failures 基线 / 改 waivable / 跳过测试"的方式让红灯消失**（裁决 R-05、R-13）。
- 新增/修改测试必须同提交注册 CI 检查项（宪章 §17 + 负责人 CI 指令）；运行产物禁止落根目录。

## 必须动作
1. 读取冻结宪章相关条款、`docs/standards/STANDARDS_REGISTRY.md`、本任务域 SCI/ALG/DATA/ARCH 文档。
2. 测试设计先行：先红后绿、负向注入必败、1/N worker parity（适用时）、确定性 bitwise（适用时）。
3. 执行并保存命令证据（timeout/cwd/argv/起止/rc/stdout/stderr/SHA）；heavy 同 run ID 资源监控。
4. 同步订正 CI：新测试目标→`ci/checks.json` 显式检查项；漂移锚→复测工具。
5. 修复后必须**本地复现 CI 同构条件**验证（不得只在本机默认环境跑绿即报 PASS）。

## 验收
- write_scope 零越界；预存 dirty 零覆盖；所有新测试故障注入必败。
- 本任务对应的 CI 检查项在本地同构条件下 **exit 0**，并给出命令与日志路径。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
