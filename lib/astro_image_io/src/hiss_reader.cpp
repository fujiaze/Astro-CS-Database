// ============================================================================
// hiss_reader.cpp - AstroCS HISS Reader (XISF 式 Header + attachments 格式)
//
// 内容:
//   1. 小端序二进制读写工具
//   2. log2i 辅助 (用于 query_pixel 坐标转换)
//   3. HEALPix NESTED 坐标转换 (ra/dec → ipix, 不依赖外部 HealpixCore)
//   4. HissReader 类实现 (按目录读取, 按需加载 Tile, 不依赖 WCS)
//
// 二进制布局 (与 Writer 对应):
//   [固定签名块 20B: MAGIC(8) + version(4) + header_offset(8)]
//   [Header: 网格规格(24B) + 元数据JSON + Tile目录]
//   [Attachment子块1] [Attachment子块2] ...
//
// 设计说明:
//   - 按目录读取, 不依赖子块物理顺序 (02_FROZEN §14)
//   - 支持只读 occupancy/signal/support/SNR
//   - 未知可选块跳过; 未知必需块报不兼容
//   - 检查 offset/size 越界, 解压长度, checksum
//   - 用 NSIDE/NESTED/ICRS 定位, 不依赖 WCS
//   - 不加载整文件 (按需读取 Tile)
//
// 注: 下列共享方法已移至 hiss_common.cpp (避免与 Writer 重复定义):
//   - HissMetadata::to_json / from_json (§16)
//   - compute_tile_depth / compute_tile_nside (§11)
//
// 注: CRC32-C (Castagnoli) 校验实现已移至 hiss_codec.cpp 共享,
//     Reader/Writer 通过 ChecksumRegistry::find() 获取同一实现。
// ============================================================================
#include "hiss_format.h"
#include "aio_util.h"  // aio_fopen_utf8 (UTF-8 路径支持)
#include "hiss_transform.h"  // WP-G 步骤12: inverse_transform (解压后逆向变换)

// Windows 头文件 (windows.h, 经 aio_util.h 引入) 中 OPTIONAL 被定义为空宏,
// 与 hiss::SubblockFlags::OPTIONAL 冲突。取消定义以恢复枚举值可用。
// REQUIRED 在 Windows 头中通常未定义, 但为安全起见一并取消。
#ifdef OPTIONAL
#undef OPTIONAL
#endif
#ifdef REQUIRED
#undef REQUIRED
#endif

#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <utility>

namespace hiss {

// ============================================================================
// 内部常量
// ============================================================================

// R04-B14: 固定签名块 (16 字节, 显式小端序)
//   magic[8] = "HISS0100" + header_length(uint32 LE) + feature_flags(uint32 LE)
static const char     kMagic[8] = { 'H','I','S','S','0','1','0','0' };

// Header 子结构大小
static const size_t kGridSpecSize   = 24;  // nside(4)+tile_nside(4)+ordering(4)+radesys(4)+pixfrac(8)
// R04-B17: 子块描述符 42 字节 (含 ext_type_id)
// type(1)+ext_type_id(2)+flags(2)+offset(8)+comp(8)+uncomp(8)+codec(2)+transform(2)+checksum_type(1)+checksum(8)
static const size_t kSubblockDescSize = HISS_SUBBLOCK_DESC_DISK_SIZE;
static const size_t kTileHeaderSize  = 15;  // parent_ipix(8)+tile_nside(4)+occ_mode(1)+n_subblocks(2)

// TLV 项头大小: tag(2) + flags(1) + length(4) = 7 字节
static const size_t kTlvHeaderSize = 7;

// schema 指纹 (必须与 Writer 一致)
static const uint8_t kSchemaFingerprint[32] = {
    0xA1, 0x00, 0x72, 0x04, 0xB1, 0x00, 0x61, 0x04,
    0x48, 0x49, 0x53, 0x53, 0x2D, 0x76, 0x31, 0x2E,
    0x30, 0x2D, 0x73, 0x63, 0x68, 0x65, 0x6D, 0x61,
    0x2D, 0x30, 0x30, 0x30, 0x31, 0x2D, 0x52, 0x30
};

// ============================================================================
// 1. 小端序二进制读取工具
// ============================================================================

static inline uint16_t read_u16_le(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t read_u64_le(const uint8_t* p) {
    return (uint64_t)read_u32_le(p) | ((uint64_t)read_u32_le(p + 4) << 32);
}

static inline int32_t read_i32_le(const uint8_t* p) {
    return (int32_t)read_u32_le(p);
}

static inline float read_f32_le(const uint8_t* p) {
    uint32_t u = read_u32_le(p);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

static inline double read_f64_le(const uint8_t* p) {
    uint64_t u = read_u64_le(p);
    double d;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}

// ============================================================================
// 2. log2i 辅助 (用于 query_pixel 坐标转换, 计算 NSIDE/tile_nside 的位移)
//    注: HissMetadata::to_json/from_json 与 compute_tile_depth/nside 已移至
//    hiss_common.cpp (与 Writer 共用, 避免重复定义)
//    注: CRC32-C 校验实现已移至 hiss_codec.cpp (通过 ChecksumRegistry 共享)
// ============================================================================

// log2 (n 为 2 的幂)
static int log2i(uint32_t n) {
    int b = 0;
    while (n > 1) { n >>= 1; b++; }
    return b;
}

// ============================================================================
// 4. HEALPix NESTED 坐标转换 (ra/dec → ipix)
//    内部实现, 不依赖外部 HealpixCore 库
//    算法改编自 healpix_core.cpp (ang2xy + xy2nest)
//    仅实现 NESTED 方案 (HISS 统一 NESTED + ICRS)
// ============================================================================

static const double kPi       = 3.14159265358979323846;
static const double kTwoPi    = 2.0 * kPi;
static const double kHalfPi   = kPi / 2.0;
static const double kTwoThirds = 2.0 / 3.0;

static inline double hpx_sq(double d) { return d * d; }

static inline int hpx_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// theta/phi → (bighp, x, y) [HEALPix 面内坐标]
static void hpx_ang2xy(double theta, double phi, int nside,
                       int* bighp, int* x, int* y) {
    double z = std::cos(theta);
    int Ns = nside;

    // phi 归一化到 [0, 2π)
    phi = phi - kTwoPi * std::floor(phi / kTwoPi);
    if (phi < 0.0) phi += kTwoPi;
    if (phi >= kTwoPi) phi -= kTwoPi;

    double phi_t = std::fmod(phi, kHalfPi);  // [0, π/2)

    if (z >= kTwoThirds || z <= -kTwoThirds) {
        // 极冠区
        bool north = (z >= kTwoThirds);
        double zfactor = north ? 1.0 : -1.0;

        double root1 = (1.0 - z * zfactor) * 3.0 *
                       hpx_sq((double)Ns * (2.0 * phi_t - kPi) / kPi);
        double kx = (root1 <= 0.0) ? 0.0 : std::sqrt(root1);
        double root2 = (1.0 - z * zfactor) * 3.0 *
                       hpx_sq((double)Ns * 2.0 * phi_t / kPi);
        double ky = (root2 <= 0.0) ? 0.0 : std::sqrt(root2);

        double xx, yy;
        if (north) { xx = Ns - kx; yy = Ns - ky; }
        else       { xx = ky;      yy = kx; }

        *x = hpx_clampi((int)std::floor(xx), 0, Ns - 1);
        *y = hpx_clampi((int)std::floor(yy), 0, Ns - 1);

        double sector = (phi - phi_t) / kHalfPi;
        int offset = (int)std::round(sector);
        offset = ((offset % 4) + 4) % 4;
        *bighp = north ? offset : (8 + offset);
    } else {
        // 赤道带
        double zunits  = (z + kTwoThirds) / (4.0 / 3.0);   // [0,1]
        double phiunits = phi_t / kHalfPi;                  // [0,1]
        double u1 = zunits + phiunits;
        double u2 = zunits - phiunits + 1.0;
        double xx = u1 * Ns;
        double yy = u2 * Ns;

        double sector = (phi - phi_t) / kHalfPi;
        int offset = (int)std::round(sector);
        offset = ((offset % 4) + 4) % 4;

        if (xx >= Ns) {
            xx -= Ns;
            if (yy >= Ns) {
                yy -= Ns;
                *bighp = offset;                 // 北极
            } else {
                *bighp = ((offset + 1) % 4) + 4; // 右赤道
            }
        } else {
            if (yy >= Ns) {
                yy -= Ns;
                *bighp = offset + 4;             // 左赤道
            } else {
                *bighp = 8 + offset;             // 南极
            }
        }
        *x = hpx_clampi((int)std::floor(xx), 0, Ns - 1);
        *y = hpx_clampi((int)std::floor(yy), 0, Ns - 1);
    }
}

// (bighp, x, y) → NESTED ipix (Morton/z-order 位交织)
static uint64_t hpx_xy2nest(int bighp, int x, int y, int nside) {
    uint64_t index = 0;
    int xb = x, yb = y;
    for (int i = 0; i < 32; i++) {
        index |= ((uint64_t)(((yb & 1) << 1) | (xb & 1))) << (i * 2);
        xb >>= 1;
        yb >>= 1;
        if (!xb && !yb) break;
    }
    return index + (uint64_t)bighp * (uint64_t)nside * (uint64_t)nside;
}

// ra/dec (度, ICRS) → NESTED ipix
static uint64_t radec_to_nested_ipix(double ra_deg, double dec_deg, int nside) {
    double theta = kHalfPi - dec_deg * kPi / 180.0;  // 极角 (0=北极)
    double phi   = ra_deg * kPi / 180.0;              // 方位角
    int bighp, x, y;
    hpx_ang2xy(theta, phi, nside, &bighp, &x, &y);
    return hpx_xy2nest(bighp, x, y, nside);
}

// ============================================================================
// 4.1 子块类型校验 (依据 02_FROZEN §13)
//     已知类型: OCCUPANCY(0), SIGNAL(1), SUPPORT(2), SNR(3), EXTENSION(255)
//     未知类型: 上述以外的任何值
//     未知必需子块 → 拒绝 (HISS_ERR_UNKNOWN_REQUIRED)
//     未知可选子块 → 跳过 (继续处理)
// ============================================================================

static inline bool is_known_subblock_type(SubblockType type) {
    return type == SubblockType::OCCUPANCY ||
           type == SubblockType::SIGNAL    ||
           type == SubblockType::SUPPORT   ||
           type == SubblockType::SNR       ||
           type == SubblockType::EXTENSION;
}

// ============================================================================
// 5. HissReader::Impl 实现
// ============================================================================

struct HissReader::Impl {
    FILE*      fp = nullptr;            // 文件句柄 (二进制只读)
    uint64_t   filesize = 0;            // 文件总大小
    uint32_t   header_length = 0;       // Header 区字节数 (R04-B14)
    uint32_t   feature_flags = 0;       // 特性标志位 (R04-B14)
    HissGridSpec  grid;                 // 网格规格
    HissMetadata  metadata;             // 元数据
    std::vector<HissTile> tiles;        // Tile 目录
    std::unordered_map<uint64_t, size_t> tile_index;  // parent_ipix → tiles[] 索引

    // ---- 内部方法 ----

    // 读取子块数据 (解压 + 校验 + 逆向变换)
    // WP-G 步骤12: 解压后若 transform_id != NONE, 执行 inverse_transform 还原原始数据
    // element_size - 元素大小 (供 transform 使用, 如 signal=4, support=1)
    // 返回 0=成功, <0=失败 (错误码见任务规范)
    int read_subblock(const HissSubblockDescriptor& desc,
                      std::vector<uint8_t>& out,
                      size_t element_size = 1) const {
        // a. R04-B16: 严格检查 offset + compressed_size 越界
        if (desc.offset >= filesize) {
            fprintf(stderr, "[hiss][reader] 子块越界: offset=%llu >= filesize=%llu\n",
                    (unsigned long long)desc.offset, (unsigned long long)filesize);
            return HISS_ERR_FORMAT_VIOLATION;
        }
        if (desc.compressed_size > filesize - desc.offset) {
            fprintf(stderr, "[hiss][reader] 子块越界: offset=%llu size=%llu filesize=%llu\n",
                    (unsigned long long)desc.offset,
                    (unsigned long long)desc.compressed_size,
                    (unsigned long long)filesize);
            return HISS_ERR_FORMAT_VIOLATION;
        }

        // b. seek 到 offset, 读取 compressed_size 字节
        //    使用 64 位 seek 支持 >2GB 文件 (02_FROZEN §14)
        std::vector<uint8_t> comp(desc.compressed_size);
#ifdef _WIN32
        if (_fseeki64(fp, (__int64)desc.offset, SEEK_SET) != 0) {
#else
        if (fseeko(fp, (off_t)desc.offset, SEEK_SET) != 0) {
#endif
            fprintf(stderr, "[hiss][reader] seek 失败: offset=%llu\n",
                    (unsigned long long)desc.offset);
            return -3;
        }
        if (desc.compressed_size > 0) {
            size_t nread = std::fread(comp.data(), 1, desc.compressed_size, fp);
            if (nread != desc.compressed_size) {
                fprintf(stderr, "[hiss][reader] 读取不足: need=%llu got=%zu\n",
                        (unsigned long long)desc.compressed_size, nread);
                return -3;
            }
        }

        // c. checksum 校验 (解压前校验压缩数据)
        //    通过 ChecksumRegistry 查找算法实现 (CRC32C 内置, 其他通过注册接入)
        //    INTERIM_BASELINE_NOT_FROZEN: 候选注册机制, 算法待 DQ-006 冻结
        if (desc.checksum_type != ChecksumType::NONE) {
            const ChecksumEntry* cs = ChecksumRegistry::instance().find(desc.checksum_type);
            if (!cs) {
                // 未注册的 checksum 类型, 报错 (避免静默跳过校验导致数据损坏未检出)
                fprintf(stderr, "[hiss][reader] checksum 未注册: id=%u\n",
                        (unsigned)desc.checksum_type);
                return -6;
            }
            uint64_t calc = cs->compute(comp.data(), comp.size());
            if (calc != desc.checksum) {
                fprintf(stderr, "[hiss][reader] %s 校验失败: calc=%llx stored=%llx\n",
                        cs->name.c_str(),
                        (unsigned long long)calc, (unsigned long long)desc.checksum);
                return -5;
            }
        }

        // d. 查找 CodecRegistry, 解压数据
        const CodecEntry* codec = CodecRegistry::instance().find(desc.codec_id);
        if (!codec) {
            fprintf(stderr, "[hiss][reader] codec 未注册: id=%u\n",
                    (unsigned)desc.codec_id);
            return -6;
        }

        out.resize(desc.uncompressed_size);
        size_t out_size = desc.uncompressed_size;
        int ret = codec->decompress(comp.data(), comp.size(),
                                     out.data(), out_size);
        if (ret != 0) {
            fprintf(stderr, "[hiss][reader] 解压失败: ret=%d codec=%s\n",
                    ret, codec->name.c_str());
            return -4;
        }

        // e. 检查解压长度 == uncompressed_size
        if (out_size != desc.uncompressed_size) {
            fprintf(stderr, "[hiss][reader] 解压长度不匹配: got=%zu expected=%llu\n",
                    out_size, (unsigned long long)desc.uncompressed_size);
            return -4;
        }

        // f. WP-G 步骤12: 逆向变换 (解压后执行)
        //    若 transform_id != NONE, 调用 inverse_transform 还原原始数据
        //    对于大小保持变换 (NONE/BYTE_SHUFFLE/DELTA): expected_output_size = out.size()
        //    对于 DELTA_VARINT: expected_output_size = 0 (从数据前缀 n_elements 自动确定)
        if (desc.transform_id != TransformId::NONE) {
            TransformType tt = transform_id_to_type(desc.transform_id);

            // 确定期望输出大小
            // DELTA_VARINT 的输出大小编码在数据前缀中, 传 0 让 inverse 自动确定
            // 其他变换的输出大小 == 输入大小 (大小保持)
            size_t expected = (tt == TransformType::DELTA_VARINT) ? 0 : out.size();

            std::vector<uint8_t> restored = inverse_transform(
                tt, out.data(), out.size(), element_size, expected);

            // 校验逆向变换结果
            // DELTA_VARINT 的 n_elements=0 时输出为空 (有效), 其他情况空输出=错误
            bool is_error = false;
            if (restored.empty() && !out.empty()) {
                if (tt == TransformType::DELTA_VARINT && out.size() >= 4) {
                    // 检查 n_elements 是否为 0 (有效的空输出)
                    uint32_t n_elem = (uint32_t)out[0] | ((uint32_t)out[1] << 8) |
                                      ((uint32_t)out[2] << 16) | ((uint32_t)out[3] << 24);
                    if (n_elem != 0) {
                        is_error = true;  // n_elements>0 但输出空 = 错误
                    }
                    // n_elements=0: 空输出是有效的, 不算错误
                } else {
                    is_error = true;  // 其他变换: 非空输入产生空输出 = 错误
                }
            }

            if (is_error) {
                fprintf(stderr,
                        "[hiss][reader] inverse_transform 失败: type=%s(%u) input_size=%zu "
                        "element_size=%zu\n",
                        transform_type_name(tt), (unsigned)desc.transform_id,
                        out.size(), element_size);
                return -6;
            }

            out = std::move(restored);

            fprintf(stderr,
                    "[hiss][reader]   inverse_transform %s: %zu → %zu 字节 (element_size=%zu)\n",
                    transform_type_name(tt), desc.uncompressed_size, out.size(), element_size);
        }

        return 0;
    }

    // 在 tile_index 中查找 Tile, 返回 tiles[] 索引
    int find_tile(uint64_t parent_ipix, size_t* idx) const {
        auto it = tile_index.find(parent_ipix);
        if (it == tile_index.end()) {
            fprintf(stderr, "[hiss][reader] Tile 未找到: parent_ipix=%llu\n",
                    (unsigned long long)parent_ipix);
            return -1;
        }
        *idx = it->second;
        return 0;
    }

    // 在 Tile 的子块中查找指定类型的子块
    static const HissSubblockDescriptor* find_subblock(const HissTile& tile,
                                                        SubblockType type) {
        for (const auto& sb : tile.subblocks) {
            if (sb.type == type) return &sb;
        }
        return nullptr;
    }
};

// ============================================================================
// 6. HissReader 公共接口实现
// ============================================================================

HissReader::HissReader() : pimpl_(std::make_unique<Impl>()) {}

HissReader::~HissReader() { close(); }

// ----------------------------------------------------------------------------
// open(): 打开 HISS 文件, 读取签名块 + Header + Tile 目录
// ----------------------------------------------------------------------------
int HissReader::open(const std::string& path) {
    Impl& impl = *pimpl_;

    // 确保之前的文件已关闭
    if (impl.fp) close();

    // 打开文件 (二进制只读, 支持 UTF-8 路径)
    impl.fp = aio_fopen_utf8(path.c_str(), "rb");
    if (!impl.fp) {
        fprintf(stderr, "[hiss][reader] 无法打开文件: %s\n", path.c_str());
        return -1;
    }

    // 获取文件大小 (使用 64 位版本支持 >2GB 文件, 02_FROZEN §14)
#ifdef _WIN32
    _fseeki64(impl.fp, 0, SEEK_END);
    __int64 fsize = _ftelli64(impl.fp);
#else
    fseeko(impl.fp, 0, SEEK_END);
    off_t fsize = ftello(impl.fp);
#endif
    if (fsize < 0) {
        fprintf(stderr, "[hiss][reader] 无法获取文件大小\n");
        close();
        return -1;
    }
    impl.filesize = (uint64_t)fsize;

    // ---- 1. R04-B14: 读取固定签名块 (16 字节, 显式小端序) ----
    //   magic[8] = "HISS0100" + header_length(uint32 LE) + feature_flags(uint32 LE)
    if (impl.filesize < HISS_SIGNATURE_SIZE) {
        fprintf(stderr, "[hiss][reader] 文件过短: %llu < %d\n",
                (unsigned long long)impl.filesize, (int)HISS_SIGNATURE_SIZE);
        close();
        return HISS_ERR_FORMAT;
    }

    uint8_t sig[HISS_SIGNATURE_SIZE];
    std::fseek(impl.fp, 0, SEEK_SET);
    if (std::fread(sig, 1, HISS_SIGNATURE_SIZE, impl.fp) != HISS_SIGNATURE_SIZE) {
        fprintf(stderr, "[hiss][reader] 读取签名块失败\n");
        close();
        return HISS_ERR_FORMAT;
    }

    // 验证 MAGIC (8 字节, 必须为 "HISS0100")
    if (std::memcmp(sig, kMagic, 8) != 0) {
        fprintf(stderr, "[hiss][reader] MAGIC 不匹配 (期望 HISS0100)\n");
        close();
        return HISS_ERR_FORMAT;  // MAGIC 不匹配 → 格式错误
    }

    // R04-B14: 读取 header_length (uint32 LE, 严格限制 Header 解析范围)
    impl.header_length = read_u32_le(sig + 8);
    if (impl.header_length == 0 || impl.header_length > 256 * 1024 * 1024) {
        // Header 长度必须 > 0, 上限 256MB 防止异常值
        fprintf(stderr, "[hiss][reader] header_length 非法: %u\n", impl.header_length);
        close();
        return HISS_ERR_FORMAT;
    }
    // R04-B14: 严格边界检查 — header_length + 签名块大小不得超过文件大小
    if ((uint64_t)impl.header_length + HISS_SIGNATURE_SIZE > impl.filesize) {
        fprintf(stderr, "[hiss][reader] header_length 越界: %u + %d > %llu\n",
                impl.header_length, (int)HISS_SIGNATURE_SIZE, (unsigned long long)impl.filesize);
        close();
        return HISS_ERR_FORMAT;
    }

    // R04-B14: 读取 feature_flags (uint32 LE)
    impl.feature_flags = read_u32_le(sig + 12);
    if ((impl.feature_flags & HISS_FEAT_TLV_HEADER) == 0) {
        // 必须支持 TLV Header 特性
        fprintf(stderr, "[hiss][reader] feature_flags 缺少 HISS_FEAT_TLV_HEADER: 0x%08x\n",
                impl.feature_flags);
        close();
        return HISS_ERR_UNSUPPORTED;
    }

    // ---- 2. R04-B15: 读取 TLV Header (header_length 字节) ----
    //   Header 是一系列 TLV (tag:u16 + flags:u8 + length:u32 + value)
    //   未知 required TLV → 拒绝; 未知 optional TLV → 跳过
    std::vector<uint8_t> hdr_buf(impl.header_length);
#ifdef _WIN32
    _fseeki64(impl.fp, (__int64)HISS_SIGNATURE_SIZE, SEEK_SET);
#else
    fseeko(impl.fp, (off_t)HISS_SIGNATURE_SIZE, SEEK_SET);
#endif
    if (std::fread(hdr_buf.data(), 1, impl.header_length, impl.fp) != impl.header_length) {
        fprintf(stderr, "[hiss][reader] 读取 Header 失败\n");
        close();
        return HISS_ERR_FORMAT;
    }

    // 解析 TLV 项
    bool has_grid = false, has_tiles = false, has_schema = false;
    size_t hdr_pos = 0;
    while (hdr_pos + kTlvHeaderSize <= impl.header_length) {
        uint16_t tag   = read_u16_le(hdr_buf.data() + hdr_pos);
        uint8_t  tflags = hdr_buf[hdr_pos + 2];
        uint32_t tlen   = read_u32_le(hdr_buf.data() + hdr_pos + 3);
        hdr_pos += kTlvHeaderSize;

        // R04-B16: 严格检查 TLV length 越界
        if (hdr_pos + tlen > impl.header_length) {
            fprintf(stderr, "[hiss][reader] TLV tag=0x%04x length=%u 越界 (pos=%zu header=%u)\n",
                    tag, tlen, hdr_pos, impl.header_length);
            close();
            return HISS_ERR_FORMAT_VIOLATION;
        }

        const uint8_t* value = hdr_buf.data() + hdr_pos;

        switch (tag) {
            case HISS_TLV_SCHEMA_FINGERPRINT: {
                // R04-B15: schema 指纹验证
                if (tlen != 32) {
                    fprintf(stderr, "[hiss][reader] SCHEMA_FINGERPRINT 长度非法: %u (期望 32)\n", tlen);
                    close();
                    return HISS_ERR_FORMAT;
                }
                if (std::memcmp(value, kSchemaFingerprint, 32) != 0) {
                    fprintf(stderr, "[hiss][reader] schema 指纹不匹配\n");
                    close();
                    return HISS_ERR_UNSUPPORTED;
                }
                has_schema = true;
                break;
            }
            case HISS_TLV_GRID_SPEC: {
                // R04-B16: 严格校验网格规格
                if (tlen != kGridSpecSize) {
                    fprintf(stderr, "[hiss][reader] GRID_SPEC 长度非法: %u (期望 %zu)\n",
                            tlen, kGridSpecSize);
                    close();
                    return HISS_ERR_FORMAT;
                }
                impl.grid.nside      = read_u32_le(value + 0);
                impl.grid.tile_nside = read_u32_le(value + 4);
                impl.grid.ordering   = read_i32_le(value + 8);
                impl.grid.radesys    = read_i32_le(value + 12);
                impl.grid.pixfrac    = read_f64_le(value + 16);
                has_grid = true;
                break;
            }
            case HISS_TLV_METADATA_JSON: {
                // R04-B15: JSON 作为可选人类可读附件 (非唯一权威)
                std::string json_str((const char*)value, tlen);
                impl.metadata.from_json(json_str);
                break;
            }
            case HISS_TLV_TILE_DIRECTORY: {
                // 解析 Tile 目录
                if (tlen < 4) {
                    fprintf(stderr, "[hiss][reader] TILE_DIRECTORY 长度非法: %u (< 4)\n", tlen);
                    close();
                    return HISS_ERR_FORMAT;
                }
                uint32_t n_tiles = read_u32_le(value);
                if (n_tiles > 100000000) {
                    fprintf(stderr, "[hiss][reader] n_tiles 异常: %u\n", n_tiles);
                    close();
                    return HISS_ERR_FORMAT;
                }

                impl.tiles.clear();
                impl.tiles.reserve(n_tiles);
                impl.tile_index.clear();
                impl.tile_index.reserve(n_tiles);

                size_t tpos = 4;
                for (uint32_t t = 0; t < n_tiles; t++) {
                    HissTile tile;
                    // R04-B16: 检查 Tile 头越界
                    if (tpos + kTileHeaderSize > tlen) {
                        fprintf(stderr, "[hiss][reader] Tile %u 头越界\n", t);
                        close();
                        return HISS_ERR_FORMAT_VIOLATION;
                    }
                    tile.parent_ipix = read_u64_le(value + tpos);
                    tile.tile_nside  = read_u32_le(value + tpos + 8);
                    tile.occ_mode    = (OccupancyMode)value[tpos + 12];
                    uint16_t n_subblocks = read_u16_le(value + tpos + 13);
                    tpos += kTileHeaderSize;

                    // R04-B16: 检查子块描述符越界
                    if (tpos + (size_t)n_subblocks * kSubblockDescSize > tlen) {
                        fprintf(stderr, "[hiss][reader] Tile %u 子块描述符越界\n", t);
                        close();
                        return HISS_ERR_FORMAT_VIOLATION;
                    }

                    tile.subblocks.resize(n_subblocks);
                    for (uint16_t s = 0; s < n_subblocks; s++) {
                        const uint8_t* sd = value + tpos + (size_t)s * kSubblockDescSize;
                        HissSubblockDescriptor& desc = tile.subblocks[s];
                        desc.type             = (SubblockType)sd[0];
                        desc.ext_type_id      = read_u16_le(sd + 1);   // R04-B17
                        desc.flags            = read_u16_le(sd + 3);
                        desc.offset           = read_u64_le(sd + 5);
                        desc.compressed_size   = read_u64_le(sd + 13);
                        desc.uncompressed_size = read_u64_le(sd + 21);
                        desc.codec_id         = (CodecId)read_u16_le(sd + 29);
                        desc.transform_id     = (TransformId)read_u16_le(sd + 31);
                        desc.checksum_type    = (ChecksumType)sd[33];
                        desc.checksum         = read_u64_le(sd + 34);
                    }
                    tpos += (size_t)n_subblocks * kSubblockDescSize;

                    // R04-B16: 检查重复 Tile (parent_ipix 重复)
                    if (impl.tile_index.find(tile.parent_ipix) != impl.tile_index.end()) {
                        fprintf(stderr, "[hiss][reader] 重复 Tile: parent_ipix=%llu\n",
                                (unsigned long long)tile.parent_ipix);
                        close();
                        return HISS_ERR_FORMAT_VIOLATION;
                    }

                    // 构建查找表
                    impl.tile_index[tile.parent_ipix] = impl.tiles.size();
                    impl.tiles.push_back(std::move(tile));
                }
                has_tiles = true;
                break;
            }
            default: {
                // R04-B15: 未知 TLV 处理
                if (tflags & HISS_TLV_FLAG_REQUIRED) {
                    // 未知必需 TLV → 拒绝
                    fprintf(stderr,
                            "[hiss][reader] 拒绝: 未知必需 TLV tag=0x%04x flags=0x%02x\n",
                            tag, tflags);
                    close();
                    return HISS_ERR_UNKNOWN_REQUIRED;
                } else {
                    // 未知可选 TLV → 跳过
                    fprintf(stderr,
                            "[hiss][reader] 跳过: 未知可选 TLV tag=0x%04x flags=0x%02x length=%u\n",
                            tag, tflags, tlen);
                }
                break;
            }
        }
        hdr_pos += tlen;
    }

    // R04-B16: 必需 TLV 必须存在
    if (!has_schema) {
        fprintf(stderr, "[hiss][reader] 缺少 SCHEMA_FINGERPRINT TLV\n");
        close();
        return HISS_ERR_FORMAT_VIOLATION;
    }
    if (!has_grid) {
        fprintf(stderr, "[hiss][reader] 缺少 GRID_SPEC TLV\n");
        close();
        return HISS_ERR_FORMAT_VIOLATION;
    }
    if (!has_tiles) {
        fprintf(stderr, "[hiss][reader] 缺少 TILE_DIRECTORY TLV\n");
        close();
        return HISS_ERR_FORMAT_VIOLATION;
    }

    // 同步元数据中的网格字段
    impl.metadata.nside      = impl.grid.nside;
    impl.metadata.tile_nside = impl.grid.tile_nside;
    impl.metadata.ordering   = impl.grid.ordering;
    impl.metadata.radesys    = impl.grid.radesys;
    impl.metadata.pixfrac    = impl.grid.pixfrac;

    fprintf(stderr, "[hiss][reader] 网格: nside=%u tile_nside=%u ordering=%d radesys=%d pixfrac=%.3f\n",
            impl.grid.nside, impl.grid.tile_nside, impl.grid.ordering,
            impl.grid.radesys, impl.grid.pixfrac);
    fprintf(stderr, "[hiss][reader] 元数据: object=%.32s filter=%.16s exptime=%.1f\n",
            impl.metadata.object, impl.metadata.filter, impl.metadata.exptime);

    // ---- 3. 全扫描子块目录, 拒绝未知必需子块 (02_FROZEN §13) ----
    // 已知类型: OCCUPANCY/SIGNAL/SUPPORT/SNR/EXTENSION
    // 未知必需子块 → 返回 HISS_ERR_UNKNOWN_REQUIRED (-7)
    // 未知可选子块 → 记录日志并跳过 (继续处理)
    // R04-B17: EXTENSION 类型需检查 ext_type_id, 未知必需扩展拒绝
    for (size_t t = 0; t < impl.tiles.size(); t++) {
        const HissTile& tile = impl.tiles[t];
        for (const auto& sb : tile.subblocks) {
            if (!is_known_subblock_type(sb.type)) {
                bool is_required = (sb.flags & (uint16_t)SubblockFlags::REQUIRED) != 0;
                bool is_optional = (sb.flags & (uint16_t)SubblockFlags::OPTIONAL) != 0;
                if (is_required) {
                    fprintf(stderr,
                            "[hiss][reader] 拒绝: Tile %zu (parent=%llu) 含未知必需子块 "
                            "type=%u flags=0x%04x — 返回 HISS_ERR_UNKNOWN_REQUIRED\n",
                            t, (unsigned long long)tile.parent_ipix,
                            (unsigned)sb.type, sb.flags);
                    close();
                    return HISS_ERR_UNKNOWN_REQUIRED;  // -7
                }
                if (is_optional) {
                    fprintf(stderr,
                            "[hiss][reader] 跳过: Tile %zu (parent=%llu) 含未知可选子块 "
                            "type=%u flags=0x%04x (optional, 跳过)\n",
                            t, (unsigned long long)tile.parent_ipix,
                            (unsigned)sb.type, sb.flags);
                } else {
                    // flags 既无 REQUIRED 也无 OPTIONAL: 视为未知, 拒绝
                    fprintf(stderr,
                            "[hiss][reader] 拒绝: Tile %zu (parent=%llu) 含未知子块 "
                            "type=%u flags=0x%04x (无 required/optional 标记, 视为必需)\n",
                            t, (unsigned long long)tile.parent_ipix,
                            (unsigned)sb.type, sb.flags);
                    close();
                    return HISS_ERR_UNKNOWN_REQUIRED;  // -7
                }
            }
            // R04-B17: EXTENSION 类型子块 — 检查 ext_type_id
            // 内置类型 ext_type_id 必须为 0; EXTENSION 类型 ext_type_id != 0
            // 未知必需扩展 (EXTENSION + REQUIRED + 未知 ext_type_id) → 拒绝
            if (sb.type == SubblockType::EXTENSION) {
                if (sb.ext_type_id == (uint16_t)ExtensionNamespace::BUILTIN) {
                    // EXTENSION 类型但 ext_type_id=0 是非法的
                    fprintf(stderr,
                            "[hiss][reader] 拒绝: Tile %zu EXTENSION 子块 ext_type_id=0 (非法)\n", t);
                    close();
                    return HISS_ERR_FORMAT_VIOLATION;
                }
                bool is_required = (sb.flags & (uint16_t)SubblockFlags::REQUIRED) != 0;
                if (is_required) {
                    // 未知必需扩展 → 拒绝 (当前无已注册扩展)
                    fprintf(stderr,
                            "[hiss][reader] 拒绝: Tile %zu 含未知必需扩展 "
                            "ext_type_id=%u flags=0x%04x\n",
                            t, (unsigned)sb.ext_type_id, sb.flags);
                    close();
                    return HISS_ERR_UNKNOWN_REQUIRED;
                }
            } else {
                // 内置类型 ext_type_id 必须为 0
                if (sb.ext_type_id != (uint16_t)ExtensionNamespace::BUILTIN) {
                    fprintf(stderr,
                            "[hiss][reader] 拒绝: Tile %zu 内置子块 type=%u 但 ext_type_id=%u (应为 0)\n",
                            t, (unsigned)sb.type, (unsigned)sb.ext_type_id);
                    close();
                    return HISS_ERR_FORMAT_VIOLATION;
                }
            }
        }
    }

    // ---- 4. R04-B16: 严格校验 Tile 完整性 ----
    //   a. BITMAP/SPARSE_LIST 模式必须有 OCCUPANCY 子块 (缺少 occupancy → 拒绝)
    //   b. FULL 模式不应有 OCCUPANCY 子块 (一致性检查)
    //   c. NSIDE/tile_nside 关系必须合法 (nside >= tile_nside, 均为 2 的幂)
    //   d. 子块 offset+compressed_size 不得越界
    //   e. 同一 Tile 内子块不得重叠
    for (size_t t = 0; t < impl.tiles.size(); t++) {
        const HissTile& tile = impl.tiles[t];

        // a/b. occupancy 一致性检查
        const HissSubblockDescriptor* occ = Impl::find_subblock(tile, SubblockType::OCCUPANCY);
        if (tile.occ_mode == OccupancyMode::BITMAP || tile.occ_mode == OccupancyMode::SPARSE_LIST) {
            if (!occ) {
                fprintf(stderr,
                        "[hiss][reader] 拒绝: Tile %zu (parent=%llu) occ_mode=%u 但缺少 OCCUPANCY 子块\n",
                        t, (unsigned long long)tile.parent_ipix, (unsigned)tile.occ_mode);
                close();
                return HISS_ERR_FORMAT_VIOLATION;
            }
        } else if (tile.occ_mode == OccupancyMode::FULL) {
            if (occ) {
                fprintf(stderr,
                        "[hiss][reader] 拒绝: Tile %zu (parent=%llu) FULL 模式但含 OCCUPANCY 子块\n",
                        t, (unsigned long long)tile.parent_ipix);
                close();
                return HISS_ERR_FORMAT_VIOLATION;
            }
        } else {
            // 未知 occupancy 模式
            fprintf(stderr,
                    "[hiss][reader] 拒绝: Tile %zu (parent=%llu) 未知 occ_mode=%u\n",
                    t, (unsigned long long)tile.parent_ipix, (unsigned)tile.occ_mode);
            close();
            return HISS_ERR_FORMAT_VIOLATION;
        }

        // c. NSIDE/tile_nside 关系校验
        if (tile.tile_nside == 0 || impl.grid.nside < tile.tile_nside) {
            fprintf(stderr,
                    "[hiss][reader] 拒绝: Tile %zu 非法 NSIDE/tile_nside: nside=%u tile_nside=%u\n",
                    t, impl.grid.nside, tile.tile_nside);
            close();
            return HISS_ERR_FORMAT_VIOLATION;
        }

        // d/e. 子块越界和重叠检查
        // 收集所有子块的 [offset, offset+compressed_size) 区间, 检查越界和重叠
        std::vector<std::pair<uint64_t, uint64_t>> ranges;
        for (const auto& sb : tile.subblocks) {
            if (sb.compressed_size == 0) continue;  // 空子块跳过重叠检查
            uint64_t end = sb.offset + sb.compressed_size;
            if (sb.offset >= impl.filesize || end > impl.filesize) {
                fprintf(stderr,
                        "[hiss][reader] 拒绝: Tile %zu 子块越界 offset=%llu size=%llu filesize=%llu\n",
                        t, (unsigned long long)sb.offset,
                        (unsigned long long)sb.compressed_size,
                        (unsigned long long)impl.filesize);
                close();
                return HISS_ERR_FORMAT_VIOLATION;
            }
            ranges.push_back({sb.offset, end});
        }
        // 按 offset 排序, 检查相邻区间是否重叠
        std::sort(ranges.begin(), ranges.end());
        for (size_t i = 1; i < ranges.size(); i++) {
            if (ranges[i].first < ranges[i - 1].second) {
                fprintf(stderr,
                        "[hiss][reader] 拒绝: Tile %zu 子块重叠 [%llu,%llu) 与 [%llu,%llu)\n",
                        t,
                        (unsigned long long)ranges[i-1].first,
                        (unsigned long long)ranges[i-1].second,
                        (unsigned long long)ranges[i].first,
                        (unsigned long long)ranges[i].second);
                close();
                return HISS_ERR_FORMAT_VIOLATION;
            }
        }
    }

    fprintf(stderr, "[hiss][reader] 打开成功: %s n_tiles=%zu\n",
            path.c_str(), impl.tiles.size());
    return 0;
}

// ----------------------------------------------------------------------------
// grid() / metadata() / tiles(): 返回已解析的数据
// ----------------------------------------------------------------------------
HissGridSpec HissReader::grid() const {
    return pimpl_->grid;
}

HissMetadata HissReader::metadata() const {
    return pimpl_->metadata;
}

// R10: 查询 precision mode 和 signal dtype
uint8_t HissReader::precision_mode() const {
    return pimpl_->metadata.precision_mode;
}

uint8_t HissReader::signal_dtype() const {
    return pimpl_->metadata.signal_dtype;
}

const std::vector<HissTile>& HissReader::tiles() const {
    return pimpl_->tiles;
}

// ----------------------------------------------------------------------------
// read_tile(): 读取 Tile 的 signal + support (展开到 n_leaf_per_tile)
//
// 步骤11 关键改动:
//   - FULL: signal/support 数组长度 = n_leaf_per_tile, 直接返回
//   - BITMAP: 读取 occupancy 位图 + n_valid 个紧凑 signal/support → 展开到 n_leaf_per_tile
//   - SPARSE_LIST: 读取索引列表 + n_valid 个紧凑 signal/support → 展开到 n_leaf_per_tile
//   展开后无效像素的 signal=0.0f, support=0
// ----------------------------------------------------------------------------
int HissReader::read_tile(uint64_t parent_ipix,
                           std::vector<float>& signal,
                           std::vector<uint8_t>& support) const {
    const Impl& impl = *pimpl_;

    // R10: FP64 文件禁止用 float32 接口读取 (禁止静默转换)
    if (impl.metadata.signal_dtype == 1) {
        fprintf(stderr,
                "[hiss][reader] read_tile 失败: 文件为 FP64 模式 (signal_dtype=1), "
                "请使用 read_tile_signal_f64 读取 float64 signal (禁止静默转换)\n");
        return HISS_ERR_UNSUPPORTED;
    }

    // 定位 Tile
    size_t idx;
    if (impl.find_tile(parent_ipix, &idx) != 0) return -1;
    const HissTile& tile = impl.tiles[idx];

    // 查找 SIGNAL 子块
    const HissSubblockDescriptor* sig_desc = Impl::find_subblock(tile, SubblockType::SIGNAL);
    if (!sig_desc) {
        fprintf(stderr, "[hiss][reader] Tile %llu 无 SIGNAL 子块\n",
                (unsigned long long)parent_ipix);
        return -6;
    }

    // 查找 SUPPORT 子块
    const HissSubblockDescriptor* sup_desc = Impl::find_subblock(tile, SubblockType::SUPPORT);
    if (!sup_desc) {
        fprintf(stderr, "[hiss][reader] Tile %llu 无 SUPPORT 子块\n",
                (unsigned long long)parent_ipix);
        return -6;
    }

    // 读取并解压 SIGNAL (紧凑或全长度)
    // WP-G 步骤12: element_size=4 (float32), 支持 transform 逆向
    std::vector<uint8_t> sig_raw;
    int ret = impl.read_subblock(*sig_desc, sig_raw, sizeof(float));
    if (ret != 0) return ret;

    // 读取并解压 SUPPORT (紧凑或全长度)
    // WP-G 步骤12: element_size=1 (uint8)
    std::vector<uint8_t> sup_raw;
    ret = impl.read_subblock(*sup_desc, sup_raw, sizeof(uint8_t));
    if (ret != 0) return ret;

    // 转换 signal: float32 数组 (紧凑)
    size_t n_sig = sig_raw.size() / sizeof(float);
    std::vector<float> signal_compact(n_sig);
    for (size_t i = 0; i < n_sig; i++) {
        signal_compact[i] = read_f32_le(sig_raw.data() + i * sizeof(float));
    }

    // support 紧凑数组
    std::vector<uint8_t> support_compact = std::move(sup_raw);

    // 计算 n_leaf_per_tile (依据 02_FROZEN §11, n_leaf = 4^depth)
    // depth = log2(nside / tile_nside), n_leaf = (nside/tile_nside)^2
    uint32_t n_leaf_per_tile = 0;
    if (impl.grid.nside > 0 && tile.tile_nside > 0 && impl.grid.nside >= tile.tile_nside) {
        uint32_t ratio = impl.grid.nside / tile.tile_nside;
        if (ratio > 0) {
            n_leaf_per_tile = ratio * ratio;  // (NSIDE/tile_nside)^2 = 4^depth
        }
    }
    if (n_leaf_per_tile == 0) {
        // R04-B16: 非法 NSIDE/tile 关系, 禁止退化, 返回格式错误
        fprintf(stderr,
                "[hiss][reader] read_tile: n_leaf_per_tile=0 (nside=%u tile_nside=%u), "
                "非法 NSIDE/tile 关系\n", impl.grid.nside, tile.tile_nside);
        return HISS_ERR_FORMAT_VIOLATION;
    }

    // 根据 occupancy 模式展开
    if (tile.occ_mode == OccupancyMode::FULL) {
        // FULL: 数组已是全长度, 直接返回
        signal = std::move(signal_compact);
        support = std::move(support_compact);
    } else if (tile.occ_mode == OccupancyMode::BITMAP) {
        // BITMAP: 读取 occupancy 位图, 展开紧凑数组到 n_leaf_per_tile
        const HissSubblockDescriptor* occ_desc = Impl::find_subblock(tile, SubblockType::OCCUPANCY);
        if (!occ_desc) {
            fprintf(stderr, "[hiss][reader] Tile %llu BITMAP 模式但无 OCCUPANCY 子块\n",
                    (unsigned long long)parent_ipix);
            return -5;
        }
        std::vector<uint8_t> occ_raw;
        ret = impl.read_subblock(*occ_desc, occ_raw);
        if (ret != 0) return ret;

        // 展开到 n_leaf_per_tile: 无效像素 signal=0, support=0
        signal.assign(n_leaf_per_tile, 0.0f);
        support.assign(n_leaf_per_tile, 0);
        size_t compact_idx = 0;
        for (uint32_t i = 0; i < n_leaf_per_tile; i++) {
            size_t byte_idx = i / 8;
            size_t bit_idx  = i % 8;
            bool valid = false;
            if (byte_idx < occ_raw.size()) {
                valid = (occ_raw[byte_idx] >> bit_idx) & 1;
            }
            if (valid) {
                if (compact_idx < signal_compact.size()) {
                    signal[i] = signal_compact[compact_idx];
                }
                if (compact_idx < support_compact.size()) {
                    support[i] = support_compact[compact_idx];
                }
                compact_idx++;
            }
        }
        fprintf(stderr,
                "[hiss][reader]   BITMAP 展开: n_leaf=%u compact=%zu expanded=%zu\n",
                n_leaf_per_tile, compact_idx, signal.size());
    } else if (tile.occ_mode == OccupancyMode::SPARSE_LIST) {
        // SPARSE_LIST: 读取索引列表, 展开紧凑数组到 n_leaf_per_tile
        const HissSubblockDescriptor* occ_desc = Impl::find_subblock(tile, SubblockType::OCCUPANCY);
        if (!occ_desc) {
            fprintf(stderr, "[hiss][reader] Tile %llu SPARSE_LIST 模式但无 OCCUPANCY 子块\n",
                    (unsigned long long)parent_ipix);
            return -5;
        }
        std::vector<uint8_t> occ_raw;
        ret = impl.read_subblock(*occ_desc, occ_raw, sizeof(uint32_t));  // WP-G: element_size=4 (uint32 索引)
        if (ret != 0) return ret;

        // 索引列表: uint32 数组 (升序)
        size_t n_sparse = occ_raw.size() / sizeof(uint32_t);

        // 展开到 n_leaf_per_tile
        signal.assign(n_leaf_per_tile, 0.0f);
        support.assign(n_leaf_per_tile, 0);
        for (size_t i = 0; i < n_sparse; i++) {
            uint32_t local_ipix = read_u32_le(occ_raw.data() + i * sizeof(uint32_t));
            if (local_ipix < n_leaf_per_tile) {
                if (i < signal_compact.size()) {
                    signal[local_ipix] = signal_compact[i];
                }
                if (i < support_compact.size()) {
                    support[local_ipix] = support_compact[i];
                }
            }
        }
        fprintf(stderr,
                "[hiss][reader]   SPARSE_LIST 展开: n_leaf=%u n_sparse=%zu expanded=%zu\n",
                n_leaf_per_tile, n_sparse, signal.size());
    } else {
        // R04-B16: 未知 occupancy 模式, 禁止退化, 返回格式错误
        fprintf(stderr, "[hiss][reader] read_tile: 未知 occ_mode=%u, 拒绝读取\n",
                (unsigned)tile.occ_mode);
        return HISS_ERR_FORMAT_VIOLATION;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// read_tile_signal(): 只读 signal (展开到 n_leaf_per_tile, 与 read_tile 一致)
// ----------------------------------------------------------------------------
int HissReader::read_tile_signal(uint64_t parent_ipix,
                                  std::vector<float>& signal) const {
    const Impl& impl = *pimpl_;

    // R10: FP64 文件禁止用 float32 接口读取 (禁止静默转换)
    if (impl.metadata.signal_dtype == 1) {
        fprintf(stderr,
                "[hiss][reader] read_tile_signal 失败: 文件为 FP64 模式 (signal_dtype=1), "
                "请使用 read_tile_signal_f64 读取 float64 signal (禁止静默转换)\n");
        return HISS_ERR_UNSUPPORTED;
    }

    size_t idx;
    if (impl.find_tile(parent_ipix, &idx) != 0) return -1;
    const HissTile& tile = impl.tiles[idx];

    const HissSubblockDescriptor* sig_desc = Impl::find_subblock(tile, SubblockType::SIGNAL);
    if (!sig_desc) {
        fprintf(stderr, "[hiss][reader] Tile %llu 无 SIGNAL 子块\n",
                (unsigned long long)parent_ipix);
        return -6;
    }

    std::vector<uint8_t> sig_raw;
    int ret = impl.read_subblock(*sig_desc, sig_raw, sizeof(float));  // WP-G: element_size=4 (float32)
    if (ret != 0) return ret;

    size_t n_sig = sig_raw.size() / sizeof(float);
    std::vector<float> signal_compact(n_sig);
    for (size_t i = 0; i < n_sig; i++) {
        signal_compact[i] = read_f32_le(sig_raw.data() + i * sizeof(float));
    }

    // 计算 n_leaf_per_tile 并按 occ_mode 展开 (与 read_tile 一致)
    uint32_t n_leaf_per_tile = 0;
    if (impl.grid.nside > 0 && tile.tile_nside > 0 && impl.grid.nside >= tile.tile_nside) {
        uint32_t ratio = impl.grid.nside / tile.tile_nside;
        if (ratio > 0) n_leaf_per_tile = ratio * ratio;
    }
    if (n_leaf_per_tile == 0) {
        // R04-B16: 非法 NSIDE/tile 关系, 禁止退化
        fprintf(stderr,
                "[hiss][reader] read_tile_signal: n_leaf_per_tile=0 (nside=%u tile_nside=%u)\n",
                impl.grid.nside, tile.tile_nside);
        return HISS_ERR_FORMAT_VIOLATION;
    }
    if (tile.occ_mode == OccupancyMode::FULL) {
        signal = std::move(signal_compact);
        return 0;
    }

    // BITMAP / SPARSE_LIST: 需读 occupancy 展开到 n_leaf_per_tile
    const HissSubblockDescriptor* occ_desc = Impl::find_subblock(tile, SubblockType::OCCUPANCY);
    if (!occ_desc) {
        // R04-B16: BITMAP/SPARSE 模式缺少 OCCUPANCY 子块, 禁止退化
        fprintf(stderr, "[hiss][reader] Tile %llu occ_mode=%u 但无 OCCUPANCY 子块\n",
                (unsigned long long)parent_ipix, (unsigned)tile.occ_mode);
        return HISS_ERR_FORMAT_VIOLATION;
    }
    // WP-G 步骤12: SPARSE_LIST 的 occupancy 为 uint32 索引数组 (element_size=4)
    size_t occ_elem_size = (tile.occ_mode == OccupancyMode::SPARSE_LIST) ? sizeof(uint32_t) : 1;
    std::vector<uint8_t> occ_raw;
    ret = impl.read_subblock(*occ_desc, occ_raw, occ_elem_size);
    if (ret != 0) return ret;

    signal.assign(n_leaf_per_tile, 0.0f);
    if (tile.occ_mode == OccupancyMode::BITMAP) {
        size_t compact_idx = 0;
        for (uint32_t i = 0; i < n_leaf_per_tile; i++) {
            size_t byte_idx = i / 8;
            size_t bit_idx  = i % 8;
            bool valid = (byte_idx < occ_raw.size()) && ((occ_raw[byte_idx] >> bit_idx) & 1);
            if (valid) {
                if (compact_idx < signal_compact.size()) signal[i] = signal_compact[compact_idx];
                compact_idx++;
            }
        }
    } else { // SPARSE_LIST
        size_t n_sparse = occ_raw.size() / sizeof(uint32_t);
        for (size_t i = 0; i < n_sparse; i++) {
            uint32_t local_ipix = read_u32_le(occ_raw.data() + i * sizeof(uint32_t));
            if (local_ipix < n_leaf_per_tile && i < signal_compact.size()) {
                signal[local_ipix] = signal_compact[i];
            }
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// read_tile_signal_f64(): 只读 signal (float64, R10 FP64 模式)
//   若文件 signal_dtype=1 (FP64), 直接返回 float64 数据
//   若文件 signal_dtype=0 (FP32), 返回错误 (禁止静默转换)
//   展开逻辑与 read_tile_signal 一致 (FULL/BITMAP/SPARSE_LIST)
// ----------------------------------------------------------------------------
int HissReader::read_tile_signal_f64(uint64_t parent_ipix,
                                      std::vector<double>& signal) const {
    const Impl& impl = *pimpl_;

    // R10: FP32 文件禁止用 float64 接口读取 (禁止静默转换)
    if (impl.metadata.signal_dtype == 0) {
        fprintf(stderr,
                "[hiss][reader] read_tile_signal_f64 失败: 文件为 FP32 模式 (signal_dtype=0), "
                "请使用 read_tile_signal 读取 float32 signal (禁止静默转换)\n");
        return HISS_ERR_UNSUPPORTED;
    }

    size_t idx;
    if (impl.find_tile(parent_ipix, &idx) != 0) return -1;
    const HissTile& tile = impl.tiles[idx];

    const HissSubblockDescriptor* sig_desc = Impl::find_subblock(tile, SubblockType::SIGNAL);
    if (!sig_desc) {
        fprintf(stderr, "[hiss][reader] Tile %llu 无 SIGNAL 子块\n",
                (unsigned long long)parent_ipix);
        return -6;
    }

    // 读取并解压 SIGNAL (element_size=8 for float64)
    std::vector<uint8_t> sig_raw;
    int ret = impl.read_subblock(*sig_desc, sig_raw, sizeof(double));
    if (ret != 0) return ret;

    // 转换 signal: float64 数组 (紧凑)
    size_t n_sig = sig_raw.size() / sizeof(double);
    std::vector<double> signal_compact(n_sig);
    for (size_t i = 0; i < n_sig; i++) {
        signal_compact[i] = read_f64_le(sig_raw.data() + i * sizeof(double));
    }

    // 计算 n_leaf_per_tile 并按 occ_mode 展开 (与 read_tile_signal 一致)
    uint32_t n_leaf_per_tile = 0;
    if (impl.grid.nside > 0 && tile.tile_nside > 0 && impl.grid.nside >= tile.tile_nside) {
        uint32_t ratio = impl.grid.nside / tile.tile_nside;
        if (ratio > 0) n_leaf_per_tile = ratio * ratio;
    }
    if (n_leaf_per_tile == 0) {
        fprintf(stderr,
                "[hiss][reader] read_tile_signal_f64: n_leaf_per_tile=0 (nside=%u tile_nside=%u)\n",
                impl.grid.nside, tile.tile_nside);
        return HISS_ERR_FORMAT_VIOLATION;
    }
    if (tile.occ_mode == OccupancyMode::FULL) {
        signal = std::move(signal_compact);
        return 0;
    }

    // BITMAP / SPARSE_LIST: 需读 occupancy 展开到 n_leaf_per_tile
    const HissSubblockDescriptor* occ_desc = Impl::find_subblock(tile, SubblockType::OCCUPANCY);
    if (!occ_desc) {
        fprintf(stderr, "[hiss][reader] Tile %llu occ_mode=%u 但无 OCCUPANCY 子块\n",
                (unsigned long long)parent_ipix, (unsigned)tile.occ_mode);
        return HISS_ERR_FORMAT_VIOLATION;
    }
    size_t occ_elem_size = (tile.occ_mode == OccupancyMode::SPARSE_LIST) ? sizeof(uint32_t) : 1;
    std::vector<uint8_t> occ_raw;
    ret = impl.read_subblock(*occ_desc, occ_raw, occ_elem_size);
    if (ret != 0) return ret;

    signal.assign(n_leaf_per_tile, 0.0);
    if (tile.occ_mode == OccupancyMode::BITMAP) {
        size_t compact_idx = 0;
        for (uint32_t i = 0; i < n_leaf_per_tile; i++) {
            size_t byte_idx = i / 8;
            size_t bit_idx  = i % 8;
            bool valid = (byte_idx < occ_raw.size()) && ((occ_raw[byte_idx] >> bit_idx) & 1);
            if (valid) {
                if (compact_idx < signal_compact.size()) signal[i] = signal_compact[compact_idx];
                compact_idx++;
            }
        }
    } else { // SPARSE_LIST
        size_t n_sparse = occ_raw.size() / sizeof(uint32_t);
        for (size_t i = 0; i < n_sparse; i++) {
            uint32_t local_ipix = read_u32_le(occ_raw.data() + i * sizeof(uint32_t));
            if (local_ipix < n_leaf_per_tile && i < signal_compact.size()) {
                signal[local_ipix] = signal_compact[i];
            }
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// read_tile_support(): 只读 support (展开到 n_leaf_per_tile, 与 read_tile 一致)
// ----------------------------------------------------------------------------
int HissReader::read_tile_support(uint64_t parent_ipix,
                                   std::vector<uint8_t>& support) const {
    const Impl& impl = *pimpl_;

    size_t idx;
    if (impl.find_tile(parent_ipix, &idx) != 0) return -1;
    const HissTile& tile = impl.tiles[idx];

    const HissSubblockDescriptor* sup_desc = Impl::find_subblock(tile, SubblockType::SUPPORT);
    if (!sup_desc) {
        fprintf(stderr, "[hiss][reader] Tile %llu 无 SUPPORT 子块\n",
                (unsigned long long)parent_ipix);
        return -6;
    }

    std::vector<uint8_t> support_compact;
    int ret = impl.read_subblock(*sup_desc, support_compact);
    if (ret != 0) return ret;

    // 计算 n_leaf_per_tile 并按 occ_mode 展开 (与 read_tile 一致)
    uint32_t n_leaf_per_tile = 0;
    if (impl.grid.nside > 0 && tile.tile_nside > 0 && impl.grid.nside >= tile.tile_nside) {
        uint32_t ratio = impl.grid.nside / tile.tile_nside;
        if (ratio > 0) n_leaf_per_tile = ratio * ratio;
    }
    if (n_leaf_per_tile == 0) {
        // R04-B16: 非法 NSIDE/tile 关系, 禁止退化
        fprintf(stderr,
                "[hiss][reader] read_tile_support: n_leaf_per_tile=0 (nside=%u tile_nside=%u)\n",
                impl.grid.nside, tile.tile_nside);
        return HISS_ERR_FORMAT_VIOLATION;
    }
    if (tile.occ_mode == OccupancyMode::FULL) {
        support = std::move(support_compact);
        return 0;
    }

    // BITMAP / SPARSE_LIST: 需读 occupancy 展开到 n_leaf_per_tile
    const HissSubblockDescriptor* occ_desc = Impl::find_subblock(tile, SubblockType::OCCUPANCY);
    if (!occ_desc) {
        // R04-B16: BITMAP/SPARSE 模式缺少 OCCUPANCY 子块, 禁止退化
        fprintf(stderr, "[hiss][reader] Tile %llu occ_mode=%u 但无 OCCUPANCY 子块\n",
                (unsigned long long)parent_ipix, (unsigned)tile.occ_mode);
        return HISS_ERR_FORMAT_VIOLATION;
    }
    // WP-G 步骤12: SPARSE_LIST 的 occupancy 为 uint32 索引数组 (element_size=4)
    size_t occ_elem_size = (tile.occ_mode == OccupancyMode::SPARSE_LIST) ? sizeof(uint32_t) : 1;
    std::vector<uint8_t> occ_raw;
    ret = impl.read_subblock(*occ_desc, occ_raw, occ_elem_size);
    if (ret != 0) return ret;

    support.assign(n_leaf_per_tile, 0);
    if (tile.occ_mode == OccupancyMode::BITMAP) {
        size_t compact_idx = 0;
        for (uint32_t i = 0; i < n_leaf_per_tile; i++) {
            size_t byte_idx = i / 8;
            size_t bit_idx  = i % 8;
            bool valid = (byte_idx < occ_raw.size()) && ((occ_raw[byte_idx] >> bit_idx) & 1);
            if (valid) {
                if (compact_idx < support_compact.size()) support[i] = support_compact[compact_idx];
                compact_idx++;
            }
        }
    } else { // SPARSE_LIST
        size_t n_sparse = occ_raw.size() / sizeof(uint32_t);
        for (size_t i = 0; i < n_sparse; i++) {
            uint32_t local_ipix = read_u32_le(occ_raw.data() + i * sizeof(uint32_t));
            if (local_ipix < n_leaf_per_tile && i < support_compact.size()) {
                support[local_ipix] = support_compact[i];
            }
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// read_tile_snr(): 读取 SNR 控制点
// R04-B18: block 级 estimator_id/sampling_scale/count + 点数据
// 冻结布局 (02_FROZEN §17 + 00_COMMON_CONTRACTS §2.5):
//   [estimator_id:  uint32 LE]   — 估计器 ID (block 级)
//   [sampling_scale: float32 LE] — 采样尺度 (block 级)
//   [n_points:      uint32 LE]   — 控制点数 (= count, block 级)
//   [points: n_points × 8B (local_ipix u32 + snr f32)]
//   不包含 snr_phot/median_snr/idw_power (估计器状态, 不写入 HISS)
// ----------------------------------------------------------------------------
int HissReader::read_tile_snr(uint64_t parent_ipix, HissSnrBlock& snr) const {
    const Impl& impl = *pimpl_;

    size_t idx;
    if (impl.find_tile(parent_ipix, &idx) != 0) return -1;
    const HissTile& tile = impl.tiles[idx];

    const HissSubblockDescriptor* snr_desc = Impl::find_subblock(tile, SubblockType::SNR);
    if (!snr_desc) {
        fprintf(stderr, "[hiss][reader] Tile %llu 无 SNR 子块\n",
                (unsigned long long)parent_ipix);
        return -6;
    }

    // 读取并解压 SNR 子块
    std::vector<uint8_t> raw;
    int ret = impl.read_subblock(*snr_desc, raw);
    if (ret != 0) return ret;

    // R04-B18: 解析 SNR 数据 (estimator_id + sampling_scale + n_points + points)
    // 最小长度: estimator_id(4) + sampling_scale(4) + n_points(4) = 12
    if (raw.size() < 12) {
        fprintf(stderr, "[hiss][reader] SNR 数据过短: %zu (最小 12)\n", raw.size());
        return HISS_ERR_FORMAT_VIOLATION;
    }

    snr.estimator_id   = read_u32_le(raw.data() + 0);
    snr.sampling_scale = read_f32_le(raw.data() + 4);
    uint32_t n_points  = read_u32_le(raw.data() + 8);

    size_t expected = 12 + (size_t)n_points * 8;
    if (raw.size() != expected) {
        fprintf(stderr, "[hiss][reader] SNR 数据长度不匹配: got=%zu expected=%zu (n_points=%u)\n",
                raw.size(), expected, n_points);
        return HISS_ERR_FORMAT_VIOLATION;
    }

    // 解析控制点: 每点 local_ipix(u32) + snr(f32) = 8 字节
    snr.points.resize(n_points);
    const uint8_t* p = raw.data() + 12;
    for (uint32_t i = 0; i < n_points; i++) {
        snr.points[i].local_ipix = read_u32_le(p);
        snr.points[i].snr        = read_f32_le(p + 4);
        p += 8;
    }

    fprintf(stderr,
            "[hiss][reader]   SNR 子块: estimator_id=%u sampling_scale=%g 读取 %u 个控制点\n",
            snr.estimator_id, snr.sampling_scale, n_points);

    return 0;
}

// ----------------------------------------------------------------------------
// query_pixel(): 查询某位置的 signal/support
// 通过 NSIDE/NESTED/ICRS 定位, 不依赖 WCS
// ----------------------------------------------------------------------------
int HissReader::query_pixel(double ra, double dec,
                             float* signal, uint8_t* support) const {
    const Impl& impl = *pimpl_;

    if (impl.grid.nside == 0 || impl.grid.tile_nside == 0) {
        fprintf(stderr, "[hiss][reader] query_pixel: 网格未初始化\n");
        return -1;
    }

    // 1. ra/dec → 全局 NESTED ipix (在 NSIDE 级别)
    uint64_t global_ipix = radec_to_nested_ipix(ra, dec, impl.grid.nside);

    // 2. 计算 parent_ipix 和 local_ipix
    //    shift = 2 * log2(NSIDE / tile_nside)
    int shift = 2 * (log2i(impl.grid.nside) - log2i(impl.grid.tile_nside));
    if (shift < 0) shift = 0;
    uint64_t parent_ipix = global_ipix >> shift;
    uint64_t local_ipix  = global_ipix & ((1ULL << shift) - 1);

    // 3. 定位 Tile
    size_t idx;
    if (impl.find_tile(parent_ipix, &idx) != 0) {
        // 该位置不在任何 Tile 中 (无覆盖)
        if (signal)  *signal = 0.0f;
        if (support) *support = 0;
        return 0;  // 不报错, 返回零值
    }
    const HissTile& tile = impl.tiles[idx];

    // 4. 读取 signal/support
    std::vector<float> sig_arr;
    std::vector<uint8_t> sup_arr;
    int ret = read_tile(parent_ipix, sig_arr, sup_arr);
    if (ret != 0) return ret;

    // 5. 根据 occupancy 模式定位像素
    if (tile.occ_mode == OccupancyMode::FULL) {
        // FULL 模式: 数组按 local_ipix 直接索引
        // 数组大小 = tile_nside² × 12
        if (local_ipix < sig_arr.size()) {
            if (signal)  *signal = sig_arr[(size_t)local_ipix];
        } else {
            if (signal)  *signal = 0.0f;
        }
        if (local_ipix < sup_arr.size()) {
            if (support) *support = sup_arr[(size_t)local_ipix];
        } else {
            if (support) *support = 0;
        }
    } else if (tile.occ_mode == OccupancyMode::BITMAP) {
        // BITMAP 模式: 需要读取 occupancy bitmap 确定像素是否有效
        // 先查找 OCCUPANCY 子块
        const HissSubblockDescriptor* occ_desc = Impl::find_subblock(tile, SubblockType::OCCUPANCY);
        if (!occ_desc) {
            // 无 occupancy 子块, 无法确定, 返回零值
            if (signal)  *signal = 0.0f;
            if (support) *support = 0;
            return 0;
        }
        std::vector<uint8_t> occ_raw;
        ret = impl.read_subblock(*occ_desc, occ_raw);
        if (ret != 0) return ret;

        // bitmap: 每 bit 代表一个潜在叶像素
        size_t byte_idx = (size_t)(local_ipix / 8);
        size_t bit_idx  = (size_t)(local_ipix % 8);
        if (byte_idx >= occ_raw.size()) {
            if (signal)  *signal = 0.0f;
            if (support) *support = 0;
            return 0;
        }
        bool valid = (occ_raw[byte_idx] >> bit_idx) & 1;
        if (!valid) {
            if (signal)  *signal = 0.0f;
            if (support) *support = 0;
            return 0;
        }
        // read_tile 在 BITMAP 模式下返回展开到 n_leaf_per_tile 的数组
        // (无效像素位置已置0), occupancy 检查已确认有效, 故直接用 local_ipix 索引
        if (local_ipix < sig_arr.size()) {
            if (signal)  *signal = sig_arr[(size_t)local_ipix];
        } else {
            if (signal)  *signal = 0.0f;
        }
        if (local_ipix < sup_arr.size()) {
            if (support) *support = sup_arr[(size_t)local_ipix];
        } else {
            if (support) *support = 0;
        }
    } else if (tile.occ_mode == OccupancyMode::SPARSE_LIST) {
        // SPARSE_LIST 模式: occupancy 子块为 uint32 升序列表
        const HissSubblockDescriptor* occ_desc = Impl::find_subblock(tile, SubblockType::OCCUPANCY);
        if (!occ_desc) {
            if (signal)  *signal = 0.0f;
            if (support) *support = 0;
            return 0;
        }
        std::vector<uint8_t> occ_raw;
        ret = impl.read_subblock(*occ_desc, occ_raw, sizeof(uint32_t));  // WP-G: element_size=4 (uint32 索引)
        if (ret != 0) return ret;

        // sparse list: uint32 数组 (升序)
        size_t n_sparse = occ_raw.size() / sizeof(uint32_t);
        // 二分查找 local_ipix
        size_t lo = 0, hi = n_sparse;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            uint32_t v = read_u32_le(occ_raw.data() + mid * sizeof(uint32_t));
            if (v < (uint32_t)local_ipix) lo = mid + 1;
            else hi = mid;
        }
        if (lo < n_sparse) {
            uint32_t v = read_u32_le(occ_raw.data() + lo * sizeof(uint32_t));
            if (v == (uint32_t)local_ipix) {
                // read_tile 在 SPARSE_LIST 模式下返回展开到 n_leaf_per_tile 的数组
                // (无效像素位置已置0), 故直接用 local_ipix 索引
                if (local_ipix < sig_arr.size()) {
                    if (signal)  *signal = sig_arr[(size_t)local_ipix];
                } else {
                    if (signal)  *signal = 0.0f;
                }
                if (local_ipix < sup_arr.size()) {
                    if (support) *support = sup_arr[(size_t)local_ipix];
                } else {
                    if (support) *support = 0;
                }
                return 0;
            }
        }
        // 未找到
        if (signal)  *signal = 0.0f;
        if (support) *support = 0;
    } else {
        // 未知 occupancy 模式
        if (signal)  *signal = 0.0f;
        if (support) *support = 0;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// close(): 关闭文件, 清理内存
// ----------------------------------------------------------------------------
void HissReader::close() {
    Impl& impl = *pimpl_;
    if (impl.fp) {
        std::fclose(impl.fp);
        impl.fp = nullptr;
    }
    impl.filesize = 0;
    impl.header_length = 0;
    impl.feature_flags = 0;
    impl.grid = HissGridSpec{};
    impl.metadata = HissMetadata{};
    impl.tiles.clear();
    impl.tile_index.clear();
}

} // namespace hiss
