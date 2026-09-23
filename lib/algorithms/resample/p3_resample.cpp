// lib/phase3_session/p3_resample.cpp — 重采样实现 (ALG-P3-003) — P3-003
// 叶级 nside = 512·2^K(K=properties order); NEAREST/BILINEAR 均经 healpix_core
// 权威函数(ang2pix/pix2ang/neighbors) — 禁止第二套数学核心。
//
// P30 (Phase3 导出性能 P0) 修复注记 —— 采样数学逐位不变, 只改 tile 存取层:
//   修复前实测 (真实 M42 T2+Blue, 14375² = 206.6 Mpx, 0.805″/px):
//     * 每个 worker 的 P3Sampler 是 p3_sampler_open_ex 新建实例, max_tiles
//       (默认 804) 只设到了主 sampler 上 → worker 实际 cache.cap = 8;
//       产物 523 个 tile 的工作集 >> 8 → 逐行抖动, 同一 tile 被反复解码。
//     * tile 缺失(覆盖外)没有负缓存: 该平面 34.2% 像素在 MOC 之外 (实测
//       astropy-healpix 逐格核对), 每像素最多 4 次 read_leaf → ~2.8e8 次
//       fits_open_file 打在**不存在的文件**上; 该调用全程持有进程级
//       aio::cfitsio_io_mutex (RT-008), 且每次构造路径/错误串 → 全进程
//       串行化 + 高 sys 时间, 实测 17 线程只用 ~1.9 核 (门禁 §10.5 需 ≥85%)。
//   本文件修复 = ①跨 worker 共享一个**有界 LRU** tile 缓存 (容量 = max_tiles,
//   与 worker 数无关 → 峰值内存不随核数增长); ②缺失 tile 负缓存 (每 tile 至多
//   一次真实 open, 结果与修复前逐位相同: 那些 open 本来就必然失败);
//   ③每线程前端热缓存 (命中不取共享锁); ④bilinear 每像素不再堆分配;
//   ⑤暴露缓存统计供 §10.5 资源证据。
//   数学路径 (ang2pix_nest / pix2ang_nest / neighbors / 四象限双线性 / NaN 与
//   coverage 语义) 与修复前逐行等价。
#include "p3_resample.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "healpix_core.h"
#include "aio_hips_reader.h"
#include "hips_properties.h"

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): 子产品 properties
// 的整文件读取经 aio 唯一实现 (aio_file::read_all), 本 TU 不自持 FILE*。
#include "aio_atomic_file.h"
#include "aio_file_io.h"

namespace astrocs::phase3 {

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
//  - absent = 负缓存: tile 不存在或读失败 → 记一次, 之后直接返回缺失,
//    不再对不存在的文件调 fits_open_file。语义与"每次 open 都失败"完全等价。
//  - 表操作在 mu 内, tile I/O 在 mu 外 (read_tile 自带进程级 CFITSIO 互斥),
//    避免缓存锁与 I/O 锁嵌套。
struct SharedTileCache {
    struct Entry {
        std::shared_ptr<const TileData> data;
        std::list<uint64_t>::iterator it;
    };
    mutable std::mutex mu;
    size_t cap = 8;                        // 默认 (可配置 max_tiles)
    std::list<uint64_t> lru;               // front = MRU
    std::unordered_map<uint64_t, Entry> map;
    std::unordered_map<uint64_t, uint8_t> absent;
    uint64_t stat_hits = 0;
    uint64_t stat_misses = 0;
    uint64_t stat_absent = 0;              // 负缓存写入次数 (=真实失败 open 次数)
    uint64_t stat_evict = 0;

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
        absent.emplace(k, 1u);
        ++stat_absent;
    }
    // P30 证据/回归: 真实失败的 tile open 次数 (与负缓存命中区分)。负缓存关闭时
    // 该计数按"每像素重试"线性增长 —— 回归用例的阴性对照正是据此判红。
    void note_open_failure() {
        std::lock_guard<std::mutex> lk(mu);
        ++stat_open_fail;
    }
    uint64_t stat_open_fail = 0;
};
}  // namespace

struct P3SamplerImpl {
    AioHipsDataset* ds = nullptr;
    int order = 0;                       // properties 实测 order(K)
    uint32_t leaf_nside = 512;           // 512·2^K
    // 共享有界 tile 缓存: 同一工作集合的多个 worker 指向同一实例 (P30)。
    std::shared_ptr<SharedTileCache> cache;
    // 本 sampler 专属前端热缓存 (每线程一个 sampler ⇒ 无锁访问)。
    //   hot_state: 0=空槽 1=命中(hot_val 有效, pin 住 tile) 2=已知缺失
    uint64_t hot_key[kHotSlots];
    uint8_t hot_state[kHotSlots];
    std::shared_ptr<const TileData> hot_val[kHotSlots];
    std::string root;
    std::string last_err;
    P3UncertaintySource unc_src = P3_UNC_NONE;   // uncertainty 子产品类型标记
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

// 取一个 tile (命中返回 pin 住的共享指针; 缺失/读失败返回 nullptr)。
// 顺序: 线程本地热缓存 → 共享 LRU(锁) → 负缓存 → 真读(进程级 CFITSIO 互斥)。
static std::shared_ptr<const TileData> fetch_tile(P3SamplerImpl* s, uint64_t tip) {
    const size_t slot = static_cast<size_t>(tip * 0x9E3779B97F4A7C15ull) &
                        (kHotSlots - 1);
    if (s->hot_state[slot] != 0 && s->hot_key[slot] == tip)
        return s->hot_state[slot] == 1 ? s->hot_val[slot]
                                       : std::shared_ptr<const TileData>();
    if (std::shared_ptr<const TileData> hit = s->cache->get(tip)) {
        s->hot_key[slot] = tip; s->hot_state[slot] = 1; s->hot_val[slot] = hit;
        return hit;
    }
    if (s->absent_cache && s->cache->is_absent(tip)) {
        s->hot_key[slot] = tip; s->hot_state[slot] = 2; s->hot_val[slot].reset();
        return std::shared_ptr<const TileData>();
    }
    auto tile = std::make_shared<TileData>(static_cast<size_t>(kReaderBuf));
    if (aio_hips_read_tile_f32(s->ds, tip, tile->data()) != 0) {
        s->last_err = std::string("read_tile ") + std::to_string(tip) + ": " +
                      aio_hips_reader_last_error();
        s->cache->note_open_failure();       // 观测: 真实失败 open (含关闭负缓存时)
        if (s->absent_cache) {
            s->cache->mark_absent(tip);      // 负缓存 (修复前: 每像素重试)
            s->hot_key[slot] = tip; s->hot_state[slot] = 2; s->hot_val[slot].reset();
        }
        return std::shared_ptr<const TileData>();
    }
    s->cache->put(tip, tile);
    s->hot_key[slot] = tip; s->hot_state[slot] = 1; s->hot_val[slot] = tile;
    return tile;
}

// 读一个叶级像素: 命中=值; tile 缺失=false(coverage=0); NaN 像素=命中(值 NaN)
static bool read_leaf(P3SamplerImpl* s, uint64_t leaf_ipix, float* out) {
    // leaf nside = 512·2^K → tile_order=K 的父 ipix; 局部 512² 由 nested_local 映射
    const int tile_order = s->order;
    const uint32_t leaf_order = static_cast<uint32_t>(tile_order) + 9;
    const uint64_t tip = astrocs::healpix::leaf_to_tile_nest(leaf_ipix, leaf_order,
                                                             static_cast<uint32_t>(tile_order));   // 传"阶"非 nside
    const std::shared_ptr<const TileData> tile = fetch_tile(s, tip);
    if (!tile) return false;   // 缺 tile
    // leaf→tile 内标准 HiPS 排列索引
    const uint64_t first = astrocs::healpix::tile_to_leaf_nest(
        tip, static_cast<uint32_t>(tile_order), leaf_order);
    const uint64_t local = leaf_ipix - first;
    const uint64_t fits_index = astrocs::healpix::nested_local_to_fits_index(
        local, 9, kTileWidth);
    *out = (*tile)[fits_index];   // NaN 是命中(值语义, §4)
    return true;
}

P3ResampleStatus p3_order_select(int max_order, double scale_deg_per_px,
                                       int* out_order) {
    if (!out_order || max_order < 0 || max_order > kMaxOrder || !(scale_deg_per_px > 0))
        return P3_RS_PARAM;
    for (int k = 0; k <= max_order; ++k) {
        const double res_deg = astrocs::healpix::pixel_resolution_arcsec(
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
    out->impl = s;
    // 暴露输入实际 order 与 BUNIT(缺省 ADU/sr, 绝不 Jy/beam)
    if (out_order) *out_order = p.order;
    // 采样值 = 输入 HiPS tile 值的凸组合 ⇒ 与输入同单位（面亮度，canonical "ADU/sr"，
    // DATA_SEMANTICS §29.3/§31.1a）；缺省串取该平面物理单位，禁裸 ADU（每像素口径）。
    if (out_bunit) *out_bunit = p.bunit.empty() ? std::string("ADU/sr") : p.bunit;
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
    const uint64_t leaf = astrocs::healpix::ang2pix_nest(impl->leaf_nside, ra_deg, dec_deg);
    if (leaf_ipix) *leaf_ipix = leaf;   // 恒输出 (c=0 时供 missing 语义消费)
    float v = 0;
    if (!read_leaf(impl, leaf, &v)) { *value = std::nanf(""); *coverage = 0; return P3_RS_OK; }
    *value = v;              // NaN 直传(§4: S=NaN, C=1)
    *coverage = 1;
    return P3_RS_OK;
}

P3ResampleStatus p3_sample_bilinear(P3Sampler* s, double ra_deg, double dec_deg,
                                          float* value, int* coverage) {
    return p3_sample_bilinear_ex(s, ra_deg, dec_deg, value, coverage, nullptr, nullptr);
}

// P30: 3×3 邻域点由 std::vector 改为定长栈数组 (单像素不再堆分配); 取样/
// 四象限选择/退化填充/双线性权重与 NaN·coverage 语义与修复前逐行等价。
P3ResampleStatus p3_sample_bilinear_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                       float* value, int* coverage,
                                       double weights[4], uint64_t leaf_ipix[4]) {
    // 薄封装: 同一数学路径 (样本级掩膜版), 不暴露计数
    return p3_sample_bilinear_nanmask_ex(s, ra_deg, dec_deg, value, coverage,
                                         weights, leaf_ipix, nullptr);
}

// 样本级掩膜口径 (rule_id NAN-SAMPLE-MASK-COVERAGE-NAN) 的唯一实现:
//   权威 = docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md §2a
//   invalid_handling 块 (唯一正本) + docs/standards/NUMERIC_STANDARD.md §MUST
//   + ALG-P3-003 §2 G4/§4 (docs/algorithms/PHASE3_RESAMPLE.md)。
//   ①不合格样本 = ¬isfinite(值) (NaN 与 ±Inf 同类);
//   ②从分子、分母、方差三项一并剔除 (被剔除样本的生效权重恰为 0 ⇒ 不留在分母);
//   ③对剩余合格邻域重归一 c_k = w_k / Σ(合格 w_j) (FP64, 固定 k 序 ⇒ 确定性);
//   ④仅零合格样本 (n_eligible==0) 或有效权重和 D_p==0 时 S=NaN (覆盖级 NaN);
//   ⑤C 只判足迹内有无 tile 像素: 4 个 tile 均可读则 C=1, 值 NaN 不改 C (§4);
//   ⑥强制计数: 被剔除样本数按原因分类暴露 (禁静默剔除; 禁零填替代)。
P3ResampleStatus p3_sample_bilinear_nanmask_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                               float* value, int* coverage,
                                               double weights[4], uint64_t leaf_ipix[4],
                                               P3SampleRejection* rejection) {
    if (rejection) *rejection = P3SampleRejection{};   // 计数 0 与「字段缺失」可区分
    if (!s || !s->impl || !value || !coverage) return P3_RS_PARAM;
    auto* impl = s->impl;
    const uint32_t nside = impl->leaf_nside;
    const uint64_t ipix = astrocs::healpix::ang2pix_nest(nside, ra_deg, dec_deg);
    // 3×3 邻域(中心+8 邻居)投影到样本点切平面
    const std::vector<uint64_t> nb = astrocs::healpix::neighbors(nside, ipix);
    struct P { uint64_t ipix; double x, y; };   // 切平面坐标(deg)
    P pts[10];
    int np = 0;
    const double d0r = dec_deg * M_PI / 180.0, a0r = ra_deg * M_PI / 180.0;
    auto add_pt = [&](uint64_t ip) {
        if (np >= 10) return;
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(nside, ip, ra, dec);
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
    // 角点值读取(缺 tile → coverage=0)
    float v00, v10, v01, v11;
    const bool g00 = read_leaf(impl, q[0][0]->ipix, &v00);
    const bool g10 = read_leaf(impl, q[1][0]->ipix, &v10);
    const bool g01 = read_leaf(impl, q[0][1]->ipix, &v01);
    const bool g11 = read_leaf(impl, q[1][1]->ipix, &v11);
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
    double wsum = 0.0;                 // D_p = Σ_{合格} w_j
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
        // 覆盖级 NaN (DATA-002 §2a 规则 2: 零合格样本 / D_p=0):
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
        // CLEAN-403: 读取经 aio (aio_file::read_all)。
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
    if (!read_leaf(impl, leaf, &v)) { *out = std::nanf(""); *st = P3_U_MISSING; return P3_RS_OK; }
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
    double acc = 0.0;                     // FP64 累加 (§30.4 精度合同)
    bool any_nan = false, any_missing = false;
    int n_masked = 0;                     // 生效权重 0 = signal 路径已剔除的样本
    for (int k = 0; k < npts; ++k) {
        const double w = (npts == 1) ? 1.0 : weights[k];
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
        // 零合格样本 (与 signal 路径同一判定, D_p=0) ⇒ 覆盖级 NaN;
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

}  // namespace astrocs::phase3
