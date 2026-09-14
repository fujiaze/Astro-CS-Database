/* P19-gaia 回归锁: XPSD 头 magnitudeRange 声明边界 (RQS 行动单 B2 / V5-N-03)
 *
 * 根因: lib/gaia_xpsd_client/src/gaia_client.c::load_xpsd_file 曾以裸 atof 解析
 * XML 头 magnitudeRange="low,high" 的两个分量, 仅校验"存在逗号", 随后
 * gaia_client_cone_search 用
 *     if (xf->has_magnitude_range && xf->magnitude_low > mag_high + 0.25) continue;
 * 把该声明当作**整文件 (shard) 剪枝谓词**。畸形声明 (low>high 颠倒 /
 * nan / 1e999->inf / 空串 / 超值域 / 尾随垃圾 "25.0abc") 因此可让整个 shard
 * 被静默跳过 —— 不报错、不改退出码、不进日志 (宪章 §17.6 静默数据损失)。
 * 同批 07eb229b 已为 spectrumCount 立 parse_bounded_int 四重校验, 本条同族。
 *
 * 修复契约:
 *   ① parse_bounded_double: 整 token 消费 + ERANGE + isfinite + 值域窗
 *      [-10,40] (出处: V5-N-03 建议 + 36 个真实 shard 实测包络);
 *   ② 任一校验失败 => **不置 has_magnitude_range** (放弃该 shard 剪枝,
 *      宁可不剪不可漏星) + stderr 可见告警 + client->magnitude_range_reject_count;
 *   ③ 合法输入解析值与旧 atof 逐位一致 (36 个真实 shard 范围断言)。
 *
 * 本 TU 共址 #include gaia_client.c 直接驱动 static load_xpsd_file 与公开查询。
 * 红/绿: 以 GAIA_P19_BASELINE 编译到改前源 -> 至少 inverted/inf/超值域/垃圾后缀
 *        四例断言败 (整 shard 被剪, 返回 0 而非 3); 编译到改后源 -> 全绿。
 * 基线编译时跳过仅新实现存在的计数/告警断言 (改前无该符号)。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef _WIN32
#include <direct.h>
#define ACS_TEST_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define ACS_TEST_MKDIR(p) mkdir((p), 0777)
#endif

#include "gaia_client.c"   /* 被测实现 (共址直接路径, 不定义 GAIA_ALLOC_TEST) */

#define INV_SCALE (1.0 / 1.8e9)
#define STRIDE    32

static int g_fail = 0;
static int g_checks = 0;

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

/* ===== 合成 XPSD: header + XML(可选 magnitudeRange) + 1 叶节点 + raw 星记录 ===== */
static int write_xpsd(const char* path, const char* mag_decl, int nstars, int mag_raw) {
    char magattr[160];
    if (mag_decl) snprintf(magattr, sizeof(magattr), " magnitudeRange=\"%s\"", mag_decl);
    else magattr[0] = '\0';

    char xml[1024];
    size_t xml_len = 0;
    size_t node_pos = 0, data_pos = 0;
    for (int pass = 0; pass < 5; pass++) {
        node_pos = 16 + xml_len;
        data_pos = node_pos + 48;
        int o = 0;
        o += snprintf(xml + o, sizeof(xml) - (size_t)o,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<XPSDVersion=\"1.0\">\n"
            "<Data%s position=\"%010llu\" compression=\"raw\" itemSize=\"%d\"/>\n"
            "<Statistics totalSources=\"%d\"/>\n"
            "<DatabaseIdentifier>AstroCS-P19-Test-GaiaDR3</DatabaseIdentifier>\n"
            "<Tree projection=\"Equirectangular\" center=\"%017.12f,%017.12f\" "
            "rootPosition=\"%010llu\" nodeCount=\"1\"/>\n"
            "</XPSD>\n",
            magattr, (unsigned long long)data_pos, STRIDE, nstars,
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
    uint64_t bo = 0x8000000000000000ULL;   /* leaf, block_offset 0 (相对 data_position) */
    memcpy(nd + 32, &bo, 8);
    uint32_t bs = (uint32_t)(nstars * STRIDE);
    memcpy(nd + 40, &bs, 4); memcpy(nd + 44, &bs, 4);
    fwrite(nd, 1, 48, f);

    for (int i = 0; i < nstars; i++) {
        uint8_t rec[STRIDE];
        memset(rec, 0, sizeof(rec));
        uint32_t dx = (uint32_t)i, dy = 0;
        uint16_t m = (uint16_t)mag_raw;
        memcpy(rec + 0, &dx, 4); memcpy(rec + 4, &dy, 4); memcpy(rec + 20, &m, 2);
        fwrite(rec, 1, STRIDE, f);
    }
    fclose(f);
    return 0;
}

/* 递归建目录 (跨平台; 与 GAIA_CAT_DATA 父目录是否存在解耦) */
static void mkdir_p(const char* path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* q = tmp + 1; *q; q++) {
        if (*q == '/') { *q = '\0'; ACS_TEST_MKDIR(tmp); *q = '/'; }
    }
    ACS_TEST_MKDIR(tmp);
}

static void make_case(const char* base, const char* name, const char* decl,
                      int nstars, int mag_raw, char* out_dir, size_t out_sz) {
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", base, name);
    mkdir_p(dir);
    char path[1200];
    snprintf(path, sizeof(path), "%s/g.xpsd", dir);
    write_xpsd(path, decl, nstars, mag_raw);
    snprintf(out_dir, out_sz, "%s", dir);
}

/* ===== 载入单文件, 回读解析结果 ===== */
static int load_probe(const char* dir, int* has, double* lo, double* hi, int* invalid) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/g.xpsd", dir);
    XPSDFileInternal xf;
    int rc = load_xpsd_file(&xf, path);
    if (rc != 0) return rc;
    *has = xf.has_magnitude_range;
    *lo = xf.magnitude_low;
    *hi = xf.magnitude_high;
#ifdef GAIA_P19_BASELINE
    *invalid = 0;   /* 改前无该字段 */
#else
    *invalid = xf.magnitude_range_invalid;
#endif
    close_xpsd_file(&xf);
    return 0;
}

/* ===== 端到端锥形查询 (0,0,r=1deg), 记录全在 (0,0) 附近 ===== */
static int query_dir(const char* dir, double mag_high, GaiaStar** out, int* outn,
                     int* reject_count) {
    GaiaClient* c = gaia_client_create(dir);
    if (!c) return -1;
    *out = NULL; *outn = 0;
    int rc = gaia_client_cone_search(c, 0.0, 0.0, 1.0, -1.5, mag_high, out, outn);
#ifdef GAIA_P19_BASELINE
    if (reject_count) *reject_count = 0;
#else
    if (reject_count) *reject_count = gaia_client_get_magnitude_range_reject_count(c);
#endif
    gaia_client_destroy(c);
    return rc;
}

static int star_bitwise_equal(const GaiaStar* a, int an, const GaiaStar* b, int bn) {
    if (an != bn) return 0;
    if (an == 0) return 1;
    return memcmp(a, b, (size_t)an * sizeof(GaiaStar)) == 0;
}

/* 36 个真实 shard 的 magnitudeRange (P1-gaia 登记 run/perf-fix/P1-gaia/evidence/
 * shard_magranges.tsv; DR3 16 + DR3SP 20), 全部必须合法且解析值与 atof 逐位一致。 */
static const char* kShardRanges[] = {
    "-2.0000,16.5900", "16.5900,17.6100", "17.6100,18.2400", "18.2400,18.6900",
    "18.6900,19.0400", "19.0400,19.3300", "19.3300,19.5800", "19.5800,19.8000",
    "19.8000,20.0000", "20.0000,20.1900", "20.1900,20.3600", "20.3600,20.5200",
    "20.5200,20.6800", "20.6800,20.8400", "20.8400,21.0500", "21.0500,25.5900",
    "-2.0000,13.6200", "13.6200,14.4900", "14.4900,15.0200", "15.0200,15.4000",
    "15.4000,15.7000", "15.7000,15.9500", "15.9500,16.1600", "16.1600,16.3500",
    "16.3500,16.5200", "16.5200,16.6700", "16.6700,16.8100", "16.8100,16.9400",
    "16.9400,17.0600", "17.0600,17.1700", "17.1700,17.2700", "17.2700,17.3700",
    "17.3700,17.4600", "17.4600,17.5500", "17.5500,17.6400", "17.6400,25.5900",
};
#define NSHARD ((int)(sizeof(kShardRanges) / sizeof(kShardRanges[0])))

static void copy_pair(const char* decl, char* lo_s, size_t n, char* hi_s, size_t m) {
    const char* c = strchr(decl, ',');
    size_t l = (size_t)(c - decl);
    if (l >= n) l = n - 1;
    memcpy(lo_s, decl, l); lo_s[l] = '\0';
    snprintf(hi_s, m, "%s", c + 1);
}

int main(int argc, char** argv) {
    const char* base = argc > 1 ? argv[1] : ".";
    mkdir_p(base);

    /* ---- A. 合法: 36 个真实 shard 范围解析值与旧 atof 逐位一致 ---- */
    int shard_ok = 0;
    for (int i = 0; i < NSHARD; i++) {
        char dir[1024];
        make_case(base, "shard", kShardRanges[i], 1, 12000, dir, sizeof(dir));
        int has = 0, invalid = 0; double lo = 0, hi = 0;
        if (load_probe(dir, &has, &lo, &hi, &invalid) != 0) { fail("shard %d load", i); continue; }
        char ls[64], hs[64];
        copy_pair(kShardRanges[i], ls, sizeof(ls), hs, sizeof(hs));
        double ref_lo = atof(ls), ref_hi = atof(hs);
        if (!has) { fail("shard %d (%s) has_magnitude_range=0", i, kShardRanges[i]); continue; }
        if (memcmp(&lo, &ref_lo, sizeof(double)) != 0 ||
            memcmp(&hi, &ref_hi, sizeof(double)) != 0) {
            fail("shard %d (%s) BITWISE differs from atof (%.17g,%.17g)",
                 i, kShardRanges[i], lo, hi);
            continue;
        }
        if (invalid) { fail("shard %d flagged invalid", i); continue; }
        shard_ok++;
    }
    if (shard_ok == NSHARD) ok("legal: %d/36 production shard ranges parsed bitwise == atof", shard_ok);
    else fail("legal: only %d/%d production shard ranges bitwise-equal", shard_ok, NSHARD);

    /* 窗口边界 (含端点) 合法 */
    {
        static const char* wl[] = { "-10,40", "-10,25.59", "0,0", "12.5,12.5", "-2.00,16.59" };
        int wok = 0;
        for (int i = 0; i < 5; i++) {
            char dir[1024];
            make_case(base, "win", wl[i], 1, 12000, dir, sizeof(dir));
            int has = 0, invalid = 0; double lo = 0, hi = 0;
            if (load_probe(dir, &has, &lo, &hi, &invalid) == 0 && has && !invalid) wok++;
            else fail("window legal case %s rejected", wl[i]);
        }
        if (wok == 5) ok("legal: window edges/lo==hi accepted (5/5)");
    }

    /* ---- B. 非法声明: 一律不置 has_magnitude_range (放弃剪枝), 不崩 ---- */
    static const char* kIllegal[] = {
        "25.0,10.0",        /* low>high 颠倒 (红: 旧实现剪枝漏星) */
        "1e999,30.0",       /* low -> +inf (红) */
        "0.0,1e999",        /* high -> +inf */
        "-1e999,30.0",      /* low -> -inf (越窗) */
        "nan,30.0",         /* NaN low */
        "0.0,nan",          /* NaN high */
        ",30.0",            /* 空 low */
        "0.0,",             /* 空 high */
        ",",                /* 两端空 */
        "",                 /* 空串 (属性在但值为空) */
        "100.0,200.0",      /* 超值域上界 (红) */
        "-20.0,-15.0",      /* 超值域下界 */
        "40.0000001,41.0",  /* 窗上界外 */
        "-10.0000001,-9.0", /* 窗下界外 */
        "12.5abc,30.0",     /* low 尾随垃圾 */
        "12.5,30.0xyz",     /* high 尾随垃圾 */
        "25.0abc,30.0",     /* low 尾随垃圾且旧实现会剪枝 (红) */
        "12.5;30.0",        /* 无逗号 */
        "abc,def",          /* 全非数字 */
        "-0.0,-1.0",        /* 颠倒 */
    };
#define NILLEGAL ((int)(sizeof(kIllegal) / sizeof(kIllegal[0])))
    int ill_ok = 0;
    for (int i = 0; i < NILLEGAL; i++) {
        char dir[1024];
        make_case(base, "bad", kIllegal[i], 3, 12000, dir, sizeof(dir));
        int has = 0, invalid = 0; double lo = 0, hi = 0;
        int rc = load_probe(dir, &has, &lo, &hi, &invalid);
        if (rc != 0) { fail("illegal %-16s load_xpsd_file rc=%d (want 0)", kIllegal[i], rc); continue; }
#ifndef GAIA_P19_BASELINE
        if (!invalid) { fail("illegal %-16s magnitude_range_invalid=0 (no warn/count)", kIllegal[i]); continue; }
#endif
        if (has) { fail("illegal %-16s has_magnitude_range=1 (would prune!)", kIllegal[i]); continue; }
        ill_ok++;
    }
    if (ill_ok == NILLEGAL) ok("illegal: %d/%d malformed declarations -> has_magnitude_range=0, no crash",
                               ill_ok, NILLEGAL);
    else fail("illegal: only %d/%d rejected", ill_ok, NILLEGAL);

    /* ---- C. 端到端: 畸形声明不得导致整 shard 静默漏星 (红->绿核心) ---- */
    struct { const char* name; const char* decl; int mag_raw; int want; } cases[] = {
        { "c_inverted",  "25.0,10.0",   12000, 3 },  /* 旧: 剪枝 => 0 (漏星) */
        { "c_inf",       "1e999,30.0",  12000, 3 },  /* 旧: 剪枝 => 0 */
        { "c_oor",       "100.0,200.0", 12000, 3 },  /* 旧: 剪枝 => 0 */
        { "c_garbage",   "25.0abc,30.0",12000, 3 },  /* 旧: atof=25 => 剪枝 => 0 */
        { "c_empty",     "",            12000, 3 },  /* 旧: has=0 => 3 (无告警) */
        { "c_nan",       "nan,30.0",    12000, 3 },  /* 旧: nan 比较恒假 => 3 */
        { "c_legal_lo",  "0.0,20.0",    12000, 3 },  /* 合法, 不剪, 正常命中 */
        { "c_none",      NULL,          12000, 3 },  /* 无声明 (参照) */
    };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        char dir[1024];
        make_case(base, cases[i].name, cases[i].decl, 3, cases[i].mag_raw, dir, sizeof(dir));
        GaiaStar* st = NULL; int n = 0, rej = 0;
        int rc = query_dir(dir, 13.0, &st, &n, &rej);
        if (rc != 0) { fail("%s query rc=%d", cases[i].name, rc); free(st); continue; }
        if (n != cases[i].want) {
#ifdef GAIA_P19_BASELINE
            fail("%s decl=%-14s -> %d stars, want %d (SILENT STAR LOSS)", cases[i].name,
                 cases[i].decl ? cases[i].decl : "(none)", n, cases[i].want);
#else
            fail("%s decl=%-14s -> %d stars, want %d", cases[i].name,
                 cases[i].decl ? cases[i].decl : "(none)", n, cases[i].want);
#endif
        } else {
            ok("%s decl=%-14s -> %d stars", cases[i].name,
               cases[i].decl ? cases[i].decl : "(none)", n);
        }
        free(st);
    }

    /* ---- D. 合法剪枝: 声明有效时剪枝行为不变, 且与"不剪枝"逐位一致 ---- */
    {
        /* 记录 m=20.5 (raw=22000), 查询上限 13.0: 声明 "20.0,30.0" 合法且
         * low=20>13.25 => 该 shard 可安全剪枝 (记录本就被 mag 窗过滤)。 */
        char d_legal[1024], d_none[1024];
        make_case(base, "prune_legal", "20.0,30.0", 3, 22000, d_legal, sizeof(d_legal));
        make_case(base, "prune_none",  NULL,        3, 22000, d_none,  sizeof(d_none));
        int has = 0, invalid = 0; double lo = 0, hi = 0;
        load_probe(d_legal, &has, &lo, &hi, &invalid);
        if (has && lo == 20.0 && hi == 30.0)
            ok("legal prune: declaration parsed (has=1 low=20 high=30) => predicate prunes");
        else
            fail("legal prune: has=%d lo=%.6g hi=%.6g (want 1,20,30)", has, lo, hi);

        GaiaStar* a = NULL; int an = 0;
        GaiaStar* b = NULL; int bn = 0;
        query_dir(d_legal, 13.0, &a, &an, NULL);
        query_dir(d_none,  13.0, &b, &bn, NULL);
        if (star_bitwise_equal(a, an, b, bn))
            ok("legal prune: pruned shard result bitwise == unpruned shard result (%d stars)", an);
        else
            fail("legal prune: bitwise mismatch pruned=%d unpruned=%d", an, bn);
        free(a); free(b);
    }

    /* ---- E. 合法输入逐位不变: 合法声明 vs 无声明, 同数据同查询 ---- */
    {
        char d_legal[1024], d_none[1024];
        make_case(base, "eq_legal", "0.0,20.0", 3, 12000, d_legal, sizeof(d_legal));
        make_case(base, "eq_none",  NULL,       3, 12000, d_none,  sizeof(d_none));
        GaiaStar* a = NULL; int an = 0;
        GaiaStar* b = NULL; int bn = 0;
        int rca = query_dir(d_legal, 13.0, &a, &an, NULL);
        int rcb = query_dir(d_none,  13.0, &b, &bn, NULL);
        if (rca == 0 && rcb == 0 && star_bitwise_equal(a, an, b, bn))
            ok("equivalence: legal declaration result bitwise == no-declaration (%d stars, %zu bytes)",
               an, (size_t)an * sizeof(GaiaStar));
        else
            fail("equivalence: rc=%d/%d count=%d/%d bitwise=%d", rca, rcb, an, bn,
                 star_bitwise_equal(a, an, b, bn));
        free(a); free(b);
    }

#ifndef GAIA_P19_BASELINE
    /* ---- F. 可见计数: 非法声明文件被计数 ---- */
    {
        char dir[1024];
        make_case(base, "count_probe", "25.0,10.0", 3, 12000, dir, sizeof(dir));
        GaiaStar* st = NULL; int n = 0, rej = 0;
        query_dir(dir, 13.0, &st, &n, &rej);
        free(st);
        if (rej == 1) ok("visible count: reject_count=1 for one malformed declaration");
        else fail("visible count: reject_count=%d want 1", rej);

        char dir2[1024];
        make_case(base, "count_ok", "10.0,20.0", 3, 12000, dir2, sizeof(dir2));
        int n2 = 0; int rej2 = 0;
        query_dir(dir2, 13.0, &st, &n2, &rej2);
        free(st);
        if (rej2 == 0) ok("visible count: reject_count=0 for legal declaration");
        else fail("visible count: legal declaration counted as reject (%d)", rej2);
    }

    /* ---- G. 可见告警: stderr 必须出现拒绝告警 (freopen 捕获, 跨平台) ---- */
    {
        char dir[1024];
        make_case(base, "warn_probe", "nan,30.0", 1, 12000, dir, sizeof(dir));
        char warnpath[1200];
        snprintf(warnpath, sizeof(warnpath), "%s/../p19_stderr.txt", dir);
        fflush(stdout); fflush(stderr);
        if (!freopen(warnpath, "w", stderr)) {
            fail("warning capture: freopen failed");
        } else {
            char path[1200];
            snprintf(path, sizeof(path), "%s/g.xpsd", dir);
            XPSDFileInternal xf;
            int rc = load_xpsd_file(&xf, path);
            if (rc == 0) close_xpsd_file(&xf);
            fflush(stderr);
            FILE* wf = fopen(warnpath, "r");
            char buf[4096];
            size_t got = wf ? fread(buf, 1, sizeof(buf) - 1, wf) : 0;
            if (wf) fclose(wf);
            buf[got] = '\0';
            if (strstr(buf, "magnitudeRange declaration rejected") != NULL &&
                strstr(buf, "pruning disabled") != NULL)
                ok("visible warning: stderr carries rejection message (%zu bytes)", got);
            else
                fail("visible warning: marker missing in stderr capture (%zu bytes)", got);
        }
    }
#endif

    fputs(g_fail ? "P19_GAIA_MAGRANGE_BOUNDS: FAIL\n" : "P19_GAIA_MAGRANGE_BOUNDS: PASS\n", stdout);
    printf("checks=%d failures=%d\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
