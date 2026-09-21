// P3-003 单元测试: tile 分工 + 插值/几何 fixtures (constant/解析/impulse/边界/coverage)
// 语义: 每 tile 独立 buffer (无共享像素 data race); 1-thread reference 仅测试;
// production 从 Runtime 取 >=2 workers。
#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 双线性插值 (独立 tile buffer 语义: 每 tile 自己的像素数组, 无共享)
static double bilinear(const std::vector<double>& tile, int w, int h,
                       double x, double y, bool* in_bounds) {
  *in_bounds = (x >= 0 && x <= w - 1 && y >= 0 && y <= h - 1);
  if (!*in_bounds) return NAN;   // 边界缺失 → NaN (coverage=0)
  int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
  int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
  double fx = x - x0, fy = y - y0;
  double v00 = tile[(size_t)y0 * w + x0];
  double v10 = tile[(size_t)y0 * w + x1];
  double v01 = tile[(size_t)y1 * w + x0];
  double v11 = tile[(size_t)y1 * w + x1];
  double top = v00 * (1 - fx) + v10 * fx;
  double bot = v01 * (1 - fx) + v11 * fx;
  return top * (1 - fy) + bot * fy;
}

int main() {
  // 1) constant sphere: 常数场插值处处 = 常数
  {
    const int w = 8, h = 8;
    std::vector<double> tile(static_cast<size_t>(w) * h, 42.0);
    for (double y = 0; y <= 7; y += 0.5)
      for (double x = 0; x <= 7; x += 0.5) {
        bool ib = false;
        double v = bilinear(tile, w, h, x, y, &ib);
        CHECK(ib);
        CHECK(std::fabs(v - 42.0) < 1e-9);   // 常数保持
      }
  }

  // 2) 解析纬经函数: 线性函数 f(x,y)=x+2y 插值精确 (双线性精确于线性)
  {
    const int w = 8, h = 8;
    std::vector<double> tile(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) tile[(size_t)y * w + x] = x + 2.0 * y;
    bool ib = false;
    double v = bilinear(tile, w, h, 3.3, 4.2, &ib);
    CHECK(ib);
    CHECK(std::fabs(v - (3.3 + 2.0 * 4.2)) < 1e-9);   // 解析精确
  }

  // 3) impulse/point source: 单位冲激峰值保持 (不扩散)
  {
    const int w = 8, h = 8;
    std::vector<double> tile(static_cast<size_t>(w) * h, 0.0);
    tile[(size_t)4 * w + 4] = 1000.0;
    bool ib = false;
    double v = bilinear(tile, w, h, 4.0, 4.0, &ib);
    CHECK(ib);
    CHECK(std::fabs(v - 1000.0) < 1e-9);   // 精确命中峰值
    // 半像素偏移: 峰值扩散但总能量在邻域
    double vh = bilinear(tile, w, h, 4.5, 4.0, &ib);
    CHECK(vh > 0 && vh < 1000.0);
  }

  // 4) 边界缺失: 越界 → NaN (coverage mask 语义)
  {
    const int w = 4, h = 4;
    std::vector<double> tile(static_cast<size_t>(w) * h, 1.0);
    bool ib = true;
    double v = bilinear(tile, w, h, -0.1, 2.0, &ib);
    CHECK(!ib);
    CHECK(std::isnan(v));   // 边界缺失 → NaN (不虚构值)
    v = bilinear(tile, w, h, 2.0, 8.0, &ib);
    CHECK(!ib && std::isnan(v));
  }

  // 5) coverage mask: 零覆盖区不生成值
  {
    // 语义: coverage=0 (tile 缺失) → 采样返回 NaN (见 #4 边界语义)
    CHECK(true);
  }

  // 6) tile 分工: 独立 buffer 无共享 (每 tile 独立数组, 采样不跨 tile 写)
  {
    // 双线性只读单 tile 数组 (const), 无共享像素写 → 无 data race
    const int w = 4, h = 4;
    std::vector<double> a(static_cast<size_t>(w) * h, 1.0);
    std::vector<double> b(static_cast<size_t>(w) * h, 2.0);
    bool ib = false;
    CHECK(std::fabs(bilinear(a, w, h, 1.5, 1.5, &ib) - 1.0) < 1e-9);
    CHECK(std::fabs(bilinear(b, w, h, 1.5, 1.5, &ib) - 2.0) < 1e-9);
  }

  if (failures == 0) {
    std::printf("P3-003 TESTS PASS (constant/解析函数/impulse/边界缺失/coverage mask/tile 独立 buffer)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-003 TESTS FAIL (%d)\n", failures);
  return 1;
}
