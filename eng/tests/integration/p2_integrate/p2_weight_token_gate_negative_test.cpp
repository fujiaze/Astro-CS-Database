// ============================================================================
// p2_weight_token_gate_negative_test — 权重来源 token 门的「能红能绿」判据面
// ============================================================================
// WHAT:  两道冻结权重来源门的**返回码语义**判据 + 接线层**消费口径**判据：
//        1) 运行时：p2_weight_source_token_reject / p2_rejection_weight_surface_guard
//           对合法来源必须 rc=0、对冻结禁词必须 rc=1、实参错误必须 rc=2；
//        2) 接线层：run_upm_rej_samp_wiring 的权重来源 token 门块必须**消费**这两个
//           返回码（不得再出现 (void)tok_ok_rc / 丢弃 rc 的裸调用）。
// WHY:   接线层曾把两个 rc 全部丢弃（恒真门，AGENTS.md §5「恒真门没有证据资格」）。
//        判据 1 会在冻结词表被放宽时转红（门失效）；判据 2 会在调用点重新丢弃
//        返回码时转红（回归）。二者合起来把「裁决必须存在且必须被消费」钉死。
// AUTHORITY: docs/contracts/DATA_SEMANTICS.md §31.8（G-WEIGHT-SOURCES /
//        G-DIAGNOSTIC-NOT-WEIGHT）、§31.3/§31.7；FZ-WEIGHT-SINGLE-PATH；
//        docs/plugins/algorithms_phase2/13_integration.md「权重来源受限表的锁定状态」；
//        既定消费口径 = eng/tests/unit/p2_samp/p2_samp_test.cpp:147-181、
//        eng/tests/unit/p2_rej/p2_rej_test.cpp:438-469。
// EXIT:   随两道冻结门退役（DOC-402）整体删除；判据 2 随接线块删除。
// ============================================================================
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "astro/phase2/coverage.h"
#include "astro/phase2/rejection.h"

namespace {

int g_checks = 0;
int g_fails = 0;

void check(bool cond, const std::string& what) {
  ++g_checks;
  if (!cond) {
    ++g_fails;
    std::fprintf(stderr, "  [FAIL] %s\n", what.c_str());
  }
}

// 接线层块里用的两组探针（与 phase2_integrate.cpp 的 tokens_ok / tokens_bad 逐字一致）。
const char* const kTokensOk[] = {"psf", "photometric_response", "noise_covariance"};
const char* const kTokensBad[] = {"support", "coverage", "median_source_snr", "fwhm",
                                  "residual"};

// ── 判据 1：两道门的返回码语义（合法=0 / 禁词=1 / 实参错误=2）─────────────
void case_gate_semantics() {
  {
    char err[256] = {0};
    const int rc = p2_weight_source_token_reject(kTokensOk, 3, err, sizeof(err));
    check(rc == 0, "p2_weight_source_token_reject(legal) must be 0, got " +
                       std::to_string(rc) + " (" + err + ")");
  }
  for (std::size_t i = 0; i < 5; ++i) {
    const char* one[1] = {kTokensBad[i]};
    char err[256] = {0};
    const int rc = p2_weight_source_token_reject(one, 1, err, sizeof(err));
    check(rc == 1, std::string("p2_weight_source_token_reject('") + kTokensBad[i] +
                           "') must be 1 (frozen forbidden source), got " +
                           std::to_string(rc));
  }
  {
    char err[256] = {0};
    check(p2_weight_source_token_reject(nullptr, 1, err, sizeof(err)) == 2,
          "p2_weight_source_token_reject(null, n>0) must be 2 (param error)");
  }
  {
    char err[256] = {0};
    const int rc = p2_rejection_weight_surface_guard(kTokensOk, 3, err, sizeof(err));
    check(rc == 0, "p2_rejection_weight_surface_guard(legal) must be 0, got " +
                       std::to_string(rc) + " (" + err + ")");
  }
  for (std::size_t i = 0; i < 5; ++i) {
    const char* one[1] = {kTokensBad[i]};
    char err[256] = {0};
    const int rc = p2_rejection_weight_surface_guard(one, 1, err, sizeof(err));
    check(rc == 1, std::string("p2_rejection_weight_surface_guard('") + kTokensBad[i] +
                           "') must be 1 (frozen forbidden source), got " +
                           std::to_string(rc));
  }
  {
    char err[256] = {0};
    check(p2_rejection_weight_surface_guard(nullptr, 0, err, sizeof(err)) == 2,
          "p2_rejection_weight_surface_guard(null, 0) must be 2 (empty token list)");
  }
}

std::string read_file(const std::string& p, bool* ok) {
  std::ifstream in(p, std::ios::binary);
  if (!in) { *ok = false; return std::string(); }
  std::ostringstream ss;
  ss << in.rdbuf();
  *ok = true;
  return ss.str();
}

// 去掉注释，避免注释里的历史符号名被当活代码（同 eng/ci/check_no_weight_mode_code.py）。
std::string strip_comments(const std::string& s) {
  std::string t;
  t.reserve(s.size());
  bool in_block = false, in_line = false, in_str = false, in_chr = false;
  for (std::size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    const char n = (i + 1 < s.size()) ? s[i + 1] : '\0';
    if (in_block) {
      if (c == '*' && n == '/') { in_block = false; ++i; }
      t.push_back(' ');
      continue;
    }
    if (in_line) {
      if (c == '\n') { in_line = false; t.push_back(c); }
      else t.push_back(' ');
      continue;
    }
    if (in_str) {
      t.push_back(c);
      if (c == '\\' && i + 1 < s.size()) { t.push_back(s[++i]); continue; }
      if (c == '"') in_str = false;
      continue;
    }
    if (in_chr) {
      t.push_back(c);
      if (c == '\\' && i + 1 < s.size()) { t.push_back(s[++i]); continue; }
      if (c == '\'') in_chr = false;
      continue;
    }
    if (c == '/' && n == '*') { in_block = true; t.push_back(' '); ++i; continue; }
    if (c == '/' && n == '/') { in_line = true; t.push_back(' '); ++i; continue; }
    if (c == '"') { in_str = true; t.push_back(c); continue; }
    if (c == '\'') { in_chr = true; t.push_back(c); continue; }
    t.push_back(c);
  }
  return t;
}

// ── 判据 2：接线层必须消费两道门的返回码（不得恒真）──────────────────────
void case_wiring_consumes_rc(const std::string& src_path) {
  bool ok = false;
  const std::string raw = read_file(src_path, &ok);
  if (!ok) {
    ++g_checks; ++g_fails;
    std::fprintf(stderr, "  [FAIL] cannot read %s\n", src_path.c_str());
    return;
  }
  const std::string code = strip_comments(raw);

  check(code.find("(void)tok_ok_rc") == std::string::npos,
        "wiring block must NOT discard the token gate rc via (void)tok_ok_rc");

  // 通用判据：本 TU 里两道的**每一次**调用，返回值都必须被绑定到变量
  // （调用点前紧邻的非空白字符必须是 '='）——裸调用即裁决被丢弃。
  auto all_calls_bound = [&code](const char* fn) -> int {
    const std::string name(fn);
    int bound = 0, unbound = 0;
    for (std::size_t p = code.find(name + "("); p != std::string::npos;
         p = code.find(name + "(", p + 1)) {
      std::size_t i = p;
      while (i > 0 && std::isspace(static_cast<unsigned char>(code[i - 1]))) --i;
      if (i > 0 && code[i - 1] == '=') ++bound; else ++unbound;
    }
    check(unbound == 0,
          std::string(fn) + ": every call site must bind its return value (" +
              std::to_string(unbound) + " unbound)");
    return bound;
  };
  check(all_calls_bound("p2_weight_source_token_reject") >= 2,
        "p2_weight_source_token_reject must be probed on both legal and forbidden tokens");
  check(all_calls_bound("p2_rejection_weight_surface_guard") >= 2,
        "p2_rejection_weight_surface_guard must be probed on both legal and forbidden tokens");

  // 两道门的返回值都必须先落到具名变量，再被比较（否则无法被裁决）。
  check(code.find("const int tok_ok_rc = p2_weight_source_token_reject(") !=
            std::string::npos,
        "p2_weight_source_token_reject rc must be bound to tok_ok_rc");
  check(code.find("const int tok_bad_rc = p2_weight_source_token_reject(") !=
            std::string::npos,
        "p2_weight_source_token_reject rc must also be bound on the forbidden probe");
  check(code.find("const int surf_ok_rc = p2_rejection_weight_surface_guard(") !=
            std::string::npos,
        "p2_rejection_weight_surface_guard rc must be bound to surf_ok_rc");
  check(code.find("const int surf_bad_rc = p2_rejection_weight_surface_guard(") !=
            std::string::npos,
        "p2_rejection_weight_surface_guard rc must also be bound on the forbidden probe");

  // 裁决 + 与 :1305 同型的收尾（写 r.error + p2_upm_ma_close + 提前 return）。
  check(code.find("if (tok_ok_rc != 0) {") != std::string::npos,
        "legal-probe rc must be adjudicated (over-rejection direction)");
  check(code.find("if (tok_bad_rc != 1) {") != std::string::npos,
        "forbidden-probe rc must be adjudicated (under-rejection direction)");
  check(code.find("if (surf_ok_rc != 0) {") != std::string::npos,
        "surface legal-probe rc must be adjudicated");
  check(code.find("if (surf_bad_rc != 1) {") != std::string::npos,
        "surface forbidden-probe rc must be adjudicated");

  int closes = 0;
  const std::string needle = "p2_upm_ma_close(model); return r;";
  for (std::size_t p = code.find(needle); p != std::string::npos;
       p = code.find(needle, p + 1))
    ++closes;
  check(closes >= 8,
        "each adjudicated failure must do the :1305 teardown "
        "(p2_upm_ma_close + early return); found " + std::to_string(closes));
}

}  // namespace

int main(int argc, char** argv) {
  std::string src = "lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp";
  if (argc > 1) src = argv[1];

  case_gate_semantics();
  case_wiring_consumes_rc(src);

  if (g_fails != 0) {
    std::fprintf(stderr, "RESULT FAIL p2_weight_token_gate checks=%d fails=%d\n",
                 g_checks, g_fails);
    return 1;
  }
  std::printf("RESULT PASS p2_weight_token_gate checks=%d\n", g_checks);
  return 0;
}
