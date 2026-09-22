// ============================================================================
// test_hips_atomic_publish.cpp — FIX-401 验证面 (独立于实现者)
//
// 被测面: lib/infrastructure/aio/src/hips/aio_hips_writer.cpp (每 tile 私有临时
// 文件 → 哈希校验 → fsync → 原子 rename; 全部 tile 完成后最后落完成清单) +
// aio_hips_reader.cpp (无完成清单 ⇒ 消费者拒绝, fail-closed)。
//
// 权威: ASTROCS_DESIGN.md §10「I/O 与原子产品」/ GAP_AUDIT G3-1。
//
// 用例 (每条都能红能绿, 无恒 PASS 占位):
//   A1 publish_ok            正常发布: 清单齐备 + 每 tile DATASUM/CHECKSUM 校验通过
//                            + 零 .tmp. 残留 + 消费者可读
//   A2 consumer_no_manifest  负例: 摘掉完成清单 ⇒ aio_hips_open 必须拒绝 (fail-closed);
//                            放回清单 ⇒ 恢复可读 (证明拒绝是清单引起, 非其它原因)
//   A3 kill_mid_write        中途 kill (SIGKILL, 驻留私有临时文件窗口):
//                            output_dir 无完成清单; 已发布 tile 全部校验通过 (无半成品);
//                            残留 .tmp. 不是产品; 重跑不消费残留 (tile 数=本次写入数)
//   A4 diskfull_injection    磁盘满/写失败/校验失败/fsync 失败/rename 失败五个注入
//                            各自必败, 且正式目录零 .fits、零清单、零 .tmp. 残留
//   A5 integrity_has_teeth   判别力红锚: 人为截断一个已发布 tile ⇒ 校验必须判红
//                            (证明 A1/A3 的"全部校验通过"不是恒真门)
//   A6 disk_full_at_failure  磁盘满在失败瞬间(清理之前)分类; 非空间类失败不得冒充
//   A7 frame_level_attribution 帧级归因协议: 并发帧各自的磁盘满判定互不覆盖/互不抢占
//                            (LOG_AND_ERROR_CONTRACT §5「失败节点 manifest 的
//                             error_kind==disk_full ⇒ exit 10」的帧级判据)
// ============================================================================
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <fitsio.h>

#include "aio_hips.h"
#include "aio_hips_reader.h"
#include "aio_disk_full.h"   // FIX-401: 磁盘满失败瞬间分类 (A6 判别力锚点)

namespace fs = std::filesystem;

static int g_fail = 0;
static std::string g_case = "(init)";

#define EXPECT(cond)                                                          \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL [%s] %s:%d: %s\n", g_case.c_str(), __FILE__,    \
                        __LINE__, #cond);                                     \
            ++g_fail;                                                         \
        }                                                                     \
    } while (0)

#define EXPECT_MSG(cond, msg)                                                 \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL [%s] %s:%d: %s (%s)\n", g_case.c_str(),         \
                        __FILE__, __LINE__, #cond, (msg));                    \
            ++g_fail;                                                         \
        }                                                                     \
    } while (0)

static void set_case(const char* c) {
    g_case = c;
    std::printf("case: %s\n", c);
}

// ── 文件系统 helper (测试面自用, 不经产品 I/O) ──────────────────────────────
static bool is_tmp_name(const std::string& name) {
    return name.find(".tmp.") != std::string::npos;
}

// HiPS 叶级 tile 文件名 = Npix<完整 tile 号>.fits (IVOA REC-HIPS-1.0 §4.1);
// 子产品根下的 metadata.fits/properties/Moc.fits 不是 tile, 不参与 tile 计数。
static bool is_tile_name(const std::string& name) {
    if (is_tmp_name(name)) return false;
    if (name.size() < 10) return false;   // 最短合法名 = "Npix0.fits" (10)
    if (name.compare(0, 4, "Npix") != 0) return false;
    if (name.compare(name.size() - 5, 5, ".fits") != 0) return false;
    for (std::size_t i = 4; i + 5 < name.size(); ++i)
        if (name[i] < '0' || name[i] > '9') return false;
    return true;
}

static std::vector<std::string> list_files(const std::string& root) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::exists(root, ec)) return out;
    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_regular_file(ec)) out.push_back(it->path().string());
    }
    return out;
}

static int count_final_fits(const std::string& root) {
    int n = 0;
    for (const auto& p : list_files(root))
        if (is_tile_name(fs::path(p).filename().string())) ++n;
    return n;
}

static int count_tmp_files(const std::string& root) {
    int n = 0;
    for (const auto& p : list_files(root))
        if (is_tmp_name(fs::path(p).filename().string())) ++n;
    return n;
}

// 独立实现的 DATASUM/CHECKSUM 校验 (逐 HDU; 不复用被测实现的静态函数)。
static bool fits_checksum_ok(const std::string& path, std::string* why) {
    int status = 0;
    fitsfile* f = nullptr;
    if (fits_open_file(&f, path.c_str(), READONLY, &status)) {
        if (why) *why = "open failed";
        fits_clear_errmsg();
        return false;
    }
    int nhdu = 0;
    if (fits_get_num_hdus(f, &nhdu, &status) || nhdu <= 0) {
        if (why) *why = "get_num_hdus failed";
        fits_close_file(f, &status);
        fits_clear_errmsg();
        return false;
    }
    int verified = 0;
    for (int h = 1; h <= nhdu; ++h) {
        if (fits_movabs_hdu(f, h, nullptr, &status)) {
            if (why) *why = "movabs failed";
            fits_close_file(f, &status);
            fits_clear_errmsg();
            return false;
        }
        char csum[FLEN_VALUE];
        csum[0] = '\0';
        int ks = 0;
        if (fits_read_key(f, TSTRING, "CHECKSUM", csum, nullptr, &ks)) {
            fits_clear_errmsg();
            status = 0;
            continue;
        }
        int dok = 0, hok = 0;
        if (fits_verify_chksum(f, &dok, &hok, &status)) {
            if (why) *why = "verify_chksum error";
            fits_close_file(f, &status);
            fits_clear_errmsg();
            return false;
        }
        if (dok != 1 || hok != 1) {
            if (why)
                *why = "DATASUM/CHECKSUM mismatch (HDU " + std::to_string(h) + ")";
            fits_close_file(f, &status);
            return false;
        }
        ++verified;
    }
    fits_close_file(f, &status);
    if (verified == 0) {
        if (why) *why = "no CHECKSUM keyword";
        return false;
    }
    return true;
}

static bool all_tiles_ok(const std::string& root, int* n_checked,
                         std::string* first_bad) {
    int n = 0;
    bool ok = true;
    for (const auto& p : list_files(root)) {
        const std::string name = fs::path(p).filename().string();
        if (!is_tile_name(name)) continue;
        ++n;
        std::string why;
        if (!fits_checksum_ok(p, &why)) {
            ok = false;
            if (first_bad && first_bad->empty()) *first_bad = p + ": " + why;
        }
    }
    if (n_checked) *n_checked = n;
    return ok;
}

// ── 产品写出 (被测面) ───────────────────────────────────────────────────────
static const uint32_t kNSide = 512;   // 叶级 nside=512 ⇒ tile_order=0 (无 hierarchy)

static int write_product(const std::string& dir, int n_tiles) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        dir.c_str(), kNSide, 512, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
        "ivo://astrocs/fix401-test", "FIX-401 test product", nullptr, 0.0, nullptr, 0);
    if (!ps) return -1;
    const size_t n = 512u * 512u;
    std::vector<float> flux(n), area(n);
    for (int t = 0; t < n_tiles; ++t) {
        for (size_t i = 0; i < n; ++i) {
            flux[i] = (float)(0.25 * (double)((i + (size_t)t * 7u) % 101u));
            area[i] = (float)(1e-7 * (double)(1 + (i % 5)));
        }
        AstroSphereTileView view;
        std::memset(&view, 0, sizeof(view));
        aio_hips_tile_view_abi_init(&view);
        view.parent_ipix = (uint64_t)t;
        view.leaf_order = 9;
        view.width = 512;
        view.data_type = AIO_HIPS_FLOAT32;
        view.flux_sum = flux.data();
        view.covered_area = area.data();
        view.valid_mask = nullptr;
        const int rc = aio_hips_write_signal_support_tile(ps, &view);
        if (rc != 0) {
            aio_hips_abort(ps);
            return rc;
        }
    }
    return aio_hips_finalize(ps);
}

static std::string read_text(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

static std::string make_root(const char* tag) {
    char tmpl[256];
    std::snprintf(tmpl, sizeof(tmpl), "/tmp/fix401_%s_XXXXXX", tag);
    char* d = mkdtemp(tmpl);
    return d ? std::string(d) : std::string();
}

// ── A1: 正常发布 ───────────────────────────────────────────────────────────
static void case_publish_ok() {
    set_case("A1_publish_ok");
    const std::string root = make_root("a1");
    const std::string dir = root + "/product";
    const int rc = write_product(dir, 3);
    EXPECT_MSG(rc == 0, aio_hips_last_error());
    // 完成清单齐备
    const std::string man = read_text(dir + "/manifest.json");
    EXPECT(!man.empty());
    EXPECT(man.find("\"products\"") != std::string::npos);
    EXPECT(man.find("\"signal\"") != std::string::npos);
    EXPECT(man.find("\"support\"") != std::string::npos);
    EXPECT(man.find("\"n_leaf_tiles\": 3") != std::string::npos);
    // 每 tile 内容哈希 (DATASUM/CHECKSUM) 校验通过 + 零临时残留
    int n_checked = 0;
    std::string bad;
    EXPECT_MSG(all_tiles_ok(dir, &n_checked, &bad), bad.c_str());
    EXPECT_MSG(n_checked == 6, "signal+support 各 3 个 tile");   // 3 tiles x 2 子产品
    EXPECT_MSG(count_tmp_files(dir) == 0, "正式目录不得有 .tmp. 残留");
    // 消费者可读
    AioHipsDataset* ds = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SIGNAL);
    EXPECT_MSG(ds != nullptr, aio_hips_reader_last_error());
    if (ds) {
        EXPECT(aio_hips_tile_count(ds) == 3);
        aio_hips_close(ds);
    }
    fs::remove_all(root);
}

// ── A2: 无完成清单 ⇒ 消费者拒绝 (fail-closed) ──────────────────────────────
static void case_consumer_rejects_without_manifest() {
    set_case("A2_consumer_no_manifest");
    const std::string root = make_root("a2");
    const std::string dir = root + "/product";
    EXPECT(write_product(dir, 2) == 0);
    // 阴性对照: 清单在 ⇒ 可读
    AioHipsDataset* ok = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SIGNAL);
    EXPECT_MSG(ok != nullptr, aio_hips_reader_last_error());
    if (ok) aio_hips_close(ok);
    // 摘掉完成清单 (模拟中途 kill / 未 finalize 的残留)
    const std::string man_path = dir + "/manifest.json";
    const std::string saved = read_text(man_path);
    EXPECT(!saved.empty());
    EXPECT(std::remove(man_path.c_str()) == 0);
    AioHipsDataset* bad = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SIGNAL);
    EXPECT_MSG(bad == nullptr, "无完成清单目录不得被消费 (§10 fail-closed)");
    if (bad) aio_hips_close(bad);
    const char* e = aio_hips_reader_last_error();
    EXPECT_MSG(e && std::strstr(e, "完成清单") != nullptr,
               e ? e : "(null last_error)");
    // 放回清单 ⇒ 恢复可读 (证明拒绝由清单缺失引起, 不是其它原因)
    {
        std::ofstream f(man_path, std::ios::binary);
        f << saved;
    }
    AioHipsDataset* again = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SIGNAL);
    EXPECT_MSG(again != nullptr, aio_hips_reader_last_error());
    if (again) aio_hips_close(again);
    fs::remove_all(root);
}

// ── A3: 中途 kill (SIGKILL) ────────────────────────────────────────────────
static void case_kill_mid_write() {
    set_case("A3_kill_mid_write");
    const std::string root = make_root("a3");
    const std::string dir = root + "/product";
    fs::create_directories(dir);

    const pid_t pid = fork();
    if (pid == 0) {
        // 子进程: 驻留"内容已进私有临时文件、尚未 rename"的窗口 (400ms/tile)
        setenv("ASTROCS_HIPS_TILE_FAULT", "tile_slow_write", 1);
        const int rc = write_product(dir, 12);
        _exit(rc == 0 ? 0 : 1);
    }
    EXPECT(pid > 0);
    // 等到"已有 ≥3 个 tile 原子发布 + 恰有 1 个私有临时文件在窗口内"再 kill:
    // 既保证确实发生在写入中途, 又保证有可判别的已发布内容。
    bool armed = false;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < deadline) {
        // 已发布 ≥4 个 tile 文件 (= 2 个叶级 tile 的 signal+support) 且恰有
        // 1 个私有临时文件在写入窗口内 ⇒ kill 落在写入中途。
        if (count_final_fits(dir) >= 4 && count_tmp_files(dir) >= 1) {
            armed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_MSG(armed, "未观察到'写入中途'窗口 (kill 锚点失效)");
    kill(pid, SIGKILL);
    int st = 0;
    waitpid(pid, &st, 0);

    const int n_final = count_final_fits(dir);
    const int n_tmp = count_tmp_files(dir);
    EXPECT_MSG(n_final >= 4, "kill 前应有已原子发布的完整 tile");
    // (a) 无完成清单
    EXPECT_MSG(!fs::exists(dir + "/manifest.json"),
               "kill 后不得存在完成清单");
    // (b) 已发布 tile 全部完整 (无半成品): 逐个 DATASUM/CHECKSUM 校验
    int checked = 0;
    std::string bad;
    EXPECT_MSG(all_tiles_ok(dir, &checked, &bad), bad.c_str());
    EXPECT(checked == n_final);
    // (c) 私有临时文件确实残留 (证明 kill 落在写入窗口内), 且它不是产品
    EXPECT_MSG(n_tmp >= 1, "kill 应留下私有临时文件 (窗口锚点)");
    // (d) 消费者拒绝该残留目录
    AioHipsDataset* ds = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SIGNAL);
    EXPECT_MSG(ds == nullptr, "kill 残留目录不得被消费");
    if (ds) aio_hips_close(ds);

    // (e) 重跑不消费残留: 本次只写 1 个 tile, 而 kill 前已发布 ≥2 个 ⇒
    //     正式目录里上次的 tile 文件必须被清除, 产品面恰为本次写入的 1 个。
    const int rc2 = write_product(dir, 1);
    EXPECT_MSG(rc2 == 0, aio_hips_last_error());
    EXPECT_MSG(!read_text(dir + "/manifest.json").empty(), "重跑必须落完成清单");
    EXPECT_MSG(count_tmp_files(dir) == 0, "重跑必须清除上次 kill 的临时残留");
    EXPECT_MSG(count_final_fits(dir) == 2,
               "signal+support 各 1 (上次 kill 的残留 tile 不得被消费/留存)");
    AioHipsDataset* ds2 = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SIGNAL);
    EXPECT_MSG(ds2 != nullptr, aio_hips_reader_last_error());
    if (ds2) {
        EXPECT_MSG(aio_hips_tile_count(ds2) == 1,
                   "重跑后 tile 数必须等于本次写入数 (残留不得混入)");
        aio_hips_close(ds2);
    }
    fs::remove_all(root);
}

// ── A4: 磁盘满/写失败/校验失败/fsync/rename 注入 ───────────────────────────
static void case_fault_injections() {
    set_case("A4_fault_injections");
    struct { const char* name; } faults[] = {
        {"tile_diskfull"}, {"tile_write_fail"}, {"tile_checksum_fail"},
        {"tile_fsync_fail"}, {"tile_rename_fail"},
    };
    for (const auto& f : faults) {
        const std::string root = make_root("a4");
        const std::string dir = root + "/product";
        fs::create_directories(dir);
        setenv("ASTROCS_HIPS_TILE_FAULT", f.name, 1);
        const int rc = write_product(dir, 1);
        unsetenv("ASTROCS_HIPS_TILE_FAULT");
        EXPECT_MSG(rc != 0, (std::string("注入 ") + f.name + " 必须必败").c_str());
        // 正式目录: 零 tile、零清单、零临时残留
        EXPECT_MSG(count_final_fits(dir) == 0,
                   (std::string("注入 ") + f.name + " 后正式目录不得有 .fits").c_str());
        EXPECT_MSG(!fs::exists(dir + "/manifest.json"),
                   (std::string("注入 ") + f.name + " 后不得有完成清单").c_str());
        EXPECT_MSG(count_tmp_files(dir) == 0,
                   (std::string("注入 ") + f.name + " 后不得有 .tmp. 残留").c_str());
        AioHipsDataset* ds = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SIGNAL);
        EXPECT(ds == nullptr);
        if (ds) aio_hips_close(ds);
        fs::remove_all(root);
    }
    // 阴性对照 (证明上面的"必败"来自注入, 不是环境本身写不出):
    const std::string root = make_root("a4c");
    const std::string dir = root + "/product";
    const int rc = write_product(dir, 1);
    EXPECT_MSG(rc == 0, aio_hips_last_error());
    EXPECT(count_final_fits(dir) == 2);
    EXPECT(fs::exists(dir + "/manifest.json"));
    fs::remove_all(root);
}

// ── A5: 判别力红锚 —— 截断的 tile 必须判红 ─────────────────────────────────
static void case_integrity_has_teeth() {
    set_case("A5_integrity_has_teeth");
    const std::string root = make_root("a5");
    const std::string dir = root + "/product";
    EXPECT(write_product(dir, 1) == 0);
    int n_checked = 0;
    std::string bad;
    EXPECT_MSG(all_tiles_ok(dir, &n_checked, &bad), bad.c_str());
    EXPECT(n_checked == 2);
    // 人为把 signal 的 tile 截断一半 —— 等价于"修复前直写正式路径时被 kill"
    // 留下的半成品。校验必须判红 (否则 A1/A3 的"全部通过"是恒真门)。
    std::string victim;
    for (const auto& p : list_files(dir)) {
        if (p.find("/signal/") != std::string::npos &&
            is_tile_name(fs::path(p).filename().string())) {
            victim = p;
            break;
        }
    }
    EXPECT(!victim.empty());
    if (!victim.empty()) {
        std::error_code ec;
        const auto sz = fs::file_size(victim, ec);
        EXPECT(!ec && sz > 2880);
        fs::resize_file(victim, sz / 2, ec);
        EXPECT(!ec);
        std::string why;
        EXPECT_MSG(!fits_checksum_ok(victim, &why),
                   "截断的 tile 必须被校验判红 (判别力红锚)");
    }
    fs::remove_all(root);
}

// ── A6: 磁盘满在**失败瞬间 (清理之前)** 就被分类, 且只有磁盘满类失败被分类 ──
// 依据: ASTROCS_DESIGN §10「失败/取消路径清理临时产物」+ §7.2「10 = 磁盘写满/
// 写盘失败」。CLI 的 exit-10 判定原为事后探针; 清理释放空间后探针必然 fail-open,
// 故判据必须是失败发生处的分类 (lib/infrastructure/aio/src/aio_disk_full.h)。
// 能红能绿: 注入 tile_diskfull(ENOSPC 等价) 必须置位; 注入 tile_write_fail
// (合成写失败, 文件系统仍有空间) 必须**不**置位 ⇒ 仍走 exit 7 而不是 10。
static void case_disk_full_classified_at_failure() {
    set_case("A6_disk_full_classified_at_failure");
    {
        const std::string root = make_root("a6full");
        const std::string dir = root + "/product";
        fs::create_directories(dir);
        setenv("ASTROCS_HIPS_TILE_FAULT", "tile_diskfull", 1);
        aio_disk::FailureEpoch epoch;   // 归因窗口: 开始写产品之前
        const int rc = write_product(dir, 1);
        unsetenv("ASTROCS_HIPS_TILE_FAULT");
        EXPECT(rc != 0);
        // 分类必须已经发生 (且发生在清理之前 —— 见 write_fits_atomic 中
        // note_full()/note_failure() 早于 remove_file(tmp) 的次序)。
        EXPECT_MSG(epoch.failed(),
                   "注入 ENOSPC 等价失败后本执行流的归因窗口必须已判定 disk_full (失败瞬间)");
        // 清理仍然生效 (分类不得以"留下残留"为代价)
        EXPECT(count_tmp_files(dir) == 0);
        EXPECT(count_final_fits(dir) == 0);
        fs::remove_all(root);
    }
    {
        const std::string root = make_root("a6write");
        const std::string dir = root + "/product";
        fs::create_directories(dir);
        setenv("ASTROCS_HIPS_TILE_FAULT", "tile_write_fail", 1);
        aio_disk::FailureEpoch epoch;
        const int rc = write_product(dir, 1);
        unsetenv("ASTROCS_HIPS_TILE_FAULT");
        EXPECT(rc != 0);
        EXPECT_MSG(!epoch.failed(),
                   "非空间类写失败不得被分类为 disk_full (否则 exit 7 被误升为 10)");
        fs::remove_all(root);
    }
}

// ── A7: **帧级归因协议** —— 并发帧各自的磁盘满判定互不覆盖/互不抢占 ──────────
// 依据: docs/contracts/LOG_AND_ERROR_CONTRACT.md §5「`IO` | 7（IO）| I/O 失败;
// **失败节点 manifest** 的 `error_kind==disk_full` 时改判 10」——判定属于**失败的
// 那一帧/那个节点**, 不是"进程里发生过一次磁盘满"的运行级事实。
// 旧实现 (进程级单比特 + aio_hips_product_begin 里的 reset() + exchange 语义的
// consume()) 在多帧并发下互相覆盖: 另一帧的 product_begin 抹掉本帧已置位的判定,
// 或另一帧的失败收尾抢走本帧的判定 ⇒ 失败帧丢 error_kind ⇒ exit 7 的 fail-open
// (FLAKE-01 剂量-反应实证: 单帧 12/12 正确, 双帧 58 次中 5 次 rc=7)。
// 本用例把该竞态钉成**确定性**判据 (旧实现下 T2/T3 必红)。
//
// T1 串行对照 : 帧 A 在 product_begin 之后真失败 ⇒ A 的窗口 failed();
// T2 不抹除   : 帧 A 判定已置位后帧 B 完整跑一遍 (含 product_begin) ⇒ A 仍 failed();
// T3 不抢占   : 两帧**真并发**各写自己的目录、各自真失败 ⇒ 两个窗口**都** failed();
// T4 阴性对照 : 非空间类失败 (tile_write_fail) 不得让任何窗口 failed()。
static void case_frame_level_attribution() {
    set_case("A7_frame_level_attribution");
    // T1 + T2: 串行两帧 (确定性; 不依赖调度)
    {
        const std::string root = make_root("a7serial");
        const std::string dir_a = root + "/A";
        const std::string dir_b = root + "/B";
        fs::create_directories(dir_a);
        fs::create_directories(dir_b);
        setenv("ASTROCS_HIPS_TILE_FAULT", "tile_diskfull", 1);
        aio_disk::FailureEpoch ep_a;              // 帧 A 的归因窗口
        const int rc_a = write_product(dir_a, 1);
        EXPECT(rc_a != 0);
        EXPECT_MSG(ep_a.failed(), "T1: 帧 A 的窗口必须看到自己的磁盘满分类");
        aio_disk::FailureEpoch ep_b;              // 帧 B 的归因窗口
        const int rc_b = write_product(dir_b, 1); // 内部含 aio_hips_product_begin
        unsetenv("ASTROCS_HIPS_TILE_FAULT");
        EXPECT(rc_b != 0);
        EXPECT_MSG(ep_b.failed(), "T2: 帧 B 的窗口必须看到自己的磁盘满分类");
        EXPECT_MSG(ep_a.failed(),
                   "T2: 帧 B 的 aio_hips_product_begin 不得抹掉帧 A 已置位的判定 "
                   "(旧实现: product_begin 里的进程级 reset() ⇒ 此处必红)");
        fs::remove_all(root);
    }
    // T3: 两帧真并发, 各自真失败 ⇒ 判定互不抢占
    {
        const std::string root = make_root("a7concurrent");
        const std::string dir_a = root + "/A";
        const std::string dir_b = root + "/B";
        fs::create_directories(dir_a);
        fs::create_directories(dir_b);
        setenv("ASTROCS_HIPS_TILE_FAULT", "tile_diskfull", 1);
        bool a_failed = false, b_failed = false;
        int rc_a = 0, rc_b = 0;
        std::thread ta([&] {
            aio_disk::FailureEpoch ep;            // 帧 A 自己的窗口
            rc_a = write_product(dir_a, 2);
            a_failed = ep.failed();
        });
        std::thread tb([&] {
            aio_disk::FailureEpoch ep;            // 帧 B 自己的窗口
            rc_b = write_product(dir_b, 2);
            b_failed = ep.failed();
        });
        ta.join();
        tb.join();
        unsetenv("ASTROCS_HIPS_TILE_FAULT");
        EXPECT(rc_a != 0);
        EXPECT(rc_b != 0);
        EXPECT_MSG(a_failed,
                   "T3: 并发帧 A 的判定不得被帧 B 的 product_begin/consume 抹掉或抢占 "
                   "(旧实现: 进程级单比特 exchange 语义 ⇒ 至少一帧必红)");
        EXPECT_MSG(b_failed, "T3: 并发帧 B 的判定不得被帧 A 抹掉或抢占");
        fs::remove_all(root);
    }
    // T4 阴性对照: 非空间类失败不得让窗口判定为磁盘满
    {
        const std::string root = make_root("a7write");
        const std::string dir = root + "/product";
        fs::create_directories(dir);
        setenv("ASTROCS_HIPS_TILE_FAULT", "tile_write_fail", 1);
        aio_disk::FailureEpoch epoch;
        const int rc = write_product(dir, 1);
        unsetenv("ASTROCS_HIPS_TILE_FAULT");
        EXPECT(rc != 0);
        EXPECT_MSG(!epoch.failed(),
                   "T4: 合成写失败 (文件系统仍有空间) 不得被判为 disk_full ⇒ 仍走 exit 7");
        fs::remove_all(root);
    }
}

int main() {
    std::printf("FIX-401 aio HiPS atomic publish / completion-manifest tests\n");
    case_publish_ok();
    case_consumer_rejects_without_manifest();
    case_kill_mid_write();
    case_fault_injections();
    case_integrity_has_teeth();
    case_disk_full_classified_at_failure();
    case_frame_level_attribution();
    if (g_fail == 0) {
        std::printf("FIX401_HIPS_ATOMIC: ALL PASS\n");
        return 0;
    }
    std::printf("FIX401_HIPS_ATOMIC: FAIL (%d)\n", g_fail);
    return 1;
}
