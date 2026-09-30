# Dependency Rules

> 上游：docs/ACSD_DESIGN.md §8（软件架构）、§12.5（状态阶梯（唯一口径））

- 唯一 I/O 依赖方向：上层模块 → aio（文件级唯一 I/O 边界）；aio 不依赖
  上层科学模块（最高设计 §10）。
- 共享面（`acsd_common`，含 SHA-256 与共享 HEALPix 核心）可被任何模块依赖，依赖方向单向。
- healpix_drizzle 依赖共享 HEALPix 核心，实现单源 = `lib/algorithms/shared/healpix/healpix_core.cpp`（CMake target `acsd_common`）。
- phase2 依赖共享 HEALPix 核心 + aio 的产品读写面（`aio_upm` / `aio_hips_reader`）。
  ⛔ **生产构建采用无 ACR/CUDA 的链接面**（最高设计 §1.4 非目标：ACR/GPU 生产路由
  生产不可达）。ACR 为 `DORMANT`：保留源码与隔离测试，
  **不进生产构建/加载/路由/benchmark/发布**（最高设计 §1.4；状态词唯一口径 = 最高设计 §12.5）。
- 唯一动态加载路径 = 安全 loader：加载前过固定检查（CPU 特征、OS 可安全执行状态、manifest、哈希、ABI），
  只认清单授权的绝对路径，失败只报错、不回退搜索（最高设计 §8.4）。

- 依赖图为有向无环图；模块产品输出落块级 `output_dir`（最高设计 §10），开发/CI 过程产物落 `run/`，testdata/ 只读。
- 归档面（`lib/infrastructure/aio/healpix_db/`）不进生产构建、不重建；其内容改动一律走变更流程。
- Python 仅限带 NON_PRODUCTION_TOOL_ONLY 标记的测试/研究脚本。

## 第三方依赖与工具链锁定的读取面

本节只规定「值在哪里、谁说了算」，不复制取值本身。

| 面 | 机器单一事实源 | 登记镜像 | 校验入口 |
|---|---|---|---|
| Windows x64 发布工具链（generator、toolset 与其版本、VS Build Tools 与 installationVersion、SDK 与 servicing bundle、CMake、C++ 标准、CRT、VS 安装位置、preset 禁面） | `eng/packaging/schemas/preset-contract.json` | `eng/packaging/windows/README.md` | `eng/cmake/toolchain/verify_toolchain.py` |
| VS 安装组件清单 | `eng/packaging/windows/.vsconfig` | `eng/packaging/windows/README.md` | `eng/cmake/toolchain/verify_toolchain.py` |
| 生产依赖与系统库锁定（vendored 清单与哈希、zlib/zstd/lz4、kernel32 与线程 shim、OpenMP、Threads、机器路径政策） | `eng/packaging/dependency-lock.json`（schema：`eng/packaging/schemas/dependency-lock.schema.json`） | — | `eng/packaging/gen_sbom_input.py --root .` |

- **冲突以机器源为准**：登记镜像、文档与本文一律不得声明第二套取值；镜像与机器源不一致时按机器源订正镜像；
- 机器路径政策：CMake 与构建输入不得读取 `F:/`、`C:/Users/<user>`、`/home/<user>` 等机器绝对路径（fresh configure 必须可复现）；Windows 正式工具链的安装位置由 preset 显式声明，属白名单例外；MSYS2/MinGW 依赖面禁止；`vcpkg` 不使用，引入时须同时以 manifest 与 baseline 双重锁定并在本锁文件登记；
- CRT 取值由 preset 合同冻结：Debug 用动态调试版 CRT，Release 与 RelWithDebInfo 用动态版 CRT，全部可执行文件、动态库与第三方一致；**静态 CRT 禁止**；
- MSVC 侧的 zlib 由显式 cache 变量指向（默认空，不硬编码用户路径）；未提供时 cfitsio 不编译压缩路径，属已登记的降级而非缺依赖；
- 依赖锁文件的每次复算同时产出 SBOM 输入清单；该清单是过程产物，不入库（`run/` 与构建目录不入库，最高设计 §10）。
