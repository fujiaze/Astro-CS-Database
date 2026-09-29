/* eng/tests/unit/aio/verify_order_probe.cpp —— 裁决 ④「两序区分用例」的被测进程。
 *
 * 判据要点：verify_fn 是**内容敏感**的 —— 重开目标文件并把全部字节与期望负载逐字节
 * 比对（不是只判"存在"）。只有这样，"rename 之后目标被改坏"才可能被判红。
 *
 * 用法: aio_verify_order_probe <workdir>
 * 输出（机器可解析；断言在 check_verify_order.py 内，探针只如实报告）:
 *   RESULT status=<int> renamed=<0|1> durability=<name> target_exists=<0|1> bytes=<n>
 *   TARGET <目标路径>
 *   MESSAGE <原样错误文本>
 * 语义锚: docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md §4/§4.1（两序关系正本）;
 *         docs/detail/infrastructure/17_aio.md §4（三态）。
 * 返回码: 0 = 已报告（不代表发布成功）; 2 = 用法错误。
 * 平台: POSIX-only（负例靠 LD_PRELOAD 注入，见 corrupt_after_rename_interposer.cpp）。
 */
#include <cstdio>
#include <string>

#include "astro/aio/atomic_publish.h"

#include <sys/stat.h>

using namespace astrocs::aio;

namespace {

const char kPayload[] = "verify-order-payload-v1";

bool read_all(const std::string& p, std::string* out) {
  std::FILE* f = std::fopen(p.c_str(), "rb");
  if (f == nullptr) return false;
  char buf[4096];
  const std::size_t n = std::fread(buf, 1, sizeof(buf), f);
  std::fclose(f);
  out->assign(buf, n);
  return true;
}

// 内容敏感验证：目标必须存在，且字节与期望负载**完全一致**。
bool verify_bytes_equal(const std::string& p, std::string* err) {
  std::string got;
  if (!read_all(p, &got)) {
    if (err) *err = "verify: unreadable " + p;
    return false;
  }
  if (got != kPayload) {
    if (err) {
      *err = "verify: content mismatch (" + std::to_string(got.size()) +
             " bytes, expected " + std::to_string(sizeof(kPayload) - 1) + ")";
    }
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: aio_verify_order_probe <workdir>\n");
    return 2;
  }
  const std::string workdir = argv[1];
  ::mkdir(workdir.c_str(), 0755);
  const std::string target = workdir + "/product.bin";

  PublishOptions opts;  // 默认: verify_after_rename=true, fsync_directory=true,
                        //       remove_on_verify_failure=true
  std::string err;
  const PublishResult r = atomic_write_bytes(target, kPayload, verify_bytes_equal,
                                             opts, CancelFn());
  struct stat st;
  const bool exists = (::stat(target.c_str(), &st) == 0);
  std::string got;
  const std::size_t bytes = (exists && read_all(target, &got)) ? got.size() : 0;
  std::printf("RESULT status=%d renamed=%d durability=%s target_exists=%d bytes=%zu\n",
              static_cast<int>(r.status), r.renamed ? 1 : 0,
              publish_durability_name(r.durability), exists ? 1 : 0, bytes);
  std::printf("TARGET %s\n", target.c_str());
  std::printf("MESSAGE %s\n", r.message.empty() ? err.c_str() : r.message.c_str());
  return 0;
}
