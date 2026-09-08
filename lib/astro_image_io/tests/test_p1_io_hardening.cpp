// ============================================================================
// test_p1_io_hardening.cpp - P1 IO 域恶意 fixture 复现测试
// (bughunt_p1_batchI; 对齐 test_p0_io_hardening.cpp 模式)
//
// 覆盖 (全部为"恶意/损坏输入必须干净拒绝, 禁止崩溃/异常跨 C 边界"):
//   T1  FP64 写路径: fits_write_file(dtype=1, data=nullptr, data_f64 非空)
//       → 拒绝 (rc=-1, 无崩溃), 修复前 memcpy(NULL) 段错误 / BITPIX=-32
//       静默截断 FP64 科学数值
//   T2  FP64 写路径: fits_write_file(dtype=1, data 非空 FP32) → 拒绝
//       (dtype=1 即 FP64 数据, 禁止静默降精度, 不看 data 指针)
//   T3  正常 FP32 写→读回归: 校验不误伤合法路径
//   T4  HCSD JSON 头超上限: 恶意 uncompLen/compLen (0x0800_0001 ≈ 128MB+1
//       > 64MB 上限) → 拒绝 (rc=HIO_ERR_BOUNDS=-7), 修复前 4GB-1 无界分配
//   T5  HCSD n_pix 超上限: 恶意 JSON n_pix=2^62 → 拒绝 (-7),
//       修复前 malloc(2^62*12) 巨量分配打爆内存
//   T6  HCSD 巨 n_pix 短文件: n_pix 合法域内 (2^30) 但文件短 → 干净
//       拒绝 (-2 HIO_ERR_FILE), 不挂起不崩溃
//   T7  HCSD read_leaf 索引项越界: data_offset 越过 ipix 数组 → 拒绝 (-7)
//   T8  异常屏障: hips reader tile_width=2048 恶意产品集 → open 返回
//       nullptr (P1-3 校验); last_error 稳定可读 (0 try → 11 try 屏障面)
//
// 落位依据: 本模块 tests/ 目录既有独立 g++ 驱动模式
// (test_p0_io_hardening.cpp / test_precision_dual.cpp 同款)。
//
// 编译 (lib/astro_image_io/tests/ 目录):
//   g++ -std=c++17 -O1 -g -DAIO_ENABLE_FITS -DAIO_ENABLE_XISF \
//     -DAIO_ENABLE_HEALPIX -I../include -I../src -I../../common \
//     test_p1_io_hardening.cpp \
//     ../src/aio_api.cpp ../src/aio_xisf.cpp ../src/aio_fits.cpp \
//     ../src/aio_log.cpp ../src/aio_util.cpp ../src/aio_pipeline.cpp \
//     ../src/healpix/aio_healpix_io.cpp \
//     ../third_party/cfitsio/*.o -lzstd -llz4 -lz -lm \
//     -o test_p1_io_hardening
//   ./test_p1_io_hardening
// ============================================================================
#include "aio_fits.h"
#include "aio_healpix_io.h"
#include "aio_log.h"
#include "aio_hips_reader.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#ifdef HAS_ZSTD
#include <zstd.h>
#endif

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", (msg)); ++g_pass; } \
    else      { printf("  [FAIL] %s\n", (msg)); ++g_fail; } \
} while (0)

// ---------------------------------------------------------------------------
// HCSD fixture 生成 (镜像 aio_healpix_io.cpp 实际布局:
// magic(4)+nside(4)+nested(4) | uncompLen(4)+compLen(4) | zstd(json) |
// leaf_index(49152*24) | ipix(n_pix*8) | pixel(n_pix*4))
// ---------------------------------------------------------------------------
#ifdef HAS_ZSTD
static std::vector<uint8_t> zstd_compress_bytes(const uint8_t* src, size_t n) {
    size_t bound = ZSTD_compressBound(n);
    std::vector<uint8_t> out(bound);
    size_t rc = ZSTD_compress(out.data(), bound, src, n, 5);
    out.resize(rc == 0 ? 0 : rc);
    return out;
}
#else
// 无 HAS_ZSTD 时 HCSD 读路径把头块按"未压缩"处理需要 compLen==uncompLen;
// 为独立于 zstd 库链接, fixture 直接存"伪压缩块"(compJson 原样字节,
// uncompLen==compLen), 让 hcsd_read 走 zstd 关闭分支按原样读入。
static std::vector<uint8_t> zstd_compress_bytes(const uint8_t* src, size_t n) {
    return std::vector<uint8_t>(src, src + n);
}
#endif

// 布局对齐 aio_hcsd_read 实读: magic(4) | uncompLen(4) | compLen(4) |
// zstd(json{含 nside,nested,n_pix}) | [leaf_index 49152*24] | ipix | pixel
static void write_hcsd_header(const char* path, uint32_t /*nside 借 JSON 传*/,
                              int /*nested 借 JSON 传*/,
                              uint32_t uncompLen, uint32_t compLen,
                              const std::vector<uint8_t>& compJson,
                              bool append_leaf_index = true) {
    FILE* fp = std::fopen(path, "wb");
    if (!fp) { fprintf(stderr, "fixture open failed: %s\n", path); return; }
    const uint8_t magic[4] = {'H','C','S','D'};
    std::fwrite(magic, 1, 4, fp);
    std::fwrite(&uncompLen, 4, 1, fp);
    std::fwrite(&compLen, 4, 1, fp);
    if (compLen > 0 && !compJson.empty())
        std::fwrite(compJson.data(), 1, compJson.size() < compLen ? compJson.size() : compLen, fp);
    if (append_leaf_index) {
        // 空白子叶索引表 (49152 * 24 字节)
        std::vector<uint8_t> zeros(49152 * 24, 0);
        std::fwrite(zeros.data(), 1, zeros.size(), fp);
    }
    std::fclose(fp);
}

int main(int argc, char** argv) {
    const char* dir = (argc > 1) ? argv[1] : ".";
    printf("=== test_p1_io_hardening (bughunt_p1_batchI) dir=%s ===\n", dir);

    // ------------------------------------------------------------------
    // T1: FP64 数据 (data=nullptr, data_f64 非空, dtype=1) 写 FITS → 拒绝
    // ------------------------------------------------------------------
    {
        AIOImageData img;
        std::memset(&img, 0, sizeof(img));
        std::vector<double> px(4 * 4, 1.5);
        img.data = nullptr;
        img.data_f64 = px.data();
        img.dtype = 1;
        img.width = 4; img.height = 4; img.channels = 1;
        img.bits_per_sample = 64; img.float_sample = 1;
        std::snprintf(img.source_format, sizeof(img.source_format), "fits");
        std::string path = std::string(dir) + "/p1_t1_out.fits";
        int rc = aio_write_fits(&img, path.c_str());
        CHECK(rc != 0, "T1 FP64(data_f64 only) 写路径硬失败 (拒绝静默降精度)");
        std::FILE* ck = std::fopen(path.c_str(), "rb");
        bool not_created = (ck == nullptr);
        if (ck) std::fclose(ck);
        CHECK(not_created, "T1 失败时未产生半成品 FITS 文件");
    }

    // ------------------------------------------------------------------
    // T2: dtype=1 + FP32 data 携带 → 仍拒绝 (dtype 是科学语义权威)
    // ------------------------------------------------------------------
    {
        AIOImageData img;
        std::memset(&img, 0, sizeof(img));
        std::vector<float> px(4 * 4, 2.0f);
        img.data = px.data();
        img.data_f64 = nullptr;
        img.dtype = 1;                       // FP64 数据, FP32 缓冲无效
        img.width = 4; img.height = 4; img.channels = 1;
        img.bits_per_sample = -32; img.float_sample = 1;
        std::snprintf(img.source_format, sizeof(img.source_format), "fits");
        std::string path = std::string(dir) + "/p1_t2_out.fits";
        int rc = aio_write_fits(&img, path.c_str());
        CHECK(rc != 0, "T2 dtype=1(FP64) 即使 data 非空也硬失败");
    }

    // ------------------------------------------------------------------
    // T3: 合法 FP32 写→读回归 (校验不误伤)
    // ------------------------------------------------------------------
    {
        AIOImageData img;
        std::memset(&img, 0, sizeof(img));
        std::vector<float> px(8 * 6);
        for (size_t i = 0; i < px.size(); ++i) px[i] = (float)i * 0.25f;
        img.data = px.data();
        img.data_f64 = nullptr;
        img.dtype = 0;
        img.width = 8; img.height = 6; img.channels = 1;
        img.bits_per_sample = -32; img.float_sample = 1;
        std::snprintf(img.source_format, sizeof(img.source_format), "fits");
        std::string path = std::string(dir) + "/p1_t3_out.fits";
        int rc = aio_write_fits(&img, path.c_str());
        CHECK(rc == 0, "T3 合法 FP32 写入成功 (不误伤)");
        if (rc == 0) {
            AIOImageData* back = aio_read_fits(path.c_str());
            bool ok = back && back->data && back->width == 8 && back->height == 6;
            if (ok) {
                ok = (back->data[0] == 0.0f) && (back->data[px.size()-1] == px.back());
            }
            CHECK(ok, "T3 写→读回环数值一致");
            if (back) aio_free_image_data(back);
        }
        std::remove(path.c_str());
    }

    // ------------------------------------------------------------------
    // T4: HCSD JSON 头超上限 (uncompLen=0x08000001 > 64MB) → -7 拒绝
    // ------------------------------------------------------------------
    {
        std::string path = std::string(dir) + "/p1_t4.hcsd";
        const char* j = "{\"nside\":64,\"nested\":1,\"n_pix\":4}";
        std::vector<uint8_t> jsonBytes(j, j + std::strlen(j));
        std::vector<uint8_t> comp = zstd_compress_bytes(jsonBytes.data(), jsonBytes.size());
        write_hcsd_header(path.c_str(), 64, 1,
                          0x08000001u /*uncompLen 超上限*/, (uint32_t)comp.size(), comp,
                          /*append_leaf_index=*/false);
        uint32_t nside = 0; int nested = 0; uint64_t n_pix = 0;
        uint64_t* ipix = nullptr; float* pixel = nullptr; char* meta = nullptr;
        int rc = aio_hcsd_read(path.c_str(), &nside, &nested, &n_pix,
                               &ipix, &pixel, &meta);
        CHECK(rc == -7, "T4 HCSD JSON 头超上限 → HIO_ERR_BOUNDS(-7)");
        std::remove(path.c_str());
    }

    // ------------------------------------------------------------------
    // T5: HCSD n_pix 超上限 (恶意 JSON n_pix=2^62) → -7 拒绝
    // ------------------------------------------------------------------
    {
        std::string path = std::string(dir) + "/p1_t5.hcsd";
        char j[128];
        std::snprintf(j, sizeof(j), "{\"nside\":64,\"nested\":true,\"n_pix\":%llu}",
                      (unsigned long long)(1ULL << 62));
        std::vector<uint8_t> jsonBytes((uint8_t*)j, (uint8_t*)j + std::strlen(j));
        std::vector<uint8_t> comp = zstd_compress_bytes(jsonBytes.data(), jsonBytes.size());
        write_hcsd_header(path.c_str(), 64, 1, (uint32_t)jsonBytes.size(),
                          (uint32_t)comp.size(), comp, /*append_leaf_index=*/false);
        uint32_t nside = 0; int nested = 0; uint64_t n_pix = 0;
        uint64_t* ipix = nullptr; float* pixel = nullptr; char* meta = nullptr;
        int rc = aio_hcsd_read(path.c_str(), &nside, &nested, &n_pix,
                               &ipix, &pixel, &meta);
        CHECK(rc == -7, "T5 HCSD n_pix=2^62 超上限 → HIO_ERR_BOUNDS(-7)");
        std::remove(path.c_str());
    }

    // ------------------------------------------------------------------
    // T6: n_pix 域内但文件短 (n_pix=2^30, 无实际数据) → -2 干净拒绝
    // ------------------------------------------------------------------
    {
        std::string path = std::string(dir) + "/p1_t6.hcsd";
        char j[128];
        std::snprintf(j, sizeof(j), "{\"nside\":64,\"nested\":true,\"n_pix\":%u}",
                      1u << 30);
        std::vector<uint8_t> jsonBytes((uint8_t*)j, (uint8_t*)j + std::strlen(j));
        std::vector<uint8_t> comp = zstd_compress_bytes(jsonBytes.data(), jsonBytes.size());
        write_hcsd_header(path.c_str(), 64, 1, (uint32_t)jsonBytes.size(),
                          (uint32_t)comp.size(), comp, /*append_leaf_index=*/true);
        uint32_t nside = 0; int nested = 0; uint64_t n_pix = 0;
        uint64_t* ipix = nullptr; float* pixel = nullptr; char* meta = nullptr;
        int rc = aio_hcsd_read(path.c_str(), &nside, &nested, &n_pix,
                               &ipix, &pixel, &meta);
        CHECK(rc == -2, "T6 HCSD 巨 n_pix 短文件 → HIO_ERR_FILE(-2) 干净拒绝");
        std::remove(path.c_str());
    }

    // ------------------------------------------------------------------
    // T7: hcsd_read_leaf 索引项越界 (data_offset 越过 ipix 数组) → -7
    //     构造: 合法头 (n_pix=16) + 索引表 leaf0 项 data_offset=2^40
    // ------------------------------------------------------------------
    {
        std::string path = std::string(dir) + "/p1_t7.hcsd";
        const char* j = "{\"nside\":64,\"nested\":true,\"n_pix\":16}";
        std::vector<uint8_t> jsonBytes((const uint8_t*)j,
                                       (const uint8_t*)j + std::strlen(j));
        std::vector<uint8_t> comp = zstd_compress_bytes(jsonBytes.data(), jsonBytes.size());
        {
            FILE* fp = std::fopen(path.c_str(), "wb");
            if (fp) {
                const uint8_t magic[4] = {'H','C','S','D'};
                std::fwrite(magic, 1, 4, fp);
                uint32_t ul = (uint32_t)jsonBytes.size();
                std::fwrite(&ul, 4, 1, fp);             // uncompLen
                uint32_t cl = (uint32_t)comp.size();
                std::fwrite(&cl, 4, 1, fp);             // compLen
                std::fwrite(comp.data(), 1, cl, fp);
                // leaf index 项 0 (字节 0..23): leaf_ipix(0..7), data_offset(8..15),
                // data_length(16..23) —— offset=2^40 越界 + length=4 保证走校验
                std::vector<uint8_t> idx(49152 * 24, 0);
                uint64_t bad_off = 1ULL << 40;
                uint64_t len = 4;
                std::memcpy(&idx[8], &bad_off, 8);
                std::memcpy(&idx[16], &len, 8);
                std::fwrite(idx.data(), 1, idx.size(), fp);
                std::vector<uint8_t> px(16 * 8 + 16 * 4, 0);
                std::fwrite(px.data(), 1, px.size(), fp);
                std::fclose(fp);
            }
        }
        uint64_t n_pix = 0; uint64_t* ipix = nullptr; float* pixel = nullptr;
        int rc = aio_hcsd_read_leaf(path.c_str(), 0, &n_pix, &ipix, &pixel);
        CHECK(rc == -7, "T7 read_leaf 索引项 data_offset 越界 → HIO_ERR_BOUNDS(-7)");
        std::remove(path.c_str());
    }

    // ------------------------------------------------------------------
    // T8: hips reader 恶意产品集 (tile_width=2048) → open 拒绝 (P1-3)
    // ------------------------------------------------------------------
    {
        std::string root = std::string(dir) + "/p1_t8_product/signal";
        std::string cmd = "mkdir -p '" + root + "'";
        if (std::system(cmd.c_str()) != 0) { /* best-effort */ }
        std::string props = "hips_version = 1.4\nhips_order = 3\nhips_tile_width = 2048\n";
        {
            FILE* fp = std::fopen((root + "/properties").c_str(), "wb");
            if (fp) { std::fwrite(props.data(), 1, props.size(), fp); std::fclose(fp); }
        }
        AioHipsDataset* d = aio_hips_open(dir, AIO_HIPS_RD_SIGNAL);
        CHECK(d == nullptr, "T8 恶意 tile_width=2048 产品集 → open 拒绝");
        const char* err = aio_hips_reader_last_error();
        CHECK(err && std::strlen(err) > 0, "T8 last_error 稳定可读 (屏障面 0→11 try)");
        std::string rm = "rm -rf '" + std::string(dir) + "/p1_t8_product'";
        std::system(rm.c_str());
    }

    printf("\n=== 汇总: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
