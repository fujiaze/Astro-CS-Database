// ============================================================================
// hiss_writer.cpp - AstroCS HISS Writer (XISF 式 Header + attachments 单体容器)
//
// 规范依据: 02_FROZEN §14/§15, 00_COMMON_CONTRACTS §2.4/§4.5
//   - 固定签名块 (20B) → Header (网格规格 + 元数据 JSON + Tile 目录) → Attachment 子块
//   - Header 是唯一权威目录; 不使用 Footer/Checkpoint/断点续写
//   - 流式写入: 每个 Tile 压缩后立即写入临时池 (HissStreamWriter),
//     内存只保留 SubblockDescriptor, 不保留 compressed_data (步骤10)
//   - occupancy 模式自动选择 (步骤11): Writer 根据占用率选择 FULL/BITMAP/SPARSE_LIST,
//     不由调用方传入
//   - BITMAP/SPARSE 模式只保存有效像素的 signal/support (步骤11)
//   - signal = 累计通量 (步骤7, finalize_signal 在 hiss_common.cpp 已修复)
//   - 元数据不保存完整 WCS/SIP (步骤8)
//
// 注: 下列共享方法已移至 hiss_common.cpp (避免与 Reader 重复定义):
//   - compute_tile_depth / compute_tile_nside (02_FROZEN §11)
//   - DrizzleTileAccumulator::finalize_signal / finalize_support / validate_support (§10)
//   - HissMetadata::to_json / from_json (§16)
// ============================================================================
#include "hiss_format.h"
#include "hiss_stream_writer.h"
#include "hiss_tile_model.h"  // make_tile_geometry (用于自动选择时的 n_leaf_per_tile)
#include "hiss_transform.h"   // WP-G 步骤12: apply_transform (压缩前正向变换)

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <algorithm>

namespace hiss {

// ============================================================================
// 内部常量
// ============================================================================

// R04-B12: occupancy 自动选择 — 不再使用硬编码阈值
//   FULL 仅当 n_valid == n_leaf_per_tile (100% 覆盖, 02_FROZEN §9)
//   BITMAP/SPARSE_LIST 按实际完整编码大小自动选最小者, 确定性 tie-break
//   不冻结经验阈值 (DQ-005 由实验决定, 但生产路径不依赖固定阈值)

// ============================================================================
// HissWriter 私有实现
// ============================================================================

struct HissWriter::Impl {
    HissGridSpec                grid;
    HissMetadata                metadata;
    HissStreamWriter            stream;   // 流式写入器 (管理临时池)
    bool                        opened = false;

    // 实验性 codec 配置 (按 SubblockType 区分), 默认 RAW/NONE
    std::map<SubblockType, std::pair<CodecId, TransformId>> experiment_codecs;

    // 实验性 checksum 配置 (按 SubblockType 区分), 默认 NONE
    // INTERIM_BASELINE_NOT_FROZEN: 候选注册机制, 算法待 DQ-006 冻结
    std::map<SubblockType, ChecksumType> experiment_checksums;

    CodecId codec_for(SubblockType t) const {
        auto it = experiment_codecs.find(t);
        return it != experiment_codecs.end() ? it->second.first : CodecId::RAW;
    }
    TransformId transform_for(SubblockType t) const {
        auto it = experiment_codecs.find(t);
        return it != experiment_codecs.end() ? it->second.second : TransformId::NONE;
    }
    ChecksumType checksum_for(SubblockType t) const {
        auto it = experiment_checksums.find(t);
        return it != experiment_checksums.end() ? it->second : ChecksumType::NONE;
    }
};

// ---------------------------------------------------------------------------
// 内部辅助: 压缩子块并追加到流式写入器
//   返回 0=成功, <0=失败; 成功时 desc 已填充 (offset/size/codec 等)
//   依据步骤10: 压缩后立即写入临时池, 不保留 compressed_data 在内存
//   依据步骤12 (WP-G): 在压缩前执行 apply_transform (若 transform_id != NONE)
//   checksum (INTERIM_BASELINE_NOT_FROZEN):
//     - 默认 ChecksumType::NONE (不计算校验, 向后兼容)
//     - 实验时通过 set_experiment_checksum 启用 CRC32C 等候选算法
//     - checksum 计算的是压缩后数据 (与 Reader 端一致, Reader 在解压前校验)
// ---------------------------------------------------------------------------

static int compress_and_append(HissStreamWriter& stream,
                                SubblockType type,
                                uint16_t flags,
                                const uint8_t* data, size_t data_size,
                                CodecId codec_id, TransformId transform_id,
                                ChecksumType checksum_type,
                                size_t element_size,
                                HissSubblockDescriptor& desc) {
    // WP-G 步骤12: 在压缩前执行正向变换 (若 transform_id != NONE)
    // 变换后的数据作为压缩输入, uncompressed_size 记录变换后大小
    std::vector<uint8_t> transformed;
    const uint8_t* data_to_compress = data;
    size_t size_to_compress = data_size;

    if (transform_id != TransformId::NONE) {
        TransformType tt = transform_id_to_type(transform_id);
        transformed = apply_transform(tt, data, data_size, element_size);

        // 校验: 非空输入变换后不应为空 (空输入的 DELTA_VARINT 输出 4 字节, 不为空)
        if (transformed.empty() && data_size > 0) {
            fprintf(stderr,
                    "[hiss][writer] compress_and_append: transform 失败 type=%s(%u) "
                    "element_size=%zu data_size=%zu\n",
                    transform_type_name(tt), (unsigned)transform_id,
                    element_size, data_size);
            return -1;
        }
        // 空输入 + DELTA_VARINT: transformed 为 4 字节 [0,0,0,0], data_size=0
        // 这是有效情况, 继续处理

        data_to_compress = transformed.data();
        size_to_compress = transformed.size();

        fprintf(stderr,
                "[hiss][writer]   transform %s: %zu → %zu 字节 (element_size=%zu)\n",
                transform_type_name(tt), data_size, size_to_compress, element_size);
    }

    // R04-B17: 内置子块 ext_type_id = 0 (非 EXTENSION 类型)
    desc.type              = type;
    desc.ext_type_id       = (uint16_t)ExtensionNamespace::BUILTIN;  // 0=内置
    desc.flags             = flags;
    desc.offset            = 0;  // append_subblock 回填
    desc.uncompressed_size = size_to_compress;  // WP-G: 变换后大小
    desc.transform_id      = transform_id;
    desc.checksum_type     = ChecksumType::NONE;  // 默认 NONE, 下方按 checksum_type 覆盖
    desc.checksum          = 0;

    // R04-B13: RAW 回退 — 若 codec != RAW, 压缩后若无收益则真实回退 RAW
    //   判据: compressed_size >= size_to_compress (压缩后不小于原始)
    //   回退: codec_id=RAW, compressed_size=size_to_compress, 写入原始数据
    const uint8_t* final_data = data_to_compress;
    size_t final_size = size_to_compress;
    CodecId final_codec = codec_id;

    if (codec_id != CodecId::RAW) {
        // 查找 codec
        const CodecEntry* entry = CodecRegistry::instance().find(codec_id);
        if (!entry) {
            fprintf(stderr, "[hiss][writer] compress_and_append: codec id=%u 未注册\n",
                    (unsigned)codec_id);
            return -1;
        }

        // 分配压缩缓冲区并压缩 (基于变换后大小)
        size_t bound = entry->bound(size_to_compress);
        std::vector<uint8_t> compressed(bound);
        size_t compressed_size = bound;
        int ret = entry->compress(data_to_compress, size_to_compress,
                                   compressed.data(), &compressed_size);
        if (ret != 0) {
            fprintf(stderr, "[hiss][writer] compress_and_append: 压缩失败 ret=%d (codec=%u type=%u)\n",
                    ret, (unsigned)codec_id, (unsigned)type);
            return -2;
        }

        // R04-B13: 判断压缩是否有收益
        if (compressed_size >= size_to_compress) {
            // 压缩无收益 → 回退 RAW
            fprintf(stderr,
                    "[hiss][writer]   R04-B13 RAW 回退: compressed=%zu >= uncompressed=%zu → 使用 RAW\n",
                    compressed_size, size_to_compress);
            final_data  = data_to_compress;
            final_size  = size_to_compress;
            final_codec = CodecId::RAW;
            // compressed 在此分支结束后析构, 释放内存
        } else {
            // 压缩有收益 → 使用压缩数据
            final_data  = compressed.data();
            final_size  = compressed_size;
            final_codec = codec_id;
        }
    }
    // codec_id == RAW 时, final_data/final_size/final_codec 已正确设置

    desc.compressed_size = final_size;
    desc.codec_id        = final_codec;

    // checksum 计算 (INTERIM_BASELINE_NOT_FROZEN)
    // 计算的是压缩后数据 (与 Reader 端一致, Reader 在解压前校验 comp.data())
    if (checksum_type != ChecksumType::NONE) {
        const ChecksumEntry* cs = ChecksumRegistry::instance().find(checksum_type);
        if (!cs) {
            fprintf(stderr,
                    "[hiss][writer] compress_and_append: checksum id=%u 未注册 (type=%u)\n",
                    (unsigned)checksum_type, (unsigned)type);
            return -4;
        }
        desc.checksum      = cs->compute(final_data, final_size);
        desc.checksum_type = checksum_type;
        fprintf(stderr,
                "[hiss][writer]   checksum %s: type=%u value=0x%016llx (comp_size=%zu)\n",
                cs->name.c_str(), (unsigned)checksum_type,
                (unsigned long long)desc.checksum, final_size);
    }

    // 立即追加到流式写入器 (写入临时池), 释放 compressed 内存
    int ret = stream.append_subblock(final_data, final_size, desc);
    if (ret != 0) {
        fprintf(stderr, "[hiss][writer] compress_and_append: append_subblock 失败 ret=%d\n", ret);
        return -3;
    }

    fprintf(stderr,
            "[hiss][writer]   子块 type=%u ext_id=%u flags=0x%04x uncompressed=%zu compressed=%zu "
            "codec=%u transform=%u checksum_type=%u offset=%llu\n",
            (unsigned)type, (unsigned)desc.ext_type_id, flags, size_to_compress, final_size,
            (unsigned)final_codec, (unsigned)transform_id,
            (unsigned)desc.checksum_type,
            (unsigned long long)desc.offset);
    return 0;
}

// ---------------------------------------------------------------------------
// 内部辅助: 自动选择 occupancy 模式 (R04-B12)
//   FULL 仅当 n_valid == n_leaf_per_tile (100% 覆盖, 02_FROZEN §9)
//   否则按实际完整编码大小自动选 BITMAP 或 SPARSE_LIST 的最小者
//   确定性 tie-break: 大小相等时选 BITMAP (解码更简单, 无需二分查找)
//   不冻结经验阈值 (R04-B12 红线)
//
//   BITMAP 完整编码大小 = ceil(n_leaf_per_tile / 8) 字节
//   SPARSE_LIST 完整编码大小 = n_valid * sizeof(uint32_t) 字节
// ---------------------------------------------------------------------------

static OccupancyMode auto_select_occupancy(uint32_t n_valid, uint32_t n_leaf_per_tile) {
    if (n_leaf_per_tile == 0) return OccupancyMode::FULL;
    // FULL 仅当 100% 覆盖 (02_FROZEN §9: 不能使用 80% 等近似阈值)
    if (n_valid == n_leaf_per_tile) {
        return OccupancyMode::FULL;
    }
    // R04-B12: 按实际完整编码大小自动选最小者
    size_t bitmap_size  = (size_t)(n_leaf_per_tile + 7) / 8;       // 位图: 1 bit/叶像素
    size_t sparse_size  = (size_t)n_valid * sizeof(uint32_t);      // 索引列表: 4 字节/有效像素
    if (sparse_size < bitmap_size) {
        return OccupancyMode::SPARSE_LIST;
    }
    // tie-break: 大小相等时选 BITMAP (解码更简单)
    return OccupancyMode::BITMAP;
}

// ---------------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------------

HissWriter::HissWriter() : pimpl_(std::make_unique<Impl>()) {
    fprintf(stderr, "[hiss][writer] HissWriter 构造\n");
}

HissWriter::~HissWriter() {
    if (pimpl_ && pimpl_->opened) {
        fprintf(stderr, "[hiss][writer] 析构时会话仍打开, 自动 cancel\n");
        cancel();
    }
}

// ---------------------------------------------------------------------------
// open: 初始化写入会话 (委托给 HissStreamWriter)
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

    // WP-C 步骤9: 测光元数据一致性校验 (02_FROZEN §7)
    {
        const std::string bunit_str(metadata.bunit);
        const bool is_relative_flux = (bunit_str == "ASTROCS_RELATIVE_FLUX");
        const bool photappl = (metadata.photappl != 0);

        if (is_relative_flux && !photappl) {
            fprintf(stderr,
                    "[hiss][writer] open 失败: 元数据不一致 - BUNIT=%s 但 PHOTAPPL=FALSE "
                    "(PHOTSCAL=%.6f)。BUNIT=ASTROCS_RELATIVE_FLUX 要求 signal 已应用 Gaia 测光校准 "
                    "(PHOTAPPL=TRUE) (HISS_ERR_INVALID_STATE)\n",
                    metadata.bunit, metadata.photscal);
            return -2;  // HISS_ERR_INVALID_STATE
        }

        fprintf(stderr,
                "[hiss][writer] 测光元数据: PHOTAPPL=%d PHOTSCAL=%.6f BUNIT=%s (一致性 OK)\n",
                metadata.photappl, metadata.photscal, metadata.bunit);

        if (photappl && !is_relative_flux) {
            fprintf(stderr,
                    "[hiss][writer] 警告: PHOTAPPL=TRUE 但 BUNIT=%s (建议改为 ASTROCS_RELATIVE_FLUX)\n",
                    metadata.bunit);
        }
    }

    pimpl_->grid     = grid;
    pimpl_->metadata = metadata;

    // 初始化流式写入器 (创建临时子块池)
    int ret = pimpl_->stream.open(output_path);
    if (ret != 0) {
        fprintf(stderr, "[hiss][writer] open 失败: HissStreamWriter.open 返回 %d\n", ret);
        return -3;
    }

    pimpl_->opened = true;
    fprintf(stderr,
            "[hiss][writer] open 成功: path=%s nside=%u tile_nside=%u ordering=%d pixfrac=%g\n",
            output_path.c_str(), grid.nside, grid.tile_nside, grid.ordering, grid.pixfrac);
    return 0;
}

// ---------------------------------------------------------------------------
// add_tile: 添加一个 Tile 的数据 (流式写入 + 自动 occupancy 选择)
//
// 步骤11 关键改动:
//   1. 从 acc 获取全长度 signal/support (finalize_signal/finalize_support)
//   2. 统计有效像素 (sum_area > 0)
//   3. 自动选择 occupancy 模式 (忽略传入的 occ_mode)
//   4. BITMAP/SPARSE 模式只保存有效像素的 signal/support (紧凑数组)
//   5. 压缩后立即写入临时池 (步骤10), 不保留 compressed_data
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

    const size_t n_leaf = acc.pixels.size();

    // 0. 写入前验证 support 合法性 (02_FROZEN §10: 明显超 1 必须报错, pixel_area<=0 硬失败)
    int vret = acc.validate_support();
    if (vret != 0) {
        fprintf(stderr,
                "[hiss][writer] add_tile 失败: support 合法性检查未通过 (parent=%llu)\n",
                (unsigned long long)parent_ipix);
        return -3;
    }

    // 1. 生成 signal (float32) 与 support (uint8) — 全长度数组
    //    signal = 累计通量 (步骤7, finalize_signal 在 hiss_common.cpp 已修复)
    //    support = 面积比 (finalize_support 用 pixel_area 归一化)
    std::vector<float>   signal_full;
    std::vector<uint8_t> support_full;
    acc.finalize_signal(signal_full);
    acc.finalize_support(support_full);

    // 2. 统计有效像素并收集索引
    //    有效定义: sum_area > 0 (有贡献的像素)
    std::vector<uint32_t> valid_indices;
    valid_indices.reserve(n_leaf);
    for (size_t i = 0; i < n_leaf; i++) {
        if (acc.pixels[i].sum_area > 0.0) {
            valid_indices.push_back((uint32_t)i);
        }
    }
    uint32_t n_valid = (uint32_t)valid_indices.size();

    // 3. 自动选择 occupancy 模式 (步骤11: 不由调用方传入)
    //    忽略 occ_mode 参数, Writer 根据占用率自动选择
    OccupancyMode auto_mode = auto_select_occupancy(n_valid, (uint32_t)n_leaf);
    fprintf(stderr,
            "[hiss][writer] add_tile: parent=%llu n_leaf=%zu n_valid=%u occ_rate=%.4f "
            "传入occ_mode=%u → 自动选择=%u\n",
            (unsigned long long)parent_ipix, n_leaf, n_valid,
            (n_leaf > 0) ? (double)n_valid / (double)n_leaf : 0.0,
            (unsigned)occ_mode, (unsigned)auto_mode);

    // 4. 根据 occupancy 模式构造紧凑 signal/support 数组和 occupancy 数据
    std::vector<float>   signal_compact;   // BITMAP/SPARSE: n_valid 个值; FULL: n_leaf 个值
    std::vector<uint8_t> support_compact;  // 同上
    std::vector<uint8_t> occ_data;         // occupancy 子块数据

    if (auto_mode == OccupancyMode::FULL) {
        // FULL: 保存全部 n_leaf 个值, 无 occupancy 子块
        signal_compact  = std::move(signal_full);
        support_compact = std::move(support_full);
    } else if (auto_mode == OccupancyMode::BITMAP) {
        // BITMAP: 1 bit/叶像素 (LSB 优先), 只保存有效像素的 signal/support
        occ_data.assign((n_leaf + 7) / 8, 0);
        for (uint32_t idx : valid_indices) {
            occ_data[idx / 8] |= (uint8_t)(1u << (idx % 8));
        }
        signal_compact.reserve(n_valid);
        support_compact.reserve(n_valid);
        for (uint32_t idx : valid_indices) {
            signal_compact.push_back(signal_full[idx]);
            support_compact.push_back(support_full[idx]);
        }
    } else {
        // SPARSE_LIST: 保存有效像素的 local_ipix 索引列表 (uint32 数组) + 紧凑 signal/support
        occ_data.reserve(n_valid * sizeof(uint32_t));
        for (uint32_t idx : valid_indices) {
            size_t off = occ_data.size();
            occ_data.resize(off + 4);
            std::memcpy(occ_data.data() + off, &idx, 4);
        }
        signal_compact.reserve(n_valid);
        support_compact.reserve(n_valid);
        for (uint32_t idx : valid_indices) {
            signal_compact.push_back(signal_full[idx]);
            support_compact.push_back(support_full[idx]);
        }
    }

    // 5. 压缩并追加子块到流式写入器 (步骤10: 立即写入临时池)
    std::vector<HissSubblockDescriptor> subblocks;

    // 5a. occupancy 子块 (FULL 时省略; BITMAP/SPARSE_LIST 时生成)
    //     R04-B12: occupancy 在 BITMAP/SPARSE Tile 中必须标记 REQUIRED
    //     WP-G 步骤12: 传递 element_size 供 transform 使用
    //       BITMAP: element_size=1 (原始字节, 位图)
    //       SPARSE_LIST: element_size=4 (uint32 索引数组, delta/varint 有意义)
    if (auto_mode != OccupancyMode::FULL) {
        size_t occ_elem_size = (auto_mode == OccupancyMode::SPARSE_LIST) ? 4 : 1;
        HissSubblockDescriptor desc;
        int ret = compress_and_append(
            pimpl_->stream,
            SubblockType::OCCUPANCY,
            (uint16_t)SubblockFlags::REQUIRED,  // R04-B12: occupancy 标记 required
            occ_data.data(), occ_data.size(),
            pimpl_->codec_for(SubblockType::OCCUPANCY),
            pimpl_->transform_for(SubblockType::OCCUPANCY),
            pimpl_->checksum_for(SubblockType::OCCUPANCY),
            occ_elem_size,
            desc);
        if (ret != 0) return ret;
        subblocks.push_back(desc);
    }

    // 5b. signal 子块 (必需) — 只含有效像素的紧凑数组 (BITMAP/SPARSE) 或全长度 (FULL)
    //     WP-G 步骤12: element_size=sizeof(float)=4, 适用于 BYTE_SHUFFLE/DELTA/DELTA_VARINT
    {
        size_t sig_bytes = signal_compact.size() * sizeof(float);
        HissSubblockDescriptor desc;
        int ret = compress_and_append(
            pimpl_->stream,
            SubblockType::SIGNAL,
            (uint16_t)SubblockFlags::REQUIRED,
            (const uint8_t*)signal_compact.data(), sig_bytes,
            pimpl_->codec_for(SubblockType::SIGNAL),
            pimpl_->transform_for(SubblockType::SIGNAL),
            pimpl_->checksum_for(SubblockType::SIGNAL),
            sizeof(float),  // element_size=4 (float32)
            desc);
        if (ret != 0) return ret;
        subblocks.push_back(desc);
    }

    // 5c. support 子块 (必需) — 只含有效像素的紧凑数组 (BITMAP/SPARSE) 或全长度 (FULL)
    //     WP-G 步骤12: element_size=1 (uint8), BYTE_SHUFFLE 为 no-op, DELTA/DELTA_VARINT 可用
    {
        HissSubblockDescriptor desc;
        int ret = compress_and_append(
            pimpl_->stream,
            SubblockType::SUPPORT,
            (uint16_t)SubblockFlags::REQUIRED,
            support_compact.data(), support_compact.size(),
            pimpl_->codec_for(SubblockType::SUPPORT),
            pimpl_->transform_for(SubblockType::SUPPORT),
            pimpl_->checksum_for(SubblockType::SUPPORT),
            sizeof(uint8_t),  // element_size=1 (uint8)
            desc);
        if (ret != 0) return ret;
        subblocks.push_back(desc);
    }

    // 5d. SNR 子块 (可选, snr != nullptr 时生成)
    //     R04-B18: block 级 estimator_id/sampling_scale/count + 点数据
    //     冻结布局 (02_FROZEN §17 + 00_COMMON_CONTRACTS §2.5):
    //       [estimator_id:  uint32 LE]   — 估计器 ID (block 级)
    //       [sampling_scale: float32 LE] — 采样尺度 (block 级)
    //       [n_points:      uint32 LE]   — 控制点数 (= count, block 级)
    //       [points: n_points * 8B]      — 每点 local_ipix(uint32) + snr(float32)
    //     重复点处理: Writer 按升序排序 local_ipix, 重复点保留首次出现 (确定性规则)
    //     无覆盖点: 不得写入 (Stage1 映射到 Tile 后才写入, 不静默丢失)
    //     WP-G 步骤12: SNR 为混合布局, element_size=1 (transform 一般不适用于 SNR)
    if (snr) {
        // R04-B18: 确定性重复点处理 — 按 local_ipix 升序排序, 重复点保留首次出现
        std::vector<HissSnrControlPoint> sorted_points = snr->points;
        std::sort(sorted_points.begin(), sorted_points.end(),
                  [](const HissSnrControlPoint& a, const HissSnrControlPoint& b) {
                      return a.local_ipix < b.local_ipix;
                  });
        auto last = std::unique(sorted_points.begin(), sorted_points.end(),
                                [](const HissSnrControlPoint& a, const HissSnrControlPoint& b) {
                                    return a.local_ipix == b.local_ipix;
                                });
        sorted_points.erase(last, sorted_points.end());

        uint32_t n_points = (uint32_t)sorted_points.size();
        // R04-B18: 布局 = estimator_id(4) + sampling_scale(4) + n_points(4) + points(n*8)
        size_t snr_bytes = 4 + 4 + 4 + (size_t)n_points * 8;
        std::vector<uint8_t> snr_data(snr_bytes);
        size_t off = 0;
        // estimator_id (uint32 LE)
        uint32_t eid = snr->estimator_id;
        snr_data[off+0] = (uint8_t)(eid & 0xFF);
        snr_data[off+1] = (uint8_t)((eid >> 8) & 0xFF);
        snr_data[off+2] = (uint8_t)((eid >> 16) & 0xFF);
        snr_data[off+3] = (uint8_t)((eid >> 24) & 0xFF);
        off += 4;
        // sampling_scale (float32 LE)
        uint32_t sbits;
        std::memcpy(&sbits, &snr->sampling_scale, 4);
        snr_data[off+0] = (uint8_t)(sbits & 0xFF);
        snr_data[off+1] = (uint8_t)((sbits >> 8) & 0xFF);
        snr_data[off+2] = (uint8_t)((sbits >> 16) & 0xFF);
        snr_data[off+3] = (uint8_t)((sbits >> 24) & 0xFF);
        off += 4;
        // n_points (uint32 LE)
        snr_data[off+0] = (uint8_t)(n_points & 0xFF);
        snr_data[off+1] = (uint8_t)((n_points >> 8) & 0xFF);
        snr_data[off+2] = (uint8_t)((n_points >> 16) & 0xFF);
        snr_data[off+3] = (uint8_t)((n_points >> 24) & 0xFF);
        off += 4;
        // points: 每点 local_ipix(uint32 LE) + snr(float32 LE) = 8 字节
        for (uint32_t i = 0; i < n_points; i++) {
            uint32_t lip = sorted_points[i].local_ipix;
            snr_data[off+0] = (uint8_t)(lip & 0xFF);
            snr_data[off+1] = (uint8_t)((lip >> 8) & 0xFF);
            snr_data[off+2] = (uint8_t)((lip >> 16) & 0xFF);
            snr_data[off+3] = (uint8_t)((lip >> 24) & 0xFF);
            uint32_t sbits2;
            std::memcpy(&sbits2, &sorted_points[i].snr, 4);
            snr_data[off+4] = (uint8_t)(sbits2 & 0xFF);
            snr_data[off+5] = (uint8_t)((sbits2 >> 8) & 0xFF);
            snr_data[off+6] = (uint8_t)((sbits2 >> 16) & 0xFF);
            snr_data[off+7] = (uint8_t)((sbits2 >> 24) & 0xFF);
            off += 8;
        }
        HissSubblockDescriptor desc;
        int ret = compress_and_append(
            pimpl_->stream,
            SubblockType::SNR,
            (uint16_t)SubblockFlags::OPTIONAL,
            snr_data.data(), snr_data.size(),
            pimpl_->codec_for(SubblockType::SNR),
            pimpl_->transform_for(SubblockType::SNR),
            pimpl_->checksum_for(SubblockType::SNR),
            1,  // element_size=1 (混合布局, 按字节处理)
            desc);
        if (ret != 0) return ret;
        subblocks.push_back(desc);

        fprintf(stderr,
                "[hiss][writer]   SNR 子块: estimator_id=%u sampling_scale=%g 写入 %u 个控制点 "
                "(去重后, 布局: eid+scale+n_points + %llu 字节)\n",
                snr->estimator_id, snr->sampling_scale, n_points,
                (unsigned long long)((size_t)n_points * 8));
    }

    // 6. 记录 Tile 目录 (只保留描述符, 不保留压缩数据 — 步骤10)
    int ret = pimpl_->stream.record_tile(parent_ipix, acc.tile_nside, auto_mode,
                                          std::move(subblocks));
    if (ret != 0) {
        fprintf(stderr, "[hiss][writer] add_tile: record_tile 失败 ret=%d\n", ret);
        return ret;
    }

    fprintf(stderr,
            "[hiss][writer] add_tile 成功: parent_ipix=%llu tile_nside=%u occ_mode=%u "
            "n_leaf=%zu n_valid=%u n_subblocks=%zu\n",
            (unsigned long long)parent_ipix, acc.tile_nside, (unsigned)auto_mode,
            n_leaf, n_valid, subblocks.size());
    return 0;
}

// ---------------------------------------------------------------------------
// finalize: 生成 Header, 组装 .partial, flush, 原子重命名 (委托给 HissStreamWriter)
// ---------------------------------------------------------------------------

int HissWriter::finalize() {
    if (!pimpl_->opened) {
        fprintf(stderr, "[hiss][writer] finalize 失败: 会话未打开\n");
        return -1;
    }

    int ret = pimpl_->stream.finalize(pimpl_->grid, pimpl_->metadata);
    if (ret != 0) {
        fprintf(stderr, "[hiss][writer] finalize 失败: HissStreamWriter.finalize 返回 %d\n", ret);
        pimpl_->opened = false;
        return ret;
    }

    pimpl_->opened = false;
    fprintf(stderr, "[hiss][writer] finalize 成功: tiles=%zu path 已原子替换\n",
            pimpl_->stream.tile_count());
    return 0;
}

// ---------------------------------------------------------------------------
// cancel: 关闭并删除临时文件, 清理内存 (委托给 HissStreamWriter)
// ---------------------------------------------------------------------------

void HissWriter::cancel() {
    if (pimpl_->opened) {
        fprintf(stderr, "[hiss][writer] cancel: 清理临时文件\n");
    }
    pimpl_->stream.cancel();
    pimpl_->experiment_codecs.clear();
    pimpl_->experiment_checksums.clear();
    pimpl_->opened = false;
}

// ---------------------------------------------------------------------------
// set_experiment_codec: 设置实验性 codec/transform (按 SubblockType)
// ---------------------------------------------------------------------------

void HissWriter::set_experiment_codec(SubblockType type,
                                      CodecId codec,
                                      TransformId transform) {
    pimpl_->experiment_codecs[type] = std::make_pair(codec, transform);
    fprintf(stderr,
            "[hiss][writer] set_experiment_codec: type=%u codec=%u transform=%u\n",
            (unsigned)type, (unsigned)codec, (unsigned)transform);
}

// ---------------------------------------------------------------------------
// set_experiment_transform (WP-G 步骤12 新增)
//   仅设置 transform, 保留已有 codec (若未设置则默认 RAW)
//   与 set_experiment_codec 类似, 但只修改 transform 部分
// ---------------------------------------------------------------------------

void HissWriter::set_experiment_transform(SubblockType type,
                                           TransformId transform) {
    auto& entry = pimpl_->experiment_codecs[type];
    // 保留已有 codec (entry.first), 仅更新 transform (entry.second)
    // 若该 type 未设置过 codec, entry.first 默认为 CodecId::RAW (0)
    entry.second = transform;
    fprintf(stderr,
            "[hiss][writer] set_experiment_transform: type=%u transform=%u (codec=%u 保持)\n",
            (unsigned)type, (unsigned)transform, (unsigned)entry.first);
}

// ---------------------------------------------------------------------------
// set_experiment_checksum (INTERIM_BASELINE_NOT_FROZEN)
//   设置实验性 checksum (按 SubblockType), 默认 NONE
//   checksum 计算的是压缩后数据 (与 Reader 端一致)
//   候选算法通过 ChecksumRegistry 注册, 内置 CRC32C
// ---------------------------------------------------------------------------

void HissWriter::set_experiment_checksum(SubblockType type,
                                           ChecksumType checksum) {
    pimpl_->experiment_checksums[type] = checksum;
    fprintf(stderr,
            "[hiss][writer] set_experiment_checksum: type=%u checksum=%u\n",
            (unsigned)type, (unsigned)checksum);
}

} // namespace hiss
