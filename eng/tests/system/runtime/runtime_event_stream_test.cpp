// P-163 判据：JsonlEmitter::emit 必须查 std::fputs / std::fflush 的返回值
//（旧实现两者都不查 ⇒ 管道对端关闭 / 重定向到满盘时**静默丢事件**，调用方以为发过）。
//
// 权威：
//   * docs/plugins/infrastructure/21_observability.md:45-46「日志/事件写入失败 → 非 0
//     （IO=7，磁盘满=10）」；manifest 登记失败 → 7；哈希失败 → 8；
//   * docs/design/LOG_AND_ERROR_SYSTEM.md:120-130（六步日志生命周期：写入 → 确认落盘）；
//   * docs/ASTROCS_DESIGN.md §6.3 stdout 纪律：stdout 恒为纯 JSONL，诊断只走 stderr；
//   * lib/infrastructure/cli/exit_codes.h（IO=7 / RESOURCE=10 码值语义唯一源）。
//
// 注入（POSIX）：stdout → /dev/full（write(2) 返回 ENOSPC）/ 关闭 fd 1（write(2) 返回 EBADF）。
// 判据（可红）：
//   A. 正例 + 纯净性：写入临时文件时 emit 返回 true、无失败状态、文件逐行合法 JSON
//      （协议拒发事件的诊断只走 stderr，不污染 stdout）、子进程退出码 0；
//   B. ENOSPC（行内直接写，stdout 无缓冲）：emit 返回 false、write_failed()、
//      write_exit_code()==10、publication_exit_code(OK)==10、子进程退出码 10，
//      且失败点落在 fputs（last_write_error() 含 ":fputs:"）；
//   C. ENOSPC（默认全缓冲）：同上，失败点落在 fflush（含 ":fflush:"）；
//   D. EBADF（fd 1 关闭）：写失败按 IO 归类 ⇒ 7，子进程退出码 7；
//   E. 发布面：名义 rc 非 0 时原码保留（写失败不掩盖 run 自身失败）。
// 判红的源码变异：把 jsonl.h 的 fputs/fflush 改回不检查返回值 ⇒ B/C/D 全红。

#include "jsonl.h"

#include <cstdio>
#include <cstring>
#include <string>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static int failures = 0;
#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

// §4 progress 冻结扩展字段（与 emit_progress 内部负载同源，此处显式构造以拿到返回值）。
static nlohmann::json progress_extra() {
  return {{"completed", 0}, {"total", 1}, {"unit", "phases"},
          {"rate", nullptr}, {"eta_seconds", nullptr}};
}

#if defined(_WIN32)
static void test_posix_injection_unavailable_note() {
  // 本平台无 /dev/full 与 fd 级写失败注入；正例/纯净性仍跑，注入判据显式声明未执行
  //（不静默、不假装绿灯）。
  astrocs::JsonlEmitter ev(astrocs::make_run_id(), "p163");
  const bool ok = ev.emit("stage_start", "info", "s", "positive");
  CHECK(ok);
  CHECK(!ev.write_failed());
  CHECK(ev.publication_exit_code(astrocs::OK) == astrocs::OK);
  std::fprintf(stderr, "P163_NOTE: ENOSPC/EBADF 注入判据为 POSIX 专属（/dev/full、关闭 fd 1），"
                       "本平台未执行\n");
}
#else
// 子进程探针：按 mode 注入写失败，发一个事件，把观测结果经管道回传，并以
// publication_exit_code(OK) 作为**自身退出码**（进程级退出码证据）。
//   mode 0 = 写临时文件（正例 + 纯净性 + 协议拒发不污染 stdout）
//   mode 1 = /dev/full，stdout 无缓冲（失败点在 fputs）
//   mode 2 = /dev/full，默认全缓冲（失败点在 fflush）
//   mode 3 = 关闭 fd 1（EBADF ⇒ IO=7）
static std::string run_probe_child(int mode, const std::string& tmp_path) {
  int pipefd[2];
  if (::pipe(pipefd) != 0) return "pipe-failed";
  const pid_t pid = ::fork();
  if (pid == 0) {
    ::close(pipefd[0]);
    if (mode == 1) std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (mode == 0) {
      const int fd = ::open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd >= 0) { ::dup2(fd, STDOUT_FILENO); ::close(fd); }
    } else if (mode == 3) {
      ::close(STDOUT_FILENO);
    } else {
      const int fd = ::open("/dev/full", O_WRONLY);
      if (fd >= 0) { ::dup2(fd, STDOUT_FILENO); ::close(fd); }
    }
    std::clearerr(stdout);
    astrocs::JsonlEmitter ev(astrocs::make_run_id(), "p163");
    const bool ok = ev.emit("progress", "info", "progress", "progress update", progress_extra());
    // 协议硬闸拒发（未登记 kind）：诊断走 stderr，stdout 不得被污染；
    // 且这**不是** I/O 写失败，不得置 write_failed_。
    const bool ok_drop = ev.emit("bogus_kind", "info", "s", "dropped");
    const std::string e = ev.last_write_error();
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "ok=%d ok_drop=%d wf=%d hf=%d code=%d pub0=%d pub2=%d fputs=%d fflush=%d",
                  ok ? 1 : 0, ok_drop ? 1 : 0, ev.write_failed() ? 1 : 0,
                  ev.human_channel_failed() ? 1 : 0, ev.write_exit_code(),
                  ev.publication_exit_code(astrocs::OK), ev.publication_exit_code(astrocs::ARGS),
                  e.find(":fputs:") != std::string::npos ? 1 : 0,
                  e.find(":fflush:") != std::string::npos ? 1 : 0);
    const ssize_t n = ::write(pipefd[1], buf, std::strlen(buf));
    (void)n;
    ::close(pipefd[1]);
    ::_exit(ev.publication_exit_code(astrocs::OK));
  }
  ::close(pipefd[1]);
  std::string report;
  char buf[512];
  for (;;) {
    const ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
    if (n <= 0) break;
    report.append(buf, static_cast<std::size_t>(n));
  }
  ::close(pipefd[0]);
  int status = 0;
  ::waitpid(pid, &status, 0);
  report += " exit=" + std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
  return report;
}

static std::string probe_tmp_path(const char* name) {
  return std::string("/tmp/acsd_p163_") + name + "_" + astrocs::make_run_id() + ".jsonl";
}

// A. 正例 + stdout 纯净性 + 进程退出码 0
static void test_positive_purity_and_process_exit_zero() {
  const std::string tmp = probe_tmp_path("positive");
  const std::string rep = run_probe_child(0, tmp);
  std::fprintf(stderr, "P163 probe[A 正例] %s\n", rep.c_str());
  CHECK(rep.find("ok=1") != std::string::npos);
  CHECK(rep.find("ok_drop=0") != std::string::npos);   // 未登记 kind 必须被拒
  CHECK(rep.find("wf=0") != std::string::npos);
  CHECK(rep.find("code=7") != std::string::npos);      // 未失败时保持 IO(7) 缺省
  CHECK(rep.find("pub0=0") != std::string::npos);
  CHECK(rep.find("pub2=2") != std::string::npos);      // 名义非 0 → 原码保留
  CHECK(rep.find("exit=0") != std::string::npos);
  // stdout 逐行合法 JSON，且恰一行（协议拒发的诊断只走 stderr）
  std::FILE* f = std::fopen(tmp.c_str(), "rb");
  CHECK(f != nullptr);
  std::string body;
  if (f) {
    char b[1024];
    std::size_t n = 0;
    while ((n = std::fread(b, 1, sizeof(b), f)) > 0) body.append(b, n);
    std::fclose(f);
  }
  std::size_t lines = 0, bad = 0;
  std::size_t pos = 0;
  while (pos < body.size()) {
    const std::size_t nl = body.find('\n', pos);
    if (nl == std::string::npos) break;
    const std::string line = body.substr(pos, nl - pos);
    pos = nl + 1;
    if (line.empty()) continue;
    ++lines;
    try {
      const auto j = nlohmann::json::parse(line);
      if (j.value("kind", std::string()) != "progress") ++bad;
    } catch (...) { ++bad; }
  }
  CHECK(lines == 1);
  CHECK(bad == 0);
  std::remove(tmp.c_str());
}

// B. ENOSPC + stdout 无缓冲：失败点在 fputs ⇒ 10（RESOURCE），进程退出码 10
static void test_enospc_at_fputs_maps_to_resource_10() {
  const std::string rep = run_probe_child(1, probe_tmp_path("enospc_unbuf"));
  std::fprintf(stderr, "P163 probe[B ENOSPC/fputs] %s\n", rep.c_str());
  CHECK(rep.find("ok=0") != std::string::npos);
  CHECK(rep.find("wf=1") != std::string::npos);
  CHECK(rep.find("code=10") != std::string::npos);
  CHECK(rep.find("pub0=10") != std::string::npos);
  CHECK(rep.find("fputs=1") != std::string::npos);
  CHECK(rep.find("exit=10") != std::string::npos);
}

// C. ENOSPC + 默认全缓冲：失败点在 fflush ⇒ 10（RESOURCE），进程退出码 10
static void test_enospc_at_fflush_maps_to_resource_10() {
  const std::string rep = run_probe_child(2, probe_tmp_path("enospc_buf"));
  std::fprintf(stderr, "P163 probe[C ENOSPC/fflush] %s\n", rep.c_str());
  CHECK(rep.find("ok=0") != std::string::npos);
  CHECK(rep.find("wf=1") != std::string::npos);
  CHECK(rep.find("code=10") != std::string::npos);
  CHECK(rep.find("pub0=10") != std::string::npos);
  CHECK(rep.find("fflush=1") != std::string::npos);
  CHECK(rep.find("exit=10") != std::string::npos);
}

// D. EBADF（fd 1 关闭）：非磁盘满的写失败 ⇒ IO=7，进程退出码 7
static void test_ebadf_maps_to_io_7() {
  const std::string rep = run_probe_child(3, probe_tmp_path("ebadf"));
  std::fprintf(stderr, "P163 probe[D EBADF] %s\n", rep.c_str());
  CHECK(rep.find("ok=0") != std::string::npos);
  CHECK(rep.find("wf=1") != std::string::npos);
  CHECK(rep.find("code=7") != std::string::npos);
  CHECK(rep.find("pub0=7") != std::string::npos);
  CHECK(rep.find("exit=7") != std::string::npos);
}
#endif  // !_WIN32

int main() {
#if defined(_WIN32)
  test_posix_injection_unavailable_note();
#else
  test_positive_purity_and_process_exit_zero();
  test_enospc_at_fputs_maps_to_resource_10();
  test_enospc_at_fflush_maps_to_resource_10();
  test_ebadf_maps_to_io_7();
#endif
  if (failures == 0) {
    std::fprintf(stderr, "P163_EVENT_STREAM_IO_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "P163_EVENT_STREAM_IO_FAIL failures=%d\n", failures);
  return 1;
}
