// eng/tests/unit/block_flow_test.cpp — ARCH-505 阶段块流执行器回归锁
//
// 依据：CONTRACT-501 docs/contracts/PIPELINE_BLOCK_CONTRACT.md；块流规格唯一事实源
//       eng/contracts/block_flow/stage_block_flow.json（由注册表派生，机器门防漂移）；
//       ASTROCS_DESIGN §8.2（阶段内命名块内存管线与块生命周期）。
// 判据（每条可证伪）：
//   A. 规格加载：三阶段规格可从 JSON 解析；节点数 8/7/5；块图校验（四条非法图判据）无 issue；
//   B. 生命周期：SHORT 块在最后一个声明消费者用完即销毁；STAGE 块活到单元结束；
//      产品块（EXTERNAL_OUT）被收集；单元结束 short_residual_bytes == 0；
//   C. fail-closed 负例：未声明写（名字级）/ 缺块 / 节点失败 / 非法块图 都必须判红且带定位；
//   D. 阶段隔离：mosaic 的 calibrated 是 EXTERNAL_IN（阶段内无生产者），不依赖 P1 节点；
//   E. 数值等价：块流路径与「直接顺序计算」逐位一致（同一算子，冻结容差 = 0）；
//   F. 确定性：同输入重复运行产品 checksum 相同。
#include "astrocs/core/block_flow.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

// 直接顺序计算参考：按同一算子手算 P1 链的最终产品（fits）值
std::vector<double> direct_p1_reference(const std::vector<double>& frames) {
  std::vector<double> calibrated(kN), cleaned(kN), sources(kN), psf(kN), wcs(kN),
      fluxes(kN), photprov(kN), snr(kN), stacked(kN), fits(kN);
  auto add = [](std::vector<double>& dst, const std::vector<double>& src, const std::string& id) {
    for (std::size_t i = 0; i < kN; ++i) dst[i] = src[i] + node_gain(id);
  };
  add(calibrated, frames, "astrocs.phase1.calibration");
  add(cleaned, calibrated, "astrocs.phase1.cosmetic");
  add(sources, cleaned, "astrocs.phase1.star-psf");
  add(psf, cleaned, "astrocs.phase1.star-psf");
  add(wcs, sources, "astrocs.phase1.wcs-platesolve");
  for (std::size_t i = 0; i < kN; ++i)
    fluxes[i] = psf[i] + sources[i] + wcs[i] + node_gain("astrocs.phase1.photometry");
  // 同一节点的多个输出块取值相同（stub 算子把同一累加值写入所有 writes）：
  // photometry 同时产出 fluxes 与 photprov，两者相等。
  photprov = fluxes;
  add(snr, fluxes, "astrocs.phase1.noise-snr");
  for (std::size_t i = 0; i < kN; ++i)
    stacked[i] = calibrated[i] + wcs[i] + photprov[i] + node_gain("astrocs.phase1.drizzle");
  add(fits, stacked, "astrocs.phase1.writer");
  return fits;
}
}  // namespace

int main() {
  ensure_dir();
  const std::string spec_text = read_file(ARCH505_SPEC_PATH);
  check(!spec_text.empty(), "A0 spec file readable: " + std::string(ARCH505_SPEC_PATH));

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

  // D. 阶段隔离：mosaic 的 calibrated 必须是 EXTERNAL_IN
  check(p2.roles.count("calibrated") && p2.roles["calibrated"] == BlockRole::EXTERNAL_IN,
        "D1 mosaic.calibrated is EXTERNAL_IN (stage isolation: no cross-stage block reuse)");
  check(p1.roles.count("calibrated") && p1.roles["calibrated"] == BlockRole::STAGE,
        "D2 normalize.calibrated is STAGE (2 in-stage consumers)");
  check(p1.roles.count("snr") && p1.roles["snr"] == BlockRole::STAGE,
        "D3 normalize.snr is STAGE (terminal intermediate persists to export)");

  // B + E + F. 运行 P1 单元
  std::vector<double> frames(kN);
  for (std::size_t i = 0; i < kN; ++i) frames[i] = 10.0 + static_cast<double>(i);
  check(f1.set_input_f64("frames", frames), "B0 set external input 'frames'");
  check(!f1.set_input_f64("frames", frames), "B0b duplicate external input rejected");

  BlockFlowOutcome o1 = f1.run_unit("frame-0001");
  check(o1.ok, "B1 normalize unit ok: " + o1.error);
  check(o1.short_residual_bytes == 0, "B2 no SHORT block residual: " +
                                          std::to_string(o1.short_residual_bytes));
  check(o1.product_bytes.count("fits") == 1, "B3 product block 'fits' collected");
  check(o1.blocks_created > 0 && o1.blocks_destroyed > 0, "B4 blocks created and destroyed");
  // SHORT 块必须已被销毁：cleaned/psf/fluxes/photprov/stacked 都不应残留
  check(!f1.product("cleaned") && !f1.product("stacked"), "B5 SHORT blocks not retained as products");

  // E. 数值等价：与直接顺序计算逐位一致
  {
    const Block* fits = f1.product("fits");
    check(fits && fits->f64(), "E1 product 'fits' has f64 view");
    const std::vector<double> want = direct_p1_reference(frames);
    bool bitwise = (fits != nullptr && fits->f64() != nullptr);
    std::size_t first = 0;
    if (bitwise)
      for (std::size_t i = 0; i < kN; ++i)
        if (fits->f64()[i] != want[i]) { bitwise = false; first = i; break; }
    check(bitwise, "E2 block-flow result BITWISE equal to direct sequential reference" +
                       (bitwise ? std::string() : " (first diff at " + std::to_string(first) + ")"));
    if (bitwise) {
      std::printf("INFO normalize.fits[0..2] = %.1f %.1f %.1f (bitwise identical to reference)\n",
                  fits->f64()[0], fits->f64()[1], fits->f64()[2]);
    } else if (fits && fits->f64()) {
      std::printf("INFO first diff at index %zu: got=%.17g want=%.17g\n", first,
                  fits->f64()[first], want[first]);
    }
  }

  // F. 确定性
  {
    StageBlockFlow g1(p1, nullptr);
    g1.set_input_f64("frames", frames);
    BlockFlowOutcome o2 = g1.run_unit("frame-0001");
    check(o2.ok && o2.checksum == o1.checksum,
          "F1 repeat run yields identical product checksum");
  }

  // C. fail-closed 负例
  {
    // C1 未声明写（名字级）：算子写了一个没声明的块名
    StageBlockFlowSpec bad = p1;
    for (auto& n : bad.nodes) {
      if (n.module_id != "astrocs.phase1.calibration") continue;
      n.run = [](BlockFrame& fr) -> bool {
        BlockMeta m; m.name = "not_declared"; m.producer = "x"; m.dtype = BlockDtype::F64;
        fr.create(m, kN);
        BlockMeta m2; m2.name = "calibrated"; m2.producer = "x"; m2.dtype = BlockDtype::F64;
        fr.create(m2, kN);
        return true;
      };
    }
    StageBlockFlow fb(bad, nullptr);
    fb.set_input_f64("frames", frames);
    BlockFlowOutcome ob = fb.run_unit("u");
    check(!ob.ok && ob.error.find("undeclared_or_missing_write") != std::string::npos &&
              ob.error.find("astrocs.phase1.calibration") != std::string::npos,
          "C1 undeclared write (name-level) rejected: " + ob.error);
  }
  {
    // C2 缺块：算子不创建它声明的写块
    StageBlockFlowSpec bad = p1;
    for (auto& n : bad.nodes) {
      if (n.module_id != "astrocs.phase1.cosmetic") continue;
      n.run = [](BlockFrame&) -> bool { return true; };   // 什么都不写
    }
    StageBlockFlow fb(bad, nullptr);
    fb.set_input_f64("frames", frames);
    BlockFlowOutcome ob = fb.run_unit("u");
    check(!ob.ok && ob.error.find("undeclared_or_missing_write") != std::string::npos,
          "C2 missing declared write rejected: " + ob.error);
  }
  {
    // C3 缺输入块：上游被改成不产出，下游读取时必须判红
    StageBlockFlowSpec bad = p1;
    for (auto& n : bad.nodes) {
      if (n.module_id != "astrocs.phase1.calibration") continue;
      n.reads.clear();   // 不再读 frames（但写块仍创建）
    }
    StageBlockFlow fb(bad, nullptr);
    BlockFlowOutcome ob = fb.run_unit("u");   // 未注入 frames 也应失败
    check(!ob.ok, "C3 unit without declared external input fails: " + ob.error);
  }
  {
    // C4 节点失败传播（带节点名）
    StageBlockFlowSpec bad = p1;
    for (auto& n : bad.nodes) {
      if (n.module_id != "astrocs.phase1.drizzle") continue;
      n.run = [](BlockFrame&) -> bool { return false; };
    }
    StageBlockFlow fb(bad, nullptr);
    fb.set_input_f64("frames", frames);
    BlockFlowOutcome ob = fb.run_unit("u");
    check(!ob.ok && ob.error == "node_failed:astrocs.phase1.drizzle",
          "C4 node failure propagates with node id: " + ob.error);
  }
  {
    // C5 非法块图：声明一个消费者不在节点集合内的块 ⇒ 构建期判红
    StageBlockFlowSpec bad = p1;
    bad.consumers["wcs"].push_back("astrocs.phase9.ghost");
    StageBlockFlow fb(bad, nullptr);
    check(!fb.spec_valid(), "C5 invalid block graph rejected at build time");
  }

  // 产品证据落盘
  {
    std::ofstream ev(std::string(ARCH505_EVIDENCE_DIR) + "/arch505_block_flow.txt", std::ios::trunc);
    if (ev) {
      ev << "# ARCH-505 块流执行器实测\n"
         << "normalize_nodes=" << p1.nodes.size() << "\n"
         << "mosaic_nodes=" << p2.nodes.size() << "\n"
         << "export_nodes=" << p3.nodes.size() << "\n"
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
