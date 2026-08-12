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
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kTileShift = 9;  // 512×512 tile: leaf_order = tile_order + 9
constexpr uint64_t kTileMask = (1ULL << 18) - 1;  // 512² - 1
constexpr int kTileDim = 512;

// ============================================================================
// V9: 浏览器侧最小 FITS 图像读取（仅用于按 order 读 HiPS hierarchy tile）。
// 不链接 CFITSIO、不改 AIO 语义；FITS 数据为大端，读取后字节序转换。
// ============================================================================
static void swap_bytes32(float* p) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    v = ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
        ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
    std::memcpy(p, &v, 4);
}

static bool read_fits_image(const std::string& path, int expect_n,
                            std::vector<float>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    int bitpix = 0;
    long naxis1 = 0, naxis2 = 0;
    bool end_found = false;
    char block[2880];
    while (f.read(block, sizeof(block))) {
        for (int off = 0; off < 2880 && !end_found; off += 80) {
            std::string card(block + off, 80);
            const std::string key = card.substr(0, 8);
            const std::string val = card.substr(10);
            if (key == "BITPIX  ") bitpix = std::atoi(val.c_str());
            else if (key == "NAXIS1  ") naxis1 = std::atol(val.c_str());
            else if (key == "NAXIS2  ") naxis2 = std::atol(val.c_str());
            else if (card.rfind("END", 0) == 0) { end_found = true; }
        }
        if (end_found) break;
    }
    if (!end_found || naxis1 != expect_n || naxis2 != expect_n)
        return false;
    const std::size_t n = (std::size_t)expect_n * (std::size_t)expect_n;
    out.resize(n);
    if (bitpix == -32) {
        f.read((char*)out.data(), (std::streamsize)(n * 4));
        for (std::size_t i = 0; i < n; ++i) swap_bytes32(&out[i]);
        return true;
    }
    if (bitpix == -64) {
        std::vector<double> tmp(n);
        f.read((char*)tmp.data(), (std::streamsize)(n * 8));
        for (std::size_t i = 0; i < n; ++i) {
            std::uint64_t v;
            std::memcpy(&v, &tmp[i], 8);
            v = ((v & 0x00000000000000FFULL) << 56) |
                ((v & 0x000000000000FF00ULL) << 40) |
                ((v & 0x0000000000FF0000ULL) << 24) |
                ((v & 0x00000000FF000000ULL) << 8) |
                ((v & 0x000000FF00000000ULL) >> 8) |
                ((v & 0x0000FF0000000000ULL) >> 24) |
                ((v & 0x00FF000000000000ULL) >> 40) |
                ((v & 0xFF00000000000000ULL) >> 56);
            std::memcpy(&tmp[i], &v, 8);
            out[i] = (float)tmp[i];
        }
        return true;
    }
    return false;
}

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
    order_tiles_.clear();
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

int HipsBrowserBackend::read_tile_at_order(int order, uint64_t tile_ipix,
                                           std::vector<float>& sig,
                                           std::vector<float>& sup) const {
    if (root_.empty() || order < 0 || order > order_) return -1;
    const std::string sig_path = root_ + "/signal/Norder" +
                                 std::to_string(order) + "/Dir" +
                                 std::to_string(tile_ipix / 10000) + "/Npix" +
                                 std::to_string(tile_ipix % 10000) + ".fits";
    const std::string sup_path = root_ + "/support/Norder" +
                                 std::to_string(order) + "/Dir" +
                                 std::to_string(tile_ipix / 10000) + "/Npix" +
                                 std::to_string(tile_ipix % 10000) + ".fits";
    std::vector<float> s, u;
    if (!read_fits_image(sig_path, kTileDim, s)) return -2;
    if (!read_fits_image(sup_path, kTileDim, u)) return -3;
    sig.swap(s);
    sup.swap(u);
    return 0;
}

void HipsBrowserBackend::load_order_tiles(int order) const {
    if (order_tiles_.count(order)) return;
    std::vector<uint64_t> list;
    const std::string dir = root_ + "/signal/Norder" + std::to_string(order);
    std::error_code ec;
    if (std::filesystem::exists(dir, ec)) {
        for (const auto& de :
             std::filesystem::recursive_directory_iterator(dir, ec)) {
            if (!de.is_regular_file(ec)) continue;
            const std::string fn = de.path().filename().string();
            if (fn.rfind("Npix", 0) != 0 || fn.size() < 5 ||
                fn.compare(fn.size() - 5, 5, ".fits") != 0)
                continue;
            const std::string dirn =
                de.path().parent_path().filename().string();
            if (dirn.rfind("Dir", 0) != 0) continue;
            const long long dirnum = std::atoll(dirn.c_str() + 3);
            const long long npix = std::atoll(fn.c_str() + 4);
            if (dirnum >= 0 && npix >= 0)
                list.push_back((uint64_t)dirnum * 10000ull + (uint64_t)npix);
        }
    }
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    order_tiles_[order] = std::move(list);
}

const std::vector<uint64_t>&
HipsBrowserBackend::tiles_at_order(int order) const {
    static const std::vector<uint64_t> kEmpty;
    if (order < 0 || order > order_) return kEmpty;
    load_order_tiles(order);
    return order_tiles_[order];
}

bool HipsBrowserBackend::has_tile_at_order(int order,
                                           uint64_t tile_ipix) const {
    if (order < 0 || order > order_) return false;
    const auto& v = tiles_at_order(order);
    return std::binary_search(v.begin(), v.end(), tile_ipix);
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
