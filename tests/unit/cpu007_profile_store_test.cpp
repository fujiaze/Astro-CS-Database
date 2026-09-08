// tests/unit/cpu007_profile_store_test.cpp — CPU-007 (V7.1) profile 存储生命周期单测
// 覆盖: 默认路径(Windows LOCALAPPDATA / Linux XDG data, 编译面同源); 原子保存
// (写临时→校验→rename, 临时文件零残留); 装载校验链(verify_profile_v2 + 身份绑定);
// 负向样例: 损坏/半写、旧版本、机器变化、build 失配、无 profile、源码树拒存、
// 外域 schema(benchmark-report/v1)拒存 —— 全部"不被使用"。
// 全部 IO 在系统临时目录, 不触碰源码树(与规格验收一致)。
#include "profile_store.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

using astrocs::backend_host::PathResult;
using astrocs::backend_host::SaveResult;
using astrocs::backend_host::LoadResult;
using astrocs::backend_host::default_profile_path_v1;
using astrocs::backend_host::save_profile_atomic_v1;
using astrocs::backend_host::load_profile_checked_v1;
using astrocs::backend_host::classify_profile_rejection_v1;

static int failures = 0;
#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

namespace {

const char* kCommit = "0123456789abcdef0123456789abcdef01234567";
const char* kCommit2 = "fedcba9876543210fedcba9876543210fedcba98";

std::string hex64(char c) { return std::string(64, c); }

// 最小合法 astrocs.cpu-profile/v2 文本(与 profile_gen_v2 产出结构一致, 过 verify)
std::string make_v2_profile(const char* vendor = "GenuineTest",
                            const char* kernel_verdict = "oracle:pass") {
    nlohmann::json j;
    j["schema"] = "astrocs.cpu-profile/v2";
    j["profile_id"] = "sha256:" + hex64('a');
    j["created_utc"] = "2026-09-08T00:00:00Z";
    j["host"] = {
        {"arch", "amd64"}, {"vendor", vendor},
        {"family", 6}, {"model", 1}, {"stepping", 0},
        {"os_abi", "linux-test"}, {"features", nlohmann::json::array({"sse2"})},
        {"xcr0", "7"}, {"logical_available", 4}, {"quota_signature", "qs-test"},
    };
    j["build"] = {
        {"astrocs_version", "0.0.0"},
        {"source_commit", kCommit},
        {"benchmark_binary_sha256", hex64('b')},
        {"runtime_build_id", "0.0.0+g0123456789ab"},
        {"provider_build_ids", nlohmann::json::object()},
    };
    j["memory_bandwidth"] = {{"copy", 1.0}, {"read", 1.0}, {"write", 1.0}, {"triad", 1.0}};
    j["raw_samples_sha256"] = hex64('c');
    j["kernels"] = nlohmann::json::object({
        {"calibration-pixel-transform", {
            {"workload_class", "compute"}, {"provider", "baseline"},
            {"workers", 2}, {"block", 1},
            {"correctness_test", kernel_verdict}, {"self_test_sha256", hex64('d')},
            {"median", 100.0}, {"mad", 1.0}, {"fallback_reason", nullptr},
        }},
    });
    return j.dump(2) + "\n";
}

// 当前机器硬件画像(与 cpu_routing check_profile_identity_v1 消费字段对齐)
std::string make_hw_json(const char* vendor = "GenuineTest") {
    nlohmann::json j;
    j["vendor"] = vendor;
    j["family"] = 6;
    j["model"] = 1;
    j["stepping"] = 0;
    j["os"] = {{"name", "linux-test"}};
    j["feature_names"] = nlohmann::json::array({"sse2", "avx2"});
    j["cli_sha256"] = hex64('b');
    return j.dump(2);
}

std::string temp_dir() {
    const char* t = std::getenv("TMPDIR");
    std::string base = (t && *t) ? t : "/tmp";
    static unsigned seq = 0;
    const std::string d = base + "/astrocs_cpu007_test_" +
                          std::to_string(static_cast<unsigned long>(::getpid())) + "_" +
                          std::to_string(seq++);
    fs::create_directories(d);
    return d;
}

std::string read_file(const std::string& p, bool* ok) {
    std::ifstream f(p, std::ios::binary);
    if (!f) { if (ok) *ok = false; return {}; }
    std::ostringstream ss;
    ss << f.rdbuf();
    if (ok) *ok = true;
    return ss.str();
}

}  // namespace

int main() {
  // ── 1) 默认路径: XDG_DATA_HOME 注入与回退; 绝对路径; 用户数据目录语义 ──
  {
    const std::string td = temp_dir();
    ::setenv("XDG_DATA_HOME", td.c_str(), 1);
    const PathResult r = default_profile_path_v1();
    CHECK(r.ok);
    CHECK(r.path == td + "/AstroCS/cpu_profile.json");
    CHECK(!r.path.empty() && r.path[0] == '/');
    ::unsetenv("XDG_DATA_HOME");
    ::setenv("HOME", td.c_str(), 1);
    const PathResult r2 = default_profile_path_v1();
    CHECK(r2.ok);
    CHECK(r2.path == td + "/.local/share/AstroCS/cpu_profile.json");
    CHECK(fs::path(r2.path).is_absolute());
    ::setenv("XDG_DATA_HOME", td.c_str(), 1);   // 还原, 后续用例不受影响
  }

  // ── 2) 正向: 原子保存 → 装载 ok(文本一致) ──
  std::string dir, target;
  {
    dir = temp_dir();
    target = dir + "/AstroCS/cpu_profile.json";   // 不存在的父目录 → 自动创建
    const std::string prof = make_v2_profile();
    const SaveResult s = save_profile_atomic_v1(prof, make_hw_json(), kCommit, target);
    CHECK(s.ok);
    CHECK(s.path == target);
    bool ok = false;
    const std::string back = read_file(target, &ok);
    CHECK(ok && back == prof);
    const LoadResult l = load_profile_checked_v1(target, make_hw_json(), kCommit, {});
    CHECK(l.valid && l.status == "ok");
    CHECK(l.json_text == prof);
    CHECK(l.reason.empty() && l.rejected_path.empty());
    // 原子性: 无 .tmp- 残留
    int tmp_left = 0;
    for (const auto& e : fs::directory_iterator(dir)) {
      const std::string n = e.path().filename().string();
      if (n.find(".tmp-") != std::string::npos) ++tmp_left;
    }
    CHECK(tmp_left == 0);
  }

  // ── 3) 负向: 半写文件(合法文本截断)不被使用 + 隔离改名 ──
  {
    const std::string prof = make_v2_profile();
    const std::string half = prof.substr(0, prof.size() / 2);   // 半写
    bool ok = false;
    const std::string t = temp_dir() + "/cpu_profile.json";
    { std::ofstream f(t, std::ios::binary); f << half; }
    const LoadResult l = load_profile_checked_v1(t, make_hw_json(), kCommit, {});
    CHECK(!l.valid);
    CHECK(l.reason.rfind("corrupted", 0) == 0);
    CHECK(!l.rejected_path.empty() && fs::exists(l.rejected_path));
    CHECK(!fs::exists(t));   // 原位置已隔离改名
    // 隔离文件内容 = 半写原文(取证不删除)
    const std::string quar = read_file(l.rejected_path, &ok);
    CHECK(ok && quar == half);
  }

  // ── 4) 负向: 损坏文件(随机字节)不被使用 ──
  {
    const std::string t = temp_dir() + "/cpu_profile.json";
    { std::ofstream f(t, std::ios::binary); f << "\x01 not json {{{ garbage"; }
    const LoadResult l = load_profile_checked_v1(t, make_hw_json(), kCommit, {});
    CHECK(!l.valid);
    CHECK(l.reason.rfind("corrupted", 0) == 0);
    CHECK(!l.rejected_path.empty());
  }

  // ── 5) 负向: 旧版本(v1 schema_version 布局)不被使用 ──
  {
    const std::string t = temp_dir() + "/cpu_profile.json";
    { std::ofstream f(t, std::ios::binary);
      f << "{\"schema_version\": 1, \"mode\": \"full\", \"kernels\": []}"; }
    const LoadResult l = load_profile_checked_v1(t, make_hw_json(), kCommit, {});
    CHECK(!l.valid);
    CHECK(l.reason.rfind("old_schema", 0) == 0);
    CHECK(!l.rejected_path.empty());   // 旧版本隔离(同 verify 语义)
  }

  // ── 6) 负向: 机器变化(CPU 指纹)不被使用; 文件保留(回滚可复核) ──
  {
    const std::string t = temp_dir() + "/cpu_profile.json";
    CHECK(save_profile_atomic_v1(make_v2_profile(), make_hw_json(), kCommit, t).ok);
    const LoadResult l = load_profile_checked_v1(t, make_hw_json("OtherVendor"),
                                                 kCommit, {});
    CHECK(!l.valid);
    CHECK(l.reason.rfind("stale_machine", 0) == 0);
    CHECK(fs::exists(t));   // 非损坏 → 不隔离
    CHECK(l.rejected_path.empty());
  }

  // ── 7) 负向: build 失配(换 commit)不被使用; 文件保留 ──
  {
    const std::string t = temp_dir() + "/cpu_profile.json";
    CHECK(save_profile_atomic_v1(make_v2_profile(), make_hw_json(), kCommit, t).ok);
    const LoadResult l = load_profile_checked_v1(t, make_hw_json(), kCommit2, {});
    CHECK(!l.valid);
    CHECK(l.reason.rfind("stale_build", 0) == 0);
    CHECK(fs::exists(t));
  }

  // ── 8) 负向: 无 profile → missing + 清晰 warning(消费方按 V8-CPU-002 回落) ──
  {
    const std::string t = temp_dir() + "/AstroCS/cpu_profile.json";
    const LoadResult l = load_profile_checked_v1(t, make_hw_json(), kCommit, {});
    CHECK(!l.valid);
    CHECK(l.status == "missing");
    CHECK(l.warning_text.find("generic ISA") != std::string::npos);
    CHECK(l.warning_text.find("dynamic worker") != std::string::npos);
    CHECK(l.json_text.empty());
  }

  // ── 9) 负向: 源码树拒存(profile 不进源码/审核包) ──
  {
    // 相对路径解析到 cwd(源码树内) → 拒绝
    const SaveResult s = save_profile_atomic_v1(make_v2_profile(), make_hw_json(),
                                                kCommit, "cpu_profile.json");
    CHECK(!s.ok);
    CHECK(s.reason.find("source tree") != std::string::npos);
    CHECK(!fs::exists("cpu_profile.json"));
    // 源码树内绝对路径 → 拒绝
    const SaveResult s2 = save_profile_atomic_v1(make_v2_profile(), make_hw_json(),
                                                 kCommit, "./build/leak_profile.json");
    CHECK(!s2.ok);
    CHECK(!fs::exists("./build/leak_profile.json"));
  }

  // ── 10) 原子性: 校验失败的保存零副作用(旧完整版保持可装载) ──
  {
    const std::string t = temp_dir() + "/cpu_profile.json";
    CHECK(save_profile_atomic_v1(make_v2_profile(), make_hw_json(), kCommit, t).ok);
    // 坏文本: verify 拒绝(缺 build) → 目标不被覆盖
    const SaveResult bad = save_profile_atomic_v1("{\"schema\":\"astrocs.cpu-profile/v2\"}",
                                                  make_hw_json(), kCommit, t);
    CHECK(!bad.ok);
    CHECK(bad.reason.find("verify_profile_v2") != std::string::npos);
    const LoadResult l = load_profile_checked_v1(t, make_hw_json(), kCommit, {});
    CHECK(l.valid);   // 旧版原样可用
    // 外域 schema(CPU-006 benchmark-report/v1) → 同一防线拒绝(结构性隔离不破坏)
    const SaveResult foreign = save_profile_atomic_v1(
        "{\"schema\":\"astrocs.benchmark-report/v1\",\"suite\":\"quick\"}",
        make_hw_json(), kCommit, t);
    CHECK(!foreign.ok);
    CHECK(foreign.reason.find("schema") != std::string::npos);
    CHECK(load_profile_checked_v1(t, make_hw_json(), kCommit, {}).valid);
  }

  // ── 11) 原子性: 孤儿临时文件被清理且不影响装载 ──
  {
    const std::string t = temp_dir() + "/cpu_profile.json";
    CHECK(save_profile_atomic_v1(make_v2_profile(), make_hw_json(), kCommit, t).ok);
    const std::string orphan = t + ".tmp-999999-deadbeefdeadbeef";
    { std::ofstream f(orphan, std::ios::binary); f << "half-written garbage"; }
    const SaveResult s = save_profile_atomic_v1(make_v2_profile(), make_hw_json(),
                                                kCommit, t);
    CHECK(s.ok);
    CHECK(!fs::exists(orphan));   // 下次写前清理
    CHECK(load_profile_checked_v1(t, make_hw_json(), kCommit, {}).valid);
  }

  // ── 12) 消费级校验: oracle:fail kernel 不被使用(CPU-005 唯一出处复用) ──
  {
    const std::string t = temp_dir() + "/cpu_profile.json";
    const std::string bad = make_v2_profile("GenuineTest", "oracle:fail");
    CHECK(save_profile_atomic_v1(bad, make_hw_json(), kCommit, t).ok);
    const LoadResult l = load_profile_checked_v1(
        t, make_hw_json(), kCommit, {"calibration-pixel-transform"});
    CHECK(!l.valid);
    CHECK(l.reason.rfind("consumer_invalid", 0) == 0);
    // 同一文件不带消费校验 → ok(分层语义: 消费面由调用方选择)
    CHECK(load_profile_checked_v1(t, make_hw_json(), kCommit, {}).valid);
  }

  // ── 13) 失效归类词表(负向样例与 load 共用同一实现) ──
  {
    CHECK(classify_profile_rejection_v1("malformed JSON: x", "") == "corrupted");
    CHECK(classify_profile_rejection_v1("schema != astrocs.cpu-profile/v2", "")
          == "old_schema");
    CHECK(classify_profile_rejection_v1(
              "build.source_commit != expected (0123)", "") == "stale_build");
    CHECK(classify_profile_rejection_v1("", "host.vendor changed") == "stale_machine");
    CHECK(classify_profile_rejection_v1("", "host.xcr0 changed (OS 不再保存 AVX/AVX-512 状态)")
          == "stale_machine");
    CHECK(classify_profile_rejection_v1("", "build.source_commit mismatch")
          == "stale_build");
    CHECK(classify_profile_rejection_v1("", "profile/hw malformed JSON") == "corrupted");
    CHECK(classify_profile_rejection_v1("", "") == "corrupted");   // 未知保守归损坏
  }

  // ── 14) 覆盖更新: 新版本原子替换旧版本, 内容生效 ──
  {
    const std::string t = temp_dir() + "/cpu_profile.json";
    const std::string v1p = make_v2_profile();
    CHECK(save_profile_atomic_v1(v1p, make_hw_json(), kCommit, t).ok);
    nlohmann::json j = nlohmann::json::parse(v1p);
    j["kernels"]["calibration-pixel-transform"]["median"] = 90.0;
    const std::string v2p = j.dump(2) + "\n";
    CHECK(save_profile_atomic_v1(v2p, make_hw_json(), kCommit, t).ok);
    const LoadResult l = load_profile_checked_v1(t, make_hw_json(), kCommit, {});
    CHECK(l.valid && l.json_text == v2p);
    CHECK(nlohmann::json::parse(l.json_text)
              ["kernels"]["calibration-pixel-transform"]["median"].get<double>() == 90.0);
  }

  if (failures == 0) {
    std::printf("cpu007_profile_store_test: ALL PASS\n");
    return 0;
  }
  std::printf("cpu007_profile_store_test: %d FAILURE(S)\n", failures);
  return 1;
}
