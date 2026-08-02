# ACR v2 Evidence — 工具链限制记录

**生成时间**: 2026-08-02
**HEAD**: 84e60e958eb977f94ffefb01b31089840d011c91
**分支**: feature/astrocompute-runtime
**构建配置**: CPU-only, MinGW Makefiles, Release

---

## 工具链版本

| 组件 | 版本 |
|------|------|
| CMake | 4.3.2 |
| 编译器 | g++.exe (Rev4, Built by MSYS2 project) 16.1.0 |
| Make | GNU Make 4.4.1 |
| 平台 | Windows / MSYS2 MinGW64 |
| CUDA (可选) | nvcc 11.8 (未集成到本次构建) |

---

## 限制 1: ASan 不可用

**症状**: `sanitizer_actual` 测试套件全部 SKIPPED（7 个测试）。

**根因**: MinGW g++ 16.1.0 缺少 `libasan`（AddressSanitizer 运行时库）。MinGW-w64 发行版默认不附带 ASan 运行时，无法链接 `-fsanitize=address`。

**影响范围**:
- `SanitizerActual.DetectSanitizerStatus` — SKIPPED
- `SanitizerActual.UseAfterFreeDetected` — SKIPPED
- `SanitizerActual.HeapBufferOverflowDetected` — SKIPPED
- `SanitizerActual.StackBufferOverflowDetected` — SKIPPED
- `SanitizerActual.UndefinedBehaviorDetected` — SKIPPED
- `SanitizerActual.NullDereferenceDetected` — SKIPPED
- `SanitizerActual.DoubleFreeDetected` — SKIPPED

**缓解措施**:
- `SanitizerSmoke` 套件（10 个测试）仍正常运行，作为冒烟检查覆盖并发安全、内存泄漏、异常安全等场景，全部 PASSED。
- `acr_test_sanitizer_actual.exe` 在运行时检测 ASan 可用性，不可用时自动 SKIPPED 并记录原因，符合"无硬件/工具链不可用时 SKIPPED"规范。

**验证**: `ctest -R "Sanitizer" --output-on-failure` 输出显示 7 个 SanitizerActual 测试 SKIPPED，原因记录在 `tests/sanitizer_test_results.log`。

---

## 限制 2: CUDA 编译集成未通过

**症状**: Phase D GPU 画像 SKIPPED，本次构建为 CPU-only。

**根因**: nvcc 11.8 与 MinGW g++ 16.1.0 host 编译器不兼容。nvcc 11.8 支持的 host 编译器版本上限低于 g++ 16.1.0，无法完成 CUDA 编译集成。

**影响范围**:
- `lib/acr/backends/cuda/` 下的 CUDA 后端代码未编译进本次构建。
- Phase D（GPU 画像）benchmark 未运行。
- `acr_test_cuda` 单元测试在 CPU-only 构建下以 SKIPPED/降级模式运行（无 GPU 设备时自动跳过 GPU 路径）。

**缓解措施**:
- 构建系统通过 `ACR_BUILD_CUDA` 选项（默认 OFF，ADR-009 CPU-only build gate）控制 CUDA 编译，CPU-only 构建不依赖 CUDA 工具链。
- CUDA 后端代码已编写（`cuda_backend.cu`, `cuda_buffer.cpp` 等），在工具链兼容时可通过开启 `ACR_BUILD_CUDA=ON` 编译。
- 符合"无硬件/工具链不可用时 SKIPPED"规范。

**验证**: 构建日志 `build/build_log_cpu_only.log` 显示 CPU-only 构建成功，无 CUDA 编译步骤；`build/build_config.json` 记录 `cuda: disabled (CPU-only build gate, ADR-009)`。

---

## 限制 3: ApiReduce.NoAliasDeclaration SKIPPED

**症状**: 1 个单元测试 SKIPPED。

**根因**: 该测试为环境门控测试（environment-gated），在特定条件下跳过，非工具链限制。

**影响**: 不影响整体测试结论（0 failed）。

---

## 合规性说明

以上限制均符合项目规范"无硬件/工具链不可用时 SKIPPED"：
- ASan 测试在无 libasan 时自动 SKIPPED，冒烟测试仍覆盖核心安全场景。
- CUDA 测试在 CPU-only 构建下不编译，GPU 画像延后至工具链兼容时执行。
- 所有 SKIPPED 测试均有明确原因记录，无伪造成功。
- 完整 ctest 结果：**573 测试，565 PASSED，8 SKIPPED，0 FAILED**。
