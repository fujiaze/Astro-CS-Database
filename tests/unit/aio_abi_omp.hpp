// AIO-001 · 1-N worker OpenMP 并行复核宏 (测试专用; 与生产零耦合)
// 契约 (重计算并行纪律): worker 数显式传入, 不硬编码线程数; 不同 worker 数
// 结果必须位级一致 (reentrant 复核)。OpenMP 不可用时回退串行 (worker>1 仍验证
// 调用面 reentrant, 并行位不激活)。
#ifndef AIO_ABI_OMP_HPP
#define AIO_ABI_OMP_HPP

#include "astrocs/io/aio_abi_v1.h"

#include <string>
#include <vector>

#if defined(_OPENMP)
#include <omp.h>
#define AIO_ABI_HAS_OMP 1
#endif

inline void aio_abi_omp_parallel_review(std::vector<int>& rc,
                                        const std::vector<std::string>& contents,
                                        const std::vector<std::string>& expect,
                                        const std::vector<int>& lenpattern,
                                        int workers) {
    const int n = static_cast<int>(rc.size());
#if defined(AIO_ABI_HAS_OMP)
    if (workers > 1) {
#pragma omp parallel for num_threads(workers) schedule(static)
        for (int i = 0; i < n; ++i) {
            rc[static_cast<std::size_t>(i)] = aio_content_hash_verify_buffer_v1(
                contents[static_cast<std::size_t>(i)].data(),
                static_cast<std::uint64_t>(lenpattern[static_cast<std::size_t>(i)]),
                expect[static_cast<std::size_t>(i)].c_str());
        }
        return;
    }
#else
    (void)workers;
#endif
    for (int i = 0; i < n; ++i) {
        rc[static_cast<std::size_t>(i)] = aio_content_hash_verify_buffer_v1(
            contents[static_cast<std::size_t>(i)].data(),
            static_cast<std::uint64_t>(lenpattern[static_cast<std::size_t>(i)]),
            expect[static_cast<std::size_t>(i)].c_str());
    }
}

// U5 组调用形态宏 (保持测试正文与并行机制解耦)
#define AIO_OMP_PARALLEL_FOR_REVIEW(rc, contents, expect, lenpattern, workers) \
    ::aio_abi_omp_parallel_review((rc), (contents), (expect), (lenpattern), (workers))

#endif /* AIO_ABI_OMP_HPP */
