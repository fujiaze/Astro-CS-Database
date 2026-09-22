// ============================================================================
// test_error_utf8.cpp — 对外可见错误串的 UTF-8 归一回归锁 (ALG-WCS-001 §4b)
// ----------------------------------------------------------------------------
// 规范依据:
//   - ENGINEERING_SPEC「文件编码 UTF-8」;
//   - docs/contracts/LOG_AND_ERROR_CONTRACT.md §8「超限按 UTF-8 边界截断」;
//   - ALG-WCS-001 §4b(本仓算法层细化): IpvWcsResult.error_msg 恒为合法 UTF-8。
// 断言面:
//   1) utf8_safe_copy 在定长缓冲上的行为 (码点边界截断 / 非法字节替换 / NUL);
//   2) 公共 ABI 入口在失败路径写出的 error_msg 必须非空且是合法 UTF-8;
//   3) 负向对照 (必须判红): 修复前的"按字节截断 + 原样透传"实现必须被判据判红
//      —— 含 GBK 字节与跨边界多字节序列两种注入。
// ============================================================================
#include "ipv_api.h"
#include "ipv_types.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0;
static int g_check = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        ++g_check;                                                          \
        if (!(cond)) {                                                      \
            std::printf("FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__); \
            ++g_fail;                                                       \
        } else {                                                            \
            std::printf("ok  : %s\n", (msg));                              \
        }                                                                   \
    } while (0)

// ---------------------------------------------------------------------------
// 独立严格 UTF-8 校验器 (RFC 3629: 拒绝过长编码/代理区/越界码点/截断序列)。
// 独立实现, 不调用被测代码 —— 判据本身必须是独立 Oracle。
// ---------------------------------------------------------------------------
static bool is_valid_utf8(const char* s, std::size_t n) {
    std::size_t i = 0;
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = 0;
        unsigned char lo = 0x80, hi = 0xBF;
        if (c < 0x80)                      { len = 1; }
        else if (c >= 0xC2 && c <= 0xDF)   { len = 2; }
        else if (c == 0xE0)                { len = 3; lo = 0xA0; }
        else if (c >= 0xE1 && c <= 0xEC)   { len = 3; }
        else if (c == 0xED)                { len = 3; hi = 0x9F; }
        else if (c >= 0xEE && c <= 0xEF)   { len = 3; }
        else if (c == 0xF0)                { len = 4; lo = 0x90; }
        else if (c >= 0xF1 && c <= 0xF3)   { len = 4; }
        else if (c == 0xF4)                { len = 4; hi = 0x8F; }
        else return false;
        if (i + len > n) return false;
        for (std::size_t k = 1; k < len; ++k) {
            const unsigned char ck = static_cast<unsigned char>(s[i + k]);
            const unsigned char l = (k == 1) ? lo : 0x80;
            const unsigned char h = (k == 1) ? hi : 0xBF;
            if (ck < l || ck > h) return false;
        }
        i += len;
    }
    return true;
}

static bool valid_cstr(const char* s) {
    return is_valid_utf8(s, std::strlen(s));
}

// ---------------------------------------------------------------------------
// 负向对照: 修复前的实现 (按字节截断 + 原样透传)
// ---------------------------------------------------------------------------
static void legacy_set_error_msg(char* dst, std::size_t dst_size, const char* msg) {
    if (dst == nullptr || dst_size == 0) return;
    if (msg == nullptr) { dst[0] = '\0'; return; }
    std::size_t n = std::strlen(msg);
    if (n >= dst_size) n = dst_size - 1;
    std::memcpy(dst, msg, n);
    dst[n] = '\0';
}

// ===========================================================================
// 1. utf8_safe_copy 行为
// ===========================================================================
static void test_helper() {
    char buf[256];

    // 1a 合法 UTF-8 原样通过
    {
        const char* s = "ipv: 求解失败 (selection/triangle)";
        const std::size_t n = ipv::utf8_safe_copy(buf, sizeof(buf), s);
        CHECK(n == std::strlen(s), "1a: 合法 UTF-8 原样写入 (长度一致)");
        CHECK(std::strcmp(buf, s) == 0, "1a: 内容逐字节一致");
        CHECK(valid_cstr(buf), "1a: 结果合法 UTF-8");
    }
    // 1b 边界截断: 第 255 字节处切断一个 3 字节汉字
    {
        std::string s;
        while (s.size() < 250) s += "a";
        s += "中文中文中文";                       // 多字节序列跨越 dst_size-1 边界
        legacy_set_error_msg(buf, 16, s.c_str());  // 15 字节: "aaaaaaaaaaaaaaa"
        CHECK(valid_cstr(buf), "1b(对照): 纯 ASCII 前缀下按字节截断仍合法");
        char small[12];
        const std::size_t n = ipv::utf8_safe_copy(small, sizeof(small), s.c_str());
        CHECK(n <= sizeof(small) - 1, "1b: 写入长度不超过缓冲容量");
        CHECK(valid_cstr(small), "1b: 截断只发生在码点边界 (结果合法 UTF-8)");
        // 具体边界: 11 字节容量, 前 11 字节是 'a' -> 写满 11 个 'a'
        CHECK(std::strlen(small) == 11, "1b: 码点边界截断取到容量上限");
        // 让多字节序列正好跨越边界
        char small2[14];
        const std::size_t n2 = ipv::utf8_safe_copy(small2, sizeof(small2), s.c_str());
        CHECK(n2 <= 13 && valid_cstr(small2),
              "1b: 13 字节容量下不切断多字节序列");
        CHECK(std::strlen(small2) == 13, "1b: 13 字节容量写满 ASCII 前缀");
        // 多字节序列正好跨越容量边界: 10×'a' + 3 字节汉字, 容量 12 (可用 11)
        {
            const std::string s2 = std::string(10, 'a') + "中中中";
            char small3[12];
            const std::size_t n3 = ipv::utf8_safe_copy(small3, sizeof(small3), s2.c_str());
            CHECK(valid_cstr(small3), "1b: 边界处不切断 3 字节序列 (结果合法 UTF-8)");
            CHECK(n3 == 10, "1b: 剩余容量放不下完整码点 => 在码点边界停下 (10)");
            legacy_set_error_msg(small3, sizeof(small3), s2.c_str());
            CHECK(!valid_cstr(small3),
                  "1b(负向对照): 按字节截断切断多字节序列 => 判据判红");
        }
    }
    // 1c 非法字节 (GBK 双字节: 0xD6 0xD0 0xCE 0xC4) -> '?'
    {
        const char gbk[] = {'i', 'p', 'v', ':', ' ',
                            (char)0xD6, (char)0xD0, (char)0xCE, (char)0xC4, '\0'};
        const std::size_t n = ipv::utf8_safe_copy(buf, sizeof(buf), gbk);
        CHECK(valid_cstr(buf), "1c: GBK 字节被归一 -> 结果合法 UTF-8");
        CHECK(n == 9, "1c: 每个非法字节替换为 1 个 '?' (长度不变)");
        CHECK(std::strcmp(buf, "ipv: ????") == 0, "1c: 替换字符为 ASCII '?'");
        legacy_set_error_msg(buf, sizeof(buf), gbk);
        CHECK(!valid_cstr(buf), "1c(负向对照): 原样透传 GBK 字节 => 判据判红");
    }
    // 1d 截断序列 (孤立首字节 / 孤立续字节 / 代理区 / 过长编码)
    {
        const char cases[][4] = {
            {(char)0xE4, '\0', '\0', '\0'},              // 孤立 3 字节首字节
            {(char)0x80, '\0', '\0', '\0'},              // 孤立续字节
            {(char)0xED, (char)0xA0, (char)0x80, '\0'},    // UTF-16 代理区
            {(char)0xC0, (char)0x80, '\0', '\0'},         // 过长编码 (overlong NUL)
        };
        for (int i = 0; i < 4; ++i) {
            ipv::utf8_safe_copy(buf, sizeof(buf), cases[i]);
            char msg[96];
            std::snprintf(msg, sizeof(msg), "1d: 非法序列 %d 被归一为合法 UTF-8", i);
            CHECK(valid_cstr(buf), msg);
            legacy_set_error_msg(buf, sizeof(buf), cases[i]);
            std::snprintf(msg, sizeof(msg), "1d(负向对照): 非法序列 %d 原样透传 => 判红", i);
            CHECK(!valid_cstr(buf), msg);
        }
    }
    // 1e 边界: 容量 1 / 空串 / NULL
    {
        char one[1] = {'X'};
        CHECK(ipv::utf8_safe_copy(one, sizeof(one), "abc") == 0 && one[0] == '\0',
              "1e: 容量 1 -> 只写结尾 NUL");
        CHECK(ipv::utf8_safe_copy(buf, sizeof(buf), "") == 0 && buf[0] == '\0',
              "1e: 空串 -> 空结果");
        CHECK(ipv::utf8_safe_copy(buf, sizeof(buf), nullptr) == 0,
              "1e: NULL 源 -> 空结果");
        CHECK(ipv::utf8_safe_copy(nullptr, 0, "x") == 0, "1e: NULL 目标 -> 0");
    }
    // 1f 定长缓冲恰好写满 (无截断) 且逐字节一致
    {
        const char* s = "中文abc";   // 6+3 = 9 字节
        char exact[10];
        const std::size_t n = ipv::utf8_safe_copy(exact, sizeof(exact), s);
        CHECK(n == 9 && std::strcmp(exact, s) == 0, "1f: 恰好放得下时逐字节一致");
    }
}

// ===========================================================================
// 2. 公共 ABI 失败路径的 error_msg
// ===========================================================================
static void test_abi_error_msg() {
    // 2a 空 detections -> 必然失败, error_msg 必须非空且合法 UTF-8
    {
        void* solver = ipv_solve_create();
        CHECK(solver != nullptr, "2a: solver 创建");
        IpvWcsResult r;
        std::memset(&r, 0, sizeof(r));
        const int ret = ipv_solve_from_detections_v1(solver, nullptr, 0,
                                                     1024, 1024, 83.28, -6.37,
                                                     1917.6, 9.0, nullptr, &r);
        CHECK(ret == 0 && r.success == 0, "2a: 空输入 fail-closed (ret=0)");
        CHECK(r.error_msg[0] != '\0', "2a: 失败必须携带非空 error_msg");
        CHECK(valid_cstr(r.error_msg), "2a: error_msg 是合法 UTF-8");
        std::printf("     error_msg = %s\n", r.error_msg);
        ipv_solve_destroy(solver);
    }
    // 2b 星点过少 -> 选星样本不足, 错误串点名 n_detected/n_saturated/n_unsat
    {
        void* solver = ipv_solve_create();
        std::vector<double> det(6, 0.0);   // 1 颗非饱和星 [x,y,flux,mag,sat,has_sat]
        det[0] = 100.0; det[1] = 100.0; det[2] = 1000.0; det[3] = 5.0;
        det[4] = 0.0;   det[5] = 0.0;
        IpvWcsResult r;
        std::memset(&r, 0, sizeof(r));
        const int ret = ipv_solve_from_detections_v1(solver, det.data(), 1,
                                                     1024, 1024, 83.28, -6.37,
                                                     1917.6, 9.0, nullptr, &r);
        CHECK(ret == 0, "2b: 样本不足 fail-closed");
        CHECK(r.error_msg[0] != '\0' && valid_cstr(r.error_msg),
              "2b: 样本不足时 error_msg 非空且合法 UTF-8");
        std::printf("     error_msg = %s\n", r.error_msg);
        ipv_solve_destroy(solver);
    }
    // 2c ABI 不匹配 -> fail-closed, error_msg 合法 UTF-8
    {
        void* solver = ipv_solve_create();
        IpvParams p;
        ipv_get_default_params(&p);
        p.struct_size = 0;   // 旧调用方 (ctypes 零初始化)
        IpvWcsResult r;
        std::memset(&r, 0, sizeof(r));
        std::vector<double> det(6, 0.0);
        const int ret = ipv_solve_from_detections_v1(solver, det.data(), 1,
                                                     1024, 1024, 83.28, -6.37,
                                                     1917.6, 9.0, &p, &r);
        CHECK(ret == 0, "2c: ABI 不匹配 fail-closed");
        CHECK(valid_cstr(r.error_msg), "2c: ABI 错误串是合法 UTF-8");
        ipv_solve_destroy(solver);
    }
}

int main() {
    std::printf("=== ALG-WCS-001 §4b 错误串 UTF-8 归一回归锁 ===\n");
    test_helper();
    test_abi_error_msg();
    std::printf("---- checks=%d failures=%d ----\n", g_check, g_fail);
    return g_fail == 0 ? 0 : 1;
}
