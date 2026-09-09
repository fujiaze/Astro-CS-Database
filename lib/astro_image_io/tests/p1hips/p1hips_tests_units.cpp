// P1-HIPS-TEST · units 组 — F1/F6 正常路径 + 解析期望 + 头键
// + SNR round-trip (I12) + manifest 计数 (I8)
//
// 控制包任务: P1-HIPS-TEST; 合同锚 HIPS_WRITER.md §9 TEST-HIPS-DESIGN-001
// (P1-HIPS-DOC 冻结)。故障注入: P1HIPS_CHECK 第三参 faultname
// (见 p1hips_test_main.hpp)。
#include "p1hips_test_main.hpp"
#include "p1hips_fixtures.hpp"
#include "p1hips_oracle.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "aio_hips.h"

using namespace p1hips::oracle;

namespace p1hips {

// ---------------------------------------------------------------------------
// POSIX 目录树助手 (selfcheck/properties 链引用)。UTC 时间戳合同上不跨运行
// 复现: cfitsio finalize 更新 CHECKSUM/DATASUM 时把 UTC 时间戳写进卡片注释
// ("/ HDU checksum updated <UTC>"), 故 fnv_file 对 .fits/.fts 做 FITS 头区
// 归一化 (P1-HIPS-TEST tree_digest flaky 修正)。
// 归一化规则: 对 FITS 头区 (文件开头到 END 卡, 80 字节对齐) 逐卡扫描, 凡
// 卡片名 (前 8 字节) 为 CHECKSUM 或 DATASUM 的卡, 第 11..80 列 (value+注释
// 区) 清零后参与哈希 —— 仅清注释区 (31..80) 不彻底: CHECKSUM 的 value 是
// 头区 32-bit 补码和的 ASCII 编码 (ffesum), 而 DATASUM 注释内嵌 UTC 时间
// 戳, 跨秒 CHECKSUM value 必变 (cfitsio 4.6.4 ffcsum/ffesum 对真实 tile
// 移植复现实证)。数据区与其余头卡照常全字节哈希; 头区解析失败 (超 360 卡
// 无 END / 头卡区含非可打印字节) 时整体退回原全字节哈希并留日志; 非 FITS
// 文件保持原流式路径。
// ---------------------------------------------------------------------------
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

inline std::uint64_t fnv_byte(std::uint64_t h, unsigned char c) {
    return (h ^ (std::uint64_t)c) * kFnvPrime;
}

// 小写扩展名 (含点); 无扩展名返回空
std::string lower_ext(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string e = path.substr(dot);
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

// 读整个文件; 成功返回 true (空文件也是成功)
bool read_file_bytes(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    if (n < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize((std::size_t)n);
    if (n > 0) f.read(reinterpret_cast<char*>(out.data()), n);
    return (std::size_t)f.gcount() == (std::size_t)n;
}

// FITS 头区扫描: 返回 END 卡结束 offset (含, 80 对齐); 解析失败返回 0。
// 80 字节对齐逐卡, 上限 360 卡 (FITS 头区常规上限); 头卡区必须全为
// 可打印 ASCII, END 卡 = "END" + 空格填满 80 列。
std::size_t fits_header_end(const std::vector<unsigned char>& b) {
    const std::size_t kMaxCards = 360;
    const std::size_t n = b.size();
    for (std::size_t off = 0, k = 0; off + 80 <= n && k < kMaxCards; off += 80, ++k) {
        for (std::size_t i = off; i < off + 80; ++i) {
            const unsigned char c = b[i];
            if (c < 0x20u || c > 0x7eu) return 0;
        }
        if (b[off] == (unsigned char)'E' && b[off + 1] == (unsigned char)'N' &&
            b[off + 2] == (unsigned char)'D') {
            bool pad_ok = true;
            for (std::size_t i = 3; i < 80; ++i)
                if (b[off + i] != (unsigned char)' ') { pad_ok = false; break; }
            if (pad_ok) return off + 80;
        }
    }
    return 0;
}

// 头卡名: 前 8 字节左对齐空格填充; 返回去尾空格后是否等于 name
bool card_is(const unsigned char* card, const char* name) {
    char key[9];
    std::memcpy(key, card, 8);
    key[8] = '\0';
    for (int i = 7; i >= 0; --i) {
        if (key[i] == ' ') key[i] = '\0';
        else break;
    }
    return std::strcmp(key, name) == 0;
}

// FITS 归一化: 头区 CHECKSUM/DATASUM 卡第 11..80 列 (0-based 10..79)
// 清零; 返回是否完成头区解析 (false = 解析失败, 退回全字节)。
bool normalize_fits(std::vector<unsigned char>& buf) {
    const std::size_t hdr_end = buf.size() >= 80 ? fits_header_end(buf) : 0;
    if (hdr_end == 0) return false;
    for (std::size_t off = 0; off + 80 <= hdr_end; off += 80) {
        const unsigned char* card = &buf[off];
        if (card_is(card, "CHECKSUM") || card_is(card, "DATASUM"))
            std::memset(&buf[off + 10], 0, 70);
    }
    return true;
}

}  // namespace

static std::uint64_t fnv_file(const std::string& path) {
    const std::string ext = lower_ext(path);
    const bool is_fits = (ext == ".fits" || ext == ".fts");
    std::vector<unsigned char> buf;
    if (is_fits && read_file_bytes(path, buf)) {
        if (!normalize_fits(buf))
            std::fprintf(stderr,
                         "[p1hips_digest] FITS 头区解析失败, 退回全字节哈希: %s\n",
                         path.c_str());
        std::uint64_t h = kFnvOffset;
        for (const unsigned char c : buf) h = fnv_byte(h, c);
        return h;
    }
    // 非 FITS (或读文件失败时保持原读法): 原流式全字节路径
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    std::uint64_t h = kFnvOffset;
    char buf8[8192];
    while (f.read(buf8, sizeof(buf8)) || f.gcount() > 0) {
        const std::size_t n = (std::size_t)f.gcount();
        for (std::size_t i = 0; i < n; ++i) h = fnv_byte(h, (unsigned char)buf8[i]);
        if (!f) break;
    }
    return h;
}

void walk_dir(const std::string& dir, std::vector<std::string>& files) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        const std::string full = dir + "/" + name;
        struct stat st{};
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) walk_dir(full, files);
        else files.push_back(full);
    }
    closedir(d);
}

bool tree_digest(const std::string& dir, std::uint64_t& digest,
                 const char* skip1, const char* skip2) {
    std::vector<std::string> files;
    walk_dir(dir, files);
    std::sort(files.begin(), files.end());
    if (files.empty()) return false;
    std::uint64_t h = 1469598103934665603ULL;
    for (const auto& f : files) {
        if (skip1 && f.find(skip1) != std::string::npos) continue;
        if (skip2 && f.find(skip2) != std::string::npos) continue;
        h = (h ^ f.size()) * 1099511628211ULL;
        h = (h ^ fnv_file(f)) * 1099511628211ULL;
    }
    digest = h;
    return true;
}

// 共用: 写一个完整 f64 产品 (3 tiles: F1 常数 + F3 边界 + F1 变体)
// 返回 finalize rc (0 = 成功)。跨组共享 (properties/selfcheck 链引用)。
int write_full_f64_product(const std::string& dir, int flags,
                           const char* obs_date, bool set_prov) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64, flags,
        "ivo://astrocs/test/p1hips", "p1hips-test", "r", 100.0, obs_date, 0);
    if (!ps) return -100;
    if (set_prov) {
        const int prc = aio_hips_set_drizzle_provenance(ps, 0.7, 0.35);
        if (prc != 0) { aio_hips_abort(ps); return -101; }
    }
    FixViewF64 t0 = fix_hips_a_tile(0, 10.0, 0.5, 1.5, true, true);
    FixViewF64 t1 = fix_hips_c_edge_tile(1, 20.0, 0.5);
    FixViewF64 t2 = fix_hips_a_tile(2, 10.0, 0.5, 1.5, false, true);  // NULL mask
    if (flags & AIO_HIPS_PRODUCT_SIGNAL) {
        int rc = aio_hips_write_signal_support_tile(ps, &t0.view);
        if (rc != 0) { aio_hips_abort(ps); return rc; }
        rc = aio_hips_write_signal_support_tile(ps, &t1.view);
        if (rc != 0) { aio_hips_abort(ps); return rc; }
        rc = aio_hips_write_signal_support_tile(ps, &t2.view);
        if (rc != 0) { aio_hips_abort(ps); return rc; }
    }
    if (flags & (AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR)) {
        int rc = aio_hips_write_variance_tile(ps, &t0.view);
        if (rc != 0) { aio_hips_abort(ps); return rc; }
        rc = aio_hips_write_variance_tile(ps, &t2.view);
        if (rc != 0) { aio_hips_abort(ps); return rc; }
    }
    if (flags & AIO_HIPS_PRODUCT_SNR) {
        std::vector<FixSnrPointF> pts = fix_hips_d_snr_points(20260907u, 64);
        int rc = aio_hips_write_snr_points(ps, pts.data(), (int)pts.size());
        if (rc != 0) { aio_hips_abort(ps); return rc; }
    }
    return aio_hips_finalize(ps);
}

// 独立 MJD (oracle): Fliegel–Van Flandern 公历 JD 公式 (天文权威, 与生产
// 实现不同路径), MJD = JD − 2400000.5
static bool oracle_iso_to_mjd(const std::string& iso, double& mjd) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    double s = 0.0;
    if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%lf", &y, &mo, &d, &h, &mi, &s) != 6)
        return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
    const double jd =
        367.0 * y
        - std::floor(7.0 * (y + std::floor((mo + 9.0) / 12.0)) / 4.0)
        + std::floor(275.0 * mo / 9.0) + d + 1721013.5
        + ((double)h + (double)mi / 60.0 + s / 3600.0) / 24.0;
    mjd = jd - 2400000.5;
    return true;
}

int test_units() {
    CheckState cs;
    cs.failures = 0;
    cs.fault_reported = false;

    // --- U1: f64 单 tile (F1+F6): signal/support 逐像素 f64 bitwise (§9 冻结)
    {
        const std::string dir = make_tmp_dir("u1");
        P1HIPS_CHECK(cs, !dir.empty(), nullptr);
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://astrocs/test/p1hips", "u1-title", "r", 100.0,
            "2026-09-07T12:00:00", 0);
        P1HIPS_CHECK_MSG(cs, ps != nullptr, "u1_begin",
                         "product_begin f64 失败: %s", aio_hips_last_error());
        if (ps) {
            FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false);
            const int rc = aio_hips_write_signal_support_tile(ps, &fx.view);
            P1HIPS_CHECK_EQ(cs, rc, 0);
            const int frc = aio_hips_finalize(ps);
            P1HIPS_CHECK_EQ(cs, frc, 0);
        }
        TileFits sig, sup;
        const bool ok_s = read_tile_fits(dir + "/signal/Norder0/Dir0/Npix0.fits", sig);
        const bool ok_p = read_tile_fits(dir + "/support/Norder0/Dir0/Npix0.fits", sup);
        P1HIPS_CHECK(cs, ok_s && ok_p, "u1_readback");
        if (ok_s && ok_p) {
            P1HIPS_CHECK_EQ(cs, sig.bitpix, -64);
            P1HIPS_CHECK_EQ(cs, (long)sig.pix_d.size(), 512L * 512L);
            // oracle 期望: I1 signal=flux/area (B 常数→10.0), I2 support=0.5
            bool bit_sig = true, bit_sup = true;
            for (std::size_t i = 0; i < sig.pix_d.size(); ++i) {
                if (!bits_eq_d(sig.pix_d[i], 10.0)) { bit_sig = false; break; }
                if (!bits_eq_d(sup.pix_d[i], 0.5)) { bit_sup = false; break; }
            }
            P1HIPS_CHECK(cs, bit_sig, "u1_signal_bitwise");
            P1HIPS_CHECK(cs, bit_sup, "u1_support_bitwise");
            // FITS 头键精确 (§9)
            P1HIPS_CHECK(cs, sig.keys.count("NSIDE") && sig.keys["NSIDE"] == "512", "u1_key_nside");
            P1HIPS_CHECK(cs, sig.keys.count("ORDERING") && sig.keys["ORDERING"] == "NESTED", "u1_key_ordering");
            P1HIPS_CHECK(cs, sig.keys.count("PIXTYPE") && sig.keys["PIXTYPE"] == "HEALPIX", "u1_key_pixtype");
            P1HIPS_CHECK(cs, sig.keys.count("COORDSYS") && sig.keys["COORDSYS"] == "C", "u1_key_coordsys");
            P1HIPS_CHECK(cs, sig.keys.count("OBJECT") && sig.keys["OBJECT"] == "u1-title", "u1_key_object");
            P1HIPS_CHECK(cs, sig.keys.count("FIRSTPIX") && sig.keys["FIRSTPIX"] == "0", "u1_key_firstpix");
            P1HIPS_CHECK(cs, sig.keys.count("LASTPIX") && sig.keys["LASTPIX"] == "262143", "u1_key_lastpix");
        }
        // properties 键值精确 (image 产品)
        const auto kv = read_properties(dir + "/signal/properties");
        P1HIPS_CHECK(cs, kv.count("creator_did") && kv.at("creator_did") == "ivo://astrocs/test/p1hips", "u1_prop_creator");
        P1HIPS_CHECK(cs, kv.count("hips_version") && kv.at("hips_version") == "1.4", "u1_prop_version");
        P1HIPS_CHECK(cs, kv.count("hips_order") && kv.at("hips_order") == "0", "u1_prop_order");
        P1HIPS_CHECK(cs, kv.count("hips_tile_width") && kv.at("hips_tile_width") == "512", "u1_prop_tilewidth");
        P1HIPS_CHECK(cs, kv.count("hips_frame") && kv.at("hips_frame") == "equatorial", "u1_prop_frame");
        P1HIPS_CHECK(cs, kv.count("dataproduct_subtype") && kv.at("dataproduct_subtype") == "surface brightness", "u1_prop_subtype");
        P1HIPS_CHECK(cs, kv.count("astrocs_signal_dtype") && kv.at("astrocs_signal_dtype") == "float64", "u1_prop_dtype");
        // moc_sky_fraction (1/12 cell) — std::to_string 格式双侧一致
        {
            const double frac = 1.0 / 12.0;
            const auto it = kv.find("moc_sky_fraction");
            P1HIPS_CHECK(cs, it != kv.end() && it->second == std::to_string(frac), "u1_prop_mocfrac");
        }
        P1HIPS_CHECK(cs, kv.count("obs_exptime") && kv.at("obs_exptime") == std::to_string(100.0), "u1_prop_exptime");
        // t_min/t_max 独立 MJD 复算 (%.8f 双侧)
        {
            double t0 = 0.0;
            P1HIPS_CHECK(cs, oracle_iso_to_mjd("2026-09-07T12:00:00", t0), "u1_mjd_parse");
            char b0[32], b1[32];
            std::snprintf(b0, sizeof(b0), "%.8f", t0);
            std::snprintf(b1, sizeof(b1), "%.8f", t0 + 100.0 / 86400.0);
            P1HIPS_CHECK(cs, kv.count("t_min") && kv.at("t_min") == b0, "u1_tmin");
            P1HIPS_CHECK(cs, kv.count("t_max") && kv.at("t_max") == b1, "u1_tmax");
        }
    }

    // --- U2: f32 单 tile (F6): 存储 rtol=1e-7 (§9 f32 通路)
    {
        const std::string dir = make_tmp_dir("u2");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT32,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://astrocs/test/p1hips", "u2-title", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK_MSG(cs, ps != nullptr, "u2_begin", "product_begin f32 失败: %s", aio_hips_last_error());
        if (ps) {
            // f32 产品: 视图 data_type 须同为 FLOAT32 (生产按 dtype 解释指针)
            FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 0.0, true, false,
                                            AIO_HIPS_FLOAT32);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        TileFits sig;
        if (read_tile_fits(dir + "/signal/Norder0/Dir0/Npix0.fits", sig)) {
            P1HIPS_CHECK_EQ(cs, sig.bitpix, -32);
            bool all_ok = true;
            for (std::size_t i = 0; i < sig.pix_f.size(); ++i) {
                if (!rel_close_f(sig.pix_f[i], 10.0, 1e-7)) { all_ok = false; break; }
            }
            P1HIPS_CHECK(cs, all_ok, "u2_signal_rtol1e7");
        } else {
            P1HIPS_CHECK(cs, false, "u2_readback");
        }
    }

    // --- U3: NULL valid_mask 全有效 + I2 clamp (area_factor>1 → sup=1.0)
    {
        const std::string dir = make_tmp_dir("u3");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://astrocs/test/p1hips", "u3-title", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "u3_begin");
        if (ps) {
            FixViewF64 fx = fix_hips_a_tile(0, 10.0, 2.0, 0.0, false, false);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        TileFits sig, sup;
        if (read_tile_fits(dir + "/signal/Norder0/Dir0/Npix0.fits", sig) &&
            read_tile_fits(dir + "/support/Norder0/Dir0/Npix0.fits", sup)) {
            bool bit_sig = true, bit_sup = true;
            for (std::size_t i = 0; i < sig.pix_d.size(); ++i) {
                if (!bits_eq_d(sig.pix_d[i], 10.0)) { bit_sig = false; break; }
                if (!bits_eq_d(sup.pix_d[i], 1.0)) { bit_sup = false; break; }  // I2 clamp
            }
            P1HIPS_CHECK(cs, bit_sig, "u3_signal_bitwise");
            P1HIPS_CHECK(cs, bit_sup, "u3_support_clamp");
        } else {
            P1HIPS_CHECK(cs, false, "u3_readback");
        }
    }

    // --- U4: SNR Catalogue round-trip (F4+I12) + metadata.xml VOTable
    {
        const std::string dir = make_tmp_dir("u4");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64,
            AIO_HIPS_PRODUCT_SNR, "ivo://astrocs/test/p1hips", "u4-title",
            nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "u4_begin");
        std::vector<FixSnrPointF> pts = fix_hips_d_snr_points(20260907u, 128);
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_write_snr_points(ps, pts.data(), (int)pts.size()), 0);
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
        }
        std::map<long long, SnrRow> got;
        std::vector<std::string> files;
        walk_dir(dir + "/snr/Norder0", files);
        P1HIPS_CHECK(cs, !files.empty(), "u4_tiles_exist");
        bool rt_ok = true;
        for (const auto& f : files) {
            std::vector<SnrRow> rows;
            if (!read_snr_tsv(f, rows)) { rt_ok = false; break; }
            for (const auto& r : rows) got[r.star_id] = r;
        }
        if (rt_ok) {
            P1HIPS_CHECK_EQ(cs, got.size(), (long long)pts.size());
            for (const auto& p : pts) {
                const auto it = got.find(p.star_id);
                if (it == got.end()) { rt_ok = false; break; }
                // I12 f64 口径: snr %.17g 打印 → bitwise 精确;
                // ra/dec %.12f 打印 → 打印值往返一致 (同精度再现)
                char rb[32];
                std::snprintf(rb, sizeof(rb), "%.12f", p.ra_deg);
                if (it->second.ra != std::atof(rb)) { rt_ok = false; break; }
                std::snprintf(rb, sizeof(rb), "%.12f", p.dec_deg);
                if (it->second.dec != std::atof(rb)) { rt_ok = false; break; }
                if (!bits_eq_d(it->second.snr, p.snr)) { rt_ok = false; break; }
                if (it->second.qflags != p.quality_flags ||
                    it->second.pstatus != p.photometric_status) { rt_ok = false; break; }
            }
        }
        P1HIPS_CHECK(cs, rt_ok, "u4_snr_roundtrip");
        {
            std::ifstream f(dir + "/snr/metadata.xml");
            std::string txt((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
            P1HIPS_CHECK(cs, txt.find("<VOTABLE") != std::string::npos &&
                             txt.find("</VOTABLE>") != std::string::npos, "u4_votable");
        }
        const auto kv = read_properties(dir + "/snr/properties");
        P1HIPS_CHECK(cs, kv.count("hips_cat_nrows") && kv.at("hips_cat_nrows") == "128", "u4_cat_nrows");
        P1HIPS_CHECK(cs, kv.count("dataproduct_type") && kv.at("dataproduct_type") == "catalog", "u4_cat_type");
    }

    // --- U5: manifest.json 计数字段 = 实际文件数 (I8) + products 列表
    {
        const std::string dir = make_tmp_dir("u5");
        const int rc = write_full_f64_product(
            dir, AIO_HIPS_PRODUCT_ALL_V19, "2026-09-07T00:00:00", false);
        P1HIPS_CHECK_EQ(cs, rc, 0);
        std::string txt;
        P1HIPS_CHECK(cs, read_manifest(dir + "/manifest.json", txt), "u5_manifest_read");
        std::string v;
        if (manifest_get(txt, "n_leaf_tiles", v)) {
            std::vector<std::string> files;
            walk_dir(dir + "/signal/Norder0", files);
            const long leaf_files = (long)files.size();
            P1HIPS_CHECK_MSG(cs, std::atoll(v.c_str()) == leaf_files, "i8_manifest_count",
                             "manifest n_leaf_tiles=%s 实际 signal 叶 tile=%ld", v.c_str(), leaf_files);
        } else {
            P1HIPS_CHECK(cs, false, "u5_manifest_field");
        }
        P1HIPS_CHECK(cs, manifest_get(txt, "data_type", v) && v == "float64", "u5_manifest_dtype");
        P1HIPS_CHECK(cs, txt.find("\"signal\"") != std::string::npos &&
                         txt.find("\"support\"") != std::string::npos &&
                         txt.find("\"snr\"") != std::string::npos &&
                         txt.find("\"variance\"") != std::string::npos &&
                         txt.find("\"ivar\"") != std::string::npos, "u5_manifest_products");
    }

    if (cs.failures == 0) {
        std::fprintf(stdout, "[p1hips] units: U1..U5 PASS\n");
        return 0;
    }
    std::fprintf(stderr, "[p1hips] units: %d check(s) failed\n", cs.failures);
    return 1;
}

}  // namespace p1hips
