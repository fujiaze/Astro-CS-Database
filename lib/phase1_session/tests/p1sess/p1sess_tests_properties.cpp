// P1-SESSION-TEST · properties 组: 确定性/取消传播/异常屏障/不变量
//
// 控制包任务: P1-SESSION-TEST (SA-P1SS-T, queue 47, lock-P1-SESSION)。
// 模板覆盖面: 取消/确定性/1-N worker —
//   P1  validate 幂等纯读 (README §4: 纯读无 IO 幂等)
//   P2  1-N worker (budget.max_workers=1/2/4) 落盘 bitwise 一致 (README §6:
//       内部并行=omp 预算驱动; 确定性合同 fixed_reduction_order)
//   P3  取消传播 (cancel 单向置位) — CANCELLED + manifest 阶段 cancelled,
//       不写 complete (验收: 取消不写完成 manifest), 无伪完整产物
//   P4  异常屏障 — 错型 config 直调 run 不 terminate (P1 batchD 层②合同)
//   P5  async_io_depth 值域 {0,1,2} 边界 (+3 拒)
//   P6  ABI 负面 — create host 结构 struct_size/abi_version 错 → ABI_MISMATCH
//   P7  重复 run 同 handle — 第二次 run 语义 (状态机面)
//
// SCI/ALG ID 锚 (逐条核到实际文档 ID, 不臆造):
//   SCI-CAL-001 — docs/science/CALIBRATION.md (FROZEN T100, 2026-08-23):
//     §7「确定性不变量」(相同输入顺序改变不改变 cal; 逐像素独立算术,
//     无跨像素归约) 与 §8 极端/退化条件 —— P2 的 1/N worker bitwise 一致性
//     断言的科学层来源。
//   ALG-CAL-001..006 — docs/algorithms/CALIBRATION_ALGORITHMS.md:
//     §6「输出 bitwise 与线程数无关 (schedule(static) 行块划分不影响单像素
//     算术)」—— P2 上位; §7 边界/invalid/退化表 (AC_ERR_PARAM、out 不写、
//     σ≤0 禁用) —— P4/P5/P7 参数域断言上位。
//   ALG-COS-001..005 — docs/algorithms/COSMETIC_ALGORITHMS.md §6
//     (复杂度/确定性/并行归约汇总) —— 本组 config 内 cosmetic 段上位。
//   装配层契约 (非 SCI/ALG, 本层无算法公式, 见 lib/phase1_session/module.yaml):
//     API-P1-SESSION (docs/contracts/PUBLIC_API.md「Phase1 装配会话」节)、
//     DATA-P1-SESSION (DATA_SEMANTICS §16)、
//     lib/phase1_session/README.md §4 (外部输入/async_io_depth)、
//     §5 (错误与取消传播、ABI 校验)、§6 (并发与资源/线程预算注入)。
//   P1/P3/P5/P6/P7 属装配层生命周期与 ABI 契约, SCI/ALG 层确无上位定义
//     —— 如实登记 finding, 不伪造 SCI/ALG ID。
#include "p1_session.h"

#include <atomic>
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

namespace p1sess {

extern CheckState g_cs;

// 与 units 组同构的 host stub (独立定义避免跨 TU 全局)
static void* p_alloc(void*, std::uint64_t n, std::uint64_t) {
    return std::malloc(static_cast<std::size_t>(n));
}
static void p_free(void*, void* p) { std::free(p); }

struct CancelFlag {
    std::atomic<int> flag{0};
};
// ud 可为 nullptr (P4/P5/P6/P7 以空 cancel 注入, 与 p_log 同守卫纪律;
// 生产 s->cancelled() 只判函数指针非空即调, stub 自身必须容忍空指针)
static int p_cancelled(void* ud) {
    return ud ? static_cast<CancelFlag*>(ud)->flag.load() : 0;
}

struct LogSink {
    std::atomic<int> stage_events{0};
};
static void p_log(void* ud, int, const char*, const char*) {
    // P4/P5/P6/P7 等场景以 nullptr LogSink 注入 (不观测日志面);
    // 生产 s->log 不解引用 user_data, stub 自身必须容忍空指针
    if (ud) static_cast<LogSink*>(ud)->stage_events.fetch_add(1);
}

static astrocs_host_services_v1 make_host2(CancelFlag* cf, LogSink* ls,
                                           std::uint32_t max_workers) {
    astrocs_host_services_v1 h{};
    h.struct_size = sizeof(astrocs_host_services_v1);
    h.abi_version = ACS_ABI_VERSION_V1;
    h.allocator = {sizeof(acs_allocator), ACS_ABI_VERSION_V1, p_alloc, p_free, nullptr};
    h.logger = {sizeof(acs_logger), ACS_ABI_VERSION_V1, p_log, ls};
    h.cancel = {sizeof(acs_cancel), ACS_ABI_VERSION_V1, p_cancelled, cf};
    h.budget = {sizeof(acs_thread_budget), ACS_ABI_VERSION_V1, max_workers, max_workers,
                nullptr, nullptr, nullptr};
    return h;
}

int run_properties() {
    CheckState& cs = g_cs;
    namespace fs = std::filesystem;
    // fixture 复用 units 同款布置 (独立实例, 隔离组间状态)
    const char* d = std::getenv("TMPDIR");
    const char* w = std::getenv("TEMP");
    const char* w2 = std::getenv("TMP");
    const std::string tmp_root = (d ? d : (w ? w : (w2 ? w2 : ".")));
    const std::string base =
        (fs::path(tmp_root) / "astrocs_p1sess_props").generic_string();
    const std::string out_dir = base + "/out";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(out_dir, ec);

    ConstField f_l1{0x5E55ED0001ULL, 100.0f};
    ConstField f_l2{0x5E55ED0002ULL, 120.0f};
    ConstField f_b{0x5E55ED0003ULL, 5.0f};
    ConstField f_dk{0x5E55ED0004ULL, 9.0f};
    ConstField f_fl{0x5E55ED0005ULL, 2.0f};
    const std::string light1 = base + "/light1.fts";
    const std::string light2 = base + "/light2.fts";
    const std::string mb = base + "/master_bias.fts";
    const std::string md = base + "/master_dark.fts";
    const std::string mf = base + "/master_flat.fts";
    if (write_fits_file(light1, 8, 8, const_field_pixel, &f_l1) ||
        write_fits_file(light2, 8, 8, const_field_pixel, &f_l2) ||
        write_fits_file(mb, 8, 8, const_field_pixel, &f_b) ||
        write_fits_file(md, 8, 8, const_field_pixel, &f_dk) ||
        write_fits_file(mf, 8, 8, const_field_pixel, &f_fl)) {
        std::fprintf(stderr, "fixture setup failed\n");
        return 2;
    }
    const std::string cfg_base =
        std::string("{\"input_lights\":[\"") + light1 + "\",\"" + light2 +
        "\"],\"output_dir\":\"" + out_dir +
        "\",\"master_bias\":\"" + mb +
        "\",\"master_dark\":\"" + md +
        "\",\"master_flat\":\"" + mf +
        "\",\"cosmetic\":{\"enabled\":false}}";

    // ── P1: validate 幂等纯读 (同 handle 两次 validate 同果; 不落任何 IO) ──
    {
        astrocs_host_services_v1 host = make_host2(nullptr, nullptr, 1);
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        acs_span_u8 cfg = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(cfg_base.c_str())),
                                      static_cast<std::uint64_t>(cfg_base.size()));
        const acs_status r1 = p1_session_validate(h, cfg);
        const acs_status r2 = p1_session_validate(h, cfg);
        P1SESS_CHECK_EQ(cs, r1, ACS_OK);
        P1SESS_CHECK_EQ(cs, r2, ACS_OK);
        // validate 后未 run: manifest 仍 created (纯读无副作用)
        acs_span_u8 mfb{};
        P1SESS_CHECK_EQ(cs, p1_session_inspect(h, &mfb), ACS_OK);
        json m = json::parse(std::string(reinterpret_cast<const char*>(mfb.data), mfb.count));
        host.allocator.free(host.allocator.user_data, mfb.data);
        P1SESS_CHECK(cs, m.value("status", "") == "created", "p1_validate_no_side_effect");
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    // ── P2: 1-N worker 落盘 bitwise 一致 (budget.max_workers=1/2/4) ──
    // 上位: SCI-CAL-001 §7 确定性不变量 + ALG-CAL-001..006 §6
    // (输出 bitwise 与线程数无关); 线程预算经 host budget 注入
    // (README §6), 本组不断言硬编码核数。
    {
        std::vector<float> ref_px;
        int rw = 0, rh = 0;
        for (const std::uint32_t workers : {1u, 2u, 4u}) {
            CancelFlag cf;
            LogSink ls;
            astrocs_host_services_v1 host = make_host2(&cf, &ls, workers);
            acs_handle h = nullptr;
            P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
            acs_span_u8 cfg = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(cfg_base.c_str())),
                                          static_cast<std::uint64_t>(cfg_base.size()));
            const acs_status rc = p1_session_run(h, cfg, 0);
            P1SESS_CHECK_MSG(cs, rc == ACS_OK, "p2_run_ok", "workers=%u rc=%d", workers, static_cast<int>(rc));
            acs_span_u8 mfb{};
            P1SESS_CHECK_EQ(cs, p1_session_inspect(h, &mfb), ACS_OK);
            json m = json::parse(std::string(reinterpret_cast<const char*>(mfb.data), mfb.count));
            host.allocator.free(host.allocator.user_data, mfb.data);
            P1SESS_CHECK(cs, m.value("status", "") == "partial", "p2_partial");
            P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
            // 独立 oracle 通道读回
            const std::string artifact = out_dir + "/calibrated_light1.fts";
            int cw = 0, ch = 0;
            std::vector<float> px;
            if (!oracle_read_fits_f32(artifact, &cw, &ch, &px)) {
                P1SESS_CHECK(cs, false, "p2_readback");
                continue;
            }
            if (workers == 1u) {
                ref_px = px;
                rw = cw;
                rh = ch;
            } else {
                P1SESS_CHECK_EQ(cs, cw, rw);
                P1SESS_CHECK_EQ(cs, ch, rh);
                for (std::size_t i = 0; i < px.size() && i < ref_px.size(); ++i)
                    if (!bits_eq_f(px[i], ref_px[i])) {
                        P1SESS_CHECK_MSG(cs, false, "p2_worker_bitwise",
                                         "px[%zu] workers=%u bitwise differs", i, workers);
                        break;
                    }
            }
        }
    }

    // ── P3: 取消传播 — 置位后 run → CANCELLED, 不写 complete ──
    {
        CancelFlag cf;
        cf.flag.store(1);   // 先置位: 第一取消点 (io_read 文件粒度) 即触发
        LogSink ls;
        astrocs_host_services_v1 host = make_host2(&cf, &ls, 1);
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        // P3 专属输出目录 (P2 已在同 out_dir 落盘产物; 取消面必须隔离观察)
        const std::string cancel_out = base + "/out_cancel";
        fs::create_directories(cancel_out, ec);
        const std::string cfg_cancel =
            std::string("{\"input_lights\":[\"") + light1 + "\",\"" + light2 +
            "\"],\"output_dir\":\"" + cancel_out +
            "\",\"master_bias\":\"" + mb +
            "\",\"master_dark\":\"" + md +
            "\",\"master_flat\":\"" + mf +
            "\",\"cosmetic\":{\"enabled\":false}}";
        acs_span_u8 cfg = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(cfg_cancel.c_str())),
                                      static_cast<std::uint64_t>(cfg_cancel.size()));
        const acs_status rc = p1_session_run(h, cfg, 0);
        P1SESS_CHECK_MSG(cs, rc == ACS_ERR_CANCELLED, "p3_cancel_rc",
                         "rc=%d (want ACS_ERR_CANCELLED=%d)", static_cast<int>(rc),
                         static_cast<int>(ACS_ERR_CANCELLED));
        acs_span_u8 mfb{};
        P1SESS_CHECK_EQ(cs, p1_session_inspect(h, &mfb), ACS_OK);
        json m = json::parse(std::string(reinterpret_cast<const char*>(mfb.data), mfb.count));
        host.allocator.free(host.allocator.user_data, mfb.data);
        // 验收关键词: 取消不写完成 manifest
        P1SESS_CHECK_MSG(cs, m.value("status", "") != "complete", "p3_not_complete",
                         "status=%s", m.value("status", "?").c_str());
        // 取消阶段标 cancelled (README §5: 文件粒度取消点)
        bool has_cancelled_stage = false;
        for (const auto& st : m.value("stages", json::array()))
            if (st.value("status", "") == "cancelled") has_cancelled_stage = true;
        P1SESS_CHECK(cs, has_cancelled_stage, "p3_cancelled_stage");
        // 无伪完整产物 (头合同: 取消→不留伪完整产物)。取消发生在 io_read
        // 首文件粒度, master/light 均未读 → 无任何产物落盘。
        bool any_artifact = false;
        for (const char* n : {"calibrated_light1.fts", "calibrated_light2.fts"})
            if (fs::exists(cancel_out + "/" + n)) any_artifact = true;
        P1SESS_CHECK(cs, !any_artifact, "p3_no_artifact");
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    // ── P4: 异常屏障 — 合同外错型直调 run 不 terminate (错误码呈现) ──
    // 上位: SCI-CAL-001 §8 + ALG-CAL-001..006 §7 边界/invalid 表
    // (AC_ERR_PARAM、out 不写); 装配层错误映射见 README §5。
    {
        astrocs_host_services_v1 host = make_host2(nullptr, nullptr, 1);
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        // dark_optimization 错型 string → run 期 doc.value<bool> 抛 → 屏障捕
        const std::string bad_cfg =
            std::string("{\"input_lights\":[\"") + light1 +
            "\"],\"output_dir\":\"" + out_dir +
            "\",\"dark_optimization\":\"oops\"}";
        acs_span_u8 cfg = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(bad_cfg.c_str())),
                                      static_cast<std::uint64_t>(bad_cfg.size()));
        const acs_status rc = p1_session_run(h, cfg, 0);
        P1SESS_CHECK_MSG(cs, rc == ACS_ERR_PARAM || rc == ACS_ERR_INTERNAL, "p4_barrier_rc",
                         "rc=%d (no terminate; PARAM or INTERNAL acceptable)", static_cast<int>(rc));
        const std::string le = astrocs::phase1::last_error(h);
        P1SESS_CHECK(cs, !le.empty(), "p4_last_error_filled");
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    // ── P5: async_io_depth 值域 {0,1,2} + 3 拒 (边界面) ──
    {
        astrocs_host_services_v1 host = make_host2(nullptr, nullptr, 1);
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        acs_span_u8 cfg = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(cfg_base.c_str())),
                                      static_cast<std::uint64_t>(cfg_base.size()));
        P1SESS_CHECK_EQ(cs, p1_session_run(h, cfg, 0), ACS_OK);
        P1SESS_CHECK_EQ(cs, p1_session_run(h, cfg, 2), ACS_OK);
        P1SESS_CHECK_EQ(cs, p1_session_run(h, cfg, -1), ACS_ERR_PARAM);
        P1SESS_CHECK_EQ(cs, p1_session_run(h, cfg, 3), ACS_ERR_PARAM);
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    // ── P6: ABI 负面 — create host 结构错 → ABI_MISMATCH ──
    {
        astrocs_host_services_v1 host = make_host2(nullptr, nullptr, 1);
        acs_handle h = nullptr;
        const std::uint32_t save_size = host.struct_size;
        host.struct_size = save_size - 1;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_ERR_ABI_MISMATCH);
        host.struct_size = save_size;
        host.abi_version = ACS_ABI_VERSION_V1 + 1;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_ERR_ABI_MISMATCH);
        host.abi_version = ACS_ABI_VERSION_V1;
        P1SESS_CHECK_EQ(cs, p1_session_create(nullptr, &h), ACS_ERR_ABI_MISMATCH);
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, nullptr), ACS_ERR_PARAM);
    }

    // ── P7: 空参数域 — run/inspect/destroy NULL handle 拒 ──
    {
        acs_span_u8 cfg{};
        P1SESS_CHECK_EQ(cs, p1_session_run(nullptr, cfg, 0), ACS_ERR_PARAM);
        acs_span_u8 out{};
        P1SESS_CHECK_EQ(cs, p1_session_inspect(nullptr, &out), ACS_ERR_PARAM);
        P1SESS_CHECK_EQ(cs, p1_session_destroy(nullptr), ACS_ERR_PARAM);
        astrocs_host_services_v1 host = make_host2(nullptr, nullptr, 1);
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        P1SESS_CHECK_EQ(cs, p1_session_run(h, cfg, 0), ACS_ERR_PARAM);   // 空 config span
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    if (cs.failures == 0) return 0;
    std::fprintf(stderr, "properties group: %d failure(s)\n", cs.failures);
    return 1;
}

}  // namespace p1sess
