#include "fits_reader.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace drizzle {

// FITS 格式常量
static const size_t FITS_BLOCK_SIZE = 2880;
static const size_t FITS_CARD_SIZE  = 80;

// ============================================================================
// UTF-8 路径文件打开 (Windows 下支持中文路径)
// ============================================================================
static FILE* openFileUtf8(const std::string& path, const char* mode) {
#ifdef _WIN32
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wpath_len <= 0) return std::fopen(path.c_str(), mode);
    std::wstring wpath(wpath_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wpath_len);
    int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
    std::wstring wmode(wmode_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, &wmode[0], wmode_len);
    return _wfopen(wpath.c_str(), wmode.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

// ============================================================================
// 字符串辅助
// ============================================================================
static std::string trimStr(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

// 去除 FITS 字符串值的单引号, 并将 '' 转义为 '
static std::string stripFitsQuotes(const std::string& s) {
    std::string v = trimStr(s);
    if (v.size() >= 2 && v.front() == '\'' && v.back() == '\'') {
        v = v.substr(1, v.size() - 2);
        size_t pos;
        while ((pos = v.find("''")) != std::string::npos) {
            v.replace(pos, 2, "'");
        }
        size_t end = v.find_last_not_of(' ');
        if (end != std::string::npos) v = v.substr(0, end + 1);
    }
    return v;
}

// 解析数值 (兼容 FITS 的 'D' 指数记法, 如 1.5D-3)
static double parseDouble(const std::string& s) {
    std::string t = trimStr(s);
    for (auto& c : t) {
        if (c == 'D' || c == 'd') c = 'E';
    }
    if (t.empty()) return 0.0;
    return std::strtod(t.c_str(), nullptr);
}

static int parseInt(const std::string& s) {
    std::string t = trimStr(s);
    if (t.empty()) return 0;
    return (int)std::strtol(t.c_str(), nullptr, 10);
}

// ============================================================================
// FITS 头卡解析
// ============================================================================
struct FitsCard {
    std::string key;
    std::string value;   // 已去除引号的值
    bool has_value = false;
};

static FitsCard parseCard(const char card[80]) {
    FitsCard c;
    std::string line(card, 80);

    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
        // 无 '=' 的卡 (END, COMMENT, HISTORY, 空行)
        c.key = trimStr(line.substr(0, 8));
        return c;
    }

    c.key = trimStr(line.substr(0, eq_pos));
    if (c.key.empty() || c.key.size() > 8) {
        c.key.clear();
        return c;
    }

    std::string rest = line.substr(eq_pos + 1);

    // 查找注释 '/' (字符串引号内不算)
    int slash_pos = -1;
    bool in_string = false;
    for (size_t i = 0; i < rest.size(); i++) {
        if (rest[i] == '\'') in_string = !in_string;
        else if (rest[i] == '/' && !in_string) { slash_pos = (int)i; break; }
    }

    std::string val_str;
    if (slash_pos >= 0) val_str = rest.substr(0, slash_pos);
    else                 val_str = rest;
    val_str = trimStr(val_str);

    if (!val_str.empty()) {
        c.has_value = true;
        if (val_str[0] == '\'') c.value = stripFitsQuotes(val_str);
        else                    c.value = val_str;
    }
    return c;
}

// ============================================================================
// SIP 系数键分类
// 返回: 0=非SIP, 1=A, 2=B, 3=AP, 4=BP
// 键格式: A_i_j, B_i_j, AP_i_j, BP_i_j
// ============================================================================
static int classifySipKey(const std::string& key, int& i, int& j) {
    if (key.size() < 4) return 0;

    int type = 0;
    size_t prefix_len = 0;

    // 优先匹配两字符前缀 AP_ / BP_ (避免被 A_ / B_ 误匹配)
    if (key.size() >= 4 && key[0] == 'A' && key[1] == 'P' && key[2] == '_') {
        type = 3; prefix_len = 3;
    } else if (key.size() >= 4 && key[0] == 'B' && key[1] == 'P' && key[2] == '_') {
        type = 4; prefix_len = 3;
    } else if (key.size() >= 4 && key[0] == 'A' && key[1] == '_') {
        type = 1; prefix_len = 2;
    } else if (key.size() >= 4 && key[0] == 'B' && key[1] == '_') {
        type = 2; prefix_len = 2;
    } else {
        return 0;
    }

    // 剩余部分应为 "i_j" (两个非负整数)
    std::string rest = key.substr(prefix_len);
    size_t us = rest.find('_');
    if (us == std::string::npos) return 0;
    std::string is = rest.substr(0, us);
    std::string js = rest.substr(us + 1);
    if (is.empty() || js.empty()) return 0;
    for (char ch : is) if (ch < '0' || ch > '9') return 0;
    for (char ch : js) if (ch < '0' || ch > '9') return 0;
    i = std::atoi(is.c_str());
    j = std::atoi(js.c_str());
    if (i < 0 || i > 5 || j < 0 || j > 5) return 0;  // 6x6 数组边界
    return type;
}

// ============================================================================
// readFits - 读取 FITS 文件 (纯 C++17, 不依赖 cfitsio)
// ============================================================================
bool readFits(const std::string& path, FitsImage& img, std::string& error_msg) {
    error_msg.clear();

    // 重置输出结构 (不能 memset, 因含 std::vector)
    img.pixels.clear();
    img.width = 0;
    img.height = 0;
    img.channels = 1;
    img.wcs = WcsParams{};
    img.bzero = 0.0;
    img.bscale = 1.0;

    // -------- 打开文件 --------
    FILE* fp = openFileUtf8(path, "rb");
    if (!fp) {
        error_msg = "无法打开文件: " + path;
        fprintf(stderr, "[fits_reader] %s\n", error_msg.c_str());
        return false;
    }

    // 校验 SIMPLE 标记
    char magic[6];
    if (std::fread(magic, 1, 6, fp) != 6 || std::memcmp(magic, "SIMPLE", 6) != 0) {
        error_msg = "不是合法的 FITS 文件 (缺少 SIMPLE 标记)";
        fprintf(stderr, "[fits_reader] %s: %s\n", error_msg.c_str(), path.c_str());
        std::fclose(fp);
        return false;
    }
    std::rewind(fp);

    // -------- 解析主头 --------
    int  bitpix = 0;
    int  naxis  = 0;
    int  naxis1 = 0, naxis2 = 0, naxis3 = 1;
    double bzero = 0.0, bscale = 1.0;
    // B5 修复: 测光校准元数据
    double photscal = 0.0;
    int    photappl = 0;
    bool   has_photscal = false, has_photappl = false;

    // WCS 字段
    double cd[4]    = {0, 0, 0, 0};
    double crval[2] = {0, 0};
    double crpix[2] = {0, 0};
    char   ctype1[16] = {0};
    char   ctype2[16] = {0};
    bool has_cd1_1 = false, has_cd1_2 = false, has_cd2_1 = false, has_cd2_2 = false;
    bool has_crval1 = false, has_crval2 = false;
    bool has_crpix1 = false, has_crpix2 = false;
    bool has_ctype1 = false, has_ctype2 = false;
    // CDELT/CROTA2 备选 (无 CD 矩阵时使用)
    double cdelt1 = 0, cdelt2 = 0, crota2 = 0;
    bool has_cdelt1 = false, has_cdelt2 = false, has_crota2 = false;

    // SIP 系数
    int a_order = 0, b_order = 0, ap_order = 0, bp_order = 0;
    bool has_a_order = false, has_b_order = false;
    bool has_ap_order = false, has_bp_order = false;
    double a[36]  = {0};
    double b[36]  = {0};
    double ap[36] = {0};
    double bp[36] = {0};

    char block[FITS_BLOCK_SIZE];
    bool end_found = false;
    size_t header_bytes = 0;

    while (!end_found) {
        size_t nread = std::fread(block, 1, FITS_BLOCK_SIZE, fp);
        if (nread < FITS_BLOCK_SIZE) {
            error_msg = "读取 FITS 头不完整";
            fprintf(stderr, "[fits_reader] %s\n", error_msg.c_str());
            std::fclose(fp);
            return false;
        }
        header_bytes += FITS_BLOCK_SIZE;

        for (size_t off = 0; off < FITS_BLOCK_SIZE; off += FITS_CARD_SIZE) {
            char card[80];
            std::memcpy(card, block + off, FITS_CARD_SIZE);

            // 检测 END (前 8 字节 trim 后为 "END")
            std::string key8 = trimStr(std::string(card, 8));
            if (key8 == "END") {
                end_found = true;
                break;
            }
            if (key8.empty() || key8 == "COMMENT" || key8 == "HISTORY") continue;

            FitsCard c = parseCard(card);
            if (c.key.empty() || !c.has_value) continue;

            const std::string& k = c.key;
            const std::string& v = c.value;

            if      (k == "BITPIX")  { bitpix  = parseInt(v); }
            else if (k == "NAXIS")   { naxis   = parseInt(v); }
            else if (k == "NAXIS1")  { naxis1  = parseInt(v); }
            else if (k == "NAXIS2")  { naxis2  = parseInt(v); }
            else if (k == "NAXIS3")  { naxis3  = parseInt(v); }
            else if (k == "BZERO")   { bzero   = parseDouble(v); }
            else if (k == "BSCALE")  { bscale  = parseDouble(v); }
            else if (k == "CD1_1")   { cd[0]   = parseDouble(v); has_cd1_1 = true; }
            else if (k == "CD1_2")   { cd[1]   = parseDouble(v); has_cd1_2 = true; }
            else if (k == "CD2_1")   { cd[2]   = parseDouble(v); has_cd2_1 = true; }
            else if (k == "CD2_2")   { cd[3]   = parseDouble(v); has_cd2_2 = true; }
            else if (k == "CRVAL1")  { crval[0] = parseDouble(v); has_crval1 = true; }
            else if (k == "CRVAL2")  { crval[1] = parseDouble(v); has_crval2 = true; }
            else if (k == "CRPIX1")  { crpix[0] = parseDouble(v); has_crpix1 = true; }
            else if (k == "CRPIX2")  { crpix[1] = parseDouble(v); has_crpix2 = true; }
            else if (k == "CTYPE1")  { std::strncpy(ctype1, v.c_str(), 15); has_ctype1 = true; }
            else if (k == "CTYPE2")  { std::strncpy(ctype2, v.c_str(), 15); has_ctype2 = true; }
            else if (k == "CDELT1")  { cdelt1 = parseDouble(v); has_cdelt1 = true; }
            else if (k == "CDELT2")  { cdelt2 = parseDouble(v); has_cdelt2 = true; }
            else if (k == "CROTA2")  { crota2 = parseDouble(v); has_crota2 = true; }
            else if (k == "A_ORDER")  { a_order  = parseInt(v); has_a_order  = true; }
            else if (k == "B_ORDER")  { b_order  = parseInt(v); has_b_order  = true; }
            else if (k == "AP_ORDER") { ap_order = parseInt(v); has_ap_order = true; }
            else if (k == "BP_ORDER") { bp_order = parseInt(v); has_bp_order = true; }
            // B5 修复: 读取测光校准元数据 (PHOTOMETRIC 阶段写入)
            else if (k == "PHOTSCAL") { photscal = parseDouble(v); has_photscal = true; }
            else if (k == "PHOTAPPL") { photappl = parseInt(v);    has_photappl = true; }
            else {
                // SIP 系数 A_i_j / B_i_j / AP_i_j / BP_i_j
                int si = 0, sj = 0;
                int sip_type = classifySipKey(k, si, sj);
                if      (sip_type == 1) a[si * 6 + sj]  = parseDouble(v);
                else if (sip_type == 2) b[si * 6 + sj]  = parseDouble(v);
                else if (sip_type == 3) ap[si * 6 + sj] = parseDouble(v);
                else if (sip_type == 4) bp[si * 6 + sj] = parseDouble(v);
            }
        }
    }

    // -------- 校验头 --------
    if (naxis < 2) {
        error_msg = "NAXIS < 2, 不支持 (NAXIS=" + std::to_string(naxis) + ")";
        fprintf(stderr, "[fits_reader] %s\n", error_msg.c_str());
        std::fclose(fp);
        return false;
    }
    if (naxis1 <= 0 || naxis2 <= 0) {
        error_msg = "NAXIS1/NAXIS2 非法 (" + std::to_string(naxis1) + "x" +
                    std::to_string(naxis2) + ")";
        fprintf(stderr, "[fits_reader] %s\n", error_msg.c_str());
        std::fclose(fp);
        return false;
    }

    bool supported_bitpix = (bitpix == 8 || bitpix == 16 || bitpix == 32 ||
                             bitpix == -32 || bitpix == -64);
    if (!supported_bitpix) {
        error_msg = "不支持的 BITPIX: " + std::to_string(bitpix);
        fprintf(stderr, "[fits_reader] %s\n", error_msg.c_str());
        std::fclose(fp);
        return false;
    }

    // 通道数处理: 仅支持 NAXIS3=1 (单通道) 或 NAXIS3=3 (RGB)
    if (naxis < 3) naxis3 = 1;
    if (naxis3 < 1) naxis3 = 1;
    int channels = 1;
    if (naxis3 == 3) {
        channels = 3;
    } else if (naxis3 != 1) {
        fprintf(stderr, "[fits_reader] 警告: NAXIS3=%d 非 1/3, 按 1 通道处理\n", naxis3);
        naxis3 = 1;
        channels = 1;
    }

    int width  = naxis1;
    int height = naxis2;

    fprintf(stderr, "[fits_reader] 头: BITPIX=%d NAXIS=%d %dx%dx%d BZERO=%.6g BSCALE=%.6g\n",
            bitpix, naxis, width, height, naxis3, bzero, bscale);

    // -------- WCS 检查 --------
    // 需要 CD1_1, CD2_2, CRVAL1/2, CRPIX1/2 (CD1_2/CD2_1 缺失时默认 0)
    bool has_wcs = (has_cd1_1 && has_cd2_2 &&
                    has_crval1 && has_crval2 &&
                    has_crpix1 && has_crpix2);

    if (!has_cd1_2) cd[1] = 0.0;
    if (!has_cd2_1) cd[2] = 0.0;

    // 无 CD 矩阵时, 尝试 CDELT + CROTA2 构造
    if (!has_wcs && has_cdelt1 && has_cdelt2 &&
        has_crval1 && has_crval2 && has_crpix1 && has_crpix2) {
        const double DEG2RAD = 0.017453292519943295769;
        double cosr = std::cos(crota2 * DEG2RAD);
        double sinr = std::sin(crota2 * DEG2RAD);
        cd[0] = cdelt1 * cosr;
        cd[1] = -cdelt2 * sinr;
        cd[2] = cdelt1 * sinr;
        cd[3] = cdelt2 * cosr;
        has_wcs = true;
        fprintf(stderr, "[fits_reader] 使用 CDELT+CROTA2 构造 CD 矩阵 (CROTA2=%s)\n",
                has_crota2 ? "yes" : "default 0");
    }

    if (!has_wcs) {
        fprintf(stderr, "[fits_reader] 警告: 缺少 WCS 关键字, has_wcs=false\n");
    }

    // SIP 阶数一致性检查 (A_ORDER 应等于 B_ORDER, AP_ORDER 应等于 BP_ORDER)
    if (has_a_order && has_b_order && a_order != b_order) {
        fprintf(stderr, "[fits_reader] 警告: A_ORDER(%d) != B_ORDER(%d)\n", a_order, b_order);
    }
    if (has_ap_order && has_bp_order && ap_order != bp_order) {
        fprintf(stderr, "[fits_reader] 警告: AP_ORDER(%d) != BP_ORDER(%d)\n", ap_order, bp_order);
    }

    // 填充 WCS 参数
    img.wcs.has_wcs = has_wcs;
    if (has_wcs) {
        img.wcs.cd[0] = cd[0]; img.wcs.cd[1] = cd[1];
        img.wcs.cd[2] = cd[2]; img.wcs.cd[3] = cd[3];
        img.wcs.crval[0] = crval[0]; img.wcs.crval[1] = crval[1];
        img.wcs.crpix[0] = crpix[0]; img.wcs.crpix[1] = crpix[1];
        if (has_ctype1) std::snprintf(img.wcs.ctype1, sizeof(img.wcs.ctype1), "%s", ctype1);
        if (has_ctype2) std::snprintf(img.wcs.ctype2, sizeof(img.wcs.ctype2), "%s", ctype2);
        img.wcs.sip.order    = has_a_order  ? a_order  : 0;
        img.wcs.sip.ap_order = has_ap_order ? ap_order : 0;
        std::memcpy(img.wcs.sip.a,  a,  sizeof(a));
        std::memcpy(img.wcs.sip.b,  b,  sizeof(b));
        std::memcpy(img.wcs.sip.ap, ap, sizeof(ap));
        std::memcpy(img.wcs.sip.bp, bp, sizeof(bp));

        fprintf(stderr, "[fits_reader] WCS: CRVAL=%.6f,%.6f CRPIX=%.3f,%.3f "
                        "CD=[%.3e,%.3e,%.3e,%.3e] SIP A_ORDER=%d B_ORDER=%d AP_ORDER=%d BP_ORDER=%d\n",
                crval[0], crval[1], crpix[0], crpix[1],
                cd[0], cd[1], cd[2], cd[3], a_order, b_order, ap_order, bp_order);
    }

    // -------- 读取像素数据 --------
    // 数据紧跟在最后一个 2880 字节头块之后
    long data_offset = (long)header_bytes;
    if (std::fseek(fp, data_offset, SEEK_SET) != 0) {
        error_msg = "fseek 到数据段失败";
        fprintf(stderr, "[fits_reader] %s\n", error_msg.c_str());
        std::fclose(fp);
        return false;
    }

    int bytes_per_pixel = std::abs(bitpix) / 8;
    size_t n_pixels = (size_t)width * height * naxis3;
    size_t data_size = n_pixels * bytes_per_pixel;

    std::vector<uint8_t> raw(data_size);
    size_t got = std::fread(raw.data(), 1, data_size, fp);
    std::fclose(fp);
    if (got < data_size) {
        fprintf(stderr, "[fits_reader] 警告: 数据读取不完整 (%zu/%zu 字节)\n", got, data_size);
        n_pixels = got / bytes_per_pixel;  // 按实际读取量处理
    }

    // -------- 转换为 float32 (FITS 大端序 -> 主机浮点) --------
    img.pixels.resize(n_pixels);
    const uint8_t* p = raw.data();

    switch (bitpix) {
        case 8:
            // 无符号字节, 无需字节序转换
            for (size_t i = 0; i < n_pixels; i++)
                img.pixels[i] = (float)raw[i];
            break;

        case 16: {
            // 有符号短整型, 大端
            for (size_t i = 0; i < n_pixels; i++) {
                int16_t v = (int16_t)((uint16_t)p[0] << 8 | (uint16_t)p[1]);
                img.pixels[i] = (float)v;
                p += 2;
            }
            break;
        }

        case 32: {
            // 有符号整型, 大端
            for (size_t i = 0; i < n_pixels; i++) {
                int32_t v = (int32_t)((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
                                      (uint32_t)p[2] << 8  | (uint32_t)p[3]);
                img.pixels[i] = (float)v;
                p += 4;
            }
            break;
        }

        case -32: {
            // 单精度浮点, 大端
            for (size_t i = 0; i < n_pixels; i++) {
                uint32_t u = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
                             (uint32_t)p[2] << 8  | (uint32_t)p[3];
                float f;
                std::memcpy(&f, &u, 4);
                img.pixels[i] = f;
                p += 4;
            }
            break;
        }

        case -64: {
            // 双精度浮点, 大端
            for (size_t i = 0; i < n_pixels; i++) {
                uint64_t u = (uint64_t)p[0] << 56 | (uint64_t)p[1] << 48 |
                             (uint64_t)p[2] << 40 | (uint64_t)p[3] << 32 |
                             (uint64_t)p[4] << 24 | (uint64_t)p[5] << 16 |
                             (uint64_t)p[6] << 8  | (uint64_t)p[7];
                double d;
                std::memcpy(&d, &u, 8);
                img.pixels[i] = (float)d;
                p += 8;
            }
            break;
        }
    }

    // -------- 应用 BZERO/BSCALE: 物理值 = raw * BSCALE + BZERO --------
    if (bzero != 0.0 || bscale != 1.0) {
        fprintf(stderr, "[fits_reader] 应用 BZERO=%.6g BSCALE=%.6g\n", bzero, bscale);
        for (size_t i = 0; i < n_pixels; i++) {
            img.pixels[i] = (float)(bscale * (double)img.pixels[i] + bzero);
        }
    }

    // -------- RGB 平面排列 -> HWC 交错排列 --------
    // FITS NAXIS3=3 时数据为 3 个独立平面 (R, G, B), 转为 HWC 交错以方便后续处理
    if (channels == 3 && naxis3 == 3) {
        size_t plane_size = (size_t)width * height;
        if (n_pixels >= plane_size * 3) {
            std::vector<float> interleaved(plane_size * 3);
            for (size_t i = 0; i < plane_size; i++) {
                interleaved[i * 3 + 0] = img.pixels[i];                  // R 平面
                interleaved[i * 3 + 1] = img.pixels[plane_size + i];     // G 平面
                interleaved[i * 3 + 2] = img.pixels[plane_size * 2 + i]; // B 平面
            }
            img.pixels = std::move(interleaved);
        }
    }

    img.width    = width;
    img.height   = height;
    img.channels = channels;
    img.bzero    = bzero;
    img.bscale   = bscale;
    // B5 修复: 填充测光校准元数据
    img.photscal = has_photscal ? photscal : 0.0;
    img.photappl = has_photappl ? photappl : 0;

    fprintf(stderr, "[fits_reader] 读取成功: %s (%dx%d ch=%d BITPIX=%d WCS=%s pixels=%zu "
                    "photscal=%s photappl=%d)\n",
            path.c_str(), width, height, channels, bitpix,
            has_wcs ? "yes" : "no", img.pixels.size(),
            has_photscal ? std::to_string(photscal).c_str() : "none",
            img.photappl);

    return true;
}

} // namespace drizzle
