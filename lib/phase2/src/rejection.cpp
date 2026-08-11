// lib/phase2/src/rejection.cpp — Phase2 Rejection Framework CPU reference
//
// W7（控制包 34A532A2...B2EB308 + wiki Phase2_Rejection）：
//   - 输入：UPM-calibrated 样本栈 values[]/valid[]/support[]/weights[]/quality[]；
//   - 首版实现：None、Sigma、WinsorizedSigma（确定性，Oracle 对照）；
//   - AveragedSigma/LinearFit/ESD/RCR 接口冻结，后续子任务按论文/Oracle 独立实现；
//   - 输出 accepted mask + low/high 计数 + 迭代数 + status。
#include "astro/phase2/rejection.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <vector>

namespace {

inline double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const std::size_t n = v.size();
    const std::size_t mid = n / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (n % 2 == 1) return v[mid];
    const double a = v[mid];
    // nth_element 后 [begin, mid) 全部 ≤ v[mid]；第 mid 小（即排序后
    // v[mid-1]）是前 mid 个元素的最大值。
    const double b = *std::max_element(v.begin(), v.begin() + mid);
    return 0.5 * (a + b);
}

inline double mad(std::vector<double> v, double med) {
    for (auto& x : v) x = std::fabs(x - med);
    return 1.4826 * median(std::move(v));
}

// 正则化不完全 beta I_x(a,b)（Lentz 连分数，Numerical Recipes betai/betacf 算法）
double ibeta_cf(double a, double b, double x) {
    const double fpmin = 1e-300;
    const double qab = a + b;
    const double qap = a + 1.0;
    const double qam = a - 1.0;
    double c = 1.0;
    double d = 1.0 - qab * x / qap;
    if (std::fabs(d) < fpmin) d = fpmin;
    d = 1.0 / d;
    double h = d;
    for (int m = 1; m <= 300; ++m) {
        const int m2 = 2 * m;
        double aa = (double)m * (b - (double)m) * x /
                    ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < fpmin) d = fpmin;
        c = 1.0 + aa / c;
        if (std::fabs(c) < fpmin) c = fpmin;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + (double)m) * (qab + (double)m) * x /
             ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < fpmin) d = fpmin;
        c = 1.0 + aa / c;
        if (std::fabs(c) < fpmin) c = fpmin;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::fabs(del - 1.0) < 1e-12) break;
    }
    return h;
}

double ibeta(double a, double b, double x) {
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    const double ln = std::lgamma(a + b) - std::lgamma(a) -
                      std::lgamma(b) + a * std::log(x) +
                      b * std::log(1.0 - x);
    const double bt = std::exp(ln);
    if (x < (a + 1.0) / (a + b + 2.0))
        return bt * ibeta_cf(a, b, x) / a;
    return 1.0 - bt * ibeta_cf(b, a, 1.0 - x) / b;
}

// Student-t CDF（双侧对称）
double t_cdf(double t, double nu) {
    const double x = nu / (nu + t * t);
    // F(t) = 1 − ½·I_x(ν/2, 1/2)（t ≥ 0），x = ν/(ν+t²)
    if (t >= 0.0) return 1.0 - 0.5 * ibeta(nu / 2.0, 0.5, x);
    return 0.5 * ibeta(nu / 2.0, 0.5, x);
}

// Student-t 分位数（二分求逆，p 为单侧下尾概率）
double t_quantile(double p, double nu) {
    if (p <= 0.0) return 0.0;
    if (p >= 1.0) return 40.0;
    double lo = 0.0, hi = 40.0;
    for (int it = 0; it < 80; ++it) {
        const double mid = 0.5 * (lo + hi);
        const double cdf = t_cdf(mid, nu);
        if (cdf < p) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

// 最小二乘直线 y = a*x + b（x 为样本序号）
void ls_fit_line(const std::vector<double>& xv, const std::vector<double>& yv,
                 double* a, double* b) {
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    const std::size_t n = xv.size();
    for (std::size_t i = 0; i < n; ++i) {
        sx += xv[i];
        sy += yv[i];
        sxx += xv[i] * xv[i];
        sxy += xv[i] * yv[i];
    }
    const double den = (double)n * sxx - sx * sx;
    if (std::fabs(den) < 1e-12) {
        *a = 0.0;
        *b = n ? sy / (double)n : 0.0;
        return;
    }
    *a = ((double)n * sxy - sx * sy) / den;
    *b = (sy - *a * sx) / (double)n;
}

// 官方公开 Chauvenet 经验修正因子（RCR.cpp nCorrect 近似公式）
inline double rcr_n_correct(std::size_t n) {
    return std::pow(1.2591, std::pow((double)n, 0.2052));
}

// ===== V4 R4：完整 sequential RCR =====
// 语义 = Maples et al. 2018（arXiv:1807.05276）核心：按
// robust → precise 顺序执行多段 iterative Chauvenet 拒绝，等价官方
// rcr 2.4.7（nickk124/robust-outlier-rejection commit a8a29a6）
// `performRejection` 的 SS_MEDIAN_DL 冻结链：
//   1) MEDIAN + DOUBLE_LINE（中位数位置 + 双线稳健尺度）
//   2) MEDIAN + SIXTY_EIGHTH_PERCENTILE
//   3) MEAN + STANDARD_DEVIATION
// 每段 sigma = 稳健尺度 × SS_MEDIAN_DL 校准修正因子 CF（SSDLUnityCF
// 表 / n≥101 幂律）；拒绝判据 n·erfc(z/√2) < 0.5（A&S 7.1.26 近似，
// 官方 erfcCustom）。本实现独立编写（不复制 oracle 源码），数值语义
// 与 oracle 逐位对齐以便 rejected-set 精确比对。
// 冻结技术：SS_MEDIAN_DL（对称/混合污染；单侧/中间污染使用同一链，
// 配置文档明确，不泛称全部 RCR 变体）。

constexpr double kRcrPi = 3.1415926535897932384626434;
constexpr double kRcrInverfMult =
    (8.0 * (kRcrPi - 3.0)) / (3.0 * kRcrPi * (4.0 - kRcrPi));

// 官方校准表（公开数据表；SSDLUnityCF[n] = 1/CF，n<101）
const double kRcrSSDLUnityCF[101] = {
    1.0, 1.0, 0.799351, 0.521918, 0.577339, 0.528513, 0.635396, 0.659658,
    0.675483, 0.696538, 0.73329, 0.727713, 0.757271, 0.768152, 0.775964, 0.783458,
    0.80077, 0.797932, 0.810314, 0.81869, 0.820111, 0.829854, 0.834026, 0.841326,
    0.843973, 0.847922, 0.858153, 0.856903, 0.858529, 0.868727, 0.862837, 0.871493,
    0.876271, 0.875507, 0.880854, 0.878536, 0.885307, 0.887318, 0.890606, 0.890264,
    0.893034, 0.898365, 0.896602, 0.899384, 0.901048, 0.904264, 0.902966, 0.906581,
    0.908435, 0.907805, 0.91133, 0.913216, 0.912063, 0.913785, 0.916749, 0.915789,
    0.918895, 0.919865, 0.920497, 0.921825, 0.92345, 0.92432, 0.923952, 0.925951,
    0.926259, 0.927963, 0.928288, 0.930179, 0.930933, 0.925191, 0.934905, 0.932677,
    0.933801, 0.934811, 0.934255, 0.936249, 0.935739, 0.931335, 0.93934, 0.939084,
    0.939209, 0.939624, 0.94091, 0.940528, 0.940834, 0.942504, 0.942799, 0.943895,
    0.943777, 0.945166, 0.944398, 0.945379, 0.946653, 0.94639, 0.946253, 0.947046,
    0.948222, 0.944239, 0.949949, 0.948752, 0.950152};

// 官方 getSingleFN 阈值表（n<1001；n≥1001 用幂律近似）
const double kRcrSSUnity[1001] = {
    0.0, 0.0, 0.0, 0.0, 2.15681, 8.81255, 2.72685, 3.45589,
    2.32749, 2.90262, 2.41888, 2.76084, 2.37034, 2.47951, 2.3207, 2.41864,
    2.21943, 2.33924, 2.18108, 2.25062, 2.14112, 2.1984, 2.09772, 2.13145,
    2.08836, 2.12187, 2.03728, 2.08679, 2.05492, 2.05858, 2.04114, 2.06094,
    2.02109, 2.02455, 1.99134, 2.01884, 2.01627, 2.0241, 1.96923, 2.01425,
    1.9681, 1.98454, 1.97773, 1.99193, 1.95293, 1.96469, 1.97371, 1.99114,
    1.93276, 1.96951, 1.9455, 1.96897, 1.96819, 1.94352, 1.9391, 1.97577,
    1.94596, 1.94278, 1.95162, 1.94947, 1.9354, 1.95432, 1.94189, 1.93058,
    1.92598, 1.93135, 1.92705, 1.94832, 1.94477, 1.93304, 1.91891, 1.92816,
    1.93071, 1.92636, 1.92686, 1.94204, 1.91158, 1.93024, 1.93606, 1.94116,
    1.94119, 1.92857, 1.92096, 1.94413, 1.90939, 1.90963, 1.91973, 1.92188,
    1.92554, 1.92088, 1.91027, 1.93532, 1.92246, 1.92766, 1.90794, 1.91497,
    1.92607, 1.93512, 1.91289, 1.94698, 1.92398, 1.93819, 1.91674, 1.93119,
    1.93532, 1.91059, 1.91698, 1.91589, 1.92052, 1.91982, 1.90968, 1.90836,
    1.91082, 1.91337, 1.8941, 1.92452, 1.92157, 1.90225, 1.90111, 1.89789,
    1.90852, 1.91614, 1.90442, 1.90563, 1.89635, 1.91467, 1.89847, 1.91982,
    1.90748, 1.93264, 1.89417, 1.91169, 1.89229, 1.92045, 1.9015, 1.90308,
    1.91328, 1.90585, 1.90799, 1.90027, 1.90334, 1.90647, 1.90834, 1.90034,
    1.91302, 1.91276, 1.9122, 1.91081, 1.88134, 1.902, 1.91332, 1.90927,
    1.92343, 1.91227, 1.91807, 1.90075, 1.90756, 1.91373, 1.90331, 1.90487,
    1.90561, 1.91607, 1.92025, 1.91531, 1.91193, 1.89599, 1.91862, 1.90261,
    1.90792, 1.90866, 1.89406, 1.91595, 1.89274, 1.89967, 1.90876, 1.89717,
    1.90859, 1.91391, 1.90559, 1.90095, 1.90033, 1.90289, 1.90295, 1.90713,
    1.90025, 1.88309, 1.90272, 1.91291, 1.90952, 1.8967, 1.9081, 1.90577,
    1.91587, 1.88008, 1.89939, 1.90212, 1.93413, 1.89113, 1.92183, 1.88261,
    1.91105, 1.91124, 1.91656, 1.91666, 1.89339, 1.90816, 1.90318, 1.90783,
    1.89488, 1.91292, 1.91516, 1.9127, 1.89191, 1.88983, 1.90066, 1.90265,
    1.90542, 1.9038, 1.8986, 1.89256, 1.88351, 1.8852, 1.92514, 1.88772,
    1.9043, 1.9022, 1.89706, 1.89796, 1.89817, 1.88593, 1.9075, 1.88818,
    1.89838, 1.89365, 1.89466, 1.90708, 1.90425, 1.91575, 1.89347, 1.89518,
    1.90846, 1.91872, 1.90166, 1.89268, 1.90013, 1.90088, 1.90508, 1.90721,
    1.92669, 1.90787, 1.87833, 1.90566, 1.88696, 1.8908, 1.8995, 1.89255,
    1.88287, 1.89618, 1.89197, 1.89878, 1.88857, 1.88001, 1.88883, 1.88522,
    1.91297, 1.89276, 1.89648, 1.89694, 1.9099, 1.89939, 1.90364, 1.91509,
    1.88928, 1.90324, 1.89895, 1.90954, 1.89269, 1.88783, 1.89777, 1.90334,
    1.88776, 1.87254, 1.90645, 1.90498, 1.90964, 1.89432, 1.90426, 1.91295,
    1.91371, 1.90281, 1.88529, 1.89394, 1.91211, 1.89553, 1.88646, 1.89605,
    1.89684, 1.89165, 1.89019, 1.90702, 1.90584, 1.8982, 1.89138, 1.88391,
    1.91162, 1.89593, 1.87554, 1.89157, 1.89109, 1.91159, 1.88954, 1.89924,
    1.90355, 1.88566, 1.91358, 1.89022, 1.9111, 1.90806, 1.90755, 1.90232,
    1.91273, 1.8973, 1.9001, 1.89809, 1.87097, 1.906, 1.90316, 1.89894,
    1.88788, 1.89129, 1.91002, 1.89125, 1.88322, 1.88575, 1.91677, 1.89967,
    1.88543, 1.90811, 1.90059, 1.89444, 1.89978, 1.89799, 1.90427, 1.90272,
    1.9061, 1.90819, 1.90026, 1.89927, 1.90479, 1.88755, 1.88131, 1.89867,
    1.90644, 1.88228, 1.88071, 1.90087, 1.90146, 1.87144, 1.91352, 1.9141,
    1.89858, 1.89862, 1.90519, 1.90058, 1.89918, 1.90559, 1.89944, 1.90062,
    1.89251, 1.90507, 1.88644, 1.89033, 1.88505, 1.88696, 1.87862, 1.90548,
    1.89442, 1.90166, 1.89409, 1.89856, 1.88847, 1.90425, 1.90832, 1.88451,
    1.9019, 1.90881, 1.91162, 1.90154, 1.90307, 1.91407, 1.88398, 1.91621,
    1.88462, 1.90465, 1.87257, 1.89214, 1.90098, 1.90786, 1.88021, 1.90179,
    1.90588, 1.88598, 1.88188, 1.9037, 1.894, 1.90562, 1.9023, 1.90257,
    1.91506, 1.88724, 1.89661, 1.89036, 1.91581, 1.89118, 1.89605, 1.88842,
    1.8849, 1.89656, 1.89162, 1.88283, 1.89988, 1.89194, 1.89482, 1.9133,
    1.89462, 1.89691, 1.89646, 1.90788, 1.91017, 1.87967, 1.90209, 1.90721,
    1.91069, 1.89462, 1.88581, 1.88022, 1.88962, 1.89843, 1.89884, 1.88286,
    1.90381, 1.89093, 1.89283, 1.92003, 1.89169, 1.91138, 1.89636, 1.91967,
    1.8953, 1.90448, 1.90181, 1.90356, 1.89381, 1.90262, 1.87854, 1.89806,
    1.92212, 1.90101, 1.88927, 1.88755, 1.89137, 1.89204, 1.90466, 1.89143,
    1.90659, 1.90207, 1.9059, 1.90186, 1.89992, 1.90816, 1.89709, 1.90251,
    1.8906, 1.8974, 1.90416, 1.89726, 1.88361, 1.89477, 1.87703, 1.89657,
    1.9032, 1.8782, 1.90761, 1.90883, 1.89922, 1.88931, 1.90294, 1.91728,
    1.8975, 1.90712, 1.90092, 1.90051, 1.90849, 1.88904, 1.90892, 1.88063,
    1.88996, 1.90751, 1.86961, 1.90242, 1.90487, 1.89017, 1.89402, 1.90003,
    1.88839, 1.89566, 1.90426, 1.90072, 1.89577, 1.88399, 1.89207, 1.90252,
    1.88967, 1.90138, 1.91235, 1.90589, 1.89749, 1.91055, 1.88218, 1.90668,
    1.90062, 1.88645, 1.91537, 1.88989, 1.87472, 1.90244, 1.91055, 1.90391,
    1.8828, 1.89432, 1.90331, 1.88782, 1.88651, 1.91428, 1.87854, 1.88879,
    1.90204, 1.88806, 1.9132, 1.87505, 1.89064, 1.89937, 1.90317, 1.87959,
    1.87987, 1.91334, 1.89087, 1.90313, 1.88701, 1.88566, 1.91023, 1.89054,
    1.8945, 1.89948, 1.89636, 1.88011, 1.8943, 1.89894, 1.89616, 1.89537,
    1.88978, 1.88551, 1.90539, 1.89958, 1.88902, 1.8978, 1.88383, 1.89353,
    1.87327, 1.89261, 1.89132, 1.91017, 1.8932, 1.90353, 1.89165, 1.90393,
    1.89115, 1.89779, 1.90125, 1.89697, 1.88323, 1.8885, 1.89575, 1.91027,
    1.89907, 1.88814, 1.90582, 1.88155, 1.90385, 1.88722, 1.89629, 1.89471,
    1.90323, 1.91022, 1.87994, 1.88495, 1.90931, 1.89433, 1.89826, 1.85809,
    1.89566, 1.90934, 1.89164, 1.88081, 1.8992, 1.87815, 1.89481, 1.90496,
    1.87239, 1.89234, 1.89378, 1.89824, 1.87987, 1.88727, 1.88735, 1.8909,
    1.88136, 1.88493, 1.89821, 1.89608, 1.90322, 1.89722, 1.90471, 1.90275,
    1.90514, 1.89977, 1.89659, 1.8967, 1.89102, 1.89847, 1.87468, 1.89986,
    1.89101, 1.90189, 1.89183, 1.88167, 1.88848, 1.8998, 1.90078, 1.88937,
    1.89129, 1.89438, 1.88236, 1.88634, 1.90459, 1.88899, 1.9121, 1.89335,
    1.88067, 1.89845, 1.90379, 1.89761, 1.88957, 1.89529, 1.89919, 1.89856,
    1.87728, 1.90058, 1.8912, 1.89529, 1.90655, 1.89635, 1.89044, 1.90135,
    1.89168, 1.88689, 1.88565, 1.88905, 1.88723, 1.87604, 1.91439, 1.90133,
    1.90071, 1.90261, 1.89697, 1.89686, 1.90015, 1.90973, 1.89383, 1.89142,
    1.8829, 1.87547, 1.88567, 1.87654, 1.90044, 1.89668, 1.88688, 1.8951,
    1.90837, 1.90282, 1.8893, 1.88972, 1.89732, 1.89699, 1.89371, 1.91153,
    1.90202, 1.89278, 1.88933, 1.88005, 1.9068, 1.89558, 1.89649, 1.89931,
    1.89903, 1.89768, 1.8823, 1.88875, 1.89121, 1.90444, 1.90323, 1.87558,
    1.90504, 1.90253, 1.90053, 1.89214, 1.90118, 1.88344, 1.89658, 1.89599,
    1.89615, 1.88639, 1.88376, 1.88869, 1.89978, 1.90987, 1.88091, 1.91121,
    1.88613, 1.88337, 1.89822, 1.88458, 1.91133, 1.88807, 1.89796, 1.89759,
    1.90655, 1.88328, 1.89338, 1.90343, 1.89968, 1.8861, 1.90912, 1.89127,
    1.87304, 1.88516, 1.88347, 1.91319, 1.89017, 1.8873, 1.90613, 1.89508,
    1.87199, 1.90935, 1.90179, 1.89347, 1.88066, 1.88283, 1.91498, 1.9015,
    1.886, 1.90733, 1.89976, 1.88605, 1.89132, 1.8883, 1.90426, 1.88447,
    1.89635, 1.88897, 1.90566, 1.89833, 1.90698, 1.89831, 1.88439, 1.8969,
    1.89293, 1.88194, 1.87484, 1.89074, 1.90319, 1.91112, 1.88657, 1.9033,
    1.89549, 1.88448, 1.90199, 1.89799, 1.87417, 1.88978, 1.8832, 1.88876,
    1.89214, 1.88684, 1.90323, 1.9013, 1.89762, 1.87905, 1.88232, 1.90718,
    1.87753, 1.9065, 1.90041, 1.88791, 1.9134, 1.8941, 1.89439, 1.90251,
    1.89448, 1.90285, 1.89068, 1.88837, 1.89769, 1.88539, 1.88133, 1.90799,
    1.89069, 1.89388, 1.89132, 1.90184, 1.87038, 1.90038, 1.90289, 1.89678,
    1.88616, 1.90152, 1.89762, 1.89782, 1.9074, 1.89764, 1.89548, 1.88731,
    1.90852, 1.90634, 1.88099, 1.91536, 1.89966, 1.87809, 1.88994, 1.88381,
    1.89081, 1.88562, 1.89853, 1.90482, 1.88468, 1.90035, 1.88939, 1.89295,
    1.90462, 1.89681, 1.88103, 1.88996, 1.89812, 1.90114, 1.88874, 1.90127,
    1.87943, 1.88883, 1.89715, 1.90068, 1.8923, 1.88333, 1.91323, 1.89676,
    1.88366, 1.89779, 1.91161, 1.88421, 1.89144, 1.90206, 1.88643, 1.90517,
    1.90919, 1.89141, 1.88577, 1.89182, 1.885, 1.90218, 1.89239, 1.90625,
    1.88104, 1.89626, 1.89945, 1.88308, 1.90484, 1.89541, 1.87879, 1.89445,
    1.89553, 1.88854, 1.88014, 1.91178, 1.91681, 1.90214, 1.89259, 1.89686,
    1.88722, 1.89506, 1.88619, 1.89281, 1.89551, 1.8855, 1.88574, 1.90563,
    1.90001, 1.88354, 1.88374, 1.90892, 1.87765, 1.89739, 1.90702, 1.89368,
    1.89146, 1.89516, 1.8783, 1.89885, 1.89899, 1.88313, 1.90027, 1.90857,
    1.89404, 1.90807, 1.90308, 1.87233, 1.88823, 1.8987, 1.87895, 1.88939,
    1.8966, 1.90515, 1.8941, 1.8914, 1.88204, 1.902, 1.91323, 1.89711,
    1.87751, 1.89697, 1.89793, 1.88654, 1.88197, 1.89874, 1.89499, 1.88469,
    1.88786, 1.88063, 1.89704, 1.89433, 1.89479, 1.89705, 1.8853, 1.90603,
    1.89423, 1.91276, 1.88896, 1.89314, 1.89921, 1.88988, 1.90855, 1.8792,
    1.89072, 1.88432, 1.87707, 1.91261, 1.90356, 1.88729, 1.8989, 1.88147,
    1.88846, 1.90401, 1.90417, 1.88635, 1.88752, 1.90154, 1.87506, 1.8958,
    1.89085, 1.88669, 1.90684, 1.87381, 1.88418, 1.8935, 1.89989, 1.87953,
    1.89611, 1.88679, 1.89886, 1.8935, 1.89282, 1.9088, 1.91984, 1.88528,
    1.90889, 1.89211, 1.88465, 1.9064, 1.88655, 1.88969, 1.89699, 1.90052,
    1.87937};

const double kRcrSSConstants[2][8] = {
    {0, 0, 0, 0, .202399, -.29158, -.03321, -.18181},
    {0, 0, 0, 0, .464231, .26031, .363776, .454703}};

inline bool rcr_is_equal(double a, double b) {
    if (std::fabs(a - b) < std::numeric_limits<double>::min()) return true;
    const double rel =
        std::fabs(a - b) / std::max(std::fabs(a), std::fabs(b));
    return rel <= 1e-8;
}

// 官方 distinctValuesCheck：flagged 中至少 3 个不同值（isEqual 容差）
bool rcr_distinct_values(const std::vector<bool>& flags,
                         const std::vector<double>& y) {
    bool have_a = false, have_b = false;
    double a = 0.0, b = 0.0;
    for (std::size_t i = 0; i < y.size(); ++i) {
        if (!flags[i]) continue;
        if (!have_a) {
            a = y[i];
            have_a = true;
            continue;
        }
        if (!have_b && !rcr_is_equal(y[i], a)) {
            b = y[i];
            have_b = true;
            continue;
        }
        if (have_b && !rcr_is_equal(y[i], a) && !rcr_is_equal(y[i], b))
            return true;
    }
    return false;
}

// 官方 getMedian（含偶数平均与奇数中位的插值语义；输入顺序即官方顺序）
inline double rcr_get_median(const std::vector<double>& y) {
    if (y.size() <= 1) return y.empty() ? 0.0 : y[0];
    const std::size_t n = y.size();
    const int high = (int)std::floor((double)n / 2.0);
    const int low = high - 1;
    const double total = (double)n;
    const double running =
        (n % 2 == 0) ? (double)n / 2.0 + 0.5 : (double)n / 2.0;
    return y[(std::size_t)low] +
           (0.5 * total - running + 1.0) *
               (y[(std::size_t)high] - y[(std::size_t)low]);
}

// 官方加权中位数（getMedian(trueCount, w, y)）
inline double rcr_get_median_w(const std::vector<double>& w,
                               const std::vector<double>& y) {
    if (y.size() <= 1) return y.empty() ? 0.0 : y[0];
    double total = 0.0;
    for (double wi : w) total += wi;
    std::size_t sc = 0;
    double running = w[0] * 0.5;
    while (running < 0.5 * total) {
        ++sc;
        running += w[sc - 1] * 0.5 + w[sc] * 0.5;
    }
    if (sc == 0) return y[0];
    const double span = w[sc - 1] * 0.5 + w[sc] * 0.5;
    return y[sc - 1] +
           (0.5 * total - (running - span)) / span *
               (y[sc] - y[sc - 1]);
}

// 官方 inverf 近似（A&S 7.1.25 变体）
inline double rcr_inverf(double x) {
    x = std::log(1.0 - x * x);
    return std::sqrt(2.0) *
           std::sqrt(-2.0 / (kRcrPi * kRcrInverfMult) - x * 0.5 +
                     std::sqrt(std::pow(2.0 / (kRcrPi * kRcrInverfMult) +
                                            x * 0.5,
                                        2.0) -
                               x / kRcrInverfMult));
}

std::vector<double> rcr_get_xvec(std::size_t n) {
    std::vector<double> x(n);
    if (n == 0) return x;
    double s = 0.682689;
    x[0] = rcr_inverf(s / (double)n);
    for (std::size_t i = 1; i < n; ++i) {
        s += 1.0;
        x[i] = rcr_inverf(s / (double)n);
    }
    return x;
}

std::vector<double> rcr_get_xvec_w(const std::vector<double>& w) {
    const std::size_t n = w.size();
    std::vector<double> x(n);
    if (n == 0) return x;
    double wsum = w[0];
    for (std::size_t i = 1; i < n; ++i) wsum += w[i];
    const double inv = (wsum != 0.0) ? 1.0 / wsum : 0.0;
    double s = 0.682689 * w[0];
    x[0] = rcr_inverf(s * inv);
    for (std::size_t i = 1; i < n; ++i) {
        s += 0.317311 * w[i - 1] + 0.682689 * w[i];
        x[i] = rcr_inverf(s * inv);
    }
    return x;
}

inline int rcr_count_under_one(const std::vector<double>& x) {
    if (x.empty()) return 0;
    if (x.size() == 1) return x[0] < 1.0 ? 1 : 0;
    int c = 0;
    while (c < (int)x.size() && x[(std::size_t)c] < 1.0) ++c;
    return c;
}

inline double rcr_origin_regression(int start, int end,
                                    const std::vector<double>& x,
                                    const std::vector<double>& y) {
    double xy = 0.0, xx = 0.0;
    for (int i = start; i < end; ++i) {
        xy += x[(std::size_t)i] * y[(std::size_t)i];
        xx += x[(std::size_t)i] * x[(std::size_t)i];
    }
    return xx != 0.0 ? xy / xx : 0.0;
}

inline double rcr_origin_regression_w(int start, int end,
                                      const std::vector<double>& w,
                                      const std::vector<double>& x,
                                      const std::vector<double>& y) {
    double xy = 0.0, xx = 0.0;
    for (int i = start; i < end; ++i) {
        const double wx = w[(std::size_t)i] * x[(std::size_t)i];
        xy += wx * y[(std::size_t)i];
        xx += wx * x[(std::size_t)i];
    }
    return xx != 0.0 ? xy / xx : 0.0;
}

int rcr_mfinder(int low, int high, int last_under, int increment,
                const std::vector<double>& x,
                const std::vector<double>& y) {
    bool stop = false;
    int best_m = -1, m_low = -1, m_high = -1;
    double min_error = 999999.0;
    while (!stop) {
        for (int m = low; m < high; m += increment) {
            double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, error = 0;
            const double xat = x[(std::size_t)m];
            for (int i = 0; i <= m; ++i) {
                a += x[(std::size_t)i] * x[(std::size_t)i];
                e += x[(std::size_t)i] * y[(std::size_t)i];
            }
            for (int i = m + 1; i < last_under + 1; ++i) {
                const double xd = x[(std::size_t)i] - xat;
                a += xat * xat;
                b += xat * xd;
                d += xd * xd;
                e += xat * y[(std::size_t)i];
                f += y[(std::size_t)i] * xd;
            }
            c = b;
            if (a != 0.0 && (d - c * b / a) != 0.0) {
                const double tau = (f - e * c / a) / (d - c * b / a);
                const double sigma = (e - tau * b) / a;
                for (int i = 0; i <= m; ++i) {
                    const double fac = sigma * x[(std::size_t)i] -
                                       y[(std::size_t)i];
                    error += fac * fac;
                }
                for (int i = m + 1; i < last_under + 1; ++i) {
                    const double fac = sigma * xat +
                                       tau * (x[(std::size_t)i] - xat) -
                                       y[(std::size_t)i];
                    error += fac * fac;
                }
                if (error < min_error) {
                    min_error = error;
                    best_m = m;
                    m_low = std::max(best_m - increment - 1, 1);
                    m_high = std::min(best_m + increment + 1, last_under);
                }
            }
        }
        if (increment > 1) {
            increment = std::max((int)std::floor(
                                     (double)(m_high - m_low) / 6.36),
                                 1);
        } else {
            stop = true;
        }
        low = m_low;
        high = m_high;
        min_error = 999999.0;
    }
    if (low == high) return low;
    return best_m;
}

int rcr_mfinder_w(int low, int high, int last_under, int increment,
                  const std::vector<double>& w,
                  const std::vector<double>& x,
                  const std::vector<double>& y) {
    bool stop = false;
    int best_m = -1, m_low = -1, m_high = -1;
    double min_error = std::numeric_limits<double>::max();
    while (!stop) {
        for (int m = low; m < high; m += increment) {
            double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, error = 0;
            const double xat = x[(std::size_t)m];
            for (int i = 0; i <= m; ++i) {
                const double wx = w[(std::size_t)i] * x[(std::size_t)i];
                a += wx * x[(std::size_t)i];
                e += wx * y[(std::size_t)i];
            }
            for (int i = m + 1; i < last_under + 1; ++i) {
                const double xd = x[(std::size_t)i] - xat;
                const double wi = w[(std::size_t)i];
                a += xat * xat * wi;
                b += xat * wi * xd;
                d += wi * xd * xd;
                e += xat * wi * y[(std::size_t)i];
                f += wi * y[(std::size_t)i] * xd;
            }
            c = b;
            if (a != 0.0 && (d - c * b / a) != 0.0) {
                const double tau = (f - e * c / a) / (d - c * b / a);
                const double sigma = (e - tau * b) / a;
                for (int i = 0; i <= m; ++i) {
                    const double fac = sigma * x[(std::size_t)i] -
                                       y[(std::size_t)i];
                    error += w[(std::size_t)i] * fac * fac;
                }
                for (int i = m + 1; i < last_under + 1; ++i) {
                    const double fac = sigma * xat +
                                       tau * (x[(std::size_t)i] - xat) -
                                       y[(std::size_t)i];
                    error += w[(std::size_t)i] * fac * fac;
                }
                if (error < min_error) {
                    min_error = error;
                    best_m = m;
                    m_low = std::max(best_m - increment - 1, 1);
                    m_high = std::min(best_m + increment + 1, last_under);
                }
            }
        }
        if (increment > 1) {
            increment = std::max((int)std::floor(
                                     (double)(m_high - m_low) / 6.36),
                                 1);
        } else {
            stop = true;
        }
        low = m_low;
        high = m_high;
        min_error = std::numeric_limits<double>::max();
    }
    if (low == high) return low;
    return best_m;
}

inline double rcr_fit_sl(const std::vector<double>& x,
                         const std::vector<double>& y) {
    return rcr_origin_regression(0, rcr_count_under_one(x), x, y);
}

inline double rcr_fit_sl_w(const std::vector<double>& w,
                           const std::vector<double>& x,
                           const std::vector<double>& y) {
    return rcr_origin_regression_w(0, rcr_count_under_one(x), w, x, y);
}

inline double rcr_get_single_fn(int n) {
    if (n < 1001) return kRcrSSUnity[n];
    return 39.2519 * std::pow((double)n, -0.7969) + 1.8688;
}

inline double rcr_fn_ratio(const std::vector<double>& x,
                           const std::vector<double>& w) {
    const int c = rcr_count_under_one(x);
    if (c < 2) return 0.0;
    double mean = 0.0;
    for (int i = 0; i < c; ++i) mean += w[(std::size_t)i];
    mean /= (double)c;
    double sd = 0.0;
    for (int i = 0; i < c; ++i)
        sd += (w[(std::size_t)i] - mean) * (w[(std::size_t)i] - mean);
    sd = std::sqrt(sd / (double)(c - 1));
    return sd / mean;
}

inline double rcr_get_single_fn_w(int n, const std::vector<double>& x,
                                  const std::vector<double>& w) {
    const double ratio = rcr_fn_ratio(x, w);
    const double logn = std::log10((double)n);
    const double logx = std::log10(ratio);
    const double y1 = (n < 1001) ? kRcrSSUnity[n]
                                 : 39.2519 * std::pow((double)n, -0.7969) +
                                       1.8688;
    if (ratio == 0.0) return y1;
    double f;
    if (3 < n && n < 8) {
        const double a1 = kRcrSSConstants[0][n];
        const double b1 = kRcrSSConstants[1][n];
        f = y1 * std::pow(10.0, std::pow(10.0, a1 + b1 * logx));
    } else if (7 < n && n < 1001) {
        const double b1 = -0.3556 * std::pow(logn, 6) +
                          3.7036 * std::pow(logn, 5) -
                          14.932 * std::pow(logn, 4) +
                          29.176 * std::pow(logn, 3) -
                          28.81 * std::pow(logn, 2) +
                          14.397 * logn - 2.6451;
        const double a1 = 0.2313 * std::pow(logn, 6) -
                          3.02 * std::pow(logn, 5) +
                          15.997 * std::pow(logn, 4) -
                          43.713 * std::pow(logn, 3) +
                          64.629 * std::pow(logn, 2) -
                          49.976 * logn + 15.484 +
                          std::pow(-1.0, n) * 0.1513 *
                              std::pow((double)n, -0.471);
        f = y1 * std::pow(10.0, std::pow(10.0, a1 + b1 * logx));
    } else if (n > 1000) {
        f = y1;
    } else {
        f = -999999.0;
    }
    return f;
}

inline double rcr_fit_dl(int counter, const std::vector<double>& x,
                         const std::vector<double>& y) {
    const int under = rcr_count_under_one(x);
    const double single = rcr_origin_regression(0, under, x, y);
    if (x.size() < 4) return single;
    const int m = rcr_mfinder(
        1, under - 1, under - 1,
        (int)std::max((double)y.size() / 6.36, 1.0), x, y);
    const double xat = x[(std::size_t)m];
    double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    for (int i = 0; i <= m; ++i) {
        a += x[(std::size_t)i] * x[(std::size_t)i];
        e += x[(std::size_t)i] * y[(std::size_t)i];
    }
    for (int i = m + 1; i < under; ++i) {
        const double xd = x[(std::size_t)i] - xat;
        a += xat * xat;
        b += xat * xd;
        d += xd * xd;
        e += xat * y[(std::size_t)i];
        f += y[(std::size_t)i] * xd;
    }
    c = b;
    const double tau = (f - e * c / a) / (d - c * b / a);
    double sigma = (e - tau * b) / a;
    double dl_err = 0.0, sl_err = 0.0;
    for (int i = 0; i <= m; ++i) {
        const double fac = sigma * x[(std::size_t)i] - y[(std::size_t)i];
        dl_err += fac * fac;
    }
    for (int i = m + 1; i < under; ++i) {
        const double fac = sigma * xat +
                           tau * (x[(std::size_t)i] - xat) -
                           y[(std::size_t)i];
        dl_err += fac * fac;
    }
    for (int i = 0; i < under; ++i) {
        const double fac = single * x[(std::size_t)i] - y[(std::size_t)i];
        sl_err += fac * fac;
    }
    const double dcs = (sl_err - dl_err) / dl_err;
    if (sigma < 0) sigma = 0.0000000001;
    if (dcs < rcr_get_single_fn(counter)) sigma = single;
    return sigma;
}

inline double rcr_fit_dl_w(int counter, const std::vector<double>& w,
                           const std::vector<double>& x,
                           const std::vector<double>& y) {
    const int under = rcr_count_under_one(x);
    const double single = rcr_origin_regression_w(0, under, w, x, y);
    if (x.size() < 4) return single;
    const int m = rcr_mfinder_w(
        1, under - 1, under - 1,
        (int)std::max((double)y.size() / 6.36, 1.0), w, x, y);
    const double xat = x[(std::size_t)m];
    double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    for (int i = 0; i <= m; ++i) {
        const double wx = w[(std::size_t)i] * x[(std::size_t)i];
        a += wx * x[(std::size_t)i];
        e += wx * y[(std::size_t)i];
    }
    for (int i = m + 1; i < under; ++i) {
        const double xd = x[(std::size_t)i] - xat;
        const double wi = w[(std::size_t)i];
        a += xat * xat * wi;
        b += xat * wi * xd;
        d += wi * xd * xd;
        e += xat * wi * y[(std::size_t)i];
        f += wi * y[(std::size_t)i] * xd;
    }
    c = b;
    const double tau = (f - e * c / a) / (d - c * b / a);
    double sigma = (e - tau * b) / a;
    double dl_err = 0.0, sl_err = 0.0;
    for (int i = 0; i <= m; ++i) {
        const double fac = sigma * x[(std::size_t)i] - y[(std::size_t)i];
        dl_err += w[(std::size_t)i] * fac * fac;
    }
    for (int i = m + 1; i < under; ++i) {
        const double fac = sigma * xat +
                           tau * (x[(std::size_t)i] - xat) -
                           y[(std::size_t)i];
        dl_err += w[(std::size_t)i] * fac * fac;
    }
    for (int i = 0; i < under; ++i) {
        const double fac = single * x[(std::size_t)i] - y[(std::size_t)i];
        sl_err += w[(std::size_t)i] * fac * fac;
    }
    const double dcs = (sl_err - dl_err) / dl_err;
    if (sigma < 0) sigma = 0.0000000001;
    if (dcs < rcr_get_single_fn_w(counter, x, w)) sigma = single;
    return sigma;
}

// 官方 get68th（未加权；diff 已升序）
inline double rcr_get_68th(const std::vector<double>& y) {
    const std::size_t n = y.size();
    if (n <= 1) return y.empty() ? 0.0 : y[0];
    std::size_t sc = 0;
    double running = 0.682689;
    const double total = 0.682689 * (double)n;
    while (running < total) {
        ++sc;
        running += 0.317311 + 0.682689;
    }
    if (sc == 0) return y[0];
    const double span = 0.317311 + 0.682689;
    return y[sc - 1] + (total - (running - span)) / span *
                           (y[sc] - y[sc - 1]);
}

// 官方 get68th（加权；w/y 需按 y 同步升序）
inline double rcr_get_68th_w(std::vector<double> w,
                             std::vector<double> y) {
    const std::size_t n = y.size();
    if (n <= 1) return y.empty() ? 0.0 : y[0];
    std::vector<std::pair<double, double>> p(n);
    for (std::size_t i = 0; i < n; ++i) p[i] = {y[i], w[i]};
    std::sort(p.begin(), p.end());
    for (std::size_t i = 0; i < n; ++i) {
        y[i] = p[i].first;
        w[i] = p[i].second;
    }
    double total = 0.0;
    for (double wi : w) total += wi;
    std::size_t sc = 0;
    double running = w[0] * 0.682689;
    while (running < 0.682689 * total) {
        ++sc;
        running += w[sc - 1] * 0.317311 + w[sc] * 0.682689;
    }
    if (sc == 0) return y[0];
    const double span = w[sc - 1] * 0.317311 + w[sc] * 0.682689;
    return y[sc - 1] +
           (0.682689 * total - (running - span)) / span *
               (y[sc] - y[sc - 1]);
}

inline double rcr_get_single_dl_cf(int n) {
    if (n < 101) return 1.0 / kRcrSSDLUnityCF[n];
    return 1.0 / (1.0 - 3.578 * std::pow((double)n, -0.942));
}

inline double rcr_cf_ratio(const std::vector<double>& w) {
    const std::size_t n = w.size();
    if (n < 2) return 0.0;
    double mean = 0.0;
    for (double wi : w) mean += wi;
    mean /= (double)n;
    double sd = 0.0;
    for (double wi : w) sd += (wi - mean) * (wi - mean);
    sd = std::sqrt(sd / (double)(n - 1));
    return sd / mean;
}

inline double rcr_get_single_dl_cf_w(int n, const std::vector<double>& w) {
    const double x = rcr_cf_ratio(w);
    const double logn = std::log10((double)n);
    const double logx = std::log10(x);
    const double y1 = (n < 101) ? 1.0 / kRcrSSDLUnityCF[n]
                                : 1.0 / (1.0 - 3.578 *
                                                  std::pow((double)n, -0.942));
    if (x == 0.0) return y1;
    double a1, b1, cf;
    if (n == 2) {
        b1 = 0.273907084639124;
        a1 = -3.15279135630884;
        cf = y1 * std::pow(10.0, std::pow(10.0, a1 + b1 * logx));
    } else if (n == 3) {
        b1 = 0.448654915529039;
        a1 = -1.19134294551807;
        cf = y1 / std::pow(10.0, std::pow(10.0, a1 + b1 * logx));
    } else if (n == 4) {
        b1 = 3.38253309393705;
        a1 = -1.05087405984868;
        cf = y1 * std::pow(10.0, std::pow(10.0, a1 + b1 * logx));
    } else if (n == 5) {
        b1 = 0.118507989164207;
        a1 = -1.41453721585464;
        cf = y1 / std::pow(10.0, std::pow(10.0, a1 + b1 * logx));
    } else {
        b1 = 0.1196 * logn + 4.5073;
        a1 = -0.7914 * logn + 0.0243;
        cf = y1 * std::pow(10.0, std::pow(10.0, a1 + b1 * logx));
    }
    return cf;
}

// 官方 erfcCustom(z) = erfc(z/√2)（A&S 7.1.26，16 次幂有理近似）
inline double rcr_erfc_custom(double z) {
    const double x = z / std::sqrt(2.0);
    return 1.0 / std::pow(
                     1.0 + x * (0.0705230784 +
                                x * (0.0422820123 +
                                     x * (0.0092705272 +
                                          x * (0.0001520143 +
                                               x * (0.0002765672 +
                                                    0.0000430638 * x))))),
                     16);
}

enum class RcrMu { kMean, kMedian };
enum class RcrSigma { kDoubleLine, k68th, kStdDev };

// 单段 iterative single-sigma RCR（官方 iterativeSingleSigmaRCR）
void rcr_iterative_pass(RcrMu mu_t, RcrSigma sig_t,
                        const std::vector<double>& vals,
                        const std::vector<double>* weights,
                        std::vector<bool>& accept) {
    while (true) {
        std::vector<double> cur, cur_w;
        std::vector<std::size_t> cur_idx;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            if (!accept[i]) continue;
            cur.push_back(vals[i]);
            if (weights) cur_w.push_back((*weights)[i]);
            cur_idx.push_back(i);
        }
        if (cur.empty()) break;
        const bool weighted = weights != nullptr;
        double mu = 0.0;
        if (mu_t == RcrMu::kMedian) {
            // 官方 handleMuTechSelect(MEDIAN) 先排序再取中位数：
            // 未加权 sort(y)；加权 sort(w, y)（权重随值同步排序）。
            if (weighted) {
                std::vector<std::pair<double, double>> pw(cur.size());
                for (std::size_t i = 0; i < cur.size(); ++i)
                    pw[i] = {cur[i], cur_w[i]};
                std::sort(pw.begin(), pw.end());
                std::vector<double> sy(cur.size()), sw(cur.size());
                for (std::size_t i = 0; i < cur.size(); ++i) {
                    sy[i] = pw[i].first;
                    sw[i] = pw[i].second;
                }
                mu = rcr_get_median_w(sw, sy);
            } else {
                std::vector<double> sy = cur;
                std::sort(sy.begin(), sy.end());
                mu = rcr_get_median(sy);
            }
        } else {
            double s = 0.0;
            if (weighted) {
                double ws = 0.0;
                for (std::size_t i = 0; i < cur.size(); ++i) {
                    s += cur_w[i] * cur[i];
                    ws += cur_w[i];
                }
                mu = ws != 0.0 ? s / ws : 0.0;
            } else {
                for (double v : cur) s += v;
                mu = s / (double)cur.size();
            }
        }
        double max_hold = -99999.0;
        std::size_t max_pos = 0;
        std::vector<double> diff(cur.size());
        for (std::size_t i = 0; i < cur.size(); ++i) {
            const double h = std::fabs(cur[i] - mu);
            diff[i] = h;
            if (h > max_hold) {
                max_hold = h;
                max_pos = i;
            }
        }
        const int true_count = (int)cur.size();
        // 官方：sort(diff)（加权时 sort(trueW, diff) 同步）
        if (weighted) {
            std::vector<std::size_t> ord(cur.size());
            for (std::size_t i = 0; i < ord.size(); ++i) ord[i] = i;
            std::sort(ord.begin(), ord.end(),
                      [&](std::size_t a, std::size_t b) {
                          return diff[a] < diff[b];
                      });
            std::vector<double> w2(cur.size()), d2(cur.size());
            for (std::size_t i = 0; i < ord.size(); ++i) {
                w2[i] = cur_w[ord[i]];
                d2[i] = diff[ord[i]];
            }
            cur_w = std::move(w2);
            diff = std::move(d2);
        } else {
            std::sort(diff.begin(), diff.end());
        }
        double st_dev = 0.0;
        if (sig_t == RcrSigma::kDoubleLine) {
            const std::vector<double> x =
                weighted ? rcr_get_xvec_w(cur_w) : rcr_get_xvec(cur.size());
            const int under = rcr_count_under_one(x);
            if (under > 2) {
                st_dev = weighted ? rcr_fit_dl_w(true_count, cur_w, x, diff)
                                  : rcr_fit_dl(true_count, x, diff);
            } else if (under > 1) {
                st_dev = weighted ? rcr_fit_sl_w(cur_w, x, diff)
                                  : rcr_fit_sl(x, diff);
            } else {
                st_dev = weighted ? rcr_get_68th_w(cur_w, diff)
                                  : rcr_get_68th(diff);
            }
        } else if (sig_t == RcrSigma::k68th) {
            st_dev = weighted ? rcr_get_68th_w(cur_w, diff)
                              : rcr_get_68th(diff);
        } else {
            double ss = 0.0, ws = 0.0, wsq = 0.0;
            if (weighted) {
                for (std::size_t i = 0; i < diff.size(); ++i) {
                    ss += cur_w[i] * diff[i] * diff[i];
                    ws += cur_w[i];
                    wsq += cur_w[i] * cur_w[i];
                }
                st_dev = std::sqrt(ss / (ws - 1.0 * wsq / ws));
            } else {
                for (double v : diff) ss += v * v;
                st_dev = std::sqrt(ss / (double)(true_count - 1));
            }
        }
        const double sigma =
            st_dev * (weighted ? rcr_get_single_dl_cf_w(true_count, cur_w)
                               : rcr_get_single_dl_cf(true_count));
        const double zmax = max_hold / sigma;
        if (!rcr_distinct_values(accept, vals) ||
            (double)true_count * rcr_erfc_custom(zmax) >= 0.5)
            break;
        accept[cur_idx[max_pos]] = false;
    }
}

} // namespace

extern "C" {

int p2_reject_stack(const P2SampleStackView* in, P2RejectionResult* out) {
    if (in == nullptr || out == nullptr) return 1;
    std::uint8_t* accepted_out = out->accepted;
    std::memset(out, 0, sizeof(*out));
    out->accepted = accepted_out;
    const std::uint32_t n = in->count;
    if (n == 0) { out->status = 1; return 0; }
    if (out->accepted == nullptr) return 1;

    // NaN/Inf 输入：标记无效
    for (std::uint32_t i = 0; i < n; ++i) {
        if (!std::isfinite(in->values[i])) {
            out->accepted[i] = 0;
            ++out->rejected_low;  // 无效归为拒绝（诊断）
            out->status = 3;
        } else {
            out->accepted[i] = 1;
        }
    }

    if (in->method == P2_REJECT_NONE) {
        // none：有效样本全接受
        std::uint32_t kept = 0;
        for (std::uint32_t i = 0; i < n; ++i) {
            if (in->valid != nullptr && !in->valid[i]) continue;
            out->accepted[i] = 1;
            ++kept;
        }
        out->accepted_count = kept;
        out->status = 0;
        return 0;
    }

    // 收集有效值（UPM-calibrated）
    std::vector<double> vals;
    std::vector<std::uint32_t> idx;
    for (std::uint32_t i = 0; i < n; ++i) {
        if (in->valid != nullptr && !in->valid[i]) continue;
        if (!std::isfinite(in->values[i])) continue;
        vals.push_back(in->values[i]);
        idx.push_back(i);
    }
    if (vals.size() < static_cast<std::size_t>(in->min_samples)) {
        out->status = 1;  // min-samples
        return 0;
    }

    const double lo = in->sigma_low != 0.0 ? in->sigma_low : -4.0;
    const double hi = in->sigma_high != 0.0 ? in->sigma_high : 3.0;
    const int max_iter = in->max_iterations > 0 ? in->max_iterations : 8;

    if (in->method == P2_REJECT_AVERAGED_SIGMA) {
        // IRAF AVSIGCLIP 语义（公开定义）：迭代中按"当前保留样本"计算均值与
        // 平均 sigma（对每样本残差绝对值的平均，而非 MAD），再按 lo/hi sigma 拒绝。
        std::vector<bool> accept(vals.size(), true);
        int it = 0;
        for (; it < max_iter; ++it) {
            double sum = 0.0; std::size_t n = 0;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (accept[i]) { sum += vals[i]; ++n; }
            }
            if (n < 2) break;
            const double mean = sum / static_cast<double>(n);
            double s = 0.0;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (accept[i]) s += std::fabs(vals[i] - mean);
            }
            s = (s / static_cast<double>(n)) * std::sqrt(3.141592653589793 / 2.0);
            if (s <= 1e-12) break;
            bool changed = false;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (!accept[i]) continue;
                const double z = (vals[i] - mean) / s;
                if (z < lo || z > hi) { accept[i] = false; changed = true; }
            }
            if (!changed) break;
        }
        out->iterations = static_cast<std::uint32_t>(it);
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) { ++kept; }
            else if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
        out->accepted_count = kept;
        out->status = kept == 0 ? 2 : 0;
        return 0;
    }

    if (in->method == P2_REJECT_GENERALIZED_ESD) {
        // NIST Generalized ESD（独立实现，NIST EDA Handbook / Rosner 1983）。
        // 正确流程（P0-05）：
        //   1. 连续移除到上限 r，记录 R_1..R_r 与 λ_1..λ_r；
        //   2. 最终取满足 R_i > λ_i 的**最大 i**，前 i 个 provisional
        //      extremes 为离群（不做"第一轮不显著就停止"的错误简化）。
        // 临界值 λ_i = t_{ν,p}·(n-i)/sqrt((n-i-1+t²)·(n-i+1))，
        // ν = n-i-1，p = 1 - α/(2·(n-i+1))，α=0.05。
        std::vector<bool> accept(vals.size(), true);
        std::vector<double> working = vals;
        const std::size_t max_out = static_cast<std::size_t>(max_iter);
        const double alpha = 0.05;
        std::vector<double> R(max_out, 0.0);
        std::vector<double> Lambda(max_out, 0.0);
        std::vector<std::size_t> removed_idx(max_out, vals.size());
        const std::size_t n0 = vals.size();
        std::size_t n_removed = 0;
        // phase 1：连续移除记录 R_i / λ_i
        for (std::size_t r = 0; r < max_out; ++r) {
            double mean = 0.0; std::size_t n = 0;
            for (std::size_t i = 0; i < working.size(); ++i) {
                if (accept[i]) { mean += working[i]; ++n; }
            }
            if (n < 3) break;
            mean /= static_cast<double>(n);
            double s = 0.0;
            for (std::size_t i = 0; i < working.size(); ++i) {
                if (accept[i]) s += (working[i] - mean) * (working[i] - mean);
            }
            s = std::sqrt(s / static_cast<double>(n - 1));
            if (s <= 1e-12) break;
            std::size_t worst = n;
            double max_r = 0.0;
            std::uint64_t worst_fid = ~0ULL;
            for (std::size_t i = 0; i < working.size(); ++i) {
                if (!accept[i]) continue;
                const double rv = std::fabs(working[i] - mean) / s;
                const std::uint64_t fid = in->frame_ids
                    ? in->frame_ids[idx[i]] : (std::uint64_t)i;
                if (rv > max_r + 1e-15 ||
                    (std::fabs(rv - max_r) <= 1e-15 && fid < worst_fid)) {
                    max_r = rv;
                    worst = i;
                    worst_fid = fid;
                }
            }
            // NIST 临界值（i 为 1-indexed 检测步；n-i = n0 - (r+1)）
            const double n_minus_i = (double)n0 - (double)(r + 1);
            const double nu = n_minus_i - 1.0;
            const double p = 1.0 - alpha / (2.0 * (n_minus_i + 1.0));
            const double tcrit = t_quantile(p, nu);
            const double crit = (tcrit * n_minus_i) /
                std::sqrt((nu + tcrit * tcrit) * (n_minus_i + 1.0));
            R[r] = max_r;
            Lambda[r] = crit;
            removed_idx[r] = worst;
            accept[worst] = false;
            ++n_removed;
        }
        // phase 2：取满足 R_i > λ_i 的最大 i
        std::size_t k_out = 0;
        for (std::size_t r = 0; r < n_removed; ++r)
            if (R[r] > Lambda[r]) k_out = r + 1;
        std::fill(accept.begin(), accept.end(), true);
        for (std::size_t r = 0; r < k_out && r < removed_idx.size(); ++r)
            accept[removed_idx[r]] = false;
        out->iterations = static_cast<std::uint32_t>(k_out);
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) { ++kept; }
            else if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
        out->accepted_count = kept;
        out->status = kept == 0 ? 2 : 0;
        return 0;
    }

    if (in->method == P2_REJECT_LINEAR_FIT) {
        // Siril Linear Fit Clipping（公开算法定义，rejection_float.c
        // LINEARFIT 分支；Siril 1.4.3，GPL ORACLE ONLY，不复制生产源码）：
        //   每轮：对当前保留样本按值排序，x=排序后序号(0..n-1)；
        //   最小二乘拟合 y=a*x+b；sigma=mean|residual|（平均绝对残差）；
        //   low 拒绝：a*i+b-y > sigma*siglow；high 拒绝：
        //   y-(a*i+b) > sigma*sighigh；迭代至无新增拒绝或 n<=3。
        // 值排序使结果与输入 frame 顺序无关（P1-05）。
        std::vector<bool> accept(vals.size(), true);
        int it = 0;
        for (; it < max_iter; ++it) {
            std::vector<std::pair<double, std::size_t>> entries;
            for (std::size_t i = 0; i < vals.size(); ++i)
                if (accept[i]) entries.push_back({vals[i], i});
            if (entries.size() < 4) break;   // Siril：N>3 才继续
            std::sort(entries.begin(), entries.end(),
                      [](const auto& a, const auto& b) {
                          return a.first < b.first;
                      });                    // 值排序（权威定义）
            const std::size_t n = entries.size();
            std::vector<double> yv(n);
            for (std::size_t j = 0; j < n; ++j) yv[j] = entries[j].first;
            std::vector<double> xv(n);
            for (std::size_t j = 0; j < n; ++j) xv[j] = (double)j;
            double a = 0.0, b = 0.0;
            ls_fit_line(xv, yv, &a, &b);
            double sigma = 0.0;
            for (std::size_t j = 0; j < n; ++j)
                sigma += std::fabs(yv[j] - (a * (double)j + b));
            sigma /= (double)n;            // mean |residual|（Siril 定义）
            if (sigma <= 1e-12) break;
            bool changed = false;
            for (std::size_t j = 0; j < n; ++j) {
                const std::size_t orig = entries[j].second;
                const double y = entries[j].first;
                const double fit = a * (double)j + b;
                if (fit - y > sigma * std::fabs(lo)) {   // 低侧
                    accept[orig] = false;
                    changed = true;
                } else if (y - fit > sigma * hi) {       // 高侧
                    accept[orig] = false;
                    changed = true;
                }
            }
            if (!changed) break;
        }
        out->iterations = static_cast<std::uint32_t>(it);
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) { ++kept; }
            else if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
        out->accepted_count = kept;
        out->status = kept == 0 ? 2 : 0;
        return 0;
    }

    if (in->method == P2_REJECT_RCR) {
        // V4 R4：完整 sequential RCR（官方 rcr 2.4.7 performRejection
        // 语义，冻结技术 SS_MEDIAN_DL）：
        //   1) MEDIAN + DOUBLE_LINE；
        //   2) MEDIAN + SIXTY_EIGHTH_PERCENTILE；
        //   3) MEAN + STANDARD_DEVIATION；
        // 每段 iterative Chauvenet（n·erfc(z/√2)<0.5），sigma 乘
        // SS_MEDIAN_DL 校准 CF；加权语义与官方权重一致。
        std::vector<bool> accept(vals.size(), true);
        const bool weighted = in->weights != nullptr;
        std::vector<double> wgt;
        if (weighted) {
            wgt.resize(vals.size());
            for (std::size_t i = 0; i < vals.size(); ++i)
                wgt[i] = in->weights[idx[i]];
        }
        const std::vector<double>* wp = weighted ? &wgt : nullptr;
        rcr_iterative_pass(RcrMu::kMedian, RcrSigma::kDoubleLine, vals, wp,
                           accept);
        rcr_iterative_pass(RcrMu::kMedian, RcrSigma::k68th, vals, wp, accept);
        rcr_iterative_pass(RcrMu::kMean, RcrSigma::kStdDev, vals, wp, accept);
        out->iterations = 3;  // 三段 sequential 链（段数）
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) { ++kept; }
            else if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
        out->accepted_count = kept;
        out->status = kept == 0 ? 2 : 0;
        return 0;
    }

    if (in->method == P2_REJECT_WINSORIZED_SIGMA) {
        // 真 Winsorized Sigma（P0-04 修复，AstroCS 精确定义）：
        //   每轮：普通 mean/std → 把当前样本 winsorize 到
        //   [mean+lo·std, mean+hi·std] → winsorized mean/std 估计 →
        //   用原始样本按 winsorized 估计做 sigma clip。
        // 与普通 Sigma（median/MAD + clip）是不同算法：winsorization
        // 抑制极端值对位置/尺度估计的影响。
        std::vector<bool> accept(vals.size(), true);
        int iters = 0;
        for (; iters < max_iter; ++iters) {
            std::vector<double> cur;
            for (std::size_t i = 0; i < vals.size(); ++i)
                if (accept[i]) cur.push_back(vals[i]);
            if (cur.size() < 2) break;
            double m0 = 0.0;
            for (double x : cur) m0 += x;
            m0 /= (double)cur.size();
            double s0 = 0.0;
            for (double x : cur) s0 += (x - m0) * (x - m0);
            s0 = std::sqrt(s0 / (double)(cur.size() - 1));
            if (s0 <= 1e-12) break;
            const double wlo = m0 + lo * s0;
            const double whi = m0 + hi * s0;
            std::vector<double> wcur = cur;
            for (double& x : wcur) x = std::clamp(x, wlo, whi);
            double wmean = 0.0;
            for (double x : wcur) wmean += x;
            wmean /= (double)wcur.size();
            double ws = 0.0;
            for (double x : wcur) ws += (x - wmean) * (x - wmean);
            ws = std::sqrt(ws / (double)(wcur.size() - 1));
            if (ws <= 1e-12) ws = s0;
            bool changed = false;
            std::vector<bool> next(vals.size(), true);
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (!accept[i]) { next[i] = false; continue; }
                const double z = (vals[i] - wmean) / ws;
                if (z < lo || z > hi) { next[i] = false; changed = true; }
            }
            accept = std::move(next);
            if (!changed) break;
        }
        out->iterations = static_cast<std::uint32_t>(iters);
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) {
                ++kept;
            } else {
                if (vals[i] < 0.0) ++out->rejected_low;
                else ++out->rejected_high;
            }
        }
        out->accepted_count = kept;
        if (out->accepted_count == 0) out->status = 2;
        return 0;
    }

    // Sigma（普通 median/MAD 迭代 clip）
    std::vector<bool> accept(vals.size(), true);
    int iters = 0;
    for (; iters < max_iter; ++iters) {
        std::vector<double> cur;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            if (accept[i]) cur.push_back(vals[i]);
        }
        if (cur.size() < 2) break;
        double m = median(cur);
        double s = mad(cur, m);
        if (s <= 1e-12) {
            // 全同值：无离群
            break;
        }
        bool changed = false;
        std::vector<bool> next(vals.size(), true);
        for (std::size_t i = 0; i < vals.size(); ++i) {
            if (!accept[i]) { next[i] = false; continue; }
            const double z = (vals[i] - m) / s;
            if (z < lo || z > hi) {
                next[i] = false;
                changed = true;
            }
        }
        accept = std::move(next);
        if (!changed) break;
    }
    out->iterations = static_cast<std::uint32_t>(iters);

    std::uint32_t kept = 0;
    for (std::size_t i = 0; i < vals.size(); ++i) {
        out->accepted[idx[i]] = accept[i] ? 1 : 0;
        if (accept[i]) {
            ++kept;
        } else {
            if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
    }
    out->accepted_count = kept;
    if (out->accepted_count == 0) out->status = 2;
    return 0;
}

} // extern "C"
