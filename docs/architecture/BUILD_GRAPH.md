# Build Graph (T304)

> 生产构建图: target/source/define/link 与 CMake File API/compile_commands.json 对应

## 1 顶层 targets

| Target | Type | Sources | CMakeLists |
|---|---|---|---|
| phase2 | STATIC | src/upm.cpp, stage2_common.cpp, rejection.cpp, coverage.cpp, sampler.cpp, block.cpp, integrate.cpp, acr_kernels.cpp, acr/api/kernel_registry.cpp, acr/backends/cuda/cuda_bridge_loader.cpp, acr/scheduler/device_executor.cpp, common/healpix_core.cpp, common/crypto/sha256.cpp | lib/phase2/CMakeLists.txt |
| astrocs-stage2 | EXEC | tools/stage2.cpp | lib/phase2/CMakeLists.txt |
| calibrated_pair_diag | EXEC | tools/calibrated_pair_diag.cpp | lib/phase2/CMakeLists.txt |
| rejection_cli | EXEC | tools/rejection_cli.cpp | lib/phase2/CMakeLists.txt |
| phase2_synthetic_gate | TEST | tests/synthetic_gate.cpp | lib/phase2/CMakeLists.txt (if GTest) |
| orchestrator.exe | EXEC | cpp/src/main.cpp, orchestrator.cpp, cli_command.cpp | lib/orchestrator/cpp/CMakeLists.txt |
| astro_image_io.dll | SHARED | src/*.cpp + hips/* + cfitsio | lib/astro_image_io/CMakeLists.txt |
| hepix_drizzle | STATIC/SHARED | healpix_drizzle/*.cpp | lib/healpix_db/healpix_drizzle/CMakeLists.txt |

## 2 Compile definitions

| Define | Target | Source | 证据 |
|---|---|---|---|
| P2_ENABLE_OPENMP=ON | phase2 | `option(P2_ENABLE_OPENMP OFF) hard-disable` → `if(P2_ENABLE_OPENMP AND OpenMP_CXX_FOUND) target_link OpenMP::OpenMP_CXX` | lib/phase2/CMakeLists.txt:18,54 |
| OpenMP_CXX_FOUND=FALSE when OFF | phase2 | `set(OpenMP_CXX_FOUND FALSE)` when OFF | lib/phase2/CMakeLists.txt:28 |
| ACR_BUILD_CUDA=OFF default | acr | `option(ACR_BUILD_CUDA OFF)` | lib/acr/CMakeLists.txt |

## 3 Link libraries

| From | To | Via |
|---|---|---|
| astrocs-stage2 | phase2 + astro_image_io.dll | `target_link_libraries(astrocs-stage2 PRIVATE phase2 astro_image_io.dll)` lib/phase2/CMakeLists.txt:73 |
| phase2 (when ON) | OpenMP::OpenMP_CXX | `target_link_libraries(phase2 PUBLIC OpenMP::OpenMP_CXX)` |
| acr_cuda_bridge.dll | phase2 executables | POST_BUILD copy to TARGET_FILE_DIR if EXISTS |

## 4 File-API / compile_commands 对应

| 声明 | File-API codemodel target | compile DB |
|---|---|---|
| phase2 STATIC | `phase2` reply `targetSources + compileGroups` | `compile_commands.json` entries for src/*.cpp with `__cplusplus=202002L` |
| astrocs-stage2 EXEC | `astrocs-stage2` target | `tools/stage2.cpp` compile command |
| orchestrator EXEC | `orchestrator` target | `cpp/src/*.cpp` commands |

验证: `cmake --build --verbose` 显示命令含 `-std=c++20` + `-D` + `-I` 与声明一致; File-API `reply/codemodel-v2-*.json` 的 `targets[].sources` 与上表一致 (T600 contracts configure 时落地)。

## 5 验证方法

```sh
cmake -S lib/phase2 -B build/linux-release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DP2_ENABLE_OPENMP=OFF
cmake --build build/linux-release --verbose | grep phase2
cat build/linux-release/compile_commands.json | python3 -m json.tool | grep -c "phase2/src"
cat build/linux-release/.cmake/api/v1/reply/codemodel-v2-*.json | python3 -m json.tool | grep target
```

## 6 ARC 映射

- ARC-BUILD-001: phase2 STATIC 源集
- ARC-BUILD-002: P2_ENABLE_OPENMP compile/linker 编排
- ARC-BUILD-003: acr_cuda_bridge.dll 运行时投递
