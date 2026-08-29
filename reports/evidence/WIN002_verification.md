# WIN-002 验证报告 — MSVC amd64 clean Debug/Release 构建零错误零失败

结论: **PASS**(Debug + Release 均 MSVC 零错误构建; exe 运行 doctor PASS; CLI 协议测试 10/10 OK; 无忽略 Error)。

## 1. 验收判据(03_TASK_DETAILS.md L146)
> MSVC amd64 clean Debug/Release; CTest/CLI basic; 不得忽略 Error。
> PASS = exit code 全 0、0 failed tests、0 ignored build errors。

## 2. 环境
- 节点: **Fatduck**(`fujia@100.104.10.71`), Windows, amd64。
- 工具链: VS18 BuildTools, MSVC **14.50.35717**(vcvarsall x64), CMake 4.3.2(`C:/msys64/mingw64/bin/cmake.exe`)。
- 仓库: `C:/Users/fujia/Astro-CS-Database`, main SHA **242a42f**(本任务最后源码为 cb35b8b; 242a42f 仅改测试取消的 Windows 兼容)。

## 3. 构建结果
| 配置 | 错误数(error) | 产物 | 运行 |
|---|---|---|---|
| Debug | **0** | `build/win_dbg/Debug/astrocs.exe` | `--version` exit 0; `doctor --json` **PASS** |
| Release | **0** | `build/win_rel/Release/astrocs.exe` | `--version` exit 0; `doctor --json` **PASS** |

- 构建日志错误数: Debug `0`, Release `0`(以 `Select-String "error [A-Z0-9]+"` 计数)。
- 链接成功生成 `astrocs.exe`(无 LNK 错误)。
- 版本输出: `astrocs 0.9.0-alpha.1+g242a42f14d79.dirty`(build_id 为当前 SHA; `.dirty` 因测试生成的 run-manifest 文件存在, 见 §6 限制)。

## 4. CLI 协议测试(CLI basic)
- `tests/cli/test_cli_protocol.py`(unittest): **Ran 10 tests ... OK**(0 failed)。
  覆盖: help 精确树、--version --json、parser reject、config init/validate、JSONL 合同、cancel→退出码9(无伪造产物)、崩溃边界→70 且信息清洗、Unicode 路径、退出码单一来源。
- `tests/cli/test_monitor.py`: **OK (skipped=4)** — 该套仅 Linux+g++ 可测，Windows 上整体跳过, 无失败。
- 未忽略任何 Error(诊断/构建错误均未见忽视项)。

## 5. 为达成 MSVC 构建所做的源码/构建改动(均已提交)
- `lib/backend_host/backend_loader.cpp`: `LoadLibraryExA` 搜索 flags 防御化(LOARD_LIBRARY_SEARCH_* 视 `_WIN32_WINNT` 而定)。
- `lib/backend_host/hardware_inspect.cpp`: 修正 MSVC `__cpuidex` 调用签名。
- `cli/main.cpp`: windows.h 前 `#define NOMINMAX`。
- `cli/CMakeLists.txt`:
  - 仅 cfitsio 需 zlib; zstd/lz4 在 CLI 走 fallback(HAS_* 未定义)。
  - zlib 由 MSVC 从源码构建(静态 `libzs.lib`), 避免运行时 `libz.dll` 缺失。
  - MSVC 编译定义 `_WIN32_WINNT=0x0A00;NOMINMAX;_USE_MATH_DEFINES`(M_PI)。
  - OS 分路 OpenMP(`/openmp` vs `-fopenmp`)、链接库分路、`/W4;/EHsc;/utf-8`。
  - cfitsio 源 include `win_compat/unistd.h` shim。
  - sampler.cpp 单文件 `/EHa`(C++ try/catch 捕获结构化异常)。
- `lib/phase1_session/p1_session.cpp`: 移动 read_image/image_w/h/px 辅助到 `extern "C"` 外(C-linkage 不能返回 `unique_ptr` → MSVC C2526)。
- `lib/phase3_session/p3_output.cpp`: `unistd.h` 缺失 → `io.h/process.h/direct.h` + POSIX→`_` 前缀宏; `gethostname`→`GetComputerNameA`。
- `lib/phase3_session/hips_properties.cpp`: MSVC 无 dirent, 用 `lib/common/dirent_win.h` shim(opendir/readdir/closedir,FindFirstFile)。
- `lib/phase3_session/p3_resample.cpp/p3_wcs.cpp`: M_PI 由 `_USE_MATH_DEFINES` 提供。
- `lib/phase2/include/astro/phase2/upm.h`: 补 `<cstddef>`(`std::size_t`)。
- `lib/phase2/src/integrate.cpp`: 补 `<algorithm>`(`std::max`)。
- `lib/phase2/src/coverage.cpp`: parse_props 移出 extern "C"(返回 std::map)。
- `lib/phase2/src/sampler.cpp`: MSVC 禁 `__try` 与 C++ 对象展开混用(C2712)→ `try/catch(...)` + `/EHa`。
- `tests/cli/test_cli_protocol.py`: test_07 cancel 兼容 Windows(`send_signal(SIGINT)` 在 Windows 不支持 → `CTRL_BREAK_EVENT`+`CREATE_NEW_PROCESS_GROUP`)。
- 新增: `lib/common/dirent_win.h`、`lib/astro_image_io/third_party/cfitsio/win_compat/unistd.h`。

## 6. 限制 / 遗留
- **Runtime CRT**: Fatduck 未装 VC redist(非仓库问题); 为运行测试, 将 Debug/Release CRT DLL(vcruntime140*、msvcp140*、vcomp140*、ucrtbased*)复制到 exe 目录。发布安装包需自带/依赖 VC redist(09 §——按发布规范处理)。
- **zlib**: 因 msys64/mingw64 只有 GCC 版 `.a` 且无 zlib, 改由 MSVC 从源码构建静态 zlib(`C:/Users/fujia/zlib-msvc`)。生成在 Fatduck(本地构建产物, 不入仓库)。
- **测试 artifacts**: `astrocs_run_*.json`(取消/崩溃边界测试输出)留在 Fatduck 工作区, 导致 `--version` build_id 带 `.dirty`; 不影响构建正确性, VERSION 记录为准。
- **test_monitor / benchmark / 全 backend 安全加载 / 真实数据**: 属 WIN-003 及后续(WIN-002 仅限构建 + CLI basic)。

## 7. 证据文件
- 构建日志: Fatduck `C:/Users/fujia/build_Debug.log`、`build_Release.log`(均 0 error)。
- exe SHA256(Release): `8350C6E2F5C1270A09F681D6310A0A9E189E901D621F4DE7D776B973EB76B4CD`(旧构建 cb35b8b 产物)。
- doctor: `{"verdict":"PASS"}`(baseline_selftest / hardware_sanity / backends_manifest 全 pass)。
