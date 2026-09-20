# Build Graph (T304)

> 生产构建图: target/source/define/link 与 CMake File API/compile_commands.json 对应

> ⚠ **DOC-202 订正（R18，2026-09-20）——生产构建不得链入 ACR/CUDA**：
> 最高设计 §8「**禁止**生产目标编译或链接 ACR 的任何源文件；**禁止**任何 GPU 路由开关与
> 第二个可执行入口」；ACR 状态 = `DORMANT`（保留源码与隔离测试，不进生产构建/加载/路由/
> benchmark/发布）。因此 **§1/§2/§3/§4 中的 ACR 源集、ACR 编译定义、ACR 链接行与
> `astrocs-stage2` 目标均不是生产构建面**（下表已逐行标注）。
> **代码侧整改（从生产源集与链接中移除 ACR）归代码任务**；本文件为文档侧如实登记。

## 1 顶层 targets

| Target | Type | Sources | CMakeLists |
|---|---|---|---|
| phase2（**现状，含 ACR 违规**） | STATIC | src/upm.cpp, stage2_common.cpp, rejection.cpp, coverage.cpp, sampler.cpp, block.cpp, integrate.cpp, common/healpix_core.cpp, common/crypto/sha256.cpp —— ⛔ **现状源集另含 `acr_kernels.cpp` / `acr/api/kernel_registry.cpp` / `acr/backends/cuda/cuda_bridge_loader.cpp` / `acr/scheduler/device_executor.cpp`，违反最高设计 §8，须从生产源集移除（代码侧整改）** | lib/algorithms/coverage/CMakeLists.txt |
| ~~astrocs-stage2~~（**非发布入口**） | EXEC | tools/stage2.cpp —— 旧 Phase2 CLI，**不是入口**（最高设计 §6.2 唯一命令树 / §7.1「旧可执行程序不是入口」）；保留仅为历史/工具面，**不得作为发布目标** | lib/algorithms/coverage/CMakeLists.txt |
| calibrated_pair_diag | EXEC | tools/calibrated_pair_diag.cpp | lib/algorithms/coverage/CMakeLists.txt |
| rejection_cli | EXEC | tools/rejection_cli.cpp | lib/algorithms/coverage/CMakeLists.txt |
| phase2_synthetic_gate | TEST | tests/synthetic_gate.cpp | lib/algorithms/coverage/CMakeLists.txt (if GTest) |
| orchestrator.exe | EXEC | cpp/src/main.cpp, orchestrator.cpp, cli_command.cpp | lib/infrastructure/pipeline/orchestrator/cpp/CMakeLists.txt |
| astro_image_io.dll | SHARED | src/*.cpp + hips/* + cfitsio | lib/infrastructure/aio/CMakeLists.txt |
| hepix_drizzle | STATIC/SHARED | healpix_drizzle/*.cpp | lib/algorithms/drizzle/healpix_drizzle/CMakeLists.txt |

## 2 Compile definitions

| Define | Target | Source | 证据 |
|---|---|---|---|
| P2_ENABLE_OPENMP=ON | phase2 | `option(P2_ENABLE_OPENMP OFF) hard-disable` → `if(P2_ENABLE_OPENMP AND OpenMP_CXX_FOUND) target_link OpenMP::OpenMP_CXX` | lib/algorithms/coverage/CMakeLists.txt:18,54 |
| OpenMP_CXX_FOUND=FALSE when OFF | phase2 | `set(OpenMP_CXX_FOUND FALSE)` when OFF | lib/algorithms/coverage/CMakeLists.txt:28 |
| ~~ACR_BUILD_CUDA=OFF default~~ **DORMANT（非生产）** | acr | `option(ACR_BUILD_CUDA OFF)` —— ACR 整体 `DORMANT`，**不进生产构建**（最高设计 §8） | lib/infrastructure/acr/CMakeLists.txt |

## 3 Link libraries

| From | To | Via |
|---|---|---|
| ~~astrocs-stage2~~（**非发布入口**） | phase2 + astro_image_io.dll | `target_link_libraries(astrocs-stage2 PRIVATE phase2 astro_image_io.dll)` lib/algorithms/coverage/CMakeLists.txt:73 —— 该目标**不是入口**（最高设计 §6.2/§7.1） |
| phase2 (when ON) | OpenMP::OpenMP_CXX | `target_link_libraries(phase2 PUBLIC OpenMP::OpenMP_CXX)` |
| ~~acr_cuda_bridge.dll~~ **DORMANT（禁止链接生产目标）** | ~~phase2 executables~~ | POST_BUILD copy to TARGET_FILE_DIR if EXISTS —— **生产构建不得链入 ACR/CUDA**（最高设计 §8），该 POST_BUILD 拷贝须移除（代码侧整改） |

## 4 File-API / compile_commands 对应

| 声明 | File-API codemodel target | compile DB |
|---|---|---|
| phase2 STATIC | `phase2` reply `targetSources + compileGroups` | `compile_commands.json` entries for src/*.cpp with `__cplusplus=202002L` |
| astrocs-stage2 EXEC | `astrocs-stage2` target | `tools/stage2.cpp` compile command |
| orchestrator EXEC | `orchestrator` target | `cpp/src/*.cpp` commands |

验证: `cmake --build --verbose` 显示命令含 `-std=c++20` + `-D` + `-I` 与声明一致; File-API `reply/codemodel-v2-*.json` 的 `targets[].sources` 与上表一致 (T600 contracts configure 时落地)。

## 5 验证方法

```sh
cmake -S lib/algorithms/coverage -B build/linux-release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DP2_ENABLE_OPENMP=OFF
cmake --build build/linux-release --verbose | grep phase2
cat build/linux-release/compile_commands.json | python3 -m json.tool | grep -c "phase2/src"
cat build/linux-release/.cmake/api/v1/reply/codemodel-v2-*.json | python3 -m json.tool | grep target
```

## 6 ARC 映射

- ARC-BUILD-001: phase2 STATIC 源集
- ARC-BUILD-002: P2_ENABLE_OPENMP compile/linker 编排
- ARC-BUILD-003: acr_cuda_bridge.dll 运行时投递
