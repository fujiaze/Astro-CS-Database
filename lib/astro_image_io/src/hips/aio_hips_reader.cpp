// ============================================================================
// aio_hips_reader.cpp - IVOA HiPS 读取器实现 (CFITSIO)
// ============================================================================

#include "aio_hips_reader.h"

#include <fitsio.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

thread_local std::string g_rd_error;

void set_err(const std::string& m) { g_rd_error = m; }

bool fits_ok(int status, const std::string& where) {
    if (status == 0) return true;
    char msg[FLEN_ERRMSG];
    fits_get_errstatus(status, msg);
    set_err(where + ": " + msg);
    return false;
}

std::string tile_path(const std::string& dir, int order, uint64_t ipix, const char* ext) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s/Norder%d/Dir%llu/Npix%llu%s",
                  dir.c_str(), order, (unsigned long long)(ipix / 10000),
                  (unsigned long long)(ipix % 10000), ext);
    return std::string(buf);
}

std::map<std::string, std::string> parse_properties(const std::string& path) {
    std::map<std::string, std::string> kv;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        // 去首尾空白
        while (!k.empty() && (k.back() == ' ' || k.back() == '\r')) k.pop_back();
        while (!k.empty() && (k.front() == ' ')) k.erase(k.begin());
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
        while (!v.empty() && (v.front() == ' ')) v.erase(v.begin());
        kv[k] = v;
    }
    return kv;
}

} // namespace

// 轻量 SNR 点 (reader 内部)
struct AioHipsSnrPoint2 {
    double ra = 0, dec = 0, snr = 0;
    int64_t star_id = 0;
};

struct AioHipsDataset {
    std::string dir;          // 子产品目录
    int product = 0;
    std::map<std::string, std::string> props;
    int hips_order = 0;
    int tile_width = 512;
    std::vector<uint64_t> tiles;      // 叶级 ipix (从 MOC 或目录扫描)
    std::vector<AioHipsSnrPoint2> snr; // 见下方类型
    int data_bitpix = -32;
};

template <typename T>
static int read_tile_t(AioHipsDataset* d, uint64_t ipix, T* out) {
    if (!d || !out) return -1;
    std::string p = tile_path(d->dir, d->hips_order, ipix, ".fits");
    int status = 0;
    fitsfile* fptr = nullptr;
    if (fits_open_file(&fptr, p.c_str(), READONLY, &status)) {
        fits_clear_errmsg();
        set_err("tile 不存在: " + p);
        return -2;
    }
    int bitpix = 0, naxis = 0;
    long naxes[2] = {0, 0};
    if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "tile param") ? -3 : -3;
    }
    if (naxis != 2 || naxes[0] != d->tile_width || naxes[1] != d->tile_width) {
        fits_close_file(fptr, &status);
        set_err("tile 尺寸非法");
        return -4;
    }
    long nelem = naxes[0] * naxes[1];
    long fpixel[2] = {1, 1};
    if (bitpix == -32) {
        std::vector<float> tmp((size_t)nelem);
        if (fits_read_pix(fptr, TFLOAT, fpixel, nelem, nullptr, tmp.data(), nullptr, &status)) {
            fits_close_file(fptr, &status);
            return fits_ok(status, "tile read") ? -5 : -5;
        }
        for (long i = 0; i < nelem; ++i) out[i] = (T)tmp[(size_t)i];
    } else if (bitpix == -64) {
        std::vector<double> tmp((size_t)nelem);
        if (fits_read_pix(fptr, TDOUBLE, fpixel, nelem, nullptr, tmp.data(), nullptr, &status)) {
            fits_close_file(fptr, &status);
            return fits_ok(status, "tile read") ? -5 : -5;
        }
        for (long i = 0; i < nelem; ++i) out[i] = (T)tmp[(size_t)i];
    } else {
        fits_close_file(fptr, &status);
        set_err("tile BITPIX 非 -32/-64");
        return -6;
    }
    fits_close_file(fptr, &status);
    return 0;
}

namespace {

// 从 MOC FITS 提取叶级 ipix (order == hips_order 的 UNIQ -> ipix)
bool load_tiles_from_moc(AioHipsDataset* d) {
    std::string moc = d->dir + "/Moc.fits";
    int status = 0;
    fitsfile* fptr = nullptr;
    if (fits_open_file(&fptr, moc.c_str(), READONLY, &status)) {
        fits_clear_errmsg();
        return true;  // 无 MOC 不算错误 (空产品)
    }
    // MOC 数据在扩展 HDU (PRIMARY 为空图头)
    if (fits_movabs_hdu(fptr, 2, nullptr, &status)) {
        fits_close_file(fptr, &status);
        fits_clear_errmsg();
        return true;
    }
    long nrows = 0;
    if (fits_get_num_rows(fptr, &nrows, &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "moc rows");
    }
    std::vector<long long> uniq((size_t)nrows);
    if (nrows > 0 && fits_read_col(fptr, TLONGLONG, 1, 1, 1, nrows, nullptr,
                                   uniq.data(), nullptr, &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "moc read");
    }
    fits_close_file(fptr, &status);
    const uint64_t order_uniq_base = 4ULL * (1ULL << (2ULL * (uint64_t)d->hips_order));
    for (long long u : uniq) {
        if ((uint64_t)u >= order_uniq_base) {
            uint64_t ipix = (uint64_t)u - order_uniq_base;
            if (ipix < 12ULL * (1ULL << (2ULL * (uint64_t)d->hips_order)))
                d->tiles.push_back(ipix);
        }
    }
    std::sort(d->tiles.begin(), d->tiles.end());
    d->tiles.erase(std::unique(d->tiles.begin(), d->tiles.end()), d->tiles.end());
    return true;
}

} // namespace

extern "C" {

AioHipsDataset* aio_hips_open(const char* out_dir, int product) {
    g_rd_error.clear();
    if (!out_dir || !*out_dir || product < 0 || product > 2) {
        set_err("参数无效");
        return nullptr;
    }
    std::unique_ptr<AioHipsDataset> d(new AioHipsDataset);
    d->product = product;
    const char* sub = product == AIO_HIPS_RD_SIGNAL ? "signal" :
                      product == AIO_HIPS_RD_SUPPORT ? "support" : "snr";
    d->dir = std::string(out_dir) + "/" + sub;
    d->props = parse_properties(d->dir + "/properties");
    auto geti = [&](const std::string& k, int def) -> int {
        auto it = d->props.find(k);
        return it == d->props.end() ? def : std::atoi(it->second.c_str());
    };
    d->hips_order = geti("hips_order", 0);
    d->tile_width = geti("hips_tile_width", 512);
    if (d->props.find("hips_version") == d->props.end()) {
        set_err("properties 缺失 hips_version: " + d->dir);
        return nullptr;
    }
    load_tiles_from_moc(d.get());
    if (product == AIO_HIPS_RD_SNR) {
        // 读取全部 SNR TSV tiles
        for (uint64_t ip : d->tiles) {
            std::string p = tile_path(d->dir, d->hips_order, ip, ".tsv");
            std::ifstream f(p);
            std::string line;
            bool first = true;
            while (std::getline(f, line)) {
                if (line.empty()) continue;
                if (first) { first = false; if (line[0] == '#') continue; }
                long long sid; double ra, dec, snr;
                if (std::sscanf(line.c_str(), "%lld %lf %lf %lf",
                                &sid, &ra, &dec, &snr) == 4) {
                    AioHipsSnrPoint2 pt;
                    pt.star_id = sid; pt.ra = ra; pt.dec = dec; pt.snr = snr;
                    d->snr.push_back(pt);
                }
            }
        }
    }
    return d.release();
}

int aio_hips_get_properties(AioHipsDataset* d, char* buf, int buf_size) {
    if (!d || !buf || buf_size <= 0) return -1;
    std::string s;
    for (const auto& kv : d->props) s += kv.first + "=" + kv.second + "\n";
    std::strncpy(buf, s.c_str(), (size_t)buf_size - 1);
    buf[buf_size - 1] = '\0';
    return 0;
}

int aio_hips_tile_count(AioHipsDataset* d) {
    return d ? (int)d->tiles.size() : -1;
}

int aio_hips_tile_ipix(AioHipsDataset* d, int i, uint64_t* out_ipix) {
    if (!d || !out_ipix || i < 0 || (size_t)i >= d->tiles.size()) return -1;
    *out_ipix = d->tiles[(size_t)i];
    return 0;
}

int aio_hips_read_tile_f32(AioHipsDataset* d, uint64_t ipix, float* out) {
    return read_tile_t(d, ipix, out);
}

int aio_hips_read_tile_f64(AioHipsDataset* d, uint64_t ipix, double* out) {
    return read_tile_t(d, ipix, out);
}

int aio_hips_read_snr_catalog(AioHipsDataset* d, double* ra, double* dec,
                              double* snr, int64_t* star_id, int max) {
    if (!d || d->product != AIO_HIPS_RD_SNR) return -1;
    int n = (int)std::min((size_t)max, d->snr.size());
    for (int i = 0; i < n; ++i) {
        if (ra) ra[i] = d->snr[(size_t)i].ra;
        if (dec) dec[i] = d->snr[(size_t)i].dec;
        if (snr) snr[i] = d->snr[(size_t)i].snr;
        if (star_id) star_id[i] = d->snr[(size_t)i].star_id;
    }
    return n;
}

void aio_hips_close(AioHipsDataset* d) {
    delete d;
}

const char* aio_hips_reader_last_error(void) {
    return g_rd_error.c_str();
}

} // extern "C"
