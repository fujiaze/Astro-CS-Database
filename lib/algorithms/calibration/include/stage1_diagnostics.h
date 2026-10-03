#ifndef ACSD_STAGE1_DIAGNOSTICS_H
#define ACSD_STAGE1_DIAGNOSTICS_H

// ============================================================================
// stage1_diagnostics.h - 最优 Dark 估计的结构化诊断
//
// 本头文件从已退场的 aio/hiss_format.h 迁出。迁出的理由: 该结构体是
// **标定 (calibration) 的诊断契约**, 与 .hiss 容器格式无任何关系,
// 却是 astro_calibration.h 公开 API 的形参类型 —— normalize 路径
// (module_adapters.cpp / cosmetic module_entry.cpp / p1_session.cpp)
// 都经它。
//
// 语义 (已冻结): 最优 Dark 估计失败时输出诊断并自动回退。
// ============================================================================

namespace acsd {

struct Stage1Diagnostics {
    int      success = 0;            // 0=成功, <0=失败
    char     stage[32] = "";         // 阶段名
    char     code[32] = "";          // 错误码
    char     message[256] = "";      // 人类可读信息
    int      fell_back = 0;          // 是否回退 (0=未回退, 1=已回退)
    char     fallback_from[32] = ""; // 原模式
    char     fallback_to[32] = "";   // 回退后模式
};

}  // namespace acsd

#endif  // ACSD_STAGE1_DIAGNOSTICS_H
