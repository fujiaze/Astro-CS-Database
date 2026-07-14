// browser_backend.h - C++ 渲染后端 (healpix_browser_cpp)
// 功能: 管理 .hiss/.hcsd 文件句柄, 响应前端视角请求按需加载子叶,
//       视角相关压缩 (中心高分辨率, 边缘低分辨率), ud_grade 降采样
// 用途: 替代 PyQt5 浏览器, 配合 HTTP 服务器 + WebView2/系统浏览器渲染 HEALPix 球面数据
// 依赖: healpix_io.dll (hiss_read/hcsd_read/hcsd_read_leaf)
// 编译: C++17, 纯标准库 + winsock2 (HTTP 服务器)

#ifndef BROWSER_BACKEND_H
#define BROWSER_BACKEND_H

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

// ============================================================================
// 视角参数
// ============================================================================
struct ViewParams {
    double center_ra;    // 中心赤经 (度)
    double center_dec;   // 中心赤纬 (度)
    double zoom;         // 缩放级别 (1.0 = 全天, 越大越放大)
    double fov_deg;      // 视场大小 (度)
};

// ============================================================================
// 子叶数据
// ============================================================================
struct LeafData {
    uint64_t leaf_ipix;  // nside=64 子叶 ipix
    uint64_t n_pix;      // 像素数
    uint64_t* ipix;      // ipix 数组 (malloc 分配, 调用者负责 free)
    float* pixel;        // 像素值数组 (malloc 分配, 调用者负责 free)
    uint32_t nside;      // 实际 nside (可能被降采样)

    // 默认构造: 空数据
    LeafData() : leaf_ipix(0), n_pix(0), ipix(nullptr), pixel(nullptr), nside(0) {}
};

// ============================================================================
// 浏览器后端类
// ============================================================================
class BrowserBackend {
public:
    BrowserBackend();
    ~BrowserBackend();

    // 打开文件
    // path: .hiss 或 .hcsd 文件路径 (UTF-8)
    // 返回 0=成功, <0=失败
    int open_file(const std::string& path);

    // 关闭文件, 释放缓存
    void close_file();

    // 获取文件信息
    bool is_open() const;
    bool is_hiss() const;  // 单帧模式
    bool is_hcsd() const;  // 球面模式
    uint32_t get_nside() const;
    uint64_t get_n_pix() const;
    const std::string& get_file_path() const;

    // 根据视角参数获取需要加载的子叶列表
    // 返回需要加载的子叶 ipix 列表 (nside=64 层), 限制最大 100 个
    std::vector<uint64_t> get_required_leaves(const ViewParams& view) const;

    // 加载指定子叶的数据 (按需加载)
    // leaf_ipix: nside=64 子叶 ipix
    // target_nside: 目标 nside (可能被降采样以减少数据量, 0 表示不降采样)
    LeafData load_leaf(uint64_t leaf_ipix, uint32_t target_nside);

    // 根据视角决定目标 nside (视角相关压缩)
    // 中心区域 (距离 < fov_deg/4): nside=8192 (全分辨率)
    // 中间区域 (距离 < fov_deg/2): nside=2048 (降采样)
    // 边缘区域:                    nside=256  (高强度压缩)
    uint32_t decide_target_nside(const ViewParams& view, uint64_t leaf_ipix) const;

    // ud_grade 降采样 (4 相邻像素合并求均值, NESTED 排序)
    // 输入 nside=N 的像素数据, 降采样到 target_nside (= N / 2^k)
    LeafData ud_grade(const LeafData& input, uint32_t target_nside);

    // 获取全量数据 (仅 .hiss 模式)
    // 返回的 LeafData 由本对象持有, close_file() 时释放
    LeafData get_all_data();

private:
    std::string file_path_;
    bool is_hiss_;
    uint32_t nside_;
    uint64_t n_pix_;
    int nested_;
    mutable std::mutex mutex_;

    // .hiss 模式下缓存的全部数据 (open_file 时加载)
    uint64_t* all_ipix_;
    float* all_pixel_;

    // HEALPix 角度计算辅助 (NESTED 排序, nside=64)
    void ipix_to_angle(uint64_t ipix, double& ra, double& dec) const;
    double angular_distance(double ra1, double dec1, double ra2, double dec2) const;
};

#endif // BROWSER_BACKEND_H
