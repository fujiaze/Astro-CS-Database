// P1-PHOT-TEST · StarField → fake gaia_client 桥 (units/properties/negative 共用)
//
// 将 fixture 星表装配为 stub::FakeClientConfig: ra/dec/magG 取自 StarField;
// 光谱/量化参数按用例选择 (常数谱或 F4 随机谱)。装配过程不做任何被测域
// 算法 (F_syn 期望值由 oracle 侧独立积分生成, 见 p1phot_oracle.hpp)。
#ifndef P1PHOT_FIELD_STUB_HPP
#define P1PHOT_FIELD_STUB_HPP

#include <cstdint>
#include <vector>

#include "p1phot_fixtures.hpp"
#include "p1phot_gaia_stub.hpp"

namespace p1phot {
namespace bridge {

// StarField + 每星光谱 → FakeClientConfig
// spectra: 每星一条 (size==n_gaia) 或空 (常数谱 fallback=spectrum_byte)
inline stub::FakeClientConfig make_config(const fix::StarField& f,
                                          const std::vector<std::vector<std::uint8_t>>& spectra,
                                          std::uint8_t spectrum_byte,
                                          float flux_min, float flux_mul,
                                          int wl_start = 336, int wl_step = 2,
                                          int wl_count = 343) {
    stub::FakeClientConfig cfg;
    cfg.wl_start = wl_start;
    cfg.wl_step = wl_step;
    cfg.wl_count = wl_count;
    const std::size_t n = f.gaia_ra.size();
    cfg.stars.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        stub::FakeStar& s = cfg.stars[i];
        s.ra = f.gaia_ra[i];
        s.dec = f.gaia_dec[i];
        s.magG = f.gaia_mag[i];
        s.flux_min = flux_min;
        s.flux_mul = flux_mul;
        if (!spectra.empty() && i < spectra.size() && spectra[i].size() == (std::size_t)wl_count)
            s.spectrum = spectra[i];
        else
            s.spectrum.assign((std::size_t)wl_count, spectrum_byte);
    }
    return cfg;
}

}  // namespace bridge
}  // namespace p1phot

#endif  // P1PHOT_FIELD_STUB_HPP
