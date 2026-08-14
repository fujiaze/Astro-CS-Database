// lib/phase2/src/integrate.cpp — Phase2 W8 加权叠加（V17 冻结语义）
#include "astro/phase2/integrate.h"

#include <cmath>
#include <cstring>

extern "C" {

int p2_validate_candidate_weights(const double* weights, std::uint32_t count) {
    if (weights == nullptr) return 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (!std::isfinite(weights[i]) || weights[i] <= 0.0) return 1;
    }
    return 0;
}

int p2_integrate_pixel(const P2PixelStack* in, P2PixelResult* out) {
    if (in == nullptr || out == nullptr) return 1;
    std::memset(out, 0, sizeof(*out));
    out->n_candidates = in->count;
    if (in->count == 0 || in->values == nullptr) {
        out->status = P2_INTEGRATE_NO_CANDIDATES;
        return 0;
    }

    // integration eligibility（V17）：finite(value)/finite(support>0)/
    // finite(weight>0)/accepted；任一非 finite 输入 → INVALID_INPUT
    bool invalid_input = false;
    double wsum = 0.0, vs = 0.0, sup_max = 0.0;
    std::uint32_t n_finite = 0, n_positive_weight = 0, n_accepted = 0;
    for (std::uint32_t i = 0; i < in->count; ++i) {
        const bool acc = (in->accepted == nullptr) || in->accepted[i];
        if (acc) ++n_accepted;
        if (!acc) continue;
        if (!std::isfinite(in->values[i])) { invalid_input = true; continue; }
        if (in->support != nullptr &&
            (!std::isfinite(in->support[i]) || in->support[i] <= 0.0)) {
            invalid_input = true;
            continue;
        }
        ++n_finite;
        double w = 1.0;
        if (in->weights != nullptr) {
            w = in->weights[i];
            if (!std::isfinite(w)) { invalid_input = true; continue; }
            if (w < 0.0) { invalid_input = true; continue; }  // 负权重=契约违规
            if (w == 0.0) continue;  // 零权重=合法但不贡献（ZERO_VALID_WEIGHT）
        }
        ++n_positive_weight;
        vs += w * in->values[i];
        wsum += w;
        if (in->support != nullptr)
            sup_max = std::max(sup_max, in->support[i]);  // canonical reducer
        ++out->n_used;
    }
    out->n_finite = n_finite;
    out->n_positive_weight = n_positive_weight;
    out->n_accepted = n_accepted;
    if (invalid_input) {
        out->status = P2_INTEGRATE_INVALID_INPUT;
        return 0;
    }
    if (n_positive_weight == 0) {
        out->status = (n_accepted == 0) ? P2_INTEGRATE_ALL_REJECTED
                                        : P2_INTEGRATE_ZERO_VALID_WEIGHT;
        return 0;
    }
    out->signal = vs / wsum;
    out->support = (in->support != nullptr) ? sup_max : 1.0;
    out->status = P2_INTEGRATE_OK;
    return 0;
}

} // extern "C"
