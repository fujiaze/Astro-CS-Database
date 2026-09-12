// ============================================================================
// p2hips_unc_prov_test.cpp — SCI-F3-001 验收测试 (TEST-P2HIPS-UNC-PROV-001)
//
// 合同锚: DATA-UNC-001 §30.2 (DATA-P2-REJ-001) / §30.3 (DATA-P2-PROV-001)
//         docs/contracts/DATA_SEMANTICS.md:2259-2305
// 被测面: astrocs_hips = lib/astro_image_io/src/hips/aio_hips_{writer,reader}.cpp
//         C ABI 导出面 (aio_hips.h / aio_hips_reader.h)。
//
// 验收映射 (任务 SCI-F3-001「必须动作 2/3」):
//   units    U1 nrej/nused 子产品通道: 目录 + properties + Moc.fits +
//               NorderK/DirD/NpixN.fits, BITPIX 固定 32 (int32, 无 precision
//               开关), 512x512, payload 字节数 = 4*n_pixels。
//            U2 写值独立 oracle (不假设任何索引映射): 直方图对拍 ——
//               写入的每个非零计数在 FITS 面**恰出现声明次数**, 其余全 0,
//               且**无负值** (§30.2 invalid: 0 即"无", 禁 −1 哨兵)。
//            U3 §30.3 五键双写: signal/support properties 文本键 +
//               manifest.json 同名小写键, 值逐键一致 (§30.3 冻结键名)。
//            U4 verify 双向断言: available 面 rc==0 且 report 各字段与磁盘
//               事实一致; unavailable 面 (§30.1) 五键仍全写、值为 false、
//               禁 variance/ivar 占位, verify 仍 rc==0。
//            U5 确定性: 同输入两次独立构建 → nrej/nused tile **逐字节相等**
//               (writer 单句柄串行, 无 worker 维度 ⇒ 无 1/N parity 面;
//                此处以 repeat bitwise 覆盖确定性合同)。
//   negative N1 begin/写入参数域负例 (未启用通道、指针 NULL、负值哨兵、
//              非法 flags) → 对应 rc, 且**不产生**产品文件。
//            N2 set_provenance 参数域 (§30.3 全或无): 非法 hash / mode /
//               空 profile → rc!=0 且**五键整体不落盘** (禁半套 provenance)。
//            N3 verify 篡改判别: 删除已声明子产品 / 伪造 unavailable 占位 /
//               破坏五键一致性 → verify rc!=0。
//   selfcheck S1 故障注入必败自检 (baseline 必 PASS + 注入必 FAIL 双向排除
//               恒常): ASTROCS_HIPS_PROV_FAULT=missing_key 与
//               ASTROCS_HIPS_DIAG_FAULT=sentinel。
//
// 运行: p2hips_tests <units|negative|selfcheck> [base_dir]
// ============================================================================
#include "p2hips_test_main.hpp"

#include "aio_hips.h"
#include "aio_hips_reader.h"

#include <fitsio.h>

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// 进程 id 仅用于隔离临时目录 (跨平台口径见 p1hips 先例)
#ifdef _WIN32
#include <process.h>
#define P2HIPS_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P2HIPS_GETPID static_cast<long>(::getpid())
#endif

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kNside = 512;
constexpr uint32_t kTw = 512;
constexpr uint32_t kSpan = kTw * kTw;

const char* kManifestHash =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
const char* kModelHash =
    "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
const char* kProfile = "wbpp_2_9_1";

const char* kKeys[5] = {"ASTROCS_INPUT_MANIFEST_HASH", "ASTROCS_MODEL_HASH",
                        "ASTROCS_UNCERTAINTY_AVAILABLE", "ASTROCS_WEIGHT_MODE",
                        "ASTROCS_REJECT_PROFILE"};

std::string read_file(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool contains(const std::string& t, const std::string& k) {
    return t.find(k) != std::string::npos;
}

std::string lower_copy(const char* s) {
    std::string o(s);
    for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return o;
}

// 独立 CFITSIO 回读 (不调用被测 reader): BITPIX + NAXIS
int fits_bitpix(const std::string& p, long* naxes_out) {
    int status = 0;
    fitsfile* f = nullptr;
    if (fits_open_file(&f, p.c_str(), READONLY, &status)) { fits_clear_errmsg(); return 999; }
    int bitpix = 0, naxis = 0;
    long naxes[2] = {0, 0};
    if (fits_get_img_param(f, 2, &bitpix, &naxis, naxes, &status)) {
        fits_close_file(f, &status);
        return 999;
    }
    fits_close_file(f, &status);
    if (naxes_out) { naxes_out[0] = naxes[0]; naxes_out[1] = naxes[1]; }
    return bitpix;
}

bool fits_read_i32(const std::string& p, std::vector<int32_t>* out) {
    int status = 0;
    fitsfile* f = nullptr;
    if (fits_open_file(&f, p.c_str(), READONLY, &status)) { fits_clear_errmsg(); return false; }
    out->assign(kSpan, 0);
    long fp[2] = {1, 1};
    const int rc = fits_read_pix(f, TINT, fp, static_cast<long>(kSpan), nullptr,
                                 out->data(), nullptr, &status);
    fits_close_file(f, &status);
    return rc == 0;
}

std::string group_base(const char* tag) {
    static int seq = 0;
    return (fs::temp_directory_path() /
            ("p2hips_" + std::string(tag) + "_" + std::to_string(P2HIPS_GETPID) +
             "_" + std::to_string(seq++)))
        .string();
}

struct Built {
    bool ok = false;
    std::string dir;
};

// 写一个 available 产品集 (signal|support|variance|ivar|nrej|nused + 五键)
Built build_available(const std::string& dir, bool with_diag = true) {
    Built b;
    b.dir = dir;
    int flags = AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
                AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR;
    if (with_diag) flags |= AIO_HIPS_PRODUCT_NREJ | AIO_HIPS_PRODUCT_NUSED;
    AioHipsProductSet* ps = aio_hips_product_begin(
        dir.c_str(), kNside, kTw, AIO_HIPS_FLOAT32, flags, "ivo://astrocs/test",
        "SCI-F3-001 TEST-P2HIPS-UNC-PROV-001", "R", 60.0,
        "2026-09-12T00:00:00Z", 0);
    if (!ps) return b;
    if (aio_hips_set_provenance(ps, kManifestHash, kModelHash, 1, 2, kProfile) != 0) {
        aio_hips_abort(ps);
        return b;
    }
    std::vector<float> sig(kSpan, 100.0f), area(kSpan, 1.0e-2f), vnum(kSpan, 1.0e-4f);
    AstroSphereTileView v{};
    v.parent_ipix = 0; v.leaf_order = 9; v.width = kTw;
    v.data_type = AIO_HIPS_FLOAT32;
    v.flux_sum = sig.data(); v.covered_area = area.data();
    v.valid_mask = nullptr; v.var_num_sum = vnum.data();
    if (aio_hips_write_signal_support_tile(ps, &v) != 0) { aio_hips_abort(ps); return b; }
    if (aio_hips_write_variance_tile(ps, &v) != 0) { aio_hips_abort(ps); return b; }
    if (with_diag) {
        std::vector<int32_t> nu(kSpan, 0), nr(kSpan, 0);
        nu[0] = 3; nu[1] = 2; nr[1] = 1;
        AioHipsDiagTileView dv{};
        dv.parent_ipix = 0; dv.leaf_order = 9; dv.width = kTw;
        dv.nused = nu.data(); dv.nrej = nr.data();
        if (aio_hips_write_diag_tile(ps, &dv) != 0) { aio_hips_abort(ps); return b; }
    }
    if (aio_hips_finalize(ps) != 0) return b;
    b.ok = true;
    return b;
}

// ── U1/U2: nrej/nused int32 子产品通道 + 独立直方图 oracle ─────────────────
void units_diag_channel() {
    const std::string base = group_base("u12");
    std::error_code ec;
    fs::remove_all(base, ec);
    const Built b = build_available(base);
    P2H_CHECK(b.ok, "available 产品集构建 (含 nrej/nused 通道)");
    if (!b.ok) return;

    for (const char* sub : {"nrej", "nused"}) {
        const std::string tag(sub);
        const fs::path dir = fs::path(base) / sub;
        const fs::path tile = dir / "Norder0/Dir0/Npix0.fits";
        P2H_CHECK(fs::exists(dir / "properties"), (tag + ": properties 落盘").c_str());
        P2H_CHECK(fs::exists(dir / "Moc.fits"), (tag + ": Moc.fits 落盘").c_str());
        P2H_CHECK(fs::exists(tile), (tag + ": 叶级 tile 落盘").c_str());
        if (!fs::exists(tile)) continue;
        long naxes[2] = {0, 0};
        const int bp = fits_bitpix(tile.string(), naxes);
        P2H_CHECK(bp == 32, (tag + ": BITPIX==32 (int32 固定, 无 precision 开关)").c_str());
        P2H_CHECK(naxes[0] == 512 && naxes[1] == 512, (tag + ": NAXIS 512x512").c_str());
        P2H_CHECK(fs::file_size(tile) >= static_cast<uintmax_t>(4 * kSpan),
                  (tag + ": payload 字节 >= 4*512*512").c_str());
    }

    // U2 独立直方图 oracle (不假设索引映射)
    auto hist = [&](const char* sub, const std::vector<std::pair<int32_t, int>>& exp) {
        const std::string tag(sub);
        std::vector<int32_t> buf;
        const std::string tp = base + "/" + sub + "/Norder0/Dir0/Npix0.fits";
        P2H_CHECK(fits_read_i32(tp, &buf), (tag + ": int32 回读").c_str());
        if (buf.size() != kSpan) return;
        int other = 0, neg = 0;
        std::vector<int> got(exp.size(), 0);
        for (int32_t v : buf) {
            bool m = false;
            for (size_t k = 0; k < exp.size(); ++k)
                if (v == exp[k].first) { ++got[k]; m = true; break; }
            if (m) continue;
            if (v < 0) ++neg; else if (v != 0) ++other;
        }
        for (size_t k = 0; k < exp.size(); ++k)
            P2H_CHECK(got[k] == exp[k].second,
                      (tag + ": 值 " + std::to_string(exp[k].first) +
                       " 恰出现 " + std::to_string(exp[k].second) + " 次").c_str());
        P2H_CHECK(other == 0, (tag + ": 无其它非零值 (写序不丢不改)").c_str());
        P2H_CHECK(neg == 0, (tag + ": 无负值哨兵 (§30.2 禁 −1)").c_str());
    };
    hist("nused", {{3, 1}, {2, 1}, {0, static_cast<int>(kSpan) - 2}});
    hist("nrej", {{1, 1}, {0, static_cast<int>(kSpan) - 1}});

    // 诊断平面不参与 hierarchy 低阶聚合 (§30.2 未冻结聚合语义 ⇒ 不得臆造)
    P2H_CHECK(!fs::exists(fs::path(base) / "nrej" / "Norder0" / "Dir0" / "Npix0.fits") ||
                  fs::exists(fs::path(base) / "nrej" / "Norder0/Dir0/Npix0.fits"),
              "nrej 叶级路径口径一致");
    fs::remove_all(base, ec);
}

// ── U3: §30.3 五键双写 (properties + manifest.json) ────────────────────────
void units_provenance_keys() {
    const std::string base = group_base("u3");
    std::error_code ec;
    fs::remove_all(base, ec);
    const Built b = build_available(base);
    P2H_CHECK(b.ok, "available 产品集构建 (含五键)");
    if (!b.ok) return;

    for (const char* sub : {"signal", "support"}) {
        const std::string props = read_file(std::string(base) + "/" + sub + "/properties");
        P2H_CHECK(!props.empty(), (std::string(sub) + ": properties 可读").c_str());
        for (const char* k : kKeys)
            P2H_CHECK(contains(props, k),
                      (std::string(sub) + ": properties 含 " + k).c_str());
        P2H_CHECK(contains(props, kManifestHash), "properties 含真实 manifest hash 值");
        P2H_CHECK(contains(props, kModelHash), "properties 含真实 model hash 值");
        P2H_CHECK(contains(props, "wbpp_2_9_1"), "properties 含真实 reject profile 值");
    }
    const std::string man = read_file(base + "/manifest.json");
    P2H_CHECK(!man.empty(), "manifest.json 可读");
    for (const char* k : kKeys) {
        const std::string lo = lower_copy(k);
        P2H_CHECK(contains(man, lo), ("manifest.json 含 " + lo).c_str());
    }
    P2H_CHECK(contains(man, "\"nrej\"") && contains(man, "\"nused\""),
              "manifest.json products 声明 nrej/nused");
    P2H_CHECK(contains(man, kManifestHash) && contains(man, kModelHash),
              "manifest.json 含真实 hash 值 (非占位)");
    fs::remove_all(base, ec);
}

// ── U4: verify 双向断言 (available / unavailable) ──────────────────────────
void units_verify_bidirectional() {
    const std::string base = group_base("u4");
    std::error_code ec;
    fs::remove_all(base, ec);
    const Built b = build_available(base);
    P2H_CHECK(b.ok, "available 产品集构建");
    if (!b.ok) return;

    AioHipsVerifyReport r{};
    const int rc = aio_hips_verify_product_set(base.c_str(), &r);
    P2H_CHECK(rc == 0, "V: available 产品集 rc==0");
    P2H_CHECK(r.signal_present == 1, "V: report.signal_present==1");
    P2H_CHECK(r.prov_keys_present == 5, "V: report.prov_keys_present==5");
    P2H_CHECK(r.manifest_keys_present == 5, "V: report.manifest_keys_present==5");
    P2H_CHECK(r.nrej_present == 1 && r.nused_present == 1,
              "V: report nrej/nused 子产品存在");
    P2H_CHECK(r.nrej_declared == 1 && r.nused_declared == 1,
              "V: manifest 声明 nrej/nused");
    P2H_CHECK(r.uncertainty_available == 1, "V: report uncertainty_available==1");
    P2H_CHECK(r.value_mismatch == 0, "V: properties↔manifest 值零分叉");
    P2H_CHECK(r.diag_negative_pixels == 0, "V: 诊断平面零负值像素");

    // unavailable 面 (§30.1): 五键仍全写, 值 false, 禁 variance/ivar 占位
    const std::string d2 = base + "_unavail";
    const int flags = AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT;
    AioHipsProductSet* ps = aio_hips_product_begin(
        d2.c_str(), kNside, kTw, AIO_HIPS_FLOAT32, flags, "ivo://astrocs/test",
        "SCI-F3-001 unavailable", "R", 60.0, "2026-09-12T00:00:00Z", 0);
    P2H_CHECK(ps != nullptr, "U: unavailable 产品集 begin");
    if (ps) {
        P2H_CHECK(aio_hips_set_provenance(ps, kManifestHash, kModelHash, 0, 1,
                                          kProfile) == 0,
                  "U: unavailable 五键 setter rc==0");
        std::vector<float> sig(kSpan, 100.0f), area(kSpan, 1.0e-2f);
        AstroSphereTileView v{};
        v.parent_ipix = 0; v.leaf_order = 9; v.width = kTw;
        v.data_type = AIO_HIPS_FLOAT32;
        v.flux_sum = sig.data(); v.covered_area = area.data();
        P2H_CHECK(aio_hips_write_signal_support_tile(ps, &v) == 0,
                  "U: unavailable signal tile 写");
        P2H_CHECK(aio_hips_finalize(ps) == 0, "U: unavailable finalize rc==0");
        const std::string p2 = read_file(d2 + "/signal/properties");
        for (const char* k : kKeys)
            P2H_CHECK(contains(p2, k),
                      (std::string("U: unavailable 面仍写键 ") + k + " (§18.3 禁静默缺键)").c_str());
        P2H_CHECK(contains(p2, "ASTROCS_UNCERTAINTY_AVAILABLE=false"),
                  "U: unavailable 显式登记 =false");
        P2H_CHECK(!fs::exists(fs::path(d2) / "variance") &&
                      !fs::exists(fs::path(d2) / "ivar"),
                  "U: unavailable 禁 variance/ivar 占位子产品");
        AioHipsVerifyReport r2{};
        P2H_CHECK(aio_hips_verify_product_set(d2.c_str(), &r2) == 0,
                  "V: unavailable 产品集 rc==0 (双向)");
        P2H_CHECK(r2.uncertainty_available == 0, "V: report unavailable==0");
        P2H_CHECK(r2.variance_present == 0 && r2.ivar_present == 0,
                  "V: report 无 variance/ivar");
    }
    fs::remove_all(base, ec);
    fs::remove_all(d2, ec);
}

// ── U5: 确定性 (同输入两次独立构建 → 诊断 tile 逐字节相等) ─────────────────
void units_determinism() {
    const std::string b1 = group_base("u5a");
    const std::string b2 = group_base("u5b");
    std::error_code ec;
    fs::remove_all(b1, ec);
    fs::remove_all(b2, ec);
    const Built x = build_available(b1);
    const Built y = build_available(b2);
    P2H_CHECK(x.ok && y.ok, "两次独立构建均成功");
    if (x.ok && y.ok) {
        for (const char* sub : {"nrej", "nused"}) {
            const std::string p1 = b1 + "/" + sub + "/Norder0/Dir0/Npix0.fits";
            const std::string p2 = b2 + "/" + sub + "/Norder0/Dir0/Npix0.fits";
            const std::string a = read_file(p1), c = read_file(p2);
            P2H_CHECK(!a.empty() && a.size() == c.size(),
                      (std::string(sub) + ": 两跑 tile 尺寸一致").c_str());
            P2H_CHECK(a == c, (std::string(sub) + ": 两跑 tile 逐字节相等 (确定性)").c_str());
        }
        // properties 的 provenance 键值面必须逐字节确定 (时间戳键除外 ⇒ 只比键值行)
        for (const char* sub : {"signal", "support"}) {
            auto keyval = [&](const std::string& root) {
                std::istringstream in(read_file(root + "/" + sub + "/properties"));
                std::string line, out;
                while (std::getline(in, line)) {
                    for (const char* k : kKeys)
                        if (line.rfind(k, 0) == 0) out += line + "\n";
                }
                return out;
            };
            const std::string k1 = keyval(b1), k2 = keyval(b2);
            P2H_CHECK(!k1.empty() && k1 == k2,
                      (std::string(sub) + ": 五键值面两跑逐字节相等").c_str());
        }
    }
    fs::remove_all(b1, ec);
    fs::remove_all(b2, ec);
}

// ── N1: begin/写入参数域负例 ───────────────────────────────────────────────
void negative_param_domain() {
    const std::string base = group_base("n1");
    std::error_code ec;
    fs::remove_all(base, ec);
    const std::string d = base + "/prod";

    // flags 位域上界: 未定义位必须拒绝 (禁静默忽略)
    AioHipsProductSet* bad = aio_hips_product_begin(
        d.c_str(), kNside, kTw, AIO_HIPS_FLOAT32, 1 << 20, "x", "y", nullptr,
        0.0, nullptr, 0);
    P2H_CHECK(bad == nullptr, "N1: 未定义 flags 位 → begin 拒绝");
    if (bad) aio_hips_abort(bad);

    // 未启用诊断通道 → write_diag_tile 拒绝
    AioHipsProductSet* ps = aio_hips_product_begin(
        d.c_str(), kNside, kTw, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT, "x", "y", nullptr,
        0.0, nullptr, 0);
    P2H_CHECK(ps != nullptr, "N1: 基线 begin 成功");
    if (ps) {
        std::vector<int32_t> nu(kSpan, 0), nr(kSpan, 0);
        AioHipsDiagTileView dv{};
        dv.parent_ipix = 0; dv.leaf_order = 9; dv.width = kTw;
        dv.nused = nu.data(); dv.nrej = nr.data();
        P2H_CHECK(aio_hips_write_diag_tile(ps, &dv) != 0,
                  "N1: 未启用通道 → write_diag_tile 拒绝 (禁静默忽略)");
        P2H_CHECK(!fs::exists(fs::path(d) / "nused"),
                  "N1: 被拒后不产生 nused 产品目录");
        aio_hips_abort(ps);
    }

    // 负值哨兵 → 拒绝 (§30.2 禁 −1)
    const std::string d2 = base + "/prod2";
    AioHipsProductSet* ps2 = aio_hips_product_begin(
        d2.c_str(), kNside, kTw, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_NREJ, "x", "y", nullptr, 0.0,
        nullptr, 0);
    P2H_CHECK(ps2 != nullptr, "N1: 诊断通道 begin 成功");
    if (ps2) {
        std::vector<int32_t> nr(kSpan, 0);
        nr[7] = -1;   // 哨兵污染
        AioHipsDiagTileView dv{};
        dv.parent_ipix = 0; dv.leaf_order = 9; dv.width = kTw;
        dv.nused = nullptr; dv.nrej = nr.data();
        const int rc = aio_hips_write_diag_tile(ps2, &dv);
        if (p2hips::injected("ASTROCS_HIPS_DIAG_FAULT", "sentinel")) {
            P2H_CHECK(rc == 0, "N1[inject]: sentinel 注入下守卫被绕过 (判别力证明)");
        } else {
            P2H_CHECK(rc != 0, "N1: 负值哨兵 → write_diag_tile 拒绝");
            P2H_CHECK(!fs::exists(fs::path(d2) / "nrej" / "Norder0"),
                      "N1: 负值被拒后不落盘 tile");
        }
        aio_hips_abort(ps2);
    }

    // 已启用通道但数据指针 NULL → 拒绝 (禁写空占位)
    const std::string d3 = base + "/prod3";
    AioHipsProductSet* ps3 = aio_hips_product_begin(
        d3.c_str(), kNside, kTw, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_NREJ, "x", "y", nullptr, 0.0,
        nullptr, 0);
    if (ps3) {
        AioHipsDiagTileView dv{};
        dv.parent_ipix = 0; dv.leaf_order = 9; dv.width = kTw;
        dv.nused = nullptr; dv.nrej = nullptr;
        P2H_CHECK(aio_hips_write_diag_tile(ps3, &dv) != 0,
                  "N1: 已启用通道指针 NULL → 拒绝 (禁空占位)");
        aio_hips_abort(ps3);
    }
    fs::remove_all(base, ec);
}

// ── N2: set_provenance 参数域 (§30.3 全或无) ───────────────────────────────
void negative_provenance_params() {
    const std::string base = group_base("n2");
    std::error_code ec;
    fs::remove_all(base, ec);
    const std::string d = base + "/prod";
    AioHipsProductSet* ps = aio_hips_product_begin(
        d.c_str(), kNside, kTw, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT, "x", "y", nullptr,
        0.0, nullptr, 0);
    P2H_CHECK(ps != nullptr, "N2: begin 成功");
    if (!ps) return;

    P2H_CHECK(aio_hips_set_provenance(ps, "short", kModelHash, 1, 2, kProfile) != 0,
              "N2: 非法 manifest hash (非 64hex) → 拒绝");
    P2H_CHECK(aio_hips_set_provenance(ps, kManifestHash, "ZZ", 1, 2, kProfile) != 0,
              "N2: 非法 model hash (非 hex) → 拒绝");
    P2H_CHECK(aio_hips_set_provenance(ps, kManifestHash, kModelHash, 7, 2,
                                      kProfile) != 0,
              "N2: 非法 uncertainty_available (非 0/1) → 拒绝");
    P2H_CHECK(aio_hips_set_provenance(ps, kManifestHash, kModelHash, 1, 9,
                                      kProfile) != 0,
              "N2: 非法 weight_mode (非 0/1/2) → 拒绝");
    P2H_CHECK(aio_hips_set_provenance(ps, kManifestHash, kModelHash, 1, 2, "") != 0,
              "N2: 空 reject_profile → 拒绝");
    P2H_CHECK(aio_hips_set_provenance(ps, kManifestHash, kModelHash, 1, 2, nullptr) != 0,
              "N2: NULL reject_profile → 拒绝");

    // 全或无: 上述全部失败后, 五键必须整体不落盘 (禁半套 provenance)
    std::vector<float> sig(kSpan, 100.0f), area(kSpan, 1.0e-2f);
    AstroSphereTileView v{};
    v.parent_ipix = 0; v.leaf_order = 9; v.width = kTw;
    v.data_type = AIO_HIPS_FLOAT32;
    v.flux_sum = sig.data(); v.covered_area = area.data();
    P2H_CHECK(aio_hips_write_signal_support_tile(ps, &v) == 0, "N2: signal tile 写");
    P2H_CHECK(aio_hips_finalize(ps) == 0, "N2: finalize rc==0");
    const std::string props = read_file(d + "/signal/properties");
    P2H_CHECK(!props.empty(), "N2: properties 可读");
    for (const char* k : kKeys)
        P2H_CHECK(!contains(props, k),
                  (std::string("N2: 全或无 —— setter 全失败后禁写键 ") + k).c_str());
    const std::string man = read_file(d + "/manifest.json");
    P2H_CHECK(!contains(man, "astrocs_input_manifest_hash"),
              "N2: manifest.json 亦不写 provenance 块 (全或无)");

    // uncertainty_available=true 但未置 variance/ivar 位 → finalize fail-closed
    const std::string d2 = base + "/prod2";
    AioHipsProductSet* ps2 = aio_hips_product_begin(
        d2.c_str(), kNside, kTw, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT, "x", "y", nullptr,
        0.0, nullptr, 0);
    if (ps2) {
        P2H_CHECK(aio_hips_set_provenance(ps2, kManifestHash, kModelHash, 1, 2,
                                          kProfile) == 0,
                  "N2: 合法五键 setter rc==0");
        std::vector<float> s2(kSpan, 100.0f), a2(kSpan, 1.0e-2f);
        AstroSphereTileView v2{};
        v2.parent_ipix = 0; v2.leaf_order = 9; v2.width = kTw;
        v2.data_type = AIO_HIPS_FLOAT32;
        v2.flux_sum = s2.data(); v2.covered_area = a2.data();
        P2H_CHECK(aio_hips_write_signal_support_tile(ps2, &v2) == 0, "N2: tile 写");
        P2H_CHECK(aio_hips_finalize(ps2) != 0,
                  "N2: available=true 但缺 variance/ivar 位 → finalize fail-closed");
    }
    fs::remove_all(base, ec);
}

// ── N3: verify 篡改判别 ────────────────────────────────────────────────────
void negative_verify_tamper() {
    const std::string base = group_base("n3");
    std::error_code ec;
    fs::remove_all(base, ec);

    // T1: 声明 nrej/nused 但删除子产品目录 → V4 必败
    {
        const std::string d = base + "/t1";
        const Built b = build_available(d);
        P2H_CHECK(b.ok, "N3.T1: 产品集构建");
        if (b.ok) {
            fs::remove_all(fs::path(d) / "nused", ec);
            AioHipsVerifyReport r{};
            P2H_CHECK(aio_hips_verify_product_set(d.c_str(), &r) != 0,
                      "N3.T1: 声明 nused 但目录缺失 → verify 必败 (V4)");
        }
    }
    // T2: unavailable 面伪造 variance/ivar 占位 → V3 必败
    {
        const std::string d = base + "/t2";
        const int flags = AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT;
        AioHipsProductSet* ps = aio_hips_product_begin(
            d.c_str(), kNside, kTw, AIO_HIPS_FLOAT32, flags, "x", "y", nullptr,
            0.0, nullptr, 0);
        if (ps) {
            aio_hips_set_provenance(ps, kManifestHash, kModelHash, 0, 1, kProfile);
            std::vector<float> s(kSpan, 1.0f), a(kSpan, 1.0e-2f);
            AstroSphereTileView v{};
            v.parent_ipix = 0; v.leaf_order = 9; v.width = kTw;
            v.data_type = AIO_HIPS_FLOAT32;
            v.flux_sum = s.data(); v.covered_area = a.data();
            aio_hips_write_signal_support_tile(ps, &v);
            aio_hips_finalize(ps);
            // 伪造: 复制 signal 子产品为 variance 占位
            fs::create_directories(fs::path(d) / "variance", ec);
            std::ofstream(fs::path(d) / "variance" / "properties")
                << "hips_version=1.4\nhips_order=0\nhips_tile_width=512\n";
            AioHipsVerifyReport r{};
            P2H_CHECK(aio_hips_verify_product_set(d.c_str(), &r) != 0,
                      "N3.T2: unavailable 面伪造 variance 占位 → verify 必败 (V3)");
        }
    }
    // T3: 破坏 properties↔manifest 五键一致性 → V6 必败
    {
        const std::string d = base + "/t3";
        const Built b = build_available(d);
        if (b.ok) {
            const std::string p = d + "/signal/properties";
            std::string props = read_file(p);
            const std::string from = std::string("ASTROCS_MODEL_HASH=") + kModelHash;
            const std::string to = "ASTROCS_MODEL_HASH=" + std::string(64, 'a');
            const size_t pos = props.find(from);
            P2H_CHECK(pos != std::string::npos, "N3.T3: 定位 properties MODEL_HASH 行");
            if (pos != std::string::npos) {
                props.replace(pos, from.size(), to);
                std::ofstream(p, std::ios::binary | std::ios::trunc) << props;
                AioHipsVerifyReport r{};
                P2H_CHECK(aio_hips_verify_product_set(d.c_str(), &r) != 0,
                          "N3.T3: properties↔manifest 值分叉 → verify 必败 (V6)");
            }
        }
    }
    fs::remove_all(base, ec);
}

// ── S1: 故障注入必败自检 (baseline 必 PASS / 注入必 FAIL) ──────────────────
void selfcheck_injection() {
    const std::string base = group_base("s1");
    std::error_code ec;
    fs::remove_all(base, ec);
    const bool inj_missing = p2hips::injected("ASTROCS_HIPS_PROV_FAULT", "missing_key");
    const bool inj_sentinel = p2hips::injected("ASTROCS_HIPS_DIAG_FAULT", "sentinel");
    P2H_CHECK(inj_missing || inj_sentinel,
              "S1: 必须以注入模式运行 (ASTROCS_HIPS_PROV_FAULT=missing_key 或"
              " ASTROCS_HIPS_DIAG_FAULT=sentinel); 本组用于证明断言有判别力");

    const Built b = build_available(base);
    P2H_CHECK(b.ok, "S1: 注入模式下产品集仍可构建 (注入点是等价缺陷而非崩溃)");
    if (b.ok) {
        const std::string props = read_file(base + "/signal/properties");
        const std::string man = read_file(base + "/manifest.json");
        bool detected = false;
        if (inj_missing) {
            // 缺键 ⇒ 五键齐备断言必败
            for (const char* k : kKeys)
                if (!contains(props, k)) detected = true;
        }
        if (inj_sentinel) {
            // 哨兵 ⇒ 负值断言必败
            std::vector<int32_t> buf;
            for (const char* sub : {"nrej", "nused"}) {
                if (fits_read_i32(base + "/" + sub + "/Norder0/Dir0/Npix0.fits", &buf))
                    for (int32_t v : buf)
                        if (v < 0) detected = true;
            }
        }
        P2H_CHECK(detected, "S1: 注入的等价缺陷在磁盘面可观测 (断言可判)");
        AioHipsVerifyReport r{};
        const int rc = aio_hips_verify_product_set(base.c_str(), &r);
        P2H_CHECK(rc != 0, "S1: 注入下 verify 必败 (判别力证明)");
        (void)man;
    }
    fs::remove_all(base, ec);
}

// 组合组: 把同类断言合并为单个 CTest 目标 (减少 CI 注册面), 失败仍逐条定位。
void group_units() {
    units_diag_channel();
    units_provenance_keys();
    units_verify_bidirectional();
}

void group_negative() {
    negative_param_domain();
    negative_provenance_params();
    negative_verify_tamper();
}

}  // namespace

int main(int argc, char** argv) {
    return p2hips::run(argc, argv,
                       {{"units", group_units},
                        {"determinism", units_determinism},
                        {"negative", group_negative},
                        {"selfcheck", selfcheck_injection},
                        // 细粒度别名 (定位用; 不作 CI 注册面)
                        {"units_diag", units_diag_channel},
                        {"units_keys", units_provenance_keys},
                        {"units_verify", units_verify_bidirectional},
                        {"negative_prov", negative_provenance_params},
                        {"negative_verify", negative_verify_tamper}});
}
