// ============================================================================
// hiss_transform.h - AstroCS HISS Transform 正式路径 (WP-G 步骤12)
//
// 依据:
//   - docs/stage1_fix/spec.md 步骤12 (transform 正式路径)
//   - docs/stage1_fix/00_COMMON_CONTRACTS.md §1.1 (模块边界)
//   - 02_FROZEN_STAGE1_HISS_SPEC.md §15 (子块目录 transform_id 字段)
//
// 职责:
//   - 提供 HISS 容器的字节级变换 (在压缩前/解压后执行)
//   - 支持 NONE / BYTE_SHUFFLE / DELTA / DELTA_VARINT 四种变换
//   - Writer 在压缩前调用 apply_transform
//   - Reader 在解压后调用 inverse_transform
//   - 实验必须复用正式路径 (不是 benchmark 单独实现)
//
// 变换类型说明:
//   NONE         - 无变换 (零开销)
//   BYTE_SHUFFLE - 字节重排, 将相同字节位置聚合 (适用于浮点数据, 提高压缩率)
//                  forward: 按字节位置重排 (第0字节一组, 第1字节一组...)
//                  inverse: 还原原始字节顺序
//   DELTA        - 差分编码 (适用于整数数据)
//                  forward: 每个元素减去前一个元素 (无符号环绕运算)
//                  inverse: 累加还原
//   DELTA_VARINT - 差分 + 可变长度整数编码 (组合变换, 适用于整数序列)
//                  forward: 先 delta 编码, 再 zig-zag + varint 编码
//                  inverse: 先 varint 解码 + zig-zag 解码, 再 delta 还原
//                  输出格式: [n_elements: uint32 LE][varint 编码的 zig-zag delta 值]
//                  (n_elements 前缀使 inverse 能自确定输出大小)
//
// 设计约束:
//   - 代码模块化, 每个 transform 类型实现为独立函数
//   - 支持的 element_size: 1 (uint8/int8), 2 (uint16/int16), 4 (uint32/int32/float32), 8 (uint64/int64/float64)
//   - DELTA 和 DELTA_VARINT 仅支持 1/2/4/8 字节元素
//   - BYTE_SHUFFLE 对 element_size=1 是 no-op (所有字节都在位置 0)
//   - 空输入安全处理 (返回空输出, 不崩溃)
// ============================================================================

#ifndef HISS_TRANSFORM_H
#define HISS_TRANSFORM_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

#include "hiss_format.h"  // TransformId (用于与 HISS 容器互转)

namespace hiss {

// ============================================================================
// TransformType - 变换类型枚举 (独立于 TransformId, 用于 transform 模块内部)
//
// 注意: 与 hiss_format.h 中的 TransformId 的关系:
//   - NONE/BYTE_SHUFFLE/DELTA 数值一致
//   - DELTA_VARINT (本枚举=3) 对应 TransformId::DELTA_VARINT (=4, 新增)
//   - TransformId::VARINT (=3, 旧值) 向后兼容映射到 TransformType::DELTA_VARINT
// ============================================================================

enum class TransformType : uint16_t {
    NONE         = 0,
    BYTE_SHUFFLE = 1,
    DELTA        = 2,
    DELTA_VARINT = 3,
};

// ============================================================================
// 正向变换接口
//
// type         - 变换类型
// data         - 输入数据指针 (可为 nullptr 当 data_size=0)
// data_size    - 输入数据字节数
// element_size - 元素大小 (如 float=4, double=8, uint8=1)
// 返回         - 变换后的数据 (失败时返回空 vector)
//
// 注意:
//   - NONE: 返回输入的副本 (零开销, 数据不变)
//   - BYTE_SHUFFLE/DELTA: 输出大小 == 输入大小
//   - DELTA_VARINT: 输出大小可变 (通常 <= 输入大小, 随机数据可能更大)
// ============================================================================

std::vector<uint8_t> apply_transform(TransformType type,
                                       const uint8_t* data,
                                       size_t data_size,
                                       size_t element_size);

// ============================================================================
// 逆向变换接口 (还原原始数据)
//
// type                 - 变换类型
// data                 - 变换后的数据指针 (可为 nullptr 当 data_size=0)
// data_size            - 变换后的数据字节数
// element_size         - 元素大小
// expected_output_size - 期望的输出大小 (0 表示自动从数据中确定;
//                        非 0 时用于校验, 不匹配则返回空)
// 返回                 - 还原后的原始数据 (失败时返回空 vector)
//
// 注意:
//   - 对于 DELTA_VARINT, 输出大小从数据前缀 (n_elements) 自动确定,
//     expected_output_size=0 时自动计算, 非 0 时校验
//   - 对于其他变换, 输出大小 == 输入大小, expected_output_size 用于校验
// ============================================================================

std::vector<uint8_t> inverse_transform(TransformType type,
                                        const uint8_t* data,
                                        size_t data_size,
                                        size_t element_size,
                                        size_t expected_output_size);

// ============================================================================
// 变换类型名称与枚举互转
// ============================================================================

const char* transform_type_name(TransformType type);
TransformType transform_type_from_name(const std::string& name);

// ============================================================================
// TransformType (transform 模块) <-> TransformId (hiss_format.h 容器)
//
// 映射关系:
//   TransformType::NONE         <-> TransformId::NONE
//   TransformType::BYTE_SHUFFLE <-> TransformId::BYTE_SHUFFLE
//   TransformType::DELTA        <-> TransformId::DELTA
//   TransformType::DELTA_VARINT <-> TransformId::DELTA_VARINT (新增, =4)
//   TransformId::VARINT (=3, 旧值) -> TransformType::DELTA_VARINT (向后兼容)
// ============================================================================

TransformType transform_id_to_type(TransformId id);
TransformId transform_type_to_id(TransformType type);

} // namespace hiss

#endif // HISS_TRANSFORM_H
