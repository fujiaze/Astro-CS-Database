// CPU-005 单元测试: 损坏 provider/ABI mismatch/缺 kernel/优雅 fallback
#include "backend_loader.h"
#include "cpu_features.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

using astrocs::backend_host::LoadResult;
using astrocs::backend_host::ManifestEntry;
using astrocs::backend_host::parse_backends_manifest;
using astrocs::backend_host::preflight_entry;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static std::string tmp_dir() {
  const char* d = std::getenv("TMPDIR");
  return d ? d : "/tmp";
}

static void write_file(const std::string& p, const std::string& content) {
  std::ofstream f(p, std::ios::binary);
  f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

int main() {
  std::string dir = tmp_dir() + "/astrocs_cpu005";
  system(("mkdir -p " + dir).c_str());

  uint64_t detected = astrocs_cpu_detect_features_v1();

  // 1) 损坏 provider: sha256 不匹配 → FALLBACK_BASELINE (安全回退)
  {
    std::string file = dir + "/corrupt.so";
    write_file(file, "not a real backend binary but present");
    ManifestEntry e;
    e.file = "corrupt.so";
    e.backend_id = "fake";
    e.sha256 = std::string(64, '0');  // 伪造 hash, 与实际不符
    e.abi_version = ACS_ABI_VERSION_V1;
    std::string reason;
    LoadResult r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision == LoadResult::FALLBACK_BASELINE);
    CHECK(!r.reason.empty());
  }

  // 2) ABI mismatch: abi_version 错误 → 拒 (不猜布局)
  {
    ManifestEntry e;
    e.file = "corrupt.so";
    e.backend_id = "fake";
    e.sha256 = std::string(64, '0');
    e.abi_version = 99;  // 非法 ABI
    std::string reason;
    LoadResult r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision != LoadResult::OK);
  }

  // 3) 缺 kernel/file: 文件不存在 → FALLBACK_BASELINE
  {
    ManifestEntry e;
    e.file = "missing_backend.so";
    e.backend_id = "ghost";
    e.sha256 = std::string(64, '0');
    e.abi_version = ACS_ABI_VERSION_V1;
    std::string reason;
    LoadResult r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision == LoadResult::FALLBACK_BASELINE);
  }

  // 4) 路径注入: 非裸文件名 → REJECT_SECURITY
  {
    ManifestEntry e;
    e.file = "../../etc/passwd";
    e.backend_id = "evil";
    e.sha256 = std::string(64, '0');
    e.abi_version = ACS_ABI_VERSION_V1;
    std::string reason;
    LoadResult r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision == LoadResult::REJECT_SECURITY);
    // 盘符
    e.file = "C:\\windows\\system32\\x.dll";
    r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision == LoadResult::REJECT_SECURITY);
  }

  // 5) 伪造 manifest: 结构非法 → err 非空, 不猜
  {
    std::vector<ManifestEntry> out;
    std::string err;
    CHECK(!parse_backends_manifest("{\"kind\":\"wrong\"}", &out, &err));
    CHECK(!err.empty());
    // 合法 manifest 但条目缺 sha256
    CHECK(!parse_backends_manifest(
        R"({"schema_version":"1","kind":"astrocs_backends_manifest","backends":[
             {"file":"a.so","backend_id":"b","sha256":"short","abi_version":1}]})",
        &out, &err));
  }

  // 6) 优雅 fallback 语义: baseline required=0 恒可加载 (05 §6 计算 stage 不静默换)
  {
    ManifestEntry e;
    e.file = "baseline.so";
    e.backend_id = "baseline";
    e.sha256 = std::string(64, '0');
    e.abi_version = ACS_ABI_VERSION_V1;
    e.required_features = 0;
    // baseline 文件不存在时应 FALLBACK(说明链完整); 存在则 OK —— 语义: 永不 REJECT
    std::string reason;
    LoadResult r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision != LoadResult::REJECT_SECURITY);
  }

  // 7) required 超集: 伪造 required → 不满足 → FALLBACK_BASELINE (损坏 provider 降级)
  {
    ManifestEntry e;
    e.file = "corrupt.so";
    e.backend_id = "fake";
    e.sha256 = std::string(64, '0');
    e.abi_version = ACS_ABI_VERSION_V1;
    e.required_features = 1ull << 42;  // 不存在
    std::string reason;
    LoadResult r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision == LoadResult::FALLBACK_BASELINE);
  }

  if (failures == 0) {
    std::printf("CPU-005 TESTS PASS (损坏/ABI mismatch/缺文件/路径注入/伪造 manifest 全拒, 优雅回退)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-005 TESTS FAIL (%d)\n", failures);
  return 1;
}
