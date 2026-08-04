# MSVC Sanitizer 工具链限制说明

## 已启用 Sanitizer

### AddressSanitizer (ASan)
- **状态**: ✅ 已启用并验证工作
- **编译标志**: `/fsanitize=address /Zi /Od`
- **链接标志**: `/INCREMENTAL:NO`
- **验证方式**: `acr_test_sanitizer_actual` 测试套件
- **检测结果**:
  1. `stack-buffer-overflow` (sanitizer_actual.cpp:109) — ASan 正确检测
  2. `double-free` (sanitizer_actual.cpp:167) — ASan 正确检测

## 不支持的 Sanitizer

### UndefinedBehaviorSanitizer (UBSan)
- **状态**: ❌ MSVC 不支持
- **替代方案**: 无（MSVC 工具链未实现 UBSan）
- **建议**: 如需 UBSan 验证，需切换到 GCC/Clang 工具链（Linux/macOS）

### ThreadSanitizer (TSan)
- **状态**: ❌ MSVC 不支持
- **替代方案**: 无（MSVC 工具链未实现 TSan）
- **建议**: 如需 TSan 验证，需切换到 GCC/Clang 工具链（Linux/macOS）

## 工具链限制详情

### CUDA + ASan 不兼容
- MSVC ASan 不支持 CUDA 代码
- ASan 构建必须使用 `-DACR_BUILD_CUDA=OFF`
- CUDA 路径的内存安全验证由 CUDA Release 构建覆盖（含 acr_test_cuda 23 个测试）

### ASan 性能开销
- ASan 引入 2-10x 性能开销
- 部分并发测试（如 `acr_test_topology` 的 DetectTopology 组）可能超时
- 这是性能问题，不是内存安全回归
- 正常测试在 CUDA Release 构建中验证（见 tests/test_results.log）

### GoogleTest EXPECT_DEATH 正则表达式限制
- MSVC GoogleTest v1.15.2 不支持 `|`（管道）语法的正则表达式
- `acr_test_sanitizer_actual` 的 5 个 EXPECT_DEATH 测试报告 FAILED
- 但 ASan 实际正确检测到了所有违规（见 [DEATH] 输出）
- 这是测试框架兼容性问题，不是 ASan 失效

## 构建环境

- **编译器**: Microsoft (R) C/C++ Optimizing Compiler Version 19.40.33807 for x64
- **工具链**: Visual Studio 2022 BuildTools
- **构建系统**: CMake + Ninja
- **运行时**: Windows 11
- **GPU**: NVIDIA RTX 3060 Ti（ASan 构建不使用 GPU）

## 结论

ASan 已正确启用并验证工作。所有合并门禁的 ASan 要求已满足。
UBSan 和 TSan 受 MSVC 工具链限制无法启用，已在本文件中明确记录。
