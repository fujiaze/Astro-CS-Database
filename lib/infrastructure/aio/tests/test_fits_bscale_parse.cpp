// ============================================================================
// test_fits_bscale_parse.cpp — FITS BSCALE/BZERO 解析失败的输入面负面测试
//
// 缺陷 (被本测试锁定):
//   aio_fits.cpp 的 parse_fits_header 手工解析路径对 BSCALE/BZERO 关键字的
//   std::stod 用 catch(...) 静默吞掉异常。解析失败时 bscale/bzero 停留在
//   初值 (1.0 / 0.0), 于是:
//
//     * 读路径的 "if (bscale != 1.0 || bzero != 0.0)" 缩放守卫形同虚设 ——
//       1.0/0.0 让守卫判假, 全幅像素**跳过**缩放, 物理值 = 原始 ADU
//       (而按 FITS Standard 4.0 §4.4.2.5 应为 BSCALE*raw + BZERO);
//     * 且**一条日志都不打** (INFO 缩放日志在守卫之内, 同样被跳过),
//       非法输入与 "文件本来就没有 BSCALE" 完全无法区分 —— 输入面静默错误。
//
// 权威依据:
//   docs/engineering/DATA_ARTIFACTS.md DATA-IMG-RAW-001 / DATA-IMG-CAL-001 ——
//   读入后物理值 = BSCALE*样本 + BZERO, "在读取时已施加", 实现锚即本文件。
//   docs/science/CALIBRATION.md §ADU 域 (冻结) = FITS 物理值域。
//   FITS Standard 4.0 §4.4.2.5: BSCALE 缺省 1.0 (合法缺省), 但**存在而非法**
//   的值属损坏头, 不得静默按缺省缩放。
//
// 覆盖 (负例为主, 合法回归防误伤):
//   N1  BSCALE='N/A'   (字符串)      -> 必须走失败路径 (不可静默 1.0)
//   N2  BZERO='N/A'    (字符串)      -> 同上
//   N3  BSCALE='1.0e'  (out_of_range/尾随垃圾) -> 同上
//   N4  BSCALE 合法 2.0 -> 必须**正常**缩放 (raw*2.0), 逐位回归
//   N5  无 BSCALE 关键字 -> 合法缺省 1.0, 像素保持 raw (不得误伤)
//   N6  BSCALE=1.0 显式  -> 合法恒等, 像素保持 raw (不得误伤)
//
// fixture 全部在进程内现写 (临时目录), 不读 testdata/, 不入库任何 .fits
// (根 .gitignore:66 全局 *.fits ⇒ 仓内不跟踪 FITS 夹具)。
// ============================================================================
#include "aio_fits.h"
#include "astro_image_io.h"
#include "aio_log.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <filesystem>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", (msg)); ++g_pass; } \
    else      { printf("  [FAIL] %s\n", (msg)); ++g_fail; } \
} while (0)

static const char* g_dir = ".";

// 80 字节 FITS 卡片; value 传 nullptr 表示无值 (如 END/注释卡)
static void fits_card(FILE* fp, const char* key, const char* value) {
    char card[81];
    std::memset(card, ' ', 80);
    std::memcpy(card, key, std::strlen(key) < 8 ? std::strlen(key) : 8);
    if (value) {
        card[8] = '='; card[9] = ' ';
        // 字符串值加引号; 数值原样
        if (value[0] == '\'') {
            std::memcpy(card + 10, value, std::strlen(value));
        } else if (strchr(value, '/') != nullptr || value[0] == 'N' ||
                   value[0] == 'n' || value[0] == 'T' || value[0] == 't') {
            card[10] = '\'';
            std::memcpy(card + 11, value, std::strlen(value));
            card[11 + std::strlen(value)] = '\'';
        } else {
            std::memcpy(card + 10, value, std::strlen(value));
        }
    }
    std::fwrite(card, 1, 80, fp);
}

// 写一个 2x1、BITPIX=16 (大端 int16) 的 FITS: 像素原始值 {raw0, raw1}。
// bscale_card/bzero_card 为 nullptr 时不下发该关键字 (合法缺省路径)。
static bool write_fits16(const char* path,
                         const char* bscale_card, const char* bzero_card,
                         int16_t raw0, int16_t raw1) {
    FILE* fp = std::fopen(path, "wb");
    if (!fp) return false;
    int ncards = 0;
    auto card = [&](const char* k, const char* v) { fits_card(fp, k, v); ++ncards; };
    card("SIMPLE", "T");
    card("BITPIX", "16");
    card("NAXIS", "2");
    card("NAXIS1", "2");
    card("NAXIS2", "1");
    if (bscale_card) card("BSCALE", bscale_card);
    if (bzero_card)  card("BZERO",  bzero_card);
    card("OBJECT", "BSCTEST");
    card("END", nullptr);
    // 补齐 2880 块
    long pos = 80L * ncards;
    int pad = (int)((2880 - (pos % 2880)) % 2880);
    for (int i = 0; i < pad; i++) fputc(' ', fp);
    // 数据区: 2x1 int16, 大端 (FITS 规范), 补齐到 2880
    uint8_t data[4] = {
        (uint8_t)(((uint16_t)raw0 >> 8) & 0xFF), (uint8_t)((uint16_t)raw0 & 0xFF),
        (uint8_t)(((uint16_t)raw1 >> 8) & 0xFF), (uint8_t)((uint16_t)raw1 & 0xFF)
    };
    std::fwrite(data, 1, 4, fp);
    int dpad = (int)((2880 - (4 % 2880)) % 2880);
    for (int i = 0; i < dpad; i++) fputc('\0', fp);
    std::fclose(fp);
    return true;
}

int main(int argc, char** argv) {
    // fixture 落系统临时目录 (eng/tests/unit/io_ownership_test.cpp:96-98 同款),
    // 退出前清理: 不在工作树、也不在 build 树留任何 .fits 残留
    // (根 .gitignore:66 全局 *.fits)。
    std::string dir = (argc > 1) ? std::string(argv[1])
                                : (std::filesystem::temp_directory_path() /
                                   "aio_fits_bscale_parse").string();
    std::filesystem::create_directories(dir);
    std::string p = dir + "/";
    std::vector<std::string> made;
    auto fixture = [&](const char* name) {
        std::string fp = p + name;
        made.push_back(fp);
        return fp;
    };
    aio_set_precision_mode(0);  // FP32
    printf("== FITS BSCALE/BZERO parse negative tests (dir=%s) ==\n", dir.c_str());

    // ---- 负例: 非法 BSCALE/BZERO 必须干净拒绝 (不得静默按 1.0/0.0 缩放) ----
    {
        std::string f = fixture("bscale_neg_str.fits");
        if (!write_fits16(f.c_str(), "N/A", nullptr, 450, 451)) { fprintf(stderr, "fixture failed\n"); return 2; }
        AIOImageData* img = aio_read(f.c_str());
        // 修复前: 返回非空, 像素=raw(450/451) (错比例), 无日志 -> 此断言红
        CHECK(img == nullptr, "N1 BSCALE='N/A' 读取干净拒绝 (修复前: 静默按 1.0 返回 raw)");
        if (img) {
            float* px = aio_get_pixel_data(img);
            printf("         [诊断] 修复前像素: %.1f %.1f (应 450*BSCALE, 但 BSCALE 非法)\n", px[0], px[1]);
            aio_free_image_data(img);
        }
    }
    {
        std::string f = fixture("bzero_neg_str.fits");
        if (!write_fits16(f.c_str(), nullptr, "TBD", 450, 451)) return 2;
        AIOImageData* img = aio_read(f.c_str());
        CHECK(img == nullptr, "N2 BZERO='TBD' 读取干净拒绝 (修复前: 静默按 0.0 返回 raw)");
        if (img) {
            float* px = aio_get_pixel_data(img);
            printf("         [诊断] 修复前像素: %.1f %.1f\n", px[0], px[1]);
            aio_free_image_data(img);
        }
    }
    {
        std::string f = fixture("bscale_neg_oor.fits");
        if (!write_fits16(f.c_str(), "1.0e", nullptr, 450, 451)) return 2;
        AIOImageData* img = aio_read(f.c_str());
        CHECK(img == nullptr, "N3 BSCALE='1.0e' (非法) 读取干净拒绝");
        if (img) aio_free_image_data(img);
    }
    {
        std::string f = fixture("bscale_neg_huge.fits");
        if (!write_fits16(f.c_str(), "1e400", nullptr, 450, 451)) return 2;
        AIOImageData* img = aio_read(f.c_str());
        CHECK(img == nullptr, "N3b BSCALE='1e400' (out_of_range) 读取干净拒绝");
        if (img) aio_free_image_data(img);
    }

    // ---- 合法回归: 缩放/缺省/恒等 必须逐位不变 (校验不得误伤合法输入) ----
    {
        std::string f = fixture("bscale_ok.fits");
        if (!write_fits16(f.c_str(), "2.0", nullptr, 450, 451)) return 2;
        AIOImageData* img = aio_read(f.c_str());
        CHECK(img != nullptr, "N4 合法 BSCALE=2.0 读取成功");
        if (img) {
            float* px = aio_get_pixel_data(img);
            bool ok = (fabsf(px[0] - 900.0f) < 1e-4f) && (fabsf(px[1] - 902.0f) < 1e-4f);
            CHECK(ok, "N4b 合法 BSCALE=2.0 像素正确缩放 450->900, 451->902");
            aio_free_image_data(img);
        }
    }
    {
        std::string f = fixture("bscale_absent.fits");
        if (!write_fits16(f.c_str(), nullptr, nullptr, 450, 451)) return 2;
        AIOImageData* img = aio_read(f.c_str());
        CHECK(img != nullptr, "N5 无 BSCALE 合法缺省读取成功 (不得误伤)");
        if (img) {
            float* px = aio_get_pixel_data(img);
            bool ok = (fabsf(px[0] - 450.0f) < 1e-4f) && (fabsf(px[1] - 451.0f) < 1e-4f);
            CHECK(ok, "N5b 无 BSCALE 像素保持 raw (缺省 1.0)");
            aio_free_image_data(img);
        }
    }
    {
        std::string f = fixture("bscale_one.fits");
        if (!write_fits16(f.c_str(), "1.0", nullptr, 450, 451)) return 2;
        AIOImageData* img = aio_read(f.c_str());
        CHECK(img != nullptr, "N6 显式 BSCALE=1.0 读取成功 (合法恒等)");
        if (img) {
            float* px = aio_get_pixel_data(img);
            bool ok = (fabsf(px[0] - 450.0f) < 1e-4f) && (fabsf(px[1] - 451.0f) < 1e-4f);
            CHECK(ok, "N6b 显式 BSCALE=1.0 像素保持 raw (恒等)");
            aio_free_image_data(img);
        }
    }

    // 清理 fixture (不留 .fits 残留在工作树/构建树)
    for (const std::string& fp : made) {
        std::error_code ec;
        std::filesystem::remove(fp, ec);
    }

    printf("== RESULT: pass=%d fail=%d ==\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
