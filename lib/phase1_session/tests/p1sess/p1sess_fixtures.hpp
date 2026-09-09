// P1-SESSION-TEST · 固定 seed fixture generator (FIX-SESS-A..F)
//
// 控制包任务: P1-SESSION-TEST (SA-P1SS-T, queue 47, lock-P1-SESSION)。
// fixture 清单 (锚 lib/phase1_session/README.md §4 文件输入合同):
//   FIX-SESS-A  最小合成 frame 家族 (常数场 8x8, light + bias/dark/flat
//               masters, 解析可控) — units 节点计数/失败传播主 fixture
//   FIX-SESS-B  多帧 light (3 帧, 8x8, 不同常数) — stages.files/frames 计数
//   FIX-SESS-C  坏 frame (截断 FITS: 头声明 NAXIS2=8 数据缺失) — io_read
//               失败注入 (缺模块/坏 artifact 传播)
//   FIX-SESS-D  非 FITS 伪文件 (文本垃圾) — aio_read 返回 nullptr 失败注入
//   FIX-SESS-E  尺寸不匹配 master (16x8) — calibrate 尺寸守卫 PARAM 注入
//   FIX-SESS-F  NaN 像素帧 (bitwise NaN 写出) — NaN 传播不吞 (invalid 面)
// 生成器规则 (MODULE_MIGRATION_TEMPLATE §<prefix>-TEST): fixture 由固定
// seed+参数生成, 不提交大二进制, 不内嵌生产算法 —— 本文件只做
// splitmix64 PRNG + 手写最小 FITS (BITPIX=-32 大端, 头/数据块填充),
// 不调用任何生产 symbol。
#ifndef P1SESS_FIXTURES_HPP
#define P1SESS_FIXTURES_HPP

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace p1sess {

// ---------------------------------------------------------------------------
// splitmix64 固定 seed PRNG (fixture 可复现; 谱系对齐 p1drz/p1hips fixtures)
// ---------------------------------------------------------------------------
struct SplitMix64 {
    std::uint64_t state;
    explicit SplitMix64(std::uint64_t seed) : state(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    // [0,1) 双精度
    double unit() {
        return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
    }
};

// ---------------------------------------------------------------------------
// 最小 FITS writer (BITPIX=-32 float32, 行优先, FITS 大端字节序)
// 与 lib/phase1_session/tests/test_p1_session_manifest.cpp 手写 fixture 同
// 谱系, 但支持任意 WxH + 任意像素值生成器 + 可控截断 (FIX-SESS-C)。
// ---------------------------------------------------------------------------

// 单条 80 字节 FITS 卡 (key= value 形式)
inline void sess_fits_card(std::FILE* fp, const char* key, const char* value) {
    char card[80];
    std::memset(card, ' ', 80);
    const std::size_t klen = std::strlen(key);
    std::memcpy(card, key, klen < 8 ? klen : 8);
    card[8] = '=';
    card[9] = ' ';
    char v[64];
    std::snprintf(v, sizeof(v), "%s", value);
    const std::size_t vlen = std::strlen(v) < 68 ? std::strlen(v) : 68;
    std::memcpy(card + 10, v, vlen);
    std::fwrite(card, 1, 80, fp);
}

inline void sess_fits_pad(std::FILE* fp, long bytes_written, char pad) {
    const int pad_n = static_cast<int>((2880 - (bytes_written % 2880)) % 2880);
    for (int i = 0; i < pad_n; ++i) std::fputc(pad, fp);
}

// float → FITS 大端 4 字节
inline void sess_fits_be32(float v, unsigned char out[4]) {
    std::uint32_t u = 0;
    std::memcpy(&u, &v, sizeof(u));
    out[0] = static_cast<unsigned char>(u >> 24);
    out[1] = static_cast<unsigned char>(u >> 16);
    out[2] = static_cast<unsigned char>(u >> 8);
    out[3] = static_cast<unsigned char>(u);
}

// 可移植截断: 把 path 裁到 size 字节 (FIX-SESS-C 坏帧注入; 不依赖 POSIX
// truncate, 纯 stdio — Windows CI 语义等价)
inline int sess_truncate_file(const std::string& path, long size) {
    std::FILE* src = std::fopen(path.c_str(), "rb");
    if (!src) return 1;
    std::fseek(src, 0, SEEK_END);
    const long cur = std::ftell(src);
    std::fclose(src);
    if (cur <= size) return 0;  // 已足够短
    src = std::fopen(path.c_str(), "rb");
    if (!src) return 1;
    std::vector<char> buf(static_cast<std::size_t>(size));
    const std::size_t got = std::fread(buf.data(), 1, buf.size(), src);
    std::fclose(src);
    if (got != buf.size()) return 1;
    std::FILE* dst = std::fopen(path.c_str(), "wb");
    if (!dst) return 1;
    const std::size_t put = std::fwrite(buf.data(), 1, buf.size(), dst);
    std::fclose(dst);
    return put == buf.size() ? 0 : 1;
}

// 写完整 FITS: 头 6 卡 (SIMPLE/BITPIX/NAXIS/NAXIS1/NAXIS2/END) + W*H 大端
// float 数据 + 块填充。pixel(i) 由调用方给 (i = 行优先像素序)。
// truncate_tail > 0: 数据块尾部截去 N 字节 (FIX-SESS-C 坏帧注入)。
// 返回 0 成功 / 非 0 IO 失败。
inline int write_fits_file(const std::string& path, int w, int h,
                           float (*pixel)(int, void*), void* user,
                           long truncate_tail = 0) {
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return 1;
    sess_fits_card(fp, "SIMPLE", "T");
    sess_fits_card(fp, "BITPIX", "-32");
    sess_fits_card(fp, "NAXIS", "2");
    char naxis[32];
    std::snprintf(naxis, sizeof(naxis), "%d", w);
    sess_fits_card(fp, "NAXIS1", naxis);
    std::snprintf(naxis, sizeof(naxis), "%d", h);
    sess_fits_card(fp, "NAXIS2", naxis);
    sess_fits_card(fp, "END", "");
    long pos = 80L * 6;
    sess_fits_pad(fp, pos, ' ');
    for (int i = 0; i < w * h; ++i) {
        unsigned char be[4];
        sess_fits_be32(pixel(i, user), be);
        if (std::fwrite(be, 1, 4, fp) != 4) { std::fclose(fp); return 2; }
    }
    pos += static_cast<long>(w) * h * 4;
    sess_fits_pad(fp, pos, '\0');
    std::fclose(fp);
    if (truncate_tail > 0 && sess_truncate_file(path, pos - truncate_tail) != 0)
        return 3;
    return 0;
}

// ---------------------------------------------------------------------------
// FIX-SESS-A: 常数场 8x8 (值由 seed 派生, 帧内恒定 → 解析期望 = 常数)
// ---------------------------------------------------------------------------
struct ConstField {
    float value;
    ConstField(std::uint64_t seed, float base) {
        SplitMix64 rng(seed);
        // 恒定场: 值=base + 同帧内恒定的小偏置 (bitwise 可复现)
        value = base + static_cast<float>(rng.unit()) * 0.25f;
    }
};
inline float const_field_pixel(int, void* user) {
    return static_cast<ConstField*>(user)->value;
}

// ---------------------------------------------------------------------------
// FIX-SESS-B: 多帧不同常数 (帧 j 的常数由 seed_j 派生)
// ---------------------------------------------------------------------------
inline float frame_const_pixel(int, void* user) {
    return *static_cast<float*>(user);
}

// ---------------------------------------------------------------------------
// FIX-SESS-F: NaN 像素帧 (像素 i==5 为 quiet NaN, 其余常数)
// ---------------------------------------------------------------------------
struct NanField {
    float value;
    explicit NanField(float v) : value(v) {}
};
inline float nan_field_pixel(int i, void* user) {
    NanField* f = static_cast<NanField*>(user);
    return i == 5 ? std::nanf("") : f->value;
}

// ---------------------------------------------------------------------------
// FIX-SESS-D: 非 FITS 伪文件 (文本垃圾, aio_read 必拒)
// ---------------------------------------------------------------------------
inline int write_garbage_file(const std::string& path) {
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return 1;
    for (int i = 0; i < 512; ++i) std::fputc('A' + (i % 26), fp);
    std::fclose(fp);
    return 0;
}

}  // namespace p1sess

#endif  // P1SESS_FIXTURES_HPP
