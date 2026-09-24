// ============================================================================
// aio_hips_writer.cpp - IVOA HiPS 1.4 生产链写入器 (Phase1 Final Closure)
//
// 数据流 (无 HISS 中转):
// Drizzle TileAccumulator -> AstroSphereTileView -> 本写入器 (流式)
//
// 输出结构 (Product Set):
// <out_dir>/
// manifest.json
// signal/ Image HiPS: signal = flux_sum/covered_area (float32/64)
// support/ Image HiPS: support = covered_area/A_cell (float32/64)
// snr/ Catalogue HiPS: NorderK/DirD/NpixN.tsv
// 每个子产品独立 properties / Moc.fits / 低阶 hierarchy tiles。
//
// FITS 全部由 vendored CFITSIO 4.6.4 写入 (含 DATASUM/CHECKSUM)。
// ============================================================================

#include "aio_hips.h"
#include "aio_hips_reader.h"   // DATA-UNC-001 §30.2/§30.3: verify 面回读 (只读)
#include "aio_atomic_file.h"   // AIO-001: 临时文件+fsync+原子 rename 落盘原语
#include "aio_disk_full.h"     // 磁盘满/配额失败的失败瞬间分类
#include "aio_sparse_punch.h"  // 裸形态体积削减: 文件系统打洞 (合同 §7 T1)
#include "healpix/healpix_core.h"

#include <fitsio.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <cerrno>
#include <functional>
#include <limits>
#include <thread>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace {

thread_local std::string g_hips_error;

void set_error(const std::string& msg) { g_hips_error = msg; }

// ---------------------------------------------------------------------------
// C 边界 ABI 前置校验 (ASTROCS_DESIGN §7.3 / ENGINEERING_SPEC §1: 版本化 C ABI;
// V11-N-01): struct_size/abi_version 必须与调用方编译期布局逐字段一致, 不一致
// 即 fail-closed (返回 AIO_HIPS_ABI_MISMATCH) + 结构化诊断 —— 永远不按盲步长
// 读取调用方缓冲区。数组形态 (snr_points/tiles) 由调用方逐元素校验。
// ---------------------------------------------------------------------------
bool abi_ok_tile_view(const AstroSphereTileView* v) {
    if (v->struct_size == (uint32_t)sizeof(AstroSphereTileView) &&
        v->abi_version == (uint32_t)AIO_HIPS_TILE_VIEW_ABI_VERSION)
        return true;
    set_error("AstroSphereTileView ABI 不匹配 (caller struct_size=" +
              std::to_string(v->struct_size) + " abi_version=" +
              std::to_string(v->abi_version) + ", expected struct_size=" +
              std::to_string((unsigned)sizeof(AstroSphereTileView)) +
              " abi_version=" + std::to_string((unsigned)AIO_HIPS_TILE_VIEW_ABI_VERSION) +
              "); 请重新编译调用方 / 同步 aio_abi_mirror.py");
    return false;
}

bool abi_ok_diag_view(const AioHipsDiagTileView* v) {
    if (v->struct_size == (uint32_t)sizeof(AioHipsDiagTileView) &&
        v->abi_version == (uint32_t)AIO_HIPS_DIAG_TILE_VIEW_ABI_VERSION)
        return true;
    set_error("AioHipsDiagTileView ABI 不匹配 (caller struct_size=" +
              std::to_string(v->struct_size) + " abi_version=" +
              std::to_string(v->abi_version) + ", expected struct_size=" +
              std::to_string((unsigned)sizeof(AioHipsDiagTileView)) +
              " abi_version=" + std::to_string((unsigned)AIO_HIPS_DIAG_TILE_VIEW_ABI_VERSION) +
              "); 请重新编译调用方 / 同步 aio_abi_mirror.py");
    return false;
}

bool abi_ok_snr_point(const AioHipsSnrPoint* p, int idx) {
    if (p->struct_size == (uint32_t)sizeof(AioHipsSnrPoint) &&
        p->abi_version == (uint32_t)AIO_HIPS_SNR_POINT_ABI_VERSION)
        return true;
    set_error("AioHipsSnrPoint[" + std::to_string(idx) + "] ABI 不匹配 (caller "
              "struct_size=" + std::to_string(p->struct_size) + " abi_version=" +
              std::to_string(p->abi_version) + ", expected struct_size=" +
              std::to_string((unsigned)sizeof(AioHipsSnrPoint)) +
              " abi_version=" + std::to_string((unsigned)AIO_HIPS_SNR_POINT_ABI_VERSION) +
              "); 步长不得错位 (重新编译调用方 / 同步 aio_abi_mirror.py)");
    return false;
}

// 输出结构 (调用方分配、库写入) 的 ABI 前置校验: 必须先于任何产品级检查与写入,
// 否则 (a) 会按库自身布局盲写调用方缓冲区, (b) ABI 不匹配会被 out_dir 检查的
// "-1 参数无效" 掩盖 (诊断不点名 ABI)。
bool abi_ok_verify_report(const AioHipsVerifyReport* r) {
    if (r->struct_size == (uint32_t)sizeof(AioHipsVerifyReport) &&
        r->abi_version == (uint32_t)AIO_HIPS_VERIFY_REPORT_ABI_VERSION)
        return true;
    set_error("AioHipsVerifyReport ABI 不匹配 (caller struct_size=" +
              std::to_string(r->struct_size) + " abi_version=" +
              std::to_string(r->abi_version) + ", expected struct_size=" +
              std::to_string((unsigned)sizeof(AioHipsVerifyReport)) +
              " abi_version=" +
              std::to_string((unsigned)AIO_HIPS_VERIFY_REPORT_ABI_VERSION) +
              "); 请重新编译调用方 / 同步 aio_abi_mirror.py");
    return false;
}

bool abi_ok_legacy_tile(const AioHipsTile* t, int idx) {
    if (t->struct_size == (uint32_t)sizeof(AioHipsTile) &&
        t->abi_version == (uint32_t)AIO_HIPS_TILE_ABI_VERSION)
        return true;
    set_error("AioHipsTile[" + std::to_string(idx) + "] ABI 不匹配 (caller "
              "struct_size=" + std::to_string(t->struct_size) + " abi_version=" +
              std::to_string(t->abi_version) + ", expected struct_size=" +
              std::to_string((unsigned)sizeof(AioHipsTile)) + " abi_version=" +
              std::to_string((unsigned)AIO_HIPS_TILE_ABI_VERSION) + ")");
    return false;
}

// 故障注入 (ASTROCS_HIPS_*): 测试专用等价缺陷注入面。未设置环境变量时
// 逐行零行为差异; 命中时按注入名产生等价缺陷, 使对应断言必败 (判别力证明)。
// 先例: eng/tests/unit/aio_abi_test_main.hpp ASTROCS_AIO_FAULT / p2002 FAULT=proj|prov。
// (原定义在句柄结构之后; AIO-001 起 write_properties 需要同一注入面, 故上移至此。)
bool fault_injected(const char* var, const char* name) {
    const char* v = std::getenv(var);
    return v && name && std::strcmp(v, name) == 0;
}

double kPi() { return std::acos(-1.0); }

// 解析 ISO-8601 "YYYY-MM-DDTHH:MM:SS"（可含小数秒）为 MJD（天文惯例，DATE-OBS 视为 UTC）。
// 日期换算采用固定纪元算法，不依赖本地时区；解析失败返回 false。
static int64_t civil_to_days(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static bool iso_to_mjd(const std::string& iso, double& mjd) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    double s = 0.0;
    if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%lf", &y, &mo, &d, &h, &mi, &s) != 6)
        return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59)
        return false;
    const int64_t days = civil_to_days(y, (unsigned)mo, (unsigned)d);
    mjd = (double)days + 40587.0
        + ((double)h * 3600.0 + (double)mi * 60.0 + s) / 86400.0;
    return true;
}


// 当前 UTC 时间 (finalize 时生成, 禁止硬编码日期)
void utc_now_iso(char* buf, size_t n) {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void utc_now_date(char* buf, size_t n) {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::snprintf(buf, n, "%04d-%02d-%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}
uint32_t ilog2_u64(uint64_t v) {
    uint32_t l = 0;
    while (v > 1) { v >>= 1; ++l; }
    return l;
}

void make_dirs(const std::string& path) {
    std::string norm = path;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    std::string cur;
    for (size_t i = 0; i <= norm.size(); ++i) {
        char c = (i < norm.size()) ? norm[i] : '/';
        if (c == '/' || i == norm.size()) {
            // 去掉段尾空白 (Windows 路径含空格时 _mkdir 会失败)
            while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t'))
                cur.pop_back();
            // 跳过空段与盘符前缀 (如 "F:")
            if (!cur.empty() && !(cur.size() == 2 && cur[1] == ':')) {
#ifdef _WIN32
                _mkdir(cur.c_str());
#else
                mkdir(cur.c_str(), 0755);
#endif
            }
            if (i < norm.size()) cur += '/';
            continue;
        }
        cur += c;
    }
}

// IVOA REC-HIPS-1.0 §4.1: order K 的 tile N 位于 NorderK/DirD/NpixN.fits,
// 其中 D = (N/10000)*10000 (10000 块起始值, 不是商), 文件名 Npix 带**完整**
// tile 号 (标准示例: 10302@order6 -> Dir10000/Npix10302.fits)。
// 旧实现写 Dir=(N/10000), Npix=(N%10000), 与标准相反 (M2b-B-01)。
std::string tile_rel_path(int order, uint64_t ipix, const char* ext) {
    uint64_t dir = (ipix / 10000u) * 10000u;
    char buf[512];
    std::snprintf(buf, sizeof(buf), "Norder%d/Dir%llu/Npix%llu%s",
                  order, (unsigned long long)dir, (unsigned long long)ipix, ext);
    return std::string(buf);
}

// 旧版非标准布局 (Dir=商, Npix=余数) 的**只读**兼容路径; 仅当标准路径不存在
// 时回退, 使既有产品仍可读, 但新产物一律按标准落盘。
std::string tile_rel_path_legacy(int order, uint64_t ipix, const char* ext) {
    uint64_t dir = ipix / 10000u;
    uint64_t npix = ipix % 10000u;
    char buf[512];
    std::snprintf(buf, sizeof(buf), "Norder%d/Dir%llu/Npix%llu%s",
                  order, (unsigned long long)dir, (unsigned long long)npix, ext);
    return std::string(buf);
}

// FITS 字符串转义 (单引号翻倍, 80 字符内)
std::string fits_str(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    if (out.size() > 68) out = out.substr(0, 68);
    return out;
}

// ---------------------------------------------------------------------------
// CFITSIO 错误辅助
// ---------------------------------------------------------------------------
bool fits_ok(int status, const std::string& where) {
    if (status == 0) return true;
    char msg[FLEN_ERRMSG];
    msg[0] = '\0';
    fits_get_errstatus(status, msg);
    set_error(where + ": " + msg);
    return false;
}

// ---------------------------------------------------------------------------
// DATA-UNC-001 §30.2 / HIPS_WRITER §7 确定性: CFITSIO fits_write_chksum(ffpcks)
// 会把当前 UTC 秒写进 CHECKSUM/DATASUM 卡注释 ("... updated YYYY-MM-DDThh:mm:ss")。
// 注释是 80 字符卡内容并参与同一 1's complement 校验和 → 同一输入跨秒两次落盘
// 的 FITS 逐字节不同 (RESCUE-FD-03: p1hips DP-U6 双跑 bitwise 40 次实测 1-4 次
// 红; 差异字节仅落在两卡)。诊断统计平面合同 (aio_hips.h:161) 要求无时间戳、
// 无随机源。这里用 CFITSIO 同一算法 (fits_get_chksum/fits_encode_chksum =
// ffcsum/ffesum) 计算, 但注释取固定文本, 使产物跨运行逐字节确定; DATASUM/
// CHECKSUM 仍可被 fits_verify_chksum 与外部 reader (astropy checksum=True) 验证。
// ---------------------------------------------------------------------------
bool write_chksum_deterministic(fitsfile* fptr, const std::string& where) {
    int status = 0;
    const char* chkcomm = "deterministic HDU checksum (no wall-clock)";
    const char* datacomm = "deterministic data checksum (no wall-clock)";
    // 1) 固定注释先落两张键; CHECKSUM 置 ASCII 0 = 校验和基准态。
    if (fits_update_key_str(fptr, "CHECKSUM", "0000000000000000", chkcomm, &status) ||
        fits_update_key_str(fptr, "DATASUM", "         0", datacomm, &status)) {
        return fits_ok(status, where + ": keys");
    }
    // 新增两卡改变了头区长度: 必须按 ffpcks 的先例 finalize 头区结构
    // (ffrdef: 重写 END/空填充并重算 head/data 偏移), 否则 ffghadll 仍用
    // 旧 headstart/datastart, ffcsum 会把错误区间当校验和基准。
    if (fits_set_hdustruc(fptr, &status)) {
        return fits_ok(status, where + ": hdustruc");
    }
    // 2) 数据单元校验和 (CHECKSUM=0 基准) → DATASUM。
    unsigned long datasum = 0, hdusum = 0;
    char dbuf[32], cbuf[32];
    if (fits_get_chksum(fptr, &datasum, &hdusum, &status)) {
        return fits_ok(status, where + ": datasum");
    }
    std::snprintf(dbuf, sizeof(dbuf), "%lu", datasum);
    if (fits_update_key_str(fptr, "DATASUM", dbuf, datacomm, &status)) {
        return fits_ok(status, where + ": datasum update");
    }
    // 3) 含更新后 DATASUM 重算 HDU 和, 补码编码写回 CHECKSUM。
    if (fits_get_chksum(fptr, &datasum, &hdusum, &status)) {
        return fits_ok(status, where + ": hdusum");
    }
    fits_encode_chksum(hdusum, 1, cbuf);
    if (fits_update_key_str(fptr, "CHECKSUM", cbuf, chkcomm, &status)) {
        return fits_ok(status, where + ": checksum update");
    }
    return true;
}

// ---------------------------------------------------------------------------
// 单 FITS 图像写 (含 checksum)
// data: 行主序数组 (NAXIS1 最快), naxis1 x naxis2
// ---------------------------------------------------------------------------
bool write_fits_image_raw(const std::string& path,
                          int bitpix,
                          long naxis1, long naxis2,
                          const void* data,
                          const std::vector<std::pair<std::string, std::string>>& cards,
                          const std::string& object,
                          const std::string& obs_filter,
                          double exptime,
                          const std::string& obs_date) {
    int status = 0;
    fitsfile* fptr = nullptr;
    std::string path_n = path;
    std::replace(path_n.begin(), path_n.end(), '\\', '/');
    // CFITSIO fits_create_file 拒绝覆盖已存在文件; 显式先删 (HiPS 输出允许 overwrite)
    std::remove(path_n.c_str());
    if (fits_create_file(&fptr, path_n.c_str(), &status)) {
        return fits_ok(status, "fits_create_file " + path);
    }
    long naxes[2] = {naxis1, naxis2};
    if (fits_create_img(fptr, bitpix, 2, naxes, &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "fits_create_img " + path);
    }
    fits_write_key_str(fptr, "PIXTYPE", (char*)"HEALPIX", (char*)"HEALPix pixelization", &status);
    fits_write_key_str(fptr, "ORDERING", (char*)"NESTED", (char*)"Pixel ordering", &status);
    fits_write_key_str(fptr, "COORDSYS", (char*)"C", (char*)"Equatorial", &status);
    if (!object.empty())
        fits_write_key_str(fptr, "OBJECT", (char*)fits_str(object).c_str(), nullptr, &status);
    if (!obs_filter.empty())
        fits_write_key_str(fptr, "FILTER", (char*)fits_str(obs_filter).c_str(), nullptr, &status);
    if (exptime > 0.0)
        fits_write_key_dbl(fptr, "EXPTIME", exptime, 8, (char*)"Exposure time (s)", &status);
    if (!obs_date.empty())
        fits_write_key_str(fptr, "DATE-OBS", (char*)fits_str(obs_date).c_str(), nullptr, &status);
    for (const auto& kv : cards) {
        if (kv.first == "NSIDE")
            fits_write_key_lng(fptr, (char*)kv.first.c_str(), std::atol(kv.second.c_str()),
                               (char*)"HEALPix nside of tile pixels", &status);
        else if (kv.first == "FIRSTPIX")
            fits_write_key_lng(fptr, (char*)kv.first.c_str(), std::atol(kv.second.c_str()),
                               (char*)"First tile pixel (NESTED)", &status);
        else if (kv.first == "LASTPIX")
            fits_write_key_lng(fptr, (char*)kv.first.c_str(), std::atol(kv.second.c_str()),
                               (char*)"Last tile pixel (NESTED)", &status);
        else
            fits_write_key_str(fptr, (char*)kv.first.c_str(),
                               (char*)fits_str(kv.second).c_str(), nullptr, &status);
        if (status) break;
    }
    long fpixel[2] = {1, 1};
    long nelem = naxis1 * naxis2;
    if (bitpix == -32)
        fits_write_pix(fptr, TFLOAT, fpixel, nelem, (void*)data, &status);
    else if (bitpix == -64)
        fits_write_pix(fptr, TDOUBLE, fpixel, nelem, (void*)data, &status);
    else if (bitpix == 32)
        // DATA-UNC-001 §30.2: 诊断统计平面 nused/nrej 固定 int32 (BITPIX=32)
        fits_write_pix(fptr, TINT, fpixel, nelem, (void*)data, &status);
    if (status) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "fits_write_pix " + path);
    }
    if (!write_chksum_deterministic(fptr, "fits checksum " + path)) {
        fits_close_file(fptr, &status);
        return false;
    }
    if (fits_close_file(fptr, &status)) {
        return fits_ok(status, "fits_close_file " + path);
    }
    return true;
}

// ---------------------------------------------------------------------------
// MOC FITS (BINTABLE UNIQ) 写
// ---------------------------------------------------------------------------
bool write_moc_fits_raw(const std::string& path,
                        const std::vector<uint64_t>& uniq,
                        uint32_t order) {
    if (uniq.empty()) return true;  // 空 MOC: 不写
    int status = 0;
    fitsfile* fptr = nullptr;
    std::string path_n = path;
    std::replace(path_n.begin(), path_n.end(), '\\', '/');
    std::remove(path_n.c_str());
    if (fits_create_file(&fptr, path_n.c_str(), &status)) {
        return fits_ok(status, "moc create " + path);
    }
    char* ttype[1] = {(char*)"UNIQ"};
    char* tform[1] = {(char*)"K"};
    char* tunit[1] = {(char*)""};
    if (fits_create_tbl(fptr, BINARY_TBL, (long)uniq.size(), 1,
                        ttype, tform, tunit, (char*)"", &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "moc create_tbl " + path);
    }
    // IVOA REC-MOC 2.0 §6 Table 3: ORDERING 与 COORDSYS 对 MOC 1.1/2.0 均为
    // mandatory; 本 MOC 为 NUNIQ 编码 + 赤道坐标系 (M2b-B-02)。
    fits_write_key_str(fptr, "ORDERING", (char*)"NUNIQ", (char*)"MOC cell ordering", &status);
    fits_write_key_str(fptr, "COORDSYS", (char*)"C", (char*)"Equatorial (ICRS)", &status);
    fits_write_key_lng(fptr, "MOCORDER", (long)order, (char*)"MOC order", &status);
    fits_write_key_lng(fptr, "PIXCOUNT", (long)uniq.size(), (char*)"Cell count", &status);
    if (status) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "moc keys " + path);
    }
    long fpixel = 1;
    if (fits_write_col(fptr, TLONGLONG, 1, fpixel, 1, (long long)uniq.size(),
                       const_cast<long long*>((const long long*)uniq.data()), &status)) {
        fits_close_file(fptr, &status);
        return fits_ok(status, "moc write_col " + path);
    }
    if (!write_chksum_deterministic(fptr, "moc checksum " + path)) {
        fits_close_file(fptr, &status);
        return false;
    }
    if (fits_close_file(fptr, &status)) {
        return fits_ok(status, "moc close " + path);
    }
    return true;
}

// ---------------------------------------------------------------------------
// ASTROCS_DESIGN.md §10「I/O 与原子产品」/ GAP_AUDIT G3-1:
// 每个 tile/元数据 FITS 走「本次运行私有临时文件 → 哈希校验 → fsync →
// 原子 rename」。修复前 write_fits_image 先 std::remove(final) 再
// fits_create_file(final) **直写正式路径** ⇒ 中途 kill / ENOSPC / 校验失败
// 都会在正式目录留下截断的半成品 tile (GAP_AUDIT G3-1 现状)。
//   * 临时名 = <final>.tmp.<pid>.<seq> (与目标同目录 ⇒ rename 不跨文件系统,
//     内核原子; 复用 aio_atomic_file.h 的 AIO-001 原语, 不另造机制);
//   * 哈希校验 = 重开临时文件独立跑 fits_verify_chksum (DATASUM/CHECKSUM 由
//     write_chksum_deterministic 写入), 不过即删除临时文件并失败;
//   * 任一环节失败 ⇒ 删除临时文件 + 返回 false —— 正式路径只可能出现完整 tile。
// 注入面 (测试专用, 未设置时逐行零行为差异): ASTROCS_HIPS_TILE_FAULT =
//   tile_write_fail | tile_diskfull | tile_checksum_fail | tile_fsync_fail |
//   tile_rename_fail。每个注入名必败 (无恒 PASS 占位), 用于负例判别力证明。
// ---------------------------------------------------------------------------
bool tile_fault(const char* name) {
    if (!fault_injected("ASTROCS_HIPS_TILE_FAULT", name)) return false;
    std::fprintf(stderr, "FAULT-INJECT: %s\n", name);
    return true;
}

// 重开临时文件独立校验 DATASUM/CHECKSUM (哈希校验环节)。
// 注意: fits_verify_chksum 只校验**当前** HDU —— 新建 BINTABLE (Moc.fits) 的
// 数据在 HDU 2 (HDU 1 是 CFITSIO 自动建的空 PRIMARY), 重开后当前 HDU 是
// PRIMARY, 直接调会得到 dataok=0/hduok=0 的假失败 (实测, 与校验和写入正确性
// 无关)。故逐 HDU 移动, 只校验带 CHECKSUM 键的 HDU, 并要求至少校验成功一个。
bool verify_fits_checksum(const std::string& path, std::string* err) {
    int status = 0;
    fitsfile* fptr = nullptr;
    if (fits_open_file(&fptr, path.c_str(), READONLY, &status)) {
        char msg[FLEN_ERRMSG];
        msg[0] = '\0';
        fits_get_errstatus(status, msg);
        if (err) *err = std::string("checksum verify open failed: ") + msg;
        fits_clear_errmsg();
        return false;
    }
    int nhdu = 0;
    if (fits_get_num_hdus(fptr, &nhdu, &status) || nhdu <= 0) {
        char msg[FLEN_ERRMSG];
        msg[0] = '\0';
        fits_get_errstatus(status, msg);
        if (err) *err = std::string("fits_get_num_hdus failed: ") + msg;
        fits_close_file(fptr, &status);
        fits_clear_errmsg();
        return false;
    }
    int verified = 0;
    for (int h = 1; h <= nhdu; ++h) {
        if (fits_movabs_hdu(fptr, h, nullptr, &status)) {
            char msg[FLEN_ERRMSG];
            msg[0] = '\0';
            fits_get_errstatus(status, msg);
            if (err) *err = "fits_movabs_hdu failed: " + std::string(msg);
            fits_close_file(fptr, &status);
            fits_clear_errmsg();
            return false;
        }
        char csum[FLEN_VALUE];
        csum[0] = '\0';
        int kstatus = 0;
        if (fits_read_key(fptr, TSTRING, "CHECKSUM", csum, nullptr, &kstatus)) {
            fits_clear_errmsg();          // 该 HDU 无 CHECKSUM: 不参与校验
            status = 0;
            continue;
        }
        int dataok = 0, hduok = 0;
        if (fits_verify_chksum(fptr, &dataok, &hduok, &status)) {
            char msg[FLEN_ERRMSG];
            msg[0] = '\0';
            fits_get_errstatus(status, msg);
            if (err)
                *err = "fits_verify_chksum failed (HDU " + std::to_string(h) +
                       "): " + msg;
            fits_close_file(fptr, &status);
            fits_clear_errmsg();
            return false;
        }
        if (dataok != 1 || hduok != 1) {
            if (err)
                *err = "DATASUM/CHECKSUM mismatch (HDU " + std::to_string(h) +
                       ", dataok=" + std::to_string(dataok) +
                       " hduok=" + std::to_string(hduok) + ")";
            fits_close_file(fptr, &status);
            return false;
        }
        ++verified;
    }
    fits_close_file(fptr, &status);
    if (verified == 0) {
        if (err) *err = "no CHECKSUM keyword in any HDU: " + path;
        return false;
    }
    return true;
}

// 私有临时文件 → 内容写出 → 哈希校验 → fsync → 原子 rename → 父目录 fsync。
bool write_fits_atomic(const std::string& final_path,
                       const std::function<bool(const std::string&)>& body,
                       std::string* err) {
    if (!body || final_path.empty()) {
        if (err) *err = "write_fits_atomic: 参数无效";
        return false;
    }
    const std::string tmp = aio_atomic::make_tmp_path(final_path);
    const bool inj_diskfull = tile_fault("tile_diskfull");
    const bool inj_write = tile_fault("tile_write_fail");
    if (inj_diskfull || inj_write || !body(tmp)) {
        // 磁盘满必须在**清理之前**、失败发生处分类 (见 aio_disk_full.h 头注:
        // 清理会释放空间, 事后探针必然 fail-open)。注入面 tile_diskfull 等价于 ENOSPC。
        if (inj_diskfull) aio_disk::note_full();
        else aio_disk::note_failure(tmp, errno);
        aio_atomic::remove_file(tmp);
        if (err)
            *err = inj_diskfull ? "ENOSPC (injected: tile_diskfull)"
                   : inj_write ? "write failed (injected: tile_write_fail)"
                               : ("FITS write failed: " + final_path);
        return false;
    }
    // kill 中断测试锚点 (测试专用; 生产零行为差异): 内容已写进**私有临时文件**、
    // 尚未校验/rename 时驻留, 供父进程在该窗口 kill 子进程。断言"正式路径无
    // 半成品 tile"正是在此窗口成立 (修复前该窗口直接写在正式路径上)。
    if (tile_fault("tile_slow_write")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    std::string cerr;
    if (tile_fault("tile_checksum_fail") || !verify_fits_checksum(tmp, &cerr)) {
        aio_atomic::remove_file(tmp);
        if (err)
            *err = "checksum verify failed: " + final_path +
                   (cerr.empty() ? " (injected: tile_checksum_fail)" : " (" + cerr + ")");
        return false;
    }
    if (tile_fault("tile_fsync_fail")) {
        aio_atomic::remove_file(tmp);
        if (err) *err = "fsync failed (injected: tile_fsync_fail): " + tmp;
        return false;
    }
    const int frc = aio_atomic::fsync_path(tmp, 0);
    if (frc != 0) {
        aio_disk::note_failure(tmp, frc);   // 清理前分类
        aio_atomic::remove_file(tmp);
        if (err) *err = "fsync failed: " + tmp + " (errno=" + std::to_string(frc) + ")";
        return false;
    }
    // 裸形态体积削减（打洞）：fsync 之后、原子发布之前。
    // 依据 docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md §7 表 T1 与
    // ENGINEERING_SPEC.md §11：只对块对齐的**字面全零**区域打洞；文件字节与
    // st_size 不变；打洞后读回复算，不一致 ⇒ 不得发布（硬错误）；卷不支持 ⇒
    // 跳过并记 warn（trim=skipped(<reason>)），不 fail-closed。
    {
        aio_sparse::PunchResult pr;
        aio_sparse::punch_all_zero_blocks(tmp, &pr, /*verify=*/true);
        if (pr.rc == aio_sparse::PUNCH_VERIFY_MISMATCH) {
            aio_atomic::remove_file(tmp);
            if (err)
                *err = "sparse punch readback mismatch (" + pr.reason + "): " + tmp;
            return false;
        }
        if (pr.rc == aio_sparse::PUNCH_OK && pr.punched_bytes != 0) {
            aio_log(AIO_LOG_INFO, "aio_sparse",
                    "trim=punched bytes=%llu holes=%u released=%llu size=%llu file=%s",
                    (unsigned long long)pr.punched_bytes, (unsigned)pr.holes,
                    (unsigned long long)pr.released_bytes(),
                    (unsigned long long)pr.size_bytes, tmp.c_str());
        } else if (pr.rc != aio_sparse::PUNCH_OK) {
            // 打洞是体积优化，不是科学语义：降级不阻断发布。
            aio_log(AIO_LOG_WARN, "aio_sparse",
                    "trim=skipped(%s) errno=%d file=%s",
                    pr.reason.c_str(), pr.sys_errno, tmp.c_str());
        }
    }
    if (tile_fault("tile_rename_fail")) {
        aio_atomic::remove_file(tmp);
        if (err) *err = "atomic rename failed (injected: tile_rename_fail): " + tmp;
        return false;
    }
    if (aio_atomic::atomic_replace(tmp, final_path) != 0) {
        aio_atomic::remove_file(tmp);
        if (err) *err = "atomic rename failed: " + tmp + " -> " + final_path;
        return false;
    }
    aio_atomic::fsync_parent_dir(final_path);
    return true;
}

// 原子 tile 写入口 (签名与 write_fits_image_raw 逐字一致): 全部调用点自动经
// 临时文件 + 哈希校验 + fsync + 原子 rename。
bool write_fits_image(const std::string& path,
                      int bitpix,
                      long naxis1, long naxis2,
                      const void* data,
                      const std::vector<std::pair<std::string, std::string>>& cards,
                      const std::string& object,
                      const std::string& obs_filter,
                      double exptime,
                      const std::string& obs_date) {
    std::string err;
    const bool ok = write_fits_atomic(
        path,
        [&](const std::string& tmp) {
            return write_fits_image_raw(tmp, bitpix, naxis1, naxis2, data, cards,
                                        object, obs_filter, exptime, obs_date);
        },
        &err);
    if (!ok) set_error("tile 原子落盘失败: " + path + " (" + err + ")");
    return ok;
}

// 原子 MOC 写入口 (签名与 write_moc_fits_raw 逐字一致)。
bool write_moc_fits(const std::string& path,
                    const std::vector<uint64_t>& uniq,
                    uint32_t order) {
    if (uniq.empty()) return true;  // 空 MOC: 不写
    std::string err;
    const bool ok = write_fits_atomic(
        path,
        [&](const std::string& tmp) {
            return write_moc_fits_raw(tmp, uniq, order);
        },
        &err);
    if (!ok) set_error("MOC 原子落盘失败: " + path + " (" + err + ")");
    return ok;
}

// properties 写出 (IVOA HiPS + ASTROCS_* provenance 唯一文本载体)。
// M9-G-6/AIO-001: 原实现 fopen 失败即静默 return、fprintf/fclose 不查, 且直写正式路径
// —— 违反 ASTROCS_DESIGN §9(失败不得留下可被误认为正式产品的半成品)与
// ENGINEERING_SPEC §9(错误须经统一状态码传播)。改为: 同目录临时文件 → fflush →
// fsync → 原子 rename (aio_atomic_file.h), 任一环节失败清理临时文件并返回 false,
// 由调用方按子产品错误码 (-3..-8) 上报。
bool write_properties(const std::string& path,
                      const std::vector<std::pair<std::string, std::string>>& kv) {
    std::string content;
    content.reserve(kv.size() * 48 + 16);
    for (const auto& p : kv) {
        content += p.first;
        content += '=';
        content += p.second;
        content += '\n';
    }
    // 注入点 (测试专用): 等价模拟"临时文件/fsync/rename 环节失败"。修复前本函数对这些
    // 失败一律静默 (void + return), 故该注入正是判别"失败是否传播"的等价缺陷面。
    if (fault_injected("ASTROCS_HIPS_PROV_FAULT", "properties_write_fail")) {
        set_error("properties 原子落盘失败 (injected@atomic): " + path);
        return false;
    }
    std::string err;
    if (aio_atomic::write_file_atomic(path, content, &err) != 0) {
        set_error("properties 原子落盘失败: " + path + " (" + err + ")");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// hierarchy 累加器 (MEM-DESIGN-01: 稀疏分块 + 按需通道 + 完备即流式写出)
// ---------------------------------------------------------------------------
// GAP_AUDIT G3-3 / DISP-HIPS-009: 父层累加 (Σflux / Σarea / Σvar_num)
// **恒在 f64 累加器**进行 (ASTROCS_DESIGN §3.3 默认科学计算双精度); 产品声明位深
// (bitpix −32/−64) 只在写出时量化一次 (finalize_hierarchy 的 (float)sig 截断)。
// 修复前 f32 产品走 float 累加 (sumFluxF/sumAreaF/sumVarF): 每步 partial sum 舍入
// 到 float, 误差随层级加深累积 —— dk=9 实测偏差 2.5e-3 (合成) / 3.95e-4 (真实),
// 超 HIPS_WRITER.md §9 冻结容差 rtol=1e-6。修法即该文 DISP-HIPS-009 处置建议
// "f32 产品仍用 double 累加 (存储时再截断)"。
//
// MEM-DESIGN-01 (内存结构优化; 权威: ASTROCS_DESIGN §8.3 静态预算/内存占用永不
// 越界 + §3.3 精度口径; 实测见 run/MEM-DESIGN-01/REPORT.md):
//
// (A) 稀疏分块 —— 每祖先 cell 的 512×512 面按 64×64 子块**惰性分配**: 没有数据
//     的子块根本不分配 (恒等于全零 ⇒ 零字节占用, 比"填零+压缩"更彻底)。
//     分块索引 = NESTED local z >> 12: z 的低 12 位恰是块内 bit-interleave 坐标
//     (z = interleave(x,y) ⇒ z>>12 = interleave(x>>6, y>>6)), 故 z>>12 就是 2D
//     均匀方块网格索引; 发布面 (FITS 行主序) 与该网格只差 flip/transpose,
//     方块性保持 ⇒ 覆盖分布可由发布 support 面直接实测 (不靠仿真)。
//     64×64 块 ⇒ 每 cell 恰好 64 块 = 稠密 512×512 的 1:1 覆盖, 故稀疏表示的
//     载荷字节**恒 ≤ 稠密**(唯一开销 = 每通道 64×8 B 指针表 = 512 B/cell/通道)。
//     真实产物实测子块占用率 0.70 (3 通道 1631 MiB → 979 MiB)。
// (C) 按需通道 —— variance 通道只在首次 var 累加时分配 (signal-only 产品不分配);
//     旧 count 通道全文件零读取 (死通道), 已删除 (每 cell 省 1 MiB = 14%)。
// (B) 完备即流式写出 —— 祖先 cell 的 4^dk 个叶槽全部到达后, 该 cell 不可能再收到
//     贡献 ⇒ 立即写出其 hierarchy tile 并释放内存, 不整层常驻到 finalize。
//     判据只依赖"槽位是否都已到达"(与写序无关) ⇒ 对任意调用序成立; 叶槽数用
//     计数器而非位图 (同一 cell 内不同叶 ipix 必属不同槽), 内存 O(1)/cell。
//
// 逐位不变 (硬约束): 每个累加元素的加法次数与顺序与稠密实现**逐位相同** ——
//   分块只改变"零从哪来"(惰性零块 vs 预置零数组), 不改变任何一次浮点运算;
//   流式写出只改变"何时落盘", 不改变累加顺序。判据见 p1hips oracle O8 组
//   (稀疏 vs 稠密注入逐字节一致 / drop_blk0 注入必判红) 与
//   run/MEM-DESIGN-01/verify 的基线-优化双二进制 sha256 对照。
//
// 负例注入面 (测试专用, 与 ASTROCS_HIPS_DIAG_FAULT / ASTROCS_HIPS_PROV_FAULT
// 同模式, 未设置时逐行零行为差异):
//   ASTROCS_HIPS_HIER_FAULT=f32_accum     复现修复前 f32 逐步舍入
//   ASTROCS_HIPS_HIER_FAULT=dense_blocks  每通道一次性分配全部 64 块 = 修复前
//                                         稠密分配 + 不流式写出 (等价旧语义);
//                                         用于"稀疏 ≡ 稠密"逐位对照与内存判据判红
//   ASTROCS_HIPS_HIER_FAULT=drop_blk0     丢弃落入子块 0 的累加 (模拟"错误跳过
//                                         子块") ⇒ 数值判据必须判红
namespace hier_sparse {
constexpr size_t kSide    = 64;                  // 子块边长 (元素)
constexpr size_t kElems   = kSide * kSide;       // 4096 = 2^12
constexpr size_t kShift   = 12;                  // log2(kElems)
constexpr size_t kMask    = kElems - 1;
constexpr size_t kPerAxis = 512 / kSide;         // 8
constexpr size_t kCount   = kPerAxis * kPerAxis; // 64
static_assert(kPerAxis * kSide == 512, "子块网格必须整除 512");
static_assert((size_t(1) << kShift) == kElems, "kShift 必须等于 log2(kElems)");
using Block = std::unique_ptr<double[]>;
using Table = std::array<Block, kCount>;

// 惰性分配: 值初始化 (new double[n]()) = 全零, 与稠密 assign(n, 0.0) 逐位等价。
inline double* ensure_block(Table& t, size_t bi) {
    Block& p = t[bi];
    if (!p) p.reset(new double[kElems]());
    return p.get();
}
// 未分配块 = 恒零 (与稠密数组读到的 0.0 逐位相同)。
inline double at(const Table& t, size_t z) {
    const Block& p = t[z >> kShift];
    return p ? p[z & kMask] : 0.0;
}
}  // namespace hier_sparse

// 注入面参数 (每 (tile, level) 解析一次, 不在逐像素热路径)
struct HierFaults {
    bool f32_accum = false;
    bool dense_blocks = false;
    bool drop_blk0 = false;
    // HIPS-DETERMINISM-01 注入面: true = 把祖先累加退化为**真正的跨瓦片求和**
    // (去掉 z 的叶槽偏移 s, 使同一祖先 cell 的多个叶 tile 写同一批 z 槽, 于是
    // 每个 z 被多次 += 且次序 = 瓦片到达序)。这是 PERF-PROFILE-01 §9.3 预警形态
    // (「AncestorAcc 按 tile 到达序 += 累加」)的等价复现 —— 生产实现的 z 映射按 s
    // 互斥, 该形态**不存在**(见 write_signal_support_tile 的 z 推导注释); 本注入面
    // 只用于证明 P8「到达序无关」判据对该类缺陷具备判别力(判据非退化, AGENTS §5)。
    bool cross_tile_sum = false;
};

struct AncestorAcc {
    hier_sparse::Table fluxD{};   // Σ sig·a (权重 = 未钳制真实覆盖面积)
    hier_sparse::Table areaD{};   // Σ a
    hier_sparse::Table varD{};    // 方差传播分子 Σ v_j w_jp² (按需通道)
    uint64_t slots_seen = 0;      // 已到达叶槽数 (重复叶 tile 不计)
    uint64_t slots_total = 0;     // 该 cell 的叶槽总数 4^dk
    // variance 通道的配对计数 (仅 variance/ivar 产品使用): 只有"待配对槽"收到
    // var 贡献才计入 ⇒ 与 signal/var 的调用时序无关地精确判定"该 cell 的
    // Σflux/Σarea/Σvar 三者都已收齐"。
    uint64_t var_slots_seen = 0;
    uint64_t pending_var_slot = UINT64_MAX;   // 最近一次 signal 写设置的待配对槽
    bool f32_accum = false;       // 注入面: true = 复现修复前 f32 逐步舍入
    bool dense_forced = false;    // 注入面: true = 复现修复前稠密分配
    bool drop_blk0 = false;       // 注入面: true = 丢弃子块 0 的累加
    bool cross_tile_sum = false;  // 注入面: true = 祖先累加退化为跨瓦片求和
    bool flushed = false;         // 已流式写出并释放
    // 热路径块指针缓存: z 关于 i 单调不减 ⇒ 连续 i 命中同一块 (命中率 ~1),
    // 每像素只多一次整数比较 (不改变任何浮点运算)。
    size_t cur_bi = SIZE_MAX;
    size_t cur_var_bi = SIZE_MAX;
    double* cur_flux = nullptr;
    double* cur_area = nullptr;
    double* cur_var = nullptr;

    void ensure(const HierFaults& f, uint64_t slots_total_) {
        f32_accum = f.f32_accum;
        drop_blk0 = f.drop_blk0;
        dense_forced = f.dense_blocks;
        cross_tile_sum = f.cross_tile_sum;
        if (slots_total == 0) slots_total = slots_total_;
        if (dense_forced) {   // 注入面: 一次性分配全部块 (复现修复前稠密语义)
            for (size_t bi = 0; bi < hier_sparse::kCount; ++bi) {
                hier_sparse::ensure_block(fluxD, bi);
                hier_sparse::ensure_block(areaD, bi);
                if (f.dense_blocks) hier_sparse::ensure_block(varD, bi);
            }
        }
    }
    // f32 逐步舍入的等价复现: 两 float 之和在 double 中精确, 再一次舍入到 float
    // = 正确舍入的 float 加法 (binary64→binary32 双重舍入在 p_d≥2·p_f+2 时无害)。
    static double f32_step(double acc, double v) {
        return (double)(float)((double)(float)acc + (double)(float)v);
    }
    void add(size_t z, double flux, double area) {
        const size_t bi = z >> hier_sparse::kShift;
        if (drop_blk0 && bi == 0) return;   // 注入面: 整块丢弃
        if (bi != cur_bi) {
            cur_bi = bi;
            cur_flux = hier_sparse::ensure_block(fluxD, bi);
            cur_area = hier_sparse::ensure_block(areaD, bi);
        }
        const size_t off = z & hier_sparse::kMask;
        if (f32_accum) {
            cur_flux[off] = f32_step(cur_flux[off], flux);
            cur_area[off] = f32_step(cur_area[off], area);
        } else {
            cur_flux[off] += flux;
            cur_area[off] += area;
        }
    }
    void add_var(size_t z, double var_num) {
        const size_t bi = z >> hier_sparse::kShift;
        if (drop_blk0 && bi == 0) return;   // 注入面: 整块丢弃
        if (bi != cur_var_bi) {
            cur_var_bi = bi;
            cur_var = hier_sparse::ensure_block(varD, bi);
        }
        const size_t off = z & hier_sparse::kMask;
        if (f32_accum) cur_var[off] = f32_step(cur_var[off], var_num);
        else           cur_var[off] += var_num;
    }
    double fluxAt(size_t i) const { return hier_sparse::at(fluxD, i); }
    double areaAt(size_t i) const { return hier_sparse::at(areaD, i); }
    double varAt(size_t i) const  { return hier_sparse::at(varD, i); }
    // 写出后立即释放 (流式写出与 finalize 共用); 保留 slots_* 供重复写判定。
    void release() {
        for (size_t bi = 0; bi < hier_sparse::kCount; ++bi) {
            fluxD[bi].reset(); areaD[bi].reset(); varD[bi].reset();
        }
        cur_bi = SIZE_MAX; cur_var_bi = SIZE_MAX;
        cur_flux = cur_area = cur_var = nullptr;
        flushed = true;
    }
    // 未释放子块数 (仅用于诊断/测试打印; 不参与数值路径)
    size_t allocated_blocks() const {
        size_t n = 0;
        for (size_t bi = 0; bi < hier_sparse::kCount; ++bi)
            if (fluxD[bi]) ++n;
        return n;
    }
};

// 注入面判定 (测试专用): 生产默认全 false; 见 AncestorAcc 注释。
HierFaults hier_faults() {
    HierFaults f;
    f.f32_accum    = fault_injected("ASTROCS_HIPS_HIER_FAULT", "f32_accum");
    f.dense_blocks = fault_injected("ASTROCS_HIPS_HIER_FAULT", "dense_blocks");
    f.drop_blk0    = fault_injected("ASTROCS_HIPS_HIER_FAULT", "drop_blk0");
    f.cross_tile_sum = fault_injected("ASTROCS_HIPS_HIER_FAULT", "cross_tile_sum");
    return f;
}

// ---------------------------------------------------------------------------
// MEM-DESIGN-01: 祖先 cell 写出与流式释放 (finalize 与流式写出**共用同一路径**
// ⇒ 两种写出时序的产物逐位相同)。
// ---------------------------------------------------------------------------
} // namespace

// ============================================================================
// Product Set 实现
// ============================================================================
struct AioHipsProductSet {
    std::string out_dir;
    // Drizzle provenance（默认未设置 → properties 不写）
    bool drizzle_prov_set = false;
    double drizzle_pixfrac = 0.0;
    double drizzle_scale_arcsec = 0.0;
    // RELEASE-02 SD-15 帧级未加权通量型 SNR 键（默认未设置 → properties 不写）
    bool frame_snr_set = false;
    double frame_snr = 0.0;        // F_ref/σ_F（信噪比, 非权重）
    double reference_flux = 0.0;   // 组内公共 F_ref
    // DATA-UNC-001 §30.3 (DATA-P2-PROV-001) provenance 四键
    // （全或无: prov_set=false → 四键整体不写, legacy 产品面不变）
    bool prov_set = false;
    std::string prov_manifest_hash, prov_model_hash, prov_reject_profile;
    int prov_uncertainty_available = 0;
    // §9.73 A44: 旧「权重模式」成员 prov_weight_mode 已删除
    // (全程只有 SNR, provenance 不承载权重模式; 见 aio_hips.h 四键通道注释)。
    // DATA-UNC-001 §30.2 诊断统计平面: 各通道是否真的写过 tile
    // （写 0 个 tile 的通道不 finalize, 禁空目录/空占位冒充产品）
    bool diag_nrej_tiles = false;
    bool diag_nused_tiles = false;
    uint32_t nside = 0;
    uint32_t tile_width = 512;
    int32_t data_type = AIO_HIPS_FLOAT32;
    int flags = AIO_HIPS_PRODUCT_ALL;
    uint32_t leaf_order = 0;   // L
    uint32_t tile_order = 0;   // K = L-9
    double A_cell = 0.0;       // 叶级 cell 面积 sr
    std::string creator_did, obs_title, obs_filter, obs_date;
    double exposure = 0.0;
    uint32_t moc_order = 0;

    std::set<uint64_t> moc_cells;          // 叶级 tile cells @ order K (有数据)
    std::vector<uint64_t> leaf_ipix_list;  // 写入顺序
    std::vector<std::map<uint64_t, AncestorAcc>> hier;  // hier[k] for k<K
    std::vector<AioHipsSnrPoint> snr;
    double moc_area_sr = 0.0;              // Σ moc cell 面积 (order K)
    double covered_area_sr = 0.0;          // Σ covered_area (真实覆盖)
    // M2a-H-3 可观测钳制计数 (properties + manifest 双写)
    uint64_t support_clamped_pixels = 0;   // 叶级 covered_area > A_cell (钳生效)
    uint64_t coverage_gt1_pixels = 0;      // 层级输出像素 Σarea > A_cell_k (不可复原)
    double sig_min = 1e300, sig_max = -1e300;
    bool finalized = false;
    // 跨 tile 累计 profile（低开销 coarse；每 tile 每段一次 clock）
    double prof_transform = 0.0;        // NESTED→FITS scatter
    double prof_fits_write = 0.0;       // CFITSIO tile 写出
    double prof_hierarchy_accum = 0.0;  // 每 tile ancestor 累加
    double prof_finalize_products = 0.0;
    double prof_hierarchy_write = 0.0;
    double prof_finalize_snr = 0.0;
    // 跨 tile 复用 scratch
    std::vector<float>  scratch_sigF, scratch_supF;   // dtype=f32 写缓冲
    std::vector<double> scratch_sigD, scratch_supD;   // dtype=f64 写缓冲
    std::vector<double> scratch_sig_n, scratch_sup_n; // NESTED 序缓存（hierarchy）
    // NESTED 序**未钳制**真实覆盖面积（hierarchy 归约权重; M2a-H-3）
    std::vector<double> scratch_area_n;
    // variance/ivar scratch
    std::vector<float>  scratch_varF, scratch_ivarF;
    std::vector<double> scratch_varD, scratch_ivarD;
    std::vector<double> scratch_var_n;                 // NESTED 序 var_num (hierarchy)
    // nused/nrej 诊断平面 scratch (int32, FITS 序写缓冲)
    std::vector<int32_t> scratch_diag_nrej, scratch_diag_nused;
};

namespace {

// M2a-H-3 可观测计数: "Σ未钳制覆盖面积 > A_cell_k"的父像素数。稀疏实现只遍历
// **已分配子块** (未分配块恒零, 恒不满足 > A_cell_k>0), 与稠密全扫描同值。
static uint64_t count_coverage_gt1(const AncestorAcc& acc, int k) {
    const uint32_t nside_k = 1u << (k + 9);
    const double A_cell_k = 4.0 * kPi() / (12.0 * (double)nside_k * nside_k);
    uint64_t cnt = 0;
    for (size_t bi = 0; bi < hier_sparse::kCount; ++bi) {
        const double* pa = acc.areaD[bi].get();
        if (!pa) continue;
        const double* pf = acc.fluxD[bi].get();
        for (size_t o = 0; o < hier_sparse::kElems; ++o)
            if (pa[o] > A_cell_k && std::isfinite(pf[o])) ++cnt;
    }
    return cnt;
}

// 单个祖先 cell 的 hierarchy tile 写出 (signal/support/variance/ivar)。
// 归约与钳制与修复前逐行一致: sig = Σflux/Σarea; sup = Σarea/A_cell_k 且发布面
// **唯一一次**钳制 sup<=1; 无贡献像素 sig=NaN/sup=0; var = Σvar_num/(Σarea)²。
static bool write_hierarchy_cell(AioHipsProductSet* ps, int k, uint64_t A,
                                 AncestorAcc& acc) {
    const int bitpix = ps->data_type == AIO_HIPS_FLOAT32 ? -32 : -64;
    const size_t n = 512 * 512;
    const uint32_t nside_k = 1u << (k + 9);
    const double A_cell_k = 4.0 * kPi() / (12.0 * (double)nside_k * nside_k);
    std::vector<std::pair<std::string, std::string>> cards;
    cards.push_back({"NSIDE", std::to_string(nside_k)});
    cards.push_back({"FIRSTPIX", "0"});
    cards.push_back({"LASTPIX", std::to_string(n - 1)});
    std::string rel = tile_rel_path(k, A, ".fits");
    // AncestorAcc 以 NESTED local 索引累加, 写出低阶 hierarchy FITS 时同样
    // scatter 到标准 HiPS 行主序 (逐像素全覆盖, 无需预置零)。
    std::unique_ptr<float[]>  sigF, supF;
    std::unique_ptr<double[]> sigD, supD;
    if (bitpix == -32) { sigF.reset(new float[n]); supF.reset(new float[n]); }
    else               { sigD.reset(new double[n]); supD.reset(new double[n]); }
    for (size_t i = 0; i < n; ++i) {
        const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
            (uint64_t)i, 9u, 512u);
        const double area = acc.areaAt(i);
        const double flux = acc.fluxAt(i);
        double sig = 0.0, sup = 0.0;
        // 同叶级：覆盖与信号可用性解耦（§4a 三态表）。父级 support 也必须在
        // 「有覆盖但信号不可用」时发布，否则层级面的覆盖并集被低估
        // （与叶级 :1313 同一处缺陷的同型实例）。
        const bool covered = area > 0.0 && std::isfinite(area);
        if (covered) {
            sup = area / A_cell_k;
            if (sup > 1.0) sup = 1.0;   // I2: 发布面唯一一次钳制
        }
        if (covered && std::isfinite(flux)) {
            sig = flux / area;
        } else {
            sig = std::numeric_limits<double>::quiet_NaN();
        }
        if (bitpix == -32) { sigF[fi] = (float)sig; supF[fi] = (float)sup; }
        else               { sigD[fi] = sig;        supD[fi] = sup; }
    }
    if (ps->flags & AIO_HIPS_PRODUCT_SIGNAL) {
        std::string p = ps->out_dir + "/signal/" + rel;
        make_dirs(p.substr(0, p.find_last_of('/')));
        if (!write_fits_image(p, bitpix, 512, 512,
                              bitpix == -32 ? (const void*)sigF.get()
                                            : (const void*)sigD.get(),
                              cards, ps->obs_title, ps->obs_filter,
                              ps->exposure, ps->obs_date))
            return false;
    }
    if (ps->flags & AIO_HIPS_PRODUCT_SUPPORT) {
        std::string p = ps->out_dir + "/support/" + rel;
        make_dirs(p.substr(0, p.find_last_of('/')));
        if (!write_fits_image(p, bitpix, 512, 512,
                              bitpix == -32 ? (const void*)supF.get()
                                            : (const void*)supD.get(),
                              cards, ps->obs_title, ps->obs_filter,
                              ps->exposure, ps->obs_date))
            return false;
    }
    // variance/ivar hierarchy (归约公式同叶级)
    if ((ps->flags & (AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR)) != 0) {
        std::unique_ptr<float[]>  varF, ivarF;
        std::unique_ptr<double[]> varD, ivarD;
        if (bitpix == -32) { varF.reset(new float[n]); ivarF.reset(new float[n]); }
        else               { varD.reset(new double[n]); ivarD.reset(new double[n]); }
        for (size_t i = 0; i < n; ++i) {
            const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
                (uint64_t)i, 9u, 512u);
            const double area = acc.areaAt(i);
            const double vnum = acc.varAt(i);
            // ── §12.4:423 损坏判定（先于一切映射；禁 clamp、禁静默跳过）────────
            // 损坏 = **有覆盖**（area>0）而 vnum 非有限或 vnum<0（上游数值损坏）。
            // 判定必须**以覆盖为前提**：无覆盖（area<=0）时 vnum 为 NaN 是**正确的产品态**
            // （与 signal 面的 NaN 同态，见下方三态表第三行），不得判损坏——否则
            // "整块无覆盖"的层次 cell 会被误判成数值损坏并硬失败 rc=-6，
            // 把"这一块天区没数据"变成"产品损坏"。
            if (!std::isfinite(area) ||
                (area > 0.0 && (!std::isfinite(vnum) || vnum < 0.0))) {
                set_error("hierarchy variance 损坏 (vnum/area 非有限或 vnum<0) Norder" +
                          std::to_string(k) + " ipix=" + std::to_string(A) +
                          " i=" + std::to_string(i) + " vnum=" + std::to_string(vnum) +
                          " area=" + std::to_string(area) +
                          " ⇒ 硬失败 (DATA_SEMANTICS §12.4:423)");
                return false;
            }
            // variance/ivar 三态（§4a:49 / §11.2:323 / §30.4:2698）：
            //   有覆盖 ∧ 方差可用 (area>0 ∧ vnum>0) → vnum/area², 1/var
            //   有覆盖 ∧ 方差不可用 (area>0 ∧ vnum==0) → 0 / 0（显式不可用，禁 NaN）
            //   无覆盖 (area<=0)                        → NaN / NaN（与 signal NaN 同态）
            double var = std::numeric_limits<double>::quiet_NaN();
            double iv = std::numeric_limits<double>::quiet_NaN();
            if (area > 0.0) {
                if (vnum > 0.0) {
                    var = vnum / (area * area);
                    iv = 1.0 / var;
                } else {
                    var = 0.0;
                    iv = 0.0;
                }
            }
            if (bitpix == -32) { varF[fi] = (float)var; ivarF[fi] = (float)iv; }
            else               { varD[fi] = var;        ivarD[fi] = iv; }
        }
        if (ps->flags & AIO_HIPS_PRODUCT_VARIANCE) {
            std::string p = ps->out_dir + "/variance/" + rel;
            make_dirs(p.substr(0, p.find_last_of('/')));
            if (!write_fits_image(p, bitpix, 512, 512,
                                  bitpix == -32 ? (const void*)varF.get()
                                                : (const void*)varD.get(),
                                  cards, ps->obs_title, ps->obs_filter,
                                  ps->exposure, ps->obs_date))
                return false;
        }
        if (ps->flags & AIO_HIPS_PRODUCT_IVAR) {
            std::string p = ps->out_dir + "/ivar/" + rel;
            make_dirs(p.substr(0, p.find_last_of('/')));
            if (!write_fits_image(p, bitpix, 512, 512,
                                  bitpix == -32 ? (const void*)ivarF.get()
                                                : (const void*)ivarD.get(),
                                  cards, ps->obs_title, ps->obs_filter,
                                  ps->exposure, ps->obs_date))
                return false;
        }
    }
    return true;
}

// (B) 完备即写出: 把因本次叶写而"叶槽齐备"的祖先 cell 立即写出并释放。
// 完备判据 = slots_seen == slots_total (= 该 cell 的 4^dk 个叶槽全部到达) ⇒ 此后
// 该 cell 不可能再收到**新**贡献 (只可能有重复叶写, 已在写叶处 fail-closed 拦截),
// 故与写序无关地对任意调用序成立。
static bool stream_flush_ready_cells(AioHipsProductSet* ps, uint64_t leaf_ipix) {
    // 注入面 dense_blocks = 修复前语义 (稠密分配 + 不流式写出), 用于 O8 的
    // "稀疏+流式 ≡ 稠密+不流式"逐字节对照与结构/内存判据判红。
    if (hier_faults().dense_blocks) return true;
    for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
        const uint64_t shift = 2ULL * (uint64_t)((int)ps->tile_order - k);
        const uint64_t A = leaf_ipix >> shift;
        auto it = ps->hier[(size_t)k].find(A);
        if (it == ps->hier[(size_t)k].end()) continue;
        AncestorAcc& acc = it->second;
        if (acc.flushed || acc.slots_total == 0 ||
            acc.slots_seen != acc.slots_total)
            continue;
        // variance/ivar 产品: Σvar 也必须收齐 (var_slots_seen 由"待配对槽"精确计数,
        // 与 signal/var 交错方式无关) ⇒ 任何调用时序下都不会提前写出。
        if ((ps->flags & (AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR)) != 0 &&
            acc.var_slots_seen != acc.slots_total)
            continue;
        ps->coverage_gt1_pixels += count_coverage_gt1(acc, k);
        if (!write_hierarchy_cell(ps, k, A, acc)) return false;
        acc.release();
    }
    return true;
}

// 重复写同一叶 tile 的 fail-closed 判定: 旧语义下重复写会产生"叶文件被最后一次
// 覆盖 + hierarchy 重复计数"的不一致产品; 若该叶的祖先 cell 已流式写出, 重复贡献
// 无处可加 ⇒ 拒绝 (禁静默丢贡献)。返回 false 时已置 last_error。
static bool hier_duplicate_allowed(AioHipsProductSet* ps, uint64_t leaf_ipix) {
    for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
        const uint64_t shift = 2ULL * (uint64_t)((int)ps->tile_order - k);
        const uint64_t A = leaf_ipix >> shift;
        auto it = ps->hier[(size_t)k].find(A);
        if (it != ps->hier[(size_t)k].end() && it->second.flushed) {
            set_error("重复写叶 tile " + std::to_string(leaf_ipix) +
                      ": 其祖先 cell (Norder" + std::to_string(k) + " ipix=" +
                      std::to_string(A) + ") 已完备并流式写出 (MEM-DESIGN-01 (B)), "
                      "重复贡献无法按旧语义累加 ⇒ fail-closed");
            return false;
        }
    }
    return true;
}

} // namespace (MEM-DESIGN-01 稀疏累加器辅助)

// ============================================================================
// P1 (R9-A): C 边界异常屏障 (bughunt_p1_batchI; 家族方案对齐 f1cb487c
// aio_api.cpp P0-4 口径)。本文件 extern "C" 9 个导出入口此前 0 个有 try
// 保护: FITS writer 内部 std::string/vector 分配 bad_alloc、length_error、
// cfitsio 包装异常均可跨 C ABI 传播 (UB/terminate)。
// 统一口径: 指针返回型 -> nullptr; int 返回型 -> -1 (既有错误码域, 异常
// 详情记入 g_hips_error 前缀 "exception:"); last_error 为 noexcept 字符串
// 返回不加壳 (g_hips_error 为命名空间级 std::string, c_str() 不抛)。
// 正常路径与修复前逐行等价。
// ============================================================================
extern "C" {

AioHipsProductSet* aio_hips_product_begin(
    const char* out_dir,
    uint32_t nside,
    uint32_t tile_width,
    int32_t data_type,
    int flags,
    const char* creator_did,
    const char* obs_title,
    const char* obs_filter,
    double exposure_s,
    const char* obs_date,
    uint32_t moc_order)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        g_hips_error.clear();
        // 磁盘满分类不再在这里复位: 归因窗口由**发起写入的执行流**在写产品之前
        // 取快照 (aio_disk::FailureEpoch), 计数只增不减。旧实现在这里做进程级
        // reset(), 多帧并发时会抹掉另一帧已置位的判定 ⇒ 失败帧 manifest 丢
        // error_kind ⇒ exit 7 的 fail-open (aio_disk_full.h 头注「归因粒度=帧级」)。
        // nside 必须恰为 2 的幂(M8d-A-01/AIO-001): 叶级几何基数 nside=2^K 是
        // ALG-HIPS-001 (1a) 的冻结构造前提, 下方 ilog2_u64 是*向下取整*, 非 2 的幂
        // (如 600) 会被静默夹逼到 2^9 并据此写出与调用方声明不一致的 NSIDE/
        // A_cell/leaf_order 产品 —— 属 ASTROCS_DESIGN §9 禁止的"看似完整产品"。
        // 上界 2^29(实现域): NESTED 计数 Npix=12·nside² 在 nside=2^29 时为 12·2^58
        // < 2^63(uint64 域内); nside>2^29 时该积将溢出/越出可寻址 tile 域, 且
        // tile_order=leaf_order-9 亦超出 MOC 阶实际可用范围 ⇒ 与非法 nside 同类拒绝。
        // 依据: docs/algorithms/HIPS_WRITER.md §1(1a) "叶级 nside=2^K>=512";
        //       API-HIPS-001 契约 aio_hips.h:101 "nside - 叶级 NSIDE (2 的幂, >= 512)";
        //       §9 负面矩阵"nside<512"(扩展到非 2 的幂/越上界同类非法输入);
        //       ASTROCS_DESIGN.md 附录 B IVOA HiPS 1.0 / Górski 2005 (Npix=12·nside²)。
        const bool nside_is_pow2 = (nside & (nside - 1u)) == 0u;
        if (!out_dir || !*out_dir || nside < 512 ||
            nside > (1u << 29) || !nside_is_pow2 || tile_width != 512 ||
            (data_type != AIO_HIPS_FLOAT32 && data_type != AIO_HIPS_FLOAT64) ||
            (flags & ~AIO_HIPS_PRODUCT_ALL_V20) != 0) {
            set_error("aio_hips_product_begin: 参数无效 (nside=2^K 且 512<=nside<=2^29, "
                      "tile_width=512, dtype 0/1)");
            return nullptr;
        }
        std::unique_ptr<AioHipsProductSet> ps(new AioHipsProductSet);
        ps->out_dir = out_dir;
        ps->nside = nside;
        ps->tile_width = tile_width;
        ps->data_type = data_type;
        ps->flags = flags;
        ps->leaf_order = ilog2_u64(nside);
        ps->tile_order = ps->leaf_order - 9;
        ps->A_cell = 4.0 * kPi() / (12.0 * (double)nside * nside);
        ps->creator_did = creator_did ? creator_did : "ivo://astrocs/phase1";
        ps->obs_title = obs_title ? obs_title : "AstroCS Phase1";
        ps->obs_filter = obs_filter ? obs_filter : "";
        ps->obs_date = obs_date ? obs_date : "";
        ps->exposure = exposure_s;
        ps->moc_order = (moc_order == 0) ? ps->tile_order : std::min(moc_order, ps->tile_order);
        ps->hier.resize(ps->tile_order);
        // §10「同一标识只有一个生产者」+「失败/取消时正式目录只出现
        // 完整产品」: 开工前先摘掉完成清单 (消费者立即 fail-closed), 再确定性
        // 删除本次将要写入的子产品目录 —— 上次 kill/失败留下的残留 tile、
        // 半成品与 .tmp.* 临时文件一律不得被本次运行"消费"或混进新清单。
        // 删除失败即 begin 失败 (不静默带病开工)。
        if (aio_atomic::remove_file(ps->out_dir + "/manifest.json") != 0) {
            set_error("aio_hips_product_begin: 旧完成清单不可移除: " + ps->out_dir +
                      "/manifest.json");
            return nullptr;
        }
        {
            struct { int flag; const char* name; } subs[] = {
                {AIO_HIPS_PRODUCT_SIGNAL, "signal"},
                {AIO_HIPS_PRODUCT_SUPPORT, "support"},
                {AIO_HIPS_PRODUCT_VARIANCE, "variance"},
                {AIO_HIPS_PRODUCT_IVAR, "ivar"},
                {AIO_HIPS_PRODUCT_SNR, "snr"},
                {AIO_HIPS_PRODUCT_NREJ, "nrej"},
                {AIO_HIPS_PRODUCT_NUSED, "nused"},
            };
            for (const auto& s : subs) {
                if ((flags & s.flag) == 0) continue;
                const int drc = aio_atomic::remove_tree(ps->out_dir + "/" + s.name, 0);
                if (drc != 0) {
                    set_error(std::string("aio_hips_product_begin: 残留子产品目录不可清除: ") +
                              ps->out_dir + "/" + s.name);
                    return nullptr;
                }
            }
        }
        return ps.release();

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return nullptr;
    } catch (...) {
        set_error("unknown exception");
        return nullptr;
    }
}

int aio_hips_write_signal_support_tile(AioHipsProductSet* ps,
                                       const AstroSphereTileView* view)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        g_hips_error.clear();
        if (!ps || !view) { set_error("null handle/view"); return -1; }
        if (!abi_ok_tile_view(view)) return AIO_HIPS_ABI_MISMATCH;
        if (view->width != 512 || view->leaf_order != ps->leaf_order ||
            view->data_type != ps->data_type) {
            set_error("view 与产品集不匹配 (width=512, leaf_order/ dtype 必须一致)");
            return -2;
        }
        const uint64_t npix_order = 12ULL * (1ULL << (2ULL * ps->tile_order));
        if (view->parent_ipix >= npix_order) {
            set_error("parent_ipix 超出 Norder" + std::to_string(ps->tile_order) + " 范围");
            return -3;
        }
        const size_t n = 512 * 512;
        const bool f32 = (ps->data_type == AIO_HIPS_FLOAT32);

        // 1. 转换 signal/support
        // 只分配当前 dtype 的 scratch，跨 tile 复用（原每 tile
        // 分配 4×262144 元素 → 首 tile 分配后零再分配）
        std::vector<float>&  sigF = ps->scratch_sigF;
        std::vector<double>& sigD = ps->scratch_sigD;
        std::vector<float>&  supF = ps->scratch_supF;
        std::vector<double>& supD = ps->scratch_supD;
        if (f32) { sigF.resize(n); supF.resize(n); }
        else     { sigD.resize(n); supD.resize(n); }
        std::vector<double>& sig_n = ps->scratch_sig_n;
        std::vector<double>& sup_n = ps->scratch_sup_n;
        std::vector<double>& area_n = ps->scratch_area_n;
        sig_n.resize(n);
        sup_n.resize(n);
        area_n.resize(n);
        std::vector<uint8_t> valid;
        if (view->valid_mask)
            valid.assign((const uint8_t*)view->valid_mask, (const uint8_t*)view->valid_mask + n);

        // view->flux_sum/covered_area/valid_mask 以 NESTED local
        // 索引 (Drizzle 热路径保持 NESTED), 写 FITS 前经共享 HEALPix core 标准映射
        // scatter: fits_index = (tile_width-1-x)*tile_width + y, x/y 由 local 位解交错
        const auto t_tr0 = std::chrono::steady_clock::now();
        double tile_covered = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
                (uint64_t)i, 9u, 512u);
            const bool v = valid.empty() || valid[i];
            double flux = 0.0, area = 0.0;
            if (f32) {
                if (view->flux_sum) flux = (double)((const float*)view->flux_sum)[i];
                if (view->covered_area) area = (double)((const float*)view->covered_area)[i];
            } else {
                if (view->flux_sum) flux = ((const double*)view->flux_sum)[i];
                if (view->covered_area) area = ((const double*)view->covered_area)[i];
            }
            double sig = 0.0, sup = 0.0, area_true = 0.0;
            // 覆盖与「信号是否可用」是两件事，必须解耦（DATA_SEMANTICS §4a 三态表）：
            //   无覆盖 (area<=0 / 非有限)      → support=0 ∧ signal=NaN
            //   有覆盖 ∧ 信号可用 (flux 有限)   → support=area/A_cell ∧ signal=flux/area
            //   有覆盖 ∧ 信号不可用             → support=area/A_cell ∧ signal=NaN
            // 原实现把 support 的发布条件与 flux 有限性绑死，第三态被写成 support=0，
            // 与**同一次写出的 variance=0** 自相矛盾（§4a 要求「无覆盖 ⟺ support=0」），
            // 且使覆盖面积被低估（tile_covered 少计 ⇒ ps->covered_area_sr 偏小）。
            const bool covered = v && area > 0.0 && std::isfinite(area);
            if (covered) {
                sup = area / ps->A_cell;
                area_true = area;   // 未钳制真实覆盖面积 (层级归约权重)
                if (sup > 1.0) {
                    sup = 1.0;      // I2: 发布面 support ∈ [0,1]
                    ++ps->support_clamped_pixels;
                }
                tile_covered += area;
            }
            if (covered && std::isfinite(flux)) {
                sig = flux / area;
                if (sig < ps->sig_min) ps->sig_min = sig;
                if (sig > ps->sig_max) ps->sig_max = sig;
            } else {
                sig = std::numeric_limits<double>::quiet_NaN();
            }
            // NESTED 序 sig/sup 缓存（与 FITS 序同一 float/double
            // 精度存储），hierarchy 直接按 NESTED 序累加，免 fi 反查。
            if (f32) {
                sigF[fi] = (float)sig; supF[fi] = (float)sup;
                sig_n[i] = (double)(float)sig; sup_n[i] = (double)(float)sup;
            } else {
                sigD[fi] = sig;        supD[fi] = sup;
                sig_n[i] = sig;        sup_n[i] = sup;
            }
            area_n[i] = area_true;
        }

        ps->prof_transform += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_tr0).count();

        // 2. 写 signal/support FITS (CFITSIO + checksum)
        const auto t_wr0 = std::chrono::steady_clock::now();
        const int bitpix = f32 ? -32 : -64;
        std::vector<std::pair<std::string, std::string>> cards;
        cards.push_back({"NSIDE", std::to_string(ps->nside)});
        cards.push_back({"FIRSTPIX", "0"});
        cards.push_back({"LASTPIX", std::to_string(n - 1)});
        std::string rel = tile_rel_path((int)ps->tile_order, view->parent_ipix, ".fits");
        if (ps->flags & AIO_HIPS_PRODUCT_SIGNAL) {
            std::string p = ps->out_dir + "/signal/" + rel;
            make_dirs(p.substr(0, p.find_last_of('/')));
            if (!write_fits_image(p, bitpix, 512, 512, f32 ? (const void*)sigF.data() : (const void*)sigD.data(),
                                  cards, ps->obs_title, ps->obs_filter, ps->exposure, ps->obs_date)) {
                return -4;
            }
        }
        if (ps->flags & AIO_HIPS_PRODUCT_SUPPORT) {
            std::string p = ps->out_dir + "/support/" + rel;
            make_dirs(p.substr(0, p.find_last_of('/')));
            if (!write_fits_image(p, bitpix, 512, 512, f32 ? (const void*)supF.data() : (const void*)supD.data(),
                                  cards, ps->obs_title, ps->obs_filter, ps->exposure, ps->obs_date)) {
                return -5;
            }
        }
        ps->prof_fits_write += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_wr0).count();

        // 3. MOC + 覆盖统计
        const bool new_leaf = ps->moc_cells.insert(view->parent_ipix).second;
        if (new_leaf) {
            ps->leaf_ipix_list.push_back(view->parent_ipix);
            ps->moc_area_sr += 4.0 * kPi() / (12.0 * (1ULL << (2 * ps->tile_order)));
        } else if (!hier_duplicate_allowed(ps, view->parent_ipix)) {
            return -8;   // 重复写落在已流式写出的祖先 cell 上 (见函数注释)
        }
        ps->covered_area_sr += tile_covered;

        // 4. hierarchy 累加 (k = K-1 .. 0)
        // 逐位不变: 每元素加法次数与顺序与稠密实现逐位相同; 稀疏分块只改变
        // "零从哪来" (惰性零块 vs 预置零数组), 不改变任何一次浮点运算。
        const auto t_ha0 = std::chrono::steady_clock::now();
        const HierFaults hf = hier_faults();
        for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
            int dk = (int)ps->tile_order - k;
            uint64_t shift = 2ULL * (uint64_t)dk;
            uint64_t mask = (shift >= 64) ? ~0ULL : ((1ULL << shift) - 1ULL);
            uint64_t A = view->parent_ipix >> shift;
            uint64_t s = view->parent_ipix & mask;
            AncestorAcc& acc = ps->hier[(size_t)k][A];
            if (acc.flushed) {
                // 该 cell 已写出并释放: 新贡献无处可加 (重复叶写已在上面拦截,
                // 故此处只可能是调用时序被破坏) ⇒ fail-closed, 禁静默丢贡献。
                set_error("叶 tile " + std::to_string(view->parent_ipix) +
                          " 在祖先 cell (Norder" + std::to_string(k) + " ipix=" +
                          std::to_string(A) + ") 流式写出之后到达 ⇒ 写出时序被破坏 "
                          "(variance tile 必须与其 signal tile 配对) ⇒ fail-closed");
                return -10;
            }
            acc.ensure(hf, shift >= 64 ? ~0ULL : (1ULL << shift));   // slots_total = 4^dk
            if (new_leaf) {                   // 叶槽到达计数 (重复叶写不计)
                ++acc.slots_seen;
                acc.pending_var_slot = s;     // 供 var 通道精确配对
            }
            // 循环不变量: z 位移与注入开关提出像素热循环 (零逐像素分支开销)
            const uint64_t zsh = 2ULL * (uint64_t)(ps->tile_order - (uint32_t)k);
            const bool cts = acc.cross_tile_sum;
            for (size_t i = 0; i < n; ++i) {
                // 直接使用 NESTED 序 sig/sup 缓存（与 FITS 序
                // 读回逐位一致），免每 i 一次 nested_local_to_fits_index 反查。
                const bool v = valid.empty() || valid[i];
                // M2a-H-3: 归约权重用**未钳制**真实覆盖面积 —— 钳后 sup 只用于
                // 发布; 否则父级面亮度是 sup 加权均值而非面积加权均值, 异质
                // 覆盖 (c>1) 下父级通量出现本可避免的损失。
                const double flux = sig_n[i] * area_n[i];
                const double area = area_n[i];
                if (!v || !(area > 0.0) || !std::isfinite(flux)) continue;
                // 叶 (P,l) -> A@k 内 order-(k+9) 单元 NESTED 索引
                // full = (s<<18)|l (order K+9 within A), z = full >> 2*(K-k)
                //
                // HIPS-DETERMINISM-01: s 是 parent_ipix 的低 2*dk 位 (dk=K-k),
                // 取值域恰为 [0, 4^dk), 而 i ∈ [0, 4^9); 故 (s<<18)|i 除以 2^(2*dk)
                // 后, **不同 s 落到互不相交的 z 区间** [s·4^(9-dk), (s+1)·4^(9-dk))。
                // ⇒ 每个祖先像素 z 在整个产品集生命周期内**恰好被一个叶 tile 写一次**,
                //   累加只做一次加法 ⇒ 结果与瓦片到达顺序无关 (浮点加法不满足结合律
                //   的前提在这里不成立: 没有第二个加数)。
                //   这否证了 PERF-PROFILE-01 §9.3「hierarchy 按 tile 到达序累加 ⇒
                //   低阶产品不可复现」的预警 (实测佐证见 run/HIPS-DETERMINISM-01)。
                // 注入面 cross_tile_sum 故意去掉 s 偏移, 复现「跨瓦片求和」形态以证明
                // P8 判据的判别力; 生产默认 false ⇒ 零行为差异。
                size_t z = cts ? (size_t)((uint64_t)i >> zsh)
                               : (size_t)(((s << 18ULL) | (uint64_t)i) >> zsh);
                acc.add(z, flux, area);
            }
        }
        // (B) 完备即写出 (signal/var 两处触发点都调用; 是否满足完备条件由
        // stream_flush_ready_cells 按产品位与三个累加通道的收齐状态判定 ⇒
        // 与调用时序无关)。
        if (!stream_flush_ready_cells(ps, view->parent_ipix)) return -9;
        ps->prof_hierarchy_accum += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_ha0).count();
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

// ============================================================================
// variance/ivar 叶级 Tile 写
// variance = var_num_sum / covered_area² ; ivar = 1/variance
// 三态（DATA_SEMANTICS §4a:49 / §11.2:323 / §12.4:423 / §30.4:2698）:
//   有覆盖 ∧ 方差可用 (area>0 ∧ vnum>0) -> variance=vnum/area², ivar=1/variance
//   有覆盖 ∧ 方差不可用 (area>0 ∧ vnum==0) -> variance=0 ∧ ivar=0 (显式不可用, 禁 NaN)
//   无覆盖 (covered_area<=0)               -> NaN (与 signal NaN 语义一致)
// 损坏 (vnum/area 非有限 或 vnum<0)        -> rc=-6 硬失败 (禁 clamp/禁静默跳过)
// hierarchy: 在 AncestorAcc 增加 var_num 通道, 归约公式与叶级一致
// (variance_parent = Σvar_num / (Σarea)²)
// ============================================================================
int aio_hips_write_variance_tile(AioHipsProductSet* ps,
                                 const AstroSphereTileView* view)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        g_hips_error.clear();
        if (!ps || !view) { set_error("null handle/view"); return -1; }
        if (!abi_ok_tile_view(view)) return AIO_HIPS_ABI_MISMATCH;
        if (!view->var_num_sum) { set_error("var_num_sum 为空 (无方差数据)"); return -2; }
        if (view->width != 512 || view->leaf_order != ps->leaf_order ||
            view->data_type != ps->data_type) {
            set_error("view 与产品集不匹配 (width=512, leaf_order/dtype 必须一致)");
            return -3;
        }
        const uint64_t npix_order = 12ULL * (1ULL << (2ULL * ps->tile_order));
        if (view->parent_ipix >= npix_order) {
            set_error("parent_ipix 超出 Norder" + std::to_string(ps->tile_order) + " 范围");
            return -4;
        }
        const size_t n = 512 * 512;
        const bool f32 = (ps->data_type == AIO_HIPS_FLOAT32);

        std::vector<float>&  varF  = ps->scratch_varF;
        std::vector<double>& varD  = ps->scratch_varD;
        std::vector<float>&  ivarF = ps->scratch_ivarF;
        std::vector<double>& ivarD = ps->scratch_ivarD;
        if (f32) { varF.resize(n); ivarF.resize(n); }
        else     { varD.resize(n); ivarD.resize(n); }
        std::vector<double>& var_n = ps->scratch_var_n;
        var_n.resize(n);
        std::vector<uint8_t> valid;
        if (view->valid_mask)
            valid.assign((const uint8_t*)view->valid_mask,
                         (const uint8_t*)view->valid_mask + n);

        // 有覆盖 = 该像素至少收到一个合格样本（valid ∧ covered_area>0 ∧ 有限）。
        // **不是**"方差可用"：§4a:49 明确「无方差信息像素写 variance=0 且 ivar=0」
        // 是合法产品态，故这类 tile 必须落盘（返回 0），不得按 −5 拒写
        // —— 拒写会让 support>0 的像素在阶段二变成 ivar tile 缺失（fail-closed rc=7）。
        bool any_covered = false;
        for (size_t i = 0; i < n; ++i) {
            const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
                (uint64_t)i, 9u, 512u);
            const bool v = valid.empty() || valid[i];
            double vnum = 0.0, area = 0.0;
            if (f32) {
                if (view->var_num_sum) vnum = (double)((const float*)view->var_num_sum)[i];
                if (view->covered_area) area = (double)((const float*)view->covered_area)[i];
            } else {
                if (view->var_num_sum) vnum = ((const double*)view->var_num_sum)[i];
                if (view->covered_area) area = ((const double*)view->covered_area)[i];
            }
            // ── §12.4:423 损坏判定（先于一切映射；禁 clamp、禁静默跳过）────────
            // 损坏 = **有覆盖**（area>0）而 vnum 非有限或 vnum<0（上游数值损坏，
            // 禁止被静默写成 NaN/0）。判定必须**以覆盖为前提**：无覆盖（area<=0）时
            // vnum 为 NaN 是**正确的产品态**（与 signal 面的 NaN 同态，见下方三态表
            // 第三行），不得判损坏——否则"这块天区没数据"会被误判成"产品损坏"并
            // rc=-6 硬失败。
            if (!std::isfinite(area) ||
                (area > 0.0 && (!std::isfinite(vnum) || vnum < 0.0))) {
                set_error("var_num_sum/covered_area 损坏 (vnum/area 非有限或 vnum<0) i=" +
                          std::to_string(i) + " vnum=" + std::to_string(vnum) +
                          " area=" + std::to_string(area) +
                          " ⇒ rc=-6 硬失败 (DATA_SEMANTICS §12.4:423，禁 clamp/禁静默跳过)");
                return -6;
            }
            const bool covered = v && area > 0.0;
            // variance/ivar 三态（§4a:49 / §11.2:323 / §30.4:2698）：
            //   有覆盖 ∧ 方差可用 (vnum>0) → vnum/area², 1/var
            //   有覆盖 ∧ 方差不可用        → 0 / 0（显式不可用，禁 NaN）
            //   无覆盖 (area<=0)           → NaN / NaN（与 signal NaN 同态）
            double var = std::numeric_limits<double>::quiet_NaN();
            double iv = std::numeric_limits<double>::quiet_NaN();
            if (covered) {
                any_covered = true;
                if (vnum > 0.0) {
                    var = vnum / (area * area);
                    iv = 1.0 / var;
                } else {
                    var = 0.0;
                    iv = 0.0;
                }
            }
            if (f32) {
                varF[fi] = (float)var;  ivarF[fi] = (float)iv;
                var_n[i] = (covered && vnum > 0.0) ? vnum : 0.0;
            } else {
                varD[fi] = var;         ivarD[fi] = iv;
                var_n[i] = (covered && vnum > 0.0) ? vnum : 0.0;
            }
        }
        if (!any_covered) {
            set_error("该 tile 无覆盖 (covered_area 全 0/无效) ⇒ 无有效方差数据");
            return -5;
        }

        const int bitpix = f32 ? -32 : -64;
        std::vector<std::pair<std::string, std::string>> cards;
        cards.push_back({"NSIDE", std::to_string(ps->nside)});
        cards.push_back({"FIRSTPIX", "0"});
        cards.push_back({"LASTPIX", std::to_string(n - 1)});
        std::string rel = tile_rel_path((int)ps->tile_order, view->parent_ipix, ".fits");
        if (ps->flags & AIO_HIPS_PRODUCT_VARIANCE) {
            std::string p = ps->out_dir + "/variance/" + rel;
            make_dirs(p.substr(0, p.find_last_of('/')));
            if (!write_fits_image(p, bitpix, 512, 512,
                                  f32 ? (const void*)varF.data() : (const void*)varD.data(),
                                  cards, ps->obs_title, ps->obs_filter,
                                  ps->exposure, ps->obs_date)) {
                return -6;
            }
        }
        if (ps->flags & AIO_HIPS_PRODUCT_IVAR) {
            std::string p = ps->out_dir + "/ivar/" + rel;
            make_dirs(p.substr(0, p.find_last_of('/')));
            if (!write_fits_image(p, bitpix, 512, 512,
                                  f32 ? (const void*)ivarF.data() : (const void*)ivarD.data(),
                                  cards, ps->obs_title, ps->obs_filter,
                                  ps->exposure, ps->obs_date)) {
                return -7;
            }
        }

        // MOC/覆盖: 与 signal/support 共享 (variance tile 必伴随 signal tile,
        // MOC 已在 write_signal_support_tile 登记, 不重复)

        // hierarchy: 累加 var_num (归约公式同叶级: var_parent = Σvar_num/(Σarea)²)
        // (C) 按需通道: varD 子块只在首次 var 累加时分配 ⇒ signal-only 产品
        // 不分配 variance 通道 (旧实现恒分配 2 MiB/cell)。
        const HierFaults hf = hier_faults();
        for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
            int dk = (int)ps->tile_order - k;
            uint64_t shift = 2ULL * (uint64_t)dk;
            uint64_t mask = (shift >= 64) ? ~0ULL : ((1ULL << shift) - 1ULL);
            uint64_t A = view->parent_ipix >> shift;
            uint64_t s = view->parent_ipix & mask;
            AncestorAcc& acc = ps->hier[(size_t)k][A];
            if (acc.flushed) {
                // 该 cell 已写出并释放。产品位未含 variance/ivar 时 var 数据本就
                // 不发布 (finalize 不读 var 通道) ⇒ 与旧语义无可观测差异, 静默跳过;
                // 含 variance/ivar 时该贡献会被发布面读到 ⇒ fail-closed。
                if (ps->flags & (AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR)) {
                    set_error("variance tile: 祖先 cell (Norder" + std::to_string(k) +
                              " ipix=" + std::to_string(A) +
                              ") 已完备并流式写出, 拒绝晚到的 var 贡献 (fail-closed)");
                    return -8;
                }
                continue;
            }
            acc.ensure(hf, shift >= 64 ? ~0ULL : (1ULL << shift));
            // var 通道配对计数: 只认"待配对槽"(= 最近一次 signal 写的槽) 的 var 贡献
            // ⇒ 与 signal/var 交错方式无关地精确判定 var 是否收齐。
            if (acc.pending_var_slot == s) {
                ++acc.var_slots_seen;
                acc.pending_var_slot = UINT64_MAX;
            }
            // z 映射与 signal 通道逐字同式 ⇒ 同样按 s 互斥, 每槽只加一次
            // (HIPS-DETERMINISM-01; 注入面语义见 write_signal_support_tile)。
            const uint64_t zsh = 2ULL * (uint64_t)(ps->tile_order - (uint32_t)k);
            const bool cts = acc.cross_tile_sum;
            for (size_t i = 0; i < n; ++i) {
                if (var_n[i] <= 0.0) continue;
                size_t z = cts ? (size_t)((uint64_t)i >> zsh)
                               : (size_t)(((s << 18ULL) | (uint64_t)i) >> zsh);
                acc.add_var(z, var_n[i]);
            }
        }
        // (B) 完备即写出 (var 累加之后): 见 write_signal_support_tile 注释。
        if (!stream_flush_ready_cells(ps, view->parent_ipix)) return -8;
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

// ============================================================================
// DATA-UNC-001 §30.2 (DATA-P2-REJ-001): nused/nrej 诊断统计平面 int32 叶级 Tile
//
// 值语义 = 已冻结 SCI 量的逐像素投影 (SCI-INT §5 n_used / SCI-REJ §5 kernel
// 拒绝计数), 本通道零新科学定义。dtype 固定 int32 (BITPIX=32), 无 precision
// 开关; 无覆盖像素 0/0 (int 无 NaN, 0 即"无", 禁 −1 哨兵)。
// 视图索引 = NESTED local 序 (与 signal/support/variance 同合同), 落盘前经共享
// HEALPix core 标准映射 scatter 到 FITS 行主序。
// hierarchy 低阶聚合: §30.2 未冻结诊断平面的聚合语义 → 不写 (不臆造)。
// ============================================================================
int aio_hips_write_diag_tile(AioHipsProductSet* ps,
                             const AioHipsDiagTileView* view)  {
    // P1 (R9-A) 同款 C 边界异常屏障
    try {
        g_hips_error.clear();
        if (!ps || !view) { set_error("null handle/view"); return -1; }
        if (!abi_ok_diag_view(view)) return AIO_HIPS_ABI_MISMATCH;
        if (view->width != 512 || view->leaf_order != ps->leaf_order) {
            set_error("view 与产品集不匹配 (width=512, leaf_order 必须一致)");
            return -2;
        }
        if ((ps->flags & (AIO_HIPS_PRODUCT_NREJ | AIO_HIPS_PRODUCT_NUSED)) == 0) {
            set_error("产品集 flags 未启用 nused/nrej 诊断平面通道 (位 32/64)");
            return -4;
        }
        const uint64_t npix_order = 12ULL * (1ULL << (2ULL * ps->tile_order));
        if (view->parent_ipix >= npix_order) {
            set_error("parent_ipix 超出 Norder" + std::to_string(ps->tile_order) + " 范围");
            return -3;
        }
        const bool want_nrej = (ps->flags & AIO_HIPS_PRODUCT_NREJ) != 0;
        const bool want_nused = (ps->flags & AIO_HIPS_PRODUCT_NUSED) != 0;
        // 已启用通道必须有数据 (禁写空占位/静默跳过)
        if ((want_nrej && !view->nrej) || (want_nused && !view->nused)) {
            set_error("诊断平面数据为空 (已启用通道的 nrej/nused 指针为 NULL)");
            return -2;
        }
        const size_t n = 512 * 512;
        // §30.2 值域守卫: 计数平面恒 >= 0 (0 即"无"; 禁 −1 哨兵)。
        // 注入面 ASTROCS_HIPS_DIAG_FAULT=sentinel 故意跳过守卫并写回 −1 占位,
        // 用于证明下游 verify/断言对"哨兵污染"具备判别力。
        const bool inj_sentinel = fault_injected("ASTROCS_HIPS_DIAG_FAULT", "sentinel");
        if (!inj_sentinel) {
            for (int ch = 0; ch < 2; ++ch) {
                const int32_t* src = ch == 0 ? view->nrej : view->nused;
                if (!src) continue;
                for (size_t i = 0; i < n; ++i) {
                    if (src[i] < 0) {
                        set_error(std::string("诊断平面 ") +
                                  (ch == 0 ? "nrej" : "nused") +
                                  " 出现负值 (禁 −1 哨兵): index " +
                                  std::to_string(i));
                        return -5;
                    }
                }
            }
        }
        std::vector<std::pair<std::string, std::string>> cards;
        cards.push_back({"NSIDE", std::to_string(ps->nside)});
        cards.push_back({"FIRSTPIX", "0"});
        cards.push_back({"LASTPIX", std::to_string(n - 1)});
        const std::string rel =
            tile_rel_path((int)ps->tile_order, view->parent_ipix, ".fits");
        const bool inj_skip = fault_injected("ASTROCS_HIPS_DIAG_FAULT", "skip_write");
        for (int ch = 0; ch < 2; ++ch) {
            const bool want = ch == 0 ? want_nrej : want_nused;
            if (!want) continue;
            const int32_t* src = ch == 0 ? view->nrej : view->nused;
            std::vector<int32_t>& buf =
                ch == 0 ? ps->scratch_diag_nrej : ps->scratch_diag_nused;
            buf.resize(n);
            for (size_t i = 0; i < n; ++i) {
                const uint64_t fi = astrocs::healpix::nested_local_to_fits_index(
                    (uint64_t)i, 9u, 512u);
                int32_t v = src[i];
                if (inj_sentinel && v == 0) v = -1;   // 等价缺陷: 0 → −1 哨兵
                buf[fi] = v;
            }
            // 注入面 ASTROCS_HIPS_DIAG_FAULT=skip_write: 登记通道但静默不落盘
            // (等价缺陷 = "声明了产品却零文件"; 由 verify V4 声明↔事实断言捕获)
            if (ch == 0) ps->diag_nrej_tiles = true;
            else         ps->diag_nused_tiles = true;
            if (inj_skip) continue;
            const std::string dir = ch == 0 ? "/nrej/" : "/nused/";
            std::string p = ps->out_dir + dir + rel;
            make_dirs(p.substr(0, p.find_last_of('/')));
            if (!write_fits_image(p, 32, 512, 512, (const void*)buf.data(),
                                  cards, ps->obs_title, ps->obs_filter,
                                  ps->exposure, ps->obs_date)) {
                return -6;
            }
        }
        // MOC 覆盖登记 (与 signal/support 同一父单元集合; 集合去重幂等)。
        // 诊断平面共享产品集 MOC 语义: 有数据的 tile 才登记。
        if (ps->moc_cells.insert(view->parent_ipix).second) {
            ps->leaf_ipix_list.push_back(view->parent_ipix);
            ps->moc_area_sr += 4.0 * kPi() / (12.0 * (1ULL << (2 * ps->tile_order)));
        }
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

int aio_hips_write_snr_points(AioHipsProductSet* ps,
                              const AioHipsSnrPoint* pts,
                              int n)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        g_hips_error.clear();
        if (!ps || (!pts && n > 0)) { set_error("null pts"); return -1; }
        if (n < 0) { set_error("n < 0"); return -1; }
        // 逐元素 ABI 校验 (V11-N-01): 元素步长由结构自描述 = sizeof(AioHipsSnrPoint);
        // 任一元素版本/尺寸不符即整体拒绝, 不做部分写入。
        for (int i = 0; i < n; ++i)
            if (!abi_ok_snr_point(&pts[i], i)) return AIO_HIPS_ABI_MISMATCH;
        for (int i = 0; i < n; ++i)
            ps->snr.push_back(pts[i]);
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

// ---------------------------------------------------------------------------
// 内部: moc_sky_fraction 的**唯一**字面量格式化函数 (properties 与
// manifest.json 共用)。%.17g = DBL_DECIMAL_DIG, 对任意 double 满足
// strtod(snprintf("%.17g", v)) == v (C11 5.2.4.2.2 / IEEE-754 十进制往返),
// 故 §9 的"绝对误差 <1e-9"与"键值精确相等"两条同时无条件成立, 且不随 K、
// 覆盖率、平台变化。修复前 properties 走 std::to_string (6dp, 半量化步长
// 5e-7 = 容差 500 倍)、manifest 走 %.8f (5e-9 = 5 倍) ⇒ 双面字面量分叉。
// ---------------------------------------------------------------------------
static std::string fmt_sky_fraction(double v) {
    char b[40];
    std::snprintf(b, sizeof(b), "%.17g", v);
    return std::string(b);
}

// ---------------------------------------------------------------------------
// 内部: 写一个子产品的 properties / metadata / MOC / hierarchy
// ---------------------------------------------------------------------------
// value_dtype: 该子产品像素值 dtype 标签 ("float32"/"float64"/"int32")
// is_diag: 诊断统计平面 (int32, §30.2) —— 额外写 astrocs_diag_dtype 登记面
static bool finalize_image_product(AioHipsProductSet* ps,
                                   const std::string& prod,
                                   const std::string& subtype,
                                   const std::string& data_range,
                                   double moc_frac,
                                   double covered_frac,
                                   const char* value_dtype = nullptr,
                                   bool is_diag = false) {
    const std::string dir = ps->out_dir + "/" + prod;
    make_dirs(dir);
    char buf[64];
    // M2b-B-03: IVOA REC-HIPS-1.0 §4.4.1 定义 hips_pixel_scale/s_pixel_scale 单位为
    // **度** (示例 8.946E-4)。叶像素角尺度解析式 = (180/π)·sqrt(π/3)/nside [deg];
    // 旧实现多乘 3600 写成角秒, 与同名标准键相差 3600×。
    std::snprintf(buf, sizeof(buf), "%.6f", 180.0 / kPi() * std::sqrt(kPi() / 3.0) / (double)ps->nside);
    std::vector<std::pair<std::string, std::string>> kv;
    kv.push_back({"creator_did", ps->creator_did});
    kv.push_back({"obs_title", ps->obs_title});
    kv.push_back({"obs_creator", "AstroCS"});
    kv.push_back({"hips_version", "1.4"});
    kv.push_back({"hips_order", std::to_string(ps->tile_order)});
    kv.push_back({"hips_tile_width", "512"});
    // P0-19 (IVOA HiPS 1.0 §4.4.1 关键字表): hips_frame 标准值域 =
    // {equatorial, galactic, ecliptic}; ICRS 参考系的标准写法是 "equatorial"
    // (规范原文: Format: "equatorial" (ICRS))。"icrs" 不是标准取值。
    // 三方取证 (2026-09-18): IVOA PR-HIPS-1.0-20170406 §4.4.1 / Hipsgen 手册 /
    // CDS Aladin Lite API 均为 equatorial; 生产 HiPS (CDS DSS/2MASS) 亦然。
    // M1a-B-005 曾把标准值/非标准值判反并写成 "icrs", 本处订正。
    // 本管道内部 frame 恒为 ICRS (SCI-P3-001 §4 "合法转换 = 仅恒等 ICRS"),
    // 故写标准值 "equatorial"; 读侧 (io/hips_core.c) 保留 "icrs" 作旧产品
    // 兼容别名。
    kv.push_back({"hips_frame", "equatorial"});
    // B2-A8: HiPS 1.0 tile 编号方案显式声明。消费者（Phase2 coverage union
    // 的 NESTED 父聚合 t>>2s）不得再依赖"缺省即 NESTED"的隐式约定。
    kv.push_back({"hips_ordering", "NESTED"});
    kv.push_back({"dataproduct_type", "image"});
    kv.push_back({"dataproduct_subtype", subtype});
    kv.push_back({"hips_tile_format", "fits"});
    kv.push_back({"hips_status", "private master"});
    kv.push_back({"hips_creator", "AstroCS (astro_image_io)"});
    kv.push_back({"hips_builder", "AstroCS aio_hips_writer (CFITSIO 4.6.4)"});
    kv.push_back({"hips_estsize", "1000000"});
    // META-001: 真实 UTC finalize 时间, 禁止硬编码日期
    char rel_date[32], cre_date[40];
    utc_now_date(rel_date, sizeof(rel_date));
    utc_now_iso(cre_date, sizeof(cre_date));
    kv.push_back({"hips_release_date", rel_date});
    kv.push_back({"hips_creation_date", cre_date});
    kv.push_back({"obs_description", "AstroCS Phase1 single-frame HiPS product"});
    kv.push_back({"prov_progenitor", "ivo://astrocs/phase1/drizzle"});
    // （K_CORR_DOMAIN）：Drizzle provenance → sampler 按帧 k_corr
    if (ps->drizzle_prov_set) {
        char pf[32], sc[32];
        std::snprintf(pf, sizeof(pf), "%.6f", ps->drizzle_pixfrac);
        kv.push_back({"ASTROCS_DRIZZLE_PIXFRAC", pf});
        // 通道 = 全或无: setter 已把 scale 合法域收紧为 (0, 824.5167388361774"],
        // 故 drizzle_prov_set 为真 ⇒ 两键必须齐备 (禁"接受 0 但静默不写键")。
        std::snprintf(sc, sizeof(sc), "%.4f", ps->drizzle_scale_arcsec);
        kv.push_back({"ASTROCS_DRIZZLE_SCALE_ARCSEC", sc});
    }
    // RELEASE-02 SD-15: 帧级未加权通量型 SNR（F_ref/σ_F）与公共参考通量 F_ref。
    // 唯一消费者 = Phase2 权重链（w = SNR²/F_ref² = 1/σ_F²; weight-chain-report
    // §6）。全或无: setter 未调用 → 两键整体不写。%.17g 保证 double round-trip
    // 精确（逐帧 F_ref 一致性门 rtol 1e-9 依赖此精度）。
    if (ps->frame_snr_set) {
        char fs[64], rf[64];
        std::snprintf(fs, sizeof(fs), "%.17g", ps->frame_snr);
        std::snprintf(rf, sizeof(rf), "%.17g", ps->reference_flux);
        kv.push_back({"ASTROCS_FRAME_SNR", fs});
        kv.push_back({"ASTROCS_REFERENCE_FLUX", rf});
    }
    kv.push_back({"obs_regime", "optical"});
    // META-002: 无真实 passband/系统响应波长范围时不伪造 em_min/em_max
    kv.push_back({"hips_hierarchy", "true"});
    kv.push_back({"hips_pixel_scale", buf});
    kv.push_back({"hips_initial_fov", "60"});
    kv.push_back({"moc_sky_fraction", fmt_sky_fraction(moc_frac)});
    kv.push_back({"astrocs_covered_sky_fraction", std::to_string(covered_frac)});
    // M2a-H-3 可观测钳制计数: 叶级 support 钳制像素数 + 层级 Σarea>A_cell_k
    // 像素数 (编码限"不可复原"从不可观测变为可测量)。
    kv.push_back({"astrocs_support_clamped_pixels", std::to_string(ps->support_clamped_pixels)});
    kv.push_back({"astrocs_coverage_gt1_pixels", std::to_string(ps->coverage_gt1_pixels)});
    kv.push_back({"astrocs_signal_dtype", ps->data_type == AIO_HIPS_FLOAT32 ? "float32" : "float64"});
    // DATA-UNC-001 §30.2: 诊断统计平面固定 int32 (无 precision 开关)
    if (is_diag)
        kv.push_back({"astrocs_diag_dtype", value_dtype ? value_dtype : "int32"});
    // DATA-UNC-001 §30.3 (DATA-P2-PROV-001) provenance 四键 (原五键
    // 中的旧「权重模式」键已按 §9.73 A44 删除, 见 aio_hips.h)。
    // 全或无 (§30.3 冻结键名): prov_set=false → 四键整体不写 (legacy 面不变);
    // prov_set=true → 四键齐备, 禁静默缺键 (注入面 ASTROCS_HIPS_PROV_FAULT=
    // missing_key 故意漏写一键, 用于证明"缺键"断言有判别力)。
    if (ps->prov_set) {
        if (!fault_injected("ASTROCS_HIPS_PROV_FAULT", "missing_key"))
            kv.push_back({"ASTROCS_INPUT_MANIFEST_HASH", ps->prov_manifest_hash});
        kv.push_back({"ASTROCS_MODEL_HASH", ps->prov_model_hash});
        kv.push_back({"ASTROCS_UNCERTAINTY_AVAILABLE",
                      ps->prov_uncertainty_available ? "true" : "false"});
        kv.push_back({"ASTROCS_REJECT_PROFILE", ps->prov_reject_profile});
    }
    if (!data_range.empty()) kv.push_back({"hips_data_range", data_range});
    // B2-A8: obs_filter 恒写出（含空值）。旧实现仅写非空值，使"未声明
    // passband"与"声明空 passband"在 properties 上不可区分，Phase2
    // coverage 的 filter 组校验无从 fail-closed（DISP-COV-003）。空值 =
    // 显式声明该产品无 filter 身份，仍参与跨帧全等比较。
    kv.push_back({"obs_filter", ps->obs_filter});
    if (ps->exposure > 0.0) kv.push_back({"obs_exptime", std::to_string(ps->exposure)});
    if (!ps->obs_date.empty()) kv.push_back({"obs_date", ps->obs_date});
    if (!ps->obs_date.empty()) {
        double t0 = 0.0;
        if (iso_to_mjd(ps->obs_date, t0)) {
            char b0[32], b1[32];
            std::snprintf(b0, sizeof(b0), "%.8f", t0);
            std::snprintf(b1, sizeof(b1), "%.8f", t0 + ps->exposure / 86400.0);
            kv.push_back({"t_min", b0});
            kv.push_back({"t_max", b1});
        }
    }
    if (!write_properties(dir + "/properties", kv)) return false;
    // metadata.fits (产品级) 由 Moc.fits 提供结构; 再写一份极简 metadata.fits
    {
        int status = 0;
        fitsfile* fptr = nullptr;
        std::string mp = dir + "/metadata.fits";
        std::replace(mp.begin(), mp.end(), '\\', '/');
        std::remove(mp.c_str());
        if (!fits_create_file(&fptr, mp.c_str(), &status)) {
            // RESCUE-FD-04: 首个 HDU 必须由 fits_create_img 建出标准主头
            // (SIMPLE=T/BITPIX/NAXIS/EXTEND/END)。直接在裸 create_file 上写键
            // 会产出缺 SIMPLE 卡的非法 FITS (astropy: No SIMPLE card found),
            // 破坏 FITS 标准互操作。NAXIS=0 + BITPIX=8 = 无数据面的头承载 HDU;
            // 既有键 (PIXTYPE/ORDERING/NSIDE/HIPSTILEWIDTH/DATAPRODTYPE) 语义不变。
            if (fits_create_img(fptr, BYTE_IMG, 0, nullptr, &status)) {
                fits_close_file(fptr, &status);
                return fits_ok(status, "metadata fits_create_img " + mp);
            }
            fits_write_key_str(fptr, "PIXTYPE", (char*)"HEALPIX", nullptr, &status);
            fits_write_key_str(fptr, "ORDERING", (char*)"NESTED", nullptr, &status);
            fits_write_key_lng(fptr, "NSIDE", (long)ps->nside, nullptr, &status);
            fits_write_key_lng(fptr, "HIPSTILEWIDTH", 512L, nullptr, &status);
            fits_write_key_str(fptr, "DATAPRODTYPE", (char*)"image", nullptr, &status);
            fits_close_file(fptr, &status);
        }
    }
    // MOC
    std::vector<uint64_t> uniq;
    uniq.reserve(ps->moc_cells.size());
    for (uint64_t c : ps->moc_cells) uniq.push_back(4ULL * (1ULL << (2ULL * ps->moc_order)) + (c >> (2ULL * ((uint64_t)ps->tile_order - ps->moc_order))));
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    if (!write_moc_fits(dir + "/Moc.fits", uniq, ps->moc_order)) return false;
    return true;
}

// hierarchy: 从 order K-1 到 0 逐级写出 (未流式写出的 cell)。
// MEM-DESIGN-01: 与流式写出共用 write_hierarchy_cell ⇒ 两种时序产物逐位相同;
// 每个 cell 写出后立即 release() (finalize 阶段峰值不再整层常驻)。
static bool finalize_hierarchy(AioHipsProductSet* ps) {
    for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
        for (auto& kv : ps->hier[(size_t)k]) {
            AncestorAcc& acc = kv.second;
            if (acc.flushed) continue;   // 已在写叶过程中流式写出并释放
            if (!write_hierarchy_cell(ps, k, kv.first, acc)) return false;
            acc.release();
        }
    }
    return true;
}

// SNR Catalogue HiPS: TSV tiles + properties + metadata.xml
static bool finalize_snr_product(AioHipsProductSet* ps) {
    if (ps->snr.empty()) return true;
    const std::string dir = ps->out_dir + "/snr";
    make_dirs(dir);
    std::map<uint64_t, std::vector<const AioHipsSnrPoint*>> by_cell;
    std::set<uint64_t> cells;
    for (const auto& p : ps->snr) {
        // 共享 HEALPix core: 不再维护 AIO 私有 ang2ipix
        uint64_t ip = astrocs::healpix::ang2pix_nest(1u << ps->tile_order,
                                                     p.ra_deg, p.dec_deg);
        by_cell[ip].push_back(&p);
        cells.insert(ip);
    }
    const char* header =
        "# star_id ra dec snr quality_flags photometric_status\n";
    for (auto& kv : by_cell) {
        std::string rel = tile_rel_path((int)ps->tile_order, kv.first, ".tsv");
        std::string p = dir + "/" + rel;
        make_dirs(p.substr(0, p.find_last_of('/')));
        FILE* f = std::fopen(p.c_str(), "wb");
        if (!f) { set_error("无法创建 SNR tile: " + p); return false; }
        std::fputs(header, f);
        // SNR-PREC-001: FP32 -> %.9g (float32 round-trip),
        // FP64 -> %.17g (float64 round-trip), 不再使用 %.6f
        const char* snr_fmt = (ps->data_type == AIO_HIPS_FLOAT32) ? "%.9g" : "%.17g";
        char line_fmt[64];
        std::snprintf(line_fmt, sizeof(line_fmt), "%%lld %%.12f %%.12f %s %%u %%u\n", snr_fmt);
        for (const AioHipsSnrPoint* sp : kv.second) {
            // 真实 star_id / quality_flags / photometric_status (禁止硬编码)
            std::fprintf(f, line_fmt,
                         (long long)sp->star_id, sp->ra_deg, sp->dec_deg, sp->snr,
                         sp->quality_flags, sp->photometric_status);
        }
        std::fclose(f);
    }
    std::vector<std::pair<std::string, std::string>> kv2;
    kv2.push_back({"creator_did", ps->creator_did});
    kv2.push_back({"obs_title", ps->obs_title + " (SNR catalogue)"});
    kv2.push_back({"hips_version", "1.4"});
    kv2.push_back({"hips_order", std::to_string(ps->tile_order)});
    // P0-19: 同图产品面, 写 IVOA HiPS 1.0 §4.4.1 标准值 "equatorial"
    // (ICRS 的标准写法; "icrs" 非标准, 读侧仅作兼容别名)。
    kv2.push_back({"hips_frame", "equatorial"});
    kv2.push_back({"dataproduct_type", "catalog"});
    kv2.push_back({"dataproduct_subtype", "snr"});
    kv2.push_back({"hips_tile_format", "tsv"});
    kv2.push_back({"hips_status", "private master"});
    kv2.push_back({"hips_creator", "AstroCS (astro_image_io)"});
    kv2.push_back({"hips_builder", "AstroCS aio_hips_writer (CFITSIO 4.6.4)"});
    // META-001: 真实 UTC finalize 时间, 禁止硬编码日期
    char rel_date2[32], cre_date2[40];
    utc_now_date(rel_date2, sizeof(rel_date2));
    utc_now_iso(cre_date2, sizeof(cre_date2));
    kv2.push_back({"hips_release_date", rel_date2});
    kv2.push_back({"hips_creation_date", cre_date2});
    kv2.push_back({"obs_description", "AstroCS Phase1 single-frame SNR catalogue HiPS product"});
    kv2.push_back({"prov_progenitor", "ivo://astrocs/phase1/drizzle"});
    kv2.push_back({"obs_regime", "optical"});
    // META-002: 无真实 passband/系统响应波长范围时不伪造 em_min/em_max
    if (!ps->obs_date.empty()) {
        double t0 = 0.0;
        if (iso_to_mjd(ps->obs_date, t0)) {
            char b0[32], b1[32];
            std::snprintf(b0, sizeof(b0), "%.8f", t0);
            std::snprintf(b1, sizeof(b1), "%.8f", t0 + ps->exposure / 86400.0);
            kv2.push_back({"t_min", b0});
            kv2.push_back({"t_max", b1});
        }
    }
    kv2.push_back({"hips_initial_fov", "60"});
    kv2.push_back({"moc_sky_fraction",
        fmt_sky_fraction((double)cells.size() * 4.0 * kPi() /
                         (12.0 * (1ULL << (2ULL * ps->tile_order))) / (4.0 * kPi()))});
    kv2.push_back({"hips_cat_nrows", std::to_string(ps->snr.size())});
    // hips_initial_ra/dec: 由真实 SNR 源位置中位数推导（单帧场中心近似，非伪造）
    if (!ps->snr.empty()) {
        std::vector<double> ra_s, dec_s;
        ra_s.reserve(ps->snr.size());
        dec_s.reserve(ps->snr.size());
        for (const auto& p : ps->snr) {
            ra_s.push_back(p.ra_deg);
            dec_s.push_back(p.dec_deg);
        }
        std::sort(ra_s.begin(), ra_s.end());
        std::sort(dec_s.begin(), dec_s.end());
        kv2.push_back({"hips_initial_ra", std::to_string(ra_s[ra_s.size() / 2])});
        kv2.push_back({"hips_initial_dec", std::to_string(dec_s[dec_s.size() / 2])});
    }
    if (!write_properties(dir + "/properties", kv2)) return false;
    {
        // §9 原子产品: metadata.xml 统一走 tmp → fsync → 原子 rename (AIO-001 原语)。
        std::string merr;
        const int mrc = aio_atomic::write_file_atomic_stream(
            dir + "/metadata.xml",
            [&](FILE* f) -> bool {
        // IVOA HiPS Catalog: metadata.xml 必须是 VOTable（Hipsgen LINT[4.4.3] 要求根元素 votable）
        std::fprintf(f,
            "<?xml version=\"1.0\"?>\n"
            // M2b-B-05: xmlns/schemaLocation 必须是合法 URI —— 旧串在 "http://"
            // 后带字面空格, 产物根元素不被 XML 解析器识别为 VOTable。
            "<VOTABLE version=\"1.3\" xmlns=\"http://www.ivoa.net/xml/VOTable/v1.3\"\n"
            "         xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
            "         xsi:schemaLocation=\"http://www.ivoa.net/xml/VOTable/v1.3 http://www.ivoa.net/xml/VOTable/v1.3\">\n"
            "  <RESOURCE type=\"meta\">\n"
            "    <TABLE>\n"
            "      <FIELD name=\"star_id\" datatype=\"long\" ucd=\"meta.id\"/>\n"
            "      <FIELD name=\"ra\" datatype=\"double\" unit=\"deg\" ucd=\"pos.eq.ra\"/>\n"
            "      <FIELD name=\"dec\" datatype=\"double\" unit=\"deg\" ucd=\"pos.eq.dec\"/>\n"
            "      <FIELD name=\"snr\" datatype=\"%s\" ucd=\"stat.snr\"/>\n"
            "      <FIELD name=\"quality_flags\" datatype=\"int\" ucd=\"meta.code.qual\"/>\n"
            "      <FIELD name=\"photometric_status\" datatype=\"int\" ucd=\"meta.code.status\"/>\n"
            "    </TABLE>\n"
            "  </RESOURCE>\n"
            "</VOTABLE>\n",
            ps->data_type == AIO_HIPS_FLOAT32 ? "float" : "double");
                return true;
            },
            &merr);
        if (mrc != 0) {
            set_error("无法原子创建 SNR metadata.xml: " + dir + "/metadata.xml (" + merr + ")");
            return false;
        }
    }
    std::vector<uint64_t> uniq;
    for (uint64_t c : cells)
        uniq.push_back(4ULL * (1ULL << (2ULL * ps->moc_order)) + (c >> (2ULL * ((uint64_t)ps->tile_order - ps->moc_order))));
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    if (!write_moc_fits(dir + "/Moc.fits", uniq, ps->moc_order)) return false;
    return true;
}

// （K_CORR_DOMAIN 选项 B）：Drizzle provenance setter
int aio_hips_set_drizzle_provenance(AioHipsProductSet* ps,
                                    double pixfrac, double scale_arcsec)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        if (!ps) return 1;
        if (!(pixfrac > 0.0 && pixfrac <= 1.0)) {
            set_error("drizzle pixfrac 必须在 (0,1] (禁 0/负/NaN/Inf/>1)");
            return 2;
        }
        if (!std::isfinite(scale_arcsec)) {
            set_error("drizzle scale_arcsec 非有限 (NaN/Inf) 不在合法域");
            return 2;
        }
        if (!(scale_arcsec > 0.0)) {
            set_error("drizzle scale_arcsec 必须 > 0 (0 不是\"未知\"哨兵: 尺度未知"
                      "时不得调用本 setter, provenance 通道为全或无)");
            return 2;
        }
        if (scale_arcsec > ACS_HIPS_MAX_FRAME_SCALE_ARCSEC) {
            set_error("drizzle scale_arcsec 超出物理域 (> 2×412.258369\" = "
                      "824.5167388361774\", 叶 nside>=512 + SCI-DRZ-001 1-2× 过采样)");
            return 2;
        }
        ps->drizzle_prov_set = true;
        ps->drizzle_pixfrac = pixfrac;
        ps->drizzle_scale_arcsec = scale_arcsec;
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

// ── RELEASE-02 SD-15 帧级 SNR setter（ASTROCS_FRAME_SNR/REFERENCE_FLUX）─────
// 全或无 + 禁伪造: 任一参数非有限/≤0 → 返回非 0 且不置 frame_snr_set。
int aio_hips_set_frame_snr(AioHipsProductSet* ps, double frame_snr,
                           double reference_flux)  {
    // P1 (R9-A) 同款 C 边界异常屏障
    try {
        if (!ps) return 1;
        if (!std::isfinite(frame_snr) || !(frame_snr > 0.0)) {
            set_error("frame_snr 必须有限且 > 0（帧级未加权通量型 SNR F_ref/σ_F;"
                      " 非信噪比/非权重不得写入）");
            return 2;
        }
        if (!std::isfinite(reference_flux) || !(reference_flux > 0.0)) {
            set_error("reference_flux 必须有限且 > 0（组内公共 F_ref; 0 不是"
                      "\"未知\"哨兵——未知时不得调用本 setter）");
            return 2;
        }
        ps->frame_snr_set = true;
        ps->frame_snr = frame_snr;
        ps->reference_flux = reference_flux;
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

// ── DATA-UNC-001 §30.3 (DATA-P2-PROV-001) provenance 四键 setter ────────────
// 全或无: 参数任一不合法 → 返回非 0 且不置 prov_set (调用方得不到半套 provenance)。
// 值语义校验面向"禁伪造": 两个 hash 必须 64 hex (§20.3 sha256 十六进制形态),
// reject_profile 非空, uncertainty_available ∈ {0,1}。
// §9.73 A44: 旧「权重模式」形参与其 0/1/2 值域校验已删除。
static bool is_sha256_hex(const char* s) {
    if (!s) return false;
    size_t n = 0;
    for (const char* p = s; *p; ++p, ++n) {
        const char c = *p;
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                         (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return n == 64;
}

int aio_hips_set_provenance(AioHipsProductSet* ps,
                            const char* input_manifest_hash,
                            const char* model_hash,
                            int uncertainty_available,
                            const char* reject_profile)  {
    // P1 (R9-A) 同款 C 边界异常屏障
    try {
        if (!ps) return 1;
        if (!is_sha256_hex(input_manifest_hash)) {
            set_error("provenance: input_manifest_hash 必须为 64 hex sha256 (§20.3)");
            return 2;
        }
        if (!is_sha256_hex(model_hash)) {
            set_error("provenance: model_hash 必须为 64 hex sha256");
            return 2;
        }
        if (uncertainty_available != 0 && uncertainty_available != 1) {
            set_error("provenance: uncertainty_available 必须为 0/1 (§30.1 判定结果)");
            return 2;
        }
        if (!reject_profile || !*reject_profile) {
            set_error("provenance: reject_profile 必须为非空版本化 profile 串");
            return 2;
        }
        ps->prov_set = true;
        ps->prov_manifest_hash = input_manifest_hash;
        ps->prov_model_hash = model_hash;
        ps->prov_uncertainty_available = uncertainty_available;
        ps->prov_reject_profile = reject_profile;
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

int aio_hips_finalize(AioHipsProductSet* ps)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        g_hips_error.clear();
        if (!ps) { set_error("null handle"); return -1; }
        if (ps->finalized) { set_error("已 finalize"); return -2; }
        ps->finalized = true;
        // finalize 分段计时（粗粒度，低开销）
        const auto t_fin0 = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[hips] finalize: n_leaf=%zu flags=%d\n",
                     ps->leaf_ipix_list.size(), ps->flags);
        // DATA-UNC-001 §30.1/§30.3 双向一致性守卫 (fail-closed, 禁占位/禁静默):
        //   uncertainty_available=true  ⇒ variance|ivar 两位必须同时置位
        //   uncertainty_available=false ⇒ 两位必须同时不置位 (禁占位子产品)
        if (ps->prov_set) {
            const bool unc = ps->prov_uncertainty_available != 0;
            const int vf = ps->flags & (AIO_HIPS_PRODUCT_VARIANCE |
                                        AIO_HIPS_PRODUCT_IVAR);
            const int both = AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR;
            if (unc && vf != both) {
                set_error("provenance: uncertainty_available=true 要求"
                          " variance|ivar 子产品位同时置位 (§30.1)");
                return -9;
            }
            if (!unc && vf != 0) {
                set_error("provenance: uncertainty_available=false 禁止 variance/"
                          "ivar 子产品位 (禁占位, §30.1 unavailable 规则)");
                return -10;
            }
        }
        const auto t_p0 = std::chrono::steady_clock::now();
        const double moc_frac = ps->moc_area_sr / (4.0 * kPi());
        const double cov_frac = ps->covered_area_sr / (4.0 * kPi());
        // M2a-H-3 可观测计数: properties 在 hierarchy 写出**之前**落盘, 故先由
        // 累加器统计"Σ未钳制覆盖面积 > A_cell_k"的父像素数 (与 finalize_hierarchy
        // 的发布面钳制逐像素一致), 使编码限从不可观测变为可测量。
        // MEM-DESIGN-01: 流式写出的 cell 在写出时刻已用同一 count_coverage_gt1
        // 计入, 此处只补**未写出**的 cell ⇒ 与"全部 cell 一次性全扫描"同值
        // (纯计数, 与扫描顺序无关; 稀疏实现只遍历已分配子块, 未分配块恒零)。
        for (int k = (int)ps->tile_order - 1; k >= 0; --k) {
            for (auto& hkv : ps->hier[(size_t)k]) {
                AncestorAcc& acc = hkv.second;
                if (acc.flushed) continue;
                ps->coverage_gt1_pixels += count_coverage_gt1(acc, k);
            }
        }
        std::string range;
        if (ps->sig_min <= ps->sig_max)
            range = std::to_string(ps->sig_min) + " " + std::to_string(ps->sig_max);
        if (ps->flags & AIO_HIPS_PRODUCT_SIGNAL) {
            std::fprintf(stderr, "[hips] finalize: signal product\n");
            if (!finalize_image_product(ps, "signal", "surface brightness", range, moc_frac, cov_frac)) {
                return -3;
            }
        }
        if (ps->flags & AIO_HIPS_PRODUCT_SUPPORT) {
            std::fprintf(stderr, "[hips] finalize: support product\n");
            if (!finalize_image_product(ps, "support", "coverage fraction", "", moc_frac, cov_frac)) {
                return -4;
            }
        }
        // variance/ivar 产品 (Drizzle 方差传播)
        if (ps->flags & AIO_HIPS_PRODUCT_VARIANCE) {
            std::fprintf(stderr, "[hips] finalize: variance product\n");
            if (!finalize_image_product(ps, "variance", "variance", "", moc_frac, cov_frac)) {
                return -7;
            }
        }
        if (ps->flags & AIO_HIPS_PRODUCT_IVAR) {
            std::fprintf(stderr, "[hips] finalize: ivar product\n");
            if (!finalize_image_product(ps, "ivar", "inverse variance", "", moc_frac, cov_frac)) {
                return -8;
            }
        }
        // DATA-UNC-001 §30.2 诊断统计平面 (int32; 只对真正写过 tile 的通道 finalize,
        // 禁"声明但零数据"的空产品占位)
        if ((ps->flags & AIO_HIPS_PRODUCT_NREJ) && ps->diag_nrej_tiles) {
            std::fprintf(stderr, "[hips] finalize: nrej diagnostic product\n");
            if (!finalize_image_product(ps, "nrej", "rejected sample count", "",
                                        moc_frac, 0.0, "int32", true)) {
                return -11;
            }
        }
        if ((ps->flags & AIO_HIPS_PRODUCT_NUSED) && ps->diag_nused_tiles) {
            std::fprintf(stderr, "[hips] finalize: nused diagnostic product\n");
            if (!finalize_image_product(ps, "nused", "used sample count", "",
                                        moc_frac, 0.0, "int32", true)) {
                return -12;
            }
        }
        ps->prof_finalize_products += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_p0).count();
        const auto t_h0 = std::chrono::steady_clock::now();
        if ((ps->flags & (AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
                          AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR)) &&
            !finalize_hierarchy(ps)) {
            std::fprintf(stderr, "[hips] finalize: hierarchy failed\n");
            return -5;
        }
        ps->prof_hierarchy_write += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_h0).count();
        const auto t_s0 = std::chrono::steady_clock::now();
        if ((ps->flags & AIO_HIPS_PRODUCT_SNR) && !finalize_snr_product(ps)) {
            std::fprintf(stderr, "[hips] finalize: snr failed\n");
            return -6;
        }
        ps->prof_finalize_snr += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_s0).count();
        std::fprintf(stderr, "[hips] finalize: ok\n");
        std::fprintf(stderr,
                     "[hips][profile] transform=%.3fs fits_write=%.3fs "
                     "hierarchy_accum=%.3fs products=%.3fs hierarchy_write=%.3fs "
                     "snr=%.3fs total=%.3fs\n",
                     ps->prof_transform, ps->prof_fits_write,
                     ps->prof_hierarchy_accum, ps->prof_finalize_products,
                     ps->prof_hierarchy_write, ps->prof_finalize_snr,
                     std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - t_fin0).count());
        // manifest.json = 产品集完成标记。§9 原子产品: 统一 tmp → fsync → rename,
        // 失败即 fail-closed (不得静默留下/缺失半成品 manifest)。
        {
            std::string merr;
            const int mrc = aio_atomic::write_file_atomic_stream(
                ps->out_dir + "/manifest.json",
                [&](FILE* f) -> bool {
                std::string prod_list;
                // 诊断平面只在真正写过 tile 时进入 products 清单 (与磁盘事实一致,
                // 供 aio_hips_verify_product_set V4 双向核对)
                struct { int flag; const char* name; bool present; } prods[] = {
                    {AIO_HIPS_PRODUCT_SIGNAL, "signal", true},
                    {AIO_HIPS_PRODUCT_SUPPORT, "support", true},
                    {AIO_HIPS_PRODUCT_VARIANCE, "variance", true},
                    {AIO_HIPS_PRODUCT_IVAR, "ivar", true},
                    {AIO_HIPS_PRODUCT_SNR, "snr", true},
                    {AIO_HIPS_PRODUCT_NREJ, "nrej", ps->diag_nrej_tiles},
                    {AIO_HIPS_PRODUCT_NUSED, "nused", ps->diag_nused_tiles},
                };
                bool first = true;
                for (const auto& p : prods) {
                    if ((ps->flags & p.flag) && p.present) {
                        if (!first) prod_list += ", ";
                        prod_list += "\"";
                        prod_list += p.name;
                        prod_list += "\"";
                        first = false;
                    }
                }
                std::fprintf(f,
                    "{\n"
                    "  \"format_version\": 1,\n"
                    "  \"hips_version\": \"1.4\",\n"
                    "  \"nside\": %u,\n"
                    "  \"tile_width\": %u,\n"
                    "  \"data_type\": \"%s\",\n"
                    "  \"products\": [%s],\n"
                    "  \"n_leaf_tiles\": %zu,\n"
                    "  \"moc_sky_fraction\": %s,\n"
                    "  \"astrocs_covered_sky_fraction\": %.8f,\n"
                    "  \"astrocs_support_clamped_pixels\": %llu,\n"
                    "  \"astrocs_coverage_gt1_pixels\": %llu,\n"
                    "  \"signal_dtype\": \"%s\",\n"
                    "  \"nrej_tiles\": %zu,\n"
                    "  \"nused_tiles\": %zu",
                    ps->nside, ps->tile_width,
                    ps->data_type == AIO_HIPS_FLOAT32 ? "float32" : "float64",
                    prod_list.c_str(),
                    ps->leaf_ipix_list.size(), fmt_sky_fraction(moc_frac).c_str(),
                    cov_frac,
                    (unsigned long long)ps->support_clamped_pixels,
                    (unsigned long long)ps->coverage_gt1_pixels,
                    ps->data_type == AIO_HIPS_FLOAT32 ? "float32" : "float64",
                    (size_t)(ps->flags & AIO_HIPS_PRODUCT_NREJ
                                 ? ps->leaf_ipix_list.size() : 0),
                    (size_t)(ps->flags & AIO_HIPS_PRODUCT_NUSED
                                 ? ps->leaf_ipix_list.size() : 0));
                // DATA-UNC-001 §30.3: provenance 四键与 properties 双写
                // (JSON 键同名小写; 调用方 schema 见 DATA-P2-PROV-001)
                // §9.73 A44: 旧「权重模式」JSON 键已删除 (四键 → 四键)。
                if (ps->prov_set) {
                    const bool inj_drift =
                        fault_injected("ASTROCS_HIPS_PROV_FAULT", "value_drift");
                    std::fprintf(f,
                        ",\n"
                        "  \"provenance\": {\n"
                        "    \"astrocs_input_manifest_hash\": \"%s\",\n"
                        "    \"astrocs_model_hash\": \"%s\",\n"
                        "    \"astrocs_uncertainty_available\": %s,\n"
                        "    \"astrocs_reject_profile\": \"%s\"\n"
                        "  }\n",
                        ps->prov_manifest_hash.c_str(),
                        // 注入面 value_drift: manifest 与 properties 分叉
                        // (证明 V6 双写面一致性断言有判别力)
                        inj_drift ? std::string(64, '0').c_str()
                                  : ps->prov_model_hash.c_str(),
                        ps->prov_uncertainty_available ? "true" : "false",
                        ps->prov_reject_profile.c_str());
                    std::fprintf(f, "}\n");
                } else {
                    std::fprintf(f, "\n}\n");
                }
                    return true;
                },
                &merr);
            if (mrc != 0) {
                aio_disk::note_failure(ps->out_dir, 0);   // 磁盘满分类
                set_error("manifest.json 原子落盘失败: " + merr);
                return -13;
            }
        }
        delete ps;
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

int aio_hips_abort(AioHipsProductSet* ps)  {
    // P1 (R9-A): C 边界异常屏障
    try {
        if (!ps) return 0;
        delete ps;
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

// ── DATA-UNC-001 §30.2/§30.3 产品集双向一致性核验 ─────────────────────────
namespace {
// properties 文本键解析 (与 reader 同口径: k=v, 去空白, 忽略 '#' 行)
std::map<std::string, std::string> read_props_file(const std::string& path) {
    std::map<std::string, std::string> kv;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return kv;
    std::string content;
    char buf[4096];
    size_t got = 0;
    while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0) content.append(buf, got);
    std::fclose(f);
    size_t pos = 0;
    while (pos <= content.size()) {
        size_t nl = content.find('\n', pos);
        const std::string line = content.substr(
            pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? content.size() + 1 : nl + 1;
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        while (!k.empty() && (k.back() == ' ' || k.back() == '\r')) k.pop_back();
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
        kv[k] = v;
    }
    return kv;
}

bool file_exists(const std::string& p) {
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

// manifest.json 标量取值 (writer 自产格式, 键唯一; 只取简单标量, 不做通用 JSON)
bool json_scalar(const std::string& doc, const std::string& key, std::string* out) {
    const std::string pat = "\"" + key + "\"";
    const size_t k = doc.find(pat);
    if (k == std::string::npos) return false;
    const size_t colon = doc.find(':', k + pat.size());
    if (colon == std::string::npos) return false;
    size_t p = colon + 1;
    while (p < doc.size() && (doc[p] == ' ' || doc[p] == '\t' ||
                              doc[p] == '\n' || doc[p] == '\r')) ++p;
    if (p >= doc.size()) return false;
    if (doc[p] == '"') {
        const size_t close = doc.find('"', p + 1);
        if (close == std::string::npos) return false;
        *out = doc.substr(p + 1, close - p - 1);
        return true;
    }
    size_t e = p;
    while (e < doc.size() && doc[e] != ',' && doc[e] != '\n' && doc[e] != '}' &&
           doc[e] != ' ' && doc[e] != '\r' && doc[e] != '\t') ++e;
    *out = doc.substr(p, e - p);
    return true;
}

// §9.73 A44: 旧「权重模式」键已从四键表删除 (原 5 → 4)。
const char* const kProvKeys[4] = {
    "ASTROCS_INPUT_MANIFEST_HASH", "ASTROCS_MODEL_HASH",
    "ASTROCS_UNCERTAINTY_AVAILABLE", "ASTROCS_REJECT_PROFILE"};

int count_prov_keys(const std::map<std::string, std::string>& props) {
    int n = 0;
    for (const char* k : kProvKeys)
        if (props.find(k) != props.end()) ++n;
    return n;
}
} // namespace

int aio_hips_verify_product_set(const char* out_dir, AioHipsVerifyReport* out)  {
    // P1 (R9-A) 同款 C 边界异常屏障
    try {
        g_hips_error.clear();
        if (!out_dir || !*out_dir || !out) {
            set_error("aio_hips_verify_product_set: 参数无效");
            return -1;
        }
        if (!abi_ok_verify_report(out)) return AIO_HIPS_ABI_MISMATCH;
        AioHipsVerifyReport rep{};
        rep.struct_size = out->struct_size;    // 头部字段原样回传 (不篡改调用方身份)
        rep.abi_version = out->abi_version;
        rep.n_signal_tiles = rep.n_variance_tiles = rep.n_ivar_tiles = -1;
        rep.n_nrej_tiles = rep.n_nused_tiles = -1;
        rep.uncertainty_available = -1;
        *out = rep;
        const std::string root = out_dir;
        // V1: signal 子产品 = 产品集事实面 (properties 必须存在且可解析)
        const std::map<std::string, std::string> sprops =
            read_props_file(root + "/signal/properties");
        if (sprops.empty()) {
            set_error("verify: signal/properties 缺失或为空: " + root);
            return -1;
        }
        out->signal_present = 1;
        {
            AioHipsDataset* ds = aio_hips_open(out_dir, AIO_HIPS_RD_SIGNAL);
            if (ds) {
                out->n_signal_tiles = aio_hips_tile_count(ds);
                aio_hips_close(ds);
            }
        }
        // V2: provenance 全或无 (四键齐备或整体不写; 禁静默缺键)
        out->prov_keys_present = count_prov_keys(sprops);
        if (out->prov_keys_present > 0) {
            if (out->prov_keys_present != 4) {
                set_error("verify: provenance 键不完整 (present=" +
                          std::to_string(out->prov_keys_present) + "/4, §30.3)");
                return 4;
            }
            const std::string ua = sprops.at("ASTROCS_UNCERTAINTY_AVAILABLE");
            if (ua == "true") out->uncertainty_available = 1;
            else if (ua == "false") out->uncertainty_available = 0;
            else {
                set_error("verify: ASTROCS_UNCERTAINTY_AVAILABLE 值非法: " + ua);
                return 4;
            }
        }
        // 注入面 (测试专用等价缺陷): 短路恒 OK —— 证明下述断言有判别力
        if (fault_injected("ASTROCS_HIPS_VERIFY_FAULT", "shortcut")) return 0;
        // V3: uncertainty_available 双向断言 (§30.1/§30.3)
        out->variance_present = file_exists(root + "/variance/properties") ? 1 : 0;
        out->ivar_present = file_exists(root + "/ivar/properties") ? 1 : 0;
        // 逐 tile 回读 (HDU 存在性 + 可读性; 声明/存在但 tile 文件缺失或
        // dtype 不符 → unreadable_tiles, 由下方规则判负)
        auto probe_float_product = [&](int product_kind, int* tiles_out) {
            AioHipsDataset* dd = aio_hips_open(out_dir, product_kind);
            if (!dd) return;
            const int n = aio_hips_tile_count(dd);
            *tiles_out = n;
            std::vector<float> buf((size_t)512 * 512);
            for (int t = 0; t < n; ++t) {
                uint64_t ipix = 0;
                if (aio_hips_tile_ipix(dd, t, &ipix) != 0) { ++out->unreadable_tiles; continue; }
                if (aio_hips_read_tile_f32(dd, ipix, buf.data()) != 0)
                    ++out->unreadable_tiles;
            }
            aio_hips_close(dd);
        };
        if (out->variance_present) probe_float_product(AIO_HIPS_RD_VARIANCE, &out->n_variance_tiles);
        if (out->ivar_present) probe_float_product(AIO_HIPS_RD_IVAR, &out->n_ivar_tiles);
        if (out->uncertainty_available == 1) {
            if (!out->variance_present || !out->ivar_present ||
                out->n_variance_tiles <= 0 || out->n_ivar_tiles <= 0 ||
                (out->n_signal_tiles > 0 &&
                 (out->n_variance_tiles != out->n_signal_tiles ||
                  out->n_ivar_tiles != out->n_signal_tiles))) {
                set_error("verify: uncertainty_available=true 但 variance/ivar"
                          " 子产品缺失或 tile 数不一致 (§30.1)");
                return 2;
            }
        } else if (out->uncertainty_available == 0) {
            if (out->variance_present || out->ivar_present) {
                set_error("verify: uncertainty_available=false 却存在 variance/"
                          "ivar 子产品 (禁占位, §30.1 unavailable 规则)");
                return 3;
            }
        }
        // V4/V5: 诊断平面声明 ↔ 磁盘事实双向 + 值域
        const std::string manifest_path = root + "/manifest.json";
        std::string mdoc;
        if (file_exists(manifest_path)) {
            FILE* f = std::fopen(manifest_path.c_str(), "rb");
            if (f) {
                char buf[4096];
                size_t got = 0;
                while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0)
                    mdoc.append(buf, got);
                std::fclose(f);
            }
        }
        struct { const char* name; bool declared; int* decl; int* present;
                 int* tiles; int rd_product; } diag[2] = {
            {"nrej", false, &out->nrej_declared, &out->nrej_present,
             &out->n_nrej_tiles, AIO_HIPS_RD_NREJ},
            {"nused", false, &out->nused_declared, &out->nused_present,
             &out->n_nused_tiles, AIO_HIPS_RD_NUSED},
        };
        for (auto& d : diag) {
            // products 清单中是否声明该通道 (键名精确匹配引号形态, 不与
            // nrej_tiles/nused_tiles 计数键混淆)
            if (!mdoc.empty() && mdoc.find("\"products\"") != std::string::npos)
                d.declared =
                    mdoc.find(std::string("\"") + d.name + "\"") != std::string::npos;
            *d.decl = d.declared ? 1 : 0;
            *d.present = file_exists(root + "/" + d.name + "/properties") ? 1 : 0;
            if (*d.present) {
                AioHipsDataset* dd = aio_hips_open(out_dir, d.rd_product);
                if (dd) {
                    *d.tiles = aio_hips_tile_count(dd);
                    // 逐 tile 回读值域 (int32; 负值 = 契约违反)
                    std::vector<int32_t> buf((size_t)512 * 512);
                    for (int t = 0; t < *d.tiles; ++t) {
                        uint64_t ipix = 0;
                        if (aio_hips_tile_ipix(dd, t, &ipix) != 0) {
                            ++out->unreadable_tiles;
                            continue;
                        }
                        // tile 文件缺失/dtype 非 int32 → 声明与事实不符 (V4)
                        if (aio_hips_read_tile_i32(dd, ipix, buf.data()) != 0) {
                            ++out->unreadable_tiles;
                            continue;
                        }
                        for (size_t i = 0; i < buf.size(); ++i)
                            if (buf[i] < 0) ++out->diag_negative_pixels;
                    }
                    aio_hips_close(dd);
                }
            }
            if (d.declared && !*d.present) {
                set_error(std::string("verify: manifest 声明 ") + d.name +
                          " 子产品但磁盘不存在 (§30.2)");
                return 5;
            }
            if (!d.declared && *d.present) {
                set_error(std::string("verify: 磁盘存在 ") + d.name +
                          " 子产品但 manifest 未声明 (禁占位, §30.2)");
                return 6;
            }
            if (d.declared && out->n_signal_tiles > 0 &&
                *d.tiles != out->n_signal_tiles) {
                set_error(std::string("verify: ") + d.name + " tile 数与 signal"
                          " 不一致 (§30.2)");
                return 5;
            }
        }
        // V4b: 声明/存在的子产品其 tile 必须真实可读 (禁"声明了产品却零文件")
        if (out->unreadable_tiles > 0) {
            set_error("verify: 子产品 tile 不可读或 dtype 不符 (unreadable=" +
                      std::to_string(out->unreadable_tiles) + "; §30.1/§30.2)");
            return out->uncertainty_available == 1 ? 2 : 5;
        }
        if (out->diag_negative_pixels > 0) {
            set_error("verify: 诊断平面出现负值 = 契约违反 (0 即\"无\", 禁 −1"
                      " 哨兵, §30.2)");
            return 7;
        }
        // V6: properties ↔ manifest.json 双写面值一致性 (§30.3 双写)
        if (out->prov_keys_present == 4 && !mdoc.empty()) {
            struct { const char* prop; const char* mkey; } pairs[3] = {
                {"ASTROCS_INPUT_MANIFEST_HASH", "astrocs_input_manifest_hash"},
                {"ASTROCS_MODEL_HASH", "astrocs_model_hash"},
                {"ASTROCS_REJECT_PROFILE", "astrocs_reject_profile"},
            };
            int mkeys = 0;
            for (const auto& p : pairs) {
                std::string v;
                if (!json_scalar(mdoc, p.mkey, &v)) continue;
                ++mkeys;
                if (v != sprops.at(p.prop)) ++out->value_mismatch;
            }
            {
                std::string v;
                if (json_scalar(mdoc, "astrocs_uncertainty_available", &v)) {
                    ++mkeys;
                    const std::string want =
                        out->uncertainty_available == 1 ? "true" : "false";
                    if (v != want) ++out->value_mismatch;
                }
            }
            out->manifest_keys_present = mkeys;
            // §9.73 A44: 双写面键数 = 4 (原五键中的「权重模式」键已删除;
            // properties 侧同口径见 kProvKeys[4] 与 != 4 断言)。
            if (mkeys != 4) {
                set_error("verify: manifest.json provenance 块不完整 (present=" +
                          std::to_string(mkeys) + "/4, §30.3 双写)");
                return 8;
            }
            if (out->value_mismatch != 0) {
                set_error("verify: properties 与 manifest.json provenance 值"
                          " 分叉 (§30.3 双写面禁止分叉)");
                return 8;
            }
        }
        return 0;

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

const char* aio_hips_last_error(void) {
    return g_hips_error.c_str();
}

// ============================================================================
// 兼容旧接口 (HISS 中转验证): 旧 AioHipsTile -> AstroSphereTileView 流式
// 旧语义: signal = F/support_frac, support uint8 (0..255)
// 转换: covered_area = su/255*A_cell, flux_sum = signal*(su/255)
// 新语义: signal = flux_sum/covered_area = 旧signal/A_cell, support 浮点
// ============================================================================
int aio_hips_write(
    const char* out_dir,
    uint32_t nside,
    uint32_t tile_width,
    const AioHipsTile* tiles,
    int n_tiles,
    int signal_dtype,
    const AioHipsSnrPoint* snr_points,
    int n_snr,
    const char* creator_did,
    const char* obs_title,
    int moc_order)  {
    // P1 (R9-A): C 边界异常屏障 (兼容旧接口入口)
    try {
        g_hips_error.clear();
        if (!out_dir || !tiles || n_tiles <= 0) { set_error("参数无效"); return -1; }
        // 逐元素 ABI 校验 (V11-N-01): 3 个跨边界数组参数均不得按盲步长解释
        for (int t = 0; t < n_tiles; ++t)
            if (!abi_ok_legacy_tile(&tiles[t], t)) return AIO_HIPS_ABI_MISMATCH;
        for (int i = 0; i < (snr_points ? n_snr : 0); ++i)
            if (!abi_ok_snr_point(&snr_points[i], i)) return AIO_HIPS_ABI_MISMATCH;
        AioHipsProductSet* ps = aio_hips_product_begin(
            out_dir, nside, tile_width, signal_dtype, AIO_HIPS_PRODUCT_ALL,
            creator_did, obs_title, nullptr, 0.0, nullptr, (uint32_t)moc_order);
        if (!ps) return -2;
        const double A_cell = 4.0 * kPi() / (12.0 * (double)nside * nside);
        std::vector<float> fluxF, areaF;
        std::vector<double> fluxD, areaD;
        for (int t = 0; t < n_tiles; ++t) {
            AstroSphereTileView view;
            std::memset(&view, 0, sizeof(view));
            aio_hips_tile_view_abi_init(&view);   // 内部转换视图: 自描述 ABI 头
            view.parent_ipix = tiles[t].parent_ipix;
            view.leaf_order = ilog2_u64(nside);
            view.width = 512;
            view.data_type = signal_dtype;
            const size_t n = 512 * 512;
            if (signal_dtype == AIO_HIPS_FLOAT32) {
                fluxF.resize(n); areaF.resize(n);
                const float* sig = (const float*)tiles[t].signal;
                const uint8_t* su = tiles[t].support;
                for (size_t i = 0; i < n; ++i) {
                    double sfrac = su ? su[i] / 255.0 : 1.0;
                    fluxF[i] = (float)(sig[i] * sfrac);
                    areaF[i] = (float)(sfrac * A_cell);
                }
                view.flux_sum = fluxF.data();
                view.covered_area = areaF.data();
            } else {
                fluxD.resize(n); areaD.resize(n);
                const double* sig = (const double*)tiles[t].signal;
                const uint8_t* su = tiles[t].support;
                for (size_t i = 0; i < n; ++i) {
                    double sfrac = su ? su[i] / 255.0 : 1.0;
                    fluxD[i] = sig[i] * sfrac;
                    areaD[i] = sfrac * A_cell;
                }
                view.flux_sum = fluxD.data();
                view.covered_area = areaD.data();
            }
            int rc = aio_hips_write_signal_support_tile(ps, &view);
            if (rc != 0) {
                aio_hips_abort(ps);
                return rc;
            }
        }
        if (snr_points && n_snr > 0) {
            if (aio_hips_write_snr_points(ps, snr_points, n_snr) != 0) {
                aio_hips_abort(ps);
                return -6;
            }
        }
        return aio_hips_finalize(ps);

    }
    catch (const std::exception &e) {
        set_error(std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        set_error("unknown exception");
        return -1;
    }
}

} // extern "C"






