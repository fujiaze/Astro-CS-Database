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

#ifdef _WIN32
#include <windows.h>
#endif

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
};

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
// finalize: 生成 Header, 组装最终文件, flush, 原子重命名
//   最终布局: 签名块(20B) → Header → 子块1 → 子块2 → ...
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

    // 2. 计算元数据 JSON
    std::string json = metadata.to_json();

    // 3. 计算 Header 大小
    //    Header = 网格规格(24) + json_len(4) + json(变长) + tile_count(4) +
    //             Σ(15 + 40 * n_subblocks)
    size_t header_size = 24 + 4 + json.size() + 4;
    for (const auto& t : pimpl_->tile_dirs) {
        header_size += HISS_TILE_DIR_PREFIX_SIZE +
                       HISS_SUBBLOCK_DESCRIPTOR_SIZE * t.subblocks.size();
    }

    // 4. 调整所有子块 offset: 临时池偏移 → 最终文件偏移
    //    最终 offset = 签名块(20) + Header大小 + 临时池偏移
    //    (在构建 Header 字节流之前调整, 确保写入 Header 的 offset 是最终值)
    uint64_t base_offset = HISS_SIGNATURE_SIZE + header_size;
    for (auto& t : pimpl_->tile_dirs) {
        for (auto& sb : t.subblocks) {
            sb.offset += base_offset;
        }
    }

    // 5. 构建 Header 字节流
    ByteBuf hdr;
    // a. 网格规格
    hdr.u32(grid.nside);
    hdr.u32(grid.tile_nside);
    hdr.u32((uint32_t)grid.ordering);
    hdr.u32((uint32_t)grid.radesys);
    hdr.f64(grid.pixfrac);
    // b. 元数据 JSON
    hdr.u32((uint32_t)json.size());
    if (!json.empty()) hdr.bytes(json.data(), json.size());
    // c. Tile 数量
    hdr.u32((uint32_t)pimpl_->tile_dirs.size());
    // d. 每个 Tile 的目录
    for (const auto& t : pimpl_->tile_dirs) {
        hdr.u64(t.parent_ipix);
        hdr.u32(t.tile_nside);
        hdr.u8((uint8_t)t.occ_mode);
        hdr.u16((uint16_t)t.subblocks.size());
        for (const auto& sb : t.subblocks) {
            hdr.u8 ((uint8_t)sb.type);
            hdr.u16(sb.flags);
            hdr.u64(sb.offset);
            hdr.u64(sb.compressed_size);
            hdr.u64(sb.uncompressed_size);
            hdr.u16((uint16_t)sb.codec_id);
            hdr.u16((uint16_t)sb.transform_id);
            hdr.u8 ((uint8_t)sb.checksum_type);
            hdr.u64(sb.checksum);
        }
    }

    if (hdr.data.size() != header_size) {
        fprintf(stderr,
                "[hiss][stream] finalize 警告: Header 实际大小 %zu 与预算 %zu 不符 (内部 bug)\n",
                hdr.data.size(), header_size);
        // 公式确定, 理论上不会走到这里; 仅记录, 不调整 offset (避免二次偏移)
    }

    // 6. 创建最终 .partial 文件: 签名块 + Header + 子块数据(从临时池复制)
    FILE* fp_out = aio_fopen_utf8(pimpl_->partial_path.c_str(), "wb");
    if (!fp_out) {
        fprintf(stderr, "[hiss][stream] finalize 失败: 无法创建 .partial %s\n",
                pimpl_->partial_path.c_str());
        return -2;
    }

    // 6a. 写入签名块 (20B: MAGIC + version + header_offset)
    uint8_t sig[20] = {0};
    std::memcpy(sig, HISS_MAGIC, 8);
    uint32_t ver = HISS_VERSION;
    std::memcpy(sig + 8, &ver, 4);
    uint64_t header_offset = HISS_SIGNATURE_SIZE;  // Header 紧跟签名块
    std::memcpy(sig + 12, &header_offset, 8);
    if (std::fwrite(sig, 1, HISS_SIGNATURE_SIZE, fp_out) != HISS_SIGNATURE_SIZE) {
        fprintf(stderr, "[hiss][stream] finalize 失败: 写入签名块失败\n");
        std::fclose(fp_out);
        return -3;
    }

    // 6b. 写入 Header
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

    // 7. flush + 关闭 .partial
    if (std::fflush(fp_out) != 0) {
        fprintf(stderr, "[hiss][stream] finalize 警告: fflush 失败\n");
    }
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
            "[hiss][stream] finalize 成功: tiles=%zu header_offset=%llu header_size=%zu "
            "total_size=%llu path=%s\n",
            pimpl_->tile_dirs.size(),
            (unsigned long long)header_offset,
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
