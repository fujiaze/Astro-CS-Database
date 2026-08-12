// ============================================================================
// hips_sky_view.h - V9 HiPS 2D 天空视图核心（无 Qt 依赖）
//
// 直接消费 AstroCS 标准 HiPS 产品集（signal/support，NESTED，Norder0..K）：
//   - 视口感知：gnomonic 逆投影逐像素求 (ra,dec) → 只触碰可见 tile；
//   - zoom-aware order：按 FOV 选 NorderK，父级缺失时逐级向上回退；
//   - 有界 LRU tile 缓存（默认 64 张，signal+support 双数组）；
//   - Signal/Support 图层切换；线性/sqrt/log/asinh 拉伸 + auto/manual B/W；
//   - 进度式渲染：可见 tile 即画，缺失区为背景色（不阻塞等全量）。
// ============================================================================

#ifndef HIPS_SKY_VIEW_H
#define HIPS_SKY_VIEW_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "stf_engine.h"

class HipsBrowserBackend;

class HipsSkyView {
public:
    HipsSkyView();

    void set_backend(HipsBrowserBackend* backend);  // 借用指针，不拥有
    void set_view(double center_ra, double center_dec, double fov_deg,
                  double aspect);
    void set_size(int width, int height);
    void set_layer(int layer);              // 0=signal, 1=support
    void set_stretch(const std::string& preset, bool auto_range);
    void set_manual_range(float lo, float hi);
    // V11：LOD 模式。strict-leaf 完全禁止 parent fallback（诊断用）。
    void set_lod_mode(bool strict_leaf) { strict_leaf_ = strict_leaf; }
    void set_cache_cap(std::size_t n) {
        cache_cap_ = (n >= 4) ? n : 4;
    }

    double center_ra() const { return ra0_; }
    double center_dec() const { return dec0_; }
    double fov() const { return fov_; }
    int layer() const { return layer_; }
    bool is_support() const { return layer_ == 1; }

    int target_order() const;

    struct Stats {
        int order = 0;
        std::size_t tiles_requested = 0;
        std::size_t cache_hits = 0;
        std::size_t tiles_decoded = 0;
        std::size_t parent_fallbacks = 0;
        double frame_ms = 0.0;
        double decode_ms = 0.0;
        float data_min = 0.0f;
        float data_max = 1.0f;
        std::size_t valid_pixels = 0;
    };
    const Stats& last_stats() const { return stats_; }

    // 渲染到 RGBA8（w*h 个 uint32，0xAABBGGRR）
    void rasterize(std::vector<std::uint32_t>& rgba);

    // 光标/测试查询（leaf order，AIO 路径）
    int query_sky(double ra, double dec, double& sig, double& sup) const;

    // 指定 order 直接采样（几何测试用）；成功返回 0
    int sample_at(double ra, double dec, int order, float& out) const;

    struct Metrics {
        std::size_t evictions = 0;
        std::vector<double> decode_ms_hist;
        std::size_t total_tile_reads = 0;
        std::size_t cache_misses = 0;
        std::size_t cache_hits_total = 0;
    };
    const Metrics& metrics() const { return metrics_; }
    void reset_metrics();

private:
    struct Tile {
        int order = 0;
        std::uint64_t ipix = 0;
        std::vector<float> sig;
        std::vector<float> sup;
        std::uint64_t stamp = 0;
    };

    Tile* get_tile(int order, std::uint64_t ipix);   // LRU 查找或解码
    bool sample_value(double ra, double dec, float& out);  // layer-aware
    void evict_one();

    HipsBrowserBackend* bk_ = nullptr;
    double ra0_ = 0.0, dec0_ = 0.0, fov_ = 8.0, aspect_ = 1.0;
    int w_ = 1024, h_ = 768;
    int layer_ = 0;
    std::string preset_ = "asinh";
    bool auto_range_ = true;
    bool strict_leaf_ = false;
    float lo_ = 0.0f, hi_ = 1.0f;
    std::uint64_t clock_ = 0;
    std::size_t cache_cap_ = 64;
    std::map<std::pair<int, std::uint64_t>, std::shared_ptr<Tile>> cache_;
    Stats stats_;
    Metrics metrics_;
};

#endif  // HIPS_SKY_VIEW_H
