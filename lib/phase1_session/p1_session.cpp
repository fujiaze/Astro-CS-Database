// lib/phase1_session/p1_session.cpp — Phase1 进程内会话实现 (API-P1-001) — CLI-004
// 阶段序列: io_read → calibrate → cosmetic → io_write; 与 PHASE1_API_V1 §1 合同一致。
// 线程: ac_set_num_threads(budget.max_workers) 注入( 迁移整改点; 禁硬编码核数)。
// 取消: 帧粒度(每帧前查 cancel); 取消→清理+ACS_ERR_CANCELLED, 不留伪完整产物。
#include "p1_session.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <future>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

extern "C" {
#include "astro_calibration.h"
}
#include "astro_image_io.h"

namespace {

using json = nlohmann::json;

struct SessionState {
    const astrocs_host_services_v1* host = nullptr;
    std::string last_error;          // 脱敏摘要(inspect 用)
    json manifest;                   // inspect 输出(逐步填充)
    bool ran = false;

    void log(int level, const char* component, const std::string& msg) const {
        if (host && host->logger.log)
            host->logger.log(host->logger.user_data, level, component, msg.c_str());
    }
    bool cancelled() const {
        return host && host->cancel.is_cancelled && host->cancel.is_cancelled(host->cancel.user_data);
    }
};

const char* ac_err_name(int rc) {
    switch (rc) {
        case AC_OK: return "ok";
        case AC_ERR_PARAM: return "param";
        case AC_ERR_MEMORY: return "memory";
        case AC_ERR_INTERNAL: return "internal";
        default: return "unknown";
    }
}

struct ImageDeleter { void operator()(AIOImageData* p) const; };
void dispose_image(AIOImageData* p);   // aio_free(经 header; 见下)

using ImagePtr = std::unique_ptr<AIOImageData, ImageDeleter>;
void ImageDeleter::operator()(AIOImageData* p) const { dispose_image(p); }

// 释放合同 (IO-002): AIOImageData 必须经 canonical deleter aio_free_image_data 释放,
// 禁止裸 free(aio_image*)——结构体含 data/data_f64/keywords 多指针, 裸 free 会泄漏。
void dispose_image(AIOImageData* p) { if (p) aio_free_image_data(p); }

[[maybe_unused]] acs_status map_aio_err(const char* what, std::string* err) {
    if (err) *err = what;
    return ACS_ERR_IO;
}

// ── P1(bug 狩猎 R6/batchD): cosmetic 值类型甄别 ──
// validate 冻结合同允许 cosmetic 值为 number|bool (p1_session_validate); run 期原
// value(key, dflt) 直接 get<T>, 对错型抛 nlohmann type_error.302 且会穿越 extern "C"
// (合法 config {"enabled":1} 即 terminate)。以下三个读取点统一先分型、显式转换,
// 对齐 validate 合同语义: number(int/uint/float)与 bool 均合法; 合同外类型回退默认值。
bool cosmetic_flag(const json& c, const char* key, bool dflt) {
    const auto it = c.find(key);
    if (it == c.end() || it->is_null()) return dflt;
    if (it->is_boolean()) return it->get<bool>();
    if (it->is_number_unsigned()) return it->get<uint64_t>() != 0;
    if (it->is_number_integer()) return it->get<int64_t>() != 0;
    if (it->is_number_float()) return it->get<double>() != 0.0;
    return dflt;
}

float cosmetic_float(const json& c, const char* key, float dflt) {
    const auto it = c.find(key);
    if (it == c.end() || it->is_null()) return dflt;
    if (it->is_boolean()) return it->get<bool>() ? 1.0f : 0.0f;
    if (it->is_number_float()) return static_cast<float>(it->get<double>());
    if (it->is_number_unsigned()) return static_cast<float>(it->get<uint64_t>());
    if (it->is_number_integer()) return static_cast<float>(it->get<int64_t>());
    return dflt;
}

int cosmetic_int(const json& c, const char* key, int dflt) {
    const auto it = c.find(key);
    if (it == c.end() || it->is_null()) return dflt;
    if (it->is_boolean()) return it->get<bool>() ? 1 : 0;
    if (it->is_number_unsigned()) {
        const auto uv = it->get<uint64_t>();
        return uv <= static_cast<uint64_t>(std::numeric_limits<int>::max())
                   ? static_cast<int>(uv)
                   : dflt;
    }
    if (it->is_number_integer()) {
        const auto iv = it->get<int64_t>();
        return (iv >= static_cast<int64_t>(std::numeric_limits<int>::min()) &&
                iv <= static_cast<int64_t>(std::numeric_limits<int>::max()))
                   ? static_cast<int>(iv)
                   : dflt;
    }
    if (it->is_number_float()) {
        const double dv = it->get<double>();
        return (dv >= static_cast<double>(std::numeric_limits<int>::min()) &&
                dv <= static_cast<double>(std::numeric_limits<int>::max()))
                   ? static_cast<int>(dv)   // 截断语义, 与原 get<int> 一致 (4.0→4)
                   : dflt;
    }
    return dflt;
}

// 异常屏障善后: 中途抛出时尾阶段若仍标 running, 修正为 fail (manifest 如实)。
void fail_trailing_running_stage(SessionState* s) {
    auto& stages = s->manifest["stages"];
    if (stages.is_array() && !stages.empty() &&
        stages.back().value("status", std::string()) == "running")
        stages.back()["status"] = "fail";
}

// 会话主体 (定义见文件后段, extern "C" 块外的匿名 namespace)
acs_status run_session(SessionState* s, const json& doc);

}  // namespace
namespace {

/* 读单帧; 失败→nullptr 并填 err。用 aio_read 自动探测: .fts/.fits → FITS, .xisf 校准母版 → XISF */
ImagePtr read_image(const std::string& path, std::string* err) {
    AIOImageData* im = aio_read(path.c_str());
    if (!im) {
        *err = "cannot read image: " + path;
        return nullptr;
    }
    return ImagePtr(im);
}

int image_w(const AIOImageData* im) { return aio_get_geometry(im).width; }
int image_h(const AIOImageData* im) { return aio_get_geometry(im).height; }
float* image_px(const AIOImageData* im) { return aio_get_pixel_data(const_cast<AIOImageData*>(im)); }

}  // namespace


extern "C" {

acs_status p1_session_create(const astrocs_host_services_v1* host, acs_handle* out) {
    if (!host || host->struct_size != sizeof(astrocs_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (!out) return ACS_ERR_PARAM;
    auto* s = new (std::nothrow) SessionState();
    if (!s) return ACS_ERR_NOMEM;
    s->host = host;
    s->manifest = {{"kind", "astrocs_phase1_session"}, {"stages", json::array()}};
    *out = reinterpret_cast<acs_handle>(s);
    return ACS_OK;
}

acs_status p1_session_validate(acs_handle h, const acs_span_u8 config_json) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s || !config_json.data || config_json.count == 0) return ACS_ERR_PARAM;
    json doc;
    try {
        doc = json::parse(std::string(reinterpret_cast<const char*>(config_json.data),
                                      static_cast<size_t>(config_json.count)));
    } catch (const json::parse_error& e) {
        s->last_error = std::string("config parse: ") + e.what();
        return ACS_ERR_PARAM;
    }
    if (!doc.is_object()) { s->last_error = "config must be an object"; return ACS_ERR_PARAM; }
    for (const char* req : {"input_lights", "output_dir"})
        if (!doc.contains(req)) {
            s->last_error = std::string("missing key '") + req + "'";
            return ACS_ERR_PARAM;
        }
    if (!doc["input_lights"].is_array() || doc["input_lights"].empty() ||
        !doc["output_dir"].is_string()) {
        s->last_error = "input_lights must be non-empty array; output_dir must be string";
        return ACS_ERR_PARAM;
    }
    for (const auto& l : doc["input_lights"])
        if (!l.is_string()) { s->last_error = "input_lights items must be strings"; return ACS_ERR_PARAM; }
    for (const char* opt : {"master_bias", "master_dark", "master_flat"})
        if (doc.contains(opt) && !doc[opt].is_null() && !doc[opt].is_string()) {
            s->last_error = std::string(opt) + " must be string or null";
            return ACS_ERR_PARAM;
        }
    if (doc.contains("cosmetic")) {
        const auto& c = doc["cosmetic"];
        if (!c.is_object()) { s->last_error = "cosmetic must be object"; return ACS_ERR_PARAM; }
        for (const auto& [k, v] : c.items())
            if (!v.is_number() && !v.is_boolean()) {
                s->last_error = "cosmetic values must be numeric/bool";
                return ACS_ERR_PARAM;
            }
    }
    if (doc.contains("dark_optimization") && !doc["dark_optimization"].is_boolean()) {
        s->last_error = "dark_optimization must be boolean";
        return ACS_ERR_PARAM;
    }
    return ACS_OK;
}


acs_status p1_session_run(acs_handle h, const acs_span_u8 config_json, int async_io_depth) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s || !config_json.data || config_json.count == 0) return ACS_ERR_PARAM;
    if (async_io_depth < 0 || async_io_depth > 2) return ACS_ERR_PARAM;
    json doc;
    try {
        doc = json::parse(std::string(reinterpret_cast<const char*>(config_json.data),
                                      static_cast<size_t>(config_json.count)));
    } catch (...) {
        s->last_error = "config parse failed (validate first)";
        return ACS_ERR_PARAM;
    }
    // P1(bug 狩猎 R6/batchD) 层②: 跨 C 边界异常屏障 (对齐已入库家族方案 f1cb487c 的
    // C 接口 catch→错误码纪律)。config 值类型错/资源异常以错误码+last_error 呈现,
    // 异常禁止穿越 extern "C" (否则 std::terminate)。
    try {
        return run_session(s, doc);
    } catch (const json::exception& e) {
        fail_trailing_running_stage(s);
        s->last_error = std::string("config value type error: ") + e.what();
        return ACS_ERR_PARAM;
    } catch (const std::bad_alloc&) {
        fail_trailing_running_stage(s);
        s->last_error = "session run: out of memory";
        return ACS_ERR_NOMEM;
    } catch (const std::exception& e) {
        fail_trailing_running_stage(s);
        s->last_error = std::string("session run exception: ") + e.what();
        return ACS_ERR_INTERNAL;
    } catch (...) {
        fail_trailing_running_stage(s);
        s->last_error = "session run: unknown exception";
        return ACS_ERR_INTERNAL;
    }
}

}  // extern "C"

namespace {

// 会话主体(原 p1_session_run 函数体, P1 batchD 整改):
//  - 各阶段 json st 改为 manifest 内元素引用 (json&), 状态/计数直接写穿 manifest;
//  - cosmetic 值读取经类型甄别 helper (cosmetic_flag/float/int), 对齐 validate
//    number|bool 合同, 错型不再抛 type_error.302。
// 异常合同: 本函数可抛 (json::exception 等), 由 p1_session_run 的 C 边界屏障捕获。
acs_status run_session(SessionState* s, const json& doc) {
    // 线程预算注入( 迁移整改点): worker 数=预算快照, 禁硬编码
    ac_set_num_threads(static_cast<int>(s->host->budget.max_workers));
    s->log(ACS_LOG_INFO, "phase1", "session run: omp threads=" +
               std::to_string(s->host->budget.max_workers) +
               " (budget injected, cpus=" + std::to_string(s->host->budget.available_cpus) + ")");

    std::string err;
    // ── 阶段 1: io_read(masters+lights; 取消点=文件粒度) ──
    {
        s->manifest["stages"].emplace_back(json{{"name", "io_read"}, {"status", "running"}});
        s->log(ACS_LOG_INFO, "phase1", "stage io_read start");
        std::vector<std::string> outs;
        for (const char* k : {"master_bias", "master_dark", "master_flat"})
            if (doc.contains(k) && !doc[k].is_null()) outs.push_back(doc[k].get<std::string>());
        for (const auto& l : doc["input_lights"]) outs.push_back(l.get<std::string>());
        for (const auto& p : outs) {
            if (s->cancelled()) {
                s->manifest["stages"].back()["status"] = "cancelled";
                return ACS_ERR_CANCELLED;
            }
            auto im = read_image(p, &err);
            if (!im) {
                s->last_error = err;
                s->manifest["error_kind"] = "input";
                s->manifest["stages"].back()["status"] = "fail";
                return ACS_ERR_IO;
            }
        }
        s->manifest["stages"].back()["status"] = "ok";
        s->manifest["stages"].back()["files"] = static_cast<uint64_t>(outs.size());
        s->log(ACS_LOG_INFO, "phase1", "stage io_read ok: " + std::to_string(outs.size()) + " files");
    }

    // ── 阶段 2: calibrate(逐帧; master 全可空; 取消点=帧粒度) ──
    const std::string out_dir = doc.value("output_dir", std::string());
    uint32_t frames_ok = 0;
    {
        s->manifest["stages"].emplace_back(json{{"name", "calibrate"}, {"status", "running"}});
        ImagePtr bias, dark, flat;
        if (doc.contains("master_bias") && !doc["master_bias"].is_null())
            if (!(bias = read_image(doc["master_bias"].get<std::string>(), &err))) {
                s->last_error = err; s->manifest["error_kind"] = "input"; return ACS_ERR_IO;
            }
        if (doc.contains("master_dark") && !doc["master_dark"].is_null())
            if (!(dark = read_image(doc["master_dark"].get<std::string>(), &err))) {
                s->last_error = err; s->manifest["error_kind"] = "input"; return ACS_ERR_IO;
            }
        if (doc.contains("master_flat") && !doc["master_flat"].is_null())
            if (!(flat = read_image(doc["master_flat"].get<std::string>(), &err))) {
                s->last_error = err; s->manifest["error_kind"] = "input"; return ACS_ERR_IO;
            }
        int W = -1, H = -1;
        for (const auto* im : {bias.get(), dark.get(), flat.get()})
            if (im) {
                if (W < 0) { W = image_w(im); H = image_h(im); }
                else if (image_w(im) != W || image_h(im) != H) {
                    s->last_error = "master frame size mismatch";
                    s->manifest["stages"].back()["status"] = "fail";
                    return ACS_ERR_PARAM;
                }
            }
        const bool dark_opt = doc.value("dark_optimization", false);
        const float k_fixed = doc.value("dark_scale_factor", 1.0f);
        float actual_k = 0;
        json per_frame = json::array();
        for (const auto& lp : doc["input_lights"]) {
            if (s->cancelled()) {
                s->manifest["stages"].back()["status"] = "cancelled";
                return ACS_ERR_CANCELLED;   // 帧粒度取消点(API 冻结)
            }
            const std::string path = lp.get<std::string>();
            auto light = read_image(path, &err);
            if (!light) { s->last_error = err; s->manifest["error_kind"] = "input"; s->manifest["stages"].back()["status"] = "fail"; return ACS_ERR_IO; }
            if (W >= 0 && (image_w(light.get()) != W || image_h(light.get()) != H)) {
                s->last_error = "light size mismatch vs masters: " + path;
                s->manifest["stages"].back()["status"] = "fail";
                return ACS_ERR_PARAM;
            }
            W = image_w(light.get()); H = image_h(light.get());
            std::vector<float> out(static_cast<size_t>(W) * static_cast<size_t>(H), 0.0f);
            const int rc = ac_calibrate_frame(
                image_px(light.get()), W, H,
                dark ? image_px(dark.get()) : nullptr,
                flat ? image_px(flat.get()) : nullptr,
                bias ? image_px(bias.get()) : nullptr,
                out.data(), dark_opt ? 1 : 0, k_fixed, &actual_k);
            if (rc != AC_OK) {
                s->last_error = "ac_calibrate_frame failed: " + path +
                                " rc=" + ac_err_name(rc);
                s->manifest["stages"].back()["status"] = "fail";
                return rc == AC_ERR_MEMORY ? ACS_ERR_NOMEM : ACS_ERR_INTERNAL;
            }
            // 校准输出落盘(io_write 阶段前缀路径)
            AIOImageData* wim = aio_read_fits(path.c_str());   // 复用读结构再覆写像素
            if (!wim) { s->last_error = "re-read failed: " + path; return ACS_ERR_IO; }
            std::memcpy(image_px(wim), out.data(), out.size() * sizeof(float));
            const size_t slash = path.find_last_of("/\\");
            const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
            const std::string outp = out_dir + "/calibrated_" + base;
            if (aio_write_fits(wim, outp.c_str()) != 0) {
                s->last_error = "write failed: " + outp;
                s->manifest["error_kind"] = "output";
                aio_free_image_data(wim);
                s->manifest["stages"].back()["status"] = "fail";
                return ACS_ERR_IO;
            }
            aio_free_image_data(wim);
            ++frames_ok;
            per_frame.push_back({{"input", base},
                                 {"output", "calibrated_" + base},
                                 {"dark_scale", dark_opt ? actual_k : k_fixed}});
            s->manifest["artifacts"] = s->manifest.value("artifacts", json::array());
            s->manifest["artifacts"].push_back(outp);
        }
        s->manifest["stages"].back()["status"] = "ok";
        s->manifest["stages"].back()["frames"] = frames_ok;
        s->manifest["stages"].back()["per_frame"] = per_frame;
        s->log(ACS_LOG_INFO, "phase1", "stage calibrate ok: " + std::to_string(frames_ok) + " frames");
    }

    // ── 阶段 3: cosmetic(校准帧坏点修复; in-place 输出覆写) ──
    // P1(bug 狩猎 R6/batchD) 整改:
    //  1) json& st = manifest 内元素引用 — 状态/计数直接写穿 manifest
    //     (原 json st 局部拷贝, push_back 后的修改落在副本上, manifest 恒 "running");
    //  2) 值读取经 cosmetic_flag/float/int 甄别: validate 冻结合同允许 number|bool,
    //     {"enabled":1} 等错型 config 不得抛 type_error.302 (原 run 期 get<T> 即崩)。
    if (doc.contains("cosmetic") && cosmetic_flag(doc["cosmetic"], "enabled", true)) {
        json& st = s->manifest["stages"].emplace_back(
            json{{"name", "cosmetic"}, {"status", "running"}});
        const json& c = doc["cosmetic"];
        const float hot_sigma = cosmetic_float(c, "hot_sigma", 5.0f);
        const float cold_sigma = cosmetic_float(c, "cold_sigma", 5.0f);
        const int method = cosmetic_int(c, "method", AC_METHOD_MEDIAN) == AC_METHOD_BILINEAR
                               ? AC_METHOD_BILINEAR
                               : AC_METHOD_MEDIAN;
        const int max_structure_size = cosmetic_int(c, "max_structure_size", 4);
        for (const auto& a : s->manifest["artifacts"]) {
            if (s->cancelled()) { st["status"] = "cancelled"; return ACS_ERR_CANCELLED; }
            auto im = read_image(a.get<std::string>(), &err);
            if (!im) { s->last_error = err; st["status"] = "fail"; return ACS_ERR_IO; }
            std::vector<float> fixed(static_cast<size_t>(image_w(im.get())) * static_cast<size_t>(image_h(im.get())));
            int hot = 0, cold = 0;
            const int rc = ac_correct_frame(
                image_px(im.get()), image_w(im.get()), image_h(im.get()),
                nullptr, nullptr, fixed.data(),
                hot_sigma, cold_sigma, method, max_structure_size, &hot, &cold);
            if (rc != AC_OK) {
                s->last_error = std::string("ac_correct_frame failed rc=") + ac_err_name(rc);
                st["status"] = "fail";
                return ACS_ERR_INTERNAL;
            }
            std::memcpy(image_px(im.get()), fixed.data(), fixed.size() * sizeof(float));
            if (aio_write_fits(im.get(), a.get<std::string>().c_str()) != 0) {
                s->last_error = "cosmetic write failed";
                st["status"] = "fail";
                return ACS_ERR_IO;
            }
            st["hot_fixed"] = st.value("hot_fixed", 0) + hot;
            st["cold_fixed"] = st.value("cold_fixed", 0) + cold;
        }
        st["status"] = "ok";
        s->log(ACS_LOG_INFO, "phase1", "stage cosmetic ok");
    }

    // ── 阶段 4: io_write 汇总(校准帧已在 calibrate 写出; 此处校验存在) ──
    // 同型整改: 局部 st 拷贝 → manifest 内引用 (原 io_write 状态亦恒 "running")。
    {
        json& st = s->manifest["stages"].emplace_back(json{{"name", "io_write"}, {"status", "running"}});
        for (const auto& a : s->manifest["artifacts"]) {
            if (!std::filesystem::exists(std::filesystem::u8path(a.get<std::string>()))) {
                s->last_error = "artifact missing after write: " + a.get<std::string>();
                st["status"] = "fail";
                return ACS_ERR_IO;
            }
        }
        st["status"] = "ok";
    }

    s->manifest["frames"] = frames_ok;
    s->manifest["status"] = "complete";
    s->ran = true;
    return ACS_OK;
}

}  // namespace (run_session definition — C++ linkage, 定义在 extern "C" 外)

extern "C" {

acs_status p1_session_inspect(acs_handle h, acs_span_u8* out) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s || !out) return ACS_ERR_PARAM;
    if (!s->ran && s->last_error.empty()) {
        s->manifest["status"] = "created";
    } else if (!s->ran) {
        s->manifest["status"] = "failed";
        s->manifest["error"] = s->last_error;
    }
    const std::string t = s->manifest.dump(2);
    const uint64_t n = t.size() + 1;
    void* p = s->host->allocator.alloc(s->host->allocator.user_data, n, 16);
    if (!p) return ACS_ERR_NOMEM;
    std::memcpy(p, t.c_str(), n);
    out->data = static_cast<uint8_t*>(p);
    out->count = n - 1;
    return ACS_OK;
}

acs_status p1_session_destroy(acs_handle h) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s) return ACS_ERR_PARAM;
    delete s;
    return ACS_OK;
}

}  // extern "C"

namespace astrocs::phase1 {
std::string last_error(acs_handle h) {
    auto* s = reinterpret_cast<SessionState*>(h);
    return s ? s->last_error : std::string();
}
}  // namespace astrocs::phase1
