// snr_evaluator.cpp - SNR 稀疏控制点模型 IDW 评估器实现
// 功能: KD-tree (nanoflann) + 球面大圆距离 IDW 评估
// 用途: 从稀疏控制点重建任意点 SNR

#include "snr_evaluator.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>

#include <omp.h>

// nanoflann: header-only KD-tree
#include "nanoflann.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gradient {

// ============================================================================
// 度 ↔ 弧度
// ============================================================================
static constexpr double D2R = 0.017453292519943295769;
static constexpr double R2D = 57.2957795130823208768;

// ============================================================================
// 球面坐标 → 3D 笛卡尔坐标 (单位球面)
// x = cos(dec)·cos(ra), y = cos(dec)·sin(ra), z = sin(dec)
// ============================================================================
struct Vec3 {
    double x, y, z;
};

static inline Vec3 radecToVec3(double ra_deg, double dec_deg) {
    double rar = ra_deg * D2R;
    double decr = dec_deg * D2R;
    double cd = std::cos(decr);
    return { cd * std::cos(rar), cd * std::sin(rar), std::sin(decr) };
}

// ============================================================================
// 球面大圆弧角距离 (度)
// γ = arccos(sin(d1)·sin(d2) + cos(d1)·cos(d2)·cos(Δra))
// 使用 clamping 防止浮点误差导致 arccos 域外
// ============================================================================
static inline double greatCircleDistanceDeg(double ra1, double dec1,
                                             double ra2, double dec2) {
    double dec1r = dec1 * D2R;
    double dec2r = dec2 * D2R;
    double dra = (ra2 - ra1) * D2R;
    double x = std::sin(dec1r) * std::sin(dec2r) +
               std::cos(dec1r) * std::cos(dec2r) * std::cos(dra);
    x = std::max(-1.0, std::min(1.0, x));
    return std::acos(x) * R2D;
}

// ============================================================================
// SnrEvaluator::Impl - KD-tree 内部实现 (PIMPL)
// ============================================================================
struct SnrEvaluator::Impl {
    // 控制点 3D 笛卡尔坐标 (用于 KD-tree)
    std::vector<Vec3> points3d;

    // 控制点原始球面坐标 + snr_psf (用于 IDW 评估)
    std::vector<double> ra_arr;
    std::vector<double> dec_arr;
    std::vector<float>  snr_psf_arr;
    std::vector<double> snr_psf_f64_arr;   // BLOCKER-TYPE-002: f64 控制点
    bool use_f64_snr = false;

    double snrAt(size_t idx) const {
        return use_f64_snr ? snr_psf_f64_arr[idx] : (double)snr_psf_arr[idx];
    }

    // nanoflann KD-tree 类型定义
    // PointCloudAdaptor: 将 points3d 包装为 nanoflann 可用的数据源
    struct PointCloudAdaptor {
        const std::vector<Vec3>* pts;
        inline size_t kdtree_get_point_count() const { return pts->size(); }
        inline double kdtree_get_pt(size_t idx, size_t dim) const {
            const Vec3& p = (*pts)[idx];
            if (dim == 0) return p.x;
            if (dim == 1) return p.y;
            return p.z;
        }
        template <class BBOX>
        bool kdtree_get_bbox(BBOX&) const { return false; }
    };

    using KDTree = nanoflann::KDTreeSingleIndexAdaptor<
        nanoflann::L2_Simple_Adaptor<double, PointCloudAdaptor>,
        PointCloudAdaptor, 3>;

    PointCloudAdaptor adaptor;
    KDTree* tree = nullptr;

    Impl() : adaptor{nullptr} {}

    ~Impl() {
        if (tree) {
            delete tree;
            tree = nullptr;
        }
    }

    void buildTree() {
        adaptor.pts = &points3d;
        if (tree) {
            delete tree;
            tree = nullptr;
        }
        if (points3d.empty()) return;
        // KD-tree 参数: leaf_max_size=32 (P13-002 修复栈溢出: 10→32 减少递归深度)
        tree = new KDTree(3, adaptor,
                          nanoflann::KDTreeSingleIndexAdaptorParams(32));
        tree->buildIndex();
    }
};

// ============================================================================
// 构造 / 析构
// ============================================================================
SnrEvaluator::SnrEvaluator() : impl_(new Impl()) {}

SnrEvaluator::~SnrEvaluator() {
    delete impl_;
}

// ============================================================================
// build - 构建 KD-tree
// ============================================================================
bool SnrEvaluator::build(uint32_t n_points,
                          const double* ra_arr,
                          const double* dec_arr,
                          const float*  snr_psf_arr,
                          double snr_phot,
                          double median_snr,
                          double idw_power) {
    // 清理旧状态
    if (impl_->tree) {
        delete impl_->tree;
        impl_->tree = nullptr;
    }
    impl_->points3d.clear();
    impl_->ra_arr.clear();
    impl_->dec_arr.clear();
    impl_->snr_psf_arr.clear();
    impl_->snr_psf_f64_arr.clear();
    impl_->use_f64_snr = false;

    built_ = false;
    n_points_ = 0;

    // 参数校验
    if (n_points == 0 || !ra_arr || !dec_arr || !snr_psf_arr) {
        fprintf(stderr, "[snr_evaluator] build 失败: n_points=%u, 空指针=%d%d%d\n",
                n_points, ra_arr==nullptr, dec_arr==nullptr, snr_psf_arr==nullptr);
        return false;
    }

    // 预分配
    impl_->points3d.reserve(n_points);
    impl_->ra_arr.reserve(n_points);
    impl_->dec_arr.reserve(n_points);
    impl_->snr_psf_arr.reserve(n_points);

    // 构建控制点数组 (过滤无效点)
    uint32_t valid = 0;
    for (uint32_t i = 0; i < n_points; i++) {
        // 跳过 NaN/Inf
        if (!std::isfinite(ra_arr[i]) || !std::isfinite(dec_arr[i]) ||
            !std::isfinite(snr_psf_arr[i])) {
            continue;
        }
        impl_->points3d.push_back(radecToVec3(ra_arr[i], dec_arr[i]));
        impl_->ra_arr.push_back(ra_arr[i]);
        impl_->dec_arr.push_back(dec_arr[i]);
        impl_->snr_psf_arr.push_back(snr_psf_arr[i]);
        valid++;
    }

    if (valid == 0) {
        fprintf(stderr, "[snr_evaluator] build 失败: 无有效控制点 (n_points=%u)\n", n_points);
        return false;
    }

    // 构建 KD-tree
    impl_->buildTree();

    // 保存全局参数
    n_points_   = valid;
    snr_phot_   = snr_phot;
    median_snr_ = (median_snr > 0.0) ? median_snr : 1.0;
    idw_power_  = (idw_power > 0.0) ? idw_power : 2.0;
    built_      = true;

    fprintf(stderr, "[snr_evaluator] build 成功: %u 控制点 (有效 %u/%u), "
                    "snr_phot=%.4f, median_snr=%.4f, idw_power=%.2f\n",
            n_points_, valid, n_points, snr_phot_, median_snr_, idw_power_);
    return true;
}

// ============================================================================
// buildF64 - FP64 控制点版本 (BLOCKER-TYPE-002)
// ============================================================================
bool SnrEvaluator::buildF64(uint32_t n_points,
                             const double* ra_arr,
                             const double* dec_arr,
                             const double* snr_psf_arr,
                             double snr_phot,
                             double median_snr,
                             double idw_power) {
    if (impl_->tree) {
        delete impl_->tree;
        impl_->tree = nullptr;
    }
    impl_->points3d.clear();
    impl_->ra_arr.clear();
    impl_->dec_arr.clear();
    impl_->snr_psf_arr.clear();
    impl_->snr_psf_f64_arr.clear();
    impl_->use_f64_snr = false;

    built_ = false;
    n_points_ = 0;
    if (n_points == 0 || !ra_arr || !dec_arr || !snr_psf_arr) return false;

    impl_->points3d.reserve(n_points);
    impl_->ra_arr.reserve(n_points);
    impl_->dec_arr.reserve(n_points);
    impl_->snr_psf_f64_arr.reserve(n_points);

    uint32_t valid = 0;
    for (uint32_t i = 0; i < n_points; i++) {
        if (!std::isfinite(ra_arr[i]) || !std::isfinite(dec_arr[i]) ||
            !std::isfinite(snr_psf_arr[i])) continue;
        impl_->points3d.push_back(radecToVec3(ra_arr[i], dec_arr[i]));
        impl_->ra_arr.push_back(ra_arr[i]);
        impl_->dec_arr.push_back(dec_arr[i]);
        impl_->snr_psf_f64_arr.push_back(snr_psf_arr[i]);
        valid++;
    }
    if (valid == 0) return false;

    impl_->use_f64_snr = true;
    impl_->buildTree();
    n_points_   = valid;
    snr_phot_   = snr_phot;
    median_snr_ = (median_snr > 0.0) ? median_snr : 1.0;
    idw_power_  = (idw_power > 0.0) ? idw_power : 2.0;
    built_      = true;
    fprintf(stderr, "[snr_evaluator] buildF64 成功: %u 控制点 (double snr)\n",
            n_points_);
    return true;
}

// ============================================================================
// evaluate - 单点 SNR 评估
// ============================================================================
float SnrEvaluator::evaluate(double ra, double dec) const {
    if (!built_ || !impl_->tree) return 0.0f;

    // 查询点 → 3D 笛卡尔
    Vec3 q = radecToVec3(ra, dec);
    double q_arr[3] = { q.x, q.y, q.z };

    // K 近邻搜索 (nanoflann IndexType = unsigned int)
    int k = std::min((int)n_points_, DEFAULT_KNN);
    std::vector<uint32_t> indices(k);
    std::vector<double> dists_sq(k);

    int found = impl_->tree->knnSearch(q_arr, k, indices.data(), dists_sq.data());
    if (found <= 0) return 0.0f;

    // IDW 评估: 用球面大圆距离 (度) 计算权重
    double w_sum = 0.0;
    double ws_sum = 0.0;

    for (int i = 0; i < found; i++) {
        uint32_t idx = indices[i];
        double gamma = greatCircleDistanceDeg(ra, dec,
                                               impl_->ra_arr[idx],
                                               impl_->dec_arr[idx]);

        // γ→0: 查询点与控制点重合, 直接返回该控制点的 snr_psf
        if (gamma < 1e-10) {
            double snr_psf = impl_->snrAt(idx);
            return static_cast<float>(snr_phot_ * snr_psf / median_snr_);
        }

        double w = 1.0 / std::pow(gamma, idw_power_);
        w_sum  += w;
        ws_sum += w * impl_->snrAt(idx);
    }

    if (w_sum <= 0.0) return 0.0f;

    double snr_psf_idw = ws_sum / w_sum;
    return static_cast<float>(snr_phot_ * snr_psf_idw / median_snr_);
}

// ============================================================================
// evaluateBatch - 批量 SNR 评估 (OpenMP 并行)
// ============================================================================
void SnrEvaluator::evaluateBatch(const double* ra_arr,
                                  const double* dec_arr,
                                  uint64_t n_query,
                                  float* out_snr) const {
    if (!built_ || !impl_->tree || !ra_arr || !dec_arr || !out_snr) {
        if (out_snr) {
            std::fill(out_snr, out_snr + n_query, 0.0f);
        }
        return;
    }

    int k = std::min((int)n_points_, DEFAULT_KNN);

    #pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < (int64_t)n_query; i++) {
        double ra = ra_arr[i];
        double dec = dec_arr[i];

        Vec3 q = radecToVec3(ra, dec);
        double q_arr[3] = { q.x, q.y, q.z };

        // 每线程独立缓冲区 (nanoflann IndexType = unsigned int)
        uint32_t idx_buf[16];
        double dist_buf[16];
        int k_use = std::min(k, 16);

        int found = impl_->tree->knnSearch(q_arr, k_use, idx_buf, dist_buf);
        if (found <= 0) {
            out_snr[i] = 0.0f;
            continue;
        }

        double w_sum = 0.0;
        double ws_sum = 0.0;

        for (int j = 0; j < found; j++) {
            uint32_t idx = idx_buf[j];
            double gamma = greatCircleDistanceDeg(ra, dec,
                                                   impl_->ra_arr[idx],
                                                   impl_->dec_arr[idx]);

            if (gamma < 1e-10) {
                ws_sum = impl_->snrAt(idx);
                w_sum = 1.0;
                break;
            }

            double w = 1.0 / std::pow(gamma, idw_power_);
            w_sum  += w;
            ws_sum += w * impl_->snrAt(idx);
        }

        if (w_sum <= 0.0) {
            out_snr[i] = 0.0f;
        } else {
            double snr_psf_idw = ws_sum / w_sum;
            out_snr[i] = static_cast<float>(snr_phot_ * snr_psf_idw / median_snr_);
        }
    }
}

} // namespace gradient
