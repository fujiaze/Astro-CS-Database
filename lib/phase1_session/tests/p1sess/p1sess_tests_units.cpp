// P1-SESSION-TEST · units 组: 节点计数 / 失败传播 / manifest 状态机 / oracle
//
// 控制包任务: P1-SESSION-TEST (SA-P1SS-T, queue 47, lock-P1-SESSION)。
// 验收关键词 (tasks/05_PHASE1_TASKS.md §3):
//   "每声明节点恰好调用一次"     → U1 (4 节点各现一次, canonical 顺序)
//   "缺任何模块/错误 ABI/坏 artifact/取消不会写完成 manifest" → U3/U4
//   "仅 Phase1 run ID"           → manifest kind=astrocs_phase1_session
// 模板 <prefix>-TEST 覆盖面: 正常/边界/NaN/invalid/确定性 (properties 组)。
//
// 被测面: p1_session 五导出 C API (astrocs_phase1_session 静态库)。
// 期望值: p1sess_oracle.hpp (独立代数式 + 手写 FITS 解码), 非被测函数生成。
#include "p1_session.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "p1sess_fixtures.hpp"
#include "p1sess_oracle.hpp"
#include "p1sess_test_main.hpp"

using json = nlohmann::json;

// units 组共享 fixture 描述 (selfcheck 子进程驱动 units 组时通过同 TU 链接)

// ---------------------------------------------------------------------------
// host services stub (allocator/logger/cancel/budget; 计数面: logger 事件
// 与 cancel 轮询只记录不干预) + 节点观测注入面:
// session 的"节点调用"在 direct 面表现为 manifest stages 数组 (每节点一个
// 条目, status running→ok/fail/cancelled) — U1 以 stages 名称+次数+顺序为
// 调用计数 oracle (CAT-GAIA-INT 计数模式的 direct 面形态)。
// ---------------------------------------------------------------------------
static void* stub_alloc(void*, std::uint64_t n, std::uint64_t) {
    return std::malloc(static_cast<std::size_t>(n));
}
static void stub_free(void*, void* p) { std::free(p); }
static int stub_not_cancelled(void*) { return 0; }

static astrocs_host_services_v1 make_host() {
    astrocs_host_services_v1 h{};
    h.struct_size = sizeof(astrocs_host_services_v1);
    h.abi_version = ACS_ABI_VERSION_V1;
    h.allocator = {sizeof(acs_allocator), ACS_ABI_VERSION_V1, stub_alloc, stub_free, nullptr};
    h.logger = {sizeof(acs_logger), ACS_ABI_VERSION_V1, nullptr, nullptr};
    h.cancel = {sizeof(acs_cancel), ACS_ABI_VERSION_V1, stub_not_cancelled, nullptr};
    h.budget = {sizeof(acs_thread_budget), ACS_ABI_VERSION_V1, 1u, 1u, nullptr, nullptr, nullptr};
    return h;
}

// ---------------------------------------------------------------------------
// 会话驱动: create→(validate)→run→inspect→parse manifest →destroy
// 返回 inspect 原文 (raw) 供独立结构断言。
// ---------------------------------------------------------------------------
struct SessionOutcome {
    acs_status create_rc = ACS_ERR_INTERNAL;
    acs_status validate_rc = ACS_ERR_INTERNAL;
    acs_status run_rc = ACS_ERR_INTERNAL;
    acs_status inspect_rc = ACS_ERR_INTERNAL;
    bool ran_validate = false;
    std::string manifest_raw;   // inspect 原文 (parse 前的独立文本)
    json manifest;
    std::string last_error;
};

static SessionOutcome drive(const std::string& config_json, bool with_validate,
                            int async_io_depth = 0) {
    SessionOutcome out;
    astrocs_host_services_v1 host = make_host();
    acs_handle h = nullptr;
    out.create_rc = p1_session_create(&host, &h);
    if (out.create_rc != ACS_OK || !h) return out;
    acs_span_u8 cfg = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(config_json.c_str())),
                                  static_cast<std::uint64_t>(config_json.size()));
    if (with_validate) {
        out.validate_rc = p1_session_validate(h, cfg);
        out.ran_validate = true;
    }
    out.run_rc = p1_session_run(h, cfg, async_io_depth);
    acs_span_u8 mf{};
    out.inspect_rc = p1_session_inspect(h, &mf);
    if (out.inspect_rc == ACS_OK && mf.data) {
        out.manifest_raw.assign(reinterpret_cast<const char*>(mf.data), mf.count);
        try { out.manifest = json::parse(out.manifest_raw); }
        catch (...) { out.manifest = json(); }
        host.allocator.free(host.allocator.user_data, mf.data);
    }
    out.last_error = astrocs::phase1::last_error(h);
    p1_session_destroy(h);
    return out;
}

static const json* stage_by_name(const json& mf, const char* name) {
    if (!mf.contains("stages") || !mf["stages"].is_array()) return nullptr;
    for (const auto& st : mf["stages"])
        if (st.contains("name") && st["name"] == name) return &st;
    return nullptr;
}

static int count_stage(const json& mf, const char* name) {
    int n = 0;
    if (!mf.contains("stages") || !mf["stages"].is_array()) return 0;
    for (const auto& st : mf["stages"])
        if (st.contains("name") && st["name"] == name) ++n;
    return n;
}

// 防崩访问器: stages[idx] 缺失/非对象时返回空串 (不自抛 — 断言面自身
// 不得用异常崩掉, 否则掩盖生产缺陷定位)
static std::string stage_name_at(const json& mf, std::size_t idx) {
    if (!mf.contains("stages") || !mf["stages"].is_array()) return std::string();
    if (idx >= mf["stages"].size()) return std::string();
    const json& st = mf["stages"][idx];
    if (!st.is_object()) return std::string();
    if (!st.contains("name") || !st["name"].is_string()) return std::string();
    return st["name"].get<std::string>();
}

// 防崩取 stage 整数字段 (缺失/错型 → -1)
static long long stage_field_int(const json& mf, const char* name, const char* key) {
    const json* st = stage_by_name(mf, name);
    if (!st || !st->is_object()) return -1;
    if (!st->contains(key) || !(*st)[key].is_number_integer()) return -1;
    return (*st)[key].get<long long>();
}

// ---------------------------------------------------------------------------
// fixture 目录管理 (std::filesystem; TMPDIR>TEMP>TMP>"." — Windows CI 兼容
// 谱系对齐 test_p1_session_manifest.cpp WR9 整改)
// ---------------------------------------------------------------------------
static std::string fixture_base() {
    const char* d = std::getenv("TMPDIR");
    const char* w = std::getenv("TEMP");
    const char* w2 = std::getenv("TMP");
    const std::string tmp_root = (d ? d : (w ? w : (w2 ? w2 : ".")));
    const std::string base =
        (std::filesystem::path(tmp_root) / "astrocs_p1sess_test").generic_string();
    return base;
}

// ---------------------------------------------------------------------------
// U 组入口
// ---------------------------------------------------------------------------
namespace p1sess {

CheckState g_cs;

// FIX-SESS-A/B 布置: 2 light + 3 master (全常数场) + out 目录
// 返回 false = fixture IO 失败 (硬错)
struct SessFixtures {
    std::string base;
    std::string out_dir;
    std::string light1, light2;
    std::string master_bias, master_dark, master_flat;
    ConstField f_light1{0x5E55ED0001ULL, 100.0f};
    ConstField f_light2{0x5E55ED0002ULL, 120.0f};
    ConstField f_bias{0x5E55ED0003ULL, 5.0f};
    ConstField f_dark{0x5E55ED0004ULL, 9.0f};
    ConstField f_flat{0x5E55ED0005ULL, 2.0f};
};

static bool setup_fixtures(SessFixtures* F) {
    namespace fs = std::filesystem;
    F->base = fixture_base();
    F->out_dir = F->base + "/out";
    std::error_code ec;
    fs::remove_all(F->base, ec);
    fs::create_directories(F->out_dir, ec);
    if (ec) return false;
    F->light1 = F->base + "/light1.fts";
    F->light2 = F->base + "/light2.fts";
    F->master_bias = F->base + "/master_bias.fts";
    F->master_dark = F->base + "/master_dark.fts";
    F->master_flat = F->base + "/master_flat.fts";
    if (write_fits_file(F->light1, 8, 8, const_field_pixel, &F->f_light1)) return false;
    if (write_fits_file(F->light2, 8, 8, const_field_pixel, &F->f_light2)) return false;
    if (write_fits_file(F->master_bias, 8, 8, const_field_pixel, &F->f_bias)) return false;
    if (write_fits_file(F->master_dark, 8, 8, const_field_pixel, &F->f_dark)) return false;
    if (write_fits_file(F->master_flat, 8, 8, const_field_pixel, &F->f_flat)) return false;
    return true;
}

int run_units() {
    CheckState& cs = g_cs;
    SessFixtures F;
    if (!setup_fixtures(&F)) {
        std::fprintf(stderr, "fixture setup failed (base=%s)\n", F.base.c_str());
        return 2;
    }

    // ── U1: 全链 happy path — 每声明节点恰好调用一次, canonical 顺序 ──
    // 验收关键词: "each node once and failure propagation" (调用计数面)
    {
        const std::string cfg =
            std::string("{\"input_lights\":[\"") + F.light1 + "\",\"" + F.light2 +
            "\"],\"output_dir\":\"" + F.out_dir +
            "\",\"master_bias\":\"" + F.master_bias +
            "\",\"master_dark\":\"" + F.master_dark +
            "\",\"master_flat\":\"" + F.master_flat +
            "\",\"cosmetic\":{\"enabled\":false}}";
        SessionOutcome o = drive(cfg, true);
        P1SESS_CHECK_EQ(cs, o.create_rc, ACS_OK);
        P1SESS_CHECK_EQ(cs, o.validate_rc, ACS_OK);
        P1SESS_CHECK_EQ(cs, o.run_rc, ACS_OK);
        P1SESS_CHECK_EQ(cs, o.inspect_rc, ACS_OK);
        // manifest kind: 仅 Phase1 run ID (验收: "仅 Phase1 run ID")
        P1SESS_CHECK_MSG(cs, o.manifest.value("kind", "") == "astrocs_phase1_session",
                         "u1_manifest_kind", "kind=%s", o.manifest.value("kind", "?").c_str());
        // 节点调用计数: 4 个声明节点各恰好 1 次
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "io_read"), 1);
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "calibrate"), 1);
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "cosmetic"), 0);  // gated off → 不执行不计数
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "io_write"), 1);
        P1SESS_CHECK_EQ(cs, o.manifest["stages"].size(), 3);
        // canonical 顺序 io_read → calibrate → io_write (防崩访问器)
        P1SESS_CHECK_MSG(cs, stage_name_at(o.manifest, 0) == "io_read" &&
                                 stage_name_at(o.manifest, 1) == "calibrate" &&
                                 stage_name_at(o.manifest, 2) == "io_write",
                         "u1_node_order", "order=%s/%s/%s", stage_name_at(o.manifest, 0).c_str(),
                         stage_name_at(o.manifest, 1).c_str(), stage_name_at(o.manifest, 2).c_str());
        // 全部节点 ok (无 running 残留 — P1 batchD 写穿合同)
        for (const auto& st : o.manifest["stages"])
            P1SESS_CHECK_MSG(cs, st.value("status", "") == "ok", "u1_stage_ok",
                             "stage %s status=%s", st.value("name", "?").c_str(),
                             st.value("status", "?").c_str());
        // 顶层状态机: partial（P1-001 complete 门 fail-closed: 链不完整禁写 complete,
        // PROD-P0-001 Phase1 侧; availability 8 域, calibration/cosmetic=available）
        P1SESS_CHECK(cs, o.manifest.value("status", "") == "partial", "u1_top_partial");
        P1SESS_CHECK(cs, o.manifest["availability"].value("calibration", "") == "available",
                     "u1_avail_cal");
        P1SESS_CHECK(cs, o.manifest["availability"].value("star_psf", "") == "unavailable",
                     "u1_avail_star");
        // frames 计数 (io_read files=5: 3 master+2 light 全计入; calibrate frames=2; top frames=2)
        P1SESS_CHECK_EQ(cs, stage_field_int(o.manifest, "io_read", "files"), 5);
        P1SESS_CHECK_EQ(cs, stage_field_int(o.manifest, "calibrate", "frames"), 2);
        P1SESS_CHECK_EQ(cs, o.manifest.value("frames", 0), 2);
        // artifacts 恰 2 条, 路径 = out_dir/calibrated_<base>
        P1SESS_CHECK_EQ(cs, o.manifest["artifacts"].size(), 2);
        P1SESS_CHECK_MSG(cs, o.manifest["artifacts"][0].get<std::string>() == F.out_dir + "/calibrated_light1.fts",
                         "u1_artifact_path0", "artifact0=%s",
                         o.manifest["artifacts"][0].get<std::string>().c_str());
    }

    // ── U1b: cosmetic enabled → 4 节点全现各恰 1 次 (cosmetic 键显式在
    // config: 生产 gate = contains("cosmetic") && cosmetic_flag(enabled)) ──
    {
        const std::string cfg =
            std::string("{\"input_lights\":[\"") + F.light1 +
            "\"],\"output_dir\":\"" + F.out_dir +
            "\",\"master_bias\":\"" + F.master_bias +
            "\",\"master_dark\":\"" + F.master_dark +
            "\",\"master_flat\":\"" + F.master_flat +
            "\",\"cosmetic\":{\"enabled\":true}}";
        SessionOutcome o = drive(cfg, true);
        P1SESS_CHECK_EQ(cs, o.run_rc, ACS_OK);
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "io_read"), 1);
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "calibrate"), 1);
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "cosmetic"), 1);
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "io_write"), 1);
        P1SESS_CHECK(cs, o.manifest.value("status", "") == "partial", "u1b_top_partial");
        // canonical 4 段顺序 (README §2 节点表; 防崩访问器)
        P1SESS_CHECK_MSG(cs, stage_name_at(o.manifest, 0) == "io_read" &&
                                 stage_name_at(o.manifest, 1) == "calibrate" &&
                                 stage_name_at(o.manifest, 2) == "cosmetic" &&
                                 stage_name_at(o.manifest, 3) == "io_write",
                         "u1b_node_order4", "order=%s/%s/%s/%s",
                         stage_name_at(o.manifest, 0).c_str(), stage_name_at(o.manifest, 1).c_str(),
                         stage_name_at(o.manifest, 2).c_str(), stage_name_at(o.manifest, 3).c_str());
    }

    // ── U2: ALG-CAL-001 oracle — 校准落盘像素 = 独立代数式 (期望非被测
    // 函数生成; oracle 手写 FITS 解码, 不走 aio_read) ──
    {
        const std::string cfg =
            std::string("{\"input_lights\":[\"") + F.light1 +
            "\"],\"output_dir\":\"" + F.out_dir +
            "\",\"master_bias\":\"" + F.master_bias +
            "\",\"master_dark\":\"" + F.master_dark +
            "\",\"master_flat\":\"" + F.master_flat +
            "\",\"cosmetic\":{\"enabled\":false}}";
        SessionOutcome o = drive(cfg, true);
        P1SESS_CHECK_EQ(cs, o.run_rc, ACS_OK);
        const std::string artifact = F.out_dir + "/calibrated_light1.fts";
        int w = 0, h = 0;
        std::vector<float> px;
        P1SESS_CHECK_MSG(cs, oracle_read_fits_f32(artifact, &w, &h, &px),
                         "u2_oracle_readback", "artifact=%s", artifact.c_str());
        if (px.size() == 64) {
            // 全帧常数一致 (dark_opt=0 标准模式: (light-dark)/max(flat,0.1))
            const double want = oracle_calibrate_const(
                F.f_light1.value, true, F.f_dark.value, true, F.f_flat.value);
            for (int i = 0; i < 64; ++i)
                P1SESS_CHECK_MSG(cs, rel_close_f(px[i], want, 1e-6), "u2_oracle_pixel",
                                 "px[%d]=%g want=%g", i, static_cast<double>(px[i]), want);
        }
        // dark_opt=1: (light - bias - K*(dark-bias))/flat, K=dark_scale_factor
        {
            const std::string cfg2 =
                std::string("{\"input_lights\":[\"") + F.light1 +
                "\"],\"output_dir\":\"" + F.out_dir +
                "\",\"master_bias\":\"" + F.master_bias +
                "\",\"master_dark\":\"" + F.master_dark +
                "\",\"master_flat\":\"" + F.master_flat +
                "\",\"cosmetic\":{\"enabled\":false},\"dark_optimization\":true,\"dark_scale_factor\":2.0}";
            SessionOutcome o2 = drive(cfg2, true);
            P1SESS_CHECK_EQ(cs, o2.run_rc, ACS_OK);
            std::vector<float> px2;
            int w2 = 0, h2 = 0;
            P1SESS_CHECK(cs, oracle_read_fits_f32(artifact, &w2, &h2, &px2), "u2_oracle_readback_dopt");
            if (px2.size() == 64) {
                const double want2 = oracle_calibrate_const_darkopt(
                    F.f_light1.value, F.f_bias.value, F.f_dark.value, 2.0, true, F.f_flat.value);
                for (int i = 0; i < 64; ++i)
                    P1SESS_CHECK_MSG(cs, rel_close_f(px2[i], want2, 1e-6), "u2_oracle_pixel_dopt",
                                     "px[%d]=%g want=%g", i, static_cast<double>(px2[i]), want2);
            }
        }
    }

    // ── U3: 失败传播 — 缺模块 (input 不可读) 不写完成 manifest ──
    // 验收关键词: "缺任何模块/坏 artifact 不会写完成 manifest"
    {
        const std::string bad = F.base + "/missing_light.fts";
        const std::string cfg =
            std::string("{\"input_lights\":[\"") + bad +
            "\"],\"output_dir\":\"" + F.out_dir + "\"}";
        SessionOutcome o = drive(cfg, true);
        P1SESS_CHECK_EQ(cs, o.validate_rc, ACS_OK);   // validate 只验 config 结构, 不验文件
        P1SESS_CHECK_EQ(cs, o.run_rc, ACS_ERR_IO);
        P1SESS_CHECK_MSG(cs, o.manifest.value("status", "") == "failed",
                         "u3_top_failed", "status=%s", o.manifest.value("status", "?").c_str());
        P1SESS_CHECK_MSG(cs, o.manifest.value("status", "") != "complete",
                         "u3_not_complete", "must not write complete manifest");
        P1SESS_CHECK(cs, o.manifest.contains("error") && !o.manifest["error"].get<std::string>().empty(),
                     "u3_error_field");
        P1SESS_CHECK_MSG(cs, o.last_error.find("cannot read image") != std::string::npos,
                         "u3_last_error", "last_error=%s", o.last_error.c_str());
        // io_read 阶段 fail; 后续节点不执行 (失败传播短路)
        const json* io = stage_by_name(o.manifest, "io_read");
        P1SESS_CHECK(cs, io && io->value("status", "") == "fail", "u3_io_read_fail");
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "calibrate"), 0);
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "io_write"), 0);
        P1SESS_CHECK(cs, o.manifest.value("error_kind", "") == "input", "u3_error_kind_input");
        // 短路: output_dir 无产物落盘
        P1SESS_CHECK(cs, !std::filesystem::exists(F.out_dir + "/calibrated_missing_light.fts"),
                     "u3_no_partial_artifact");
    }

    // ── U4: 失败传播 — calibrate 域 INTERNAL (坏 master 经 FIX-SESS-D 垃圾
    // 文件) 不写完成 manifest ──
    {
        const std::string garbage = F.base + "/garbage_bias.fts";
        if (write_garbage_file(garbage) != 0) return 2;
        const std::string cfg =
            std::string("{\"input_lights\":[\"") + F.light1 +
            "\"],\"output_dir\":\"" + F.out_dir +
            "\",\"master_bias\":\"" + garbage + "\"}";
        SessionOutcome o = drive(cfg, true);
        // io_read 阶段即失败 (master 在 io_read 读取, 缺模块传播)
        P1SESS_CHECK_EQ(cs, o.run_rc, ACS_ERR_IO);
        P1SESS_CHECK_MSG(cs, o.manifest.value("status", "") == "failed",
                         "u4_top_failed", "status=%s", o.manifest.value("status", "?").c_str());
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "calibrate"), 0);
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "io_write"), 0);
    }

    // ── U5: 失败传播 — calibrate 域参数错 (FIX-SESS-E 尺寸不匹配 master) ──
    {
        ConstField f_big{0x5E55ED0009ULL, 1.0f};
        const std::string big = F.base + "/master_bias_big.fts";
        if (write_fits_file(big, 16, 8, const_field_pixel, &f_big) != 0) return 2;
        const std::string cfg =
            std::string("{\"input_lights\":[\"") + F.light1 +
            "\"],\"output_dir\":\"" + F.out_dir +
            "\",\"master_bias\":\"" + big + "\"}";
        SessionOutcome o = drive(cfg, true);
        // io_read ok (16x8 是合法 FITS); calibrate 尺寸守卫 → PARAM
        P1SESS_CHECK_EQ(cs, o.run_rc, ACS_ERR_PARAM);
        P1SESS_CHECK_MSG(cs, o.manifest.value("status", "") == "failed",
                         "u5_top_failed", "status=%s", o.manifest.value("status", "?").c_str());
        const json* cal = stage_by_name(o.manifest, "calibrate");
        P1SESS_CHECK(cs, cal && cal->value("status", "") == "fail", "u5_calibrate_fail");
        P1SESS_CHECK_EQ(cs, count_stage(o.manifest, "io_write"), 0);
    }

    // ── U6: FIX-SESS-C 截断 FITS → io_read 失败传播 (坏 artifact 面) ──
    {
        ConstField f_tr{0x5E55ED000AULL, 50.0f};
        const std::string tr = F.base + "/truncated_light.fts";
        // 头 480 字节 + 数据 256 字节后截去 200 → 数据块残缺
        if (write_fits_file(tr, 8, 8, const_field_pixel, &f_tr, /*truncate_tail=*/200) != 0) return 2;
        const std::string cfg =
            std::string("{\"input_lights\":[\"") + tr +
            "\"],\"output_dir\":\"" + F.out_dir + "\"}";
        SessionOutcome o = drive(cfg, true);
        P1SESS_CHECK_EQ(cs, o.run_rc, ACS_ERR_IO);
        P1SESS_CHECK_MSG(cs, o.manifest.value("status", "") != "complete",
                         "u6_not_complete", "truncated input must not complete");
        const json* io = stage_by_name(o.manifest, "io_read");
        P1SESS_CHECK(cs, io && io->value("status", "") == "fail", "u6_io_read_fail");
    }

    // ── U7: NaN 传播 (FIX-SESS-F) — NaN 像素不吞 (invalid 面) ──
    {
        NanField f_nan{30.0f};
        const std::string nan_light = F.base + "/nan_light.fts";
        if (write_fits_file(nan_light, 8, 8, nan_field_pixel, &f_nan) != 0) return 2;
        const std::string cfg =
            std::string("{\"input_lights\":[\"") + nan_light +
            "\"],\"output_dir\":\"" + F.out_dir +
            "\",\"cosmetic\":{\"enabled\":false}}";
        SessionOutcome o = drive(cfg, true);
        P1SESS_CHECK_EQ(cs, o.run_rc, ACS_OK);
        P1SESS_CHECK(cs, o.manifest.value("status", "") == "partial", "u7_partial");
        std::vector<float> px;
        int w = 0, h = 0;
        P1SESS_CHECK(cs, oracle_read_fits_f32(F.out_dir + "/calibrated_nan_light.fts", &w, &h, &px),
                     "u7_readback");
        if (px.size() == 64) {
            P1SESS_CHECK_MSG(cs, std::isnan(px[5]), "u7_nan_propagates",
                             "px[5]=%g (NaN must propagate through session pipeline)",
                             static_cast<double>(px[5]));
            // 其余像素 = light/max(flat,0.1) 但 flat 未给 → light 原样 (bitwise)
            for (int i = 0; i < 64; ++i)
                if (i != 5)
                    P1SESS_CHECK_MSG(cs, bits_eq_f(px[i], f_nan.value), "u7_nonnan_bitwise",
                                     "px[%d] bitwise", i);
        }
    }

    // ── U8: manifest 状态机 created (未 run 无错) → inspect 契约 ──
    {
        astrocs_host_services_v1 host = make_host();
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        acs_span_u8 mf{};
        P1SESS_CHECK_EQ(cs, p1_session_inspect(h, &mf), ACS_OK);
        json m = json::parse(std::string(reinterpret_cast<const char*>(mf.data), mf.count));
        host.allocator.free(host.allocator.user_data, mf.data);
        P1SESS_CHECK_MSG(cs, m.value("status", "") == "created", "u8_created_state",
                         "status=%s", m.value("status", "?").c_str());
        P1SESS_CHECK_EQ(cs, m["stages"].size(), 0);
        // inspect 缓冲 host 分配 → host free (所有权合同 README §5)
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    if (cs.failures == 0) return 0;
    std::fprintf(stderr, "units group: %d failure(s)\n", cs.failures);
    return 1;
}

}  // namespace p1sess
