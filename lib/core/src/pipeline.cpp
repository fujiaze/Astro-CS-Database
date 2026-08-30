// CORE-004 Pipeline IR 解析 + 静态验证实现
#include "astrocs/core/pipeline.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <set>

namespace astrocs::core {

namespace {

// 最小 JSON 解析: 顶层键/节点数组提取 (不依赖外部库)
bool json_trim(std::string& s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return false;
  s = s.substr(b);
  size_t e = s.find_last_not_of(" \t\r\n");
  s = s.substr(0, e + 1);
  return !s.empty();
}

std::string top_key_value(const std::string& json, const char* key) {
  std::string pat = std::string("\"") + key + "\":";
  auto pos = json.find(pat);
  if (pos == std::string::npos) return "";
  pos += pat.size();
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
  if (pos >= json.size()) return "";
  char c = json[pos];
  if (c == '"') {
    auto end = json.find('"', pos + 1);
    return end == std::string::npos ? "" : json.substr(pos + 1, end - pos - 1);
  }
  // 数字/布尔
  auto end = json.find_first_of(",}]", pos);
  return end == std::string::npos ? json.substr(pos) : json.substr(pos, end - pos);
}

// 提取 nodes 数组的每个对象 (平衡花括号)
std::vector<std::string> extract_objects(const std::string& json, const char* key) {
  std::string pat = std::string("\"") + key + "\":";
  auto pos = json.find(pat);
  if (pos == std::string::npos) return {};
  pos += pat.size();
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) ++pos;
  if (pos >= json.size() || json[pos] != '[') return {};
  ++pos;
  std::vector<std::string> objs;
  int depth = 0;
  size_t start = std::string::npos;
  bool in_str = false;
  for (size_t i = pos; i < json.size(); ++i) {
    char c = json[i];
    if (in_str) {
      if (c == '"' && (i == 0 || json[i - 1] != '\\')) in_str = false;
      continue;
    }
    if (c == '"') { in_str = true; continue; }
    if (c == '{') { if (depth == 0) start = i; ++depth; }
    else if (c == '}') {
      --depth;
      if (depth == 0 && start != std::string::npos) {
        objs.push_back(json.substr(start, i - start + 1));
        start = std::string::npos;
      }
    }
    if (depth == 0 && c == ']') break;
  }
  return objs;
}

// 提取对象内 "k":"v" 字符串键值
std::string obj_key_str(const std::string& obj, const char* key) {
  return top_key_value(obj, key);
}

// 提取对象内 "k":{...} 子对象
std::string obj_key_obj(const std::string& obj, const char* key) {
  std::string pat = std::string("\"") + key + "\":";
  auto pos = obj.find(pat);
  if (pos == std::string::npos) return "";
  pos += pat.size();
  while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t' || obj[pos] == '\n' || obj[pos] == '\r')) ++pos;
  if (pos >= obj.size() || obj[pos] != '{') return "";
  int depth = 0;
  bool in_str = false;
  size_t start = pos;
  for (size_t i = start; i < obj.size(); ++i) {
    char c = obj[i];
    if (in_str) { if (c == '"' && (i == 0 || obj[i - 1] != '\\')) in_str = false; continue; }
    if (c == '"') { in_str = true; continue; }
    if (c == '{') ++depth;
    else if (c == '}') {
      --depth;
      if (depth == 0) return obj.substr(start, i - start + 1);
    }
  }
  return "";
}

// 提取扁平对象的所有 "k":"v" 对
std::map<std::string, std::string> obj_key_pairs(const std::string& obj) {
  std::map<std::string, std::string> out;
  size_t i = 0;
  while (i < obj.size()) {
    auto ks = obj.find('"', i);
    if (ks == std::string::npos) break;
    auto ke = obj.find('"', ks + 1);
    if (ke == std::string::npos) break;
    std::string k = obj.substr(ks + 1, ke - ks - 1);
    auto colon = obj.find(':', ke);
    if (colon == std::string::npos) break;
    size_t vs = colon + 1;
    while (vs < obj.size() && (obj[vs] == ' ' || obj[vs] == '\t')) ++vs;
    if (vs >= obj.size()) break;
    if (obj[vs] == '"') {
      auto ve = obj.find('"', vs + 1);
      if (ve == std::string::npos) break;
      out[k] = obj.substr(vs + 1, ve - vs - 1);
      i = ve + 1;
    } else if (obj[vs] == '{') {
      // 跳过子对象
      int d = 0; bool s = false;
      size_t j = vs;
      for (; j < obj.size(); ++j) {
        char c = obj[j];
        if (s) { if (c == '"' && obj[j-1] != '\\') s = false; continue; }
        if (c == '"') { s = true; continue; }
        if (c == '{') ++d;
        else if (c == '}') { --d; if (d == 0) break; }
      }
      i = j + 1;
    } else {
      auto ve = obj.find_first_of(",}", vs);
      if (ve == std::string::npos) break;
      out[k] = obj.substr(vs, ve - vs);
      i = ve;
    }
  }
  return out;
}

}  // namespace

std::string PipelineIR::to_json() const {
  std::string s = "{\"schema\":\"astrocs.pipeline/v1\",\"pipeline_id\":\"" + pipeline_id +
                  "\",\"version\":\"" + version + "\",\"nodes\":[";
  bool first = true;
  for (const auto& n : nodes) {
    if (!first) s += ",";
    first = false;
    s += "{\"node_id\":\"" + n.node_id + "\",\"module_id\":\"" + n.module_id + "\"}";
  }
  s += "]}";
  return s;
}

Result<PipelineIR> PipelineIRParser::parse(const std::string& json_text) const {
  std::string json = json_text;
  if (!json_trim(json) || json.empty()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "empty pipeline JSON"));
  }
  PipelineIR ir;
  if (top_key_value(json, "schema") != "astrocs.pipeline/v1") {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "schema must be astrocs.pipeline/v1"));
  }
  ir.pipeline_id = top_key_value(json, "pipeline_id");
  ir.version = top_key_value(json, "version");
  if (ir.pipeline_id.empty() || ir.version.empty()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "pipeline_id/version required"));
  }
  auto node_objs = extract_objects(json, "nodes");
  if (node_objs.empty()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "nodes array empty/missing"));
  }
  for (const auto& obj : node_objs) {
    PipelineNode n;
    n.node_id = obj_key_str(obj, "node_id");
    n.module_id = obj_key_str(obj, "module_id");
    n.module_api = obj_key_str(obj, "module_api");
    if (n.node_id.empty() || n.module_id.empty()) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "node missing node_id/module_id"));
    }
    std::string ins = obj_key_obj(obj, "inputs");
    std::string outs = obj_key_obj(obj, "outputs");
    n.inputs = obj_key_pairs(ins);
    n.outputs = obj_key_pairs(outs);
    if (n.inputs.empty() || n.outputs.empty()) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
          "node " + n.node_id + " missing inputs/outputs"));
    }
    std::string res = obj_key_obj(obj, "resources");
    auto res_pairs = obj_key_pairs(res);
    n.resource_class = res_pairs["class"];
    n.parallel = res_pairs["parallel"] == "true";
    if (n.resource_class.empty()) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
          "node " + n.node_id + " missing resources.class"));
    }
    // cpu_heavy 必须 parallel (控制包 schema allOf)
    if (n.resource_class == "cpu_heavy" && !n.parallel) {
      return Result<PipelineIR>::fail(Error(ErrorDomain::DATA,
          "node " + n.node_id + ": cpu_heavy must be parallel=true"));
    }
    ir.nodes.push_back(std::move(n));
  }
  // pipeline 顶层 outputs 为扁平对象 (非数组)
  {
    std::string outs_obj = obj_key_obj(json, "outputs");
    ir.outputs = obj_key_pairs(outs_obj);
  }
  if (ir.outputs.empty()) {
    return Result<PipelineIR>::fail(Error(ErrorDomain::DATA, "pipeline outputs required"));
  }
  return Result<PipelineIR>::ok(std::move(ir));
}

std::vector<IrIssue> PipelineIRParser::validate(
    const PipelineIR& ir, const std::vector<std::string>& known_modules) const {
  std::vector<IrIssue> issues;
  std::set<std::string> known(known_modules.begin(), known_modules.end());
  std::map<std::string, std::string> producer_of;  // artifact -> node_id
  std::map<std::string, std::vector<std::string>> consumers_of;
  std::map<std::string, std::string> node_by_id;

  for (const auto& n : ir.nodes) {
    node_by_id[n.node_id] = n.module_id;
    if (!known.count(n.module_id)) {
      issues.push_back({IrError::UNKNOWN_MODULE, n.node_id,
                        "module not registered: " + n.module_id});
    }
    for (const auto& [port, art] : n.outputs) {
      if (producer_of.count(art)) {
        issues.push_back({IrError::DUPLICATE_PRODUCER, n.node_id,
                          "artifact " + art + " already produced by " + producer_of[art]});
      }
      producer_of[art] = n.node_id;
    }
    for (const auto& [port, art] : n.inputs) {
      consumers_of[art].push_back(n.node_id);
    }
  }

  // 环检测 (DFS on node deps via artifact producer)
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
    if (visiting.count(id)) return true;  // cycle
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

  // 未消费的必需产物 (pipeline outputs 必须被产出)
  for (const auto& [port, art] : ir.outputs) {
    if (!producer_of.count(art)) {
      issues.push_back({IrError::UNCONSUMED, "", "output artifact not produced: " + art});
    }
  }
  return issues;
}

}  // namespace astrocs::core
