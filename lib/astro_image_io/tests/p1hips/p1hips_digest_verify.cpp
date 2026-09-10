// P1-HIPS-DIGEST-001 验收 harness — tree_digest 多 HDU 归一化专项验证
//
// 不入 CTest 常规矩阵 (EXCLUDE_FROM_ALL; 含 sleep 2 强制跨秒与字节翻转
// 双向验证, 秒级慢测不适合 CI 常驻), 手动验收:
//   cmake --build <build_tree> --target p1hips_digest_verify
//   <build_tree>/tests/unit/p1hips/p1hips_digest_verify
// 验证面 (被测内核: p1hips_tests_units.cpp normalize_fits 全文件卡位扫描):
//   V1 单 HDU fixture: CHECKSUM/DATASUM value+注释区清零 + 数据区逐字节
//      不动 + 非白名单卡 (OBJECT 注释时间戳) 不动
//   V2 多 HDU fixture (第二头区带 CHECKSUM/DATASUM): 全 HDU 逐头区归一化
//      (64c1e988 缺陷场景: 只扫第一个 END 漏第二头区) + 数据区敏感
//   V3 非白名单字节保持: 无 CHECKSUM/DATASUM 卡的文件归一化后逐字节不变
//      (垃圾文件/数据区零改动 —— 科学 payload 逐字节敏感的静态面)
//   V4 端到端生产产品 (write_full_f64_product ALL_V19, 含多 HDU Moc.fits):
//      a) 基线 digest; sleep 2 强制跨秒重写 → 位级一致 (跨秒稳定)
//      b) 第二头区 CHECKSUM 卡注释区翻转 1 字节 → digest 不变 (白名单
//         稳定; 64c1e988 旧逻辑下必变 — 回归锚)
//      c) Moc.fits 数据区字节翻转 → digest 变化 (多 HDU 科学 payload 敏感)
//      d) tile .fits 数据区字节翻转 → digest 变化 (单 HDU 科学 payload 敏感)
#include "p1hips_test_main.hpp"
#include "p1hips_fixtures.hpp"
#include "p1hips_oracle.hpp"

// 单翻译单元收编实现: 访问匿名 namespace 归一化内核
#include "p1hips_tests_units.cpp"

#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace p1hips;  // 匿名 namespace 归一化内核非限定可见
using namespace p1hips::oracle;

namespace {

int g_fail = 0;

#define VERIFY(cond, name)                                                     \
    do {                                                                       \
        if (cond) {                                                            \
            std::printf("  PASS %s\n", name);                                  \
        } else {                                                               \
            std::printf("  FAIL %s (%s:%d)\n", name, __FILE__, __LINE__);      \
            ++g_fail;                                                          \
        }                                                                      \
    } while (0)

// ---- 手工 FITS fixture 构造 (80 卡 + 2880 块对齐) --------------------------

std::string card_str(const char* name, const char* value_comment) {
    std::string c(80, ' ');
    const std::size_t nl = std::strlen(name);
    for (std::size_t i = 0; i < nl && i < 8; ++i) c[i] = name[i];
    // value 区: "= " + 右对齐 20 列
    std::string vc = value_comment;
    if (vc.size() < 20) vc = std::string(20 - vc.size(), ' ') + vc;
    c[8] = '=';
    c[9] = ' ';
    for (std::size_t i = 0; i < 20 && i < vc.size(); ++i) c[10 + i] = vc[i];
    return c;
}

std::string card_end() { return std::string(80, ' ').replace(0, 3, "END"); }

// 第一个 END 卡 offset (harness 自扫描, 定位第二头区用); 无 END 返回 -1
long find_first_end(const std::vector<unsigned char>& b) {
    for (std::size_t off = 0; off + 80 <= b.size(); off += 80)
        if (card_is(&b[off], "END")) return (long)off;
    return -1;
}

// 文件内定位最后一张 CHECKSUM/DATASUM 卡; 无则返回 -1
long find_last_card(const std::vector<unsigned char>& b, const char* name) {
    for (long off = (long)((b.size() / 80) * 80) - 80; off >= 0; off -= 80) {
        if (off + 80 > (long)b.size()) continue;
        if (card_is(&b[(std::size_t)off], name)) return off;
    }
    return -1;
}

// 全文件 CHECKSUM/DATASUM 卡 10..79 列是否全零; n_found 回填命中卡数
bool cards_zeroed(const std::vector<unsigned char>& b, int* n_found) {
    int found = 0;
    for (std::size_t off = 0; off + 80 <= b.size(); off += 80) {
        if (card_is(&b[off], "CHECKSUM") || card_is(&b[off], "DATASUM")) {
            ++found;
            for (std::size_t i = 10; i < 80; ++i)
                if (b[off + i] != 0) return false;
        }
    }
    if (n_found) *n_found = found;
    return true;
}

// 单 HDU: SIMPLE/BITPIX=16/NAXIS=1/NAXIS1=4/CHECKSUM/DATASUM/OBJECT/END
// + 数据 4×i16 (payload 敏感性验证翻转点)
std::vector<unsigned char> make_single_hdu(short v0) {
    std::string h;
    h += card_str("SIMPLE", "T");
    h += card_str("BITPIX", "16");
    h += card_str("NAXIS", "1");
    h += card_str("NAXIS1", "4");
    h += card_str("CHECKSUM", "'abcDefGhiDjkLmn' / HDU checksum updated 2026-01-01T00:00:00");
    h += card_str("DATASUM", "'1234567' / data unit checksum updated 2026-01-01T00:00:00");
    h += card_str("OBJECT", "'t0' / utc comment 2026-01-01T00:00:00");
    h += card_end();
    // 头区 pad 到 2880 (标准形态), 数据 8 字节 + 零 pad 到 2880
    h.resize(2880, ' ');
    std::vector<unsigned char> b(h.begin(), h.end());
    const short vals[4] = {v0, 2, 3, 4};
    unsigned char payload[8];
    std::memcpy(payload, vals, sizeof(payload));
    b.insert(b.end(), payload, payload + sizeof(payload));
    b.resize(5760, 0);
    return b;
}

// 多 HDU: 主 HDU (i16×4) + IMAGE 扩展 (i32×2), 两个头区均带 CHECKSUM/DATASUM
// (cfitsio 实测形态: 主头 pad 2880; 扩展头 END 后直接落数据不 pad)
std::vector<unsigned char> make_multi_hdu(short v0, int ev0) {
    std::string h;
    h += card_str("SIMPLE", "T");
    h += card_str("BITPIX", "16");
    h += card_str("NAXIS", "1");
    h += card_str("NAXIS1", "4");
    h += card_str("CHECKSUM", "'PriCksumValUe12' / HDU checksum updated 2026-01-01T00:00:00");
    h += card_str("DATASUM", "'111' / data unit checksum updated 2026-01-01T00:00:00");
    h += card_end();
    h.resize(2880, ' ');  // 主头 pad
    h += card_str("XTENSION", "'IMAGE   '");
    h += card_str("BITPIX", "32");
    h += card_str("NAXIS", "1");
    h += card_str("NAXIS1", "2");
    h += card_str("PCOUNT", "0");
    h += card_str("GCOUNT", "1");
    h += card_str("CHECKSUM", "'SecCksumValUe34' / HDU checksum updated 2026-01-01T00:00:00");
    h += card_str("DATASUM", "'222' / data unit checksum updated 2026-01-01T00:00:00");
    h += card_end();
    std::vector<unsigned char> b(h.begin(), h.end());
    const int vals[2] = {ev0, 7};  // 扩展数据 8 字节, 直接跟 END 卡后 (不 pad)
    unsigned char payload[8];
    std::memcpy(payload, vals, sizeof(payload));
    b.insert(b.end(), payload, payload + sizeof(payload));
    b.resize(b.size() + 2880 - 8, 0);  // 文件尾零 pad (cfitsio 形态)
    return b;
}

bool write_bytes(const std::string& path, const std::vector<unsigned char>& b) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(b.data()), (std::streamsize)b.size());
    return f.good();
}

}  // namespace

int main() {
    p1hips::init_fault_registry_from_env();
    std::printf("[digest_verify] P1-HIPS-DIGEST-001 归一化专项验证\n");

    // --- V1: 单 HDU fixture ---
    {
        std::printf("V1 单 HDU fixture\n");
        std::vector<unsigned char> b = make_single_hdu(11);
        std::vector<unsigned char> orig = b;
        const long ck = find_last_card(b, "CHECKSUM");
        const long ds = find_last_card(b, "DATASUM");
        VERIFY(ck >= 0 && ds >= 0, "v1_cards_present");
        VERIFY(p1hips::normalize_fits(b), "v1_normalize_ok");
        int n_found = 0;
        VERIFY(cards_zeroed(b, &n_found) && n_found == 2,
               "v1_checksum_datasum_zeroed");
        // 数据区 (payload) 逐字节不动
        VERIFY(std::memcmp(&b[2880], &orig[2880], b.size() - 2880) == 0,
               "v1_payload_intact");
        // 非白名单卡 (OBJECT 注释含时间戳) 不动
        const long obj = find_last_card(b, "OBJECT");
        VERIFY(obj > 0 && std::memcmp(&b[obj], &orig[obj], 80) == 0,
               "v1_nonwhitelist_untouched");
    }

    // --- V2: 多 HDU fixture (第二头区 CHECKSUM/DATASUM 覆盖, 缺陷回归锚) ---
    {
        std::printf("V2 多 HDU fixture\n");
        std::vector<unsigned char> b = make_multi_hdu(11, 77);
        std::vector<unsigned char> orig = b;
        const long first_end = find_first_end(b);
        const long ck2 = find_last_card(b, "CHECKSUM");
        const long ds2 = find_last_card(b, "DATASUM");
        VERIFY(first_end > 0 && ck2 > first_end && ds2 > first_end,
               "v2_second_hdr_cards_beyond_first_end");
        const std::size_t payload_off = (std::size_t)ds2 + 160;  // DATASUM+END 之后
        VERIFY(p1hips::normalize_fits(b), "v2_normalize_ok");
        int n_found = 0;
        VERIFY(cards_zeroed(b, &n_found) && n_found == 4,
               "v2_all_hdu_cards_zeroed");
        // 数据区 (第二头区 END 后 payload) 逐字节不动
        VERIFY(std::memcmp(&b[payload_off], &orig[payload_off],
                           b.size() - payload_off) == 0,
               "v2_payload_intact");
    }

    // --- V3: 非白名单字节保持 (无卡文件逐字节不变) ---
    {
        std::printf("V3 非白名单字节保持\n");
        // 数据区含可打印噪声 (无 CHECKSUM/DATASUM 字样) → 归一化零改动
        std::vector<unsigned char> b(5760, 0);
        for (std::size_t i = 2880; i < b.size(); ++i)
            b[i] = (unsigned char)(0x20 + (i % 0x40));
        std::vector<unsigned char> orig = b;
        VERIFY(p1hips::normalize_fits(b), "v3_normalize_ok");
        VERIFY(b == orig, "v3_no_card_untouched");
        // 过短文件 (<80) → 退回 false
        std::vector<unsigned char> tiny(40, 'S');
        VERIFY(!p1hips::normalize_fits(tiny), "v3_tiny_rejected");
    }

    // --- V4: 端到端生产产品 (多 HDU Moc.fits) ---
    {
        std::printf("V4 端到端生产产品 (sleep 2 跨秒 + 翻转双向)\n");
        const std::string dir0 = make_tmp_dir("dv0");
        VERIFY(!dir0.empty(), "v4_tmpdir0");
        int rc = write_full_f64_product(dir0, AIO_HIPS_PRODUCT_ALL_V19,
                                        "2026-09-07T00:00:00", false);
        VERIFY(rc == 0, "v4_write0");
        std::uint64_t d0 = 0;
        VERIFY(p1hips::tree_digest(dir0, d0, "properties", nullptr),
               "v4_digest0");

        sleep(2);  // 强制跨秒 (CHECKSUM/DATASUM 注释 UTC 秒级时间戳)

        const std::string dir1 = make_tmp_dir("dv1");
        VERIFY(!dir1.empty(), "v4_tmpdir1");
        rc = write_full_f64_product(dir1, AIO_HIPS_PRODUCT_ALL_V19,
                                    "2026-09-07T00:00:00", false);
        VERIFY(rc == 0, "v4_write1");
        std::uint64_t d1 = 0;
        VERIFY(p1hips::tree_digest(dir1, d1, "properties", nullptr),
               "v4_digest1");
        VERIFY(d1 == d0, "v4_cross_second_stable");

        // b) 第二头区 CHECKSUM 注释翻转 → digest 不变 (白名单稳定; 旧逻辑
        //    只扫第一个 END 时此卡漏归一化 → 必变)
        {
            const std::string moc = dir1 + "/signal/Moc.fits";
            std::vector<unsigned char> b;
            VERIFY(read_file_bytes(moc, b), "v4_moc_read");
            const long first_end = find_first_end(b);
            const long ck2 = find_last_card(b, "CHECKSUM");
            VERIFY(first_end > 0 && ck2 > first_end,
                   "v4_moc_second_hdr_checksum");
            std::vector<unsigned char> orig = b;
            b[(std::size_t)ck2 + 30] ^= 0x01;  // 注释区翻转 1 字节
            VERIFY(write_bytes(moc, b), "v4_moc_rewrite");
            std::uint64_t d2 = 0;
            VERIFY(p1hips::tree_digest(dir1, d2, "properties", nullptr),
                   "v4_digest2");
            VERIFY(d2 == d0, "v4_checksum_comment_ignored");
            // 恢复原文件
            VERIFY(write_bytes(moc, orig), "v4_moc_restore");
        }

        // c) Moc.fits 数据区字节翻转 → digest 必变 (多 HDU 科学敏感)
        {
            const std::string moc = dir1 + "/signal/Moc.fits";
            std::vector<unsigned char> b;
            VERIFY(read_file_bytes(moc, b), "v4_moc_read_c");
            const long ds2 = find_last_card(b, "DATASUM");
            const std::size_t data_off = (std::size_t)ds2 + 160;  // END 后数据区
            std::vector<unsigned char> orig = b;
            b[data_off] ^= 0x01;
            VERIFY(write_bytes(moc, b), "v4_moc_rewrite_c");
            std::uint64_t d3 = 0;
            VERIFY(p1hips::tree_digest(dir1, d3, "properties", nullptr),
                   "v4_digest3");
            VERIFY(d3 != d0, "v4_moc_payload_sensitive");
            VERIFY(write_bytes(moc, orig), "v4_moc_restore_c");
        }

        // d) tile .fits (单 HDU) 数据区字节翻转 → digest 必变
        {
            const std::string tile = dir1 + "/signal/Norder0/Dir0/Npix0.fits";
            std::vector<unsigned char> b;
            VERIFY(read_file_bytes(tile, b), "v4_tile_read");
            const long first_end = find_first_end(b);
            VERIFY(first_end > 0, "v4_tile_has_end");
            const std::size_t data_off =
                (std::size_t)(((first_end + 80 + 2879) / 2880) * 2880);
            std::vector<unsigned char> orig = b;
            b[data_off] ^= 0x01;
            VERIFY(write_bytes(tile, b), "v4_tile_rewrite");
            std::uint64_t d4 = 0;
            VERIFY(p1hips::tree_digest(dir1, d4, "properties", nullptr),
                   "v4_digest4");
            VERIFY(d4 != d0, "v4_tile_payload_sensitive");
            VERIFY(write_bytes(tile, orig), "v4_tile_restore");
        }
    }

    if (g_fail == 0) {
        std::printf("[digest_verify] ALL PASS\n");
        return 0;
    }
    std::printf("[digest_verify] FAILED (%d check(s))\n", g_fail);
    return 1;
}
