// lib/phase2/tools/rejection_cli.cpp — Rejection Oracle 驱动
//
// 用途：外部 Oracle（Astropy/NIST/SciPy）对照用。从 stdin 读取 double 值
//（空白/换行分隔），输出 accepted mask（0/1，空格分隔）与统计。
// 参数：method sigma_low sigma_high max_iterations min_samples [weighted]
// 例：rejection_cli 1 -4 3 8 3 < values.txt
// weighted=1 时 stdin 每行为 "weight value"（RCR 加权 oracle 对照）。
#include "astro/phase2/rejection.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

int main(int argc, char** argv) {
    int method = P2_REJECT_SIGMA;
    double lo = -4.0, hi = 3.0;
    int max_iter = 8, min_samples = 3;
    if (argc >= 2) method = std::atoi(argv[1]);
    if (argc >= 3) lo = std::atof(argv[2]);
    if (argc >= 4) hi = std::atof(argv[3]);
    if (argc >= 5) max_iter = std::atoi(argv[4]);
    if (argc >= 6) min_samples = std::atoi(argv[5]);
    const int weighted = (argc >= 7) ? std::atoi(argv[6]) : 0;

    std::vector<double> vals;
    std::vector<double> wgts;
    double v, w;
    if (weighted) {
        while (std::fscanf(stdin, "%lf %lf", &w, &v) == 2) {
            wgts.push_back(w);
            vals.push_back(v);
        }
    } else {
        while (std::fscanf(stdin, "%lf", &v) == 1) vals.push_back(v);
    }
    if (vals.empty()) return 0;

    P2SampleStackView in{};
    in.values = vals.data();
    in.count = (std::uint32_t)vals.size();
    in.data_type = 1;
    in.method = method;
    in.sigma_low = lo;
    in.sigma_high = hi;
    in.max_iterations = max_iter;
    in.min_samples = min_samples;
    if (weighted) in.weights = wgts.data();
    std::vector<std::uint8_t> acc(vals.size(), 0);
    P2RejectionResult out{};
    out.accepted = acc.data();
    if (p2_reject_stack(&in, &out) != 0) return 1;
    for (std::size_t i = 0; i < acc.size(); ++i) {
        if (i) std::fputc(' ', stdout);
        std::fputc(acc[i] ? '1' : '0', stdout);
    }
    std::fprintf(stdout, "\nstatus=%u accepted=%u rejected_lo=%u rejected_hi=%u iterations=%u\n",
                 out.status, out.accepted_count, out.rejected_low,
                 out.rejected_high, out.iterations);
    return 0;
}
