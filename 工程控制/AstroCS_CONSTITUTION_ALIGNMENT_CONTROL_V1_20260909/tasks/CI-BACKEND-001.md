# CI-BACKEND-001｜CI 镜像依赖与工具链 PATH 对齐（UT-BACKEND / WIN-PACKAGE-CANDIDATE 转绿）

## 目标
修复 hosted runner 环境配置缺陷（**非**科学或数值门禁问题）：

**(1) `UT-BACKEND`（linux-main 红）** —— job 日志实测：hosted 镜像缺 `numpy`（`tests/io` 全量 import → `ModuleNotFoundError`，3 tests 0 ran）、缺 `astropy`（`UT-IO` 的 MOC 断言在缺 astropy 时必然 FAIL）、缺 `nlohmann-json3-dev`（`UT-BACKEND abi_loader/backend_loader.cpp` 的 CMake include 路径）。

**(2) `WIN-PACKAGE-CANDIDATE`（windows-main 红）** —— job 日志实测：`dumpbin.exe` 存在于 VS2022 但不在 PATH ⇒ CI-001 收紧后 `prerequisite FAIL(prerequisite)` 不可豁免 ⇒ `ci/run.py exit 1`。

必须动作：
1. 在 `.github/workflows/` 补齐依赖安装步骤（`numpy`/`astropy`/`nlohmann-json3-dev` 等，**按实际 import 清单最小充分安装**，宪章 §14.4 禁止防御性堆叠）；
2. Windows 侧按 **MSVC 工具链固定布局通配**注入 `dumpbin.exe` 所在目录到 PATH —— **禁止硬编码版本号目录**（宪章 §14.1「不写死服务器绝对路径」）；
3. **不得**把 `UT-BACKEND`/`WIN-PACKAGE-CANDIDATE` 改 `waivable=true`、不得降级 SKIPPED、不得移出注册（裁决 R-13）；
4. 若某依赖项确属"CI 不应安装"（例如仅 deep profile 需要），须在 `ci/checks.json` 的 profile 归属上**显式收口**并说明理由，不得静默跳过；
5. 证据须含：依赖安装步骤日志 + 三 profile 的 `ci/run.py --plan-only` 与对应检查项本地复跑结果。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `.github/workflows/`
- `ci/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-13**：CI 镜像依赖与工具链 PATH 属配置缺陷，按配置修；禁止改 `waivable`、禁止降级为 SKIPPED。
- 宪章 §15.2 GitHub 托管 CI「可公开、可重复」，「不把托管镜像变化当成放宽数值门禁的理由」；§14.1 禁止写死绝对路径；§14.4 最小充分校验。

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
