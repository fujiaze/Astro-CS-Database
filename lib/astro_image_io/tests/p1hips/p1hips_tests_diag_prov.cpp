// P1-HIPS-TEST · SCI-F3-001 增补组: DATA-UNC-001 §30.2/§30.3 AIO 通道
//
// 控制包任务: SCI-F3-001 (AIO NREJ/NUSED 通道与五 provenance 键落盘)。
// finding 锚: 05_FINDINGS_REGISTER §STD-F3（原 F-P2-002-03）。
//
// 覆盖 (被测量 = astrocs_hips 生产实现 aio_hips_writer/reader):
//   §30.2  NREJ=32 / NUSED=64 int32 子产品通道
//          (BITPIX=32, NESTED 512×512 tile, 无 precision 开关, 0 即"无"禁 −1 哨兵)
//   §30.3  ASTROCS_* 五 provenance 键双写 (properties 大写键 + manifest.json 小写键)
//   verify 双向断言: available=true ⇒ variance/ivar 子产品必在 (HDU/PRODUCT);
//          available=false ⇒ 禁占位 (子产品不得存在);
//          诊断平面 manifest 声明 ↔ 磁盘事实双向; 值域 (负值 = 契约违反)
//          与 properties↔manifest 值一致性
//
// 测试设计纪律 (控制包「测试设计先行」):
//   * 先红后绿: 基线 (未实现通道) 上本组断言必败 —— 证据见任务返回包
//     run/scif3001/logs/; 实现后必绿。
//   * 故障注入必败: 三处库级等价缺陷注入 (ASTROCS_HIPS_PROV_FAULT=
//     missing_key|value_drift、ASTROCS_HIPS_DIAG_FAULT=sentinel|skip_write、
//     ASTROCS_HIPS_VERIFY_FAULT=shortcut) 由 test_diag_prov_selfcheck() 逐条
//     验证"基线必 PASS + 注入必 FAIL"双向排除恒常。
//   * 独立性: 期望值由本 TU 的独立 oracle 计算 (FITS 序解析期望 + 独立
//     CFITSIO TINT 只读通道), 不用被测函数生成期望值。
//   * 确定性: 双跑 nrej/nused tile 逐字节 bitwise 相等。
//
// 本 TU 不新增 CTest 目标 (零注册债): 正向用例挂入既有 units 组、
// 负向用例挂入既有 negative 组、注入自检挂入既有 selfcheck 可执行 ——
// 三者均为 ci/ctest_baseline.json 冻结存量目标, CTEST-REGISTRATION 闭包不变。
#include "p1hips_test_main.hpp"
#include "p1hips_fixtures.hpp"
#include "p1hips_oracle.hpp"

#include <fitsio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "aio_hips.h"
#include "aio_hips_reader.h"
#include "healpix/healpix_core.h"

using namespace p1hips;
using namespace p1hips::oracle;

namespace p1hips {
namespace {

// ── 冻结常量 (DATA-UNC-001 §30.2/§30.3) ────────────────────────────────────
const char* kProvKeys[5] = {"ASTROCS_INPUT_MANIFEST_HASH", "ASTROCS_MODEL_HASH",
                            "ASTROCS_UNCERTAINTY_AVAILABLE",
                            "ASTROCS_WEIGHT_MODE", "ASTROCS_REJECT_PROFILE"};
constexpr uint32_t kSpan = 512u * 512u;
// 64 hex 真实形态值 (形态即契约: sha256 十六进制)
const char* kManifestHash =
    "3f2a1c0d9b8e7f60514233241506172839485766758493021122334455667788";
const char* kModelHash =
    "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
const char* kProfile = "wbpp_2_9_1";
constexpr int kWeightMode = 2;

std::string dp_dir(const std::string& d, const char* sub) {
    return d + "/" + sub;
}

bool file_has(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::string();
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

// properties 文本键解析 (独立实现, 不复用被测解析路径)
std::map<std::string, std::string> parse_props(const std::string& p) {
    std::map<std::string, std::string> kv;
    std::ifstream f(p);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
        kv[k] = v;
    }
    return kv;
}

// 独立 int32 FITS 只读 oracle (raw CFITSIO TINT; 不经被测 reader 函数)
bool oracle_read_i32(const std::string& path, int* bitpix, std::vector<int32_t>* out) {
    fitsfile* fptr = nullptr;
    int status = 0;
    if (fits_open_file(&fptr, path.c_str(), READONLY, &status)) return false;
    int bp = 0, naxis = 0;
    long naxes[2] = {0, 0};
    if (fits_get_img_param(fptr, 2, &bp, &naxis, naxes, &status)) {
        fits_close_file(fptr, &status);
        return false;
    }
    *bitpix = bp;
    const long n = naxes[0] * naxes[1];
    out->assign((size_t)n, 0);
    long fpixel[2] = {1, 1};
    const bool ok = (bp == 32) &&
                    fits_read_pix(fptr, TINT, fpixel, n, nullptr, out->data(),
                                  nullptr, &status) == 0;
    fits_close_file(fptr, &status);
    return ok;
}

// ── fixture: 诊断平面期望面 (FITS row-major 为断言唯一坐标口径) ────────────
// 覆盖区 [0,448)²: nused=3, 每 32px 网格 (x%32==16 ∧ y%32==16) 处 nrej=1;
// void 角落 [448,512)²: nused=0 ∧ nrej=0 (§30.2 invalid: 0 即"无", 禁 −1)
inline bool dp_is_void(uint32_t x, uint32_t y) { return x >= 448u && y >= 448u; }
inline bool dp_is_rej(uint32_t x, uint32_t y) {
    return (x % 32u == 16u) && (y % 32u == 16u);
}
std::vector<int32_t> dp_expected_nused() {
    std::vector<int32_t> v(kSpan, 0);
    for (uint32_t y = 0; y < 512u; ++y)
        for (uint32_t x = 0; x < 512u; ++x)
            if (!dp_is_void(x, y)) v[(size_t)y * 512u + x] = 3;
    return v;
}
std::vector<int32_t> dp_expected_nrej() {
    std::vector<int32_t> v(kSpan, 0);
    for (uint32_t y = 0; y < 512u; ++y)
        for (uint32_t x = 0; x < 512u; ++x)
            if (!dp_is_void(x, y) && dp_is_rej(x, y)) v[(size_t)y * 512u + x] = 1;
    return v;
}
// FITS 序期望面 → NESTED local 序视图 (writer view 合同)
std::vector<int32_t> dp_to_local(const std::vector<int32_t>& fits_order) {
    std::vector<int32_t> local(kSpan, 0);
    for (uint32_t fi = 0; fi < kSpan; ++fi) {
        const uint32_t local_i = (uint32_t)astrocs::healpix::fits_index_to_nested_local(
            (uint64_t)fi, 9u, 512u);
        local[local_i] = fits_order[fi];
    }
    return local;
}

struct DpProduct {
    std::string dir;
    std::vector<int32_t> nused_local, nrej_local;
};

// 写一个完整产品集 (调用方给 flags / provenance 形态)
// diag: 0=不写诊断平面; 1=写 (BITPIX=32 int32)
int dp_write_product(const std::string& dir, int flags, bool with_prov,
                     int uncertainty_available, bool with_diag, DpProduct* out) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT32, flags,
        "ivo://astrocs/test/scif3001", "SCI-F3-001", "r", 60.0,
        "2026-09-12T00:00:00Z", 0);
    if (!ps) return -100;
    if (with_prov) {
        const int prc = aio_hips_set_provenance(ps, kManifestHash, kModelHash,
                                                uncertainty_available,
                                                kWeightMode, kProfile);
        if (prc != 0) { aio_hips_abort(ps); return -101; }
    }
    FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 1.5, true, true,
                                    AIO_HIPS_FLOAT32);
    int rc = aio_hips_write_signal_support_tile(ps, &fx.view);
    if (rc != 0) { aio_hips_abort(ps); return rc; }
    if (flags & (AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR)) {
        rc = aio_hips_write_variance_tile(ps, &fx.view);
        if (rc != 0) { aio_hips_abort(ps); return rc; }
    }
    if (with_diag || (flags & (AIO_HIPS_PRODUCT_NREJ | AIO_HIPS_PRODUCT_NUSED))) {
        AioHipsDiagTileView dv{};
        dv.parent_ipix = 0;
        dv.leaf_order = FIX_LEAF_ORDER;
        dv.width = 512;
        if (out) {
            out->nused_local = dp_to_local(dp_expected_nused());
            out->nrej_local = dp_to_local(dp_expected_nrej());
        }
        const std::vector<int32_t> nused_buf =
            out ? out->nused_local : dp_to_local(dp_expected_nused());
        const std::vector<int32_t> nrej_buf =
            out ? out->nrej_local : dp_to_local(dp_expected_nrej());
        dv.nused = (flags & AIO_HIPS_PRODUCT_NUSED) ? nused_buf.data() : nullptr;
        dv.nrej = (flags & AIO_HIPS_PRODUCT_NREJ) ? nrej_buf.data() : nullptr;
        rc = aio_hips_write_diag_tile(ps, &dv);
        if (rc != 0) { aio_hips_abort(ps); return rc; }
    }
    rc = aio_hips_finalize(ps);
    if (rc != 0) return rc;
    if (out) out->dir = dir;
    return 0;
}

const int kFullFlags = AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
                       AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR |
                       AIO_HIPS_PRODUCT_NREJ | AIO_HIPS_PRODUCT_NUSED;
const int kDiagOnlyFlags = AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
                           AIO_HIPS_PRODUCT_NREJ | AIO_HIPS_PRODUCT_NUSED;

// 五键齐备性 + 值对拍 (properties, 大写键)
int dp_check_prov_props(CheckState& cs, const std::string& dir,
                        const char* prod, const char* fault) {
    const std::string pp = dp_dir(dir, prod) + "/properties";
    const auto kv = parse_props(pp);
    int missing = 0;
    for (const char* k : kProvKeys)
        if (kv.find(k) == kv.end()) ++missing;
    P1HIPS_CHECK_MSG(cs, missing == 0, fault,
                     "%s/properties 五 provenance 键缺失 %d 个 (§30.3)", prod,
                     missing);
    if (missing != 0) return 1;
    int bad = 0;
    bad += (kv.at("ASTROCS_INPUT_MANIFEST_HASH") != kManifestHash);
    bad += (kv.at("ASTROCS_MODEL_HASH") != kModelHash);
    bad += (kv.at("ASTROCS_REJECT_PROFILE") != kProfile);
    bad += (kv.at("ASTROCS_WEIGHT_MODE") != std::to_string(kWeightMode));
    P1HIPS_CHECK_MSG(cs, bad == 0, fault,
                     "%s/properties provenance 值与 setter 输入不符 (%d 处)",
                     prod, bad);
    return bad;
}

// manifest.json 标量取值 (独立解析)
bool dp_json_scalar(const std::string& doc, const std::string& key, std::string* out) {
    const std::string pat = "\"" + key + "\"";
    const size_t k = doc.find(pat);
    if (k == std::string::npos) return false;
    const size_t colon = doc.find(':', k + pat.size());
    if (colon == std::string::npos) return false;
    size_t p = colon + 1;
    while (p < doc.size() && (doc[p] == ' ' || doc[p] == '\n' || doc[p] == '\r' ||
                              doc[p] == '\t')) ++p;
    if (p < doc.size() && doc[p] == '"') {
        const size_t close = doc.find('"', p + 1);
        if (close == std::string::npos) return false;
        *out = doc.substr(p + 1, close - p - 1);
        return true;
    }
    size_t e = p;
    while (e < doc.size() && doc[e] != ',' && doc[e] != '\n' && doc[e] != '}' &&
           doc[e] != ' ' && doc[e] != '\r') ++e;
    *out = doc.substr(p, e - p);
    return true;
}

// repo 相对文件 (合同面只读对拍; build 目录运行也可解析)
std::string dp_repo_file(const char* rel) {
    std::string f = __FILE__;
    const std::string anchor = "/lib/astro_image_io/tests/p1hips/";
    const size_t p = f.find(anchor);
    if (p != std::string::npos)
        return slurp(f.substr(0, p) + "/" + rel);
    return slurp(rel);
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 正向组 (挂入 units 组)
// ═══════════════════════════════════════════════════════════════════════════
int test_diag_prov_units() {
    CheckState cs;

    // DP-U1 位值冻结 (§30.2) + 掩码扩展 + 合同文件只读对拍
    {
        P1HIPS_CHECK_EQ(cs, AIO_HIPS_PRODUCT_NREJ, 32);
        P1HIPS_CHECK_EQ(cs, AIO_HIPS_PRODUCT_NUSED, 64);
        P1HIPS_CHECK_EQ(cs, AIO_HIPS_PRODUCT_ALL, 7);
        P1HIPS_CHECK_EQ(cs, AIO_HIPS_PRODUCT_ALL_V19, 31);
        P1HIPS_CHECK_EQ(cs, AIO_HIPS_PRODUCT_ALL_V20, 127);
        const std::string c = dp_repo_file(
            "contracts/data/phase2_uncertainty_rejection_provenance_v1.json");
        P1HIPS_CHECK_MSG(cs, !c.empty() && c.find("\"NREJ\": 32") != std::string::npos &&
                                 c.find("\"NUSED\": 64") != std::string::npos,
                         "dp1_bits_contract",
                         "合同位值 NREJ=32/NUSED=64 与头文件不一致");
    }

    // DP-U2/DP-U3 全产品写: 六通道 + 五键 + verify 双向 + int32 回读
    {
        const std::string dir = make_tmp_dir("dp2");
        DpProduct pr;
        const int rc = dp_write_product(dir, kFullFlags, true, 1, true, &pr);
        P1HIPS_CHECK_MSG(cs, rc == 0, "dp2_write", "全产品写失败 rc=%d (%s)", rc,
                         aio_hips_last_error());
        if (rc == 0) {
            // 六子产品目录齐备
            for (const char* sub : {"signal", "support", "variance", "ivar",
                                    "nrej", "nused"})
                P1HIPS_CHECK_MSG(cs, file_has(dp_dir(dir, sub) + "/properties"),
                                 "dp2_products", "%s/properties 缺失", sub);
            // §30.3 五键 (每个 image 子产品 properties)
            for (const char* sub : {"signal", "support", "variance", "ivar",
                                    "nrej", "nused"})
                dp_check_prov_props(cs, dir, sub, "dp2_prov_keys");
            // 诊断平面 dtype 登记面 (§30.2 int32 无 precision 开关)
            for (const char* sub : {"nrej", "nused"}) {
                const auto kv = parse_props(dp_dir(dir, sub) + "/properties");
                P1HIPS_CHECK_MSG(cs,
                                 kv.count("astrocs_diag_dtype") &&
                                     kv.at("astrocs_diag_dtype") == "int32",
                                 "dp2_diag_dtype",
                                 "%s properties 缺 astrocs_diag_dtype=int32", sub);
            }
            // §30.3 manifest.json 双写 (小写键) + products 清单
            const std::string man = slurp(dir + "/manifest.json");
            struct { const char* k; const char* want; } mpairs[] = {
                {"astrocs_input_manifest_hash", kManifestHash},
                {"astrocs_model_hash", kModelHash},
                {"astrocs_uncertainty_available", "true"},
                {"astrocs_weight_mode", "2"},
                {"astrocs_reject_profile", kProfile},
            };
            for (const auto& mp : mpairs) {
                std::string v;
                const bool got = dp_json_scalar(man, mp.k, &v);
                P1HIPS_CHECK_MSG(cs, got && v == mp.want, "dp2_manifest_keys",
                                 "manifest.json %s 缺失或值不符 (got=%s want=%s)",
                                 mp.k, got ? v.c_str() : "<missing>", mp.want);
            }
            for (const char* sub : {"nrej", "nused"})
                P1HIPS_CHECK_MSG(cs,
                                 man.find(std::string("\"") + sub + "\"") !=
                                     std::string::npos,
                                 "dp2_manifest_products",
                                 "manifest.json products 未声明 %s", sub);
            // verify 双向断言 (available=true ⇒ variance/ivar 必在)
            AioHipsVerifyReport rep{};
            const int vrc = aio_hips_verify_product_set(dir.c_str(), &rep);
            P1HIPS_CHECK_MSG(cs, vrc == 0, "dp3_verify_bidir",
                             "verify rc=%d (%s)", vrc, aio_hips_last_error());
            P1HIPS_CHECK_EQ(cs, rep.prov_keys_present, 5);
            P1HIPS_CHECK_EQ(cs, rep.uncertainty_available, 1);
            P1HIPS_CHECK_EQ(cs, rep.variance_present, 1);
            P1HIPS_CHECK_EQ(cs, rep.ivar_present, 1);
            P1HIPS_CHECK_EQ(cs, rep.nrej_present, 1);
            P1HIPS_CHECK_EQ(cs, rep.nused_present, 1);
            P1HIPS_CHECK_EQ(cs, rep.n_signal_tiles, 1);
            P1HIPS_CHECK_EQ(cs, rep.n_nrej_tiles, 1);
            P1HIPS_CHECK_EQ(cs, rep.diag_negative_pixels, 0);
            P1HIPS_CHECK_EQ(cs, rep.value_mismatch, 0);
            // §30.2 int32 平面独立 oracle 回读 (BITPIX=32 + 逐像素期望)
            const std::string rel = "/Norder0/Dir0/Npix0.fits";
            const std::vector<int32_t> exp_nused = dp_expected_nused();
            const std::vector<int32_t> exp_nrej = dp_expected_nrej();
            int bp = 0;
            std::vector<int32_t> got;
            const bool ok_n = oracle_read_i32(dir + "/nused" + rel, &bp, &got);
            P1HIPS_CHECK_MSG(cs, ok_n && bp == 32, "dp3_int32_dtype",
                             "nused tile BITPIX 必须 =32 (got %d, read=%d)", bp,
                             (int)ok_n);
            if (ok_n) {
                size_t mism = 0;
                for (size_t i = 0; i < got.size(); ++i)
                    if (got[i] != exp_nused[i]) ++mism;
                P1HIPS_CHECK_MSG(cs, mism == 0 && got.size() == (size_t)kSpan,
                                 "dp3_int32_values",
                                 "nused 平面与独立期望不符 (mismatch=%zu/%u)",
                                 mism, kSpan);
            }
            const bool ok_r = oracle_read_i32(dir + "/nrej" + rel, &bp, &got);
            P1HIPS_CHECK_MSG(cs, ok_r && bp == 32, "dp3_int32_dtype",
                             "nrej tile BITPIX 必须 =32 (got %d, read=%d)", bp,
                             (int)ok_r);
            if (ok_r) {
                size_t mism = 0, nrej_sum = 0;
                for (size_t i = 0; i < got.size(); ++i) {
                    if (got[i] != exp_nrej[i]) ++mism;
                    nrej_sum += (size_t)got[i];
                }
                P1HIPS_CHECK_MSG(cs, mism == 0, "dp3_int32_values",
                                 "nrej 平面与独立期望不符 (mismatch=%zu)", mism);
                // void 角落 0/0 (0 即"无", 禁 −1 哨兵) —— 与 nused 同点对拍
                const size_t void_fi = (size_t)480 * 512 + 480;
                P1HIPS_CHECK_MSG(cs, got[void_fi] == 0, "dp3_void_zero",
                                 "void 像素 nrej 必须 =0 (got %d)", got[void_fi]);
                P1HIPS_CHECK_MSG(cs, nrej_sum > 0, "dp3_rej_surface",
                                 "fixture 必须产生非空拒绝面 (判别力守卫)");
            }
            // 被测 reader 通道: int32 读取 + 产品枚举 + DATASUM 在位
            {
                AioHipsDataset* dnr = aio_hips_open(dir.c_str(), AIO_HIPS_RD_NREJ);
                P1HIPS_CHECK_MSG(cs, dnr != nullptr, "dp3_reader_open",
                                 "AIO_HIPS_RD_NREJ 打开失败: %s",
                                 aio_hips_reader_last_error());
                if (dnr) {
                    P1HIPS_CHECK_EQ(cs, aio_hips_tile_count(dnr), 1);
                    uint64_t ipix = 0;
                    P1HIPS_CHECK_EQ(cs, aio_hips_tile_ipix(dnr, 0, &ipix), 0);
                    std::vector<int32_t> buf((size_t)kSpan, 0);
                    P1HIPS_CHECK_EQ(cs, aio_hips_read_tile_i32(dnr, ipix, buf.data()), 0);
                    size_t mism = 0;
                    for (size_t i = 0; i < buf.size(); ++i)
                        if (buf[i] != exp_nrej[i]) ++mism;
                    P1HIPS_CHECK_MSG(cs, mism == 0, "dp3_reader_values",
                                     "reader int32 通道 mismatch=%zu", mism);
                    aio_hips_close(dnr);
                }
                // 浮点子产品经 int32 通道读取必须拒绝 (-6: dtype 契约)
                AioHipsDataset* dsig = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SIGNAL);
                if (dsig) {
                    std::vector<int32_t> buf((size_t)kSpan, 0);
                    P1HIPS_CHECK_EQ(cs, aio_hips_read_tile_i32(dsig, 0, buf.data()), -6);
                    aio_hips_close(dsig);
                }
            }
        }
    }

    // DP-U4 unavailable 面: 五键齐全且 =false, variance/ivar 不落盘, verify rc=0
    {
        const std::string dir = make_tmp_dir("dp4");
        const int rc = dp_write_product(dir, kDiagOnlyFlags, true, 0, true, nullptr);
        P1HIPS_CHECK_MSG(cs, rc == 0, "dp4_write", "unavailable 面写失败 rc=%d", rc);
        if (rc == 0) {
            const auto kv = parse_props(dir + "/signal/properties");
            P1HIPS_CHECK_MSG(cs,
                             kv.count("ASTROCS_UNCERTAINTY_AVAILABLE") &&
                                 kv.at("ASTROCS_UNCERTAINTY_AVAILABLE") == "false",
                             "dp4_unavail_key",
                             "unavailable 必须显式登记 =false (§18.3/§30.3)");
            P1HIPS_CHECK_MSG(cs, !file_has(dir + "/variance/properties") &&
                                     !file_has(dir + "/ivar/properties"),
                             "dp4_no_placeholder",
                             "unavailable 面禁写 variance/ivar 占位子产品");
            AioHipsVerifyReport rep{};
            const int vrc = aio_hips_verify_product_set(dir.c_str(), &rep);
            P1HIPS_CHECK_MSG(cs, vrc == 0, "dp4_verify", "unavailable verify rc=%d (%s)",
                             vrc, aio_hips_last_error());
            P1HIPS_CHECK_EQ(cs, rep.uncertainty_available, 0);
            P1HIPS_CHECK_EQ(cs, rep.variance_present, 0);
            P1HIPS_CHECK_EQ(cs, rep.ivar_present, 0);
            P1HIPS_CHECK_EQ(cs, rep.nrej_present, 1);
        }
    }

    // DP-U5 legacy 面: 未调用 setter → 五键整体不写 (全或无, P1 产品面不变)
    {
        const std::string dir = make_tmp_dir("dp5");
        const int rc = dp_write_product(dir,
                                        AIO_HIPS_PRODUCT_SIGNAL |
                                            AIO_HIPS_PRODUCT_SUPPORT,
                                        false, 0, false, nullptr);
        P1HIPS_CHECK_MSG(cs, rc == 0, "dp5_write", "legacy 面写失败 rc=%d", rc);
        if (rc == 0) {
            const auto kv = parse_props(dir + "/signal/properties");
            int present = 0;
            for (const char* k : kProvKeys)
                if (kv.count(k)) ++present;
            P1HIPS_CHECK_MSG(cs, present == 0, "dp5_all_or_none",
                             "未设置 provenance 时五键必须整体缺席 (got %d)",
                             present);
            AioHipsVerifyReport rep{};
            P1HIPS_CHECK_EQ(cs, aio_hips_verify_product_set(dir.c_str(), &rep), 0);
            P1HIPS_CHECK_EQ(cs, rep.prov_keys_present, 0);
            P1HIPS_CHECK_EQ(cs, rep.uncertainty_available, -1);
        }
    }

    // DP-U6 确定性: 双跑诊断平面逐字节 bitwise 相等 (§30.2 无时间戳/无随机源)
    {
        const std::string d0 = make_tmp_dir("dp6a");
        const std::string d1 = make_tmp_dir("dp6b");
        const int r0 = dp_write_product(d0, kDiagOnlyFlags, true, 0, true, nullptr);
        const int r1 = dp_write_product(d1, kDiagOnlyFlags, true, 0, true, nullptr);
        P1HIPS_CHECK(cs, r0 == 0 && r1 == 0, "dp6_write");
        if (r0 == 0 && r1 == 0) {
            const std::string rel = "/Norder0/Dir0/Npix0.fits";
            const std::string a = slurp(d0 + "/nrej" + rel);
            const std::string b = slurp(d1 + "/nrej" + rel);
            P1HIPS_CHECK_MSG(cs, !a.empty() && a == b, "dp6_bitwise",
                             "nrej tile 双跑必须 bitwise 相等 (size %zu vs %zu)",
                             a.size(), b.size());
            const std::string c = slurp(d0 + "/nused" + rel);
            const std::string e = slurp(d1 + "/nused" + rel);
            P1HIPS_CHECK_MSG(cs, !c.empty() && c == e, "dp6_bitwise",
                             "nused tile 双跑必须 bitwise 相等");
            // 五键行文本确定性 (properties 内 UTC 日期键除外)
            const std::string p0 = slurp(d0 + "/signal/properties");
            const std::string p1 = slurp(d1 + "/signal/properties");
            size_t p = 0;
            bool all_eq = true;
            while (true) {
                const size_t nl = p0.find('\n', p);
                if (nl == std::string::npos) break;
                const std::string line = p0.substr(p, nl - p);
                p = nl + 1;
                if (line.rfind("ASTROCS_", 0) != 0) continue;
                if (p1.find(line + "\n") == std::string::npos) all_eq = false;
            }
            P1HIPS_CHECK_MSG(cs, all_eq, "dp6_prov_lines",
                             "五键行文本必须跨运行确定");
        }
    }

    if (cs.failures == 0) {
        std::fprintf(stdout,
                     "[p1hips] diag_prov units: DP-U1..DP-U6 PASS "
                     "(§30.2 int32 通道 + §30.3 五键双写 + verify 双向)\n");
        return 0;
    }
    std::fprintf(stderr, "[p1hips] diag_prov units: %d check(s) failed\n",
                 cs.failures);
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════════
// 负向组 (挂入 negative 组)
// ═══════════════════════════════════════════════════════════════════════════
int test_diag_prov_negative() {
    CheckState cs;

    // DP-N1 provenance setter 参数域 (§30.3 禁伪造: 非 64hex/越界值拒绝)
    {
        const std::string dir = make_tmp_dir("dpn1");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT32,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "dpn1_begin");
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(nullptr, kManifestHash,
                                                        kModelHash, 1, 2, kProfile), 1);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, "short", kModelHash, 1,
                                                        2, kProfile), 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, nullptr, kModelHash, 1,
                                                        2, kProfile), 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, kManifestHash, "", 1, 2,
                                                        kProfile), 2);
            // 非 hex (64 长度但含 'z')
            std::string bad(64, 'z');
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, bad.c_str(), kModelHash,
                                                        1, 2, kProfile), 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, kManifestHash, kModelHash,
                                                        2, 2, kProfile), 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, kManifestHash, kModelHash,
                                                        1, 3, kProfile), 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, kManifestHash, kModelHash,
                                                        1, 2, nullptr), 2);
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, kManifestHash, kModelHash,
                                                        1, 2, ""), 2);
            // 参数非法后必须保持"未设置" (得不到半套 provenance)
            P1HIPS_CHECK_EQ(cs, aio_hips_finalize(ps), 0);
            const auto kv = parse_props(dir + "/signal/properties");
            int present = 0;
            for (const char* k : kProvKeys)
                if (kv.count(k)) ++present;
            P1HIPS_CHECK_MSG(cs, present == 0, "dpn1_no_partial",
                             "setter 失败后禁写部分 provenance 键 (got %d)", present);
        }
    }

    // DP-N2 finalize 双向守卫 (§30.1/§30.3: available 与产品位必须一致)
    {
        const std::string dir = make_tmp_dir("dpn2a");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT32,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
            "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, kManifestHash, kModelHash,
                                                        1, 2, kProfile), 0);
            FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 1.5, true, true,
                                            AIO_HIPS_FLOAT32);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            // available=true 但未置 variance/ivar 位 → finalize 必须 fail-closed
            P1HIPS_CHECK_MSG(cs, aio_hips_finalize(ps) == -9, "dpn2_guard_true",
                             "available=true 缺 variance/ivar 位必须 finalize 拒绝");
        } else {
            P1HIPS_CHECK(cs, false, "dpn2_begin");
        }
    }
    {
        const std::string dir = make_tmp_dir("dpn2b");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT32,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
                AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR,
            "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        if (ps) {
            P1HIPS_CHECK_EQ(cs, aio_hips_set_provenance(ps, kManifestHash, kModelHash,
                                                        0, 2, kProfile), 0);
            FixViewF64 fx = fix_hips_a_tile(0, 10.0, 0.5, 1.5, true, true,
                                            AIO_HIPS_FLOAT32);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_signal_support_tile(ps, &fx.view), 0);
            // available=false 却置 variance/ivar 位 → 禁占位, finalize 必须拒绝
            P1HIPS_CHECK_MSG(cs, aio_hips_finalize(ps) == -10, "dpn2_guard_false",
                             "available=false 置 variance/ivar 位必须 finalize 拒绝");
        } else {
            P1HIPS_CHECK(cs, false, "dpn2_begin");
        }
    }

    // DP-N3 write_diag_tile 参数域/值域 (§30.2)
    {
        const std::string dir = make_tmp_dir("dpn3");
        AioHipsProductSet* ps = aio_hips_product_begin(
            dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT32,
            AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_NREJ,
            "ivo://t", "t", nullptr, 0.0, nullptr, 0);
        P1HIPS_CHECK(cs, ps != nullptr, "dpn3_begin");
        if (ps) {
            std::vector<int32_t> ok((size_t)kSpan, 0);
            AioHipsDiagTileView dv{};
            dv.parent_ipix = 0;
            dv.leaf_order = FIX_LEAF_ORDER;
            dv.width = 512;
            dv.nrej = ok.data();
            dv.nused = ok.data();
            P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(nullptr, &dv), -1);
            P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(ps, nullptr), -1);
            AioHipsDiagTileView bad = dv;
            bad.width = 1024;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(ps, &bad), -2);
            bad = dv;
            bad.leaf_order = 10;
            P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(ps, &bad), -2);
            bad = dv;
            bad.parent_ipix = 12ULL * (1ULL << (2ULL * 0));   // ≥ Norder0 上限
            P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(ps, &bad), -3);
            bad = dv;
            bad.nrej = nullptr;                 // 已启用位无数据 → 禁静默跳过
            P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(ps, &bad), -2);
            std::vector<int32_t> neg((size_t)kSpan, 0);
            neg[7] = -1;                        // 禁 −1 哨兵
            bad = dv;
            bad.nrej = neg.data();
            P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(ps, &bad), -5);
            P1HIPS_CHECK_MSG(cs, aio_hips_last_error() && *aio_hips_last_error(),
                             "dpn3_last_error", "负面返回后 last_error 必须非空");
            aio_hips_abort(ps);
        }
        // flags 未启用诊断位 → -4
        {
            const std::string d2 = make_tmp_dir("dpn3b");
            AioHipsProductSet* p2 = aio_hips_product_begin(
                d2.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT32,
                AIO_HIPS_PRODUCT_SIGNAL, "ivo://t", "t", nullptr, 0.0, nullptr, 0);
            if (p2) {
                std::vector<int32_t> z((size_t)kSpan, 0);
                AioHipsDiagTileView dv2{};
                dv2.parent_ipix = 0;
                dv2.leaf_order = FIX_LEAF_ORDER;
                dv2.width = 512;
                dv2.nrej = z.data();
                dv2.nused = z.data();
                P1HIPS_CHECK_EQ(cs, aio_hips_write_diag_tile(p2, &dv2), -4);
                aio_hips_abort(p2);
            } else {
                P1HIPS_CHECK(cs, false, "dpn3b_begin");
            }
        }
    }

    // DP-N4 verify 双向违反逐条必败 (§30.1/§30.2/§30.3)
    {
        // a) unavailable 面 + variance 占位 → rc=3
        const std::string d = make_tmp_dir("dpn4a");
        P1HIPS_CHECK_EQ(cs, dp_write_product(d, kDiagOnlyFlags, true, 0, true, nullptr), 0);
        {
            std::string err;
            ::mkdir((d + "/variance").c_str(), 0755);
            std::ofstream f(d + "/variance/properties");
            f << "hips_version=1.4\nhips_order=0\nhips_tile_width=512\n";
        }
        AioHipsVerifyReport rep{};
        const int vrc = aio_hips_verify_product_set(d.c_str(), &rep);
        P1HIPS_CHECK_MSG(cs, vrc == 3, "dpn4_placeholder_forbidden",
                         "unavailable 但存在 variance 占位必须 rc=3 (got %d)", vrc);
    }
    {
        // b) available 面删 variance → rc=2
        const std::string d = make_tmp_dir("dpn4b");
        P1HIPS_CHECK_EQ(cs, dp_write_product(d, kFullFlags, true, 1, true, nullptr), 0);
        std::remove((d + "/variance/properties").c_str());
        AioHipsVerifyReport rep{};
        P1HIPS_CHECK_MSG(cs, aio_hips_verify_product_set(d.c_str(), &rep) == 2,
                         "dpn4_hdu_missing",
                         "available=true 缺 variance 子产品必须 rc=2");
    }
    {
        // c) 声明 nrej 但目录缺失 → rc=5
        const std::string d = make_tmp_dir("dpn4c");
        P1HIPS_CHECK_EQ(cs, dp_write_product(d, kDiagOnlyFlags, true, 0, true, nullptr), 0);
        std::remove((d + "/nrej/properties").c_str());
        AioHipsVerifyReport rep{};
        P1HIPS_CHECK_MSG(cs, aio_hips_verify_product_set(d.c_str(), &rep) == 5,
                         "dpn4_declared_missing",
                         "manifest 声明 nrej 但磁盘缺失必须 rc=5");
    }
    {
        // d) 未声明但磁盘存在 nused → rc=6 (禁占位)
        const std::string d = make_tmp_dir("dpn4d");
        P1HIPS_CHECK_EQ(cs, dp_write_product(d,
                                             AIO_HIPS_PRODUCT_SIGNAL |
                                                 AIO_HIPS_PRODUCT_SUPPORT,
                                             true, 0, false, nullptr), 0);
        ::mkdir((d + "/nused").c_str(), 0755);
        {
            std::ofstream f(d + "/nused/properties");
            f << "hips_version=1.4\nhips_order=0\nhips_tile_width=512\n";
        }
        AioHipsVerifyReport rep{};
        P1HIPS_CHECK_MSG(cs, aio_hips_verify_product_set(d.c_str(), &rep) == 6,
                         "dpn4_undeclared_present",
                         "未声明却存在 nused 子产品必须 rc=6 (禁占位)");
    }
    {
        // e) 五键缺一 → rc=4 (禁静默缺键)
        const std::string d = make_tmp_dir("dpn4e");
        P1HIPS_CHECK_EQ(cs, dp_write_product(d, kDiagOnlyFlags, true, 0, true, nullptr), 0);
        const std::string pp = d + "/signal/properties";
        const std::string body = slurp(pp);
        std::ofstream f(pp, std::ios::binary | std::ios::trunc);
        std::string line;
        std::istringstream is(body);
        while (std::getline(is, line)) {
            if (line.rfind("ASTROCS_MODEL_HASH", 0) == 0) continue;
            f << line << "\n";
        }
        f.close();
        AioHipsVerifyReport rep{};
        P1HIPS_CHECK_MSG(cs, aio_hips_verify_product_set(d.c_str(), &rep) == 4,
                         "dpn4_partial_keys",
                         "五键缺一必须 rc=4");
    }
    {
        // f) properties↔manifest 值分叉 → rc=8 (双写面禁止分叉)
        const std::string d = make_tmp_dir("dpn4f");
        P1HIPS_CHECK_EQ(cs, dp_write_product(d, kDiagOnlyFlags, true, 0, true, nullptr), 0);
        const std::string mp = d + "/manifest.json";
        std::string man = slurp(mp);
        const size_t k = man.find(kModelHash);
        if (k != std::string::npos) man.replace(k, std::strlen(kModelHash),
                                                std::string(64, '0'));
        {
            std::ofstream f(mp, std::ios::binary | std::ios::trunc);
            f << man;
        }
        AioHipsVerifyReport rep{};
        P1HIPS_CHECK_MSG(cs, aio_hips_verify_product_set(d.c_str(), &rep) == 8,
                         "dpn4_double_write_drift",
                         "properties/manifest 值分叉必须 rc=8");
        P1HIPS_CHECK_EQ(cs, rep.value_mismatch, 1);
    }
    {
        // g) 空目录/NULL 参数 → -1
        const std::string d = make_tmp_dir("dpn4g");
        AioHipsVerifyReport rep{};
        P1HIPS_CHECK_EQ(cs, aio_hips_verify_product_set(d.c_str(), &rep), -1);
        P1HIPS_CHECK_EQ(cs, aio_hips_verify_product_set(nullptr, &rep), -1);
        P1HIPS_CHECK_EQ(cs, aio_hips_verify_product_set(d.c_str(), nullptr), -1);
    }

    if (cs.failures == 0) {
        std::fprintf(stdout,
                     "[p1hips] diag_prov negative: DP-N1..DP-N4 PASS "
                     "(setter 参数域 + finalize 双向守卫 + diag 值域 + verify 违反面)\n");
        return 0;
    }
    std::fprintf(stderr, "[p1hips] diag_prov negative: %d check(s) failed\n",
                 cs.failures);
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════════
// 库级故障注入自检 (挂入 selfcheck 可执行): 基线必 PASS + 注入必 FAIL
// ═══════════════════════════════════════════════════════════════════════════
namespace {

// 场景 1: 全产品正向 (五键 + verify rc=0)
int dp_scenario_full(CheckState& cs) {
    const std::string dir = make_tmp_dir("dps1");
    const int rc = dp_write_product(dir, kFullFlags, true, 1, true, nullptr);
    P1HIPS_CHECK_MSG(cs, rc == 0, nullptr, "场景写失败 rc=%d", rc);
    if (rc != 0) return cs.failures;
    dp_check_prov_props(cs, dir, "signal", nullptr);
    dp_check_prov_props(cs, dir, "nrej", nullptr);
    const std::string man = slurp(dir + "/manifest.json");
    std::string v;
    P1HIPS_CHECK_MSG(cs,
                     dp_json_scalar(man, "astrocs_model_hash", &v) && v == kModelHash,
                     nullptr, "manifest.model_hash 缺失或值不符");
    AioHipsVerifyReport rep{};
    P1HIPS_CHECK_MSG(cs, aio_hips_verify_product_set(dir.c_str(), &rep) == 0, nullptr,
                     "verify 正向必须 rc=0");
    return cs.failures;
}

// 场景 2: unavailable 面 + 占位检测 (期望 rc=3)
int dp_scenario_placeholder(CheckState& cs) {
    const std::string dir = make_tmp_dir("dps2");
    const int rc = dp_write_product(dir, kDiagOnlyFlags, true, 0, true, nullptr);
    P1HIPS_CHECK_MSG(cs, rc == 0, nullptr, "场景写失败 rc=%d", rc);
    if (rc != 0) return cs.failures;
    ::mkdir((dir + "/variance").c_str(), 0755);
    {
        std::ofstream f(dir + "/variance/properties");
        f << "hips_version=1.4\nhips_order=0\nhips_tile_width=512\n";
    }
    AioHipsVerifyReport rep{};
    const int vrc = aio_hips_verify_product_set(dir.c_str(), &rep);
    P1HIPS_CHECK_MSG(cs, vrc == 3, nullptr,
                     "unavailable 占位必须被 verify 拒绝 (got %d)", vrc);
    return cs.failures;
}

// 场景 3: 诊断平面值域回读 (期望全 >= 0 且与独立期望一致)
int dp_scenario_diag_values(CheckState& cs) {
    const std::string dir = make_tmp_dir("dps3");
    const int rc = dp_write_product(dir, kDiagOnlyFlags, true, 0, true, nullptr);
    P1HIPS_CHECK_MSG(cs, rc == 0, nullptr, "场景写失败 rc=%d", rc);
    if (rc != 0) return cs.failures;
    int bp = 0;
    std::vector<int32_t> got;
    const bool ok = oracle_read_i32(dir + "/nrej/Norder0/Dir0/Npix0.fits", &bp, &got);
    P1HIPS_CHECK_MSG(cs, ok && bp == 32, nullptr, "nrej tile 不可读或 BITPIX≠32");
    if (ok) {
        const std::vector<int32_t> exp = dp_expected_nrej();
        size_t mism = 0, neg = 0;
        for (size_t i = 0; i < got.size(); ++i) {
            if (got[i] != exp[i]) ++mism;
            if (got[i] < 0) ++neg;
        }
        P1HIPS_CHECK_MSG(cs, mism == 0, nullptr, "nrej 平面 mismatch=%zu", mism);
        P1HIPS_CHECK_MSG(cs, neg == 0, nullptr, "nrej 平面出现负值 (哨兵污染)");
    }
    AioHipsVerifyReport rep{};
    P1HIPS_CHECK_MSG(cs, aio_hips_verify_product_set(dir.c_str(), &rep) == 0, nullptr,
                     "verify 正向必须 rc=0 (含诊断值域)");
    return cs.failures;
}

// 场景 4: manifest 声明 nrej 但磁盘缺失 (期望 rc=5)
int dp_scenario_declared_missing(CheckState& cs) {
    const std::string dir = make_tmp_dir("dps4");
    const int rc = dp_write_product(dir, kDiagOnlyFlags, true, 0, true, nullptr);
    P1HIPS_CHECK_MSG(cs, rc == 0, nullptr, "场景写失败 rc=%d", rc);
    if (rc != 0) return cs.failures;
    std::remove((dir + "/nrej/properties").c_str());
    AioHipsVerifyReport rep{};
    const int vrc = aio_hips_verify_product_set(dir.c_str(), &rep);
    P1HIPS_CHECK_MSG(cs, vrc == 5, nullptr,
                     "声明但缺失必须被 verify 拒绝 (got %d)", vrc);
    return cs.failures;
}

struct DpFaultCase {
    const char* env;
    const char* value;
    int (*scenario)(CheckState&);
    const char* name;
};

}  // namespace

int test_diag_prov_selfcheck() {
    // 基线对照: 无注入时三场景必 PASS (排除恒 FAIL 侧)
    struct { int (*fn)(CheckState&); const char* name; } base[] = {
        {dp_scenario_full, "full"},
        {dp_scenario_placeholder, "placeholder"},
        {dp_scenario_diag_values, "diag_values"},
        {dp_scenario_declared_missing, "declared_missing"},
    };
    for (const auto& b : base) {
        CheckState cs;
        const int f = b.fn(cs);
        if (f != 0) {
            std::fprintf(stderr,
                         "SELFCHECK(diag_prov): baseline '%s' FAIL (%d) — 恒 FAIL 侧不通过\n",
                         b.name, f);
            return 1;
        }
    }
    std::fprintf(stdout,
                 "SELFCHECK(diag_prov) baseline: 4 场景 PASS (非恒 FAIL)\n");

    // 注入面: 库级等价缺陷 (env) → 对应场景必 FAIL
    const DpFaultCase faults[] = {
        {"ASTROCS_HIPS_PROV_FAULT", "missing_key", dp_scenario_full,
         "prov_missing_key→五键断言"},
        {"ASTROCS_HIPS_PROV_FAULT", "value_drift", dp_scenario_full,
         "prov_value_drift→verify rc=8"},
        {"ASTROCS_HIPS_DIAG_FAULT", "sentinel", dp_scenario_diag_values,
         "diag_sentinel→值域/verify rc=7"},
        {"ASTROCS_HIPS_DIAG_FAULT", "skip_write", dp_scenario_diag_values,
         "diag_skip_write→声明但零 tile 落盘 (值域回读 + verify V4)"},
        {"ASTROCS_HIPS_VERIFY_FAULT", "shortcut", dp_scenario_placeholder,
         "verify_shortcut→占位漏检"},
    };
    for (const auto& fc : faults) {
        ::setenv(fc.env, fc.value, 1);
        CheckState cs;
        const int f = fc.scenario(cs);
        ::unsetenv(fc.env);
        if (f == 0) {
            std::fprintf(stderr,
                         "SELFCHECK(diag_prov): 注入 %s=%s 后场景仍 PASS — 恒 PASS 占位/注入失效\n",
                         fc.env, fc.value);
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK(diag_prov): inject %s=%s → FAIL(%d) 必败验证通过 (%s)\n",
                     fc.env, fc.value, f, fc.name);
    }
    std::fprintf(stdout,
                 "P1HIPS SELFCHECK(diag_prov) PASS (baseline + 5 注入点双向验证)\n");
    return 0;
}

}  // namespace p1hips
