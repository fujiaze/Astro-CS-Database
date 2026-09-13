// RT-008 CLI Runtime client 实现：preset→IR→Runtime 唯一执行路径。
#include "runtime_client.h"

#include "astrocs/core/context.h"
#include "cancel_token.h"
#include "p3_wcs.h"   // B2-A4/A5: 请求层投影/frame/coverage_output 唯一校验源

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <thread>

namespace astrocs::cli {

using astrocs::core::ModuleRegistry;
using astrocs::core::Result;

astrocs::core::Result<void> register_cli_modules(ModuleRegistry& reg) {
  return astrocs::core::register_phase_modules(reg);
}

namespace {

// 从 run config 提取 phase 子配置（与旧 cmd_run_pipeline 语义一致）
// 兼容两种输入：run 格式（顶层 inputs.lights）与 phase 格式（input_lights/hips_paths 直接可用）。
// 直通(passthrough)模式不自动补 output_dir——session validate 负责拒绝缺失项
// （缺 output_dir → PARAM → CLI 2，与旧 CLI 行为一致）。
nlohmann::json phase_config(const nlohmann::json& doc, int phase,
                            const std::string& out_dir, std::string* err) {
  // B1-A4: 不再"重建 pdoc"（旧实现只搬 hips_paths/input_lights, 静默丢弃
  // cosmetic/weight_mode/reject_profile 等会话键 → 节点消费不到）。改为以完整
  // doc 为基, 只在 V1 `inputs.lights` 形态做必要映射; 直通形态零改动。
  nlohmann::json pdoc = doc;
  if (phase == 3) {
    if (doc.contains("phase3") && doc["phase3"].is_object()) {
      pdoc = doc["phase3"];
      pdoc["output_dir"] = out_dir;
    } else if (doc.contains("output_fits_path") || doc.contains("sampler_used") ||
               doc.contains("mode") || doc.contains("source") || doc.contains("projection") ||
               doc.contains("coverage_output")) {
      pdoc = doc;  // phase3 格式 config 直通；不自动补 output_dir
    } else {
      if (err) *err = "run config missing 'phase3' object for --phases 3";
      return {};
    }
    // B2-A4/A5: projection/frame/coverage_output 在此 config 面 fail-closed ——
    // validate/plan/run 三面共用本函数, 故三面一致拒绝非法值; 唯一语义源 =
    // astrocs::phase3::p3_wcs_validate_request(未实现投影不得静默映射为 TAN)。
    {
      for (const char* k : {"projection", "frame", "coverage_output"}) {
        if (pdoc.contains(k) && !pdoc[k].is_string()) {
          if (err) *err = std::string("phase3 ") + k + " must be string";
          return {};
        }
      }
      const std::string proj = pdoc.value("projection", std::string("TAN"));
      const std::string frame = pdoc.value("frame", std::string("icrs"));
      const std::string cov = pdoc.value("coverage_output", std::string("mask"));
      std::string why;
      const astrocs::phase3::P3WcsStatus pst = astrocs::phase3::p3_wcs_validate_request(
          pdoc.contains("projection") ? proj.c_str() : nullptr,
          pdoc.contains("frame") ? frame.c_str() : nullptr,
          pdoc.contains("coverage_output") ? cov.c_str() : nullptr, &why);
      if (pst != astrocs::phase3::P3_WCS_OK) {
        if (err) *err = std::string("phase3 config rejected: ") + why;
        return {};
      }
    }
  } else if (phase == 2) {
    if (doc.contains("inputs") && doc["inputs"].is_object() &&
        doc["inputs"].contains("lights")) {
      pdoc["hips_paths"] = doc["inputs"]["lights"];
      pdoc["output_dir"] = out_dir;
    } else if (doc.contains("hips_paths")) {
      pdoc = doc;  // phase2 格式 config 直通；不自动补 output_dir
    } else {
      if (err) *err = "run config missing 'inputs.lights' for --phases 2";
      return {};
    }
  } else {
    if (doc.contains("inputs") && doc["inputs"].is_object() &&
        doc["inputs"].contains("lights")) {
      pdoc["input_lights"] = doc["inputs"]["lights"];
      pdoc["output_dir"] = out_dir;
    } else if (doc.contains("input_lights")) {
      pdoc = doc;  // phase1 格式 config 直通；不自动补 output_dir
    } else {
      if (err) *err = "run config missing 'inputs.lights' for --phases 1";
      return {};
    }
  }
  return pdoc;
}

}  // namespace

std::string build_pipeline_ir(const std::vector<int>& phases,
                              const std::string& config_json, std::string* err) {
  if (err) err->clear();  // 每次调用重置，避免前次失败残留
  nlohmann::json doc;
  try {
    doc = nlohmann::json::parse(config_json);
  } catch (const std::exception& e) {
    if (err) *err = std::string("config JSON parse: ") + e.what();
    return "";
  }
  const std::string out_dir = doc.value("output_dir", std::string("."));
  nlohmann::json ir;
  ir["schema"] = "astrocs.pipeline/v1";
  ir["pipeline_id"] = "cli.run.preset";
  ir["version"] = "1.0.0";
  ir["nodes"] = nlohmann::json::array();
  nlohmann::json outs = nlohmann::json::object();

  // P1-001 (attempt 2) / FIX-E2E B1-A1: Canonical Phase1 IR 8 节点链
  // cal → cos → psf → wcs → phot → snr → drz → wr。
  // 节点集/端口名与 core module_adapters 的 descriptor 及
  // runtime/pipeline/module_ports.registry.json 的 phase1 冻结端口链逐节点一致
  // （GAP-10: 新增门 tests/cli/test_phase1_inprocess.py::test_ir_matches_frozen_chain
  // 断言该一致性, 防再次静默漂移到 2 节点）。
  // 平台说明(B1-A1 风险): Linux 上 ipv 求解为源内 stub, 故 wcs 节点走"显式 WCS
  // 配置"路径(见 p1_op_wcs 的 explicit_config 分支), drizzle 从 wcs 节点产物
  // p1_wcs.json 透传 header KV —— 不伪造求解, manifest 记 wcs_source=explicit_config。
  auto phase1_nodes = [&]() -> std::vector<nlohmann::json> {
    nlohmann::json pc = phase_config(doc, 1, out_dir, err);
    if (err && !err->empty()) return {};
    auto mk = [&](const std::string& nid, const std::string& mod,
                  const nlohmann::json& ins, const nlohmann::json& outs_,
                  const char* cls, bool par) {
      nlohmann::json n;
      n["node_id"] = nid;
      n["module_id"] = mod;
      n["module_api"] = "1.x";
      n["config"] = pc;
      n["inputs"] = ins;
      n["outputs"] = outs_;
      n["resources"] = {{"class", cls}, {"parallel", par}};
      return n;
    };
    return {
        mk("cal", "astrocs.phase1.calibration",
           {{"frames", "artifact:in"}}, {{"calibrated", "artifact:cal"}},
           "cpu_heavy", true),
        mk("cos", "astrocs.phase1.cosmetic",
           {{"calibrated", "artifact:cal"}}, {{"cleaned", "artifact:cos"}},
           "cpu_heavy", true),
        mk("psf", "astrocs.phase1.star-psf",
           {{"cleaned", "artifact:cos"}},
           {{"sources", "artifact:p1_sources"}, {"psf", "artifact:p1_psf"}},
           "cpu_heavy", true),
        mk("wcs", "astrocs.phase1.wcs-platesolve",
           {{"sources", "artifact:p1_sources"}}, {{"wcs", "artifact:p1_wcs"}},
           "cpu_heavy", true),
        mk("phot", "astrocs.phase1.photometry",
           {{"psf", "artifact:p1_psf"}, {"sources", "artifact:p1_sources"}},
           {{"fluxes", "artifact:p1_flux"}},
           "cpu_heavy", true),
        mk("snr", "astrocs.phase1.noise-snr",
           {{"fluxes", "artifact:p1_flux"}}, {{"snr", "artifact:p1_snr"}},
           "cpu_heavy", true),
        mk("drz", "astrocs.phase1.drizzle",
           {{"calibrated", "artifact:cal"}}, {{"stacked", "artifact:p1_stack"}},
           "cpu_heavy", true),
        mk("wr", "astrocs.phase1.writer",
           {{"stacked", "artifact:p1_stack"}}, {{"fits", "artifact:p1_hips"}},
           "io", false),
    };
  };
  // P2-006 (G5): Canonical Phase2 IR 7 节点链
  // coverage → sample → upm_fit → upm_apply → reject → integrate → write。
  // 端口名与 core module_adapters 的 descriptor 端口一致(DATA/单位/Artifact ID)。
  auto phase2_nodes = [&]() -> std::vector<nlohmann::json> {
    nlohmann::json pc = phase_config(doc, 2, out_dir, err);
    if (err && !err->empty()) return {};
    // node_id, module_id, input 端口, output 端口
    const std::vector<std::tuple<std::string, std::string, std::string, std::string>> chain = {
        {"coverage", "astrocs.phase2.coverage", "calibrated", "coverage"},
        {"sample", "astrocs.phase2.sample", "coverage", "samples"},
        {"upm_fit", "astrocs.phase2.upm-fit", "samples", "upm_model"},
        {"upm_apply", "astrocs.phase2.upm-apply", "upm_model", "corrected"},
        {"reject", "astrocs.phase2.reject", "corrected", "accepted_mask"},
        {"integrate", "astrocs.phase2.integrate", "accepted_mask", "integrated"},
        {"write", "astrocs.phase2.write", "integrated", "mosaic"},
    };
    std::vector<nlohmann::json> nodes;
    std::string prev = "artifact:cal";
    for (const auto& [nid, mod, in_port, out_port] : chain) {
      nlohmann::json n;
      n["node_id"] = nid;
      n["module_id"] = mod;
      n["module_api"] = "1.x";
      n["config"] = pc;
      n["inputs"] = {{in_port, prev}};
      n["outputs"] = {{out_port, "artifact:" + nid}};
      n["resources"] = {{"class", "cpu_heavy"}, {"parallel", true}};
      if (nid == "write") n["resources"] = {{"class", "io"}, {"parallel", false}};
      nodes.push_back(n);
      prev = "artifact:" + nid;
    }
    return nodes;
  };
  // P3-006 (G6): Canonical Phase3 IR 链
  // source(artifact:hips_in) → properties → wcs → resample2 → writer → verify。
  // 端口名与 core module_adapters 的 descriptor 端口一致。
  auto phase3_nodes = [&]() -> std::vector<nlohmann::json> {
    nlohmann::json pc = phase_config(doc, 3, out_dir, err);
    if (err && !err->empty()) return {};
    const std::vector<std::tuple<std::string, std::string, std::string, std::string>> chain = {
        {"properties", "astrocs.phase3.properties", "hips", "props"},
        {"wcs", "astrocs.phase3.wcs", "props", "wcs_plan"},
        {"resample2", "astrocs.phase3.resample2", "wcs_plan", "resampled"},
        {"writer", "astrocs.phase3.writer", "resampled", "fits"},
        {"verify", "astrocs.phase3.verify", "fits", "verified"},
    };
    std::vector<nlohmann::json> nodes;
    std::string prev = "artifact:hips_in";
    for (const auto& [nid, mod, in_port, out_port] : chain) {
      nlohmann::json n;
      n["node_id"] = nid;
      n["module_id"] = mod;
      n["module_api"] = "1.x";
      n["config"] = pc;
      n["inputs"] = {{in_port, prev}};
      n["outputs"] = {{out_port, "artifact:" + nid}};
      n["resources"] = {{"class", "cpu_heavy"}, {"parallel", true}};
      if (nid == "writer") n["resources"] = {{"class", "io"}, {"parallel", false}};
      nodes.push_back(n);
      prev = "artifact:" + nid;
    }
    return nodes;
  };

  bool want1 = false, want2 = false, want3 = false;
  for (int ph : phases) {
    if (ph == 1) want1 = true;
    if (ph == 2) want2 = true;
    if (ph == 3) want3 = true;
  }
  if (want1) {
    for (auto& n : phase1_nodes()) ir["nodes"].push_back(n);
  }
  if (want2) {
    for (auto& n : phase2_nodes()) ir["nodes"].push_back(n);
  }
  if (want3) {
    for (auto& n : phase3_nodes()) ir["nodes"].push_back(n);
  }
  if (err && !err->empty()) return "";  // 任一 phase config 缺失 → 整体失败
  if (ir["nodes"].empty()) {
    if (err) *err = "no phases requested";
    return "";
  }
  if (want2) outs["mosaic"] = "artifact:write";
  if (want3) outs["verified"] = "artifact:verify";
  if (want1) {
    // B1-A1: phase1 内部产物 p1_wcs/p1_snr/p1_hips 无下游 IR 消费者（wcs 产物
    // 经 output_dir 文件约定由 drizzle 透传, 不声明为 IR edge），必须显式登记为
    // pipeline 输出以满足 IR 静态验证 UNCONSUMED；cal/cleaned 保留既有语义。
    outs["calibrated"] = "artifact:cal";
    outs["cleaned"] = "artifact:cos";
    outs["wcs"] = "artifact:p1_wcs";
    outs["snr"] = "artifact:p1_snr";
    outs["hips"] = "artifact:p1_hips";
  }
  ir["outputs"] = outs;
  return ir.dump();
}

namespace {

// RT-008: 最近一次 run_pipeline 的 Runtime 节点 manifest（供 CLI 收集 artifact）
std::mutex g_man_mu;
std::vector<std::pair<std::string, std::string>> g_manifests;

// RT-009: 最近一次 run_pipeline 的节点 trace + PipelineIR（供 observed graph）
std::mutex g_tr_mu;
std::vector<astrocs::core::Runtime::NodeTrace> g_trace;
std::string g_ir_json;

}  // namespace

// RT-009: 收集节点 trace
void collect_node_trace(std::vector<astrocs::core::Runtime::NodeTrace>* out) {
  std::lock_guard<std::mutex> lock(g_tr_mu);
  if (out) *out = g_trace;
}

// RT-009: 最近一次 run 的 PipelineIR JSON（静态图来源）
const std::string& last_pipeline_ir_json() {
  return g_ir_json;
}

int run_pipeline(const std::vector<int>& phases, const std::string& config_json,
                 uint32_t budget, std::string* fail_reason,
                 std::atomic<bool>* cancel_ext) {
  // MON-002 测试钩子(非用户接口): 假 workload(低 CPU 睡眠)供资源门禁 first-10s/
  // RESOURCE(10) 端到端验证; 不设环境变量时零影响。循环响应信号与外部取消源,
  // 保证 gate 快速失败后本钩子立即让路协作取消。
  if (const char* sl = std::getenv("ASTROCS_TEST_PIPELINE_SLEEP_MS")) {
    const long ms = std::strtol(sl, nullptr, 10);
    if (ms > 0) {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
      while (std::chrono::steady_clock::now() < deadline) {
        if (astrocs::is_cancelled() ||
            (cancel_ext && cancel_ext->load(std::memory_order_relaxed)))
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }
  }
  astrocs::core::Result<void> rt_ret;   // cancel 监视线程作用域外保存 run 结果
  ModuleRegistry reg;
  auto rr = register_cli_modules(reg);
  if (rr.failed()) {
    if (fail_reason) *fail_reason = rr.error().message();
    return 70;
  }
  auto rt = astrocs::core::create_runtime(budget);
  if (rt.failed()) {
    if (fail_reason) *fail_reason = rt.error().message();
    return 70;
  }
  std::string err;
  const std::string ir_json = build_pipeline_ir(phases, config_json, &err);
  if (ir_json.empty()) {
    if (fail_reason) *fail_reason = err;
    return 2;
  }
  auto load = rt.value()->load_pipeline(ir_json, reg);
  if (load.failed()) {
    if (fail_reason) *fail_reason = load.error().message();
    return 4;  // 静态验证失败 → 科学/配置错误
  }
  astrocs::core::RunContext ctx;
  // QA-002/LNX-004: cancel 接线 — SIGINT/SIGTERM 置位 CLI cancel_flag 后，
  // 本监视线程轮询并转发到 Runtime::cancel()（scheduler 安全点协作取消）。
  // 此前 cancel_flag 无人消费 → 信号不达 runtime（真实缺陷，本修复闭合）。
  {
    std::atomic<bool> stop{false};
    std::thread cancel_watch([rt = rt.value().get(), &stop, cancel_ext]() {
      while (!stop.load(std::memory_order_acquire)) {
        if (astrocs::is_cancelled() ||
            (cancel_ext && cancel_ext->load(std::memory_order_relaxed))) {
          rt->cancel();
          return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    });
    rt_ret = rt.value()->run(ctx);
    stop.store(true, std::memory_order_release);
    cancel_watch.join();
  }
  // RT-008: 保留最近一次 Runtime 实例的节点 manifest，供 collect_node_manifests 读取
  {
    std::lock_guard<std::mutex> lock(g_man_mu);
    g_manifests = rt.value()->node_manifests();
  }
  // RT-009: 保留节点 trace 与 PipelineIR（observed graph 数据源）
  {
    std::lock_guard<std::mutex> lock(g_tr_mu);
    g_trace = rt.value()->node_trace();
    g_ir_json = ir_json;
  }
  if (rt_ret.failed()) {
    if (fail_reason) *fail_reason = rt_ret.error().message();
    // RT-008: 退出码映射保持 CLI 合同（04）:
    //   失败 manifest 的 error_kind==input → 3(INPUT)；DATA(参数/配置/数据) → 2(ARGS)；
    //   IO → 7；CANCELLED → 9；RESOURCE → 5；其余 → 70
    // 先看失败节点 manifest 是否带 error_kind
    if (rt_ret.error().domain() == astrocs::core::ErrorDomain::DATA ||
        rt_ret.error().domain() == astrocs::core::ErrorDomain::IO) {
      bool input_err = false;
      {
        std::lock_guard<std::mutex> lock(g_man_mu);
        for (const auto& [nid, mtext] : g_manifests) {
          try {
            auto m = nlohmann::json::parse(mtext);
            if (m.value("error_kind", std::string()) == "input") { input_err = true; break; }
          } catch (...) {}
        }
      }
      if (input_err) return 3;
    }
    switch (rt_ret.error().domain()) {
      case astrocs::core::ErrorDomain::DATA: return 2;
      case astrocs::core::ErrorDomain::IO: return 7;
      case astrocs::core::ErrorDomain::CANCELLED: return 9;
      case astrocs::core::ErrorDomain::RESOURCE: return 5;
      default: return 70;
    }
  }
  return 0;
}

void collect_node_manifests(std::vector<std::pair<std::string, std::string>>* out) {
  std::lock_guard<std::mutex> lock(g_man_mu);
  if (out) *out = g_manifests;
}

std::vector<std::string> collect_node_artifact_paths(
    const std::vector<std::pair<std::string, std::string>>& manifests) {
  std::vector<std::string> paths;
  std::set<std::string> seen;
  for (const auto& [nid, mtext] : manifests) {
    nlohmann::json m;
    try { m = nlohmann::json::parse(mtext); } catch (...) { continue; }
    if (!m.is_object()) continue;
    for (const auto& a : m.value("artifacts", nlohmann::json::array())) {
      if (!a.is_string()) continue;
      const std::string ap = a.get<std::string>();
      if (seen.insert(ap).second) paths.push_back(ap);
    }
  }
  return paths;
}

}  // namespace astrocs::cli
