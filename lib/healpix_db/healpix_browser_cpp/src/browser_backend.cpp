// browser_backend.cpp - C++ 渲染后端实现 (healpix_browser_cpp)
// 功能: 实现 BrowserBackend 类, 管理 .hiss/.hcsd 文件, 按需加载子叶,
//       视角相关压缩 (中心高分辨率/边缘低分辨率), ud_grade 降采样
// 用途: 配合 HTTP 服务器为前端提供 HEALPix 数据 API
// 依赖: healpix_io.dll (hiss_read/hcsd_read/hcsd_read_leaf/hio_free)
// 编译: C++17, 纯标准库 + healpix_io + winsock2 (HTTP 服务器)

#include "browser_backend.h"
#include "healpix_io.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <map>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 构造 / 析构
// ============================================================================

BrowserBackend::BrowserBackend()
    : is_hiss_(false), nside_(0), n_pix_(0), nested_(1),
      all_ipix_(nullptr), all_pixel_(nullptr) {}

BrowserBackend::~BrowserBackend() {
    close_file();
}

// ============================================================================
// 打开文件
// ============================================================================

int BrowserBackend::open_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 先关闭已有文件
    if (all_ipix_ || all_pixel_) {
        if (all_ipix_) { hio_free(all_ipix_); all_ipix_ = nullptr; }
        if (all_pixel_) { hio_free(all_pixel_); all_pixel_ = nullptr; }
    }
    file_path_.clear();
    is_hiss_ = false;
    nside_ = 0;
    n_pix_ = 0;
    nested_ = 1;

    // 读取前 4 字节 Magic
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[BrowserBackend] 无法打开文件: " << path << std::endl;
        return -1;
    }

    char magic[4] = {0};
    f.read(magic, 4);
    if (!f) {
        std::cerr << "[BrowserBackend] 读取 Magic 失败" << std::endl;
        return -2;
    }
    f.close();

    file_path_ = path;

    if (std::memcmp(magic, "HISS", 4) == 0) {
        // 单帧模式 - 全量加载到内存
        is_hiss_ = true;
        uint32_t nside = 0;
        int nested = 0;
        uint64_t n_pix = 0;
        uint64_t* ipix = nullptr;
        float* pixel = nullptr;
        char* meta_json = nullptr;

        int ret = hiss_read(path.c_str(), &nside, &nested, &n_pix,
                            &ipix, &pixel, &meta_json);
        if (ret != 0) {
            std::cerr << "[BrowserBackend] hiss_read 失败: " << ret << std::endl;
            file_path_.clear();
            return -3;
        }

        nside_ = nside;
        nested_ = nested;
        n_pix_ = n_pix;
        all_ipix_ = ipix;
        all_pixel_ = pixel;

        if (meta_json) hio_free(meta_json);

        std::cout << "[BrowserBackend] .hiss 已加载: nside=" << nside_
                  << " nested=" << nested_
                  << " n_pix=" << n_pix_ << std::endl;
        return 0;
    } else if (std::memcmp(magic, "HCSD", 4) == 0) {
        // 球面模式 - 仅读取 JSON 头获取元信息, 不全量加载像素数据
        // 注: healpix_io 未提供单独读头的 API, 用 hcsd_read 一次读取后立即释放像素数据
        //     (索引表已 O(1) 定位, 按需 hcsd_read_leaf 加载子叶)
        is_hiss_ = false;
        uint32_t nside = 0;
        int nested = 0;
        uint64_t n_pix = 0;
        uint64_t* ipix = nullptr;
        float* pixel = nullptr;
        char* meta_json = nullptr;

        int ret = hcsd_read(path.c_str(), &nside, &nested, &n_pix,
                            &ipix, &pixel, &meta_json);
        if (ret != 0) {
            std::cerr << "[BrowserBackend] hcsd_read 失败: " << ret << std::endl;
            file_path_.clear();
            return -4;
        }

        nside_ = nside;
        nested_ = nested;
        n_pix_ = n_pix;

        // 释放全量像素数据, 球面模式按需加载
        if (ipix) hio_free(ipix);
        if (pixel) hio_free(pixel);
        if (meta_json) hio_free(meta_json);
        all_ipix_ = nullptr;
        all_pixel_ = nullptr;

        std::cout << "[BrowserBackend] .hcsd 已打开: nside=" << nside_
                  << " nested=" << nested_
                  << " n_pix=" << n_pix_ << std::endl;
        return 0;
    } else {
        std::cerr << "[BrowserBackend] 未知 Magic: "
                  << std::hex << (int)(unsigned char)magic[0] << " "
                  << (int)(unsigned char)magic[1] << " "
                  << (int)(unsigned char)magic[2] << " "
                  << (int)(unsigned char)magic[3] << std::dec << std::endl;
        file_path_.clear();
        return -5;
    }
}

void BrowserBackend::close_file() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (all_ipix_) { hio_free(all_ipix_); all_ipix_ = nullptr; }
    if (all_pixel_) { hio_free(all_pixel_); all_pixel_ = nullptr; }
    file_path_.clear();
    is_hiss_ = false;
    nside_ = 0;
    n_pix_ = 0;
    nested_ = 1;
}

// ============================================================================
// 文件信息查询
// ============================================================================

bool BrowserBackend::is_open() const { return !file_path_.empty(); }
bool BrowserBackend::is_hiss() const { return is_hiss_; }
bool BrowserBackend::is_hcsd() const { return !is_hiss_ && !file_path_.empty(); }
uint32_t BrowserBackend::get_nside() const { return nside_; }
uint64_t BrowserBackend::get_n_pix() const { return n_pix_; }
const std::string& BrowserBackend::get_file_path() const { return file_path_; }

// ============================================================================
// HEALPix 坐标转换 (nside=64, NESTED)
// ============================================================================

void BrowserBackend::ipix_to_angle(uint64_t ipix, double& ra, double& dec) const {
    // 简化版 HEALPix NESTED pix2ang (nside=64)
    // 参考 HEALPix 标准算法 ( Healpix_base::pix2ang )
    // 注: 仅用于 nside=64 子叶中心坐标计算, 不需要超高精度
    const int nside = 64;
    const int npface = nside * nside;  // 4096

    int face_num = (int)(ipix / (uint64_t)npface);
    uint64_t ip_low = ipix % (uint64_t)npface;

    // NESTED 位交错解码: 从 ip_low 提取 (ix, iy)
    int ix = 0, iy = 0;
    for (int i = 0; i < 6; i++) {  // log2(64) = 6
        ix |= (int)(((ip_low >> (2 * i)) & 1ULL) << i);
        iy |= (int)(((ip_low >> (2 * i + 1)) & 1ULL) << i);
    }

    // HEALPix 面索引到 (jr, jp) 的查找表
    static const int jrll[12] = {2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
    static const int jpll[12] = {1, 3, 5, 7, 0, 2, 4, 6, 1, 3, 5, 7};

    int jr = jrll[face_num] * nside - ix - 1;
    int jp = jpll[face_num] * nside + ix - iy;

    double z, phi;
    if (jr < nside) {
        // 北极帽区
        int tmp = nside - jr;
        z = 1.0 - (double)(tmp * tmp) / (3.0 * (double)npface);
        if (tmp == 0) {
            phi = 0.0;
        } else {
            phi = (M_PI / 2.0) * (double)jp / (4.0 * (double)tmp);
        }
    } else if (jr > 3 * nside) {
        // 南极帽区
        int tmp = jr - 3 * nside;
        z = -1.0 + (double)(tmp * tmp) / (3.0 * (double)npface);
        if (tmp == 0) {
            phi = 0.0;
        } else {
            phi = (M_PI / 2.0) * (double)jp / (4.0 * (double)tmp);
        }
    } else {
        // 赤道带
        z = (2.0 * (double)nside - (double)jr) / (3.0 * (double)nside);
        phi = (M_PI / 4.0) * (double)jp / (double)nside;
    }

    // 归一化 phi 到 [0, 2π)
    while (phi < 0) phi += 2.0 * M_PI;
    while (phi >= 2.0 * M_PI) phi -= 2.0 * M_PI;

    // z -> dec, phi -> ra
    if (z > 1.0) z = 1.0;
    if (z < -1.0) z = -1.0;
    dec = std::asin(z) * 180.0 / M_PI;
    ra = phi * 180.0 / M_PI;
}

double BrowserBackend::angular_distance(double ra1, double dec1,
                                        double ra2, double dec2) const {
    // 球面余弦定理
    const double DEG2RAD = M_PI / 180.0;
    double dec1_r = dec1 * DEG2RAD;
    double dec2_r = dec2 * DEG2RAD;
    double d_ra = (ra2 - ra1) * DEG2RAD;

    double cos_d = std::sin(dec1_r) * std::sin(dec2_r) +
                   std::cos(dec1_r) * std::cos(dec2_r) * std::cos(d_ra);

    if (cos_d > 1.0) cos_d = 1.0;
    if (cos_d < -1.0) cos_d = -1.0;

    return std::acos(cos_d) * 180.0 / M_PI;
}

// ============================================================================
// 视角相关: 获取需要加载的子叶列表
// ============================================================================

std::vector<uint64_t> BrowserBackend::get_required_leaves(const ViewParams& view) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::pair<uint64_t, double>> candidates;  // (ipix, 角距离)

    const int nside_leaf = 64;
    const uint64_t n_leaf = 12ULL * nside_leaf * nside_leaf;  // 49152
    const double half_fov = view.fov_deg / 2.0;

    for (uint64_t ipix = 0; ipix < n_leaf; ipix++) {
        double ra, dec;
        ipix_to_angle(ipix, ra, dec);
        double dist = angular_distance(view.center_ra, view.center_dec, ra, dec);

        if (dist < half_fov) {
            candidates.push_back({ipix, dist});
        }
    }

    // 按距离升序排序 (中心优先)
    std::sort(candidates.begin(), candidates.end(),
              [](const std::pair<uint64_t, double>& a,
                 const std::pair<uint64_t, double>& b) {
                  return a.second < b.second;
              });

    // 限制最大返回 100 个
    std::vector<uint64_t> result;
    size_t max_leaves = std::min((size_t)100, candidates.size());
    result.reserve(max_leaves);
    for (size_t i = 0; i < max_leaves; i++) {
        result.push_back(candidates[i].first);
    }

    std::cout << "[BrowserBackend] get_required_leaves: 视场内候选 "
              << candidates.size() << " 个, 返回 " << result.size() << " 个"
              << std::endl;
    return result;
}

// ============================================================================
// 视角相关: 决定目标 nside
// ============================================================================

uint32_t BrowserBackend::decide_target_nside(const ViewParams& view,
                                              uint64_t leaf_ipix) const {
    double ra, dec;
    ipix_to_angle(leaf_ipix, ra, dec);
    double dist = angular_distance(view.center_ra, view.center_dec, ra, dec);

    const double quarter_fov = view.fov_deg / 4.0;
    const double half_fov = view.fov_deg / 2.0;

    if (dist < quarter_fov) {
        return 8192;  // 中心区域 - 全分辨率
    } else if (dist < half_fov) {
        return 2048;  // 中间区域 - 中等降采样
    } else {
        return 256;   // 边缘区域 - 高强度压缩
    }
}

// ============================================================================
// 加载子叶数据
// ============================================================================

LeafData BrowserBackend::load_leaf(uint64_t leaf_ipix, uint32_t target_nside) {
    std::lock_guard<std::mutex> lock(mutex_);

    LeafData result;
    result.leaf_ipix = leaf_ipix;

    if (is_hiss_) {
        // 单帧模式 - 从内存缓存中筛选属于该子叶的像素
        // 子叶 ipix_at_nside64 = ipix_fine >> (2 * log2(nside_/64))
        int shift = 0;
        uint32_t n = nside_;
        while (n > 64) { shift += 2; n >>= 1; }

        std::vector<uint64_t> sel_ipix;
        std::vector<float> sel_pixel;
        for (uint64_t i = 0; i < n_pix_; i++) {
            if (shift == 0) {
                // nside == 64, 直接比较
                if (all_ipix_[i] == leaf_ipix) {
                    sel_ipix.push_back(all_ipix_[i]);
                    sel_pixel.push_back(all_pixel_[i]);
                }
            } else {
                if ((all_ipix_[i] >> shift) == leaf_ipix) {
                    sel_ipix.push_back(all_ipix_[i]);
                    sel_pixel.push_back(all_pixel_[i]);
                }
            }
        }

        result.n_pix = sel_ipix.size();
        result.nside = nside_;
        if (result.n_pix > 0) {
            result.ipix = (uint64_t*)std::malloc(result.n_pix * sizeof(uint64_t));
            result.pixel = (float*)std::malloc(result.n_pix * sizeof(float));
            if (result.ipix && result.pixel) {
                std::memcpy(result.ipix, sel_ipix.data(), result.n_pix * sizeof(uint64_t));
                std::memcpy(result.pixel, sel_pixel.data(), result.n_pix * sizeof(float));
            } else {
                std::free(result.ipix); result.ipix = nullptr;
                std::free(result.pixel); result.pixel = nullptr;
                result.n_pix = 0;
            }
        }

        std::cout << "[BrowserBackend] load_leaf(.hiss) leaf=" << leaf_ipix
                  << " n_pix=" << result.n_pix
                  << " nside=" << result.nside << std::endl;

        // 降采样
        if (target_nside < nside_ && target_nside > 0 && result.n_pix > 0) {
            LeafData downsampled = ud_grade(result, target_nside);
            std::free(result.ipix);
            std::free(result.pixel);
            return downsampled;
        }
        return result;
    } else {
        // 球面模式 - 调用 hcsd_read_leaf 按需读取
        uint64_t n_pix = 0;
        uint64_t* ipix = nullptr;
        float* pixel = nullptr;

        int ret = hcsd_read_leaf(file_path_.c_str(), leaf_ipix, &n_pix, &ipix, &pixel);
        if (ret != 0) {
            std::cerr << "[BrowserBackend] hcsd_read_leaf 失败: " << ret
                      << " leaf=" << leaf_ipix << std::endl;
            return result;
        }

        result.n_pix = n_pix;
        result.ipix = ipix;
        result.pixel = pixel;
        result.nside = nside_;

        std::cout << "[BrowserBackend] load_leaf(.hcsd) leaf=" << leaf_ipix
                  << " n_pix=" << result.n_pix
                  << " nside=" << result.nside << std::endl;

        // 降采样 (用 hio_free 释放 healpix_io 分配的原始数据)
        if (target_nside < nside_ && target_nside > 0 && result.n_pix > 0) {
            LeafData downsampled = ud_grade(result, target_nside);
            hio_free(result.ipix);
            hio_free(result.pixel);
            // 注意: 此时 result.ipix/pixel 已释放, 不可再用
            return downsampled;
        }
        return result;
    }
}

// ============================================================================
// ud_grade 降采样 (4 相邻像素合并求均值, NESTED 排序)
// ============================================================================

LeafData BrowserBackend::ud_grade(const LeafData& input, uint32_t target_nside) {
    LeafData result;
    result.leaf_ipix = input.leaf_ipix;
    result.nside = target_nside;

    if (input.n_pix == 0 || target_nside >= input.nside || input.nside == 0) {
        return result;
    }

    // 计算降采样位移: 每个 2 倍 nside 对应 ipix 右移 2 位
    // shift = 2 * log2(ratio), 例如 ratio=4 -> shift=4 (16 像素合并)
    uint32_t ratio = input.nside / target_nside;
    int shift = 0;
    uint32_t r = ratio;
    while (r > 1) { shift += 2; r >>= 1; }

    // 按 ipix_coarse 分组 (NESTED 排序下 ipix_coarse = ipix_fine >> shift)
    std::map<uint64_t, std::pair<double, uint32_t>> groups;  // ipix_coarse -> (sum, count)
    for (uint64_t i = 0; i < input.n_pix; i++) {
        uint64_t ipix_coarse = (shift > 0) ? (input.ipix[i] >> shift) : input.ipix[i];
        auto& g = groups[ipix_coarse];
        g.first += (double)input.pixel[i];
        g.second += 1;
    }

    result.n_pix = groups.size();
    if (result.n_pix == 0) return result;

    result.ipix = (uint64_t*)std::malloc(result.n_pix * sizeof(uint64_t));
    result.pixel = (float*)std::malloc(result.n_pix * sizeof(float));
    if (!result.ipix || !result.pixel) {
        std::free(result.ipix); result.ipix = nullptr;
        std::free(result.pixel); result.pixel = nullptr;
        result.n_pix = 0;
        return result;
    }

    uint64_t idx = 0;
    for (const auto& kv : groups) {
        result.ipix[idx] = kv.first;
        result.pixel[idx] = (float)(kv.second.first / (double)kv.second.second);
        idx++;
    }

    std::cout << "[BrowserBackend] ud_grade: " << input.n_pix << " -> "
              << result.n_pix << " (nside " << input.nside << " -> "
              << target_nside << ", shift=" << shift << ")" << std::endl;
    return result;
}

// ============================================================================
// 获取全量数据 (仅 .hiss 模式)
// ============================================================================

LeafData BrowserBackend::get_all_data() {
    std::lock_guard<std::mutex> lock(mutex_);

    LeafData result;
    if (!is_hiss_ || !all_ipix_ || !all_pixel_) {
        return result;
    }

    result.leaf_ipix = 0;
    result.n_pix = n_pix_;
    result.nside = nside_;
    // 注意: 返回的 ipix/pixel 由本对象持有, close_file() 时释放
    // 调用者不应释放这些指针
    result.ipix = all_ipix_;
    result.pixel = all_pixel_;

    return result;
}
