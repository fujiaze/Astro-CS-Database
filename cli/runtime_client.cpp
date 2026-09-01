// RT-008 CLI Runtime client 实现：preset→IR→Runtime 唯一执行路径。
#include "runtime_client.h"

#include "astrocs/core/context.h"

#include <nlohmann/json.hpp>

#include <cstdio>

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
  nlohmann::json pdoc;
  if (phase == 3) {
    if (doc.contains("phase3") && doc["phase3"].is_object()) {
      pdoc = doc["phase3"];
      pdoc["output_dir"] = out_dir;
    } else if (doc.contains("output_fits_path") || doc.contains("sampler_used") ||
               doc.contains("mode") || doc.contains("source") || doc.contains("projection") ||
               doc.contains("coverage_output")) {
      pdoc = doc;  // phase3 格式 config 直通；不自动补 output_dir
    } else {
      *err = "run config missing 'phase3' object for --phases 3";
      return {};
    }
  } else if (phase == 2) {
    if (doc.contains("inputs") && doc["inputs"].is_object() &&
        doc["inputs"].contains("lights")) {
      pdoc["hips_paths"] = doc["inputs"]["lights"];
      pdoc["output_dir"] = out_dir;
    } else if (doc.contains("hips_paths")) {
      pdoc = doc;  // phase2 格式 config 直通；不自动补 output_dir
    } else {
      *err = "run config missing 'inputs.lights' for --phases 2";
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
      *err = "run config missing 'inputs.lights' for --phases 1";
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

  auto phase1_node = [&]() {
    nlohmann::json n;
    n["node_id"] = "cal";
    n["module_id"] = "astrocs.phase1.calibration";
    n["module_api"] = "1.x";
    nlohmann::json pc = phase_config(doc, 1, out_dir, err);
    if (!err->empty()) return nlohmann::json();
    n["config"] = pc;
    n["inputs"] = {{"frames", "artifact:in"}};
    n["outputs"] = {{"calibrated", "artifact:cal"}};
    n["resources"] = {{"class", "cpu_heavy"}, {"parallel", true}};
    return n;
  };
  // P2-006 (G5): Canonical Phase2 IR 7 节点链
  // coverage → sample → upm_fit → upm_apply → reject → integrate → write。
  // 端口名与 core module_adapters 的 descriptor 端口一致(DATA/单位/Artifact ID)。
  auto phase2_nodes = [&]() -> std::vector<nlohmann::json> {
    nlohmann::json pc = phase_config(doc, 2, out_dir, err);
    if (!err->empty()) return {};
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
  auto phase3_node = [&]() {
    nlohmann::json n;
    n["node_id"] = "hips";
    n["module_id"] = "astrocs.phase3.resample";
    n["module_api"] = "1.x";
    nlohmann::json pc = phase_config(doc, 3, out_dir, err);
    if (!err->empty()) return nlohmann::json();
    n["config"] = pc;
    n["inputs"] = {{"hips", "artifact:hips_in"}};
    n["outputs"] = {{"tile", "artifact:tile"}};
    n["resources"] = {{"class", "cpu_heavy"}, {"parallel", true}};
    return n;
  };

  bool want1 = false, want2 = false, want3 = false;
  for (int ph : phases) {
    if (ph == 1) want1 = true;
    if (ph == 2) want2 = true;
    if (ph == 3) want3 = true;
  }
  if (want1) ir["nodes"].push_back(phase1_node());
  if (want2) {
    for (auto& n : phase2_nodes()) ir["nodes"].push_back(n);
  }
  if (want3) ir["nodes"].push_back(phase3_node());
  if (!err->empty()) return "";  // 任一 phase config 缺失 → 整体失败
  if (ir["nodes"].empty()) {
    if (err) *err = "no phases requested";
    return "";
  }
  if (want2) outs["mosaic"] = "artifact:write";
  if (want3) outs["tile"] = "artifact:tile";
  if (want1 && !want2 && !want3) outs["calibrated"] = "artifact:cal";
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
                 uint32_t budget, std::string* fail_reason) {
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
  auto r = rt.value()->run(ctx);
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
  if (r.failed()) {
    if (fail_reason) *fail_reason = r.error().message();
    // RT-008: 退出码映射保持 CLI 合同（04）:
    //   失败 manifest 的 error_kind==input → 3(INPUT)；DATA(参数/配置/数据) → 2(ARGS)；
    //   IO → 7；CANCELLED → 9；RESOURCE → 5；其余 → 70
    // 先看失败节点 manifest 是否带 error_kind
    if (r.error().domain() == astrocs::core::ErrorDomain::DATA ||
        r.error().domain() == astrocs::core::ErrorDomain::IO) {
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
    switch (r.error().domain()) {
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

}  // namespace astrocs::cli
