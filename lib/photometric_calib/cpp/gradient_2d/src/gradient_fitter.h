#ifndef PC_GRADIENT_FITTER_H
#define PC_GRADIENT_FITTER_H

#include <vector>
#include <string>

namespace pc {

// 梯度曲面拟合结果 (对应 Python GradientSurface)
struct GradientSurface {
    int    order;             // 多项式阶数
    std::vector<double> coeffs; // 系数 (长度 = (order+1)*(order+2)/2)
    double loocv_error;       // LOOCV 误差
    double residual_median;   // 内点残差中位数
    double residual_std;      // 内点残差标准差
    int    n_used;            // IRLS 内点数 (weight > 0)
    int    n_rejected;        // IRLS 排除数
    double ridge_alpha;       // 选中的 Ridge alpha
};

// 2D 多项式梯度曲面稳健拟合器
// 算法: IRLS + Tukey biweight (c=4.685) + Ridge L2 正则化 + 帽子矩阵 LOOCV 选阶
class GradientFitter {
public:
    GradientFitter();

    // 乘性梯度拟合: r = log10(F_instr / F_syn)
    // 输入: x_arr/y_arr 像素坐标, r_arr 观测比值 (log10), img_w/img_h 图像尺寸
    // max_order: 最高阶数, 内部限制为 min(max_order, MAX_ORDER=3)
    GradientSurface fit_multiplicative(
        const std::vector<double>& x_arr,
        const std::vector<double>& y_arr,
        const std::vector<double>& r_arr,
        double img_w, double img_h, int max_order);

    // 全图像曲面评估: 生成 (height, width) 的曲面值数组
    // 返回值: 长度 height*width 的数组, 按行优先存储
    std::vector<double> evaluate_surface_fullimage(
        const GradientSurface& surface, int width, int height) const;

    // 在指定坐标点评估曲面 (批量)
    // x_arr/y_arr 长度 n, 返回长度 n 的评估值数组
    std::vector<double> evaluate_surface_points(
        const GradientSurface& surface,
        const std::vector<double>& x_arr,
        const std::vector<double>& y_arr,
        double img_w, double img_h) const;

    // 计算 R² (决定系数), 仅在 IRLS 内点 (weight>0) 上计算
    // R² < 0 表示拟合比均值还差; R² ≈ 0 表示无空间信号; R² → 1 表示完美拟合
    // 用于 R² 信号检测: R² < MIN_R_SQUARED (0.02) 时跳过乘性校正
    double compute_r_squared(const GradientSurface& surface,
                             const std::vector<double>& x_arr,
                             const std::vector<double>& y_arr,
                             const std::vector<double>& val_arr,
                             double img_w, double img_h) const;

    // ---- 公共静态工具 (供外部调用) ----

    // 单项式项列表: 返回 (j,k) 对, j+k <= order, 按 total_degree 升序
    // 例如 order=1: [(0,0), (1,0), (0,1)], order=2: 加 (2,0),(1,1),(0,2)
    static std::vector<std::pair<int,int>> monomial_terms(int order);

    // 在归一化坐标 (x_norm, y_norm) 处评估单项式基向量
    static std::vector<double> eval_basis(double x_norm, double y_norm, int order);

    // 在归一化坐标 (x_norm, y_norm) 处评估曲面值
    static double eval_surface(const GradientSurface& surface,
                               double x_norm, double y_norm);

    // 构造恒等曲面 (1阶, 系数全0): 乘性 r=0 -> M=1; 加性 s=0 -> S=0
    static GradientSurface identity_surface();

    // 常量定义 (公开供外部引用)
    static constexpr double TUKEY_C = 4.685;       // Tukey biweight 调整常数
    static constexpr double MAD_SCALE = 0.6745;    // MAD -> sigma 缩放因子
    static constexpr int    MIN_ORDER = 1;
    static constexpr int    MAX_ORDER = 3;
    static constexpr int    MIN_SAMPLE_FACTOR = 3; // 每个系数至少 3 个样本
    static constexpr double CV_REL_TOL = 0.10;     // LOOCV 10% 容差选最简模型
    static constexpr int    IRLS_MAX_ITER = 50;
    static constexpr double IRLS_TOL = 1e-8;
    static constexpr int    MIN_MATCHED_FOR_FIT = 6;
    static constexpr double MIN_R_SQUARED = 0.02;  // R² 信号检测阈值

private:
    // 坐标归一化: x_norm = (2*x/img_w) - 1
    static std::vector<double> normalize_coords(
        const std::vector<double>& arr, double img_size);

    // 构造设计矩阵 X (n × p), p = (order+1)(order+2)/2
    // 行 i: [x_i^0 * y_i^0, x_i^1 * y_i^0, x_i^0 * y_i^1, ...]
    static std::vector<std::vector<double>> build_design_matrix(
        const std::vector<double>& x_norm,
        const std::vector<double>& y_norm, int order);

    // IRLS + Tukey biweight + Ridge 拟合
    // 返回: coeffs (长度 p), weights (长度 n), residuals (长度 n)
    void irls_fit(const std::vector<std::vector<double>>& X,
                  const std::vector<double>& r,
                  double ridge_alpha,
                  std::vector<double>& coeffs,
                  std::vector<double>& weights,
                  std::vector<double>& residuals) const;

    // 帽子矩阵 LOOCV 分数 (单次拟合, 不逐次重新拟合)
    // 公式: H = W^{1/2} X (X^T W X + alpha*D)^{-1} X^T W^{1/2}
    //       LOOCV = mean(W_i * (r_i - r_hat_i)^2 / (1 - H_ii)^2)
    double loocv_score(const std::vector<std::vector<double>>& X,
                       const std::vector<double>& r,
                       const std::vector<double>& weights,
                       double ridge_alpha) const;

    // LOOCV 自动选阶 + Ridge alpha 网格搜索
    // 对每个阶数 order in [MIN_ORDER, min(max_order, MAX_ORDER)],
    // 搜索 alpha in {0, 0.1, 1, 10, 100, 1000}, 选 (order, alpha) 使 LOOCV 最小,
    // 再用 10% 容差选最简模型 (最低阶 + 最小 alpha)
    void select_order(const std::vector<double>& x_arr,
                      const std::vector<double>& y_arr,
                      const std::vector<double>& r_arr,
                      double img_w, double img_h, int max_order,
                      int& best_order, double& best_cv,
                      double& best_alpha) const;

    // 内部拟合核心 (供 fit_multiplicative 调用)
    GradientSurface fit_surface(
        const std::vector<double>& x_arr,
        const std::vector<double>& y_arr,
        const std::vector<double>& val_arr,
        double img_w, double img_h, int max_order,
        const std::string& channel_name);

    // ---- 线性代数工具 (无 Eigen 依赖, 矩阵规模小, 手写实现) ----

    // 解线性方程组 A x = b (高斯消元 + 部分主元)
    // A: n×n (row-major), b: n, 返回 x: n; 奇异返回 false
    static bool solve_linear(const std::vector<std::vector<double>>& A,
                             const std::vector<double>& b,
                             std::vector<double>& x);

    // 矩阵求逆 (高斯-若尔当消元)
    static bool inverse_matrix(const std::vector<std::vector<double>>& A,
                               std::vector<std::vector<double>>& A_inv);

    // ---- 统计工具 ----
    static double median(std::vector<double> arr); // 排序中位数 (传值修改)
    static double median_abs_dev(const std::vector<double>& arr, double med);
};

} // namespace pc

#endif // PC_GRADIENT_FITTER_H
