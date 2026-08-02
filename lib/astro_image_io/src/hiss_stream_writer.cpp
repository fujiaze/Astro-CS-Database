// ============================================================================
// hiss_stream_writer.cpp - AstroCS HISS 流式写入器实现
//
// 依据:
//   - 02_FROZEN_STAGE1_HISS_SPEC.md §14 (HISS 容器)
//   - docs/stage1_fix/00_COMMON_CONTRACTS.md §4.5/§4.6 (流式写入/原子替换)
//   - docs/stage1_fix/spec.md 步骤10 (流式写入)
//
// 实现要点:
//   1. 临时子块池 (temp_pool) 是一个独立文件, add_tile 时压缩数据立即追加
//   2. 内存只保留 SubblockDescriptor (offset/size/codec/checksum), 不保留 compressed_data
//   3. finalize 时组装最终文件: 签名块(20B) + Header + 子块数据(从 temp_pool 复制)
//   4. 原子替换: Windows 使用 MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
//      不先删除旧文件再 rename (避免竞态窗口)
//   5. 子块 offset 在 append_subblock 时记录为 temp_pool 内偏移;
//      finalize 时统一加上 (签名块大小 + Header大小) 调整为最终文件偏移
// ============================================================================

#include "hiss_stream_writer.h"
#include "aio_util.h"  // aio_fopen_utf8

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <filesystem>
#include <io.h>      // _commit, _fileno (Windows)
#include <fcntl.h>   // _O_BINARY

#ifdef _WIN32
#include <windows.h>
#endif

namespace hiss {

// ============================================================================
// 内部常量
// ============================================================================

// R04-B14: 固定签名块 (16 字节, 显式小端序)
//   magic[8] = "HISS0100" (ASCII, 含容器布局标识 0100)
//   header_length: uint32 LE (Header 区字节数, 不含签名块)
//   feature_flags: uint32 LE (特性标志位)
// 旧格式 (ACSHISS\0 + version + header_offset) 已废弃: 主机端序 + 无严格长度
static const char     HISS_MAGIC[8] = { 'H','I','S','S','0','1','0','0' };
static const uint32_t HISS_FEATURE_FLAGS = HISS_FEAT_TLV_HEADER;  // TLV Header 必需
// HISS_SIGNATURE_SIZE 由 hiss_format.h 定义为宏 (16), 不再在此重复定义

// 子块描述符固定字节数 (写入 Header 时每项大小)
// R05-B09: 新增 decoded_size(8), 总大小 50 字节 (三长度字段)
// type(1) + ext_type_id(2) + flags(2) + offset(8) + compressed_size(8) +
// uncompressed_size(8) + decoded_size(8) + codec_id(2) + transform_id(2) +
// checksum_type(1) + checksum(8) = 50
static const size_t HISS_SUBBLOCK_DESCRIPTOR_SIZE = HISS_SUBBLOCK_DESC_DISK_SIZE;

// Tile 目录固定前缀字节数 (不含子块描述符)
// parent_ipix(8) + tile_nside(4) + occ_mode(1) + subblock_count(2) = 15
static const size_t HISS_TILE_DIR_PREFIX_SIZE = 15;

// TLV 项头大小: tag(2) + flags(1) + length(4) = 7 字节
static const size_t HISS_TLV_HEADER_SIZE = 7;

// schema 指纹 (32 字节, 标识当前 HISS schema 版本)
// Reader 用于验证 schema 兼容性; 实际指纹值由规范冻结, 这里用确定性填充
// R05-B08/B09: schema 0002-R1 (新增 SCIENCE_METADATA TLV + decoded_size 三长度字段)
static const uint8_t HISS_SCHEMA_FINGERPRINT[32] = {
    0xA1, 0x00, 0x72, 0x04, 0xB1, 0x00, 0x61, 0x04,
    0x48, 0x49, 0x53, 0x53, 0x2D, 0x76, 0x31, 0x2E,
    0x30, 0x2D, 0x73, 0x63, 0x68, 0x65, 0x6D, 0x61,
    0x2D, 0x30, 0x30, 0x30, 0x32, 0x2D, 0x52, 0x31
};

// 临时池复制缓冲区大小 (4MB, 减少小字节读写的系统调用开销)
static const size_t COPY_BUF_SIZE = 4 * 1024 * 1024;

// ============================================================================
// 内部辅助: 小端字节追加工具 (用于构建 Header 字节流)
// ============================================================================

// 显式小端序写入工具 (02_FROZEN §14: 所有数值按显式小端序写入, Reader 不依赖本机端序)
struct ByteBuf {
    std::vector<uint8_t> data;

    void u8 (uint8_t v)            { data.push_back(v); }
    void u16(uint16_t v)           { size_t n = data.size(); data.resize(n + 2);
                                     data[n+0] = (uint8_t)(v & 0xFF);
                                     data[n+1] = (uint8_t)((v >> 8) & 0xFF); }
    void u32(uint32_t v)           { size_t n = data.size(); data.resize(n + 4);
                                     data[n+0] = (uint8_t)(v & 0xFF);
                                     data[n+1] = (uint8_t)((v >> 8) & 0xFF);
                                     data[n+2] = (uint8_t)((v >> 16) & 0xFF);
                                     data[n+3] = (uint8_t)((v >> 24) & 0xFF); }
    void u64(uint64_t v)           { size_t n = data.size(); data.resize(n + 8);
                                     for (int i = 0; i < 8; i++)
                                         data[n+i] = (uint8_t)((v >> (8*i)) & 0xFF); }
    void f64(double v)             { uint64_t bits; std::memcpy(&bits, &v, 8); u64(bits); }
    void bytes(const void* p, size_t n) {
        size_t off = data.size();
        data.resize(off + n);
        if (n > 0) std::memcpy(data.data() + off, p, n);
    }
    // 写入 C 字符串 (不含终止符), 返回长度
    size_t str(const char* s) {
        size_t n = s ? std::strlen(s) : 0;
        bytes(s, n);
        return n;
    }
};

// ============================================================================
// R05-B08: 构建 SCIENCE_METADATA TLV value (typed key/value 二进制记录)
//   格式: version(u32) + n_fields(u32) + n_fields * field
//   field: key_id(u16) + value_type(u8) + value_len(u32) + value
//   JSON 仅镜像, 科学语义唯一权威来源为此 TLV
// ============================================================================
static void build_science_metadata(const HissMetadata& md, ByteBuf& out) {
    // 先写入字段到临时缓冲, 统计 n_fields, 再组装
    ByteBuf fields;
    uint32_t n_fields = 0;

    // 辅助: 添加 uint32 字段
    auto add_u32 = [&](uint16_t key_id, uint32_t val) {
        fields.u16(key_id);
        fields.u8 (HISS_SCI_TYPE_UINT32);
        fields.u32(4);
        fields.u32(val);
        n_fields++;
    };
    // 辅助: 添加 uint8 字段
    auto add_u8 = [&](uint16_t key_id, uint8_t val) {
        fields.u16(key_id);
        fields.u8 (HISS_SCI_TYPE_UINT8);
        fields.u32(1);
        fields.u8 (val);
        n_fields++;
    };
    // 辅助: 添加 float64 字段
    auto add_f64 = [&](uint16_t key_id, double val) {
        fields.u16(key_id);
        fields.u8 (HISS_SCI_TYPE_FLOAT64);
        fields.u32(8);
        fields.f64(val);
        n_fields++;
    };
    // 辅助: 添加 string 字段 (非空才添加)
    auto add_str = [&](uint16_t key_id, const char* s) {
        if (!s || s[0] == '\0') return;  // 空字符串不写入
        size_t len = std::strlen(s);
        fields.u16(key_id);
        fields.u8 (HISS_SCI_TYPE_STRING);
        fields.u32((uint32_t)len);
        fields.bytes(s, len);
        n_fields++;
    };

    // 必需字段 (Writer 必须写入)
    add_u32(HISS_SCI_KEY_SIGNAL_SEMANTICS,  1);  // 1=累计相对通量
    add_u32(HISS_SCI_KEY_SUPPORT_SEMANTICS, 1);  // 1=几何覆盖率[0,1]
    add_u8 (HISS_SCI_KEY_SIGNAL_DATATYPE,   4);  // float32
    add_u8 (HISS_SCI_KEY_SUPPORT_DATATYPE,  1);  // uint8
    add_u8 (HISS_SCI_KEY_PHOTAPPL, (uint8_t)(md.photappl ? 1 : 0));
    add_f64(HISS_SCI_KEY_PHOTSCAL, md.photscal);
    add_str(HISS_SCI_KEY_BUNIT, md.bunit);

    // 可选字段 (有值才写入)
    add_str(HISS_SCI_KEY_CALMODE,  md.calmode);
    add_f64(HISS_SCI_KEY_DARKSCL,  md.darkscl);
    add_f64(HISS_SCI_KEY_EXPTIME,  md.exptime);
    add_str(HISS_SCI_KEY_FILTER,   md.filter);
    add_str(HISS_SCI_KEY_TELESCOP, md.telescop);
    add_str(HISS_SCI_KEY_OBJECT,   md.object);
    add_str(HISS_SCI_KEY_DATE_OBS, md.date_obs);

    // 组装: version + n_fields + fields
    out.u32(HISS_SCI_META_VERSION);
    out.u32(n_fields);
    if (n_fields > 0) {
        out.bytes(fields.data.data(), fields.data.size());
    }
}

// ============================================================================
// 内部辅助: UTF-8 路径转宽字符 (Windows)
// ============================================================================

#ifdef _WIN32
static std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return std::wstring(s.begin(), s.end());
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    ws.resize(len - 1);  // 去掉末尾 null
    return ws;
}
#endif

// ============================================================================
// 内部辅助: 原子替换 (Windows 优先 MoveFileExW)
//   依据 00_COMMON_CONTRACTS §4.6:
//     Windows: MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
//     不能先删除旧文件再 rename (避免竞态: 删除后 rename 前若进程崩溃, 文件丢失)
//   返回 0=成功, <0=失败
// ============================================================================

static int atomic_replace(const std::string& temp_path, const std::string& final_path) {
#ifdef _WIN32
    // Windows: 使用 MoveFileExW 原子替换 (目标存在时覆盖)
    // MOVEFILE_REPLACE_EXISTING: 目标文件存在则替换
    // MOVEFILE_WRITE_THROUGH: 确保数据落盘后才返回
    std::wstring wtemp = widen(temp_path);
    std::wstring wfinal = widen(final_path);
    BOOL ok = MoveFileExW(wtemp.c_str(), wfinal.c_str(),
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        DWORD err = GetLastError();
        fprintf(stderr, "[hiss][stream] atomic_replace: MoveFileExW 失败 err=%lu "
                "temp=%s final=%s\n", err, temp_path.c_str(), final_path.c_str());
        return -1;
    }
    return 0;
#else
    // POSIX: rename 本身是原子的 (目标存在时覆盖)
    if (std::rename(temp_path.c_str(), final_path.c_str()) != 0) {
        fprintf(stderr, "[hiss][stream] atomic_replace: rename 失败 errno=%d\n", errno);
        return -1;
    }
    return 0;
#endif
}

// ============================================================================
// HissStreamWriter::Impl 实现
// ============================================================================

// 待记录的 Tile 目录 (只保留描述符, 不保留压缩数据)
struct PendingTileDir {
    uint64_t                               parent_ipix = 0;
    uint32_t                               tile_nside  = 0;
    OccupancyMode                          occ_mode    = OccupancyMode::FULL;
    std::vector<HissSubblockDescriptor>    subblocks;
};

struct HissStreamWriter::Impl {
    std::string                 final_path;      // 最终 .hiss 路径
    std::string                 partial_path;    // .partial 路径 (最终组装文件)
    std::string                 temp_pool_path;  // 临时子块池路径
    FILE*                       temp_pool_fp = nullptr;  // 临时子块池文件句柄
    uint64_t                    temp_pool_size = 0;      // 临时池当前大小 (字节)
    std::vector<PendingTileDir> tile_dirs;       // Tile 目录列表 (只含描述符)
    bool                        opened = false;

    ~Impl() {
        if (temp_pool_fp) {
            std::fclose(temp_pool_fp);
            temp_pool_fp = nullptr;
        }
    }
};

// ---------------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------------

HissStreamWriter::HissStreamWriter() : pimpl_(std::make_unique<Impl>()) {
    fprintf(stderr, "[hiss][stream] HissStreamWriter 构造\n");
}

HissStreamWriter::~HissStreamWriter() {
    if (pimpl_ && pimpl_->opened) {
        fprintf(stderr, "[hiss][stream] 析构时会话仍打开, 自动 cancel\n");
        cancel();
    }
}

// ---------------------------------------------------------------------------
// open: 初始化写入会话, 创建临时子块池
// ---------------------------------------------------------------------------

int HissStreamWriter::open(const std::string& final_path) {
    if (pimpl_->opened) {
        fprintf(stderr, "[hiss][stream] open 失败: 已有会话进行中\n");
        return -1;
    }

    pimpl_->final_path     = final_path;
    pimpl_->partial_path   = final_path + ".partial";
    pimpl_->temp_pool_path = final_path + ".tmppool";
    pimpl_->tile_dirs.clear();
    pimpl_->temp_pool_size = 0;

    // 创建临时子块池文件 (二进制写, "wb" 截断已存在的)
    pimpl_->temp_pool_fp = aio_fopen_utf8(pimpl_->temp_pool_path.c_str(), "wb");
    if (!pimpl_->temp_pool_fp) {
        fprintf(stderr, "[hiss][stream] open 失败: 无法创建临时池 %s\n",
                pimpl_->temp_pool_path.c_str());
        return -2;
    }

    pimpl_->opened = true;
    fprintf(stderr, "[hiss][stream] open 成功: final=%s temp_pool=%s\n",
            final_path.c_str(), pimpl_->temp_pool_path.c_str());
    return 0;
}

// ---------------------------------------------------------------------------
// append_subblock: 追加已压缩子块到临时池
//   offset 记录为临时池内偏移 (从 0 开始), finalize 时调整为最终文件偏移
// ---------------------------------------------------------------------------

int HissStreamWriter::append_subblock(const uint8_t* data, size_t size,
                                       HissSubblockDescriptor& desc) {
    if (!pimpl_->opened || !pimpl_->temp_pool_fp) {
        fprintf(stderr, "[hiss][stream] append_subblock 失败: 会话未打开\n");
        return -1;
    }

    // 记录子块在临时池中的偏移 (finalize 时加上签名块+Header大小)
    desc.offset = pimpl_->temp_pool_size;

    // 写入压缩数据到临时池
    if (size > 0) {
        if (!data) {
            fprintf(stderr, "[hiss][stream] append_subblock: data 为空但 size=%zu\n", size);
            return -2;
        }
        size_t written = std::fwrite(data, 1, size, pimpl_->temp_pool_fp);
        if (written != size) {
            fprintf(stderr, "[hiss][stream] append_subblock: 写入不足 need=%zu got=%zu\n",
                    size, written);
            return -3;
        }
        // 立即 flush, 确保数据落盘 (流式写入要点: 不在内存缓存)
        std::fflush(pimpl_->temp_pool_fp);
    }

    pimpl_->temp_pool_size += size;
    desc.compressed_size = size;

    fprintf(stderr,
            "[hiss][stream]   append_subblock: type=%u offset=%llu size=%zu "
            "codec=%u transform=%u checksum_type=%u\n",
            (unsigned)desc.type, (unsigned long long)desc.offset,
            size, (unsigned)desc.codec_id, (unsigned)desc.transform_id,
            (unsigned)desc.checksum_type);
    return 0;
}

// ---------------------------------------------------------------------------
// record_tile: 记录 Tile 目录 (只保留描述符, 不保留压缩数据)
// ---------------------------------------------------------------------------

int HissStreamWriter::record_tile(uint64_t parent_ipix, uint32_t tile_nside,
                                    OccupancyMode occ_mode,
                                    std::vector<HissSubblockDescriptor> subblocks) {
    if (!pimpl_->opened) {
        fprintf(stderr, "[hiss][stream] record_tile 失败: 会话未打开\n");
        return -1;
    }

    PendingTileDir td;
    td.parent_ipix = parent_ipix;
    td.tile_nside  = tile_nside;
    td.occ_mode    = occ_mode;
    td.subblocks   = std::move(subblocks);

    fprintf(stderr,
            "[hiss][stream]   record_tile: parent=%llu tile_nside=%u occ_mode=%u n_subblocks=%zu\n",
            (unsigned long long)td.parent_ipix, td.tile_nside,
            (unsigned)td.occ_mode, td.subblocks.size());

    pimpl_->tile_dirs.push_back(std::move(td));
    return 0;
}

// ---------------------------------------------------------------------------
// finalize: 生成 TLV Header, 组装最终文件, flush, 原子重命名
//   R04-B14: 新签名块 (16B, 显式小端序): "HISS0100" + header_length(u32 LE) + feature_flags(u32 LE)
//   R04-B15: Header 为可扩展 TLV 二进制结构, 未知可选跳过/未知必需拒绝
//   最终布局: 签名块(16B) → TLV Header → 子块1 → 子块2 → ...
// ---------------------------------------------------------------------------

int HissStreamWriter::finalize(const HissGridSpec& grid, const HissMetadata& metadata) {
    if (!pimpl_->opened) {
        fprintf(stderr, "[hiss][stream] finalize 失败: 会话未打开\n");
        return -1;
    }

    // 1. 关闭临时池 (确保所有数据落盘)
    if (pimpl_->temp_pool_fp) {
        std::fflush(pimpl_->temp_pool_fp);
        std::fclose(pimpl_->temp_pool_fp);
        pimpl_->temp_pool_fp = nullptr;
    }

    // 2. 计算元数据 JSON (R04-B15: JSON 作为可选人类可读镜像, 非唯一权威)
    //    R05-B08: 科学语义权威来源为 SCIENCE_METADATA TLV, JSON 仅镜像
    std::string json = metadata.to_json();

    // 2a. 构建 SCIENCE_METADATA TLV value (R05-B08: typed key/value, required)
    ByteBuf sci_meta;
    build_science_metadata(metadata, sci_meta);

    // 3. 计算 TLV Header 大小
    //    每个 TLV: tag(2) + flags(1) + length(4) + value = 7 + value_size
    //    a. SCHEMA_FINGERPRINT (required): 7 + 32
    //    b. GRID_SPEC (required): 7 + 24
    //    c. SCIENCE_METADATA (required): 7 + sci_meta.size()  [R05-B08]
    //    d. METADATA_JSON (optional): 7 + json.size()
    //    e. TILE_DIRECTORY (required): 7 + (4 + Σ(15 + 50*n_subblocks))  [R05-B09: 50字节]
    size_t tile_dir_value_size = 4;  // n_tiles (uint32)
    for (const auto& t : pimpl_->tile_dirs) {
        tile_dir_value_size += HISS_TILE_DIR_PREFIX_SIZE +
                               HISS_SUBBLOCK_DESCRIPTOR_SIZE * t.subblocks.size();
    }

    size_t header_size = 0;
    header_size += HISS_TLV_HEADER_SIZE + 32;                       // SCHEMA_FINGERPRINT
    header_size += HISS_TLV_HEADER_SIZE + 24;                       // GRID_SPEC
    header_size += HISS_TLV_HEADER_SIZE + sci_meta.data.size();     // SCIENCE_METADATA (R05-B08)
    header_size += HISS_TLV_HEADER_SIZE + json.size();              // METADATA_JSON (镜像)
    header_size += HISS_TLV_HEADER_SIZE + tile_dir_value_size;      // TILE_DIRECTORY

    // 4. 调整所有子块 offset: 临时池偏移 → 最终文件偏移
    //    最终 offset = 签名块(16) + Header大小 + 临时池偏移
    //    (在构建 Header 字节流之前调整, 确保写入 Header 的 offset 是最终值)
    uint64_t base_offset = HISS_SIGNATURE_SIZE + header_size;
    for (auto& t : pimpl_->tile_dirs) {
        for (auto& sb : t.subblocks) {
            sb.offset += base_offset;
        }
    }

    // 5. 构建 TLV Header 字节流 (R04-B15: 可扩展二进制 TLV 结构)
    ByteBuf hdr;

    // a. SCHEMA_FINGERPRINT TLV (required) — schema 版本标识
    hdr.u16(HISS_TLV_SCHEMA_FINGERPRINT);
    hdr.u8 (HISS_TLV_FLAG_REQUIRED);
    hdr.u32(32);
    hdr.bytes(HISS_SCHEMA_FINGERPRINT, 32);

    // b. GRID_SPEC TLV (required) — 网格规格 (24 字节)
    hdr.u16(HISS_TLV_GRID_SPEC);
    hdr.u8 (HISS_TLV_FLAG_REQUIRED);
    hdr.u32(24);
    hdr.u32(grid.nside);
    hdr.u32(grid.tile_nside);
    hdr.u32((uint32_t)grid.ordering);
    hdr.u32((uint32_t)grid.radesys);
    hdr.f64(grid.pixfrac);

    // c. SCIENCE_METADATA TLV (required) — R05-B08: 科学语义权威来源 (typed key/value)
    //    包含 signal/support 语义、PHOTAPPL、PHOTSCAL、BUNIT、校准模式等
    //    JSON 仅作为人类可读镜像, 不得是科学语义唯一来源
    hdr.u16(HISS_TLV_SCIENCE_METADATA);
    hdr.u8 (HISS_TLV_FLAG_REQUIRED);
    hdr.u32((uint32_t)sci_meta.data.size());
    if (!sci_meta.data.empty()) hdr.bytes(sci_meta.data.data(), sci_meta.data.size());

    // d. METADATA_JSON TLV (optional) — 元数据 JSON, 人类可读镜像
    //    R05-B08: 科学语义权威来源为 SCIENCE_METADATA TLV, JSON 标记 optional
    hdr.u16(HISS_TLV_METADATA_JSON);
    hdr.u8 (HISS_TLV_FLAG_OPTIONAL);
    hdr.u32((uint32_t)json.size());
    if (!json.empty()) hdr.bytes(json.data(), json.size());

    // e. TILE_DIRECTORY TLV (required) — Tile 目录
    hdr.u16(HISS_TLV_TILE_DIRECTORY);
    hdr.u8 (HISS_TLV_FLAG_REQUIRED);
    hdr.u32((uint32_t)tile_dir_value_size);
    hdr.u32((uint32_t)pimpl_->tile_dirs.size());  // n_tiles
    for (const auto& t : pimpl_->tile_dirs) {
        hdr.u64(t.parent_ipix);
        hdr.u32(t.tile_nside);
        hdr.u8 ((uint8_t)t.occ_mode);
        hdr.u16((uint16_t)t.subblocks.size());
        for (const auto& sb : t.subblocks) {
            // R05-B09: 子块描述符 50 字节 (含 decoded_size, 三长度字段)
            hdr.u8 ((uint8_t)sb.type);
            hdr.u16(sb.ext_type_id);             // 扩展命名空间 ID (0=内置)
            hdr.u16(sb.flags);
            hdr.u64(sb.offset);
            hdr.u64(sb.compressed_size);          // stored_size
            hdr.u64(sb.uncompressed_size);        // transform_size
            hdr.u64(sb.decoded_size);             // decoded_size (R05-B09)
            hdr.u16((uint16_t)sb.codec_id);
            hdr.u16((uint16_t)sb.transform_id);
            hdr.u8 ((uint8_t)sb.checksum_type);
            hdr.u64(sb.checksum);
        }
    }

    // R06-B18 修复: Header 大小不一致必须硬失败, 不得仅警告继续
    // 不一致时 offset 错位, 产出"看似合法但 offset 错误"的 HISS 文件, 且会被原子替换覆盖现有文件
    if (hdr.data.size() != header_size) {
        fprintf(stderr,
                "[hiss][stream] finalize 失败: Header 实际大小 %zu 与预算 %zu 不符 (内部 bug), "
                "中止写入并清理 (R06-B18)\n",
                hdr.data.size(), header_size);
        pimpl_->opened = false;
        return HISS_ERR_FORMAT_VIOLATION;
    }

    // 6. 创建最终 .partial 文件: 签名块 + Header + 子块数据(从临时池复制)
    FILE* fp_out = aio_fopen_utf8(pimpl_->partial_path.c_str(), "wb");
    if (!fp_out) {
        fprintf(stderr, "[hiss][stream] finalize 失败: 无法创建 .partial %s\n",
                pimpl_->partial_path.c_str());
        return -2;
    }

    // 6a. 写入签名块 (16B: MAGIC + header_length + feature_flags, 全显式小端序)
    //     R04-B14: 禁止 host-endian memcpy, 所有数值显式按小端序写入
    uint8_t sig[16] = {0};
    std::memcpy(sig, HISS_MAGIC, 8);  // MAGIC 是字节字面量, 不涉及端序
    uint32_t hlen = (uint32_t)header_size;
    sig[8]  = (uint8_t)(hlen & 0xFF);          // header_length: uint32 LE
    sig[9]  = (uint8_t)((hlen >> 8) & 0xFF);
    sig[10] = (uint8_t)((hlen >> 16) & 0xFF);
    sig[11] = (uint8_t)((hlen >> 24) & 0xFF);
    uint32_t fflags = HISS_FEATURE_FLAGS;
    sig[12] = (uint8_t)(fflags & 0xFF);        // feature_flags: uint32 LE
    sig[13] = (uint8_t)((fflags >> 8) & 0xFF);
    sig[14] = (uint8_t)((fflags >> 16) & 0xFF);
    sig[15] = (uint8_t)((fflags >> 24) & 0xFF);
    if (std::fwrite(sig, 1, HISS_SIGNATURE_SIZE, fp_out) != HISS_SIGNATURE_SIZE) {
        fprintf(stderr, "[hiss][stream] finalize 失败: 写入签名块失败\n");
        std::fclose(fp_out);
        return -3;
    }

    // 6b. 写入 TLV Header
    if (!hdr.data.empty()) {
        if (std::fwrite(hdr.data.data(), 1, hdr.data.size(), fp_out) != hdr.data.size()) {
            fprintf(stderr, "[hiss][stream] finalize 失败: 写入 Header 失败\n");
            std::fclose(fp_out);
            return -4;
        }
    }

    // 6c. 从临时池复制子块数据到 .partial
    if (pimpl_->temp_pool_size > 0) {
        FILE* fp_in = aio_fopen_utf8(pimpl_->temp_pool_path.c_str(), "rb");
        if (!fp_in) {
            fprintf(stderr, "[hiss][stream] finalize 失败: 无法打开临时池 %s\n",
                    pimpl_->temp_pool_path.c_str());
            std::fclose(fp_out);
            return -5;
        }

        // 批量复制 (4MB 缓冲区)
        std::vector<uint8_t> copy_buf(COPY_BUF_SIZE);
        uint64_t remaining = pimpl_->temp_pool_size;
        while (remaining > 0) {
            size_t chunk = (remaining > COPY_BUF_SIZE) ? COPY_BUF_SIZE : (size_t)remaining;
            size_t nread = std::fread(copy_buf.data(), 1, chunk, fp_in);
            if (nread != chunk) {
                fprintf(stderr, "[hiss][stream] finalize 失败: 读取临时池不足 "
                        "need=%zu got=%zu\n", chunk, nread);
                std::fclose(fp_in);
                std::fclose(fp_out);
                return -6;
            }
            size_t nwritten = std::fwrite(copy_buf.data(), 1, chunk, fp_out);
            if (nwritten != chunk) {
                fprintf(stderr, "[hiss][stream] finalize 失败: 写入 .partial 不足 "
                        "need=%zu got=%zu\n", chunk, nwritten);
                std::fclose(fp_in);
                std::fclose(fp_out);
                return -7;
            }
            remaining -= chunk;
        }
        std::fclose(fp_in);
    }

    // 7. flush + OS 级持久化同步 + 关闭 .partial
    // R06-B19 修复: fflush 只刷 C 库缓冲, 不保证落盘; 需 OS 级 _commit/FlushFileBuffers
    // fflush 失败必须硬失败, 不得仅警告继续 (否则产出空/截断文件后原子替换覆盖现有文件)
    if (std::fflush(fp_out) != 0) {
        fprintf(stderr, "[hiss][stream] finalize 失败: fflush 失败 (R06-B19: 硬失败)\n");
        std::fclose(fp_out);
        // 清理 .partial
        std::error_code ec_clean;
        std::filesystem::remove(pimpl_->partial_path, ec_clean);
        pimpl_->opened = false;
        return HISS_ERR_IO;
    }
    // Windows: _commit 强制 OS 缓冲刷盘; POSIX: fdatasync
#ifdef _WIN32
    int fd = _fileno(fp_out);
    if (fd >= 0) {
        if (_commit(fd) != 0) {
            fprintf(stderr, "[hiss][stream] finalize 失败: _commit (OS 级同步) 失败 (R06-B19)\n");
            std::fclose(fp_out);
            std::error_code ec_clean;
            std::filesystem::remove(pimpl_->partial_path, ec_clean);
            pimpl_->opened = false;
            return HISS_ERR_IO;
        }
    }
#else
    int fd = fileno(fp_out);
    if (fd >= 0) {
        if (fdatasync(fd) != 0) {
            fprintf(stderr, "[hiss][stream] finalize 失败: fdatasync 失败 (R06-B19)\n");
            std::fclose(fp_out);
            std::error_code ec_clean;
            std::filesystem::remove(pimpl_->partial_path, ec_clean);
            pimpl_->opened = false;
            return HISS_ERR_IO;
        }
    }
#endif
    std::fclose(fp_out);

    // 8. 删除临时池 (数据已复制到 .partial)
    std::error_code ec;
    std::filesystem::remove(pimpl_->temp_pool_path, ec);

    // 9. 原子重命名 .partial → 最终路径
    //    使用 MoveFileExW (Windows), 不先删除旧文件
    int ret = atomic_replace(pimpl_->partial_path, pimpl_->final_path);
    if (ret != 0) {
        fprintf(stderr, "[hiss][stream] finalize 失败: 原子替换失败 %s -> %s\n",
                pimpl_->partial_path.c_str(), pimpl_->final_path.c_str());
        pimpl_->opened = false;
        return -8;
    }

    uint64_t total_size = base_offset + pimpl_->temp_pool_size;
    fprintf(stderr,
            "[hiss][stream] finalize 成功: tiles=%zu header_size=%zu "
            "total_size=%llu path=%s\n",
            pimpl_->tile_dirs.size(),
            header_size,
            (unsigned long long)total_size,
            pimpl_->final_path.c_str());

    pimpl_->opened = false;
    return 0;
}

// ---------------------------------------------------------------------------
// cancel: 关闭并删除临时文件, 清理内存
// ---------------------------------------------------------------------------

void HissStreamWriter::cancel() {
    if (pimpl_->opened) {
        fprintf(stderr, "[hiss][stream] cancel: 清理临时文件\n");
    }
    if (pimpl_->temp_pool_fp) {
        std::fclose(pimpl_->temp_pool_fp);
        pimpl_->temp_pool_fp = nullptr;
    }
    // 删除临时池和 .partial (若存在)
    std::error_code ec;
    if (!pimpl_->temp_pool_path.empty()) {
        std::filesystem::remove(pimpl_->temp_pool_path, ec);
    }
    if (!pimpl_->partial_path.empty()) {
        std::filesystem::remove(pimpl_->partial_path, ec);
    }
    pimpl_->tile_dirs.clear();
    pimpl_->temp_pool_size = 0;
    pimpl_->opened = false;
    pimpl_->final_path.clear();
    pimpl_->partial_path.clear();
    pimpl_->temp_pool_path.clear();
}

// ---------------------------------------------------------------------------
// tile_count: 查询已记录的 Tile 数
// ---------------------------------------------------------------------------

size_t HissStreamWriter::tile_count() const {
    return pimpl_->tile_dirs.size();
}

} // namespace hiss
