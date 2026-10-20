#ifndef ACSD_SHARED_CANONICAL_DOUBLE_H
#define ACSD_SHARED_CANONICAL_DOUBLE_H
// ============================================================================
// canonical_double.h - 浮点**无损**规范串化的**单一实现**（全仓共享）
// ----------------------------------------------------------------------------
// 用途: 任何进入**内容身份**（哈希前规范串）的浮点量都必须用本实现序列化。
// 理由（Issue #106 独立源码审核 P2-1）:
//
//   · 十进制定宽写法（printf "%.12g" / std::to_string）**不是单射**:
//     656.28 与 nextafter(656.28, +inf) 都序列化成 "656.28"，尽管两者的
//     binary64 位模式不同 ⇒ 两个**不同**的响应参数会共享同一身份串。
//     身份串一旦被当作严格内容身份，这就是静默的身份碰撞。
//   · C++17 std::to_chars(chars_format::hex) 输出 IEEE-754 值的**十六进制
//     有效数字 + p 指数**：与 binary64 一一对应（strtod 可逐位回读），
//     不经过十进制往返，也不经过 locale（小数点/千分位无关）。
//
// 与 acsd::phase1::precision::canonical_double 的关系: 后者是本实现的
// **薄封装**（保持既有公开 API 与行为不变），本头是唯一定义处。
// 依据: docs/engineering/standards/CODE.md「MUST：禁止重复 production
// science implementation（单一实现 + oracle）」。
// ============================================================================

#include <charconv>
#include <string>
#include <system_error>

namespace acsd {
namespace shared {

// 无损规范串: 有限值 ⇒ 十六进制有效数字（可 strtod 逐位回读）;
//             inf/nan ⇒ std::to_chars 的 "inf"/"nan" 文本。
// 缓冲区 64 字节对任何 binary64 的 hexfloat 形式都足够（最长 ~24 字节）。
inline std::string canonical_double_hex(double v) {
    char buf[64];
    const auto res = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::hex);
    if (res.ec != std::errc()) {
        // 理论上不可达（64 字节足够任何 binary64 hexfloat）;
        // 具名退化而不是静默截断（身份串必须能看出"未表示成功"）。
        return std::string("unrepresentable");
    }
    return std::string(buf, res.ptr);
}

}  // namespace shared
}  // namespace acsd

#endif  // ACSD_SHARED_CANONICAL_DOUBLE_H
