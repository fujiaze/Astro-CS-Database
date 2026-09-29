// P-173 负例探针：并发取临时路径的唯一性（aio_atomic::make_tmp_path -> next_seq）。
//
// 台账依据: 代码域台账 P-173「next_seq 非原子 static++」。
// 权威依据: docs/detail/infrastructure/17_aio.md:26「所有产品 = 临时文件/目录 +
//   校验 + fsync + 原子 rename 提交」+ aio_atomic_file.h:14-16「临时文件名
//   <final>.tmp.<pid>.<seq>，与目标同目录」⇒ 并发取号必须唯一，否则两个发布者
//   互踩同一 tmp（C++ 数据竞争 ⇒ UB）。
//
// 用法: tmp_path_uniqueness_probe [threads] [iters] [target]
//   退出码 0 = 全部唯一；2 = 出现重复临时名（判红）。
#include "aio_atomic_file.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
  const int threads = argc > 1 ? std::atoi(argv[1]) : 8;
  const int iters = argc > 2 ? std::atoi(argv[2]) : 20000;
  const std::string target = argc > 3 ? argv[3] : "/tmp/p173_target.fits";
  std::vector<std::vector<std::string>> per(static_cast<std::size_t>(threads));
  std::vector<std::thread> ts;
  ts.reserve(static_cast<std::size_t>(threads));
  for (int t = 0; t < threads; ++t) {
    ts.emplace_back([&per, t, iters, &target]() {
      auto& v = per[static_cast<std::size_t>(t)];
      v.reserve(static_cast<std::size_t>(iters));
      for (int i = 0; i < iters; ++i) v.push_back(aio_atomic::make_tmp_path(target));
    });
  }
  for (auto& th : ts) th.join();
  std::set<std::string> uniq;
  std::size_t total = 0;
  for (const auto& v : per) {
    total += v.size();
    for (const auto& s : v) uniq.insert(s);
  }
  std::printf("P173 total=%zu unique=%zu\n", total, uniq.size());
  if (uniq.size() != total) {
    std::printf("P173_DUPLICATE_TMP_NAME\n");
    return 2;
  }
  std::printf("P173_UNIQUE_OK\n");
  return 0;
}
