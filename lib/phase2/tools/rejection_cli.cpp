// lib/phase2/tools/rejection_cli.cpp — Rejection Oracle 驱动（V15）
//
// 用途：外部 Oracle（Astropy/NIST/Siril/RCR）对照与边界证据。从 stdin
// 读取 double 值（空白/换行分隔；weighted 模式每行 "weight value"）。
//
// 用法：
//   rejection_cli <method> <lo> <hi> <max_iter> <min_samples> [weighted]
//       —— 旧兼容模式（经 p2_reject_stack COMPAT adapter；auto 以候选数
//          作为 nominal 近似）
//   rejection_cli --plan <plan.json> [--weighted] [--reasons]
//       —— V15 显式 plan（p2_reject_plan_resolve + p2_reject_stack_ex）；
//          plan.json = P2RejectionPlanRequest + method-specific typed 参数
//
// 输出：accepted mask（0/1，空格分隔）+ 统计行；--reasons 时输出第二行
// reason 码（0=accepted 1=low 2=high 3=underdetermined）。
#include "astro/phase2/rejection.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

int run_ex(const P2RejectionPlan& plan, bool weighted, bool reasons_out) {
    std::vector<double> vals;
    std::vector<double> wgts;
    double v = 0.0, w = 0.0;
    if (weighted) {
        while (std::fscanf(stdin, "%lf %lf", &w, &v) == 2) {
            wgts.push_back(w);
            vals.push_back(v);
        }
    } else {
        while (std::fscanf(stdin, "%lf", &v) == 1) vals.push_back(v);
    }
    if (vals.empty()) return 0;
    P2CandidateStack st{};
    st.values = vals.data();
    st.weights = weighted ? wgts.data() : nullptr;
    st.count = (std::uint32_t)vals.size();
    st.data_type = 1;
    std::vector<std::uint8_t> reasons(vals.size(), 0);
    P2RejectionDecision dec{};
    dec.reasons = reasons.data();
    if (p2_reject_stack_ex(&st, &plan, &dec) != 0) return 1;
    for (std::size_t i = 0; i < vals.size(); ++i) {
        if (i) std::fputc(' ', stdout);
        std::fputc(dec.reasons[i] == P2_REASON_ACCEPTED ||
                           dec.reasons[i] == P2_REASON_UNDERDETERMINED
                       ? '1' : '0',
                   stdout);
    }
    if (reasons_out) {
        std::fputc('\n', stdout);
        for (std::size_t i = 0; i < vals.size(); ++i) {
            if (i) std::fputc(' ', stdout);
            std::fputc((char)('0' + dec.reasons[i]), stdout);
        }
    }
    std::fprintf(stdout,
                 "\nstatus=%u accepted=%u rejected_lo=%u rejected_hi=%u "
                 "iterations=%u method=%d\n",
                 dec.status, dec.accepted_count, dec.rejected_low,
                 dec.rejected_high, dec.iterations, plan.method);
    return 0;
}

int run_compat(int method, double lo, double hi, int max_iter,
               int min_samples, int weighted) {
    std::vector<double> vals;
    std::vector<double> wgts;
    double v = 0.0, w = 0.0;
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
    std::fprintf(stdout,
                 "\nstatus=%u accepted=%u rejected_lo=%u rejected_hi=%u "
                 "iterations=%u\n",
                 out.status, out.accepted_count, out.rejected_low,
                 out.rejected_high, out.iterations);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 3 && std::strcmp(argv[1], "--plan") == 0) {
        std::ifstream f(argv[2]);
        if (!f) {
            std::fprintf(stderr, "cannot open plan json: %s\n", argv[2]);
            return 2;
        }
        nlohmann::json j;
        try {
            f >> j;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "plan json parse error: %s\n", e.what());
            return 2;
        }
        bool weighted = false, reasons_out = false;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--weighted") == 0) weighted = true;
            if (std::strcmp(argv[i], "--reasons") == 0) reasons_out = true;
        }
        const std::string req_s = j.value("request", std::string("sigma"));
        static const struct { const char* n; int v; } kMethods[] = {
            {"none", P2_REJECT_NONE}, {"sigma", P2_REJECT_SIGMA},
            {"winsorized_sigma", P2_REJECT_WINSORIZED_SIGMA},
            {"averaged_sigma", P2_REJECT_AVERAGED_SIGMA},
            {"linear_fit", P2_REJECT_LINEAR_FIT},
            {"generalized_esd", P2_REJECT_GENERALIZED_ESD},
            {"rcr", P2_REJECT_RCR}, {"percentile", P2_REJECT_PERCENTILE},
            {"median_sigma", P2_REJECT_MEDIAN_SIGMA},
            {"minmax", P2_REJECT_MINMAX}, {"auto", P2_REJECT_AUTO}};
        int request = -1;
        for (const auto& m : kMethods)
            if (req_s == m.n) { request = m.v; break; }
        if (request < 0) {
            std::fprintf(stderr, "unknown request: %s\n", req_s.c_str());
            return 2;
        }
        P2RejectionPlanRequest req{};
        req.request = request;
        req.nominal_contributors = j.value("nominal", (std::uint32_t)20);
        const std::string profile_str =
            j.value("profile", std::string("wbpp_current"));
        req.profile = profile_str.c_str();
        req.underdetermined_n =
            j.value("underdetermined_n", (std::uint32_t)2);
        P2RejectionPlan plan{};
        char err[160] = {0};
        if (p2_reject_plan_resolve(&req, &plan, err, sizeof(err)) != 0) {
            std::fprintf(stderr, "plan resolve failed: %s\n", err);
            return 2;
        }
        auto sig = [&](const char* key, double dfl) {
            return j.value(key, dfl);
        };
        auto ival = [&](const char* key, int dfl) {
            return j.value(key, dfl);
        };
        plan.sigma.lower_sigma = sig("sigma.lower_sigma", 4.0);
        plan.sigma.upper_sigma = sig("sigma.upper_sigma", 3.0);
        plan.sigma.max_iterations = ival("sigma.max_iterations", 8);
        plan.winsorized.lower_sigma = sig("winsorized.lower_sigma", 4.0);
        plan.winsorized.upper_sigma = sig("winsorized.upper_sigma", 3.0);
        plan.winsorized.max_iterations = ival("winsorized.max_iterations", 8);
        plan.averaged.lower_sigma = sig("averaged.lower_sigma", 4.0);
        plan.averaged.upper_sigma = sig("averaged.upper_sigma", 3.0);
        plan.averaged.max_iterations = ival("averaged.max_iterations", 8);
        plan.linear_fit.lower = sig("linear_fit.lower", 4.0);
        plan.linear_fit.upper = sig("linear_fit.upper", 3.0);
        plan.linear_fit.max_iterations = ival("linear_fit.max_iterations", 8);
        plan.esd.alpha = sig("esd.alpha", 0.05);
        plan.esd.max_outliers = ival("esd.max_outliers", 10);
        plan.percentile.low_fraction = sig("percentile.low_fraction", 0.1);
        plan.percentile.high_fraction =
            sig("percentile.high_fraction", 0.1);
        plan.median_sigma.lower_sigma = sig("median_sigma.lower_sigma", 4.0);
        plan.median_sigma.upper_sigma = sig("median_sigma.upper_sigma", 3.0);
        plan.median_sigma.max_iterations =
            ival("median_sigma.max_iterations", 8);
        plan.minmax.reject_low_count = ival("minmax.reject_low_count", 1);
        plan.minmax.reject_high_count = ival("minmax.reject_high_count", 1);
        plan.minmax.min_kept = ival("minmax.min_kept", 4);
        return run_ex(plan, weighted, reasons_out);
    }

    int method = P2_REJECT_SIGMA;
    double lo = -4.0, hi = 3.0;
    int max_iter = 8, min_samples = 3;
    if (argc >= 2) method = std::atoi(argv[1]);
    if (argc >= 3) lo = std::atof(argv[2]);
    if (argc >= 4) hi = std::atof(argv[3]);
    if (argc >= 5) max_iter = std::atoi(argv[4]);
    if (argc >= 6) min_samples = std::atoi(argv[5]);
    const int weighted = (argc >= 7) ? std::atoi(argv[6]) : 0;
    return run_compat(method, lo, hi, max_iter, min_samples, weighted);
}
