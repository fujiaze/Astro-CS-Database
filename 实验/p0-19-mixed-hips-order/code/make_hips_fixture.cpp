// 实验/p0-19-mixed-hips-order/code/make_hips_fixture.cpp
//
// P0-19（Issue #19）测试夹具生成器：用**生产 HiPS 写侧**（lib/infrastructure/aio/
// include/aio_hips.h 的 aio_hips_product_begin / aio_hips_write_signal_support_tile /
// aio_hips_finalize）写出真实可读的 IVOA HiPS 512-tile 产品集。
//
// 为什么不用手写文本伪产品：coverage 的真实入口 inspect_frame 经 aio_hips_open 读
// manifest.json（§10 完成清单 fail-closed）+ signal/properties + Moc.fits；手写
// 极易与生产格式漂移，不能证明「合法可读输入」。本工具产出的输入与生产同源。
//
// 用法：
//   make_hips_fixture <out_dir> <hips_order K> <obs_filter> [signal_offset]
//   · 叶级 nside = 2^(K+9)，一个 tile（NESTED parent_ipix=0），512x512。
//   · signal = flux_sum / covered_area，flux_sum 为**非恒定**线性梯度（而非常数场），
//     避免"常数场伪装守恒"这类无牙齿判据（Issue #19「不依赖噪声与 PSF 的解析反例」）。
#include "aio_hips.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr,
                 "usage: %s <out_dir> <hips_order K> <obs_filter> "
                 "[signal_offset] [tile_ipix]\n",
                 argv[0]);
    return 2;
  }
  const std::string out = argv[1];
  const int order = std::atoi(argv[2]);
  const std::string filter = argv[3];
  const double offset = (argc >= 5) ? std::atof(argv[4]) : 0.0;
  const std::uint64_t tile_ipix = (argc >= 6)
                                      ? std::strtoull(argv[5], nullptr, 10)
                                      : 0ull;
  if (order < 0 || order > 8) {
    std::fprintf(stderr, "hips_order out of supported fixture range [0,8]\n");
    return 2;
  }
  const uint32_t nside = 1u << static_cast<uint32_t>(order + 9);

  AioHipsProductSet* ps = aio_hips_product_begin(
      out.c_str(), nside, 512u, AIO_HIPS_FLOAT64,
      AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
      "ivo://acsd/test/p0-19", "P0-19 mixed hips_order fixture",
      filter.c_str(), 100.0, "2026-10-16", static_cast<uint32_t>(order));
  if (!ps) {
    std::fprintf(stderr, "aio_hips_product_begin failed: %s\n",
                 aio_hips_last_error());
    return 1;
  }

  std::vector<double> flux(512u * 512u);
  std::vector<double> area(512u * 512u, 1.0);
  for (uint64_t i = 0; i < flux.size(); ++i)
    flux[i] = offset + 1.0 + 1e-3 * static_cast<double>(i);  // 非恒定真值

  AstroSphereTileView v;
  aio_hips_tile_view_abi_init(&v);
  v.parent_ipix = tile_ipix;
  v.leaf_order = static_cast<uint32_t>(order + 9);
  v.width = 512u;
  v.data_type = AIO_HIPS_FLOAT64;
  v.flux_sum = flux.data();
  v.covered_area = area.data();
  v.valid_mask = nullptr;
  v.var_num_sum = nullptr;

  const int wrc = aio_hips_write_signal_support_tile(ps, &v);
  if (wrc != 0) {
    std::fprintf(stderr, "write_signal_support_tile failed rc=%d: %s\n", wrc,
                 aio_hips_last_error());
    aio_hips_abort(ps);
    return 1;
  }
  const int frc = aio_hips_finalize(ps);
  if (frc != 0) {
    std::fprintf(stderr, "aio_hips_finalize failed rc=%d: %s\n", frc,
                 aio_hips_last_error());
    return 1;
  }
  std::printf("ok out=%s hips_order=%d filter=%s nside=%u tile_ipix=%llu\n",
              out.c_str(), order, filter.c_str(), nside,
              (unsigned long long)tile_ipix);
  return 0;
}
