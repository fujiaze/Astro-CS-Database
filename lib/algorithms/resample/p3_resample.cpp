// lib/algorithms/resample/p3_resample.cpp — 重采样实现 (ALG-P3-003)
// 叶级 nside = 512·2^K(K=properties order); NEAREST/BILINEAR 均经 healpix_core
// 权威函数(ang2pix/pix2ang/neighbors) — 禁止第二套数学核心。
//
// tile 存取层的两条结构性不变量 (与采样数学无关, 改动须保持逐位等价):
//   ①跨 worker 共享同一个**有界 LRU** tile 缓存: 容量 = max_tiles 个 512²f32 tile
//     (1 MiB/个), 与 worker 数无关 ⇒ 峰值内存不随核数增长;
//   ②缺失 tile **负缓存**: 每 tile 至多一次真实 open, 结果与"每次 open 都失败"
//     逐位相同 (那些 open 本来就必然失败)。
//   ③每 sampler 前端热缓存 kHotSlots 槽 (命中不取共享锁; 每线程一个 sampler
//     ⇒ 无锁访问)。
//   缓存策略只影响 I/O 命中率, **不影响任何像素值** (tile 内容只读); 一旦引入
//   跨 tile 的数值状态即失效。数学路径 (ang2pix_nest / pix2ang_nest / neighbors /
//   四象限双线性 / NaN 与 coverage 语义) 与缓存策略、逐出顺序、容量均无关。
#include "p3_resample.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "healpix_core.h"
#include "aio_hips_reader.h"
#include "hips_properties.h"
#include "p3_input_unit.h"   // Issue #91: 唯一输入单位身份裁决

// (docs/ACSD_DESIGN §10「统一 I/O 是文件级唯一边界」): 子产品 properties
// 的整文件读取经 aio 唯一实现 (aio_file::read_all), 本 TU 不自持 FILE*。
#include "aio_atomic_file.h"
#include "aio_file_io.h"

namespace acsd::phase3 {

namespace {
constexpr uint32_t kTileWidth = 512;
constexpr int kReaderBuf = 512 * 512;
// 每 sampler(生产路径 = 每工作线程一个)前端热缓存槽数: 覆盖 bilinear 单像素
// 触及的 2-4 个 tile 及相邻行带切换, 命中时完全不进入共享缓存的锁。
constexpr size_t kHotSlots = 8;

using TileData = std::vector<float>;

// ── 有界 tile 缓存 (跨 worker 共享; 线程安全) ───────────────────────────────
//  - 容量 cap = max_tiles 个 512²f32 tile (1 MiB/个), 与 worker 数无关:
//    峰值内存 = cap MiB + Σ(每线程热缓存 pin ≤ kHotSlots 个), 有界。
//  - LRU 逐出顺序确定, 但只影响命中率, 不影响任何像素值 (tile 内容只读)。
//  - absent = **合法无覆盖**负缓存（OUTSIDE_DECLARED_FOOTPRINT）：之后直接返回
//    缺失，不再对不存在的文件调 fits_open_file。语义与"每次 open 都失败"等价。
//  - errors = **完整性错误**独立存储（DECLARED_MISSING/INVALID/CHECKSUM/IO/
//    UNDETERMINED）。命中错误缓存复现同一 code/path/detail，不会把失败降级成
//    缺失，也不会重新读文件改变故障语义。按 tile_ipix 升序（std::map）聚合。
//  - 表操作在 mu 内, tile I/O 在 mu 外 (read_tile 自带进程级 CFITSIO 互斥),
//    避免缓存锁与 I/O 锁嵌套。
struct SharedTileCache {
    struct Entry {
        std::shared_ptr<const TileData> data;
        std::list<uint64_t>::iterator it;
    };
    struct ErrorRec {
        P3TileFailure f;
        std::set<int> workers;
    };
    static constexpr size_t kMaxErrorEntries = 4096;   // 故障聚合上限（失败路径）

    mutable std::mutex mu;
    size_t cap = 8;                        // 默认 (可配置 max_tiles)
    std::list<uint64_t> lru;               // front = MRU
    std::unordered_map<uint64_t, Entry> map;
    std::unordered_map<uint64_t, uint8_t> absent;   // 合法无覆盖
    std::map<uint64_t, ErrorRec> errors;            // 完整性错误（tile_ipix 升序）
    uint64_t stat_hits = 0;
    uint64_t stat_misses = 0;
    uint64_t stat_absent = 0;              // 合法无覆盖负缓存写入次数
    uint64_t stat_evict = 0;
    uint64_t stat_open_fail = 0;           // 真实失败 open/read 总次数
    uint64_t stat_error_read = 0;          // 完整性错误真实读取次数
    uint64_t stat_error_hit = 0;           // 错误缓存命中次数
    uint64_t stat_outside = 0;             // 合法无覆盖真实 open 失败次数
    uint64_t stat_checksum_present = 0;    // 成功读取且做了校验的次数
    uint64_t stat_checksum_absent = 0;     // 成功读取但缺校验字段的次数
    uint64_t stat_unverified_absent = 0;   // Issue #87/E2-R: 未证实缺失真实 open 次数

    std::shared_ptr<const TileData> get(uint64_t k) {
        std::lock_guard<std::mutex> lk(mu);
        auto it = map.find(k);
        if (it == map.end()) { ++stat_misses; return std::shared_ptr<const TileData>(); }
        lru.splice(lru.begin(), lru, it->second.it);
        ++stat_hits;
        return it->second.data;
    }
    void put(uint64_t k, std::shared_ptr<const TileData> d) {
        std::lock_guard<std::mutex> lk(mu);
        absent.erase(k);
        auto it = map.find(k);
        if (it != map.end()) {
            it->second.data = std::move(d);
            lru.splice(lru.begin(), lru, it->second.it);
            return;
        }
        lru.push_front(k);
        map.emplace(k, Entry{std::move(d), lru.begin()});
        const size_t limit = cap == 0 ? 1u : cap;
        while (map.size() > limit) {
            const uint64_t victim = lru.back();
            lru.pop_back();
            map.erase(victim);
            ++stat_evict;
        }
    }
    bool is_absent(uint64_t k) {
        std::lock_guard<std::mutex> lk(mu);
        return absent.find(k) != absent.end();
    }
    void mark_absent(uint64_t k) {
        std::lock_guard<std::mutex> lk(mu);
        absent.emplace(k, uint8_t{1});
        ++stat_absent;
    }
    void note_outside_read() {
        std::lock_guard<std::mutex> lk(mu);
        ++stat_open_fail;
        ++stat_outside;
    }
    // Issue #87/E2-R: FP_NONE 下缺失 = 未证实缺失（不是已证实无覆盖，也不是错误）。
    void note_unverified_read() {
        std::lock_guard<std::mutex> lk(mu);
        ++stat_open_fail;
        ++stat_unverified_absent;
    }
    void note_checksum(bool present) {
        std::lock_guard<std::mutex> lk(mu);
        if (present) ++stat_checksum_present;
        else ++stat_checksum_absent;
    }
    // P30 证据/回归: 真实失败的 tile open/read 次数 (与缓存命中区分)。
    void note_open_failure() {
        std::lock_guard<std::mutex> lk(mu);
        ++stat_open_fail;
    }

    static void sync_workers(ErrorRec* r) {
        if (r->workers.empty()) {
            r->f.worker_min = -1;
            r->f.worker_max = -1;
            r->f.worker_count = 0;
            return;
        }
        r->f.worker_min = *r->workers.begin();
        r->f.worker_max = *r->workers.rbegin();
        r->f.worker_count = (uint64_t)r->workers.size();
    }

    // 错误缓存命中：只增加 access_hits / worker 集合（不重复计 read_failures）。
    bool get_error(uint64_t k, int worker_id, P3TileFailure* out) {
        std::lock_guard<std::mutex> lk(mu);
        auto it = errors.find(k);
        if (it == errors.end()) return false;
        ++stat_error_hit;
        ++it->second.f.access_hits;
        if (worker_id >= 0) it->second.workers.insert(worker_id);
        sync_workers(&it->second);
        if (out) *out = it->second.f;
        return true;
    }
    // 首次/再次真实读失败：read_failures/access_hits 各 +1；code 保留**首错**
    // （同一 tile 在运行期间被并发改写时，不把不同错因混成一条）。
    void put_error(uint64_t k, const P3TileFailure& f, int worker_id) {
        std::lock_guard<std::mutex> lk(mu);
        ++stat_open_fail;
        ++stat_error_read;
        auto it = errors.find(k);
        if (it == errors.end()) {
            if (errors.size() >= kMaxErrorEntries) return;   // 上限保护，仍判红
            ErrorRec r;
            r.f = f;
            r.f.read_failures = 1;
            r.f.access_hits = 1;
            if (worker_id >= 0) r.workers.insert(worker_id);
            sync_workers(&r);
            errors.emplace(k, std::move(r));
            return;
        }
        ++it->second.f.read_failures;
        ++it->second.f.access_hits;
        if (worker_id >= 0) it->second.workers.insert(worker_id);
        sync_workers(&it->second);
    }
    // 热缓存错误槽命中：与 get_error 同语义（不含锁内 map 查找）。
    void note_error_hit(uint64_t k, int worker_id) {
        std::lock_guard<std::mutex> lk(mu);
        auto it = errors.find(k);
        if (it == errors.end()) return;
        ++stat_error_hit;
        ++it->second.f.access_hits;
        if (worker_id >= 0) it->second.workers.insert(worker_id);
        sync_workers(&it->second);
    }
};

// AIO 分类码 -> 采样状态（IO 独立，其余完整性错误统一 P3_RS_INTEGRITY）。
int status_from_tile_code(int code) {
    if (code == P3_TILE_IO_ERROR) return P3_RS_IO;
    if (code == P3_TILE_OK ||
        code == P3_TILE_OUTSIDE_DECLARED_FOOTPRINT)
        return P3_RS_OK;
    return P3_RS_INTEGRITY;
}

P3TileFailure make_tile_failure(const AioHipsTileStatus& st, uint64_t tip,
                                int worker_id) {
    P3TileFailure f;
    f.code = (st.code >= P3_TILE_DECLARED_MISSING && st.code <= P3_TILE_FOOTPRINT_UNDETERMINED)
                 ? st.code
                 : P3_TILE_IO_ERROR;
    f.tile_ipix = tip;
    f.cfitsio_status = st.cfitsio_status;
    f.footprint_identity = st.footprint_identity;
    f.declared = st.declared;
    f.read_failures = 1;
    f.access_hits = 1;
    f.worker_min = worker_id >= 0 ? worker_id : -1;
    f.worker_max = worker_id >= 0 ? worker_id : -1;
    f.worker_count = worker_id >= 0 ? 1u : 0u;
    std::snprintf(f.tile_path, sizeof(f.tile_path), "%s", st.tile_path);
    std::snprintf(f.detail, sizeof(f.detail), "%s", st.detail);
    return f;
}
}  // namespace

struct P3SamplerImpl {
    AioHipsDataset* ds = nullptr;
    int order = 0;                       // properties 实测 order(K)
    uint32_t leaf_nside = 512;           // 512·2^K
    // 共享有界 tile 缓存: 同一工作集合的多个 worker 指向同一实例 (P30)。
    std::shared_ptr<SharedTileCache> cache;
    // 本 sampler 专属前端热缓存 (每线程一个 sampler ⇒ 无锁访问)。
    //   hot_state: 0=空槽 1=命中(hot_val 有效) 2=已证实无覆盖 3=完整性错误
    //              4=未证实缺失(legacy unverified coverage, Issue #87/E2-R)
    uint64_t hot_key[kHotSlots];
    uint8_t hot_state[kHotSlots];
    std::shared_ptr<const TileData> hot_val[kHotSlots];
    P3TileFailure hot_err[kHotSlots];
    std::string root;
    std::string last_err;
    P3UncertaintySource unc_src = P3_UNC_NONE;   // uncertainty 子产品类型标记
    int footprint_identity = -1;                 // AIO_HIPS_FOOTPRINT_*
    int coverage_verified = -1;                  // Issue #87/E2-R: 1=可证完整性
    int worker_id = -1;                          // 聚合用（不改变数学）
    P3TileFailure last_failure;                  // 本 sampler 最近一次采样失败
    // P30: "缺失 tile 负缓存"开关 (默认开)。1=正常生产语义; 0=仅回归阴性对照
    // (退化为修复前"每个缺失像素重试一次失败 open"), 不得用于生产配置。
    int absent_cache = 1;

    P3SamplerImpl() : cache(std::make_shared<SharedTileCache>()) {
        for (size_t i = 0; i < kHotSlots; ++i) { hot_key[i] = ~0ull; hot_state[i] = 0; }
    }
    void clear_hot() {
        for (size_t i = 0; i < kHotSlots; ++i) {
            hot_key[i] = ~0ull; hot_state[i] = 0; hot_val[i].reset();
        }
    }
};

// 取一个 tile 的结果：数据 / 已证实无覆盖 / 未证实缺失 / 完整性错误。
enum class TileFetch { Ok, Outside, Unverified, Error };

// 取一个 tile。顺序: 线程本地热缓存 → 共享 LRU(锁) → 错误缓存 → 合法 absent
// → 真读(进程级 CFITSIO 互斥)。失败时 last_failure 必被置为具体错误身份。
static TileFetch fetch_tile(P3SamplerImpl* s, uint64_t tip,
                            std::shared_ptr<const TileData>* out) {
    out->reset();
    const size_t slot = static_cast<size_t>(tip * 0x9E3779B97F4A7C15ull) &
                        (kHotSlots - 1);
    if (s->hot_state[slot] != 0 && s->hot_key[slot] == tip) {
        if (s->hot_state[slot] == 1) {
            *out = s->hot_val[slot];
            return TileFetch::Ok;
        }
        if (s->hot_state[slot] == 2) return TileFetch::Outside;
        if (s->hot_state[slot] == 4) return TileFetch::Unverified;
        s->last_failure = s->hot_err[slot];
        s->cache->note_error_hit(tip, s->worker_id);
        return TileFetch::Error;
    }
    if (std::shared_ptr<const TileData> hit = s->cache->get(tip)) {
        s->hot_key[slot] = tip; s->hot_state[slot] = 1; s->hot_val[slot] = hit;
        *out = hit;
        return TileFetch::Ok;
    }
    P3TileFailure cached_failure;
    if (s->cache->get_error(tip, s->worker_id, &cached_failure)) {
        s->last_failure = cached_failure;
        s->hot_key[slot] = tip; s->hot_state[slot] = 3;
        s->hot_err[slot] = cached_failure;
        return TileFetch::Error;
    }
    if (s->absent_cache && s->cache->is_absent(tip)) {
        s->hot_key[slot] = tip; s->hot_state[slot] = 2; s->hot_val[slot].reset();
        return TileFetch::Outside;
    }
    auto tile = std::make_shared<TileData>(static_cast<size_t>(kReaderBuf));
    AioHipsTileStatus st{};
    const int rc = aio_hips_read_tile_f32_ex(s->ds, tip, tile->data(), &st);
    if (rc == AIO_HIPS_TILE_OK) {
        s->cache->note_checksum(st.checksum_present != 0);
        s->cache->put(tip, tile);
        s->hot_key[slot] = tip; s->hot_state[slot] = 1; s->hot_val[slot] = tile;
        *out = tile;
        return TileFetch::Ok;
    }
    if (rc == AIO_HIPS_TILE_OUTSIDE_DECLARED_FOOTPRINT) {
        s->cache->note_outside_read();       // 观测: 已证实无覆盖代价
        if (s->absent_cache) s->cache->mark_absent(tip);
        s->hot_key[slot] = tip; s->hot_state[slot] = 2; s->hot_val[slot].reset();
        return TileFetch::Outside;
    }
    // Issue #87/E2-R: FP_NONE（legacy unverified coverage）下的缺失。既不能证实
    // 也不能否证 ⇒ 不进错误表（不是完整性错误, 不 fail-closed），但必须与已证实
    // 无覆盖分开计数，使产物身份可判 coverage_verified=false。
    if (rc == AIO_HIPS_TILE_UNKNOWN_LEGACY_COVERAGE) {
        s->cache->note_unverified_read();
        if (s->absent_cache) s->cache->mark_absent(tip);
        s->hot_key[slot] = tip; s->hot_state[slot] = 4; s->hot_val[slot].reset();
        return TileFetch::Unverified;
    }
    // Issue #87: 真实读取失败/损坏/声明内缺失/校验失败 —— **不**进 absent 负缓存，
    // 进独立错误缓存并保留具体身份；下次命中复现同一故障，不会变成"缺失"。
    P3TileFailure f = make_tile_failure(st, tip, s->worker_id);
    if (rc < 0) {   // 参数/异常屏障（正常不可达）
        f.code = P3_TILE_IO_ERROR;
        f.cfitsio_status = 0;
        std::snprintf(f.detail, sizeof(f.detail), "reader returned %d: %s", rc,
                      aio_hips_reader_last_error());
    }
    s->last_err = std::string("read_tile ") + std::to_string(tip) + ": " + f.detail;
    s->last_failure = f;
    s->cache->put_error(tip, f, s->worker_id);
    s->hot_key[slot] = tip; s->hot_state[slot] = 3; s->hot_err[slot] = f;
    return TileFetch::Error;
}

// 读一个叶级像素：
//   *present=true  → *out 有效（tile 内 NaN 也是有效值, §4）;
//   *present=false → 合法无覆盖（OUTSIDE, coverage=0）;
//   返回 P3_RS_INTEGRITY/P3_RS_IO → 真实故障，调用方不得当 OK 继续。
static P3ResampleStatus read_leaf(P3SamplerImpl* s, uint64_t leaf_ipix, float* out,
                                  bool* present) {
    *present = false;
    // leaf nside = 512·2^K → tile_order=K 的父 ipix; 局部 512² 由 nested_local 映射
    const int tile_order = s->order;
    const uint32_t leaf_order = static_cast<uint32_t>(tile_order) + 9;
    const uint64_t tip = acsd::healpix::leaf_to_tile_nest(
        leaf_ipix, leaf_order, static_cast<uint32_t>(tile_order));   // 传"阶"非 nside
    std::shared_ptr<const TileData> tile;
    const TileFetch fs = fetch_tile(s, tip, &tile);
    if (fs == TileFetch::Outside) return P3_RS_OK;   // 已证实无覆盖
    if (fs == TileFetch::Unverified) return P3_RS_OK;  // 未证实缺失: best-effort NaN/0
    if (fs == TileFetch::Error)
        return static_cast<P3ResampleStatus>(status_from_tile_code(s->last_failure.code));
    // leaf→tile 内标准 HiPS 排列索引
    const uint64_t first = acsd::healpix::tile_to_leaf_nest(
        tip, static_cast<uint32_t>(tile_order), leaf_order);
    const uint64_t local = leaf_ipix - first;
    const uint64_t fits_index = acsd::healpix::nested_local_to_fits_index(
        local, 9, kTileWidth);
    *out = (*tile)[fits_index];   // NaN 是命中(值语义, §4)
    *present = true;
    return P3_RS_OK;
}

P3ResampleStatus p3_order_select(int max_order, double scale_deg_per_px,
                                       int* out_order) {
    if (!out_order || max_order < 0 || max_order > kMaxOrder || !(scale_deg_per_px > 0))
        return P3_RS_PARAM;
    for (int k = 0; k <= max_order; ++k) {
        const double res_deg = acsd::healpix::pixel_resolution_arcsec(
                                   (512u << k)) / 3600.0;
        if (res_deg <= scale_deg_per_px) { *out_order = k; return P3_RS_OK; }
    }
    *out_order = max_order;
    return P3_RS_OK;
}

P3ResampleStatus p3_resample_check_mode(const char* input_mode) {
    if (!input_mode || !*input_mode) return P3_RS_PARAM;
    const std::string m = input_mode;
    // §4 显式拒清单: weight/flux-per-pixel 输入模式 (不变, SCI §9a-8);
    // "variance"/"ivar" 依 DATA-P3-UNC-001 §30.4-4 从 UNSUPPORTED 拒绝项移除
    // (转 uncertainty 子产品消费面) → 落未知输入模式 PARAM。
    if (m == "weight" || m == "flux-per-pixel") return P3_RS_UNSUPPORTED;
    if (m == "surface_brightness") return P3_RS_OK;
    return P3_RS_PARAM;
}

void p3_sampler_set_max_tiles(P3Sampler* s, int max_tiles) {
    if (!s || !s->impl) return;
    const size_t cap = (max_tiles <= 0) ? 8u : static_cast<size_t>(max_tiles);
    std::lock_guard<std::mutex> lk(s->impl->cache->mu);
    s->impl->cache->cap = cap;
}

void p3_sampler_attach_cache(P3Sampler* dst, const P3Sampler* src) {
    if (!dst || !dst->impl || !src || !src->impl) return;
    if (dst->impl->cache == src->impl->cache) return;
    dst->impl->cache = src->impl->cache;   // 共享同一个有界缓存 (容量已由主 sampler 设定)
    dst->impl->clear_hot();
}

void p3_sampler_cache_stats(const P3Sampler* s, P3CacheStats* out) {
    if (!out) return;
    *out = P3CacheStats{};
    if (!s || !s->impl || !s->impl->cache) return;
    SharedTileCache* c = s->impl->cache.get();
    std::lock_guard<std::mutex> lk(c->mu);
    out->cap_tiles = c->cap;
    out->resident_tiles = c->map.size();
    out->hits = c->stat_hits;
    out->misses = c->stat_misses;
    out->absent_reads = c->stat_absent;
    out->open_failures = c->stat_open_fail;
    out->absent_entries = c->absent.size();
    out->evictions = c->stat_evict;
    out->tile_error_reads = c->stat_error_read;
    out->tile_error_hits = c->stat_error_hit;
    out->tile_error_entries = c->errors.size();
    out->outside_reads = c->stat_outside;
    out->checksum_present_reads = c->stat_checksum_present;
    out->checksum_absent_reads = c->stat_checksum_absent;
    out->unverified_absent_reads = c->stat_unverified_absent;
    out->footprint_identity = s->impl->footprint_identity;
    out->coverage_verified = s->impl->coverage_verified;
    for (const auto& kv : c->errors) {
        switch (kv.second.f.code) {
            case P3_TILE_DECLARED_MISSING: ++out->declared_missing; break;
            case P3_TILE_INVALID: ++out->invalid_tiles; break;
            case P3_TILE_CHECKSUM_FAILED: ++out->checksum_failures; break;
            case P3_TILE_IO_ERROR: ++out->io_errors; break;
            case P3_TILE_FOOTPRINT_UNDETERMINED: ++out->undetermined; break;
            default: break;
        }
    }
}

void p3_sampler_set_worker_id(P3Sampler* s, int worker_id) {
    if (!s || !s->impl) return;
    s->impl->worker_id = worker_id;
}

int p3_sampler_tile_failures(const P3Sampler* s, P3TileFailure* out, int max) {
    if (!s || !s->impl || !s->impl->cache) return 0;
    SharedTileCache* c = s->impl->cache.get();
    std::lock_guard<std::mutex> lk(c->mu);
    if (!out || max <= 0) return (int)c->errors.size();
    int n = 0;
    for (const auto& kv : c->errors) {
        if (n >= max) break;
        out[n++] = kv.second.f;
    }
    return n;
}

uint64_t p3_sampler_tile_failure_count(const P3Sampler* s) {
    if (!s || !s->impl || !s->impl->cache) return 0;
    std::lock_guard<std::mutex> lk(s->impl->cache->mu);
    return (uint64_t)s->impl->cache->errors.size();
}

int p3_sampler_last_tile_failure(const P3Sampler* s, P3TileFailure* out) {
    if (!out) return 0;
    *out = P3TileFailure{};
    if (!s || !s->impl || s->impl->last_failure.code == 0) return 0;
    *out = s->impl->last_failure;
    return 1;
}

int p3_sampler_footprint_identity(const P3Sampler* s) {
    if (!s || !s->impl) return -1;
    return s->impl->footprint_identity;
}

int p3_sampler_coverage_verified(const P3Sampler* s) {
    if (!s || !s->impl) return -1;
    return s->impl->coverage_verified;
}

void p3_sampler_set_absent_cache(P3Sampler* s, int enabled) {
    if (!s || !s->impl) return;
    s->impl->absent_cache = enabled ? 1 : 0;
    s->impl->clear_hot();   // 语义切换后热缓存的负项立即失效
}

P3ResampleStatus p3_sampler_open(const char* product_dir, P3Sampler* out,
                                        std::string* err) {
    return p3_sampler_open_ex(product_dir, out, nullptr, nullptr, err);
}

P3ResampleStatus p3_sampler_open_ex(const char* product_dir, P3Sampler* out,
                                    int* out_order, std::string* out_bunit,
                                    std::string* err) {
    if (!product_dir || !out) return P3_RS_PARAM;
    // ── Issue #91: 输入单位身份裁决先行 ────────────────────────────────────
    // 唯一实现 = acsd::phase3::p3_input_unit_verdict（session 与 scheduler 同源
    // 同判）。缺失/空串/重复/冲突/表外语汇/语义矛盾一律显式拒绝，**绝不**补写
    // 确定量纲（旧行为 `p.bunit.empty() ? "ADU/sr" : p.bunit` 是缺陷本体）。
    const acsd::phase3::P3InputUnitVerdict unit = acsd::phase3::p3_input_unit_verdict(product_dir);
    if (!unit.accepted) {
        if (err) *err = unit.code + ": " + unit.reason;
        return P3_RS_PARAM;   // 无 silent default；错误码经 err 贯通 session/scheduler
    }
    // product_dir = HiPS 根(内含 signal/ 子产品); 严格校验 signal/properties
    const std::string signal_dir = std::string(product_dir) + "/signal";
    HipsProperties p{};
    if (!hips_product_validate(signal_dir, &p, err))
        return P3_RS_PARAM;   // 无 silent default
    auto* s = new P3SamplerImpl();
    s->ds = aio_hips_open(product_dir, AIO_HIPS_RD_SIGNAL);
    if (!s->ds) {
        if (err) *err = aio_hips_reader_last_error();
        delete s;
        return P3_RS_IO;
    }
    s->order = p.order;
    s->leaf_nside = kTileWidth << p.order;
    s->root = product_dir;
    s->footprint_identity = aio_hips_footprint_identity(s->ds);
    s->coverage_verified = aio_hips_coverage_verified(s->ds);
    out->impl = s;
    // 暴露输入实际 order 与**经裁决的**单位身份。
    if (out_order) *out_order = p.order;
    // 采样值 = 输入 HiPS tile 值的凸组合 ⇒ 与输入同单位（面亮度）。产品单位串取
    // 裁决给出的冻结 canonical 面亮度串（acsd::phase3::kP3SignalBunitCanonical,
    // "ADU/sr"）；输入原始声明由调用方另存 bunit_input 供审计。单位身份已由裁决
    // 证实，此处不再做任何缺省/猜测（docs/science/unified/DATA_SEMANTICS.md §3 +
    // docs/detail/registry/acsd.phase3.resample2.md「输入输出端口、DATA、单位、坐标、
    // invalid」一节）。
    if (out_bunit) *out_bunit = unit.bunit_canonical;
    return P3_RS_OK;
}
P3ResampleStatus p3_sample_nearest(P3Sampler* s, double ra_deg, double dec_deg,
                                         float* value, int* coverage) {
    return p3_sample_nearest_ex(s, ra_deg, dec_deg, value, coverage, nullptr);
}

P3ResampleStatus p3_sample_nearest_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                      float* value, int* coverage,
                                      uint64_t* leaf_ipix) {
    if (!s || !s->impl || !value || !coverage) return P3_RS_PARAM;
    auto* impl = s->impl;
    const uint64_t leaf = acsd::healpix::ang2pix_nest(impl->leaf_nside, ra_deg, dec_deg);
    if (leaf_ipix) *leaf_ipix = leaf;   // 恒输出 (c=0 时供 missing 语义消费)
    float v = 0;
    bool present = false;
    const P3ResampleStatus r = read_leaf(impl, leaf, &v, &present);
    if (r != P3_RS_OK) {          // Issue #87: 真实故障不得折叠成 OK+NaN
        *value = std::nanf("");
        *coverage = 0;
        return r;
    }
    if (!present) {               // 合法无覆盖
        *value = std::nanf("");
        *coverage = 0;
        return P3_RS_OK;
    }
    *value = v;              // NaN 直传(§4: S=NaN, C=1)
    *coverage = 1;
    return P3_RS_OK;
}

P3ResampleStatus p3_sample_bilinear(P3Sampler* s, double ra_deg, double dec_deg,
                                          float* value, int* coverage) {
    return p3_sample_bilinear_ex(s, ra_deg, dec_deg, value, coverage, nullptr, nullptr);
}

// 3×3 邻域点用定长栈数组 pts[10] 承载; 取样/四象限选择/退化填充/双线性权重与
// NaN·coverage 语义与 bilinear_ex 逐行等价。
// 性能注记 (非不变量): 邻域 ipix 列表仍由 healpix::neighbors 返回 std::vector, 故本
// 像素路径**仍含一次 per-pixel 堆分配** (CODE_STANDARD §MUST「无 per-pixel
// malloc/new」未满足); 该分配只影响性能, 不影响任何像素值。
P3ResampleStatus p3_sample_bilinear_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                       float* value, int* coverage,
                                       double weights[4], uint64_t leaf_ipix[4]) {
    // 薄封装: 同一数学路径 (样本级掩膜版), 不暴露计数
    return p3_sample_bilinear_nanmask_ex(s, ra_deg, dec_deg, value, coverage,
                                         weights, leaf_ipix, nullptr);
}

// 样本级掩膜口径 (rule_id NAN-SAMPLE-MASK-COVERAGE-NAN) 的唯一实现:
//   权威 = docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md「组成块」一节
//   invalid_handling 块 (唯一正本) + docs/engineering/standards/NUMERIC.md「MUST」一节
//   + ALG-P3-003 §2 G4/§4 (docs/science/algorithms/PHASE3_RESAMPLE.md)。
//   ①不合格样本 = ¬isfinite(值) (NaN 与 ±Inf 同类);
//   ②从分子、分母、方差三项一并剔除 (被剔除样本的生效权重恰为 0 ⇒ 不留在分母);
//   ③对剩余合格邻域重归一 c_k = w_k / Σ(合格 w_j) (FP64, 固定 k 序 ⇒ 确定性);
//   ④仅零合格样本 (n_eligible==0) 或有效权重和 W_p==0 时 S=NaN (覆盖级 NaN);
//   ⑤C 只判足迹内有无 tile 像素: 4 个 tile 均可读则 C=1, 值 NaN 不改 C (§4);
//   ⑥强制计数: 被剔除样本数按原因分类暴露 (禁静默剔除; 禁零填替代)。
P3ResampleStatus p3_sample_bilinear_nanmask_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                               float* value, int* coverage,
                                               double weights[4], uint64_t leaf_ipix[4],
                                               P3SampleRejection* rejection) {
    if (rejection) *rejection = P3SampleRejection{};   // 计数 0 与「字段缺失」可区分
    if (!s || !s->impl || !value || !coverage) return P3_RS_PARAM;
    // #89: 四角权重是完整输出参数；被 NaN 掩膜的角也必须写出精确 0。
    if (weights) std::fill(weights, weights + 4, 0.0);
    auto* impl = s->impl;
    const uint32_t nside = impl->leaf_nside;
    const uint64_t ipix = acsd::healpix::ang2pix_nest(nside, ra_deg, dec_deg);
    // 3×3 邻域(中心+8 邻居)投影到样本点切平面
    const std::vector<uint64_t> nb = acsd::healpix::neighbors(nside, ipix);
    struct P { uint64_t ipix; double x, y; };   // 切平面坐标(deg)
    P pts[10];
    int np = 0;
    const double d0r = dec_deg * M_PI / 180.0, a0r = ra_deg * M_PI / 180.0;
    auto add_pt = [&](uint64_t ip) {
        if (np >= 10) return;
        double ra = 0, dec = 0;
        acsd::healpix::pix2ang_nest(nside, ip, ra, dec);
        const double ar = ra * M_PI / 180.0, dr = dec * M_PI / 180.0;
        const double den = std::sin(d0r) * std::sin(dr) +
                           std::cos(d0r) * std::cos(dr) * std::cos(ar - a0r);
        if (den <= 0) return;
        const double xi = std::cos(dr) * std::sin(ar - a0r) / den;
        const double eta = (std::sin(dr) * std::cos(d0r) -
                            std::cos(dr) * std::sin(d0r) * std::cos(ar - a0r)) / den;
        pts[np].ipix = ip; pts[np].x = xi; pts[np].y = eta;
        ++np;
    };
    add_pt(ipix);
    for (uint64_t n : nb) add_pt(n);
    // 四象限最近中心(确定性: 距离并列时取更小 ipix)
    const P* q[2][2] = {{nullptr, nullptr}, {nullptr, nullptr}};   // [x<0|x>0][y<0|y>0]
    double best_d[2][2] = {{1e300, 1e300}, {1e300, 1e300}};
    for (int i = 0; i < np; ++i) {
        const P& p = pts[i];
        const int ix = p.x >= 0 ? 1 : 0;
        const int iy = p.y >= 0 ? 1 : 0;
        const double d2 = p.x * p.x + p.y * p.y;
        if (d2 < best_d[ix][iy] ||
            (d2 == best_d[ix][iy] && q[ix][iy] != nullptr && p.ipix < q[ix][iy]->ipix)) {
            best_d[ix][iy] = d2;
            q[ix][iy] = &p;
        }
    }
    // 退化防护: 角点位置某些象限可能无邻域点 → 用最近邻点填充(确定性双线性退化)
    const P* nearest_pt = nullptr;
    double nd = 1e300;
    for (int i = 0; i < np; ++i) {
        const double d2 = pts[i].x * pts[i].x + pts[i].y * pts[i].y;
        if (d2 < nd) { nd = d2; nearest_pt = &pts[i]; }
    }
    if (nearest_pt == nullptr) {   // 无有效邻域点(不可达于合法 nside) → 显式缺失
        *value = std::nanf(""); *coverage = 0;
        return P3_RS_OK;
    }
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            if (!q[i][j]) q[i][j] = nearest_pt;
    // 角点值读取(缺 tile → coverage=0)。Issue #87: 读取失败必须保留故障——
    // 四角全部读取（确定性与角序无关），选 tile_ipix 最小的失败作为确定性首错；
    // 任一角真实故障 ⇒ 返回 P3_RS_INTEGRITY/P3_RS_IO，不得返回 OK+NaN。
    float v00 = 0, v10 = 0, v01 = 0, v11 = 0;
    bool g00 = false, g10 = false, g01 = false, g11 = false;
    P3ResampleStatus fail_status = P3_RS_OK;
    P3TileFailure first_fail{};
    uint64_t first_fail_tip = UINT64_MAX;
    auto read_corner = [&](uint64_t ip, float* out, bool* present) {
        const P3ResampleStatus r = read_leaf(impl, ip, out, present);
        if (r != P3_RS_OK) {
            const P3TileFailure f = impl->last_failure;
            if (fail_status == P3_RS_OK || f.tile_ipix < first_fail_tip) {
                fail_status = r;
                first_fail = f;
                first_fail_tip = f.tile_ipix;
            }
        }
    };
    read_corner(q[0][0]->ipix, &v00, &g00);
    read_corner(q[1][0]->ipix, &v10, &g10);
    read_corner(q[0][1]->ipix, &v01, &g01);
    read_corner(q[1][1]->ipix, &v11, &g11);
    if (fail_status != P3_RS_OK) {
        impl->last_failure = first_fail;
        *value = std::nanf("");
        *coverage = 0;
        return fail_status;
    }
    if (!g00 || !g10 || !g01 || !g11) {
        *value = std::nanf(""); *coverage = 0; return P3_RS_OK;
    }
    // 平面双线性: 以四角平均中心定义局部坐标, 解 (u,v)
    const double x0 = q[0][0]->x, x1 = q[1][0]->x, y0 = q[0][0]->y, y1 = q[0][1]->y;
    // 取每角到原点符号距离的归一权重(双线性一般式, 四角非共线时退化为面积权重)
    double u = 0, v = 0;
    const double dx = x1 - x0, dy = y1 - y0;
    if (std::fabs(dx) > 1e-300) u = (0.0 - x0) / dx;
    if (std::fabs(dy) > 1e-300) v = (0.0 - y0) / dy;
    u = std::min(1.0, std::max(0.0, u));
    v = std::min(1.0, std::max(0.0, v));
    const double wg[4] = {(1 - u) * (1 - v), u * (1 - v),
                          (1 - u) * v, u * v};      // G4 几何权重 (Σ=1±k·ULP)
    const float vv[4] = {v00, v10, v01, v11};
    // ── 样本级掩膜 (¬isfinite, 含 ±Inf) + 剩余有效邻域重归一 ───────────────
    // rule_id NAN-SAMPLE-MASK-COVERAGE-NAN: 不合格样本从分子、分母、方差三项
    // 一并剔除; 被剔除样本的生效权重恰为 0 ⇒ 其权重不进分母、其值不进分子、
    // 其方差项 (c²u) 恰为 0 ⇒ 方差传播必须消费下面暴露的**生效权重**。
    bool ok[4];
    double wsum = 0.0;                 // W_p = Σ_{合格} w_j (无量纲; 非 Phase1 面积 D_p[sr])
    int n_rej = 0;
    for (int k = 0; k < 4; ++k) {
        ok[k] = std::isfinite(vv[k]);
        if (ok[k]) wsum += wg[k]; else ++n_rej;
    }
    if (rejection) {
        rejection->n_rejected_nonfinite = n_rej;
        rejection->n_rejected_nonfinite_value = n_rej;   // 本核唯一原因类 (值非有限)
        rejection->n_rejected_nonfinite_variance = 0;    // 信号核不消费方差面
        rejection->n_rejected_nonpositive_weight = 0;    // 权重由几何唯一确定
        rejection->n_eligible = 4 - n_rej;
    }
    const bool zero_eligible = (n_rej == 4) || !(wsum > 0.0);
    if (zero_eligible) {
        // 覆盖级 NaN (DATA-002 §2a 规则 2: 零合格样本 / W_p=0):
        // S=NaN; C 不变 (§4: C 只判足迹内有无 tile 像素 ⇒ 4 tile 可读则 C=1);
        // 禁零填替代; 生效权重全 0 (此时 Σc_k=1 不变量不适用)。
        *value = std::nanf("");
        *coverage = 1;
        if (weights) weights[0] = weights[1] = weights[2] = weights[3] = 0.0;
    } else {
        double val = 0.0;
        for (int k = 0; k < 4; ++k) {
            if (!ok[k]) continue;
            const double eff = wg[k] / wsum;   // 重归一 (FP64, 固定 k 序)
            val += eff * static_cast<double>(vv[k]);
            if (weights) weights[k] = eff;
        }
        *value = static_cast<float>(val);
        *coverage = 1;   // §4: 值非有限不改 C
    }
    if (leaf_ipix) {
        leaf_ipix[0] = q[0][0]->ipix; leaf_ipix[1] = q[1][0]->ipix;
        leaf_ipix[2] = q[0][1]->ipix; leaf_ipix[3] = q[1][1]->ipix;
    }
    (void)y1;
    return P3_RS_OK;
}

void p3_sampler_close(P3Sampler* s) {
    if (!s || !s->impl) return;
    if (s->impl->ds) aio_hips_close(s->impl->ds);
    delete s->impl;
    s->impl = nullptr;
}

// ── 不确定度传播面 (DATA-P3-UNC-001 §30.4) ─────────────────────────────────

P3ResampleStatus p3_uncertainty_open(const char* product_dir, int signal_order,
                                     P3UncertaintySource* out_src, P3Sampler* out) {
    if (!product_dir || !out_src) return P3_RS_PARAM;
    if (out) out->impl = nullptr;
    *out_src = P3_UNC_NONE;
    // 严格优先序: variance → ivar (§30.4-1)
    struct Cand { const char* sub; P3UncertaintySource src; int product; };
    const Cand cands[2] = {
        {"variance", P3_UNC_VARIANCE, AIO_HIPS_RD_VARIANCE},
        {"ivar", P3_UNC_IVAR, AIO_HIPS_RD_IVAR},
    };
    const std::string root = std::string(product_dir);
    for (const auto& c : cands) {
        const std::string props_path = root + "/" + c.sub + "/properties";
        // 读取经 aio (aio_file::read_all) — 本 TU 不自持文件通道。
        // 子产品不存在 → 试下一候选 (与原 fopen 失败同语义);
        // 存在但读取失败 → P3_RS_IO (fail-closed, 不静默跳过)。
        int props_is_dir = 0;
        if (!aio_atomic::path_exists(props_path, &props_is_dir) || props_is_dir)
            continue;
        std::string text;
        if (!aio_file::read_all(props_path.c_str(), &text)) return P3_RS_IO;
        // properties 存在 → 键集严格解析 + order 一致性 (无 silent default)
        HipsProperties p{};
        std::string err;
        if (!hips_properties_parse(text, &p, &err)) return P3_RS_PARAM;
        if (p.order != signal_order) return P3_RS_PARAM;   // 覆盖面错位=损坏
        if (!out) { *out_src = c.src; return P3_RS_OK; }
        auto* s = new (std::nothrow) P3SamplerImpl();
        if (!s) return P3_RS_IO;
        s->ds = aio_hips_open(product_dir, c.product);
        if (!s->ds) {
            s->last_err = aio_hips_reader_last_error();
            delete s;
            return P3_RS_IO;
        }
        s->order = p.order;
        s->leaf_nside = kTileWidth << p.order;
        s->root = product_dir;
        s->unc_src = c.src;
        s->footprint_identity = aio_hips_footprint_identity(s->ds);
        s->coverage_verified = aio_hips_coverage_verified(s->ds);
        out->impl = s;
        *out_src = c.src;
        return P3_RS_OK;
    }
    // 两者皆无 → 显式 unavailable (合法, 非错误)
    return P3_RS_OK;
}

void p3_uncertainty_close(P3Sampler* s) { p3_sampler_close(s); }

// 读单 leaf u 值并按 §30.4 值域守卫归类。
// 返回 P3_RS_OK (*st=OK/NAN/MISSING) 或 P3_RS_PARAM (u<0/Inf 产品损坏)。
static P3ResampleStatus read_u_leaf(P3Sampler* u, uint64_t leaf, double* out,
                                    P3UncPixelState* st) {
    auto* impl = u->impl;
    float v = 0;
    bool present = false;
    const P3ResampleStatus r = read_leaf(impl, leaf, &v, &present);
    if (r != P3_RS_OK) return r;   // Issue #87: 真实故障不得当 missing 吞掉
    if (!present) { *out = std::nanf(""); *st = P3_U_MISSING; return P3_RS_OK; }
    if (std::isnan(v)) { *out = std::nanf(""); *st = P3_U_NAN; return P3_RS_OK; }
    const double d = static_cast<double>(v);
    // ivar==0 → u 无效 (§30.4-1: 像素级规则, NaN 传播态, 非产品损坏);
    // 其余负/Inf (负/Inf variance, 负 ivar) → 产品损坏 (§30.4-3)。
    if (d == 0.0 && impl->unc_src == P3_UNC_IVAR) {
        *out = std::nanf(""); *st = P3_U_NAN; return P3_RS_OK;
    }
    if (d < 0.0 || !std::isfinite(d)) return P3_RS_PARAM;   // 产品损坏 (§30.4-3)
    *out = (impl->unc_src == P3_UNC_IVAR) ? 1.0 / d : d;    // FP64
    if (!std::isfinite(*out)) return P3_RS_PARAM;
    *st = P3_U_OK;
    return P3_RS_OK;
}

P3ResampleStatus p3_uncertainty_propagate(P3Sampler* u, const double* weights,
                                          const uint64_t* leaf_ipix, int npts,
                                          double* u_out, P3UncPixelState* st) {
    if (!u || !u->impl || !u_out || !st || (npts != 1 && npts != 4))
        return P3_RS_PARAM;
    if (npts == 4 && (!weights || !leaf_ipix)) return P3_RS_PARAM;
    if (npts == 1 && !leaf_ipix) return P3_RS_PARAM;
    *u_out = std::nanf("");
    *st = P3_U_OK;
    // #88: 退化四角可能指向同一个输入 leaf；重复读取同一个随机变量并非
    // 独立样本。先按 leaf 身份合并信号系数，再累加每个唯一 leaf 的 a_j^2 u_j。
    // 所有 leaf 的原始读取/产品损坏检查依旧执行（含零权角）。
    double grouped_weights[4] = {0.0, 0.0, 0.0, 0.0};
    if (npts == 4) {
        for (int k = 0; k < 4; ++k) {
            int representative = k;
            for (int j = 0; j < k; ++j) {
                if (leaf_ipix[j] == leaf_ipix[k]) {
                    representative = j;
                    break;
                }
            }
            grouped_weights[representative] += weights[k];
        }
    }
    double acc = 0.0;                     // FP64 累加 (§30.4 精度合同)
    bool any_nan = false, any_missing = false;
    int n_masked = 0;                     // 生效权重 0 = signal 路径已剔除的样本
    for (int k = 0; k < npts; ++k) {
        const double w = (npts == 1) ? 1.0 : grouped_weights[k];
        double u_k = 0;
        P3UncPixelState s_k = P3_U_OK;
        const P3ResampleStatus r = read_u_leaf(u, leaf_ipix[k], &u_k, &s_k);
        if (r != P3_RS_OK) return r;      // 负/Inf 损坏 → run 拒绝 (fail-closed 不变)
        // 样本级掩膜 (rule_id NAN-SAMPLE-MASK-COVERAGE-NAN, DATA-002 §2a 规则 1:
        // 不合格样本从分子、分母、**方差**三项一并剔除): p3_sample_bilinear_ex
        // 暴露的生效权重对被剔除样本恰为 0 ⇒ 该项 c²u 恰为 0 ⇒ 从方差一并剔除,
        // 不参与 NaN/missing 合成。u 仍读入 ⇒ 产品损坏 (负/Inf) 仍 fail-closed。
        if (npts == 4 && w == 0.0) { ++n_masked; continue; }
        if (s_k == P3_U_MISSING) { any_missing = true; continue; }
        if (s_k == P3_U_NAN) { any_nan = true; continue; }
        acc += w * w * u_k;               // var_out = Σ c_k²·u_k (nearest: c=1)
    }
    if (npts == 4 && n_masked == npts) {
        // 零合格样本 (与 signal 路径同一判定, W_p=0) ⇒ 覆盖级 NaN;
        // 禁静默 0 冒充无效 (DATA-002 §2a 规则 2)。
        *st = P3_U_NAN; *u_out = std::nanf(""); return P3_RS_OK;
    }
    // 合成序: 损坏 > missing > NaN > 数值 (§30.4 invalid 表:
    // 覆盖不一致与 NaN 传播输出面同为 NaN, 差别在 missing 计数语义)
    if (any_missing) { *st = P3_U_MISSING; *u_out = std::nanf(""); return P3_RS_OK; }
    if (any_nan) { *st = P3_U_NAN; *u_out = std::nanf(""); return P3_RS_OK; }
    *st = P3_U_OK;
    *u_out = acc;
    return P3_RS_OK;
}

}  // namespace acsd::phase3
