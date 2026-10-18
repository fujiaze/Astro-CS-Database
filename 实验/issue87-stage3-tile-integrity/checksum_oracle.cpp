// 实验/issue87-stage3-tile-integrity/checksum_oracle.cpp — Issue #87/E2-R
// FITS tile checksum 快速路径 vs 独立 CFITSIO 对拍（磁盘原始字节口径）。
//
// 背景（#87 独立审计「校验补充风险」）：reader 的快速路径用
// `fits_read_pix` **解码后的像元位样** 计算 DATASUM，而 FITS 校验和定义在
// **磁盘原始字节**（header + data records）上。一般未压缩 float/double 逐位等价，
// 但压缩 tile / BSCALE/BZERO/BLANK / 特殊字节变换下二者可能不等价。
//
// 本 harness：
//   1) 构造受控 HiPS 产品（signal/properties + manifest + 单个 tile ipix=4）；
//   2) 每个用例给出 reader 实际返回码（aio_hips_read_tile_f32/i32_ex）；
//   3) 用 cfitsio `fits_verify_chksum` 作**独立 oracle**（它按 ffgcks 读磁盘原始
//      字节算 DATASUM/HDU sum）；
//   4) 复制快速路径公式（raw header + typed pixels）并与 raw-bytes 公式对比，
//      证明"何时等价 / 何时必须回退"；
//   5) 判据：reader 的 CHECKSUM_FAILED ⟺ oracle 判某个存在的字段失败；合法文件
//      reader 必须 OK（不得误报）。
//
// 用法: checksum_oracle <out_root>
// 输出: 每个用例一行 JSON（供 checksum_oracle.py 收集成证据），末行 SUMMARY。
#include "aio_hips_reader.h"

#include <fitsio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

extern "C" {
// vendored cfitsio 内部编码函数（checksum.c；用于构造合法 CHECKSUM-only 文件）。
void ffesum(unsigned long sum, int complm, char* ascii);
// HDU 地址（fits_get_hduaddrll = ffghadll）——压缩 HDU 的 header/data 偏移。
int ffghadll(fitsfile* fptr, long long* headstart, long long* datastart,
             long long* dataend, int* status);
}

namespace {

constexpr int kTile = 512;
constexpr long kNelem = (long)kTile * kTile;
constexpr uint64_t kTileIpix = 4;

std::string g_root;

std::string pjoin(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

bool mkdirs(const std::string& p) {
    std::string cur;
    for (size_t i = 0; i < p.size(); ++i) {
        cur += p[i];
        if (p[i] == '/' || i + 1 == p.size()) {
            if (cur == "/" || cur.empty()) continue;
            ::mkdir(cur.c_str(), 0755);
        }
    }
    return true;
}

void write_text(const std::string& path, const std::string& text) {
    mkdirs(path.substr(0, path.find_last_of('/')));
    std::ofstream f(path, std::ios::binary);
    f << text;
}

std::string props_text() {
    return "creator_did = ivo://acsd/issue87/checksum-oracle\n"
           "hips_version = 1.4\n"
           "hips_order = 0\n"
           "hips_tile_width = 512\n"
           "hips_frame = equatorial\n"
           "hips_ordering = NESTED\n"
           "dataproduct_type = image\n"
           "hips_tile_format = fits\n"
           "BUNIT=ADU/sr\n"
           "ACSD_SIGNAL_UNIT=ADU/sr\n"
           "ACSD_PIXEL_SEMANTICS=surface_brightness\n"
           "ACSD_PIXEL_AREA_POWER=-2\n";
}

std::string manifest_text() {
    return "{\n  \"schema\": \"acsd-hips-manifest/1\",\n  \"products\": [\"signal\"]\n}\n";
}

std::string tile_path_of(const std::string& root) {
    return pjoin(root, "signal/Norder0/Dir0/Npix4.fits");
}

void make_product(const std::string& root) {
    mkdirs(pjoin(root, "signal"));
    mkdirs(pjoin(root, "signal/Norder0/Dir0"));
    write_text(pjoin(root, "signal/properties"), props_text());
    write_text(pjoin(root, "manifest.json"), manifest_text());
}

// ── 原始字节 1's-complement（与 reader/ffcsum 同口径）─────────────────────────
uint32_t fold_ones(uint64_t s) {
    while (s >> 32) s = (s & 0xffffffffULL) + (s >> 32);
    return (uint32_t)s;
}

uint32_t sum_be(const unsigned char* p, size_t n) {
    uint64_t s = 0;
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        s += ((uint32_t)p[i] << 24) | ((uint32_t)p[i + 1] << 16) |
             ((uint32_t)p[i + 2] << 8) | (uint32_t)p[i + 3];
    }
    return fold_ones(s);
}

uint32_t sum_typed_f32(const float* p, long n) {
    uint64_t s = 0;
    for (long i = 0; i < n; ++i) {
        uint32_t w = 0;
        std::memcpy(&w, p + i, 4);
        s += w;
    }
    return fold_ones(s);
}

uint32_t sum_typed_f64(const double* p, long n) {
    uint64_t s = 0;
    for (long i = 0; i < n; ++i) {
        uint64_t b = 0;
        std::memcpy(&b, p + i, 8);
        s += (b >> 32) + (b & 0xffffffffULL);
    }
    return fold_ones(s);
}

uint32_t sum_typed_i32(const int32_t* p, long n) {
    uint64_t s = 0;
    for (long i = 0; i < n; ++i) {
        uint32_t w = 0;
        std::memcpy(&w, p + i, 4);
        s += w;
    }
    return fold_ones(s);
}

// 读 raw header（直到 END 卡所在 2880 block）。
bool read_raw_header(const std::string& path, std::string* out, size_t* hdr_bytes) {
    std::ifstream f(path, std::ios::binary);
    if (!f.good()) return false;
    out->clear();
    char block[2880];
    for (int b = 0; b < 100; ++b) {
        f.read(block, sizeof(block));
        if (f.gcount() != (std::streamsize)sizeof(block)) return false;
        out->append(block, sizeof(block));
        for (int c = 0; c < 36; ++c) {
            const char* card = block + c * 80;
            if (card[0] == 'E' && card[1] == 'N' && card[2] == 'D') {
                bool blank = true;
                for (int k = 3; k < 80; ++k)
                    if (card[k] != ' ') { blank = false; break; }
                if (blank) { *hdr_bytes = out->size(); return true; }
            }
        }
    }
    return false;
}

bool read_raw_range(const std::string& path, size_t off, size_t n, std::string* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.good()) return false;
    f.seekg((std::streamoff)off);
    out->resize(n);
    f.read(&(*out)[0], (std::streamsize)n);
    return f.gcount() == (std::streamsize)n;
}

void flip_byte(const std::string& path, size_t off) {
    std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
    f.seekg((std::streamoff)off);
    char c = 0;
    f.read(&c, 1);
    c = (char)((unsigned char)c ^ 0x01);
    f.seekp((std::streamoff)off);
    f.write(&c, 1);
}

// ── tile 写入 ───────────────────────────────────────────────────────────────
struct TileSpec {
    int bitpix = -32;          // -32 / -64 / 32
    bool nan_payloads = false;
    bool bscale = false;       // 写非平凡 BSCALE=2.0
    bool bzero = false;        // 写非平凡 BZERO=100.0
    bool blank = false;        // 写 BLANK（仅整型有意义）
    bool compress = false;     // fpack tile-compressed
    std::string cksum = "both"; // both | datasum | checksum | none
};

void fill_f32(std::vector<float>* v, bool nan_payloads) {
    v->resize((size_t)kNelem);
    for (long i = 0; i < kNelem; ++i) (*v)[(size_t)i] = 1000.0f + (float)(i % 97);
    if (nan_payloads) {
        auto setbits = [&](long i, uint32_t bits) {
            float f = 0;
            std::memcpy(&f, &bits, 4);
            (*v)[(size_t)i] = f;
        };
        setbits(0, 0x7fc00000u);   // quiet NaN
        setbits(1, 0x7f800001u);   // signaling NaN payload 1
        setbits(2, 0xffc00001u);   // negative NaN payload
        setbits(3, 0x7f800000u);   // +Inf
        setbits(4, 0xff800000u);   // -Inf
        setbits(5, 0x00000000u);   // +0
        setbits(6, 0x80000000u);   // -0
        setbits(7, 0x00000001u);   // denormal
        setbits(8, 0x80000001u);   // negative denormal
    }
}

void fill_f64(std::vector<double>* v, bool nan_payloads) {
    v->resize((size_t)kNelem);
    for (long i = 0; i < kNelem; ++i) (*v)[(size_t)i] = 2000.0 + (double)(i % 89);
    if (nan_payloads) {
        auto setbits = [&](long i, uint64_t bits) {
            double d = 0;
            std::memcpy(&d, &bits, 8);
            (*v)[(size_t)i] = d;
        };
        setbits(0, 0x7ff8000000000000ULL);
        setbits(1, 0x7ff0000000000001ULL);
        setbits(2, 0xfff8000000000001ULL);
        setbits(3, 0x7ff0000000000000ULL);
        setbits(4, 0xfff0000000000000ULL);
        setbits(5, 0x0000000000000000ULL);
        setbits(6, 0x8000000000000000ULL);
        setbits(7, 0x0000000000000001ULL);
    }
}

bool write_uncompressed(const std::string& path, const TileSpec& s) {
    fitsfile* f = nullptr;
    int status = 0;
    std::remove(path.c_str());
    if (fits_create_file(&f, path.c_str(), &status)) return false;
    long naxes[2] = {kTile, kTile};
    if (fits_create_img(f, s.bitpix, 2, naxes, &status)) { fits_close_file(f, &status); return false; }
    if (s.bitpix == -32) {
        std::vector<float> v; fill_f32(&v, s.nan_payloads);
        if (fits_write_img(f, TFLOAT, 1, kNelem, v.data(), &status)) { fits_close_file(f, &status); return false; }
    } else if (s.bitpix == -64) {
        std::vector<double> v; fill_f64(&v, s.nan_payloads);
        if (fits_write_img(f, TDOUBLE, 1, kNelem, v.data(), &status)) { fits_close_file(f, &status); return false; }
    } else {
        std::vector<int32_t> v((size_t)kNelem);
        for (long i = 0; i < kNelem; ++i) v[(size_t)i] = (int32_t)(i % 1000);
        if (fits_write_img(f, TINT, 1, kNelem, v.data(), &status)) { fits_close_file(f, &status); return false; }
    }
    if (s.bscale) {
        double v = 2.0;
        fits_write_key(f, TDOUBLE, (char*)"BSCALE", &v, nullptr, &status);
    }
    if (s.bzero) {
        double v = 100.0;
        fits_write_key(f, TDOUBLE, (char*)"BZERO", &v, nullptr, &status);
    }
    if (s.blank) {
        long v = 0;
        fits_write_key(f, TLONG, (char*)"BLANK", &v, nullptr, &status);
    }
    // 头部标记卡（header bitflip 用例的翻转目标；不影响数据）
    fits_write_comment(f, (char*)"ORACLE-HEADER-MARKER", &status);
    if (fits_close_file(f, &status)) return false;
    return true;
}

// 若当前 HDU 是空 PRIMARY 且 HDU2 是压缩图像，则移动到压缩 HDU。
void move_to_compressed_hdu(fitsfile* f) {
    int bitpix = 0, naxis = 0;
    long naxes[4] = {0, 0, 0, 0};
    int status = 0;
    if (fits_get_img_param(f, 2, &bitpix, &naxis, naxes, &status) != 0 || naxis != 0) {
        fits_clear_errmsg();
        return;
    }
    int nhdu = 0, s2 = 0;
    if (fits_get_num_hdus(f, &nhdu, &s2) == 0 && nhdu >= 2) {
        int s3 = 0;
        if (fits_movabs_hdu(f, 2, nullptr, &s3) == 0) {
            int s4 = 0;
            if (fits_is_compressed_image(f, &s4) != 1) {
                int s5 = 0;
                fits_movabs_hdu(f, 1, nullptr, &s5);
            }
        }
    }
    fits_clear_errmsg();
}

// 写 checksum 字段（both/datasum/checksum）。
bool write_checksums(const std::string& path, const std::string& mode) {
    if (mode == "none") return true;
    fitsfile* f = nullptr;
    int status = 0;
    if (fits_open_file(&f, path.c_str(), READWRITE, &status)) return false;
    move_to_compressed_hdu(f);
    if (fits_write_chksum(f, &status)) { fits_close_file(f, &status); return false; }
    if (mode == "datasum") {
        fits_delete_key(f, "CHECKSUM", &status);
    } else if (mode == "checksum") {
        // 构造合法 CHECKSUM-only：把 CHECKSUM 归零、删掉 DATASUM，再对**零值
        // CHECKSUM**的 HDU 求 hdusum，最后写 ffesum 的补码编码（FITS 定义：
        // sum(header+data, CHECKSUM=0) + encoded = 0xFFFFFFFF）。
        fits_update_key(f, TSTRING, (char*)"CHECKSUM", (char*)"0000000000000000",
                        nullptr, &status);
        fits_delete_key(f, "DATASUM", &status);
        unsigned long dsum = 0, hdusum = 0;
        status = 0;
        if (ffgcks(f, &dsum, &hdusum, &status)) { fits_close_file(f, &status); return false; }
        char enc[FLEN_VALUE] = {0};
        ffesum(hdusum, 1, enc);
        if (fits_update_key(f, TSTRING, (char*)"CHECKSUM", enc, nullptr, &status)) {
            fits_close_file(f, &status);
            return false;
        }
    }
    return fits_close_file(f, &status) == 0;
}

bool create_tile(const std::string& path, const TileSpec& s) {
    if (!s.compress) {
        if (!write_uncompressed(path, s)) return false;
        return write_checksums(path, s.cksum);
    }
    const std::string tmp = path + ".plain.tmp";
    if (!write_uncompressed(tmp, s)) return false;
    fitsfile* in = nullptr;
    fitsfile* out = nullptr;
    int status = 0;
    if (fits_open_file(&in, tmp.c_str(), READONLY, &status)) return false;
    std::remove(path.c_str());
    if (fits_create_file(&out, path.c_str(), &status)) { fits_close_file(in, &status); return false; }
    if (fits_img_compress(in, out, &status)) {
        int s2 = 0;
        fits_close_file(in, &s2);
        fits_close_file(out, &s2);
        std::remove(tmp.c_str());
        return false;
    }
    int s2 = 0;
    fits_close_file(in, &s2);
    fits_close_file(out, &s2);
    std::remove(tmp.c_str());
    return write_checksums(path, s.cksum);
}

// ── oracle ──────────────────────────────────────────────────────────────────
struct Oracle {
    int dataok = -9, hduok = -9;   // cfitsio: 1=ok, 0=absent, -1=bad
    bool has_datasum = false, has_checksum = false;
    unsigned long datasum = 0, hdusum = 0;
    bool ok = false;
};

Oracle run_oracle(const std::string& path) {
    Oracle o;
    fitsfile* f = nullptr;
    int status = 0;
    if (fits_open_file(&f, path.c_str(), READONLY, &status)) return o;
    move_to_compressed_hdu(f);
    char buf[FLEN_VALUE] = {0};
    int st = 0;
    o.has_datasum = fits_read_key(f, TSTRING, (char*)"DATASUM", buf, nullptr, &st) == 0;
    fits_clear_errmsg();
    st = 0;
    o.has_checksum = fits_read_key(f, TSTRING, (char*)"CHECKSUM", buf, nullptr, &st) == 0;
    fits_clear_errmsg();
    status = 0;
    if (fits_verify_chksum(f, &o.dataok, &o.hduok, &status) == 0) {
        o.ok = true;
        unsigned long d = 0, h = 0;
        int s2 = 0;
        if (ffgcks(f, &d, &h, &s2) == 0) { o.datasum = d; o.hdusum = h; }
    }
    int s2 = 0;
    fits_close_file(f, &s2);
    fits_clear_errmsg();
    return o;
}

struct CaseResult {
    std::string name;
    int reader_code = 0;
    bool reader_ok = false;
    Oracle oracle;
    bool oracle_fail = false;
    bool agree = false;
    // 快速路径复制（诊断）
    bool fast_computable = false;
    uint32_t fast_total = 0;
    uint32_t raw_hdu = 0;
    bool fast_equals_raw = false;
    bool typed_equals_raw = false;
    bool raw_equals_cfitsio = false;
};

CaseResult run_case(const std::string& name, const TileSpec& spec, bool use_i32 = false,
                    bool header_flip = false, bool data_flip = false) {
    CaseResult r;
    r.name = name;
    const std::string root = pjoin(g_root, name);
    make_product(root);
    const std::string tp = tile_path_of(root);
    if (!create_tile(tp, spec)) {
        r.reader_code = -999;
        return r;
    }
    if (header_flip || data_flip) {
        size_t off = 0;
        bool found = false;
        if (spec.compress) {
            // 压缩：翻转目标在 HDU2（压缩表）的 header 注释字节 / 压缩数据字节。
            fitsfile* f = nullptr;
            int status = 0;
            if (fits_open_file(&f, tp.c_str(), READONLY, &status) == 0) {
                int s2 = 0;
                fits_movabs_hdu(f, 2, nullptr, &s2);
                long long hs = 0, ds = 0, de = 0;
                int s3 = 0;
                if (ffghadll(f, &hs, &ds, &de, &s3) == 0) {
                    off = (size_t)(header_flip ? hs + 79 : ds + 100);
                    found = true;
                }
                int s4 = 0;
                fits_close_file(f, &s4);
            }
        } else {
            std::string hdr;
            size_t hb = 0;
            if (read_raw_header(tp, &hdr, &hb)) {
                if (header_flip) {
                    const size_t pos = hdr.find("ORACLE-HEADER-MARKER");
                    if (pos != std::string::npos) { off = pos + 3; found = true; }
                } else {
                    off = hb + 100;
                    found = true;
                }
            }
        }
        if (found) flip_byte(tp, off);
    }

    // reader
    AioHipsDataset* d = aio_hips_open(root.c_str(), AIO_HIPS_RD_SIGNAL);
    if (!d) {
        r.reader_code = -998;
        return r;
    }
    AioHipsTileStatus st{};
    if (use_i32) {
        std::vector<int32_t> buf((size_t)kNelem);
        r.reader_code = aio_hips_read_tile_i32_ex(d, kTileIpix, buf.data(), &st);
    } else {
        std::vector<float> buf((size_t)kNelem);
        r.reader_code = aio_hips_read_tile_f32_ex(d, kTileIpix, buf.data(), &st);
    }
    aio_hips_close(d);
    r.reader_ok = (r.reader_code == AIO_HIPS_TILE_OK);

    // oracle
    r.oracle = run_oracle(tp);
    r.oracle_fail = (r.oracle.has_datasum && r.oracle.dataok == -1) ||
                    (r.oracle.has_checksum && r.oracle.hduok == -1);
    // 核心 fail-closed 判据：oracle 判坏 ⇒ reader 不得 OK；oracle 判好 ⇒ reader 必须 OK。
    r.agree = (r.reader_ok == !r.oracle_fail);

    // 快速路径复制：raw header + typed pixels vs raw header + raw data
    std::string hdr;
    size_t hb = 0;
    if (read_raw_header(tp, &hdr, &hb)) {
        const uint32_t hsum = sum_be((const unsigned char*)hdr.data(), hdr.size());
        std::string dataraw;
        const size_t data_bytes = (size_t)kNelem * (size_t)(spec.bitpix == -64 ? 8 : 4);
        if (read_raw_range(tp, hb, data_bytes, &dataraw)) {
            r.raw_hdu = fold_ones((uint64_t)hsum +
                                  sum_be((const unsigned char*)dataraw.data(), dataraw.size()));
            r.raw_equals_cfitsio = (r.raw_hdu == (uint32_t)r.oracle.hdusum);
        }
        // typed（经 cfitsio 解码；压缩时口径不同）
        fitsfile* f = nullptr;
        int status = 0;
        if (fits_open_file(&f, tp.c_str(), READONLY, &status) == 0) {
            uint32_t typed = 0;
            if (spec.bitpix == -64) {
                std::vector<double> v((size_t)kNelem);
                long fp[2] = {1, 1};
                if (fits_read_pix(f, TDOUBLE, fp, kNelem, nullptr, v.data(), nullptr, &status) == 0)
                    typed = sum_typed_f64(v.data(), kNelem);
            } else if (spec.bitpix == 32) {
                std::vector<int32_t> v((size_t)kNelem);
                long fp[2] = {1, 1};
                if (fits_read_pix(f, TINT, fp, kNelem, nullptr, v.data(), nullptr, &status) == 0)
                    typed = sum_typed_i32(v.data(), kNelem);
            } else {
                std::vector<float> v((size_t)kNelem);
                long fp[2] = {1, 1};
                if (fits_read_pix(f, TFLOAT, fp, kNelem, nullptr, v.data(), nullptr, &status) == 0)
                    typed = sum_typed_f32(v.data(), kNelem);
            }
            int s2 = 0;
            fits_close_file(f, &s2);
            r.fast_computable = true;
            r.fast_total = fold_ones((uint64_t)hsum + typed);
            r.typed_equals_raw = (r.fast_total == r.raw_hdu);
        }
        fits_clear_errmsg();
    }
    return r;
}

std::string jbool(bool b) { return b ? "true" : "false"; }

void emit(const CaseResult& r, bool expect_fail, bool strict_checksum,
          bool expect_raw_equals) {
    const bool checksum_ok = (r.reader_code == AIO_HIPS_TILE_CHECKSUM_FAILED) == r.oracle_fail;
    const bool raw_ok = !expect_raw_equals || r.raw_equals_cfitsio;
    std::printf(
        "{\"case\":\"%s\",\"reader_code\":%d,\"reader_ok\":%s,"
        "\"has_datasum\":%s,\"has_checksum\":%s,"
        "\"oracle_dataok\":%d,\"oracle_hduok\":%d,\"oracle_fail\":%s,"
        "\"reader_checksum_failed\":%s,\"checksum_verdict_agrees\":%s,"
        "\"failclosed_agrees\":%s,\"expect_fail\":%s,\"strict_checksum\":%s,"
        "\"fast_computable\":%s,\"typed_equals_raw\":%s,\"raw_equals_cfitsio\":%s,"
        "\"expect_raw_equals\":%s,\"verdict\":\"%s\"}\n",
        r.name.c_str(), r.reader_code, jbool(r.reader_ok).c_str(),
        jbool(r.oracle.has_datasum).c_str(), jbool(r.oracle.has_checksum).c_str(),
        r.oracle.dataok, r.oracle.hduok, jbool(r.oracle_fail).c_str(),
        jbool(r.reader_code == AIO_HIPS_TILE_CHECKSUM_FAILED).c_str(),
        jbool(checksum_ok).c_str(), jbool(r.agree).c_str(),
        jbool(expect_fail).c_str(), jbool(strict_checksum).c_str(),
        jbool(r.fast_computable).c_str(), jbool(r.typed_equals_raw).c_str(),
        jbool(r.raw_equals_cfitsio).c_str(), jbool(expect_raw_equals).c_str(),
        (r.agree && (r.reader_ok == !expect_fail) && (!strict_checksum || checksum_ok) &&
         raw_ok)
            ? "PASS" : "FAIL");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <out_root>\n", argv[0]);
        return 64;
    }
    g_root = argv[1];
    mkdirs(g_root);

    struct Item {
        const char* name;
        TileSpec spec;
        bool i32;
        bool header_flip;
        bool data_flip;
        bool expect_fail;
        bool strict_checksum;
        bool expect_raw_equals;   // 未压缩才断言 raw 复算 == cfitsio
    };
    auto mk = [](int bitpix, bool nan, const char* cks, bool compress,
                 bool bscale, bool bzero, bool blank) {
        TileSpec s;
        s.bitpix = bitpix; s.nan_payloads = nan; s.cksum = cks; s.compress = compress;
        s.bscale = bscale; s.bzero = bzero; s.blank = blank;
        return s;
    };
    std::vector<Item> items = {
        {"f32_both",          mk(-32, false, "both", false, false, false, false), false, false, false, false, true, true},
        {"f32_datasum_only",  mk(-32, false, "datasum", false, false, false, false), false, false, false, false, true, true},
        {"f32_checksum_only", mk(-32, false, "checksum", false, false, false, false), false, false, false, false, true, true},
        {"f32_none",          mk(-32, false, "none", false, false, false, false), false, false, false, false, true, true},
        {"f32_nan_payloads",  mk(-32, true, "both", false, false, false, false), false, false, false, false, true, true},
        {"f64_both",          mk(-64, false, "both", false, false, false, false), false, false, false, false, true, true},
        {"f64_nan_payloads",  mk(-64, true, "both", false, false, false, false), false, false, false, false, true, true},
        {"i32_blank_both",    mk(32, false, "both", false, false, false, true), true, false, false, false, true, true},
        {"f32_bscale",        mk(-32, false, "both", false, true, false, false), false, false, false, false, true, true},
        {"f32_bzero",         mk(-32, false, "both", false, false, true, false), false, false, false, false, true, true},
        {"f32_compressed",    mk(-32, false, "both", true, false, false, false), false, false, false, false, true, false},
        {"f32_header_bitflip",mk(-32, false, "both", false, false, false, false), false, true, false, true, true, true},
        {"f32_data_bitflip",  mk(-32, false, "both", false, false, false, false), false, false, true, true, true, true},
        {"f32_compressed_header_bitflip", mk(-32, false, "both", true, false, false, false), false, true, false, true, true, false},
        // 压缩数据区损坏：cfitsio 解压可能先失败（INVALID_TILE）而非走到校验；
        // 只要求 fail-closed（不得 OK），不强制具体码。
        {"f32_compressed_data_bitflip", mk(-32, false, "both", true, false, false, false), false, false, true, true, false, false},
    };

    int n_fail = 0;
    int n_pass = 0;
    for (const Item& it : items) {
        CaseResult r = run_case(it.name, it.spec, it.i32, it.header_flip, it.data_flip);
        emit(r, it.expect_fail, it.strict_checksum, it.expect_raw_equals);
        const bool checksum_ok =
            (r.reader_code == AIO_HIPS_TILE_CHECKSUM_FAILED) == r.oracle_fail;
        const bool raw_ok = !it.expect_raw_equals || r.raw_equals_cfitsio;
        const bool pass = r.agree && (r.reader_ok == !it.expect_fail) &&
                          (!it.strict_checksum || checksum_ok) && raw_ok;
        if (pass) ++n_pass; else ++n_fail;
    }
    std::printf("SUMMARY cases=%zu pass=%d fail=%d\n", items.size(), n_pass, n_fail);
    return n_fail == 0 ? 0 : 1;
}
