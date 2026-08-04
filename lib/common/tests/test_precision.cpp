// ============================================================================
// test_precision.cpp - 验证 astro_scalar.h / precision_context.h 可正确编译与运行
// ----------------------------------------------------------------------------
// 覆盖点:
//   1) AstroScalarTraits 编译时映射 (float / double)
//   2) astro_scalar_type_name / astro_scalar_type_size 运行时查询
//   3) PrecisionContext 单例 + set/get + 便捷查询
//   4) ASTRO_SCALAR_DISPATCH   (lambda 形式) 运行时分发
//   5) ASTRO_SCALAR_DISPATCH_T (typedef 形式) 运行时分发
// 失败时通过 static_assert (编译期) 或返回非零 (运行期) 暴露问题.
// ============================================================================

#include "astro_scalar.h"
#include "precision_context.h"

#include <cstdio>
#include <string>
#include <type_traits>

// ----------------------------------------------------------------------------
// 1) 编译时映射校验
// ----------------------------------------------------------------------------
static_assert(std::is_same_v<AstroScalarTraits<AstroScalarType::FP32>::type, float>,
              "FP32 traits must map to float");
static_assert(std::is_same_v<AstroScalarTraits<AstroScalarType::FP64>::type, double>,
              "FP64 traits must map to double");
static_assert(AstroScalarTraits<AstroScalarType::FP32>::size == 4, "FP32 size must be 4");
static_assert(AstroScalarTraits<AstroScalarType::FP64>::size == 8, "FP64 size must be 8");
static_assert(AstroScalarTraits<AstroScalarType::FP32>::value == AstroScalarType::FP32, "value echo");
static_assert(AstroScalarTraits<AstroScalarType::FP64>::value == AstroScalarType::FP64, "value echo");

// 简单的运行期断言宏: 失败则打印并返回非零
#define RUNTIME_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
            return 1; \
        } \
    } while(0)

int main() {
    // ----------------------------------------------------------------------
    // 2) 运行时辅助函数
    // ----------------------------------------------------------------------
    RUNTIME_CHECK(astro_scalar_type_size(AstroScalarType::FP32) == 4, "FP32 size==4");
    RUNTIME_CHECK(astro_scalar_type_size(AstroScalarType::FP64) == 8, "FP64 size==8");
    RUNTIME_CHECK(std::string(astro_scalar_type_name(AstroScalarType::FP32)) == "float32", "FP32 name");
    RUNTIME_CHECK(std::string(astro_scalar_type_name(AstroScalarType::FP64)) == "float64", "FP64 name");

    // ----------------------------------------------------------------------
    // 3) PrecisionContext 单例 + 默认值 + 切换
    // ----------------------------------------------------------------------
    auto& ctx = PrecisionContext::instance();
    RUNTIME_CHECK(ctx.is_fp32(), "default should be FP32");
    RUNTIME_CHECK(ctx.scalar_size() == 4, "default scalar_size==4");
    RUNTIME_CHECK(std::string(ctx.scalar_name()) == "float32", "default name==float32");

    ctx.set_scalar_type(AstroScalarType::FP64);
    RUNTIME_CHECK(ctx.is_fp64(), "after set FP64 should be FP64");
    RUNTIME_CHECK(ctx.scalar_size() == 8, "FP64 scalar_size==8");
    RUNTIME_CHECK(std::string(ctx.scalar_name()) == "float64", "FP64 name==float64");
    RUNTIME_CHECK(ctx.scalar_type() == AstroScalarType::FP64, "scalar_type echo");

    // 单例唯一性: 同一进程取到的应是同一实例
    auto& ctx2 = PrecisionContext::instance();
    RUNTIME_CHECK(&ctx == &ctx2, "singleton identity");
    RUNTIME_CHECK(ctx2.is_fp64(), "singleton state shared");

    // ----------------------------------------------------------------------
    // 4) ASTRO_SCALAR_DISPATCH (lambda 形式)
    // ----------------------------------------------------------------------
    // 注意: 宏的两个分支都会被编译器实例化, 因此 lambda 内不能写只对单一类型
    //       成立的 static_assert; 用 if constexpr 区分分支, 运行时验证走了哪条.
    //       关键: 必须用 decltype(st)::value 取出嵌套枚举常量传给 AstroScalarTraits.
    {
        double d_from_lambda = 0.0;
        float  f_from_lambda = 0.0f;

        ctx.set_scalar_type(AstroScalarType::FP64);
        ASTRO_SCALAR_DISPATCH(ctx.scalar_type(), [&](auto st) {
            using Scalar = typename AstroScalarTraits<decltype(st)::value>::type;
            if constexpr (std::is_same_v<Scalar, double>) {
                d_from_lambda = 3.14159265358979;
            } else {
                f_from_lambda = 2.7182818f;
            }
        });
        RUNTIME_CHECK(d_from_lambda > 3.14 && d_from_lambda < 3.15, "FP64 dispatch -> double branch");
        RUNTIME_CHECK(f_from_lambda == 0.0f, "FP64 dispatch did not touch float branch");

        d_from_lambda = 0.0;
        f_from_lambda = 0.0f;
        ctx.set_scalar_type(AstroScalarType::FP32);
        ASTRO_SCALAR_DISPATCH(ctx.scalar_type(), [&](auto st) {
            using Scalar = typename AstroScalarTraits<decltype(st)::value>::type;
            if constexpr (std::is_same_v<Scalar, float>) {
                f_from_lambda = 2.7182818f;
            } else {
                d_from_lambda = 3.14159265358979;
            }
        });
        RUNTIME_CHECK(f_from_lambda > 2.7f && f_from_lambda < 2.8f, "FP32 dispatch -> float branch");
        RUNTIME_CHECK(d_from_lambda == 0.0, "FP32 dispatch did not touch double branch");
    }

    // ----------------------------------------------------------------------
    // 5) ASTRO_SCALAR_DISPATCH_T (typedef 形式)
    // ----------------------------------------------------------------------
    // 同样: 两个分支的 body 都会被实例化, 用 if constexpr 区分类型特定逻辑.
    {
        double d_from_dispatch_t = 0.0;
        float  f_from_dispatch_t = 0.0f;

        ctx.set_scalar_type(AstroScalarType::FP64);
        ASTRO_SCALAR_DISPATCH_T(ctx.scalar_type(), T, {
            if constexpr (std::is_same_v<T, double>) {
                d_from_dispatch_t = 1.4142135623730951;
            } else {
                f_from_dispatch_t = 1.41f;
            }
        });
        RUNTIME_CHECK(d_from_dispatch_t > 1.41 && d_from_dispatch_t < 1.42, "FP64 dispatch_t -> double");
        RUNTIME_CHECK(f_from_dispatch_t == 0.0f, "FP64 dispatch_t did not touch float");

        d_from_dispatch_t = 0.0;
        f_from_dispatch_t = 0.0f;
        ctx.set_scalar_type(AstroScalarType::FP32);
        ASTRO_SCALAR_DISPATCH_T(ctx.scalar_type(), T, {
            if constexpr (std::is_same_v<T, float>) {
                f_from_dispatch_t = 1.41f;
            } else {
                d_from_dispatch_t = 1.4142135623730951;
            }
        });
        RUNTIME_CHECK(f_from_dispatch_t > 1.4f && f_from_dispatch_t < 1.42f, "FP32 dispatch_t -> float");
        RUNTIME_CHECK(d_from_dispatch_t == 0.0, "FP32 dispatch_t did not touch double");
    }

    std::printf("[OK] all precision header checks passed\n");
    return 0;
}
