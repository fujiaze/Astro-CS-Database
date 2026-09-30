/* atomic_durability_probe.cpp — P-174 三终态发布探针（正/负例的被测进程）。
 *
 * 用法: aio_atomic_durability_probe <file|dir> <workdir>
 * 输出（机器可解析；断言在 check_atomic_durability.py 内，探针只如实报告 ——
 * 这样同一套断言可直接跑在**变异后源码**上判红）:
 *   RESULT mode=<m> status=<name> renamed=<0|1> durability=<name> visible=<0|1>
 *          target_exists=<0|1> tmp_residue=<0|1> consistent=<0|1>
 *   TARGET <目标路径>      （独立一行: 路径可含空格，不参与 RESULT 的空格分词）
 *   MESSAGE <原样错误文本>  （可空）
 * 语义锚: docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md §4（步序正本）/§6（错误语义）;
 *         docs/detail/infrastructure/17_aio.md §4（P-174：发布终态三态）。
 * 返回码: 0 = 已报告（不代表发布成功）; 2 = 用法错误。
 * 平台: POSIX-only —— 第三态的负例注入靠 LD_PRELOAD（只让 rename 之后目录 fd 的
 *       fsync 失败），Windows 无此机制；且 Windows 无目录 fsync 等价物，第三态在
 *       Windows 上恒成立但不可观测（见 docs/detail/infrastructure/17_aio.md §4）。
 */
#include <cstdio>
#include <string>

#include "astro/aio/atomic_publish.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace acsd::aio;

namespace {

const char kFilePayload[] = "hello-durability-v1";
const char kTilePayload[] = "tile-durability-v1";

void mkdir_p(const std::string& path) {
  std::string cur;
  std::size_t i = 0;
  if (!path.empty() && path[0] == '/') {
    cur = "/";
    i = 1;
  }
  while (i <= path.size()) {
    const std::size_t slash = path.find('/', i);
    const std::string seg =
        path.substr(i, slash == std::string::npos ? std::string::npos : slash - i);
    if (!seg.empty()) {
      if (!cur.empty() && cur.back() != '/') cur += "/";
      cur += seg;
      ::mkdir(cur.c_str(), 0755);
    }
    if (slash == std::string::npos) break;
    i = slash + 1;
  }
}

bool read_file(const std::string& p, std::string* out) {
  std::FILE* f = std::fopen(p.c_str(), "rb");
  if (!f) return false;
  char buf[512];
  const std::size_t n = std::fread(buf, 1, sizeof(buf), f);
  std::fclose(f);
  out->assign(buf, n);
  return true;
}

// 目录内是否残留 tmp/staging 命名族（`.tmp-` / `.staging.tmp-`）——与探针的
// 进程无关的独立扫描，避免只信 PublishResult::tmp_residue。
bool has_tmp_residue(const std::string& dir) {
  DIR* d = ::opendir(dir.c_str());
  if (d == nullptr) return false;
  bool found = false;
  while (struct dirent* e = ::readdir(d)) {
    if (std::string(e->d_name).find("tmp-") != std::string::npos) {
      found = true;
      break;
    }
  }
  ::closedir(d);
  return found;
}

bool verify_present(const std::string& p, std::string* err) {
  std::FILE* f = std::fopen(p.c_str(), "rb");
  if (!f) {
    if (err) *err = "verify: missing " + p;
    return false;
  }
  std::fclose(f);
  return true;
}

bool verify_tree(const std::string& dir, std::string* err) {
  return verify_present(dir + "/properties", err) &&
         verify_present(dir + "/norder6/tile.fits", err);
}

bool build_tree(const std::string& stage, const CancelFn&, std::string* err) {
  ::mkdir((stage + "/norder6").c_str(), 0755);
  std::FILE* f = std::fopen((stage + "/norder6/tile.fits").c_str(), "wb");
  if (!f) {
    if (err) *err = "cannot create tile";
    return false;
  }
  std::fputs(kTilePayload, f);
  std::fclose(f);
  f = std::fopen((stage + "/properties").c_str(), "wb");
  if (!f) {
    if (err) *err = "cannot create properties";
    return false;
  }
  std::fputs("creator_did = ivo://acsd/probe\n", f);
  std::fclose(f);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr,
                 "usage: aio_atomic_durability_probe <file|dir> <workdir>\n");
    return 2;
  }
  const std::string mode = argv[1];
  const std::string workdir = argv[2];
  mkdir_p(workdir);

  PublishResult r;
  bool visible = false;
  if (mode == "file") {
    const std::string target = workdir + "/product.txt";
    r = atomic_write_bytes(target, kFilePayload, verify_present, PublishOptions(),
                           CancelFn());
    std::string got;
    visible = read_file(target, &got) && got == kFilePayload;
  } else if (mode == "dir") {
    const std::string target = workdir + "/product.hips";
    r = atomic_publish_directory(target, build_tree, verify_tree,
                                 PublishOptions(), CancelFn());
    std::string tile;
    visible = read_file(target + "/norder6/tile.fits", &tile) &&
              tile == kTilePayload;
  } else {
    std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
    return 2;
  }

  // 目标路径的独立可见性（不信 visible 字段的调用方也可复核）。
  const bool target_exists = [&r]() {
    std::FILE* f = std::fopen(r.target.c_str(), "rb");
    if (f) {
      std::fclose(f);
      return true;
    }
    return false;
  }();
  const bool residue = r.tmp_residue || has_tmp_residue(workdir);
  std::printf(
      "RESULT mode=%s status=%s renamed=%d durability=%s visible=%d "
      "target_exists=%d tmp_residue=%d consistent=%d\n",
      mode.c_str(), publish_status_name(r.status), r.renamed ? 1 : 0,
      publish_durability_name(r.durability), visible ? 1 : 0,
      target_exists ? 1 : 0, residue ? 1 : 0,
      publish_result_consistent(r) ? 1 : 0);
  std::printf("TARGET %s\n", r.target.c_str());
  std::printf("MESSAGE %s\n", r.message.c_str());
  return 0;
}
