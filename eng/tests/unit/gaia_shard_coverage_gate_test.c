/* G-1 shard 覆盖门 (GAIA-FAILCLOSED-01, P0)
 *
 * 判据（非退化、能红能绿）:
 *   GREEN ⟺ create 成功 ∧ file_count > 0 ∧ file_load_fail_count == 0
 *             ∧ file_count == 目录内 *.xpsd 条目数
 *   RED   ⟺ 上述任一条不成立
 *
 * 背景（根因）: run/WCS-DETERMINISM-01/REPORT.md §1.2 —— gaia_client_create_ex
 * 对目录里每个 *.xpsd 调 load_xpsd_file, 返回 -1 时**没有 else 分支**: 装载失败
 * 的 shard 被静默丢弃（无日志/无计数/create 仍返回非 NULL）。地址空间受限
 * （RLIMIT_AS）时丢掉的恰是唯一含亮星的 shard ⇒ 参考星表静默变成"暗 shard 里
 * 最亮的 60 颗" ⇒ iter_trans_solve 全败（WCS 节点 fail-closed 发生在更下游）。
 * **只断言 file_count > 0 不够**: 本缺陷下 file_count=2 却仍不可用 —— 本门因此
 * 同时断言 "file_count == 条目数" 与 "fail_count == 0"。
 *
 * 正例（必须绿）: 2 个星等区间不相交、均覆盖查询窗的 shard ⇒ 两条 shard 的星
 *   都必须出现在查询结果里（单 shard fixture 会恒真, 故必须双 shard + 端点断言）。
 * 负例（必须红）:
 *   ① 3 条目含 1 个 8 字节垃圾 ⇒ 装载失败 ⇒ create NULL（fail-closed）;
 *   ② 2 条目含 1 个魔数错误（尺寸正常）⇒ 同上;
 *   ③ 空目录 ⇒ 空星表（M42 主题: 不得静默降级解算）。
 * `--self-test`: 纯判据自检 —— 直接喂合成快照给 g1_judge(),
 *   证明判据本身能红能绿, 且能识别**修复前缺陷签名**（create 成功、条目=3、
 *   file_count=2、fail=1 ⇒ 必红）, 不依赖被测实现的行为。
 *
 * 基线红例（证据, 非 ctest）: 用 -DGAIA_G1_BASELINE 编译到**改前** gaia_client.c
 *   （run/GAIA-FAILCLOSED-01/baseline/）时, 负例 ①②③ 由 "file_count < 条目数"
 *   判红 —— 即改前实现必红（见 run/GAIA-FAILCLOSED-01/REPORT.md §3）。
 *
 * 本 TU 共址 #include gaia_client.c（直接驱动真实装载路径, 不定义 GAIA_ALLOC_TEST）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define ACS_TEST_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#define ACS_TEST_MKDIR(p) mkdir((p), 0777)
#endif

#include "gaia_client.c"   /* 被测实现（共址直接路径） */

#define INV_SCALE (1.0 / 1.8e9)
#define STRIDE    32

static int g_fail = 0;
static int g_checks = 0;
static int g_green = 0;
static int g_red = 0;

static void ok(const char* fmt, ...) {
    g_checks++;
    va_list ap; va_start(ap, fmt);
    fputs("ok   ", stdout); vprintf(fmt, ap); fputc('\n', stdout);
    va_end(ap);
}
static void fail(const char* fmt, ...) {
    g_fail++;
    va_list ap; va_start(ap, fmt);
    fputs("FAIL ", stdout); vprintf(fmt, ap); fputc('\n', stdout);
    va_end(ap);
}

/* ===== 合成 XPSD: header + XML(magnitudeRange 可选) + 1 叶节点 + raw 星记录 =====
 * 与 eng/tests/unit/gaia_magnitude_range_bounds_test.c 同款 fixture 写法（P19
 * 先例, 已被 ctest 验证可装载）; 本门扩展为多星 + 逐星等 + 可指定 DatabaseIdentifier。 */
static int write_xpsd(const char* path, const char* db_id, const char* mag_decl,
                      const uint16_t* mag_raws, int nstars) {
    char magattr[160];
    if (mag_decl) snprintf(magattr, sizeof(magattr), " magnitudeRange=\"%s\"", mag_decl);
    else magattr[0] = '\0';

    char xml[1024];
    size_t xml_len = 0, node_pos = 0, data_pos = 0;
    for (int pass = 0; pass < 5; pass++) {
        node_pos = 16 + xml_len;
        data_pos = node_pos + 48;
        int o = 0;
        o += snprintf(xml + o, sizeof(xml) - (size_t)o,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<XPSDVersion=\"1.0\">\n"
            "<Data%s position=\"%010llu\" compression=\"raw\" itemSize=\"%d\"/>\n"
            "<Statistics totalSources=\"%d\"/>\n"
            "<DatabaseIdentifier>%s</DatabaseIdentifier>\n"
            "<Tree projection=\"Equirectangular\" center=\"%017.12f,%017.12f\" "
            "rootPosition=\"%010llu\" nodeCount=\"1\"/>\n"
            "</XPSD>\n",
            magattr, (unsigned long long)data_pos, STRIDE, nstars, db_id,
            0.0, 0.0, (unsigned long long)node_pos);
        if ((size_t)o == xml_len) break;
        xml_len = (size_t)o;
    }

    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot create %s\n", path); exit(2); }
    uint32_t header_len = (uint32_t)xml_len, pad = 0;
    fwrite("XPSD0100", 1, 8, f);
    fwrite(&header_len, 4, 1, f);
    fwrite(&pad, 4, 1, f);
    fwrite(xml, 1, xml_len, f);

    uint8_t nd[48];
    memset(nd, 0, sizeof(nd));
    double x0 = 0.0, y0 = 0.0, x1 = 1.0, y1 = 1.0;
    memcpy(nd + 0, &x0, 8); memcpy(nd + 8, &y0, 8);
    memcpy(nd + 16, &x1, 8); memcpy(nd + 24, &y1, 8);
    uint64_t bo = 0x8000000000000000ULL;   /* leaf, block_offset 0（相对 data_position） */
    memcpy(nd + 32, &bo, 8);
    uint32_t bs = (uint32_t)(nstars * STRIDE);
    memcpy(nd + 40, &bs, 4); memcpy(nd + 44, &bs, 4);
    fwrite(nd, 1, 48, f);

    for (int i = 0; i < nstars; i++) {
        uint8_t rec[STRIDE];
        memset(rec, 0, sizeof(rec));
        uint32_t dx = (uint32_t)i, dy = 0;
        uint16_t m = mag_raws ? mag_raws[i] : 12000;
        memcpy(rec + 0, &dx, 4); memcpy(rec + 4, &dy, 4); memcpy(rec + 20, &m, 2);
        fwrite(rec, 1, STRIDE, f);
    }
    fclose(f);
    return 0;
}

static void mkdir_p(const char* path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* q = tmp + 1; *q; q++) {
        if (*q == '/') { *q = '\0'; ACS_TEST_MKDIR(tmp); *q = '/'; }
    }
    ACS_TEST_MKDIR(tmp);
}

/* 独立统计目录内 *.xpsd 条目数（门判据的左侧真值, 不依赖被测实现） */
static int count_xpsd_entries(const char* dir) {
    int n = 0;
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.xpsd", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        size_t len = strlen(fd.cFileName);
        if (len > 5 && strcmp(fd.cFileName + len - 5, ".xpsd") == 0) n++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len > 5 && strcmp(e->d_name + len - 5, ".xpsd") == 0) n++;
    }
    closedir(d);
#endif
    return n;
}

/* ===== 门判据（纯函数; --self-test 直接喂合成快照） ===== */
typedef struct {
    int create_ok;      /* gaia_client_create 是否返回非 NULL */
    int entry_count;    /* 目录内 *.xpsd 条目数（独立统计） */
    int file_count;     /* gaia_client_get_file_count */
    int fail_count;     /* gaia_client_get_file_load_fail_count */
} G1Snapshot;

static int g1_judge(const G1Snapshot* s, char* why, size_t n) {
    if (!s->create_ok) {
        snprintf(why, n, "create returned NULL (shard load failure -> fail-closed)");
        return 0;
    }
    if (s->entry_count <= 0) {
        snprintf(why, n, "no *.xpsd entry in directory (empty catalog)");
        return 0;
    }
    if (s->file_count <= 0) {
        snprintf(why, n, "empty catalog: file_count=0 (silent degradation refused)");
        return 0;
    }
    if (s->fail_count != 0) {
        snprintf(why, n, "%d shard(s) failed to load (partial catalog)", s->fail_count);
        return 0;
    }
    if (s->file_count != s->entry_count) {
        snprintf(why, n, "file_count=%d != xpsd_entries=%d (silent partial load)",
                 s->file_count, s->entry_count);
        return 0;
    }
    snprintf(why, n, "file_count=%d == xpsd_entries=%d, fail_count=0",
             s->file_count, s->entry_count);
    return 1;
}

/* 从真实目录取快照（门在真实实现上的判据输入） */
static int g1_snapshot(const char* dir, G1Snapshot* s) {
    memset(s, 0, sizeof(*s));
    s->entry_count = count_xpsd_entries(dir);
    GaiaClient* c = gaia_client_create(dir);
    s->create_ok = (c != NULL);
#ifndef GAIA_G1_BASELINE
    if (c) {
        s->file_count = gaia_client_get_file_count(c);
        s->fail_count = gaia_client_get_file_load_fail_count(c);
    } else {
        GaiaCreateDiagnostics d;
        memset(&d, 0, sizeof(d));
        if (gaia_client_get_last_create_diagnostics(&d) == 0) {
            s->file_count = d.file_count;
            s->fail_count = d.fail_count;
        }
    }
#else
    /* 基线（改前源码）无 fail 计数与诊断面: 只能用 file_count vs 条目数判据,
     * 而这恰是本缺陷的签名（file_count=2 却仍不可用）。 */
    if (c) s->file_count = gaia_client_get_file_count(c);
#endif
    gaia_client_destroy(c);
    return 0;
}

/* 跑一个用例并核对期望（expect_green: 1=必须绿, 0=必须红） */
static int run_case(const char* dir, const char* name, int expect_green) {
    G1Snapshot s;
    char why[256];
    g1_snapshot(dir, &s);
    int green = g1_judge(&s, why, sizeof(why));
    if (green) g_green++; else g_red++;
    if (green == expect_green) {
        ok("G-1 %-22s verdict=%s expected=%s [%s] (create_ok=%d entries=%d files=%d fail=%d)",
           name, green ? "GREEN" : "RED", expect_green ? "GREEN" : "RED", why,
           s.create_ok, s.entry_count, s.file_count, s.fail_count);
        return 1;
    }
    fail("G-1 %-22s verdict=%s expected=%s [%s] (create_ok=%d entries=%d files=%d fail=%d)",
         name, green ? "GREEN" : "RED", expect_green ? "GREEN" : "RED", why,
         s.create_ok, s.entry_count, s.file_count, s.fail_count);
    return 0;
}

static void write_garbage(const char* path, size_t nbytes) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot create %s\n", path); exit(2); }
    for (size_t i = 0; i < nbytes; i++) fputc((int)(i & 0xff), f);
    fclose(f);
}

/* ===== --self-test: 纯判据非退化性（不依赖被测实现） ===== */
static int judge_selftest(void) {
    struct { const char* name; G1Snapshot s; int want_green; } cases[] = {
        /* 健康: 2 条目 2 装载 0 失败 */
        { "healthy(2/2/0)",       { 1, 2, 2, 0 }, 1 },
        /* **修复前缺陷签名**: create 成功、条目=3、file_count=2、1 个装载失败
         * （这正是"只断言 file_count>0 不够"的反例） */
        { "prefix-defect(3/2/1)", { 1, 3, 2, 1 }, 0 },
        { "fail-but-files(2/2/1)",{ 1, 2, 2, 1 }, 0 },
        { "partial(4/3/0)",       { 1, 4, 3, 0 }, 0 },
        { "empty(0/0/0)",         { 1, 0, 0, 0 }, 0 },
        { "zero-files(2/0/0)",    { 1, 2, 0, 0 }, 0 },
        { "create-null(3/-/1)",   { 0, 3, 0, 1 }, 0 },
    };
    int ncase = (int)(sizeof(cases) / sizeof(cases[0]));
    int green = 0, red = 0, bad = 0;
    for (int i = 0; i < ncase; i++) {
        char why[256];
        int g = g1_judge(&cases[i].s, why, sizeof(why));
        if (g) green++; else red++;
        if (g != cases[i].want_green) {
            fail("self-test %-24s verdict=%s want=%s [%s]", cases[i].name,
                 g ? "GREEN" : "RED", cases[i].want_green ? "GREEN" : "RED", why);
            bad++;
        } else {
            ok("self-test %-24s verdict=%s [%s]", cases[i].name,
               g ? "GREEN" : "RED", why);
        }
    }
    if (bad == 0 && green > 0 && red > 0)
        ok("self-test 判据非退化: %d GREEN / %d RED（能红能绿, 且识别修复前缺陷签名）",
           green, red);
    else
        fail("self-test 判据退化: green=%d red=%d bad=%d", green, red, bad);
    return bad;
}

int main(int argc, char** argv) {
    int self_test = 0;
    const char* base = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--self-test") == 0) self_test = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("usage: gaia_shard_coverage_gate_test [workdir] [--self-test]\n"
                   "  G-1 shard coverage gate: file_count == #(*.xpsd) && fail_count == 0\n");
            return 0;
        } else base = argv[i];
    }
    if (!base) base = ".";
    mkdir_p(base);

    if (self_test) {
        puts("== G-1 判据自检（合成快照, 不依赖被测实现） ==");
        judge_selftest();
    }

    puts("== G-1 正例/负例（真实装载路径） ==");

    /* ---- A. 正例: 2 个星等区间不相交、均覆盖查询窗的 shard ----
     * A 片: magnitudeRange 8.00,12.00, 3 颗 m=8.5 (raw=10000)
     * B 片: magnitudeRange 12.00,18.00, 3 颗 m=13.5 (raw=15000)
     * 查询 (-1.5, 15.0): 两片都不得被 shard 剪枝谓词跳过 ⇒ 结果必须含 6 颗,
     * 且两个星等各 3 颗（**单 shard 恒真**的退化被此端点断言排除）。 */
    char pos[1024];
    snprintf(pos, sizeof(pos), "%s/g1_positive", base);
    mkdir_p(pos);
    {
        uint16_t mags_a[3] = { 10000, 10000, 10000 };
        uint16_t mags_b[3] = { 15000, 15000, 15000 };
        char p[1200];
        snprintf(p, sizeof(p), "%s/shard_a.xpsd", pos);
        write_xpsd(p, "AstroCS-G1-Test-GaiaDR3", "8.0000,12.0000", mags_a, 3);
        snprintf(p, sizeof(p), "%s/shard_b.xpsd", pos);
        write_xpsd(p, "AstroCS-G1-Test-GaiaDR3", "12.0000,18.0000", mags_b, 3);
    }
    int pos_ok = run_case(pos, "positive(2 shards)", 1);
    if (pos_ok) {
        GaiaClient* c = gaia_client_create(pos);
        if (!c) {
            fail("positive: create NULL（正例 fixture 必须可装载）");
        } else {
            GaiaStar* st = NULL; int n = 0;
            int rc = gaia_client_cone_search(c, 0.0, 0.0, 1.0, -1.5, 15.0, &st, &n);
            int n_bright = 0, n_faint = 0;
            for (int i = 0; i < n; i++) {
                if (st[i].magG < 10.0) n_bright++;
                else if (st[i].magG > 12.0) n_faint++;
            }
            if (rc == 0 && n == 6 && n_bright == 3 && n_faint == 3)
                ok("positive: 查询返回两片共 6 颗 (亮片 3 + 暗片 3) —— 覆盖判据非退化");
            else
                fail("positive: rc=%d n=%d bright=%d faint=%d（应 0/6/3/3）",
                     rc, n, n_bright, n_faint);
            free(st);
            /* 星等剪枝端点: mag_high=10 ⇒ 暗片 (12.00,18.00) 被整片剪枝 ⇒ 仅亮片 3 颗。
             * 若该片**装载失败**, 这里的星数会从 3 变 0 ⇒ 覆盖缺陷可见。 */
            st = NULL; n = 0;
            rc = gaia_client_cone_search(c, 0.0, 0.0, 1.0, -1.5, 10.0, &st, &n);
            if (rc == 0 && n == 3)
                ok("positive: mag_high=10 剪掉暗片后剩 3 颗（shard 覆盖可观测）");
            else
                fail("positive: mag_high=10 rc=%d n=%d（应 0/3）", rc, n);
            free(st);
            gaia_client_destroy(c);
        }
    }

    /* ---- B. 负例①: 3 条目含 1 个 8 字节垃圾 ⇒ 装载失败 ⇒ create NULL ---- */
    char neg1[1024];
    snprintf(neg1, sizeof(neg1), "%s/g1_corrupt", base);
    mkdir_p(neg1);
    {
        uint16_t mags[2] = { 10000, 10000 };
        char p[1200];
        snprintf(p, sizeof(p), "%s/shard_a.xpsd", neg1);
        write_xpsd(p, "AstroCS-G1-Test-GaiaDR3", "8.0000,12.0000", mags, 2);
        snprintf(p, sizeof(p), "%s/shard_b.xpsd", neg1);
        write_xpsd(p, "AstroCS-G1-Test-GaiaDR3", "12.0000,18.0000", mags, 2);
        snprintf(p, sizeof(p), "%s/shard_c_corrupt.xpsd", neg1);
        write_garbage(p, 8);          /* 8 字节垃圾（报告 §4 G-1 建议的负例构造） */
    }
    int neg1_ok = run_case(neg1, "negative-corrupt(3)", 0);
#ifndef GAIA_G1_BASELINE
    if (neg1_ok) {
        GaiaCreateDiagnostics d;
        memset(&d, 0, sizeof(d));
        int drc = gaia_client_get_last_create_diagnostics(&d);
        if (drc == 0 && d.entry_count == 3 && d.fail_count == 1 &&
            strstr(d.first_failed_path, "shard_c_corrupt.xpsd") != NULL &&
            d.first_failed_reason[0] != '\0')
            ok("negative-corrupt: 诊断 entry=%d failed=%d first=%s (%s)",
               d.entry_count, d.fail_count, d.first_failed_path, d.first_failed_reason);
        else
            fail("negative-corrupt: 诊断 rc=%d entry=%d failed=%d first=%s reason=%s",
                 drc, d.entry_count, d.fail_count, d.first_failed_path,
                 d.first_failed_reason);
        const char* err = gaia_client_get_last_create_error();
        if (err && strstr(err, "refusing partial catalog") != NULL)
            ok("negative-corrupt: 可见错误串含 'refusing partial catalog'");
        else
            fail("negative-corrupt: 可见错误串缺失 (%s)", err ? err : "(null)");
    }
#endif

    /* ---- C. 负例②: 魔数错误（尺寸正常）⇒ 同样必须 fail-closed ---- */
    char neg2[1024];
    snprintf(neg2, sizeof(neg2), "%s/g1_badmagic", base);
    mkdir_p(neg2);
    {
        uint16_t mags[2] = { 10000, 10000 };
        char p[1200];
        snprintf(p, sizeof(p), "%s/shard_a.xpsd", neg2);
        write_xpsd(p, "AstroCS-G1-Test-GaiaDR3", "8.0000,12.0000", mags, 2);
        snprintf(p, sizeof(p), "%s/shard_b.xpsd", neg2);
        write_xpsd(p, "AstroCS-G1-Test-GaiaDR3", "12.0000,18.0000", mags, 2);
        /* 尺寸与合法片相同, 仅魔数被破坏（改前: 静默跳过 ⇒ 参考星表静默少一片） */
        snprintf(p, sizeof(p), "%s/shard_b.xpsd", neg2);
        FILE* f = fopen(p, "r+b");
        if (!f) { fail("badmagic: 无法改写 %s", p); }
        else { fwrite("XPSD0000", 1, 8, f); fclose(f); }
    }
    run_case(neg2, "negative-badmagic(2)", 0);

    /* ---- D. 负例③: 空目录 ⇒ 空星表（M42: 不得静默降级解算） ---- */
    char neg3[1024];
    snprintf(neg3, sizeof(neg3), "%s/g1_empty", base);
    mkdir_p(neg3);
    run_case(neg3, "negative-empty(0)", 0);

    /* ---- E. 非退化总断言: 本次运行必须同时出现 GREEN 与 RED ---- */
    if (g_green > 0 && g_red > 0)
        ok("G-1 非退化: GREEN=%d RED=%d（判据既能绿也能红）", g_green, g_red);
    else
        fail("G-1 退化: GREEN=%d RED=%d（判据恒真或恒假）", g_green, g_red);

    fputs(g_fail ? "G1_SHARD_COVERAGE: FAIL\n" : "G1_SHARD_COVERAGE: PASS\n", stdout);
    printf("checks=%d failures=%d green=%d red=%d\n", g_checks, g_fail, g_green, g_red);
    return g_fail ? 1 : 0;
}
