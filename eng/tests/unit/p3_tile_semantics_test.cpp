// eng/tests/unit/p3_tile_semantics_test.cpp — Issue #87 tile 完整性分类/缓存/聚合单测
//
// 被测对象 = 生产实现：
//   * lib/infrastructure/aio/src/hips/aio_hips_reader.cpp 的 *_ex 分类与覆盖身份
//   * lib/algorithms/resample/p3_resample.cpp 的 fetch/read_leaf/采样/错误缓存/聚合
//
// 期望来源（非被测实现自身）：
//   * docs/engineering/contracts/HIPS_IO.md「状态与缺覆盖」
//   * docs/science/algorithms/export/PHASE3_RESAMPLE.md「边界/NaN/Inf」与 #87 合同
//   * 受控夹具的已知字节事实（make_fixtures.py：MOC/CHECKSUM/bitflip/截断/目录）
//
// 负例必须有牙齿：把分类退回"任何非 0 都是缺 tile"，本测试立即变红。
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "p3_resample.h"

namespace {

int g_fail = 0;
int g_total = 0;

void check(bool ok, const std::string& what, const std::string& detail = "") {
    ++g_total;
    if (!ok) {
        ++g_fail;
        std::printf("[FAIL] %s%s%s\n", what.c_str(),
                    detail.empty() ? "" : " :: ", detail.c_str());
    }
}

std::string fixture(const std::string& root, const char* name) {
    return root + "/" + name;
}

struct Sample {
    int status = 0;
    float value = 0;
    int coverage = 0;
};

Sample nearest(acsd::phase3::P3Sampler* s, double ra, double dec) {
    Sample out;
    out.status = (int)acsd::phase3::p3_sample_nearest(s, ra, dec, &out.value,
                                                     &out.coverage);
    return out;
}

void expect_error(acsd::phase3::P3Sampler* s, double ra, double dec,
                  int expect_code, uint64_t expect_tile, const char* what) {
    using namespace acsd::phase3;
    const Sample r = nearest(s, ra, dec);
    const int expect_status = (expect_code == P3_TILE_IO_ERROR) ? P3_RS_IO
                                                                : P3_RS_INTEGRITY;
    check(r.status == expect_status, std::string(what) + " status",
          "got " + std::to_string(r.status) + " expect " + std::to_string(expect_status));
    P3TileFailure f{};
    const int got = p3_sampler_last_tile_failure(s, &f);
    check(got == 1, std::string(what) + " last_failure present");
    check(f.code == expect_code, std::string(what) + " code",
          "got " + std::to_string(f.code) + " expect " + std::to_string(expect_code));
    check(f.tile_ipix == expect_tile, std::string(what) + " tile_ipix",
          "got " + std::to_string(f.tile_ipix) + " expect " + std::to_string(expect_tile));
}

acsd::phase3::P3Sampler open_ok(const std::string& hips, const char* what) {
    using namespace acsd::phase3;
    P3Sampler s{};
    std::string err;
    const P3ResampleStatus st = p3_sampler_open_ex(hips.c_str(), &s, nullptr, nullptr, &err);
    check(st == P3_RS_OK, std::string(what) + " open", err);
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace acsd::phase3;
    if (argc < 14) {
        std::fprintf(stderr,
                     "usage: %s <fixtures_root> <t0ra> <t0dec> <t4ra> <t4dec> "
                     "<t5ra> <t5dec> <t7ra> <t7dec> <t8ra> <t8dec> "
                     "<o1t5ra> <o1t5dec>\n",
                     argv[0]);
        return 64;
    }
    const std::string root = argv[1];
    const double t0ra = std::atof(argv[2]), t0dec = std::atof(argv[3]);
    const double t4ra = std::atof(argv[4]), t4dec = std::atof(argv[5]);
    const double t5ra = std::atof(argv[6]), t5dec = std::atof(argv[7]);
    const double t7ra = std::atof(argv[8]), t7dec = std::atof(argv[9]);
    const double t8ra = std::atof(argv[10]), t8dec = std::atof(argv[11]);
    const double o1t5ra = std::atof(argv[12]), o1t5dec = std::atof(argv[13]);

    // 1) 完整合法：常量 + 校验正例
    {
        P3Sampler s = open_ok(fixture(root, "legal_full"), "legal_full");
        p3_sampler_set_worker_id(&s, 0);
        const Sample r = nearest(&s, t4ra, t4dec);
        check(r.status == P3_RS_OK && r.coverage == 1, "legal_full status/coverage");
        check(std::fabs(r.value - 1004.0f) < 1e-6f, "legal_full constant 1004",
              "got " + std::to_string(r.value));
        P3CacheStats cs{};
        p3_sampler_cache_stats(&s, &cs);
        check(cs.footprint_identity == 1 /* MOC_LEAF */, "legal_full footprint MOC_LEAF");
        check(cs.coverage_verified == 1, "legal_full coverage_verified true");
        check(p3_sampler_coverage_verified(&s) == 1, "legal_full sampler coverage_verified");
        check(cs.checksum_present_reads >= 1, "legal_full checksum verified");
        check(cs.tile_error_entries == 0, "legal_full no tile errors");
        p3_sampler_close(&s);
    }

    // 2) 合法稀疏 MOC：声明内正常、声明外合法 NaN
    {
        P3Sampler s = open_ok(fixture(root, "legal_sparse_moc"), "sparse_moc");
        p3_sampler_set_worker_id(&s, 0);
        const Sample in = nearest(&s, t4ra, t4dec);
        check(in.status == P3_RS_OK && in.coverage == 1, "sparse_moc covered");
        check(std::fabs(in.value - 2004.0f) < 1e-6f, "sparse_moc constant 2004");
        const Sample out = nearest(&s, t7ra, t7dec);
        check(out.status == P3_RS_OK && out.coverage == 0 && std::isnan(out.value),
              "sparse_moc outside -> OK + NaN/coverage=0");
        P3CacheStats cs{};
        p3_sampler_cache_stats(&s, &cs);
        check(cs.outside_reads >= 1, "sparse_moc outside_reads counted");
        check(cs.tile_error_entries == 0, "sparse_moc no integrity error");
        p3_sampler_close(&s);
    }

    // 3) 无 MOC 无覆盖计数：身份 UNVERIFIED_LEGACY，缺失是"未证实缺失"
    //    （Issue #87/E2-R：不再冒充已证实无覆盖；best-effort 可出图但身份降级）
    {
        P3Sampler s = open_ok(fixture(root, "legal_unhinted_sparse"), "unhinted");
        p3_sampler_set_worker_id(&s, 0);
        const Sample out = nearest(&s, t7ra, t7dec);
        check(out.status == P3_RS_OK && out.coverage == 0, "unhinted outside best-effort NaN");
        const Sample in = nearest(&s, t4ra, t4dec);
        check(in.status == P3_RS_OK && in.coverage == 1, "unhinted covered tile present");
        P3CacheStats cs{};
        p3_sampler_cache_stats(&s, &cs);
        check(cs.footprint_identity == 0 /* UNVERIFIED_LEGACY */,
              "unhinted footprint UNVERIFIED_LEGACY");
        check(cs.coverage_verified == 0, "unhinted coverage_verified false");
        check(p3_sampler_coverage_verified(&s) == 0, "unhinted sampler coverage_verified");
        check(cs.unverified_absent_reads >= 1, "unhinted unverified_absent_reads counted");
        check(cs.outside_reads == 0, "unhinted missing is NOT counted as proven-outside");
        check(cs.tile_error_entries == 0, "unhinted missing is not an integrity error");
        p3_sampler_close(&s);
    }

    // 4) 声明内缺失 / 非法结构
    {
        P3Sampler s = open_ok(fixture(root, "declared_missing"), "declared_missing");
        expect_error(&s, t5ra, t5dec, P3_TILE_DECLARED_MISSING, 5, "declared_missing");
        p3_sampler_close(&s);
    }
    {
        P3Sampler s = open_ok(fixture(root, "invalid_dims"), "invalid_dims");
        expect_error(&s, t4ra, t4dec, P3_TILE_INVALID, 4, "invalid_dims");
        p3_sampler_close(&s);
    }
    {
        P3Sampler s = open_ok(fixture(root, "invalid_bitpix"), "invalid_bitpix");
        expect_error(&s, t4ra, t4dec, P3_TILE_INVALID, 4, "invalid_bitpix");
        p3_sampler_close(&s);
    }
    {
        P3Sampler s = open_ok(fixture(root, "truncated"), "truncated");
        expect_error(&s, t4ra, t4dec, P3_TILE_INVALID, 4, "truncated");
        p3_sampler_close(&s);
    }

    // 5) CHECKSUM：bitflip 反例（错误缓存命中不改变故障语义）
    {
        P3Sampler s = open_ok(fixture(root, "checksum_bad"), "checksum_bad");
        p3_sampler_set_worker_id(&s, 0);
        expect_error(&s, t4ra, t4dec, P3_TILE_CHECKSUM_FAILED, 4, "checksum_bad[1]");
        expect_error(&s, t4ra, t4dec, P3_TILE_CHECKSUM_FAILED, 4, "checksum_bad[2]");
        P3CacheStats cs{};
        p3_sampler_cache_stats(&s, &cs);
        check(cs.tile_error_entries == 1, "checksum_bad error cache entries == 1");
        check(cs.tile_error_reads == 1, "checksum_bad real read counted once",
              "got " + std::to_string(cs.tile_error_reads));
        check(cs.tile_error_hits >= 1, "checksum_bad cache hit counted");
        check(p3_sampler_tile_failure_count(&s) == 1, "checksum_bad unique failures == 1");
        p3_sampler_close(&s);
    }

    // 6) CHECKSUM 正例 / 缺校验字段（明确规则: 缺省不失败）
    {
        P3Sampler s = open_ok(fixture(root, "checksum_ok"), "checksum_ok");
        p3_sampler_set_worker_id(&s, 0);
        const Sample r = nearest(&s, t4ra, t4dec);
        check(r.status == P3_RS_OK && r.coverage == 1, "checksum_ok success");
        check(std::fabs(r.value - 5004.0f) < 1e-6f, "checksum_ok constant 5004");
        P3CacheStats cs{};
        p3_sampler_cache_stats(&s, &cs);
        check(cs.checksum_present_reads >= 1, "checksum_ok present_reads");
        p3_sampler_close(&s);
    }
    {
        P3Sampler s = open_ok(fixture(root, "checksum_absent"), "checksum_absent");
        p3_sampler_set_worker_id(&s, 0);
        const Sample r = nearest(&s, t4ra, t4dec);
        check(r.status == P3_RS_OK && r.coverage == 1, "checksum_absent success");
        P3CacheStats cs{};
        p3_sampler_cache_stats(&s, &cs);
        check(cs.checksum_absent_reads >= 1, "checksum_absent absent_reads");
        check(cs.checksum_present_reads == 0, "checksum_absent present_reads == 0");
        check(cs.tile_error_entries == 0, "checksum_absent is not a failure");
        p3_sampler_close(&s);
    }

    // 7) 受控 I/O
    {
        P3Sampler s = open_ok(fixture(root, "io_error"), "io_error");
        expect_error(&s, t4ra, t4dec, P3_TILE_IO_ERROR, 4, "io_error");
        p3_sampler_close(&s);
    }

    // 8) 足迹不可判
    {
        P3Sampler s = open_ok(fixture(root, "undetermined_coarse_moc"),
                              "undetermined_coarse_moc");
        expect_error(&s, o1t5ra, o1t5dec, P3_TILE_FOOTPRINT_UNDETERMINED, 5,
                     "undetermined_coarse_moc");
        P3CacheStats cs{};
        p3_sampler_cache_stats(&s, &cs);
        check(cs.footprint_identity == 2 /* MOC_UNDETERMINED */,
              "undetermined_coarse footprint MOC_UNDETERMINED");
        p3_sampler_close(&s);
    }
    {
        P3Sampler s = open_ok(fixture(root, "undetermined_manifest_only"),
                              "undetermined_manifest_only");
        expect_error(&s, t5ra, t5dec, P3_TILE_FOOTPRINT_UNDETERMINED, 5,
                     "undetermined_manifest_only");
        P3CacheStats cs{};
        p3_sampler_cache_stats(&s, &cs);
        check(cs.footprint_identity == 3 /* MANIFEST_ONLY */,
              "undetermined_manifest footprint MANIFEST_ONLY");
        p3_sampler_close(&s);
    }

    // 9) 多坏 tile：顺序无关的排序/去重 + 四类码
    {
        P3Sampler s = open_ok(fixture(root, "multi_bad"), "multi_bad");
        p3_sampler_set_worker_id(&s, 3);
        // 逆序采样，聚合仍按 tile_ipix 升序
        expect_error(&s, t5ra, t5dec, P3_TILE_CHECKSUM_FAILED, 5, "multi_bad[t5]");
        expect_error(&s, t0ra, t0dec, P3_TILE_DECLARED_MISSING, 0, "multi_bad[t0]");
        expect_error(&s, t8ra, t8dec, P3_TILE_IO_ERROR, 8, "multi_bad[t8]");
        expect_error(&s, t4ra, t4dec, P3_TILE_INVALID, 4, "multi_bad[t4]");
        P3TileFailure f[8];
        const int n = p3_sampler_tile_failures(&s, f, 8);
        check(n == 4, "multi_bad unique failures == 4", "got " + std::to_string(n));
        const uint64_t want_tiles[4] = {0, 4, 5, 8};
        const int want_codes[4] = {P3_TILE_DECLARED_MISSING, P3_TILE_INVALID,
                                   P3_TILE_CHECKSUM_FAILED, P3_TILE_IO_ERROR};
        for (int i = 0; i < n && i < 4; ++i) {
            check(f[i].tile_ipix == want_tiles[i], "multi_bad sorted tile[" +
                  std::to_string(i) + "]");
            check(f[i].code == want_codes[i], "multi_bad code[" +
                  std::to_string(i) + "]");
        }
        check(p3_sampler_tile_failure_count(&s) == 4, "multi_bad count == 4");
        p3_sampler_close(&s);
    }

    // 10) 同一坏 tile 被两个 worker 访问：1 条聚合 + worker_min/max/count
    {
        P3Sampler main_s = open_ok(fixture(root, "same_bad_multi_worker"), "same_bad.main");
        P3Sampler w{};
        std::string err;
        check(p3_sampler_open_ex(fixture(root, "same_bad_multi_worker").c_str(), &w,
                                 nullptr, nullptr, &err) == P3_RS_OK,
              "same_bad worker open", err);
        p3_sampler_attach_cache(&w, &main_s);
        p3_sampler_set_worker_id(&w, 7);
        expect_error(&w, t4ra, t4dec, P3_TILE_CHECKSUM_FAILED, 4, "same_bad[w7]");
        p3_sampler_set_worker_id(&main_s, 2);
        expect_error(&main_s, t4ra, t4dec, P3_TILE_CHECKSUM_FAILED, 4, "same_bad[w2]");
        P3TileFailure f[4];
        const int n = p3_sampler_tile_failures(&main_s, f, 4);
        check(n == 1, "same_bad one aggregated entry", "got " + std::to_string(n));
        check(f[0].tile_ipix == 4, "same_bad tile 4");
        check(f[0].worker_min == 2 && f[0].worker_max == 7 && f[0].worker_count == 2,
              "same_bad worker_min/max/count",
              std::to_string(f[0].worker_min) + "/" + std::to_string(f[0].worker_max) +
                  "/" + std::to_string(f[0].worker_count));
        check(f[0].read_failures >= 1, "same_bad read_failures >= 1");
        p3_sampler_close(&w);
        p3_sampler_close(&main_s);
    }

    std::printf("%d/%d checks passed%s\n", g_total - g_fail, g_total,
                g_fail == 0 ? "" : " (FAILURES)");
    return g_fail == 0 ? 0 : 1;
}
