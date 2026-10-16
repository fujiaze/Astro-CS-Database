// eng/tests/unit/frame_science_leaf_order_reader_test.cpp
//
// Issue #10 验证：用**真实** `acsd::normalize::read_frame_science_document`
// （lib/algorithms/integration/normalize_science，生产 reader 本体）核对已发布
// `frame_science.json` 的 `signal.leaf_order` 与真实 HEALPix 叶阶 K 的一致性，
// 并如实记录旧格式（K−9）sidecar 在当前 reader 下是否被静默接受。
//
// 判据（positive：必须全绿）：
//   · reader.ok == true 且 findings 为空（该产品是 canonical precision 真源）；
//   · doc.signal.leaf_order == K（K = log2(frame_nside)，由生产 CLI 构型给定）；
//   · doc.precision.controls[*].leaf_order == K；
//   · doc.precision.controls[*].leaf_ipix < 12·4^K。
// 探针（informational，记录真实行为，不新增/不实现迁移）：
//   · 把同一 sidecar 的 signal.leaf_order 改成旧值 K−9，再经 reader 读回，
//     记录 reader 是否**静默接受**（ok=true 且值原样为 K−9）——这是历史兼容风险；
//   · 若提供真实旧 sidecar（argv[3]），同样读回并登记。
//
// 用法：frame_science_leaf_order_reader_test <product_dir> <K> [legacy_sidecar.json]
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "acsd/normalize_science.h"

using acsd::normalize::FrameScienceReadResult;

static int g_fail = 0;
static void chk(bool ok, const std::string& what, const std::string& detail = "") {
  std::printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", what.c_str(),
              detail.empty() ? "" : " — ", detail.c_str());
  if (!ok) ++g_fail;
}
static void finding(const std::string& what) {
  std::printf("  [FINDING] %s\n", what.c_str());
}

static uint64_t npix_at(int K) {
  // 12·4^K（K<=29 时 12·2^58 < 2^63，uint64 安全）
  return 12ull << (2 * K);
}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <product_dir> <K> [legacy_sidecar.json]\n", argv[0]);
    return 2;
  }
  const std::string prod = argv[1];
  const int K = std::atoi(argv[2]);
  const std::string sidecar = prod + "/frame_science.json";
  std::printf("== frame_science leaf_order reader test (Issue #10) ==\n");

  // ── 数学 oracle（不分配任何帧）：K=9/11/13 → tile 阶 K−9；Ω=4π/(12·4^K) ──
  {
    const double pi = 3.14159265358979323846;
    const int cases[][2] = {{9, 0}, {11, 2}, {13, 4}};
    for (const auto& c : cases) {
      const int kk = c[0], tile = c[1];
      const uint64_t np = npix_at(kk);
      const double om = 4.0 * pi / (double)np;  // Omega = 4pi/(12*4^K)
      const double ps_deg = (180.0 / pi) * std::sqrt(pi / 3.0) / (double)(1u << kk);
      const double om_ps = (ps_deg * pi / 180.0) * (ps_deg * pi / 180.0);
      chk(kk - 9 == tile, "math: K-9 == tile order",
          "K=" + std::to_string(kk) + " tile=" + std::to_string(tile));
      chk(std::fabs(om - om_ps) / om < 1e-12, "math: Omega == (pixel_scale_rad)^2",
          "K=" + std::to_string(kk) + " Omega=" + std::to_string(om));
    }
  }

  std::printf("product=%s  K=%d  npix=12*4^K=%llu  Omega=4pi/(12*4^K)=%.6e sr\n",
              prod.c_str(), K, (unsigned long long)npix_at(K),
              4.0 * 3.14159265358979323846 / (double)npix_at(K));

  const auto r = acsd::normalize::read_frame_science_document(sidecar);
  chk(r.sidecar_present, "sidecar present", sidecar);
  chk(r.schema_supported, "schema supported (acsd.normalize.frame-science/v1)");
  chk(r.ok, "reader.ok == true (canonical precision source)",
      r.ok ? "" : r.error);
  chk(r.findings.empty(), "reader.findings empty",
      r.findings.empty() ? "" : r.findings.front());
  chk(r.doc.signal.leaf_order == K,
      "doc.signal.leaf_order == K (真实 HEALPix 叶阶)",
      "signal.leaf_order=" + std::to_string(r.doc.signal.leaf_order) +
          " expected K=" + std::to_string(K));
  bool ctrl_ok = !r.doc.precision.controls.empty();
  bool ipix_ok = true;
  for (const auto& c : r.doc.precision.controls) {
    if (c.leaf_order != K) ctrl_ok = false;
    if (c.leaf_ipix_valid && c.leaf_ipix >= npix_at(K)) ipix_ok = false;
  }
  chk(ctrl_ok, "controls[*].leaf_order == K",
      "n_controls=" + std::to_string(r.doc.precision.controls.size()));
  chk(ipix_ok, "controls[*].leaf_ipix < 12*4^K");

  // ── 旧格式探针：signal.leaf_order = K−9 ──────────────────────────────────
  if (K - 9 >= 0) {
    std::ifstream in(sidecar, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    nlohmann::json j;
    try {
      j = nlohmann::json::parse(ss.str());
    } catch (const std::exception& e) {
      chk(false, "legacy probe: sidecar parse", e.what());
      j = nlohmann::json::object();
    }
    if (j.contains("signal") && j["signal"].is_object()) {
      // 探针 1: 旧格式 K−9；探针 2: 0（K=9 的旧值，schema minimum=9 也不满足）；
      // 探针 3: 负数（完全非法）。记录 reader 是否静默接受。
      for (int probe : {K - 9, 0, -5}) {
        nlohmann::json jp = j;
        jp["signal"]["leaf_order"] = probe;
        const std::string legacy_path =
            "/tmp/issue10_legacy_probe_sidecar_" + std::to_string(probe) + ".json";
        {
          std::ofstream out(legacy_path, std::ios::binary | std::ios::trunc);
          out << jp.dump(2);
        }
        const auto lr = acsd::normalize::read_frame_science_document(legacy_path);
        const bool silent = lr.ok && lr.findings.empty();
        std::printf("  [probe] signal.leaf_order=%-4d -> reader.ok=%d findings=%zu "
                    "read-back=%d %s\n",
                    probe, (int)lr.ok, lr.findings.size(), lr.doc.signal.leaf_order,
                    silent ? "(silently accepted)" : "(rejected/flagged)");
        if (probe == K - 9 && silent && lr.doc.signal.leaf_order == probe) {
          finding("当前 reader 静默接受旧格式 signal.leaf_order=K-9（不区分真实叶阶 K=" +
                  std::to_string(K) + "）；无自动 +9 迁移、无版本判定。");
          chk(true, "legacy probe K-9: passed through unchanged");
        } else if (probe == K - 9 && !silent) {
          finding("reader 对旧格式 K-9 sidecar 判红: " + lr.error);
          chk(true, "legacy probe K-9: reader rejected");
        } else if (probe == 0 && silent) {
          finding("reader 对 signal.leaf_order=0 也静默接受（schema minimum=9 未在运行时执行）");
          chk(true, "probe 0: silently accepted (documented)");
        } else if (probe == -5 && silent) {
          finding("reader 对 signal.leaf_order=-5 也静默接受（signal.leaf_order 运行时零校验）");
          chk(true, "probe -5: silently accepted (documented)");
        }
      }
    } else {
      chk(false, "legacy probe: sidecar has no signal object");
    }
  }

  // ── 真实旧 sidecar（可选）────────────────────────────────────────────────
  if (argc >= 4) {
    const auto lr = acsd::normalize::read_frame_science_document(argv[3]);
    std::printf("  [probe] real legacy sidecar %s -> reader.ok=%d findings=%zu "
                "signal.leaf_order=%d controls.leaf_order(0)=%d\n",
                argv[3], (int)lr.ok, lr.findings.size(), lr.doc.signal.leaf_order,
                lr.doc.precision.controls.empty() ? -1
                                                  : lr.doc.precision.controls[0].leaf_order);
    if (lr.ok && lr.doc.signal.leaf_order + 9 ==
                     (lr.doc.precision.controls.empty()
                          ? -1
                          : lr.doc.precision.controls[0].leaf_order)) {
      finding("真实旧产品被 reader 静默接受：signal.leaf_order=" +
              std::to_string(lr.doc.signal.leaf_order) + " 实为 tile 阶，真实叶阶=" +
              std::to_string(lr.doc.precision.controls[0].leaf_order) + "。");
    }
  }

  std::printf("== %s (failures=%d) ==\n", g_fail == 0 ? "ALL PASS" : "HAS FAILURES", g_fail);
  return g_fail == 0 ? 0 : 1;
}
