// tests/backend/candidates_probe_main.cpp — 候选生成器实参打印(证明派生、无硬编码)
#include <cstdio>
#include "bench_harness.h"

int main() {
    using namespace astrocs::backend_host;
    for (uint32_t avail : {1u, 2u, 3u, 8u, 16u, 33u}) {
        std::printf("W avail=%u:", avail);
        for (uint32_t w : worker_candidates(avail)) std::printf(" %u", w);
        std::printf("\n");
    }
    for (uint64_t l2 : {(uint64_t)128 << 10, (uint64_t)512 << 10, (uint64_t)2 << 20}) {
        std::printf("BLOCK l2=%llu:", (unsigned long long)l2);
        for (uint64_t b : block_candidates(l2, 4)) std::printf(" %llu", (unsigned long long)b);
        std::printf("\n");
    }
    return 0;
}
