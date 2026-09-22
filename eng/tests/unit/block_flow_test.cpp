// eng/tests/unit/block_flow_test.cpp — ARCH-505 阶段块流执行器回归锁
//
// 依据：CONTRACT-501 docs/contracts/PIPELINE_BLOCK_CONTRACT.md
//         §1.1（跨阶段唯一载体 = HiPS 产品树；同一产物身份不得在一个阶段产出、在另一阶段被消费）
//         §2 第 4 条（声明的 lifecycle 与实际消费者跨度不一致 ⇒ 非法）
//         §3（块被全部声明消费者用完即销毁；阶段结束不得残留任何块）
//       ASTROCS_DESIGN.md §8.1（三命令独立、阶段间只走磁盘产品）、§8.2（命名块内存管线与块生命周期）
//       lib/include/astrocs/core/block_flow.h（BlockRole：EXTERNAL_IN/EXTERNAL_OUT/STAGE/SHORT）
//       块流规格唯一事实源 eng/contracts/block_flow/stage_block_flow.json
//       （由 lib/infrastructure/pipeline/module_ports.registry.json 派生，机器门
//        eng/tools/quality/check_block_flow_spec.py 防漂移）
//       生命周期派生规则：eng/tools/quality/gen_block_flow_spec.py docstring L7-13
//         （无生产者 ⇒ EXTERNAL_IN；阶段终产物 ⇒ EXTERNAL_OUT；消费者数 >= 2 ⇒ STAGE；
//          消费者数 == 0 且非终产物 ⇒ STAGE（终态中间产物）；消费者数 == 1 ⇒ SHORT）
//
// 判据纪律：**不硬编码块名与生命周期**。除阶段外部输入块名（lights，注册表端口名）与
// 注册表 module_id 外，D1/D2/D3/B3/B5/E1/E2 的期望值全部从规格正本数据推导 ⇒ 规格改名、
// 增删块、调整消费者时判据仍然有效，只有真正的语义违规才判红。
//
// 判据（每条可证伪）：
//   A. 规格加载：三阶段规格可从 JSON 解析；节点数 8/7/5；块图校验（四条非法图判据）无 issue；
//   B. 生命周期：SHORT 块在最后一个声明消费者用完即销毁；STAGE 块活到单元结束；
//       产品块（EXTERNAL_OUT）被收集；单元结束 short_residual_bytes == 0；
//   C. fail-closed 负例：未声明写（名字级）/ 缺声明写 / 缺阶段外部输入 / 节点失败 / 非法块图
//      都必须判红且带定位；
//   D. 阶段隔离与生命周期自洽：mosaic 的跨阶段边界恰为一个 EXTERNAL_IN 块（由上一阶段的
//      EXTERNAL_OUT 提供、本阶段内无生产者）；多消费者块是 STAGE；
//      逐块校验「lifecycle ⟺ 消费者跨度」不变量（双向，覆盖三阶段全部块）；
//   E. 数值等价：块流路径与「直接顺序计算」逐位一致（同一算子，冻结容差 = 0），
//      且比较对输入扰动有判别力（非恒真）；
//   F. 确定性：同输入重复运行产品 checksum 相同。
#include "astrocs/core/block_flow.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace astrocs::core;

#ifndef ARCH505_EVIDENCE_DIR
#define ARCH505_EVIDENCE_DIR "run/RELEASE-05/evidence"
#endif
#ifndef ARCH505_SPEC_PATH
#define ARCH505_SPEC_PATH "eng/contracts/block_flow/stage_block_flow.json"
#endif

namespace {
int g_fail = 0, g_total = 0;
void check(bool ok, const std::string& what) {
  ++g_total;
  if (!ok) { ++g_fail; std::printf("FAIL %s\n", what.c_str()); }
}

void ensure_dir() {
  std::string p(ARCH505_EVIDENCE_DIR), acc;
  std::size_t i = 0;
  if (!p.empty() && p[0] == '/') { acc = "/"; i = 1; }
  while (i <= p.size()) {
    const std::size_t j = p.find('/', i);
    const std::string part = p.substr(i, (j == std::string::npos ? p.size() : j) - i);
    if (!part.empty()) {
      if (!acc.empty() && acc.back() != '/') acc.push_back('/');
      acc += part;
      ::mkdir(acc.c_str(), 0755);
    }
    if (j == std::string::npos) break;
    i = j + 1;
  }
}

std::string read_file(const std::string& p) {
  std::ifstream f(p);
  std::stringstream ss; ss << f.rdbuf();
  return ss.str();
}

// 规格路径：默认 = 唯一事实源正本。环境变量 ARCH505_SPEC_PATH 可指向**临时副本**，
// 供负例注入（故意制造规格违规）使用 —— 这样负例不会改写正本。
std::string spec_path() {
  const char* env = std::getenv("ARCH505_SPEC_PATH");
  return (env && *env != '\0') ? std::string(env) : std::string(ARCH505_SPEC_PATH);
}

constexpr std::size_t kN = 8;   // 每块 8 个 f64

// 节点算子：把**所有读块**逐元素相加，再各自 +kNodeGain[node_index]，写入每个写块。
// 纯确定性、可手算，用于与「直接顺序计算」做逐位比对。
double node_gain(const std::string& module_id) {
  double g = 0.0;
  for (char c : module_id) g += static_cast<double>(static_cast<unsigned char>(c));
  return g;
}

void bind_ops(StageBlockFlowSpec* spec) {
  for (auto& n : spec->nodes) {
    const std::string mid = n.module_id;
    const std::vector<std::string> reads = n.reads;
    const std::vector<std::string> writes = n.writes;
    n.run = [mid, reads, writes](BlockFrame& fr) -> bool {
      std::vector<double> acc(kN, 0.0);
      for (const std::string& rd : reads) {
        const Block* b = fr.find(rd);
        if (!b || !b->f64()) return false;
        for (std::size_t i = 0; i < kN; ++i) acc[i] += b->f64()[i];
      }
      for (double& v : acc) v += node_gain(mid);
      for (const std::string& wr : writes) {
        BlockMeta m;
        m.name = wr;
        m.producer = mid;
        m.consumers = {};
        m.dtype = BlockDtype::F64;
        Block* nb = fr.create(m, kN);
        if (!nb || !nb->f64()) return false;
        for (std::size_t i = 0; i < kN; ++i) nb->f64()[i] = acc[i];
      }
      return true;
    };
  }
}

// ── 规格驱动的查询助手（判据据此推导期望值，不硬编码块名）────────────────────
std::vector<std::string> blocks_with_role(const StageBlockFlowSpec& s, BlockRole r) {
  std::vector<std::string> v;
  for (const auto& kv : s.roles)
    if (kv.second == r) v.push_back(kv.first);
  return v;   // std::map 迭代按 key 升序
}

std::size_t consumer_count(const StageBlockFlowSpec& s, const std::string& block) {
  const auto it = s.consumers.find(block);
  return (it == s.consumers.end()) ? 0 : it->second.size();
}

const BlockFlowNode* node_reading(const StageBlockFlowSpec& s, const std::string& block) {
  for (const auto& n : s.nodes)
    if (std::find(n.reads.begin(), n.reads.end(), block) != n.reads.end()) return &n;
  return nullptr;
}

std::string join(const std::vector<std::string>& v, const char* sep = ",") {
  std::string out;
  for (std::size_t i = 0; i < v.size(); ++i) { if (i) out += sep; out += v[i]; }
  return out;
}

// 按规格注入**全部** EXTERNAL_IN 块：执行器要求阶段输入齐备（缺一即
// missing_external_input:<块名>，fail-closed）。各输入块取值互不相同，保证和值非退化。
std::map<std::string, std::vector<double>> make_inputs(const StageBlockFlowSpec& s) {
  std::map<std::string, std::vector<double>> in;
  double base = 0.0;
  for (const std::string& nm : blocks_with_role(s, BlockRole::EXTERNAL_IN)) {
    std::vector<double> v(kN);
    for (std::size_t i = 0; i < kN; ++i) v[i] = base + static_cast<double>(i);
    in[nm] = v;
    base += 100.0;
  }
  return in;
}

bool inject_all(StageBlockFlow* f, const std::map<std::string, std::vector<double>>& in) {
  bool ok = true;
  for (const auto& kv : in)
    if (!f->set_input_f64(kv.first, kv.second)) ok = false;
  return ok;
}

// 「直接顺序计算」参考：按规格声明序逐节点求值，算子与块流路径**同一** stub 算子
// （读块逐元素求和 + node_gain，累加序也一致），但不使用帧/生命周期机制
// ⇒ 与块流路径逐位可比（冻结容差 = 0）。
std::map<std::string, std::vector<double>> direct_sequential(
    const StageBlockFlowSpec& spec,
    const std::map<std::string, std::vector<double>>& inputs, bool* ok) {
  std::map<std::string, std::vector<double>> vals = inputs;
  for (const auto& n : spec.nodes) {
    std::vector<double> acc(kN, 0.0);
    for (const std::string& rd : n.reads) {
      const auto it = vals.find(rd);
      if (it == vals.end()) { if (ok) *ok = false; return {}; }
      for (std::size_t i = 0; i < kN; ++i) acc[i] += it->second[i];
    }
    for (double& v : acc) v += node_gain(n.module_id);
    for (const std::string& wr : n.writes) vals[wr] = acc;
  }
  if (ok) *ok = true;
  return vals;
}
}  // namespace

int main() {
  ensure_dir();
  const std::string spec_file = spec_path();
  const std::string spec_text = read_file(spec_file);
  std::printf("INFO spec path = %s (%zu bytes)\n", spec_file.c_str(), spec_text.size());
  check(!spec_text.empty(), "A0 spec file readable: " + spec_file);

  // A. 规格加载 + 块图校验
  StageBlockFlowSpec p1, p2, p3;
  std::string err;
  check(parse_stage_block_flow_spec(spec_text, "normalize", &p1, &err), "A1 parse normalize: " + err);
  check(parse_stage_block_flow_spec(spec_text, "mosaic", &p2, &err), "A2 parse mosaic: " + err);
  check(parse_stage_block_flow_spec(spec_text, "export", &p3, &err), "A3 parse export: " + err);
  check(p1.nodes.size() == 8, "A4 normalize has 8 nodes, got " + std::to_string(p1.nodes.size()));
  check(p2.nodes.size() == 7, "A5 mosaic has 7 nodes, got " + std::to_string(p2.nodes.size()));
  check(p3.nodes.size() == 5, "A6 export has 5 nodes, got " + std::to_string(p3.nodes.size()));

  bind_ops(&p1);
  bind_ops(&p2);
  bind_ops(&p3);

  ProbeSink sink(std::string(ARCH505_EVIDENCE_DIR) + "/arch505_probes.jsonl");
  StageBlockFlow f1(p1, &sink), f2(p2, &sink), f3(p3, &sink);
  auto dump_issues = [](const char* tag, const StageBlockFlow& f) {
    for (const auto& is : f.graph_issues())
      std::printf("INFO %s issue kind=%d block=%s detail=%s\n", tag,
                  static_cast<int>(is.kind), is.block.c_str(), is.detail.c_str());
  };
  dump_issues("normalize", f1);
  dump_issues("mosaic", f2);
  dump_issues("export", f3);
  check(f1.spec_valid(), "A7 normalize block graph valid (BlockDagValidator)");
  check(f2.spec_valid(), "A8 mosaic block graph valid");
  check(f3.spec_valid(), "A9 export block graph valid");

  // ── D. 阶段隔离 + 生命周期自洽（期望值全部由规格数据推导）────────────────────
  // 规范：CONTRACT-501 §1.1（跨阶段只走阶段对外产品/磁盘产品，阶段间不得共享阶段内块）、
  //       §2 第 4 条（lifecycle 与实际消费者跨度不一致 ⇒ 非法）；
  //       派生规则 gen_block_flow_spec.py docstring L7-13（见文件头）。
  const std::vector<std::string> p1_out = blocks_with_role(p1, BlockRole::EXTERNAL_OUT);
  const std::vector<std::string> p2_in = blocks_with_role(p2, BlockRole::EXTERNAL_IN);
  const std::vector<std::string> p1_in = blocks_with_role(p1, BlockRole::EXTERNAL_IN);

  // D1 阶段隔离：mosaic 的跨阶段边界**恰有一个** EXTERNAL_IN 块；该块必须是上一阶段
  //    （normalize）声明的 EXTERNAL_OUT（跨阶段只走阶段对外产品），且在 mosaic 内无生产者
  //    （⇒ mosaic 不依赖任何 P1 节点，阶段隔离）。
  {
    const bool exactly_one = (p2_in.size() == 1);
    bool is_upstream_product = false, no_in_stage_producer = false;
    if (exactly_one) {
      is_upstream_product = std::find(p1_out.begin(), p1_out.end(), p2_in[0]) != p1_out.end();
      no_in_stage_producer = (p2.producer.count(p2_in[0]) == 0);
    }
    check(exactly_one && is_upstream_product && no_in_stage_producer,
          "D1 mosaic stage isolation: exactly one cross-stage boundary block [" + join(p2_in) +
              "], it is a normalize EXTERNAL_OUT [" + join(p1_out) +
              "] and has no in-stage producer (mosaic depends on no P1 node)");
  }

  // D2 多消费者块必须活到阶段末：normalize 中消费阶段外部输入 lights 的节点，其产出块必须是
  //    STAGE（块名与消费者数从规格读，不写死数字）。
  const std::string kExternalInput = "lights";
  const BlockFlowNode* cal_node = node_reading(p1, kExternalInput);
  const std::string cal_out =
      (cal_node != nullptr && !cal_node->writes.empty()) ? cal_node->writes[0] : std::string();
  const std::size_t cal_cons = cal_out.empty() ? 0 : consumer_count(p1, cal_out);
  const bool cal_is_stage =
      !cal_out.empty() && p1.roles.count(cal_out) != 0 && p1.roles.at(cal_out) == BlockRole::STAGE;
  check(cal_is_stage && cal_cons >= 2,
        "D2 normalize block '" + cal_out + "' (produced by the node consuming EXTERNAL_IN '" +
            kExternalInput + "') is STAGE with >= 2 in-stage consumers (actual consumers = " +
            std::to_string(cal_cons) + ")");

  // D3 生命周期自洽（逐块、覆盖三阶段、**双向**）：
  //      SHORT ⟺ 恰有 1 个本阶段消费者（最后一次消费即销毁）；
  //      STAGE ⟺ 0 个消费者（终态中间产物）或 >= 2 个（必须活到阶段末）；
  //      EXTERNAL_IN ⟺ 本阶段内无生产者；EXTERNAL_OUT ⇒ 本阶段内有生产者（阶段终产物，
  //      允许同时被本阶段内部消费）；
  //      且任何块的消费者都必须落在本阶段节点集合内（阶段隔离）。
  //    机器门 check_block_flow_spec.py R7 只判 SHORT 一侧（单向弱化）；本判据按派生规则做
  //    双向断言 ⇒ 对规格改名/增删块稳健，但 lifecycle 与消费者集合不自洽时必判红。
  {
    std::vector<std::string> viol;
    const std::pair<const char*, const StageBlockFlowSpec*> stages[] = {
        {"normalize", &p1}, {"mosaic", &p2}, {"export", &p3}};
    for (const auto& st : stages) {
      const StageBlockFlowSpec& s = *st.second;
      std::vector<std::string> node_ids;
      for (const auto& n : s.nodes) node_ids.push_back(n.module_id);
      for (const auto& kv : s.roles) {
        const std::string& b = kv.first;
        const BlockRole r = kv.second;
        const std::size_t nc = consumer_count(s, b);
        const bool has_producer = s.producer.count(b) != 0;
        const auto cit = s.consumers.find(b);
        const std::vector<std::string> cons =
            (cit == s.consumers.end()) ? std::vector<std::string>{} : cit->second;
        std::string why;
        if (r == BlockRole::EXTERNAL_IN) {
          if (has_producer) why = "EXTERNAL_IN but has an in-stage producer";
        } else if (!has_producer) {
          why = std::string(block_role_name(r)) + " but no in-stage producer";
        }
        if (why.empty() && r == BlockRole::SHORT && nc != 1)
          why = "SHORT but " + std::to_string(nc) + " consumers (must be exactly 1)";
        if (why.empty() && r == BlockRole::STAGE && nc == 1)
          why = "STAGE but exactly 1 consumer (must be SHORT)";
        if (why.empty()) {
          for (const std::string& c : cons) {
            if (std::find(node_ids.begin(), node_ids.end(), c) == node_ids.end()) {
              why = "consumer out of stage: " + c;
              break;
            }
          }
        }
        if (!why.empty())
          viol.push_back(std::string(st.first) + "/" + b + " [" + block_role_name(r) + ", " +
                         std::to_string(nc) + " consumers]: " + why);
      }
    }
    for (const std::string& v : viol) std::printf("INFO D3 violation: %s\n", v.c_str());
    check(viol.empty(),
          "D3 lifecycle <-> consumer-span invariant holds for every block of all 3 stages" +
              (viol.empty() ? std::string()
                            : " (violating blocks: " + join(viol, " | ") + ")"));
  }

  // ── B + E + F. 运行 P1 单元 ───────────────────────────────────────────────
  const std::map<std::string, std::vector<double>> inputs = make_inputs(p1);
  const std::vector<std::string> p1_products = p1_out;   // EXTERNAL_OUT（升序）
  check(!p1_in.empty() && inject_all(&f1, inputs) &&
            std::find(p1_in.begin(), p1_in.end(), kExternalInput) != p1_in.end(),
        "B0 all normalize EXTERNAL_IN injected [" + join(p1_in) + "] (must include '" +
            kExternalInput + "')");
  const auto lights_it = inputs.find(kExternalInput);
  check(lights_it != inputs.end() && !f1.set_input_f64(kExternalInput, lights_it->second),
        "B0b duplicate external input '" + kExternalInput + "' rejected");

  BlockFlowOutcome o1 = f1.run_unit("frame-0001");
  check(o1.ok, "B1 normalize unit ok: " + o1.error);
  check(o1.ok && o1.short_residual_bytes == 0,
        "B2 no SHORT block residual: " + std::to_string(o1.short_residual_bytes));
  check(o1.product_bytes.size() == p1_products.size() && f1.product_names() == p1_products,
        "B3 all EXTERNAL_OUT products collected: got [" + join(f1.product_names()) + "] want [" +
            join(p1_products) + "]");
  check(o1.blocks_created > 0 && o1.blocks_destroyed > 0, "B4 blocks created and destroyed");
  {
    const std::vector<std::string> short_blocks = blocks_with_role(p1, BlockRole::SHORT);
    std::string leaked;
    for (const std::string& nm : short_blocks)
      if (f1.product(nm)) { leaked = nm; break; }
    check(!short_blocks.empty() && leaked.empty(),
          "B5 no SHORT block retained as product (SHORT blocks [" + join(short_blocks) + "]" +
              (leaked.empty() ? std::string() : ", leaked: " + leaked) + ")");
  }

  // B6 产品发布不变量（三阶段）：每个阶段都要能跑完一个单元，且**每个** EXTERNAL_OUT 块
  //    都必须活到单元末被 ⑤ 搬进保留产品 frame（存活 + 满字节），同时阶段内不得残留
  //    （short_residual_bytes == 0）、引用计数销毁路径仍然生效（blocks_destroyed > 0
  //    ⇒ EXTERNAL_OUT 豁免没有把销毁机制整体架空）。
  //    规范：gen_block_flow_spec.py L77-78「阶段终产物 ⇒ EXTERNAL_OUT（必须发布，也可被本
  //    阶段内部消费）」；block_flow.h L34「EXTERNAL_OUT = 阶段对外产品，必须发布」。
  {
    struct StageRun { const char* name; StageBlockFlow* flow; };
    StageRun runs[] = {{"normalize", &f1}, {"mosaic", &f2}, {"export", &f3}};
    inject_all(&f2, make_inputs(p2));
    inject_all(&f3, make_inputs(p3));
    std::string bad;
    for (const StageRun& sr : runs) {
      const std::vector<std::string> outs =
          blocks_with_role(sr.flow->spec(), BlockRole::EXTERNAL_OUT);
      const BlockFlowOutcome oc = sr.flow->run_unit(std::string("unit-") + sr.name);
      std::printf("INFO B6 %s: ok=%d err=%s products=[%s] short_residual_bytes=%zu "
                  "blocks_destroyed=%zu external_out=%zu\n",
                  sr.name, oc.ok ? 1 : 0, oc.error.c_str(),
                  join(sr.flow->product_names()).c_str(), oc.short_residual_bytes,
                  oc.blocks_destroyed, outs.size());
      if (!oc.ok) { bad = std::string(sr.name) + ":unit_failed:" + oc.error; break; }
      if (oc.short_residual_bytes != 0) {
        bad = std::string(sr.name) + ":short_residual=" + std::to_string(oc.short_residual_bytes);
        break;
      }
      if (oc.blocks_destroyed == 0) { bad = std::string(sr.name) + ":nothing_destroyed"; break; }
      for (const std::string& nm : outs) {
        const Block* b = sr.flow->product(nm);
        if (!b || !b->is_alive() || b->bytes() != kN * sizeof(double)) {
          bad = std::string(sr.name) + ":product_not_published:" + nm;
          break;
        }
      }
      if (!bad.empty()) break;
    }
    check(bad.empty(),
          "B6 all 3 stages run one unit and publish every EXTERNAL_OUT block (alive, full bytes)"
          " while the reference-count destroy path still runs" +
              (bad.empty() ? std::string() : " (bad: " + bad + ")"));
  }

  // E. 数值等价：与直接顺序计算逐位一致
  bool ref_ok = false;
  const std::map<std::string, std::vector<double>> ref = direct_sequential(p1, inputs, &ref_ok);
  {
    bool all_f64 = true;
    std::string bad_block;
    for (const std::string& nm : p1_products) {
      const Block* b = f1.product(nm);
      if (!b || !b->f64()) { all_f64 = false; bad_block = nm; break; }
    }
    check(ref_ok && all_f64,
          "E1 all EXTERNAL_OUT products have f64 view" +
              (all_f64 ? std::string() : " (bad block: " + bad_block + ")"));
  }
  {
    bool bitwise = ref_ok;
    std::string bad_block;
    std::size_t first = 0;
    double got = 0.0, want = 0.0;
    if (bitwise) {
      for (const std::string& nm : p1_products) {
        const Block* b = f1.product(nm);
        const auto it = ref.find(nm);
        if (!b || !b->f64() || it == ref.end()) { bitwise = false; bad_block = nm; break; }
        for (std::size_t i = 0; i < kN; ++i) {
          if (b->f64()[i] != it->second[i]) {
            bitwise = false; bad_block = nm; first = i;
            got = b->f64()[i]; want = it->second[i];
            break;
          }
        }
        if (!bitwise) break;
      }
    }
    // 非退化自证：把阶段外部输入扰动 +1.0 后重算参考，必须与产品**不**逐位相等，
    // 否则说明本比较恒真（空断言）。
    std::map<std::string, std::vector<double>> perturbed = inputs;
    if (!perturbed.empty()) perturbed.begin()->second[0] += 1.0;
    bool pert_ok = false;
    const std::map<std::string, std::vector<double>> pref = direct_sequential(p1, perturbed, &pert_ok);
    bool discriminates = false;
    for (const std::string& nm : p1_products) {
      const Block* b = f1.product(nm);
      const auto it = pref.find(nm);
      if (b && b->f64() && it != pref.end() && b->f64()[0] != it->second[0]) {
        discriminates = true;
        break;
      }
    }
    check(bitwise && pert_ok && discriminates,
          "E2 all " + std::to_string(p1_products.size()) +
              " products BITWISE equal to the direct sequential reference, and the comparison"
              " discriminates an input perturbation (non-degenerate)" +
              (bitwise ? std::string()
                       : " (first diff: block " + bad_block + "[" + std::to_string(first) +
                             "] got=" + std::to_string(got) + " want=" + std::to_string(want) + ")"));
    if (bitwise)
      std::printf("INFO normalize products [%s] bitwise identical to the sequential reference\n",
                  join(p1_products).c_str());
  }

  // F. 确定性
  {
    StageBlockFlow g1(p1, nullptr);
    inject_all(&g1, inputs);
    BlockFlowOutcome o2 = g1.run_unit("frame-0001");
    check(o2.ok && o2.checksum == o1.checksum,
          "F1 repeat run yields identical product checksum");
  }

  // C. fail-closed 负例
  {
    // C1 未声明写（**纯名字级**）：算子只创建一个块，数量与 writes 相同但名字不对
    StageBlockFlowSpec bad = p1;
    for (auto& n : bad.nodes) {
      if (n.module_id != "astrocs.phase1.calibration") continue;
      n.run = [](BlockFrame& fr) -> bool {
        BlockMeta m; m.name = "not_declared"; m.producer = "x"; m.dtype = BlockDtype::F64;
        fr.create(m, kN);
        return true;
      };
    }
    StageBlockFlow fb(bad, nullptr);
    inject_all(&fb, inputs);
    BlockFlowOutcome ob = fb.run_unit("u");
    check(!ob.ok && ob.error.find("undeclared_or_missing_write") != std::string::npos &&
              ob.error.find("astrocs.phase1.calibration") != std::string::npos &&
              ob.error.find("not_declared") != std::string::npos,
          "C1 undeclared write (name-level, same count) rejected: " + ob.error);
  }
  {
    // C2 缺声明写：算子不创建它声明的写块
    StageBlockFlowSpec bad = p1;
    for (auto& n : bad.nodes) {
      if (n.module_id != "astrocs.phase1.cosmetic") continue;
      n.run = [](BlockFrame&) -> bool { return true; };   // 什么都不写
    }
    StageBlockFlow fb(bad, nullptr);
    inject_all(&fb, inputs);
    BlockFlowOutcome ob = fb.run_unit("u");
    check(!ob.ok && ob.error.find("undeclared_or_missing_write") != std::string::npos,
          "C2 missing declared write rejected: " + ob.error);
  }
  {
    // C3 缺阶段外部输入：少注入一个 EXTERNAL_IN ⇒ 必须 fail-closed 且错误点名该块
    StageBlockFlow fb(p1, nullptr);
    std::map<std::string, std::vector<double>> partial = inputs;
    const std::string omitted = p1_in.empty() ? std::string() : p1_in.back();
    if (!omitted.empty()) partial.erase(omitted);
    inject_all(&fb, partial);
    BlockFlowOutcome ob = fb.run_unit("u");
    check(!omitted.empty() && !ob.ok && ob.error == "missing_external_input:" + omitted,
          "C3 unit missing declared external input '" + omitted + "' fails closed: " + ob.error);
  }
  {
    // C4 节点失败传播（带节点名）：让「跨阶段产品的生产者」失败（链中段，非末节点）
    std::string fail_node;
    if (p2_in.size() == 1 && p1.producer.count(p2_in[0]) != 0) fail_node = p1.producer.at(p2_in[0]);
    StageBlockFlowSpec bad = p1;
    for (auto& n : bad.nodes) {
      if (n.module_id != fail_node) continue;
      n.run = [](BlockFrame&) -> bool { return false; };
    }
    StageBlockFlow fb(bad, nullptr);
    inject_all(&fb, inputs);
    BlockFlowOutcome ob = fb.run_unit("u");
    check(!fail_node.empty() && !ob.ok && ob.error == "node_failed:" + fail_node,
          "C4 node failure propagates with node id '" + fail_node + "': " + ob.error);
  }
  {
    // C5 非法块图：给一个 STAGE 块追加一个不在节点集合内的消费者 ⇒ 构建期判红
    StageBlockFlowSpec bad = p1;
    std::string target;
    for (const auto& kv : p1.roles) {
      if (kv.second == BlockRole::STAGE && consumer_count(p1, kv.first) >= 1) {
        target = kv.first;
        break;
      }
    }
    if (!target.empty()) bad.consumers[target].push_back("astrocs.phase9.ghost");
    StageBlockFlow fb(bad, nullptr);
    check(!target.empty() && !fb.spec_valid(),
          "C5 invalid block graph (ghost consumer on STAGE block '" + target +
              "') rejected at build time");
  }

  // 产品证据落盘
  {
    std::ofstream ev(std::string(ARCH505_EVIDENCE_DIR) + "/arch505_block_flow.txt", std::ios::trunc);
    if (ev) {
      ev << "# ARCH-505 块流执行器实测\n"
         << "spec_path=" << spec_file << "\n"
         << "normalize_nodes=" << p1.nodes.size() << "\n"
         << "mosaic_nodes=" << p2.nodes.size() << "\n"
         << "export_nodes=" << p3.nodes.size() << "\n"
         << "normalize_external_in=" << join(p1_in) << "\n"
         << "normalize_products=" << join(p1_products) << "\n"
         << "normalize_blocks_created=" << o1.blocks_created << "\n"
         << "normalize_blocks_destroyed=" << o1.blocks_destroyed << "\n"
         << "short_residual_bytes=" << o1.short_residual_bytes << "\n"
         << "peak_bytes=" << o1.peak_bytes << "\n"
         << "product_checksum=" << o1.checksum << "\n";
    }
  }

  sink.flush();
  std::printf("BLOCK-FLOW: %d/%d checks passed, %d failed\n", g_total - g_fail, g_total, g_fail);
  return g_fail == 0 ? 0 : 1;
}
