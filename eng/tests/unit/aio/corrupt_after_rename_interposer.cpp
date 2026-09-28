// eng/tests/unit/aio/corrupt_after_rename_interposer.cpp —— 裁决 ④「两序区分用例」注入器。
//
// 目的：制造一个**只能被"rename 之后再验证"这一序检出**的故障 —— 在 rename 成功返回
// 之后向**目标文件**追加坏字节（模拟外部进程改写/截断、回写坏块）。于是：
//   · 机制层（atomic_publish：rename -> 重开已发布对象验证 -> 失败撤销）必须检出并撤销；
//   · IO_003 §4 的生产者序（校验先于 rename）在该窗口没有观测点，无法检出。
// 这正是 docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md §4.1 所述的"机制层检测面更强"。
//
// 启用：仅当 ASTROCS_TEST_CORRUPT_AFTER_RENAME=1；其余情况纯透传（零影响）。
// stderr 事件行（供 runner 判"注入确实发生"，避免判据退化）：
//   EVENT RENAME <old> -> <new> ok|fail
//   EVENT CORRUPT-AFTER-RENAME <path>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <string>

namespace {

int (*g_real_rename)(const char*, const char*) = nullptr;
int g_corrupt = 0;

void emit(const char* ev, const std::string& arg) {
  const std::string line = std::string("EVENT ") + ev + " " + arg + "\n";
  (void)::write(2, line.data(), line.size());
}

__attribute__((constructor)) void aio_corrupt_init(void) {
  g_real_rename =
      (int (*)(const char*, const char*))dlsym(RTLD_NEXT, "rename");
  g_corrupt = std::getenv("ASTROCS_TEST_CORRUPT_AFTER_RENAME") ? 1 : 0;
}

}  // namespace

extern "C" int rename(const char* oldp, const char* newp) {
  const int rc = g_real_rename ? g_real_rename(oldp, newp) : -1;
  emit("RENAME", std::string(oldp ? oldp : "-") + " -> " +
                     std::string(newp ? newp : "-") + (rc == 0 ? " ok" : " fail"));
  if (rc == 0 && g_corrupt && newp != nullptr) {
    struct stat st;
    if (::stat(newp, &st) == 0 && S_ISREG(st.st_mode)) {
      const int fd = ::open(newp, O_WRONLY | O_APPEND);
      if (fd >= 0) {
        const char kBad[] = "XXXX";
        (void)::write(fd, kBad, sizeof(kBad) - 1);
        (void)::close(fd);
        emit("CORRUPT-AFTER-RENAME", newp);
      }
    }
  }
  return rc;
}
