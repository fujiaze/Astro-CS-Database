#ifndef HISS_STREAM_WRITER_H
#define HISS_STREAM_WRITER_H

// ============================================================================
// hiss_stream_writer.h - AstroCS HISS 流式写入器
//
// 依据:
//   - 02_FROZEN_STAGE1_HISS_SPEC.md §14 (HISS 容器: 流式生成临时子块池, 再生成
//     最终 Header, 组装 .partial, flush 后原子重命名)
//   - docs/stage1_fix/00_COMMON_CONTRACTS.md §4.5 (流式写入)
//   - docs/stage1_fix/spec.md 步骤10 (流式写入)
//
// 职责:
//   - 管理 .partial 临时子块池 (一个临时文件)
//   - add_tile 压缩后的子块立即写入临时池, 内存只保留 SubblockDescriptor
//   - finalize 生成 Header (含完整子块目录) + 合并临时池 + 原子重命名
//   - 原子替换使用 MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
//     (Windows), 不能先删除旧文件再 rename
//
// 设计说明:
//   - HissWriter 负责压缩逻辑 (调用 codec), HissStreamWriter 负责 I/O
//   - 每个 Tile 的子块压缩后立即 append 到 temp_pool_, 释放压缩数据内存
//   - 内存中只保留 SubblockDescriptor (offset/size/codec/checksum 等)
//   - finalize 时一次性生成 Header, 写入最终文件, 追加 temp_pool_ 内容
// ============================================================================

#include "hiss_format.h"

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

namespace hiss {

// ============================================================================
// HissStreamWriter - 流式写入器
// ============================================================================
class HissStreamWriter {
public:
    HissStreamWriter();
    ~HissStreamWriter();

    // 打开写入会话
    // final_path - 最终 .hiss 文件路径 (临时文件为 final_path + ".partial")
    // 返回 0=成功, <0=失败
    int open(const std::string& final_path);

    // 追加一个已压缩的子块到临时池
    // data/size - 压缩后的子块数据
    // desc - 子块描述符 (输入: type/flags/compressed_size/uncompressed_size/
    //                      codec_id/transform_id/checksum_type/checksum;
    //                      输出: offset 回填为临时池中的偏移)
    // 返回 0=成功, <0=失败
    int append_subblock(const uint8_t* data, size_t size,
                         HissSubblockDescriptor& desc);

    // 记录一个 Tile 的目录信息 (parent_ipix/tile_nside/occ_mode + 子块描述符列表)
    // subblocks - 子块描述符列表 (调用 append_subblock 后已填充 offset)
    // 返回 0=成功, <0=失败
    int record_tile(uint64_t parent_ipix, uint32_t tile_nside,
                     OccupancyMode occ_mode,
                     std::vector<HissSubblockDescriptor> subblocks);

    // finalize: 生成 Header, 组装最终文件, flush, 原子重命名
    // grid - 网格规格 (写入 Header)
    // metadata - 元数据 (序列化为 JSON 写入 Header)
    // 返回 0=成功, <0=失败
    int finalize(const HissGridSpec& grid, const HissMetadata& metadata);

    // 取消: 关闭并删除临时文件, 清理内存
    void cancel();

    // 查询已记录的 Tile 数 (用于测试验证流式写入不保留数据)
    size_t tile_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace hiss

#endif // HISS_STREAM_WRITER_H
