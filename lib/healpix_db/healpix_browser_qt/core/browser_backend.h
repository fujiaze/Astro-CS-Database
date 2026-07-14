// browser_backend.h - HEALPix 浏览器数据后端 (healpix_browser_qt)
// 功能: 管理 .hiss/.hcsd 文件, 按需加载子叶, 视角相关压缩, ud_grade 降采样
// 用途: 为 GLRenderer 提供数据源, 无 Qt 依赖, 无 HTTP 服务器
// 依赖: healpix_io.dll (hiss_read/hcsd_read/hcsd_read_leaf/hio_free)
// 编译: C++17, 纯标准库 + healpix_io
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md §3.1

#ifndef BROWSER_BACKEND_H
#define BROWSER_BACKEND_H

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

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
    uint64_t* ipix;      // ipix 数组 (malloc 分配)
    float* pixel;        // 像素值数组 (malloc 分配)
    uint32_t nside;      // 实际 nside (可能被降采样)

    LeafData() : leaf_ipix(0), n_pix(0), ipix(nullptr), pixel(nullptr), nside(0) {}
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

    // 根据视角决定目标 nside (视角相关压缩)
    // 中心区域 (距离 < fov_deg/4): nside=8192 (全分辨率)
    // 中间区域 (距离 < fov_deg/2): nside=2048 (降采样)
    // 边缘区域:                    nside=256  (高强度压缩)
    uint32_t decide_target_nside(const ViewParams& view, uint64_t leaf_ipix) const;

    // ---- 降采样 ----
    // NESTED 排序位运算: ipix_coarse = ipix_fine >> (2 * log2(ratio))
    // 4^k 个相邻像素求均值合并
    LeafData ud_grade(const LeafData& input, uint32_t target_nside);

    // ---- 全量数据 (仅 .hiss 模式, 单帧切面投影用) ----
    // 返回的 LeafData 由本对象持有, close_file() 时释放, 调用者不应释放
    LeafData get_all_data();

    // 释放 LeafData 内存 (malloc 分配的数据用此释放; get_all_data 返回的不要用此释放)
    void release_leaf(LeafData& leaf);

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

    void free_all_data();
};

#endif // BROWSER_BACKEND_H
