// lib/acr/qualification/benchmarks/numa_benchmark.cpp — NUMA 本地/远端 host buffer 带宽
//
// 设计（06 §6 + 17 §2）：
// 1. 利用 hwloc 拓扑识别 NUMA 节点（ADR-003 hwloc 为唯一拓扑来源）
// 2. 对每个 NUMA 节点对（local, remote），分配 host buffer 并绑定到该 NUMA 节点
// 3. 运行 STREAM Triad（y[i] = a*x[i] + y[i]）测量带宽
// 4. 报告 local vs remote 带宽差异（GB/s）
// 5. 无 hwloc 或单 NUMA 节点时 SkipWithError 降级（ADR-009 降级策略）
// 6. 公共头不暴露 hwloc 类型（所有 hwloc 调用仅在本 .cpp 内）
//
// 内存绑定策略：
// - hwloc_alloc_membind：按 NUMA 节点集分配并绑定
// - 失败时降级到普通 malloc 并标记 "unbound"
// - 读 / 写流式访问，强制缓存不命中（大数组超出 LLC）
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

// hwloc 检测（与 topology/hwloc_topo.cpp 一致）
#if defined(__has_include)
#  if __has_include(<hwloc.h>)
#    define ACR_BENCH_HAVE_HWLOC 1
#  endif
#endif

#ifdef ACR_BENCH_HAVE_HWLOC
#  include <hwloc.h>
#endif

namespace astro::compute::qualification::bench {

#ifdef ACR_BENCH_HAVE_HWLOC

namespace {

// RAII guard for hwloc topology
struct HwlocTopoGuard {
    hwloc_topology_t topo{nullptr};
    bool ok{false};
    HwlocTopoGuard() {
        if (hwloc_topology_init(&topo) != 0) return;
        hwloc_topology_set_flags(topo, HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM |
                                        HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM);
        if (hwloc_topology_load(topo) != 0) {
            hwloc_topology_destroy(topo);
            topo = nullptr;
            return;
        }
        ok = true;
    }
    ~HwlocTopoGuard() {
        if (topo) hwloc_topology_destroy(topo);
    }
};

// NUMA 节点信息
struct NumaNode {
    int os_index{-1};
    hwloc_obj_t obj{nullptr};
};

// 获取所有 NUMA 节点
std::vector<NumaNode> get_numa_nodes(hwloc_topology_t topo) {
    std::vector<NumaNode> nodes;
    unsigned n = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_NUMANODE);
    nodes.reserve(n);
    for (unsigned i = 0; i < n; ++i) {
        hwloc_obj_t obj = hwloc_get_obj_by_type(topo, HWLOC_OBJ_NUMANODE, i);
        if (obj) nodes.push_back({static_cast<int>(obj->os_index), obj});
    }
    return nodes;
}

// 在指定 NUMA 节点分配内存（绑定），失败返回 nullptr
// 使用 hwloc_alloc_membind 按节点集分配
// 正确签名：hwloc_alloc_membind(topology, len, set, policy, flags)
// - set = nodeset（bitmap）
// - policy = HWLOC_MEMBIND_BIND
// - flags = HWLOC_MEMBIND_STRICT | HWLOC_MEMBIND_BYNODESET（都是 flags 类型）
void* alloc_on_numa(hwloc_topology_t topo, hwloc_obj_t numa_obj, std::size_t bytes) {
    if (!numa_obj) return nullptr;
    hwloc_nodeset_t nodeset = hwloc_bitmap_alloc();
    if (!nodeset) return nullptr;
    hwloc_bitmap_zero(nodeset);
    hwloc_bitmap_set(nodeset, numa_obj->os_index);
    int flags = HWLOC_MEMBIND_STRICT | HWLOC_MEMBIND_BYNODESET;
    void* ptr = hwloc_alloc_membind(topo, bytes, nodeset, HWLOC_MEMBIND_BIND, flags);
    hwloc_bitmap_free(nodeset);
    return ptr;
}

// 释放 NUMA 绑定内存
void free_numa(hwloc_topology_t topo, void* ptr, std::size_t /*bytes*/) {
    if (ptr) hwloc_free(topo, ptr, 0);
}

// RAII wrapper for NUMA-bound buffer
struct NumaBuffer {
    hwloc_topology_t topo{nullptr};
    void* ptr{nullptr};
    std::size_t bytes{0};
    int numa_os_index{-1};
    bool bound{false};

    NumaBuffer(hwloc_topology_t t, hwloc_obj_t numa_obj, std::size_t sz)
        : topo(t), bytes(sz), numa_os_index(numa_obj ? static_cast<int>(numa_obj->os_index) : -1) {
        ptr = alloc_on_numa(topo, numa_obj, sz);
        if (ptr) {
            bound = true;
        } else {
            // 降级到普通分配（MinGW 无 std::aligned_alloc）
            // 对齐可能不足，但作为 NUMA 绑定失败的降级路径可接受
            ptr = std::malloc(sz);
            bound = false;
        }
    }
    ~NumaBuffer() {
        if (ptr) {
            if (bound) free_numa(topo, ptr, bytes);
            else std::free(ptr);
        }
    }
    NumaBuffer(const NumaBuffer&) = delete;
    NumaBuffer& operator=(const NumaBuffer&) = delete;
    NumaBuffer(NumaBuffer&& o) noexcept
        : topo(o.topo), ptr(o.ptr), bytes(o.bytes), numa_os_index(o.numa_os_index), bound(o.bound) {
        o.ptr = nullptr;
    }
    operator bool() const noexcept { return ptr != nullptr; }
    template<class T> T* as() noexcept { return static_cast<T*>(ptr); }
};

} // anonymous namespace

// ===== Google Benchmark fixture：NUMA 带宽测试 =====
// Triad 在 NUMA local / remote 上跑：y[i] = a*x[i] + y[i]
// 数组远大于 LLC，确保走主存带宽
class NumaBenchFixture : public ::benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& st) override {
        bytes_per_array_ = static_cast<std::size_t>(st.range(0));
        // 解析 NUMA src/dst 从 name（numa/local_<idx>/remote_<idx>）
        const std::string name = st.name();
        local_idx_ = parse_idx(name, "local_");
        remote_idx_ = parse_idx(name, "remote_");
        // 尝试获取拓扑
        topo_guard_ = std::make_unique<HwlocTopoGuard>();
    }
    void TearDown(const ::benchmark::State& /*st*/) override {
        topo_guard_.reset();
    }

    int parse_idx(const std::string& name, const std::string& key) {
        auto pos = name.find(key);
        if (pos == std::string::npos) return -1;
        pos += key.size();
        int v = 0;
        while (pos < name.size() && std::isdigit(static_cast<unsigned char>(name[pos]))) {
            v = v * 10 + (name[pos] - '0');
            ++pos;
        }
        return v;
    }

    std::size_t bytes_per_array_{0};
    int local_idx_{-1};
    int remote_idx_{-1};
    std::unique_ptr<HwlocTopoGuard> topo_guard_;
};

// NUMA local 带宽测试：buffer 分配在 local NUMA 节点，CPU 也绑定在该节点
// 由于 Google Benchmark 多线程由 oneTBB 调度，此处仅测单线程 + 显式绑定
static void run_numa_local_bench(::benchmark::State& state) {
    HwlocTopoGuard guard;
    if (!guard.ok) {
        state.SkipWithError("hwloc unavailable");
        return;
    }
    auto nodes = get_numa_nodes(guard.topo);
    if (nodes.size() < 1) {
        state.SkipWithError("no NUMA nodes");
        return;
    }
    int local_idx = static_cast<int>(state.range(1));
    if (local_idx < 0 || local_idx >= static_cast<int>(nodes.size())) {
        state.SkipWithError("invalid local NUMA idx");
        return;
    }
    std::size_t bytes = static_cast<std::size_t>(state.range(0));
    std::size_t n = bytes / sizeof(float);

    // 在 local NUMA 节点分配 x 和 y
    NumaBuffer x_buf(guard.topo, nodes[local_idx].obj, bytes);
    NumaBuffer y_buf(guard.topo, nodes[local_idx].obj, bytes);
    if (!x_buf || !y_buf) {
        state.SkipWithError("NUMA alloc failed");
        return;
    }
    fill_uniform(x_buf.as<float>(), n, kBenchmarkSeed);
    fill_uniform(y_buf.as<float>(), n, kBenchmarkSeed ^ 0xDEADBEEF);
    float a = 3.14f;

    // 将当前线程绑定到 local NUMA 节点（CPU 集）
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    hwloc_bitmap_zero(cpuset);
    hwloc_cpuset_from_nodeset(guard.topo, cpuset, nodes[local_idx].obj->nodeset);
    hwloc_set_cpubind(guard.topo, cpuset, HWLOC_CPUBIND_THREAD | HWLOC_CPUBIND_STRICT);
    hwloc_bitmap_free(cpuset);

    for (auto _ : state) {
        float* x = x_buf.as<float>();
        float* y = y_buf.as<float>();
        for (std::size_t i = 0; i < n; ++i) {
            y[i] = a * x[i] + y[i];
        }
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize_array(y_buf.as<float>(), n);

    std::size_t bytes_total = 3 * bytes;  // read x + read y + write y
    state.SetBytesProcessed(static_cast<int64_t>(bytes_total) * state.iterations());
    state.SetItemsProcessed(static_cast<int64_t>(n) * state.iterations());
    state.counters["numa_node"] = ::benchmark::Counter(
        static_cast<double>(nodes[local_idx].os_index));
    state.counters["bound"] = ::benchmark::Counter(
        x_buf.bound ? 1.0 : 0.0);
}

// NUMA remote 带宽测试：buffer 分配在 remote NUMA 节点，CPU 绑定在 local 节点
static void run_numa_remote_bench(::benchmark::State& state) {
    HwlocTopoGuard guard;
    if (!guard.ok) {
        state.SkipWithError("hwloc unavailable");
        return;
    }
    auto nodes = get_numa_nodes(guard.topo);
    if (nodes.size() < 2) {
        state.SkipWithError("single NUMA node, no remote");
        return;
    }
    int local_idx = static_cast<int>(state.range(1));
    int remote_idx = static_cast<int>(state.range(2));
    if (local_idx < 0 || remote_idx < 0 ||
        local_idx >= static_cast<int>(nodes.size()) ||
        remote_idx >= static_cast<int>(nodes.size()) ||
        local_idx == remote_idx) {
        state.SkipWithError("invalid NUMA idx pair");
        return;
    }
    std::size_t bytes = static_cast<std::size_t>(state.range(0));
    std::size_t n = bytes / sizeof(float);

    // 在 remote NUMA 节点分配 x 和 y
    NumaBuffer x_buf(guard.topo, nodes[remote_idx].obj, bytes);
    NumaBuffer y_buf(guard.topo, nodes[remote_idx].obj, bytes);
    if (!x_buf || !y_buf) {
        state.SkipWithError("NUMA alloc failed");
        return;
    }
    fill_uniform(x_buf.as<float>(), n, kBenchmarkSeed);
    fill_uniform(y_buf.as<float>(), n, kBenchmarkSeed ^ 0xDEADBEEF);
    float a = 3.14f;

    // 将当前线程绑定到 local NUMA 节点（CPU 集），制造 cross-node 访问
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    hwloc_bitmap_zero(cpuset);
    hwloc_cpuset_from_nodeset(guard.topo, cpuset, nodes[local_idx].obj->nodeset);
    hwloc_set_cpubind(guard.topo, cpuset, HWLOC_CPUBIND_THREAD | HWLOC_CPUBIND_STRICT);
    hwloc_bitmap_free(cpuset);

    for (auto _ : state) {
        float* x = x_buf.as<float>();
        float* y = y_buf.as<float>();
        for (std::size_t i = 0; i < n; ++i) {
            y[i] = a * x[i] + y[i];
        }
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }
    do_not_optimize_array(y_buf.as<float>(), n);

    std::size_t bytes_total = 3 * bytes;
    state.SetBytesProcessed(static_cast<int64_t>(bytes_total) * state.iterations());
    state.SetItemsProcessed(static_cast<int64_t>(n) * state.iterations());
    state.counters["numa_local"] = ::benchmark::Counter(
        static_cast<double>(nodes[local_idx].os_index));
    state.counters["numa_remote"] = ::benchmark::Counter(
        static_cast<double>(nodes[remote_idx].os_index));
    state.counters["bound"] = ::benchmark::Counter(
        x_buf.bound ? 1.0 : 0.0);
}

// 注册：对每个 NUMA 节点对注册（运行时通过 Args 动态发现）
// 由于 Google Benchmark 注册是编译期静态，这里用 0/1 占位，运行时跳过无效组合
// 实际 NUMA 节点数在 runtime 检测（SkipWithError 降级）
// 用 Args 为节点对提供索引（节点 0 → local, 节点 1 → remote，单节点系统会 skip）
#define ACR_BENCH_REGISTER_NUMA_LOCAL(NODE_IDX)                                       \
    BENCHMARK(run_numa_local_bench)                                                    \
        ->Args({static_cast<int64_t>(8 * 1024 * 1024), NODE_IDX})                     \
        ->Args({static_cast<int64_t>(64 * 1024 * 1024), NODE_IDX})                    \
        ->Args({static_cast<int64_t>(256 * 1024 * 1024), NODE_IDX})                  \
        ->Unit(::benchmark::kMillisecond)                                              \
        ->UseRealTime();

#define ACR_BENCH_REGISTER_NUMA_REMOTE(LOCAL_IDX, REMOTE_IDX)                          \
    BENCHMARK(run_numa_remote_bench)                                                  \
        ->Args({static_cast<int64_t>(8 * 1024 * 1024), LOCAL_IDX, REMOTE_IDX})        \
        ->Args({static_cast<int64_t>(64 * 1024 * 1024), LOCAL_IDX, REMOTE_IDX})       \
        ->Args({static_cast<int64_t>(256 * 1024 * 1024), LOCAL_IDX, REMOTE_IDX})     \
        ->Unit(::benchmark::kMillisecond)                                             \
        ->UseRealTime();

// 为常见 2-NUMA 节点系统注册（单节点系统会 SkipWithError）
ACR_BENCH_REGISTER_NUMA_LOCAL(0)
ACR_BENCH_REGISTER_NUMA_LOCAL(1)
ACR_BENCH_REGISTER_NUMA_REMOTE(0, 1)
ACR_BENCH_REGISTER_NUMA_REMOTE(1, 0)

#else // !ACR_BENCH_HAVE_HWLOC

// 无 hwloc 降级：注册一个 always-skip 的 benchmark
static void run_numa_unavailable(::benchmark::State& state) {
    state.SkipWithError("hwloc not available; NUMA benchmark skipped");
    (void)state;
}
BENCHMARK(run_numa_unavailable)->Unit(::benchmark::kMillisecond);

#endif // ACR_BENCH_HAVE_HWLOC

} // namespace astro::compute::qualification::bench
