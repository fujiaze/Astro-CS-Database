#include <cstdint>
// P3-004 单元测试: coverage 与边界语义
// 零覆盖区不生成值; 部分覆盖插值正确; coverage mask 一致性; 边界缺失 NaN。
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 采样模型: value = 有覆盖? 插值 : NaN; coverage mask 独立于值
//
// 7)-10) 段（本次新增）冻结**样本级掩膜口径**（rule_id NAN-SAMPLE-MASK-COVERAGE-NAN,
// 权威 = docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md §2a +
// docs/standards/NUMERIC_STANDARD.md §MUST + ALG-P3-003 §2 G4/§4）:
//   不合格样本 = ¬isfinite（NaN 与 ±Inf 同类）⇒ 从分子、分母、方差三项一并剔除,
//   剩余合格邻域**重归一**; 仅零合格样本 ⇒ value=NaN（覆盖级 NaN）; coverage 只判
//   足迹内有无 tile 像素（值非有限不改 coverage）; 被剔除样本计数必须暴露（强制计数）。
// 本段是**独立参考模型**（不调用生产码, 故不构成生产自证; 生产侧判据见
// lib/algorithms/resample/tests/p3rsmp/p3_nan_mask_test.cpp）。
struct TileSampler {
  int w, h;
  std::vector<double> val{};    // 像素值 (默认空, 避免缺省聚合初始化 warning)
  std::vector<uint8_t> cov{};   // coverage mask (0/1)
  // 采样: 4 邻居全部有覆盖才插值; 否则 NaN (边界/零覆盖语义)
  double sample(double x, double y) const {
    if (x < 0 || x >= w - 1 || y < 0 || y >= h - 1) return NAN;  // 边界缺失
    int x0 = (int)x, y0 = (int)y;
    if (!cov[(size_t)y0 * (size_t)w + (size_t)x0] || !cov[(size_t)y0 * (size_t)w + (size_t)x0 + 1] ||
        !cov[(size_t)(y0 + 1) * (size_t)w + (size_t)x0] || !cov[(size_t)(y0 + 1) * (size_t)w + (size_t)x0 + 1])
      return NAN;   // 邻居覆盖不全 → 不插值 (零覆盖不生成值)
    double fx = x - x0, fy = y - y0;
    double v00 = val[(size_t)y0 * (size_t)w + (size_t)x0];
    double v10 = val[(size_t)y0 * (size_t)w + (size_t)x0 + 1];
    double v01 = val[(size_t)(y0 + 1) * (size_t)w + (size_t)x0];
    double v11 = val[(size_t)(y0 + 1) * (size_t)w + (size_t)x0 + 1];
    return (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy;
  }

  // 样本级掩膜口径版 (返回值 + coverage + 被剔除样本计数)
  struct Masked { double value; int cov; int n_rej; };
  Masked sample_masked(double x, double y) const {
    Masked r{0.0, 0, 0};
    if (x < 0 || x >= w - 1 || y < 0 || y >= h - 1) { r.value = NAN; return r; }
    const int x0 = (int)x, y0 = (int)y;
    // coverage = 足迹内有无 tile 像素 (存在判定); 值非有限不改 coverage
    if (!cov[(size_t)y0 * (size_t)w + (size_t)x0] ||
        !cov[(size_t)y0 * (size_t)w + (size_t)x0 + 1] ||
        !cov[(size_t)(y0 + 1) * (size_t)w + (size_t)x0] ||
        !cov[(size_t)(y0 + 1) * (size_t)w + (size_t)x0 + 1]) {
      r.value = NAN; r.cov = 0; return r;
    }
    r.cov = 1;
    const double fx = x - x0, fy = y - y0;
    const double wg[4] = {(1 - fx) * (1 - fy), fx * (1 - fy),
                          (1 - fx) * fy, fx * fy};
    const double vv[4] = {val[(size_t)y0 * (size_t)w + (size_t)x0],
                          val[(size_t)y0 * (size_t)w + (size_t)x0 + 1],
                          val[(size_t)(y0 + 1) * (size_t)w + (size_t)x0],
                          val[(size_t)(y0 + 1) * (size_t)w + (size_t)x0 + 1]};
    double wsum = 0.0;
    for (int k = 0; k < 4; ++k) {
      if (std::isfinite(vv[k])) wsum += wg[k]; else ++r.n_rej;   // 计数 (强制)
    }
    if (r.n_rej == 4 || !(wsum > 0.0)) { r.value = NAN; return r; }  // 覆盖级 NaN
    double acc = 0.0;
    for (int k = 0; k < 4; ++k) {
      if (!std::isfinite(vv[k])) continue;      // 分子/分母/方差三项一并剔除
      acc += (wg[k] / wsum) * vv[k];            // 剩余合格邻域重归一
    }
    r.value = acc;
    return r;
  }
};

int main() {
  // 1) 全覆盖区: 插值正常
  {
    TileSampler t{4, 4};
    t.val.assign(16, 10.0);
    t.cov.assign(16, 1);
    double v = t.sample(1.5, 1.5);
    CHECK(!std::isnan(v) && std::fabs(v - 10.0) < 1e-9);
  }

  // 2) 零覆盖区: 不生成值 (NaN)
  {
    TileSampler t{4, 4};
    t.val.assign(16, 10.0);
    t.cov.assign(16, 1);
    t.cov[2 * 4 + 2] = 0;   // (2,2) 无覆盖
    // 采样 (2.2, 2.2) 邻居含 (2,2) → NaN
    double v = t.sample(2.2, 2.2);
    CHECK(std::isnan(v));   // 零覆盖不生成值
  }

  // 3) 部分覆盖: 全覆盖邻居区仍可插值
  {
    TileSampler t{4, 4};
    t.val.assign(16, 10.0); t.cov.assign(16, 1);
    t.cov[3 * 4 + 3] = 0;   // 角部无覆盖
    double v = t.sample(1.5, 1.5);   // 远离角部
    CHECK(!std::isnan(v));
    CHECK(std::fabs(v - 10.0) < 1e-9);
  }

  // 4) 边界缺失: 越界 → NaN
  {
    TileSampler t{4, 4};
    t.val.assign(16, 5.0);
    t.cov.assign(16, 1);
    CHECK(std::isnan(t.sample(-0.5, 1.0)));
    CHECK(std::isnan(t.sample(1.0, 4.0)));
    CHECK(std::isnan(t.sample(3.9, 1.0)));   // x>=w-1 边界
  }

  // 5) coverage mask 一致性: mask 与值数组同尺寸; 采样结果仅由 mask+值决定
  {
    TileSampler t{4, 4};
    t.val.assign(16, 0.0);
    t.cov.assign(16, 1);
    t.val[1 * 4 + 1] = 100.0;   // 局部峰值
    double v = t.sample(1.0, 1.0);
    CHECK(std::fabs(v - 100.0) < 1e-9);
  }

  // 6) 1-thread reference 语义: 单线程采样结果确定 (同输入同输出)
  {
    TileSampler t{4, 4};
    t.val.assign(16, 0.0); t.cov.assign(16, 1);
    for (int i = 0; i < 16; ++i) { t.val[static_cast<size_t>(i)] = (double)i; }
    double a = t.sample(2.3, 1.7);
    double b = t.sample(2.3, 1.7);
    CHECK(a == b);   // 确定性 (1-thread reference 可复现)
  }

  // 7) 样本级掩膜 + 重归一: 4 邻居全覆盖但 1 个值为 NaN ⇒ 剩余 3 个按权重**重归一**
  //    (解析真值: (0.1875*10 + 0.0625*20 + 0.5625*30) / 0.8125 = 24.615384615384613)
  {
    TileSampler t{4, 4};
    t.val.assign(16, 0.0); t.cov.assign(16, 1);
    t.val[1 * 4 + 1] = 10.0; t.val[1 * 4 + 2] = 20.0;
    t.val[2 * 4 + 1] = 30.0; t.val[2 * 4 + 2] = 40.0;
    t.val[2 * 4 + 2] = NAN;                       // 角 (2,2) 不合格
    const TileSampler::Masked r = t.sample_masked(1.25, 1.75);
    CHECK(r.cov == 1);                            // 值 NaN 不改 coverage
    CHECK(r.n_rej == 1);                          // 强制计数
    CHECK(!std::isnan(r.value));
    CHECK(std::fabs(r.value - 24.615384615384613) < 1e-12);
    // 非退化锚: 真值与「等权平均剩余 3 个」(=20) 差 >20%, 与「零填」(NaN→0) 亦显著不同
    CHECK(std::fabs(r.value - 20.0) / 20.0 > 0.2);
    const double zero_fill = (0.1875 * 10.0 + 0.0625 * 20.0 + 0.5625 * 30.0 + 0.1875 * 0.0);
    CHECK(std::fabs(r.value - zero_fill) / r.value > 0.01);   // 禁「零填」替代语义
  }

  // 8) 2/3 个不合格 ⇒ 逐级重归一; 4 个全不合格 ⇒ NaN + coverage=1 (覆盖级 NaN)
  {
    TileSampler t{4, 4};
    t.val.assign(16, 0.0); t.cov.assign(16, 1);
    t.val[1 * 4 + 1] = 10.0; t.val[1 * 4 + 2] = 20.0;
    t.val[2 * 4 + 1] = 30.0; t.val[2 * 4 + 2] = 40.0;
    t.val[1 * 4 + 1] = NAN;
    TileSampler::Masked r = t.sample_masked(1.25, 1.75);
    CHECK(r.n_rej == 1 && r.cov == 1 && !std::isnan(r.value));
    t.val[1 * 4 + 2] = NAN;
    r = t.sample_masked(1.25, 1.75);
    CHECK(r.n_rej == 2 && r.cov == 1 && !std::isnan(r.value));
    CHECK(std::fabs(r.value - ((0.5625 * 30.0 + 0.1875 * 40.0) / 0.75)) < 1e-12);
    t.val[2 * 4 + 1] = NAN;
    r = t.sample_masked(1.25, 1.75);
    CHECK(r.n_rej == 3 && r.cov == 1);
    CHECK(std::fabs(r.value - 40.0) < 1e-12);
    t.val[2 * 4 + 2] = NAN;
    r = t.sample_masked(1.25, 1.75);
    CHECK(r.n_rej == 4 && r.cov == 1);            // 零合格样本
    CHECK(std::isnan(r.value));                   // 覆盖级 NaN (禁零填/禁哨兵)
  }

  // 9) ±Inf 与 NaN 同属不合格样本 (同类掩膜 + 计数)
  {
    TileSampler t{4, 4};
    t.val.assign(16, 0.0); t.cov.assign(16, 1);
    t.val[1 * 4 + 1] = 10.0; t.val[1 * 4 + 2] = 20.0;
    t.val[2 * 4 + 1] = 30.0;
    t.val[2 * 4 + 2] = INFINITY;
    TileSampler::Masked r = t.sample_masked(1.25, 1.75);
    CHECK(r.n_rej == 1 && r.cov == 1 && !std::isnan(r.value));
    CHECK(std::fabs(r.value - 24.615384615384613) < 1e-12);
    t.val[2 * 4 + 2] = -INFINITY;
    r = t.sample_masked(1.25, 1.75);
    CHECK(r.n_rej == 1 && std::fabs(r.value - 24.615384615384613) < 1e-12);
    t.val[2 * 4 + 2] = 40.0;                      // 复位: 仅 3 个不合格 (混合 ±Inf/NaN)
    t.val[1 * 4 + 1] = -INFINITY; t.val[1 * 4 + 2] = INFINITY;
    t.val[2 * 4 + 1] = NAN;
    r = t.sample_masked(1.25, 1.75);
    CHECK(r.n_rej == 3 && r.cov == 1);
    CHECK(std::fabs(r.value - 40.0) < 1e-12);
  }

  // 10) 覆盖语义不受值影响: 角 tile 缺失 ⇒ cov=0 (与值是否非有限无关)
  {
    TileSampler t{4, 4};
    t.val.assign(16, 5.0); t.cov.assign(16, 1);
    t.cov[2 * 4 + 2] = 0;
    const TileSampler::Masked r = t.sample_masked(1.25, 1.75);
    CHECK(r.cov == 0 && std::isnan(r.value));
    CHECK(r.n_rej == 0);                          // 无候选样本 ⇒ 计数 0 (非「字段缺失」)
  }

  if (failures == 0) {
    std::printf("P3-004 TESTS PASS (全覆盖/零覆盖不生成值/部分覆盖/边界 NaN/mask 一致/1T 确定/"
                "样本级掩膜+重归一+强制计数/±Inf 同类/覆盖级 NaN)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-004 TESTS FAIL (%d)\n", failures);
  return 1;
}
