# WCS-PATH-001｜p1wcs astropy 交叉测试环境无关化（CTEST-P1WCS-ASTROPY-CROSS 转绿）

## 目标
修复 `CTEST-P1WCS-ASTROPY-CROSS`（linux-main 非豁免门，exit 8）与由其级联的 `CTEST-LINUX-FULL`（168 项中唯一失败即它）。根因（前台实测 + FD-R1-019）：`tests/unit/p1wcs/p1wcs_astropy_cross.py:32-37` 默认路径硬编码 `/workspace/Astro CS Database/run/p1wcs_wcs003/...`，GitHub runner 上 `/workspace` 不可写 → `os.makedirs` 抛 `PermissionError: [Errno 13] Permission denied: '/workspace'`。

必须动作：
1. 消灭硬编码绝对路径：改为**仓库内相对路径** + 显式 `--work-dir`/环境变量覆盖 + `tempfile` 兜底；默认值必须是可在任意宿主创建的工作目录；
2. 在 CI 侧（`ci/checks.json` 该检查项的 command，或测试自身默认）显式传入工作目录，确保 hosted runner 可写；
3. **不得**降低 astropy 交叉判定强度、不得改 `waivable`、不得把该 ctest 目标移出注册；
4. 负向守卫：工作目录不可写时必须以**明确错误信息**失败（fail-fast，宪章 §14.4），不得静默跳过而假绿。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `tests/unit/p1wcs/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-14**：环境无关化，保留 astropy 交叉判定能力。
- 裁决 **R-05**：`CTEST-LINUX-FULL` 不得以扩基线或豁免处置。
- 宪章 §14.1「不写死服务器绝对路径」；§13.1 必备"确定性合成数据生成器"与"独立 Oracle"；§15.2 GitHub 托管 CI 不依赖本机私有数据。

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
