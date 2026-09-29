// lib/infrastructure/benchmark/cpu/avx512/src/avx512_kernels.cpp
//   — AVX-512 provider 的**计算面 TU**（本 target 是唯一带 AVX-512 旗标的 TU; R-60 配方）
//
// 为什么与门面 TU (avx512_provider.cpp) 分开（工具链约束，不是风格选择）:
//   · GCC/Clang 有**函数级**指令集覆盖（门面 TU 里
//     ACS_CPU_AVX512_CAP_GATE_NOEVEX 即此机制）；
//   · MSVC/clang-cl **没有**: /arch: 的作用域是**整个 TU**，pragma 全表无 target、
//     __declspec 全表无 cpu_specific/cpu_dispatch ⇒ TU 内任意函数（含 query 握手、
//     self_test、结构体清零的编译器内联）都可能被生成 EVEX 指令。改前 cap_gate 与
//     kernel 同 TU，在缺 AVX-512 的主机上会在"判定自身是否支持 AVX-512"之前 #UD
//     （鸡生蛋；GCC 侧靠 target 属性把 cap_gate 逐函数降回，MSVC 侧无此手段）。
//
// 平台旗标口径（唯一登记点 = 根 CMakeLists.txt 的 astrocs_cpuprov_avx512_kernels）:
//   GCC/Clang: -mavx512f -mavx512cd -mavx512bw -mavx512dq -mavx512vl
//   MSVC     : /arch:AVX512（官方许可面 = F+CD+BW+DQ+VL；无子集档位旗标）
//
// 旗标失效不得静默（R-60 硬约束 2）: 下面的 #error 把「/arch: 取值不被识别而只报
// D9002 且 rc=0（静默忽略）」与「工具链过老」变成**编译期红灯**。
#if defined(_MSC_VER) && !defined(__AVX512F__)
#error "astrocs_cpuprov_avx512 计算面 TU 未获得 AVX-512 许可面: MSVC 需 /arch:AVX512（自动向量化面自 VS2019 16.3）。工具链不支持时不得以基线同码产物冒充变体。"
#endif
/* 声明面 (ACS_CPU_AVX512_REQUIRED_FEATURES = F|CD|BW|DQ|VL 五子集) 必须被编译
 * 许可面覆盖 —— 缺任一位就是「声明 ⊋ 编译」⇒ 该机器上加载放行、首调撞非法指令。
 * /arch:AVX512 的官方许可面正是这五位（MS docs /arch (x64) 预定义宏段）；若某个
 * 工具链只给子集，这里是编译期红灯，必须显式改口径而不是静默退回。 */
#if defined(_MSC_VER) && (!defined(__AVX512CD__) || !defined(__AVX512BW__) || \
                          !defined(__AVX512DQ__) || !defined(__AVX512VL__))
#error "astrocs_cpuprov_avx512 计算面 TU 的 MSVC 许可面缺 CD/BW/DQ/VL 之一: /arch:AVX512 的官方许可面是 F+CD+BW+DQ+VL，缺位即声明位无编译许可面（声明 ⊋ 编译）。"
#endif

#include "astrocs/cpu/avx512_provider_v1.h"
#include "astrocs/cpu/cpuprov_kernels_v1.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

/* ───────────────────────── 热点 kernel 数值实现 ─────────────────────────
 * 公式与算术序与 baseline/avx2 provider (CPU-002/003) 同式 (ALG-P3-002 离散
 * 公式) —— 只迁移实测获益热点 (hips); 数值语义零变更
 * (scientific_change=false), 差异仅编译旗标 (-mavx512* 自动向量化 + FMA
 * 收缩; 可能每元素 ≤ 数十 ULP 舍入差, 容差 2e-4 冻结)。
 * 标量源码 + TU 局部旗标 (同 avx2/legacy ISA-004 策略), 不手写 intrinsic。
 *
 * R-60 拆分说明: 本函数体自 avx512_provider.cpp **逐字符搬移**（只去掉 static、
 * 改名为跨 TU 桥入口，形参/语句/项序一字未动）。 */
extern "C" void astrocs_cpuprov_kernel_range_v1(const acs_cpu_baseline_params_v1* P,
                                                uint32_t kidx,
                                                const float* const* in, float* const* out,
                                                uint64_t i0, uint64_t i1) {
    const float kf = P->k;
    switch (kidx) {
    case ACS_CPU_AVX512_KIDX_HIPS_BULK: {
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
            /* 固定项序: v00 → v10 → v01 → v11 (与 baseline/avx2 同式;
             * AVX-512/FMA 只收缩乘加, 不重排项序) */
            o[i] = (1.0f - fx) * (1.0f - fy) * v00 + fx * (1.0f - fy) * v10 +
                   (1.0f - fx) * fy * v01 + fx * fy * v11;
        }
        break;
    }
    default:
        break;   /* 不可达 (门面 TU 的 avx512_validate 已挡) */
    }
}
