#pragma once
// ============================================================================
// precision_context.h - AstroCS 双精度 ABI 改造: 全局精度上下文
// ----------------------------------------------------------------------------
// 设计意图:
// 要求 AstroCS 全链路统一精度, 且只能存在唯一一个 PrecisionContext.
// 本类作为单例, 由 orchestrator 在启动阶段根据配置设置一次精度,
// 各模块在初始化/数据处理时查询当前精度, 据此决定走 FP32 还是 FP64 实例.
//
// 使用约定:
// - 启动顺序: orchestrator 读配置 -> PrecisionContext::set_scalar_type()
// -> 各子模块初始化 (读取 scalar_type()) -> 数据处理
// - 线程安全: 仅启动阶段写入, 数据处理阶段只读; 多线程只读无需加锁.
// 若需要在运行中切换精度 (当前不支持), 调用方需自行同步.
// - 默认值: FP32, 保证未显式初始化时与历史行为兼容.
// ============================================================================

#include "astro_scalar.h"

// ============================================================================
// PrecisionContext - 全局精度上下文 (单例)
// 要求: 唯一 PrecisionContext, 全链路统一精度
// 由 orchestrator 在启动时设置, 各模块查询当前精度
// ============================================================================
class PrecisionContext {
public:
    // 单例入口: 全进程唯一实例
    static PrecisionContext& instance() {
        static PrecisionContext ctx;
        return ctx;
    }

    // 获取当前精度类型
    AstroScalarType scalar_type() const { return scalar_type_; }

    // 设置精度类型 (由 orchestrator 在启动时调用)
    // 注: 数据处理阶段不应再调用, 否则需调用方自行处理线程同步
    void set_scalar_type(AstroScalarType type) { scalar_type_ = type; }

    // 便捷查询: 当前是否 FP32 / FP64
    bool is_fp32() const { return scalar_type_ == AstroScalarType::FP32; }
    bool is_fp64() const { return scalar_type_ == AstroScalarType::FP64; }

    // 获取当前精度的类型大小 (字节)
    size_t scalar_size() const { return astro_scalar_type_size(scalar_type_); }

    // 获取当前精度的类型名称
    const char* scalar_name() const { return astro_scalar_type_name(scalar_type_); }

private:
    // 单例: 禁止外部构造/拷贝/赋值
    PrecisionContext() = default;
    PrecisionContext(const PrecisionContext&) = delete;
    PrecisionContext& operator=(const PrecisionContext&) = delete;

    AstroScalarType scalar_type_ = AstroScalarType::FP32;  // 默认 FP32 (历史兼容)
};
