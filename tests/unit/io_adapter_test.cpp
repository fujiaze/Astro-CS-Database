// IO-001 单元测试: Artifact 事务 + FileIoAdapter
#include "astrocs/io/io_adapter.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

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
  const char* d = std::getenv("TMPDIR");
  return d ? d : "/tmp";
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
