// acsd JSON/JSONL writer (API-002 §3/§4 协议 v1) — CLI-002/CLI-004
//
// stdout 纪律（docs/ASTROCS_DESIGN §6.3）：运行事件流是**默认输出**（事件流 = 默认输出，
// 不需要旗标开启；GUI 用其它语言直接捕获 CLI 输出）。
//   * 机器通道 = stdout：每行恰一个 UTF-8 JSON 事件（JSONL），禁夹普通文字；
//     --json（--version/doctor/模板）时 stdout 恰一个 JSON 文档 —— 这些命令不发事件。
//   * 人可读通道 = stderr：与机器 JSONL **同源生成**（同一事件对象；LOG-001 §2 双通道纪律），
//     单行 ≤ 4096 字节，自由文本先脱敏（绝对路径/盘符/UNC/凭据 → <redacted>）。
// CLI-004: 发送侧经 protocol.h ValidateEventV1 硬闸（GUI 可调用进程协议冻结合同）。
// 运行事件流 schema **唯一** = 本文件 + protocol.h；
// LOG-001（lib/infrastructure/observability/logging/**）是**另一份**「结构化日志」合同
// （字段 schema/seq/ts/run/…/level/event），**不是**运行事件流，两者的键名/枚举/工件名
// 不得互相冒充（本文件禁止把 LOG-001 的 seq/event/level 键名当作运行事件字段）。
#pragma once
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "exit_codes.h"
#include "protocol.h"

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
    char buf[64];
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

// ── 单行大小上限（§6.3 stdout 纪律 / LOG-001 §6 同口径：4096 字节，含换行）──
inline constexpr std::size_t kEventLineMaxBytes = 4096;

// ── 自由文本脱敏（LOG-001 §5 同规则；只作用于自由文本字段）──
// 结构化字段（artifact.path 等）是协议合同字段，保持原值（GUI 需要真实路径），
// 但**不进入**人可读通道的自由文本（见 human_summary_of）。
inline const std::vector<std::regex>& redact_patterns() {
    // 注意: std::regex 的 ECMAScript 文法**不支持**内联 (?i) 标志（GCC libstdc++ 抛
    // "Invalid '(?...)' zero-width assertion"）⇒ 大小写不敏感一律用 std::regex::icase。
    const auto icase = std::regex::icase;
    static const std::vector<std::regex> k = {
        std::regex(R"(\b(password|passwd|pwd|token|secret|api[_-]?key|credential|private[_-]?key)\s*[=:]\s*[^\s,;"']+)", icase),
        std::regex(R"(\bAuthorization\s*:\s*Bearer\s+\S+)", icase),
        std::regex(R"(\bbearer\s+[A-Za-z0-9._~+/=:-]+)", icase),
        std::regex(R"([A-Za-z]:\\[^\s"',;]+)"),          // Windows 盘符绝对路径
        std::regex(R"(\\\\[^\\\s"',;]+\\[^\s"',;]+)"),   // UNC 路径
        std::regex(R"(/home/[^/\s"',;]+(?:/[^\s"',;]*)*)"),
        std::regex(R"(/Users/[^/\s"',;]+(?:/[^\s"',;]*)*)"),
        std::regex(R"(/tmp/[^\s"',;]+)"),
        std::regex(R"([a-zA-Z][a-zA-Z0-9+.-]*://[^\s"',;]+)"),   // scheme://...（URL/凭据）
    };
    return k;
}

inline std::string redact_free_text(const std::string& text) {
    std::string out = text;
    for (const auto& pat : redact_patterns())
        out = std::regex_replace(out, pat, "<redacted>");
    return out;
}

// UTF-8 边界裁剪（不切坏多字节字符；尾部补省略号）。
inline std::string clip_utf8(const std::string& text, std::size_t max_bytes) {
    if (text.size() <= max_bytes) return text;
    std::size_t n = 0;
    std::string cut;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        std::size_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > text.size()) len = text.size() - i;
        if (n + len > max_bytes) break;
        cut.append(text, i, len);
        n += len;
        i += len;
    }
    return cut + "…";
}

// 自由文本字段名（脱敏 + 超限裁剪的作用域；结构化路径字段不在内）。
inline bool is_free_text_field(const std::string& key) {
    return key == "message" || key == "summary" || key == "reason" || key == "detail";
}

// 单行 ≤ max_bytes（含换行）：裁剪最长的自由文本字段，保持合法 JSON。
inline std::string fit_event_line(nlohmann::json ev, std::size_t max_bytes = kEventLineMaxBytes) {
    std::string line = ev.dump();
    const std::size_t budget = max_bytes > 1 ? max_bytes - 1 : max_bytes;   // 去掉换行符
    for (int guard = 0; guard < 8 && line.size() > budget; ++guard) {
        std::string longest;
        for (auto it = ev.begin(); it != ev.end(); ++it) {
            if (!it.value().is_string() || !is_free_text_field(it.key())) continue;
            const std::string v = it.value().get<std::string>();
            if (v.size() > longest.size()) longest = it.key();
        }
        if (longest.empty()) break;   // 无可裁剪自由文本（结构化字段不动）
        const std::string v = ev[longest].get<std::string>();
        const std::size_t over = line.size() - budget;
        const std::size_t room = v.size() > over + 8 ? v.size() - over - 8 : 8;
        ev[longest] = clip_utf8(v, room);
        line = ev.dump();
    }
    return line + "\n";
}

// ── 人可读摘要（与机器 JSONL 同源；LOG-001 §4 模板同族，字段名用运行事件流自己的）──
inline std::string human_summary_of(const nlohmann::json& ev) {
    std::string s = "[" + ev.value("timestamp_utc", std::string()) + "] seq=" +
                    std::to_string(ev.value("sequence", 0LL)) + " " +
                    ev.value("severity", std::string()) + " " + ev.value("kind", std::string());
    const std::string phase = ev.value("phase", std::string());
    const std::string stage = ev.value("stage", std::string());
    if (!phase.empty() || !stage.empty()) s += " (" + phase + "/" + stage + ")";
    const std::string msg = ev.value("message", std::string());
    if (!msg.empty()) s += "：" + msg;
    if (ev.contains("exit_code") && ev["exit_code"].is_number_integer())
        s += "（exit_code=" + std::to_string(ev["exit_code"].get<int>()) + "）";
    return clip_utf8(s, kEventLineMaxBytes - 1) + "\n";
}

// JSONL 事件发射器: 固定 10 必含字段 + kind 扩展; sequence 从 0 单调递增。
// **默认启用**（不再是开关）；enabled() 恒真，保留为调用方
// 表达「事件流是唯一 stdout 结果通道」的语义锚。
class JsonlEmitter {
public:
    JsonlEmitter(std::string run_id, std::string phase)
        : run_id_(std::move(run_id)), phase_(std::move(phase)) {}

    bool enabled() const { return true; }
    const std::string& run_id() const { return run_id_; }

    // kind 基础事件(progress/resource/artifact/backend 由 extra 扩展; final 见 emit_final)。
    // CLI-004: 发送侧协议自检硬闸 —— 违反冻结合同(10 必含字段/sequence 单调/kind 扩展
    // 字段/final.exit_code 域)的事件拒绝发出(stderr 诊断), stdout 保持纯 JSONL。
    //
    // P-163: 返回值 = 本事件是否**完整写出**（机器通道 + 人可读通道都成功）。
    //   false 的两种来源可区分：协议硬闸拒发（不置 write_failed_）与 I/O 写失败
    //   （置 write_failed_/write_exit_code_）。写失败**不吞**：fputs 与 fflush 的
    //   返回值都查，失败即落可查询状态 + stderr 结构化诊断行；诊断只走 stderr，
    //   stdout 恒为纯 JSONL（docs/ASTROCS_DESIGN §6.3）。
    bool emit(const std::string& kind, const std::string& severity, const std::string& stage,
              const std::string& message, const nlohmann::json& extra = {}) {
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
            {"message", redact_free_text(message)},
        };
        for (auto it = extra.begin(); it != extra.end(); ++it) {
            ev[it.key()] = (it.value().is_string() && is_free_text_field(it.key()))
                               ? nlohmann::json(redact_free_text(it.value().get<std::string>()))
                               : it.value();
        }
        // protocol.h (CLI-004): 发送前 ValidateEventV1; 违规行禁入 stdout。
        if (!astrocs::ValidateEventV1(ev, seq_)) {
            std::fprintf(stderr, "acsd: protocol: event dropped (kind=%s seq=%llu)\n",
                         kind.c_str(), static_cast<unsigned long long>(seq_));
            ++seq_;  // 保持 sequence 单调性不变(violation 仍占序)
            return false;  // 协议拒发不是 I/O 写失败: 不置 write_failed_
        }
        // 机器通道(stdout): 单行 ≤ 4096 字节。fputs/fflush 返回值都查(P-163)。
        std::string io_err;
        int io_errno = 0;
        const bool machine_ok = write_all(stdout, fit_event_line(ev), "stdout", &io_err, &io_errno);
        // 人可读通道(stderr): 与上面同一事件对象同源生成（同样查返回值）。
        std::string human_err;
        int human_errno = 0;
        const bool human_ok = write_all(stderr, human_summary_of(ev), "stderr", &human_err, &human_errno);
        if (!machine_ok) {
            last_write_error_ = io_err;
            if (!write_failed_) {   // 首个失败 = 根因; 后续失败多为同一根因的后果
                write_failed_ = true;
                write_exit_code_ = exit_code_for_write_errno(io_errno);
            }
            report_write_failure("stdout", io_err, write_exit_code_, seq_);
        }
        if (!human_ok) {
            human_channel_failed_ = true;
            last_write_error_ = human_err;
            report_write_failure("stderr", human_err, exit_code_for_write_errno(human_errno), seq_);
        }
        ++seq_;
        return machine_ok && human_ok;
    }

    // §4 progress 事件扩展字段冻结: {completed,total,unit,rate,eta_seconds}。
    // rate=null 表示不可用(尚无完成样本); eta_seconds=null 表示不可估计。
    void emit_progress(uint64_t completed, uint64_t total, const std::string& unit,
                       const double* rate, const double* eta_seconds) {
        nlohmann::json extra = {
            {"completed", completed},
            {"total", total},
            {"unit", unit},
            {"rate", rate == nullptr ? nlohmann::json(nullptr) : nlohmann::json(*rate)},
            {"eta_seconds",
             eta_seconds == nullptr ? nlohmann::json(nullptr) : nlohmann::json(*eta_seconds)},
        };
        emit("progress", "info", "progress", "progress update", extra);
    }

    void stage(const std::string& name, bool start) {
        emit(start ? "stage_start" : "stage_end", "info", name,
             start ? "stage started" : "stage finished");
    }

    // 本次 run 写出的 manifest 路径（write_run_manifest 落盘后登记）。
    // SMOKE-001 D8: 协议 §4 的 final.run_manifest 必须回填本次 manifest；此前所有
    // 调用点都传 nullptr ⇒ 恒 null。登记-回填在本类内完成，调用点无需各自传参。
    void set_run_manifest(const std::string& path) { run_manifest_path_ = path; }

    // final 事件: {exit_code,status,run_manifest,summary}
    // run_manifest == nullptr → 用已登记的 manifest 路径；未登记则 null（不伪造）。
    void emit_final(int exit_code, const std::string& status, const char* run_manifest,
                    const std::string& summary) {
        const std::string path = (run_manifest != nullptr) ? std::string(run_manifest)
                                                           : run_manifest_path_;
        nlohmann::json extra = {
            {"exit_code", exit_code},
            {"status", status},
            {"run_manifest", path.empty() ? nlohmann::json(nullptr) : nlohmann::json(path)},
            {"summary", summary},
        };
        emit("final", exit_code == OK ? "info" : "error", "n/a", summary, extra);
    }

    // ── P-163: 写失败可查询状态（只增不减；emitter 生命周期内）──
    // 机器通道（stdout JSONL = 事件流合同面）写失败过 ⇒ write_failed() == true。
    bool write_failed() const { return write_failed_; }
    // 人可读通道（stderr 摘要）写失败过。**只登记**，不驱动退出码：诊断通道缺失
    // 不得把机器合同已成功的 run 判成 IO(7)（见 publication_exit_code 注释）。
    bool human_channel_failed() const { return human_channel_failed_; }
    // 首个机器通道失败的退出码：ENOSPC（磁盘满）→ 10(RESOURCE)，其它写失败 → 7(IO)。
    int write_exit_code() const { return write_exit_code_; }
    const std::string& last_write_error() const { return last_write_error_; }

    // 发布面退出码（P-163）：事件流是本次运行的默认输出通道（docs/ASTROCS_DESIGN §6.3），
    // 因此「成功」必须包含事件已写出 —— 名义 rc == OK 而事件流写失败时改报写失败码
    // （IO=7；磁盘满 ENOSPC → 10，docs/plugins/infrastructure/21_observability.md:45-46）。
    // 名义 rc 已非 0 时保留原码：run 自身的失败更具体，不被写失败掩盖（且已非 0）。
    int publication_exit_code(int nominal_rc) const {
        if (!write_failed_ || nominal_rc != astrocs::OK) return nominal_rc;
        return write_exit_code_;
    }

private:
    // 单通道写：fputs 与 fflush 的返回值都查（P-163 核心）。失败时填 *out_err/*out_errno。
    static bool write_all(std::FILE* stream, const std::string& text, const char* channel,
                          std::string* out_err, int* out_errno) {
        std::clearerr(stream);
        errno = 0;
        if (std::fputs(text.c_str(), stream) == EOF) {
            const int e = errno;
            if (out_errno) *out_errno = e;
            if (out_err) *out_err = std::string(channel) + ":fputs:errno=" + std::to_string(e) +
                                    ":" + std::strerror(e);
            return false;
        }
        errno = 0;
        if (std::fflush(stream) != 0) {
            const int e = errno;
            if (out_errno) *out_errno = e;
            if (out_err) *out_err = std::string(channel) + ":fflush:errno=" + std::to_string(e) +
                                    ":" + std::strerror(e);
            return false;
        }
        return true;
    }

    // 写失败 → 退出码：磁盘满 = RESOURCE(10)，其它 I/O 写失败 = IO(7)。
    // 码值唯一源 = exit_codes.h；判据语义 = 21_observability.md:45-46。
    static int exit_code_for_write_errno(int e) {
        return e == ENOSPC ? astrocs::RESOURCE : astrocs::IO;
    }

    // 结构化诊断行（stderr；机器可解析的 key=value 前缀）。只走 stderr —— stdout
    // 必须保持纯 JSONL。stderr 自身坏掉时本行写失败即可忽略（状态已可查询）。
    static void report_write_failure(const char* channel, const std::string& err, int exit_code,
                                     unsigned long long seq) {
        std::fprintf(stderr,
                     "acsd: io_error symbol=JsonlEmitter::emit channel=%s exit_code=%d seq=%llu"
                     " detail=%s\n",
                     channel, exit_code, seq, err.c_str());
        std::fflush(stderr);
    }

    std::string run_id_;
    std::string phase_;
    std::string run_manifest_path_;
    unsigned long long seq_ = 0;
    bool write_failed_ = false;
    bool human_channel_failed_ = false;
    int write_exit_code_ = astrocs::IO;
    std::string last_write_error_;
};

}  // namespace astrocs
