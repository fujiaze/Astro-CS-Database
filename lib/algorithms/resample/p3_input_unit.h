// lib/algorithms/resample/p3_input_unit.h — Stage3 signal 输入单位身份裁决
// （Issue #91 唯一实现；session 与 scheduler 两个入口同源同判）
//
// 判决依据（不另建第二套单位解析体系）:
//   · 单位串解析/归类 = acsd::p3rsmp::resolve_bunit（冻结单位表，
//     lib/algorithms/resample/p3_rsmp_units.cpp 已是唯一实现，本文件只调用它）;
//   · docs/science/unified/DATA_SEMANTICS.md §3「区分观测计数、线性通量和面亮度」;
//   · FZ-BUNIT-SEMANTICS（产品 signal 面亮度单位 canonical 串由线性基单位导出，
//     裸 "ADU"/"XPMODEL" 需 ACSD_PIXEL_SEMANTICS=surface_brightness + ACSD_PIXEL_AREA_POWER=-2）。
//
// 通过 ⇒ accepted=true 且 bunit_canonical = 冻结单位表对该线性基单位导出的 canonical
//        面亮度串（Issue #9: 原生 "ADU/sr"、已施加 Gaia XP 模型标度 "XPMODEL/sr"）。
// 拒绝 ⇒ accepted=false，code/reason 可诊断；input_error 决定 CLI 退出码类别:
//        true  → 3（输入缺失/格式错/声明冲突）
//        false → 4（科学语义/身份矛盾）
//
// 未通过时**绝不**补写任何确定量纲（禁 silent default "ADU/sr"）。
#ifndef ACSD_P3_INPUT_UNIT_H
#define ACSD_P3_INPUT_UNIT_H

#include <string>

#include "p3_rsmp.h"

namespace acsd::phase3 {

// 冻结产品 signal 面亮度单位串（唯一单源；docs/science/unified/DATA_SEMANTICS.md §3）。
// 通过裁决后写出的所有单位面（p3_props.json / p3_resampled.json / FITS BUNIT）一律取
// **裁决返回的 canonical 串**（= 该输入线性基单位的面亮度串）。
inline constexpr const char* kP3SignalBunitCanonical =
    "ADU/sr";  // 原生校准计数族（LinearBase::Adu）
inline constexpr const char* kP3SignalBunitCanonicalModel =
    "XPMODEL/sr";  // Gaia XP 合成通带模型通量族（LinearBase::XpModel; Issue #9）

struct P3InputUnitVerdict {
    bool accepted = false;
    bool input_error = false;      // true → exit 3; false → exit 4
    std::string code;              // 机器可判错误码（节点 manifest semantic_code）
    std::string reason;            // 人类可读（含定位）
    std::string properties_path;   // 本裁决实际读取的文件（定位信息）
    std::string bunit_raw;         // 文件中 BUNIT 的原始声明（未归一）
    std::string bunit_canonical;   // 通过时 = 该线性基单位的 canonical 面亮度串
    std::string linear_base;       // 通过时 = "ADU" | "XPMODEL"（Issue #9 身份面）
    int unit_declarations = 0;     // 单位声明面数量（BUNIT 逻辑键 + ACSD_SIGNAL_UNIT）
    std::string pixel_semantics;   // ACSD_PIXEL_SEMANTICS 原值（未声明 = 空）
    std::string pixel_area_power;  // ACSD_PIXEL_AREA_POWER 原值（未声明 = 空）
};

/* 对已读入的 properties 文本裁决（纯函数，无 I/O；便于独立单元测试与复用）。
 * text = <hips_dir>/signal/properties 的字节内容；properties_path 仅用于诊断定位。 */
P3InputUnitVerdict p3_input_unit_verdict_from_text(const std::string& text,
                                                   const std::string& properties_path);

/* 读 <hips_dir>/signal/properties 并对 signal 单位身份裁决（定义在
 * p3_input_unit_io.cpp；I/O 一律经 aio 唯一实现）。 */
P3InputUnitVerdict p3_input_unit_verdict(const std::string& hips_dir);

}  // namespace acsd::phase3

#endif  // ACSD_P3_INPUT_UNIT_H
