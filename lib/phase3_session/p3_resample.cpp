// lib/phase3_session/p3_resample.cpp — 重采样实现 (ALG-P3-003) — P3-003
// 叶级 nside = 512·2^K(K=properties order); NEAREST/BILINEAR 均经 healpix_core
// 权威函数(ang2pix/pix2ang/neighbors) — 禁止第二套数学核心。
#include "p3_resample.h"

#include <cmath>
#include <cstring>
#include <new>
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
    size_t cap = 8;   // 默认; 可配置(max_tiles)
    const float* get(uint64_t k) const {
        for (size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == k) return tiles[i].data();
        return nullptr;
    }
    void put(uint64_t k, std::vector<float>&& t) {
        if (keys.size() >= cap) { keys.erase(keys.begin()); tiles.erase(tiles.begin()); }
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
    P3UncertaintySource unc_src = P3_UNC_NONE;   // uncertainty 子产品类型标记
};

// 读一个叶级像素: 命中=值; tile 缺失=false(coverage=0); NaN 像素=命中(值 NaN)
static bool read_leaf(P3SamplerImpl* s, uint64_t leaf_ipix, float* out) {
    // leaf nside = 512·2^K → tile_order=K 的父 ipix; 局部 512² 由 nested_local 映射
    const int tile_order = s->order;
    const uint32_t leaf_order = static_cast<uint32_t>(tile_order) + 9;
    const uint64_t tip = astrocs::healpix::leaf_to_tile_nest(leaf_ipix, leaf_order,
                                                             static_cast<uint32_t>(tile_order));   // 传"阶"非 nside
    if (const float* hit = s->cache.get(tip)) {
        // 缓存命中: leaf→tile 内标准 HiPS 排列索引
        const uint64_t first = astrocs::healpix::tile_to_leaf_nest(
            tip, static_cast<uint32_t>(tile_order), leaf_order);
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
    const uint64_t first = astrocs::healpix::tile_to_leaf_nest(
        tip, static_cast<uint32_t>(tile_order), leaf_order);
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
    // §4 显式拒清单: weight/flux-per-pixel 输入模式 (不变, SCI §9a-8);
    // "variance"/"ivar" 依 DATA-P3-UNC-001 §30.4-4 从 UNSUPPORTED 拒绝项移除
    // (转 uncertainty 子产品消费面) → 落未知输入模式 PARAM。
    if (m == "weight" || m == "flux-per-pixel") return P3_RS_UNSUPPORTED;
    if (m == "surface_brightness") return P3_RS_OK;
    return P3_RS_PARAM;
}

void p3_sampler_set_max_tiles(P3Sampler* s, int max_tiles) {
    if (!s || !s->impl) return;
    if (max_tiles <= 0) { s->impl->cache.cap = 8; return; }
    s->impl->cache.cap = static_cast<size_t>(max_tiles);
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
    // 暴露输入实际 order 与 BUNIT(缺省 ADU, 绝不 Jy/beam)
    if (out_order) *out_order = p.order;
    if (out_bunit) *out_bunit = p.bunit.empty() ? std::string("ADU") : p.bunit;
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

P3ResampleStatus p3_sample_bilinear_ex(P3Sampler* s, double ra_deg, double dec_deg,
                                       float* value, int* coverage,
                                       double weights[4], uint64_t leaf_ipix[4]) {
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
    if (weights) {
        weights[0] = w00; weights[1] = w10; weights[2] = w01; weights[3] = w11;
    }
    if (leaf_ipix) {
        leaf_ipix[0] = q[0][0]->ipix; leaf_ipix[1] = q[1][0]->ipix;
        leaf_ipix[2] = q[0][1]->ipix; leaf_ipix[3] = q[1][1]->ipix;
    }
    (void)y1; (void)c_ra; (void)c_dec;
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
        std::FILE* pf = std::fopen(props_path.c_str(), "rb");
        if (!pf) continue;                       // 子产品不存在 → 试下一候选
        std::string text;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), pf)) > 0) text.append(buf, n);
        const bool read_ok = (std::ferror(pf) == 0);
        std::fclose(pf);
        if (!read_ok) return P3_RS_IO;
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
    for (int k = 0; k < npts; ++k) {
        double u_k = 0;
        P3UncPixelState s_k = P3_U_OK;
        const P3ResampleStatus r = read_u_leaf(u, leaf_ipix[k], &u_k, &s_k);
        if (r != P3_RS_OK) return r;      // 负/Inf 损坏 → run 拒绝
        if (s_k == P3_U_MISSING) { any_missing = true; continue; }
        if (s_k == P3_U_NAN) { any_nan = true; continue; }
        const double w = (npts == 1) ? 1.0 : weights[k];
        acc += w * w * u_k;               // var_out = Σ c_k²·u_k (nearest: c=1)
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
