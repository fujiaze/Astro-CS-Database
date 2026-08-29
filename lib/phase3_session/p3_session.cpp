// lib/phase3_session/p3_session.cpp — Phase3 会话 (API-P3-001) — CLI-006
// 组装 P3-001(P3 sampler_open 即 properties 严格校验)→P3-002(WCS)→P3-003(采样)→P3-004(原子写)。
#include "p3_session.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "p3_output.h"
#include "p3_resample.h"
#include "p3_wcs.h"

using nlohmann::json;

namespace astrocs::phase3 {
namespace {
struct SessionState {
    const astrocs_host_services_v1* host = nullptr;
    std::string last_error;
    json result;
    bool ran = false;

    void log(int level, const char* component, const std::string& msg) const {
        if (host && host->logger.log)
            host->logger.log(host->logger.user_data, level, component, msg.c_str());
    }
    bool cancelled() const {
        return host && host->cancel.is_cancelled &&
               host->cancel.is_cancelled(host->cancel.user_data);
    }
};
}  // namespace

std::string last_error(acs_handle h) {
    if (!h) return "";
    auto* s = reinterpret_cast<SessionState*>(h);
    return s->last_error;
}

}  // namespace astrocs::phase3

using namespace astrocs::phase3;

extern "C" {

acs_status p3_session_create(const astrocs_host_services_v1* host, acs_handle* out) {
    if (!host || host->struct_size != sizeof(astrocs_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (!out) return ACS_ERR_PARAM;
    auto* s = new (std::nothrow) SessionState();
    if (!s) return ACS_ERR_NOMEM;
    s->host = host;
    s->result = json::object();
    *out = reinterpret_cast<acs_handle>(s);
    return ACS_OK;
}

acs_status p3_session_destroy(acs_handle h) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s) return ACS_ERR_PARAM;
    delete s;
    return ACS_OK;
}

// 请求解析 + 显式拒清单 (纯校验, 无 IO)
static acs_status parse_request(SessionState* s, const acs_span_u8 req, json* out) {
    if (!s || !req.data || req.count == 0) return ACS_ERR_PARAM;
    json doc;
    try {
        doc = json::parse(std::string(reinterpret_cast<const char*>(req.data),
                                      static_cast<size_t>(req.count)));
    } catch (const json::parse_error& e) {
        s->last_error = std::string("request parse: ") + e.what();
        return ACS_ERR_PARAM;
    }
    if (!doc.is_object()) { s->last_error = "request must be an object"; return ACS_ERR_PARAM; }
    // source.hips_dir 必填
    if (!doc.contains("source") || !doc["source"].is_object() ||
        !doc["source"].contains("hips_dir") || !doc["source"]["hips_dir"].is_string()) {
        s->last_error = "missing source.hips_dir";
        return ACS_ERR_PARAM;
    }
    if (!doc.contains("center") || !doc["center"].is_object() ||
        !doc["center"].contains("ra_deg") || !doc["center"].contains("dec_deg")) {
        s->last_error = "missing center.ra_deg/dec_deg";
        return ACS_ERR_PARAM;
    }
    // projection 仅 TAN
    const std::string proj = doc.value("projection", std::string("TAN"));
    if (proj != "TAN") { s->last_error = "projection must be TAN"; return ACS_ERR_UNSUPPORTED; }
    // frame≠ICRS 显式拒(请求层 frame, 缺省 icrs)
    if (doc.contains("frame")) {
        const std::string fr = doc["frame"].get<std::string>();
        if (fr != "icrs" && fr != "ICRS") {
            s->last_error = "frame must be icrs";
            return ACS_ERR_UNSUPPORTED;
        }
    }
    const double dec = doc["center"]["dec_deg"].get<double>();
    if (std::fabs(dec) < 5.0) { s->last_error = "|center.dec_deg| must be ≥ 5°"; return ACS_ERR_PARAM; }
    const double ra = doc["center"]["ra_deg"].get<double>();
    const double scale = doc.value("scale_deg_per_px", 0.0);
    if (!(scale > 0.0)) { s->last_error = "scale_deg_per_px must be > 0"; return ACS_ERR_PARAM; }
    const int wpx = doc.value("width_px", 0);
    const int hpx = doc.value("height_px", 0);
    if (wpx < 1 || wpx > 20000 || hpx < 1 || hpx > 20000) {
        s->last_error = "width_px/height_px must be in [1,20000]";
        return ACS_ERR_PARAM;
    }
    const std::string sampler = doc.value("sampler", std::string("bilinear"));
    if (sampler != "nearest" && sampler != "bilinear") {
        s->last_error = "sampler must be nearest|bilinear";
        return ACS_ERR_PARAM;
    }
    const std::string parity = doc.value("longitude_parity", std::string("east_left"));
    if (parity != "east_left" && parity != "east_right") {
        s->last_error = "longitude_parity must be east_left|east_right";
        return ACS_ERR_PARAM;
    }
    const int bitpix = doc.value("bitpix", -32);
    if (bitpix != -32 && bitpix != -64) { s->last_error = "bitpix must be -32|-64"; return ACS_ERR_PARAM; }
    const std::string covout = doc.value("coverage_output", std::string("mask"));
    if (covout != "mask") { s->last_error = "coverage_output must be mask"; return ACS_ERR_PARAM; }
    *out = doc;
    return ACS_OK;
}

acs_status p3_session_validate(acs_handle h, const acs_span_u8 request_json) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s) return ACS_ERR_PARAM;
    json doc;
    return parse_request(s, request_json, &doc);
}

acs_status p3_session_run(acs_handle h, const acs_span_u8 request_json) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s) return ACS_ERR_PARAM;
    json doc;
    const acs_status prc = parse_request(s, request_json, &doc);
    if (prc != ACS_OK) return prc;
    s->ran = true;

    const std::string hips_dir = doc["source"]["hips_dir"].get<std::string>();
    const double ra = doc["center"]["ra_deg"].get<double>();
    const double dec = doc["center"]["dec_deg"].get<double>();
    const double scale = doc.value("scale_deg_per_px", 0.0);
    const int wpx = doc.value("width_px", 0);
    const int hpx = doc.value("height_px", 0);
    const std::string sampler = doc.value("sampler", std::string("bilinear"));
    const std::string parity = doc.value("longitude_parity", std::string("east_left"));
    const int max_tiles = doc.value("max_tiles", 1024);

    // WCS (P3-002)
    P3WcsDescriptor wcs{};
    const P3WcsStatus wst = p3_wcs_make(ra, dec, scale, wpx, hpx, parity.c_str(), 0.0, &wcs);
    if (wst != P3_WCS_OK) {
        s->last_error = "WCS construction rejected";
        return (wst == P3_WCS_UNSUPPORTED) ? ACS_ERR_UNSUPPORTED : ACS_ERR_PARAM;
    }

    // sampler open (P3-001 properties 严格校验 + 打开 signal)
    P3Sampler samp{};
    std::string serr;
    const P3ResampleStatus sst = p3_sampler_open(hips_dir.c_str(), &samp, &serr);
    if (sst != P3_RS_OK) {
        s->last_error = "sampler open: " + serr;
        return (sst == P3_RS_IO) ? ACS_ERR_IO : (sst == P3_RS_UNSUPPORTED) ? ACS_ERR_UNSUPPORTED
                                                                          : ACS_ERR_PARAM;
    }

    // order select (P3-003): max_order=20 冻结内存守卫
    int order_sel = -1;
    p3_order_select(20, scale, &order_sel);

    // 输出平面: S/C
    const long nelem = (long)wpx * hpx;
    std::vector<float> sig((size_t)nelem, std::nanf(""));
    std::vector<float> cov((size_t)nelem, 0.0f);
    int cancelled_row = -1;

    // 行带粒度采样 + 取消点 (禁硬编码线程数: 单线程串行, budget 有多核可扩——P3 采样线程由上层注入)
    for (int y = 0; y < hpx; ++y) {
        if (s->cancelled()) { cancelled_row = y; break; }
        for (int x = 0; x < wpx; ++x) {
            double px_ra = 0, px_dec = 0;
            if (p3_wcs_pix2world(&wcs, (double)x, (double)y, &px_ra, &px_dec) != P3_WCS_OK)
                continue;   // 半球外像素保持 NaN/0
            float v = 0; int c = 0;
            const P3ResampleStatus rst = (sampler == "nearest")
                                             ? p3_sample_nearest(&samp, px_ra, px_dec, &v, &c)
                                             : p3_sample_bilinear(&samp, px_ra, px_dec, &v, &c);
            if (rst != P3_RS_OK) continue;
            const long i = (long)y * wpx + x;
            sig[(size_t)i] = (c == 1) ? v : std::nanf("");
            cov[(size_t)i] = (c == 1) ? 1.0f : 0.0f;
        }
    }
    if (cancelled_row >= 0) {
        p3_sampler_close(&samp);
        s->last_error = "cancelled at row " + std::to_string(cancelled_row);
        return ACS_ERR_CANCELLED;
    }

    // provenance
    const std::string order_sel_str = std::to_string(order_sel);
    P3Provenance prov{};
    prov.hips_id = "ivo://astrocs/phase3";
    prov.manifest_hash = nullptr;
    prov.missing_tiles = nullptr;
    prov.missing_count = 0;
    prov.software_version = "0.1.0";
    prov.run_id = "phase3-run";
    prov.order_sel_used = order_sel_str.c_str();
    prov.sampler_used = sampler.c_str();

    const std::string out_path = std::string(hips_dir) + "/../output_phase3.fits";
    // 用 output_dir 若在请求中
    std::string opath = out_path;
    if (doc.contains("output_dir") && doc["output_dir"].is_string())
        opath = doc["output_dir"].get<std::string>() + "/output_phase3.fits";

    P3OutputResult ores{};
    const P3OutputStatus ost = p3_output_write_atomic(
        sig.data(), cov.data(), wpx, hpx, &wcs, "Jy/beam", opath.c_str(), &prov, -1, &ores);
    p3_sampler_close(&samp);
    if (ost != P3_OUT_OK) {
        s->last_error = "output write failed";
        return ACS_ERR_IO;
    }

    // inspect result
    long covn = 0;
    for (long i = 0; i < nelem; ++i) if (cov[(size_t)i] > 0.5f) ++covn;
    s->result = {{"kind", "astrocs_phase3_session"},
                 {"run_id", "phase3-run"},
                 {"exit_code", 0},
                 {"output_fits_path", opath},
                 {"sha256", ores.sha256},
                 {"order_sel_used", order_sel},
                 {"sampler_used", sampler},
                 {"coverage_stats", {{"covered_px", covn}, {"total_px", nelem}}},
                 {"provenance",
                  {{"hips_id", "ivo://astrocs/phase3"},
                   {"missing_tiles", json::array()},
                   {"software_version", "0.1.0"}}}};
    s->last_error.clear();
    return ACS_OK;
}

acs_status p3_session_inspect(acs_handle h, acs_span_u8* out_result_json) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s || !out_result_json) return ACS_ERR_PARAM;
    const std::string txt = s->result.dump();
    char* buf = static_cast<char*>(s->host->allocator.alloc(s->host->allocator.user_data,
                                                           txt.size() + 1, 8));
    if (!buf) return ACS_ERR_NOMEM;
    std::memcpy(buf, txt.data(), txt.size());
    buf[txt.size()] = '\0';
    out_result_json->data = reinterpret_cast<uint8_t*>(buf);
    out_result_json->count = txt.size();
    return ACS_OK;
}

}  // extern "C"
