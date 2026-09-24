/**
 * @file nls_lm.h
 * @brief 自研信赖域 Levenberg–Marquardt 非线性最小二乘求解器（GSL 替代）
 *
 * 位置依据: ASTROCS_DESIGN.md §8.4「顶层结构」——lib/algorithms/ 是科学算法唯一家,
 * 模块并联放置; 本求解器是 lib/algorithms/star_detection 的**模块内部数值后端**,
 * 不是独立可调度模块(ASTROCS_DESIGN §8.5 的 README/module.yaml/独立 DLL 要求不适用,
 * 因为它是单一调用点的内部实现细节), 亦非第三方代码(故不入 lib/third_party/)。
 * 头文件与实现同置于 src/, 不进公共 include 面(不跨 DLL 传 STL/异常, §8.5)。
 *
 * 算法出处(逐条对应实现, 详见 nls_lm.cpp 顶部算法说明与
 * run/GSL-REPLACE-01/REPORT.md):
 *   - Moré, J. J. 1978, "The Levenberg-Marquardt algorithm: implementation and theory",
 *     Lecture Notes in Mathematics 630, 105（信赖域 + 直接步 + 缩放矩阵 D）
 *   - Madsen, Nielsen & Tingleff 2004, "Methods for Non-Linear Least Squares Problems",
 *     IMM-DTU（L-M 阻尼最小二乘、增益比 ρ、停止判据）
 *   - Garbow, Hillstrom & Moré, MINPACK-1 lmder/lmdif（lmpar 的 Δ↔λ 换算与 Δ 更新）
 *
 * 与 GSL gsl_multifit_nlinear(trs_lm, solver=qr, scale=more) 的行为对齐点:
 *   - 缩放矩阵 D = diag(||J 的第 i 列||₂)（GSL scale=more）;
 *   - 子问题 = 直接步: (JᵀJ + λD²)h = −Jᵀf, Cholesky 求解;
 *   - 停止判据: xtol/gtol/ftol 三判据, 语义见 nls_lm.cpp（info 编码 1/2/3 与 GSL 一致）;
 *   - 迭代上限超出即返回 MaxIterations（对应 GSL GSL_EMAXITER, 生产侧映射为
 *     SDET_FIT_NO_CONVERGENCE）。
 */
#ifndef ASTROCS_SDET_NLS_LM_H
#define ASTROCS_SDET_NLS_LM_H

#include <cstddef>
#include <vector>

namespace astrocs {
namespace star_detection {
namespace nls {

/** 求解结局。生产侧只区分 Success 与其余（映射 SDET_FIT_NO_CONVERGENCE）。 */
enum class Status {
    Success = 0,          /**< 三判据之一在 max_iter 内满足 */
    MaxIterations = 1,    /**< 迭代上限用尽（GSL: GSL_EMAXITER） */
    InvalidArgument = 2,  /**< n/p/初值非法（GSL: GSL_EINVAL/GSL_EDOM） */
    NumericalFailure = 3  /**< 线性子问题不可解且阻尼放大无法修复 */
};

/** 触发终止的判据（info 语义与 GSL driver 一致）。 */
enum class Criterion {
    None = 0, /**< 未终止（MaxIterations） */
    Xtol = 1, /**< 参数相对变化判据 */
    Ftol = 2, /**< 代价函数相对变化 + 线性模型预测下降量判据 */
    Gtol = 3  /**< 梯度无穷范数判据 */
};

/** 求解器参数。默认值 = 原 GSL 调用点语义
 *  （sdet_api.cpp: sdet_lm_fit 的 LM_XTOL/LM_GTOL/LM_FTOL=1e-3; GSL
 *   gsl_multifit_nlinear_default_parameters 的 factor_up=3/factor_down=2/avmax=0.75）。*/
struct Options {
    std::size_t max_iter = 20;  /**< 迭代上限（生产: LM_MAX_ITER_ANGLE=20, 饱和星 ×3） */
    double xtol = 1e-3;         /**< |Δx_i| ≤ xtol_abs + xtol·|x_i| 逐分量（相对参数变化） */
    double ftol = 1e-3;         /**< |Δχ²| ≤ ftol·χ² 且预测下降 ≤ ftol·χ² 且 ρ ≤ 2 */
    double gtol = 1e-3;         /**< ||Jᵀf||_∞ ≤ gtol（绝对；无量纲判据在残差尺度上极难触发） */
    double xtol_abs = 1e-6;     /**< xtol 的绝对项（GSL 实证语义, 见 REPORT §3.1） */
    double factor_up = 3.0;     /**< 步长/信赖域放大因子（GSL 默认） */
    double factor_down = 2.0;   /**< 步长/信赖域缩小因子（GSL 默认） */
    double avmax = 0.75;        /**< 判「好步」的增益比门（GSL 默认） */

    /** 诊断用逐迭代回调（可为 nullptr）: 接受步之后调用。 */
    void (*trace)(std::size_t iter, const double* x, const double* h,
                  double cost, void* ctx) = nullptr;
    void* trace_ctx = nullptr;
};

/** 求解报告。 */
struct Report {
    Status status = Status::InvalidArgument;
    Criterion criterion = Criterion::None;
    std::size_t iterations = 0; /**< 接受的迭代数（GSL w->niter 同义） */
    std::size_t nfev = 0;       /**< 残差求值次数 */
    std::size_t njev = 0;       /**< 雅可比求值次数 */
    double cost = 0.0;          /**< 最终 χ² = Σ f_i² */
    double rms = 0.0;           /**< sqrt(χ²/n) */
    double mu = 0.0;            /**< 最终阻尼参数（诊断用） */
    double delta = 0.0;         /**< 最终信赖域半径 ‖D·Δx‖（诊断用） */
};

/** 残差函数: f = f(x), 长度 n, 行主序连续写出。*/
typedef void (*ResidualFn)(const double* x, void* ctx, double* f);
/** 雅可比: J = ∂f/∂x, n×p 行主序连续写出。*/
typedef void (*JacobianFn)(const double* x, void* ctx, double* J);

/** 可复用工作区（避免逐星堆分配; 同一线程复用同一实例即可）。 */
class Workspace {
public:
    void ensure(std::size_t n, std::size_t p);

    std::size_t n = 0;
    std::size_t p = 0;
    std::vector<double> f;       /**< 当前残差 n */
    std::vector<double> ftrial;  /**< 试探点残差 n */
    std::vector<double> J;       /**< 当前雅可比 n×p */
    std::vector<double> A;       /**< JᵀJ, p×p（对称, 存下三角亦可, 此处存满） */
    std::vector<double> g;       /**< Jᵀf, p */
    std::vector<double> D;       /**< 缩放矩阵对角 ‖J_i‖₂, p */
    std::vector<double> h;       /**< 当前步长, p */
    std::vector<double> xtrial;  /**< 试探点 p */
    std::vector<double> M;       /**< Cholesky 工作矩阵 p×p（A 的拷贝, 不破坏 A） */
    std::vector<double> wa1;     /**< 临时 p（右端项） */
    std::vector<double> wa2;     /**< 临时 p（前代临时） */
};

/**
 * 求解 min_x ‖f(x)‖²（x 原地更新为解）。
 *
 * @param residual 残差回调（每次调用写满 n 个分量）
 * @param jacobian 雅可比回调（每次调用写满 n×p 个分量, 行主序）
 * @param ctx      回调上下文
 * @param n        残差维数（样本数）
 * @param p        参数维数（本仓固定 7）
 * @param x        [in,out] 初值 → 解, 长度 p
 * @param opts     求解参数
 * @param ws       可复用工作区（可为 nullptr, 则内部临时分配）
 * @return 求解报告
 */
Report solve(ResidualFn residual, JacobianFn jacobian, void* ctx,
             std::size_t n, std::size_t p, double* x,
             const Options& opts, Workspace* ws = nullptr);

}  // namespace nls
}  // namespace star_detection
}  // namespace astrocs

#endif  // ASTROCS_SDET_NLS_LM_H
