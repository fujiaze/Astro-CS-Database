#ifndef ASTROCS_V6_P1_CAL_ORACLE_HPP
#define ASTROCS_V6_P1_CAL_ORACLE_HPP

/* 独立 Oracle（不调用被测实现作为真值）
 * ----------------------------------------------------------------------------
 * 1) dense_jcj: 用显式 4x4 矩阵与嵌套循环复算 J C_in J^T（与被测闭式标量路径
 *    结构不同）；
 * 2) term 公式以字面常量独立复算；
 * 3) 共享 covariance 用定种子确定性 MC（共同模式 + 独立噪声）估计联合方差，
 *    与解析 joint = Σ c_p^2 v_ind + (Σ c_p)^2 v_shared 对比。
 * 真值来源: independent_stdlib（无第三方依赖，固定 LCG 种子）。
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace astrocs {
namespace calibration {
namespace v6 {
namespace oracle {

/* 显式矩阵 J C J^T：J 为 1x4 行向量，C=diag(V0..V3)。返回 J C J^T 标量。 */
inline double dense_jcj(const double J[4], const double V[4]) {
  double C[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
  for (int i = 0; i < 4; ++i) C[i][i] = V[i];
  double out = 0.0;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      out += J[i] * C[i][j] * J[j];
    }
  }
  return out;
}

/* 解析 J = [1/f, -(1-alpha)/f, -alpha/f, -y/f]（原样继承 ALG-P1-001 §2.2）。 */
inline void analytic_jacobian(double alpha, double f, double y, double J[4]) {
  J[0] = 1.0 / f;
  J[1] = -(1.0 - alpha) / f;
  J[2] = -alpha / f;
  J[3] = -y / f;
}

/* 独立复算逐像素总方差（含 dark photon 项），使用字面公式。 */
inline double full_pixel_variance(double alpha, double f, double y,
                                  double read_noise_e, double gain,
                                  double q_adu, double r, double d,
                                  double v_bias_master, double v_dark_master,
                                  double v_flat_master) {
  const double v_rn = (read_noise_e / gain) * (read_noise_e / gain);
  const double v_ph = (r > 0.0 ? r : 0.0) / gain;
  const double v_q = q_adu * q_adu / 12.0;
  const double v_dp = (d > 0.0 ? d : 0.0) / gain;
  const double v_r = v_rn + v_ph + v_q;
  const double v_d = v_dark_master + v_dp;
  double J[4];
  analytic_jacobian(alpha, f, y, J);
  const double V[4] = {v_r, v_bias_master, v_d, v_flat_master};
  return dense_jcj(J, V);
}

/* 低秩共享项：显式构造 C_shared = L L^T 再做 c^T C c（与被测按列求范数不同）。 */
inline double low_rank_dense(const std::vector<double>& c,
                             const std::vector<double>& L, int n, int rank) {
  std::vector<double> C(static_cast<std::size_t>(n) * n, 0.0);
  for (int p = 0; p < n; ++p) {
    for (int q = 0; q < n; ++q) {
      double acc = 0.0;
      for (int r = 0; r < rank; ++r) {
        acc += L[static_cast<std::size_t>(p) * rank + r] *
               L[static_cast<std::size_t>(q) * rank + r];
      }
      C[static_cast<std::size_t>(p) * n + q] = acc;
    }
  }
  double out = 0.0;
  for (int p = 0; p < n; ++p) {
    for (int q = 0; q < n; ++q) {
      out += c[static_cast<std::size_t>(p)] * c[static_cast<std::size_t>(q)] *
             C[static_cast<std::size_t>(p) * n + q];
    }
  }
  return out;
}

/* 确定性 LCG（Numerical Recipes 常数）——固定种子 MC 可复跑。 */
class Lcg {
 public:
  explicit Lcg(std::uint64_t seed) : s_(seed) {}
  double uniform01() {
    s_ = s_ * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<double>((s_ >> 11) & ((1ULL << 53) - 1)) /
           static_cast<double>(1ULL << 53);
  }
  /* Box–Muller 标准正态。 */
  double normal() {
    const double u1 = uniform01() + 1e-300;
    const double u2 = uniform01();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * 3.14159265358979323846 * u2);
  }

 private:
  std::uint64_t s_;
};

/* 共享 covariance MC：x_k = c_k * (sqrt(v_ind) z_k + sqrt(v_shared) g)，
 * 估计 Var(Σ x_k) 与解析 joint 对比。返回 MC 估计。 */
inline double shared_variance_mc(const std::vector<double>& c, double v_ind,
                                 double v_shared, int trials, std::uint64_t seed) {
  Lcg rng(seed);
  const int n = static_cast<int>(c.size());
  double sum = 0.0;
  double sum2 = 0.0;
  for (int t = 0; t < trials; ++t) {
    const double g = rng.normal();
    double total = 0.0;
    for (int k = 0; k < n; ++k) {
      const double z = rng.normal();
      total += c[static_cast<std::size_t>(k)] *
               (std::sqrt(v_ind) * z + std::sqrt(v_shared) * g);
    }
    sum += total;
    sum2 += total * total;
  }
  const double mean = sum / trials;
  return sum2 / trials - mean * mean;
}

}  // namespace oracle
}  // namespace v6
}  // namespace calibration
}  // namespace astrocs

#endif
