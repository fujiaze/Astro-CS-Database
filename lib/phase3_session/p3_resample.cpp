// lib/phase3_session/p3_resample.cpp — 重采样实现 (ALG-P3-003) — P3-003
// 叶级 nside = 512·2^K(K=properties order); NEAREST/BILINEAR 均经 healpix_core
// 权威函数(ang2pix/pix2ang/neighbors) — 禁止第二套数学核心。
#include "p3_resample.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "healpix_core.h"
#include "aio_hips_reader.h"
#include "hips_properties.h"

namespace astrocs::phase3 {

namespace {
constexpr uint32_t kTileWidth = 512;
constexpr int kReaderBuf = 512 * 512;

// 简单 8-tile 缓存(确定性逐出=最旧), 跨 tile 采样避免反复 IO
struct TileCache {
    std::vector<uint64_t> keys;
    std::vector<std::vector<float>> tiles;
    const float* get(uint64_t k) const {
        for (size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == k) return tiles[i].data();
        return nullptr;
    }
    void put(uint64_t k, std::vector<float>&& t) {
        if (keys.size() == 8) { keys.erase(keys.begin()); tiles.erase(tiles.begin()); }
        keys.push_back(k);
        tiles.push_back(std::move(t));
    }
};
}  // namespace

struct P3SamplerImpl {
    AioHipsDataset* ds = nullptr;
    int order = 0;                       // properties 实测 order(K)
    uint32_t leaf_nside = 512;           // 512·2^K
    TileCache cache;
    std::string root;
    std::string last_err;
};

// 读一个叶级像素: 命中=值; tile 缺失=false(coverage=0); NaN 像素=命中(值 NaN)
static bool read_leaf(P3SamplerImpl* s, uint64_t leaf_ipix, float* out) {
    // leaf nside = 512·2^K → tile_order=K 的父 ipix; 局部 512² 由 nested_local 映射
    const int tile_order = s->order;
    const uint32_t leaf_order = tile_order + 9;
    const uint64_t tip = astrocs::healpix::leaf_to_tile_nest(leaf_ipix, leaf_order,
                                                             tile_order);   // 传"阶"非 nside
    if (const float* hit = s->cache.get(tip)) {
        // 缓存命中: leaf→tile 内标准 HiPS 排列索引
        const uint64_t first = astrocs::healpix::tile_to_leaf_nest(tip, tile_order, leaf_order);
        const uint64_t local = leaf_ipix - first;
        const uint64_t fits_index = astrocs::healpix::nested_local_to_fits_index(
            local, 9, kTileWidth);
        *out = hit[fits_index];   // NaN 是命中(值语义, §4)
        return true;
    }
    std::vector<float> tile(kReaderBuf);
    if (aio_hips_read_tile_f32(s->ds, tip, tile.data()) != 0) {
        s->last_err = std::string("read_tile ") + std::to_string(tip) + ": " +
                      aio_hips_reader_last_error();
        return false;   // 缺 tile
    }
    s->cache.put(tip, std::move(tile));
    const uint64_t first = astrocs::healpix::tile_to_leaf_nest(tip, tile_order, leaf_order);
    const uint64_t local = leaf_ipix - first;
    const uint64_t fits_index = astrocs::healpix::nested_local_to_fits_index(local, 9,
                                                                             kTileWidth);
    const float* hit = s->cache.get(tip);
    *out = hit[fits_index];
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
    // §4 显式拒清单: variance/weight/ivar/flux-per-pixel 输入模式
    if (m == "variance" || m == "ivar" || m == "weight" || m == "flux-per-pixel")
        return P3_RS_UNSUPPORTED;
    if (m == "surface_brightness") return P3_RS_OK;
    return P3_RS_PARAM;
}

P3ResampleStatus p3_sampler_open(const char* product_dir, P3Sampler* out,
                                       std::string* err) {
    if (!product_dir || !out) return P3_RS_PARAM;
    // product_dir = HiPS 根(内含 signal/ 子产品); 严格校验 signal/properties(P3-001)
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
    return P3_RS_OK;
}

P3ResampleStatus p3_sample_nearest(P3Sampler* s, double ra_deg, double dec_deg,
                                         float* value, int* coverage) {
    if (!s || !s->impl || !value || !coverage) return P3_RS_PARAM;
    auto* impl = s->impl;
    const uint64_t leaf = astrocs::healpix::ang2pix_nest(impl->leaf_nside, ra_deg, dec_deg);
    float v = 0;
    if (!read_leaf(impl, leaf, &v)) { *value = std::nanf(""); *coverage = 0; return P3_RS_OK; }
    *value = v;              // NaN 直传(§4: S=NaN, C=1)
    *coverage = 1;
    return P3_RS_OK;
}

P3ResampleStatus p3_sample_bilinear(P3Sampler* s, double ra_deg, double dec_deg,
                                          float* value, int* coverage) {
    if (!s || !s->impl || !value || !coverage) return P3_RS_PARAM;
    auto* impl = s->impl;
    const uint32_t nside = impl->leaf_nside;
    const uint64_t ipix = astrocs::healpix::ang2pix_nest(nside, ra_deg, dec_deg);
    double c_ra = 0, c_dec = 0;
    astrocs::healpix::pix2ang_nest(nside, ipix, c_ra, c_dec);
    // 3×3 邻域(中心+8 邻居)投影到样本点切平面
    std::vector<uint64_t> nb = astrocs::healpix::neighbors(nside, ipix);
    struct P { uint64_t ipix; double x, y; };   // 切平面坐标(deg)
    std::vector<P> pts;
    pts.reserve(9);
    const double d0r = dec_deg * M_PI / 180.0, a0r = ra_deg * M_PI / 180.0;
    auto project = [&](double ra, double dec) {
        const double ar = ra * M_PI / 180.0, dr = dec * M_PI / 180.0;
        const double den = std::sin(d0r) * std::sin(dr) +
                           std::cos(d0r) * std::cos(dr) * std::cos(ar - a0r);
        if (den <= 0) return false;
        const double xi = std::cos(dr) * std::sin(ar - a0r) / den;
        const double eta = (std::sin(dr) * std::cos(d0r) -
                            std::cos(dr) * std::sin(d0r) * std::cos(ar - a0r)) / den;
        return true;
    };
    (void)project;
    auto add_pt = [&](uint64_t ip) {
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(nside, ip, ra, dec);
        const double ar = ra * M_PI / 180.0, dr = dec * M_PI / 180.0;
        const double den = std::sin(d0r) * std::sin(dr) +
                           std::cos(d0r) * std::cos(dr) * std::cos(ar - a0r);
        if (den <= 0) return;
        const double xi = std::cos(dr) * std::sin(ar - a0r) / den;
        const double eta = (std::sin(dr) * std::cos(d0r) -
                            std::cos(dr) * std::sin(d0r) * std::cos(ar - a0r)) / den;
        pts.push_back({ip, xi, eta});
    };
    add_pt(ipix);
    for (uint64_t n : nb) add_pt(n);
    // 四象限最近中心(确定性: 距离并列时取更小 ipix)
    const P* q[2][2] = {{nullptr, nullptr}, {nullptr, nullptr}};   // [x<0|x>0][y<0|y>0]
    double best_d[2][2] = {{1e300, 1e300}, {1e300, 1e300}};
    for (const auto& p : pts) {
        const int ix = p.x >= 0 ? 1 : 0;
        const int iy = p.y >= 0 ? 1 : 0;
        const double d2 = p.x * p.x + p.y * p.y;
        if (d2 < best_d[ix][iy] || (d2 == best_d[ix][iy] && p.ipix < q[ix][iy]->ipix)) {
            best_d[ix][iy] = d2;
            q[ix][iy] = &pts[static_cast<size_t>(&p - pts.data())];
        }
    }
    // 退化防护: 角点位置某些象限可能无邻域点 → 用最近邻点填充(确定性双线性退化)
    const P* nearest_pt = nullptr;
    double nd = 1e300;
    for (const auto& p : pts) {
        const double d2 = p.x * p.x + p.y * p.y;
        if (d2 < nd) { nd = d2; nearest_pt = &pts[static_cast<size_t>(&p - pts.data())]; }
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
    const double w00 = (1 - u) * (1 - v), w10 = u * (1 - v);
    const double w01 = (1 - u) * v, w11 = u * v;
    const double val = w00 * v00 + w10 * v10 + w01 * v01 + w11 * v11;
    const bool any_nan = std::isnan(v00) || std::isnan(v10) || std::isnan(v01) ||
                         std::isnan(v11);
    *value = any_nan ? std::nanf("") : static_cast<float>(val);
    *coverage = 1;   // §4: NaN 参与仍 C=1(S=NaN)
    (void)y1; (void)c_ra; (void)c_dec;
    return P3_RS_OK;
}

void p3_sampler_close(P3Sampler* s) {
    if (!s || !s->impl) return;
    if (s->impl->ds) aio_hips_close(s->impl->ds);
    delete s->impl;
    s->impl = nullptr;
}

}  // namespace astrocs::phase3
