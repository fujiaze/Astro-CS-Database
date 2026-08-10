# ADR-008: CMake FetchContent 依赖拉取策略

| 项目 | 值 |
| --- | --- |
| 状态 | Accepted |
| 日期 | 2026-08-02 |
| 依赖锁 | `lib/acr/docs/dependency-lock.json` |
| 机制 | CMake `FetchContent_Declare` + `FetchContent_MakeAvailable` |
| 缓存目录 | `build/_deps`（gitignored，不入仓） |
| 平台 | Windows / Linux |

## 状态

Accepted。所有 ACR 第三方库通过 CMake FetchContent 拉取，GIT_TAG 钉死 release tag，GIT_SHALLOW TRUE，缓存到 `build/_deps`（gitignored，不入仓）。CPU-only 构建不拉取任何 GPU 依赖。

## 背景

ACR 依赖多个第三方库（alpaka、oneTBB、hwloc、cpu_features、Google Benchmark、GoogleTest、CLI11、nlohmann/json、spdlog、fmt）。依赖来源必须满足：

1. **可追溯**：每个库的版本与 commit 固定，构建可复现。
2. **不入仓**：第三方源码不进入 ACR 仓库，避免仓库膨胀。
3. **CPU-only 友好**：无 CUDA/ROCm/oneAPI SDK 的普通用户构建必须成功，不拉取 GPU 相关依赖。
4. **离线二次构建**：首次联网下载后，二次构建使用缓存，无需联网。

候选方案中，vcpkg 与 Conan 在用户机器未安装；MSYS2 pacman 部分库（alpaka、cpu_features）无包；手动 third_party 入仓会导致仓库膨胀且升级困难。CMake FetchContent 是 CMake 原生机制，无外部工具依赖，与 ACR 的 CMake 构建链天然契合。

## 决策

1. 所有第三方库通过 `FetchContent_Declare` + `FetchContent_MakeAvailable` 拉取。
2. `GIT_TAG` 钉死 release tag（如 `1.2.0`、`v0.9.0`），**禁止跟踪 `main`/`develop`/`master`**。
3. `GIT_SHALLOW TRUE`，仅拉取指定 tag 的浅克隆，减少下载量。
4. 缓存目录 `build/_deps`，在 `.gitignore` 中排除，**不入仓**。
5. CPU-only 构建不拉取 GPU 依赖：`ACR_BUILD_CUDA=OFF` 时不 `FetchContent` 任何 CUDA 相关库（CUDA Toolkit 本身为系统级，非 FetchContent 拉取）。
6. 版本锁定信息集中维护在 `lib/acr/docs/dependency-lock.json`，CMake 脚本从中读取或与之保持一致。
7. 首次构建联网下载；离线二次构建使用 `build/_deps` 缓存。

## 理由

- CMake 原生机制，无外部工具依赖，跨平台一致。
- GIT_TAG 钉死保证可复现构建，避免上游 breaking change 影响 ACR。
- GIT_SHALLOW 减少下载量与磁盘占用。
- 缓存目录 gitignored，仓库保持精简。
- 与 ADR-009 的 CPU-only 构建门禁协同：按 `ACR_BUILD_*` 开关条件性拉取，避免无谓下载。
- dependency-lock.json 作为单一可信源，CMake 脚本与之同步，避免版本漂移。

## 集成边界

- **职责内**：第三方库源码拉取、版本锁定、缓存管理、条件性拉取（按 `ACR_BUILD_*`）。
- **职责外**：第三方库的构建配置（由各库自带 CMakeLists 负责）、系统级 SDK（CUDA Toolkit / ROCm / oneAPI，由用户提供，非 FetchContent）。
- **缓存边界**：`build/_deps` 为唯一缓存目录，禁止散落到 `third_party/` 等位置；`.gitignore` 必须覆盖。
- **版本边界**：所有 `GIT_TAG` 必须与 `dependency-lock.json` 一致，CI 校验。
- **离线边界**：CI 与开发者机器二次构建不依赖网络；首次构建需联网或预置缓存。

## 替代方案

1. **vcpkg**：
   - 未采用：用户机器未安装 vcpkg；引入需额外环境配置，违反"普通用户 clean machine 可构建"原则。
2. **Conan**：
   - 未采用：用户机器未安装 Conan；Python 依赖与额外配置增加门槛。
3. **MSYS2 pacman**：
   - 未采用：部分库（alpaka、cpu_features）无 MSYS2 包；版本不可控（跟随 MSYS2 仓库）；Windows 专属，Linux 不可用。
4. **手动 third_party 目录入仓**：
   - 未采用：仓库膨胀严重；升级需手动替换文件；版本追溯困难；违反"不入仓"原则。
5. **Git submodule**：
   - 未采用：submodule 初始化需额外步骤（`git submodule update --init`）；浅克隆支持弱；与 FetchContent 相比无优势且体验更差。

## 未采用原因

vcpkg / Conan 未安装且增加环境门槛；MSYS2 pacman 覆盖不全且 Windows 专属；手动入仓违反仓库精简原则；Git submodule 体验与浅克隆支持不及 FetchContent。

## 验收实验

| 实验 | 目标 | 通过条件 |
| --- | --- | --- |
| 首次构建联网下载 | clean `build/_deps` 后构建 | 所有依赖自动下载，构建成功 |
| 离线二次构建 | 断网后二次构建 | 使用 `build/_deps` 缓存，构建成功，无网络请求 |
| 版本锁定可追溯 | 校验 CMake GIT_TAG 与 dependency-lock.json | 所有 tag 一致，CI 校验通过 |
| CPU-only 不拉 GPU 依赖 | `ACR_BUILD_CUDA=OFF` | `build/_deps` 中无 CUDA 相关库（CUDA Toolkit 为系统级，不在 _deps） |
| 缓存不入仓 | `git status` | `build/_deps` 不出现在 untracked 列表 |
| GIT_SHALLOW 生效 | 检查克隆深度 | 浅克隆，无完整历史 |

构建日志写入 `run/logs/acr/build/<YYYYMMDD>/`。
