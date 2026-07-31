// ============================================================================
// hiss_writer.cpp - AstroCS HISS Writer (XISF 式 Header + attachments 单体容器)
//
// 规范依据: 02_FROZEN §14/§15
//   - 固定签名块 (20B) → Header (网格规格 + 元数据 JSON + Tile 目录) → Attachment 子块
//   - Header 是唯一权威目录; 不使用 Footer/Checkpoint/断点续写
//   - 写入流程: open 写签名块占位 → add_tile 流式生成子块池(内存) →
//     finalize 计算子块 offset, 重写签名块 + Header + 子块, flush, 原子重命名
//   - 每子块目录项独立记录: type, flags, offset, compressed/uncompressed size,
//     codec_id, transform_id, checksum_type, checksum
//   - RAW codec 必须可用; 其他 codec 通过 CodecRegistry 接入
//   - 同一 HISS 中允许不同 Tile/子块使用不同 codec/transform
//
// 注: 下列共享方法已移至 hiss_common.cpp (避免与 Reader 重复定义):
//   - compute_tile_depth / compute_tile_nside (02_FROZEN §11)
//   - DrizzleTileAccumulator::finalize_signal / finalize_support / validate_support (§10)
//   - HissMetadata::to_json / from_json (§16)
// ============================================================================
#include "hiss_format.h"

#include <cstdio>      // fprintf, std::fopen/fclose/fwrite/fseek/ftell
#include <cerrno>      // errno
#include <cstring>     // std::memcpy, std::strncpy
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <fstream>     // std::ofstream
#include <filesystem>  // C++17 原子重命名

namespace hiss {

// ============================================================================
// 内部常量
// ============================================================================

// 固定签名块: MAGIC(8) + version(4) + header_offset(8) = 20 字节
static const char     HISS_MAGIC[8] = { 'A','C','S','H','I','S','S','\0' };
static const uint32_t HISS_VERSION  = 1;
static const uint64_t HISS_SIGNATURE_SIZE = 20;

// 子块描述符固定字节数 (写入 Header 时每项大小)
// type(1) + flags(2) + offset(8) + compressed_size(8) + uncompressed_size(8) +
// codec_id(2) + transform_id(2) + checksum_type(1) + checksum(8) = 40
static const size_t HISS_SUBBLOCK_DESCRIPTOR_SIZE = 40;

// Tile 目录固定前缀字节数 (不含子块描述符)
// parent_ipix(8) + tile_nside(4) + occ_mode(1) + subblock_count(2) = 15
static const size_t HISS_TILE_DIR_PREFIX_SIZE = 15;

// ============================================================================
// 内部辅助: 小端字节追加工具 (用于构建 Header 字节流)
// ============================================================================
// 注: JSON 转义/序列化辅助已移至 hiss_common.cpp (由 HissMetadata::to_json 使用)

struct ByteBuf {
    std::vector<uint8_t> data;

    void u8 (uint8_t v)            { data.push_back(v); }
    void u16(uint16_t v)           { size_t n = data.size(); data.resize(n + 2); std::memcpy(data.data() + n, &v, 2); }
    void u32(uint32_t v)           { size_t n = data.size(); data.resize(n + 4); std::memcpy(data.data() + n, &v, 4); }
    void u64(uint64_t v)           { size_t n = data.size(); data.resize(n + 8); std::memcpy(data.data() + n, &v, 8); }
    void f64(double v)             { size_t n = data.size(); data.resize(n + 8); std::memcpy(data.data() + n, &v, 8); }
    void bytes(const void* p, size_t n) {
        size_t off = data.size();
        data.resize(off + n);
        if (n > 0) std::memcpy(data.data() + off, p, n);
    }
};

// ============================================================================
// HissWriter 实现
// ============================================================================
// 注: compute_tile_depth/nside, DrizzleTileAccumulator::finalize_signal/
// finalize_support/validate_support, HissMetadata::to_json/from_json
// 共 7 个共享方法已移至 hiss_common.cpp (与 Reader 共用, 避免重复定义)

// 待写入子块 (压缩后数据 + 描述符), finalize 前在内存中暂存
struct PendingSubblock {
    HissSubblockDescriptor  desc;
    std::vector<uint8_t>    compressed_data;
};

// 待写入 Tile (元信息 + 子块列表)
struct PendingTile {
    uint64_t                          parent_ipix = 0;
    uint32_t                          tile_nside  = 0;
    OccupancyMode                     occ_mode    = OccupancyMode::FULL;
    std::vector<PendingSubblock>      subblocks;
};

// HissWriter 私有实现
struct HissWriter::Impl {
    std::string                 output_path;     // 最终 .hiss 路径
    std::string                 partial_path;    // 临时 .partial 路径
    HissGridSpec                grid;            // 网格规格
    HissMetadata                metadata;        // 元数据
    std::vector<PendingTile>    tiles;           // 待写入 Tile 列表
    bool                        opened = false;  // 会话是否打开

    // 实验性 codec 配置 (按 SubblockType 区分), 默认 RAW/NONE
    std::map<SubblockType, std::pair<CodecId, TransformId>> experiment_codecs;

    CodecId codec_for(SubblockType t) const {
        auto it = experiment_codecs.find(t);
        return it != experiment_codecs.end() ? it->second.first : CodecId::RAW;
    }
    TransformId transform_for(SubblockType t) const {
        auto it = experiment_codecs.find(t);
        return it != experiment_codecs.end() ? it->second.second : TransformId::NONE;
    }
};

// ---------------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------------

HissWriter::HissWriter() : pimpl_(std::make_unique<Impl>()) {
    fprintf(stderr, "[hiss][writer] HissWriter 构造\n");
}

HissWriter::~HissWriter() {
    // 析构时若会话仍打开, 自动清理临时文件 (不重命名, 视为放弃)
    if (pimpl_ && pimpl_->opened) {
        fprintf(stderr, "[hiss][writer] 析构时会话仍打开, 自动 cancel\n");
        cancel();
    }
}

// ---------------------------------------------------------------------------
// open: 初始化写入会话
// ---------------------------------------------------------------------------

int HissWriter::open(const std::string& output_path,
                     const HissGridSpec& grid,
                     const HissMetadata& metadata) {
    if (pimpl_->opened) {
        fprintf(stderr, "[hiss][writer] open 失败: 已有会话进行中\n");
        return -1;
    }
    if (grid.nside == 0) {
        fprintf(stderr, "[hiss][writer] open 失败: nside=0 非法\n");
        return -2;
    }

    pimpl_->output_path  = output_path;
    pimpl_->partial_path = output_path + ".partial";
    pimpl_->grid         = grid;
    pimpl_->metadata     = metadata;
    pimpl_->tiles.clear();

    // 创建临时文件 (二进制写, "wb" 截断已存在的 .partial)
    FILE* fp = std::fopen(pimpl_->partial_path.c_str(), "wb");
    if (!fp) {
        fprintf(stderr, "[hiss][writer] open 失败: 无法创建临时文件 %s (errno=%d)\n",
                pimpl_->partial_path.c_str(), errno);
        return -3;
    }

    // 写入签名块占位 (20B: MAGIC + version + header_offset=0)
    // header_offset 在 finalize 时回填为 20 (Header 紧跟签名块)
    uint8_t sig[20] = {0};
    std::memcpy(sig, HISS_MAGIC, 8);
    uint32_t ver = HISS_VERSION;
    std::memcpy(sig + 8, &ver, 4);
    uint64_t header_offset = 0;  // 占位
    std::memcpy(sig + 12, &header_offset, 8);
    if (std::fwrite(sig, 1, HISS_SIGNATURE_SIZE, fp) != HISS_SIGNATURE_SIZE) {
        fprintf(stderr, "[hiss][writer] open 失败: 写入签名块失败\n");
        std::fclose(fp);
        return -4;
    }
    std::fclose(fp);

    pimpl_->opened = true;
    fprintf(stderr,
            "[hiss][writer] open 成功: path=%s nside=%u tile_nside=%u ordering=%d pixfrac=%g\n",
            output_path.c_str(), grid.nside, grid.tile_nside, grid.ordering, grid.pixfrac);
    return 0;
}

// ---------------------------------------------------------------------------
// 内部辅助: 压缩并构建待写入子块
//   返回 0=成功, <0=失败; 成功时填充 pending.desc 和 pending.compressed_data
// ---------------------------------------------------------------------------

static int build_pending_subblock(SubblockType type,
                                  uint16_t flags,
                                  const uint8_t* data, size_t data_size,
                                  CodecId codec_id, TransformId transform_id,
                                  PendingSubblock& pending) {
    // 查找 codec
    const CodecEntry* entry = CodecRegistry::instance().find(codec_id);
    if (!entry) {
        fprintf(stderr, "[hiss][writer] build_subblock: codec id=%u 未注册\n", (unsigned)codec_id);
        return -1;
    }

    // 分配压缩缓冲区并压缩
    size_t bound = entry->bound(data_size);
    std::vector<uint8_t> compressed(bound);
    size_t compressed_size = bound;
    int ret = entry->compress(data, data_size, compressed.data(), &compressed_size);
    if (ret != 0) {
        fprintf(stderr, "[hiss][writer] build_subblock: 压缩失败 ret=%d (codec=%u type=%u)\n",
                ret, (unsigned)codec_id, (unsigned)type);
        return -2;
    }
    compressed.resize(compressed_size);

    // 填充描述符 (offset 在 finalize 时回填)
    pending.desc.type             = type;
    pending.desc.flags            = flags;
    pending.desc.offset           = 0;  // finalize 时回填
    pending.desc.compressed_size  = compressed_size;
    pending.desc.uncompressed_size= data_size;
    pending.desc.codec_id         = codec_id;
    pending.desc.transform_id     = transform_id;
    pending.desc.checksum_type    = ChecksumType::NONE;  // 默认 NONE, 实验时可扩展
    pending.desc.checksum         = 0;
    pending.compressed_data       = std::move(compressed);

    fprintf(stderr,
            "[hiss][writer]   子块 type=%u flags=0x%04x uncompressed=%zu compressed=%zu codec=%u transform=%u\n",
            (unsigned)type, flags, data_size, compressed_size,
            (unsigned)codec_id, (unsigned)transform_id);
    return 0;
}

// ---------------------------------------------------------------------------
// add_tile: 添加一个 Tile 的数据
//   生成 occupancy(可选) / signal / support / snr(可选) 子块
// ---------------------------------------------------------------------------

int HissWriter::add_tile(uint64_t parent_ipix,
                         const DrizzleTileAccumulator& acc,
                         const HissSnrBlock* snr,
                         OccupancyMode occ_mode) {
    if (!pimpl_->opened) {
        fprintf(stderr, "[hiss][writer] add_tile 失败: 会话未打开\n");
        return -1;
    }
    if (acc.tile_nside == 0) {
        fprintf(stderr, "[hiss][writer] add_tile 失败: acc.tile_nside=0 非法\n");
        return -2;
    }

    PendingTile tile;
    tile.parent_ipix = parent_ipix;
    tile.tile_nside  = acc.tile_nside;
    tile.occ_mode    = occ_mode;

    const size_t n_leaf = acc.pixels.size();

    // 1. 生成 signal (float32) 与 support (uint8)
    std::vector<float>   signal;
    std::vector<uint8_t> support;
    acc.finalize_signal(signal);
    acc.finalize_support(support);

    // 2. occupancy 子块 (FULL 时省略; BITMAP/SPARSE_LIST 时生成)
    if (occ_mode != OccupancyMode::FULL) {
        std::vector<uint8_t> occ_data;
        if (occ_mode == OccupancyMode::BITMAP) {
            // 1 bit/叶像素, LSB 优先 (bit 0 = 像素 0)
            occ_data.assign((n_leaf + 7) / 8, 0);
            for (size_t i = 0; i < n_leaf; i++) {
                if (acc.pixels[i].sum_area > 0.0) {
                    occ_data[i / 8] |= (uint8_t)(1u << (i % 8));
                }
            }
        } else {  // SPARSE_LIST: 有效像素局部索引 (uint32 数组)
            for (size_t i = 0; i < n_leaf; i++) {
                if (acc.pixels[i].sum_area > 0.0) {
                    uint32_t idx = (uint32_t)i;
                    size_t off = occ_data.size();
                    occ_data.resize(off + 4);
                    std::memcpy(occ_data.data() + off, &idx, 4);
                }
            }
        }
        PendingSubblock ps;
        int ret = build_pending_subblock(
            SubblockType::OCCUPANCY,
            (uint16_t)SubblockFlags::OPTIONAL,
            occ_data.data(), occ_data.size(),
            pimpl_->codec_for(SubblockType::OCCUPANCY),
            pimpl_->transform_for(SubblockType::OCCUPANCY),
            ps);
        if (ret != 0) return ret;
        tile.subblocks.push_back(std::move(ps));
    }

    // 3. signal 子块 (必需)
    {
        size_t bytes = signal.size() * sizeof(float);
        PendingSubblock ps;
        int ret = build_pending_subblock(
            SubblockType::SIGNAL,
            (uint16_t)SubblockFlags::REQUIRED,
            (const uint8_t*)signal.data(), bytes,
            pimpl_->codec_for(SubblockType::SIGNAL),
            pimpl_->transform_for(SubblockType::SIGNAL),
            ps);
        if (ret != 0) return ret;
        tile.subblocks.push_back(std::move(ps));
    }

    // 4. support 子块 (必需)
    {
        PendingSubblock ps;
        int ret = build_pending_subblock(
            SubblockType::SUPPORT,
            (uint16_t)SubblockFlags::REQUIRED,
            support.data(), support.size(),
            pimpl_->codec_for(SubblockType::SUPPORT),
            pimpl_->transform_for(SubblockType::SUPPORT),
            ps);
        if (ret != 0) return ret;
        tile.subblocks.push_back(std::move(ps));
    }

    // 5. SNR 子块 (可选, snr != nullptr 时生成)
    //    二进制布局: snr_phot(8) + median_snr(8) + idw_power(8) + n_points(4) +
    //                n_points * (local_ipix(4) + snr(4))
    if (snr) {
        std::vector<uint8_t> snr_data;
        snr_data.resize(28);  // 3*8 + 4
        std::memcpy(snr_data.data() + 0,  &snr->snr_phot,   8);
        std::memcpy(snr_data.data() + 8,  &snr->median_snr, 8);
        std::memcpy(snr_data.data() + 16, &snr->idw_power,  8);
        uint32_t n_points = (uint32_t)snr->points.size();
        std::memcpy(snr_data.data() + 24, &n_points, 4);
        for (const auto& p : snr->points) {
            size_t off = snr_data.size();
            snr_data.resize(off + 8);
            std::memcpy(snr_data.data() + off,     &p.local_ipix, 4);
            std::memcpy(snr_data.data() + off + 4, &p.snr,        4);
        }
        PendingSubblock ps;
        int ret = build_pending_subblock(
            SubblockType::SNR,
            (uint16_t)SubblockFlags::OPTIONAL,
            snr_data.data(), snr_data.size(),
            pimpl_->codec_for(SubblockType::SNR),
            pimpl_->transform_for(SubblockType::SNR),
            ps);
        if (ret != 0) return ret;
        tile.subblocks.push_back(std::move(ps));
    }

    size_t n_sb = tile.subblocks.size();
    pimpl_->tiles.push_back(std::move(tile));
    fprintf(stderr,
            "[hiss][writer] add_tile 成功: parent_ipix=%llu tile_nside=%u occ_mode=%u n_leaf=%zu n_subblocks=%zu\n",
            (unsigned long long)parent_ipix, acc.tile_nside, (unsigned)occ_mode, n_leaf, n_sb);
    return 0;
}

// ---------------------------------------------------------------------------
// finalize: 生成 Header, 组装 .partial, flush, 原子重命名
//   最终布局: 签名块(20B) → Header → 子块1 → 子块2 → ...
// ---------------------------------------------------------------------------

int HissWriter::finalize() {
    if (!pimpl_->opened) {
        fprintf(stderr, "[hiss][writer] finalize 失败: 会话未打开\n");
        return -1;
    }

    // 1. 计算元数据 JSON
    std::string json = pimpl_->metadata.to_json();

    // 2. 计算 Header 大小
    //    Header = 网格规格(24) + json_len(4) + json(变长) + tile_count(4) +
    //             Σ(15 + 40 * n_subblocks)
    size_t header_size = 24 + 4 + json.size() + 4;
    for (const auto& t : pimpl_->tiles) {
        header_size += HISS_TILE_DIR_PREFIX_SIZE +
                       HISS_SUBBLOCK_DESCRIPTOR_SIZE * t.subblocks.size();
    }

    // 3. 计算每个子块的 offset 并回填 desc.offset
    //    第一个子块 offset = 签名块(20) + Header 大小
    uint64_t cur_offset = HISS_SIGNATURE_SIZE + header_size;
    for (auto& t : pimpl_->tiles) {
        for (auto& sb : t.subblocks) {
            sb.desc.offset = cur_offset;
            cur_offset += sb.desc.compressed_size;
        }
    }

    // 4. 构建 Header 字节流
    ByteBuf hdr;
    // a. 网格规格
    hdr.u32(pimpl_->grid.nside);
    hdr.u32(pimpl_->grid.tile_nside);
    hdr.u32((uint32_t)pimpl_->grid.ordering);
    hdr.u32((uint32_t)pimpl_->grid.radesys);
    hdr.f64(pimpl_->grid.pixfrac);
    // b. 元数据 JSON
    hdr.u32((uint32_t)json.size());
    if (!json.empty()) hdr.bytes(json.data(), json.size());
    // c. Tile 数量
    hdr.u32((uint32_t)pimpl_->tiles.size());
    // d. 每个 Tile 的目录
    for (const auto& t : pimpl_->tiles) {
        hdr.u64(t.parent_ipix);
        hdr.u32(t.tile_nside);
        hdr.u8((uint8_t)t.occ_mode);
        hdr.u16((uint16_t)t.subblocks.size());
        for (const auto& sb : t.subblocks) {
            hdr.u8 ((uint8_t)sb.desc.type);
            hdr.u16(sb.desc.flags);
            hdr.u64(sb.desc.offset);
            hdr.u64(sb.desc.compressed_size);
            hdr.u64(sb.desc.uncompressed_size);
            hdr.u16((uint16_t)sb.desc.codec_id);
            hdr.u16((uint16_t)sb.desc.transform_id);
            hdr.u8 ((uint8_t)sb.desc.checksum_type);
            hdr.u64(sb.desc.checksum);
        }
    }

    if (hdr.data.size() != header_size) {
        fprintf(stderr,
                "[hiss][writer] finalize 警告: Header 实际大小 %zu 与预算 %zu 不符 (内部 bug)\n",
                hdr.data.size(), header_size);
        // 继续用实际大小
        header_size = hdr.data.size();
    }

    // 5. 重写 .partial 文件: 签名块(带 header_offset) + Header + 所有子块
    FILE* fp = std::fopen(pimpl_->partial_path.c_str(), "r+b");
    if (!fp) {
        fprintf(stderr, "[hiss][writer] finalize 失败: 无法打开 .partial %s\n",
                pimpl_->partial_path.c_str());
        return -2;
    }

    // 5a. 回到文件开头, 重写签名块 (header_offset = 20, Header 紧跟签名块)
    if (std::fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "[hiss][writer] finalize 失败: fseek(0) 失败\n");
        std::fclose(fp);
        return -3;
    }
    uint8_t sig[20] = {0};
    std::memcpy(sig, HISS_MAGIC, 8);
    uint32_t ver = HISS_VERSION;
    std::memcpy(sig + 8, &ver, 4);
    uint64_t header_offset = HISS_SIGNATURE_SIZE;  // Header 紧跟签名块
    std::memcpy(sig + 12, &header_offset, 8);
    if (std::fwrite(sig, 1, HISS_SIGNATURE_SIZE, fp) != HISS_SIGNATURE_SIZE) {
        fprintf(stderr, "[hiss][writer] finalize 失败: 重写签名块失败\n");
        std::fclose(fp);
        return -4;
    }

    // 5b. 写入 Header
    if (!hdr.data.empty()) {
        if (std::fwrite(hdr.data.data(), 1, hdr.data.size(), fp) != hdr.data.size()) {
            fprintf(stderr, "[hiss][writer] finalize 失败: 写入 Header 失败\n");
            std::fclose(fp);
            return -5;
        }
    }

    // 5c. 写入所有子块 (按 Tile 顺序)
    for (const auto& t : pimpl_->tiles) {
        for (const auto& sb : t.subblocks) {
            if (sb.compressed_data.empty()) continue;
            if (std::fwrite(sb.compressed_data.data(), 1, sb.compressed_data.size(), fp)
                != sb.compressed_data.size()) {
                fprintf(stderr, "[hiss][writer] finalize 失败: 写入子块数据失败\n");
                std::fclose(fp);
                return -6;
            }
        }
    }

    // 6. flush + 关闭
    if (std::fflush(fp) != 0) {
        fprintf(stderr, "[hiss][writer] finalize 警告: fflush 失败 (errno=%d)\n", errno);
    }
    std::fclose(fp);

    // 7. 原子重命名 .partial → 最终路径
    std::error_code ec;
    std::filesystem::rename(pimpl_->partial_path, pimpl_->output_path, ec);
    if (ec) {
        // Windows 上目标已存在时 rename 可能失败, 尝试先删除目标再重命名
        std::error_code ec2;
        std::filesystem::remove(pimpl_->output_path, ec2);
        std::filesystem::rename(pimpl_->partial_path, pimpl_->output_path, ec);
        if (ec) {
            fprintf(stderr,
                    "[hiss][writer] finalize 失败: 重命名失败 %s -> %s: %s\n",
                    pimpl_->partial_path.c_str(), pimpl_->output_path.c_str(),
                    ec.message().c_str());
            pimpl_->opened = false;
            return -7;
        }
    }

    pimpl_->opened = false;
    fprintf(stderr,
            "[hiss][writer] finalize 成功: tiles=%zu header_offset=%llu header_size=%zu total_size=%llu path=%s\n",
            pimpl_->tiles.size(),
            (unsigned long long)header_offset,
            header_size,
            (unsigned long long)cur_offset,
            pimpl_->output_path.c_str());
    return 0;
}

// ---------------------------------------------------------------------------
// cancel: 关闭并删除临时文件, 清理内存
// ---------------------------------------------------------------------------

void HissWriter::cancel() {
    if (pimpl_->opened) {
        fprintf(stderr, "[hiss][writer] cancel: 清理临时文件 %s\n",
                pimpl_->partial_path.c_str());
    }
    // 删除 .partial 临时文件 (若存在)
    if (!pimpl_->partial_path.empty()) {
        std::error_code ec;
        std::filesystem::remove(pimpl_->partial_path, ec);
    }
    pimpl_->tiles.clear();
    pimpl_->experiment_codecs.clear();
    pimpl_->opened = false;
    pimpl_->output_path.clear();
    pimpl_->partial_path.clear();
}

// ---------------------------------------------------------------------------
// set_experiment_codec: 设置实验性 codec/transform (按 SubblockType)
//   默认: 所有子块 RAW/NONE; 调用后指定 type 使用 codec/transform
// ---------------------------------------------------------------------------

void HissWriter::set_experiment_codec(SubblockType type,
                                      CodecId codec,
                                      TransformId transform) {
    pimpl_->experiment_codecs[type] = std::make_pair(codec, transform);
    fprintf(stderr,
            "[hiss][writer] set_experiment_codec: type=%u codec=%u transform=%u\n",
            (unsigned)type, (unsigned)codec, (unsigned)transform);
}

} // namespace hiss
