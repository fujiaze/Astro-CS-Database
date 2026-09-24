// ============================================================================
// sparse_punch_probe.cpp - 打洞原语的可执行探针（判据 CHK-SPARSE-PUNCH-PROBE）
//
// 依据: docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md §7 表 T1；
//       ACCEPTANCE_SPEC.md §3.2；ENGINEERING_SPEC.md §8（每项检查必须有可执行
//       正/负例）。
//
// 关键性质: 本探针 **#include 生产头** lib/infrastructure/aio/src/aio_sparse_punch.h，
// 不是另写一份实现 —— 被测代码 = 上线代码。探针只做三件事:
//   1) 用同一头文件里的谓词跑差分语料（供 Python 侧与独立模型逐块比对）；
//   2) 调 punch_all_zero_blocks 做真实打洞，输出前后指标（字节 / 分配 / 洞图）；
//   3) 调 punch_range_forced 做负例注入（对 NaN 区强制打洞 ⇒ 必须判红）。
// 探针不链接 ACSD 任何可执行体与库，只编译 aio_sparse_punch.h + aio_log.cpp
// + crypto/sha256.cpp 三个翻译单元。
//
// 用法:
//   sparse_punch_probe predicate
//   sparse_punch_probe punch <file>
//   sparse_punch_probe repunch <file>
//   sparse_punch_probe forced <file> <offset> <length>
// 输出: 单行 JSON（stdout）。退出码: 0 = 探针自身跑通；2 = 参数/IO 不可用。
// ============================================================================

#include "aio_sparse_punch.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void json_escape(const std::string& s, std::string* out) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '"' || c == '\\') { out->push_back('\\'); out->push_back(c); }
        else if (c == '\n') out->append("\\n");
        else out->push_back(c);
    }
}

std::string jstr(const std::string& s) {
    std::string o = "\"";
    json_escape(s, &o);
    o += "\"";
    return o;
}

std::string u64(std::uint64_t v) { return std::to_string(v); }

// 洞图不在本探针内取：SEEK_DATA/SEEK_HOLE 属文件 I/O，本仓「aio 是文件级唯一
// I/O 边界」，判据工具不得引入第二处实现 ⇒ 由判据侧（Python）在文件层面计算。

struct Metrics {
    std::uint64_t size = 0;
    std::uint64_t alloc = 0;
    std::string sha;
};

Metrics metrics_of(const std::string& path) {
    Metrics m;
    (void)aio_sparse::file_metrics(path, &m.size, &m.alloc);
    (void)aio_file::sha256_hex(path.c_str(), &m.sha);
    return m;
}

std::string result_json(const aio_sparse::PunchResult& r) {
    std::string s = "{";
    s += "\"rc\":" + std::to_string(r.rc);
    s += ",\"reason\":" + jstr(r.reason);
    s += ",\"sys_errno\":" + std::to_string(r.sys_errno);
    s += ",\"size_bytes\":" + u64(r.size_bytes);
    s += ",\"alloc_before\":" + u64(r.alloc_before);
    s += ",\"alloc_after\":" + u64(r.alloc_after);
    s += ",\"released_bytes\":" + u64(r.released_bytes());
    s += ",\"zero_blocks\":" + u64(r.zero_blocks);
    s += ",\"punched_bytes\":" + u64(r.punched_bytes);
    s += ",\"holes\":" + std::to_string(r.holes);
    s += ",\"verified\":" + std::string(r.verified ? "true" : "false");
    s += ",\"sha256_before\":" + jstr(r.sha256_before);
    s += ",\"sha256_after\":" + jstr(r.sha256_after);
    s += ",\"block_bytes\":" + u64(aio_sparse::kPunchBlockBytes);
    s += "}";
    return s;
}

// ── 差分语料：谓词必须逐块一致（C++ 实现 vs Python 独立模型）────────────────
// 语料是**确定性**的（固定线性同余序列），每块 4096 B。
std::vector<std::pair<std::string, std::string>> make_corpus() {
    std::vector<std::pair<std::string, std::string>> c;   // (kind, bytes)
    const std::size_t blk = 4096;
    // 1) 全零
    c.push_back(std::make_pair("all_zero", std::string(blk, '\0')));
    // 2) 大端 canonical NaN 0x7FC00000（FITS >f4 的边距位型）
    {
        std::string b(blk, '\0');
        for (std::size_t i = 0; i + 4 <= blk; i += 4) {
            b[i] = '\x7f'; b[i + 1] = '\xc0'; b[i + 2] = '\x00'; b[i + 3] = '\x00';
        }
        c.push_back(std::make_pair("be_nan_7fc00000", b));
    }
    // 3) 大端 signalling NaN 0x7F800001
    {
        std::string b(blk, '\0');
        for (std::size_t i = 0; i + 4 <= blk; i += 4) {
            b[i] = '\x7f'; b[i + 1] = '\x80'; b[i + 2] = '\x00'; b[i + 3] = '\x01';
        }
        c.push_back(std::make_pair("be_snan_7f800001", b));
    }
    // 4) 大端负 NaN 0xFFC00000
    {
        std::string b(blk, '\0');
        for (std::size_t i = 0; i + 4 <= blk; i += 4) {
            b[i] = '\xff'; b[i + 1] = '\xc0'; b[i + 2] = '\x00'; b[i + 3] = '\x00';
        }
        c.push_back(std::make_pair("be_neg_nan_ffc00000", b));
    }
    // 5) 大端 -0.0（0x80000000）：浮点比较下等于 0.0，位型含非零字节
    {
        std::string b(blk, '\0');
        for (std::size_t i = 0; i + 4 <= blk; i += 4) b[i] = '\x80';
        c.push_back(std::make_pair("be_neg_zero_80000000", b));
    }
    // 6) 小端 -0.0（0x00000080）
    {
        std::string b(blk, '\0');
        for (std::size_t i = 0; i + 4 <= blk; i += 4) b[i + 3] = '\x80';
        c.push_back(std::make_pair("le_neg_zero_00000080", b));
    }
    // 7) 单个非零字节（块首 / 块尾）
    {
        std::string b(blk, '\0'); b[0] = '\x01';
        c.push_back(std::make_pair("one_nonzero_at_head", b));
        std::string b2(blk, '\0'); b2[blk - 1] = '\x01';
        c.push_back(std::make_pair("one_nonzero_at_tail", b2));
    }
    // 8) 真值非零（大端 1.0 = 0x3F800000）
    {
        std::string b(blk, '\0');
        for (std::size_t i = 0; i + 4 <= blk; i += 4) {
            b[i] = '\x3f'; b[i + 1] = '\x80'; b[i + 2] = '\x00'; b[i + 3] = '\x00';
        }
        c.push_back(std::make_pair("be_one_3f800000", b));
    }
    // 9) 固定种子伪随机（确定性 LCG）
    {
        std::string b(blk, '\0');
        std::uint32_t x = 0x12345678u;
        for (std::size_t i = 0; i < blk; ++i) {
            x = x * 1664525u + 1013904223u;
            b[i] = static_cast<char>((x >> 24) & 0xFF);
        }
        c.push_back(std::make_pair("lcg_random_seed_12345678", b));
    }
    // 10) 前半零 + 后半 NaN（混合块 ⇒ 不可打洞）
    {
        std::string b(blk, '\0');
        for (std::size_t i = blk / 2; i + 4 <= blk; i += 4) {
            b[i] = '\x7f'; b[i + 1] = '\xc0'; b[i + 2] = '\x00'; b[i + 3] = '\x00';
        }
        c.push_back(std::make_pair("half_zero_half_nan", b));
    }
    return c;
}

int cmd_predicate() {
    const std::vector<std::pair<std::string, std::string>> c = make_corpus();
    std::string s = "{\"corpus\":[";
    for (std::size_t i = 0; i < c.size(); ++i) {
        const unsigned char* p =
            reinterpret_cast<const unsigned char*>(c[i].second.data());
        const std::size_t n = c[i].second.size();
        const bool zero = aio_sparse::block_is_literal_zero(p, n);
        const bool nan = aio_sparse::block_contains_binary32_nan(p, n);
        if (i) s += ",";
        s += "{\"kind\":" + jstr(c[i].first) + ",\"bytes\":" + u64(n) +
             ",\"cxx_literal_zero\":" + std::string(zero ? "true" : "false") +
             ",\"cxx_has_nan\":" + std::string(nan ? "true" : "false") + "}";
    }
    s += "],\"block_bytes\":" + u64(aio_sparse::kPunchBlockBytes) + "}";
    std::printf("%s\n", s.c_str());
    return 0;
}

int cmd_punch(const std::string& path, bool twice) {
    const Metrics before = metrics_of(path);
    aio_sparse::PunchResult r;
    aio_sparse::punch_all_zero_blocks(path, &r, true);
    const Metrics after = metrics_of(path);
    std::string s = "{\"op\":\"punch\",\"path\":" + jstr(path) +
                    ",\"before\":{\"size\":" + u64(before.size) +
                    ",\"alloc\":" + u64(before.alloc) + ",\"sha256\":" +
                    jstr(before.sha) + "},\"after\":{\"size\":" + u64(after.size) +
                    ",\"alloc\":" + u64(after.alloc) + ",\"sha256\":" +
                    jstr(after.sha) + "},\"result\":" + result_json(r);
    if (twice) {
        aio_sparse::PunchResult r2;
        aio_sparse::punch_all_zero_blocks(path, &r2, true);
        const Metrics after2 = metrics_of(path);
        s += ",\"second\":" + result_json(r2) +
             ",\"after2\":{\"size\":" + u64(after2.size) +
             ",\"alloc\":" + u64(after2.alloc) + ",\"sha256\":" + jstr(after2.sha) + "}";
    }
    s += "}";
    std::printf("%s\n", s.c_str());
    return 0;
}

int cmd_forced(const std::string& path, std::uint64_t off, std::uint64_t len) {
    const Metrics before = metrics_of(path);
    aio_sparse::PunchResult r;
    aio_sparse::punch_range_forced(path, off, len, &r, true);
    const Metrics after = metrics_of(path);
    std::string s = "{\"op\":\"forced\",\"path\":" + jstr(path) +
                    ",\"offset\":" + u64(off) + ",\"length\":" + u64(len) +
                    ",\"before\":{\"size\":" + u64(before.size) +
                    ",\"alloc\":" + u64(before.alloc) + ",\"sha256\":" +
                    jstr(before.sha) + "},\"after\":{\"size\":" + u64(after.size) +
                    ",\"alloc\":" + u64(after.alloc) + ",\"sha256\":" +
                    jstr(after.sha) + "},\"result\":" + result_json(r) + "}";
    std::printf("%s\n", s.c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    aio_sparse::set_sink(nullptr);   // 不触发日志后端（探针不写仓库日志）
    if (argc < 2) {
        std::fprintf(stderr, "usage: sparse_punch_probe <predicate|punch|repunch|forced> ...\n");
        return 2;
    }
    const std::string cmd = argv[1];
    if (cmd == "predicate") return cmd_predicate();
    if (cmd == "punch" && argc >= 3) return cmd_punch(argv[2], false);
    if (cmd == "repunch" && argc >= 3) return cmd_punch(argv[2], true);
    if (cmd == "forced" && argc >= 5) {
        return cmd_forced(argv[2], std::strtoull(argv[3], nullptr, 10),
                          std::strtoull(argv[4], nullptr, 10));
    }
    std::fprintf(stderr, "bad args\n");
    return 2;
}
