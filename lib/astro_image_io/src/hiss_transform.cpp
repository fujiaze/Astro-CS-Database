// ============================================================================
// hiss_transform.cpp - AstroCS HISS Transform 正式路径实现 (WP-G 步骤12)
//
// 实现:
//   1. BYTE_SHUFFLE: 字节重排 (forward/inverse)
//   2. DELTA: 差分编码 (forward/inverse, 无符号环绕运算)
//   3. DELTA_VARINT: 差分 + zig-zag + varint 组合编码 (forward/inverse)
//   4. 分发函数: apply_transform / inverse_transform
//   5. 枚举互转: TransformType <-> name, TransformType <-> TransformId
//
// 设计说明:
//   - 每个 transform 类型实现为独立 static 函数, 通过分发函数调度
//   - DELTA 使用无符号环绕运算 (uint64 内部计算, 截断到 element_size 字节)
//     正确处理有符号/无符号整数, 包括溢出情况
//   - DELTA_VARINT 输出格式: [n_elements: uint32 LE][varint 编码的 zig-zag delta]
//     n_elements 前缀使 inverse 能自确定输出大小, 无需外部信息
//   - zig-zag 编码: 小幅度的正负 delta 映射到小的无符号值, 提升 varint 压缩率
//   - varint (LEB128): 每字节 7 bit 数据 + 1 bit 继续标志
//   - 空输入安全处理 (返回空或仅含 n_elements=0 的前缀)
// ============================================================================

#include "hiss_transform.h"

#include <cstdio>
#include <cstring>

// R13 (HISS_IO_REPAIR): 逐调用日志降级 — 仅编译期 HISS_VERBOSE 输出
// (正常模式只保留阶段/汇总/错误; stderr 重定向文件时每条 fprintf 写盘,
//  285 Tile 的 forward/inverse 逐调用日志会拖慢写入与 Verify)
#ifdef HISS_VERBOSE
#define HISS_DLOG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define HISS_DLOG(fmt, ...) do {} while (0)
#endif

namespace hiss {

// ============================================================================
// 1. 枚举互转
// ============================================================================

const char* transform_type_name(TransformType type) {
    switch (type) {
        case TransformType::NONE:         return "NONE";
        case TransformType::BYTE_SHUFFLE: return "BYTE_SHUFFLE";
        case TransformType::DELTA:        return "DELTA";
        case TransformType::DELTA_VARINT: return "DELTA_VARINT";
        default:                          return "UNKNOWN";
    }
}

TransformType transform_type_from_name(const std::string& name) {
    if (name == "NONE")         return TransformType::NONE;
    if (name == "BYTE_SHUFFLE") return TransformType::BYTE_SHUFFLE;
    if (name == "DELTA")        return TransformType::DELTA;
    if (name == "DELTA_VARINT") return TransformType::DELTA_VARINT;
    return TransformType::NONE;
}

TransformType transform_id_to_type(TransformId id) {
    switch (id) {
        case TransformId::NONE:         return TransformType::NONE;
        case TransformId::BYTE_SHUFFLE: return TransformType::BYTE_SHUFFLE;
        case TransformId::DELTA:        return TransformType::DELTA;
        // 向后兼容: 旧 TransformId::VARINT(=3) 语义为 delta+varint 组合
        case TransformId::VARINT:       return TransformType::DELTA_VARINT;
        case TransformId::DELTA_VARINT: return TransformType::DELTA_VARINT;
        default:                        return TransformType::NONE;
    }
}

TransformId transform_type_to_id(TransformType type) {
    switch (type) {
        case TransformType::NONE:         return TransformId::NONE;
        case TransformType::BYTE_SHUFFLE: return TransformId::BYTE_SHUFFLE;
        case TransformType::DELTA:        return TransformId::DELTA;
        case TransformType::DELTA_VARINT: return TransformId::DELTA_VARINT;
        default:                          return TransformId::NONE;
    }
}

// ============================================================================
// 2. 内部辅助: 小端序读写 + 元素掩码
// ============================================================================

// 读取 element_size 字节为 uint64 (小端序, 零扩展)
static inline uint64_t read_element_le(const uint8_t* p, size_t element_size) {
    uint64_t v = 0;
    for (size_t i = 0; i < element_size; i++) {
        v |= (uint64_t)p[i] << (8 * i);
    }
    return v;
}

// 写入 uint64 为 element_size 字节 (小端序, 截断到低 element_size 字节)
static inline void write_element_le(uint8_t* p, uint64_t value, size_t element_size) {
    for (size_t i = 0; i < element_size; i++) {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// element_size 字节的掩码 (如 element_size=4 → 0xFFFFFFFF)
static inline uint64_t element_mask(size_t element_size) {
    return (element_size >= 8) ? ~0ULL : ((1ULL << (8 * element_size)) - 1);
}

// 检查 element_size 是否受 DELTA/DELTA_VARINT 支持
static inline bool is_valid_element_size(size_t element_size) {
    return element_size == 1 || element_size == 2 ||
           element_size == 4 || element_size == 8;
}

// ============================================================================
// 3. BYTE_SHUFFLE 实现
//
// 将 [e0_b0, e0_b1, ..., e0_bS, e1_b0, e1_b1, ..., e1_bS, ...]
// 重排为 [e0_b0, e1_b0, ..., eN_b0, e0_b1, e1_b1, ..., eN_b1, ...]
// 即: 相同字节位置的字节聚合到一起 (适用于浮点数据, 高位字节往往相似)
//
// element_size=1 时为 no-op (所有字节都在位置 0)
// ============================================================================

static std::vector<uint8_t> apply_byte_shuffle(const uint8_t* data,
                                                 size_t data_size,
                                                 size_t element_size) {
    if (data_size == 0 || element_size == 0) {
        return {};
    }
    if (data_size % element_size != 0) {
        fprintf(stderr,
                "[hiss][transform] BYTE_SHUFFLE forward: data_size=%zu 不是 element_size=%zu 的倍数\n",
                data_size, element_size);
        return {};
    }

    size_t n_elements = data_size / element_size;
    std::vector<uint8_t> output(data_size);

    // 按字节位置分组: 输出[byte_pos * n_elements + elem_idx] = 输入[elem_idx * element_size + byte_pos]
    for (size_t byte_pos = 0; byte_pos < element_size; byte_pos++) {
        const uint8_t* src = data + byte_pos;
        uint8_t*       dst = output.data() + byte_pos * n_elements;
        for (size_t elem_idx = 0; elem_idx < n_elements; elem_idx++) {
            dst[elem_idx] = src[elem_idx * element_size];
        }
    }

    HISS_DLOG(
            "[hiss][transform] BYTE_SHUFFLE forward: data_size=%zu element_size=%zu n_elements=%zu\n",
            data_size, element_size, n_elements);
    return output;
}

static std::vector<uint8_t> inverse_byte_shuffle(const uint8_t* data,
                                                   size_t data_size,
                                                   size_t element_size) {
    if (data_size == 0 || element_size == 0) {
        return {};
    }
    if (data_size % element_size != 0) {
        fprintf(stderr,
                "[hiss][transform] BYTE_SHUFFLE inverse: data_size=%zu 不是 element_size=%zu 的倍数\n",
                data_size, element_size);
        return {};
    }

    size_t n_elements = data_size / element_size;
    std::vector<uint8_t> output(data_size);

    // 逆操作: 输出[elem_idx * element_size + byte_pos] = 输入[byte_pos * n_elements + elem_idx]
    for (size_t byte_pos = 0; byte_pos < element_size; byte_pos++) {
        const uint8_t* src = data + byte_pos * n_elements;
        uint8_t*       dst = output.data() + byte_pos;
        for (size_t elem_idx = 0; elem_idx < n_elements; elem_idx++) {
            dst[elem_idx * element_size] = src[elem_idx];
        }
    }

    HISS_DLOG(
            "[hiss][transform] BYTE_SHUFFLE inverse: data_size=%zu element_size=%zu n_elements=%zu\n",
            data_size, element_size, n_elements);
    return output;
}

// ============================================================================
// 4. DELTA 实现
//
// 差分编码: delta[0] = input[0], delta[i] = input[i] - input[i-1]
// 使用无符号环绕运算 (uint64 内部计算, 截断到 element_size 字节)
// 对有符号/无符号整数均正确 (二补码运算)
//
// 输出大小 == 输入大小 (大小保持变换)
// ============================================================================

static std::vector<uint8_t> apply_delta(const uint8_t* data,
                                          size_t data_size,
                                          size_t element_size) {
    if (data_size == 0) {
        return {};
    }
    if (!is_valid_element_size(element_size)) {
        fprintf(stderr,
                "[hiss][transform] DELTA forward: 不支持的 element_size=%zu (需 1/2/4/8)\n",
                element_size);
        return {};
    }
    if (data_size % element_size != 0) {
        fprintf(stderr,
                "[hiss][transform] DELTA forward: data_size=%zu 不是 element_size=%zu 的倍数\n",
                data_size, element_size);
        return {};
    }

    size_t n_elements = data_size / element_size;
    std::vector<uint8_t> output(data_size);
    uint64_t mask = element_mask(element_size);

    uint64_t prev = 0;
    for (size_t i = 0; i < n_elements; i++) {
        uint64_t curr = read_element_le(data + i * element_size, element_size);
        uint64_t delta = (curr - prev) & mask;  // 无符号环绕减法
        write_element_le(output.data() + i * element_size, delta, element_size);
        prev = curr;
    }

    HISS_DLOG(
            "[hiss][transform] DELTA forward: data_size=%zu element_size=%zu n_elements=%zu\n",
            data_size, element_size, n_elements);
    return output;
}

static std::vector<uint8_t> inverse_delta(const uint8_t* data,
                                            size_t data_size,
                                            size_t element_size,
                                            size_t expected_output_size) {
    if (data_size == 0) {
        return {};
    }
    if (!is_valid_element_size(element_size)) {
        fprintf(stderr,
                "[hiss][transform] DELTA inverse: 不支持的 element_size=%zu (需 1/2/4/8)\n",
                element_size);
        return {};
    }
    if (data_size % element_size != 0) {
        fprintf(stderr,
                "[hiss][transform] DELTA inverse: data_size=%zu 不是 element_size=%zu 的倍数\n",
                data_size, element_size);
        return {};
    }

    // DELTA 是大小保持变换, 输出大小 == 输入大小
    // expected_output_size 非 0 时校验
    if (expected_output_size != 0 && expected_output_size != data_size) {
        fprintf(stderr,
                "[hiss][transform] DELTA inverse: expected_output_size=%zu != data_size=%zu\n",
                expected_output_size, data_size);
        return {};
    }

    size_t n_elements = data_size / element_size;
    std::vector<uint8_t> output(data_size);
    uint64_t mask = element_mask(element_size);

    uint64_t accum = 0;
    for (size_t i = 0; i < n_elements; i++) {
        uint64_t delta = read_element_le(data + i * element_size, element_size);
        accum = (accum + delta) & mask;  // 无符号环绕加法
        write_element_le(output.data() + i * element_size, accum, element_size);
    }

    HISS_DLOG(
            "[hiss][transform] DELTA inverse: data_size=%zu element_size=%zu n_elements=%zu\n",
            data_size, element_size, n_elements);
    return output;
}

// ============================================================================
// 5. VARINT (LEB128) + ZIG-ZAG 编码辅助
// ============================================================================

// Zig-zag 编码: 有符号 → 无符号 (小幅度映射到小值)
//   0 → 0, -1 → 1, 1 → 2, -2 → 3, 2 → 4, ...
static inline uint64_t zig_zag_encode(int64_t n) {
    return (uint64_t)((n << 1) ^ (n >> 63));
}

// Zig-zag 解码: 无符号 → 有符号
static inline int64_t zig_zag_decode(uint64_t u) {
    return (int64_t)((u >> 1) ^ -(int64_t)(u & 1));
}

// Varint 编码 (LEB128): 写入变长无符号整数
// 每字节 7 bit 数据 + 1 bit 继续 (高位=1 表示后续还有字节)
static void varint_encode(uint64_t value, std::vector<uint8_t>& out) {
    while (value >= 0x80) {
        out.push_back((uint8_t)((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.push_back((uint8_t)value);
}

// Varint 解码 (LEB128): 读取变长无符号整数
// p     - 输入指针 (向前推进)
// end   - 输入结尾 (防止越界)
// value - 输出解码值
// 返回 true=成功, false=数据截断或溢出
static bool varint_decode(const uint8_t*& p, const uint8_t* end, uint64_t& value) {
    value = 0;
    int shift = 0;
    while (p < end) {
        uint8_t byte = *p++;
        value |= (uint64_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return true;  // 最后一个字节
        }
        shift += 7;
        if (shift > 63) {
            return false;  // 溢出 (varint 过长)
        }
    }
    return false;  // 数据截断 (未读到终止字节)
}

// ============================================================================
// 6. DELTA_VARINT 实现 (组合变换: delta + zig-zag + varint)
//
// 输出格式:
//   [n_elements: uint32 LE]  (4 字节, 元素数量)
//   [varint 编码的 zig-zag delta 值...]  (变长)
//
// forward 流程:
//   1. 读取 N 个 element_size 字节元素为 uint64
//   2. 计算 delta[i] = input[i] - input[i-1] (无符号环绕)
//   3. 将 delta 重解释为有符号 (int8/int16/int32/int64)
//   4. zig-zag 编码 → 无符号
//   5. varint 编码 → 变长字节
//   6. 输出 = [n_elements: u32] + [varint 数据]
//
// inverse 流程:
//   1. 读取 n_elements 前缀
//   2. varint 解码 n_elements 个值
//   3. zig-zag 解码 → 有符号 delta
//   4. 转为无符号进行环绕累加
//   5. 输出 N * element_size 字节
// ============================================================================

static std::vector<uint8_t> apply_delta_varint(const uint8_t* data,
                                                 size_t data_size,
                                                 size_t element_size) {
    // 空输入: 输出仅含 n_elements=0 的前缀 (4 字节)
    if (data_size == 0) {
        std::vector<uint8_t> output(4, 0);
        HISS_DLOG(
                "[hiss][transform] DELTA_VARINT forward: 空输入 → n_elements=0 (4 字节)\n");
        return output;
    }
    if (!is_valid_element_size(element_size)) {
        fprintf(stderr,
                "[hiss][transform] DELTA_VARINT forward: 不支持的 element_size=%zu (需 1/2/4/8)\n",
                element_size);
        return {};
    }
    if (data_size % element_size != 0) {
        fprintf(stderr,
                "[hiss][transform] DELTA_VARINT forward: data_size=%zu 不是 element_size=%zu 的倍数\n",
                data_size, element_size);
        return {};
    }

    size_t n_elements = data_size / element_size;
    uint64_t mask = element_mask(element_size);

    std::vector<uint8_t> output;
    output.reserve(data_size + 4);  // 预分配 (下限估计)

    // 写入 n_elements (uint32 小端序)
    uint32_t n = (uint32_t)n_elements;
    output.push_back((uint8_t)(n & 0xFF));
    output.push_back((uint8_t)((n >> 8) & 0xFF));
    output.push_back((uint8_t)((n >> 16) & 0xFF));
    output.push_back((uint8_t)((n >> 24) & 0xFF));

    // 逐元素: delta + zig-zag + varint
    uint64_t prev = 0;
    for (size_t i = 0; i < n_elements; i++) {
        uint64_t curr = read_element_le(data + i * element_size, element_size);
        uint64_t delta_u = (curr - prev) & mask;  // 无符号环绕减法

        // 将无符号 delta 重解释为有符号 (用于 zig-zag 编码)
        int64_t delta_s;
        switch (element_size) {
            case 1: delta_s = (int8_t)(uint8_t)delta_u;  break;
            case 2: delta_s = (int16_t)(uint16_t)delta_u; break;
            case 4: delta_s = (int32_t)(uint32_t)delta_u; break;
            case 8: delta_s = (int64_t)delta_u;           break;
            default: delta_s = (int64_t)delta_u;          break;
        }

        uint64_t zz = zig_zag_encode(delta_s);
        varint_encode(zz, output);
        prev = curr;
    }

    HISS_DLOG(
            "[hiss][transform] DELTA_VARINT forward: data_size=%zu → output_size=%zu "
            "element_size=%zu n_elements=%zu\n",
            data_size, output.size(), element_size, n_elements);
    return output;
}

static std::vector<uint8_t> inverse_delta_varint(const uint8_t* data,
                                                   size_t data_size,
                                                   size_t element_size,
                                                   size_t expected_output_size) {
    // 空输入或不足 n_elements 前缀
    if (data_size == 0) {
        return {};
    }
    if (data_size < 4) {
        fprintf(stderr,
                "[hiss][transform] DELTA_VARINT inverse: data_size=%zu < 4 (无 n_elements 前缀)\n",
                data_size);
        return {};
    }
    if (!is_valid_element_size(element_size)) {
        fprintf(stderr,
                "[hiss][transform] DELTA_VARINT inverse: 不支持的 element_size=%zu (需 1/2/4/8)\n",
                element_size);
        return {};
    }

    // 读取 n_elements 前缀 (uint32 小端序)
    uint32_t n_elements = (uint32_t)data[0] |
                          ((uint32_t)data[1] << 8) |
                          ((uint32_t)data[2] << 16) |
                          ((uint32_t)data[3] << 24);

    size_t output_size = (size_t)n_elements * element_size;
    // Gate 4 fuzz: DELTA_VARINT n_elements 前缀可被损坏为巨大值, 防巨额分配
    if (output_size > HISS_MAX_SUBBLOCK_UNCOMPRESSED ||
        (n_elements > 0 && output_size / (size_t)n_elements != (size_t)element_size)) {
        fprintf(stderr,
                "[hiss][transform] DELTA_VARINT inverse: 输出大小超限 %zu\n",
                output_size);
        return {};
    }

    // expected_output_size 非 0 时校验
    if (expected_output_size != 0 && expected_output_size != output_size) {
        fprintf(stderr,
                "[hiss][transform] DELTA_VARINT inverse: expected_output_size=%zu != computed=%zu "
                "(n_elements=%u, element_size=%zu)\n",
                expected_output_size, output_size, n_elements, element_size);
        return {};
    }

    // n_elements=0: 输出为空 (有效情况)
    if (n_elements == 0) {
        HISS_DLOG(
                "[hiss][transform] DELTA_VARINT inverse: n_elements=0 → 空输出\n");
        return {};
    }

    std::vector<uint8_t> output(output_size);
    uint64_t mask = element_mask(element_size);

    // 解码 varint + zig-zag + 累加
    const uint8_t* p = data + 4;
    const uint8_t* end = data + data_size;
    uint64_t accum = 0;

    for (uint32_t i = 0; i < n_elements; i++) {
        uint64_t zz;
        if (!varint_decode(p, end, zz)) {
            fprintf(stderr,
                    "[hiss][transform] DELTA_VARINT inverse: varint 解码失败 (元素 %u/%u, 已用 %zu/%zu 字节)\n",
                    i, n_elements, (size_t)(p - data), data_size);
            return {};
        }
        int64_t delta_s = zig_zag_decode(zz);
        // 有符号 delta 转为无符号进行环绕加法
        uint64_t delta_u = (uint64_t)delta_s & mask;
        accum = (accum + delta_u) & mask;
        write_element_le(output.data() + i * element_size, accum, element_size);
    }

    HISS_DLOG(
            "[hiss][transform] DELTA_VARINT inverse: data_size=%zu → output_size=%zu "
            "element_size=%zu n_elements=%u\n",
            data_size, output_size, element_size, n_elements);
    return output;
}

// ============================================================================
// 7. 分发函数: apply_transform / inverse_transform
// ============================================================================

std::vector<uint8_t> apply_transform(TransformType type,
                                       const uint8_t* data,
                                       size_t data_size,
                                       size_t element_size) {
    // 空输入: NONE 返回空, DELTA_VARINT 返回 [0,0,0,0], 其他返回空
    if (data == nullptr && data_size > 0) {
        fprintf(stderr, "[hiss][transform] apply_transform: data=nullptr 但 data_size=%zu\n", data_size);
        return {};
    }

    switch (type) {
        case TransformType::NONE:
            // 零开销: 返回输入的副本
            if (data_size == 0) return {};
            return std::vector<uint8_t>(data, data + data_size);

        case TransformType::BYTE_SHUFFLE:
            return apply_byte_shuffle(data, data_size, element_size);

        case TransformType::DELTA:
            return apply_delta(data, data_size, element_size);

        case TransformType::DELTA_VARINT:
            return apply_delta_varint(data, data_size, element_size);

        default:
            fprintf(stderr, "[hiss][transform] apply_transform: 未知 type=%u\n", (unsigned)type);
            return {};
    }
}

std::vector<uint8_t> inverse_transform(TransformType type,
                                        const uint8_t* data,
                                        size_t data_size,
                                        size_t element_size,
                                        size_t expected_output_size) {
    if (data == nullptr && data_size > 0) {
        fprintf(stderr, "[hiss][transform] inverse_transform: data=nullptr 但 data_size=%zu\n", data_size);
        return {};
    }

    switch (type) {
        case TransformType::NONE:
            // 零开销: 返回输入的副本
            if (data_size == 0) return {};
            return std::vector<uint8_t>(data, data + data_size);

        case TransformType::BYTE_SHUFFLE:
            // BYTE_SHUFFLE 是大小保持变换, expected_output_size 用于校验
            if (expected_output_size != 0 && expected_output_size != data_size) {
                fprintf(stderr,
                        "[hiss][transform] BYTE_SHUFFLE inverse: expected=%zu != data_size=%zu\n",
                        expected_output_size, data_size);
                return {};
            }
            return inverse_byte_shuffle(data, data_size, element_size);

        case TransformType::DELTA:
            return inverse_delta(data, data_size, element_size, expected_output_size);

        case TransformType::DELTA_VARINT:
            // DELTA_VARINT 输出大小从数据前缀自动确定, expected_output_size=0 时自动, 非 0 时校验
            return inverse_delta_varint(data, data_size, element_size, expected_output_size);

        default:
            fprintf(stderr, "[hiss][transform] inverse_transform: 未知 type=%u\n", (unsigned)type);
            return {};
    }
}

} // namespace hiss
