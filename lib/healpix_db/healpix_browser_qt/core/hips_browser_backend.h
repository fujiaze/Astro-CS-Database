// ============================================================================
// hips_browser_backend.h - HiPS 产品集浏览器后端 (Phase1 Final Signoff )
//
// 正式 Browser 数据源: HiPS Product Set (signal/support/snr)
// 仅通过 astro_image_io.dll 的 AIO HiPS Reader API 读取, 不直接链接 CFITSIO。
// HISS 仅保留为 legacy (BrowserBackend::open_file 旧路径)。
//
// LOD: 直接使用 writer 已生成的 Norder0..K 层级; 全分辨率查询直接按
// NESTED leaf ipix 定位叶级 tile (512×512), FITS 行主序 [y*512+x]。
// ============================================================================

#ifndef HIPS_BROWSER_BACKEND_H
#define HIPS_BROWSER_BACKEND_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct AioHipsDataset;  // 不透明句柄 (aio_hips_reader.h)

// ============================================================================
// HiPS Browser 后端
// ============================================================================
class HipsBrowserBackend {
public:
    HipsBrowserBackend();
    ~HipsBrowserBackend();

    // 打开产品集根目录 (内含 signal/ support/ snr/ 三个子产品)
    // 返回 0=成功, <0=失败
    int open_product(const std::string& out_dir);
    void close();

    bool is_open() const { return sig_ != nullptr; }
    bool is_fp64() const { return fp64_; }
    int get_hips_order() const { return order_; }
    int get_leaf_order() const { return leaf_order_; }
    int get_tile_width() const { return width_; }
    uint64_t get_n_tiles() const;
    std::string get_root() const { return root_; }
    bool is_flat_standard() const { return flat_; }  // Hipsgen 扁平布局

    // 查询 (ra,dec) -> signal/support
    // 0=成功; -2=outside MOC / tile 缺失; -3=内部错误
    int query_pixel(double ra, double dec, double& signal, double& support) const;

    // 读取叶级 tile 为 double (LOD/渲染用; 空区为 NaN)
    int read_tile(uint64_t tile_ipix, std::vector<double>& out) const;
    // 叶级 support tile（reference renderer 用）
    int read_support_tile(uint64_t tile_ipix, std::vector<double>& out) const;

    // ---- 多 order 读取（浏览器 LOD 用） ----
    // 读取指定 order 的 signal/support tile（512×512 float）。
    // tile 不存在返回非 0；成功时 sig/sup 均填充（缺失产品返回 -3）。
    int read_tile_at_order(int order, uint64_t tile_ipix,
                           std::vector<float>& sig,
                           std::vector<float>& sup) const;

    // 指定 order 下存在的 tile ipix 集合（目录扫描，缓存）。
    // order 范围 [0, get_hips_order()]。
    const std::vector<uint64_t>& tiles_at_order(int order) const;

    // order 层是否存在该 tile
    bool has_tile_at_order(int order, uint64_t tile_ipix) const;

    // SNR Catalogue 全量读取
    int read_snr_catalog(std::vector<double>& ra, std::vector<double>& dec,
                         std::vector<double>& snr, std::vector<int64_t>& star_id,
                         std::vector<uint32_t>& quality_flags,
                         std::vector<uint32_t>& photometric_status) const;

    // 判断 (ra,dec) 是否在产品 MOC 内
    bool contains(double ra, double dec) const;

private:
    void load_order_tiles(int order) const;   // 惰性目录扫描

    AioHipsDataset* sig_ = nullptr;
    AioHipsDataset* sup_ = nullptr;
    AioHipsDataset* snr_ = nullptr;
    std::string root_;
    int order_ = 0;        // 叶级 tile 阶 (NorderK)
    int leaf_order_ = 0;   // 叶像素阶 = order_ + 9 (512×512 tile)
    int width_ = 512;
    bool fp64_ = false;
    mutable std::map<int, std::vector<uint64_t>> order_tiles_;
    bool flat_ = false;  // 标准扁平布局（root/properties + root/NorderK）
};

#endif // HIPS_BROWSER_BACKEND_H
