// eng/tests/unit/core_block_frame_test.cpp — ARCH-501 命名块生命周期回归锁
//
// 覆盖（CONTRACT-501 docs/contracts/PIPELINE_BLOCK_CONTRACT.md）：
//   A. 状态机全分支：CREATED → CONSUMED → DESTROYED；多消费者引用计数；
//   B. 内存即时归还（raw→calibrated→photo 链，销毁点 bytes_alive 下降）；
//   C. DAG 校验器四类非法图（消费不存在 / 重复生产 / 生命周期不一致 / 名字非法）
//      各有红例，合法图绿；
//   D. 缺块显式降级（optional）与非 optional 缺块的负例；
//   E. 取消路径无泄漏（destroy_all 归还全部字节）；
//   F. 无跨帧/全局单例状态（两个 frame 互不影响）。
#include "astrocs/core/block_frame.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace astrocs::core;

namespace {
int g_fail = 0;
int g_total = 0;
void check(bool ok, const char* what) {
  ++g_total;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL %s\n", what);
  }
}

BlockMeta mk(const std::string& name, BlockDtype dt, std::vector<std::int64_t> shape,
             std::vector<std::string> consumers, BlockLifecycle lc = BlockLifecycle::FRAME,
             bool optional = false) {
  BlockMeta m;
  m.name = name;
  m.shape = std::move(shape);
  m.dtype = dt;
  m.unit = "ADU";
  m.optional = optional;
  m.producer = "n0";
  m.consumers = std::move(consumers);
  m.lifecycle = lc;
  return m;
}

std::size_t elems(const std::vector<std::int64_t>& shape) {
  std::size_t n = 1;
  for (auto d : shape) n *= static_cast<std::size_t>(d);
  return n;
}
}  // namespace

int main() {
  // ── A. 状态机与引用计数 ──────────────────────────────────────────────
  {
    BlockFrame f;
    Block* b = f.create(mk("calibrated", BlockDtype::F64, {4, 4}, {"photo", "psf"}), 16);
    check(b != nullptr, "A1 create");
    check(b->state() == BlockState::CREATED, "A2 initial state CREATED");
    check(f.blocks_alive() == 1, "A3 alive=1");
    check(!f.consume("calibrated", "photo"), "A4 first consumer does not destroy");
    check(f.blocks_alive() == 1, "A5 still alive after 1/2 consumers");
    check(f.consume("calibrated", "psf"), "A6 last consumer destroys");
    check(f.blocks_alive() == 0, "A7 destroyed after all consumers");
    check(f.find("calibrated") == nullptr, "A8 find() null after destroy");
    check(!f.consume("calibrated", "photo"), "A9 consume after destroy fails (no silent ok)");
    // 未声明的消费者不得消费
    Block* c = f.create(mk("wcs", BlockDtype::F64, {2}, {"solve"}), 2);
    check(c != nullptr && !f.consume("wcs", "stranger"), "A10 undeclared consumer rejected");
  }

  // ── B. 内存即时归还（raw → calibrated → photo 链）────────────────────
  {
    BlockFrame f;
    const std::size_t n = 1024 * 1024;   // 8 MiB @f64
    f.create(mk("raw", BlockDtype::F64, {static_cast<std::int64_t>(n)}, {"cal"}), n);
    const std::size_t after_raw = f.bytes_alive();
    check(after_raw == n * 8, "B1 raw bytes accounted");
    f.create(mk("calibrated", BlockDtype::F64, {static_cast<std::int64_t>(n)}, {"photo"}), n);
    check(f.bytes_alive() == 2 * after_raw, "B2 two blocks alive");
    f.consume("raw", "cal");
    check(f.bytes_alive() == after_raw, "B3 raw reclaimed immediately after its consumer");
    f.create(mk("photo", BlockDtype::F64, {static_cast<std::int64_t>(n)}, {"snr"}), n);
    check(f.bytes_alive() == 2 * after_raw, "B4 photo allocated");
    f.consume("calibrated", "photo");
    check(f.bytes_alive() == after_raw, "B5 calibrated reclaimed immediately");
    f.consume("photo", "snr");
    check(f.bytes_alive() == 0, "B6 all reclaimed at end of chain");
    check(f.blocks_total_created() == 3, "B7 created count");
  }

  // ── C. DAG 校验器（四类非法图 + 合法图）──────────────────────────────
  {
    // C0 合法图
    BlockGraph ok;
    ok.node_ids = {"n0", "n1", "n2"};
    ok.blocks.push_back(mk("raw", BlockDtype::F64, {8}, {"n1"}));
    ok.blocks.push_back(mk("calibrated", BlockDtype::F64, {8}, {"n2"}));
    check(BlockDagValidator::validate(ok).empty(), "C0 legal graph green");

    // C1 重复生产
    BlockGraph dup = ok;
    dup.blocks.push_back(mk("raw", BlockDtype::F64, {8}, {"n2"}));
    {
      auto is = BlockDagValidator::validate(dup);
      bool hit = false;
      for (const auto& i : is) hit = hit || i.kind == BlockGraphError::DUPLICATE_PRODUCER;
      check(hit, "C1 duplicate producer red");
    }

    // C2 生命周期不一致（short 却有两个消费者）
    BlockGraph bad_lc = ok;
    bad_lc.blocks.push_back(
        mk("scratch", BlockDtype::F32, {8}, {"n1", "n2"}, BlockLifecycle::SHORT));
    {
      auto is = BlockDagValidator::validate(bad_lc);
      bool hit = false;
      for (const auto& i : is) hit = hit || i.kind == BlockGraphError::LIFECYCLE_MISMATCH;
      check(hit, "C2 lifecycle mismatch red");
    }

    // C3 名字非法
    BlockGraph bad_name = ok;
    bad_name.blocks.push_back(mk("BadName", BlockDtype::U8, {8}, {"n1"}));
    {
      auto is = BlockDagValidator::validate(bad_name);
      bool hit = false;
      for (const auto& i : is) hit = hit || i.kind == BlockGraphError::EMPTY_NAME;
      check(hit, "C3 illegal name red");
    }

    // C4 消费不存在（非 optional 且无生产者）
    BlockGraph missing = ok;
    BlockMeta ext = mk("external_input", BlockDtype::F64, {8}, {"n1"});
    ext.producer.clear();
    ext.optional = false;
    missing.blocks.push_back(ext);
    {
      auto is = BlockDagValidator::validate(missing);
      bool hit = false;
      for (const auto& i : is) hit = hit || i.kind == BlockGraphError::CONSUME_MISSING;
      check(hit, "C4 consume-missing red");
    }

    // C5 消费者不在节点集合
    BlockGraph bad_node = ok;
    bad_node.blocks.push_back(mk("tmp", BlockDtype::U8, {8}, {"nope"}));
    {
      auto is = BlockDagValidator::validate(bad_node);
      bool hit = false;
      for (const auto& i : is) hit = hit || i.kind == BlockGraphError::UNKNOWN_NODE;
      check(hit, "C5 unknown consumer red");
    }
  }

  // ── D. 缺块显式降级 ──────────────────────────────────────────────────
  {
    BlockFrame f;
    // optional 块缺失 ⇒ 消费方必须显式登记降级原因
    check(f.find("variance") == nullptr, "D1 optional block absent");
    f.mark_degraded("variance", "no_star_mask_input");
    check(f.degradations().size() == 1 &&
              f.degradations()[0].second == "no_star_mask_input",
          "D2 degradation recorded explicitly");
    // 退化面不挂帧：不得创建空块冒充存在
    Block* empty = f.create(mk("empty", BlockDtype::F64, {0}, {"n1"}), 0);
    check(empty != nullptr && empty->bytes() == 0, "D3 zero-element block is explicit, not a fake");
  }

  // ── E. 取消路径无泄漏 ────────────────────────────────────────────────
  {
    BlockFrame f;
    const std::size_t n = 4096;
    f.create(mk("a", BlockDtype::F64, {static_cast<std::int64_t>(n)}, {"x"}), n);
    f.create(mk("b", BlockDtype::F32, {static_cast<std::int64_t>(n)}, {"y"}), n);
    const std::size_t before = f.bytes_alive();
    check(before == n * 8 + n * 4, "E1 bytes before cancel");
    const std::size_t freed = f.destroy_all();
    check(freed == before, "E2 cancel path returns all bytes");
    check(f.bytes_alive() == 0 && f.blocks_alive() == 0, "E3 no leak after cancel");
  }

  // ── F. provenance 流转 + 无跨帧状态 ──────────────────────────────────
  {
    BlockFrame f1, f2;
    Block* b = f1.create(mk("calibrated", BlockDtype::F64, {2}, {"photo"}), 2);
    check(b != nullptr, "F1 create");
    b->set_provenance("frame_id", "42");
    b->set_provenance("photometry_applied", "1");
    const Block* same = f1.find("calibrated");
    check(same != nullptr && same->provenance().at("frame_id") == "42" &&
              same->provenance().at("photometry_applied") == "1",
          "F2 provenance flows with the block");
    check(f2.find("calibrated") == nullptr && f2.bytes_alive() == 0,
          "F3 no cross-frame / global singleton state");
  }

  std::printf("CORE-BLOCK-FRAME: %d/%d checks passed, %d failed\n", g_total - g_fail,
              g_total, g_fail);
  return g_fail == 0 ? 0 : 1;
}
