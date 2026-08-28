# BLD-002/003 —— AIO 可移植性现状核验与范围选项（供审核人裁定）

> 状态：**IN_PROGRESS**（orchestrator Windows 泄漏部分已修复并双平台验证；AIO/模块 DLL
> 体系部分待范围裁定）。更新：2026-08-28。

## 1 已修复并验证（本会话，`main` 提交链）

| 缺陷 | 修复 | 验证 |
|---|---|---|
| orchestrator.cpp 12 处裸 Windows API（LoadLibraryExA×2/GetProcAddress×8/FreeLibrary×2） | 改走 DllLoader 可移植封装（Linux stub 报"非 Windows 平台不支持"，不再编译失败） | Linux make 全绿 + orchestrator.exe 语义验证（--validate 退出码 0/1/7 等）；Windows toolchain build 10 模块 exit 0 |
| Makefile LDFLAGS MinGW 专属（`-static`/`--stack`）在 Linux 硬崩 | 按 `$(OS)` 条件化；Linux `-Wl,-z,stacksize` | 双平台构建 exit 0 |
| Makefile 4 处 cmd.exe `if not exist mkdir` 在 Linux 失效；以及后续发现的 `mkdir -p` 在 Windows cmd.exe 回归 | `MKDIR_TESTDIR` 双平台条件定义 | Windows 回归由 WIN-002 实测抓到并闭环（7737f53） |
| test_orchestrator_cli 缺 sha256.cpp 链接 + `exec_with_stdin` 无 POSIX 实现 | 补链 + popen 分支（首 token `./` 前缀、stderr 分离捕获） | Linux 202/31（失败全外部）+ Windows **233/0** |

## 2 待裁定：AIO 库与模块 DLL 的可移植性（BLD-002/003 剩余部分）

**现状**（机器核实）：
- `lib/astro_image_io/astro_image_io.dll`（4.27MB）实为 **ELF 64 位 Linux 共享对象**，
  由 Windows PowerShell `build.ps1` 预构建并提交，运行时可加载（ldd 确认）。
- 锚点 A/B/C 树中 `lib/astro_image_io/*.dll` 被 `.gitignore`（`*.dll`）——历史锚需先源码
  构建 AIO（RUN-001 已在 Fatduck 完成：A/B/C 锚 AIO build_exit=0，dll SHA 在案）。
- 当前 main 的 phase2 CMakeLists 对 `astro_image_io.dll` 为硬编码引用（Linux 链接器不
  区分扩展名，功能可用；属命名/来源卫生问题，非功能缺陷）。
- stage1 五个模块 DLL（calibration/ipv_solver/dynamic_psf/photometric_calib/snr_estimator）
  为 Windows 产物，**无 Linux 构建入口**——Linux 上 stage1 全链路不可用（TST-005 BLOCKED 根因）。

**选项**：
- **A（最小收尾）**：保持模块 DLL Windows-only（与"Windows 为正式构建平台"的既定事实
  一致），把 BLD-002/003 收敛为"Windows 泄漏已清除 + AIO 命名/预构建产物卫生问题记录
  为已知限制"；Linux 侧 stage2/合成链路已全绿。工程量 0，**本预发布可收口**。
- **B（完整可移植）**：为 AIO + 5 模块建立 Linux 构建入口（CMake/Make 统一、`.so` 命名、
  去硬编码），解除 TST-005 BLOCKED。涉及 6 个模块的构建体系改造 + CI 双平台验证，
  工程量约数人日，**建议列为下一版本范围**。

**建议**：本预发布按选项 A 收口（BLD-002/003 标 PASS 并附已知限制记录），选项 B 进
V21 backlog。请审核人裁定。
