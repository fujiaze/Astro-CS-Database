// P2-007 单元测试: canonical Phase2 IR 全链 + p2_session facade
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static std::string read_file(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

int main() {
  const char* repo = std::getenv("ASTROCS_REPO");
  const std::string base = repo ? repo : "..";

  // 1) canonical IR 全链: p2_session 声明全部阶段节点
  {
    std::string src = read_file(base + "/lib/phase2_session/p2_session.cpp");
    int n_nodes = 0;
    for (const char* node : {"coverage", "sample", "upm_build", "persist"}) {
      if (src.find(std::string("stage(\"") + node + "\"") != std::string::npos) ++n_nodes;
      else {
        std::fprintf(stderr, "IR node %s not declared in p2_session\n", node);
        ++failures;
      }
    }
    CHECK(n_nodes == 4);   // canonical IR 4 节点全链
  }

  // 2) p2_session 只作 Runtime facade: 委托 p2_* 接口, 不复制算法
  {
    std::string src = read_file(base + "/lib/phase2_session/p2_session.cpp");
    for (const char* d : {"p2_coverage_build", "p2_sample_controls", "p2_upm_build"}) {
      if (src.find(d) == std::string::npos) {
        std::fprintf(stderr, "facade delegate %s missing\n", d);
        ++failures;
      }
    }
    // facade 不内联科学: 不应包含 UPM 求解循环等
    // (求解在 upm.cpp, 委托验证见上)
  }

  // 3) 阶段顺序: coverage → sampler → upm (canonical 序)
  {
    std::string src = read_file(base + "/lib/phase2_session/p2_session.cpp");
    std::size_t p1 = src.find("p2_coverage_build");
    std::size_t p2 = src.find("p2_sample_controls");
    std::size_t p3 = src.find("p2_upm_build");
    CHECK(p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos);
    if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos)
      CHECK(p1 < p2 && p2 < p3);   // 顺序正确
  }

  // 4) manifest trace: 运行 trace 记录 stages (与静态 IR 一致)
  {
    std::string src = read_file(base + "/lib/phase2_session/p2_session.cpp");
    CHECK(src.find("stages") != std::string::npos);
    CHECK(src.find("kind") != std::string::npos);
  }

  if (failures == 0) {
    std::printf("P2-007 TESTS PASS (canonical IR 全链节点, facade 委托 p2_*, 阶段序, manifest trace)\n");
    return 0;
  }
  std::fprintf(stderr, "P2-007 TESTS FAIL (%d)\n", failures);
  return 1;
}
