// ACSD Core — ARCH-505 阶段块流执行器实现
// 依据：ASTROCS_DESIGN.md §8.2/§8.3；CONTRACT-501 PIPELINE_BLOCK_CONTRACT
#include "astrocs/core/block_flow.h"

#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

namespace astrocs::core {
namespace {

double now_seconds() {
  using clock = std::chrono::steady_clock;
  static const clock::time_point t0 = clock::now();
  return std::chrono::duration<double>(clock::now() - t0).count();
}

std::uint64_t fnv1a(std::uint64_t h, const void* p, std::size_t n) {
  const std::uint8_t* b = static_cast<const std::uint8_t*>(p);
  for (std::size_t i = 0; i < n; ++i) {
    h ^= static_cast<std::uint64_t>(b[i]);
    h *= 1099511628211ULL;
  }
  return h;
}

}  // namespace

const char* block_role_name(BlockRole r) {
  switch (r) {
    case BlockRole::EXTERNAL_IN: return "EXTERNAL_IN";
    case BlockRole::EXTERNAL_OUT: return "EXTERNAL_OUT";
    case BlockRole::STAGE: return "STAGE";
    case BlockRole::SHORT: return "SHORT";
  }
  return "?";
}

bool parse_block_role(const std::string& s, BlockRole* out) {
  if (s == "EXTERNAL_IN") { *out = BlockRole::EXTERNAL_IN; return true; }
  if (s == "EXTERNAL_OUT") { *out = BlockRole::EXTERNAL_OUT; return true; }
  if (s == "STAGE") { *out = BlockRole::STAGE; return true; }
  if (s == "SHORT") { *out = BlockRole::SHORT; return true; }
  return false;
}

// 角色 → BlockLifecycle 的映射依据 BlockDagValidator 的冻结语义：
//   - SHORT 要求**恰有 1 个消费者**（节点内即时消耗）；
//   - RUN 要求**至少 1 个消费者**（存活到导出，且仍在阶段内被消费）；
//   - 终态块（consumers 为空）只能是 FRAME。
// 阶段对外产品（EXTERNAL_OUT）映射为 FRAME：它按规格**必须发布**，且允许同时被本阶段内部
// 消费（规格派生规则 eng/tools/quality/gen_block_flow_spec.py L77-78「阶段终产物 ⇒
// EXTERNAL_OUT（必须发布，也可被本阶段内部消费）」；机器门 check_block_flow_spec.py R7
// 「== 1 ⇒ SHORT，除非该块同时是阶段终产物（EXTERNAL_OUT 优先）」），
// 因此它的消费者集合**不一定为空**（如 normalize/frame_hips ← writer、export/p3_fits ← verify）。
// 执行器据此在 run_unit ③ 对 EXTERNAL_OUT 免于引用计数销毁、在 ⑤ 把产品块搬进独立 frame
// 延长生命期并发布 —— 二者必须成对成立：缺 ③ 的豁免，产品块会在 ⑤ 之前被销毁而报
// missing_product，阶段永远跑不完一个单元。
// 阶段输入是外部注入、无生产者，按合同必须声明 optional=true（见 build_graph）。
BlockLifecycle lifecycle_of(BlockRole r) {
  return (r == BlockRole::SHORT) ? BlockLifecycle::SHORT : BlockLifecycle::FRAME;
}

StageBlockFlow::StageBlockFlow(StageBlockFlowSpec spec, ProbeSink* sink)
    : spec_(std::move(spec)), sink_(sink) {
  // 构建期块图校验（合同 §2 四条非法图判据）：非法图不得进入运行期
  BlockGraph g;
  for (const auto& n : spec_.nodes) g.node_ids.push_back(n.module_id);
  for (const auto& kv : spec_.roles) {
    BlockMeta m;
    m.name = kv.first;
    m.producer = spec_.producer.count(kv.first) ? spec_.producer.at(kv.first) : std::string();
    if (spec_.consumers.count(kv.first)) m.consumers = spec_.consumers.at(kv.first);
    m.lifecycle = lifecycle_of(kv.second);
    // 合同（PIPELINE_BLOCK_CONTRACT §2 第 8 条）：非 optional 块必须有生产者；
    // 阶段输入无生产者 ⇒ 必须 optional=true（缺块时消费方须显式登记降级）。
    m.optional = (kv.second == BlockRole::EXTERNAL_IN);
    auto dt = spec_.dtypes.find(kv.first);
    m.dtype = (dt != spec_.dtypes.end()) ? dt->second : BlockDtype::F64;
    auto un = spec_.units.find(kv.first);
    if (un != spec_.units.end()) m.unit = un->second;
    g.blocks.push_back(std::move(m));
  }
  graph_issues_ = BlockDagValidator::validate(g);
}

bool StageBlockFlow::set_input(const std::string& name, std::size_t element_count) {
  auto it = spec_.roles.find(name);
  if (it == spec_.roles.end() || it->second != BlockRole::EXTERNAL_IN) return false;
  if (inputs_.count(name)) return false;   // 重名覆盖 = 拒绝
  inputs_[name] = element_count;
  return true;
}

bool StageBlockFlow::set_input_f64(const std::string& name, const std::vector<double>& values) {
  if (!set_input(name, values.size())) return false;
  input_values_[name] = values;
  return true;
}

const Block* StageBlockFlow::product(const std::string& name) const {
  auto it = products_.find(name);
  return (it == products_.end()) ? nullptr : it->second.find(name);
}

BlockFlowOutcome StageBlockFlow::run_unit(const std::string& unit_id) {
  BlockFlowOutcome out;
  const double t0 = now_seconds();
  if (!graph_issues_.empty()) {
    out.error = "invalid_block_graph";
    return out;
  }

  BlockFrame frame;
  // ① 注入 EXTERNAL_IN 块（producer = "external"，consumers 按规格声明）
  for (const auto& kv : spec_.roles) {
    if (kv.second != BlockRole::EXTERNAL_IN) continue;
    auto iv = inputs_.find(kv.first);
    if (iv == inputs_.end()) {
      out.error = "missing_external_input:" + kv.first;
      return out;
    }
    BlockMeta m;
    m.name = kv.first;
    m.producer = "external";
    m.consumers = spec_.consumers.count(kv.first) ? spec_.consumers.at(kv.first)
                                                  : std::vector<std::string>{};
    m.lifecycle = BlockLifecycle::FRAME;
    m.optional = true;   // 外部输入：无阶段内生产者
    auto dt = spec_.dtypes.find(kv.first);
    m.dtype = (dt != spec_.dtypes.end()) ? dt->second : BlockDtype::F64;
    Block* b = frame.create(m, iv->second);
    if (!b) { out.error = "create_external_failed:" + kv.first; return out; }
    auto vals = input_values_.find(kv.first);
    if (vals != input_values_.end() && b->f64()) {
      std::copy(vals->second.begin(), vals->second.end(), b->f64());
    }
    b->set_provenance("stage", spec_.stage);
    b->set_provenance("unit", unit_id);
  }

  // ② 逐节点执行（顺序 = 规格声明顺序 = 拓扑序）
  for (const BlockFlowNode& n : spec_.nodes) {
    // 未声明读 = fail-closed
    for (const std::string& rd : n.reads) {
      if (!frame.find(rd)) {
        out.error = "missing_block:" + n.module_id + ":" + rd;
        frame.destroy_all();
        return out;
      }
    }
    const std::vector<std::string> alive_before = frame.names();
    const double nt0 = now_seconds();
    if (!n.run || !n.run(frame)) {
      out.error = "node_failed:" + n.module_id;
      frame.destroy_all();
      return out;
    }
    // 未声明写 = fail-closed，且必须做**名字级**校验：
    // 只比数量会漏掉「写了名字不对的块」。新增块集合必须恰等于 writes。
    {
      std::vector<std::string> newly;
      const std::vector<std::string> alive_after = frame.names();
      for (const std::string& nm : alive_after)
        if (std::find(alive_before.begin(), alive_before.end(), nm) == alive_before.end())
          newly.push_back(nm);
      std::vector<std::string> want = n.writes;
      std::sort(want.begin(), want.end());
      if (newly != want) {
        std::string got;
        for (const std::string& nm : newly) got += nm + ",";
        std::string exp;
        for (const std::string& nm : want) exp += nm + ",";
        out.error = "undeclared_or_missing_write:" + n.module_id + " expected=[" + exp +
                    "] got=[" + got + "]";
        frame.destroy_all();
        return out;
      }
    }
    // 生命周期归执行器：为刚创建的块盖上规格里的消费者集合
    for (const std::string& wr : n.writes) {
      const auto cit = spec_.consumers.find(wr);
      const std::vector<std::string> cons =
          (cit == spec_.consumers.end()) ? std::vector<std::string>{} : cit->second;
      if (!frame.declare_consumers(wr, cons)) {
        out.error = "node_overrides_lifecycle:" + n.module_id + ":" + wr;
        frame.destroy_all();
        return out;
      }
    }
    {
      ProbeEvent ev;
      ev.ts = now_seconds();
      ev.kind = ProbeKind::NODE_WALL;
      ev.node = n.module_id;
      ev.value = now_seconds() - nt0;
      ev.unit = "s";
      ev.stage = spec_.stage;
      ev.block = unit_id;
      if (sink_) sink_->emit(ev);
    }
    // ③ 块生命周期由执行器掌管：声明消费（节点不得自行销毁）
    for (const std::string& rd : n.reads) {
      // 阶段对外产品（EXTERNAL_OUT）免于引用计数销毁：规格规定阶段终产物**必须发布**，
      // 且允许它同时被本阶段内部消费（gen_block_flow_spec.py L77-78；机器门
      // check_block_flow_spec.py R7 明确认可「EXTERNAL_OUT + 1 个本阶段消费者」合法）。
      // 而 ⑤ 在单元末要求该块仍然存活才能搬进产品 frame ⇒ 若在此按剩余消费者数销毁它，
      // 「终产物同时被本阶段消费」的块（normalize/frame_hips ← writer、export/p3_fits ←
      // verify）就会在 ⑤ 报 missing_product，该阶段永远跑不完一个单元。
      // 判据是**角色**（凡 EXTERNAL_OUT 一律豁免），不是块名特例。
      const auto role_it = spec_.roles.find(rd);
      if (role_it != spec_.roles.end() && role_it->second == BlockRole::EXTERNAL_OUT) continue;
      const bool destroyed = frame.consume(rd, n.module_id);
      if (destroyed) {
        ++out.blocks_destroyed;
        ProbeEvent ev;
        ev.ts = now_seconds();
        ev.kind = ProbeKind::BLOCK_DEATH;
        ev.block = rd;
        ev.stage = spec_.stage;
        ev.node = n.module_id;
        ev.value = 1.0;
        ev.unit = "1";
        if (sink_) sink_->emit(ev);
      }
    }
    out.blocks_created = frame.blocks_total_created();
    out.peak_bytes = std::max(out.peak_bytes, frame.bytes_alive());
  }

  // ④ 单元结束：除 FRAME/RUN 块外不得残留（SHORT 残留 = 生命周期违规）
  for (const auto& kv : spec_.roles) {
    if (kv.second != BlockRole::SHORT) continue;
    const Block* b = frame.find(kv.first);
    if (b && b->is_alive()) out.short_residual_bytes += b->bytes();
  }
  if (out.short_residual_bytes != 0) {
    out.error = "short_block_residual:" + std::to_string(out.short_residual_bytes);
    frame.destroy_all();
    return out;
  }

  // ⑤ 收集产品块（RUN）：把产品移出本单元 frame，延长到 StageBlockFlow 生命期
  std::uint64_t h = 1469598103934665603ULL;
  product_names_.clear();
  products_.clear();
  for (const auto& kv : spec_.roles) {
    if (kv.second != BlockRole::EXTERNAL_OUT) continue;
    Block* b = frame.find(kv.first);
    if (!b || !b->is_alive()) {
      out.error = "missing_product:" + kv.first;
      frame.destroy_all();
      return out;
    }
    BlockFrame keep;
    BlockMeta m = b->meta();
    m.consumers.clear();                 // 产品块不再被本阶段消费
    Block* nb = keep.create(m, b->element_count());
    if (!nb) { out.error = "product_clone_failed:" + kv.first; frame.destroy_all(); return out; }
    if (b->raw() && nb->raw()) std::copy(b->raw(), b->raw() + b->bytes(), nb->raw());
    for (const auto& pv : b->provenance()) nb->set_provenance(pv.first, pv.second);
    products_[kv.first] = std::move(keep);
    product_names_.push_back(kv.first);
    out.product_bytes[kv.first] = b->bytes();
    h = fnv1a(h, b->raw(), b->bytes());
  }
  std::sort(product_names_.begin(), product_names_.end());

  out.checksum = h;
  out.ok = true;
  out.wall_seconds = now_seconds() - t0;
  frame.destroy_all();
  if (sink_) sink_->flush();
  return out;
}

bool parse_stage_block_flow_spec(const std::string& json_text, const std::string& stage,
                                 StageBlockFlowSpec* out, std::string* error) {
  using nlohmann::json;
  json doc;
  try {
    doc = json::parse(json_text);
  } catch (const std::exception& e) {
    if (error) *error = std::string("parse: ") + e.what();
    return false;
  }
  if (!doc.is_object() || !doc.contains("nodes") || !doc.contains("blocks")) {
    if (error) *error = "missing nodes/blocks";
    return false;
  }
  StageBlockFlowSpec spec;
  spec.stage = stage;
  for (const auto& n : doc["nodes"]) {
    if (n.value("stage", std::string()) != stage) continue;
    BlockFlowNode node;
    node.module_id = n.value("module_id", std::string());
    node.stage = n.value("stage", std::string());
    node.operation = n.value("operation", std::string());
    node.entry = n.value("entry", std::string());
    for (const auto& r : n["reads"]) node.reads.push_back(r.get<std::string>());
    for (const auto& w : n["writes"]) node.writes.push_back(w.get<std::string>());
    spec.nodes.push_back(std::move(node));
  }
  for (const auto& b : doc["blocks"]) {
    if (b.value("stage", std::string()) != stage) continue;
    const std::string name = b.value("block", std::string());
    BlockRole role;
    if (!parse_block_role(b.value("lifecycle", std::string()), &role)) {
      if (error) *error = "bad lifecycle for block " + name;
      return false;
    }
    spec.roles[name] = role;
    std::vector<std::string> cons;
    for (const auto& c : b["consumed_by"]) cons.push_back(c.get<std::string>());
    spec.consumers[name] = cons;
    const auto& pj = b["produced_by"];
    if (pj.is_array() && pj.size() == 1) spec.producer[name] = pj[0].get<std::string>();
  }
  if (spec.nodes.empty()) {
    if (error) *error = "no nodes for stage " + stage;
    return false;
  }
  *out = std::move(spec);
  return true;
}

}  // namespace astrocs::core
