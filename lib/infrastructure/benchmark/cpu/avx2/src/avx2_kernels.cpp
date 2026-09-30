// lib/infrastructure/benchmark/cpu/avx2/src/avx2_kernels.cpp
//   — AVX2/FMA provider 的**计算面 TU**（本 target 是唯一带 AVX2/FMA 旗标的 TU; R-60 配方）
//
// 为什么与门面 TU (avx2_provider.cpp) 分开（工具链约束，不是风格选择）:
//   · GCC/Clang 有**函数级**指令集覆盖（同族 avx512 的
//     ACS_CPU_AVX512_CAP_GATE_NOEVEX target 属性即此机制）；
//   · MSVC/clang-cl **没有**: pragma 全表无 target、__declspec 全表无
//     cpu_specific/cpu_dispatch、/arch: 的作用域是**整个 TU** —— TU 内任意函数
//     （含 query 握手、self_test、结构体清零的编译器内联）都可能被生成宽向量指令。
//     ⇒ 本 TU 只放热点 kernel 的数值循环；握手/自检/注册表在门面 TU 里，
//     保证「能力预检不过则不加载」在仅 SSE2 的主机上真的能跑到判定那一步
//     （而不是加载期/查询期撞非法指令）。
//
// 平台旗标口径（唯一登记点 = 根 CMakeLists.txt 的 acsd_cpuprov_avx2_kernels）:
//   GCC/Clang: -mavx2 -mfma
//   MSVC     : /arch:AVX2（官方口径: AVX2 档同时许可 FMA 指令面；MSVC 无独立
//              /mfma 类开关）+ /fp:contract（仅 VS2022 起需要，见根 CMakeLists.txt）
//
// 旗标失效不得静默（R-60 硬约束 2）: 下面的 #error 把「旗标被工具链忽略 / 工具链
// 过老」从「静默退化成与 baseline 同码」变成**编译期红灯**。
#if defined(_MSC_VER) && !defined(__AVX2__)
#error "acsd_cpuprov_avx2 计算面 TU 未获得 AVX2 许可面: MSVC 需 /arch:AVX2（VS2013 Update 2 / MSVC 1800+ 支持）。工具链不支持时不得以基线同码产物冒充变体，退回基线必须显式改口径。"
#endif

#include "acsd/cpu/avx2_provider_v1.h"
#include "acsd/cpu/cpuprov_kernels_v1.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

/* ───────────────────────── 热点 kernel 数值实现 ─────────────────────────
 * 公式与算术序与 baseline provider (CPU-002) 同式 (ALG-001 / ALG-P3-002 离散
 * 公式; CPU-002 oracle 同源) —— 本 provider 只迁移经 profile 证明的热点
 * (calibration/hips), 不复制其余 10 个科学 kernel; 数值语义零变更
 * (scientific_change=false), 差异仅编译旗标 (-mavx2 -mfma 自动向量化 +
 * FMA 收缩; 可能每元素 ≤ 数 ULP 舍入差, 容差 2e-4 冻结)。
 * 本实现写为标量源码 (与 legacy ISA-001 avx2 变体同策略: 共享同式源 +
 * TU 局部旗标), 由编译器按 -mavx2 -mfma 自动向量化; 不手写 intrinsic
 * (防科学漂移, 02 §10.1)。
 *
 * R-60 拆分说明: 本函数体自 avx2_provider.cpp **逐字符搬移**（只去掉 static、
 * 改名为跨 TU 桥入口，形参/语句/项序一字未动）—— 科学内容零变更，
 * 变的只是它被编译时所在的 TU 与链接可见性。 */
extern "C" void acsd_cpuprov_kernel_range_v1(const acsd_cpu_baseline_params_v1* P,
                                                uint32_t kidx,
                                                const float* const* in, float* const* out,
                                                uint64_t i0, uint64_t i1) {
    const float kf = P->k;
    switch (kidx) {
    case ACS_CPU_AVX2_KIDX_CALIBRATION: {
        const float* a = in[0]; const float* b = in[1];
        const float* c = in[2]; const float* d = in[3];
        float* o = out[0];
        for (uint64_t i = i0; i < i1; ++i)
            o[i] = (a[i] - b[i] - kf * c[i]) * d[i];
        break;
    }
    case ACS_CPU_AVX2_KIDX_HIPS_BULK: {
        const uint64_t iw = P->aux0, ih = P->aux1;
        const float s = kf;
        const float* src = in[0]; float* o = out[0];
        const uint32_t w = P->w;
        for (uint64_t i = i0; i < i1; ++i) {
            const float x = static_cast<float>(i % w) * s;
            const float y = static_cast<float>(i / w) * s;
            const float flx = std::floor(x), fly = std::floor(y);
            int x0 = static_cast<int>(flx);
            int y0 = static_cast<int>(fly);
            float fx = x - flx, fy = y - fly;
            x0 = std::min(std::max(x0, 0), static_cast<int>(iw) - 2);
            y0 = std::min(std::max(y0, 0), static_cast<int>(ih) - 2);
            fx = std::min(std::max(fx, 0.0f), 1.0f);
            fy = std::min(std::max(fy, 0.0f), 1.0f);
            const uint64_t r0 = static_cast<uint64_t>(y0) * iw;
            const uint64_t r1 = r0 + iw;
            const uint64_t x0s = static_cast<uint64_t>(x0);
            const float v00 = src[r0 + x0s], v10 = src[r0 + x0s + 1];
            const float v01 = src[r1 + x0s], v11 = src[r1 + x0s + 1];
            /* 固定项序: v00 项 → v10 项 → v01 项 → v11 项 (与 baseline 同式;
             * FMA 只收缩乘加, 不重排项序; 见文件头 FMA 记录) */
            o[i] = (1.0f - fx) * (1.0f - fy) * v00 + fx * (1.0f - fy) * v10 +
                   (1.0f - fx) * fy * v01 + fx * fy * v11;
        }
        break;
    }
    default:
        break;   /* 不可达 (门面 TU 的 avx2_validate 已挡); 防静态分析告警 */
    }
}
