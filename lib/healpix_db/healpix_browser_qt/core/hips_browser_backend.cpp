// ============================================================================
// hips_browser_backend.cpp - HiPS 产品集浏览器后端实现
//
// 数据源: AIO HiPS Reader (astro_image_io.dll), 唯一读取入口。
// 查询映射:
//   leaf_ipix = ang2pix_nest(2^leaf_order, ra, dec)
//   tile_ipix = leaf_ipix >> 18, z = leaf_ipix & (512²-1)
//   V5 (HIPS-IMG-001): 共享 HEALPix core 标准映射
//     fits_index = (511-x)*512 + y, (x,y) = nested_local_to_xy(z, 9)
//   (不再使用 z % 512 / z / 512 私有线性约定)
// ============================================================================

#include "hips_browser_backend.h"

#include "aio_hips_reader.h"
#include "healpix/healpix_core.h"
#include "logger.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kTileShift = 9;  // 512×512 tile: leaf_order = tile_order + 9
constexpr uint64_t kTileMask = (1ULL << 18) - 1;  // 512² - 1
constexpr int kTileDim = 512;

bool props_has(const std::string& props, const std::string& key_value) {
    return props.find(key_value) != std::string::npos;
}

} // namespace

HipsBrowserBackend::HipsBrowserBackend() = default;

HipsBrowserBackend::~HipsBrowserBackend() {
    close();
}

int HipsBrowserBackend::open_product(const std::string& out_dir) {
    close();
    root_ = out_dir;
    if (root_.empty()) return -1;
    sig_ = aio_hips_open(root_.c_str(), AIO_HIPS_RD_SIGNAL);
    if (!sig_) {
        LOG_ERROR("hips_backend", "open signal 失败: %s", aio_hips_reader_last_error());
        return -2;
    }
    sup_ = aio_hips_open(root_.c_str(), AIO_HIPS_RD_SUPPORT);
    if (!sup_) {
        LOG_ERROR("hips_backend", "open support 失败: %s", aio_hips_reader_last_error());
        close();
        return -3;
    }
    snr_ = aio_hips_open(root_.c_str(), AIO_HIPS_RD_SNR);
    if (!snr_) {
        LOG_WARN("hips_backend", "open snr 失败 (catalogue 不可用): %s",
                 aio_hips_reader_last_error());
        // Catalogue 可选: 不视为硬失败
    }
    char props_buf[8192];
    if (aio_hips_get_properties(sig_, props_buf, (int)sizeof(props_buf)) != 0) {
        close();
        return -4;
    }
    const std::string props(props_buf);
    // 解析 hips_order
    order_ = 0;
    size_t p = props.find("hips_order=");
    if (p != std::string::npos) {
        order_ = std::atoi(props.c_str() + p + 11);
    }
    leaf_order_ = order_ + kTileShift;
    width_ = kTileDim;
    fp64_ = props_has(props, "astrocs_signal_dtype=float64");
    LOG_INFO("hips_backend", "open_product: order=%d leaf_order=%d fp64=%d tiles=%llu",
             order_, leaf_order_, fp64_ ? 1 : 0,
             (unsigned long long)get_n_tiles());
    return 0;
}

void HipsBrowserBackend::close() {
    if (snr_) { aio_hips_close(snr_); snr_ = nullptr; }
    if (sup_) { aio_hips_close(sup_); sup_ = nullptr; }
    if (sig_) { aio_hips_close(sig_); sig_ = nullptr; }
    root_.clear();
    order_ = 0;
    leaf_order_ = 0;
    width_ = kTileDim;
    fp64_ = false;
}

uint64_t HipsBrowserBackend::get_n_tiles() const {
    return sig_ ? (uint64_t)aio_hips_tile_count(sig_) : 0;
}

bool HipsBrowserBackend::contains(double ra, double dec) const {
    if (!sig_) return false;
    const uint32_t nside = uint32_t(1) << (uint32_t)leaf_order_;
    const uint64_t leaf_ipix = astrocs::healpix::ang2pix_nest(nside, ra, dec);
    const uint64_t tile_ipix = leaf_ipix >> 18;
    const int n = aio_hips_tile_count(sig_);
    for (int i = 0; i < n; ++i) {
        uint64_t ip = 0;
        if (aio_hips_tile_ipix(sig_, i, &ip) == 0 && ip == tile_ipix) return true;
    }
    return false;
}

int HipsBrowserBackend::query_pixel(double ra, double dec,
                                    double& signal, double& support) const {
    signal = std::numeric_limits<double>::quiet_NaN();
    support = 0.0;
    if (!sig_ || !sup_) return -3;
    const uint32_t nside = uint32_t(1) << (uint32_t)leaf_order_;
    const uint64_t leaf_ipix = astrocs::healpix::ang2pix_nest(nside, ra, dec);
    const uint64_t tile_ipix = leaf_ipix >> 18;
    const uint64_t z = leaf_ipix & kTileMask;
    const uint64_t idx = astrocs::healpix::nested_local_to_fits_index(z, 9u, 512u);

    std::vector<double> tile((size_t)kTileDim * kTileDim);
    int rc = read_tile(tile_ipix, tile);
    if (rc != 0) return -2;  // outside MOC
    signal = tile[idx];
    // support 产品
    std::vector<double> sup((size_t)kTileDim * kTileDim);
    int rc2 = 0;
    if (fp64_) {
        rc2 = aio_hips_read_tile_f64(sup_, tile_ipix, sup.data());
    } else {
        std::vector<float> tmp((size_t)kTileDim * kTileDim);
        rc2 = aio_hips_read_tile_f32(sup_, tile_ipix, tmp.data());
        for (size_t i = 0; i < tmp.size(); ++i) sup[i] = (double)tmp[i];
    }
    if (rc2 != 0) return -3;
    support = sup[idx];
    return 0;
}

int HipsBrowserBackend::read_tile(uint64_t tile_ipix, std::vector<double>& out) const {
    if (!sig_) return -1;
    out.assign((size_t)kTileDim * kTileDim, std::numeric_limits<double>::quiet_NaN());
    int rc = 0;
    if (fp64_) {
        rc = aio_hips_read_tile_f64(sig_, tile_ipix, out.data());
    } else {
        std::vector<float> tmp((size_t)kTileDim * kTileDim);
        rc = aio_hips_read_tile_f32(sig_, tile_ipix, tmp.data());
        if (rc == 0) {
            for (size_t i = 0; i < tmp.size(); ++i) out[i] = (double)tmp[i];
        }
    }
    return rc;
}

int HipsBrowserBackend::read_snr_catalog(std::vector<double>& ra, std::vector<double>& dec,
                                         std::vector<double>& snr, std::vector<int64_t>& star_id,
                                         std::vector<uint32_t>& quality_flags,
                                         std::vector<uint32_t>& photometric_status) const {
    ra.clear(); dec.clear(); snr.clear(); star_id.clear();
    quality_flags.clear(); photometric_status.clear();
    if (!snr_) return -1;
    const int max = 100000;
    ra.resize((size_t)max); dec.resize((size_t)max); snr.resize((size_t)max);
    star_id.resize((size_t)max);
    quality_flags.resize((size_t)max); photometric_status.resize((size_t)max);
    int n = aio_hips_read_snr_catalog(snr_, ra.data(), dec.data(), snr.data(),
                                      star_id.data(), quality_flags.data(),
                                      photometric_status.data(), max);
    if (n < 0) { ra.clear(); dec.clear(); snr.clear(); star_id.clear();
                 quality_flags.clear(); photometric_status.clear(); return n; }
    ra.resize((size_t)n); dec.resize((size_t)n); snr.resize((size_t)n);
    star_id.resize((size_t)n);
    quality_flags.resize((size_t)n); photometric_status.resize((size_t)n);
    return n;
}
