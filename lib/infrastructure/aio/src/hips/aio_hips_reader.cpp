// ============================================================================
// aio_hips_reader.cpp - IVOA HiPS 读取器实现 (CFITSIO)
// ============================================================================

#include "aio_hips_reader.h"
#include "healpix/healpix_core.h"

#include <fitsio.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// RT-008: cfitsio 全局表并行访问非线程安全 → 进程级串行化（共享单例，见头文件）
#include "aio_cfitsio_mutex.h"

namespace {

thread_local std::string g_rd_error;

void set_err(const std::string& m) { g_rd_error = m; }

bool fits_ok(int status, const std::string& where) {
    if (status == 0) return true;
    char msg[FLEN_ERRMSG];
    fits_get_errstatus(status, msg);
    set_err(where + ": " + msg);
    return false;
}

// IVOA REC-HIPS-1.0 §4.1 标准布局: Dir = (N/10000)*10000, Npix = N (完整 tile 号)。
std::string tile_path(const std::string& dir, int order, uint64_t ipix, const char* ext) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s/Norder%d/Dir%llu/Npix%llu%s",
                  dir.c_str(), order, (unsigned long long)((ipix / 10000u) * 10000u),
                  (unsigned long long)ipix, ext);
    return std::string(buf);
}

// 旧版非标准布局 (Dir=商, Npix=余数) —— 只读兼容, 仅标准路径不存在时回退。
std::string tile_path_legacy(const std::string& dir, int order, uint64_t ipix, const char* ext) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s/Norder%d/Dir%llu/Npix%llu%s",
                  dir.c_str(), order, (unsigned long long)(ipix / 10000u),
                  (unsigned long long)(ipix % 10000u), ext);
    return std::string(buf);
}

// 解析实际存在的 tile 路径: 标准布局优先, 缺失时回退旧布局 (M2b-B-01 迁移兼容)。
// ipix<10000 时两式同路径, 不产生额外 stat。
std::string tile_path_resolve(const std::string& dir, int order, uint64_t ipix, const char* ext) {
    const std::string std_p = tile_path(dir, order, ipix, ext);
    if (ipix < 10000u) return std_p;
    std::ifstream f(std_p, std::ios::binary);
    if (f.good()) return std_p;
    const std::string legacy_p = tile_path_legacy(dir, order, ipix, ext);
    std::ifstream g(legacy_p, std::ios::binary);
    return g.good() ? legacy_p : std_p;
}

// P1 (R9-A): 读侧产品参数硬校验 —— 与写侧 aio_hips_product_begin 完全一致
// 的口径 (写侧 :399 tile_width==512 硬校验)。公共头合同承诺调用方缓冲为
// 512*512*elem (aio_hips_reader.h:49-53), 恶意产品集 properties
// hips_tile_width=2048 + 恶意 tile FITS 同尺寸此前直接经 fits_read_pix 对
// 调用方 512^2 缓冲 4x 堆越界写 (越界 16 字节/像素)。
//   - tile_width 必须 =512 (与写侧、公共头合同逐字一致);
//   - hips_order ∈ [0,29]: HEALPix 合法域, 且 2*order <= 58 保证
//     1ULL << (2*order) 与 12*(1<<(2*order)) 均在 uint64 内, 消除移位 UB
//     (恶意 hips_order=32+ 此前 UB 移位/回绕)。
static bool valid_product_params(int tile_width, int hips_order) {
    return tile_width == 512 && hips_order >= 0 && hips_order <= 29;
}

// ---------------------------------------------------------------------------
// FIX-401 (ASTROCS_DESIGN.md §10「I/O 与原子产品」/ GAP_AUDIT G3-1):
// **完成清单 fail-closed** —— 「没有完成清单就不算成功对象」。产品集根下的
// manifest.json 由 writer 在**全部 tile 完成之后**最后原子落盘 (aio_hips_finalize);
// 中途 kill / 失败 / 取消只会留下无清单的 tile 残骸。消费者 (aio_hips_open)
// 必须拒绝这类目录, 否则残骸会被当成产品消费 (G3-1「重跑消费残留」)。
// 判据: 清单存在且非空 + 含 products 数组 + 声明了本次请求的子产品。
// ---------------------------------------------------------------------------
bool manifest_products_of(const std::string& root, std::string* products,
                          std::string* err) {
    const std::string mp = root + "/manifest.json";
    std::ifstream f(mp, std::ios::binary);
    if (!f.good()) {
        if (err) *err = "完成清单缺失 (manifest.json, §10 fail-closed): " + root;
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    if (text.empty()) {
        if (err) *err = "完成清单为空 (manifest.json, §10 fail-closed): " + mp;
        return false;
    }
    const size_t k = text.find("\"products\"");
    if (k == std::string::npos) {
        if (err) *err = "完成清单无 products 声明 (manifest.json, §10 fail-closed): " + mp;
        return false;
    }
    const size_t lb = text.find('[', k);
    const size_t rb = (lb == std::string::npos) ? std::string::npos : text.find(']', lb);
    if (lb == std::string::npos || rb == std::string::npos || rb < lb) {
        if (err) *err = "完成清单 products 声明不可解析 (manifest.json): " + mp;
        return false;
    }
    *products = text.substr(lb, rb - lb + 1);
    return true;
}

std::map<std::string, std::string> parse_properties(const std::string& path) {
    std::map<std::string, std::string> kv;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        // 去首尾空白
        while (!k.empty() && (k.back() == ' ' || k.back() == '\r')) k.pop_back();
        while (!k.empty() && (k.front() == ' ')) k.erase(k.begin());
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
        while (!v.empty() && (v.front() == ' ')) v.erase(v.begin());
        kv[k] = v;
    }
    return kv;
}

} // namespace

// 轻量 SNR 点 (reader 内部)
struct AioHipsSnrPoint2 {
    double ra = 0, dec = 0, snr = 0;
    int64_t star_id = 0;
    uint32_t quality_flags = 0;
    uint32_t photometric_status = 0;
};

struct AioHipsDataset {
    std::string dir;          // 子产品目录
    int product = 0;
    std::map<std::string, std::string> props;
    int hips_order = 0;
    int tile_width = 512;
    std::vector<uint64_t> tiles;      // 叶级 ipix (从 MOC 或目录扫描)
    std::vector<AioHipsSnrPoint2> snr; // 见下方类型
    int data_bitpix = -32;
    size_t bad_snr_rows = 0;          // 损坏 TSV 行计数 (G6 robustness)
};

// PERF-401: 读路径不再取进程级串行化锁。
// 线程模型依据（见 aio_cfitsio_mutex.h 头注释）：cfitsio 4.6.4 以 _REENTRANT
// 构建，FptrTable 与错误栈均由 cfitsio 自带 Fitsio_Lock 保护；READONLY 打开时
// fits_already_open 直接返回（cfileio.c:1544），不会跨线程复用同一 FITSfile*。
// 因此「每次调用各自 open/read/close、句柄只在本线程栈上」即可并发安全：
// 读路径零共享可变状态（原先的全局锁把 16 worker 的 tile 读串行化成 1 条流）。
template <typename T>
static int read_tile_t(AioHipsDataset* d, uint64_t ipix, T* out) {
    if (!d || !out) return -1;
    std::string p = tile_path_resolve(d->dir, d->hips_order, ipix, ".fits");
    int status = 0;
    fitsfile* fptr = nullptr;
    if (fits_open_file(&fptr, p.c_str(), READONLY, &status)) {
        fits_clear_errmsg();
        set_err("tile 不存在: " + p);
        return -2;
    }
    int bitpix = 0, naxis = 0;
    long naxes[2] = {0, 0};
    if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "tile param") ? -3 : -3;
    }
    if (naxis != 2 || naxes[0] != d->tile_width || naxes[1] != d->tile_width) {
        fits_close_file(fptr, &status);
        set_err("tile 尺寸非法");
        return -4;
    }
    long nelem = naxes[0] * naxes[1];
    long fpixel[2] = {1, 1};
    if (bitpix == -32) {
        std::vector<float> tmp((size_t)nelem);
        if (fits_read_pix(fptr, TFLOAT, fpixel, nelem, nullptr, tmp.data(), nullptr, &status)) {
            fits_close_file(fptr, &status);
            return fits_ok(status, "tile read") ? -5 : -5;
        }
        for (long i = 0; i < nelem; ++i) out[i] = (T)tmp[(size_t)i];
    } else if (bitpix == -64) {
        std::vector<double> tmp((size_t)nelem);
        if (fits_read_pix(fptr, TDOUBLE, fpixel, nelem, nullptr, tmp.data(), nullptr, &status)) {
            fits_close_file(fptr, &status);
            return fits_ok(status, "tile read") ? -5 : -5;
        }
        for (long i = 0; i < nelem; ++i) out[i] = (T)tmp[(size_t)i];
    } else {
        fits_close_file(fptr, &status);
        set_err("tile BITPIX 非 -32/-64");
        return -6;
    }
    fits_close_file(fptr, &status);
    return 0;
}

// DATA-UNC-001 §30.2: 诊断统计平面回读 (int32 专用通道)。
// tile BITPIX 必须 = 32 (诊断平面 dtype 由 §30.2 固定 int32, 不接受 float 冒充);
// 输出为 standard HiPS row-major (与 read_tile_t 同合同)。
static int read_tile_i32(AioHipsDataset* d, uint64_t ipix, int32_t* out) {
    if (!d || !out) return -1;
    const std::string p = tile_path_resolve(d->dir, d->hips_order, ipix, ".fits");
    int status = 0;
    fitsfile* fptr = nullptr;
    if (fits_open_file(&fptr, p.c_str(), READONLY, &status)) {
        fits_clear_errmsg();
        set_err("tile 不存在: " + p);
        return -2;
    }
    int bitpix = 0, naxis = 0;
    long naxes[2] = {0, 0};
    if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
        fits_close_file(fptr, &status);
        return -3;
    }
    if (naxis != 2 || naxes[0] != d->tile_width || naxes[1] != d->tile_width) {
        fits_close_file(fptr, &status);
        set_err("tile 尺寸非法");
        return -4;
    }
    if (bitpix != 32) {
        fits_close_file(fptr, &status);
        set_err("诊断平面 tile BITPIX 必须 =32 (int32), 实际 " +
                std::to_string(bitpix));
        return -6;
    }
    const long nelem = naxes[0] * naxes[1];
    long fpixel[2] = {1, 1};
    if (fits_read_pix(fptr, TINT, fpixel, nelem, nullptr, out, nullptr, &status)) {
        fits_close_file(fptr, &status);
        return -5;
    }
    fits_close_file(fptr, &status);
    return 0;
}

namespace {

// 从 MOC FITS 提取叶级 ipix (order == hips_order 的 UNIQ -> ipix)
bool load_tiles_from_moc(AioHipsDataset* d) {
    // PERF-401: 同 read_tile_t —— READONLY 打开不需要进程级串行化。
    std::string moc = d->dir + "/Moc.fits";
    int status = 0;
    fitsfile* fptr = nullptr;
    if (fits_open_file(&fptr, moc.c_str(), READONLY, &status)) {
        fits_clear_errmsg();
        return true;  // 无 MOC 不算错误 (空产品)
    }
    // MOC 数据在扩展 HDU (PRIMARY 为空图头)
    if (fits_movabs_hdu(fptr, 2, nullptr, &status)) {
        fits_close_file(fptr, &status);
        fits_clear_errmsg();
        return true;
    }
    long nrows = 0;
    if (fits_get_num_rows(fptr, &nrows, &status)) {
        fits_close_file(fptr, &status);
        set_err("moc rows: fits_get_num_rows failed");
        return false;   // P2 (R9-A): MOC 读失败不再当空产品吞掉 (此前返回值语义颠倒)
    }
    // P2 (R9-A): MOC 行数上限 —— nrows 来自恶意 BINTABLE 头 (NAXIS2) 不可信,
    // 此前 uniq vector 按 nrows 巨量分配打爆内存。合法产品 tile 数
    // <= 12*4^tile_order, 1e6 行 (800MB 产品面) 余量充足, 超限按损坏拒绝。
    if (nrows > 1000000) {
        fits_close_file(fptr, &status);
        set_err("moc rows 超上限 (nrows=" + std::to_string(nrows) + ")");
        return false;
    }
    std::vector<long long> uniq((size_t)nrows);
    if (nrows > 0 && fits_read_col(fptr, TLONGLONG, 1, 1, 1, nrows, nullptr,
                                   uniq.data(), nullptr, &status)) {
        fits_close_file(fptr, &status);
        set_err("moc read: fits_read_col failed");
        return false;   // P2 (R9-A): 同上, 读失败不再当空产品
    }
    fits_close_file(fptr, &status);
    const uint64_t order_uniq_base = 4ULL * (1ULL << (2ULL * (uint64_t)d->hips_order));
    for (long long u : uniq) {
        if ((uint64_t)u >= order_uniq_base) {
            uint64_t ipix = (uint64_t)u - order_uniq_base;
            if (ipix < 12ULL * (1ULL << (2ULL * (uint64_t)d->hips_order)))
                d->tiles.push_back(ipix);
        }
    }
    std::sort(d->tiles.begin(), d->tiles.end());
    d->tiles.erase(std::unique(d->tiles.begin(), d->tiles.end()), d->tiles.end());
    return true;
}

} // namespace

namespace {
// PERF-401: 单像素读。原实现为取 1 个 leaf 值而分配并读满整块 512² tile
// （1 MiB），而它在 control-ivar 装配循环里被调用 n_cells × n_frames 次 ——
// 单这一处就占 Phase2 全量 tile 读的 1/3 左右。此处只向 cfitsio 要目标像素。
// 取值与 read_tile_t 的 tmp[fi] 逐位相同：同一文件、同一 BITPIX 分支、同一
// fits_index -> (col=fits_index%512, row=fits_index/512) 行主序映射
// （healpix_core.cpp:288 nested_local_to_fits_index = (maxv-x)*width + y）。
template <typename T>
static int read_tile_pixel_t(AioHipsDataset* d, uint64_t ipix, uint64_t fits_index, T* out) {
    if (!d || !out) return -1;
    const uint64_t n_pix = (uint64_t)d->tile_width * (uint64_t)d->tile_width;
    if (fits_index >= n_pix) return -6;
    std::string p = tile_path_resolve(d->dir, d->hips_order, ipix, ".fits");
    int status = 0;
    fitsfile* fptr = nullptr;
    if (fits_open_file(&fptr, p.c_str(), READONLY, &status)) {
        fits_clear_errmsg();
        set_err("tile 不存在: " + p);
        return -2;
    }
    int bitpix = 0, naxis = 0;
    long naxes[2] = {0, 0};
    if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
        fits_close_file(fptr, &status);
        return -3;
    }
    if (naxis != 2 || naxes[0] != d->tile_width || naxes[1] != d->tile_width) {
        fits_close_file(fptr, &status);
        set_err("tile 尺寸非法");
        return -4;
    }
    const long w = (long)d->tile_width;
    long fpixel[2] = {(long)(fits_index % (uint64_t)w) + 1,
                      (long)(fits_index / (uint64_t)w) + 1};
    if (bitpix == -32) {
        float v = 0.0f;
        if (fits_read_pix(fptr, TFLOAT, fpixel, 1, nullptr, &v, nullptr, &status)) {
            fits_close_file(fptr, &status);
            return -5;
        }
        *out = (T)v;
    } else if (bitpix == -64) {
        double v = 0.0;
        if (fits_read_pix(fptr, TDOUBLE, fpixel, 1, nullptr, &v, nullptr, &status)) {
            fits_close_file(fptr, &status);
            return -5;
        }
        *out = (T)v;
    } else {
        fits_close_file(fptr, &status);
        set_err("tile BITPIX 非 -32/-64");
        return -6;
    }
    fits_close_file(fptr, &status);
    return 0;
}

template <typename T>
int read_leaf_t(AioHipsDataset* d, uint64_t leaf_ipix, T* out) {
    if (!d || !out) return -1;
    if (d->tile_width != 512) {
        set_err("leaf 查询要求 tile_width=512");
        return -6;
    }
    const uint64_t tile = leaf_ipix >> 18;
    const uint64_t z = leaf_ipix & ((1ULL << 18) - 1ULL);
    const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(z, 9u, 512u);
    return read_tile_pixel_t(d, tile, fi, out);
}
// ============================================================================
// P1 (R9-A): C 边界异常屏障 (bughunt_p1_batchI; 家族方案对齐 f1cb487c
// aio_api.cpp P0-4 口径)。本文件 extern "C" 12 个导出入口此前 0 个有 try
// 保护: reader 内部 std::string/vector/map 分配 bad_alloc、length_error,
// parse_properties/std::atoi 链上的构造异常均可跨 C ABI 传播 (UB/terminate)。
// 统一口径: 指针返回型 -> nullptr; int 返回型 -> -1 (既有参数错误码);
// void -> 仅记录; last_error 为 noexcept 字符串返回不加壳 (g_rd_error 为
// 命名空间级 thread_local std::string, 其 c_str() 不抛)。正常路径与修复前
// 逐行等价。
// ============================================================================
} // namespace
extern "C" {

AioHipsDataset* aio_hips_open(const char* out_dir, int product)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        g_rd_error.clear();
        if (!out_dir || !*out_dir || product < AIO_HIPS_RD_SIGNAL ||
            product > AIO_HIPS_RD_NUSED) {
            set_err("参数无效");
            return nullptr;
        }
        std::unique_ptr<AioHipsDataset> d(new AioHipsDataset);
        d->product = product;
        const char* sub = product == AIO_HIPS_RD_SIGNAL ? "signal" :
                          product == AIO_HIPS_RD_SUPPORT ? "support" :
                          product == AIO_HIPS_RD_SNR ? "snr" :
                          product == AIO_HIPS_RD_VARIANCE ? "variance" :
                          product == AIO_HIPS_RD_IVAR ? "ivar" :
                          product == AIO_HIPS_RD_NREJ ? "nrej" : "nused";
        // FIX-401 §10: 完成清单 fail-closed (先于任何子产品内容读取)。
        {
            std::string products, merr;
            if (!manifest_products_of(out_dir, &products, &merr)) {
                set_err(merr);
                return nullptr;
            }
            const std::string quoted = std::string("\"") + sub + "\"";
            if (products.find(quoted) == std::string::npos) {
                set_err("完成清单未声明子产品 '" + std::string(sub) +
                        "' (manifest.json, §10 fail-closed): " + std::string(out_dir));
                return nullptr;
            }
        }
        d->dir = std::string(out_dir) + "/" + sub;
        d->props = parse_properties(d->dir + "/properties");
        auto geti = [&](const std::string& k, int def) -> int {
            auto it = d->props.find(k);
            return it == d->props.end() ? def : std::atoi(it->second.c_str());
        };
        d->hips_order = geti("hips_order", 0);
        d->tile_width = geti("hips_tile_width", 512);
        // P1 (R9-A): 恶意产品集读侧硬校验 (口径=写侧 :399), 见 valid_product_params。
        // 此前恶意 hips_tile_width=2048 + 同尺寸恶意 tile 直接 4x 堆越界写调用方
        // 512^2 缓冲; 恶意 hips_order=32+ 触发 1ULL<<(2*order) 移位 UB。
        if (!valid_product_params(d->tile_width, d->hips_order)) {
            set_err("产品参数非法 (tile_width=" + std::to_string(d->tile_width) +
                    " hips_order=" + std::to_string(d->hips_order) +
                    "; 要求 tile_width=512, hips_order∈[0,29]): " + d->dir);
            return nullptr;
        }
        if (d->props.find("hips_version") == d->props.end()) {
            set_err("properties 缺失 hips_version: " + d->dir);
            return nullptr;
        }
        // P2 (R9-A): MOC 读失败此前被静默吞掉 (返回值忽略) → 产品按"空集"伪装
        // 成功。fail-closed: Moc.fits 存在但损坏/超限时 open 硬失败; 无 MOC 仍
        // 合法空产品 (load_tiles_from_moc 返回 true)。
        if (!load_tiles_from_moc(d.get())) {
            set_err("MOC 读取失败: " + d->dir + " (" + g_rd_error + ")");
            return nullptr;
        }
        if (product == AIO_HIPS_RD_SNR) {
            // 读取全部 SNR TSV tiles
            for (uint64_t ip : d->tiles) {
                std::string p = tile_path_resolve(d->dir, d->hips_order, ip, ".tsv");
                std::ifstream f(p);
                std::string line;
                bool first = true;
                while (std::getline(f, line)) {
                    if (line.empty()) continue;
                    if (first) { first = false; if (line[0] == '#') continue; }
                    long long sid; double ra, dec, snr;
                    unsigned int qf, ps;
                    if (std::sscanf(line.c_str(), "%lld %lf %lf %lf %u %u",
                                    &sid, &ra, &dec, &snr, &qf, &ps) == 6) {
                        AioHipsSnrPoint2 pt;
                        pt.star_id = sid; pt.ra = ra; pt.dec = dec; pt.snr = snr;
                        pt.quality_flags = qf; pt.photometric_status = ps;
                        d->snr.push_back(pt);
                    } else {
                        ++d->bad_snr_rows;
                    }
                }
            }
        }
        return d.release();

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return nullptr;
    } catch (...) {
        set_err("unknown exception");
        return nullptr;
    }
}

int aio_hips_get_properties(AioHipsDataset* d, char* buf, int buf_size)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        if (!d || !buf || buf_size <= 0) return -1;
        std::string s;
        for (const auto& kv : d->props) s += kv.first + "=" + kv.second + "\n";
        std::strncpy(buf, s.c_str(), (size_t)buf_size - 1);
        buf[buf_size - 1] = '\0';
        return 0;

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_tile_count(AioHipsDataset* d)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        return d ? (int)d->tiles.size() : -1;

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_tile_ipix(AioHipsDataset* d, int i, uint64_t* out_ipix)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        if (!d || !out_ipix || i < 0 || (size_t)i >= d->tiles.size()) return -1;
        *out_ipix = d->tiles[(size_t)i];
        return 0;

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_read_tile_f32(AioHipsDataset* d, uint64_t ipix, float* out)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        return read_tile_t(d, ipix, out);

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_read_tile_f64(AioHipsDataset* d, uint64_t ipix, double* out)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        return read_tile_t(d, ipix, out);

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}


int aio_hips_read_tile_i32(AioHipsDataset* d, uint64_t ipix, int32_t* out)  {
    // P1 (R9-A) 同款 C 边界异常屏障
    try {
        return read_tile_i32(d, ipix, out);

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_read_leaf_f32(AioHipsDataset* d, uint64_t leaf_ipix, float* out)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        return read_leaf_t(d, leaf_ipix, out);

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_read_leaf_f64(AioHipsDataset* d, uint64_t leaf_ipix, double* out)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        return read_leaf_t(d, leaf_ipix, out);

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_read_tile_datasum(AioHipsDataset* d, uint64_t tile_ipix,
                               char* out, int out_size)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        if (!d || !out || out_size <= 0) return -1;
        // 诊断接口（非热路径）：保留计数式串行化，等待时间进 lock_wait_ns。
        aio::CfitsioLockGuard cfitsio_guard;
        std::string p = tile_path_resolve(d->dir, d->hips_order, tile_ipix, ".fits");
        int status = 0;
        fitsfile* fptr = nullptr;
        if (fits_open_file(&fptr, p.c_str(), READONLY, &status)) {
            fits_clear_errmsg();
            set_err("tile 不存在: " + p);
            return -2;
        }
        char value[FLEN_VALUE] = {0};
        if (fits_read_key(fptr, TSTRING, "DATASUM", value, nullptr, &status)) {
            fits_clear_errmsg();
            fits_close_file(fptr, &status);
            set_err("tile 无 DATASUM: " + p);
            return -3;
        }
        fits_close_file(fptr, &status);
        std::strncpy(out, value, (std::size_t)out_size - 1);
        out[out_size - 1] = '\0';
        return 0;

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}
int aio_hips_read_snr_catalog(AioHipsDataset* d, double* ra, double* dec,
                              double* snr, int64_t* star_id,
                              uint32_t* quality_flags, uint32_t* photometric_status,
                              int max)  {
    // P1 (R9-A): C 边界异常屏障 (点数返回型, 异常 -> -1)
    try {
        if (!d || d->product != AIO_HIPS_RD_SNR) return -1;
        int n = (int)std::min((size_t)max, d->snr.size());
        for (int i = 0; i < n; ++i) {
            if (ra) ra[i] = d->snr[(size_t)i].ra;
            if (dec) dec[i] = d->snr[(size_t)i].dec;
            if (snr) snr[i] = d->snr[(size_t)i].snr;
            if (star_id) star_id[i] = d->snr[(size_t)i].star_id;
            if (quality_flags) quality_flags[i] = d->snr[(size_t)i].quality_flags;
            if (photometric_status) photometric_status[i] = d->snr[(size_t)i].photometric_status;
        }
        return n;

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}

void aio_hips_close(AioHipsDataset* d)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        delete d;

    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
    } catch (...) {
        set_err("unknown exception");
    }
}

const char* aio_hips_reader_last_error(void) {
    return g_rd_error.c_str();
}

} // extern "C"
