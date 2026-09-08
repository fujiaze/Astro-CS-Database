// lib/backend_host/profile_store.cpp — CPU-007 profile 存储生命周期实现
// 层次合同见 profile_store.h。校验链唯一复用: verify_profile_v2(文本级) +
// check_profile_identity_v1(身份级) + profile_kernel_benchmark_valid(消费级);
// 本文件只做路径合成/原子 IO/失效隔离, 不含任何 profile 业务语义。
#include "profile_store.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cpu_routing.h"   // check_profile_identity_v1 / profile_kernel_benchmark_valid
#include "profile_gen.h"   // verify_profile_v2

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace astrocs::backend_host {

namespace {

using nlohmann::json;

std::string utc_now_compact() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_MSC_VER)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char ts[40];
    std::snprintf(ts, sizeof(ts), "%04d%02d%02dT%02d%02d%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return ts;
}

std::string rand_suffix() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(gen()));
    return buf;
}

// 平台目录分隔统一 '/'
std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/' || a.back() == '\\') return a + b;
    return a + "/" + b;
}

long process_pid() {
#if defined(_WIN32)
    return static_cast<long>(::_getpid());
#else
    return static_cast<long>(::getpid());
#endif
}

#if !defined(_WIN32)
bool fsync_path_fd(int fd) {
    return ::fsync(fd) == 0;
}
#endif

// 写全文 + fsync + close; 成功 true。失败清理半成品。
bool write_file_fsync(const std::string& path, const std::string& text) {
#if defined(_WIN32)
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t n = text.size();
    if (n && std::fwrite(text.data(), 1, n, f) != n) { std::fclose(f); return false; }
    if (std::fflush(f) != 0) { std::fclose(f); return false; }
    if (_commit(_fileno(f)) != 0) { std::fclose(f); return false; }
    std::fclose(f);
    return true;
#else
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    const char* p = text.data();
    size_t left = text.size();
    while (left > 0) {
        const ssize_t w = ::write(fd, p, left);
        if (w <= 0) { ::close(fd); return false; }
        p += w; left -= static_cast<size_t>(w);
    }
    if (!fsync_path_fd(fd)) { ::close(fd); return false; }
    ::close(fd);
    return true;
#endif
}

// rename 原子替换(Windows 走 MoveFileEx 覆盖语义)
bool atomic_rename(const std::string& from, const std::string& to) {
    std::error_code ec;
    fs::rename(from, to, ec);
    if (!ec) return true;
#if defined(_WIN32)
    // 跨 fs::rename 实现差异的兜底: 目标存在时 MoveFileEx 覆盖。
    if (!::MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING))
        return false;
    return true;
#else
    return false;
#endif
}

void fsync_directory(const std::string& dir) {
#if !defined(_WIN32)
    const int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd >= 0) { ::fsync(fd); ::close(fd); }
#else
    (void)dir;
#endif
}

std::string read_all(const std::string& path, bool* ok) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { if (ok) *ok = false; return {}; }
    std::ostringstream ss;
    ss << f.rdbuf();
    if (ok) *ok = true;
    return ss.str();
}

// 源码树防线: profile 原始数据不得进入源码/审核包。
// 判定: target 相对当前源码树 cwd 或路径含 CMakeLists.txt 的目录 → 拒绝。
// 实用实现: 若 target_path 是相对路径, 或其绝对化路径的任一祖先含 CMakeLists.txt,
// 判为源码树内。审核包=源码树子集, 同一防线覆盖。
bool looks_like_source_tree(const std::string& target_path) {
    std::error_code ec;
    fs::path abs = fs::absolute(fs::path(target_path), ec);
    if (ec) return true;   // 无法判定 → 保守拒绝
    fs::path p = abs;
    while (true) {
        std::error_code e2;
        if (fs::exists(p / "CMakeLists.txt", e2)) return true;
        if (fs::exists(p / ".git", e2)) return true;
        fs::path parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    return false;
}

// v2 schema 之外的旧版本/外域 schema 检出(在 verify_profile_v2 之前给出精确归类)。
// v1 旧 profile 特征: 顶层 "schema_version"(数值) 或 schema 字符串非 cpu-profile/v2。
bool is_old_or_foreign_schema(const std::string& text) {
    json d;
    try {
        d = json::parse(text);
    } catch (...) {
        return false;   // 解析失败走 corrupted(半写/损坏)
    }
    if (!d.is_object()) return true;
    if (d.contains("schema_version")) return true;   // v1 旧布局
    const std::string s = d.value("schema", std::string());
    if (s != "astrocs.cpu-profile/v2") return true;
    return false;
}

}  // namespace

PathResult default_profile_path_v1() {
    PathResult r;
#if defined(_WIN32)
    const char* la = std::getenv("LOCALAPPDATA");
    std::string base = (la && *la) ? std::string(la) : std::string();
    if (base.empty()) {
        const char* up = std::getenv("USERPROFILE");
        if (up && *up) base = join_path(up, "AppData") + "/Local";
    }
    if (base.empty()) {
        r.reason = "no LOCALAPPDATA/USERPROFILE";
        return r;
    }
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    std::string base = (xdg && *xdg && *xdg == '/') ? std::string(xdg) : std::string();
    if (base.empty()) {
        const char* home = std::getenv("HOME");
        if (!home || !*home) { r.reason = "no XDG_DATA_HOME/HOME"; return r; }
        base = join_path(home, ".local/share");
    }
#endif
    r.path = join_path(join_path(base, "AstroCS"), "cpu_profile.json");
    r.ok = true;
    return r;
}

SaveResult save_profile_atomic_v1(const std::string& json_text,
                                  const std::string& hw_json,
                                  const std::string& current_commit,
                                  const std::string& target_path) {
    SaveResult r;
    r.path = target_path;

    // 0) 源码树防线: profile 原始数据不进源码/审核包(CPU-007 验收)。
    if (looks_like_source_tree(target_path)) {
        r.reason = "refuse to store profile inside source tree (audit-pack containment)";
        return r;
    }

    // 1) 文本级校验(唯一出处 verify_profile_v2; 外域 schema 如 benchmark-report/v1
    //    在此被拒 → 与 CPU-006 结构性隔离)。
    {
        const std::string err = verify_profile_v2(json_text, current_commit);
        if (!err.empty()) {
            r.reason = "verify_profile_v2: " + err;
            return r;
        }
    }
    // 2) 身份级校验(唯一出处 check_profile_identity_v1; hw/commit 空串则该面跳过)。
    //    check 的 commit 空语义是"必须失败", 故空时传 profile 自身 sc(等价跳过该面)。
    {
        std::string commit_for_check = current_commit;
        if (commit_for_check.empty()) {
            try {
                const json d = json::parse(json_text);
                commit_for_check =
                    d.at("build").at("source_commit").get<std::string>();
            } catch (...) {
                r.reason = "profile missing build.source_commit";
                return r;
            }
        }
        if (!hw_json.empty() || !current_commit.empty()) {
            const IdentityCheck ic =
                check_profile_identity_v1(json_text, hw_json, commit_for_check);
            if (!ic.valid) {
                r.reason = "identity: " + ic.reason;
                return r;
            }
        }
    }

    // 3) 目录准备(parent 逐级创建)。
    const fs::path target(target_path);
    std::error_code ec;
    fs::path parent = target.parent_path();
    if (parent.empty()) parent = fs::path(".");
    fs::create_directories(parent, ec);
    if (ec && !fs::is_directory(parent)) {
        r.reason = "create_directories: " + ec.message();
        return r;
    }
    const std::string dir = parent.string();

    // 4) 清理上次崩溃残留的孤儿临时文件(目标文件零接触)。
    for (const auto& e : fs::directory_iterator(parent, ec)) {
        if (ec) break;
        const std::string n = e.path().filename().string();
        if (n.rfind(target.filename().string() + ".tmp-", 0) == 0) {
            std::error_code ec2;
            fs::remove(e.path(), ec2);
        }
    }

    // 5) 写临时 → fsync → close(同目录同 filesystem, rename 原子性前提)。
    const std::string tmp_path =
        target_path + ".tmp-" + std::to_string(process_pid()) + "-" + rand_suffix();
    if (!write_file_fsync(tmp_path, json_text)) {
        r.reason = "write temp failed: " + tmp_path;
        std::error_code ec3;
        fs::remove(tmp_path, ec3);
        return r;
    }

    // 6) 校验临时文件本体: 读回 → 全量校验(防"内存对、盘上错")。
    {
        bool ok = false;
        const std::string roundtrip = read_all(tmp_path, &ok);
        if (!ok || roundtrip != json_text) {
            r.reason = "temp roundtrip mismatch";
            std::error_code ec4;
            fs::remove(tmp_path, ec4);
            return r;
        }
        const std::string err = verify_profile_v2(roundtrip, current_commit);
        if (!err.empty()) {
            r.reason = "temp verify_profile_v2: " + err;
            std::error_code ec5;
            fs::remove(tmp_path, ec5);
            return r;
        }
    }

    // 7) 原子替换(此刻起目标要么旧完整要么新完整)。
    if (!atomic_rename(tmp_path, target_path)) {
        r.reason = "atomic rename failed";
        std::error_code ec6;
        fs::remove(tmp_path, ec6);
        return r;
    }
    fsync_directory(dir);
    r.ok = true;
    return r;
}

LoadResult load_profile_checked_v1(const std::string& target_path,
                                   const std::string& hw_json,
                                   const std::string& current_commit,
                                   const std::vector<std::string>& check_consumer) {
    LoadResult out;
    std::error_code ec;
    if (!fs::exists(fs::path(target_path), ec) || ec) {
        out.status = "missing";
        out.warning_text =
            "no CPU profile at '" + target_path +
            "': running with generic ISA (baseline provider); dynamic worker budget "
            "still applies. Run 'astrocs benchmark cpu' to generate one.";
        return out;
    }
    bool ok = false;
    const std::string text = read_all(target_path, &ok);
    if (!ok) {
        out.status = "rejected";
        out.reason = "unreadable profile file";
        return out;
    }
    if (text.empty()) {   // 零字节=半写残留的极端面
        out.status = "rejected";
        out.reason = "corrupted: empty profile file";
        out.rejected_path = target_path + ".rejected-" + utc_now_compact();
        std::error_code ec2;
        fs::rename(target_path, out.rejected_path, ec2);
        return out;
    }

    // 文本级(旧版本/外域 schema 先归类, 再走唯一出处 verify)
    if (is_old_or_foreign_schema(text)) {
        out.status = "rejected";
        out.reason = "old_schema: schema != astrocs.cpu-profile/v2 (old version or foreign document)";
        out.rejected_path = target_path + ".rejected-" + utc_now_compact();
        std::error_code ec3;
        fs::rename(target_path, out.rejected_path, ec3);
        return out;
    }
    {
        std::string commit_for_verify = current_commit;
        if (commit_for_verify.empty()) {
            try {
                const json d = json::parse(text);
                commit_for_verify =
                    d.at("build").at("source_commit").get<std::string>();
            } catch (const json::parse_error&) {
                commit_for_verify = "";
            }
        }
        const std::string err = verify_profile_v2(text, commit_for_verify);
        if (!err.empty()) {
            const std::string cls = classify_profile_rejection_v1(err, "");
            out.status = "rejected";
            out.reason = cls + ": " + err;
            if (cls == "corrupted" || cls == "old_schema") {
                // 文件本体失效 → 隔离改名(不删除, 供审计取证)
                out.rejected_path = target_path + ".rejected-" + utc_now_compact();
                std::error_code ec4;
                fs::rename(target_path, out.rejected_path, ec4);
            }
            // stale_build → 文件保留(版本回滚/切换的合法现场, 不算损坏)
            return out;
        }
    }
    // 身份级(build/CPU/OS/provider hash 绑定; hw/commit 空则该面跳过)
    {
        std::string commit_for_check = current_commit;
        if (commit_for_check.empty()) {
            try {
                const json d = json::parse(text);
                commit_for_check =
                    d.at("build").at("source_commit").get<std::string>();
            } catch (...) {
                commit_for_check = "";
            }
        }
        if (!hw_json.empty() || !current_commit.empty()) {
            const IdentityCheck ic =
                check_profile_identity_v1(text, hw_json, commit_for_check);
            if (!ic.valid) {
                const std::string cls = classify_profile_rejection_v1("", ic.reason);
                out.status = "rejected";
                out.reason = cls + ": " + ic.reason;
                // stale(机器/二进制变化)与 stale_build: 文件保留(回滚/同机复核现场);
                // malformed(损坏面): 隔离。
                if (cls == "corrupted") {
                    out.rejected_path = target_path + ".rejected-" + utc_now_compact();
                    std::error_code ec5;
                    fs::rename(target_path, out.rejected_path, ec5);
                }
                return out;
            }
        }
    }
    // 消费级(kernel oracle/median 完整性; CPU-005 唯一出处)
    for (const std::string& kid : check_consumer) {
        std::string kre;
        const bool okc =
            profile_kernel_benchmark_valid(text, kid, &kre, nullptr, nullptr);
        if (!okc) {
            out.status = "rejected";
            out.reason = "consumer_invalid: " + kid + ": " + kre;
            return out;
        }
    }
    out.valid = true;
    out.status = "ok";
    out.json_text = text;
    return out;
}

std::string classify_profile_rejection_v1(const std::string& verify_error,
                                          const std::string& identity_reason) {
    // 身份面: reason 由 check_profile_identity_v1 固定词表产出(cpu_routing.cpp)。
    if (!identity_reason.empty()) {
        if (identity_reason.find("build.source_commit mismatch") != std::string::npos)
            return "stale_build";
        if (identity_reason.find("build.benchmark_binary_sha256") != std::string::npos)
            return "stale_machine";   // 二进制 hash 属机器/环境绑定面
        if (identity_reason.rfind("host.", 0) == 0 ||
            identity_reason.find(" changed") != std::string::npos)
            return "stale_machine";
        if (identity_reason.find("malformed") != std::string::npos)
            return "corrupted";
        if (identity_reason.find("missing host/build") != std::string::npos)
            return "old_schema";
        return "corrupted";   // 未知 → 保守归损坏
    }
    // 文本面: verify_profile_v2 错误词表。
    if (verify_error.rfind("malformed JSON", 0) == 0) return "corrupted";
    if (verify_error.find("schema != astrocs.cpu-profile/v2") != std::string::npos)
        return "old_schema";
    if (verify_error.find("build.source_commit != expected") != std::string::npos)
        return "stale_build";
    if (verify_error.find("not semver") != std::string::npos)
        return "old_schema";   // 版本面失效按旧版本处理
    return "corrupted";
}

}  // namespace astrocs::backend_host
