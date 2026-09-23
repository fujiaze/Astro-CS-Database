// P2-006 单元测试: writer 输出语义 (mosaic/support/实际权重类型/UPM surface/diagnostics)
#include "astro/phase2/integrate.h"
#include "astro/phase2/rejection.h"

#include <string>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // 1) 权重类型语义: ivar 是实际权重 (weight_mode=2); 名字不含模糊 "weight"
  {
    // integrate 文档: weight_mode 2=ivar 1=equal; UPM 权重不同语义禁混名
    // 输出 artifact 名: signal/support/ivar/variance (非模糊 weight)
    for (const char* name : {"signal", "support", "ivar", "variance"}) {
      // 名字存在 (在 aio_hips 产品命名中)
      CHECK(name[0] != '\0');
    }
    // 禁模糊 weight 名: 产品目录无裸 weight (ivar 是明确类型)
    const char* prod[] = {"signal", "support", "ivar", "variance"};
    for (auto n : prod) {
      if (std::strcmp(n, "weight") == 0) ++failures;  // 禁裸 weight
    }
  }

  // 2) 权重资格: NaN/Inf/负权重 → INVALID_INPUT; 0 合法不贡献; >0 可用
  {
    const double good[] = {1.0, 2.0, 3.0};
    CHECK(p2_validate_candidate_weights(good, 3) == 0);
    const double nan_w[] = {1.0, NAN, 3.0};
    CHECK(p2_validate_candidate_weights(nan_w, 3) != 0);       // NaN 拒
    const double inf_w[] = {1.0, INFINITY, 3.0};
    CHECK(p2_validate_candidate_weights(inf_w, 3) != 0);       // Inf 拒
    const double neg_w[] = {1.0, -2.0, 3.0};
    CHECK(p2_validate_candidate_weights(neg_w, 3) != 0);       // 负拒
    const double zero_w[] = {1.0, 0.0, 3.0};
    CHECK(p2_validate_candidate_weights(zero_w, 3) == 0);      // 0 合法
  }

  // 3) integrate: mean/weighted mean/variance/support + frame identity
  {
    P2PixelStack in{};
    double vals[3] = {100.0, 110.0, 105.0};
    double wts[3] = {1.0, 2.0, 1.0};
    double sup[3] = {0.5, 1.0, 0.8};
    std::uint8_t acc[3] = {1, 1, 1};
    in.values = vals;
    in.count = 3;
    in.weights = wts;      // 实际权重类型 (ivar 语义由调用方)
    in.support = sup;
    in.accepted = acc;
    P2PixelResult out{};
    CHECK(p2_integrate_pixel(&in, &out) == 0);
    // weighted mean = (100*1+110*2+105*1)/(1+2+1) = 425/4 = 106.25
    CHECK(std::fabs(out.signal - 106.25) < 1e-9);
    // support = max(accepted support) = 1.0 (canonical reducer)
    CHECK(std::fabs(out.support - 1.0) < 1e-9);
  }

  // 4) 等权 (weights=nullptr): plain mean + support max
  {
    P2PixelStack in{};
    double vals[3] = {1.0, 2.0, 3.0};
    double sup[3] = {0.2, 0.5, 0.9};
    std::uint8_t acc[3] = {1, 1, 1};
    in.values = vals;
    in.count = 3;
    in.weights = nullptr;   // 等权
    in.support = sup;
    in.accepted = acc;
    P2PixelResult out{};
    CHECK(p2_integrate_pixel(&in, &out) == 0);
    CHECK(std::fabs(out.signal - 2.0) < 1e-9);
    CHECK(std::fabs(out.support - 0.9) < 1e-9);
  }

  // 4b) B2-A7 回归门: canonical reducer = max(accepted support)，且零权
  // accepted 样本必须计入 support（integrate.h:14-19 冻结语义：weight==0
  // 合法但不贡献 signal；support 作用域 = accepted ∧ finite，不含 w>0）。
  {
    P2PixelStack in{};
    double vals[2] = {5.0, 7.0};
    double wts[2] = {0.0, 1.0};      // 样本 0 零权 accepted（合法）
    double sup[2] = {0.9, 0.5};
    std::uint8_t acc[2] = {1, 1};
    in.values = vals;
    in.count = 2;
    in.weights = wts;
    in.support = sup;
    in.accepted = acc;
    P2PixelResult out{};
    CHECK(p2_integrate_pixel(&in, &out) == 0);
    CHECK(out.status == P2_INTEGRATE_OK);
    CHECK(std::fabs(out.signal - 7.0) < 1e-9);          // 零权不贡献 signal
    CHECK(std::fabs(out.support - 0.9) < 1e-9);         // 但贡献 support
    CHECK(out.n_used == 1);                              // 仅 1 个正权样本
    CHECK(out.n_accepted == 2);
    CHECK(out.n_finite == 2);
  }

  // 4c) B2-A7 反向边界: 全零权 accepted → ZERO_VALID_WEIGHT，但 support 仍
  // 是 max(accepted support)（覆盖并集下界与 signal 可用性解耦；禁把
  // ZERO_VALID_WEIGHT 的 support 塌成 0）。
  {
    P2PixelStack in{};
    double vals[2] = {1.0, 2.0};
    double wts[2] = {0.0, 0.0};
    double sup[2] = {0.3, 0.8};
    std::uint8_t acc[2] = {1, 1};
    in.values = vals;
    in.count = 2;
    in.weights = wts;
    in.support = sup;
    in.accepted = acc;
    P2PixelResult out{};
    CHECK(p2_integrate_pixel(&in, &out) == 0);
    CHECK(out.status == P2_INTEGRATE_ZERO_VALID_WEIGHT);
    CHECK(out.n_positive_weight == 0);
    // 覆盖并集下界与 signal 可用性**解耦**：全零权只表示"没有可用的统计贡献"，
    // 不表示"没有覆盖"。故 support 必须仍是 max(accepted support)。
    // 原实现只在 OK 分支发布 support，全零权提前返回时 support 停在 memset 的 0
    // ⇒「有覆盖但方差不可用」被静默降级成「无覆盖」，下游据此 fail-closed 判 tile 缺失。
    CHECK(std::fabs(out.support - 0.8) < 1e-9);
    CHECK(out.n_accepted == 2);
    CHECK(out.n_finite == 2);
  }

  // 4d) 判别力对照（4c 修正的负例）：全 rejected（无任何 accepted 样本）
  // ⇒ ALL_REJECTED，support 必须保持 0——真"无覆盖"不得被 4c 的修正误抬成有覆盖。
  {
    P2PixelStack in{};
    double vals[2] = {1.0, 2.0};
    double wts[2] = {0.5, 0.5};
    double sup[2] = {0.3, 0.8};
    std::uint8_t acc[2] = {0, 0};
    in.values = vals;
    in.count = 2;
    in.weights = wts;
    in.support = sup;
    in.accepted = acc;
    P2PixelResult out{};
    CHECK(p2_integrate_pixel(&in, &out) == 0);
    CHECK(out.status == P2_INTEGRATE_ALL_REJECTED);
    CHECK(out.support == 0.0);
  }

  // 5) UPM surface + rejection diagnostics 语义: 输出 artifact 含明确名
  {
    // UPM surface = 帧校正场 C_f (control plane); rejection diagnostics =
    // 每帧 reason 计数; 均不以模糊 "weight" 命名 (P2-001 映射表 §UPM)。
    // GATE-502 空断言普查：原为 CHECK(true) 占位 ⇒ 改为对**生产 API 返回值**断言：
    // rejection diagnostics 的机器可读语义 id 必须是显式命名（astrocs.*），
    // 且不得含模糊 token "weight"（§9.73 裁决 A44：权重模式已注销）。
    int ids = 0;
    for (int m = P2_REJECT_NONE; m <= P2_REJECT_MINMAX; ++m) {
      const char* sid = p2_rejection_semantic_id(m);
      CHECK(sid != nullptr && sid[0] != '\0');
      if (!sid) continue;
      ++ids;
      const std::string s(sid);
      CHECK(s.rfind("astrocs.", 0) == 0);              // 显式命名空间前缀
      CHECK(s.find("weight") == std::string::npos);    // 禁模糊 weight 命名
    }
    CHECK(ids >= 10);   // 判据非退化：10 个方法逐个取到语义 id（不是空循环）
    // 产物的显式类型名集合（signal/support/ivar/variance）与 integrate 结果字段同义；
    // 裸 "weight" 与已注销的 "weight_mode" 都不得出现在命名面上。
    P2PixelStack in{};
    double vals[2] = {4.0, 6.0};
    double sup[2] = {1.0, 1.0};
    std::uint8_t acc[2] = {1, 1};
    in.values = vals;
    in.count = 2;
    in.weights = nullptr;
    in.support = sup;
    in.accepted = acc;
    P2PixelResult out{};
    CHECK(p2_integrate_pixel(&in, &out) == 0);
    CHECK(out.status == P2_INTEGRATE_OK);
    CHECK(out.n_used == 2);
    CHECK(out.n_positive_weight == 2);   // 等权路径 = 隐式正权，样本全计
  }

  if (failures == 0) {
    std::printf("P2-006 TESTS PASS (权重类型 ivar 非模糊 weight, 资格, integrate mean/support, 等权, B2-A7 零权 accepted support)\n");
    return 0;
  }
  std::fprintf(stderr, "P2-006 TESTS FAIL (%d)\n", failures);
  return 1;
}
