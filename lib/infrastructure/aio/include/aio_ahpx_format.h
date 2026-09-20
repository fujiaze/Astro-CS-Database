#ifndef AIO_AHPX_FORMAT_H
#define AIO_AHPX_FORMAT_H

#include <cstdint>
#include <cstddef>
#include <string>

namespace aio::ahpx {

// .ahpx 文件格式常量
constexpr char MAGIC[4] = {'A', 'H', 'P', 'X'};
constexpr uint16_t VERSION = 1;

// 文件头固定部分大小 (bytes)
// Magic(4) + Version(2) + HeaderSize(4) + HeaderCompSize(4) + BlockCount(4) = 18
constexpr size_t HEADER_FIXED_SIZE = 18;

// ============================================================================
// 数据面 (变更 AHPX-WEIGHT-RETIRE-20260920 / 2026-09-20)
//
// 依据 ASTROCS_DESIGN §2.1 与 GAP_AUDIT §9.73 裁决 A44: 全程只有 SNR,
// 不存在「权重模式」。本容器只承载两样东西:
//   - "pixel" 块: 图像数据;
//   - "snr"   块: 帧级 SNR (信噪比)。
// **不承载任何权重**: 权重是阶段二按天球像素对应的那组输入帧现场算出的派生量,
// 阶段一/阶段三既不产生也不消费。稀疏相对 SNR 比值 (SNR_c / SNR_frame) 由 HiPS
// 产品层承载 (ASTROCS_DESIGN §3.4), 不在本容器内。
//
// 旧版 .ahpx 头 JSON 的 "weight" 字段 (权重模式 SCALAR/GRID/PIXEL 的载体) 已作废:
// 读侧见到该字段或同名数据块 ⇒ 显式拒绝 (fail-closed, 禁静默忽略);
// 写侧见到调用方元数据携带该字段 ⇒ 拒绝写出。
// ============================================================================
constexpr char RETIRED_WEIGHT_FIELD[] = "weight";

// 判断 JSON 文本是否存在键 key: 只认 '"key"' 之后跟 ':' 的键位,
// 不匹配值中的同名子串。用于已作废字段探测, 不引入第三方 JSON 库。
inline bool hasJsonKey(const std::string& json, const char* key) {
    const std::string pattern = std::string("\"") + key + "\"";
    size_t pos = json.find(pattern);
    while (pos != std::string::npos) {
        size_t p = pos + pattern.size();
        while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                                   json[p] == '\n' || json[p] == '\r')) {
            p++;
        }
        if (p < json.size() && json[p] == ':') return true;
        pos = json.find(pattern, pos + 1);
    }
    return false;
}

// 压缩编码
enum class Codec : uint8_t {
    NONE = 0,
    ZSTD = 1,
    LZ4  = 2,
    JPEG = 3   // 仅用于缩略图(保留，当前不使用)
};

// 数据块索引
struct BlockIndex {
    char     id[32];        // 块标识 (如 "pixel", "snr")
    uint64_t offset;        // 文件内偏移
    uint64_t size;          // 压缩后大小
    uint8_t  codec;         // Codec 枚举值
    int      level;         // 压缩级别 (zstd 1-22, lz4 忽略)
};

} // namespace aio::ahpx

#endif // AIO_AHPX_FORMAT_H
