// snr_evaluator.h - SNR 稀疏控制点模型 IDW 评估器
// 功能: 从 SnrModel (稀疏控制点) 用球面 IDW 重建任意点 SNR
// 用途: drizzle 端逐像素 SNR 重建; 梯度校正阶段 SNR 评估
//
// 算法:
//   1. 控制点 (ra,dec) → 3D 笛卡尔 (x,y,z),用 nanoflann 建 KD-tree
//   2. 查询点 (ra,dec) → KD-tree K 近邻搜索 → 球面大圆距离 γ → IDW 权重
//   3. SNR(ra,dec) = snr_phot × (Σ w_i·snr_psf_i / Σ w_i) / median_snr
//      w_i = 1 / γ_i^idw_power

#ifndef SNR_EVALUATOR_H
#define SNR_EVALUATOR_H

#include <cstdint>
#include <vector>

namespace gradient {

// ============================================================================
// SnrEvaluator - SNR 稀疏控制点模型 IDW 评估器
//
// 生命周期:
//   SnrEvaluator eval;
//   eval.build(n_points, ra_arr, dec_arr, snr_psf_arr, snr_phot, median_snr, idw_power);
//   eval.evaluate(ra, dec);                    // 单点评估
//   eval.evaluateBatch(ra_arr, dec_arr, n, out_snr);  // 批量评估
// ============================================================================
class SnrEvaluator {
public:
    SnrEvaluator();
    ~SnrEvaluator();

    SnrEvaluator(const SnrEvaluator&) = delete;
    SnrEvaluator& operator=(const SnrEvaluator&) = delete;

    // ========================================================================
    // build - 从控制点数组构建 KD-tree
    //
    // 输入:
    //   n_points   - 控制点数
    //   ra_arr     - 控制点赤经数组 [n_points] (度)
    //   dec_arr    - 控制点赤纬数组 [n_points] (度)
    //   snr_psf_arr - 控制点 SNR_psf 数组 [n_points] (无量纲)
    //   snr_phot   - 全局测光 SNR 标量 1/(ln10×sigma_residual)
    //   median_snr - median(snr_psf) 归一化基准
    //   idw_power  - IDW 幂次 (默认 2.0)
    //
    // 返回: true=成功, false=失败 (n_points<=0 或参数非法)
    // ========================================================================
    bool build(uint32_t n_points,
               const double* ra_arr,
               const double* dec_arr,
               const float*  snr_psf_arr,
               double snr_phot,
               double median_snr,
               double idw_power);

    // FP64 控制点版本 (BLOCKER-TYPE-002): snr_psf 以 double 存储并参与评估
    bool buildF64(uint32_t n_points,
                  const double* ra_arr,
                  const double* dec_arr,
                  const double* snr_psf_arr,
                  double snr_phot,
                  double median_snr,
                  double idw_power);

    // ========================================================================
    // evaluate - 单点 SNR 评估
    //
    // 输入: ra, dec (度)
    // 返回: SNR(ra,dec) = snr_phot × (IDW_snr_psf / median_snr)
    //                   = 0.0 若未 build 或控制点为空
    // ========================================================================
    float evaluate(double ra, double dec) const;

    // ========================================================================
    // evaluateBatch - 批量 SNR 评估 (OpenMP 并行)
    //
    // 输入:
    //   ra_arr  - 查询点赤经数组 [n_query] (度)
    //   dec_arr - 查询点赤纬数组 [n_query] (度)
    //   n_query - 查询点数
    // 输出:
    //   out_snr - SNR 数组 [n_query] (调用者分配)
    // ========================================================================
    void evaluateBatch(const double* ra_arr,
                       const double* dec_arr,
                       uint64_t n_query,
                       float* out_snr) const;

    // ========================================================================
    // 状态查询
    // ========================================================================
    bool isBuilt() const { return built_; }
    uint32_t nPoints() const { return n_points_; }
    double snrPhot() const { return snr_phot_; }
    double medianSnr() const { return median_snr_; }
    double idwPower() const { return idw_power_; }

private:
    // KD-tree 内部数据 (PIMPL 风格, 隐藏 nanoflann 头依赖)
    struct Impl;
    Impl* impl_;

    // 全局参数
    uint32_t n_points_ = 0;
    double   snr_phot_ = 0.0;
    double   median_snr_ = 0.0;
    double   idw_power_ = 2.0;
    bool     built_ = false;

    // K 近邻数 (默认 16, 控制精度与性能平衡)
    static constexpr int DEFAULT_KNN = 16;
};

} // namespace gradient

#endif // SNR_EVALUATOR_H
