#pragma once
// ============================================================================
// astro_scalar.h - AstroCS 双精度 ABI 改造: 公共精度类型定义
// ----------------------------------------------------------------------------
// 设计意图:
//   控制包要求 AstroCS 全链路支持真正双精度 (FP64=IEEE 754 binary64),
//   同时保留对历史 FP32 (IEEE 754 binary32) 实现的兼容能力。
//   本头文件提供:
//     1) AstroScalarType 枚举: 运行时标识当前精度
//     2) AstroScalarTraits  : 编译时由枚举值映射到具体 C++ 类型 (float/double)
//     3) 运行时辅助函数    : 类型名/字节数查询, 用于日志与序列化
//     4) ASTRO_SCALAR_DISPATCH 宏: 运行时 dtype -> 编译时模板实例的分发桥
//   所有定义均为 header-only, 无外部依赖, 兼容 C++17。
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

// ============================================================================
// AstroScalarType - AstroCS 精度类型枚举
// 控制包要求: 真正双精度, FP32=IEEE 754 binary32, FP64=IEEE 754 binary64
// ----------------------------------------------------------------------------
// 说明:
//   - 显式指定底层类型为 uint8_t, 确保 ABI 稳定 (用于跨 DLL/Python 传递)
//   - 数值不对外承诺稳定性, 比较请使用枚举常量而非魔法数字
// ============================================================================
enum class AstroScalarType : uint8_t {
    FP32 = 0,  // IEEE 754 binary32 (float)
    FP64 = 1,  // IEEE 754 binary64 (double)
};

// ============================================================================
// AstroScalarTraits - 编译时类型映射
// 给定 AstroScalarType 常量, 提供对应的:
//   - type  : C++ 标量类型 (float / double)
//   - name  : 字符串名称 (用于日志/序列化)
//   - size  : 字节数 (sizeof(type))
//   - value : 反向回指枚举值
// 用法:
//   using Scalar = typename AstroScalarTraits<AstroScalarType::FP64>::type;
//   static_assert(std::is_same_v<Scalar, double>);
// ============================================================================
template<AstroScalarType S>
struct AstroScalarTraits;

template<>
struct AstroScalarTraits<AstroScalarType::FP32> {
    using type = float;
    static constexpr const char* name = "float32";
    static constexpr size_t size = 4;
    static constexpr AstroScalarType value = AstroScalarType::FP32;
};

template<>
struct AstroScalarTraits<AstroScalarType::FP64> {
    using type = double;
    static constexpr const char* name = "float64";
    static constexpr size_t size = 8;
    static constexpr AstroScalarType value = AstroScalarType::FP64;
};

// ============================================================================
// 运行时类型转字符串
// 用于日志、配置回显、HISS 头字段等需要可读名称的场景
// ============================================================================
inline const char* astro_scalar_type_name(AstroScalarType t) {
    return (t == AstroScalarType::FP32) ? "float32" : "float64";
}

// ============================================================================
// 运行时类型转字节数
// 用于缓冲区分配、ABI 校验等需要精确字节数的场景
// ============================================================================
inline size_t astro_scalar_type_size(AstroScalarType t) {
    return (t == AstroScalarType::FP32) ? 4 : 8;
}

// ============================================================================
// ASTRO_SCALAR_DISPATCH - 运行时 dtype 分发宏 (lambda 形式)
// ----------------------------------------------------------------------------
// 设计意图:
//   当运行时从配置/文件读到 AstroScalarType 后, 需要在编译时实例化对应的
//   模板代码 (例如 drizzle / HISS 编解码). 本宏通过 if-else + std::integral_constant
//   将运行时枚举映射为编译时常量, 再交给 lambda 推导出具体类型.
//
// 用法:
//   ASTRO_SCALAR_DISPATCH(dtype, [&](auto scalar_type_constant) {
//       // 注意: 必须用 decltype(scalar_type_constant)::value 取出嵌套的枚举常量,
//       //       decltype(scalar_type_constant) 本身是 std::integral_constant 类型,
//       //       不能直接作为 AstroScalarTraits 的非类型模板参数.
//       using Scalar = typename AstroScalarTraits<decltype(scalar_type_constant)::value>::type;
//       // ... 使用 Scalar 类型实例化算法
//   });
//
// 注意:
//   - lambda 的两个分支都会被编译器实例化 (宏展开为 if-else, body 在两分支都出现),
//     因此 lambda 内不要写只对单一类型成立的 static_assert; 若需类型特定逻辑,
//     请用 if constexpr (std::is_same_v<Scalar, ...>) 区分.
//   - 运行时只执行匹配分支.
//   - 若 dtype 取值不在 FP32/FP64 范围内, 不执行任何分支 (调用方需自行保证).
// ============================================================================
#define ASTRO_SCALAR_DISPATCH(dtype, lambda) \
    do { \
        if ((dtype) == AstroScalarType::FP32) { \
            lambda(std::integral_constant<AstroScalarType, AstroScalarType::FP32>{}); \
        } else if ((dtype) == AstroScalarType::FP64) { \
            lambda(std::integral_constant<AstroScalarType, AstroScalarType::FP64>{}); \
        } else { \
            /* 不应该到达这里: dtype 取值非法, 静默跳过 */ \
        } \
    } while(0)

// ============================================================================
// ASTRO_SCALAR_DISPATCH_T - 运行时 dtype 分发宏 (直接 typedef 形式)
// ----------------------------------------------------------------------------
// 设计意图:
//   提供更轻量的写法, 在宏内部直接 using 出类型名 T, 适合不需要 integral_constant
//   场景的快速分发. 缺点是 T 名字固定, 不能在同一个作用域里多次使用.
//
// 用法:
//   ASTRO_SCALAR_DISPATCH_T(dtype, T, {
//       std::vector<T> buf(n);
//       // ... 使用 buf
//   });
//
// 注意:
//   - body 内的 T 在两个分支中分别被 using 为 float / double
//   - body 内不要写 return / break 试图跳出宏 (do-while 包裹)
// ============================================================================
#define ASTRO_SCALAR_DISPATCH_T(dtype, T, body) \
    do { \
        if ((dtype) == AstroScalarType::FP32) { \
            using T = float; body; \
        } else if ((dtype) == AstroScalarType::FP64) { \
            using T = double; body; \
        } \
    } while(0)
