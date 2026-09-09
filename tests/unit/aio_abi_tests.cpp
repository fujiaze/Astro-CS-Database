// AIO-001 · 唯一 AIO C ABI v1 与内容哈希复核 · units/negative 测试组
//
// 合同锚: include/astrocs/io/aio_abi_v1.h + contracts/data/aio_abi_contract_v1.json。
// 独立 oracle: FIPS 180-4 标准向量 (python hashlib 预生成, 非被测代码生成)
// + astrocs::crypto::Sha256 (lib/common 单一实现, 与 AIO UPM 同源) 交叉对拍。
// 状态码对齐: 与 acs_fio_status (fits_stream_v1.h) 0..13 全域编译期交叉断言。
//
// 组:
//   units    U1 ABI 握手 / U2 NIST 向量+双实现对拍 / U3 复核正向 /
//            U4 确定性 / U5 1-N worker 并发复核位级一致 / U6 空内容与块边界
//   negative N1 握手失配 / N2 hash_buffer 参数域 / N3 声明非法+篡改 /
//            N4 文件复核 (不存在/截断/篡改/大小核对) / N5 文件边界
#include "aio_abi_test_main.hpp"

#include "astrocs/io/aio_abi_v1.h"
#include "crypto/sha256.h"
#include "astrocs/io/fits_stream_v1.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "aio_abi_omp.hpp"

namespace {

using astrocs::crypto::Sha256;
using namespace ::aio_abi_test;

// FIPS 180-4 标准向量 (python hashlib.sha256 权威预生成)
// content: 非空=字面内容; 空=NUL 且以 'x' 填充 len 字节 (块边界族)
struct HashCase {
    const char* name;
    unsigned long long len;
    const char* content;  // NULL → 'x' * len
    const char* expect;
};

const HashCase kNistCases[] = {
    {"empty", 0, nullptr, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    {"abc", 3, "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
    {"fips56", 56, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
    {"fips112",
     112, "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopq"
          "klmnopqrlmnopqrsmnopqrstnopqrstu",
     "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"},
    {"b55", 55, nullptr, "d5e285683cd4efc02d021a5c62014694958901005d6f71e89e0989fac77e4072"},
    {"b56", 56, nullptr, "04c26261370ee7541549d16dee320c723e3fd14671e66a099afe0a377c16888e"},
    {"b63", 63, nullptr, "75220b47218278e656f2013bb8f0c455a25eaf01e86c64924e9d48d89776d6f2"},
    {"b64", 64, nullptr, "7ce100971f64e7001e8fe5a51973ecdfe1ced42befe7ee8d5fd6219506b5393c"},
    {"b65", 65, nullptr, "9537c5fdf120482f7d58d25e9ed583f52c02b4e304ea814db1633ad565aed7e9"},
    {"b127", 127, nullptr, "70156a14adbabf98cff3a71c7084b417abf057a8efd27329ca36b7202c87d81f"},
    {"b128", 128, nullptr, "24da1b81d0b16df6428eee73c69fcb2a93c76bc6df706f0c6670fe6bfe800464"},
    {"b129", 129, nullptr, "0ec9eb33e74510bcdd1f2ea55206e82f21649c5c2becbf2b433eb475b34c01bd"},
};

// 独立 SHA-256 oracle: lib/common 单一实现 (公开算法; 非 AIO ABI 通道)
std::string oracle_hex(const void* data, std::size_t len) {
    Sha256 h;
    h.update(data, len);
    return h.final_hex();
}

bool is_valid_decl(const char* s) { return aio_abi_test_decl_valid(s) != 0; }

}  // namespace

namespace aio_abi_test {

// 声明格式校验 (由被测实现同规则使用; 测试侧独立复刻, 双向一致性断言)
// 64 字符小写 [0-9a-f]。放头外自由函数, selfcheck 与 negative 共用。
int aio_abi_test_decl_valid(const char* s) {
    if (!s) return 0;
    if (std::strlen(s) != AIO_DIGEST_HEX_LEN) return 0;
    for (std::size_t i = 0; i < AIO_DIGEST_HEX_LEN; ++i) {
        const char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

// ───────── units 组 ─────────
int test_units(CheckState& cs) {
    // U1: ABI 唯一握手 (正向; 严格 handshake 模式: 调用方预填 head, 服务端验证后填充)
    {
        aio_abi_info_v1 info;
        std::memset(&info, 0, sizeof(info));
        info.struct_size = sizeof(aio_abi_info_v1);
        info.abi_version = AIO_ABI_VERSION_V1;
        const int st = aio_abi_query_v1(&info);
        AIO_CHECK_MSG(cs, st == AIO_OK, "u1_query_ok", "query rc=%d", st);
        AIO_CHECK(cs, info.struct_size == sizeof(aio_abi_info_v1), "u1_struct_size");
        AIO_CHECK(cs, info.abi_version == AIO_ABI_VERSION_V1, "u1_abi_version");
        AIO_CHECK(cs, info.status_count == AIO_STATUS_COUNT, "u1_status_count");
        AIO_CHECK(cs, info.digest_hex_len == 64, "u1_digest_len");
        AIO_CHECK(cs, info.hash_algorithm_id == AIO_HASH_ALGORITHM_SHA256, "u1_alg_id");
        AIO_CHECK(cs, std::strcmp(info.hash_algorithm_name, "sha256") == 0, "u1_alg_name");
        AIO_CHECK(cs, (info.capability_bits & AIO_CAP_HASH_REVIEW) != 0, "u1_cap_hash");
        // 编译期对齐证明的运行时复核: IO 家族全域数值一致 (fits_stream_v1.h)
        AIO_CHECK(cs, AIO_OK == ACS_FIO_OK && AIO_ERR_PARAM == ACS_FIO_ERR_PARAM &&
                          AIO_ERR_ABI_MISMATCH == ACS_FIO_ERR_ABI_MISMATCH &&
                          AIO_ERR_NOMEM == ACS_FIO_ERR_NOMEM && AIO_ERR_IO == ACS_FIO_ERR_IO &&
                          AIO_ERR_UNSUPPORTED == ACS_FIO_ERR_UNSUPPORTED &&
                          AIO_ERR_CANCELLED == ACS_FIO_ERR_CANCELLED &&
                          AIO_ERR_STATE == ACS_FIO_ERR_STATE &&
                          AIO_ERR_TRUNCATED == ACS_FIO_ERR_TRUNCATED &&
                          AIO_ERR_BAD_HEADER == ACS_FIO_ERR_BAD_HEADER &&
                          AIO_ERR_MISMATCH == ACS_FIO_ERR_MISMATCH &&
                          AIO_ERR_CHECKSUM == ACS_FIO_ERR_CHECKSUM &&
                          AIO_ERR_NANINF == ACS_FIO_ERR_NANINF &&
                          AIO_ERR_DISKFULL == ACS_FIO_ERR_DISKFULL,
                  "u1_status_align_fio");
    }

    // U2: NIST 标准向量 (oracle=hashlib 预生成) + lib/common Sha256 交叉对拍
    {
        for (const auto& tc : kNistCases) {
            // 内容: 字面向量 (abc/fips*) 或 'x'*len (块边界族); 空向量 data=NULL
            std::vector<unsigned char> xbuf(
                tc.content ? 0 : static_cast<std::size_t>(tc.len), 'x');
            std::vector<unsigned char> cbuf;
            if (tc.content) cbuf.assign(tc.content, tc.content + tc.len);
            const unsigned char* data =
                tc.len == 0 ? nullptr : (tc.content ? cbuf.data() : xbuf.data());
            char hex[AIO_DIGEST_HEX_LEN + 1] = {0};
            const int st = aio_content_hash_buffer_v1(data, tc.len, hex);
            AIO_CHECK_MSG(cs, st == AIO_OK, "n1_hash_value_flip",
                          "case %s hash rc=%d", tc.name, st);
            AIO_CHECK_MSG(cs, std::strcmp(hex, tc.expect) == 0, "n1_hash_value_flip",
                          "case %s digest mismatch: %s", tc.name, hex);
            // oracle 对拍 (独立实现)
            const std::string ref = oracle_hex(data, static_cast<std::size_t>(tc.len));
            AIO_CHECK_MSG(cs, ref == tc.expect, nullptr,
                          "oracle case %s mismatch: %s", tc.name, ref.c_str());
            AIO_CHECK_MSG(cs, ref == hex, "n1_hash_value_flip",
                          "case %s ABI vs oracle diverge", tc.name);
        }
        // 百万 'a' (FIPS 长消息向量; 流式多块)
        {
            const std::size_t N = 1000000u;
            std::string big(N, 'a');
            char hex[AIO_DIGEST_HEX_LEN + 1] = {0};
            const int st = aio_content_hash_buffer_v1(big.data(), N, hex);
            AIO_CHECK(cs, st == AIO_OK, "u2_million_rc");
            AIO_CHECK_MSG(cs, std::strcmp(hex,
                "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0,
                "n1_hash_value_flip", "million 'a' digest: %s", hex);
        }
    }

    // U3: 复核正向 (声明=重算值)
    {
        const char* content = "astrocs-aio-content-hash-review";
        char hex[AIO_DIGEST_HEX_LEN + 1] = {0};
        AIO_CHECK(cs, aio_content_hash_buffer_v1(content, std::strlen(content), hex) == AIO_OK,
                  "u3_hash_rc");
        AIO_CHECK(cs, is_valid_decl(hex), "u3_decl_format");
        AIO_CHECK(cs, aio_content_hash_verify_buffer_v1(content, std::strlen(content), hex) ==
                          AIO_OK, "u3_verify_ok");
        AIO_CHECK(cs, is_valid_decl(hex), "n1_hash_value_flip");
    }

    // U4: 确定性 (同输入重复重算位级一致)
    {
        const char* content = "determinism-probe";
        const std::size_t n = std::strlen(content);
        char first[AIO_DIGEST_HEX_LEN + 1] = {0};
        AIO_CHECK(cs, aio_content_hash_buffer_v1(content, n, first) == AIO_OK, "u4_first");
        bool all_same = true;
        for (int i = 0; i < 100; ++i) {
            char hex[AIO_DIGEST_HEX_LEN + 1] = {0};
            if (aio_content_hash_buffer_v1(content, n, hex) != AIO_OK ||
                std::strcmp(hex, first) != 0) {
                all_same = false;
                break;
            }
        }
        AIO_CHECK(cs, all_same, "u4_deterministic");
    }

    // U5: 1-N worker 并发复核位级一致 (reentrant; OpenMP 并行轴)
    {
        // 伪随机内容表 (splitmix64, fixture 模式; 不内嵌生产算法)
        const int kItems = 256;
        std::vector<std::string> contents(static_cast<std::size_t>(kItems));
        std::vector<std::string> expect(static_cast<std::size_t>(kItems));
        std::vector<int> lenpattern(static_cast<std::size_t>(kItems));
        std::uint64_t st64 = 0x243F6A8885A308D3ull;  // 固定 seed
        for (int i = 0; i < kItems; ++i) {
            st64 += 0x9E3779B97F4A7C15ull;
            std::uint64_t z = st64;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            z ^= (z >> 31);
            const std::size_t len = static_cast<std::size_t>(z % 300);  // 0..299B 跨块边界
            lenpattern[static_cast<std::size_t>(i)] = static_cast<int>(len);
            std::string& s = contents[static_cast<std::size_t>(i)];
            s.resize(len);
            std::uint64_t f = z | 1;
            for (std::size_t j = 0; j < len; ++j) {
                f = f * 6364136223846793005ull + 1442695040888963407ull;
                s[j] = static_cast<char>((f >> 33) & 0xFFu);
            }
            expect[static_cast<std::size_t>(i)] = oracle_hex(s.data(), len);
        }
        // 串行基线
        std::vector<int> rc_serial(static_cast<std::size_t>(kItems), -1);
        for (int i = 0; i < kItems; ++i) {
            rc_serial[static_cast<std::size_t>(i)] = aio_content_hash_verify_buffer_v1(
                contents[static_cast<std::size_t>(i)].data(),
                static_cast<std::uint64_t>(lenpattern[static_cast<std::size_t>(i)]),
                expect[static_cast<std::size_t>(i)].c_str());
        }
        // 1 vs N worker
        for (int workers : {1, 4, 16}) {
            std::vector<int> rc(static_cast<std::size_t>(kItems), -1);
            AIO_OMP_PARALLEL_FOR_REVIEW(rc, contents, expect, lenpattern, workers);
            bool ok = true;
            for (int i = 0; i < kItems; ++i) {
                if (rc[static_cast<std::size_t>(i)] != AIO_OK ||
                    rc[static_cast<std::size_t>(i)] != rc_serial[static_cast<std::size_t>(i)]) {
                    ok = false;
                    break;
                }
            }
            AIO_CHECK_MSG(cs, ok, "n2_verify_mismatch_shortcut",
                          "workers=%d verify diverge", workers);
        }
    }

    // U6: 空内容 + 块边界文件复核
    {
        // 空内容 (len=0, data=NULL) 合同合法
        char hex[AIO_DIGEST_HEX_LEN + 1] = {0};
        AIO_CHECK(cs, aio_content_hash_buffer_v1(nullptr, 0, hex) == AIO_OK, "u6_empty_rc");
        AIO_CHECK(cs, std::strcmp(hex, kNistCases[0].expect) == 0, "u6_empty_digest");
        AIO_CHECK(cs, aio_content_hash_verify_buffer_v1(nullptr, 0, kNistCases[0].expect) ==
                          AIO_OK, "u6_empty_verify");
    }

    return cs.failures == 0 ? 0 : 1;
}

// ───────── negative 组 ─────────
int test_negative(CheckState& cs) {
    // N1: 握手失配 (不猜布局)
    {
        AIO_CHECK(cs, aio_abi_query_v1(nullptr) == AIO_ERR_PARAM, "n1_null_out");
        aio_abi_info_v1 info;
        std::memset(&info, 0, sizeof(info));
        info.struct_size = sizeof(info) + 8;  // 篡改 size
        info.abi_version = AIO_ABI_VERSION_V1;
        AIO_CHECK(cs, aio_abi_query_v1(&info) == AIO_ERR_ABI_MISMATCH, "n1_size_mismatch");
        std::memset(&info, 0, sizeof(info));
        info.struct_size = sizeof(info);
        info.abi_version = AIO_ABI_VERSION_V1 + 1;  // 篡改 version
        AIO_CHECK(cs, aio_abi_query_v1(&info) == AIO_ERR_ABI_MISMATCH, "n1_version_mismatch");
    }

    // N2: hash_buffer 参数域
    {
        char hex[AIO_DIGEST_HEX_LEN + 1] = {0};
        AIO_CHECK(cs, aio_content_hash_buffer_v1(nullptr, 5, hex) == AIO_ERR_PARAM,
                  "n2_null_data_nonzero_len");
        AIO_CHECK(cs, aio_content_hash_buffer_v1("abc", 3, nullptr) == AIO_ERR_PARAM,
                  "n2_null_out_hex");
    }

    // N3: 声明非法 + 内容篡改 (先验声明格式; 篡改 → HASH_MISMATCH 非 DECL_INVALID)
    {
        const char* content = "tamper-probe-0xAIO";
        const std::size_t n = std::strlen(content);
        char hex[AIO_DIGEST_HEX_LEN + 1] = {0};
        AIO_CHECK(cs, aio_content_hash_buffer_v1(content, n, hex) == AIO_OK, "n3_hash");
        // 声明非法域: NULL / 长度错 / 大写 / 非 hex 字符
        AIO_CHECK(cs, aio_content_hash_verify_buffer_v1(content, n, nullptr) ==
                          AIO_ERR_DECL_INVALID, "n3_decl_null");
        AIO_CHECK(cs, aio_content_hash_verify_buffer_v1(content, n, "abc") ==
                          AIO_ERR_DECL_INVALID, "n3_decl_short");
        char upper[AIO_DIGEST_HEX_LEN + 1] = {0};
        for (std::size_t i = 0; i < AIO_DIGEST_HEX_LEN; ++i)
            upper[i] = static_cast<char>(hex[i] >= 'a' ? hex[i] - 'a' + 'A' : hex[i]);
        AIO_CHECK(cs, aio_content_hash_verify_buffer_v1(content, n, upper) ==
                          AIO_ERR_DECL_INVALID, "n3_decl_upper");
        char badch[AIO_DIGEST_HEX_LEN + 1];
        std::snprintf(badch, sizeof(badch), "%s", hex);
        badch[3] = 'g';  // 非 hex
        AIO_CHECK(cs, aio_content_hash_verify_buffer_v1(content, n, badch) ==
                          AIO_ERR_DECL_INVALID, "n3_decl_nonhex");
        // 格式合法但内容篡改 → HASH_MISMATCH
        char tampered[AIO_DIGEST_HEX_LEN + 1];
        std::snprintf(tampered, sizeof(tampered), "%s", hex);
        tampered[0] = (tampered[0] == '0') ? '1' : '0';
        AIO_CHECK_MSG(cs, aio_content_hash_verify_buffer_v1(content, n, tampered) ==
                              AIO_ERR_HASH_MISMATCH, "n2_verify_mismatch_shortcut",
                      "declared mismatch not detected (rc=%d)",
                      aio_content_hash_verify_buffer_v1(content, n, tampered));
        // 内容篡改 (改一字节) → HASH_MISMATCH
        char mutated[64];
        std::snprintf(mutated, sizeof(mutated), "%s", content);
        mutated[0] = static_cast<char>(mutated[0] + 1);
        AIO_CHECK(cs, aio_content_hash_verify_buffer_v1(mutated, n, hex) ==
                          AIO_ERR_HASH_MISMATCH, "n2_verify_mismatch_shortcut");
    }

    // N4: 文件复核 (打开失败 / 截断 / 篡改 / 大小核对)
    {
        // 临时目录 (mkdtemp; 每次运行唯一)
        std::string tmpl = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") +
                           "/aio_abi_test_XXXXXX";
        std::vector<char> dirbuf(tmpl.begin(), tmpl.end());
        dirbuf.push_back('\0');
        if (!mkdtemp(dirbuf.data())) {
            std::fprintf(stderr, "CHECK failed: mkdtemp\n");
            ++cs.failures;
            return 1;
        }
        const std::string dir(dirbuf.data());

        // 4a: 不存在 → AIO_ERR_IO
        {
            const int st = aio_content_hash_verify_file_v1(
                (dir + "/no_such_file.bin").c_str(),
                "0000000000000000000000000000000000000000000000000000000000000000", 0, nullptr);
            AIO_CHECK(cs, st == AIO_ERR_IO, "n4_open_missing");
        }

        // 4b: 写文件 (16KB+137B 跨块) → 复核正向; actual_size 回填
        const std::string path = dir + "/payload.bin";
        const std::size_t kLen = AIO_HASH_FILE_CHUNK + 137;
        std::vector<unsigned char> payload(kLen);
        for (std::size_t i = 0; i < kLen; ++i)
            payload[i] = static_cast<unsigned char>((i * 131u + 17u) & 0xFFu);
        {
            FILE* f = std::fopen(path.c_str(), "wb");
            AIO_CHECK(cs, f != nullptr, "n4_tmp_write");
            if (f) {
                std::fwrite(payload.data(), 1, payload.size(), f);
                std::fclose(f);
            }
        }
        {
            const std::string decl = oracle_hex(payload.data(), payload.size());
            std::uint64_t actual = 0;
            AIO_CHECK_MSG(cs, aio_content_hash_verify_file_v1(path.c_str(), decl.c_str(),
                                                              payload.size(), &actual) == AIO_OK,
                          "n4_verify_ok", "file verify not AIO_OK");
            AIO_CHECK(cs, actual == payload.size(), "n4_actual_size");
            // expected_size=0 → 跳过大小核对仍复核内容
            AIO_CHECK(cs, aio_content_hash_verify_file_v1(path.c_str(), decl.c_str(), 0,
                                                          nullptr) == AIO_OK,
                      "n4_size_skip_allowed");
            // expected_size 不符 → AIO_ERR_TRUNCATED
            AIO_CHECK_MSG(cs, aio_content_hash_verify_file_v1(path.c_str(), decl.c_str(),
                                                              payload.size() + 1,
                                                              &actual) == AIO_ERR_TRUNCATED,
                          "n3_file_size_skip", "size mismatch not detected");
            // 声明非法 → DECL_INVALID (先于文件读取)
            AIO_CHECK(cs, aio_content_hash_verify_file_v1(path.c_str(), "zz", 0, nullptr) ==
                          AIO_ERR_DECL_INVALID, "n4_decl_invalid");
        }

        // 4c: 内容篡改 (改中间一字节) → HASH_MISMATCH
        {
            std::vector<unsigned char> copy(payload);
            copy[copy.size() / 2] ^= 0x01u;
            const std::string decl = oracle_hex(payload.data(), payload.size());  // 原内容声明
            FILE* f = std::fopen(path.c_str(), "wb");
            AIO_CHECK(cs, f != nullptr, "n4_tamper_write");
            if (f) {
                std::fwrite(copy.data(), 1, copy.size(), f);
                std::fclose(f);
            }
            AIO_CHECK_MSG(cs, aio_content_hash_verify_file_v1(path.c_str(), decl.c_str(),
                                                              copy.size(), nullptr) ==
                              AIO_ERR_HASH_MISMATCH, "n2_verify_mismatch_shortcut",
                          "file tamper not detected");
        }

        // 4d: 空文件 (0 字节) 复核 = 空摘要向量
        {
            const std::string p0 = dir + "/empty.bin";
            FILE* f = std::fopen(p0.c_str(), "wb");
            AIO_CHECK(cs, f != nullptr, "n4_empty_write");
            if (f) std::fclose(f);
            AIO_CHECK(cs, aio_content_hash_verify_file_v1(
                              p0.c_str(),
                              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                              0, nullptr) == AIO_OK, "n4_empty_ok");
            AIO_CHECK(cs, aio_content_hash_verify_file_v1(
                              p0.c_str(),
                              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                              1, nullptr) == AIO_ERR_TRUNCATED, "n4_empty_size");
        }

        // 清理临时目录
        std::remove(path.c_str());
        std::remove((dir + "/empty.bin").c_str());
        std::remove(dir.c_str());
    }

    // N5: 55..129B 块边界文件逐长度复核 (流式块边界正确性)
    {
        std::string tmpl = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") +
                           "/aio_abi_edge_XXXXXX";
        std::vector<char> dirbuf(tmpl.begin(), tmpl.end());
        dirbuf.push_back('\0');
        if (!mkdtemp(dirbuf.data())) {
            std::fprintf(stderr, "CHECK failed: mkdtemp(edge)\n");
            ++cs.failures;
            return 1;
        }
        const std::string dir(dirbuf.data());
        bool ok = true;
        for (const auto& tc : kNistCases) {
            if (tc.len == 0 || tc.content) continue;  // 空内容 4d 已覆盖; 字面向量 U2 已覆盖
            std::vector<unsigned char> buf(static_cast<std::size_t>(tc.len), 'x');
            const std::string p = dir + ("/e" + std::to_string(tc.len) + ".bin");
            FILE* f = std::fopen(p.c_str(), "wb");
            if (!f) { ok = false; break; }
            std::fwrite(buf.data(), 1, buf.size(), f);
            std::fclose(f);
            if (aio_content_hash_verify_file_v1(p.c_str(), tc.expect, tc.len, nullptr) != AIO_OK) {
                ok = false;
                break;
            }
            std::remove(p.c_str());
        }
        AIO_CHECK(cs, ok, "n5_edge_lengths");
        std::remove(dir.c_str());
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace aio_abi_test

// ───────── main (全局; 组分发; selfcheck TU 经 AIO_ABI_TEST_NO_MAIN 复用本 TU) ─────────
#ifndef AIO_ABI_TEST_NO_MAIN
int main(int argc, char** argv) { return aio_abi_tests_main(argc, argv); }
#endif

int aio_abi_tests_main(int argc, char** argv) {
    ::aio_abi_test::init_fault_registry_from_env();
    if (argc >= 2 && std::strcmp(argv[1], "units") == 0)
        return ::aio_abi_test::run_group("units");
    if (argc >= 2 && std::strcmp(argv[1], "negative") == 0)
        return ::aio_abi_test::run_group("negative");
    if (argc >= 2 && std::strcmp(argv[1], "all") == 0) {
        if (::aio_abi_test::run_group("units") != 0) return 1;
        return ::aio_abi_test::run_group("negative");
    }
    std::fprintf(stderr, "usage: aio_abi_tests [units|negative|all]\n");
    return argc >= 2 ? 127 : 1;
}
