// lib/acr/tests/fault/sanitizer_actual.cpp — Phase H 实际 sanitizer 验证
// 设计：故意触发 UAF / heap-overflow / 未定义行为，验证 sanitizer 实际开启。
//
// 安全策略：
//   1. 编译时检测 sanitizer（ASan/UBSan）是否开启
//   2. sanitizer 开启时：用 EXPECT_DEATH 触发 UAF/overflow，期望 sanitizer 终止进程
//   3. sanitizer 未开启时：SKIP（不触发未定义行为，避免崩溃）
//   4. 无 sanitizer 时不编译危险代码，避免 CI 中的未定义行为
#include <gtest/gtest.h>

#include <cstdlib>
#include <new>
#include <string>
#include "exit_safe.hpp"

// ===== sanitizer 编译时检测 =====
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define ACR_ASAN_ENABLED 1
#  endif
#  if __has_feature(undefined_behavior_sanitizer)
#    define ACR_UBSAN_ENABLED 1
#  endif
#  if __has_feature(thread_sanitizer)
#    define ACR_TSAN_ENABLED 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#  define ACR_ASAN_ENABLED 1
#endif
#if defined(__SANITIZE_UNDEFINED__)
#  define ACR_UBSAN_ENABLED 1
#endif
#if defined(__SANITIZE_THREAD__)
#  define ACR_TSAN_ENABLED 1
#endif

#if !defined(ACR_ASAN_ENABLED)
#  define ACR_ASAN_ENABLED 0
#endif
#if !defined(ACR_UBSAN_ENABLED)
#  define ACR_UBSAN_ENABLED 0
#endif
#if !defined(ACR_TSAN_ENABLED)
#  define ACR_TSAN_ENABLED 0
#endif

// ============================================================================
// 1. sanitizer 状态检测（不触发危险行为）
// ============================================================================
TEST(SanitizerActual, DetectSanitizerStatus) {
#if ACR_ASAN_ENABLED
    SUCCEED() << "ASan is enabled";
#elif ACR_UBSAN_ENABLED
    SUCCEED() << "UBSan is enabled";
#elif ACR_TSAN_ENABLED
    SUCCEED() << "TSan is enabled";
#else
    GTEST_SKIP() << "No sanitizer enabled (ASan/UBSan/TSan not detected)";
#endif
}

// ============================================================================
// 2. Use-After-Free：只在 ASan 开启时触发
// ============================================================================
#if ACR_ASAN_ENABLED
TEST(SanitizerActual, UseAfterFreeDetected) {
    // 故意触发 UAF：ASan 应检测到并终止进程
    EXPECT_DEATH({
        int* p = new int(42);
        delete p;
        // ASan 应在此处报告 use-after-free
        volatile int x = *p;
        (void)x;
        std::exit(0);  // 如果 ASan 未捕获，正常退出（EXPECT_DEATH 仍期望非零退出）
    }, "use-after-free|heap-use-after-free|AddressSanitizer");
}
#else
TEST(SanitizerActual, UseAfterFreeDetected) {
    GTEST_SKIP() << "ASan not enabled, skipping UAF trigger test";
}
#endif

// ============================================================================
// 3. Heap-Buffer-Overflow：只在 ASan 开启时触发
// ============================================================================
#if ACR_ASAN_ENABLED
TEST(SanitizerActual, HeapBufferOverflowDetected) {
    // 故意触发 heap-overflow：ASan 应检测到并终止进程
    EXPECT_DEATH({
        char* p = new char[16];
        // ASan 应在此处报告 heap-buffer-overflow
        volatile char x = p[32];  // 越界读
        (void)x;
        delete[] p;
        std::exit(0);
    }, "heap-buffer-overflow|AddressSanitizer");
}
#else
TEST(SanitizerActual, HeapBufferOverflowDetected) {
    GTEST_SKIP() << "ASan not enabled, skipping heap-overflow trigger test";
}
#endif

// ============================================================================
// 4. Stack-Buffer-Overflow：只在 ASan 开启时触发
// ============================================================================
#if ACR_ASAN_ENABLED
TEST(SanitizerActual, StackBufferOverflowDetected) {
    EXPECT_DEATH({
        char buf[16];
        // ASan 应在此处报告 stack-buffer-overflow
        volatile char x = buf[32];
        (void)x;
        std::exit(0);
    }, "stack-buffer-overflow|AddressSanitizer");
}
#else
TEST(SanitizerActual, StackBufferOverflowDetected) {
    GTEST_SKIP() << "ASan not enabled, skipping stack-overflow trigger test";
}
#endif

// ============================================================================
// 5. Undefined Behavior：只在 UBSan 开启时触发
// ============================================================================
#if ACR_UBSAN_ENABLED
TEST(SanitizerActual, UndefinedBehaviorDetected) {
    // 故意触发未定义行为：整数溢出（有符号）
    // UBSan 应检测到并报告
    EXPECT_DEATH({
        int max_int = 2147483647;
        volatile int overflow = max_int + 1;  // 有符号整数溢出（UB）
        (void)overflow;
        std::exit(0);
    }, "undefined|UBSan|signed integer overflow|runtime error");
}
#else
TEST(SanitizerActual, UndefinedBehaviorDetected) {
    GTEST_SKIP() << "UBSan not enabled, skipping UB trigger test";
}
#endif

// ============================================================================
// 6. Null Pointer Dereference：只在 sanitizer 开启时触发
// ============================================================================
#if ACR_ASAN_ENABLED || ACR_UBSAN_ENABLED
TEST(SanitizerActual, NullDereferenceDetected) {
    EXPECT_DEATH({
        int* p = nullptr;
        // sanitizer 应检测到空指针解引用
        volatile int x = *p;
        (void)x;
        std::exit(0);
    }, "SEGV|null|AddressSanitizer|UBSan");
}
#else
TEST(SanitizerActual, NullDereferenceDetected) {
    GTEST_SKIP() << "No sanitizer enabled, skipping null deref trigger test";
}
#endif

// ============================================================================
// 7. Double Free：只在 ASan 开启时触发
// ============================================================================
#if ACR_ASAN_ENABLED
TEST(SanitizerActual, DoubleFreeDetected) {
    EXPECT_DEATH({
        int* p = new int(42);
        delete p;
        delete p;  // double free
        std::exit(0);
    }, "double-free|AddressSanitizer|alloc-dealloc-mismatch");
}
#else
TEST(SanitizerActual, DoubleFreeDetected) {
    GTEST_SKIP() << "ASan not enabled, skipping double-free trigger test";
}
#endif

// ============================================================================
// main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    // death test 使用 threadsafe 模式
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    int result = RUN_ALL_TESTS();
    astro::compute::test::exit_after_tests(result);
}
