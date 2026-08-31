// CORE-004 / RT-004: Pipeline IR 解析 + 静态验证
// 使用 nlohmann::json（真正 JSON parser），schema 驱动校验，接受完整 ModuleRegistry。
#include "astrocs/core/pipeline.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <functional>
#include <set>
#include <sstream>

namespace astrocs::core {

using nlohmann::json;

namespace {

// 从 JSON 值读取扁平字符串映射（port -> artifact）
std::map<std::string, std::string> str_map(const json& obj) {
  std::map<std::string, std::string> out;
  if (!obj.is_object()) return out;
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    if (it.value().is_string()) out[it.key()] = it.value().get<std::string>();
  }
  return out;
}

bool is_artifact_ref(const std::string& s) {
  return s.rfind("artifact:", 0) == 0;
}

}  // namespace

std::string PipelineIR::to_json() const {
  json j;
  j["schema"] = schema.empty() ? "astrocs.pipeline/v1" : schema;
  j["pipeline_id"] = pipeline_id;
  j["version"] = version;
  j["nodes"] = json::array();
  for (const auto& n : nodes) {
    json nj;
    nj["node_id"] = n.node_id;
    nj["module_id"] = n.module_id;
    if (!n.module_api.empty()) nj["module_api"] = n.module_api;
    nj["inputs"] = n.inputs;
    nj["outputs"] = n.outputs;
    nj["resources"] = {{"class", n.resource_class}, {"parallel", n.parallel}};
    if (!n.config_json.empty()) nj["config"] = json::parse(n.config_json, nullptr, false);
    j["nodes"].push_back(std::move(nj));
  }
  j["outputs"] = outputs;
  return j.dump();
}

Result<PipelineIR> PipelineIRParser::parse(const std::string& json_text) const {
  json doc;
  try {
    doc = json::parse(json_text);
  } catch (const json::parse_error& e) {
    return Result<PipelineIR>::fail(
        Error(ErrorDomain::DATA, std::string("pipeline JSON parse error: ") + e.what()));
  }
  if (!doc.is_object()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "pipeline JSON must be object"));
  }

  // schema 字段
  auto it_schema = doc.find("schema");
  if (it_schema == doc.end() || !it_schema->is_string()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "schema required"));
  }
  const std::string schema = it_schema->get<std::string>();
  if (schema != "astrocs.pipeline/v1" && schema != "astrocs.pipeline/v2") {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
        "schema must be astrocs.pipeline/v1 or /v2, got " + schema));
  }

  PipelineIR ir;
  ir.schema = schema;
  auto get_str = [&](const char* key, std::string* out) -> bool {
    auto it = doc.find(key);
    if (it == doc.end() || !it->is_string()) return false;
    *out = it->get<std::string>();
    return true;
  };
  if (!get_str("pipeline_id", &ir.pipeline_id) || ir.pipeline_id.empty()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "pipeline_id required"));
  }
  if (!get_str("version", &ir.version) || ir.version.empty()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "version required"));
  }

  auto it_nodes = doc.find("nodes");
  if (it_nodes == doc.end() || !it_nodes->is_array() || it_nodes->empty()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
        "nodes array required and non-empty"));
  }

  for (const auto& nj : *it_nodes) {
    PipelineNode n;
    if (!nj.is_object()) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "node must be object"));
    }
    auto git = nj.find("node_id");
    if (git == nj.end() || !git->is_string()) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "node missing node_id"));
    }
    n.node_id = git->get<std::string>();
    auto mit = nj.find("module_id");
    if (mit == nj.end() || !mit->is_string()) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
          "node " + n.node_id + " missing module_id"));
    }
    n.module_id = mit->get<std::string>();
    auto ait = nj.find("module_api");
    if (ait != nj.end() && ait->is_string()) n.module_api = ait->get<std::string>();

    // inputs/outputs: port -> artifact
    n.inputs = str_map(nj.value("inputs", json::object()));
    n.outputs = str_map(nj.value("outputs", json::object()));
    if (n.inputs.empty() || n.outputs.empty()) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
          "node " + n.node_id + " requires non-empty inputs and outputs"));
    }
    for (const auto& [port, art] : n.inputs) {
      if (!is_artifact_ref(art)) {
        return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
            "node " + n.node_id + " input " + port + " not artifact ref: " + art));
      }
    }
    for (const auto& [port, art] : n.outputs) {
      if (!is_artifact_ref(art)) {
        return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
            "node " + n.node_id + " output " + port + " not artifact ref: " + art));
      }
    }

    // resources
    auto rit = nj.find("resources");
    if (rit == nj.end() || !rit->is_object()) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
          "node " + n.node_id + " missing resources"));
    }
    auto cit = rit->find("class");
    if (cit == rit->end() || !cit->is_string()) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
          "node " + n.node_id + " missing resources.class"));
    }
    n.resource_class = cit->get<std::string>();
    if (n.resource_class != "metadata" && n.resource_class != "io" &&
        n.resource_class != "cpu_light" && n.resource_class != "cpu_heavy") {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
          "node " + n.node_id + " invalid resources.class: " + n.resource_class));
    }
    auto pit = rit->find("parallel");
    n.parallel = pit != rit->end() && pit->is_boolean() && pit->get<bool>();
    // cpu_heavy 必须 parallel（schema allOf）
    if (n.resource_class == "cpu_heavy" && !n.parallel) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
          "node " + n.node_id + ": cpu_heavy must be parallel=true"));
    }

    // config: inline JSON 或 config_ref {schema, sha256}
    auto cit_inline = nj.find("config");
    if (cit_inline != nj.end() && cit_inline->is_object()) {
      n.config_json = cit_inline->dump();
    }
    auto cref = nj.find("config_ref");
    if (cref != nj.end() && cref->is_object()) {
      auto cs = cref->find("schema");
      if (cs != cref->end() && cs->is_string()) n.config_ref_schema = cs->get<std::string>();
      auto ch = cref->find("sha256");
      if (ch != cref->end() && ch->is_string()) n.config_ref_sha256 = ch->get<std::string>();
    }

    ir.nodes.push_back(std::move(n));
  }

  // pipeline 顶层 outputs
  auto oit = doc.find("outputs");
  if (oit == doc.end() || !oit->is_object()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "pipeline outputs required"));
  }
  ir.outputs = str_map(*oit);
  if (ir.outputs.empty()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "pipeline outputs empty"));
  }

  return Result<PipelineIR>::ok(std::move(ir));
}

std::vector<IrIssue> PipelineIRParser::validate(
    const PipelineIR& ir, const ModuleRegistry& registry) const {
  std::vector<IrIssue> issues;
  std::set<std::string> produced_artifacts;
  std::set<std::string> consumed_artifacts;
  std::map<std::string, std::string> producer_of;  // artifact -> node_id
  std::map<std::string, std::string> producer_port_of;  // artifact -> port
  std::map<std::string, std::vector<std::string>> consumers_of;
  std::map<std::string, std::string> node_by_id;

  for (const auto& n : ir.nodes) {
    node_by_id[n.node_id] = n.module_id;
    const ModuleDescriptor* desc = registry.find(n.module_id);
    if (desc == nullptr) {
      issues.push_back({IrError::UNKNOWN_MODULE, n.node_id,
                        "module not registered: " + n.module_id});
      continue;  // 无描述符无法继续端口级检查
    }
    // 执行类一致性
    if (desc->execution_class == "cpu_heavy" && n.resource_class != "cpu_heavy") {
      issues.push_back({IrError::SERIAL_HEAVY, n.node_id,
                        "module declares cpu_heavy but node resources.class=" +
                            n.resource_class});
    }
    // 端口检查: 输入/输出端口必须存在于 descriptor
    std::map<std::string, PortDescriptor> in_ports, out_ports;
    for (const auto& p : desc->ports) {
      if (p.is_input) in_ports[p.name] = p;
      else out_ports[p.name] = p;
    }
    for (const auto& [port, art] : n.inputs) {
      auto pit = in_ports.find(port);
      if (pit == in_ports.end()) {
        issues.push_back({IrError::MISSING_PORT, n.node_id,
                          "input port '" + port + "' not in module " + n.module_id});
      }
      consumed_artifacts.insert(art);
      consumers_of[art].push_back(n.node_id);
    }
    for (const auto& [port, art] : n.outputs) {
      auto pit = out_ports.find(port);
      if (pit == out_ports.end()) {
        issues.push_back({IrError::MISSING_PORT, n.node_id,
                          "output port '" + port + "' not in module " + n.module_id});
      }
      if (producer_of.count(art)) {
        issues.push_back({IrError::DUPLICATE_PRODUCER, n.node_id,
                          "artifact " + art + " already produced by " + producer_of[art]});
      }
      producer_of[art] = n.node_id;
      producer_port_of[art] = port;
      produced_artifacts.insert(art);
    }
  }

  // 端口级 DATA schema / unit / coordinate 冲突（在已注册端口之间）
  for (const auto& [art, producer_node] : producer_of) {
    const PipelineNode* pn = nullptr;
    for (const auto& n : ir.nodes) if (n.node_id == producer_node) { pn = &n; break; }
    if (!pn) continue;
    const ModuleDescriptor* pd = registry.find(pn->module_id);
    if (!pd) continue;
    PortDescriptor prod_port;
    bool have_prod = false;
    for (const auto& p : pd->ports) {
      if (!p.is_input && pn->outputs.count(p.name) &&
          pn->outputs.at(p.name) == art) {
        prod_port = p;
        have_prod = true;
        break;
      }
    }
    if (!have_prod) continue;
    for (const auto& cons_node : consumers_of[art]) {
      const PipelineNode* cn = nullptr;
      for (const auto& n : ir.nodes) if (n.node_id == cons_node) { cn = &n; break; }
      if (!cn) continue;
      const ModuleDescriptor* cd = registry.find(cn->module_id);
      if (!cd) continue;
      for (const auto& [port, c_art] : cn->inputs) {
        if (c_art != art) continue;
        for (const auto& p : cd->ports) {
          if (p.is_input && p.name == port) {
            if (prod_port.data_schema_id != p.data_schema_id &&
                !prod_port.data_schema_id.empty() && !p.data_schema_id.empty()) {
              issues.push_back({IrError::DATA_MISMATCH, cons_node,
                                "artifact " + art + " producer schema " +
                                    prod_port.data_schema_id + " != consumer " +
                                    p.data_schema_id});
            }
            if (prod_port.unit != p.unit &&
                prod_port.unit != UnitId::UNKNOWN && p.unit != UnitId::UNKNOWN) {
              issues.push_back({IrError::UNIT_MISMATCH, cons_node,
                                "artifact " + art + " producer unit " +
                                    unit_name(prod_port.unit) + " != consumer " +
                                    unit_name(p.unit)});
            }
            if (prod_port.coordinate != p.coordinate) {
              issues.push_back({IrError::COORDINATE_MISMATCH, cons_node,
                                "artifact " + art + " producer coord mismatch"});
            }
          }
        }
      }
    }
  }

  // 环检测（artifact producer 依赖）
  std::map<std::string, std::vector<std::string>> deps;
  for (const auto& n : ir.nodes) {
    for (const auto& [port, art] : n.inputs) {
      auto it = producer_of.find(art);
      if (it != producer_of.end()) deps[n.node_id].push_back(it->second);
    }
  }
  std::set<std::string> visiting, done;
  std::function<bool(const std::string&)> dfs = [&](const std::string& id) -> bool {
    if (done.count(id)) return false;
    if (visiting.count(id)) return true;
    visiting.insert(id);
    for (const auto& d : deps[id]) {
      if (dfs(d)) return true;
    }
    visiting.erase(id);
    done.insert(id);
    return false;
  };
  for (const auto& [id, m] : node_by_id) {
    if (dfs(id)) {
      issues.push_back({IrError::CYCLE, id, "dependency cycle detected"});
      break;
    }
  }

  // UNCONSUMED: 内部产物被产出但从未消费（除 pipeline outputs 本身）
  for (const auto& art : produced_artifacts) {
    bool is_pipeline_output = false;
    for (const auto& [port, oa] : ir.outputs) {
      if (oa == art) { is_pipeline_output = true; break; }
    }
    if (!is_pipeline_output && !consumed_artifacts.count(art)) {
      issues.push_back({IrError::UNCONSUMED, producer_of[art],
                        "produced but never consumed: " + art});
    }
  }

  // UNPRODUCED_OUTPUT: pipeline outputs 必须被产出
  for (const auto& [port, art] : ir.outputs) {
    if (!produced_artifacts.count(art)) {
      issues.push_back({IrError::UNPRODUCED_OUTPUT, "",
                        "pipeline output artifact not produced: " + art});
    }
  }

  return issues;
}

}  // namespace astrocs::core
