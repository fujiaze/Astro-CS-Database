// P1-HIPS-TEST · 独立 oracle (不调用被测函数, 不复制同一实现)
//
// 控制包任务: P1-HIPS-TEST; 合同锚 HIPS_WRITER.md §9 TEST-HIPS-DESIGN-001:
// "oracle 独立朴素实现 (不调用生产 symbol, 不复制源码公式) —— 可用解析解
// 或许可隔离的测试参考库"。本文件:
//   1) FITS 排列: 独立位解交织 (LSB 起 bit2i→x, bit2i+1→y; 标准 NESTED
//      定义) + DATA_SEMANTICS §3 闭式 fits_index=(511−x)·512+y
//   2) 叶级/层次期望值: 由 §9 冻结不变量 I1..I4 的解析式独立复算
//   3) MOC UNIQ: IVOA MOC 1.1 标准定义 uniq = 4·4^order + (ipix>>2Δ) 独立式
//   4) 产物读回解析器 (properties/manifest/TSV + vendored CFITSIO 只读
//      访问 tile FITS / Moc.fits —— CFITSIO 为许可隔离的第三方参考库)
//   5) SNR cell 归属弱 oracle: 独立球面角距离 + cell 角尺度解析上界
#ifndef P1HIPS_ORACLE_HPP
#define P1HIPS_ORACLE_HPP

#include <fitsio.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace p1hips {
namespace oracle {

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// 1) 独立 NESTED local ↔ (x,y) 位解交织 (标准 NESTED: 从 LSB 起
//    bit(2i)→x, bit(2i+1)→y) 与 DATA_SEMANTICS §3 FITS 行主序闭式
// ---------------------------------------------------------------------------
inline void local_to_xy(std::uint64_t local, std::uint32_t shift,
                        std::uint32_t& x, std::uint32_t& y) {
    x = 0;
    y = 0;
    for (std::uint32_t i = 0; i < shift; ++i) {
        x |= (std::uint32_t)((local >> (2u * i)) & 1ULL) << i;
        y |= (std::uint32_t)((local >> (2u * i + 1u)) & 1ULL) << i;
    }
}

inline std::uint64_t xy_to_local(std::uint32_t x, std::uint32_t y, std::uint32_t shift) {
    std::uint64_t out = 0;
    for (std::uint32_t i = 0; i < shift; ++i) {
        out |= (std::uint64_t)((x >> i) & 1u) << (2u * i);
        out |= (std::uint64_t)((y >> i) & 1u) << (2u * i + 1u);
    }
    return out;
}

// DATA_SEMANTICS §3: FITS 行 = 511−x, 列 = y, 行主序 = (511−x)·512 + y
inline std::uint64_t local_to_fits_index(std::uint64_t local,
                                         std::uint32_t tile_width = 512) {
    std::uint32_t x = 0, y = 0;
    local_to_xy(local, 9u, x, y);
    const std::uint32_t maxv = tile_width - 1u;
    return (std::uint64_t)(maxv - x) * tile_width + y;
}

inline std::uint64_t fits_index_to_local(std::uint64_t fits_index,
                                         std::uint32_t tile_width = 512) {
    const std::uint32_t row = (std::uint32_t)(fits_index / tile_width);
    const std::uint32_t col = (std::uint32_t)(fits_index % tile_width);
    const std::uint32_t maxv = tile_width - 1u;
    const std::uint32_t x = maxv - row;
    const std::uint32_t y = col;
    return xy_to_local(x, y, 9u);
}

// ---------------------------------------------------------------------------
// 2) 叶级期望值 (§9 I1/I2/I3/I4 解析式; 独立复算, 不经被测函数)
// ---------------------------------------------------------------------------
inline double leaf_signal(double flux, double area, bool valid) {
    if (!valid || !(area > 0.0) || !std::isfinite(flux) || !std::isfinite(area))
        return std::numeric_limits<double>::quiet_NaN();
    return flux / area;   // I1: signal = flux_sum / covered_area
}

inline double leaf_support(double area, double a_cell, bool valid) {
    if (!valid) return 0.0;   // I3: invalid → support=0
    if (!(area > 0.0)) return 0.0;
    double s = area / a_cell; // I2: support = covered_area / A_cell
    if (s > 1.0) s = 1.0;     // I2: clamp ≤ 1
    return s;
}

inline double leaf_variance(double var_num, double area, bool valid) {
    if (!valid || !(area > 0.0) || !(var_num > 0.0) ||
        !std::isfinite(area) || !std::isfinite(var_num))
        return std::numeric_limits<double>::quiet_NaN();
    return var_num / (area * area);   // I4: variance = var_num / area²
}

inline double leaf_ivar(double var_num, double area, bool valid) {
    const double v = leaf_variance(var_num, area, valid);
    if (std::isnan(v)) return std::numeric_limits<double>::quiet_NaN();
    return 1.0 / v;   // I4: ivar = 1/variance (有限域互倒)
}

// hierarchy 聚合期望 (I7): 父 pixel sig = Σ子flux / Σ子area (子域由
// sig·sup·A_cell 重构), sup = Σ子area / A_cell_k, 含 ≤1 clamp。
// 子像素输入取"写叶 tile 时存进 NESTED 缓存的 double 值"(§9 冻结口径:
// f64 通路 bitwise, f32 通路 rtol=1e-6, DISP-HIPS-009 累加器漂移界)。
inline void hierarchy_pixel_expect(double sig_child, double sup_child,
                                   double a_cell_leaf, double a_cell_k,
                                   double& sig_parent, double& sup_parent) {
    const double flux = sig_child * sup_child * a_cell_leaf;
    const double area = sup_child * a_cell_leaf;
    if (area > 0.0 && std::isfinite(flux)) {
        sig_parent = flux / area;
        sup_parent = area / a_cell_k;
        if (sup_parent > 1.0) sup_parent = 1.0;
    } else {
        sig_parent = std::numeric_limits<double>::quiet_NaN();
        sup_parent = 0.0;
    }
}

// MOC UNIQ (IVOA MOC 1.1 §2.3): uniq = 4·4^order + ipix@order (由 order-K
// cell 右移 2Δ 位降阶)
inline std::uint64_t moc_uniq(std::uint64_t order_k_ipix,
                              std::uint32_t order_k, std::uint32_t order_m) {
    const std::uint64_t uniq_base = 4ULL << (2ULL * order_m);
    return uniq_base + (order_k_ipix >> (2ULL * (order_k - order_m)));
}

inline double moc_cell_area_sr(std::uint32_t order) {
    return 4.0 * kPi / (12.0 * std::pow(4.0, (double)order));
}

// SNR cell 角尺度解析上界 (度): order-K cell 面积 4π/(12·4^K) 的
// 外接圆上界 = sqrt(2)·sqrt(A) (正方形对角保守界)。
inline double snr_cell_diameter_deg_ub(std::uint32_t order_k) {
    const double area_sr = moc_cell_area_sr(order_k);
    return std::sqrt(2.0) * std::sqrt(area_sr) * 180.0 / kPi;
}

// 独立球面角距离 (度) —— 球面余弦式, 与生产 angular_distance 独立
inline double ang_dist_deg(double ra1, double dec1, double ra2, double dec2) {
    const double d1 = dec1 * kPi / 180.0, d2 = dec2 * kPi / 180.0;
    const double dr = (ra2 - ra1) * kPi / 180.0;
    const double c = std::sin(d1) * std::sin(d2) +
                     std::cos(d1) * std::cos(d2) * std::cos(dr);
    return std::acos(std::max(-1.0, std::min(1.0, c))) * 180.0 / kPi;
}

// ---------------------------------------------------------------------------
// 4) 产物读回解析器
// ---------------------------------------------------------------------------
inline std::map<std::string, std::string> read_properties(const std::string& path) {
    std::map<std::string, std::string> kv;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return kv;
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        const std::size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        kv[s.substr(0, eq)] = s.substr(eq + 1);
    }
    std::fclose(f);
    return kv;
}

// manifest.json 极简字段提取 (字符串/数值字面量)
inline bool manifest_get(const std::string& text, const std::string& key,
                         std::string& value) {
    const std::string pat = "\"" + key + "\":";
    const std::size_t p = text.find(pat);
    if (p == std::string::npos) return false;
    std::size_t v = p + pat.size();
    while (v < text.size() && (text[v] == ' ' || text[v] == '\t')) ++v;
    if (v >= text.size()) return false;
    if (text[v] == '"') {
        const std::size_t e = text.find('"', v + 1);
        if (e == std::string::npos) return false;
        value = text.substr(v + 1, e - v - 1);
        return true;
    }
    std::size_t e = v;
    while (e < text.size() && text[e] != ',' && text[e] != '\n' && text[e] != '}') ++e;
    value = text.substr(v, e - v);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\r')) value.pop_back();
    return true;
}

inline bool read_manifest(const std::string& path, std::string& text) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[4096];
    std::size_t n;
    text.clear();
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    std::fclose(f);
    return true;
}

// SNR TSV tile 读回 (跳过 # 注释行; 期望 6 列)
struct SnrRow {
    long long star_id = 0;
    double ra = 0.0, dec = 0.0, snr = 0.0;
    unsigned long long qflags = 0, pstatus = 0;
};

inline bool read_snr_tsv(const std::string& path, std::vector<SnrRow>& rows) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char line[512];
    rows.clear();
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        SnrRow r{};
        if (std::sscanf(line, "%lld %lf %lf %lf %llu %llu", &r.star_id, &r.ra,
                        &r.dec, &r.snr, &r.qflags, &r.pstatus) == 6)
            rows.push_back(r);
    }
    std::fclose(f);
    return true;
}

// tile FITS 只读 (vendored CFITSIO; oracle 只读参考通道)
// 返回 false = 打开/读失败; bitpix=-32 → float 模式, -64 → double 模式
struct TileFits {
    int bitpix = 0;
    long naxis1 = 0, naxis2 = 0;
    std::vector<float> pix_f;
    std::vector<double> pix_d;
    std::map<std::string, std::string> keys;
};

inline bool read_tile_fits(const std::string& path, TileFits& out) {
    fitsfile* fptr = nullptr;
    int status = 0;
    if (fits_open_file(&fptr, path.c_str(), READONLY, &status)) {
        fits_report_error(stderr, status);
        return false;
    }
    int bitpix = 0, naxis = 0;
    long naxes[2] = {0, 0};
    if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
        fits_close_file(fptr, &status);
        return false;
    }
    out.bitpix = bitpix;
    out.naxis1 = naxes[0];
    out.naxis2 = naxes[1];
    const long n = naxes[0] * naxes[1];
    char key[9], val[80];
    const char* wanted[] = {"PIXTYPE", "ORDERING", "COORDSYS", "OBJECT", "FILTER",
                            "DATE-OBS", "NSIDE", "FIRSTPIX", "LASTPIX"};
    for (const char* k : wanted) {
        val[0] = '\0';
        std::snprintf(key, sizeof(key), "%s", k);
        if (fits_read_key_str(fptr, key, val, nullptr, &status) == 0)
            out.keys[k] = val;
        else
            status = 0;
    }
    long fpixel[2] = {1, 1};
    bool ok = true;
    if (bitpix == -32) {
        out.pix_f.assign((std::size_t)n, 0.0f);
        fits_read_pix(fptr, TFLOAT, fpixel, n, nullptr, out.pix_f.data(), nullptr, &status);
        ok = (status == 0);
    } else if (bitpix == -64) {
        out.pix_d.assign((std::size_t)n, 0.0);
        fits_read_pix(fptr, TDOUBLE, fpixel, n, nullptr, out.pix_d.data(), nullptr, &status);
        ok = (status == 0);
    } else {
        ok = false;
    }
    fits_close_file(fptr, &status);
    return ok;
}

// Moc.fits UNIQ 列只读 (BINTABLE TFORM='K')
inline bool read_moc_uniq(const std::string& path, std::vector<std::uint64_t>& uniq) {
    fitsfile* fptr = nullptr;
    int status = 0;
    if (fits_open_file(&fptr, path.c_str(), READONLY, &status)) {
        fits_close_file(fptr, &status);
        return false;
    }
    // MOC 数据在 HDU2 (binary table); 主 HDU 为空头
    int hdutype = 0;
    if (fits_movabs_hdu(fptr, 2, &hdutype, &status)) {
        fits_close_file(fptr, &status);
        return false;
    }
    long nrows = 0;
    if (fits_get_num_rows(fptr, &nrows, &status)) {
        fits_close_file(fptr, &status);
        return false;
    }
    std::vector<long long> col((std::size_t)nrows);
    if (nrows > 0 &&
        fits_read_col(fptr, TLONGLONG, 1, 1, 1, nrows, nullptr, col.data(),
                      nullptr, &status)) {
        fits_close_file(fptr, &status);
        return false;
    }
    uniq.assign(col.begin(), col.end());
    fits_close_file(fptr, &status);
    return true;
}

// Moc.fits MOCORDER 头键
inline bool read_moc_order(const std::string& path, long& moc_order) {
    fitsfile* fptr = nullptr;
    int status = 0;
    if (fits_open_file(&fptr, path.c_str(), READONLY, &status)) {
        fits_close_file(fptr, &status);
        return false;
    }
    int hdutype_m = 0;
    if (fits_movabs_hdu(fptr, 2, &hdutype_m, &status)) {
        fits_close_file(fptr, &status);
        return false;
    }
    const bool ok = (fits_read_key_lng(fptr, "MOCORDER", &moc_order, nullptr, &status) == 0);
    fits_close_file(fptr, &status);
    return ok;
}

// ---------------------------------------------------------------------------
// 5) 临时目录工具 (fixture 不落仓库大二进制; 每用例独立 mkdtemp)
// ---------------------------------------------------------------------------
inline std::string make_tmp_dir(const char* tag) {
    std::string tpl = std::string("/tmp/p1hips_") + tag + "_XXXXXX";
    std::vector<char> buf(tpl.begin(), tpl.end());
    buf.push_back('\0');
    if (!mkdtemp(buf.data())) return std::string();
    return std::string(buf.data());
}

}  // namespace oracle
}  // namespace p1hips

#endif  // P1HIPS_ORACLE_HPP
