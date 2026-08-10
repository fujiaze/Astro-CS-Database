// lib/phase2/src/integrate.cpp — Phase2 W8 加权叠加
#include "astro/phase2/integrate.h"

#include <cmath>
#include <cstring>

extern "C" {

int p2_integrate_pixel(const P2PixelStack* in, P2PixelResult* out) {
    if (in == nullptr || out == nullptr) return 1;
    std::memset(out, 0, sizeof(*out));
    double wsum = 0.0, vs = 0.0, sup = 0.0;
    std::uint32_t n_used = 0;
    for (std::uint32_t i = 0; i < in->count; ++i) {
        if (in->accepted != nullptr && !in->accepted[i]) continue;
        if (!std::isfinite(in->values[i])) continue;
        double w = 1.0;
        if (in->weight_mode == 0) {
            // snr2 归一化：调用方传入 weights 时视为 SNR^2/(1+SNR^2)
            if (in->weights != nullptr) {
                w = in->weights[i];
            }
        } else if (in->weights != nullptr) {
            w = in->weights[i];
        }
        if (w <= 0.0) continue;
        vs += w * in->values[i];
        wsum += w;
        if (in->support != nullptr) sup += w * in->support[i];
        ++n_used;
    }
    if (wsum <= 0.0) {
        out->status = (n_used == 0) ? 2 : 1;  // zero-weight / all-rejected
        return 0;
    }
    out->signal = vs / wsum;
    out->support = (in->support != nullptr) ? sup / wsum : 1.0;
    out->n_used = n_used;
    out->status = 0;
    return 0;
}

} // extern "C"