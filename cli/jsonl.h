// astrocs JSON/JSONL writer (API-002 §3/§4 协议 v1) — CLI-002
// stdout 纪律: --json 恰一个 JSON 文档; --events-jsonl 每行一个 UTF-8 JSON 事件, 禁夹普通文字。
#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include <nlohmann/json.hpp>

#include "exit_codes.h"

namespace astrocs {

inline std::string iso8601_utc_now() {
    const auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

inline std::string make_run_id() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto h = std::hash<long long>{}(static_cast<long long>(now)) & 0xFFFFFFFFFFFFULL;  // 恒 12 hex
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%012llx", static_cast<unsigned long long>(h));
    return buf;
}

// JSONL 事件发射器: 固定 10 必含字段 + kind 扩展; sequence 从 0 单调递增。
class JsonlEmitter {
public:
    JsonlEmitter(bool enabled, std::string run_id, std::string phase)
        : enabled_(enabled), run_id_(std::move(run_id)), phase_(std::move(phase)) {}

    bool enabled() const { return enabled_; }
    const std::string& run_id() const { return run_id_; }

    // kind 基础事件(progress/resource/artifact/backend 由 extra 扩展; final 见 emit_final)
    void emit(const std::string& kind, const std::string& severity, const std::string& stage,
              const std::string& message, const nlohmann::json& extra = {}) {
        if (!enabled_) return;
        nlohmann::json ev = {
            {"schema_version", "1"},
            {"event_id", "evt-" + run_id_ + "-" + std::to_string(seq_)},
            {"run_id", run_id_},
            {"timestamp_utc", iso8601_utc_now()},
            {"sequence", seq_},
            {"kind", kind},
            {"severity", severity},
            {"phase", phase_},
            {"stage", stage},
            {"message", message},
        };
        for (auto it = extra.begin(); it != extra.end(); ++it) ev[it.key()] = it.value();
        std::fputs(ev.dump().c_str(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        ++seq_;
    }

    void stage(const std::string& name, bool start) {
        emit(start ? "stage_start" : "stage_end", "info", name,
             start ? "stage started" : "stage finished");
    }

    // final 事件: {exit_code,status,run_manifest,summary}
    void emit_final(int exit_code, const std::string& status, const char* run_manifest,
                    const std::string& summary) {
        nlohmann::json extra = {
            {"exit_code", exit_code},
            {"status", status},
            {"run_manifest", run_manifest == nullptr ? nlohmann::json(nullptr)
                                                     : nlohmann::json(run_manifest)},
            {"summary", summary},
        };
        emit("final", exit_code == OK ? "info" : "error", "n/a", summary, extra);
    }

private:
    bool enabled_;
    std::string run_id_;
    std::string phase_;
    unsigned long long seq_ = 0;
};

}  // namespace astrocs
