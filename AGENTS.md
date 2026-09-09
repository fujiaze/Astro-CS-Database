# AstroCS Agent 入口

全程中文执行与汇报。开始任务前先读：

1. **冻结约束**：根 [`AstroCS_ENGINEERING_CONSTRAINTS.md`](AstroCS_ENGINEERING_CONSTRAINTS.md) —— 项目负责人冻结工程约束（来源与控制包 hash 关系见其文件头 YAML）。优先级最高，Agent 不得修改、放宽或重新解释；修改权仅在项目负责人。
2. **记忆**：根 `memory.md` 及本任务相关模块 memory。
3. **模块文档**：相关模块 `README.md`、`module.yaml`、公共头与共址测试；科学/算法权威在 `docs/science/`、`docs/algorithms/`。
4. **控制包**（执行控制包派发任务时）：对应控制包 `00_READ_FIRST.md` 与本任务规格。

具体规则一律以冻结约束文件为准，`AGENTS.md` 不复制长文。
## 执行纪律（详见约束文件）

- 仅 `main` 原子提交并立即 push；禁止分支、force push 及破坏性 Git；SubAgent 不直接 commit。
- 一个 task 对应一个可独立验证的 commit；科学、架构、性能、文档清理不混提。
- 科学定义 = 算法 = 接口 = 代码 = 测试；科学公式与默认容差不得改动。
- 重计算禁止单线程并自动资源监控；线程/ISA/block 由逐内核 benchmark 选择，禁止硬编码。
- 所有外部命令带 timeout 并保存日志；修改后必须验证才能报告完成。
- 确认工作后，将任务分配给subagent。尽可能并行

## 目录规范（强制，2026-09-09 整理后基线）

仓库根目录只允许下述固定条目。**任何新产物必须落位到对应目录，禁止散落根目录**；确需新增根目录条目，必须先在本节登记并获得项目负责人确认。

**顶层文件（固定，不得增删）**：
- 入口与约束：`README.md`、`AGENTS.md`、`AstroCS_ENGINEERING_CONSTRAINTS.md`、`memory.md`、`VERSION`
- 文档：`CHANGELOG.md`、`DEPENDENCIES.md`、`HANDOVER.md`、`REVIEW.md`、`FATDUCK_ACCESS.md`、`VISUAL_CHECK_README.md`
- 构建面：`CMakeLists.txt`、`CMakePresets.json`、`build.sh`、`toolchain.ps1`、`.github/`、`.clang-format`、`.editorconfig`、`.gitignore`、`.gitattributes`

**固定目录**：
- 代码与合同：`lib/`（模块源码）、`include/`、`cli/`、`providers/`、`runtime/`、`modules/`、`graph/`、`docs/`、`contracts/`、`schemas/`、`cmake/`
- 测试与工具：`tests/`、`scripts/`、`tools/`、`ci/`、`testdata/`、`third_party/`
- 工程与发布：`engineering/`、`packaging/`、`launch/`、`logs/`（gitignore）、`evidence/`

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

**CI 固定路径（ci/checks.json 引用，不得移动；内容为运行产物，gitignore）**：
- `resource_samples.csv`、`resource_summary.json`、`worker_balance.csv`

**违规处理**：发现根目录散落产物，整理归位到上述目录并在 commit 或 memory.md 中注明来源与去向。

## Fatduck 节点 SSH 接入（self-hosted runner 机器）

- Windows 11，`fujia@100.104.10.71`（Tailscale），专用密钥 `/home/dsh/.ssh/id_ed25519_fatduck`（仅可用 `-i` 路径引用；禁止读取/复制/打印密钥内容）。
- 节点 sshd 的 DefaultShell 指向 bash 语义 shim（`bash -c` 直转），ssh exec 直接写 bash 语法；禁止把该节点的 shell 环境当作 cmd 语法来用：
  `ssh -i /home/dsh/.ssh/id_ed25519_fatduck fujia@100.104.10.71 'ls /d/AstroCSRunner && git --version'`
- 节点级运维（DefaultShell 调整、服务与 runner 修复等 ssh exec 覆盖不到的操作）走 `.github/workflows/fatduck-admin.yml` 的 `workflow_dispatch` 带外通道；`pwsh` 禁止用于开发环境默认 shell，仅节点运维通道可用。
