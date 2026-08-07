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
//   - 内部 float64 几何, signal 最终 float32 (FP32 默认) 或 float64 (FP64 模式), support 最终 uint8
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
// 1.1 Tile 几何 (依据 02_FROZEN §11, 00_COMMON_CONTRACTS §2.1)
//     详细定义见 src/hiss_tile_model.h
//     通过 make_tile_geometry(nside) / make_tile_geometry_for_parent(nside, parent) 构造
//
//     关键公式 (修正了旧版 "tile_nside^2 * 12" 错误):
//       d              = min(9, log2(NSIDE/16))
//       tile_nside     = NSIDE / 2^d
//       n_leaf_per_tile = 4^d = (NSIDE / tile_nside)^2
//       满 Tile 最多 4^9 = 262144 个叶像素
//
//     使用方式:
//       #include "hiss_tile_model.h"
//       hiss::HissTileGeometry g = hiss::make_tile_geometry(64);
//       // g.depth=2, g.tile_nside=16, g.n_leaf_per_tile=16
// ============================================================================

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
//
//    R04-B17: 扩展子块命名空间
//      内置类型 (OCCUPANCY/SIGNAL/SUPPORT/SNR): ext_type_id = 0
//      扩展类型 (EXTENSION): ext_type_id != 0, 标识扩展命名空间
//      Reader 遇到未知必需扩展 (EXTENSION + ext_type_id 未知 + REQUIRED) 必须拒绝
// ============================================================================

enum class SubblockType : uint8_t {
    OCCUPANCY  = 0,  // 占用图 (FULL 时省略; BITMAP/SPARSE 时为必需子块)
    SIGNAL     = 1,  // 已测光校准的累计通量 (float32 或 float64, 取决于 metadata 中的 signal_dtype)
    SUPPORT    = 2,  // 几何面积比例 (uint8, round(255*S))
    SNR        = 3,  // 稀疏 SNR 控制点
    EXTENSION  = 255, // 未来可选扩展 (需配合 ext_type_id 标识命名空间)
};

enum class SubblockFlags : uint16_t {
    REQUIRED = 0x0001,  // 必需子块 (未知必需块必须拒绝)
    OPTIONAL = 0x0002,  // 可选子块 (未知可选块可跳过)
};

// 内置扩展命名空间 ID (ext_type_id, 用于 EXTENSION 类型子块)
// 0 reserved for 内置类型 (OCCUPANCY/SIGNAL/SUPPORT/SNR)
// 1-32767: 已注册扩展命名空间
// 32768-65535: 私有/实验扩展命名空间
enum class ExtensionNamespace : uint16_t {
    BUILTIN      = 0,     // 内置类型 (非 EXTENSION)
    RESERVED_LO  = 1,     // 已注册扩展起点
    PRIVATE_LO   = 32768, // 私有扩展起点
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
    VARINT      = 3,   // 变长整数 (SPARSE_LIST 用, 旧值, 向后兼容映射到 DELTA_VARINT)
    DELTA_VARINT = 4,  // 差分 + 变长整数组合编码 (WP-G 步骤12 新增)
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
//
//    R04-B17: 新增 ext_type_id 字段
//      - 内置类型 (OCCUPANCY/SIGNAL/SUPPORT/SNR): ext_type_id = 0
//      - 扩展类型 (EXTENSION): ext_type_id 标识扩展命名空间
//      - Reader 遇到未知必需扩展必须拒绝 (HISS_ERR_UNKNOWN_REQUIRED)
//
//    磁盘格式 (42 字节, 显式小端序):
//      type(1) + ext_type_id(2) + flags(2) + offset(8) + compressed_size(8) +
//      uncompressed_size(8) + codec_id(2) + transform_id(2) +
//      checksum_type(1) + checksum(8) = 42
// ============================================================================

struct HissSubblockDescriptor {
    SubblockType   type;            // 子块类型
    uint16_t       ext_type_id;     // 扩展命名空间 ID (0=内置, 非0=扩展)
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
// 8. SNR 控制点 (已冻结: 02_FROZEN §17, 00_COMMON_CONTRACTS §2.5)
//    每点仅 local_ipix(uint32) + snr(float32)
//
// SNR 子块二进制布局 (R04-B18: 新增 block 级 estimator_id/sampling_scale):
//   [estimator_id:  uint32 LE]   — 估计器 ID (block 级)
//   [sampling_scale: float32 LE] — 采样尺度 (block 级)
//   [n_points:      uint32 LE]   — 控制点数 (= count, block 级)
//   [points: n_points * 8B]      — 每点 local_ipix(uint32) + snr(float32)
//   不包含 snr_phot/median_snr/idw_power (这些是估计器状态, 不写入 HISS)
//
// 重复点处理: Writer 按升序排序 local_ipix, 重复点保留首次出现 (确定性规则)
// 无覆盖点: 不得写入 (Stage1 映射到 Tile 后才写入, 不静默丢失)
// ============================================================================

struct HissSnrControlPoint {
    uint32_t local_ipix;  // Tile 内局部 ipix
    float    snr;         // SNR 值
};

// R11: FP64 SNR 控制点 (HISS snr_dtype=1, 每点 12B: local_ipix u32 + snr f64)
struct HissSnrControlPointF64 {
    uint32_t local_ipix;
    double   snr;
};

// HissSnrBlock: SNR 子块在内存中的表示
// 依据 02_FROZEN §17 和 00_COMMON_CONTRACTS §2.5, HISS 文件中保存
// estimator_id + sampling_scale + n_points + points (每点 local_ipix + snr),
// 不保存估计器状态量 (snr_phot/median_snr/idw_power)。
// R04-B18: 新增 block 级 estimator_id/sampling_scale, count = points.size()
struct HissSnrBlock {
    uint32_t estimator_id = 0;        // 估计器 ID (block 级, R04-B18)
    float    sampling_scale = 0.0f;   // 采样尺度 (block 级, R04-B18)
    std::vector<HissSnrControlPoint> points;  // 控制点列表 (count = points.size())
};

// R11: FP64 SNR 块 (snr_dtype=1)
struct HissSnrBlockF64 {
    uint32_t estimator_id = 0;
    float    sampling_scale = 0.0f;
    std::vector<HissSnrControlPointF64> points;
};

// ============================================================================
// 8.1 HISS 错误码 (依据 00_COMMON_CONTRACTS §3.3)
//     Reader 在遇到未知必需子块时返回 HISS_ERR_UNKNOWN_REQUIRED
//     R04-B16: 新增 HISS_ERR_FORMAT_VIOLATION 用于严格格式校验失败
// ============================================================================
#define HISS_OK                     0
#define HISS_ERR_INVALID_ARG       -1   // 非法参数
#define HISS_ERR_INVALID_STATE     -2   // 非法状态
#define HISS_ERR_IO                -3   // I/O 错误
#define HISS_ERR_CHECKSUM          -4   // 校验失败
#define HISS_ERR_FORMAT            -5   // 格式错误
#define HISS_ERR_UNSUPPORTED       -6   // 不支持的特性
#define HISS_ERR_UNKNOWN_REQUIRED  -7   // 未知必需子块 (规范 §13 要求拒绝)
#define HISS_ERR_FORMAT_VIOLATION  -8   // 严格格式校验失败 (R04-B16: 越界/重叠/重复/非法关系)

// ============================================================================
// 8.2 HISS 容器签名与 Header TLV 常量 (R04-B14/B15)
//     签名块: magic[8]="HISS0100" + header_length(u32 LE) + feature_flags(u32 LE) = 16B
//     Header: 一系列 TLV (tag:u16 LE + flags:u8 + length:u32 LE + value)
// ============================================================================

// 固定签名块大小 (字节)
#define HISS_SIGNATURE_SIZE 16

// feature_flags 位定义
#define HISS_FEAT_TLV_HEADER   0x00000001u  // 使用 TLV Header (必须)
#define HISS_FEAT_HAS_EXT_BLKS 0x00000002u  // 含扩展子块

// Header TLV tag (uint16 LE)
#define HISS_TLV_SCHEMA_FINGERPRINT 0x0001  // schema 指纹 (required)
#define HISS_TLV_GRID_SPEC          0x0002  // 网格规格 (required)
#define HISS_TLV_METADATA_JSON      0x0003  // 元数据 JSON (optional, 人类可读附件)
#define HISS_TLV_TILE_DIRECTORY     0x0004  // Tile 目录 (required)
#define HISS_TLV_FEATURE_REQ        0x0005  // 必需特性列表 (optional)

// TLV flags 位定义
#define HISS_TLV_FLAG_REQUIRED 0x01  // 必需 TLV (未知必需 → 拒绝)
#define HISS_TLV_FLAG_OPTIONAL 0x00  // 可选 TLV (未知可选 → 跳过)

// 磁盘子块描述符大小 (字节, R04-B17: 含 ext_type_id)
// type(1) + ext_type_id(2) + flags(2) + offset(8) + compressed_size(8) +
// uncompressed_size(8) + codec_id(2) + transform_id(2) + checksum_type(1) + checksum(8) = 42
#define HISS_SUBBLOCK_DESC_DISK_SIZE 42

// 冻结资源上限 (Gate 4 fuzz): 单子块解压大小上限 64 MiB
// (生产最大子块: NSIDE=2^22 Tile signal f64 = 262144*8 = 2 MiB, 留 32 倍裕量)
#define HISS_MAX_SUBBLOCK_UNCOMPRESSED (64ull << 20)
// HISS 网格 NSIDE 上限 (冻结支持域, 2^14~2^22 高 NSIDE 门)
#define HISS_MAX_NSIDE (1u << 22)

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

    // 精度模式 (R10: FP32/FP64 双模式)
    uint8_t precision_mode = 0;  // 0=FP32 (binary32), 1=FP64 (binary64)
    uint8_t signal_dtype = 0;    // 0=float32, 1=float64 (与 precision_mode 一致)
    // R11: SNR 稀疏控制点与科学 metadata 浮点 dtype (HISS-102 / PREC-110)
    uint8_t snr_dtype = 0;           // 0=float32 (snr 值), 1=float64
    uint8_t metadata_float_dtype = 0; // 0=float32, 1=float64 (随 precision_mode)

    // 序列化为 JSON (用于 Header)
    HISS_EXPORT std::string to_json() const;
    HISS_EXPORT int from_json(const std::string& json);
};

// ============================================================================
// 10. Drizzle Tile 累加器 (已冻结: 02_FROZEN §8/§10)
//     float64 内部累加, 最终输出 float32 signal + uint8 support
//
//     语义修正 (依据 00_COMMON_CONTRACTS §2.2, spec.md 步骤2/7):
//       signal[p]  = float(sumFlux)                       — 累计通量 (不除面积)
//       support[p] = uint8(round(255 * clamp(S, 0, 1)))   — 面积比
//       其中 S = sum_area / A_p, A_p = 目标 HEALPix 像素面积 (球面度)
//
//     A_p 通过成员变量 pixel_area 传入 (调用方在 finalize_* 前设置)。
//     默认值 1.0 仅为向后兼容, 调用方应正确设置为 hp.pixel_area()。
// ============================================================================

struct DrizzleTileAccumulator {
    // 按 Tile 局部 ipix 索引的累加器
    struct Accum {
        double sum_flux = 0.0;     // Σ L_j * (a_jp / A_j_drop) — 累计通量 (02_FROZEN §8)
        double sum_area = 0.0;     // Σ a_jp — 球面重叠面积 (球面度, 未归一化)
        uint32_t n_contrib = 0;    // 贡献源像素数 (诊断用)
    };
    std::vector<Accum> pixels;      // 按 Tile 内局部 ipix 排列
    uint32_t tile_nside = 0;
    uint64_t parent_ipix = 0;

    // 目标 HEALPix 像素面积 A_p (球面度), 用于 support 归一化
    // 调用方需在 finalize_support/validate_support 前设置为 hp.pixel_area()
    // 默认 1.0 仅向后兼容 (旧调用未设置时退化为 sum_area 直接作为 S)
    double pixel_area = 1.0;

    // 最终输出
    // signal: 直接保存累计通量 (不除面积), 无贡献像素 sum_flux=0 自然写 0
    HISS_EXPORT void finalize_signal(std::vector<float>& signal) const;   // float32, = float(sumFlux)
    // FP64 模式: 直接输出 float64 signal (不损失精度)
    HISS_EXPORT void finalize_signal_f64(std::vector<double>& signal) const; // float64, = sumFlux
    // support: S = sum_area / pixel_area, 钳制 [0,1], uint8 = round(255*S)
    HISS_EXPORT void finalize_support(std::vector<uint8_t>& support) const; // uint8 round(255*S/A_p)
    // support 范围检查 (基于归一化后的 S): 仅浮点误差级超限可钳制; 明显超 1 是错误
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

    // FP64 模式: 添加 Tile 数据 (float64 signal)
    // 与 add_tile 类似, 但 signal 子块写入 float64 数据
    // 调用后 metadata 中的 precision_mode/signal_dtype 自动设置为 1 (FP64)
    // 返回 0=成功, <0=失败
    HISS_EXPORT int add_tile_f64(uint64_t parent_ipix,
                                  const DrizzleTileAccumulator& acc,
                                  const HissSnrBlock* snr,
                                  OccupancyMode occ_mode);

    // FP64 模式 + FP64 SNR 控制点 (BLOCKER-TYPE-002):
    // signal 子块 float64, SNR 子块直接写 f64 控制点 (12B/点, snr_dtype=1)
    HISS_EXPORT int add_tile_f64_snr(uint64_t parent_ipix,
                                     const DrizzleTileAccumulator& acc,
                                     const HissSnrBlockF64* snr,
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

    // 设置实验性 transform (未冻结, 仅供实验, WP-G 步骤12 新增)
    // 仅修改 transform, 保留已有 codec 设置 (若未设置则默认 RAW)
    HISS_EXPORT void set_experiment_transform(SubblockType type,
                                                TransformId transform);

    // 设置实验性 checksum (未冻结, 仅供实验, INTERIM_BASELINE_NOT_FROZEN)
    // 默认: 所有子块 checksum_type=NONE (不计算校验)
    // 实验时可启用 CRC32C 等候选算法, 通过 ChecksumRegistry 查找实现
    // checksum 计算的是压缩后数据 (与 Reader 端一致, Reader 在解压前校验)
    HISS_EXPORT void set_experiment_checksum(SubblockType type,
                                               ChecksumType checksum);

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

    // 查询 precision mode 和 signal dtype (R10: FP32/FP64 双模式)
    // precision_mode: 0=FP32, 1=FP64
    // signal_dtype: 0=float32, 1=float64
    HISS_EXPORT uint8_t precision_mode() const;
    HISS_EXPORT uint8_t signal_dtype() const;

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

    // FP64 模式: 只读 signal (float64)
    // 若文件 signal_dtype=1 (FP64), 直接返回 float64 数据
    // 若文件 signal_dtype=0 (FP32), 返回错误 (禁止静默转换)
    HISS_EXPORT int read_tile_signal_f64(uint64_t parent_ipix,
                                          std::vector<double>& signal) const;

    // 只读 support
    HISS_EXPORT int read_tile_support(uint64_t parent_ipix,
                                       std::vector<uint8_t>& support) const;

    // 读取 SNR 控制点
    HISS_EXPORT int read_tile_snr(uint64_t parent_ipix,
                                   HissSnrBlock& snr) const;

    // R11: 读取 FP64 SNR 控制点 (仅 snr_dtype=1 文件; f32 文件返回错误, 禁止静默转换)
    HISS_EXPORT int read_tile_snr_f64(uint64_t parent_ipix,
                                       HissSnrBlockF64& snr) const;

    // 查询某位置的 signal/support (通过 NSIDE/NESTED/ICRS 定位)
    // ra, dec - 度
    HISS_EXPORT int query_pixel(double ra, double dec,
                                float* signal, uint8_t* support) const;

    // R10: 查询某位置的 signal (FP64) / support
    // 仅适用于 FP64 模式文件 (signal_dtype=1); FP32 文件会返回错误 (禁止静默转换)
    // ra, dec - 度
    // signal - 输出参数 (单个 double 值)
    // support - 输出参数 (单个 uint8_t 值, 与 FP32 版本一致)
    HISS_EXPORT int query_pixel_f64(double ra, double dec,
                                    double* signal, uint8_t* support) const;

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
// 13.1 Checksum 注册表 (未冻结: DQ-006, INTERIM_BASELINE_NOT_FROZEN)
//     允许实验性 checksum 算法通过注册接入, 默认 checksum_type=NONE
//     CRC32C 内置 (复用 hiss_reader.cpp 的实现, 已移至 hiss_codec.cpp 共享)
//     XXHASH 等其他算法待 DQ-006 确认后通过 register_checksum 接入
//
// 设计与 CodecRegistry 对称:
//   - 头文件中仅声明类, 私有状态以文件作用域静态容器承载 (见 hiss_codec.cpp)
//   - Meyers singleton, 首次访问时线程安全地初始化并注册内置 checksum
//   - register/find/list 经 std::mutex 保护
// ============================================================================

using ChecksumFunc = std::function<uint64_t(const uint8_t*, size_t)>;

struct ChecksumEntry {
    ChecksumType id;
    std::string  name;
    ChecksumFunc compute;  // 计算校验值, 返回 uint64_t (由具体算法填充, 高位补 0)
};

class ChecksumRegistry {
public:
    HISS_EXPORT static ChecksumRegistry& instance();

    // 注册 checksum (NONE 不允许注册, 返回 <0)
    // 已存在则覆盖 (更新实现)
    HISS_EXPORT int register_checksum(const ChecksumEntry& entry);

    // 查找 checksum, 未找到返回 nullptr
    HISS_EXPORT const ChecksumEntry* find(ChecksumType id) const;

    // 列出所有已注册 checksum (NONE 不在列表中)
    HISS_EXPORT std::vector<ChecksumType> list() const;
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
