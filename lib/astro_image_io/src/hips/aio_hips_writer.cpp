// ============================================================================
// aio_hips_writer.cpp - IVOA HiPS 1.4 生产链写入器 (Phase1 Final Closure )
//
// 数据流 (无 HISS 中转):
// Drizzle TileAccumulator -> AstroSphereTileView -> 本写入器 (流式)
//
// 输出结构 (Product Set):
// <out_dir>/
// manifest.json
// signal/ Image HiPS: signal = flux_sum/covered_area (float32/64)
// support/ Image HiPS: support = covered_area/A_cell (float32/64)
// snr/ Catalogue HiPS: NorderK/DirD/NpixN.tsv
// 每个子产品独立 properties / Moc.fits / 低阶 hierarchy tiles。
//
// FITS 全部由 vendored CFITSIO 4.6.4 写入 (含 DATASUM/CHECKSUM)。
// ============================================================================

#include "aio_hips.h"
#include "healpix/healpix_core.h"

#include <fitsio.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
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

// 解析 ISO-8601 "YYYY-MM-DDTHH:MM:SS"（可含小数秒）为 MJD（天文惯例，DATE-OBS 视为 UTC）。
// 日期换算采用固定纪元算法，不依赖本地时区；解析失败返回 false。
static int64_t civil_to_days(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static bool iso_to_mjd(const std::string& iso, double& mjd) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    double s = 0.0;
    if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%lf", &y, &mo, &d, &h, &mi, &s) != 6)
        return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59)
        return false;
    const int64_t days = civil_to_days(y, (unsigned)mo, (unsigned)d);
    mjd = (double)days + 40587.0
        + ((double)h * 3600.0 + (double)mi * 60.0 + s) / 86400.0;
    return true;
}


// 当前 UTC 时间 (finalize 时生成, 禁止硬编码日期)
void utc_now_iso(char* buf, size_t n) {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void utc_now_date(char* buf, size_t n) {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::snprintf(buf, n, "%04d-%02d-%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}
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
// data: 行主序数组 (NAXIS1 最快), naxis1 x naxis2
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
    // 方差传播分子 Σ v_j w_jp² (hierarchy 归约同叶级公式)
    std::vector<float>  sumVarF;
    std::vector<double> sumVarD;
    std::vector<uint32_t> count;
    bool is_f32 = true;

    void ensure(bool f32) {
        is_f32 = f32;
        const size_t n = 512 * 512;
        if (f32) {
            if (sumFluxF.empty()) sumFluxF.assign(n, 0.0f);
            if (sumAreaF.empty()) sumAreaF.assign(n, 0.0f);
            if (sumVarF.empty()) sumVarF.assign(n, 0.0f);
        } else {
            if (sumFluxD.empty()) sumFluxD.assign(n, 0.0);
            if (sumAreaD.empty()) sumAreaD.assign(n, 0.0);
            if (sumVarD.empty()) sumVarD.assign(n, 0.0);
        }
        if (count.empty()) count.assign(n, 0u);
    }
    void add(size_t i, double flux, double area) {
        if (is_f32) { sumFluxF[i] += (float)flux; sumAreaF[i] += (float)area; }
        else        { sumFluxD[i] += flux;        sumAreaD[i] += area; }
        ++count[i];
    }
    void add_var(size_t i, double var_num, double area) {
        (void)area;
        if (is_f32) sumVarF[i] += (float)var_num;
        else        sumVarD[i] += var_num;
    }
    double fluxAt(size_t i) const { return is_f32 ? (double)sumFluxF[i] : sumFluxD[i]; }
    double areaAt(size_t i) const { return is_f32 ? (double)sumAreaF[i] : sumAreaD[i]; }
    double varAt(size_t i) const  { return is_f32 ? (double)sumVarF[i] : sumVarD[i]; }
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
    // 跨 tile 累计 profile（低开销 coarse；每 tile 每段一次 clock）
    double prof_transform = 0.0;        // NESTED→FITS scatter
    double prof_fits_write = 0.0;       // CFITSIO tile 写出
    double prof_hierarchy_accum = 0.0;  // 每 tile ancestor 累加
    double prof_finalize_products = 0.0;
    double prof_hierarchy_write = 0.0;
    double prof_finalize_snr = 0.0;
    // 跨 tile 复用 scratch
    std::vector<float>  scratch_sigF, scratch_supF;   // dtype=f32 写缓冲
    std::vector<double> scratch_sigD, scratch_supD;   // dtype=f64 写缓冲
    std::vector<double> scratch_sig_n, scratch_sup_n; // NESTED 序缓存（hierarchy）
    // variance/ivar scratch
    std::vector<float>  scratch_varF, scratch_ivarF;
    std::vector<double> scratch_varD, scratch_ivarD;
    std::vector<double> scratch_var_n;                 // NESTED 序 var_num (hierarchy)
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
    // 只分配当前 dtype 的 scratch，跨 tile 复用（原每 tile
    // 分配 4×262144 元素 → 首 tile 分配后零再分配）
    std::vector<float>&  sigF = ps->scratch_sigF;
    std::vector<double>& sigD = ps->scratch_sigD;
    std::vector<float>&  supF = ps->scratch_supF;
    std::vector<double>& supD = ps->scratch_supD;
    if (f32) { sigF.resize(n); supF.resize(n); }
    else     { sigD.resize(n); supD.resize(n); }
    std::vector<double>& sig_n = ps->scratch_sig_n;
    std::vector<double>& sup_n = ps->scratch_sup_n;
    sig_n.resize(n);
    sup_n.resize(n);
    std::vector<uint8_t> valid;
    if (view->valid_mask)
        valid.assign((const uint8_t*)view->valid_mask, (const uint8_t*)view->valid_mask + n);

    // view->flux_sum/covered_area/valid_mask 以 NESTED local
    // 索引 (Drizzle 热路径保持 NESTED), 写 FITS 前经共享 HEALPix core 标准映射
    // scatter: fits_index = (tile_width-1-x)*tile_width + y, x/y 由 local 位解交错
    const auto t_tr0 = std::chrono::steady_clock::now();
    double tile_covered = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
            (uint64_t)i, 9u, 512u);
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
        // NESTED 序 sig/sup 缓存（与 FITS 序同一 float/double
        // 精度存储），hierarchy 直接按 NESTED 序累加，免 fi 反查。
        if (f32) {
            sigF[fi] = (float)sig; supF[fi] = (float)sup;
            sig_n[i] = (double)(float)sig; sup_n[i] = (double)(float)sup;
        } else {
            sigD[fi] = sig;        supD[fi] = sup;
            sig_n[i] = sig;        sup_n[i] = sup;
        }
    }

    ps->prof_transform += std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_tr0).count();

    // 2. 写 signal/support FITS (CFITSIO + checksum)
    const auto t_wr0 = std::chrono::steady_clock::now();
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
    ps->prof_fits_write += std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_wr0).count();

    // 3. MOC + 覆盖统计
    if (ps->moc_cells.insert(view->parent_ipix).second) {
        ps->leaf_ipix_list.push_back(view->parent_ipix);
        ps->moc_area_sr += 4.0 * kPi() / (12.0 * (1ULL << (2 * ps->tile_order)));
    }
    ps->covered_area_sr += tile_covered;

    // 4. hierarchy 累加 (k = K-1 .. 0)
    const auto t_ha0 = std::chrono::steady_clock::now();
    for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
        int dk = (int)ps->tile_order - k;
        uint64_t shift = 2ULL * (uint64_t)dk;
        uint64_t mask = (shift >= 64) ? ~0ULL : ((1ULL << shift) - 1ULL);
        uint64_t A = view->parent_ipix >> shift;
        uint64_t s = view->parent_ipix & mask;
        AncestorAcc& acc = ps->hier[(size_t)k][A];
        acc.ensure(f32);
        for (size_t i = 0; i < n; ++i) {
            // 直接使用 NESTED 序 sig/sup 缓存（与 FITS 序
            // 读回逐位一致），免每 i 一次 nested_local_to_fits_index 反查。
            const bool v = valid.empty() || valid[i];
            double flux = 0.0, area = 0.0;
            flux = sig_n[i] * sup_n[i] * ps->A_cell;
            area = sup_n[i] * ps->A_cell;
            if (!v || !(area > 0.0) || !std::isfinite(flux)) continue;
            // 叶 (P,l) -> A@k 内 order-(k+9) 单元 NESTED 索引
            // full = (s<<18)|l (order K+9 within A), z = full >> 2*(K-k)
            size_t z = (size_t)(((s << 18ULL) | (uint64_t)i) >>
                                (2ULL * (uint64_t)(ps->tile_order - (uint32_t)k)));
            acc.add(z, flux, area);
        }
    }
    ps->prof_hierarchy_accum += std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_ha0).count();
    return 0;
}

// ============================================================================
// variance/ivar 叶级 Tile 写
// variance = var_num_sum / covered_area² ; ivar = 1/variance
// covered_area<=0 -> NaN (与 signal NaN 语义一致)
// hierarchy: 在 AncestorAcc 增加 var_num 通道, 归约公式与叶级一致
// (variance_parent = Σvar_num / (Σarea)²)
// ============================================================================
int aio_hips_write_variance_tile(AioHipsProductSet* ps,
                                 const AstroSphereTileView* view) {
    g_hips_error.clear();
    if (!ps || !view) { set_error("null handle/view"); return -1; }
    if (!view->var_num_sum) { set_error("var_num_sum 为空 (无方差数据)"); return -2; }
    if (view->width != 512 || view->leaf_order != ps->leaf_order ||
        view->data_type != ps->data_type) {
        set_error("view 与产品集不匹配 (width=512, leaf_order/dtype 必须一致)");
        return -3;
    }
    const uint64_t npix_order = 12ULL * (1ULL << (2ULL * ps->tile_order));
    if (view->parent_ipix >= npix_order) {
        set_error("parent_ipix 超出 Norder" + std::to_string(ps->tile_order) + " 范围");
        return -4;
    }
    const size_t n = 512 * 512;
    const bool f32 = (ps->data_type == AIO_HIPS_FLOAT32);

    std::vector<float>&  varF  = ps->scratch_varF;
    std::vector<double>& varD  = ps->scratch_varD;
    std::vector<float>&  ivarF = ps->scratch_ivarF;
    std::vector<double>& ivarD = ps->scratch_ivarD;
    if (f32) { varF.resize(n); ivarF.resize(n); }
    else     { varD.resize(n); ivarD.resize(n); }
    std::vector<double>& var_n = ps->scratch_var_n;
    var_n.resize(n);
    std::vector<uint8_t> valid;
    if (view->valid_mask)
        valid.assign((const uint8_t*)view->valid_mask,
                     (const uint8_t*)view->valid_mask + n);

    bool any_valid = false;
    for (size_t i = 0; i < n; ++i) {
        const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
            (uint64_t)i, 9u, 512u);
        const bool v = valid.empty() || valid[i];
        double vnum = 0.0, area = 0.0;
        if (f32) {
            if (view->var_num_sum) vnum = (double)((const float*)view->var_num_sum)[i];
            if (view->covered_area) area = (double)((const float*)view->covered_area)[i];
        } else {
            if (view->var_num_sum) vnum = ((const double*)view->var_num_sum)[i];
            if (view->covered_area) area = ((const double*)view->covered_area)[i];
        }
        double var = std::numeric_limits<double>::quiet_NaN();
        double iv = std::numeric_limits<double>::quiet_NaN();
        if (v && area > 0.0 && vnum > 0.0 && std::isfinite(area) && std::isfinite(vnum)) {
            var = vnum / (area * area);
            iv = 1.0 / var;
            any_valid = true;
        }
        if (f32) {
            varF[fi] = (float)var;  ivarF[fi] = (float)iv;
            var_n[i] = (v && area > 0.0 && vnum > 0.0) ? vnum : 0.0;
        } else {
            varD[fi] = var;         ivarD[fi] = iv;
            var_n[i] = (v && area > 0.0 && vnum > 0.0) ? vnum : 0.0;
        }
    }
    if (!any_valid) {
        set_error("该 tile 无有效方差数据 (var_num_sum 全 0)");
        return -5;
    }

    const int bitpix = f32 ? -32 : -64;
    std::vector<std::pair<std::string, std::string>> cards;
    cards.push_back({"NSIDE", std::to_string(ps->nside)});
    cards.push_back({"FIRSTPIX", "0"});
    cards.push_back({"LASTPIX", std::to_string(n - 1)});
    std::string rel = tile_rel_path((int)ps->tile_order, view->parent_ipix, ".fits");
    if (ps->flags & AIO_HIPS_PRODUCT_VARIANCE) {
        std::string p = ps->out_dir + "/variance/" + rel;
        make_dirs(p.substr(0, p.find_last_of('/')));
        if (!write_fits_image(p, bitpix, 512, 512,
                              f32 ? (const void*)varF.data() : (const void*)varD.data(),
                              cards, ps->obs_title, ps->obs_filter,
                              ps->exposure, ps->obs_date)) {
            return -6;
        }
    }
    if (ps->flags & AIO_HIPS_PRODUCT_IVAR) {
        std::string p = ps->out_dir + "/ivar/" + rel;
        make_dirs(p.substr(0, p.find_last_of('/')));
        if (!write_fits_image(p, bitpix, 512, 512,
                              f32 ? (const void*)ivarF.data() : (const void*)ivarD.data(),
                              cards, ps->obs_title, ps->obs_filter,
                              ps->exposure, ps->obs_date)) {
            return -7;
        }
    }

    // MOC/覆盖: 与 signal/support 共享 (variance tile 必伴随 signal tile,
    // MOC 已在 write_signal_support_tile 登记, 不重复)

    // hierarchy: 累加 var_num (归约公式同叶级: var_parent = Σvar_num/(Σarea)²)
    for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
        int dk = (int)ps->tile_order - k;
        uint64_t shift = 2ULL * (uint64_t)dk;
        uint64_t mask = (shift >= 64) ? ~0ULL : ((1ULL << shift) - 1ULL);
        uint64_t A = view->parent_ipix >> shift;
        uint64_t s = view->parent_ipix & mask;
        AncestorAcc& acc = ps->hier[(size_t)k][A];
        acc.ensure(f32);
        for (size_t i = 0; i < n; ++i) {
            if (var_n[i] <= 0.0) continue;
            size_t z = (size_t)(((s << 18ULL) | (uint64_t)i) >>
                                (2ULL * (uint64_t)(ps->tile_order - (uint32_t)k)));
            acc.add_var(z, var_n[i], 0.0);
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
    kv.push_back({"hips_status", "private master"});
    kv.push_back({"hips_creator", "AstroCS (astro_image_io)"});
    kv.push_back({"hips_builder", "AstroCS aio_hips_writer (CFITSIO 4.6.4)"});
    kv.push_back({"hips_estsize", "1000000"});
    // META-001 : 真实 UTC finalize 时间, 禁止硬编码日期
    char rel_date[32], cre_date[40];
    utc_now_date(rel_date, sizeof(rel_date));
    utc_now_iso(cre_date, sizeof(cre_date));
    kv.push_back({"hips_release_date", rel_date});
    kv.push_back({"hips_creation_date", cre_date});
    kv.push_back({"obs_description", "AstroCS Phase1 single-frame HiPS product"});
    kv.push_back({"prov_progenitor", "ivo://astrocs/phase1/drizzle"});
    kv.push_back({"obs_regime", "optical"});
    // META-002 : 无真实 passband/系统响应波长范围时不伪造 em_min/em_max
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
    if (!ps->obs_date.empty()) {
        double t0 = 0.0;
        if (iso_to_mjd(ps->obs_date, t0)) {
            char b0[32], b1[32];
            std::snprintf(b0, sizeof(b0), "%.8f", t0);
            std::snprintf(b1, sizeof(b1), "%.8f", t0 + ps->exposure / 86400.0);
            kv.push_back({"t_min", b0});
            kv.push_back({"t_max", b1});
        }
    }
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
            // AncestorAcc 以 NESTED local 索引累加,
            // 写出低阶 hierarchy FITS 时同样 scatter 到标准 HiPS 行主序
            for (size_t i = 0; i < n; ++i) {
                const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
                    (uint64_t)i, 9u, 512u);
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
                if (bitpix == -32) { sigF[fi] = (float)sig; supF[fi] = (float)sup; }
                else               { sigD[fi] = sig;        supD[fi] = sup; }
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
            // variance/ivar hierarchy (归约公式同叶级)
            if ((ps->flags & (AIO_HIPS_PRODUCT_VARIANCE |
                              AIO_HIPS_PRODUCT_IVAR)) != 0) {
                std::vector<float>  varF(n), ivarF(n);
                std::vector<double> varD(n), ivarD(n);
                for (size_t i = 0; i < n; ++i) {
                    const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
                        (uint64_t)i, 9u, 512u);
                    const double area = acc.areaAt(i);
                    const double vnum = acc.varAt(i);
                    double var = std::numeric_limits<double>::quiet_NaN();
                    double iv = std::numeric_limits<double>::quiet_NaN();
                    if (area > 0.0 && vnum > 0.0 && std::isfinite(area) && std::isfinite(vnum)) {
                        var = vnum / (area * area);
                        iv = 1.0 / var;
                    }
                    if (bitpix == -32) { varF[fi] = (float)var; ivarF[fi] = (float)iv; }
                    else               { varD[fi] = var;        ivarD[fi] = iv; }
                }
                if (ps->flags & AIO_HIPS_PRODUCT_VARIANCE) {
                    std::string p = ps->out_dir + "/variance/" + rel;
                    make_dirs(p.substr(0, p.find_last_of('/')));
                    if (!write_fits_image(p, bitpix, 512, 512,
                                          bitpix == -32 ? (const void*)varF.data()
                                                        : (const void*)varD.data(),
                                          cards, ps->obs_title, ps->obs_filter,
                                          ps->exposure, ps->obs_date))
                        return false;
                }
                if (ps->flags & AIO_HIPS_PRODUCT_IVAR) {
                    std::string p = ps->out_dir + "/ivar/" + rel;
                    make_dirs(p.substr(0, p.find_last_of('/')));
                    if (!write_fits_image(p, bitpix, 512, 512,
                                          bitpix == -32 ? (const void*)ivarF.data()
                                                        : (const void*)ivarD.data(),
                                          cards, ps->obs_title, ps->obs_filter,
                                          ps->exposure, ps->obs_date))
                        return false;
                }
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
        // 共享 HEALPix core : 不再维护 AIO 私有 ang2ipix
        uint64_t ip = astrocs::healpix::ang2pix_nest(1u << ps->tile_order,
                                                     p.ra_deg, p.dec_deg);
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
        // SNR-PREC-001 : FP32 -> %.9g (float32 round-trip),
        // FP64 -> %.17g (float64 round-trip), 不再使用 %.6f
        const char* snr_fmt = (ps->data_type == AIO_HIPS_FLOAT32) ? "%.9g" : "%.17g";
        char line_fmt[64];
        std::snprintf(line_fmt, sizeof(line_fmt), "%%lld %%.12f %%.12f %s %%u %%u\n", snr_fmt);
        for (const AioHipsSnrPoint* sp : kv.second) {
            // 真实 star_id / quality_flags / photometric_status (禁止硬编码)
            std::fprintf(f, line_fmt,
                         (long long)sp->star_id, sp->ra_deg, sp->dec_deg, sp->snr,
                         sp->quality_flags, sp->photometric_status);
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
    kv2.push_back({"hips_status", "private master"});
    kv2.push_back({"hips_creator", "AstroCS (astro_image_io)"});
    kv2.push_back({"hips_builder", "AstroCS aio_hips_writer (CFITSIO 4.6.4)"});
    // META-001 : 真实 UTC finalize 时间, 禁止硬编码日期
    char rel_date2[32], cre_date2[40];
    utc_now_date(rel_date2, sizeof(rel_date2));
    utc_now_iso(cre_date2, sizeof(cre_date2));
    kv2.push_back({"hips_release_date", rel_date2});
    kv2.push_back({"hips_creation_date", cre_date2});
    kv2.push_back({"obs_description", "AstroCS Phase1 single-frame SNR catalogue HiPS product"});
    kv2.push_back({"prov_progenitor", "ivo://astrocs/phase1/drizzle"});
    kv2.push_back({"obs_regime", "optical"});
    // META-002 : 无真实 passband/系统响应波长范围时不伪造 em_min/em_max
    if (!ps->obs_date.empty()) {
        double t0 = 0.0;
        if (iso_to_mjd(ps->obs_date, t0)) {
            char b0[32], b1[32];
            std::snprintf(b0, sizeof(b0), "%.8f", t0);
            std::snprintf(b1, sizeof(b1), "%.8f", t0 + ps->exposure / 86400.0);
            kv2.push_back({"t_min", b0});
            kv2.push_back({"t_max", b1});
        }
    }
    kv2.push_back({"hips_initial_fov", "60"});
    kv2.push_back({"moc_sky_fraction",
        std::to_string((double)cells.size() * 4.0 * kPi() /
                       (12.0 * (1ULL << (2ULL * ps->tile_order))) / (4.0 * kPi()))});
    kv2.push_back({"hips_cat_nrows", std::to_string(ps->snr.size())});
    // hips_initial_ra/dec: 由真实 SNR 源位置中位数推导（单帧场中心近似，非伪造）
    if (!ps->snr.empty()) {
        std::vector<double> ra_s, dec_s;
        ra_s.reserve(ps->snr.size());
        dec_s.reserve(ps->snr.size());
        for (const auto& p : ps->snr) {
            ra_s.push_back(p.ra_deg);
            dec_s.push_back(p.dec_deg);
        }
        std::sort(ra_s.begin(), ra_s.end());
        std::sort(dec_s.begin(), dec_s.end());
        kv2.push_back({"hips_initial_ra", std::to_string(ra_s[ra_s.size() / 2])});
        kv2.push_back({"hips_initial_dec", std::to_string(dec_s[dec_s.size() / 2])});
    }
    write_properties(dir + "/properties", kv2);
    {
        FILE* f = std::fopen((dir + "/metadata.xml").c_str(), "wb");
        if (!f) { set_error("无法创建 SNR metadata.xml: " + dir + "/metadata.xml"); return false; }
        // IVOA HiPS Catalog: metadata.xml 必须是 VOTable（Hipsgen LINT[4.4.3] 要求根元素 votable）
        std::fprintf(f,
            "<?xml version=\"1.0\"?>\n"
            "<VOTABLE version=\"1.3\" xmlns=\"http:// www.ivoa.net/xml/VOTable/v1.3\"\n"
            "         xmlns:xsi=\"http:// www.w3.org/2001/XMLSchema-instance\"\n"
            "         xsi:schemaLocation=\"http:// www.ivoa.net/xml/VOTable/v1.3 http:// www.ivoa.net/xml/VOTable/v1.3\">\n"
            "  <RESOURCE type=\"meta\">\n"
            "    <TABLE>\n"
            "      <FIELD name=\"star_id\" datatype=\"long\" ucd=\"meta.id\"/>\n"
            "      <FIELD name=\"ra\" datatype=\"double\" unit=\"deg\" ucd=\"pos.eq.ra\"/>\n"
            "      <FIELD name=\"dec\" datatype=\"double\" unit=\"deg\" ucd=\"pos.eq.dec\"/>\n"
            "      <FIELD name=\"snr\" datatype=\"%s\" ucd=\"stat.snr\"/>\n"
            "      <FIELD name=\"quality_flags\" datatype=\"int\" ucd=\"meta.code.qual\"/>\n"
            "      <FIELD name=\"photometric_status\" datatype=\"int\" ucd=\"meta.code.status\"/>\n"
            "    </TABLE>\n"
            "  </RESOURCE>\n"
            "</VOTABLE>\n",
            ps->data_type == AIO_HIPS_FLOAT32 ? "float" : "double");
        std::fclose(f);
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
    // finalize 分段计时（粗粒度，低开销）
    const auto t_fin0 = std::chrono::steady_clock::now();
    std::fprintf(stderr, "[hips] finalize: n_leaf=%zu flags=%d\n",
                 ps->leaf_ipix_list.size(), ps->flags);
    const auto t_p0 = std::chrono::steady_clock::now();
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
    // variance/ivar 产品 (Drizzle 方差传播)
    if (ps->flags & AIO_HIPS_PRODUCT_VARIANCE) {
        std::fprintf(stderr, "[hips] finalize: variance product\n");
        if (!finalize_image_product(ps, "variance", "variance", "", moc_frac, cov_frac)) {
            return -7;
        }
    }
    if (ps->flags & AIO_HIPS_PRODUCT_IVAR) {
        std::fprintf(stderr, "[hips] finalize: ivar product\n");
        if (!finalize_image_product(ps, "ivar", "inverse variance", "", moc_frac, cov_frac)) {
            return -8;
        }
    }
    ps->prof_finalize_products += std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_p0).count();
    const auto t_h0 = std::chrono::steady_clock::now();
    if ((ps->flags & (AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
                      AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR)) &&
        !finalize_hierarchy(ps)) {
        std::fprintf(stderr, "[hips] finalize: hierarchy failed\n");
        return -5;
    }
    ps->prof_hierarchy_write += std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_h0).count();
    const auto t_s0 = std::chrono::steady_clock::now();
    if ((ps->flags & AIO_HIPS_PRODUCT_SNR) && !finalize_snr_product(ps)) {
        std::fprintf(stderr, "[hips] finalize: snr failed\n");
        return -6;
    }
    ps->prof_finalize_snr += std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_s0).count();
    std::fprintf(stderr, "[hips] finalize: ok\n");
    std::fprintf(stderr,
                 "[hips][profile] transform=%.3fs fits_write=%.3fs "
                 "hierarchy_accum=%.3fs products=%.3fs hierarchy_write=%.3fs "
                 "snr=%.3fs total=%.3fs\n",
                 ps->prof_transform, ps->prof_fits_write,
                 ps->prof_hierarchy_accum, ps->prof_finalize_products,
                 ps->prof_hierarchy_write, ps->prof_finalize_snr,
                 std::chrono::duration<double>(
                     std::chrono::steady_clock::now() - t_fin0).count());
    // manifest.json
    {
        FILE* f = std::fopen((ps->out_dir + "/manifest.json").c_str(), "wb");
        if (f) {
            std::string prod_list;
            struct { int flag; const char* name; } prods[] = {
                {AIO_HIPS_PRODUCT_SIGNAL, "signal"},
                {AIO_HIPS_PRODUCT_SUPPORT, "support"},
                {AIO_HIPS_PRODUCT_VARIANCE, "variance"},
                {AIO_HIPS_PRODUCT_IVAR, "ivar"},
                {AIO_HIPS_PRODUCT_SNR, "snr"},
            };
            bool first = true;
            for (const auto& p : prods) {
                if (ps->flags & p.flag) {
                    if (!first) prod_list += ", ";
                    prod_list += "\"";
                    prod_list += p.name;
                    prod_list += "\"";
                    first = false;
                }
            }
            std::fprintf(f,
                "{\n"
                "  \"format_version\": 1,\n"
                "  \"hips_version\": \"1.4\",\n"
                "  \"nside\": %u,\n"
                "  \"tile_width\": %u,\n"
                "  \"data_type\": \"%s\",\n"
                "  \"products\": [%s],\n"
                "  \"n_leaf_tiles\": %zu,\n"
                "  \"moc_sky_fraction\": %.8f,\n"
                "  \"astrocs_covered_sky_fraction\": %.8f,\n"
                "  \"signal_dtype\": \"%s\"\n"
                "}\n",
                ps->nside, ps->tile_width,
                ps->data_type == AIO_HIPS_FLOAT32 ? "float32" : "float64",
                prod_list.c_str(),
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
// 旧语义: signal = F/support_frac, support uint8 (0..255)
// 转换: covered_area = su/255*A_cell, flux_sum = signal*(su/255)
// 新语义: signal = flux_sum/covered_area = 旧signal/A_cell, support 浮点
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












