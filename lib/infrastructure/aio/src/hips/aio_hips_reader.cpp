// ============================================================================
// aio_hips_reader.cpp - IVOA HiPS 读取器实现 (CFITSIO)
// ============================================================================

#include "aio_hips_reader.h"
#include "healpix/healpix_core.h"

#include <fitsio.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
// Issue #87: 文件存在性/大小经 aio 唯一文件系统原语（不在本 TU 自持 stat 语义）
#include "aio_atomic_file.h"

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

// ---------------------------------------------------------------------------
// Issue #87: tile 失败分类所需的确定性信息。
// FITS 文件总长必须是 2880 的整数倍；不是 ⇒ 截断（结构损坏），不是 I/O 故障。
// 目录/设备等非普通文件 ⇒ I/O 故障（open 时 cfitsio 报的 104/108 不足以区分）。
// ---------------------------------------------------------------------------
struct TileDiskInfo {
    bool exists = false;
    bool is_dir = false;
    uint64_t size = 0;
    int stat_errno = 0;
};

TileDiskInfo tile_disk_info(const std::string& path) {
    TileDiskInfo out;
    uint64_t size = 0;
    int is_dir = 0;
    const int exists = aio_atomic::path_size(path, &size, &is_dir);
    out.exists = exists != 0;
    out.is_dir = is_dir != 0;
    out.size = size;
    out.stat_errno = exists ? 0 : errno;
    return out;
}

// cfitsio 状态码中属于 FITS 结构损坏（而不是存储 I/O 故障）的集合。
// 依据 fitsio.h 的错误码分段: 头/表结构 201..264、HDU/列/维度 301..323、
// 关键字格式 401..412、EOF 107、BAD_DATA_FILL/BAD_HEADER_FILL。
bool is_structural_cfitsio_status(int status) {
    if (status == END_OF_FILE) return true;                 // 107
    if (status >= 201 && status <= 264) return true;        // 头/表结构
    if (status >= 301 && status <= 323) return true;        // HDU/维度/行
    if (status >= 401 && status <= 412) return true;        // 关键字格式
    return false;
}

// 文件长度不是 2880 倍数 = 截断（FITS 记录粒度）。
bool is_truncated_fits_size(const TileDiskInfo& d) {
    return d.exists && d.size % 2880u != 0u;
}

// ── FITS 1's-complement 32-bit checksum（DATASUM / 全 HDU CHECKSUM 共用）───
// 32 位字按 1's complement 相加，等价于对 2^32-1 取模求和；用 64 位累加后
// 回卷即可（512² tile 至多 2^19 字，不会溢出 64 位）。
inline uint32_t fold_ones_complement(uint64_t s) {
    while (s >> 32) s = (s & 0xffffffffULL) + (s >> 32);
    return static_cast<uint32_t>(s);
}

// 原始大端字节流（FITS 头卡）→ checksum。
uint32_t datasum_be_bytes(const char* p, size_t nbytes, uint32_t seed = 0) {
    uint64_t s = seed;
    size_t i = 0;
    for (; i + 4 <= nbytes; i += 4) {
        const uint32_t w = (static_cast<uint32_t>(static_cast<unsigned char>(p[i])) << 24) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(p[i + 1])) << 16) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(p[i + 2])) << 8) |
                           static_cast<uint32_t>(static_cast<unsigned char>(p[i + 3]));
        s += w;
    }
    // 尾部不足 4 字节按 FITS 规则补 0；头长恒为 2880 倍数，实际不可达。
    if (i < nbytes) {
        uint32_t w = 0;
        for (int k = 0; i < nbytes; ++i, ++k)
            w |= static_cast<uint32_t>(static_cast<unsigned char>(p[i])) << (24 - 8 * k);
        s += w;
    }
    return fold_ones_complement(s);
}

// 已解码的 FITS 像元缓冲 → 原始大端 32 位字 checksum。
//   BITPIX=-32: 每个 float 一个 32 位字（bit pattern 与磁盘大端字数值相同）;
//   BITPIX=-64: 每个 double 两个 32 位字（高字在前）;
//   BITPIX= 32: 每个 int32 一个 32 位字。
// 只在本 reader 实际读入的 dtype 与 BITPIX 一一对应时调用（read_tile_t 的
// tmp 缓冲满足该前提）；BSCALE/BZERO/BLANK/压缩等转换路径走 cfitsio 校验回退。
uint32_t datasum_typed_buffer(const void* data, size_t nelem, int bitpix) {
    uint64_t s = 0;
    if (bitpix == -32) {
        const float* p = static_cast<const float*>(data);
        for (size_t i = 0; i < nelem; ++i) {
            uint32_t w = 0;
            std::memcpy(&w, p + i, sizeof(w));
            s += w;
        }
    } else if (bitpix == -64) {
        const double* p = static_cast<const double*>(data);
        for (size_t i = 0; i < nelem; ++i) {
            uint64_t bits = 0;
            std::memcpy(&bits, p + i, sizeof(bits));
            s += (bits >> 32) + (bits & 0xffffffffULL);
        }
    } else if (bitpix == 32) {
        const int32_t* p = static_cast<const int32_t*>(data);
        for (size_t i = 0; i < nelem; ++i) {
            uint32_t w = 0;
            std::memcpy(&w, p + i, sizeof(w));
            s += w;
        }
    } else {
        return 0;   // 调用方必须先判可计算性
    }
    return fold_ones_complement(s);
}

// 读原始 FITS 头字节（直到 END 卡 + 2880 对齐）。tile 是单 HDU 文件，头从 0 起。
// 读取失败/找不到 END 时返回 false ⇒ 调用方回退 cfitsio 全 HDU 校验。
bool read_raw_fits_header(const std::string& path, std::string* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.good()) return false;
    out->clear();
    char block[2880];
    for (int b = 0; b < 100; ++b) {   // 8000 张卡上限；tile 头远小于此
        f.read(block, sizeof(block));
        if (f.gcount() != static_cast<std::streamsize>(sizeof(block))) return false;
        out->append(block, sizeof(block));
        for (int c = 0; c < 36; ++c) {
            const char* card = block + c * 80;
            if (card[0] == 'E' && card[1] == 'N' && card[2] == 'D') {
                bool blank = true;
                for (int k = 3; k < 80; ++k) {
                    if (card[k] != ' ') { blank = false; break; }
                }
                if (blank) return true;
            }
        }
    }
    return false;
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
// docs/ACSD_DESIGN.md §10「I/O 与原子产品」:
// **完成清单 fail-closed** —— 「没有完成清单就不算成功对象」。产品集根下的
// manifest.json 由 writer 在**全部 tile 完成之后**最后原子落盘 (aio_hips_finalize);
// 中途 kill / 失败 / 取消只会留下无清单的 tile 残骸。消费者 (aio_hips_open)
// 必须拒绝这类目录, 否则残骸会被当成产品消费。
// 判据: 清单存在且非空 + 含 products 数组 + 声明了本次请求的子产品。
// ---------------------------------------------------------------------------
bool manifest_products_of(const std::string& root, std::string* products,
                          std::string* err, uint64_t* n_leaf = nullptr,
                          bool* has_n_leaf = nullptr) {
    if (n_leaf) *n_leaf = 0;
    if (has_n_leaf) *has_n_leaf = false;
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
    // Issue #87: manifest.n_leaf_tiles 是**覆盖数量声明**（不是 tile 身份）。
    // 它只用于在 MOC 缺失/不完整时把"缺 tile"判成 FOOTPRINT_UNDETERMINED，
    // 绝不据此认定某个具体 tile 合法存在。
    const size_t nk = text.find("\"n_leaf_tiles\"");
    if (nk != std::string::npos) {
        const size_t colon = text.find(':', nk);
        if (colon != std::string::npos) {
            size_t i = colon + 1;
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
            const char* begin = text.c_str() + i;
            char* end = nullptr;
            const unsigned long long v = std::strtoull(begin, &end, 10);
            if (end != begin) {
                if (n_leaf) *n_leaf = static_cast<uint64_t>(v);
                if (has_n_leaf) *has_n_leaf = true;
            }
        }
    }
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
    // Issue #87: 覆盖身份等级（见 aio_hips_reader.h 与 HIPS_IO.md「状态与缺覆盖」）
    enum Footprint {
        FP_NONE = AIO_HIPS_FOOTPRINT_NONE,
        FP_MOC_LEAF = AIO_HIPS_FOOTPRINT_MOC_LEAF,
        FP_MOC_UNDETERMINED = AIO_HIPS_FOOTPRINT_MOC_UNDETERMINED,
        FP_MANIFEST_ONLY = AIO_HIPS_FOOTPRINT_MANIFEST_ONLY
    };
    std::string dir;          // 子产品目录
    int product = 0;
    std::map<std::string, std::string> props;
    int hips_order = 0;
    int tile_width = 512;
    std::vector<uint64_t> tiles;      // 叶级 ipix (MOC 声明的 order==hips_order 单元)
    std::vector<AioHipsSnrPoint2> snr; // 见下方类型
    int data_bitpix = -32;
    size_t bad_snr_rows = 0;          // 损坏 TSV 行计数 (G6 robustness)
    // Issue #87 覆盖身份状态（不参与历史调用方的像素语义）
    Footprint footprint = FP_NONE;
    std::string footprint_detail;
    bool moc_present = false;
    uint64_t moc_entries = 0;
    uint64_t moc_coarse_entries = 0;  // order < hips_order（父级，未归约；#95 独立任务）
    uint64_t moc_extra_entries = 0;   // order > hips_order 或 ipix 越界
};

// NUNIQ -> 阶: uniq ∈ [4·4^o, 4·4^(o+1))。o≤29 保证 64 位移安全。
int uniq_order(uint64_t u) {
    for (int o = 0; o <= 29; ++o) {
        const uint64_t base = 4ULL << (2ULL * (uint64_t)o);
        const uint64_t next = base << 2;
        if (u >= base && u < next) return o;
    }
    return -1;
}

bool declared_contains(const AioHipsDataset* d, uint64_t ipix) {
    return std::binary_search(d->tiles.begin(), d->tiles.end(), ipix);
}

int legacy_code_for(int code) {
    switch (code) {
        case AIO_HIPS_TILE_OUTSIDE_DECLARED_FOOTPRINT: return -2;
        case AIO_HIPS_TILE_DECLARED_MISSING:           return -7;
        case AIO_HIPS_TILE_IO_ERROR:                   return -8;
        case AIO_HIPS_TILE_INVALID:                    return -5;
        case AIO_HIPS_TILE_CHECKSUM_FAILED:            return -9;
        case AIO_HIPS_TILE_FOOTPRINT_UNDETERMINED:     return -10;
        default:                                       return 0;
    }
}

// 缺失 tile 的语义由覆盖身份决定（见头文件注释的判决表）。
int missing_code_for(const AioHipsDataset* d, uint64_t ipix) {
    const bool in = declared_contains(d, ipix);
    switch (d->footprint) {
        case AioHipsDataset::FP_MOC_LEAF:
            return in ? AIO_HIPS_TILE_DECLARED_MISSING
                      : AIO_HIPS_TILE_OUTSIDE_DECLARED_FOOTPRINT;
        case AioHipsDataset::FP_MOC_UNDETERMINED:
            return in ? AIO_HIPS_TILE_DECLARED_MISSING
                      : AIO_HIPS_TILE_FOOTPRINT_UNDETERMINED;
        case AioHipsDataset::FP_MANIFEST_ONLY:
            return AIO_HIPS_TILE_FOOTPRINT_UNDETERMINED;
        case AioHipsDataset::FP_NONE:
        default:
            return AIO_HIPS_TILE_OUTSIDE_DECLARED_FOOTPRINT;
    }
}

std::string cfitsio_msg(int status) {
    char msg[FLEN_ERRMSG] = {0};
    fits_get_errstatus(status, msg);
    return std::string(msg);
}

void fill_status(AioHipsTileStatus* st, AioHipsDataset* d, uint64_t ipix,
                 int code, int legacy, int cfitsio, const std::string& path,
                 const std::string& detail) {
    if (!st) return;
    std::memset(st, 0, sizeof(*st));
    st->code = code;
    st->legacy_code = legacy;
    st->cfitsio_status = cfitsio;
    st->tile_ipix = ipix;
    st->declared = d ? (declared_contains(d, ipix) ? 1 : 0) : -1;
    st->footprint_identity = d ? (int)d->footprint : -1;
    std::snprintf(st->tile_path, sizeof(st->tile_path), "%s", path.c_str());
    std::snprintf(st->detail, sizeof(st->detail), "%s", detail.c_str());
}

// open/读取失败的统一分类：结构损坏 vs 存储 I/O。
int classify_tile_failure(int cfitsio_status, const TileDiskInfo& disk, int* legacy) {
    if (is_structural_cfitsio_status(cfitsio_status) || is_truncated_fits_size(disk) ||
        disk.size == 0) {
        if (legacy) *legacy = -5;
        return AIO_HIPS_TILE_INVALID;
    }
    if (legacy) *legacy = -8;
    return AIO_HIPS_TILE_IO_ERROR;
}

// 打开并校验 tile 的公共前半段（float/double/int32 三入口共用）。
struct TileHandle {
    fitsfile* f = nullptr;
    std::string path;
    TileDiskInfo disk;
    int bitpix = 0, naxis = 0;
    long naxes[2] = {0, 0};
    int code = AIO_HIPS_TILE_OK;
    int legacy = 0;
    int cfitsio = 0;
    std::string detail;
};

TileHandle open_tile_common(AioHipsDataset* d, uint64_t ipix, bool want_i32) {
    TileHandle h;
    h.path = tile_path_resolve(d->dir, d->hips_order, ipix, ".fits");
    h.disk = tile_disk_info(h.path);
    if (!h.disk.exists) {
        h.code = missing_code_for(d, ipix);
        h.legacy = legacy_code_for(h.code);
        h.detail = (h.code == AIO_HIPS_TILE_DECLARED_MISSING)
                       ? "tile declared by coverage hint but file is missing"
                   : (h.code == AIO_HIPS_TILE_FOOTPRINT_UNDETERMINED)
                       ? "tile missing and coverage identity is insufficient"
                       : "tile absent outside declared footprint";
        return h;
    }
    if (h.disk.is_dir) {
        h.code = AIO_HIPS_TILE_IO_ERROR;
        h.legacy = -8;
        h.detail = "tile path exists but is a directory (not a regular FITS file)";
        return h;
    }
    int status = 0;
    if (fits_open_file(&h.f, h.path.c_str(), READONLY, &status)) {
        h.cfitsio = status;
        h.code = classify_tile_failure(status, h.disk, &h.legacy);
        h.detail = "fits_open_file: " + cfitsio_msg(status) +
                   " (status " + std::to_string(status) + ")";
        fits_clear_errmsg();
        h.f = nullptr;
        return h;
    }
    status = 0;
    if (fits_get_img_param(h.f, 2, &h.bitpix, &h.naxis, h.naxes, &status)) {
        h.cfitsio = status;
        h.code = classify_tile_failure(status, h.disk, &h.legacy);
        if (h.code == AIO_HIPS_TILE_INVALID) h.legacy = -3;
        h.detail = "fits_get_img_param: " + cfitsio_msg(status) +
                   " (status " + std::to_string(status) + ")";
        fits_clear_errmsg();
        int s = 0;
        fits_close_file(h.f, &s);
        h.f = nullptr;
        return h;
    }
    if (h.naxis != 2 || h.naxes[0] != d->tile_width || h.naxes[1] != d->tile_width) {
        h.code = AIO_HIPS_TILE_INVALID;
        h.legacy = -4;
        h.detail = "tile geometry invalid: NAXIS=" + std::to_string(h.naxis) +
                   " NAXIS1=" + std::to_string(h.naxes[0]) +
                   " NAXIS2=" + std::to_string(h.naxes[1]) +
                   " (required 2 x " + std::to_string(d->tile_width) + " x " +
                   std::to_string(d->tile_width) + ")";
        int s = 0;
        fits_close_file(h.f, &s);
        h.f = nullptr;
        return h;
    }
    if (want_i32) {
        if (h.bitpix != 32) {
            h.code = AIO_HIPS_TILE_INVALID;
            h.legacy = -6;
            h.detail = "diagnostic tile BITPIX must be 32 (int32), got " +
                       std::to_string(h.bitpix);
            int s = 0;
            fits_close_file(h.f, &s);
            h.f = nullptr;
        }
    } else if (h.bitpix != -32 && h.bitpix != -64) {
        h.code = AIO_HIPS_TILE_INVALID;
        h.legacy = -6;
        h.detail = "tile BITPIX must be -32/-64, got " + std::to_string(h.bitpix);
        int s = 0;
        fits_close_file(h.f, &s);
        h.f = nullptr;
    }
    return h;
}

// FITS CHECKSUM/DATASUM 校验。共同路径从"已经读入的原始像元缓冲 + 原始头字节"
// 直接算 1's-complement 校验和，避免为校验再读一遍 1 MiB 数据；只在
// BSCALE/BZERO/BLANK/压缩/特殊 BITPIX 等会改变原始字节语义的情况下回退
// cfitsio fits_verify_chksum（显式全 HDU 校验）。
struct ChecksumOutcome {
    bool ok = true;
    bool present = false;
    int code = 0;          // 非 ok 时为 AIO_HIPS_TILE_*
    int legacy = 0;
    int cfitsio = 0;
    std::string detail;
};

bool has_transform_keywords(fitsfile* f) {
    int any = 0;
    auto check = [&](const char* key, bool* present) {
        double v = 0.0;
        int st = 0;
        if (fits_read_key(f, TDOUBLE, key, &v, nullptr, &st) == 0) {
            *present = true;
            ++any;
            return v;
        }
        fits_clear_errmsg();
        return 0.0;
    };
    bool bs = false, bz = false, bl = false;
    const double bscale = check("BSCALE", &bs);
    const double bzero = check("BZERO", &bz);
    check("BLANK", &bl);
    if (bs && bscale != 1.0) return true;
    if (bz && bzero != 0.0) return true;
    if (bl) return true;
    (void)any;
    return false;
}

ChecksumOutcome verify_tile_checksum(fitsfile* f, const std::string& path,
                                     const void* data, long nelem, int bitpix) {
    ChecksumOutcome out;
    char dsum[FLEN_VALUE] = {0};
    char cksum[FLEN_VALUE] = {0};
    int dstate = 0, cstate = 0;   // 1 present, 0 absent, -1 read error
    int st = 0;
    if (fits_read_key(f, TSTRING, "DATASUM", dsum, nullptr, &st) == 0) dstate = 1;
    else if (st == KEY_NO_EXIST) { dstate = 0; fits_clear_errmsg(); }
    else dstate = -1;
    st = 0;
    if (fits_read_key(f, TSTRING, "CHECKSUM", cksum, nullptr, &st) == 0) cstate = 1;
    else if (st == KEY_NO_EXIST) { cstate = 0; fits_clear_errmsg(); }
    else cstate = -1;
    auto blank = [](const char* s) {
        for (; s && *s; ++s)
            if (!std::isspace(static_cast<unsigned char>(*s))) return false;
        return true;
    };
    if (dstate == 1 && blank(dsum)) dstate = 0;
    if (cstate == 1 && blank(cksum)) cstate = 0;
    out.present = (dstate != 0) || (cstate != 0);
    if (!out.present && dstate == 0 && cstate == 0) return out;   // 明确规则: 缺校验字段不失败

    // 只有 DATASUM/CHECKSUM 都在常规可计算域内才走单次读取路径。
    const bool computable = (bitpix == -32 || bitpix == -64 || bitpix == 32) &&
                            !has_transform_keywords(f);
    if (computable && dstate >= 0 && cstate >= 0) {
        std::string hdr;
        if (read_raw_fits_header(path, &hdr)) {
            const uint32_t hsum = datasum_be_bytes(hdr.data(), hdr.size());
            const uint32_t payload = datasum_typed_buffer(data, (size_t)nelem, bitpix);
            const uint32_t total = fold_ones_complement((uint64_t)hsum + payload);
            bool bad = false;
            if (cstate == 1 && total != 0u && total != 0xffffffffu) bad = true;
            if (dstate == 1) {
                char* end = nullptr;
                const unsigned long long declared = std::strtoull(dsum, &end, 10);
                if (end == dsum) {
                    bad = true;   // DATASUM 存在但不可解析 = 校验声明损坏
                } else if (static_cast<uint32_t>(declared) != payload) {
                    bad = true;
                }
            }
            if (bad) {
                out.ok = false;
                out.code = AIO_HIPS_TILE_CHECKSUM_FAILED;
                out.legacy = -9;
                out.detail = "FITS checksum mismatch (DATASUM/CHECKSUM present)";
            }
            return out;
        }
    }

    // 回退：cfitsio 全 HDU 校验（会再读数据；只在少见变换/压缩路径发生）。
    int dataok = -9, hduok = -9;
    st = 0;
    if (fits_verify_chksum(f, &dataok, &hduok, &st)) {
        out.cfitsio = st;
        out.ok = false;
        out.code = classify_tile_failure(st, TileDiskInfo{true, false, 0, 0}, &out.legacy);
        out.detail = "fits_verify_chksum failed: " + cfitsio_msg(st) +
                     " (status " + std::to_string(st) + ")";
        fits_clear_errmsg();
        return out;
    }
    if (dataok == -1 || hduok == -1) {
        out.ok = false;
        out.code = AIO_HIPS_TILE_CHECKSUM_FAILED;
        out.legacy = -9;
        out.detail = "FITS checksum verification failed (dataok=" +
                     std::to_string(dataok) + ", hduok=" + std::to_string(hduok) + ")";
    }
    return out;
}

// 三入口共用的读取后半段（数据读入 + 校验）。返回 0 或 AIO_HIPS_TILE_*。
int read_payload(fitsfile* f, const std::string& path, void* buf, long nelem,
                 int bitpix, const TileDiskInfo& disk, AioHipsTileStatus* st,
                 int* legacy_out, std::string* detail) {
    int status = 0;
    long fpixel[2] = {1, 1};
    const int dtype = (bitpix == -64) ? TDOUBLE : (bitpix == 32) ? TINT : TFLOAT;
    if (fits_read_pix(f, dtype, fpixel, nelem, nullptr, buf, nullptr, &status)) {
        fits_clear_errmsg();
        int legacy = 0;
        const int code = classify_tile_failure(status, disk, &legacy);
        if (st) {
            st->cfitsio_status = status;
            st->code = code;
            st->legacy_code = legacy;
        }
        if (legacy_out) *legacy_out = legacy;
        if (detail)
            *detail = "fits_read_pix: " + cfitsio_msg(status) +
                      " (status " + std::to_string(status) + ")";
        return code;
    }
    const ChecksumOutcome ck = verify_tile_checksum(f, path, buf, nelem, bitpix);
    if (st) st->checksum_present = ck.present ? 1 : 0;
    if (!ck.ok) {
        if (st) {
            st->cfitsio_status = ck.cfitsio;
            st->code = ck.code;
            st->legacy_code = ck.legacy;
        }
        if (legacy_out) *legacy_out = ck.legacy;
        if (detail) *detail = ck.detail;
        return ck.code;
    }
    return AIO_HIPS_TILE_OK;
}

// PERF-401: 读路径不再取进程级串行化锁（详见头注释）。
template <typename T>
static int read_tile_t_ex(AioHipsDataset* d, uint64_t ipix, T* out, AioHipsTileStatus* st) {
    if (!d || !out) {
        if (st) std::memset(st, 0, sizeof(*st));
        set_err("tile 读取参数无效");
        return -1;
    }
    TileHandle h = open_tile_common(d, ipix, false);
    if (h.code != AIO_HIPS_TILE_OK) {
        set_err(h.detail + " [" + h.path + "]");
        fill_status(st, d, ipix, h.code, h.legacy, h.cfitsio, h.path, h.detail);
        return h.code;
    }
    const long nelem = h.naxes[0] * h.naxes[1];
    int legacy = 0;
    std::string detail;
    int code = AIO_HIPS_TILE_OK;
    if (h.bitpix == -32) {
        std::vector<float> tmp((size_t)nelem);
        code = read_payload(h.f, h.path, tmp.data(), nelem, h.bitpix, h.disk, st,
                            &legacy, &detail);
        if (code == AIO_HIPS_TILE_OK)
            for (long i = 0; i < nelem; ++i) out[i] = (T)tmp[(size_t)i];
    } else {
        std::vector<double> tmp((size_t)nelem);
        code = read_payload(h.f, h.path, tmp.data(), nelem, h.bitpix, h.disk, st,
                            &legacy, &detail);
        if (code == AIO_HIPS_TILE_OK)
            for (long i = 0; i < nelem; ++i) out[i] = (T)tmp[(size_t)i];
    }
    int s = 0;
    fits_close_file(h.f, &s);
    h.f = nullptr;
    if (code != AIO_HIPS_TILE_OK) {
        set_err(detail + " [" + h.path + "]");
        fill_status(st, d, ipix, code, legacy, st ? st->cfitsio_status : 0, h.path, detail);
        return code;
    }
    const int checksum_present = st ? st->checksum_present : 0;
    fill_status(st, d, ipix, AIO_HIPS_TILE_OK, 0, 0, h.path, "ok");
    if (st) st->checksum_present = checksum_present;
    return AIO_HIPS_TILE_OK;
}

// DATA-UNC-001 §30.2: 诊断统计平面回读 (int32 专用通道)。
static int read_tile_i32_ex(AioHipsDataset* d, uint64_t ipix, int32_t* out,
                            AioHipsTileStatus* st) {
    if (!d || !out) {
        if (st) std::memset(st, 0, sizeof(*st));
        set_err("tile 读取参数无效");
        return -1;
    }
    TileHandle h = open_tile_common(d, ipix, true);
    if (h.code != AIO_HIPS_TILE_OK) {
        set_err(h.detail + " [" + h.path + "]");
        fill_status(st, d, ipix, h.code, h.legacy, h.cfitsio, h.path, h.detail);
        return h.code;
    }
    const long nelem = h.naxes[0] * h.naxes[1];
    std::vector<int32_t> tmp((size_t)nelem);
    int legacy = 0;
    std::string detail;
    const int code = read_payload(h.f, h.path, tmp.data(), nelem, h.bitpix, h.disk, st,
                                  &legacy, &detail);
    int s = 0;
    fits_close_file(h.f, &s);
    h.f = nullptr;
    if (code != AIO_HIPS_TILE_OK) {
        set_err(detail + " [" + h.path + "]");
        fill_status(st, d, ipix, code, legacy, st ? st->cfitsio_status : 0, h.path, detail);
        return code;
    }
    for (long i = 0; i < nelem; ++i) out[i] = tmp[(size_t)i];
    const int checksum_present = st ? st->checksum_present : 0;
    fill_status(st, d, ipix, AIO_HIPS_TILE_OK, 0, 0, h.path, "ok");
    if (st) st->checksum_present = checksum_present;
    return AIO_HIPS_TILE_OK;
}

namespace {

// 从 MOC FITS 提取叶级 ipix (order == hips_order 的 UNIQ -> ipix)，并判定
// 覆盖身份等级。低阶(父级)条目**不归约**（#95 独立任务），而是把身份降为
// MOC_UNDETERMINED：集合外缺失不得冒充合法空洞。
enum class MocLoad { NoMoc, Loaded, Error };
MocLoad load_tiles_from_moc(AioHipsDataset* d, std::string* why) {
    const std::string moc = d->dir + "/Moc.fits";
    const TileDiskInfo di = tile_disk_info(moc);
    if (!di.exists) return MocLoad::NoMoc;
    d->moc_present = true;
    int status = 0;
    fitsfile* fptr = nullptr;
    if (fits_open_file(&fptr, moc.c_str(), READONLY, &status)) {
        if (why) *why = "Moc.fits open failed: " + cfitsio_msg(status) +
                        " (status " + std::to_string(status) + ")";
        fits_clear_errmsg();
        return MocLoad::Error;
    }
    // MOC 数据在扩展 HDU (PRIMARY 为空图头)
    if (fits_movabs_hdu(fptr, 2, nullptr, &status)) {
        if (why) *why = "Moc.fits has no BINTABLE extension HDU 2 (corrupt MOC)";
        fits_clear_errmsg();
        int s = 0;
        fits_close_file(fptr, &s);
        return MocLoad::Error;
    }
    long nrows = 0;
    if (fits_get_num_rows(fptr, &nrows, &status)) {
        if (why) *why = "moc rows: fits_get_num_rows failed";
        int s = 0;
        fits_close_file(fptr, &s);
        return MocLoad::Error;
    }
    // P2 (R9-A): MOC 行数上限（恶意/损坏 NAXIS2 不得打爆内存）。
    if (nrows > 1000000) {
        if (why) *why = "moc rows 超上限 (nrows=" + std::to_string(nrows) + ")";
        int s = 0;
        fits_close_file(fptr, &s);
        return MocLoad::Error;
    }
    std::vector<long long> uniq((size_t)nrows);
    if (nrows > 0 && fits_read_col(fptr, TLONGLONG, 1, 1, 1, nrows, nullptr,
                                   uniq.data(), nullptr, &status)) {
        if (why) *why = "moc read: fits_read_col failed: " + cfitsio_msg(status);
        int s = 0;
        fits_close_file(fptr, &s);
        return MocLoad::Error;
    }
    int s = 0;
    fits_close_file(fptr, &s);
    d->moc_entries = (uint64_t)nrows;
    const uint64_t leaf_count = 12ULL * (1ULL << (2ULL * (uint64_t)d->hips_order));
    for (long long u : uniq) {
        const int order = (u >= 0) ? uniq_order((uint64_t)u) : -1;
        if (order < 0) { ++d->moc_extra_entries; continue; }
        if (order < d->hips_order) { ++d->moc_coarse_entries; continue; }
        if (order > d->hips_order) { ++d->moc_extra_entries; continue; }
        const uint64_t base = 4ULL * (1ULL << (2ULL * (uint64_t)d->hips_order));
        const uint64_t ipix = (uint64_t)u - base;
        if (ipix < leaf_count) d->tiles.push_back(ipix);
        else ++d->moc_extra_entries;
    }
    std::sort(d->tiles.begin(), d->tiles.end());
    d->tiles.erase(std::unique(d->tiles.begin(), d->tiles.end()), d->tiles.end());
    if (d->moc_coarse_entries > 0 || d->moc_extra_entries > 0) {
        d->footprint = AioHipsDataset::FP_MOC_UNDETERMINED;
        d->footprint_detail =
            "Moc.fits contains non-leaf entries (coarse=" +
            std::to_string(d->moc_coarse_entries) + ", unexpected=" +
            std::to_string(d->moc_extra_entries) +
            "); parent-level coverage reduction is out of scope (#95)";
    } else {
        d->footprint = AioHipsDataset::FP_MOC_LEAF;
        d->footprint_detail = "Moc.fits enumerates " + std::to_string(d->tiles.size()) +
                              " leaf cells at order " + std::to_string(d->hips_order);
    }
    return MocLoad::Loaded;
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
    const TileDiskInfo disk = tile_disk_info(p);
    if (!disk.exists) {
        const int code = missing_code_for(d, ipix);
        set_err("tile missing: " + p + " (" + d->footprint_detail + ")");
        return legacy_code_for(code);
    }
    if (disk.is_dir) {
        set_err("tile path is a directory: " + p);
        return -8;
    }
    int status = 0;
    fitsfile* fptr = nullptr;
    if (fits_open_file(&fptr, p.c_str(), READONLY, &status)) {
        int legacy = 0;
        classify_tile_failure(status, disk, &legacy);
        set_err("tile open: " + cfitsio_msg(status) + " [" + p + "]");
        fits_clear_errmsg();
        return legacy;
    }
    int bitpix = 0, naxis = 0;
    long naxes[2] = {0, 0};
    status = 0;
    if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
        int legacy = 0;
        classify_tile_failure(status, disk, &legacy);
        if (legacy == -5) legacy = -3;
        set_err("tile param: " + cfitsio_msg(status) + " [" + p + "]");
        fits_clear_errmsg();
        int s = 0;
        fits_close_file(fptr, &s);
        return legacy;
    }
    if (naxis != 2 || naxes[0] != d->tile_width || naxes[1] != d->tile_width) {
        set_err("tile 尺寸非法");
        int s = 0;
        fits_close_file(fptr, &s);
        return -4;
    }
    const long w = (long)d->tile_width;
    long fpixel[2] = {(long)(fits_index % (uint64_t)w) + 1,
                      (long)(fits_index / (uint64_t)w) + 1};
    // 单像素路径不解 DATASUM/CHECKSUM（PERF-401 的既有契约；Stage3 export 走
    // 整 tile 入口并做完整校验）。叶级单点读取只做结构/存在性分类。
    if (bitpix == -32) {
        float v = 0.0f;
        status = 0;
        if (fits_read_pix(fptr, TFLOAT, fpixel, 1, nullptr, &v, nullptr, &status)) {
            int legacy = 0;
            classify_tile_failure(status, disk, &legacy);
            set_err("tile pixel read: " + cfitsio_msg(status) + " [" + p + "]");
            fits_clear_errmsg();
            int s = 0;
            fits_close_file(fptr, &s);
            return legacy;
        }
        *out = (T)v;
    } else if (bitpix == -64) {
        double v = 0.0;
        status = 0;
        if (fits_read_pix(fptr, TDOUBLE, fpixel, 1, nullptr, &v, nullptr, &status)) {
            int legacy = 0;
            classify_tile_failure(status, disk, &legacy);
            set_err("tile pixel read: " + cfitsio_msg(status) + " [" + p + "]");
            fits_clear_errmsg();
            int s = 0;
            fits_close_file(fptr, &s);
            return legacy;
        }
        *out = (T)v;
    } else {
        set_err("tile BITPIX 非 -32/-64");
        int s = 0;
        fits_close_file(fptr, &s);
        return -6;
    }
    int s = 0;
    fits_close_file(fptr, &s);
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
    const uint64_t fi = acsd::healpix::nested_local_to_fits_index(z, 9u, 512u);
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
        // §10: 完成清单 fail-closed (先于任何子产品内容读取)。
        // Issue #87: 同时取 manifest.n_leaf_tiles 作为**覆盖数量声明**（不是身份）。
        uint64_t manifest_n_leaf = 0;
        bool manifest_has_n_leaf = false;
        {
            std::string products, merr;
            if (!manifest_products_of(out_dir, &products, &merr, &manifest_n_leaf,
                                      &manifest_has_n_leaf)) {
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
        // 合法（HiPS MOC 是可选枚举提示，见 HIPS_IO §3）。
        // Issue #87: 覆盖身份分级——MOC 叶级完整枚举 = MOC_LEAF；含低阶/非法阶
        // 条目 = MOC_UNDETERMINED（不冒充合法空洞）；无 MOC 但有 manifest 覆盖
        // 计数 = MANIFEST_ONLY；都没有 = NONE。
        {
            std::string moc_why;
            const MocLoad ml = load_tiles_from_moc(d.get(), &moc_why);
            if (ml == MocLoad::Error) {
                set_err("MOC 读取失败: " + d->dir + " (" + moc_why + ")");
                return nullptr;
            }
            if (ml == MocLoad::NoMoc) {
                if (manifest_has_n_leaf && manifest_n_leaf > 0) {
                    d->footprint = AioHipsDataset::FP_MANIFEST_ONLY;
                    d->footprint_detail =
                        "manifest.json declares n_leaf_tiles=" +
                        std::to_string(manifest_n_leaf) +
                        " but no Moc.fits tile identity exists";
                } else {
                    d->footprint = AioHipsDataset::FP_NONE;
                    d->footprint_detail =
                        "no Moc.fits and no manifest coverage count; product "
                        "declares no enumerated coverage (MOC is an optional hint)";
                }
            } else if (d->footprint == AioHipsDataset::FP_MOC_LEAF &&
                       manifest_has_n_leaf && manifest_n_leaf > d->tiles.size()) {
                // 覆盖数量声明大于 MOC 叶级条目 = 声明不完整；集合外缺失不得
                // 被当成合法空洞（可能是未枚举的已声明 tile）。
                d->footprint = AioHipsDataset::FP_MOC_UNDETERMINED;
                d->footprint_detail =
                    "manifest.json n_leaf_tiles=" + std::to_string(manifest_n_leaf) +
                    " exceeds MOC leaf entries=" + std::to_string(d->tiles.size()) +
                    "; coverage identity is incomplete";
            }
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
        AioHipsTileStatus st{};
        const int code = read_tile_t_ex(d, ipix, out, &st);
        if (code <= 0) return code;   // 0=OK, -1=参数/异常
        return st.legacy_code;
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
        AioHipsTileStatus st{};
        const int code = read_tile_t_ex(d, ipix, out, &st);
        if (code <= 0) return code;
        return st.legacy_code;
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
        AioHipsTileStatus st{};
        const int code = read_tile_i32_ex(d, ipix, out, &st);
        if (code <= 0) return code;
        return st.legacy_code;
    }
    catch (const std::exception &e) {
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_read_tile_f32_ex(AioHipsDataset* d, uint64_t ipix, float* out,
                              AioHipsTileStatus* st)  {
    try {
        return read_tile_t_ex(d, ipix, out, st);
    } catch (const std::exception &e) {
        if (st) {
            std::memset(st, 0, sizeof(*st));
            st->code = AIO_HIPS_TILE_IO_ERROR;
            st->legacy_code = -1;
            std::snprintf(st->detail, sizeof(st->detail), "exception: %s", e.what());
        }
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        if (st) {
            std::memset(st, 0, sizeof(*st));
            st->code = AIO_HIPS_TILE_IO_ERROR;
            st->legacy_code = -1;
            std::snprintf(st->detail, sizeof(st->detail), "unknown exception");
        }
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_read_tile_f64_ex(AioHipsDataset* d, uint64_t ipix, double* out,
                              AioHipsTileStatus* st)  {
    try {
        return read_tile_t_ex(d, ipix, out, st);
    } catch (const std::exception &e) {
        if (st) {
            std::memset(st, 0, sizeof(*st));
            st->code = AIO_HIPS_TILE_IO_ERROR;
            st->legacy_code = -1;
            std::snprintf(st->detail, sizeof(st->detail), "exception: %s", e.what());
        }
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        if (st) {
            std::memset(st, 0, sizeof(*st));
            st->code = AIO_HIPS_TILE_IO_ERROR;
            st->legacy_code = -1;
            std::snprintf(st->detail, sizeof(st->detail), "unknown exception");
        }
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_read_tile_i32_ex(AioHipsDataset* d, uint64_t ipix, int32_t* out,
                              AioHipsTileStatus* st)  {
    try {
        return read_tile_i32_ex(d, ipix, out, st);
    } catch (const std::exception &e) {
        if (st) {
            std::memset(st, 0, sizeof(*st));
            st->code = AIO_HIPS_TILE_IO_ERROR;
            st->legacy_code = -1;
            std::snprintf(st->detail, sizeof(st->detail), "exception: %s", e.what());
        }
        set_err(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        if (st) {
            std::memset(st, 0, sizeof(*st));
            st->code = AIO_HIPS_TILE_IO_ERROR;
            st->legacy_code = -1;
            std::snprintf(st->detail, sizeof(st->detail), "unknown exception");
        }
        set_err("unknown exception");
        return -1;
    }
}

int aio_hips_footprint_identity(AioHipsDataset* d)  {
    try {
        return d ? (int)d->footprint : -1;
    } catch (...) {
        return -1;
    }
}

int aio_hips_declared_tile_count(AioHipsDataset* d)  {
    try {
        return d ? (int)d->tiles.size() : -1;
    } catch (...) {
        return -1;
    }
}

int aio_hips_tile_declared(AioHipsDataset* d, uint64_t ipix)  {
    try {
        if (!d) return -1;
        if (d->footprint == AioHipsDataset::FP_MANIFEST_ONLY ||
            d->footprint == AioHipsDataset::FP_NONE)
            return -1;   // 无 tile 身份可判
        return declared_contains(d, ipix) ? 1 : 0;
    } catch (...) {
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
