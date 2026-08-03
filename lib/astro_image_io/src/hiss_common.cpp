// ============================================================================
// hiss_common.cpp - AstroCS HISS 共享方法实现
//
// 本文件集中实现 hiss_format.h 中声明、被 Writer 与 Reader 共同依赖的方法,
// 避免在 hiss_writer.cpp 和 hiss_reader.cpp 中重复定义导致链接错误。
//
// 包含的方法 (实现以 Writer 为权威版本):
//   1. compute_tile_depth / compute_tile_nside (02_FROZEN §11)
//   2. DrizzleTileAccumulator::finalize_signal / finalize_support / validate_support (§10)
//   3. HissMetadata::to_json / from_json (§16)
//
// 说明:
//   - 实现取自 hiss_writer.cpp (Writer 是权威写入方, 且带 HISS_EXPORT 标记,
//     与 hiss_format.h 中的声明一致)
//   - 日志前缀统一改为 [hiss][common], 体现归属文件
//   - json_escape 为本翻译单元内部辅助函数 (static), 仅 to_json 使用
// ============================================================================
#include "hiss_format.h"

#include <cstdio>      // fprintf, std::snprintf
#include <cstring>     // std::memcpy
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>     // std::ostringstream
#include <cmath>       // std::lround

namespace hiss {

// ============================================================================
// 内部辅助: JSON 字符串转义
//   处理 " \ \n \r \t 及控制字符 (写入 JSON 字符串值时使用)
// ============================================================================

static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// ============================================================================
// 1. Tile 自适应层级计算 (02_FROZEN §11)
//    d = min(9, log2(NSIDE/16))
//    tile_nside = NSIDE / 2^d
//    保证: 满 Tile 最多 4^9=262144 叶像素, tile_nside >= 16
// ============================================================================

HISS_EXPORT uint32_t compute_tile_depth(uint32_t nside) {
    if (nside < 16) {
        // NSIDE < 16 时 d=0, tile_nside = nside (整个球面一个 Tile 组)
        return 0;
    }
    // nside 是 2 的幂, 用位运算求 log2(nside)
    int log2_nside = 0;
    uint32_t v = nside;
    while (v > 1) { v >>= 1; log2_nside++; }
    // log2(nside/16) = log2(nside) - 4
    int d = log2_nside - 4;
    if (d < 0) d = 0;
    if (d > 9) d = 9;  // 上限 9
    return (uint32_t)d;
}

HISS_EXPORT uint32_t compute_tile_nside(uint32_t nside) {
    uint32_t d = compute_tile_depth(nside);
    return nside >> d;  // nside / 2^d
}

// ============================================================================
// 2. DrizzleTileAccumulator 方法 (02_FROZEN §8/§10)
//    float64 内部累加, 最终输出 float32 signal + uint8 support
//
//    语义修正 (依据 00_COMMON_CONTRACTS §2.2, spec.md 步骤2/7):
//      signal[p]  = float(sumFlux)                       — 累计通量 (不除面积)
//      support[p] = uint8(round(255 * clamp(S, 0, 1)))   — 面积比
//      其中 S = sum_area / A_p, A_p = pixel_area (成员变量, 由调用方设置)
// ============================================================================

// finalize_signal: 直接保存累计通量 (不除面积), 无贡献像素 sum_flux=0 自然写 0
//   旧错误: signal = sum_flux / sum_area (平均面亮度)
//   新正确: signal = sum_flux (累计通量, 02_FROZEN §8)
void DrizzleTileAccumulator::finalize_signal(std::vector<float>& signal) const {
    signal.resize(pixels.size());
    for (size_t i = 0; i < pixels.size(); i++) {
        // 直接保存累计通量 (不除面积)
        // 无贡献像素 sum_flux=0, 自然写 0; 不需要单独判断 sum_area
        signal[i] = (float)(pixels[i].sum_flux);
    }
}

// finalize_signal_f64: FP64 模式, 直接输出 float64 累计通量 (无精度损失)
//   R10: 与 finalize_signal 语义一致, 但保留 double 精度
void DrizzleTileAccumulator::finalize_signal_f64(std::vector<double>& signal) const {
    signal.resize(pixels.size());
    for (size_t i = 0; i < pixels.size(); i++) {
        signal[i] = pixels[i].sum_flux;
    }
}

// finalize_support: S = sum_area / pixel_area, 钳制 [0,1], uint8 = round(255*S)
//   旧错误: S = sum_area (未归一化, 假设 sum_area 已经在 [0,1])
//   新正确: S = sum_area / A_p (A_p = pixel_area, 目标 HEALPix 像素面积, 球面度)
//
//   pixel_area 默认 1.0 (向后兼容); 调用方应设置为 hp.pixel_area()
void DrizzleTileAccumulator::finalize_support(std::vector<uint8_t>& support) const {
    support.resize(pixels.size());
    // A_p 必须为正, 否则视为 1.0 (避免除零)
    const double A_p = (pixel_area > 0.0) ? pixel_area : 1.0;
    for (size_t i = 0; i < pixels.size(); i++) {
        // 归一化面积比 S = sum_area / A_p
        double S = pixels[i].sum_area / A_p;
        // 仅浮点误差级超限可钳制 (02_FROZEN §10)
        if (S < 0.0) S = 0.0;
        if (S > 1.0) S = 1.0;
        long v = std::lround(255.0 * S);
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
        support[i] = (uint8_t)v;
    }
}

// validate_support: 检查归一化后 S = sum_area / A_p 在 [0,1] 范围内 (允许浮点误差)
//   0=OK, <0=错误 (明显超 1 是几何/WCS/实现错误)
//   -1=S 超限, -2=pixel_area<=0 (02_FROZEN §10: 目标像素面积非法时必须报错, 不能回退到虚构值)
//
//   旧错误: 直接检查 sum_area 在 [0,1]
//   新正确: 检查 sum_area / pixel_area 在 [0,1]
int DrizzleTileAccumulator::validate_support() const {
    const double eps = 1e-4;  // R10: 放宽容差, 浮点累计误差可致 S 略超 1.0
    // 02_FROZEN §10: pixel_area<=0 必须硬失败, 不能回退到虚构值
    if (pixel_area <= 0.0) {
        fprintf(stderr,
                "[hiss][common] validate_support: pixel_area=%g 非法 (必须 > 0)\n",
                pixel_area);
        return -2;
    }
    const double A_p = pixel_area;
    for (size_t i = 0; i < pixels.size(); i++) {
        double S = pixels[i].sum_area / A_p;
        if (S < -eps || S > 1.0 + eps) {
            fprintf(stderr,
                    "[hiss][common] validate_support: 像素 %zu sum_area=%g, A_p=%g, "
                    "S=%g 超限 (有效范围 [0,1])\n",
                    i, pixels[i].sum_area, A_p, S);
            return -1;
        }
    }
    return 0;
}

// ============================================================================
// 3. HissMetadata JSON 序列化/反序列化 (02_FROZEN §16)
//    手写 JSON (无外部依赖), 字段固定
// ============================================================================

std::string HissMetadata::to_json() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"nside\":"      << nside      << ",";
    ss << "\"tile_nside\":" << tile_nside << ",";
    ss << "\"ordering\":"   << ordering   << ",";
    ss << "\"radesys\":"    << radesys    << ",";
    ss << "\"pixfrac\":"    << pixfrac    << ",";
    ss << "\"photscal\":"   << photscal   << ",";
    ss << "\"photappl\":"   << photappl   << ",";
    ss << "\"bunit\":\""    << json_escape(bunit)    << "\",";
    ss << "\"calmode\":\""  << json_escape(calmode)  << "\",";
    ss << "\"darkreq\":\""  << json_escape(darkreq)  << "\",";
    ss << "\"darkmode\":\"" << json_escape(darkmode) << "\",";
    ss << "\"darkscl\":"    << darkscl    << ",";
    ss << "\"object\":\""   << json_escape(object)   << "\",";
    ss << "\"date_obs\":\"" << json_escape(date_obs) << "\",";
    ss << "\"exptime\":"    << exptime    << ",";
    ss << "\"filter\":\""   << json_escape(filter)   << "\",";
    ss << "\"telescop\":\"" << json_escape(telescop) << "\",";
    ss << "\"instrume\":\"" << json_escape(instrume) << "\",";
    ss << "\"gain\":"       << gain       << ",";
    ss << "\"history\":\""  << json_escape(history)  << "\",";
    ss << "\"precision_mode\":" << (unsigned)precision_mode << ",";
    ss << "\"signal_dtype\":"   << (unsigned)signal_dtype;
    ss << "}";
    return ss.str();
}

// from_json: 简易解析 (按 "key":value 查找), 容忍字段缺失/顺序变化
// 仅解析 to_json 生成的格式, 不追求通用 JSON 兼容
int HissMetadata::from_json(const std::string& json) {
    // 数字字段提取 (double 兼容整数)
    auto get_num = [&](const std::string& key, double& out) -> bool {
        std::string needle = "\"" + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos += needle.size();
        try {
            size_t used = 0;
            out = std::stod(json.substr(pos), &used);
            return true;
        } catch (...) {
            return false;
        }
    };
    // 字符串字段提取 (处理基本转义), 输出到 std::string
    auto get_str = [&](const std::string& key, std::string& out) -> bool {
        std::string needle = "\"" + key + "\":\"";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos += needle.size();
        size_t end = pos;
        while (end < json.size()) {
            if (json[end] == '\\') { end += 2; continue; }
            if (json[end] == '"')  break;
            end++;
        }
        if (end >= json.size()) return false;
        std::string raw = json.substr(pos, end - pos);
        // 反转义
        out.clear();
        for (size_t i = 0; i < raw.size(); i++) {
            if (raw[i] == '\\' && i + 1 < raw.size()) {
                char next = raw[i + 1];
                switch (next) {
                    case '"':  out += '"';  i++; break;
                    case '\\': out += '\\'; i++; break;
                    case 'n':  out += '\n'; i++; break;
                    case 'r':  out += '\r'; i++; break;
                    case 't':  out += '\t'; i++; break;
                    default:   out += raw[i];   break;
                }
            } else {
                out += raw[i];
            }
        }
        return true;
    };
    // 拷贝到固定大小 char[] (保证终止符)
    auto to_buf = [&](const std::string& s, char* buf, size_t buf_size) {
        if (buf_size == 0) return;
        size_t n = (s.size() < buf_size - 1) ? s.size() : (buf_size - 1);
        std::memcpy(buf, s.data(), n);
        buf[n] = '\0';
    };

    double v = 0.0;
    std::string s;
    if (get_num("nside", v))      nside      = (uint32_t)v;
    if (get_num("tile_nside", v)) tile_nside = (uint32_t)v;
    if (get_num("ordering", v))   ordering   = (int)v;
    if (get_num("radesys", v))    radesys    = (int)v;
    if (get_num("pixfrac", v))    pixfrac    = v;
    if (get_num("photscal", v))   photscal   = v;
    if (get_num("photappl", v))   photappl   = (int)v;
    if (get_num("darkscl", v))    darkscl    = v;
    if (get_num("exptime", v))    exptime    = v;
    if (get_num("gain", v))       gain       = v;
    if (get_str("bunit", s))    to_buf(s, bunit, sizeof(bunit));
    if (get_str("calmode", s))  to_buf(s, calmode, sizeof(calmode));
    if (get_str("darkreq", s))  to_buf(s, darkreq, sizeof(darkreq));
    if (get_str("darkmode", s)) to_buf(s, darkmode, sizeof(darkmode));
    if (get_str("object", s))   to_buf(s, object, sizeof(object));
    if (get_str("date_obs", s)) to_buf(s, date_obs, sizeof(date_obs));
    if (get_str("filter", s))   to_buf(s, filter, sizeof(filter));
    if (get_str("telescop", s)) to_buf(s, telescop, sizeof(telescop));
    if (get_str("instrume", s)) to_buf(s, instrume, sizeof(instrume));
    if (get_str("history", s))  history = s;
    // R10: precision_mode/signal_dtype (缺失时默认 0=FP32, 向后兼容)
    if (get_num("precision_mode", v)) precision_mode = (uint8_t)v;
    if (get_num("signal_dtype", v))   signal_dtype   = (uint8_t)v;
    return 0;
}

} // namespace hiss
