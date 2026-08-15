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
#include <unordered_set>
#include <cstdlib>
#include <string>

// ============================================================================
// 构造 / 析构
// ============================================================================

// forward declaration
static bool detect_fp64_from_meta(const char* meta_json);

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
    // 重置 HISS Header (避免旧文件 Header 残留)
    hiss_header_ = HissHeader{};
    hiss_header_loaded_ = false;
    // 重置精度模式标志 (避免旧文件残留)
    is_fp64_ = false;

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
        // 单帧模式 - 按需加载: 只读 Header/Tile 目录, 不加载像素数据
        // WP-H: "ACSH" 是新 HISS 格式 (8字节 MAGIC "ACSHISS\0") 的前4字节
        // "HISS" 是旧格式, 两者均由 HissReader 统一处理
        // 规范要求: Browser 打开 HISS 时先读 Header/目录, 不加载整文件; 按视野读取 Tile
        // - 旧方案 aio_hiss_read: 全量加载所有像素 (内存随文件大小线性增长)
        // - 新方案 aio_hiss_inspect: 只读 Header + Tile 目录 (NSIDE/Tile数/元数据)
        // - load_leaf 时按 parent_ipix 调用 aio_hiss_read_tile_signal 按需读取
        is_hiss_ = true;
        uint32_t nside = 0, tile_nside = 0, depth = 0, n_leaf_per_tile = 0;
        uint64_t n_tiles = 0, n_pix_total = 0;
        char* meta_json = nullptr;
        uint64_t* tile_ipix_list = nullptr;

        int ret = aio_hiss_inspect(path.c_str(), &nside, &tile_nside, &depth,
                                    &n_leaf_per_tile, &n_tiles, &n_pix_total,
                                    &meta_json, &tile_ipix_list);
        if (ret != 0) {
            LOG_ERROR("aio_hiss_inspect 失败: ret=%d path=%s", ret, path.c_str());
            file_path_.clear();
            return -3;
        }

        nside_ = nside;
        nested_ = 1;  // HISS 固定 NESTED
        n_pix_ = n_pix_total;

        // 保存 Header (供 load_leaf 按需加载使用)
        hiss_header_.nside = nside;
        hiss_header_.tile_nside = tile_nside;
        hiss_header_.depth = depth;
        hiss_header_.n_leaf_per_tile = n_leaf_per_tile;
        hiss_header_.n_tiles = n_tiles;
        hiss_header_.n_pix_total = n_pix_total;
        hiss_header_loaded_ = true;

        if (tile_ipix_list && n_tiles > 0) {
            hiss_header_.tile_ipix_list.assign(tile_ipix_list, tile_ipix_list + n_tiles);
            aio_hio_free(tile_ipix_list);
        }

        // 解析 filter (从 meta_json 提取)
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
            // 检测精度模式 (signal_dtype=1 或 precision_mode="fp64")
            is_fp64_ = detect_fp64_from_meta(meta_json);
            hiss_header_.meta_json = meta_json;
            aio_hio_free(meta_json);
        }

        // 按需模式: 不加载像素数据, 不建立子叶索引
        // all_ipix_/all_pixel_ 保持 nullptr, hiss_leaf_index_ 保持空
        // load_leaf 通过 aio_hiss_read_tile_signal 按 parent_ipix 按需读取 Tile

        LOG_INFO(".hiss 已打开(按需模式): nside=%u tile_nside=%u depth=%u "
                 "n_tiles=%llu n_pix=%llu filter=%s",
                 nside_, tile_nside, depth,
                 (unsigned long long)n_tiles, (unsigned long long)n_pix_,
                 filter_.c_str());

        return 0;
    } else if (std::memcmp(magic, "HCSD", 4) == 0) {
        // 球面模式 - 仅读取元信息, 不全量加载像素数据
        // 注: healpix_io 未提供单独读头的 API, 用 hcsd_read 一次读取后立即释放像素数据
        // (索引表已 O(1) 定位, 按需 hcsd_read_leaf 加载子叶)
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
    // 重置 HISS Header (按需模式状态)
    hiss_header_ = HissHeader{};
    hiss_header_loaded_ = false;
    // 重置精度模式标志
    is_fp64_ = false;
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

    // B19: 用 hiss_header_.tile_nside 替代硬编码 64
    // 支持任意合法 tile_nside (16/64/128/8192 等), .hiss 按需模式从 Header 读取
    uint32_t nside_leaf = 64;
    if (is_hiss_ && hiss_header_loaded_ && hiss_header_.tile_nside > 0) {
        nside_leaf = hiss_header_.tile_nside;
    }
    // .hcsd 模式无 tile_nside, 沿用 64 作为子叶层 (hcsd_read_leaf 按 nside=64 子叶读取)

    // 预加载范围: FOV × 1.5 + 1.0° 余量 (渲染用 FOV × 1.2, 预加载区供缩放流畅)
    // 加 1.0° 余量确保: 子叶中心在范围外但边缘在视场内的子叶也被加载
    const double half_fov = view.fov_deg * 0.75 + 1.0;

    // B19: 使用 HISS 目录空间查询 (query_disc 球面圆盘), 不扫固定全天 nside=64
    // query_disc 基于球面大圆距离, 天然处理 RA 跨越 (359°→0°) 和极区
    std::vector<uint64_t> disc_ipix = HealpixMath::query_disc(
        nside_leaf, view.center_ra, view.center_dec, half_fov);

    // B19: 若 HISS 按需模式有 Tile 目录, 只保留文件中实际存在的 Tile (取交集)
    // 避免对不存在的 Tile 触发无意义的 load_leaf
    std::unordered_set<uint64_t> tile_set;
    if (is_hiss_ && hiss_header_loaded_ && !hiss_header_.tile_ipix_list.empty()) {
        tile_set.insert(hiss_header_.tile_ipix_list.begin(),
                        hiss_header_.tile_ipix_list.end());
    }

    for (uint64_t ipix : disc_ipix) {
        // 若有 Tile 目录, 过滤掉文件中不存在的 Tile
        if (!tile_set.empty() && tile_set.find(ipix) == tile_set.end()) {
            continue;
        }
        double ra, dec;
        HealpixMath::pix2ang_nest(nside_leaf, ipix, ra, dec);
        double dist = HealpixMath::angular_distance(
            view.center_ra, view.center_dec, ra, dec);
        candidates.push_back({ipix, dist});
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

    LOG_DEBUG("get_required_leaves: nside_leaf=%u 视场内候选 %zu 个, 返回 %zu 个",
              nside_leaf, candidates.size(), result.size());
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
    // nside = 360 / (theta_screen * sqrt(12)) = 58.6 / theta_screen (近似)
    // 略大于屏幕: 用 theta_screen * 1.0 (像素 ≈ 屏幕, 不超采样)
    double nside_ideal = 58.6 / theta_screen;

    // B19: nside_target 下限改为 hiss_header_.tile_nside (支持 16/64/128/8192 等)
    // 不再硬编码 64; .hiss 按需模式从 Header 读取 tile_nside, 其他模式 fallback 64
    uint32_t nside_min = 64;
    if (is_hiss_ && hiss_header_loaded_ && hiss_header_.tile_nside > 0) {
        nside_min = hiss_header_.tile_nside;
    }
    // nside_min 不能超过数据 nside_ (高分辨率数据 tile_nside 可能 == nside_)
    if (nside_min > nside_) nside_min = nside_;

    // 向上取整到 2 的幂 (确保 HEALPix 像素 ≤ 屏幕像素, 避免欠采样模糊)
    // 向下取整会导致多个顶点映射到同一 HEALPix 像素, 产生块状模糊
    uint32_t nside_target = nside_min;
    while (nside_target < (uint32_t)std::ceil(nside_ideal) && nside_target < nside_) {
        nside_target <<= 1;
    }

    // clamp 到 [nside_min, nside_]
    if (nside_target < nside_min) nside_target = nside_min;
    if (nside_target > nside_) nside_target = nside_;

    LOG_DEBUG("decide_target_nside: fov=%.2f vp=%d theta_screen=%.4f°/px "
              "nside_ideal=%.1f -> nside_target=%u (nside_min=%u 原始 %u)",
              fov_deg, vp, theta_screen, nside_ideal, nside_target, nside_min, nside_);
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
        // 按需加载: 调用 aio_hiss_read_tile_signal 读取 Tile signal
        // B19: leaf_ipix 已是 tile_nside 层的 ipix (get_required_leaves 用
        // hiss_header_.tile_nside 空间查询返回), 直接作为 parent_ipix
        // 每个 Tile 含 n_leaf_per_tile = 4^depth 个像素 (NESTED 排序)
        result.n_pix = 0;
        result.nside = nside_;
        result.owned = true;  // malloc/aio 分配, release_leaf 释放

        uint32_t depth = hiss_header_.depth;

        // B19: leaf_ipix 与 tile_nside 同层, 直接作为 parent_ipix
        // (不再需要 nside=64 ↔ tile_nside 的移位转换, 支持 16/64/128/8192)
        uint64_t parent_ipix = leaf_ipix;

        // 调用按需 API 读取 Tile signal (只读这一个 Tile, 不加载其他 Tile)
        float* signal = nullptr;
        uint32_t n_signal = 0;
        int ret = aio_hiss_read_tile_signal(file_path_.c_str(), parent_ipix,
                                             &signal, &n_signal);
        if (ret != 0) {
            // Tile 无数据 (视场外或文件中不存在)
            LOG_DEBUG("load_leaf(.hiss ondemand) leaf=%llu parent=%llu 无数据 ret=%d",
                      (unsigned long long)leaf_ipix,
                      (unsigned long long)parent_ipix, ret);
            return result;
        }
        if (!signal || n_signal == 0) {
            if (signal) aio_hio_free(signal);
            LOG_DEBUG("load_leaf(.hiss ondemand) leaf=%llu 空Tile",
                      (unsigned long long)leaf_ipix);
            return result;
        }

        // 生成 ipix 数组: Tile 内第 i 个像素 ipix = parent_ipix << (2*depth) | i
        // (NESTED: 高位是 parent_ipix, 低位是 Tile 内 local_ipix)
        int tile_shift = (int)depth * 2;
        uint64_t* ipix = (uint64_t*)std::malloc((size_t)n_signal * sizeof(uint64_t));
        if (!ipix) {
            aio_hio_free(signal);
            LOG_ERROR("load_leaf(.hiss) malloc ipix 失败 n=%u", n_signal);
            return result;
        }
        for (uint32_t i = 0; i < n_signal; i++) {
            ipix[i] = (parent_ipix << tile_shift) | (uint64_t)i;
        }

        result.n_pix = n_signal;
        result.ipix = ipix;       // std::malloc 分配, release_leaf 用 std::free
        result.pixel = signal;    // aio 分配 (与 hcsd 路径一致, 同一 CRT 下 std::free 兼容)
        result.owned = true;

        LOG_DEBUG("load_leaf(.hiss ondemand) leaf=%llu parent=%llu n_pix=%u nside=%u",
                  (unsigned long long)leaf_ipix,
                  (unsigned long long)parent_ipix, n_signal, result.nside);

        // 降采样 (ud_grade 返回 owned=true 的 malloc 内存, uint8)
        if (target_nside < nside_ && target_nside > 0 && result.n_pix > 0) {
            LeafData downsampled = ud_grade(result, target_nside, data_min_, data_max_);
            // 释放原始 Tile 数据 (pixel 用 aio_hio_free, ipix 用 std::free)
            aio_hio_free(result.pixel);
            std::free(result.ipix);
            result.ipix = nullptr;
            result.pixel = nullptr;
            return downsampled;
        }
        // 无降采样: 返回 owned=true 的数据, release_leaf 会用 std::free 释放
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

    // B20: signal = 累计通量 (HISS 规范: 不除面积), LOD 降采样必须求和
    // 合并后大像素的累计通量 = 4^k 个子像素累计通量之和
    // (若取平均会丢失面积信息, 违反 HISS signal 语义)
    // 注: support 是面积比 [0,255] uint8, 应独立按面积求和后归一化处理,
    // 不在此函数中 (本函数仅处理 signal 路径, input.pixel 为 signal)
    std::unordered_map<uint64_t, std::pair<double, uint32_t>> groups;
    groups.reserve(input.n_pix >> shift + 1);
    for (uint64_t i = 0; i < input.n_pix; i++) {
        uint64_t ipix_coarse = (shift > 0) ? (input.ipix[i] >> shift) : input.ipix[i];
        auto& g = groups[ipix_coarse];
        g.first += (double)input.pixel[i];  // signal 求和 (不取平均)
        g.second += 1;  // 像素计数 (仅用于日志/校验, 不参与归一化)
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
        // B20: signal 降采样用求和 (非平均), 符合累计通量语义
        double sum = kv.second.first;  // 已累加的 signal 总和 (不除以 count)
        // 归一化到 [0, 255]
        double normalized = (sum - (double)data_min) * inv_range;
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

    if (is_hiss_ && hiss_header_loaded_ && !hiss_header_.tile_ipix_list.empty()) {
        // .hiss 按需模式: 从 Tile 目录 (tile_nside 层) 计算 bbox
        // open_file 仅读 Header, hiss_leaf_index_ 为空, 用 tile_ipix_list 替代
        double min_ra = 360.0, max_ra = 0.0, min_dec = 90.0, max_dec = -90.0;
        uint32_t tile_nside = hiss_header_.tile_nside;
        if (tile_nside == 0) tile_nside = 64;
        for (uint64_t tile_ipix : hiss_header_.tile_ipix_list) {
            double ra, dec;
            HealpixMath::pix2ang_nest(tile_nside, tile_ipix, ra, dec);
            if (ra < min_ra) min_ra = ra;
            if (ra > max_ra) max_ra = ra;
            if (dec < min_dec) min_dec = dec;
            if (dec > max_dec) max_dec = dec;
        }
        center_ra = (min_ra + max_ra) * 0.5;
        center_dec = (min_dec + max_dec) * 0.5;
        width_deg = max_ra - min_ra;
        height_deg = max_dec - min_dec;
        // 加 1° 余量 (tile_nside=64 子叶约 0.92°, 半径约 0.46°)
        width_deg += 1.0;
        height_deg += 1.0;

        LOG_INFO("get_data_bbox(.hiss ondemand): center=(%.4f,%.4f) size=%.4fx%.4f deg "
                 "n_tiles=%llu tile_nside=%u",
                 center_ra, center_dec, width_deg, height_deg,
                 (unsigned long long)hiss_header_.n_tiles, tile_nside);
        return 0;
    } else if (is_hiss_ && !hiss_leaf_index_.empty()) {
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
// 从 HISS metadata JSON 检测 FP64 精度模式
// 匹配以下两种字段之一 (任一命中即为 FP64):
// "signal_dtype": 1 (0=float32, 1=float64)
// "precision_mode": "fp64"
// 使用字符串查找而非完整 JSON 解析, 避免引入 JSON 依赖
// ============================================================================
static bool detect_fp64_from_meta(const char* meta_json) {
    if (meta_json == nullptr) return false;

    // 检查 "signal_dtype" : 1
    const char* key_dtype = "\"signal_dtype\"";
    const char* p = std::strstr(meta_json, key_dtype);
    if (p) {
        p += std::strlen(key_dtype);
        while (*p && (*p == ' ' || *p == ':' || *p == '"')) p++;
        if (*p == '1') return true;
    }

    // 检查 "precision_mode": "fp64"
    const char* key_prec = "\"precision_mode\"";
    p = std::strstr(meta_json, key_prec);
    if (p) {
        p += std::strlen(key_prec);
        while (*p && (*p == ' ' || *p == ':' || *p == '"')) p++;
        if (std::strncmp(p, "fp64", 4) == 0) return true;
    }

    return false;
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
    is_fp64_ = false;

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
        // 检测精度模式 (signal_dtype=1 或 precision_mode="fp64")
        is_fp64_ = detect_fp64_from_meta(meta_json);
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
    // 按文件精度选择 SNR 读取 (FP64 文件 SNR 为 f64 存储, 12B/点)
    int ret = is_fp64_
        ? aio_hiss_read_tile_snr_f64(file_path_.c_str(), parent_ipix, &snr_data, &n_points)
        : aio_hiss_read_tile_snr(file_path_.c_str(), parent_ipix, &snr_data, &n_points);
    if (ret != 0) {
        LOG_ERROR("read_tile_snr: aio_hiss_read_tile_snr%s 失败 ret=%d parent=%llu",
                  is_fp64_ ? "_f64" : "",
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

// ============================================================================
// FP64 路径 - read_tile_signal_f64 / query_pixel_f64
// 仅适用于 FP64 模式文件 (signal_dtype=1); FP32 文件由 AIO 返回错误, 禁止静默转换
// ============================================================================

int BrowserBackend::read_tile_signal_f64(uint64_t parent_ipix, HissTileData& tile) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_path_.empty()) {
        LOG_ERROR("read_tile_signal_f64: 未打开 HISS 文件");
        return -1;
    }

    double* signal = nullptr;
    uint32_t n_signal = 0;
    int ret = aio_hiss_read_tile_signal_f64(file_path_.c_str(), parent_ipix,
                                              &signal, &n_signal);
    if (ret != 0) {
        LOG_ERROR("read_tile_signal_f64: aio_hiss_read_tile_signal_f64 失败 ret=%d parent=%llu",
                  ret, (unsigned long long)parent_ipix);
        return -2;
    }

    tile.parent_ipix = parent_ipix;
    tile.signal_f64 = signal;
    tile.n_signal = n_signal;
    return 0;
}

int BrowserBackend::query_pixel_f64(double ra, double dec, double& signal, uint8_t& support) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_path_.empty()) {
        LOG_ERROR("query_pixel_f64: 未打开 HISS 文件");
        return -1;
    }

    signal = 0.0;
    support = 0;
    int ret = aio_hiss_query_pixel_f64(file_path_.c_str(), ra, dec, &signal, &support);
    if (ret != 0) {
        LOG_ERROR("query_pixel_f64: aio_hiss_query_pixel_f64 失败 ret=%d ra=%.4f dec=%.4f",
                  ret, ra, dec);
        return -2;
    }
    return 0;
}

void BrowserBackend::release_tile(HissTileData& tile) {
    if (tile.signal) { aio_hio_free(tile.signal); tile.signal = nullptr; }
    if (tile.signal_f64) { aio_hio_free(tile.signal_f64); tile.signal_f64 = nullptr; }
    if (tile.support) { aio_hio_free(tile.support); tile.support = nullptr; }
    if (tile.snr_data) { aio_hio_free(tile.snr_data); tile.snr_data = nullptr; }
    tile.n_signal = 0;
    tile.n_snr_points = 0;
    tile.parent_ipix = 0;
}
