// gradient_sampler.h - 阶段1: 球面背景采样
//
// 功能:
//   逐帧流式读取 .hiss, 在每帧 FOV 内布置控制点网格,
//   对每个控制点 query_disc 邻域 → Gaia 星拒绝 → 零值拒绝 → bg_median + SNR 评估,
//   输出样本表 (cp_ra, cp_dec, frame_id, bg_median, snr, leaf_ipix_nside64)。
//
// 用途:
//   为阶段2 (gradient_fitter Gauss-Seidel 迭代) 提供输入样本。
//
// 依赖:
//   - healpix_core (HEALpix 像素运算, 静态编译)
//   - healpix_io.dll (hiss_read_snr_model 读取稀疏 SNR 模型)
//   - snr_evaluator (KD-tree IDW 重建逐像素 SNR)
//   - gaia_client.dll (Gaia 星表查询, 子叶批量接口)
//
// 设计文档: .trae/specs/snr-compact-storage-and-gradient-correction/spec.md §3.3

#ifndef GRADIENT_SAMPLER_H
#define GRADIENT_SAMPLER_H

#include <cstdint>
#include <string>
#include <vector>

namespace gradient {

// ============================================================================
// 样本表行 (一个控制点 × 一帧 = 一行)
// ============================================================================
#pragma pack(push, 1)
struct SampleRow {
    double  cp_ra;               // 控制点赤经 (度)           [0:8]
    double  cp_dec;              // 控制点赤纬 (度)           [8:16]
    int32_t frame_id;            // 帧索引 (0-based)          [16:20]
    float   bg_median;           // 背景中位数                [20:24]
    float   snr;                 // SNR-B (IDW 重建)          [24:28]
    int64_t leaf_ipix_nside64;   // nside=64 子叶 ipix        [28:36]
};
#pragma pack(pop)
static_assert(sizeof(SampleRow) == 36, "SampleRow must be 36 bytes");

// ============================================================================
// 帧信息 (采样前由调用方准备)
// ============================================================================
struct FrameInfo {
    std::string hiss_path;       // .hiss 文件路径
    int32_t     frame_id;        // 帧索引 (0-based, 唯一)
    // FOV 由 .hiss 内 ipix 集合自动计算, 无需外部传入
};

// ============================================================================
// 采样参数
// ============================================================================
struct SamplerParams {
    // 控制点网格
    int     k_target = 100;      // 每帧目标控制点数 (nside_i 选最小 2^k 使 FOV 内像素数 ≥ k_target)
    int     nside_i_min = 64;    // nside_i 下限 (避免过低分辨率)
    int     nside_i_max = 8192;  // nside_i 上限 (避免过高分辨率)

    // 邻域采样
    double  neighborhood_factor = 2.0;  // query_disc 半径 = factor × pixel_size_i

    // Gaia 星拒绝
    double  star_reject_base_arcsec = 5.0;   // 基础拒绝半径 (角秒)
    double  star_reject_growth_rate = 0.01;  // 通量增长率 (角秒/ADU)
    double  star_reject_mag_high = 18.0;     // Gaia 星等上限 (只拒绝亮于 this 的星)

    // 零值拒绝
    float   zero_threshold = 0.0f;  // 像素值 ≤ this 视为零值 (边缘/无数据)

    // 降采样
    int     max_samples_per_frame = 500;  // 每帧最大样本数 (超过则 NESTED Morton 合并)
    bool    enable_downsample = true;     // 是否启用降采样

    // SNR 评估
    double  idw_power = 2.0;  // IDW 幂次 (与 snr_model 一致, 若 snr_model 含 idw_power 则覆盖)

    // 马赛克总 FOV 定义域截断 (spec §3.3.1)
    // 仅保留落在马赛克总 FOV 内的控制点, 定义域外不考虑
    // 默认 0 = 不截断 (单帧或全天空)
    double  mosaic_fov_ra = 0.0;         // 马赛克总 FOV 中心赤经 (度)
    double  mosaic_fov_dec = 0.0;        // 马赛克总 FOV 中心赤纬 (度)
    double  mosaic_fov_radius_deg = 0.0; // 马赛克总 FOV 半径 (度), 0 = 不截断
};

// ============================================================================
// 采样结果
// ============================================================================
struct SampleResult {
    std::vector<SampleRow> rows;   // 样本表 (全帧合并)
    std::vector<int32_t>   frame_ids;  // 实际采样的 frame_id 列表
    // 统计
    int     total_samples = 0;
    int     n_frames_processed = 0;
    int     n_frames_skipped = 0;  // FOV 内像素数 < k_target 的帧
};

// ============================================================================
// GradientSampler 类
// ============================================================================
class GradientSampler {
public:
    GradientSampler();
    ~GradientSampler();

    // 采样主入口 (逐帧流式处理)
    // frames: 帧信息数组
    // n_frames: 帧数
    // gaia_data_dir: Gaia 数据库目录 (传给 gaia_client_create)
    // params: 采样参数
    // result: 输出采样结果
    // 返回: 0=成功, 非0=失败
    int sample(const FrameInfo* frames, int n_frames,
               const char* gaia_data_dir,
               const SamplerParams& params,
               SampleResult& result);

    // 获取最后一次错误信息
    const std::string& lastError() const { return error_msg_; }

private:
    std::string error_msg_;
};

} // namespace gradient

#endif // GRADIENT_SAMPLER_H
