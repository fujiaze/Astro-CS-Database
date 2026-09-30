// eng/tests/unit/aio/fsync_dir_fail_interposer.cpp — P-174 第三态负例注入器。
//
// LD_PRELOAD 拦截 fsync(2): 仅当 (a) 环境变量 ACSD_FAIL_DIR_FSYNC=1、
// (b) 目标 fd 经 fstat(2) 判定为**目录**、(c) 本进程已发生过一次成功的 rename(2)
// 时返回 -1/EIO。文件 fd 的 fsync 与 rename **之前**的目录 fsync(目录树 staging
// 落盘)一律放行 —— 负例只注入「rename 之后目录 fsync 失败」这一个条件，其余步序
// (IO_003 §4: 写 -> fsync -> 校验 -> rename -> 目录 fsync)不变。
// stderr 事件行 (EVENT FSYNC-PASS/FSYNC-DIR-FAIL/RENAME) 供 runner 记录原始证据。
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <string>

namespace {

int (*g_real_fsync)(int) = nullptr;
int (*g_real_rename)(const char*, const char*) = nullptr;
int g_fail_dir_fsync = 0;
int g_renames_ok = 0;

// 直接系统调用写 stderr: 不得经 stdio(会被污染/递归)。
void emit(const char* ev, const std::string& arg) {
  const std::string line = std::string("EVENT ") + ev + " " + arg + "\n";
  (void)::write(2, line.data(), line.size());
}

__attribute__((constructor)) void aio_fsync_dir_fail_init(void) {
  g_real_fsync = (int (*)(int))dlsym(RTLD_NEXT, "fsync");
  g_real_rename =
      (int (*)(const char*, const char*))dlsym(RTLD_NEXT, "rename");
  g_fail_dir_fsync = std::getenv("ACSD_FAIL_DIR_FSYNC") ? 1 : 0;
}

}  // namespace

extern "C" int fsync(int fd) {
  struct stat st;
  const bool is_dir = (::fstat(fd, &st) == 0) && S_ISDIR(st.st_mode);
  if (g_fail_dir_fsync && is_dir && g_renames_ok > 0) {
    emit("FSYNC-DIR-FAIL", std::to_string(fd));
    errno = EIO;
    return -1;
  }
  emit("FSYNC-PASS", std::to_string(fd) + (is_dir ? " dir" : " file"));
  return g_real_fsync ? g_real_fsync(fd) : 0;
}

extern "C" int rename(const char* oldp, const char* newp) {
  const int rc = g_real_rename ? g_real_rename(oldp, newp) : -1;
  if (rc == 0) ++g_renames_ok;
  emit("RENAME", std::string(oldp ? oldp : "-") + " -> " +
                     std::string(newp ? newp : "-") +
                     (rc == 0 ? " ok" : " fail"));
  return rc;
}
