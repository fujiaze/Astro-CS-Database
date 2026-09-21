// IO-001 单元测试: Artifact 事务 + FileIoAdapter
#include "astrocs/io/io_adapter.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

using namespace astrocs::io;
using astrocs::core::ErrorDomain;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static std::string tmp_dir() {
  // 跨平台临时目录: Linux 用 TMPDIR(/tmp); Windows 无 /proc 也没有 "/tmp",
  // MSVC 会把 "/tmp/x" 解析到当前盘根 (runner 上不存在) → begin 失败 →
  // Result::value() on error 抛 std::logic_error → terminate (0xc0000409)。
  // 回退顺序 TMPDIR → TEMP → TMP → "."(最后手段), 与 Windows runner 环境一致。
  const char* d = std::getenv("TMPDIR");
  if (!d || !*d) d = std::getenv("TEMP");
  if (!d || !*d) d = std::getenv("TMP");
#if defined(_WIN32)
  return (d && *d) ? std::string(d) : std::string(".");
#else
  return (d && *d) ? std::string(d) : std::string("/tmp");
#endif
}

static std::string read_file(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static void test_transaction_commit() {
  std::string target = tmp_dir() + "/astrocs_io_test_target.bin";
  std::remove(target.c_str());
  ArtifactTransaction tx;
  auto b = tx.begin(target);
  CHECK(b.ok());
  CHECK(tx.active());
  tx.write("hello ", 6);
  tx.write("world", 5);
  auto c = tx.commit();
  CHECK(c.ok());
  CHECK(!tx.active());
  CHECK(read_file(target) == "hello world");
  std::remove(target.c_str());
}

static void test_transaction_abort_cleans() {
  std::string target = tmp_dir() + "/astrocs_io_test_abort.bin";
  std::remove(target.c_str());
  ArtifactTransaction tx;
  auto b = tx.begin(target);
  CHECK(b.ok());
  std::string tmp = b.value();
  tx.write("partial", 7);
  tx.abort();
  CHECK(!tx.active());
  // 临时文件已清理; target 未被创建
  std::ifstream t(tmp);
  CHECK(!t.is_open());
  std::ifstream tg(target);
  CHECK(!tg.is_open());
}

static void test_transaction_verify_rejects() {
  // 模拟 length mismatch: 直接篡改临时文件
  std::string target = tmp_dir() + "/astrocs_io_test_verify.bin";
  std::remove(target.c_str());
  ArtifactTransaction tx;
  auto b = tx.begin(target);
  CHECK(b.ok());
  tx.write("abc", 3);
  // 在 commit 前向临时文件追加字节 (绕过 write 计数)
  {
    std::ofstream f(b.value(), std::ios::binary | std::ios::app);
    f.write("X", 1);
  }
  auto c = tx.commit();
  CHECK(c.failed());
  CHECK(c.error().domain() == ErrorDomain::IO);
  // target 未被污染
  std::ifstream tg(target);
  CHECK(!tg.is_open());
}

static void test_commit_without_begin() {
  ArtifactTransaction tx;
  auto c = tx.commit();
  CHECK(c.failed());
}

// B13-R13-1: commit 必须真校验内容 — 长度不变、内容损坏 (如 bitflip/静默截断
// 后补齐) 时 commit 不得假成功 (verify 全量重算 FNV-1a 校验和)
static void test_transaction_checksum_rejects_bitflip() {
  std::string target = tmp_dir() + "/astrocs_io_test_bitflip.bin";
  std::remove(target.c_str());
  ArtifactTransaction tx;
  auto b = tx.begin(target);
  CHECK(b.ok());
  tx.write("hello world", 11);
  // 同长度篡改: 追加后回退一位 — 文件长度仍 11, 但第 0 字节内容被改
  {
    std::ofstream f(b.value(), std::ios::binary | std::ios::in | std::ios::out);
    f.seekp(0);
    f.put('H');  // 'h' -> 'H', 长度不变
  }
  auto c = tx.commit();
  CHECK(c.failed());
  CHECK(c.error().domain() == ErrorDomain::IO);
  // 错误消息必须指向校验和 (而非长度), 证明校验和真的被比较了
  CHECK(c.error().message().find("checksum") != std::string::npos);
  // target 未被污染
  std::ifstream tg(target);
  CHECK(!tg.is_open());
}

// B13-R13-1: write 失败不得静默 — 直接构造: begin 后把 tmp 替换为同名目录,
// 后续 write 的 reopen 必然失败 (锁存 write_failed_), commit 必须失败且
// target 不落盘 (假成功根除)
static void test_transaction_write_failure_not_silent() {
  std::string target = tmp_dir() + "/astrocs_io_test_writefail.bin";
  std::remove(target.c_str());
  ArtifactTransaction tx;
  auto b = tx.begin(target);
  CHECK(b.ok());
  std::string tmp = b.value();
  tx.write("first", 5);   // 正常写
  std::remove(tmp.c_str());     // 摘除 tmp 文件
  CHECK(std::filesystem::create_directory(tmp));  // 同名目录 → reopen 必失败
  tx.write("second", 6);  // reopen 失败 → 锁存, 不静默
  auto c = tx.commit();
  CHECK(c.failed());
  CHECK(c.error().domain() == ErrorDomain::IO);
  CHECK(c.error().message().find("write failed") != std::string::npos);
  CHECK(!tx.active());  // abort 已清理现场
  std::error_code rmec;
  std::filesystem::remove(tmp, rmec);  // 清理测试目录 (abort 只能 remove 文件)
  std::ifstream tg(target);
  CHECK(!tg.is_open());
}

// B13-R13-1: 并发 begin 的 tmp 唯一性 — static seq 原子化后, 多线程 begin
// 不得撞名 (修复前 ++seq 数据竞争可能产生重复 tmp 名互踩)
static void test_concurrent_begin_unique_tmp() {
  constexpr int kThreads = 8;
  constexpr int kPer = 16;
  std::vector<std::string> got(static_cast<size_t>(kThreads * kPer));
  std::vector<std::thread> pool;
  for (int t = 0; t < kThreads; ++t) {
    pool.emplace_back([&got, t] {
      for (int i = 0; i < kPer; ++i) {
        ArtifactTransaction tx;
        auto b = tx.begin(tmp_dir() + "/astrocs_io_test_seq.bin");
        if (b.ok()) got[static_cast<size_t>(t) * kPer + i] = b.value();
      }
    });
  }
  for (auto& th : pool) th.join();
  std::set<std::string> uniq(got.begin(), got.end());
  CHECK(uniq.size() == got.size());
  CHECK(uniq.count(std::string()) == 0);  // 无 begin 失败
}

static void test_file_adapter() {
  FileIoAdapter io;
  std::string p = tmp_dir() + "/astrocs_io_test_adapter.txt";
  std::remove(p.c_str());
  CHECK(!io.exists(p));
  CHECK(io.write_text(p, "line1\nline2").ok());
  CHECK(io.exists(p));
  auto r = io.read_text(p);
  CHECK(r.ok());
  CHECK(r.value() == "line1\nline2");
  std::vector<uint8_t> bytes;
  CHECK(io.read_bytes(p, &bytes).ok());
  CHECK(bytes.size() == 11);
  std::remove(p.c_str());
}

static void test_atomic_write() {
  FileIoAdapter io;
  std::string p = tmp_dir() + "/astrocs_io_test_atomic.json";
  std::remove(p.c_str());
  CHECK(io.atomic_write(p, "{\"ok\":1}").ok());
  CHECK(read_file(p) == "{\"ok\":1}");
  // 覆盖已有文件仍原子
  CHECK(io.atomic_write(p, "{\"ok\":2}").ok());
  CHECK(read_file(p) == "{\"ok\":2}");
  std::remove(p.c_str());
}

static void test_io_adapter_no_scheduler_include() {
  // IO-001: io_adapter.h 不 include Runtime scheduler/模块实现
  const char* repo = std::getenv("ASTROCS_REPO");
  std::string hp = (repo ? repo : "..") + std::string("/include/astrocs/io/io_adapter.h");
  std::ifstream h(hp);
  std::string content((std::istreambuf_iterator<char>(h)),
                      std::istreambuf_iterator<char>());
  CHECK(content.find("scheduler.h") == std::string::npos);
  CHECK(content.find("phase1") == std::string::npos);
  CHECK(content.find("phase2") == std::string::npos);
  CHECK(content.find("phase3") == std::string::npos);
  CHECK(content.find("calibration") == std::string::npos);
}

int main() {
  test_transaction_commit();
  test_transaction_abort_cleans();
  test_transaction_verify_rejects();
  test_commit_without_begin();
  test_transaction_checksum_rejects_bitflip();
  test_transaction_write_failure_not_silent();
  test_concurrent_begin_unique_tmp();
  test_file_adapter();
  test_atomic_write();
  test_io_adapter_no_scheduler_include();
  if (failures == 0) {
    std::printf("IO-001 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "IO-001 TESTS FAIL (%d)\n", failures);
  return 1;
}
