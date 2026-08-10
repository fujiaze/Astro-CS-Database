// lib/acr/tests/fault/exit_safe.hpp — 测试进程安全退出
//
// 背景：MSYS2 MinGW + oneTBB 2023 在测试进程退出时偶发 0xC00000FD（栈溢出），
// 位置在 runtime_shutdown / tbb 静态清理阶段，与系统负载相关且测试间游走
// （SanitizerSmoke/FaultInjection/E03 均出现过；测试体本身全部通过）。
// 规避：测试体完成后直接 std::_Exit，跳过 tbb 静态析构。
// 测试语义不受影响；内存/资源可靠性由 MSVC ASan 独立验证覆盖
// （tests/sanitizer/msvc_asan_main.cpp，真实 shared_work_pool + kernel_registry）。
#pragma once

#include <cstdlib>

namespace astro::compute::test {

inline void exit_after_tests(int result) {
    std::_Exit(result);
}

} // namespace astro::compute::test
