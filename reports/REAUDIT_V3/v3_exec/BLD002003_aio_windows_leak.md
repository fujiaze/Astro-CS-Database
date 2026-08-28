# BLD-002/003 AIO fPIC 与 Windows 构建泄漏 —— 核验与发现

> BLD-002: AIO fPIC 与 shared-library 可移植性。
> BLD-003: 移除 Linux 构建中的 Windows-only 泄漏。
> 状态：**IN_PROGRESS**（共享库可加载、fPIC 正常；但 `.dll` 命名 + Windows build.ps1 依赖
> ⚠ 可移植/可复现缺口，需修复后判 PASS）。
> 复核：2026-08-27。

## 1 观察证据

- `lib/astro_image_io/astro_image_io.dll`：`file` = **ELF 64-bit LSB shared object, x86-64
  (GNU/Linux), dynamically linked**，4.27MB。即名义 `.dll`、实为 Linux 共享库。
- `ldd build/run-release/phase2_synthetic_gate` 确认该库被 **动态加载**（解析 `aio_*` 符号）。
- `nm` 显示 `aio_hips_open` 在 phase2_synthetic_gate 中为 `U`（由运行期共享库解析）。
- phase2 CMakeLists:69-79 用 `target_link_libraries(... ${CMAKE_CURRENT_SOURCE_DIR}/../astro_image_io/
  astro_image_io.dll)` 硬编码相对路径链接。
- 该库由 `lib/astro_image_io/build.ps1`（Windows PowerShell）构建，产物**预编译并提交**，Linux
  工具链**无源码重建入口**。

## 2 结论

- **fPIC / shared-library 功能正常**（共享库可加载、符号可解析、测试通过）⇒ BLD-002 的
  "shared-library 可用" 部分满足。
- **可移植/可复现缺口**（BLD-002 PORTABILITY + BLD-003 WINDOWS-LEAK）：
  1）Linux 共享库以 **Windows `.dll` 命名**（实为 ELF）；
  2）由 **Windows PowerShell `build.ps1`** 构建、**预编译提交**，Linux CMake 无法从源码重建；
  3）phase2 CMakeLists **硬编码相对路径**链接该 `.dll`。
  这都是 Linux 构建中的 Windows 风格泄漏 ⇒ **可移植性/可复现性不达标**。

## 3 建议修复（各自独立 Task/commit）

- BLD-002：把 AIO 库以标准名产出（Linux 用 `libastro_image_io.so`、用 CMake
  `add_library(... SHARED)`），并提供 Linux 源码构建入口，替代 `build.ps1`。
- BLD-003：移除 phase2 CMake 中各 test target 对 `astro_image_io.dll` 的硬编码链接，改为
  CMake target（`target_link_libraries(... astro_image_io)`）链接，删 Windows-only 残留。
- 说明：考虑到改动涉及 AIO 构建体系（新增 Linux 源码构建 + 改名），须在独立 commit 内完成并
  重跑 TST-001/002 验证。当前记录为 IN_PROGRESS，未冒进改名以免破坏现有可运行 build。
