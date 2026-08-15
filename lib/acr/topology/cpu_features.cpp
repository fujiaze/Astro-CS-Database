// lib/acr/topology/cpu_features.cpp — CPU ISA 安全门禁实现
// ADR-004：cpu_features 作为 ISA 检测来源；MSYS2 无 cpu_features 包时
// 用 __builtin_cpu_supports（GCC/Clang）降级。
//
// 设计要点：
// - 检测在构造时一次性完成，结果存入 mask_（atomic-free，构造后不可变）
// - AVX-512 子集独立 bit，禁止合并（ADR-004 关键约束）
// - has() 安全门禁：传入组合 mask 时全部 bit 都支持才返回 true
// - baseline 路径（无任何扩展）永远可用，不依赖检测结果
// - 无 __builtin_cpu_supports（如 MSVC）时 mask_ 仅含 SSE2（x86-64 必有）
#include "astro/compute/topology.hpp"

#include <sstream>
#include <string>

#if defined(__has_include)
#  if __has_include(<cpu_features/cpu_features_cache.h>)
#    define ACR_HAVE_CPU_FEATURES_LIB 1
#  endif
#endif

#ifdef ACR_HAVE_CPU_FEATURES_LIB
#  include <cpu_features/cpu_features_cache.h>
#endif

namespace astro::compute {

namespace {

// 用 __builtin_cpu_supports 检测 ISA mask（GCC/Clang 专属）
// 注意：__builtin_cpu_supports 参数必须是字符串字面量，不能用变量传入
std::uint64_t detect_mask_builtin() {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    std::uint64_t m = 0;
    if (__builtin_cpu_supports("sse"))      m |= static_cast<std::uint64_t>(IsaLevel::SSE);
    if (__builtin_cpu_supports("sse2"))     m |= static_cast<std::uint64_t>(IsaLevel::SSE2);
    if (__builtin_cpu_supports("sse3"))     m |= static_cast<std::uint64_t>(IsaLevel::SSE3);
    if (__builtin_cpu_supports("ssse3"))    m |= static_cast<std::uint64_t>(IsaLevel::SSSE3);
    if (__builtin_cpu_supports("sse4.1"))   m |= static_cast<std::uint64_t>(IsaLevel::SSE41);
    if (__builtin_cpu_supports("sse4.2"))   m |= static_cast<std::uint64_t>(IsaLevel::SSE42);
    if (__builtin_cpu_supports("avx"))      m |= static_cast<std::uint64_t>(IsaLevel::AVX);
    if (__builtin_cpu_supports("avx2"))     m |= static_cast<std::uint64_t>(IsaLevel::AVX2);
    if (__builtin_cpu_supports("fma"))      m |= static_cast<std::uint64_t>(IsaLevel::FMA);
    if (__builtin_cpu_supports("avx512f"))  m |= static_cast<std::uint64_t>(IsaLevel::AVX512F);
    if (__builtin_cpu_supports("avx512cd")) m |= static_cast<std::uint64_t>(IsaLevel::AVX512CD);
    if (__builtin_cpu_supports("avx512bw")) m |= static_cast<std::uint64_t>(IsaLevel::AVX512BW);
    if (__builtin_cpu_supports("avx512dq")) m |= static_cast<std::uint64_t>(IsaLevel::AVX512DQ);
    if (__builtin_cpu_supports("avx512vl")) m |= static_cast<std::uint64_t>(IsaLevel::AVX512VL);
    return m;
#else
    // 非 GCC/Clang 或非 x86：x86-64 必有 SSE2，其他架构无 x86 ISA
#  if defined(__x86_64__)
    return static_cast<std::uint64_t>(IsaLevel::SSE2);
#  else
    return 0;
#  endif
#endif
}

// 用 google/cpu_features 库检测（如可用）
std::uint64_t detect_mask_lib() {
#ifdef ACR_HAVE_CPU_FEATURES_LIB
    X86Features f = GetX86Features().features;
    std::uint64_t m = 0;
    if (f.sse)       m |= static_cast<std::uint64_t>(IsaLevel::SSE);
    if (f.sse2)      m |= static_cast<std::uint64_t>(IsaLevel::SSE2);
    if (f.sse3)      m |= static_cast<std::uint64_t>(IsaLevel::SSE3);
    if (f.ssse3)     m |= static_cast<std::uint64_t>(IsaLevel::SSSE3);
    if (f.sse4_1)    m |= static_cast<std::uint64_t>(IsaLevel::SSE41);
    if (f.sse4_2)    m |= static_cast<std::uint64_t>(IsaLevel::SSE42);
    if (f.avx)       m |= static_cast<std::uint64_t>(IsaLevel::AVX);
    if (f.avx2)      m |= static_cast<std::uint64_t>(IsaLevel::AVX2);
    if (f.fma)       m |= static_cast<std::uint64_t>(IsaLevel::FMA);
    if (f.avx512f)   m |= static_cast<std::uint64_t>(IsaLevel::AVX512F);
    if (f.avx512cd)  m |= static_cast<std::uint64_t>(IsaLevel::AVX512CD);
    if (f.avx512bw)  m |= static_cast<std::uint64_t>(IsaLevel::AVX512BW);
    if (f.avx512dq)  m |= static_cast<std::uint64_t>(IsaLevel::AVX512DQ);
    if (f.avx512vl)  m |= static_cast<std::uint64_t>(IsaLevel::AVX512VL);
    return m;
#else
    return detect_mask_builtin();
#endif
}

} // anonymous namespace

CpuIsaCaps::CpuIsaCaps() noexcept : mask_(detect_mask_lib()) {}

bool CpuIsaCaps::has(IsaLevel level) const noexcept {
    if (static_cast<std::uint64_t>(level) == 0) return true;  // None 永真
    return (mask_ & static_cast<std::uint64_t>(level)) == static_cast<std::uint64_t>(level);
}

std::string CpuIsaCaps::to_json() const {
    std::ostringstream os;
    os << "{";
    os << "\"mask\":\"0x" << std::hex << mask_ << std::dec << "\"";
    os << ",\"SSE\":"     << (has(IsaLevel::SSE)     ? "true" : "false");
    os << ",\"SSE2\":"    << (has(IsaLevel::SSE2)    ? "true" : "false");
    os << ",\"SSE3\":"    << (has(IsaLevel::SSE3)    ? "true" : "false");
    os << ",\"SSSE3\":"   << (has(IsaLevel::SSSE3)   ? "true" : "false");
    os << ",\"SSE41\":"   << (has(IsaLevel::SSE41)   ? "true" : "false");
    os << ",\"SSE42\":"   << (has(IsaLevel::SSE42)   ? "true" : "false");
    os << ",\"AVX\":"     << (has(IsaLevel::AVX)     ? "true" : "false");
    os << ",\"AVX2\":"    << (has(IsaLevel::AVX2)    ? "true" : "false");
    os << ",\"FMA\":"     << (has(IsaLevel::FMA)     ? "true" : "false");
    os << ",\"AVX512F\":" << (has(IsaLevel::AVX512F) ? "true" : "false");
    os << ",\"AVX512CD\":"<< (has(IsaLevel::AVX512CD)? "true" : "false");
    os << ",\"AVX512BW\":"<< (has(IsaLevel::AVX512BW)? "true" : "false");
    os << ",\"AVX512DQ\":"<< (has(IsaLevel::AVX512DQ)? "true" : "false");
    os << ",\"AVX512VL\":"<< (has(IsaLevel::AVX512VL)? "true" : "false");
    os << "}";
    return os.str();
}

// ===== 自由函数 =====
std::string detect_isa_caps() {
    CpuIsaCaps caps;
    return caps.to_json();
}

} // namespace astro::compute
