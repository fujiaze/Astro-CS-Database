#ifndef HISS_FORMAT_H
#define HISS_FORMAT_H

// ============================================================================
// hiss_format.h - AstroCS 1.0 HISS (HEALPix Image Storage System) 格式接口
//
// 已冻结规范 (见 Wiki HISS-Container-and-Tiles.md):
//   - XISF 式 Header + attachments, 无 Footer/Checkpoint
//   - 自适应 Tile (d = min(9, log2(NSIDE/16)))
//   - 独立子块: occupancy/signal/support/SNR/extension
//   - 每子块独立 codec/transform/checksum
//   - RAW 必须可用, 其他 codec 通过注册接口接入
//   - 内部 float64 几何, signal 最终 float32, support 最终 uint8
//   - NESTED ordering, ICRS 坐标系
//
// 未冻结事项 (见 Wiki Stage1-Decision-Status.md):
//   - DQ-001~007: codec/阈值/checksum/对齐 待 C++ 实验后由用户冻结
//   - 实验接口通过 HissCodecRegistry 和实验配置参数支持
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>

#ifdef _WIN32
#define HISS_EXPORT __declspec(dllexport)
#else
#define HISS_EXPORT __attribute__((visibility("default")))
#endif

namespace hiss {

// ============================================================================
// 1. 网格规格 (已冻结)
// ============================================================================

struct HissGridSpec {
    uint32_t nside = 0;          // HEALPix NSIDE (2 的幂)
    uint32_t tile_nside = 0;     // Tile 父级 NSIDE (>=16)
    int      ordering = 1;       // 1=NESTED (HISS 内部统一 NESTED, 0=RING 仅适配器用)
    int      radesys = 0;       // 0=ICRS (HISS 统一 ICRS)
    double   pixfrac = 1.0;     // 0 < pixfrac <= 1
};

// ============================================================================
// 2. Tile 自适应层级计算 (已冻结: 02_FROZEN §11)
//    d = min(9, log2(NSIDE/16))
//    tile_nside = NSIDE / 2^d
//    保证: 满 Tile 最多 4^9=262144 叶像素, tile_nside >= 16, 特征角尺度 <= 3.7°
// ============================================================================

HISS_EXPORT uint32_t compute_tile_depth(uint32_t nside);
HISS_EXPORT uint32_t compute_tile_nside(uint32_t nside);

// ============================================================================
// 3. 占用编码模式 (已冻结: 02_FROZEN §12)
// ============================================================================

enum class OccupancyMode : uint8_t {
    FULL         = 0,  // 全部叶像素有效, 无占用块
    BITMAP       = 1,  // 1 bit/潜在叶像素
    SPARSE_LIST  = 2,  // 有效局部索引列表
    // 切换阈值未冻结 (DQ-005), 由 Writer 按实验配置决定
};

// ============================================================================
// 4. 子块类型 (已冻结: 02_FROZEN §13)
// ============================================================================

enum class SubblockType : uint8_t {
    OCCUPANCY  = 0,  // 占用图 (FULL 时省略)
    SIGNAL     = 1,  // 已测光校准的累计通量 (float32)
    SUPPORT    = 2,  // 几何面积比例 (uint8, round(255*S))
    SNR        = 3,  // 稀疏 SNR 控制点
    EXTENSION  = 255, // 未来可选扩展
};

enum class SubblockFlags : uint16_t {
    REQUIRED = 0x0001,  // 必需子块 (未知必需块必须拒绝)
    OPTIONAL = 0x0002,  // 可选子块 (未知可选块可跳过)
};

// ============================================================================
// 5. Codec / Transform 注册 (未冻结: DQ-001~004, DQ-007)
//    RAW 必须可用, 其他 codec 通过注册接入
// ============================================================================

enum class CodecId : uint16_t {
    RAW     = 0,   // 无压缩 (必须可用)
    LZ4     = 1,
    ZSTD    = 2,
    // byte-shuffle + LZ4/Zstd 通过 transform_id 组合
};

enum class TransformId : uint16_t {
    NONE        = 0,
    BYTE_SHUFFLE = 1,  // 字节重排 (配合 LZ4/Zstd)
    DELTA       = 2,   // 差分编码 (SPARSE_LIST 用)
    VARINT      = 3,   // 变长整数 (SPARSE_LIST 用)
};

enum class ChecksumType : uint8_t {
    NONE     = 0,  // 仅基线, 不作为推荐
    CRC32C   = 1,
    XXHASH   = 2,
    // 最终算法待 DQ-006 确认
};

// ============================================================================
// 6. 子块目录项 (已冻结: 02_FROZEN §15)
//    每个子块独立记录所有元数据
// ============================================================================

struct HissSubblockDescriptor {
    SubblockType   type;            // 子块类型
    uint16_t       flags;           // required/optional
    uint64_t       offset;          // 文件内偏移
    uint64_t       compressed_size;  // 压缩后字节数
    uint64_t       uncompressed_size; // 压缩前字节数
    CodecId        codec_id;        // 压缩算法
    TransformId    transform_id;    // 前置变换
    ChecksumType   checksum_type;   // 校验算法
    uint64_t       checksum;        // 校验值
};

// ============================================================================
// 7. Tile 数据结构 (已冻结: 02_FROZEN §11/§12/§13)
// ============================================================================

struct HissTile {
    uint64_t                       parent_ipix;   // NESTED 父像素 ipix
    uint32_t                       tile_nside;    // Tile 父级 NSIDE
    OccupancyMode                  occ_mode;      // 占用模式
    std::vector<HissSubblockDescriptor> subblocks; // 子块目录
};

// ============================================================================
// 8. SNR 控制点 (已冻结: 02_FROZEN §17)
//    每点仅 local_ipix(uint32) + snr(float32)
// ============================================================================

struct HissSnrControlPoint {
    uint32_t local_ipix;  // Tile 内局部 ipix
    float    snr;         // SNR 值
};

struct HissSnrBlock {
    std::vector<HissSnrControlPoint> points;  // 控制点列表
    double snr_phot = 0.0;       // 全局标量 (子块头保存一次)
    double median_snr = 0.0;     // 归一化基准
    double idw_power = 2.0;      // IDW 幂次
};

// ============================================================================
// 9. 元数据 (已冻结: 02_FROZEN §16)
//    精简 FITS 风格, 不保存完整 WCS/SIP
// ============================================================================

struct HissMetadata {
    // 必需容器信息
    uint32_t nside = 0;
    uint32_t tile_nside = 0;
    int      ordering = 1;       // NESTED
    int      radesys = 0;       // ICRS
    double   pixfrac = 1.0;

    // 测光字段
    double   photscal = 1.0;     // 实际应用比例
    int      photappl = 0;       // 是否已应用
    char     bunit[32] = "ASTROCS_RELATIVE_FLUX";

    // 校准字段
    char     calmode[32] = "";   // 实际采用模式
    char     darkreq[32] = "";   // 用户请求模式
    char     darkmode[32] = "";  // 实际模式
    double   darkscl = 1.0;      // 最终 k

    // 传统 FITS 字段 (按输入继承)
    char     object[128] = "";
    char     date_obs[64] = "";
    double   exptime = 0.0;
    char     filter[64] = "";
    char     telescop[64] = "";
    char     instrume[64] = "";
    double   gain = 0.0;

    // 历史/诊断
    std::string history;

    // 序列化为 JSON (用于 Header)
    HISS_EXPORT std::string to_json() const;
    HISS_EXPORT int from_json(const std::string& json);
};

// ============================================================================
// 10. Drizzle Tile 累加器 (已冻结: 02_FROZEN §8/§10)
//     float64 内部累加, 最终输出 float32 signal + uint8 support
// ============================================================================

struct DrizzleTileAccumulator {
    // 按 Tile 局部 ipix 索引的累加器
    struct Accum {
        double sum_flux = 0.0;     // Σ L_j * a_jp / A_j_drop
        double sum_area = 0.0;     // Σ a_jp (用于 support)
        uint32_t n_contrib = 0;    // 贡献源像素数 (诊断用)
    };
    std::vector<Accum> pixels;      // 按 Tile 内局部 ipix 排列
    uint32_t tile_nside = 0;
    uint64_t parent_ipix = 0;

    // 最终输出
    void finalize_signal(std::vector<float>& signal) const;   // float32
    void finalize_support(std::vector<uint8_t>& support) const; // uint8 round(255*S)
    // support 范围检查: 仅浮点误差级超限可钳制; 明显超 1 是错误
    HISS_EXPORT int validate_support() const; // 0=OK, <0=错误
};

// ============================================================================
// 11. Writer 接口 (已冻结: 02_FROZEN §14 + 04_IMPLEMENTATION §4)
//     Header 前置, attachments 后置, .partial 原子提交
//     不实现 Checkpoint/Footer/断点恢复
// ============================================================================

class HissWriter {
public:
    HISS_EXPORT HissWriter();
    HISS_EXPORT ~HissWriter();

    // 初始化写入会话
    // output_path - 最终 .hiss 文件路径
    // grid - 网格规格
    // metadata - 元数据
    // 返回 0=成功, <0=失败
    HISS_EXPORT int open(const std::string& output_path,
                         const HissGridSpec& grid,
                         const HissMetadata& metadata);

    // 添加一个 Tile 的数据
    // parent_ipix - Tile 父像素 NESTED ipix
    // acc - Drizzle 累加器 (float64 内部)
    // snr - 可选 SNR 控制点 (nullptr 则无 SNR 子块)
    // occ_mode - 占用模式 (FULL/BITMAP/SPARSE_LIST)
    // 返回 0=成功, <0=失败
    HISS_EXPORT int add_tile(uint64_t parent_ipix,
                              const DrizzleTileAccumulator& acc,
                              const HissSnrBlock* snr,
                              OccupancyMode occ_mode);

    // 完成写入: 生成 Header, 组装 .partial, flush, 原子重命名
    // 返回 0=成功, <0=失败
    HISS_EXPORT int finalize();

    // 取消并清理临时文件
    HISS_EXPORT void cancel();

    // 设置实验性 codec/transform (未冻结, 仅供实验)
    // 默认: signal=RAW, support=RAW, bitmap=RAW, sparse=RAW
    HISS_EXPORT void set_experiment_codec(SubblockType type,
                                           CodecId codec,
                                           TransformId transform);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

// ============================================================================
// 12. Reader 接口 (已冻结: 02_FROZEN §14 + 04_IMPLEMENTATION §5)
//     按目录读取, 不依赖物理顺序
//     未知可选块跳过; 未知必需块报不兼容
//     检查 offset/size 越界, 解压长度, checksum
//     用 NSIDE/NESTED/ICRS 定位, 不依赖 WCS
// ============================================================================

class HissReader {
public:
    HISS_EXPORT HissReader();
    HISS_EXPORT ~HissReader();

    // 打开 HISS 文件
    // path - .hiss 文件路径
    // 返回 0=成功, <0=失败
    HISS_EXPORT int open(const std::string& path);

    // 获取网格规格和元数据
    HISS_EXPORT HissGridSpec grid() const;
    HISS_EXPORT HissMetadata metadata() const;

    // 获取 Tile 列表
    HISS_EXPORT const std::vector<HissTile>& tiles() const;

    // 按 Tile 父 ipix 读取数据
    // parent_ipix - Tile 父像素 NESTED ipix
    // signal - 输出 signal 数组 (float32)
    // support - 输出 support 数组 (uint8)
    // 返回 0=成功, <0=失败 (含 checksum 错误, 越界)
    HISS_EXPORT int read_tile(uint64_t parent_ipix,
                               std::vector<float>& signal,
                               std::vector<uint8_t>& support) const;

    // 只读 signal
    HISS_EXPORT int read_tile_signal(uint64_t parent_ipix,
                                      std::vector<float>& signal) const;

    // 只读 support
    HISS_EXPORT int read_tile_support(uint64_t parent_ipix,
                                       std::vector<uint8_t>& support) const;

    // 读取 SNR 控制点
    HISS_EXPORT int read_tile_snr(uint64_t parent_ipix,
                                   HissSnrBlock& snr) const;

    // 查询某位置的 signal/support (通过 NSIDE/NESTED/ICRS 定位)
    // ra, dec - 度
    HISS_EXPORT int query_pixel(double ra, double dec,
                                float* signal, uint8_t* support) const;

    // 关闭文件
    HISS_EXPORT void close();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

// ============================================================================
// 13. Codec 注册表 (未冻结: DQ-001~004)
//     允许实验性 codec 通过注册接入
// ============================================================================

using CompressFunc = std::function<int(const uint8_t* input, size_t input_size,
                                        uint8_t* output, size_t* output_size)>;
using DecompressFunc = std::function<int(const uint8_t* input, size_t input_size,
                                          uint8_t* output, size_t output_size)>;

struct CodecEntry {
    CodecId id;
    std::string name;
    CompressFunc compress;
    DecompressFunc decompress;
    size_t (*bound)(size_t) = nullptr;
};

class CodecRegistry {
public:
    HISS_EXPORT static CodecRegistry& instance();

    // 注册 codec (RAW 已内置, 不可覆盖)
    HISS_EXPORT int register_codec(const CodecEntry& entry);

    // 查找 codec
    HISS_EXPORT const CodecEntry* find(CodecId id) const;

    // 获取所有已注册 codec (用于 benchmark)
    HISS_EXPORT std::vector<CodecId> list() const;
};

// ============================================================================
// 14. 结构化诊断 (已冻结: 02_FROZEN §2.3)
//     最优 Dark 失败时输出诊断, 自动回退
// ============================================================================

struct Stage1Diagnostics {
    int      success = 0;          // 0=成功, <0=失败
    char     stage[32] = "";       // 阶段名
    char     code[32] = "";        // 错误码
    char     message[256] = "";    // 人类可读信息
    int      fell_back = 0;        // 是否回退 (0=未回退, 1=已回退)
    char     fallback_from[32] = ""; // 原模式
    char     fallback_to[32] = "";   // 回退后模式
};

} // namespace hiss

#endif // HISS_FORMAT_H
