// browser_backend.cpp - HEALPix 浏览器数据后端实现 (healpix_browser_qt)
// 功能: 管理 .hiss/.hcsd 文件, 按需加载子叶, 视角相关压缩, ud_grade 降采样
// 用途: 为 GLRenderer 提供数据源, 无 Qt 依赖, 无 HTTP 服务器
// 依赖: healpix_io.dll (hiss_read/hcsd_read/hcsd_read_leaf/hio_free)
// 编译: C++17, 纯标准库 + healpix_io
// 移植来源: healpix_browser_cpp/src/browser_backend.cpp (去掉 HTTP, 用 HealpixMath)

#include "browser_backend.h"
#include "healpix_io.h"
#include "healpix_math.h"
#include "logger.h"

#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <map>
#include <cstdlib>
#include <string>

// ============================================================================
// 构造 / 析构
// ============================================================================

BrowserBackend::BrowserBackend()
    : is_hiss_(false), nside_(0), n_pix_(0), nested_(1),
      all_ipix_(nullptr), all_pixel_(nullptr) {}

BrowserBackend::~BrowserBackend() {
    close_file();
}

void BrowserBackend::free_all_data() {
    if (all_ipix_) { hio_free(all_ipix_); all_ipix_ = nullptr; }
    if (all_pixel_) { hio_free(all_pixel_); all_pixel_ = nullptr; }
}

// ============================================================================
// 打开文件
// ============================================================================

int BrowserBackend::open_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 先关闭已有文件
    free_all_data();
    file_path_.clear();
    filter_.clear();
    is_hiss_ = false;
    nside_ = 0;
    n_pix_ = 0;
    nested_ = 1;

    // 读取前 4 字节 Magic 判断格式
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        LOG_ERROR("无法打开文件: %s", path.c_str());
        return -1;
    }

    char magic[4] = {0};
    f.read(magic, 4);
    if (!f) {
        LOG_ERROR("读取 Magic 失败: %s", path.c_str());
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
            LOG_ERROR("hiss_read 失败: ret=%d path=%s", ret, path.c_str());
            file_path_.clear();
            return -3;
        }

        nside_ = nside;
        nested_ = nested;
        n_pix_ = n_pix;
        all_ipix_ = ipix;
        all_pixel_ = pixel;

        // 解析 filter (简单从 meta_json 提取)
        if (meta_json) {
            const char* key = "\"filter\"";
            const char* p = std::strstr(meta_json, key);
            if (p) {
                p += std::strlen(key);
                while (*p && (*p == ' ' || *p == ':' || *p == '"')) p++;
                std::string val;
                while (*p && *p != '"' && *p != ',' && *p != '}') {
                    val += *p++;
                }
                filter_ = val;
            }
            hio_free(meta_json);
        }

        LOG_INFO(".hiss 已加载: nside=%u nested=%d n_pix=%llu filter=%s",
                 nside_, nested_, (unsigned long long)n_pix_, filter_.c_str());
        return 0;
    } else if (std::memcmp(magic, "HCSD", 4) == 0) {
        // 球面模式 - 仅读取元信息, 不全量加载像素数据
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
            LOG_ERROR("hcsd_read 失败: ret=%d path=%s", ret, path.c_str());
            file_path_.clear();
            return -4;
        }

        nside_ = nside;
        nested_ = nested;
        n_pix_ = n_pix;

        // 解析 filter
        if (meta_json) {
            const char* key = "\"filter\"";
            const char* p = std::strstr(meta_json, key);
            if (p) {
                p += std::strlen(key);
                while (*p && (*p == ' ' || *p == ':' || *p == '"')) p++;
                std::string val;
                while (*p && *p != '"' && *p != ',' && *p != '}') {
                    val += *p++;
                }
                filter_ = val;
            }
            hio_free(meta_json);
        }

        // 释放全量像素数据, 球面模式按需加载
        if (ipix) hio_free(ipix);
        if (pixel) hio_free(pixel);
        all_ipix_ = nullptr;
        all_pixel_ = nullptr;

        LOG_INFO(".hcsd 已打开: nside=%u nested=%d n_pix=%llu filter=%s",
                 nside_, nested_, (unsigned long long)n_pix_, filter_.c_str());
        return 0;
    } else {
        LOG_ERROR("未知 Magic: %02x %02x %02x %02x",
                  (unsigned char)magic[0], (unsigned char)magic[1],
                  (unsigned char)magic[2], (unsigned char)magic[3]);
        file_path_.clear();
        return -5;
    }
}

void BrowserBackend::close_file() {
    std::lock_guard<std::mutex> lock(mutex_);
    free_all_data();
    file_path_.clear();
    filter_.clear();
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
const std::string& BrowserBackend::get_filter() const { return filter_; }

// ============================================================================
// HEALPix 角度计算 (转发到 HealpixMath, 支持任意 nside)
// ============================================================================

void BrowserBackend::ipix_to_angle(uint32_t nside, uint64_t ipix, bool nested,
                                    double& ra, double& dec) {
    if (nested) {
        HealpixMath::pix2ang_nest(nside, ipix, ra, dec);
    } else {
        // RING 排序暂不支持 (本项目数据均为 NESTED)
        ra = 0.0;
        dec = 0.0;
    }
}

double BrowserBackend::angular_distance(double ra1, double dec1,
                                        double ra2, double dec2) {
    return HealpixMath::angular_distance(ra1, dec1, ra2, dec2);
}

// ============================================================================
// 视角相关: 获取需要加载的子叶列表
// ============================================================================

std::vector<uint64_t> BrowserBackend::get_required_leaves(const ViewParams& view) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::pair<uint64_t, double>> candidates;  // (ipix, 角距离)

    const uint32_t nside_leaf = 64;
    const uint64_t n_leaf = 12ULL * nside_leaf * nside_leaf;  // 49152
    const double half_fov = view.fov_deg / 2.0;

    for (uint64_t ipix = 0; ipix < n_leaf; ipix++) {
        double ra, dec;
        HealpixMath::pix2ang_nest(nside_leaf, ipix, ra, dec);
        double dist = HealpixMath::angular_distance(
            view.center_ra, view.center_dec, ra, dec);

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

    LOG_DEBUG("get_required_leaves: 视场内候选 %zu 个, 返回 %zu 个",
              candidates.size(), result.size());
    return result;
}

// ============================================================================
// 视角相关: 决定目标 nside
// ============================================================================

uint32_t BrowserBackend::decide_target_nside(const ViewParams& view,
                                              uint64_t leaf_ipix) const {
    double ra, dec;
    HealpixMath::pix2ang_nest(64, leaf_ipix, ra, dec);
    double dist = HealpixMath::angular_distance(
        view.center_ra, view.center_dec, ra, dec);

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

        LOG_DEBUG("load_leaf(.hiss) leaf=%llu n_pix=%llu nside=%u",
                  (unsigned long long)leaf_ipix,
                  (unsigned long long)result.n_pix, result.nside);

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
            LOG_ERROR("hcsd_read_leaf 失败: ret=%d leaf=%llu",
                      ret, (unsigned long long)leaf_ipix);
            return result;
        }

        result.n_pix = n_pix;
        result.ipix = ipix;
        result.pixel = pixel;
        result.nside = nside_;

        LOG_DEBUG("load_leaf(.hcsd) leaf=%llu n_pix=%llu nside=%u",
                  (unsigned long long)leaf_ipix,
                  (unsigned long long)result.n_pix, result.nside);

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
// ud_grade 降采样 (4^k 个相邻像素合并求均值, NESTED 排序)
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

    LOG_DEBUG("ud_grade: %llu -> %llu (nside %u -> %u, shift=%d)",
              (unsigned long long)input.n_pix,
              (unsigned long long)result.n_pix,
              input.nside, target_nside, shift);
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

// ============================================================================
// 释放 LeafData 内存
// ============================================================================

void BrowserBackend::release_leaf(LeafData& leaf) {
    if (leaf.ipix) { std::free(leaf.ipix); leaf.ipix = nullptr; }
    if (leaf.pixel) { std::free(leaf.pixel); leaf.pixel = nullptr; }
    leaf.n_pix = 0;
    leaf.nside = 0;
    leaf.leaf_ipix = 0;
}
