// browser_backend.cpp - HEALPix 浏览器数据后端实现 (healpix_browser_qt)
// 功能: 管理 .hiss/.hcsd 文件, 按需加载子叶, 视角相关压缩, ud_grade 降采样
// 用途: 为 GLRenderer 提供数据源, 无 Qt 依赖, 无 HTTP 服务器
// 依赖: astro_image_io.dll (aio_hiss_read/aio_hcsd_read/aio_hcsd_read_leaf/aio_hio_free, 旧 API 通过兼容宏)
// 编译: C++17, 纯标准库 + astro_image_io
// 移植来源: healpix_browser_cpp/src/browser_backend.cpp (去掉 HTTP, 用 HealpixMath)

#include "browser_backend.h"
#include "aio_healpix_io.h"
#include "healpix_math.h"
#include "logger.h"

#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <map>
#include <unordered_map>
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
    hiss_leaf_index_.clear();
    hiss_leaf_shift_ = 0;
}

// ============================================================================
// 建立 .hiss 子叶索引
// 将 all_ipix_/all_pixel_ 按 nside=64 子叶分组, 记录每组在数组中的 (起始, 数量)
// 建立: 遍历一次, 按 leaf_ipix 分组到 std::map (有序), 然后重排数据
// 之后 load_leaf 用索引 O(1) 定位, 不再遍历全部像素
// ============================================================================

void BrowserBackend::build_hiss_leaf_index() {
    hiss_leaf_index_.clear();

    if (!all_ipix_ || n_pix_ == 0 || nside_ < 64) {
        hiss_leaf_shift_ = 0;
        return;
    }

    // 计算 shift: ipix_fine >> shift = ipix_coarse (nside=64)
    hiss_leaf_shift_ = 0;
    uint32_t n = nside_;
    while (n > 64) { hiss_leaf_shift_ += 2; n >>= 1; }

    LOG_INFO("build_hiss_leaf_index: 开始建立子叶索引 (n_pix=%llu, shift=%d)",
             (unsigned long long)n_pix_, hiss_leaf_shift_);

    // 方案: 创建 (leaf_ipix, original_index) 数组, 按 leaf_ipix 排序
    // 排序后同一子叶的像素连续, 记录 (start, count) 索引
    // 比 std::map<vector> 快 10 倍以上 (避免 61.6M 次 vector push_back)
    struct LeafIndexEntry {
        uint64_t leaf_ipix;
        uint64_t orig_idx;
    };

    std::vector<LeafIndexEntry> entries(n_pix_);
    for (uint64_t i = 0; i < n_pix_; i++) {
        entries[i].leaf_ipix = all_ipix_[i] >> hiss_leaf_shift_;
        entries[i].orig_idx = i;
    }

    // 按 leaf_ipix 排序 (稳定排序, 同一子叶内保持原始顺序)
    std::stable_sort(entries.begin(), entries.end(),
                     [](const LeafIndexEntry& a, const LeafIndexEntry& b) {
                         return a.leaf_ipix < b.leaf_ipix;
                     });

    // 分配新数组, 按排序顺序重排数据 (同一子叶连续存储)
    uint64_t* new_ipix = (uint64_t*)std::malloc(n_pix_ * sizeof(uint64_t));
    float* new_pixel = (float*)std::malloc(n_pix_ * sizeof(float));
    if (!new_ipix || !new_pixel) {
        LOG_ERROR("build_hiss_leaf_index: 内存分配失败 n_pix=%llu",
                  (unsigned long long)n_pix_);
        std::free(new_ipix);
        std::free(new_pixel);
        hiss_leaf_shift_ = 0;
        return;
    }

    for (uint64_t i = 0; i < n_pix_; i++) {
        uint64_t idx = entries[i].orig_idx;
        new_ipix[i] = all_ipix_[idx];
        new_pixel[i] = all_pixel_[idx];
    }

    // 建立 (leaf_ipix → start, count) 索引, 并对每个子叶内部按 ipix 排序
    // (排序后 render_sphere 可用二分查找 O(log n) 替代 unordered_map)
    uint64_t pos = 0;
    while (pos < n_pix_) {
        uint64_t leaf_ipix = entries[pos].leaf_ipix;
        uint64_t start = pos;
        while (pos < n_pix_ && entries[pos].leaf_ipix == leaf_ipix) {
            pos++;
        }
        uint64_t count = pos - start;
        hiss_leaf_index_[leaf_ipix] = {start, count};

        // 对该子叶内部按 ipix 排序 (同时重排 pixel, 保持 ipix-pixel 对应)
        // 用 index sort 避免交换大块数据
        std::vector<uint64_t> sort_idx(count);
        for (uint64_t i = 0; i < count; i++) sort_idx[i] = i;
        std::sort(sort_idx.begin(), sort_idx.end(),
                  [&](uint64_t a, uint64_t b) {
                      return new_ipix[start + a] < new_ipix[start + b];
                  });
        // 应用排序到临时数组再拷回
        std::vector<uint64_t> tmp_ipix(count);
        std::vector<float> tmp_pixel(count);
        for (uint64_t i = 0; i < count; i++) {
            tmp_ipix[i] = new_ipix[start + sort_idx[i]];
            tmp_pixel[i] = new_pixel[start + sort_idx[i]];
        }
        std::memcpy(new_ipix + start, tmp_ipix.data(), count * sizeof(uint64_t));
        std::memcpy(new_pixel + start, tmp_pixel.data(), count * sizeof(float));
    }

    // 替换原数组
    hio_free(all_ipix_);
    hio_free(all_pixel_);
    all_ipix_ = new_ipix;
    all_pixel_ = new_pixel;

    LOG_INFO("build_hiss_leaf_index: 完成 nside=%u shift=%d 子叶数=%zu (覆盖 %llu 像素)",
             nside_, hiss_leaf_shift_, hiss_leaf_index_.size(),
             (unsigned long long)n_pix_);
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

    if (std::memcmp(magic, "HISS", 4) == 0 ||
        std::memcmp(magic, "ACSH", 4) == 0) {
        // 单帧模式 - 全量加载到内存
        // WP-H: "ACSH" 是新 HISS 格式 (8字节 MAGIC "ACSHISS\0") 的前4字节
        //        "HISS" 是旧格式, 两者均由 aio_hiss_read (HissReader) 统一处理
        is_hiss_ = true;
        uint32_t nside = 0;
        int nested = 0;
        uint64_t n_pix = 0;
        uint64_t* ipix = nullptr;
        float* pixel = nullptr;
        char* meta_json = nullptr;

        // hiss_read 8 参数: path, nside, nested, n_pix, ipix, pixel, snr, meta_json
        // browser 不使用 SNR 通道, snr 传 nullptr (snr_format=0/1 均兼容, 不读取 snr 数据)
        int ret = hiss_read(path.c_str(), &nside, &nested, &n_pix,
                            &ipix, &pixel, nullptr, &meta_json);
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

        // 建立子叶索引 (加速 load_leaf, 避免每次遍历全部像素)
        build_hiss_leaf_index();

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
    // 预加载范围: FOV × 1.5 + 1.0° 余量 (渲染用 FOV × 1.2, 预加载区供缩放流畅)
    // 加 1.0° 余量确保: 子叶中心在范围外但边缘在视场内的子叶也被加载
    // nside=64 子叶约 0.92°, 半径约 0.46°, 1.0° 余量足够覆盖
    const double half_fov = view.fov_deg * 0.75 + 1.0;

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

    // 不限制子叶数量, 让 decide_target_nside 的 LOD 控制每个子叶分辨率
    // 边缘子叶用低 nside (ud_grade 后像素少), 中心用全分辨率
    std::vector<uint64_t> result;
    result.reserve(candidates.size());
    for (const auto& c : candidates) {
        result.push_back(c.first);
    }

    LOG_DEBUG("get_required_leaves: 视场内候选 %zu 个, 返回 %zu 个",
              candidates.size(), result.size());
    return result;
}

// ============================================================================
// 视角相关: 决定目标 nside (LOD 自动阈值)
// 按屏幕分辨率停止下钻: HEALPix 像素角分辨率 ≥ 屏幕像素角分辨率时停止
// 全视场统一此阈值, 不再按中心/中间/边缘分三档
// ============================================================================

uint32_t BrowserBackend::decide_target_nside(const ViewParams& view,
                                              uint64_t leaf_ipix,
                                              int viewport_w, int viewport_h) const {
    // 屏幕像素角分辨率 (度/像素)
    double fov_deg = view.fov_deg;
    if (fov_deg < 0.01) fov_deg = 60.0;
    if (fov_deg > 170.0) fov_deg = 170.0;

    int vp = std::min(viewport_w, viewport_h);
    if (vp <= 0) vp = 768;
    double theta_screen = fov_deg / (double)vp;  // 度/像素

    // HEALPix 像素角分辨率: θ_hp = 360 / sqrt(12 * nside²) 度
    // 求 θ_hp = theta_screen 时的 nside:
    //   nside = 360 / (theta_screen * sqrt(12)) = 58.6 / theta_screen (近似)
    // 略大于屏幕: 用 theta_screen * 1.0 (像素 ≈ 屏幕, 不超采样)
    double nside_ideal = 58.6 / theta_screen;

    // 向上取整到 2 的幂 (确保 HEALPix 像素 ≤ 屏幕像素, 避免欠采样模糊)
    // 向下取整会导致多个顶点映射到同一 HEALPix 像素, 产生块状模糊
    uint32_t nside_target = 64;
    while (nside_target < (uint32_t)std::ceil(nside_ideal) && nside_target < nside_) {
        nside_target <<= 1;
    }

    // clamp 到 [64, nside_]
    if (nside_target < 64) nside_target = 64;
    if (nside_target > nside_) nside_target = nside_;

    LOG_DEBUG("decide_target_nside: fov=%.2f vp=%d theta_screen=%.4f°/px "
              "nside_ideal=%.1f -> nside_target=%u (原始 %u)",
              fov_deg, vp, theta_screen, nside_ideal, nside_target, nside_);
    return nside_target;
}

// ============================================================================
// 加载子叶数据
// ============================================================================

LeafData BrowserBackend::load_leaf(uint64_t leaf_ipix, uint32_t target_nside) {
    std::lock_guard<std::mutex> lock(mutex_);

    LeafData result;
    result.leaf_ipix = leaf_ipix;

    if (is_hiss_) {
        // 单帧模式 - 用子叶索引快速查找 (避免遍历全部像素)
        // 子叶索引在 open_file 时建立, 按 leaf_ipix 分组连续存储
        result.n_pix = 0;
        result.nside = nside_;
        result.owned = false;

        auto it = hiss_leaf_index_.find(leaf_ipix);
        if (it != hiss_leaf_index_.end()) {
            uint64_t start = it->second.first;
            uint64_t count = it->second.second;
            result.n_pix = count;
            if (count > 0) {
                // 零拷贝: 直接指向 all_ipix_/all_pixel_ 的切片 (owned=false)
                result.ipix = all_ipix_ + start;
                result.pixel = all_pixel_ + start;
                result.owned = false;
            }
        }

        LOG_DEBUG("load_leaf(.hiss) leaf=%llu n_pix=%llu nside=%u owned=%d",
                  (unsigned long long)leaf_ipix,
                  (unsigned long long)result.n_pix, result.nside, result.owned);

        // 降采样 (需要拷贝+聚合, 返回 owned=true 的独立内存, uint8)
        if (target_nside < nside_ && target_nside > 0 && result.n_pix > 0) {
            LeafData downsampled = ud_grade(result, target_nside, data_min_, data_max_);
            // ud_grade 返回 owned=true 的 malloc 内存, use_u8=true
            return downsampled;
        }
        // 无降采样: 返回零拷贝切片 (release_leaf 不会 free)
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
            LeafData downsampled = ud_grade(result, target_nside, data_min_, data_max_);
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

LeafData BrowserBackend::ud_grade(const LeafData& input, uint32_t target_nside,
                                   float data_min, float data_max) {
    LeafData result;
    result.leaf_ipix = input.leaf_ipix;
    result.nside = target_nside;
    result.owned = true;  // malloc 分配, 需 release_leaf 释放
    result.use_u8 = true; // ud_grade 输出 uint8

    if (input.n_pix == 0 || target_nside >= input.nside || input.nside == 0) {
        return result;
    }

    // 计算降采样位移: 每个 2 倍 nside 对应 ipix 右移 2 位
    uint32_t ratio = input.nside / target_nside;
    int shift = 0;
    uint32_t r = ratio;
    while (r > 1) { shift += 2; r >>= 1; }

    // 归一化范围 (用于 float→uint8 转换)
    double range = (double)data_max - (double)data_min;
    if (range < 1e-6) range = 1.0;
    double inv_range = 255.0 / range;

    // 按 ipix_coarse 分组, 用 uint32 累加 (速度优先, 显示用)
    // 注: 若数据范围大, sum 可能超 uint32, 用 double 累加安全
    std::unordered_map<uint64_t, std::pair<double, uint32_t>> groups;
    groups.reserve(input.n_pix >> shift + 1);
    for (uint64_t i = 0; i < input.n_pix; i++) {
        uint64_t ipix_coarse = (shift > 0) ? (input.ipix[i] >> shift) : input.ipix[i];
        auto& g = groups[ipix_coarse];
        g.first += (double)input.pixel[i];
        g.second += 1;
    }

    result.n_pix = groups.size();
    if (result.n_pix == 0) return result;

    result.ipix = (uint64_t*)std::malloc(result.n_pix * sizeof(uint64_t));
    result.pixel_u8 = (uint8_t*)std::malloc(result.n_pix * sizeof(uint8_t));
    if (!result.ipix || !result.pixel_u8) {
        std::free(result.ipix); result.ipix = nullptr;
        std::free(result.pixel_u8); result.pixel_u8 = nullptr;
        result.n_pix = 0;
        return result;
    }

    // 填入数组 + float→uint8 归一化
    uint64_t idx = 0;
    for (const auto& kv : groups) {
        result.ipix[idx] = kv.first;
        double mean = kv.second.first / (double)kv.second.second;
        // 归一化到 [0, 255]
        double normalized = (mean - (double)data_min) * inv_range;
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 255.0) normalized = 255.0;
        result.pixel_u8[idx] = (uint8_t)(normalized + 0.5);
        idx++;
    }

    // 按 ipix 排序 (index sort, 供二分查找)
    std::vector<uint64_t> sort_idx(result.n_pix);
    for (uint64_t i = 0; i < result.n_pix; i++) sort_idx[i] = i;
    std::sort(sort_idx.begin(), sort_idx.end(),
              [&](uint64_t a, uint64_t b) { return result.ipix[a] < result.ipix[b]; });
    std::vector<uint64_t> tmp_i(result.n_pix);
    std::vector<uint8_t> tmp_p(result.n_pix);
    for (uint64_t i = 0; i < result.n_pix; i++) {
        tmp_i[i] = result.ipix[sort_idx[i]];
        tmp_p[i] = result.pixel_u8[sort_idx[i]];
    }
    std::memcpy(result.ipix, tmp_i.data(), result.n_pix * sizeof(uint64_t));
    std::memcpy(result.pixel_u8, tmp_p.data(), result.n_pix * sizeof(uint8_t));

    LOG_DEBUG("ud_grade: %llu -> %llu (nside %u -> %u, shift=%d, uint8)",
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
// 获取数据 bbox (用于初始视角设置)
// 从子叶索引的 key (nside=64 子叶 ipix) 计算数据覆盖范围
// ============================================================================

int BrowserBackend::get_data_bbox(double& center_ra, double& center_dec,
                                   double& width_deg, double& height_deg) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_hiss_ && !hiss_leaf_index_.empty()) {
        // .hiss: 从子叶索引的 key 计算 bbox
        double min_ra = 360.0, max_ra = 0.0, min_dec = 90.0, max_dec = -90.0;
        for (const auto& kv : hiss_leaf_index_) {
            double ra, dec;
            HealpixMath::pix2ang_nest(64, kv.first, ra, dec);
            if (ra < min_ra) min_ra = ra;
            if (ra > max_ra) max_ra = ra;
            if (dec < min_dec) min_dec = dec;
            if (dec > max_dec) max_dec = dec;
        }
        center_ra = (min_ra + max_ra) * 0.5;
        center_dec = (min_dec + max_dec) * 0.5;
        width_deg = max_ra - min_ra;
        height_deg = max_dec - min_dec;

        // 子叶 nside=64 分辨率约 0.92°, 加 1 个子叶宽度作为边界余量
        double margin = 1.0;
        width_deg += margin;
        height_deg += margin;

        LOG_INFO("get_data_bbox(.hiss): center=(%.4f,%.4f) size=%.4fx%.4f deg",
                 center_ra, center_dec, width_deg, height_deg);
        return 0;
    } else if (is_hiss_ && all_ipix_ && n_pix_ > 0) {
        // .hiss 无子叶索引 (nside<64), 从全量数据采样
        double min_ra = 360.0, max_ra = 0.0, min_dec = 90.0, max_dec = -90.0;
        uint64_t step = std::max<uint64_t>(1, n_pix_ / 10000);
        for (uint64_t i = 0; i < n_pix_; i += step) {
            double ra, dec;
            HealpixMath::pix2ang_nest(nside_, all_ipix_[i], ra, dec);
            if (ra < min_ra) min_ra = ra;
            if (ra > max_ra) max_ra = ra;
            if (dec < min_dec) min_dec = dec;
            if (dec > max_dec) max_dec = dec;
        }
        center_ra = (min_ra + max_ra) * 0.5;
        center_dec = (min_dec + max_dec) * 0.5;
        width_deg = max_ra - min_ra;
        height_deg = max_dec - min_dec;
        LOG_INFO("get_data_bbox(.hiss sample): center=(%.4f,%.4f) size=%.4fx%.4f deg",
                 center_ra, center_dec, width_deg, height_deg);
        return 0;
    } else if (!file_path_.empty()) {
        // .hcsd: 返回全天
        center_ra = 0.0;
        center_dec = 0.0;
        width_deg = 360.0;
        height_deg = 180.0;
        return 0;
    }

    return -1;
}

void BrowserBackend::release_leaf(LeafData& leaf) {
    // 仅释放 owned=true 的 malloc 内存, 零拷贝切片不释放
    if (leaf.owned) {
        if (leaf.ipix) { std::free(leaf.ipix); }
        if (leaf.pixel) { std::free(leaf.pixel); }
        if (leaf.pixel_u8) { std::free(leaf.pixel_u8); }
    }
    leaf.ipix = nullptr;
    leaf.pixel = nullptr;
    leaf.pixel_u8 = nullptr;
    leaf.n_pix = 0;
    leaf.nside = 0;
    leaf.leaf_ipix = 0;
    leaf.owned = false;
    leaf.use_u8 = false;
}

// ============================================================================
// WP-H 步骤14: HISS Tile 按需加载 (新 API)
// load_hiss - 只读 Header + Tile 目录, 不加载像素数据
// read_tile_signal/support/snr - 按 parent_ipix 读取 Tile 数据
// query_pixel - 通过 ra/dec 查询像素值
// ============================================================================

int BrowserBackend::load_hiss(const std::string& path, HissHeader& header) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 重置状态
    hiss_header_loaded_ = false;
    hiss_header_ = HissHeader{};

    uint32_t nside = 0, tile_nside = 0, depth = 0, n_leaf_per_tile = 0;
    uint64_t n_tiles = 0, n_pix_total = 0;
    char* meta_json = nullptr;
    uint64_t* tile_ipix_list = nullptr;

    int ret = aio_hiss_inspect(path.c_str(), &nside, &tile_nside, &depth,
                                &n_leaf_per_tile, &n_tiles, &n_pix_total,
                                &meta_json, &tile_ipix_list);
    if (ret != 0) {
        LOG_ERROR("load_hiss: aio_hiss_inspect 失败 ret=%d path=%s", ret, path.c_str());
        return -1;
    }

    // 填充 header
    header.nside = nside;
    header.tile_nside = tile_nside;
    header.depth = depth;
    header.n_leaf_per_tile = n_leaf_per_tile;
    header.n_tiles = n_tiles;
    header.n_pix_total = n_pix_total;
    if (meta_json) {
        header.meta_json = meta_json;
        aio_hio_free(meta_json);
    }
    if (tile_ipix_list && n_tiles > 0) {
        header.tile_ipix_list.assign(tile_ipix_list, tile_ipix_list + n_tiles);
        aio_hio_free(tile_ipix_list);
    }

    // 保存到成员 (供 read_tile_*/query_pixel 使用)
    file_path_ = path;
    hiss_header_ = header;
    hiss_header_loaded_ = true;

    LOG_INFO("load_hiss: nside=%u tile_nside=%u depth=%u n_leaf=%u n_tiles=%llu path=%s",
             nside, tile_nside, depth, n_leaf_per_tile,
             (unsigned long long)n_tiles, path.c_str());
    return 0;
}

int BrowserBackend::read_tile_signal(uint64_t parent_ipix, HissTileData& tile) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_path_.empty()) {
        LOG_ERROR("read_tile_signal: 未打开 HISS 文件");
        return -1;
    }

    float* signal = nullptr;
    uint32_t n_signal = 0;
    int ret = aio_hiss_read_tile_signal(file_path_.c_str(), parent_ipix,
                                         &signal, &n_signal);
    if (ret != 0) {
        LOG_ERROR("read_tile_signal: aio_hiss_read_tile_signal 失败 ret=%d parent=%llu",
                  ret, (unsigned long long)parent_ipix);
        return -2;
    }

    tile.parent_ipix = parent_ipix;
    tile.signal = signal;
    tile.n_signal = n_signal;
    return 0;
}

int BrowserBackend::read_tile_support(uint64_t parent_ipix, HissTileData& tile) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_path_.empty()) {
        LOG_ERROR("read_tile_support: 未打开 HISS 文件");
        return -1;
    }

    uint8_t* support = nullptr;
    uint32_t n_support = 0;
    int ret = aio_hiss_read_tile_support(file_path_.c_str(), parent_ipix,
                                          &support, &n_support);
    if (ret != 0) {
        LOG_ERROR("read_tile_support: aio_hiss_read_tile_support 失败 ret=%d parent=%llu",
                  ret, (unsigned long long)parent_ipix);
        return -2;
    }

    tile.parent_ipix = parent_ipix;
    tile.support = support;
    if (tile.n_signal == 0) tile.n_signal = n_support;
    return 0;
}

int BrowserBackend::read_tile_snr(uint64_t parent_ipix, HissTileData& tile) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_path_.empty()) {
        LOG_ERROR("read_tile_snr: 未打开 HISS 文件");
        return -1;
    }

    uint8_t* snr_data = nullptr;
    uint32_t n_points = 0;
    int ret = aio_hiss_read_tile_snr(file_path_.c_str(), parent_ipix,
                                      &snr_data, &n_points);
    if (ret != 0) {
        LOG_ERROR("read_tile_snr: aio_hiss_read_tile_snr 失败 ret=%d parent=%llu",
                  ret, (unsigned long long)parent_ipix);
        return -2;
    }

    tile.parent_ipix = parent_ipix;
    tile.snr_data = snr_data;
    tile.n_snr_points = n_points;
    return 0;
}

int BrowserBackend::query_pixel(double ra, double dec, float& signal, uint8_t& support) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_path_.empty()) {
        LOG_ERROR("query_pixel: 未打开 HISS 文件");
        return -1;
    }

    signal = 0.0f;
    support = 0;
    int ret = aio_hiss_query_pixel(file_path_.c_str(), ra, dec, &signal, &support);
    if (ret != 0) {
        LOG_ERROR("query_pixel: aio_hiss_query_pixel 失败 ret=%d ra=%.4f dec=%.4f",
                  ret, ra, dec);
        return -2;
    }
    return 0;
}

void BrowserBackend::release_tile(HissTileData& tile) {
    if (tile.signal) { aio_hio_free(tile.signal); tile.signal = nullptr; }
    if (tile.support) { aio_hio_free(tile.support); tile.support = nullptr; }
    if (tile.snr_data) { aio_hio_free(tile.snr_data); tile.snr_data = nullptr; }
    tile.n_signal = 0;
    tile.n_snr_points = 0;
    tile.parent_ipix = 0;
}
