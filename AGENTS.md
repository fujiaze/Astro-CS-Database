# AstroCS Agent 入口

> **最高守则声明（强制）**：根 [`ASTROCS_PROJECT_CONSTITUTION.md`](ASTROCS_PROJECT_CONSTITUTION.md)（`ASTROCS-CONSTITUTION-001`，状态 `FROZEN`）是项目**最高守则**与仓库唯一最高工程约束，**前台 Agent 与全部 SubAgent 必读**——前台派发前须确认 SubAgent 已按本条读取，SubAgent 不得以未读为由跳过宪章条款。宪章与任何历史文档冲突时以宪章为准（§1.1 权威分层）；Agent 无权放宽或重新解释宪章，修改权仅在项目负责人（§1.2 宪章变更流程）。

全程中文执行与汇报。开始任务前先读：

1. **冻结宪章（最高约束）**：根 [`ASTROCS_PROJECT_CONSTITUTION.md`](ASTROCS_PROJECT_CONSTITUTION.md) —— `ASTROCS-CONSTITUTION-001`，状态 `FROZEN`（GOV-001 冻结，四项负责人裁决见其 §18）。本文是仓库唯一最高工程约束；supersession 生效，Agent 不得修改、放宽或重新解释，修改权仅在项目负责人（§1.2 宪章变更流程）。
2. **历史约束（ARCHIVED_NON_NORMATIVE）**：根 [`AstroCS_ENGINEERING_CONSTRAINTS.md`](AstroCS_ENGINEERING_CONSTRAINTS.md) —— 已被宪章替代，仅作历史追溯；与宪章冲突处一律以宪章为准。
3. **记忆**：根 `memory.md` 及本任务相关模块 memory。
4. **模块文档**：相关模块 `README.md`、`module.yaml`、公共头与共址测试；科学/算法权威在 `docs/science/`、`docs/algorithms/`。
5. **控制包**（执行控制包派发任务时）：对应控制包 `00_READ_FIRST.md` 与本任务规格。

具体规则一律以冻结宪章为准，`AGENTS.md` 不复制长文。 本文件只承载入口、路由、执行纪律与上表映射，**不复制宪章长文**；映射与宪章冲突时以宪章为准。
## 治理要素 → 冻结宪章条款映射（10/10，机器门 `AGENTS-GOV`）

本节是**映射**而非复制：10 项治理要素一律以冻结宪章条款为语义源，逐项给出条款号与一句话要点，由 `tools/check_agents_gov.py` 机器校验（CI 检查项 `AGENTS-GOV`，三个 profile，非豁免门，见宪章 §12.3 机器一致性检查）。**本表与宪章冲突时一律以宪章为准**；条款号已逐条核对宪章实际内容，不得臆造。

| 检查器要素键 | 宪章条款 | 要点（语义以宪章为准） |
|---|---|---|
| `main-only` | §14.5 Agent 与提交；§14.1 现有仓库原地演进 | 只在 main 原子提交并立即 push——每个任务验证后立即 commit 并 push `main`；禁止分支及破坏性 Git——只使用现有 Git 工作区、不新建开发分支/worktree/额外 clone（§14.1），禁止 force push、amend、历史重写（§14.5）。 |
| `amd64` | §3.1 用户入口；§3.3 当前非目标 | 仅支持 amd64——正式入口只有 Windows 10+ amd64 `astrocs.exe` 与 Linux amd64 `astrocs`（§3.1）；ARM 或其他非 amd64 架构属当前非目标（§3.3）。 |
| `节点` | §15.1 Linux Agent 工作区；§15.3 Fatduck；§15.4 验证职责矩阵 | 控制节点 `vm-bj`（宪章称 Linux 服务器／DeepSeek Harness 常在线开发节点，§15.1）负责源码修改、静态与文档追踪、Linux 构建、单元测试、调度与证据汇总，并用本机 testdata/Gaia 完成控制包末尾真实数据流终验；Fatduck（§15.3）是 Windows 10+ amd64 正式验证节点，不 checkout 源码、不编译、不运行仓库任意脚本。主机名 `vm-bj` 为仓库运行事实（见 `FATDUCK_ACCESS.md`、`docs/architecture/ISA_VARIANTS.md`），宪章按角色而非主机名表述。 |
| `cpu-only` | §10.1 ACR；§10.2 CPU Provider；§10.3 Benchmark 与 Profile；§3.3 | ACR 暂不接入——ACR 是正式发布后的 CPU/GPU 异构优化项目，当前生产构建默认关闭、运行时不可达，Phase1/2/3 不得依赖 ACR 才能运行（§10.1），ACR/GPU/CPU+GPU 混合生产路由属当前非目标（§3.3）；生产仅纯 CPU 自适应 backend——CPU provider 只承载经 profiling 证明值得优化的重计算 kernel（§10.2），ISA 与并行度由 `astrocs benchmark cpu` 生成、绑定 CPU 特征/OS/软件版本/provider 哈希的 `cpu_profile` 决定，无或损坏 profile 时退 baseline ISA 并据 affinity/cgroup/系统资源动态定并行度（§10.3）。 |
| `单入口` | §3.1 用户入口；§8.1 CLI；§3.2 三个 Phase 相互隔离 | 仅一个 astrocs CLI——每个平台只提供一个用户可见入口，CLI 最终只暴露一个薄的 `astrocs` 入口、不实现科学公式（§8.1）；Phase1/2/3 由 CLI 调用（`astrocs phase1`/`phase2`/`phase3` 各自的 `validate`、`plan`、`run`、`inspect` 子命令）分别启动，严禁把三个 Phase 隐式串接为一次运行的入口（§3.2）。 |
| `资源门禁` | §10.5 重计算利用率门禁（负责人裁决见 §18.2）；§10.4 统一线程预算；§17.6 | 重计算自动监控——每个 heavy 运行自动记录进程/线程 CPU、每线程 CPU、RSS/PSS、内存增长、读写字节、I/O wait、work units、队列深度、worker 均衡与墙钟（§10.5）；低利用率或异常内存增长为失败——计算区间平均 CPU 利用率不低于已分配容量的 85%，任何连续 10 秒低于 60% 或只有一个活跃计算线程均失败，无界内存增长与单线程长计算同属发布门禁禁止项（§17.6）。 |
| `无硬编码` | §10.4 统一线程预算；§10.3 Benchmark 与 Profile；§17.6 | ISA、workers、block 由逐内核 benchmark 选择，禁止硬编码——模块不得硬编码 workers、不得建立不受 Runtime 管理的私有长期线程池，一个进程只有一个资源调度器与线程预算源（§10.4）；`astrocs benchmark cpu` 按 kernel 测量数值误差、吞吐、线程扩展、内存带宽、block 与 worker 数，选择依据使用稳定统计而非一次最快值（§10.3），heavy 路径无硬编码线程属发布门禁（§17.6）。 |
| `alpha/发布` | §17.12 最终发布决定；§16.1 版本；§15.3 | 未经最终外部审核不得宣称发布——只有项目负责人可以作最终发布决定，Agent 无权宣布发布（§17.12）；预发布固定 `MAJOR.MINOR.PATCH-alpha.N`、根 `VERSION` 为唯一输入（§16.1），`alpha.N` 只在外部审核通过后提升（`docs/governance/VERSION_NAMESPACES.md`）；审核包状态字面量 `AWAITING_EXTERNAL_RELEASE_REVIEW` 表示"已备齐、待外部审阅"，**不等同发布**，其"合法达成"由审核校验器判定（`docs/archive/review/RELEASE_STATUS.md`）；Fatduck 未复验只能标 `AWAITING_WINDOWS_VALIDATION` 且不得据此发布（§15.3）。 |
| `状态机` | §14.5 控制包完成顺序；§14.4 最小充分校验（fail-fast）；台账与 waiver 工具链 | 控制包完成顺序冻结为：任务提交全部完成 → GitHub CI 通过 → Linux 最终 SHA 真实数据流终验 → Agent 图像初审 → Windows/Fatduck 复验 → 汇总和打包，不得在真实数据终验前宣布控制包完成（§14.5）。任务级状态取 `NOT_STARTED -> IN_PROGRESS -> PASS/FAIL/BLOCKED/REVIEW_PENDING`（V5 期字面量，实际状态列以控制包 `TASK_LEDGER.csv` 为准）；`REVIEW_PENDING` 系 V5 遗留状态，当前台账验证器（`tools/quality/validate_task_ledger.py`）已将其判为**非法状态**，本条仅作历史映射、不得据此新增状态；放行一律走 waiver 登记（`ci/checks.json` 的 `waivable` 与 `ci/known_failures.json`），且严禁用 waiver 掩盖红灯（裁决 R-05/R-13）。 |
| `不停工` | §14.5 Agent 与提交；§15.3 Fatduck | 不设等待外部批准的停止点——不设置频繁人工 checkpoint，机器门禁通过后自动推进，只在科学定义冲突、权限/数据缺失、不可恢复失败或最终发布时请求负责人（§14.5）；Fatduck 离线不中止 Linux 可执行任务——Fatduck 离线不阻塞 Linux 开发、GitHub CI 和 Linux 真实数据终验，等待期间继续完成所有与 Windows 无关的任务（§15.3）。 |

**维护规则**：本表 10 项与 `tools/check_agents_gov.py` 的 `REQUIRED` 一一对应；宪章条款号若变更（§1.2 流程），必须同提交订正本表，不得只改一侧。若某项检查器断言的措辞与宪章表述不一致，以宪章为准并登记 finding，不得为过检查写入与宪章相悖的规则。

## 执行纪律（详见约束文件）

- 仅 `main` 原子提交并立即 push；禁止分支、force push 及破坏性 Git；SubAgent 不直接 commit。
- 一个 task 对应一个可独立验证的 commit；科学、架构、性能、文档清理不混提。
- 科学定义 = 算法 = 接口 = 代码 = 测试；科学公式与默认容差不得改动。
- 重计算禁止单线程并自动资源监控；线程/ISA/block 由逐内核 benchmark 选择，禁止硬编码。
- 所有外部命令带 timeout 并保存日志；修改后必须验证才能报告完成。
- 确认工作后，将任务分配给subagent。尽可能并行

## 目录规范（强制）

仓库根目录只允许下述固定条目。**任何新产物必须落位到对应目录，禁止散落根目录**；确需新增根目录条目，必须先在本节登记并获得项目负责人确认。

**顶层文件（固定，不得增删）**：
- 入口与约束：`README.md`、`AGENTS.md`、`ASTROCS_PROJECT_CONSTITUTION.md`（冻结宪章，GOV-001 登记）、`AstroCS_ENGINEERING_CONSTRAINTS.md`（ARCHIVED_NON_NORMATIVE 历史参照，保留不删）、`memory.md`、`VERSION`
- 文档：`CHANGELOG.md`、`DEPENDENCIES.md`、`HANDOVER.md`、`REVIEW.md`、`FATDUCK_ACCESS.md`、`VISUAL_CHECK_README.md`
- 构建面：`CMakeLists.txt`、`CMakePresets.json`、`build.sh`、`toolchain.ps1`、`.github/`、`.clang-format`、`.editorconfig`、`.gitignore`、`.gitattributes`

**固定目录**：
- 代码与合同：`lib/`（模块源码）、`include/`、`cli/`、`providers/`、`runtime/`、`modules/`、`docs/`、`contracts/`（含 `contracts/schemas/` 合同 schema 唯一事实源）、`cmake/`
- 测试与工具：`tests/`、`scripts/`、`tools/`、`ci/`、`testdata/`、`third_party/`
- 工程与发布：`engineering/`、`packaging/`（含 `packaging/launch/`）、`logs/`（gitignore）、`evidence/`

**工作域目录（产物落位规则）**：
- `run/`：一切临时操作、agent 工作区、影子树、日志。CLI 运行产物 `astrocs_run_*.json` 一律落 `run/cli_runs/`；ctest 根目录残留归 `run/Testing_archive/`。`run/*` 全部 gitignore。
- `工程控制/`：控制包解压文档（一个控制包一个子目录）；控制包/审核包 zip 原件归 `工程控制/_control_packs/`。
- `reports/`：正式报告（含任务交付报告）。
- `artifacts/`：证据封装、capsule、测量产物。
- `build/`、`out/`：构建输出（gitignore，不入库）。

**用户/资料区（原地保留，禁改禁删，gitignore）**：
- `BASS DR3/`：备用测试数据集（索引/工具入仓库规则见 .gitignore）
- `AstroCS.wiki/`：wiki 本地克隆
- `GaiaDR3/`、`GaiaDR3SP/`：真实数据拉取落点（fatduck 拉取，禁止移动/重命名/删除，路径被拉取与测试脚本引用）

**CI 资源产物路径（ci/checks.json dirty_ignore 引用；运行产物，gitignore，落 `run/resource/`）**：
- `resource_samples.csv`、`resource_summary.json`、`worker_balance.csv`
修改代码、测试后应同步订正ci。禁止将运行产物产出到项目根目录
**违规处理**：发现根目录散落产物，整理归位到上述目录并在 commit 或 memory.md 中注明来源与去向。

## Fatduck 节点 SSH 接入（self-hosted runner 机器）

- Windows 11，`fujia@100.104.10.71`（Tailscale），专用密钥 `/home/dsh/.ssh/id_ed25519_fatduck`（仅可用 `-i` 路径引用；禁止读取/复制/打印密钥内容）。
- 节点 sshd 的 DefaultShell 指向 bash 语义 shim（`bash -c` 直转），ssh exec 直接写 bash 语法；禁止把该节点的 shell 环境当作 cmd 语法来用：
  `ssh -i /home/dsh/.ssh/id_ed25519_fatduck fujia@100.104.10.71 'ls /d/AstroCSRunner && git --version'`
- 节点级运维（DefaultShell 调整、服务与 runner 修复等 ssh exec 覆盖不到的操作）走 `.github/workflows/fatduck-admin.yml` 的 `workflow_dispatch` 带外通道；`pwsh` 禁止用于开发环境默认 shell，仅节点运维通道可用。
