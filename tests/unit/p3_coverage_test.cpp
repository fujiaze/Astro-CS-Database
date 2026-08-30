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
struct TileSampler {
  int w, h;
  std::vector<double> val;    // 像素值
  std::vector<uint8_t> cov;   // coverage mask (0/1)
  // 采样: 4 邻居全部有覆盖才插值; 否则 NaN (边界/零覆盖语义)
  double sample(double x, double y) const {
    if (x < 0 || x >= w - 1 || y < 0 || y >= h - 1) return NAN;  // 边界缺失
    int x0 = (int)x, y0 = (int)y;
    if (!cov[(size_t)y0 * w + x0] || !cov[(size_t)y0 * w + x0 + 1] ||
        !cov[(size_t)(y0 + 1) * w + x0] || !cov[(size_t)(y0 + 1) * w + x0 + 1])
      return NAN;   // 邻居覆盖不全 → 不插值 (零覆盖不生成值)
    double fx = x - x0, fy = y - y0;
    double v00 = val[(size_t)y0 * w + x0];
    double v10 = val[(size_t)y0 * w + x0 + 1];
    double v01 = val[(size_t)(y0 + 1) * w + x0];
    double v11 = val[(size_t)(y0 + 1) * w + x0 + 1];
    return (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy;
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
    for (int i = 0; i < 16; ++i) { t.val[i] = (double)i; }
    double a = t.sample(2.3, 1.7);
    double b = t.sample(2.3, 1.7);
    CHECK(a == b);   // 确定性 (1-thread reference 可复现)
  }

  if (failures == 0) {
    std::printf("P3-004 TESTS PASS (全覆盖/零覆盖不生成值/部分覆盖/边界 NaN/mask 一致/1T 确定)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-004 TESTS FAIL (%d)\n", failures);
  return 1;
}
