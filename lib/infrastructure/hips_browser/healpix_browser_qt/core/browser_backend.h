// browser_backend.h - HEALPix 浏览器数据后端 (healpix_browser_qt)
// 功能: 管理 .hiss/.hcsd 文件, 按需加载子叶, 视角相关压缩, ud_grade 降采样
// 用途: 为 GLRenderer 提供数据源, 无 Qt 依赖, 无 HTTP 服务器
// 依赖: astro_image_io.dll (aio_hiss_read/aio_hcsd_read/aio_hcsd_read_leaf/aio_hio_free, 旧 API 通过兼容宏)
// 编译: C++17, 纯标准库 + astro_image_io
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md §3.1

#ifndef BROWSER_BACKEND_H
#define BROWSER_BACKEND_H

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <unordered_map>

// ============================================================================
// 视角参数 (widget 层填充后传给 core)
// ============================================================================
struct ViewParams {
    double center_ra;    // 中心赤经 (度, 从 forward 反算, 用于日志/screen_to_sky)
    double center_dec;   // 中心赤纬 (度)
    double zoom;         // 缩放级别 (1.0 = 全天, 越大越放大)
    double fov_deg;      // 视场大小 (度)
    // 双向量四元数导航 (自由滚动, 左右不旋转)
    // forward: 视线方向 (单位向量, 从球心向外)
    // up: 画面上方 (单位向量, 与 forward 正交)
    // 由 widget 层维护, renderer 直接使用不重算
    double forward_x = 1.0;
    double forward_y = 0.0;
    double forward_z = 0.0;
    double up_x = 0.0;
    double up_y = 0.0;
    double up_z = 1.0;
};

// ============================================================================
// 子叶数据 (内存由 BrowserBackend 持有, 调用者用完调 release_leaf)
// ============================================================================
struct LeafData {
    uint64_t leaf_ipix;  // nside=64 子叶 ipix
    uint64_t n_pix;      // 像素数
    uint64_t* ipix;      // ipix 数组 (malloc 分配 或 指向 all_ipix_ 切片, 见 owned)
    float* pixel;        // 像素值数组 (float32, malloc 或 切片)
    uint8_t* pixel_u8;   // uint8 降采样结果 (owned=true 时分配, 优先于 pixel)
    uint32_t nside;      // 实际 nside (可能被降采样)
    bool owned;          // true=malloc 分配需 release_leaf 释放, false=指向 all_ipix_ 切片不释放
    bool use_u8;         // true=pixel_u8 有效, false=pixel(float) 有效

    LeafData() : leaf_ipix(0), n_pix(0), ipix(nullptr), pixel(nullptr),
                 pixel_u8(nullptr), nside(0), owned(false), use_u8(false) {}
};

// ============================================================================
// WP-H 步骤14: HISS Header 信息 (只读, 不加载像素数据)
// 用于 Browser 首次打开 .hiss 文件时只读 Header + Tile 目录
// ============================================================================
struct HissHeader {
    uint32_t nside = 0;              // HEALPix NSIDE
    uint32_t tile_nside = 0;         // Tile 父级 NSIDE
    uint32_t depth = 0;              // Tile 深度 (log2(nside/tile_nside))
    uint32_t n_leaf_per_tile = 0;    // 每个 Tile 的子叶像素数 (4^depth)
    uint64_t n_tiles = 0;            // Tile 数量
    uint64_t n_pix_total = 0;        // 全部 Tile 展开后的总像素数
    std::string meta_json;           // 元数据 JSON
    std::vector<uint64_t> tile_ipix_list;  // Tile 目录 (parent_ipix 列表)
};

// ============================================================================
// WP-H 步骤14: HISS Tile 数据 (按需加载, malloc 分配, 用 release_tile 释放)
// ============================================================================
struct HissTileData {
    uint64_t parent_ipix = 0;        // Tile 父像素 ipix
    float* signal = nullptr;         // n_leaf_per_tile 个 float32 (malloc, FP32 模式有效)
    double* signal_f64 = nullptr;    // n_leaf_per_tile 个 float64 (malloc, FP64 模式有效,)
    uint8_t* support = nullptr;      // n_leaf_per_tile 个 uint8 (malloc)
    uint32_t n_signal = 0;           // signal/signal_f64/support 数组长度
    uint8_t* snr_data = nullptr;     // n_snr_points * 8 字节 (local_ipix(u32) + snr(f32))
    uint32_t n_snr_points = 0;       // SNR 控制点数

    HissTileData() = default;
};

// ============================================================================
// 浏览器后端类
// ============================================================================
class BrowserBackend {
public:
    BrowserBackend();
    ~BrowserBackend();

    // ---- 文件管理 ----
    // 打开 .hiss 或 .hcsd 文件
    // 返回 0=成功, <0=失败
    int open_file(const std::string& path);
    void close_file();

    bool is_open() const;
    bool is_hiss() const;   // 单帧模式
    bool is_hcsd() const;   // 球面模式
    uint32_t get_nside() const;
    uint64_t get_n_pix() const;
    const std::string& get_file_path() const;
    const std::string& get_filter() const;   // 滤光片名 (从 meta 读取)

    // ---- 按需加载 (球面模式 .hcsd) ----
    // 根据视角参数获取需要加载的子叶列表 (nside=64 层)
    // 返回按距离升序排列的子叶 ipix, 限制最大 100 个
    std::vector<uint64_t> get_required_leaves(const ViewParams& view) const;

    // 加载指定子叶的数据
    // leaf_ipix: nside=64 子叶 ipix
    // target_nside: 目标 nside (若 < 原始 nside 则 ud_grade 降采样)
    LeafData load_leaf(uint64_t leaf_ipix, uint32_t target_nside);

    // 决定目标 nside (LOD 自动阈值: 按屏幕分辨率停止下钻)
    // viewport_w/h: 视口像素数, 用于计算屏幕角分辨率
    // 返回: 目标 nside (2 的幂, clamp 到 [64, nside_])
    uint32_t decide_target_nside(const ViewParams& view, uint64_t leaf_ipix,
                                  int viewport_w, int viewport_h) const;

    // ---- 降采样 ----
    // NESTED 排序位运算: ipix_coarse = ipix_fine >> (2 * log2(ratio))
    // 4^k 个相邻像素求均值合并
    // ud_grade 降采样, 输出 uint8 (归一化到 [0,255])
    // data_min/data_max: 归一化范围 (来自 STF data_range)
    LeafData ud_grade(const LeafData& input, uint32_t target_nside,
                      float data_min = 0.0f, float data_max = 1.0f);

    // 设置数据范围 (供 ud_grade 归一化用, 由 widget 在 compute_data_range 后调用)
    void set_data_range(float data_min, float data_max) {
        data_min_ = data_min;
        data_max_ = data_max;
    }

    // ---- 全量数据 (仅 .hiss 模式, 单帧切面投影用) ----
    // 返回的 LeafData 由本对象持有, close_file 时释放, 调用者不应释放
    LeafData get_all_data();

    // ---- 数据 bbox (用于初始视角设置) ----
    // 从子叶索引采样计算数据覆盖范围的 (center_ra, center_dec, width_deg, height_deg)
    // .hiss: 从子叶索引的 key (nside=64 子叶 ipix) 计算
    // .hcsd: 从文件元信息或前若干子叶采样计算 (暂返回全天)
    // 返回 0=成功, <0=失败/无数据
    int get_data_bbox(double& center_ra, double& center_dec,
                      double& width_deg, double& height_deg) const;

    // 释放 LeafData 内存 (malloc 分配的数据用此释放; get_all_data 返回的不要用此释放)
    void release_leaf(LeafData& leaf);

    // ---- WP-H 步骤14: HISS Tile 按需加载 (新 API, 不全量加载像素) ----
    // 加载 HISS 文件 Header (只读 Header + Tile 目录, 不加载像素数据)
    // path: .hiss 文件路径
    // header: 输出参数, 含 nside/tile_nside/depth/n_tiles/tile_ipix_list 等
    // 返回: 0=成功, <0=失败
    int load_hiss(const std::string& path, HissHeader& header);

    // 读取 Tile signal (按 parent_ipix, 调用 aio_hiss_read_tile_signal)
    // 用完后调 release_tile 释放
    // 注: 仅适用于 FP32 模式文件 (signal_dtype=0)
    int read_tile_signal(uint64_t parent_ipix, HissTileData& tile);

    // 读取 Tile signal (FP64 模式, 调用 aio_hiss_read_tile_signal_f64)
    // 仅适用于 FP64 模式文件 (signal_dtype=1); FP32 文件会返回错误 (禁止静默转换)
    // 用完后调 release_tile 释放 (release_tile 会释放 signal_f64)
    int read_tile_signal_f64(uint64_t parent_ipix, HissTileData& tile);

    // 读取 Tile support (按 parent_ipix, 调用 aio_hiss_read_tile_support)
    int read_tile_support(uint64_t parent_ipix, HissTileData& tile);

    // 读取 Tile SNR 控制点 (按 parent_ipix, 调用 aio_hiss_read_tile_snr)
    int read_tile_snr(uint64_t parent_ipix, HissTileData& tile);

    // 查询像素值 (ra/dec -> signal/support, 调用 aio_hiss_query_pixel)
    // 返回: 0=成功, <0=失败
    // 注: 仅适用于 FP32 模式文件 (signal_dtype=0)
    int query_pixel(double ra, double dec, float& signal, uint8_t& support);

    // 查询像素值 (FP64 版本, 调用 aio_hiss_query_pixel_f64)
    // 仅适用于 FP64 模式文件 (signal_dtype=1); FP32 文件会返回错误 (禁止静默转换)
    // 返回: 0=成功, <0=失败
    int query_pixel_f64(double ra, double dec, double& signal, uint8_t& support);

    // 查询当前 HISS 文件是否为 FP64 精度模式
    // load_hiss / open_file 时从 metadata (signal_dtype / precision_mode) 检测
    bool is_fp64() const { return is_fp64_; }

    // 释放 HissTileData 内存 (signal/signal_f64/support/snr_data)
    void release_tile(HissTileData& tile);

    // 获取已加载 HISS 文件的 Header (load_hiss 后可用)
    const HissHeader& get_hiss_header() const { return hiss_header_; }
    bool is_hiss_header_loaded() const { return hiss_header_loaded_; }

    // ---- HEALPix 角度计算辅助 (公开, 供 GLRenderer 使用) ----
    static void ipix_to_angle(uint32_t nside, uint64_t ipix, bool nested,
                              double& ra, double& dec);
    static double angular_distance(double ra1, double dec1,
                                   double ra2, double dec2);

private:
    std::string file_path_;
    std::string filter_;
    bool is_hiss_;
    uint32_t nside_;
    uint64_t n_pix_;
    int nested_;
    mutable std::mutex mutex_;

    // .hiss 模式下缓存的全部数据 (open_file 时加载)
    uint64_t* all_ipix_;
    float* all_pixel_;

    // .hiss 子叶索引 (open_file 时建立, 加速 load_leaf)
    // key: nside=64 子叶 ipix, value: (起始索引, 像素数) 在 all_ipix_/all_pixel_ 中的范围
    // 建立: 遍历 all_ipix_ 一次, 按 ipix>>shift 分组排序
    std::unordered_map<uint64_t, std::pair<uint64_t, uint64_t>> hiss_leaf_index_;
    int hiss_leaf_shift_ = 0;  // ipix >> hiss_leaf_shift_ = nside=64 子叶 ipix

    void build_hiss_leaf_index();  // 建立 .hiss 子叶索引
    void free_all_data();

    // 数据范围 (供 ud_grade 归一化, 由 set_data_range 设置)
    float data_min_ = 0.0f;
    float data_max_ = 1.0f;

    // WP-H 步骤14: 新 HISS Header (load_hiss 后可用, 与 open_file 独立)
    HissHeader hiss_header_;
    bool hiss_header_loaded_ = false;

    // FP64 精度模式标志 (从 HISS metadata signal_dtype / precision_mode 检测)
    // load_hiss / open_file 时设置, 供 read_tile_signal_f64 / query_pixel_f64 路径选择
    bool is_fp64_ = false;
};

#endif // BROWSER_BACKEND_H
