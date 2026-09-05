# TOOLCHAIN_AGENT_HOST — Agent 主机工具链快照（V81-ADOPT-004）

- 任务：`V81-ADOPT-004`（owner=`SA-ADOPT-31`，mode=write）
- 生成依据：`ci/toolchain.lock.json`（schema_version=2，scope=`agent-host`）
- 证据日志：`evidence/v8_1_ci_control/tasks/V81-ADOPT-004/logs/`（每项命令 stdout/stderr 原文 + `commands.jsonl`）
- 采集方式：全部为**本机实测**（`command -v` 探测路径 + `--version` 采集输出，外部命令均带 `timeout` 并落盘日志）；缺失工具如实标注 `missing`，未安装、未重配服务器。
- 盘点基准 SHA：`98c2354fcd59423e80ae9592f19b5b9094d20fc1`（main）

## 1. 工具链实测快照（12 项盘点 + 1 项附加观察）

| 工具 | 版本 | 路径 | 来源命令 | 状态 |
|---|---|---|---|---|
| cmake | — | — | `cmake --version`（exit 127，未找到命令） | **missing** |
| gcc | — | — | `gcc --version`（exit 127） | **missing** |
| g++ | — | — | `g++ --version`（exit 127） | **missing** |
| clang | — | — | `clang --version`（exit 127） | **missing** |
| clang++ | — | — | `clang++ --version`（exit 127） | **missing** |
| ninja | — | — | `ninja --version`（exit 127） | **missing** |
| make | — | — | `make --version`（exit 127） | **missing** |
| python3 | 3.13.5 | `/usr/bin/python3` | `python3 --version` → `Python 3.13.5` | present |
| git | 2.47.3 | `/usr/bin/git` | `git --version` → `git version 2.47.3` | present |
| ccache | — | — | `ccache --version`（exit 127） | **missing** |
| zstd | 1.5.7 | `/usr/bin/zstd` | `zstd --version` → `*** Zstandard CLI (64-bit) v1.5.7, by Yann Collet ***` | present |
| pytest | — | — | `pytest --version`（exit 127）；`python3 -m pytest --version` → `No module named pytest` | **missing** |
| （附加）xz | 5.8.1 | `/usr/bin/xz` | `xz --version` → `xz (XZ Utils) 5.8.1` | present |

缺失工具合计：`cmake, gcc, g++, clang, clang++, ninja, make, ccache, pytest`（9 项，见 lock `missing_tools`）。

**缺失复核（非臆造依据）**：PATH 为 `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin`；`which -a` 多路径、`/usr/local/bin`、`/opt`、`/snap`、`~/.local/bin` 目录枚举、版本化名称（`gcc-14`、`clang-19` 等）扫描、`compgen -c` 全命令扫描、`python3 -m cmake` / pip list、`/usr/lib/llvm-*` 定向探测、`find /usr /opt /usr/local /snap`（maxdepth 3）均无命中。`dpkg -l` 仅存在 `gcc-14-base`（GCC 运行时基础包，非编译器驱动）与 `libgcc-s1`。详见 `logs/deep_probe.log`。

## 2. 仓库既有工具链约束（只读取证，未重新配置）

| 文件 | 观察到的约束 |
|---|---|
| `CMakePresets.json` | `cmakeMinimumRequired 3.31.0`（presets version 10）；configure presets：`base-msvc`（hidden）、`win-msvc-17.14.39-x64`（Windows 正式：VS 17 2022 / x64 / v143 14.44.35207 / SDK 10.0.26100.0 / cmake_pin 3.31.12）、`linux-control`（Linux 控制节点轻验证，generator `Unix Makefiles`，仅非 Windows 主机条件生效） |
| `CMakeLists.txt` | `cmake_minimum_required(VERSION 3.24)`；`CMAKE_CXX_STANDARD 17` + `REQUIRED`；sanitizer 仅 GCC/Clang（MSVC 不支持）；MSVC `/utf-8` 分支；OpenMP 在 UNIX 且找到时链接；版本单源 `VERSION`（VER-001） |
| `AGENTS.md` | Linux amd64 = Agent 执行与控制节点（控制、静态分析、轻量编译、小合成实验），bash+git+Linux 工具链；Windows x64 = 正式开发/客户端/发布平台，经远程节点执行，离线不阻塞 Linux 任务；外部命令须带 timeout 并保存日志 |
| `build.sh` | BLD-001 根级 Linux configure/build/test 入口；依赖 `cmake`（默认生成器）与 `nproc`；以 `P2_ENABLE_OPENMP=ON` 构建 `lib/phase2` 各测试目标 |
| `toolchain.ps1` | Windows 侧统一工具链入口（MSYS2 MinGW64 `C:\msys64\mingw64\bin`、正式 Python 3.12 路径、gh CLI；`check`/`build` 用 g++/make）。属 Windows 主机关注点，不适用于本 Linux Agent 主机盘点 |

## 3. 与 hosted CI 版本策略的关系

策略文件 `ci/toolchain.policy.json`（自控制包原样复制，SHA256 `7c66e1a5ff33c192653b60f2b71ffbd856d1af6821b520f8c301ae98d2881469`）的 `agent_host` 段规定：

| 策略项 | 值 | 本机对应状态 |
|---|---|---|
| `purpose` | `static_analysis_light_build_and_control` | 与 AGENTS.md 对 Linux 节点的定位一致 |
| `require_exact_hosted_versions` | `false` | lock `hosted_ci_versions_required=false`：**不要求本机与 hosted CI 版本一致** |
| `host_reprovisioning` | `false` | lock 同值：不做主机重配置/重开新布局 |
| `acr` | `false` | lock 同值：不做 ACR |
| `missing_optional_tools` | `record_and_continue_independent_tasks` | 9 项 missing 已如实记录，不依赖任务继续 |

- hosted CI 的版本参考（`linux_hosted`: ubuntu-24.04 / gcc-14 / clang-19 / cmake 3.31.12 / Ninja；`windows_hosted`: windows-2022 / VS 17 / v143）**仅为策略记录，不代表本机已安装**；本 lock 中没有任何一个 hosted 版本被写成已安装值（验收脚本含负向检查）。
- 本机 cmake/gcc 等构建驱动缺失，意味着 Agent 主机当前**只能执行控制、静态分析、Python/git 类任务**；`linux-control` preset 与 `build.sh` 的实际 configure/build 需要构建工具链就位后才能运行。按策略这**不阻塞**独立任务（记录并继续），也不触发主机重配置。

## 4. 结论

1. 本机可用：`python3 3.13.5`、`git 2.47.3`、`zstd 1.5.7`（附加观察 `xz 5.8.1`）——满足控制/静态/取证类任务需求。
2. 缺失 9 项构建/测试工具（cmake、gcc、g++、clang、clang++、ninja、make、ccache、pytest），全部如实登记于 `ci/toolchain.lock.json` 的 `missing_tools`，**未安装任何组件、未重新配置服务器**，符合 `host_reprovisioning=false`。
3. 版本数据 100% 来自实测输出（`evidence/.../logs/*.log` 原文可复核），无臆造、无占位值；验收脚本 `ci/verify_toolchain.py --policy ci/toolchain.policy.json --actual ci/toolchain.lock.json --scope agent-host` 通过（exit 0）。
4. Agent 主机与 hosted CI 版本不要求一致（policy `require_exact_hosted_versions=false`）；后续任务若需要本机编译，须由控制面在遵守该策略的前提下另行处理，本任务只负责如实盘点。
