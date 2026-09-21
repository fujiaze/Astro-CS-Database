#ifndef V6_P1_DRZ_ORACLE_HPP
#define V6_P1_DRZ_ORACLE_HPP

// ============================================================================
// IMPL-P1-DRZ-001 独立 Oracle（共址测试）
//
// 真值来源：解析恒等式（冻结公式的独立转写，long double 直算），
// 不调用被测实现生成期望（ORACLE_AND_ZERO_CASE_POLICY §1）。
//   S_p        = Sum_j B_j a_jp / Sum_j a_jp ,  B_j = x_j / A_pixel_j
//   variance_p = Sum_j v_j (w_jp/D_p)^2 ,       w_jp = a_jp / A_pixel_j
//   Cov(S_p,S_q)= Sum_j (w_jp/D_p)(w_jq/D_q) v_j
//   Var(Sum a_p S_p) = Sum_{p,q} a_p a_q Cov
//   Var(S_parent) = Sum_{p,q} (D_p D_q/(Sum D)^2) Cov
//   Phi_out    = Sum_p S_p D_p
// ============================================================================

#include <cmath>
#include <cstdint>
#include <vector>

namespace v6_p1_drz_oracle {

struct Item {
    std::uint32_t src = 0;
    std::uint32_t dst = 0;
    double a = 0.0;
};

class Model {
public:
    static Model build(std::uint32_t n_src, std::uint32_t n_dst,
                       const std::vector<double>& A_pixel, double pixfrac,
                       const std::vector<Item>& items) {
        Model m;
        m.n_src_ = n_src;
        m.n_dst_ = n_dst;
        m.pixfrac_ = pixfrac;
        m.A_pixel_ = A_pixel;
        m.rows_.assign(n_dst, {});
        m.D_.assign(n_dst, 0.0L);
        for (const Item& it : items) {
            m.D_[it.dst] += (long double)it.a;
            m.rows_[it.dst].push_back(it);
        }
        return m;
    }

    long double D(std::uint32_t p) const { return D_[p]; }
    const std::vector<Item>& row(std::uint32_t p) const { return rows_[p]; }

    long double signal(std::uint32_t p, const double* x) const {
        long double num = 0.0L;
        long double den = 0.0L;
        for (const Item& it : rows_[p]) {
            num += ((long double)x[it.src] / (long double)A_pixel_[it.src]) *
                   (long double)it.a;
            den += (long double)it.a;
        }
        return (den > 0.0L) ? num / den : 0.0L;
    }

    long double variance(std::uint32_t p, const double* v) const {
        const long double D = D_[p];
        long double acc = 0.0L;
        for (const Item& it : rows_[p]) {
            const long double w = (long double)it.a / (long double)A_pixel_[it.src];
            const long double c = w / D;
            acc += (long double)v[it.src] * c * c;
        }
        return acc;
    }

    long double covariance(std::uint32_t p, std::uint32_t q, const double* v) const {
        const long double Dp = D_[p], Dq = D_[q];
        long double acc = 0.0L;
        for (const Item& a : rows_[p]) {
            for (const Item& b : rows_[q]) {
                if (a.src != b.src) continue;
                const long double wa = (long double)a.a / (long double)A_pixel_[a.src];
                const long double wb = (long double)b.a / (long double)A_pixel_[b.src];
                acc += (long double)v[a.src] * (wa / Dp) * (wb / Dq);
            }
        }
        return acc;
    }

    long double aperture(const double* weights, const double* v) const {
        long double acc = 0.0L;
        for (std::uint32_t p = 0; p < n_dst_; ++p) {
            for (std::uint32_t q = 0; q < n_dst_; ++q) {
                acc += (long double)weights[p] * (long double)weights[q] *
                       covariance(p, q, v);
            }
        }
        return acc;
    }

    long double aperture_diag(const double* weights, const double* v) const {
        long double acc = 0.0L;
        for (std::uint32_t p = 0; p < n_dst_; ++p) {
            acc += (long double)weights[p] * (long double)weights[p] *
                   variance(p, v);
        }
        return acc;
    }

    long double parent_exact(const double* v) const {
        long double sumD = 0.0L;
        for (long double d : D_) sumD += d;
        if (sumD <= 0.0L) return 0.0L;
        long double acc = 0.0L;
        for (std::uint32_t p = 0; p < n_dst_; ++p) {
            for (std::uint32_t q = 0; q < n_dst_; ++q) {
                acc += (D_[p] / sumD) * (D_[q] / sumD) * covariance(p, q, v);
            }
        }
        return acc;
    }

    long double parent_diag(const double* v) const {
        long double sumD = 0.0L;
        for (long double d : D_) sumD += d;
        if (sumD <= 0.0L) return 0.0L;
        long double acc = 0.0L;
        for (std::uint32_t p = 0; p < n_dst_; ++p) {
            acc += (D_[p] / sumD) * (D_[p] / sumD) * variance(p, v);
        }
        return acc;
    }

    long double flux(const double* x) const {
        long double acc = 0.0L;
        for (std::uint32_t p = 0; p < n_dst_; ++p) {
            acc += signal(p, x) * D_[p];
        }
        return acc;
    }

    std::uint32_t n_dst() const { return n_dst_; }

private:
    std::uint32_t n_src_ = 0;
    std::uint32_t n_dst_ = 0;
    double pixfrac_ = 1.0;
    std::vector<double> A_pixel_;
    std::vector<long double> D_;
    std::vector<std::vector<Item>> rows_;
};

inline bool rel_close(long double a, long double b, long double tol) {
    const long double scale = std::max(std::fabs(a), std::fabs(b));
    if (scale == 0.0L) return true;
    return std::fabs(a - b) / scale <= tol;
}

} // namespace v6_p1_drz_oracle

#endif // V6_P1_DRZ_ORACLE_HPP
