// baseline_kernels_impl.inc — baseline 生产 kernel 实现 (ABI-003, 被 baseline_backend.cpp include)
// 并发合同(全部 kernel): reentrant=yes; threadsafe=yes(输出区间互斥, 无共享写);
// internal_parallel=per-call std::thread worker, 数量由 host budget 全或无租借决定
// (Σactive ≤ max_workers, ARCH-004 §1; 无持久线程池——调用后 join);
// 取消点=kernel 调用边界(v1); 确定性=输出元素间独立、无跨线程浮点归约 →
// 结果逐位不随 worker 数变化(ARCH-004 §4; Oracle 测试以 budget 1 vs 4 双跑证明)。

namespace {

using std::size_t;

/* 并行外层: [0,n) 分 workers 个连续行带; 全或无租借, 失败减半, 1=串行兜底。
 * body(y0, y1, tid): 左闭右开。返回实际 worker 数。 */
uint32_t run_banded(const astrocs_host_services_v1* host, uint32_t n,
                    const std::function<void(uint32_t, uint32_t, uint32_t)>& body) {
    uint32_t cap = host->budget.max_workers;
    if (cap < 1) cap = 1;
    uint32_t workers = 1;
    while (workers < cap && workers < n) workers <<= 1;
    if (workers > cap) workers = cap;
    if (workers > n) workers = n;
    while (workers > 1 &&
           host->budget.acquire(host->budget.user_data, workers) != 0)
        workers >>= 1;                                   // 预算不足 → 减半(05 §6 保守)
    if (workers <= 1) {
        if (host->budget.acquire(host->budget.user_data, 1) == 0) {
            body(0, n, 0);
            host->budget.release(host->budget.user_data, 1);
        } else {
            body(0, n, 0);                               // 预算耗尽仍须可跑(串行)
        }
        return 1;
    }
    const uint32_t base = n / workers, rem = n % workers;
    std::vector<uint32_t> start(workers + 1, 0);
    for (uint32_t t = 0; t < workers; ++t)
        start[t + 1] = start[t] + base + (t < rem ? 1u : 0u);
    std::vector<std::thread> ths;
    ths.reserve(workers - 1);
    for (uint32_t t = 1; t < workers; ++t)
        ths.emplace_back([&body, &start, t]() { body(start[t], start[t + 1], t); });
    body(start[0], start[1], 0);                         // 主线程承担 tid0
    for (auto& th : ths) th.join();
    host->budget.release(host->budget.user_data, workers);
    return workers;
}

/* 中位数: 只依赖多重集(值确定性), 就地排序 */
inline float median_inplace(std::vector<float>& v) {
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

inline bool spans_ok(const acs_span_f32& s, uint64_t need) {
    return s.data != nullptr && s.count >= need;
}

std::atomic<uint32_t> g_last_workers_used{0};

/* 单元素计算器: 每输出元素独立(确定性关键); tid 仅供诊断 */
struct OpComputer {
    const acs_baseline_params_v1* p;
    uint32_t frames;

    void operator()(uint32_t y0, uint32_t y1, uint32_t tid) const {
        (void)tid;
        const uint32_t N = p->w * p->h;
        const float kf = p->k;
        std::vector<float> vals, tmp;
        switch (p->op) {
        case ACS_KOP_CALIBRATION:
            for (uint32_t i = y0; i < y1; ++i)
                p->out0.data[i] = (p->in0.data[i] - p->in1.data[i] - kf * p->in2.data[i]) *
                                  p->in3.data[i];
            break;
        case ACS_KOP_NOISE_REDUCTIONS:
        case ACS_KOP_REJECTION_STATS: {
            const bool is_noise = (p->op == ACS_KOP_NOISE_REDUCTIONS);
            for (uint32_t i = y0; i < y1; ++i) {
                vals.resize(frames);
                for (uint32_t f = 0; f < frames; ++f)
                    vals[f] = p->in0.data[static_cast<size_t>(f) * N + i];
                if (is_noise) {
                    // PAR-006: noise 只输出 med/mad, 不再用原值, 就地排序免一次 tmp 拷贝
                    // 与一个 scratch vector; 中位数只依赖多重集(确定性不变, bitwise 等价)。
                    const float med = median_inplace(vals);      // 就地; 多重集同
                    for (uint32_t q = 0; q < frames; ++q)
                        vals[q] = std::fabs(vals[q] - med);
                    const float mad_sigma = median_inplace(vals) * 1.4826f;
                    p->out0.data[i] = med;
                    if (p->out1.data) p->out1.data[i] = mad_sigma;
                } else {
                    tmp = vals;
                    const float med = median_inplace(tmp);       // tmp 排序; 多重集同
                    for (uint32_t q = 0; q < frames; ++q)
                        tmp[q] = std::fabs(tmp[q] - med);
                    const float mad_sigma = median_inplace(tmp) * 1.4826f;
                    uint32_t cnt = 0;
                    for (uint32_t f = 0; f < frames; ++f)        // 原值(未排序)计数
                        if (std::fabs(vals[f] - med) > kf * mad_sigma) ++cnt;
                    p->out0.data[i] = static_cast<float>(cnt);
                }
            }
            break;
        }
        case ACS_KOP_PSF_BATCH: {
            const float cx = p->in0.data[0], cy = p->in0.data[1];
            for (uint32_t i = y0; i < y1; ++i) {
                const float x = static_cast<float>(i % p->w);
                const float y = static_cast<float>(i / p->w);
                const float r2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                p->out0.data[i] = kf * std::exp(-r2 * 0.5f);
            }
            break;
        }
        case ACS_KOP_DRIZZLE_OVERLAP:
            for (uint32_t i = y0; i < y1; ++i) {
                const float wx = std::max(0.0f, 1.0f - std::fabs(p->in0.data[i]));
                const float wy = std::max(0.0f, 1.0f - std::fabs(p->in1.data[i]));
                p->out0.data[i] = wx * wy;
            }
            break;
        case ACS_KOP_DRIZZLE_ACCUMULATE:
            for (uint32_t i = y0; i < y1; ++i) {
                float acc = 0.0f;
                for (uint32_t f = 0; f < frames; ++f)            // 固定下标序
                    acc += p->in0.data[static_cast<size_t>(f) * N + i] *
                           p->in1.data[static_cast<size_t>(f) * N + i];
                p->out0.data[i] = acc;
            }
            break;
        case ACS_KOP_DRIZZLE_NORMALIZE:
            for (uint32_t i = y0; i < y1; ++i)
                p->out0.data[i] = (p->in1.data[i] > 1e-6f) ? p->in0.data[i] / p->in1.data[i]
                                                           : 0.0f;
            break;
        case ACS_KOP_UPM_SPMV:
            for (uint32_t row = y0; row < y1; ++row) {
                const uint32_t a = static_cast<uint32_t>(p->in2.data[row]);
                const uint32_t b = static_cast<uint32_t>(p->in2.data[row + 1]);
                float acc = 0.0f;
                for (uint32_t k = a; k < b; ++k)                 // 固定列序
                    acc += p->in0.data[k] *
                           p->in3.data[static_cast<uint32_t>(p->in1.data[k])];
                p->out0.data[row] = acc;
            }
            break;
        case ACS_KOP_UPM_RESIDUAL:
            for (uint32_t i = y0; i < y1; ++i)
                p->out0.data[i] = p->in0.data[i] - p->in1.data[i];
            break;
        case ACS_KOP_UPM_WEIGHT_UPDATE:
            for (uint32_t i = y0; i < y1; ++i)
                p->out0.data[i] = std::max(p->in0.data[i], kf);
            break;
        case ACS_KOP_INTEGRATION_ACCUM:
            for (uint32_t i = y0; i < y1; ++i) {
                float acc = 0.0f, wsum = 0.0f;
                for (uint32_t f = 0; f < frames; ++f) {          // 固定下标序
                    acc += p->in1.data[static_cast<size_t>(f) * N + i] *
                           p->in0.data[static_cast<size_t>(f) * N + i];
                    wsum += p->in1.data[static_cast<size_t>(f) * N + i];
                }
                p->out0.data[i] = (wsum > 1e-6f) ? acc / wsum : 0.0f;
            }
            break;
        case ACS_KOP_HIPS_BULK: {
            const uint32_t iw = p->aux0, ih = p->aux1;
            const float s = kf;
            for (uint32_t i = y0; i < y1; ++i) {
                const float x = static_cast<float>(i % p->w) * s;
                const float y = static_cast<float>(i / p->w) * s;
                int x0 = static_cast<int>(std::floor(x)), y0i = static_cast<int>(std::floor(y));
                float fx = x - std::floor(x), fy = y - std::floor(y);
                x0 = std::min(std::max(x0, 0), static_cast<int>(iw) - 2);
                y0i = std::min(std::max(y0i, 0), static_cast<int>(ih) - 2);
                fx = std::min(std::max(fx, 0.0f), 1.0f);
                fy = std::min(std::max(fy, 0.0f), 1.0f);
                const size_t r0 = static_cast<size_t>(y0i) * iw, r1 = r0 + iw;
                const size_t x0s = static_cast<size_t>(x0);
                const float v00 = p->in0.data[r0 + x0s], v10 = p->in0.data[r0 + x0s + 1];
                const float v01 = p->in0.data[r1 + x0s], v11 = p->in0.data[r1 + x0s + 1];
                p->out0.data[i] = (1 - fx) * (1 - fy) * v00 + fx * (1 - fy) * v10 +
                                  (1 - fx) * fy * v01 + fx * fy * v11;
            }
            break;
        }
        default:
            break;
        }
    }
};

/* 分发器: 校验→取消检查→并行行带→workers_used 写回 */
acs_status kernel_dispatch(const astrocs_host_services_v1* host,
                           const void* params, uint32_t params_bytes,
                           const void* in, void* out) {
    (void)in; (void)out;
    if (host == nullptr || host->struct_size != sizeof(astrocs_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (params == nullptr || params_bytes != sizeof(acs_baseline_params_v1))
        return ACS_ERR_PARAM;
    acs_baseline_params_v1 P;
    std::memcpy(&P, params, sizeof(P));
    if (P.head.struct_size != sizeof(acs_baseline_params_v1) ||
        P.head.abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (host->cancel.is_cancelled && host->cancel.is_cancelled(host->cancel.user_data))
        return ACS_ERR_CANCELLED;                        // 取消点=调用边界(v1)
    const uint32_t N = P.w * P.h;
    if (N == 0) return ACS_ERR_PARAM;

    uint32_t frames = 0;
    switch (P.op) {
    case ACS_KOP_CALIBRATION:
        if (!spans_ok(P.in0, N) || !spans_ok(P.in1, N) || !spans_ok(P.in2, N) ||
            !spans_ok(P.in3, N) || !spans_ok(P.out0, N))
            return ACS_ERR_PARAM;
        break;
    case ACS_KOP_NOISE_REDUCTIONS:
    case ACS_KOP_REJECTION_STATS:
        frames = P.aux0;
        if (frames == 0 || P.in0.count < static_cast<uint64_t>(frames) * N ||
            !spans_ok(P.out0, N))
            return ACS_ERR_PARAM;
        if (P.op == ACS_KOP_NOISE_REDUCTIONS && !spans_ok(P.out1, N)) return ACS_ERR_PARAM;
        break;
    case ACS_KOP_PSF_BATCH:
        if (!spans_ok(P.in0, 2) || !spans_ok(P.out0, N)) return ACS_ERR_PARAM;
        break;
    case ACS_KOP_DRIZZLE_OVERLAP:
    case ACS_KOP_DRIZZLE_NORMALIZE:
    case ACS_KOP_UPM_RESIDUAL:
        if (!spans_ok(P.in0, N) || !spans_ok(P.in1, N) || !spans_ok(P.out0, N))
            return ACS_ERR_PARAM;
        break;
    case ACS_KOP_DRIZZLE_ACCUMULATE:
    case ACS_KOP_INTEGRATION_ACCUM:
        frames = P.aux0;
        if (frames == 0 || P.in0.count < static_cast<uint64_t>(frames) * N ||
            P.in1.count < static_cast<uint64_t>(frames) * N || !spans_ok(P.out0, N))
            return ACS_ERR_PARAM;
        break;
    case ACS_KOP_UPM_SPMV:
        if (!spans_ok(P.in0, P.aux0) || !spans_ok(P.in1, P.aux0) || !spans_ok(P.in2, N + 1) ||
            !spans_ok(P.in3, P.aux1) || !spans_ok(P.out0, N))
            return ACS_ERR_PARAM;
        break;
    case ACS_KOP_UPM_WEIGHT_UPDATE:
        if (!spans_ok(P.in0, N) || !spans_ok(P.out0, N)) return ACS_ERR_PARAM;
        break;
    case ACS_KOP_HIPS_BULK:
        if (P.aux0 < 2 || P.aux1 < 2 ||
            !spans_ok(P.in0, static_cast<uint64_t>(P.aux0) * P.aux1) || !spans_ok(P.out0, N))
            return ACS_ERR_PARAM;
        break;
    default:
        return ACS_ERR_UNSUPPORTED;
    }

    OpComputer comp{&P, frames};
    const uint32_t used = run_banded(host, N, comp);
    g_last_workers_used.store(used, std::memory_order_relaxed);
    // workers_used 写回调用方 params(唯一允许的写回字段; 其余保持 const)
    auto* mp = const_cast<acs_baseline_params_v1*>(
        static_cast<const acs_baseline_params_v1*>(params));
    mp->workers_used = used;
    return ACS_OK;
}

}  // namespace
