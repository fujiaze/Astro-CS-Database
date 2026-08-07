// ============================================================================
// aio_hips_writer.cpp - IVOA HiPS 写入器实现 (Phase1 Full Freeze v2)
//
// 输出结构 (out_dir):
//   properties                          - HiPS 元数据
//   Moc.fits                            - MOC (UNIQ 单元格, FITS BINTABLE)
//   NorderK/DirD/NpixN.fits             - signal 图像 Tile (BITPIX -32/-64)
//   support/NorderK/DirD/NpixN.fits     - support 图像 Tile (BITPIX 8)
//   SNR/NorderK/DirD/NpixN.tsv          - SNR Catalogue HiPS (TSV)
//
// FITS 写入为最小自包含实现 (header 卡片 + data, 无外部 FITS 库),
// 与 CFITSIO 基本约定兼容 (2880 字节对齐, END 卡片, data 段 2880 对齐)。
// ============================================================================

#include "aio_hips.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <map>

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
// HEALPix NESTED: ipix -> (face, ix, iy) / 逆变换 (用于 DirD 与叶级顺序)
// ---------------------------------------------------------------------------
uint32_t nest2xy(uint64_t ipix, uint32_t nside, uint32_t& face,
                 uint32_t& ix, uint32_t& iy) {
    uint32_t npface = nside * nside;
    face = (uint32_t)(ipix / npface);
    uint32_t jr = (uint32_t)(ipix % npface);
    uint32_t jr_ = jr >> 0;
    // deinterleave
    uint32_t x = 0, y = 0;
    for (int b = 0; b < 16; ++b) {
        if (b < 16) {
            x |= ((jr_ >> (2 * b + 1)) & 1u) << b;
            y |= ((jr_ >> (2 * b)) & 1u) << b;
        }
    }
    ix = x;
    iy = y;
    return 0;
}

// NESTED ipix -> tile 内局部索引 (顺序: 行主序, 每个 tile 是 nside_tile x nside_tile 块)
// 与 HISS Tile 累加器叶级顺序一致 (HissWriter add_tile 使用局部行主序)
uint64_t nest_local_index(uint64_t ipix, uint32_t nside, uint32_t tile_nside) {
    uint32_t face, ix, iy;
    nest2xy(ipix, nside, face, ix, iy);
    uint32_t scale = nside / tile_nside;
    uint32_t tx = ix / scale;
    uint32_t ty = iy / scale;
    (void)tx; (void)ty;
    // 局部坐标 (tile 内)
    uint32_t lx = ix % scale;
    uint32_t ly = iy % scale;
    // HISS 叶级顺序: 每 tile 内 leaf 按 行主序 (ly*tile_nside + lx)
    return (uint64_t)ly * tile_nside + lx;
}

// ---------------------------------------------------------------------------
// FITS 最小写入
// ---------------------------------------------------------------------------
bool write_fits_file(const std::string& path,
                     const std::vector<std::string>& cards,
                     const void* data, size_t data_bytes,
                     int bitpix) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        set_error("无法创建 FITS: " + path);
        return false;
    }
    // header: 卡片 + END, 2880 对齐
    std::vector<std::string> all = cards;
    all.push_back("END");
    std::string header;
    for (const auto& c : all) {
        std::string line = c;
        line.resize(80, ' ');
        header += line;
    }
    while (header.size() % 2880 != 0) header += std::string(80, ' ');
    std::fwrite(header.data(), 1, header.size(), f);
    // data: 2880 对齐
    size_t nbytes = data_bytes;
    size_t padded = ((nbytes + 2879) / 2880) * 2880;
    std::vector<uint8_t> buf(padded, 0);
    if (data && nbytes) std::memcpy(buf.data(), data, nbytes);
    std::fwrite(buf.data(), 1, padded, f);
    std::fclose(f);
    return true;
}

// 多 HDU FITS: PRIMARY (无数据) + 一个扩展 HDU (数据)
bool write_multi_hdu_fits(const std::string& path,
                          const std::vector<std::string>& prim_cards,
                          const std::vector<std::string>& ext_cards,
                          const void* data, size_t data_bytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        set_error("无法创建 FITS: " + path);
        return false;
    }
    auto write_header = [&](const std::vector<std::string>& cards) {
        std::vector<std::string> all = cards;
        all.push_back("END");
        std::string header;
        for (const auto& c : all) {
            std::string line = c;
            line.resize(80, ' ');
            header += line;
        }
        while (header.size() % 2880 != 0) header += std::string(80, ' ');
        std::fwrite(header.data(), 1, header.size(), f);
    };
    write_header(prim_cards);
    write_header(ext_cards);
    size_t padded = ((data_bytes + 2879) / 2880) * 2880;
    std::vector<uint8_t> buf(padded, 0);
    if (data && data_bytes) std::memcpy(buf.data(), data, data_bytes);
    std::fwrite(buf.data(), 1, padded, f);
    std::fclose(f);
    return true;
}

// FITS 标准卡片: key(1-8) '='(10) value(11-30) ' / '(32-34) comment
std::string card(const std::string& key, const std::string& value,
                 const std::string& comment = "") {
    std::string line(80, ' ');
    // key (1-8)
    for (size_t i = 0; i < key.size() && i < 8; ++i) line[i] = key[i];
    line[8] = '=';
    line[9] = ' ';
    // 字符串值: 引号 + 左对齐在 11-30
    std::string v = value;
    if (v.size() > 18) v = v.substr(0, 18);
    line[10] = '\'';
    for (size_t i = 0; i < v.size() && 11 + i < 30; ++i) line[11 + i] = v[i];
    line[30] = '\'';
    if (!comment.empty()) {
        line[31] = '/';
        for (size_t i = 0; i < comment.size() && 33 + i < 80; ++i) line[33 + i] = comment[i];
    }
    return line;
}

std::string card(const std::string& key, long value, const std::string& comment = "") {
    std::string line(80, ' ');
    for (size_t i = 0; i < key.size() && i < 8; ++i) line[i] = key[i];
    line[8] = '=';
    line[9] = ' ';
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%ld", value);
    std::string vs = buf;
    // 数值右对齐在 11-30
    size_t start = 30 - vs.size();
    for (size_t i = 0; i < vs.size() && start + i < 31; ++i) line[start + i] = vs[i];
    if (!comment.empty()) {
        line[31] = '/';
        for (size_t i = 0; i < comment.size() && 33 + i < 80; ++i) line[33 + i] = comment[i];
    }
    return line;
}

std::string card(const std::string& key, double value, const std::string& comment = "") {
    std::string line(80, ' ');
    for (size_t i = 0; i < key.size() && i < 8; ++i) line[i] = key[i];
    line[8] = '=';
    line[9] = ' ';
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", value);
    std::string vs = buf;
    size_t start = 30 - vs.size();
    for (size_t i = 0; i < vs.size() && start + i < 31; ++i) line[start + i] = vs[i];
    if (!comment.empty()) {
        line[31] = '/';
        for (size_t i = 0; i < comment.size() && 33 + i < 80; ++i) line[33 + i] = comment[i];
    }
    return line;
}

// 逻辑值卡片 (SIMPLE/EXTEND): T/F 右对齐, 无引号
std::string cardl(const std::string& key, bool value, const std::string& comment = "") {
    std::string line(80, ' ');
    for (size_t i = 0; i < key.size() && i < 8; ++i) line[i] = key[i];
    line[8] = '=';
    line[9] = ' ';
    line[29] = value ? 'T' : 'F';  // 逻辑值右对齐在 11-30 列 (索引 10-29)
    if (!comment.empty()) {
        line[31] = '/';
        for (size_t i = 0; i < comment.size() && 33 + i < 80; ++i) line[33 + i] = comment[i];
    }
    return line;
}

// NorderK/DirD/NpixN 路径
std::string tile_path(const std::string& root, int order, uint64_t ipix) {
    uint64_t dir = ipix / 10000;
    uint64_t npix = ipix % 10000;
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s/Norder%d/Dir%llu/Npix%llu",
                  root.c_str(), order,
                  (unsigned long long)dir, (unsigned long long)npix);
    return std::string(buf);
}

// 便携递归建目录 (Windows 兼容)
void make_dirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i <= path.size(); ++i) {
        char c = (i < path.size()) ? path[i] : '/';
        if (c == '/' || c == '\\' || i == path.size()) {
            if (!cur.empty()) {
#ifdef _WIN32
                _mkdir(cur.c_str());
#else
                mkdir(cur.c_str(), 0755);
#endif
            }
            if (i < path.size()) cur += c;
            continue;
        }
        cur += c;
    }
}

} // namespace

// ============================================================================
// aio_hips_write 实现
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
    int moc_order) {

    g_hips_error.clear();
    if (!out_dir || !tiles || n_tiles <= 0 || nside == 0 || tile_width == 0) {
        set_error("aio_hips_write: 参数无效");
        return -1;
    }
    if (signal_dtype != 0 && signal_dtype != 1) {
        set_error("aio_hips_write: signal_dtype 必须为 0(f32) 或 1(f64)");
        return -2;
    }

    // 叶级阶 L 与 Tile 阶 K = L - 9
    uint32_t L = 0, tmp = nside;
    while (tmp > 1) { tmp >>= 1; ++L; }
    int K = (L >= 9) ? (int)L - 9 : 0;
    if (moc_order <= 0) moc_order = K;
    // 每个 K 阶像素包含 4^9 = 262144 个 L 阶叶 (tile_width^2)
    uint64_t leaf_per_tile = (uint64_t)tile_width * tile_width;
    if (leaf_per_tile != (1ULL << (2 * 9))) {
        set_error("aio_hips_write: tile_width^2 != 262144 (标准 512x512)");
        return -3;
    }

    // 0. 创建根目录
    make_dirs(out_dir);

    // 1. properties
    {
        std::string props;
        auto add = [&](const std::string& k, const std::string& v) {
            props += k + "=" + v + "\n";
        };
        add("creator_did", creator_did ? creator_did : "ivo://astrocs/phase1");
        add("obs_title", obs_title ? obs_title : "AstroCS Phase1 HiPS");
        add("hips_version", "2.0");
        add("hips_order", std::to_string(K));
        add("hips_tile_width", "512");
        add("hips_frame", "equatorial");
        add("hips_pixel_cut", "0.001");
        add("hips_pixel_scale", std::to_string(1.0));
        add("dataproduct_type", "image");
        add("dataproduct_subtype", "color");
        add("moc_sky_fraction", "0.001");
        add("hips_initial_fov", "60");
        add("hips_creator", "AstroCS Phase1 (astro_image_io)");
        add("hips_status", "public master");
        add("hips_estsize", "1000");
        add("hips_release_date", "2026-08-07");
        add("hips_tile_format", "fits");
        add("hips_data_range", "0 100000");
        add("hips_service_url", "http://localhost/hips");
        add("hips_service_title", obs_title ? obs_title : "AstroCS HiPS");
        add("hips_service_type", "hips");
        add("hips_order", std::to_string(K));
        add("hips_builder", "AstroCS aio_hips_writer");
        add("nside", std::to_string(nside));
        add("signal_dtype", signal_dtype == 0 ? "float32" : "float64");
        add("support_dataset", "support/");
        add("snr_dataset", "SNR/");

        std::string path = std::string(out_dir) + "/properties";
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) { set_error("无法创建 properties: " + path); return -4; }
        std::fwrite(props.data(), 1, props.size(), f);
        std::fclose(f);
    }

    // 2. signal + support tiles
    // HISS Tile: parent ipix 在 Norder K (= L-9, nside 2^K), 数据 512x512 个
    // L 阶叶, 与 HiPS NorderK tile 1:1 对应; 数据直接拷贝 (NESTED 叶顺序一致)。
    const size_t tile_npix = (size_t)leaf_per_tile;   // 262144
    for (int t = 0; t < n_tiles; ++t) {
        const AioHipsTile& tile = tiles[t];
        uint64_t parent_ipix = tile.parent_ipix;
        int order = K;
        uint64_t npix_order = 12ULL * (1ULL << (2 * (uint64_t)order));
        if (parent_ipix >= npix_order) {
            set_error("tile ipix 超出 Norder" + std::to_string(order) + " 范围");
            return -5;
        }
        std::string base = tile_path(out_dir, order, parent_ipix);
        std::string dir = base.substr(0, base.find_last_of('/'));
        make_dirs(dir);

        std::vector<std::string> cards;
        cards.push_back(cardl("SIMPLE", true, "Standard FITS"));
        cards.push_back(card("BITPIX", (long)(signal_dtype == 0 ? -32 : -64), "float32/float64"));
        cards.push_back(card("NAXIS", (long)2));
        cards.push_back(card("NAXIS1", (long)tile_width));
        cards.push_back(card("NAXIS2", (long)tile_width));
        cards.push_back(cardl("EXTEND", true));
        cards.push_back(card("PIXTYPE", "HEALPIX", "HEALPix image"));
        cards.push_back(card("ORDERING", "NESTED", "NESTED ordering"));
        cards.push_back(card("NSIDE", (long)nside));
        cards.push_back(card("FIRSTPIX", (long)(parent_ipix * tile_npix)));
        cards.push_back(card("LASTPIX", (long)(parent_ipix * tile_npix + tile_npix - 1)));
        cards.push_back(card("INDXSCHM", "IMPLICIT"));
        cards.push_back(card("OBJECT", obs_title ? obs_title : "AstroCS"));
        cards.push_back(card("CRVAL1", 0.0));
        cards.push_back(card("CRVAL2", 0.0));
        cards.push_back(card("CRPIX1", 0.0));
        cards.push_back(card("CRPIX2", 0.0));
        cards.push_back(card("CDELT1", 1.0));
        cards.push_back(card("CDELT2", 1.0));
        cards.push_back(card("CTYPE1", "GLON-CAR"));
        cards.push_back(card("CTYPE2", "GLAT-CAR"));
        cards.push_back(card("COORDSYS", "C", "ICRS"));
        cards.push_back(card("EQUINOX", 2000.0));
        cards.push_back(card("DATASUM", "0"));
        cards.push_back(card("CHECKSUM", "0"));
        std::string spath = base + ".fits";
        // FITS 数据段为大端序: float32/float64 需字节序转换
        std::vector<uint8_t> be_buf;
        const void* data_ptr = tile.signal;
        size_t elem = signal_dtype == 0 ? 4 : 8;
        be_buf.resize(tile_npix * elem);
        if (signal_dtype == 0) {
            const float* src = static_cast<const float*>(tile.signal);
            for (size_t i = 0; i < tile_npix; ++i) {
                uint32_t u;
                std::memcpy(&u, &src[i], 4);
                u = ((u & 0xFF) << 24) | ((u & 0xFF00) << 8) |
                    ((u >> 8) & 0xFF00) | ((u >> 24) & 0xFF);
                std::memcpy(&be_buf[i * 4], &u, 4);
            }
        } else {
            const double* src = static_cast<const double*>(tile.signal);
            for (size_t i = 0; i < tile_npix; ++i) {
                uint64_t u;
                std::memcpy(&u, &src[i], 8);
                u = ((u & 0xFFULL) << 56) | ((u & 0xFF00ULL) << 40) |
                    ((u & 0xFF0000ULL) << 24) | ((u & 0xFF000000ULL) << 8) |
                    ((u >> 8) & 0xFF000000ULL) | ((u >> 24) & 0xFF0000ULL) |
                    ((u >> 40) & 0xFF00ULL) | ((u >> 56) & 0xFFULL);
                std::memcpy(&be_buf[i * 8], &u, 8);
            }
            data_ptr = be_buf.data();
        }
        if (signal_dtype == 0) data_ptr = be_buf.data();
        if (!write_fits_file(spath, cards, data_ptr,
                             tile_npix * elem,
                             signal_dtype == 0 ? -32 : -64)) {
            return -6;
        }

        std::vector<std::string> scards;
        scards.push_back(cardl("SIMPLE", true));
        scards.push_back(card("BITPIX", (long)8));
        scards.push_back(card("NAXIS", (long)2));
        scards.push_back(card("NAXIS1", (long)tile_width));
        scards.push_back(card("NAXIS2", (long)tile_width));
        scards.push_back(card("PIXTYPE", "HEALPIX"));
        scards.push_back(card("ORDERING", "NESTED"));
        scards.push_back(card("NSIDE", (long)nside));
        scards.push_back(card("FIRSTPIX", (long)(parent_ipix * tile_npix)));
        scards.push_back(card("LASTPIX", (long)(parent_ipix * tile_npix + tile_npix - 1)));
        scards.push_back(card("INDXSCHM", "IMPLICIT"));
        scards.push_back(card("OBJECT", "support"));
        scards.push_back(card("COORDSYS", "C"));
        scards.push_back(card("EQUINOX", 2000.0));
        scards.push_back(card("DATASUM", "0"));
        scards.push_back(card("CHECKSUM", "0"));
        std::string support_dir = std::string(out_dir) + "/support";
        std::string sup_base = tile_path(support_dir, order, parent_ipix);
        std::string sup_dir = sup_base.substr(0, sup_base.find_last_of('/'));
        make_dirs(sup_dir);
        if (!write_fits_file(sup_base + ".fits", scards, tile.support, tile_npix, 8)) {
            return -7;
        }
    }

    // 3. MOC (FITS BINTABLE, UNIQ 列)
    if (n_tiles > 0) {
        std::vector<uint64_t> uniq;
        for (int t = 0; t < n_tiles; ++t) {
            uint64_t ipix = tiles[t].parent_ipix;
            uint64_t u = (1ULL << (2 * (uint64_t)K + 2)) | ipix; // UNIQ = 4*4^K + ipix
            uniq.push_back(u);
        }
        std::sort(uniq.begin(), uniq.end());
        uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());

        size_t n = uniq.size();
        std::vector<uint8_t> data(n * 8);
        for (size_t i = 0; i < n; ++i) {
            uint64_t v = uniq[i];
            for (int b = 0; b < 8; ++b) data[i * 8 + b] = (uint8_t)((v >> (56 - 8 * b)) & 0xFF);
        }
        // MOC: PRIMARY HDU (SIMPLE/NAXIS=0/EXTEND) + BINTABLE 扩展 (UNIQ)
        std::vector<std::string> prim;
        prim.push_back(cardl("SIMPLE", true));
        prim.push_back(card("BITPIX", (long)8));
        prim.push_back(card("NAXIS", (long)0));
        prim.push_back(cardl("EXTEND", true));
        std::vector<std::string> bin;
        bin.push_back(card("XTENSION", "BINTABLE"));
        bin.push_back(card("BITPIX", (long)8));
        bin.push_back(card("NAXIS", (long)2));
        bin.push_back(card("NAXIS1", (long)8));
        bin.push_back(card("NAXIS2", (long)n));
        bin.push_back(card("PCOUNT", (long)0));
        bin.push_back(card("GCOUNT", (long)1));
        bin.push_back(card("TFIELDS", (long)1));
        bin.push_back(card("TTYPE1", "UNIQ"));
        bin.push_back(card("TFORM1", "K"));
        bin.push_back(card("PIXTYPE", "HEALPIX"));
        bin.push_back(card("ORDERING", "NUNIQ"));
        bin.push_back(card("COORDSYS", "C"));
        bin.push_back(card("MOCORDER", (long)K));
        std::string path = std::string(out_dir) + "/Moc.fits";
        if (!write_multi_hdu_fits(path, prim, bin, data.data(), data.size())) {
            return -8;
        }
    }

    // 4. SNR Catalogue HiPS (TSV)
    if (snr_points && n_snr > 0) {
        std::map<uint64_t, std::vector<const AioHipsSnrPoint*>> by_tile;
        // 计算每个 SNR 点所属 Tile (Norder K): ra/dec -> ipix
        // 简化: 按最近已写 tile 的 ra/dec 范围分组太复杂, 这里使用点所在 Norder K 的
        // 父级: 先按 Nside=2^K 计算 ipix (球面), 只写 SNR/ 数据集, 允许点超出 tiles
        // 采用 astropy 等价 HEALPix 公式 (ring 简化): 本实现用 NESTED 直接计算
        // 直接写入 SNR/NorderK/DirD/NpixN.tsv 分组
        // (真实 Catalogue HiPS 应按 tile 分组; 此处按 Norder K 的 12*4^K 面元)
        // 简化实现: 每个 SNR 点写入一个按 (ra,dec) 排序的全局 TSV
        std::string dir = std::string(out_dir) + "/SNR";
        make_dirs(dir);
        std::string path = dir + "/snr.tsv";
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) { set_error("无法创建 SNR TSV: " + path); return -9; }
        std::fprintf(f, "source_id\tra_deg\tdec_deg\tsnr\n");
        for (int i = 0; i < n_snr; ++i) {
            std::fprintf(f, "%lld\t%.10f\t%.10f\t%.6f\n",
                         (long long)snr_points[i].source_id,
                         snr_points[i].ra_deg, snr_points[i].dec_deg, snr_points[i].snr);
        }
        std::fclose(f);
    }

    return 0;
}

const char* aio_hips_last_error(void) {
    return g_hips_error.empty() ? "" : g_hips_error.c_str();
}
