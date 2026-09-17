// ============================================================================
// BIAS-001 · bias 参与门（模块级、可变异）— 独立判据 harness
// ----------------------------------------------------------------------------
// 用途：check_bias_influence.py 编译本文件 + 生产源（calibrator/ac_api/
//       master_generator/cosmetic_corrector）后运行；对**契约**（SCI-CAL-001 §5
//       订正后）逐条断言，期望值用字面量手算而**不复用实现**。
//
// 判据（任一失败 → rc=1）：
//   T1 标准式含 bias:  cal = (light − bias − K·dark)/max(flat,0.1)
//   T2 标准式无 bias:  cal = (light − K·dark)/max(flat,0.1)
//   T3 bias 参与:      T1 与 T2 必须逐位不同（提供 bias 必须改变产物）
//   T4 只给 bias:      cal = light − bias（旧实现此处返回 light，零影响）
//   T5 K 参与:         cal = light − bias − K·dark（K 不再被强制 1.0）
//   T6 兼容式:         cal = light − bias − K·(dark − bias)（dark 含 bias 声明）
//   T6b 兼容式 bias 参与: 同一 dark/K 下改 bias 必须改变 cal（(∂y/∂bias)=−(1−K)/f ≠ 0, K≠1）
//   T7 约定差异:       K=1 时，把含 bias 的 dark 当"已减 bias"消费（标准式）
//                      必须与兼容式**不同**（差 = bias/max(flat,0.1)；旧实现在
//                      此把 bias 静默丢弃，两式逐位相同）
//   T8 FP64 路径与 T1/T5 同式
//
// 退出：0 = 全部通过；1 = 有断言失败。
// ============================================================================
#include <cmath>
#include <cstdio>
#include <vector>

#include "astro_calibration.h"

namespace {

int g_fail = 0;

void check_bitwise(const char* name, float got, float want) {
    const bool ok = (got == want);
    std::printf("BIAS-GATE %s %s got=%.9g want=%.9g\n", name, ok ? "PASS" : "FAIL",
                static_cast<double>(got), static_cast<double>(want));
    if (!ok) ++g_fail;
}

void check_bitwise_d(const char* name, double got, double want) {
    const bool ok = (got == want);
    std::printf("BIAS-GATE %s %s got=%.17g want=%.17g\n", name, ok ? "PASS" : "FAIL", got, want);
    if (!ok) ++g_fail;
}

struct Call {
    std::vector<float> out;
    float actual_k = 0.0f;
    int rc = -1;
};

Call run(const std::vector<float>& light, const std::vector<float>* dark,
         const std::vector<float>* flat, const std::vector<float>* bias,
         int dark_opt, float k) {
    Call c;
    c.out.assign(light.size(), -777.0f);
    c.rc = ac_calibrate_frame(light.data(), 4, 4, dark ? dark->data() : nullptr,
                              flat ? flat->data() : nullptr, bias ? bias->data() : nullptr,
                              c.out.data(), dark_opt, k, &c.actual_k);
    return c;
}

}  // namespace

int main() {
    // 手算字面量（16 像素常量场；全部值在 float32 下精确可表）
    const float L = 1000.0f, B = 100.5f, D = 200.25f, F = 2.0f;  // max(F,0.1)=2.0
    std::vector<float> light(16, L), dark(16, D), flat(16, F), bias(16, B);

    // T1 标准式 (K=1): (1000 − 100.5 − 1·200.25)/2 = 349.625
    const Call t1 = run(light, &dark, &flat, &bias, 0, 1.0f);
    if (t1.rc != AC_OK) { std::printf("BIAS-GATE T1_rc FAIL rc=%d\n", t1.rc); ++g_fail; }
    check_bitwise("T1_std_with_bias", t1.out.empty() ? 0.0f : t1.out[0], 349.625f);
    check_bitwise("T1_actual_k", t1.actual_k, 1.0f);

    // T2 标准式无 bias: (1000 − 200.25)/2 = 399.875
    const Call t2 = run(light, &dark, &flat, nullptr, 0, 1.0f);
    check_bitwise("T2_std_no_bias", t2.out.empty() ? 0.0f : t2.out[0], 399.875f);

    // T3 bias 参与: T1 与 T2 必须逐位不同
    {
        bool differs = false;
        for (std::size_t i = 0; i < t1.out.size(); ++i)
            if (t1.out[i] != t2.out[i]) differs = true;
        std::printf("BIAS-GATE T3_bias_matters %s\n", differs ? "PASS" : "FAIL");
        if (!differs) ++g_fail;
    }

    // T4 只给 bias（dark/flat 均 NULL）: 1000 − 100.5 = 899.5
    const Call t4 = run(light, nullptr, nullptr, &bias, 0, 1.0f);
    check_bitwise("T4_bias_only", t4.out.empty() ? 0.0f : t4.out[0], 899.5f);

    // T5 K 参与 (K=2): (1000 − 100.5 − 2·200.25)/2 = 249.5
    const Call t5 = run(light, &dark, &flat, &bias, 0, 2.0f);
    check_bitwise("T5_std_k_applied", t5.out.empty() ? 0.0f : t5.out[0], 249.5f);
    check_bitwise("T5_actual_k", t5.actual_k, 2.0f);

    // T6 兼容式 (dark 含 bias, K=2): (1000 − 100.5 − 2·(200.25 − 100.5))/2 = 350
    const Call t6 = run(light, &dark, &flat, &bias, 1, 2.0f);
    check_bitwise("T6_compat_explicit_separation", t6.out.empty() ? 0.0f : t6.out[0], 350.0f);

    // T6b 兼容式内 bias 参与: bias=100.5 vs bias=150.5, K=2, dark=200.25, flat=2
    //     (1000 − 100.5 − 2·99.75)/2 = 350 vs (1000 − 150.5 − 2·49.75)/2 = 375
    {
        std::vector<float> bias2(16, 150.5f);
        const Call c1 = run(light, &dark, &flat, &bias, 1, 2.0f);
        const Call c2 = run(light, &dark, &flat, &bias2, 1, 2.0f);
        check_bitwise("T6b_compat_bias_value_1", c1.out.empty() ? 0.0f : c1.out[0], 350.0f);
        check_bitwise("T6b_compat_bias_value_2", c2.out.empty() ? 0.0f : c2.out[0], 375.0f);
        bool differs = false;
        for (std::size_t i = 0; i < c1.out.size(); ++i)
            if (c1.out[i] != c2.out[i]) differs = true;
        std::printf("BIAS-GATE T6b_compat_bias_matters %s\n", differs ? "PASS" : "FAIL");
        if (!differs) ++g_fail;
    }

    // T7 约定差异 (K=1, dark 含 bias 但按标准式消费):
    //    标准式 (1000 − 100.5 − 200.25)/2 = 349.625
    //    兼容式 (1000 − 100.5 − (200.25 − 100.5))/2 = 399.875
    //    必须不同（旧实现把两式都算成 399.875 → 静默丢弃 bias）
    const Call t7a = run(light, &dark, &flat, &bias, 0, 1.0f);   // 标准式
    const Call t7b = run(light, &dark, &flat, &bias, 1, 1.0f);   // 兼容式
    {
        bool differs = false;
        for (std::size_t i = 0; i < t7a.out.size(); ++i)
            if (t7a.out[i] != t7b.out[i]) differs = true;
        std::printf("BIAS-GATE T7_convention_distinct %s\n", differs ? "PASS" : "FAIL");
        if (!differs) ++g_fail;
    }
    check_bitwise("T7_std_convention_value", t7a.out.empty() ? 0.0f : t7a.out[0], 349.625f);
    check_bitwise("T7_compat_convention_value", t7b.out.empty() ? 0.0f : t7b.out[0], 399.875f);

    // T8 FP64 路径: 与 T1/T5 同式
    {
        std::vector<double> l64(16, 1000.0), d64(16, 200.25), f64(16, 2.0), b64(16, 100.5);
        std::vector<double> o64(16, -777.0);
        double ak = 0.0;
        const int rc = ac_calibrate_frame_f64(l64.data(), 4, 4, d64.data(), f64.data(),
                                              b64.data(), o64.data(), 0, 2.0, &ak);
        if (rc != AC_OK) { std::printf("BIAS-GATE T8_rc FAIL rc=%d\n", rc); ++g_fail; }
        check_bitwise_d("T8_f64_std_k_applied", o64.empty() ? 0.0 : o64[0], 249.5);
        check_bitwise_d("T8_f64_actual_k", ak, 2.0);
    }

    std::printf("BIAS-GATE RESULT: %s (failures=%d)\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
