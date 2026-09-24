# DEPENDENCIES.md — 工具链版本锁定 (QA-005 / BLD-001)

本文件是 Astro Celestial Sphere Database（ACSD） 工具链与依赖的版本锁定权威（仓库根）。Windows x64 发布
工具链以控制包 `09_WINDOWS_TOOLCHAIN_LOCK.md` 为唯一编译依据；机器校验入口
`eng/cmake/toolchain/verify_toolchain.py`，冻结事实合同
`eng/packaging/schemas/preset-contract.json`。

## Windows x64 发布工具链（冻结，BLD-001；Alpha 0.1.0）

| 组件 | 冻结值 | 说明 |
|------|--------|------|
| 目标架构 | AMD64/x86-64 | 不构建 Win32/ARM/ARM64 |
| 技术兼容下限 | Windows 10 22H2 x64 (OS build 19045) | 主验证环境为 Windows 11 x64 |
| Visual Studio | VS 2022 Build Tools `17.14.39` | installationVersion `17.14.37614.0` |
| 正式 generator | `Visual Studio 17 2022` | 唯一正式 generator（VS17 2022），`-A x64`，host tool `x64` |
| platform toolset | `v143,host=x64,version=14.44.35207` | compiler family `19.44` |
| Windows SDK | `10.0.26100.0` | servicing bundle `10.0.26100.9169`（CMake target 不变） |
| CMake | `3.31.12` x64 | 本轮不使用 CMake 4.x |
| C++ 标准 | C++17 | 本轮不升 C++20 |
| CRT | Debug `/MDd`；Release/RelWithDebInfo `/MD` | 全 EXE/DLL/第三方一致；禁 `/MT` |
| 浮点 | `/fp:precise`；science oracle `/fp:strict` | 禁 `/fp:fast` |
| 优化 | `/O2` | Alpha 禁 `/GL`、LTCG、PGO |
| LLVM/clang-tidy | `19.1.5` | 仅辅助静态检查，不产发布二进制 |
| 验证脚本 Python | `3.12.10` x64 | 仅测试/打包，非产品运行依赖 |
| VS 组件清单 | `eng/packaging/windows/.vsconfig`（7 组件） | 见 09 §4，禁 `--includeRecommended` |
| preset 合同 | `eng/packaging/schemas/preset-contract.json` | verifier 单一事实源 |
| verifier | `eng/cmake/toolchain/verify_toolchain.py` | preset/.vsconfig 漂移 FAIL fast |

正式 preset：`win-msvc-17.14.39-x64`（`CMakePresets.json`）——Windows 工具链取值以该 preset 的显式声明为来源。preset 边界之外的取值面（VS 2026/v144/v145、17.14 evergreen latest、CMake 4.x、Ninja 以外的 Windows generator、MinGW/MSYS2、`/MT`、`/fp:fast`、全局 `/arch:AVX*`、固定 `-j16`/`/m:16`、Windows 11-only API）不属正式工具链面。

## Linux 控制节点（轻验证；非发布产物）

| 组件 | 版本 | 用途 |
|------|------|------|
| GCC (g++/gcc) | 14.2.0 | Release 轻验证构建 |
| Clang (clang++) | 18 (Debian 13) | Debug/static analysis |
| CMake | 3.31.6（控制节点现值） | 构建系统；preset `linux-control` 轻验证 |
| Python | 3.13（控制节点）/ 3.12.10 x64（Windows 验证脚本） | 工具链/测试脚本 |
| cfitsio | vendored (lib/infrastructure/aio/third_party/cfitsio, 60 源显式清单) | FITS I/O (第三方) |
| PyYAML | installed | 控制包解析 |

Linux preset `linux-control` 仅供静态检查/轻量编译/小合成实验；Linux 性能
不作为 Windows 发布性能结论。不固定 Ninja/MinGW，不写死并行度。

## 构建 flags (Release, Linux 轻验证)
- CMAKE_BUILD_TYPE=Release (GCC)
- -O3 默认; 无未锁定 flag; 无全域 -w
- Debug: -DCMAKE_BUILD_TYPE=Debug (clang++)

## 复现
- build id = VERSION + g<commit> (cli/version_generated.h.in)
- 同 commit 重构建 → 相同 build id
- SBOM: 输入 = build/sbom-input.jsonl（`eng/packaging/gen_sbom_input.py` 现行产物, 不入库）;
  发布候选 SBOM 文档在发布流程内生成（发布决定权属负责人）

## 依赖锁定 (BLD-004)

依赖权威锁文件：`eng/packaging/dependency-lock.json`（schema:
`eng/packaging/schemas/dependency-lock.schema.json`）。机器校验入口：
`eng/packaging/gen_sbom_input.py --root .`（lock <-> 本文一致性 + fresh
configure 无机器绝对路径扫描 + SBOM 输入生成）。

### 生产依赖 (vendored/系统标准库)
- cfitsio `4.6.4` — vendored `lib/infrastructure/aio/third_party/cfitsio`
  (60 C 源显式清单, BLD-001 禁 GLOB; 树内 168 git 追踪文件); 来源 heasarc;
  hash + 目录级聚合哈希见 lock（`eng/packaging/gen_sbom_input.py` 每次复算）。
- nlohmann-json `3.12.0` — vendored `lib/third_party/nlohmann/json.hpp`
  （单头）; 来源 nlohmann/json (MIT); hash 见 lock。**当前树零引用**
  （lock `reference_status=UNREFERENCED` + 理由; 有机器判据）。
- zlib / zstd / lz4 — Linux 链接系统发行版库; MSVC 经 `ACS_ZLIB_ROOT`
  显式 cache 变量 (默认空, 不硬编码用户路径), 无则 cfitsio zcompress
  路径降级。
- kernel32 — MSVC 侧 `win32_pthread_shim` 的 SRWLock 宿主（系统 DLL）。
- OpenMP / Threads (pthread) / libm / libdl — 平台标准, 版本随工具链。

### test-only oracle (仅测试, 非产品运行依赖)
- astropy (>=5, 控制节点 7.0.1) — FITS/天体测量科学 oracle
- numpy (控制节点 2.2.4) — 数值 oracle
- pytest (控制节点 8.3.5) — 契约测试运行器
- astropy-healpix (控制节点 2.0.1) — HEALPix 几何独立 oracle
- rcr `2.4.7` — 稳健 Cat 率回归独立 oracle (`rcr_oracle_compare.py`)
- astrometry.net — 外部求解器 (仅 P* 集成 oracle)

### 机器路径政策 (BLD-004)
`machine_absolute_path: FORBIDDEN` (F:/、C:/Users/<user>、/home/<user>
只出现在非构建输入面); `msys2_mingw: FORBIDDEN`; `vcpkg: NOT_USED`
(若引入必须 manifest + baseline)。冻结 Windows 工具链安装约定
`C:/AstroCS/toolchains/...` 由 preset 显式声明, 属白名单例外。
遗留 Windows 开发脚本 (eng/build/toolchain.ps1 等) 含 C:\msys64 / C:\Users\fujia
→ 非 CMake 构建输入, 由 WIN-*/CLI 系列清理或归档 (known_limits)。
