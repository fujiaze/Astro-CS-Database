// RESCUE-FD-05 permanent regression gate (FOLLOW-UP-FD05-02).
//
// Contract asserted here is platform-neutral and requires no external data:
//   (1) the C ABI of the production entry returns 0;
//   (2) an empty Gaia catalogue can never be claimed as a successful solve
//       (deterministic fail-closed, not a silent success);
//   (3) ALG-WCS-001 §11.4 F4 -> a failing solve carries a NON-EMPTY error_msg;
//   (4) non-Windows (Linux amd64, a formal entry per Constitution §3.1) the
//       direct static-linked C-API binding MUST be active: the production entry
//       actually runs star detection and fires the detection callback.  The
//       pre-RESCUE-FD-05 platform stub returned before detection -> 0 callbacks
//       -> this assertion goes red, which is the regression this gate guards.
//
// The Windows branch is intentionally NOT asserted for callback activity: it
// resolves symbols via LoadLibraryA at runtime, which is an environment
// property, not a source contract.  Windows still asserts (1)(2)(3).
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "ipv_api.h"
#include "star_detector.h"

extern "C" {
typedef struct GaiaClient GaiaClient;
GaiaClient *gaia_client_create(const char *data_dir);
void gaia_client_destroy(GaiaClient *client);
}

namespace {
int g_cb_calls = 0;
int g_cb_max = 0;
void det_sink(const double *dets, int n, void *) {
    (void)dets;
    ++g_cb_calls;
    if (n > g_cb_max) g_cb_max = n;
}
}  // namespace

int main() {
    const int W = 512, H = 512;
    std::vector<double> img(static_cast<size_t>(W) * H, 1000.0);
    // 12 well-separated Gaussian PSFs (sigma 2, amplitude 5000 over bg 1000)
    const double xs[12] = {60, 140, 220, 300, 380, 460, 90, 170, 250, 330, 410, 480};
    const double ys[12] = {64, 120, 200, 280, 360, 440, 448, 392, 312, 232, 152, 72};
    for (int k = 0; k < 12; ++k)
        for (int dy = -8; dy <= 8; ++dy)
            for (int dx = -8; dx <= 8; ++dx) {
                int x = static_cast<int>(xs[k]) + dx, y = static_cast<int>(ys[k]) + dy;
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                double r2 = static_cast<double>(dx) * dx + static_cast<double>(dy) * dy;
                img[static_cast<size_t>(y) * W + x] += 5000.0 * std::exp(-r2 / (2.0 * 2.0 * 2.0));
            }

    SDetParams sp;
    std::memset(&sp, 0, sizeof(sp));
    sp.structureLayers = 5;
    sp.hotPixelFilterRadius = 2;
    sp.iterativeClipSigma = 5.0f;
    sp.iterativeMaxRounds = 3;
    sp.medianFilterDetail = 2;
    sp.maxStars = 2000;
    sp.fitRadius = 0;
    sp.fwhmClipSigma = 3.0f;
    sp.maxAxisRatio = 2.0f;
    StarDetectorHandle sdet = sdet_create(&sp);
    if (!sdet) {
        std::printf("FAIL: sdet_create\n");
        return 1;
    }

    std::error_code ec;
    std::filesystem::path dir =
        std::filesystem::temp_directory_path(ec) /
        ("ipv_bind_" + std::to_string(static_cast<long long>(
                          std::chrono::steady_clock::now().time_since_epoch().count())));
    std::filesystem::create_directories(dir, ec);
    std::string dir_s = dir.string();
    GaiaClient *gaia = gaia_client_create(dir_s.c_str());
    if (!gaia) {
        std::printf("FAIL: gaia_client_create(empty dir)\n");
        sdet_destroy(sdet);
        return 1;
    }

    void *solver = ipv_solve_create();
    if (!solver) {
        std::printf("FAIL: ipv_solve_create\n");
        gaia_client_destroy(gaia);
        sdet_destroy(sdet);
        return 1;
    }
    IpvParams ip;
    ipv_get_default_params(&ip);
    std::memset(ip.log_dir, 0, sizeof(ip.log_dir));
    ipv_set_gaia_handle(solver, reinterpret_cast<intptr_t>(gaia));
    ipv_set_detector_handle(solver, reinterpret_cast<intptr_t>(sdet));

    IpvWcsResult r;
    std::memset(&r, 0, sizeof(r));
    int rc = ipv_solve_from_memory_with_callback_d(solver, img.data(), W, H, 10.0, 20.0,
                                                   1000.0, 3.76, &ip, det_sink, nullptr, &r);
    int n_cb = g_cb_calls, n_max = g_cb_max;
    ipv_solve_destroy(solver);
    gaia_client_destroy(gaia);
    sdet_destroy(sdet);
    std::filesystem::remove_all(dir, ec);

    int fails = 0;
    if (rc != 0) {
        std::printf("FAIL: C ABI returned rc=%d (must be 0)\n", rc);
        ++fails;
    }
    if (r.success != 0) {
        std::printf("FAIL: success=1 with an empty Gaia catalogue (silent false positive)\n");
        ++fails;
    }
    if (r.error_msg[0] == '\0') {
        std::printf("FAIL: error_msg is empty (ALG-WCS-001 F4)\n");
        ++fails;
    }
#ifndef _WIN32
    if (n_cb <= 0 || n_max <= 0) {
        std::printf("FAIL: non-Windows direct binding inactive: detection callback never fired "
                    "(pre-port platform stub behaviour)\n");
        ++fails;
    }
#endif

    std::printf("callback_calls=%d max_dets=%d rc=%d success=%d error_msg=%s\n", n_cb, n_max, rc,
                static_cast<int>(r.success), r.error_msg);
    std::printf("%s\n", fails ? "IPV_PLATFORM_BINDING FAIL" : "IPV_PLATFORM_BINDING PASS");
    return fails ? 1 : 0;
}
