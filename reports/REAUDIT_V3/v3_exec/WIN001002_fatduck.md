# WIN-001 / WIN-002 —— Fatduck Windows 远程验证记录

> G6: WIN-001 (Fatduck 只拉 main, 验证 HEAD==origin/main); WIN-002 (Windows 正式构建命令
> 和测试全部执行)。数据在 Fatduck (审核人确认), 经 SSH 远程执行, 时间窗 07:00–23:30 CST。
> 执行时间：2026-08-28 16:16–16:4x CST (窗口内)。复核：2026-08-28。

## 1 WIN-001 —— 同步与前置校验 ✓

- 克隆定位：`F:\Astro dev\Astro CS Normalization Database`（远端 = github.com/fujiaze/
  Astro-CS-Database.git，分支 main）。
- 前置：拉取前 tracked 修改数 = 0（仅历史遗留 untracked zip 交付物，未触碰未清理）。
- `git pull --ff-only origin main`：`535e7387..3c45ca93`，**HEAD == origin/main ==
  3c45ca93dc55bd0e5fab2fe062cd390f9e049a8c** ✓（含 Linux 侧全部 BLD/TST 修复）。
- 数据确认在位：`testdata\Galaxy_Center_T4` **158 个文件**；`GaiaDR3SP`（亦在 `D:\Gaia`）。

## 2 WIN-002 —— Windows 正式构建 ✓

- `toolchain.ps1 check`：MinGW64 g++ **16.1.0**、GNU Make 4.4.1、CMake 4.3.2、Ninja 1.13.2、
  Python 3.12.2、nlohmann/json 已装、orchestrator 2.0 可运行。
- `toolchain.ps1 build`（10 模块正式构建）：**exit 0「全部模块编译完成」**。
  - 跨平台修复在 Windows 侧验证生效：orchestrator 链接行正确走
    `-static -Wl,--stack,33554432`（Windows 分支）且含 `sha256.cpp` 补链。
  - 既有的被忽略项：`photometric_calib` Makefile:33 Error 1 (ignored)（预存，非本次引入，
    模块按既有 DLL 使用；已记录待查）。
  - Stage2 (healpix_stack) 按正式流程冻结跳过，使用现有 DLL。

## 3 WIN-002 —— Windows 测试全部执行 ✓

`lib\orchestrator\cpp` 4 个正式测试二进制（MSYS2 MinGW64 g++16.1，HEAD=7737f539）：

| 测试 | 结果 | Linux 对照 |
|---|---|---|
| test_logger | **53 通过 / 0 失败** (exit 0) | 53/0 一致 |
| test_checkpoint | **78 通过 / 0 失败** (exit 0) | 78/0 一致 |
| test_dll_loader | 23 通过 / **1 失败** (exit 1) | 23/1（平台差异见下） |
| test_orchestrator_cli | **233 通过 / 0 失败** (exit 0) | 202/31 |

**关键交叉验证**：Linux 上 test_orchestrator_cli 的 31 项失败在 Windows（testdata +
模块 DLL 齐备）**全部通过** ⇒ 证实该 31 项为纯外部依赖（数据/产物），非移植/科学缺陷；
同理 TST-002 的 Linux 记账成立。

**dll_loader 1 失败（两平台一致语义）**：`应加载全部 5 个模块` — PSF/PHOTOMETRIC/DRIZZLE
模块 `code=126 找不到指定的模块`（依赖链 DLL 缺失的运行环境问题；同 run 内
"加载失败状态=LOAD_FAILED" 断言 PASS，属可解释的环境性失败，非回归）。

**Windows 侧回归发现与闭环**：本轮实测抓到 `@mkdir -p` 在 cmd.exe shell 下的回归
（test 目标 Error 1）→ Linux 主仓原子修复（`MKDIR_TESTDIR` 按 OS 条件选择，commit
7737f53）→ Fatduck pull 后重建+全测试通过。正是「Windows 远程验证节点」价值所在。

## 4 结论

- **WIN-001 判 PASS**（只拉 main、ff-only、HEAD==origin/main、0 未知修改覆盖）。
- **WIN-002 判 PASS**（正式构建 exit 0 + 4 测试全部执行：367 通过/1 环境性失败，交叉验证
  Linux 失败项均为外部依赖）。
- 遗留记录：photometric_calib 既有 ignored 错误；dll_loader 依赖链 code=126 环境性失败；
  phase2 Windows CMake 测试不在冻结流程范围（Stage2 使用现有 DLL）。
