// eng/tests/unit/p3_verify_reopen_test.cpp — Issue #94
// 生产 FITS 写出/独立重开 API 的定向单测：
//   * P3FitsVerifyStream 在数据/WCS 不符时仍返回 P3_OUT_OK，但 result.reopen_ok=0
//     （即"低层 API 成功 ≠ 产品通过验证"这一最易被误当成功的形态）；
//   * canonical 哈希对非 WCS/非像素头卡（SWVER）敏感，而重开判据不敏感 ⇒
//     canonical_match 必须由调用方独立消费。
// 链接当前构建的生产库（不复制算法）。
#include "p3_output.h"
#include "p3_wcs.h"
#include "acsd/core/canonical_hash.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace acsd::phase3;

static int g_total = 0, g_fail = 0;
static void check(bool ok, const std::string& what, const std::string& detail = "") {
    ++g_total;
    if (!ok) {
        ++g_fail;
        std::printf("FAIL %s %s\n", what.c_str(), detail.c_str());
    } else {
        std::printf("PASS %s\n", what.c_str());
    }
}

// 读整文件并定位数据起点（END 卡所在 2880 记录的下一记录）。
static bool read_file(const std::string& p, std::string* b) {
    std::ifstream in(p, std::ios::binary);
    if (!in.good()) return false;
    *b = std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    return true;
}
static bool write_file(const std::string& p, const std::string& b) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out.good()) return false;
    out.write(b.data(), (std::streamsize)b.size());
    return out.good();
}
static size_t data_start(const std::string& b) {
    for (size_t blk = 0; blk + 2880u <= b.size() && blk < 2880u * 100u; blk += 2880u)
        for (size_t c = 0; c < 36u; ++c) {
            const char* card = b.data() + blk + c * 80u;
            if (card[0] == 'E' && card[1] == 'N' && card[2] == 'D' && card[3] == ' ')
                return blk + 2880u;
        }
    return 0;
}
static bool flip_data_byte(const std::string& p) {
    std::string b;
    if (!read_file(p, &b)) return false;
    const size_t off = data_start(b);
    if (off == 0 || off >= b.size()) return false;
    b[off] = (char)((unsigned char)b[off] ^ 0x01u);
    return write_file(p, b);
}
static bool flip_card_value(const std::string& p, const char* key) {
    std::string b;
    if (!read_file(p, &b)) return false;
    const size_t ds = data_start(b);
    if (ds == 0) return false;
    for (size_t off = 0; off + 80u <= ds; off += 80u) {
        char* card = &b[off];
        std::string k(card, 8);
        while (!k.empty() && k.back() == ' ') k.pop_back();
        if (k != key) continue;
        for (size_t i = 10; i < 80u; ++i) {
            const char ch = card[i];
            if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
                (ch >= 'a' && ch <= 'z')) {
                card[i] = (ch == '9') ? '8' : (char)(ch + 1);
                return write_file(p, b);
            }
        }
    }
    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <tmp_dir>\n", argv[0]);
        return 64;
    }
    const std::string dir = argv[1];
    const int W = 64, H = 64;
    P3WcsDescriptor wcs{};
    if (p3_wcs_make(0.0, 0.0, 0.01, W, H, "east_left", 0.0, &wcs, "TAN") != P3_WCS_OK) {
        std::fprintf(stderr, "p3_wcs_make failed\n");
        return 1;
    }
    std::vector<float> sig((size_t)W * H), cov((size_t)W * H, 1.0f);
    for (size_t i = 0; i < sig.size(); ++i) sig[i] = 1000.0f + (float)(i % 97);
    P3Provenance prov{};
    prov.hips_id = "ivo://acsd/test";
    prov.manifest_hash = "deadbeef";
    prov.missing_tiles = nullptr;
    prov.missing_count = 0;
    prov.software_version = "test";
    prov.run_id = "run-test";
    prov.order_sel_used = "0";
    prov.sampler_used = "nearest";
    prov.uncertainty_source = nullptr;
    prov.uncertainty_missing_pixels = 0;
    prov.input_completeness = "VERIFIED";
    prov.coverage_verified = 1;

    auto write = [&](const std::string& p) {
        P3OutputResult r{};
        const P3OutputStatus st = p3_output_write_atomic_ex(
            sig.data(), cov.data(), nullptr, nullptr, W, H, &wcs, "ADU/sr",
            p.c_str(), &prov, -32, -1, &r);
        return std::make_pair(st, r);
    };
    auto verify = [&](const std::string& p, P3OutputResult* out) {
        P3FitsVerifyStream vs;
        P3OutputStatus st = vs.open(p.c_str(), &wcs, W, H, false);
        if (st == P3_OUT_OK)
            st = vs.check_block(0, 0, 0, W, H, sig.data());
        if (st == P3_OUT_OK) st = vs.close(out);
        return st;
    };

    const std::string p = dir + "/output_phase3.fits";
    // 1) 正常写出 + 重开
    {
        auto wr = write(p);
        check(wr.first == P3_OUT_OK, "write_atomic_ex OK");
        check(wr.second.reopen_ok == 1, "written product reopen_ok==1");
        P3OutputResult vr{};
        const P3OutputStatus st = verify(p, &vr);
        check(st == P3_OUT_OK && vr.reopen_ok == 1, "matching verify reopen_ok==1");
    }
    // 2) 像元改变：低层 close 仍 OK，但 reopen_ok=0（Issue #94 核心判据）
    {
        auto wr = write(p);
        (void)wr;
        check(flip_data_byte(p), "tamper pixel byte");
        P3OutputResult vr{};
        const P3OutputStatus st = verify(p, &vr);
        check(st == P3_OUT_OK, "pixel-changed: verify close()==P3_OUT_OK");
        check(vr.reopen_ok == 0, "pixel-changed: reopen_ok==0",
              "got " + std::to_string(vr.reopen_ok));
    }
    // 3) WCS 改变：同样 close()==OK 且 reopen_ok=0
    {
        auto wr = write(p);
        (void)wr;
        check(flip_card_value(p, "CRVAL1"), "tamper CRVAL1");
        P3OutputResult vr{};
        const P3OutputStatus st = verify(p, &vr);
        check(st == P3_OUT_OK, "wcs-changed: verify close()==P3_OUT_OK");
        check(vr.reopen_ok == 0, "wcs-changed: reopen_ok==0",
              "got " + std::to_string(vr.reopen_ok));
    }
    // 4) canonical-only 改变（SWVER）：重开仍 OK，但 canonical 哈希变化
    {
        auto wr = write(p);
        (void)wr;
        const auto c0 = acsd::core::canonical_product_hash_file(p);
        check(c0.ok, "canonical hash before tamper");
        check(flip_card_value(p, "SWVER"), "tamper SWVER");
        const auto c1 = acsd::core::canonical_product_hash_file(p);
        check(c1.ok, "canonical hash after tamper");
        check(c0.canonical_sha256 != c1.canonical_sha256,
              "SWVER tamper changes canonical hash");
        P3OutputResult vr{};
        const P3OutputStatus st = verify(p, &vr);
        check(st == P3_OUT_OK && vr.reopen_ok == 1,
              "SWVER tamper leaves reopen_ok==1 (canonical must be consumed separately)");
    }
    // 5) 缺 COVSTAT/COVVERIF 的旧产物：读回 API 必须显式不可判（P3_OUT_PARAM）
    {
        auto wr = write(p);
        (void)wr;
        std::string b;
        read_file(p, &b);
        const size_t ds = data_start(b);
        // 把 COVSTAT 关键字改名（保留 80 字节卡结构）
        bool done = false;
        for (size_t off = 0; off + 80u <= ds && !done; off += 80u) {
            if (std::strncmp(b.data() + off, "COVSTAT ", 8) == 0) {
                std::memcpy(&b[off], "COVSTA0 ", 8);
                done = true;
            }
        }
        check(done, "rename COVSTAT card");
        write_file(p, b);
        char cs[64] = {0};
        int cv = 0;
        const P3OutputStatus st =
            p3_output_read_coverage_identity(p.c_str(), cs, (int)sizeof(cs), &cv);
        check(st == P3_OUT_PARAM, "missing COVSTAT => P3_OUT_PARAM (not silent VERIFIED)");
    }

    std::printf("%d/%d checks passed%s\n", g_total - g_fail, g_total,
                g_fail == 0 ? "" : " (FAILURES)");
    return g_fail == 0 ? 0 : 1;
}
