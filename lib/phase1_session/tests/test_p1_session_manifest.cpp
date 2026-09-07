// P1 bug 狩猎 R6 确认条目单测 (bughunt_p1_batchD) — 共址 lib/phase1_session/tests/
// 覆盖两项 P1:
//   P1-1 cosmetic/io_write 阶段 manifest st 局部拷贝不回写 → 阶段状态恒 "running";
//   P1-2 cosmetic 值类型合同 (validate: number|bool 均合法) vs run 期 get<T> 错型
//        抛 nlohmann type_error.302 穿越 extern "C" → std::terminate。
// 合同: {"enabled":1} (int) 与 {"enabled":true} (bool) 均为合法 config, 必须跑通;
//       {"enabled":"x"} (string, 合同外) 由 validate 拒绝, 不得 terminate。
// 模式参照: tests/unit/p1_ir_facade_test.cpp (CHECK 宏) +
//           lib/astro_image_io/tests/test_p0_io_hardening.cpp (手写 FITS fixture)。
#include "p1_session.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

static int failures = 0;
static int checks = 0;
#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        ++checks;                                                               \
        if (!(cond)) {                                                          \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, \
                         msg);                                                  \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

// ── host services stub (allocator/logger/cancel/budget 最小实现) ─────────────
static void* stub_alloc(void*, uint64_t n, uint64_t) { return std::malloc(static_cast<size_t>(n)); }
static void  stub_free(void*, void* p) { std::free(p); }
static void  stub_log(void*, int, const char*, const char*) {}
static int   stub_not_cancelled(void*) { return 0; }

static astrocs_host_services_v1 make_host() {
    astrocs_host_services_v1 h{};
    h.struct_size = sizeof(astrocs_host_services_v1);
    h.abi_version = ACS_ABI_VERSION_V1;
    h.allocator = {sizeof(acs_allocator), ACS_ABI_VERSION_V1, stub_alloc, stub_free, nullptr};
    h.logger = {sizeof(acs_logger), ACS_ABI_VERSION_V1, stub_log, nullptr};
    h.cancel = {sizeof(acs_cancel), ACS_ABI_VERSION_V1, stub_not_cancelled, nullptr};
    h.budget = {sizeof(acs_thread_budget), ACS_ABI_VERSION_V1, 1u, 1u, nullptr, nullptr, nullptr};
    return h;
}

// ── 最小 FITS fixture (8x8 BITPIX=-32; 数据按 FITS 大端写入) ─────────────────
static void fits_card(std::FILE* fp, const char* key, const char* value) {
    char card[80];
    std::memset(card, ' ', 80);
    std::memcpy(card, key, std::strlen(key));
    card[8] = '=';
    card[9] = ' ';
    char v[24];
    std::snprintf(v, sizeof(v), "%s", value);
    std::memcpy(card + 10, v, std::strlen(v));
    std::fwrite(card, 1, 80, fp);
}

static void write_fits_8x8(const std::string& path, float pixel_value) {
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) { std::fprintf(stderr, "fixture open failed: %s\n", path.c_str()); std::exit(2); }
    fits_card(fp, "SIMPLE", "T");
    fits_card(fp, "BITPIX", "-32");
    fits_card(fp, "NAXIS", "2");
    fits_card(fp, "NAXIS1", "8");
    fits_card(fp, "NAXIS2", "8");
    fits_card(fp, "END", "");
    long pos = 80L * 6;
    int pad = static_cast<int>((2880 - (pos % 2880)) % 2880);
    for (int i = 0; i < pad; ++i) std::fputc(' ', fp);
    // 数据块: 8*8 float, big-endian (FITS 字节序)
    uint32_t u = 0;
    std::memcpy(&u, &pixel_value, sizeof(u));
    unsigned char be[4] = {static_cast<unsigned char>(u >> 24), static_cast<unsigned char>(u >> 16),
                           static_cast<unsigned char>(u >> 8), static_cast<unsigned char>(u)};
    for (int i = 0; i < 64; ++i) std::fwrite(be, 1, 4, fp);
    long dpos = 256;
    int dpad = static_cast<int>((2880 - (dpos % 2880)) % 2880);
    for (int i = 0; i < dpad; ++i) std::fputc('\0', fp);
    std::fclose(fp);
}

// ── 会话驱动: create→(可选 validate)→run→inspect→parse manifest ──────────────
struct SessionOutcome {
    acs_status validate_rc = ACS_ERR_INTERNAL;
    acs_status run_rc = ACS_ERR_INTERNAL;
    bool ran_validate = false;
    json manifest;
};

static SessionOutcome drive(const std::string& config_json, bool with_validate) {
    SessionOutcome out;
    astrocs_host_services_v1 host = make_host();
    acs_handle h = nullptr;
    if (p1_session_create(&host, &h) != ACS_OK || !h) {
        std::fprintf(stderr, "session create failed\n");
        std::exit(2);
    }
    acs_span_u8 cfg = ACS_SPAN_U8(reinterpret_cast<uint8_t*>(const_cast<char*>(config_json.c_str())),
                                  static_cast<uint64_t>(config_json.size()));
    if (with_validate) {
        out.validate_rc = p1_session_validate(h, cfg);
        out.ran_validate = true;
    }
    out.run_rc = p1_session_run(h, cfg, 0);
    acs_span_u8 mf{};
    if (p1_session_inspect(h, &mf) == ACS_OK && mf.data) {
        std::string s(reinterpret_cast<const char*>(mf.data), mf.count);
        try { out.manifest = json::parse(s); } catch (...) { out.manifest = json(); }
    }
    p1_session_destroy(h);
    return out;
}

static const json* stage_by_name(const json& mf, const char* name) {
    if (!mf.contains("stages") || !mf["stages"].is_array()) return nullptr;
    for (const auto& st : mf["stages"])
        if (st.contains("name") && st["name"] == name) return &st;
    return nullptr;
}

int main() {
    const char* d = std::getenv("TMPDIR");
    const std::string base = (d ? d : std::string("/tmp")) + "/astrocs_p1_batchD_test";
    const std::string out_dir = base + "/out";
    std::system(("rm -rf '" + base + "'").c_str());
    std::system(("mkdir -p '" + out_dir + "'").c_str());
    const std::string light = base + "/light1.fts";
    write_fits_8x8(light, 100.0f);

    // fixture 自检: fixture 可被 AIO 读回 (防止 fixture 自身缺陷制造假红/假绿)
    {
        std::FILE* fp = std::fopen(light.c_str(), "rb");
        CHECK(fp != nullptr, "fixture fits exists");
        if (fp) std::fclose(fp);
    }

    std::fprintf(stderr, "== P1 batchD: p1_session manifest write-through + cosmetic type contract ==\n");

    // T1 (P1-1): 默认 cosmetic (enabled 缺省=true) — 全阶段 manifest 状态写穿
    {
        SessionOutcome o = drive("{\"input_lights\":[\"" + light + "\"],\"output_dir\":\"" + out_dir + "\",\"cosmetic\":{}}", true);
        CHECK(o.validate_rc == ACS_OK, "T1 validate ok");
        CHECK(o.run_rc == ACS_OK, "T1 run ok");
        CHECK(o.manifest.value("status", "") == "complete", "T1 top status complete");
        const json* st = stage_by_name(o.manifest, "cosmetic");
        CHECK(st != nullptr, "T1 cosmetic stage present");
        if (st) {
            // 修复前: 局部拷贝 → manifest 中恒 "running"
            CHECK((*st).value("status", "") == "ok", "T1 cosmetic status write-through (not running)");
            CHECK((*st).contains("hot_fixed") && (*st).contains("cold_fixed"), "T1 cosmetic counters recorded");
        }
        CHECK(stage_by_name(o.manifest, "io_write") != nullptr &&
                  stage_by_name(o.manifest, "io_write")->value("status", "") == "ok",
              "T1 io_write status write-through (not running)");
        bool all_ok = true;
        for (const auto& st2 : o.manifest.value("stages", json::array()))
            if (st2.value("status", "") == "running") all_ok = false;
        CHECK(all_ok, "T1 no stage left in running");
    }

    // T2 (P1-2): {"enabled":1} — int 非 bool, validate 合同合法, run 不得 terminate
    {
        SessionOutcome o = drive("{\"input_lights\":[\"" + light + "\"],\"output_dir\":\"" + out_dir + "\",\"cosmetic\":{\"enabled\":1}}", true);
        CHECK(o.validate_rc == ACS_OK, "T2 validate accepts int enabled");
        CHECK(o.run_rc == ACS_OK, "T2 run ok with enabled=1 (no type_error.302)");
        const json* st = stage_by_name(o.manifest, "cosmetic");
        CHECK(st != nullptr && (*st).value("status", "") == "ok", "T2 cosmetic status ok");
    }

    // T3 (P1-2): {"enabled":true} — bool 原语义, run 不得 terminate
    {
        SessionOutcome o = drive("{\"input_lights\":[\"" + light + "\"],\"output_dir\":\"" + out_dir + "\",\"cosmetic\":{\"enabled\":true}}", true);
        CHECK(o.validate_rc == ACS_OK, "T3 validate accepts bool enabled");
        CHECK(o.run_rc == ACS_OK, "T3 run ok with enabled=true");
        const json* st = stage_by_name(o.manifest, "cosmetic");
        CHECK(st != nullptr && (*st).value("status", "") == "ok", "T3 cosmetic status ok");
    }

    // T4 (P1-2): {"enabled":0} — int 0 → false 语义, cosmetic 阶段被门控关闭
    {
        SessionOutcome o = drive("{\"input_lights\":[\"" + light + "\"],\"output_dir\":\"" + out_dir + "\",\"cosmetic\":{\"enabled\":0}}", true);
        CHECK(o.validate_rc == ACS_OK, "T4 validate accepts int 0");
        CHECK(o.run_rc == ACS_OK, "T4 run ok with enabled=0");
        CHECK(stage_by_name(o.manifest, "cosmetic") == nullptr, "T4 cosmetic stage gated off");
    }

    // T5 (P1-2): {"enabled":"x"} — 合同外 string, validate 拒绝 (不 terminate)
    {
        SessionOutcome o = drive("{\"input_lights\":[\"" + light + "\"],\"output_dir\":\"" + out_dir + "\",\"cosmetic\":{\"enabled\":\"x\"}}", true);
        CHECK(o.ran_validate && o.validate_rc == ACS_ERR_PARAM, "T5 validate rejects string enabled");
    }

    // T6 (P1-2): 热点参数错型混合 — hot_sigma 布尔(合同内 number|bool), max_structure_size 浮点
    {
        SessionOutcome o = drive("{\"input_lights\":[\"" + light + "\"],\"output_dir\":\"" + out_dir + "\",\"cosmetic\":{\"hot_sigma\":true,\"cold_sigma\":1,\"max_structure_size\":4.0}}", true);
        CHECK(o.validate_rc == ACS_OK, "T6 validate accepts number|bool mix");
        CHECK(o.run_rc == ACS_OK, "T6 run ok with bool hot_sigma (no type_error.302)");
        const json* st = stage_by_name(o.manifest, "cosmetic");
        CHECK(st != nullptr && (*st).value("status", "") == "ok", "T6 cosmetic status ok");
    }

    // T7 (P1-2 层②屏障): 直调 run 跳过 validate, 合同外 string 不穿 C 边界 (宽容默认/错误码, 不 terminate)
    {
        SessionOutcome o = drive("{\"input_lights\":[\"" + light + "\"],\"output_dir\":\"" + out_dir + "\",\"cosmetic\":{\"enabled\":\"x\"}}", false);
        CHECK(o.run_rc == ACS_OK, "T7 run barrier: no terminate, error surfaced as status code");
    }

    if (failures == 0) {
        std::printf("P1 batchD p1_session tests PASS (%d checks)\n", checks);
        return 0;
    }
    std::fprintf(stderr, "P1 batchD p1_session tests FAIL (%d/%d)\n", failures, checks);
    return 1;
}
