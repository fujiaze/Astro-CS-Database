// ============================================================================
// hiss_reader.cpp - AstroCS HISS Reader (XISF 式 Header + attachments 格式)
//
// 内容:
//   1. 小端序二进制读写工具
//   2. CRC32-C (Castagnoli) 校验实现
//   3. log2i 辅助 (用于 query_pixel 坐标转换)
//   4. HEALPix NESTED 坐标转换 (ra/dec → ipix, 不依赖外部 HealpixCore)
//   5. HissReader 类实现 (按目录读取, 按需加载 Tile, 不依赖 WCS)
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
// ============================================================================
#include "hiss_format.h"
#include "aio_util.h"  // aio_fopen_utf8 (UTF-8 路径支持)

#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace hiss {

// ============================================================================
// 内部常量
// ============================================================================

// 固定签名块: MAGIC(8) + version(4) + header_offset(8) = 20 字节
static const char     kMagic[8] = { 'A','C','S','H','I','S','S','\0' };
static const uint32_t kVersion  = 1;                // 当前格式版本
static const size_t   kSignatureSize = 20;          // 固定签名块大小

// Header 子结构大小
static const size_t kGridSpecSize   = 24;  // nside(4)+tile_nside(4)+ordering(4)+radesys(4)+pixfrac(8)
static const size_t kSubblockDescSize = 40; // type(1)+flags(2)+offset(8)+comp(8)+uncomp(8)+codec(2)+transform(2)+checksum_type(1)+checksum(8)
static const size_t kTileHeaderSize  = 15;  // parent_ipix(8)+tile_nside(4)+occ_mode(1)+n_subblocks(2)

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
// 2. CRC32-C (Castagnoli) 校验实现
//    多项式: 0x1EDC6F41 (反向: 0x82F63B78)
//    初始值: 0xFFFFFFFF, 最终异或: 0xFFFFFFFF
//    用于 HissSubblockDescriptor.checksum (ChecksumType::CRC32C)
// ============================================================================

// CRC32-C 查表实现 (运行时生成表, 首次调用时初始化)
static uint32_t crc32c_table[256];
static bool crc32c_table_init = false;

static void init_crc32c_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x82F63B78u;  // Castagnoli 反向多项式
            else
                crc >>= 1;
        }
        crc32c_table[i] = crc;
    }
    crc32c_table_init = true;
}

static uint32_t crc32c(const uint8_t* data, size_t size) {
    if (!crc32c_table_init) init_crc32c_table();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++) {
        crc = crc32c_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// ============================================================================
// 3. log2i 辅助 (用于 query_pixel 坐标转换, 计算 NSIDE/tile_nside 的位移)
//    注: HissMetadata::to_json/from_json 与 compute_tile_depth/nside 已移至
//    hiss_common.cpp (与 Writer 共用, 避免重复定义)
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
// 5. HissReader::Impl 实现
// ============================================================================

struct HissReader::Impl {
    FILE*      fp = nullptr;            // 文件句柄 (二进制只读)
    uint64_t   filesize = 0;            // 文件总大小
    uint32_t   version = 0;             // 格式版本
    uint64_t   header_offset = 0;       // Header 在文件中的偏移
    HissGridSpec  grid;                 // 网格规格
    HissMetadata  metadata;             // 元数据
    std::vector<HissTile> tiles;        // Tile 目录
    std::unordered_map<uint64_t, size_t> tile_index;  // parent_ipix → tiles[] 索引

    // ---- 内部方法 ----

    // 读取子块数据 (解压 + 校验)
    // 返回 0=成功, <0=失败 (错误码见任务规范)
    int read_subblock(const HissSubblockDescriptor& desc,
                      std::vector<uint8_t>& out) const {
        // a. 检查 offset + compressed_size 越界
        if (desc.offset + desc.compressed_size > filesize) {
            fprintf(stderr, "[hiss][reader] 子块越界: offset=%llu size=%llu filesize=%llu\n",
                    (unsigned long long)desc.offset,
                    (unsigned long long)desc.compressed_size,
                    (unsigned long long)filesize);
            return -3;
        }

        // b. seek 到 offset, 读取 compressed_size 字节
        std::vector<uint8_t> comp(desc.compressed_size);
        if (std::fseek(fp, (long)desc.offset, SEEK_SET) != 0) {
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
        if (desc.checksum_type != ChecksumType::NONE) {
            uint64_t calc = 0;
            if (desc.checksum_type == ChecksumType::CRC32C) {
                calc = crc32c(comp.data(), comp.size());
            } else if (desc.checksum_type == ChecksumType::XXHASH) {
                // XXHASH 暂未实现, 打印警告但不阻塞
                fprintf(stderr, "[hiss][reader] 警告: XXHASH 校验未实现, 跳过\n");
            }
            if (desc.checksum_type == ChecksumType::CRC32C && calc != desc.checksum) {
                fprintf(stderr, "[hiss][reader] CRC32C 校验失败: calc=%llx stored=%llx\n",
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

        // f. 反向变换检查 (目前不支持任何变换)
        if (desc.transform_id != TransformId::NONE) {
            fprintf(stderr, "[hiss][reader] transform_id=%u 不支持\n",
                    (unsigned)desc.transform_id);
            return -6;
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

    // 获取文件大小
    std::fseek(impl.fp, 0, SEEK_END);
    long fsize = std::ftell(impl.fp);
    if (fsize < 0) {
        fprintf(stderr, "[hiss][reader] 无法获取文件大小\n");
        close();
        return -1;
    }
    impl.filesize = (uint64_t)fsize;

    // ---- 1. 读取固定签名块 (20 字节) ----
    if (impl.filesize < kSignatureSize) {
        fprintf(stderr, "[hiss][reader] 文件过短: %llu < %zu\n",
                (unsigned long long)impl.filesize, kSignatureSize);
        close();
        return -1;
    }

    uint8_t sig[kSignatureSize];
    std::fseek(impl.fp, 0, SEEK_SET);
    if (std::fread(sig, 1, kSignatureSize, impl.fp) != kSignatureSize) {
        fprintf(stderr, "[hiss][reader] 读取签名块失败\n");
        close();
        return -1;
    }

    // 验证 MAGIC (8 字节)
    if (std::memcmp(sig, kMagic, 8) != 0) {
        fprintf(stderr, "[hiss][reader] MAGIC 不匹配\n");
        close();
        return -1;  // MAGIC 不匹配 → -1
    }

    // 读取 version (4 字节 uint32 LE)
    impl.version = read_u32_le(sig + 8);
    if (impl.version != kVersion) {
        fprintf(stderr, "[hiss][reader] version 不兼容: %u (期望 %u)\n",
                impl.version, kVersion);
        close();
        return -2;  // version 不兼容 → -2
    }

    // 读取 header_offset (8 字节 uint64 LE)
    impl.header_offset = read_u64_le(sig + 12);
    if (impl.header_offset + kGridSpecSize > impl.filesize) {
        fprintf(stderr, "[hiss][reader] header_offset 越界: %llu\n",
                (unsigned long long)impl.header_offset);
        close();
        return -3;
    }

    // ---- 2. 读取 Header: 网格规格 + 元数据JSON + Tile目录 ----
    std::fseek(impl.fp, (long)impl.header_offset, SEEK_SET);

    // 2a. 网格规格 HissGridSpec (24 字节)
    uint8_t gs[kGridSpecSize];
    if (std::fread(gs, 1, kGridSpecSize, impl.fp) != kGridSpecSize) {
        fprintf(stderr, "[hiss][reader] 读取网格规格失败\n");
        close();
        return -3;
    }
    impl.grid.nside      = read_u32_le(gs + 0);
    impl.grid.tile_nside = read_u32_le(gs + 4);
    impl.grid.ordering   = read_i32_le(gs + 8);
    impl.grid.radesys    = read_i32_le(gs + 12);
    impl.grid.pixfrac    = read_f64_le(gs + 16);

    fprintf(stderr, "[hiss][reader] 网格: nside=%u tile_nside=%u ordering=%d radesys=%d pixfrac=%.3f\n",
            impl.grid.nside, impl.grid.tile_nside, impl.grid.ordering,
            impl.grid.radesys, impl.grid.pixfrac);

    // 2b. 元数据 JSON (4 字节长度 + JSON 数据)
    uint8_t json_len_buf[4];
    if (std::fread(json_len_buf, 1, 4, impl.fp) != 4) {
        fprintf(stderr, "[hiss][reader] 读取 JSON 长度失败\n");
        close();
        return -3;
    }
    uint32_t json_len = read_u32_le(json_len_buf);
    if (json_len > 16 * 1024 * 1024) {  // 限制 16MB 防止异常值
        fprintf(stderr, "[hiss][reader] JSON 长度异常: %u\n", json_len);
        close();
        return -3;
    }
    std::string json_str(json_len, '\0');
    if (json_len > 0) {
        if (std::fread(&json_str[0], 1, json_len, impl.fp) != json_len) {
            fprintf(stderr, "[hiss][reader] 读取 JSON 数据失败\n");
            close();
            return -3;
        }
    }
    impl.metadata.from_json(json_str);
    // 同步元数据中的网格字段
    impl.metadata.nside      = impl.grid.nside;
    impl.metadata.tile_nside = impl.grid.tile_nside;
    impl.metadata.ordering   = impl.grid.ordering;
    impl.metadata.radesys    = impl.grid.radesys;
    impl.metadata.pixfrac    = impl.grid.pixfrac;

    fprintf(stderr, "[hiss][reader] 元数据: object=%.32s filter=%.16s exptime=%.1f\n",
            impl.metadata.object, impl.metadata.filter, impl.metadata.exptime);

    // 2c. Tile 目录 (4 字节 n_tiles + 每个 Tile 的描述符)
    uint8_t n_tiles_buf[4];
    if (std::fread(n_tiles_buf, 1, 4, impl.fp) != 4) {
        fprintf(stderr, "[hiss][reader] 读取 n_tiles 失败\n");
        close();
        return -3;
    }
    uint32_t n_tiles = read_u32_le(n_tiles_buf);
    if (n_tiles > 100000000) {  // 限制 1 亿防止异常值
        fprintf(stderr, "[hiss][reader] n_tiles 异常: %u\n", n_tiles);
        close();
        return -3;
    }

    impl.tiles.clear();
    impl.tiles.reserve(n_tiles);
    impl.tile_index.clear();
    impl.tile_index.reserve(n_tiles);

    for (uint32_t t = 0; t < n_tiles; t++) {
        HissTile tile;

        // 读取 Tile 头 (15 字节: parent_ipix + tile_nside + occ_mode + n_subblocks)
        uint8_t th[kTileHeaderSize];
        if (std::fread(th, 1, kTileHeaderSize, impl.fp) != kTileHeaderSize) {
            fprintf(stderr, "[hiss][reader] 读取 Tile %u 头失败\n", t);
            close();
            return -3;
        }
        tile.parent_ipix = read_u64_le(th + 0);
        tile.tile_nside  = read_u32_le(th + 8);
        tile.occ_mode    = (OccupancyMode)th[12];
        uint16_t n_subblocks = read_u16_le(th + 13);

        // 读取子块描述符 (每个 40 字节)
        tile.subblocks.resize(n_subblocks);
        for (uint16_t s = 0; s < n_subblocks; s++) {
            uint8_t sd[kSubblockDescSize];
            if (std::fread(sd, 1, kSubblockDescSize, impl.fp) != kSubblockDescSize) {
                fprintf(stderr, "[hiss][reader] 读取 Tile %u 子块 %u 描述符失败\n", t, s);
                close();
                return -3;
            }
            HissSubblockDescriptor& desc = tile.subblocks[s];
            desc.type             = (SubblockType)sd[0];
            desc.flags            = read_u16_le(sd + 1);
            desc.offset           = read_u64_le(sd + 3);
            desc.compressed_size   = read_u64_le(sd + 11);
            desc.uncompressed_size = read_u64_le(sd + 19);
            desc.codec_id         = (CodecId)read_u16_le(sd + 27);
            desc.transform_id     = (TransformId)read_u16_le(sd + 29);
            desc.checksum_type    = (ChecksumType)sd[31];
            desc.checksum         = read_u64_le(sd + 32);
        }

        // 构建查找表
        impl.tile_index[tile.parent_ipix] = impl.tiles.size();
        impl.tiles.push_back(std::move(tile));
    }

    fprintf(stderr, "[hiss][reader] 打开成功: %s n_tiles=%u\n",
            path.c_str(), n_tiles);
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

const std::vector<HissTile>& HissReader::tiles() const {
    return pimpl_->tiles;
}

// ----------------------------------------------------------------------------
// read_tile(): 读取 Tile 的 signal + support
// ----------------------------------------------------------------------------
int HissReader::read_tile(uint64_t parent_ipix,
                           std::vector<float>& signal,
                           std::vector<uint8_t>& support) const {
    const Impl& impl = *pimpl_;

    // 定位 Tile
    size_t idx;
    if (impl.find_tile(parent_ipix, &idx) != 0) return -1;
    const HissTile& tile = impl.tiles[idx];

    // 查找 SIGNAL 子块
    const HissSubblockDescriptor* sig_desc = Impl::find_subblock(tile, SubblockType::SIGNAL);
    if (!sig_desc) {
        fprintf(stderr, "[hiss][reader] Tile %llu 无 SIGNAL 子块\n",
                (unsigned long long)parent_ipix);
        return -6;  // 未知必需子块 → -6
    }

    // 查找 SUPPORT 子块
    const HissSubblockDescriptor* sup_desc = Impl::find_subblock(tile, SubblockType::SUPPORT);
    if (!sup_desc) {
        fprintf(stderr, "[hiss][reader] Tile %llu 无 SUPPORT 子块\n",
                (unsigned long long)parent_ipix);
        return -6;
    }

    // 读取并解压 SIGNAL
    std::vector<uint8_t> sig_raw;
    int ret = impl.read_subblock(*sig_desc, sig_raw);
    if (ret != 0) return ret;

    // 读取并解压 SUPPORT
    std::vector<uint8_t> sup_raw;
    ret = impl.read_subblock(*sup_desc, sup_raw);
    if (ret != 0) return ret;

    // 转换 signal: float32 数组
    size_t n_sig = sig_raw.size() / sizeof(float);
    signal.resize(n_sig);
    for (size_t i = 0; i < n_sig; i++) {
        signal[i] = read_f32_le(sig_raw.data() + i * sizeof(float));
    }

    // 转换 support: uint8 数组
    support = std::move(sup_raw);

    return 0;
}

// ----------------------------------------------------------------------------
// read_tile_signal(): 只读 signal
// ----------------------------------------------------------------------------
int HissReader::read_tile_signal(uint64_t parent_ipix,
                                  std::vector<float>& signal) const {
    const Impl& impl = *pimpl_;

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
    int ret = impl.read_subblock(*sig_desc, sig_raw);
    if (ret != 0) return ret;

    size_t n_sig = sig_raw.size() / sizeof(float);
    signal.resize(n_sig);
    for (size_t i = 0; i < n_sig; i++) {
        signal[i] = read_f32_le(sig_raw.data() + i * sizeof(float));
    }
    return 0;
}

// ----------------------------------------------------------------------------
// read_tile_support(): 只读 support
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

    std::vector<uint8_t> sup_raw;
    int ret = impl.read_subblock(*sup_desc, sup_raw);
    if (ret != 0) return ret;

    support = std::move(sup_raw);
    return 0;
}

// ----------------------------------------------------------------------------
// read_tile_snr(): 读取 SNR 控制点
// SNR 子块解压后布局:
//   n_points (uint32) + points[n_points × 8B (local_ipix u32 + snr f32)]
//   + snr_phot (f64) + median_snr (f64) + idw_power (f64)
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

    // 解析 SNR 数据
    // 最小长度: n_points(4) + 3 scalars(24) = 28
    if (raw.size() < 28) {
        fprintf(stderr, "[hiss][reader] SNR 数据过短: %zu\n", raw.size());
        return -4;
    }

    uint32_t n_points = read_u32_le(raw.data());
    size_t expected = 4 + (size_t)n_points * 8 + 24;
    if (raw.size() != expected) {
        fprintf(stderr, "[hiss][reader] SNR 数据长度不匹配: got=%zu expected=%zu (n_points=%u)\n",
                raw.size(), expected, n_points);
        return -4;
    }

    // 解析控制点
    snr.points.resize(n_points);
    const uint8_t* p = raw.data() + 4;
    for (uint32_t i = 0; i < n_points; i++) {
        snr.points[i].local_ipix = read_u32_le(p);
        snr.points[i].snr        = read_f32_le(p + 4);
        p += 8;
    }

    // 解析 3 个标量
    snr.snr_phot   = read_f64_le(p); p += 8;
    snr.median_snr = read_f64_le(p); p += 8;
    snr.idw_power  = read_f64_le(p);

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
        // 计算 local_ipix 之前有多少个有效像素 (即数组索引)
        size_t arr_idx = 0;
        for (size_t b = 0; b < byte_idx; b++) {
            // popcount of byte
            uint8_t v = occ_raw[b];
            while (v) { arr_idx += (v & 1); v >>= 1; }
        }
        // 当前字节中 bit_idx 之前的有效位
        for (size_t b = 0; b < bit_idx; b++) {
            arr_idx += (occ_raw[byte_idx] >> b) & 1;
        }
        if (arr_idx < sig_arr.size()) {
            if (signal)  *signal = sig_arr[arr_idx];
        } else {
            if (signal)  *signal = 0.0f;
        }
        if (arr_idx < sup_arr.size()) {
            if (support) *support = sup_arr[arr_idx];
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
        ret = impl.read_subblock(*occ_desc, occ_raw);
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
            if (v == (uint32_t)local_ipix && lo < sig_arr.size()) {
                if (signal)  *signal = sig_arr[lo];
                if (support) *support = (lo < sup_arr.size()) ? sup_arr[lo] : 0;
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
    impl.version = 0;
    impl.header_offset = 0;
    impl.grid = HissGridSpec{};
    impl.metadata = HissMetadata{};
    impl.tiles.clear();
    impl.tile_index.clear();
}

} // namespace hiss
