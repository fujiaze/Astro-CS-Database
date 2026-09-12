# CI-WIN-001｜Windows 构建断链修复（WIN-BUILD-RELEASE / WIN-PACKAGE-CANDIDATE 转绿）

## 目标
修复 `WIN-BUILD-RELEASE`（windows-main 非豁免门）。前台实测失败原文：

```
CMake Error ... Could NOT find ZLIB (missing: ZLIB_LIBRARY) (found version "1.3.2")
  FindZLIB.cmake:202 -> lib/gaia_xpsd_client/CMakeLists.txt:68 (find_package)
```

根因：`e6254d4d` 的修复**不完整**（FD-R1-011）——gaia 段只消费了 runner 注入的 `ACS_ZLIB_ROOT`（令 include 命中，故报 "found version 1.3.2"），**未消费** `ACS_ZLIB_LIB`（因此 `ZLIB_LIBRARY` 仍缺）；根 `CMakeLists.txt` 的 `astrocs_cfitsio` 段**已**消费两者 ⇒ 两段口径分叉。级联导致 `WIN-PACKAGE-CANDIDATE`（configure 未产出 `cmake_install.cmake`）与 `WIN-TEST-UNIT` 全红。

必须动作：
1. `lib/gaia_xpsd_client/CMakeLists.txt` 对齐根 `astrocs_cfitsio` 段口径，同时消费 `ACS_ZLIB_ROOT` 与 `ACS_ZLIB_LIB`（`CMP0074` `ZLIB_ROOT` 语义）；
2. **不得**改为可选依赖、不得去掉 `REQUIRED`、不得以 `waivable` 处置；
3. 三场景本地复验并留证：env 为空（系统兜底，基线不变）／有效 root+lib（rc=0）／无效 root（走系统搜索）；
4. 复验 `WIN-PACKAGE-CANDIDATE` 级联转绿；
5. 若 `dumpbin.exe` PATH 问题在本任务复验中仍构成阻塞，按写域边界**登记给 `CI-BACKEND-001`**，不得越界顺手改。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `lib/gaia_xpsd_client/`
- `.github/workflows/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-12**：两段 ZLIB 消费口径对齐，不得放宽 REQUIRED。
- 裁决 **R-13**：环境/工具链配置缺陷按配置修，不得降级数值门禁或改 `waivable`。
- 宪章 §15.2「Windows 正式工具链为 VS2022/MSVC v143…不把托管镜像变化当成放宽数值门禁的理由」；§17.7 同 SHA Linux/Windows CI 通过。
- 关联 finding：**FD-R1-011**（`e6254d4d` 判定"已修复"与实际不符）。

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
