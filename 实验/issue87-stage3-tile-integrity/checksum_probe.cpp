// 实验/issue87-stage3-tile-integrity/checksum_probe.cpp — Issue #87 reader 层每 tile 开销
//
// 对同一真实 M42 writer 产品，遍历 MOC 声明的全部叶级 tile，调用生产整 tile 读取
// 入口。分别链接**修复前**与**修复后** libacsd_hips 编译同一份源码，量化
// DATASUM/CHECKSUM 校验 + 错误分类在 reader 层的每 tile 开销（不含 export 编排/缓存）。
//
// 用法: checksum_probe <hips_root>
// 输出: 多行 "tiles=... ok=... fail=... best_ns_per_tile=..."
#include "aio_hips_reader.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <hips_root>\n", argv[0]);
        return 64;
    }
    AioHipsDataset* d = aio_hips_open(argv[1], AIO_HIPS_RD_SIGNAL);
    if (!d) {
        std::fprintf(stderr, "open failed: %s\n", aio_hips_reader_last_error());
        return 1;
    }
    const int n = aio_hips_tile_count(d);
    std::vector<uint64_t> ips;
    ips.reserve((size_t)(n > 0 ? n : 0));
    for (int i = 0; i < n; ++i) {
        uint64_t ip = 0;
        if (aio_hips_tile_ipix(d, i, &ip) == 0) ips.push_back(ip);
    }
    std::vector<float> buf(512u * 512u);
    // 预热：读一遍（页缓存 + cfitsio 初始化）
    for (uint64_t ip : ips) (void)aio_hips_read_tile_f32(d, ip, buf.data());
    double best = 1e300;
    int ok = 0, fail = 0;
    for (int rep = 0; rep < 3; ++rep) {
        ok = fail = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (uint64_t ip : ips) {
            if (aio_hips_read_tile_f32(d, ip, buf.data()) == 0) ++ok; else ++fail;
        }
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        const double per = (ips.empty() ? 0.0 : ns / (double)ips.size());
        if (per < best) best = per;
        std::printf("tiles=%zu ok=%d fail=%d total_ms=%.3f ns_per_tile=%.0f\n",
                    ips.size(), ok, fail, ns / 1e6, per);
    }
    std::printf("best_ns_per_tile=%.0f\n", best);
    aio_hips_close(d);
    return (fail == 0) ? 0 : 2;
}
