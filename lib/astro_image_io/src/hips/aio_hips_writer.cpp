// ============================================================================
// aio_hips_writer.cpp - IVOA HiPS 1.4 生产链写入器 (Phase1 Final Closure V3)
//
// 数据流 (无 HISS 中转):
//   Drizzle TileAccumulator -> AstroSphereTileView -> 本写入器 (流式)
//
// 输出结构 (Product Set):
//   <out_dir>/
//     manifest.json
//     signal/   Image HiPS: signal  = flux_sum/covered_area  (float32/64)
//     support/  Image HiPS: support = covered_area/A_cell    (float32/64)
//     snr/      Catalogue HiPS: NorderK/DirD/NpixN.tsv
//   每个子产品独立 properties / Moc.fits / 低阶 hierarchy tiles。
//
// FITS 全部由 vendored CFITSIO 4.6.4 写入 (含 DATASUM/CHECKSUM)。
// ============================================================================

#include "aio_hips.h"

#include <fitsio.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <limits>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace {

thread_local std::string g_hips_error;

void set_error(const std::string& msg) { g_hips_error = msg; }

double kPi() { return std::acos(-1.0); }

uint32_t ilog2_u64(uint64_t v) {
    uint32_t l = 0;
    while (v > 1) { v >>= 1; ++l; }
    return l;
}

void make_dirs(const std::string& path) {
    std::string norm = path;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    std::string cur;
    for (size_t i = 0; i <= norm.size(); ++i) {
        char c = (i < norm.size()) ? norm[i] : '/';
        if (c == '/' || i == norm.size()) {
            // 去掉段尾空白 (Windows 路径含空格时 _mkdir 会失败)
            while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t'))
                cur.pop_back();
            // 跳过空段与盘符前缀 (如 "F:")
            if (!cur.empty() && !(cur.size() == 2 && cur[1] == ':')) {
#ifdef _WIN32
                _mkdir(cur.c_str());
#else
                mkdir(cur.c_str(), 0755);
#endif
            }
            if (i < norm.size()) cur += '/';
            continue;
        }
        cur += c;
    }
}

std::string tile_rel_path(int order, uint64_t ipix, const char* ext) {
    uint64_t dir = ipix / 10000;
    uint64_t npix = ipix % 10000;
    char buf[512];
    std::snprintf(buf, sizeof(buf), "Norder%d/Dir%llu/Npix%llu%s",
                  order, (unsigned long long)dir, (unsigned long long)npix, ext);
    return std::string(buf);
}

// RA/Dec (deg) -> NESTED ipix @ nside (标准 HEALPix ang2pix, 支持任意 2 幂 nside)
uint64_t ang2ipix_nest(double ra_deg, double dec_deg, uint32_t nside) {
    const double z = std::sin(dec_deg * kPi() / 180.0);
    const double phi = ra_deg * kPi() / 180.0;
    const double za = std::acos(std::max(-1.0, std::min(1.0, z)));
    const double tt = za / (kPi() / 2.0);  // [0,2]
    uint32_t face = 0;
    double x = 0.0, y = 0.0;
    if (tt <= 1.0) {
        double th = 0.5 * za;
        double zph = (phi >= 0.0 && phi < kPi()) ? phi : phi + 2.0 * kPi();
        double t1 = std::tan(th);
        double t2 = std::tan((kPi() / 2.0 - th) / 2.0);
        double s = std::sin(zph), c = std::cos(zph);
        face = (zph >= 0.0 && zph < kPi() / 2.0) ? 0 :
               (zph >= kPi() / 2.0 && zph < kPi()) ? 1 :
               (zph >= kPi() && zph < 3.0 * kPi() / 2.0) ? 2 : 3;
        if (face == 0)      { x = t2;           y = t1; }
        else if (face == 1) { x = -t2;          y = t1; }
        else if (face == 2) { x = -t1 / std::max(s, 1e-30); y = -t2; }
        else                { x =  t1 / std::max(s, 1e-30); y =  t2; }
        x = std::max(-1.0, std::min(1.0, x));
        y = std::max(-1.0, std::min(1.0, y));
    } else {
        double th = kPi() - 0.5 * za;
        double zph = (phi >= 0.0 && phi < kPi()) ? phi : phi + 2.0 * kPi();
        double t1 = std::tan(th);
        double t2 = std::tan((kPi() / 2.0 - th) / 2.0);
        double s = std::sin(zph), c = std::cos(zph);
        face = (zph >= 0.0 && zph < kPi() / 2.0) ? 4 :
               (zph >= kPi() / 2.0 && zph < kPi()) ? 5 :
               (zph >= kPi() && zph < 3.0 * kPi() / 2.0) ? 6 : 7;
        if (face == 4)      { x = -t1 / std::max(s, 1e-30); y =  t2; }
        else if (face == 5) { x =  t2;           y = t1; }
        else if (face == 6) { x =  t2;           y = -t1; }
        else                { x = -t1 / std::max(s, 1e-30); y = -t2; }
        x = std::max(-1.0, std::min(1.0, x));
        y = std::max(-1.0, std::min(1.0, y));
    }
    // (x,y) in [-1,1] -> 投影坐标
    const double s_ = std::sin((kPi() / 2.0) * tt);  // unused; keep for clarity
    (void)s_;
    double xx = x * (nside / 2.0);
    double yy = y * (nside / 2.0);
    double tmp = std::max(0.0, std::min(2.0 * nside - 1.0, nside * (1.0 - yy)));
    uint32_t jr = (uint32_t)std::floor(tmp);
    tmp = std::max(0.0, std::min(2.0 * nside - 1.0, nside * (1.0 + xx)));
    uint32_t ir = (uint32_t)std::floor(tmp);
    uint32_t iy = (jr < nside) ? jr : (2 * nside - 1 - jr);
    uint32_t ix = (ir < nside) ? ir : (2 * nside - 1 - ir);
    // interleave
    uint64_t ipix = 0;
    for (uint32_t b = 0; b < 16; ++b) {
        uint64_t bit = 1ULL << b;
        if (ix & bit) ipix |= (1ULL << (2 * b + 1));
        if (iy & bit) ipix |= (1ULL << (2 * b));
    }
    ipix += (uint64_t)face * (uint64_t)nside * nside;
    return ipix;
}

// FITS 字符串转义 (单引号翻倍, 80 字符内)
std::string fits_str(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    if (out.size() > 68) out = out.substr(0, 68);
    return out;
}

// ---------------------------------------------------------------------------
// CFITSIO 错误辅助
// ---------------------------------------------------------------------------
bool fits_ok(int status, const std::string& where) {
    if (status == 0) return true;
    char msg[FLEN_ERRMSG];
    msg[0] = '\0';
    fits_get_errstatus(status, msg);
    set_error(where + ": " + msg);
    return false;
}

// ---------------------------------------------------------------------------
// 单 FITS 图像写 (含 checksum)
//   data: 行主序数组 (NAXIS1 最快), naxis1 x naxis2
// ---------------------------------------------------------------------------
bool write_fits_image(const std::string& path,
                      int bitpix,
                      long naxis1, long naxis2,
                      const void* data,
                      const std::vector<std::pair<std::string, std::string>>& cards,
                      const std::string& object,
                      const std::string& obs_filter,
                      double exptime,
                      const std::string& obs_date) {
    int status = 0;
    fitsfile* fptr = nullptr;
    std::string path_n = path;
    std::replace(path_n.begin(), path_n.end(), '\\', '/');
    // CFITSIO fits_create_file 拒绝覆盖已存在文件; 显式先删 (HiPS 输出允许 overwrite)
    std::remove(path_n.c_str());
    if (fits_create_file(&fptr, path_n.c_str(), &status)) {
        return fits_ok(status, "fits_create_file " + path);
    }
    long naxes[2] = {naxis1, naxis2};
    if (fits_create_img(fptr, bitpix, 2, naxes, &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "fits_create_img " + path);
    }
    fits_write_key_str(fptr, "PIXTYPE", (char*)"HEALPIX", (char*)"HEALPix pixelization", &status);
    fits_write_key_str(fptr, "ORDERING", (char*)"NESTED", (char*)"Pixel ordering", &status);
    fits_write_key_str(fptr, "COORDSYS", (char*)"C", (char*)"Equatorial", &status);
    if (!object.empty())
        fits_write_key_str(fptr, "OBJECT", (char*)fits_str(object).c_str(), nullptr, &status);
    if (!obs_filter.empty())
        fits_write_key_str(fptr, "FILTER", (char*)fits_str(obs_filter).c_str(), nullptr, &status);
    if (exptime > 0.0)
        fits_write_key_dbl(fptr, "EXPTIME", exptime, 8, (char*)"Exposure time (s)", &status);
    if (!obs_date.empty())
        fits_write_key_str(fptr, "DATE-OBS", (char*)fits_str(obs_date).c_str(), nullptr, &status);
    for (const auto& kv : cards) {
        if (kv.first == "NSIDE")
            fits_write_key_lng(fptr, (char*)kv.first.c_str(), std::atol(kv.second.c_str()),
                               (char*)"HEALPix nside of tile pixels", &status);
        else if (kv.first == "FIRSTPIX")
            fits_write_key_lng(fptr, (char*)kv.first.c_str(), std::atol(kv.second.c_str()),
                               (char*)"First tile pixel (NESTED)", &status);
        else if (kv.first == "LASTPIX")
            fits_write_key_lng(fptr, (char*)kv.first.c_str(), std::atol(kv.second.c_str()),
                               (char*)"Last tile pixel (NESTED)", &status);
        else
            fits_write_key_str(fptr, (char*)kv.first.c_str(),
                               (char*)fits_str(kv.second).c_str(), nullptr, &status);
        if (status) break;
    }
    long fpixel[2] = {1, 1};
    long nelem = naxis1 * naxis2;
    if (bitpix == -32)
        fits_write_pix(fptr, TFLOAT, fpixel, nelem, (void*)data, &status);
    else if (bitpix == -64)
        fits_write_pix(fptr, TDOUBLE, fpixel, nelem, (void*)data, &status);
    if (status) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "fits_write_pix " + path);
    }
    if (fits_write_chksum(fptr, &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "fits_write_chksum " + path);
    }
    if (fits_close_file(fptr, &status)) {
        return fits_ok(status, "fits_close_file " + path);
    }
    return true;
}

// ---------------------------------------------------------------------------
// MOC FITS (BINTABLE UNIQ) 写
// ---------------------------------------------------------------------------
bool write_moc_fits(const std::string& path,
                    const std::vector<uint64_t>& uniq,
                    uint32_t order) {
    if (uniq.empty()) return true;  // 空 MOC: 不写
    int status = 0;
    fitsfile* fptr = nullptr;
    std::string path_n = path;
    std::replace(path_n.begin(), path_n.end(), '\\', '/');
    std::remove(path_n.c_str());
    if (fits_create_file(&fptr, path_n.c_str(), &status)) {
        return fits_ok(status, "moc create " + path);
    }
    char* ttype[1] = {(char*)"UNIQ"};
    char* tform[1] = {(char*)"K"};
    char* tunit[1] = {(char*)""};
    if (fits_create_tbl(fptr, BINARY_TBL, (long)uniq.size(), 1,
                        ttype, tform, tunit, (char*)"", &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "moc create_tbl " + path);
    }
    fits_write_key_lng(fptr, "MOCORDER", (long)order, (char*)"MOC order", &status);
    fits_write_key_lng(fptr, "PIXCOUNT", (long)uniq.size(), (char*)"Cell count", &status);
    if (status) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "moc keys " + path);
    }
    long fpixel = 1;
    if (fits_write_col(fptr, TLONGLONG, 1, fpixel, 1, (long long)uniq.size(),
                       const_cast<long long*>((const long long*)uniq.data()), &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "moc write_col " + path);
    }
    if (fits_write_chksum(fptr, &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "moc chksum " + path);
    }
    if (fits_close_file(fptr, &status)) {
        return fits_ok(status, "moc close " + path);
    }
    return true;
}

void write_properties(const std::string& path,
                      const std::vector<std::pair<std::string, std::string>>& kv) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    for (const auto& p : kv)
        std::fprintf(f, "%s=%s\n", p.first.c_str(), p.second.c_str());
    std::fclose(f);
}

// ---------------------------------------------------------------------------
// hierarchy 累加器
// ---------------------------------------------------------------------------
struct AncestorAcc {
    std::vector<float>  sumFluxF;
    std::vector<double> sumFluxD;
    std::vector<float>  sumAreaF;
    std::vector<double> sumAreaD;
    std::vector<uint32_t> count;
    bool is_f32 = true;

    void ensure(bool f32) {
        is_f32 = f32;
        const size_t n = 512 * 512;
        if (f32) {
            if (sumFluxF.empty()) sumFluxF.assign(n, 0.0f);
            if (sumAreaF.empty()) sumAreaF.assign(n, 0.0f);
        } else {
            if (sumFluxD.empty()) sumFluxD.assign(n, 0.0);
            if (sumAreaD.empty()) sumAreaD.assign(n, 0.0);
        }
        if (count.empty()) count.assign(n, 0u);
    }
    void add(size_t i, double flux, double area) {
        if (is_f32) { sumFluxF[i] += (float)flux; sumAreaF[i] += (float)area; }
        else        { sumFluxD[i] += flux;        sumAreaD[i] += area; }
        ++count[i];
    }
    double fluxAt(size_t i) const { return is_f32 ? (double)sumFluxF[i] : sumFluxD[i]; }
    double areaAt(size_t i) const { return is_f32 ? (double)sumAreaF[i] : sumAreaD[i]; }
};

} // namespace

// ============================================================================
// Product Set 实现
// ============================================================================
struct AioHipsProductSet {
    std::string out_dir;
    uint32_t nside = 0;
    uint32_t tile_width = 512;
    int32_t data_type = AIO_HIPS_FLOAT32;
    int flags = AIO_HIPS_PRODUCT_ALL;
    uint32_t leaf_order = 0;   // L
    uint32_t tile_order = 0;   // K = L-9
    double A_cell = 0.0;       // 叶级 cell 面积 sr
    std::string creator_did, obs_title, obs_filter, obs_date;
    double exposure = 0.0;
    uint32_t moc_order = 0;

    std::set<uint64_t> moc_cells;          // 叶级 tile cells @ order K (有数据)
    std::vector<uint64_t> leaf_ipix_list;  // 写入顺序
    std::vector<std::map<uint64_t, AncestorAcc>> hier;  // hier[k] for k<K
    std::vector<AioHipsSnrPoint> snr;
    double moc_area_sr = 0.0;              // Σ moc cell 面积 (order K)
    double covered_area_sr = 0.0;          // Σ covered_area (真实覆盖)
    double sig_min = 1e300, sig_max = -1e300;
    bool finalized = false;
};

extern "C" {

AioHipsProductSet* aio_hips_product_begin(
    const char* out_dir,
    uint32_t nside,
    uint32_t tile_width,
    int32_t data_type,
    int flags,
    const char* creator_did,
    const char* obs_title,
    const char* obs_filter,
    double exposure_s,
    const char* obs_date,
    uint32_t moc_order) {
    g_hips_error.clear();
    if (!out_dir || !*out_dir || nside < 512 || tile_width != 512 ||
        (data_type != AIO_HIPS_FLOAT32 && data_type != AIO_HIPS_FLOAT64) ||
        (flags & ~AIO_HIPS_PRODUCT_ALL) != 0) {
        set_error("aio_hips_product_begin: 参数无效 (nside>=512, tile_width=512, dtype 0/1)");
        return nullptr;
    }
    std::unique_ptr<AioHipsProductSet> ps(new AioHipsProductSet);
    ps->out_dir = out_dir;
    ps->nside = nside;
    ps->tile_width = tile_width;
    ps->data_type = data_type;
    ps->flags = flags;
    ps->leaf_order = ilog2_u64(nside);
    ps->tile_order = ps->leaf_order - 9;
    ps->A_cell = 4.0 * kPi() / (12.0 * (double)nside * nside);
    ps->creator_did = creator_did ? creator_did : "ivo://astrocs/phase1";
    ps->obs_title = obs_title ? obs_title : "AstroCS Phase1";
    ps->obs_filter = obs_filter ? obs_filter : "";
    ps->obs_date = obs_date ? obs_date : "";
    ps->exposure = exposure_s;
    ps->moc_order = (moc_order == 0) ? ps->tile_order : std::min(moc_order, ps->tile_order);
    ps->hier.resize(ps->tile_order);
    return ps.release();
}

int aio_hips_write_signal_support_tile(AioHipsProductSet* ps,
                                       const AstroSphereTileView* view) {
    g_hips_error.clear();
    if (!ps || !view) { set_error("null handle/view"); return -1; }
    if (view->width != 512 || view->leaf_order != ps->leaf_order ||
        view->data_type != ps->data_type) {
        set_error("view 与产品集不匹配 (width=512, leaf_order/ dtype 必须一致)");
        return -2;
    }
    const uint64_t npix_order = 12ULL * (1ULL << (2ULL * ps->tile_order));
    if (view->parent_ipix >= npix_order) {
        set_error("parent_ipix 超出 Norder" + std::to_string(ps->tile_order) + " 范围");
        return -3;
    }
    const size_t n = 512 * 512;
    const bool f32 = (ps->data_type == AIO_HIPS_FLOAT32);

    // 1. 转换 signal/support
    std::vector<float>  sigF(n), supF(n);
    std::vector<double> sigD(n), supD(n);
    std::vector<uint8_t> valid;
    if (view->valid_mask)
        valid.assign((const uint8_t*)view->valid_mask, (const uint8_t*)view->valid_mask + n);

    double tile_covered = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const bool v = valid.empty() || valid[i];
        double flux = 0.0, area = 0.0;
        if (f32) {
            if (view->flux_sum) flux = (double)((const float*)view->flux_sum)[i];
            if (view->covered_area) area = (double)((const float*)view->covered_area)[i];
        } else {
            if (view->flux_sum) flux = ((const double*)view->flux_sum)[i];
            if (view->covered_area) area = ((const double*)view->covered_area)[i];
        }
        double sig = 0.0, sup = 0.0;
        if (v && area > 0.0 && std::isfinite(flux) && std::isfinite(area)) {
            sig = flux / area;
            sup = area / ps->A_cell;
            if (sup > 1.0) sup = 1.0;
            tile_covered += area;
            if (sig < ps->sig_min) ps->sig_min = sig;
            if (sig > ps->sig_max) ps->sig_max = sig;
        } else {
            sig = std::numeric_limits<double>::quiet_NaN();
        }
        if (f32) { sigF[i] = (float)sig; supF[i] = (float)sup; }
        else     { sigD[i] = sig;        supD[i] = sup; }
    }

    // 2. 写 signal/support FITS (CFITSIO + checksum)
    const int bitpix = f32 ? -32 : -64;
    std::vector<std::pair<std::string, std::string>> cards;
    cards.push_back({"NSIDE", std::to_string(ps->nside)});
    cards.push_back({"FIRSTPIX", "0"});
    cards.push_back({"LASTPIX", std::to_string(n - 1)});
    std::string rel = tile_rel_path((int)ps->tile_order, view->parent_ipix, ".fits");
    if (ps->flags & AIO_HIPS_PRODUCT_SIGNAL) {
        std::string p = ps->out_dir + "/signal/" + rel;
        make_dirs(p.substr(0, p.find_last_of('/')));
        if (!write_fits_image(p, bitpix, 512, 512, f32 ? (const void*)sigF.data() : (const void*)sigD.data(),
                              cards, ps->obs_title, ps->obs_filter, ps->exposure, ps->obs_date)) {
            return -4;
        }
    }
    if (ps->flags & AIO_HIPS_PRODUCT_SUPPORT) {
        std::string p = ps->out_dir + "/support/" + rel;
        make_dirs(p.substr(0, p.find_last_of('/')));
        if (!write_fits_image(p, bitpix, 512, 512, f32 ? (const void*)supF.data() : (const void*)supD.data(),
                              cards, ps->obs_title, ps->obs_filter, ps->exposure, ps->obs_date)) {
            return -5;
        }
    }

    // 3. MOC + 覆盖统计
    if (ps->moc_cells.insert(view->parent_ipix).second) {
        ps->leaf_ipix_list.push_back(view->parent_ipix);
        ps->moc_area_sr += 4.0 * kPi() / (12.0 * (1ULL << (2 * ps->tile_order)));
    }
    ps->covered_area_sr += tile_covered;

    // 4. hierarchy 累加 (k = K-1 .. 0)
    for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
        int dk = (int)ps->tile_order - k;
        uint64_t shift = 2ULL * (uint64_t)dk;
        uint64_t mask = (shift >= 64) ? ~0ULL : ((1ULL << shift) - 1ULL);
        uint64_t A = view->parent_ipix >> shift;
        uint64_t s = view->parent_ipix & mask;
        AncestorAcc& acc = ps->hier[(size_t)k][A];
        acc.ensure(f32);
        for (size_t i = 0; i < n; ++i) {
            const bool v = valid.empty() || valid[i];
            double flux = 0.0, area = 0.0;
            if (f32) {
                flux = (double)sigF[i] * (double)supF[i] * ps->A_cell;
                area = (double)supF[i] * ps->A_cell;
            } else {
                flux = sigD[i] * supD[i] * ps->A_cell;
                area = supD[i] * ps->A_cell;
            }
            if (!v || !(area > 0.0) || !std::isfinite(flux)) continue;
            // 叶 (P,l) -> A@k 内 order-(k+9) 单元 NESTED 索引
            //   full = (s<<18)|l (order K+9 within A), z = full >> 2*(K-k)
            size_t z = (size_t)(((s << 18ULL) | (uint64_t)i) >>
                                (2ULL * (uint64_t)(ps->tile_order - (uint32_t)k)));
            acc.add(z, flux, area);
        }
    }
    return 0;
}

int aio_hips_write_snr_points(AioHipsProductSet* ps,
                              const AioHipsSnrPoint* pts,
                              int n) {
    g_hips_error.clear();
    if (!ps || (!pts && n > 0)) { set_error("null pts"); return -1; }
    for (int i = 0; i < n; ++i)
        ps->snr.push_back(pts[i]);
    return 0;
}

// ---------------------------------------------------------------------------
// 内部: 写一个子产品的 properties / metadata / MOC / hierarchy
// ---------------------------------------------------------------------------
static bool finalize_image_product(AioHipsProductSet* ps,
                                   const std::string& prod,
                                   const std::string& subtype,
                                   const std::string& data_range,
                                   double moc_frac,
                                   double covered_frac) {
    const std::string dir = ps->out_dir + "/" + prod;
    make_dirs(dir);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", 3600.0 * 180.0 / kPi() * std::sqrt(kPi() / 3.0) / (double)ps->nside);
    std::vector<std::pair<std::string, std::string>> kv;
    kv.push_back({"creator_did", ps->creator_did});
    kv.push_back({"obs_title", ps->obs_title});
    kv.push_back({"obs_creator", "AstroCS"});
    kv.push_back({"hips_version", "1.4"});
    kv.push_back({"hips_order", std::to_string(ps->tile_order)});
    kv.push_back({"hips_tile_width", "512"});
    kv.push_back({"hips_frame", "equatorial"});
    kv.push_back({"dataproduct_type", "image"});
    kv.push_back({"dataproduct_subtype", subtype});
    kv.push_back({"hips_tile_format", "fits"});
    kv.push_back({"hips_status", "public master"});
    kv.push_back({"hips_creator", "AstroCS (astro_image_io)"});
    kv.push_back({"hips_builder", "AstroCS aio_hips_writer (CFITSIO 4.6.4)"});
    kv.push_back({"hips_estsize", "1000000"});
    kv.push_back({"hips_release_date", "2026-08-08"});
    kv.push_back({"hips_hierarchy", "true"});
    kv.push_back({"hips_pixel_scale", buf});
    kv.push_back({"hips_initial_fov", "60"});
    kv.push_back({"moc_sky_fraction", std::to_string(moc_frac)});
    kv.push_back({"astrocs_covered_sky_fraction", std::to_string(covered_frac)});
    kv.push_back({"astrocs_signal_dtype", ps->data_type == AIO_HIPS_FLOAT32 ? "float32" : "float64"});
    if (!data_range.empty()) kv.push_back({"hips_data_range", data_range});
    if (!ps->obs_filter.empty()) kv.push_back({"obs_filter", ps->obs_filter});
    if (ps->exposure > 0.0) kv.push_back({"obs_exptime", std::to_string(ps->exposure)});
    if (!ps->obs_date.empty()) kv.push_back({"obs_date", ps->obs_date});
    write_properties(dir + "/properties", kv);
    // metadata.fits (产品级) 由 Moc.fits 提供结构; 再写一份极简 metadata.fits
    {
        int status = 0;
        fitsfile* fptr = nullptr;
        std::string mp = dir + "/metadata.fits";
        std::replace(mp.begin(), mp.end(), '\\', '/');
        std::remove(mp.c_str());
        if (!fits_create_file(&fptr, mp.c_str(), &status)) {
            fits_write_key_str(fptr, "PIXTYPE", (char*)"HEALPIX", nullptr, &status);
            fits_write_key_str(fptr, "ORDERING", (char*)"NESTED", nullptr, &status);
            fits_write_key_lng(fptr, "NSIDE", (long)ps->nside, nullptr, &status);
            fits_write_key_lng(fptr, "HIPSTILEWIDTH", 512L, nullptr, &status);
            fits_write_key_str(fptr, "DATAPRODTYPE", (char*)"image", nullptr, &status);
            fits_close_file(fptr, &status);
        }
    }
    // MOC
    std::vector<uint64_t> uniq;
    uniq.reserve(ps->moc_cells.size());
    for (uint64_t c : ps->moc_cells) uniq.push_back(4ULL * (1ULL << (2ULL * ps->moc_order)) + (c >> (2ULL * ((uint64_t)ps->tile_order - ps->moc_order))));
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    if (!write_moc_fits(dir + "/Moc.fits", uniq, ps->moc_order)) return false;
    return true;
}

// hierarchy: 从 order K-1 到 0 逐级写出
static bool finalize_hierarchy(AioHipsProductSet* ps) {
    const int bitpix = ps->data_type == AIO_HIPS_FLOAT32 ? -32 : -64;
    const size_t n = 512 * 512;
    std::vector<float> sigF(n), supF(n);
    std::vector<double> sigD(n), supD(n);
    for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
        for (auto& kv : ps->hier[(size_t)k]) {
            uint64_t A = kv.first;
            AncestorAcc& acc = kv.second;
            const uint32_t nside_k = 1u << (k + 9);
            const double A_cell_k = 4.0 * kPi() / (12.0 * (double)nside_k * nside_k);
            std::vector<std::pair<std::string, std::string>> cards;
            cards.push_back({"NSIDE", std::to_string(nside_k)});
            cards.push_back({"FIRSTPIX", "0"});
            cards.push_back({"LASTPIX", std::to_string(n - 1)});
            std::string rel = tile_rel_path(k, A, ".fits");
            for (size_t i = 0; i < n; ++i) {
                double area = acc.areaAt(i);
                double flux = acc.fluxAt(i);
                double sig = 0.0, sup = 0.0;
                if (area > 0.0 && std::isfinite(flux)) {
                    sig = flux / area;
                    sup = area / A_cell_k;
                    if (sup > 1.0) sup = 1.0;
                } else {
                    sig = std::numeric_limits<double>::quiet_NaN();
                }
                if (bitpix == -32) { sigF[i] = (float)sig; supF[i] = (float)sup; }
                else               { sigD[i] = sig;        supD[i] = sup; }
            }
            if (ps->flags & AIO_HIPS_PRODUCT_SIGNAL) {
                std::string p = ps->out_dir + "/signal/" + rel;
                make_dirs(p.substr(0, p.find_last_of('/')));
                if (!write_fits_image(p, bitpix, 512, 512,
                                      bitpix == -32 ? (const void*)sigF.data() : (const void*)sigD.data(),
                                      cards, ps->obs_title, ps->obs_filter, ps->exposure, ps->obs_date))
                    return false;
            }
            if (ps->flags & AIO_HIPS_PRODUCT_SUPPORT) {
                std::string p = ps->out_dir + "/support/" + rel;
                make_dirs(p.substr(0, p.find_last_of('/')));
                if (!write_fits_image(p, bitpix, 512, 512,
                                      bitpix == -32 ? (const void*)supF.data() : (const void*)supD.data(),
                                      cards, ps->obs_title, ps->obs_filter, ps->exposure, ps->obs_date))
                    return false;
            }
        }
    }
    return true;
}

// SNR Catalogue HiPS: TSV tiles + properties + metadata.xml
static bool finalize_snr_product(AioHipsProductSet* ps) {
    if (ps->snr.empty()) return true;
    const std::string dir = ps->out_dir + "/snr";
    make_dirs(dir);
    std::map<uint64_t, std::vector<const AioHipsSnrPoint*>> by_cell;
    std::set<uint64_t> cells;
    for (const auto& p : ps->snr) {
        uint64_t ip = ang2ipix_nest(p.ra_deg, p.dec_deg, 1u << ps->tile_order);
        by_cell[ip].push_back(&p);
        cells.insert(ip);
    }
    const char* header =
        "# star_id ra dec snr quality_flags photometric_status\n";
    for (auto& kv : by_cell) {
        std::string rel = tile_rel_path((int)ps->tile_order, kv.first, ".tsv");
        std::string p = dir + "/" + rel;
        make_dirs(p.substr(0, p.find_last_of('/')));
        FILE* f = std::fopen(p.c_str(), "wb");
        if (!f) { set_error("无法创建 SNR tile: " + p); return false; }
        std::fputs(header, f);
        for (const AioHipsSnrPoint* sp : kv.second) {
            std::fprintf(f, "%lld %.12f %.12f %.6f 0 1\n",
                         (long long)sp->source_id, sp->ra_deg, sp->dec_deg, sp->snr);
        }
        std::fclose(f);
    }
    std::vector<std::pair<std::string, std::string>> kv2;
    kv2.push_back({"creator_did", ps->creator_did});
    kv2.push_back({"obs_title", ps->obs_title + " (SNR catalogue)"});
    kv2.push_back({"hips_version", "1.4"});
    kv2.push_back({"hips_order", std::to_string(ps->tile_order)});
    kv2.push_back({"hips_frame", "equatorial"});
    kv2.push_back({"dataproduct_type", "catalog"});
    kv2.push_back({"dataproduct_subtype", "snr"});
    kv2.push_back({"hips_tile_format", "tsv"});
    kv2.push_back({"hips_status", "public master"});
    kv2.push_back({"hips_creator", "AstroCS (astro_image_io)"});
    kv2.push_back({"hips_builder", "AstroCS aio_hips_writer (CFITSIO 4.6.4)"});
    kv2.push_back({"hips_release_date", "2026-08-08"});
    kv2.push_back({"hips_initial_fov", "60"});
    kv2.push_back({"moc_sky_fraction",
        std::to_string((double)cells.size() * 4.0 * kPi() /
                       (12.0 * (1ULL << (2ULL * ps->tile_order))) / (4.0 * kPi()))});
    kv2.push_back({"catalog_nrows", std::to_string(ps->snr.size())});
    write_properties(dir + "/properties", kv2);
    {
        FILE* f = std::fopen((dir + "/metadata.xml").c_str(), "wb");
        if (f) {
            std::fprintf(f,
                "<?xml version=\"1.0\"?>\n<hips_metadata>\n"
                "  <property name=\"dataproduct_type\" value=\"catalog\"/>\n"
                "  <column name=\"star_id\" ucd=\"meta.id\"/>\n"
                "  <column name=\"ra\" ucd=\"pos.eq.ra\"/>\n"
                "  <column name=\"dec\" ucd=\"pos.eq.dec\"/>\n"
                "  <column name=\"snr\" ucd=\"stat.snr\"/>\n"
                "  <column name=\"quality_flags\" ucd=\"meta.code.qual\"/>\n"
                "  <column name=\"photometric_status\" ucd=\"meta.code.status\"/>\n"
                "</hips_metadata>\n");
            std::fclose(f);
        }
    }
    std::vector<uint64_t> uniq;
    for (uint64_t c : cells)
        uniq.push_back(4ULL * (1ULL << (2ULL * ps->moc_order)) + (c >> (2ULL * ((uint64_t)ps->tile_order - ps->moc_order))));
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    if (!write_moc_fits(dir + "/Moc.fits", uniq, ps->moc_order)) return false;
    return true;
}

int aio_hips_finalize(AioHipsProductSet* ps) {
    g_hips_error.clear();
    if (!ps) { set_error("null handle"); return -1; }
    if (ps->finalized) { set_error("已 finalize"); return -2; }
    ps->finalized = true;
    std::fprintf(stderr, "[hips] finalize: n_leaf=%zu flags=%d\n",
                 ps->leaf_ipix_list.size(), ps->flags);
    const double moc_frac = ps->moc_area_sr / (4.0 * kPi());
    const double cov_frac = ps->covered_area_sr / (4.0 * kPi());
    std::string range;
    if (ps->sig_min <= ps->sig_max)
        range = std::to_string(ps->sig_min) + " " + std::to_string(ps->sig_max);
    if (ps->flags & AIO_HIPS_PRODUCT_SIGNAL) {
        std::fprintf(stderr, "[hips] finalize: signal product\n");
        if (!finalize_image_product(ps, "signal", "surface brightness", range, moc_frac, cov_frac)) {
            return -3;
        }
    }
    if (ps->flags & AIO_HIPS_PRODUCT_SUPPORT) {
        std::fprintf(stderr, "[hips] finalize: support product\n");
        if (!finalize_image_product(ps, "support", "coverage fraction", "", moc_frac, cov_frac)) {
            return -4;
        }
    }
    if ((ps->flags & (AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT)) &&
        !finalize_hierarchy(ps)) {
        std::fprintf(stderr, "[hips] finalize: hierarchy failed\n");
        return -5;
    }
    if ((ps->flags & AIO_HIPS_PRODUCT_SNR) && !finalize_snr_product(ps)) {
        std::fprintf(stderr, "[hips] finalize: snr failed\n");
        return -6;
    }
    std::fprintf(stderr, "[hips] finalize: ok\n");
    // manifest.json
    {
        FILE* f = std::fopen((ps->out_dir + "/manifest.json").c_str(), "wb");
        if (f) {
            std::fprintf(f,
                "{\n"
                "  \"format_version\": 1,\n"
                "  \"hips_version\": \"1.4\",\n"
                "  \"nside\": %u,\n"
                "  \"tile_width\": %u,\n"
                "  \"data_type\": \"%s\",\n"
                "  \"products\": [%s%s%s],\n"
                "  \"n_leaf_tiles\": %zu,\n"
                "  \"moc_sky_fraction\": %.8f,\n"
                "  \"astrocs_covered_sky_fraction\": %.8f,\n"
                "  \"signal_dtype\": \"%s\"\n"
                "}\n",
                ps->nside, ps->tile_width,
                ps->data_type == AIO_HIPS_FLOAT32 ? "float32" : "float64",
                (ps->flags & AIO_HIPS_PRODUCT_SIGNAL) ? "\"signal\"" : "",
                ((ps->flags & AIO_HIPS_PRODUCT_SIGNAL) && (ps->flags & AIO_HIPS_PRODUCT_SUPPORT)) ? "," : "",
                (ps->flags & AIO_HIPS_PRODUCT_SUPPORT) ? "\"support\"" : "",
                ps->leaf_ipix_list.size(), moc_frac, cov_frac,
                ps->data_type == AIO_HIPS_FLOAT32 ? "float32" : "float64");
            std::fclose(f);
        }
    }
    delete ps;
    return 0;
}

int aio_hips_abort(AioHipsProductSet* ps) {
    if (!ps) return 0;
    delete ps;
    return 0;
}

const char* aio_hips_last_error(void) {
    return g_hips_error.c_str();
}

// ============================================================================
// 兼容旧接口 (HISS 中转验证): 旧 AioHipsTile -> AstroSphereTileView 流式
//   旧语义: signal = F/support_frac, support uint8 (0..255)
//   转换:   covered_area = su/255*A_cell, flux_sum = signal*(su/255)
//   新语义: signal = flux_sum/covered_area = 旧signal/A_cell, support 浮点
// ============================================================================
int aio_hips_write(
    const char* out_dir,
    uint32_t nside,
    uint32_t tile_width,
    const AioHipsTile* tiles,
    int n_tiles,
    int signal_dtype,
    const AioHipsSnrPoint* snr_points,
    int n_snr,
    const char* creator_did,
    const char* obs_title,
    int moc_order) {
    g_hips_error.clear();
    if (!out_dir || !tiles || n_tiles <= 0) { set_error("参数无效"); return -1; }
    AioHipsProductSet* ps = aio_hips_product_begin(
        out_dir, nside, tile_width, signal_dtype, AIO_HIPS_PRODUCT_ALL,
        creator_did, obs_title, nullptr, 0.0, nullptr, (uint32_t)moc_order);
    if (!ps) return -2;
    const double A_cell = 4.0 * kPi() / (12.0 * (double)nside * nside);
    std::vector<float> fluxF, areaF;
    std::vector<double> fluxD, areaD;
    for (int t = 0; t < n_tiles; ++t) {
        AstroSphereTileView view;
        std::memset(&view, 0, sizeof(view));
        view.parent_ipix = tiles[t].parent_ipix;
        view.leaf_order = ilog2_u64(nside);
        view.width = 512;
        view.data_type = signal_dtype;
        const size_t n = 512 * 512;
        if (signal_dtype == AIO_HIPS_FLOAT32) {
            fluxF.resize(n); areaF.resize(n);
            const float* sig = (const float*)tiles[t].signal;
            const uint8_t* su = tiles[t].support;
            for (size_t i = 0; i < n; ++i) {
                double sfrac = su ? su[i] / 255.0 : 1.0;
                fluxF[i] = (float)(sig[i] * sfrac);
                areaF[i] = (float)(sfrac * A_cell);
            }
            view.flux_sum = fluxF.data();
            view.covered_area = areaF.data();
        } else {
            fluxD.resize(n); areaD.resize(n);
            const double* sig = (const double*)tiles[t].signal;
            const uint8_t* su = tiles[t].support;
            for (size_t i = 0; i < n; ++i) {
                double sfrac = su ? su[i] / 255.0 : 1.0;
                fluxD[i] = sig[i] * sfrac;
                areaD[i] = sfrac * A_cell;
            }
            view.flux_sum = fluxD.data();
            view.covered_area = areaD.data();
        }
        int rc = aio_hips_write_signal_support_tile(ps, &view);
        if (rc != 0) {
            aio_hips_abort(ps);
            return rc;
        }
    }
    if (snr_points && n_snr > 0) {
        if (aio_hips_write_snr_points(ps, snr_points, n_snr) != 0) {
            aio_hips_abort(ps);
            return -6;
        }
    }
    return aio_hips_finalize(ps);
}

} // extern "C"
