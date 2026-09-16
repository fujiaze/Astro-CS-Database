// ============================================================================
// test_p0_io_hardening.cpp - P0 IO 域四连恶意 fixture 复现测试
// (bughunt_p0_io; 父任务规格 P0-2/P0-3/P0-4 AIO 侧)
//
// 覆盖 (全部为"恶意/损坏输入必须干净拒绝, 禁止崩溃/异常跨 C 边界"):
//   T1  XISF geometry="999999999:999999999:1" header_only → 拒绝 (rc<0),
//       修复前: calloc(4e18) NULL → 上层解引用崩溃 / ASAN allocation-too-big
//   T2  XISF xml_length 超上限 (64MB) header_only → 拒绝
//       (构造: 8 字节小端长度 = 0x08000000+1, 文件短读也必须干净拒绝)
//   T3  FITS NAXIS1/NAXIS2=999999999 header_only → 拒绝 (修复前同 T1)
//   T4  FITS NAXIS1×NAXIS2 巨头 read 路径 raw(data_size) 巨量分配 → 拒绝
//       (data_size 上限 1GB, 修复前 std::vector 巨量分配 bad_alloc 跨 C 边界)
//   T5  FITS CCD-TEMP='TBD' → aio_read_metadata 干净返回 (cal.ccd_temp=0.0
//       缺省, has_ccd_temp=1), 修复前 std::stod throw → terminate
//   T6  合法 XISF/FITS 回归: 正常文件 header_only/metadata 仍成功 (校验不误伤)
//
// 落位依据: 本模块 tests/ 目录既有独立 g++ 驱动模式
// (test_snr_unknown_block.cpp / test_precision_dual.cpp 同款)。
//
// 编译 (tests/ 目录, Linux mingw/wsl 通用):
//   g++ -std=c++17 -O1 -g -DAIO_ENABLE_FITS -DAIO_ENABLE_XISF \
//     -DAIO_ENABLE_HEALPIX -I../include -I../src -I../../third_party \
//     test_p0_io_hardening.cpp \
//     ../src/aio_api.cpp ../src/aio_xisf.cpp ../src/aio_fits.cpp \
//     ../src/aio_log.cpp ../src/aio_util.cpp ../src/aio_pipeline.cpp \
//     ../third_party/cfitsio/*.o -lzstd -llz4 -lz -lm \
//     -o test_p0_io_hardening
//   ./test_p0_io_hardening
// ============================================================================
#include "aio_fits.h"
#include "aio_xisf.h"
#include "aio_log.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", (msg)); ++g_pass; } \
    else      { printf("  [FAIL] %s\n", (msg)); ++g_fail; } \
} while (0)

static const char* g_dir = ".";

// ---------------------------------------------------------------------------
// fixture 生成
// ---------------------------------------------------------------------------

// 合法最小 XISF: magic(8) + xml_len(8) + xml(含 geometry + 关键字)
static void write_xisf(const char* path, int w, int h, int c) {
    char xml[1024];
    std::snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\"?><xisf root=\"XISF\"><Image geometry=\"%d:%d:%d\" "
        "sampleFormat=\"Float32\" location=\"0:0\"/>"
        "<FITSKeyword name=\"OBJECT\" value=\"test\"/></xisf>",
        w, h, c);
    FILE* fp = std::fopen(path, "wb");
    if (!fp) { fprintf(stderr, "fixture open failed: %s\n", path); return; }
    const uint8_t magic[8] = {'X','I','S','F','0','1','0','0'};
    std::fwrite(magic, 1, 8, fp);
    uint64_t xlen = (uint64_t)std::strlen(xml);
    for (int i = 0; i < 8; i++) { uint8_t b = (uint8_t)(xlen >> (8*i)); std::fwrite(&b,1,1,fp); }
    std::fwrite(xml, 1, (size_t)xlen, fp);
    // Image location=0:0 (内嵌数据) — header_only 不读数据, 但 read 路径要求;
    // 本测试仅用 header_only, 内嵌 XML 数据足够。
    std::fclose(fp);
}

// 恶意 XISF: geometry 由调用者传任意字符串 (如 999999999:999999999:1)
static void write_xisf_raw_geometry(const char* path, const char* geometry) {
    char xml[1024];
    std::snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\"?><xisf root=\"XISF\"><Image geometry=\"%s\" "
        "sampleFormat=\"Float32\" location=\"0:0\"/></xisf>", geometry);
    FILE* fp = std::fopen(path, "wb");
    if (!fp) return;
    const uint8_t magic[8] = {'X','I','S','F','0','1','0','0'};
    std::fwrite(magic, 1, 8, fp);
    uint64_t xlen = (uint64_t)std::strlen(xml);
    for (int i = 0; i < 8; i++) { uint8_t b = (uint8_t)(xlen >> (8*i)); std::fwrite(&b,1,1,fp); }
    std::fwrite(xml, 1, (size_t)xlen, fp);
    std::fclose(fp);
}

// 恶意 XISF: xml 长度字段超上限 (0x08000000+1 = 64MB+1), 文件本体极短
static void write_xisf_huge_xmllen(const char* path) {
    FILE* fp = std::fopen(path, "wb");
    if (!fp) return;
    const uint8_t magic[8] = {'X','I','S','F','0','1','0','0'};
    std::fwrite(magic, 1, 8, fp);
    uint64_t xlen = 64ull * 1024 * 1024 + 1;
    for (int i = 0; i < 8; i++) { uint8_t b = (uint8_t)(xlen >> (8*i)); std::fwrite(&b,1,1,fp); }
    std::fclose(fp);
}

// FITS 卡片写入辅助
static void fits_card(FILE* fp, const char* key, const char* value) {
    char card[81];
    std::memset(card, ' ', 80);
    std::memcpy(card, key, std::strlen(key));
    if (value) {
        card[8] = '='; card[9] = ' ';
        if (value[0] >= '0' && value[0] <= '9')
            std::memcpy(card + 10, value, std::strlen(value));
        else {
            card[10] = '\'';
            std::memcpy(card + 11, value, std::strlen(value));
            card[11 + std::strlen(value)] = '\'';
        }
    }
    std::fwrite(card, 1, 80, fp);
}

static void write_fits(const char* path, long naxis1, long naxis2,
                       const char* ccd_temp, bool with_end) {
    FILE* fp = std::fopen(path, "wb");
    if (!fp) return;
    int ncards = 0;
    auto card = [&](const char* key, const char* value) {
        fits_card(fp, key, value);
        ++ncards;
    };
    char v1[32], v2[32];
    std::snprintf(v1, sizeof(v1), "%ld", naxis1);
    std::snprintf(v2, sizeof(v2), "%ld", naxis2);
    card("SIMPLE", "T");
    card("BITPIX", "-32");
    card("NAXIS", "2");
    card("NAXIS1", v1);
    card("NAXIS2", v2);
    if (ccd_temp) card("CCD-TEMP", ccd_temp);
    card("OBJECT", "P0TEST");
    if (with_end) card("END", nullptr);
    // 补齐 2880 块 (按实际卡数)
    long pos = 80L * ncards;
    int pad = (int)((2880 - (pos % 2880)) % 2880);
    for (int i = 0; i < pad; i++) fputc(' ', fp);
    std::fclose(fp);
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc > 1) g_dir = argv[1];
    std::string p = std::string(g_dir) + "/";

    aio_set_precision_mode(0);  // FP32
    printf("== P0 IO hardening: malicious fixture tests (dir=%s) ==\n", g_dir);

    // ---- T0: 合法回归基准 (校验不得误伤合法输入) --------------------------
    {
        std::string f = p + "p0_ok.xisf";
        write_xisf(f.c_str(), 16, 8, 1);
        AIOImageData* img = aio_read_header_only(f.c_str());
        CHECK(img != nullptr, "T0a 合法 XISF (16x8x1) header_only 仍成功");
        if (img) {
            CHECK(aio_get_width(img) == 16 && aio_get_height(img) == 8,
                  "T0b 合法 XISF 几何透传正确");
            aio_free_image_data(img);
        }
        AIOImageMetadata meta = aio_read_metadata(f.c_str());
        CHECK(meta.geometry.width == 16, "T0c aio_read_metadata 合法 XISF 正常");
    }
    {
        std::string f = p + "p0_ok.fits";
        write_fits(f.c_str(), 16, 8, "-10.5", true);
        AIOImageData* img = aio_read_header_only(f.c_str());
        CHECK(img != nullptr, "T0d 合法 FITS (16x8) header_only 仍成功");
        if (img) aio_free_image_data(img);
        AIOImageMetadata meta = aio_read_metadata(f.c_str());
        CHECK(meta.calibration.has_ccd_temp == 1 && meta.calibration.ccd_temp == -10.5,
              "T0e FITS CCD-TEMP=-10.5 正常解析");
    }

    // ---- T1: XISF 恶意 geometry header_only ------------------------------
    {
        std::string f = p + "p0_bad_geometry.xisf";
        write_xisf_raw_geometry(f.c_str(), "999999999:999999999:1");
        AIOImageData* img = aio_read_header_only(f.c_str());
        CHECK(img == nullptr, "T1 XISF geometry=999999999:999999999:1 header_only 干净拒绝");
        if (img) aio_free_image_data(img);
        AIOImageMetadata meta = aio_read_metadata(f.c_str());
        CHECK(meta.geometry.width == 0 && meta.geometry.height == 0,
              "T1b 恶意 XISF metadata 返回零几何 (不崩溃)");
        write_xisf_raw_geometry(f.c_str(), "65536:2:1");
        img = aio_read_header_only(f.c_str());
        CHECK(img == nullptr, "T1c XISF w=65536 (>65535) 拒绝 (与 read_file 同口径)");
        if (img) aio_free_image_data(img);
        write_xisf_raw_geometry(f.c_str(), "0:8:1");
        img = aio_read_header_only(f.c_str());
        CHECK(img == nullptr, "T1d XISF w=0 拒绝");
        if (img) aio_free_image_data(img);
    }

    // ---- T2: XISF xml_length 超上限 ---------------------------------------
    {
        std::string f = p + "p0_huge_xmllen.xisf";
        write_xisf_huge_xmllen(f.c_str());
        AIOImageData* img = aio_read_header_only(f.c_str());
        CHECK(img == nullptr, "T2 XISF xml_length=64MB+1 干净拒绝 (无巨量分配)");
        if (img) aio_free_image_data(img);
    }

    // ---- T3: FITS 恶意 NAXIS header_only ----------------------------------
    {
        std::string f = p + "p0_bad_naxis.fits";
        write_fits(f.c_str(), 999999999L, 999999999L, nullptr, true);
        AIOImageData* img = aio_read_header_only(f.c_str());
        CHECK(img == nullptr, "T3 FITS NAXIS1=NAXIS2=999999999 header_only 干净拒绝");
        if (img) aio_free_image_data(img);
        AIOImageMetadata meta = aio_read_metadata(f.c_str());
        CHECK(meta.geometry.width == 0, "T3b 恶意 FITS metadata 零几何 (不崩溃)");
    }

    // ---- T4: FITS 巨量 data_size (read 路径 raw(data_size) 上限) ----------
    {
        // NAXIS1=NAXIS2=65535 → data_size = 65535^2*4 ≈ 17.2GB > 1GB 上限;
        // 头完整 (带 END), 头解析通过, 在 raw(hdr.data_size) 分配前被上限拒绝。
        std::string f = p + "p0_huge_data.fits";
        write_fits(f.c_str(), 65535L, 65535L, nullptr, true);
        AIOImageData* img = aio_read(f.c_str());
        CHECK(img == nullptr, "T4 FITS 65535x65535x f32 (17GB data) 读取干净拒绝");
        if (img) aio_free_image_data(img);
        // 边界内合法: 4096x4096 头 (有 END 无数据) — 走到数据读取失败而非几何拒绝
        std::string f2 = p + "p0_ok_nodata.fits";
        write_fits(f2.c_str(), 4096L, 4096L, nullptr, true);
        AIOImageMetadata meta = aio_read_metadata(f2.c_str());
        CHECK(meta.geometry.width == 4096, "T4b 4096x4096 合法几何 metadata 正常 (65535 内不误伤)");
    }

    // ---- T5: FITS CCD-TEMP='TBD' (std::stod 异常) --------------------------
    {
        std::string f = p + "p0_tbd_temp.fits";
        write_fits(f.c_str(), 16, 8, "TBD", true);
        AIOImageMetadata meta = aio_read_metadata(f.c_str());
        CHECK(meta.geometry.width == 16, "T5a CCD-TEMP='TBD' 时 aio_read_metadata 正常返回");
        CHECK(meta.calibration.has_ccd_temp == 1 && meta.calibration.ccd_temp == 0.0,
              "T5b CCD-TEMP='TBD' 解析失败取缺省 0.0 (has_ccd_temp=1)");
        // 值返回型 C 边界屏障抽查
        AIOImageData* img = aio_read_header_only(f.c_str());
        CHECK(aio_get_keyword_count(img) >= 0 && aio_get_width(img) == 16,
              "T5c 修复后 C 边界入口行为正常");
        if (img) aio_free_image_data(img);
    }

    printf("== RESULT: pass=%d fail=%d ==\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
