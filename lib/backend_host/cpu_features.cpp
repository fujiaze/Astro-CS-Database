// lib/backend_host/cpu_features.cpp — CPUID/OSXSAVE/XGETBV/affinity 实测 (05 §3) — ABI-002
#include "cpu_features.h"

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#include <x86intrin.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#else
#include <sched.h>
#include <unistd.h>
#endif

extern "C" {

#if defined(__x86_64__) || defined(__i386__)

__attribute__((target("xsave"))) static unsigned long long read_xcr0_impl() {
    return _xgetbv(0);
}

static unsigned long long read_xcr0() {
    static bool have_xsave = false;
    static unsigned long long cached = 0;
    if (!have_xsave) {
        unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
        have_xsave = __get_cpuid(1, &eax, &ebx, &ecx, &edx) && (ecx & (1u << 27));
        cached = have_xsave ? read_xcr0_impl() : 0;
    }
    return cached;
}

uint64_t astrocs_cpu_detect_features_v1(void) {
    uint64_t f = ACS_FEAT_SSE2;   // amd64 基线恒置位
    __builtin_cpu_init();
    if (__builtin_cpu_supports("sse4.1")) f |= ACS_FEAT_SSE4_1;
    if (__builtin_cpu_supports("avx")) f |= ACS_FEAT_AVX;
    if (__builtin_cpu_supports("avx2")) f |= ACS_FEAT_AVX2;
    if (__builtin_cpu_supports("fma")) f |= ACS_FEAT_FMA;
    if (__builtin_cpu_supports("avx512f")) f |= ACS_FEAT_AVX512F;
    // 05 §3-2/3: AVX 系必须 OSXSAVE 且 XCR0 保存 XMM/YMM(avx)/opmask+ZMM(avx512)
    const uint64_t avx_family = ACS_FEAT_AVX | ACS_FEAT_AVX2 | ACS_FEAT_FMA;
    if (f & (avx_family | ACS_FEAT_AVX512F)) {
        const unsigned long long xcr0 = read_xcr0();
        if (xcr0 == 0) {
            f &= ~(avx_family | ACS_FEAT_AVX512F);   // 无 OSXSAVE/XSAVE → 全部降级
        } else {
            if ((xcr0 & 0x6) != 0x6) f &= ~avx_family;          // XMM|YMM 未保存
            if ((xcr0 & 0xE0) != 0xE0) f &= ~ACS_FEAT_AVX512F;  // opmask|ZMM_Hi256|Hi16_ZMM
        }
    }
    return f;
}

#else

uint64_t astrocs_cpu_detect_features_v1(void) { return 0; }

#endif

uint32_t astrocs_cpu_affinity_count_v1(void) {
#if defined(_WIN32)
    DWORD_PTR proc = 0, sys = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys) && proc != 0) {
        uint32_t n = 0;
        while (proc) { n += (uint32_t)(proc & 1ull); proc >>= 1; }
        return n;
    }
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
#else
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) != 0) return 1;
    return static_cast<uint32_t>(CPU_COUNT(&set));
#endif
}

}  // extern "C"
