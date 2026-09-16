# 任务：TEST-CLI-SYNC 存量测试同步新命令树

状态：NOT_STARTED
层：L1　依赖：CLI-001（已完成，命令树已切换）
文件域互斥组：S3-A（tests/ 的存量测试 + tools/check_cli_run_preset.py）

## 背景（前台实测 2026-09-16）

CLI-001 已把用户命令树切换为 `normalize|mosaic|export|help|--version|doctor|benchmark`，旧 phase 命令一律 rc=2（rc 矩阵 55 条 mismatch=0，前台复跑通过）。但**存量测试仍直接调用旧命令**：

- CLI-001 实测：**41 个测试文件**含 `phase1 run` 等字面量（tests/cli/** 16、tests/api/** 7、tests/backend/** 11、tests/artifact/** 3，其余在 pipeline/runtime/quality/integration）；
- 前台复跑 `python3 -m unittest discover -s tests/cli -t .` → **Ran 77, failures=2, errors=11, skipped=38**，其中 11 个 ERROR 全是 `setUpClass`（测试类前置依赖旧命令/旧 surface：test_bench_cli、test_cli001_vpi、test_cli004_process_protocol、test_cli_protocol(2)、test_cli_v7_surface(3)、test_phase{1,2,3}_inprocess），2 个 FAIL 在 `test_p1003_drizzle_path`（drizzle 命令已删除 ⇒ 断言语义失效）；
- `tools/check_cli_run_preset.py` 判据是 `{"phase1 run"` 字面量 ⇒ 现为红，且已注册 CI 项 `CLI-RUN-PRESET`（`waivable=false`）。

## 目标

让存量测试与检查器**与新命令树一致**：能红的仍要能红（不许用 skip/放宽掩盖），断言语义改为新命令。

## 权威依据

- ASTROCS_DESIGN.md §6.2（七行命令树；phase 仅内部指代）；
- ENGINEERING_SPEC.md §8（测试/检查必须能红能绿，禁 skip 掩盖）；
- docs/plugins/infrastructure/18_cli.md（CLI 插件契约，只读引用）；
- CLI-001 的 rc 矩阵与 `tools/check_cli_command_layer.py` 判据（新命令树的事实源）。

## 允许改

- `tests/**` 中调用旧 CLI 命令的测试（夹具/断言改新命令；**不得删除测试意图**）；
- `tools/check_cli_run_preset.py`（判据改造为实测运行：新命令 + 预设互斥/组合语义）或**显式退役**（须给依据并同步 `ci/checks.json` 与 `docs/ci/01_CHECKS.md`）；
- `tools/check_cli_command_layer.py` 的**调用点**如需同步（判据本体属 CLI-001 已完成产物，不改语义）。

## 禁止改

- 产品代码（`lib/**`、`cli/**`）与文档权威（`docs/science/**`、`docs/algorithms/**`、`docs/plugins/**`）；
- **不得**把测试改成 skip/xfail 来变绿；每个改动必须给「改前红 → 改后绿」的复跑证据；
- `ci/checks.json` 的**其它**注册项（CI-001 正在独占收敛；若必须同步 CLI-RUN-PRESET，先向前台申请窗口）。

## 步骤

1. 用 `grep -rn 'phase1 run\|phase2 run\|phase3 run\|\"phase1\"' tests/ tools/` 实测补齐清单（与 41 文件对照）；
2. 逐类处置：① 命令名调用 → 改新命令；② 旧 surface 断言（modules/selftest/version/config/graph/drizzle）→ 按新树语义重写或**显式退役该测试**（须写明依据）；③ inprocess 前置（`test_phase{1,2,3}_inprocess`）→ 改为按新命令 + 会话 ABI 调用；
3. `tools/check_cli_run_preset.py`：改造或退役（二选一，给依据）；
4. 复跑并留证：`python3 -m unittest discover -s tests/cli -t .`、`tests/api`、`tests/backend`、`tests/artifact`，以及每个被改检查器的正例+负例。

## 验收门（前台独立复跑）

- [ ] `tests/cli` rc=0（或用例数/失败集合逐条解释，**不允许新增 skip**）；
- [ ] `tools/check_cli_run_preset.py` 正例 rc=0 + 负例能红；
- [ ] 清单内 41 个文件**逐一**给出「改/退役/与 CLI 无关」结论（计数一致）；
- [ ] 无新增 skip/xfail（与基线逐条对照）；
- [ ] 越界自查（`git status` 仅含本域）。

## 交付物

1. 测试与检查器改动；2. 逐文件处置表；3. 复跑证据；4. 自证摘要。