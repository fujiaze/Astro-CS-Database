// eng/tests/unit/p0_19_mixed_hips_order_test.cpp
//
// P0-19（Issue #19）跨 hips_order fail-closed 的 C++ 生产入口验证。
//
// 被测 = **生产模块本体**（route = ModuleRegistry → IModule::plan → execute →
// last_manifest，与 Runtime 调度同一生命周期，见 lib/include/acsd/core/module.h）：
//   · acsd.phase2.coverage   —— 生产准入（写 coverage artifact 之前必须拒绝混阶）;
//   · acsd.phase2.sample     —— 经 p2_coverage_from_artifact 消费 artifact 的真实入口;
//   · acsd.phase2.upm-apply  —— 直接读 p2_coverage.json 的独立像元映射入口;
//   · acsd.phase2.write      —— 发布端（读 p2_coverage.json，禁混阶落 complete 产品）。
//
// 编译/链接见 run_p0_19_mixed_hips_order_test.py（g++ 直编 + 链接已构建静态库，
// 与仓内 photo_identity_test 同款，不新增 CMake 目标、不改生产代码接口）。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "acsd/core/context.h"
#include "acsd/core/module.h"
#include "acsd/core/module_adapters.h"

namespace fs = std::filesystem;
using Json = nlohmann::json;
using acsd::core::ModuleRegistry;
using acsd::core::RunContext;

static int g_fail = 0;
static int g_pass = 0;
static int g_skip = 0;

static void chk(bool ok, const std::string& what, const std::string& detail = "") {
  if (ok) ++g_pass; else ++g_fail;
  std::printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", what.c_str(),
              detail.empty() ? "" : " — ", detail.c_str());
}
static void skip(const std::string& what) {
  ++g_skip;
  std::printf("  [SKIP] %s\n", what.c_str());
}
static bool has(const std::string& s, const std::string& sub) {
  return s.find(sub) != std::string::npos;
}
static const char* kNamed =
    "mixed hips_order unsupported without spherical resampling";

static void write_text(const std::string& path, const std::string& text) {
  fs::create_directories(fs::path(path).parent_path());
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  f << text;
}

// 经真实 module 生命周期执行一个 P2 子节点；返回 (ok, manifest_json, error_msg)。
struct NodeOutcome {
  bool ok = false;
  std::string manifest;   // 节点 manifest（含 "error"/"error_kind"/"status"）
  std::string error;      // Result 的错误文本
};

static NodeOutcome run_node(const std::string& module_id,
                            const std::string& cfg) {
  NodeOutcome out;
  ModuleRegistry reg;
  auto rr = acsd::core::register_phase_modules(reg);
  if (rr.failed()) {
    out.error = "register_phase_modules: " + rr.error().message();
    return out;
  }
  auto mm = reg.create(module_id);
  if (mm.failed()) {
    out.error = "create: " + mm.error().message();
    return out;
  }
  std::unique_ptr<acsd::core::IModule> m = std::move(mm.value());
  auto pr = m->plan("p0-19-test-node", cfg);  // plan 保存 config_（execute 消费）
  if (pr.failed()) {
    out.error = "plan: " + pr.error().message();
    return out;
  }
  RunContext ctx;  // 非调度测试上下文：空租约（context.h 明示允许）
  auto r = m->execute(ctx);
  out.ok = r.ok();
  if (r.failed()) out.error = r.error().message();
  auto lm = m->last_manifest();
  if (lm.ok()) out.manifest = lm.value();
  return out;
}

static std::string json_field(const std::string& s, const std::string& key) {
  try {
    const Json j = Json::parse(s);
    if (j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
  } catch (...) {
  }
  return "";
}

// 合法 JSON schema 的 coverage artifact（frames[].max_leaf_order 可控）。
static Json mk_cov(const std::vector<std::pair<std::string, int>>& frames) {
  Json fr = Json::array();
  Json paths = Json::array();
  for (const auto& kv : frames) {
    fr.push_back(Json{{"hips_path", kv.first},
                      {"frame_id", fs::path(kv.first).filename().string()},
                      {"max_leaf_order", kv.second},
                      {"n_tiles", 1},
                      {"filter_passband", "Halpha"},
                      {"frame_type", "equatorial"}});
    paths.push_back(kv.first);
  }
  return Json{{"schema", "DATA-P2-COV"},
              {"entry", "p2_coverage_build"},
              {"n_inputs", frames.size()},
              {"target_order", 0},
              {"n_union_cells", 1},
              {"hips_paths", paths},
              {"frames", fr},
              {"union_cells", Json::array({Json{{"order", 0}, {"ipix", 0}}})}};
}

int main(int argc, char** argv) {
  const std::string fix = (argc >= 2 && std::strlen(argv[1]) > 0) ? argv[1] : "";
  const std::string scratch = (argc >= 3) ? argv[2] : "/tmp/p019_cxx_test";
  std::error_code ec;
  fs::remove_all(scratch, ec);

  const std::string d0 = fix.empty() ? "" : fix + "/ord0_halpha";
  const std::string d1 = fix.empty() ? "" : fix + "/ord1_halpha";
  const std::string d0r = fix.empty() ? "" : fix + "/ord0_red";
  const bool have_fx = !fix.empty() && fs::exists(d0) && fs::exists(d1) &&
                       fs::exists(d0r);

  std::printf("== P0-19 跨 hips_order fail-closed 生产入口验证 ==\n");

  // ── A. acsd.phase2.coverage 生产准入（需要真实可读 HiPS）────────────────
  std::printf("[A] acsd.phase2.coverage 生产准入\n");
  if (!have_fx) {
    skip("真实 HiPS 夹具缺失（argv[1]），A 段跳过");
  } else {
    auto cov = [&](const std::vector<std::string>& paths,
                   const std::string& od) {
      fs::create_directories(od);
      Json doc{{"hips_paths", paths}, {"output_dir", od}};
      return run_node("acsd.phase2.coverage", doc.dump());
    };
    const std::string od_mix = scratch + "/a_mix01";
    NodeOutcome r1 = cov({d0, d1}, od_mix);
    chk(!r1.ok, "混阶 (0,1) 被拒", r1.ok ? "unexpected ok" : r1.error);
    chk(has(r1.manifest, "\"error_kind\": \"input\""),
        "节点 manifest error_kind=input", r1.manifest.substr(0, 0));
    chk(has(r1.error, kNamed), "具名错误串");
    chk(has(r1.error, "distinct orders={0,1}"), "冲突 order 集合 {0,1}");
    chk(has(r1.error, "ord0_halpha") && has(r1.error, "ord1_halpha"),
        "含输入 path/frame_id");
    chk(!fs::exists(od_mix + "/p2_coverage.json"), "混阶不发布 coverage artifact");

    const std::string od_perm = scratch + "/a_mix10";
    NodeOutcome r2 = cov({d1, d0}, od_perm);
    chk(!r2.ok, "混阶排列互换 (1,0) 被拒");
    chk(!r2.ok && r2.error == r1.error,
        "诊断与输入排列无关（(0,1) 与 (1,0) 逐字相同）");
    chk(!fs::exists(od_perm + "/p2_coverage.json"), "排列互换后仍不发布 artifact");

    const std::string od_mix3 = scratch + "/a_mix010";
    NodeOutcome r3 = cov({d0, d1, d0r}, od_mix3);
    chk(!r3.ok && has(r3.error, "distinct orders={0,1}"),
        "三帧 (0,1,0) 被拒且集合 {0,1}");

    const std::string od_same = scratch + "/a_same00";
    NodeOutcome r4 = cov({d0, d0r}, od_same);
    chk(r4.ok, "同阶异滤镜 (0,0) 通过", r4.ok ? "" : r4.error);
    chk(fs::exists(od_same + "/p2_coverage.json"), "同阶发布 coverage artifact");

    const std::string od_same11 = scratch + "/a_same11";
    NodeOutcome r5 = cov({d1, d1}, od_same11);
    chk(r5.ok, "同阶 (1,1) 通过", r5.ok ? "" : r5.error);

    const std::string od_single = scratch + "/a_single";
    NodeOutcome r6 = cov({d1}, od_single);
    chk(r6.ok, "单帧 order=1 通过（无跨帧混合）");
  }

  // ── B. acsd.phase2.sample 消费端第二防线（crafted mixed artifact）────────
  std::printf("[B] acsd.phase2.sample 消费端第二防线\n");
  {
    const std::string od = scratch + "/b_sample_mixed";
    fs::create_directories(od);
    write_text(od + "/p2_coverage.json",
               mk_cov({{"/nonexistent/frame_a", 0}, {"/nonexistent/frame_b", 1}}).dump(2));
    Json doc{{"output_dir", od},
             {"hips_paths", Json::array({"/nonexistent/frame_a", "/nonexistent/frame_b"})}};
    NodeOutcome r = run_node("acsd.phase2.sample", doc.dump());
    chk(!r.ok, "混阶 artifact → sample 拒绝", r.ok ? "unexpected ok" : r.error);
    chk(has(r.error, kNamed) || has(r.manifest, kNamed), "sample 报具名错误");

    const std::string od2 = scratch + "/b_sample_same";
    fs::create_directories(od2);
    write_text(od2 + "/p2_coverage.json",
               mk_cov({{"/nonexistent/frame_a", 1}, {"/nonexistent/frame_b", 1}}).dump(2));
    Json doc2{{"output_dir", od2},
              {"hips_paths", Json::array({"/nonexistent/frame_a", "/nonexistent/frame_b"})}};
    NodeOutcome r2 = run_node("acsd.phase2.sample", doc2.dump());
    chk(!(has(r2.error, kNamed) || has(r2.manifest, kNamed)),
        "同阶 artifact 不被 order 门拒绝（后续因缺真实输入失败属预期）",
        r2.ok ? "ok" : r2.error);
  }

  // ── C. 另两处直接读 artifact 的入口（upm-apply / write）──────────────────
  std::printf("[C] acsd.phase2.upm-apply / acsd.phase2.write artifact 读取处\n");
  {
    // 最小合法 sky plane（p2_sky_plane_open 的 JSON 格式）；只为越过前置读。
    const std::string sky = R"({"format":"acsd-sky-plane-v1","ra0_deg":0.0,
"dec0_deg":0.0,"t0u":0.0,"t0v":0.0,"h":1.0,"degree":3,"delta_order":0,
"nx":1,"ny":1,"gauge_shift":0.0,"uc":0.0,"vc":0.0,"us":1.0,"vs":1.0,
"du_min":-1.0,"du_max":1.0,"dv_min":-1.0,"dv_max":1.0,
"coeff":[0.0],"frame_ids":[],"deltas":[]})";
    const std::string od = scratch + "/c_upm_apply";
    fs::create_directories(od);
    write_text(od + "/p2_sky_plane.bin", sky);
    write_text(od + "/p2_coverage.json",
               mk_cov({{"/nonexistent/fa", 0}, {"/nonexistent/fb", 1}}).dump(2));
    Json doc{{"output_dir", od},
             {"hips_paths", Json::array({"/nonexistent/fa", "/nonexistent/fb"})}};
    NodeOutcome r = run_node("acsd.phase2.upm-apply", doc.dump());
    chk(!r.ok && (has(r.error, kNamed) || has(r.manifest, kNamed)),
        "upm-apply 混阶 artifact 报具名错误",
        r.ok ? "unexpected ok" : r.error);

    const std::string od2 = scratch + "/c_write";
    fs::create_directories(od2);
    write_text(od2 + "/p2_integrated.json", "{}");
    write_text(od2 + "/p2_coverage.json",
               mk_cov({{"/nonexistent/fa", 0}, {"/nonexistent/fb", 1}}).dump(2));
    Json doc2{{"output_dir", od2},
              {"hips_paths", Json::array({"/nonexistent/fa", "/nonexistent/fb"})}};
    NodeOutcome r2 = run_node("acsd.phase2.write", doc2.dump());
    chk(!r2.ok && (has(r2.error, kNamed) || has(r2.manifest, kNamed)),
        "write 混阶 artifact 报具名错误（无 complete 发布）",
        r2.ok ? "unexpected ok" : r2.error);
  }

  std::printf("== 汇总: PASS=%d FAIL=%d SKIP=%d ==\n", g_pass, g_fail, g_skip);
  return g_fail == 0 ? 0 : 1;
}
