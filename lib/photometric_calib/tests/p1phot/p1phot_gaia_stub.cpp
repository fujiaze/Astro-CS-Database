// P1-PHOT-TEST · gaia_client 测试桩实现 (符号定义面)
//
// 定义 pc_api.o 引用的外部域符号 (nm 实测 undefined 面:
// gaia_client_cone_search_with_spectrum / gaia_client_get_spectrum_params;
// 另提供 create/destroy 供测试构造 handle)。链接本桩后被测域 pc_api.cpp
// 生产源按原样编译链接, 零修改。
#include "p1phot_gaia_stub.hpp"

#include <cstdlib>
#include <cstring>

#include "gaia_client.h"  // GaiaSpectrumStar 完整定义 + 函数签名 (他域头, 只读)

// fake client 完整定义 (gaia_client.h 中为不完整 typedef; 同一全局 tag 的
// 完整化定义合法, pc_api 仅作 opaque 指针传递)
struct GaiaClient {
    p1phot::stub::FakeClientConfig cfg;
};

namespace p1phot {
namespace stub {

GaiaClient* create(const FakeClientConfig& cfg) {
    auto* c = new GaiaClient();
    c->cfg = cfg;
    return c;
}

void destroy(GaiaClient* c) { delete c; }

void set_fail_mode(GaiaClient* c, int fail_mode) {
    if (c) c->cfg.fail_mode = fail_mode;
}

}  // namespace stub
}  // namespace p1phot

// ─── 外部域 C ABI 替身 (签名与 gaia_client.h 一致) ──────────────────────

extern "C" {

GaiaClient* gaia_client_create(const char* /*data_dir*/) {
    return new GaiaClient();
}

void gaia_client_destroy(GaiaClient* client) { delete client; }

int gaia_client_cone_search_with_spectrum(
    GaiaClient* client,
    double /*ra*/, double /*dec*/, double /*radius_deg*/,
    double /*mag_low*/, double /*mag_high*/,
    GaiaSpectrumStar** out_stars,
    std::uint8_t** out_spectra,
    int* out_count) {
    if (!client || !out_stars || !out_spectra || !out_count) return -1;
    const p1phot::stub::FakeClientConfig& cfg = client->cfg;
    if (cfg.fail_mode != 0) return -2;  // 锥形搜索失败注入 → 被测 rc=-3 路径
    const int n = (int)cfg.stars.size();
    *out_count = n;
    if (n <= 0) return 0;  // 0 星: 被测退化路径 (scale=1.0)
    auto* stars = (GaiaSpectrumStar*)std::malloc(sizeof(GaiaSpectrumStar) * (std::size_t)n);
    auto* buf = (std::uint8_t*)std::malloc((std::size_t)n * (std::size_t)cfg.wl_count);
    if (!stars || !buf) {
        std::free(stars);
        std::free(buf);
        return -3;
    }
    for (int i = 0; i < n; ++i) {
        const p1phot::stub::FakeStar& s = cfg.stars[(std::size_t)i];
        stars[i].ra = s.ra;
        stars[i].dec = s.dec;
        stars[i].magG = s.magG;
        // XPSD 量化参数 (compute_f_syn_cached_xpsd 解码 F(λ) 的输入;
        // 漏写会留堆残留 → F_syn 错乱, U10/P2/O1 类全链用例必炸)
        stars[i].flux_min = s.flux_min;
        stars[i].flux_mul = s.flux_mul;
        const std::size_t n_copy = std::min(s.spectrum.size(),
                                            (std::size_t)cfg.wl_count);
        std::memcpy(buf + (std::size_t)i * cfg.wl_count, s.spectrum.data(), n_copy);
        for (std::size_t j = n_copy; j < (std::size_t)cfg.wl_count; ++j)
            buf[(std::size_t)i * cfg.wl_count + j] = 0;
    }
    *out_stars = stars;
    *out_spectra = buf;
    return 0;
}

int gaia_client_get_spectrum_params(
    GaiaClient* client,
    int* out_start_nm, int* out_step_nm, int* out_count) {
    if (!client || !out_start_nm || !out_step_nm || !out_count) return -1;
    *out_start_nm = client->cfg.wl_start;
    *out_step_nm = client->cfg.wl_step;
    *out_count = client->cfg.wl_count;
    return 0;
}

}  // extern "C"
