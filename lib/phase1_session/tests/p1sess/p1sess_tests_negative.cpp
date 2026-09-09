// P1-SESSION-TEST · negative 组: config validate 负面矩阵 (逐键拒收)
//
// 控制包任务: P1-SESSION-TEST (SA-P1SS-T, queue 47, lock-P1-SESSION)。
// 合同锚: README §4 config 键集表 (validate 拒收面: 缺必需键/类型错/
// 空数组/非串元素/master 错型/cosmetic 非对象/值错型/dark_optimization 错型)
// + §5 PARAM 传播。模板覆盖面: invalid/负面矩阵。
// 注: session 层负面 = ABI/参数域/失败传播 (manifest 面), 算法域负面归
// P1-CAL/P1-COS 各自 TEST (本层无算法)。
#include "p1_session.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "p1sess_fixtures.hpp"
#include "p1sess_test_main.hpp"

using json = nlohmann::json;

namespace p1sess {

extern CheckState g_cs;

static void* n_alloc(void*, std::uint64_t n, std::uint64_t) {
    return std::malloc(static_cast<std::size_t>(n));
}
static void n_free(void*, void* p) { std::free(p); }
static int n_not_cancelled(void*) { return 0; }

static astrocs_host_services_v1 n_make_host() {
    astrocs_host_services_v1 h{};
    h.struct_size = sizeof(astrocs_host_services_v1);
    h.abi_version = ACS_ABI_VERSION_V1;
    h.allocator = {sizeof(acs_allocator), ACS_ABI_VERSION_V1, n_alloc, n_free, nullptr};
    h.logger = {sizeof(acs_logger), ACS_ABI_VERSION_V1, nullptr, nullptr};
    h.cancel = {sizeof(acs_cancel), ACS_ABI_VERSION_V1, n_not_cancelled, nullptr};
    h.budget = {sizeof(acs_thread_budget), ACS_ABI_VERSION_V1, 1u, 1u, nullptr, nullptr, nullptr};
    return h;
}

int run_negative() {
    CheckState& cs = g_cs;
    namespace fs = std::filesystem;
    const char* d = std::getenv("TMPDIR");
    const char* w = std::getenv("TEMP");
    const char* w2 = std::getenv("TMP");
    const std::string tmp_root = (d ? d : (w ? w : (w2 ? w2 : ".")));
    const std::string base =
        (fs::path(tmp_root) / "astrocs_p1sess_neg").generic_string();
    const std::string out_dir = base + "/out";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(out_dir, ec);
    ConstField f_l{0x5E55ED0001ULL, 100.0f};
    const std::string light = base + "/light1.fts";
    if (write_fits_file(light, 8, 8, const_field_pixel, &f_l)) return 2;

    // N1: validate 负面矩阵 — 每条 (config, 名称); 全部必须 PARAM 且不崩
    struct NegCase {
        const char* name;
        std::string cfg;
    };
    const std::string L = std::string("[\"") + light + "\"]";
    const std::string O = std::string("\"") + out_dir + "\"";
    const std::vector<NegCase> cases = {
        {"n1_parse_error", "{not json"},
        {"n1_not_object", "[1,2,3]"},
        {"n1_missing_input_lights", std::string("{\"output_dir\":") + O + "}"},
        {"n1_missing_output_dir", std::string("{\"input_lights\":") + L + "}"},
        {"n1_empty_lights", std::string("{\"input_lights\":[],\"output_dir\":") + O + "}"},
        {"n1_lights_not_array", std::string("{\"input_lights\":\"x\",\"output_dir\":") + O + "}"},
        {"n1_output_dir_not_string", std::string("{\"input_lights\":") + L + ",\"output_dir\":123}"},
        {"n1_lights_item_not_string", std::string("{\"input_lights\":[42],\"output_dir\":") + O + "}"},
        {"n1_master_not_string", std::string("{\"input_lights\":") + L + ",\"output_dir\":" + O + ",\"master_bias\":7}"},
        {"n1_cosmetic_not_object", std::string("{\"input_lights\":") + L + ",\"output_dir\":" + O + ",\"cosmetic\":[1]}"},
        {"n1_cosmetic_value_wrong_type", std::string("{\"input_lights\":") + L + ",\"output_dir\":" + O + ",\"cosmetic\":{\"enabled\":\"x\"}}"},
        {"n1_dark_optimization_wrong_type", std::string("{\"input_lights\":") + L + ",\"output_dir\":" + O + ",\"dark_optimization\":1}"},
        {"n1_null_input_lights", std::string("{\"input_lights\":null,\"output_dir\":") + O + "}"},
    };
    for (const auto& c : cases) {
        astrocs_host_services_v1 host = n_make_host();
        acs_handle h = nullptr;
        if (p1_session_create(&host, &h) != ACS_OK || !h) return 2;
        acs_span_u8 cfg = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(c.cfg.c_str())),
                                      static_cast<std::uint64_t>(c.cfg.size()));
        const acs_status rc = p1_session_validate(h, cfg);
        P1SESS_CHECK_MSG(cs, rc == ACS_ERR_PARAM, "n1_validate_reject",
                         "%s: rc=%d (want PARAM=%d)", c.name, static_cast<int>(rc),
                         static_cast<int>(ACS_ERR_PARAM));
        // 失败传播面: last_error 脱敏摘要非空 (README §5)
        const std::string le = astrocs::phase1::last_error(h);
        P1SESS_CHECK_MSG(cs, !le.empty(), "n1_last_error", "%s: last_error empty", c.name);
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    // N2: 合法 config 对照 — 上述矩阵不误伤 (validate 对合法配置 ACS_OK)
    {
        const std::string ok_cfg =
            std::string("{\"input_lights\":") + L + ",\"output_dir\":" + O +
            ",\"master_bias\":null,\"master_dark\":null,\"master_flat\":null" +
            ",\"cosmetic\":{\"enabled\":true,\"hot_sigma\":5},\"dark_optimization\":false}";
        astrocs_host_services_v1 host = n_make_host();
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        acs_span_u8 cfg = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(ok_cfg.c_str())),
                                      static_cast<std::uint64_t>(ok_cfg.size()));
        P1SESS_CHECK_EQ(cs, p1_session_validate(h, cfg), ACS_OK);
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    // N3: master string-or-null 合同 — null 显式合法
    {
        const std::string cfg =
            std::string("{\"input_lights\":") + L + ",\"output_dir\":" + O +
            ",\"master_dark\":null}";
        astrocs_host_services_v1 host = n_make_host();
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        acs_span_u8 sp = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(cfg.c_str())),
                                     static_cast<std::uint64_t>(cfg.size()));
        P1SESS_CHECK_EQ(cs, p1_session_validate(h, sp), ACS_OK);
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    // N4: inspect/destroy 参数域 (null out / null handle)
    {
        astrocs_host_services_v1 host = n_make_host();
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        P1SESS_CHECK_EQ(cs, p1_session_inspect(h, nullptr), ACS_ERR_PARAM);
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
    }

    if (cs.failures == 0) return 0;
    std::fprintf(stderr, "negative group: %d failure(s)\n", cs.failures);
    return 1;
}

}  // namespace p1sess
