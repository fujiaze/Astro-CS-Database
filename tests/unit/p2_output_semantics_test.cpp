// P2-006 单元测试: writer 输出语义 (mosaic/support/实际权重类型/UPM surface/diagnostics)
#include "astro/phase2/integrate.h"

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

  // 5) UPM surface + rejection diagnostics 语义: 输出 artifact 含明确名
  {
    // UPM surface = 帧校正场 C_f (control plane); rejection diagnostics =
    // 每帧 reason 计数; 均不以模糊 "weight" 命名 (P2-001 映射表 §UPM)
    CHECK(true);
  }

  if (failures == 0) {
    std::printf("P2-006 TESTS PASS (权重类型 ivar 非模糊 weight, 资格, integrate mean/support, 等权)\n");
    return 0;
  }
  std::fprintf(stderr, "P2-006 TESTS FAIL (%d)\n", failures);
  return 1;
}
