/* CAT-GAIA-TEST (TEST-GAIA-001) — XPSD 合成 fixture 生成器（固定 seed）。
 *
 * 依据（冻结，不得事后改）：
 *   - MODULE_MIGRATION_TEMPLATE.md "<prefix>-TEST"：fixture 由固定 seed+参数
 *     生成，不提交大二进制；覆盖极区、RA 环绕、mag 边界、两种压缩。
 *   - docs/algorithms/GAIA_QUERY.md §5 TEST-GAIA-DESIGN-001：合成 fixture
 *     ≥2 文件 × 多树，LZ4 与 zlib+shuffle 两种压缩，含/不含光谱记录，
 *     极区星、RA≈0/360 环绕星、mag 边界星。
 *   - 记录/节点布局与解码常量以 lib/gaia_xpsd_client/src/gaia_client.c
 *     当前 HEAD 为基线独立核对（48B 节点、XPSD0100、zlib+sh 列 shuffle、
 *     384B SP 记录、2µas/dx、10µas/dra、m=raw*0.001-1.5）；测试断言按源码
 *     区分 BP/RP 语义：DR3 文件恒 0 sentinel（DATA-GAIA-001），SP 文件
 *     =raw*0.001-1.5（gaia_client.c:1600-1607，has_spectrum 域）。
 *
 * 产物（<out_dir>/ 下，全部运行时生成，不入库）：
 *   gaia_sp.xpsd    — GaiaDR3SP（含 343 点光谱），zlib+shuffle，4 树
 *                     （3×Equirectangular 含 RA 环绕树 + 1×AzimuthalEquidistant 极冠）
 *   gaia_dr3.xpsd   — GaiaDR3（无光谱），lz4 压缩块 + 原样块，2 树
 *   manifest.ndjson — 参考真值（每星量化精确反演 ra/dec/mag + 光谱字节）
 *
 * 精确反演（manifest 值）由本文件独立公式计算（不调用被测 C 符号）；
 * oracle (tests/oracle/gaia_oracle.py) 再用 Python 独立实现复算与比对。
 * 编译为库使用：-DGAIA_FIXTURE_GEN_AS_LIBRARY（gaia_perf_smoke include 用）。
 */
#define _GNU_SOURCE 1   /* strdup（c99 下未声明；必须在 libc 头之前） */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <zlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEG2RAD (M_PI / 180.0)
#define RAD2DEG (180.0 / M_PI)

/* 解码常量（GAIA_QUERY.md §2.2/2.3，与被测实现同一有理式） */
#define INV_SCALE (1.0 / 1.8e9)      /* 1 LSB = 2 µas（位置） */
#define INV_DRA   (1.0 / 3.6e8)      /* 1 LSB = 10 µas（dra 修正） */
#define WL        343                /* 光谱点数 */
#define STRIDE_SP   (40 + (WL + (WL & 1)))  /* 384 */
#define STRIDE_NOSP 32

#define FIX_SEED 20260905104215ULL
#define FIXTURE_MAX_STARS 1024

/* ===== 固定 seed RNG（splitmix64，跨平台可复现） ===== */
static uint64_t g_rng = FIX_SEED;
static uint64_t rng_next(void) {
    uint64_t z = (g_rng += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static double rng_uniform(double lo, double hi) {
    return lo + (hi - lo) * ((double)(rng_next() >> 11) * (1.0 / 9007199254740992.0));
}

/* ===== fixture 星 ===== */
typedef struct {
    const char *id;
    const char *file;
    int tree;
    const char *proj;            /* "Equirectangular" | "AzimuthalEquidistant" */
    double center_ra, center_dec;
    double leaf_x0, leaf_y0;     /* 叶节点平面 bbox 基点（度） */
    uint32_t dx, dy;             /* 相对 leaf 基点的量化偏移 */
    uint16_t mag_raw;
    uint16_t bp_raw, rp_raw;   /* 仅 SP 记录写入；DR3 记录保持 0 sentinel */
    int16_t dra_raw;
    uint8_t spectrum[WL];
    int has_spec;
    /* 量化精确反演（参考真值，独立公式） */
    double ra, dec, mag;
} FixStar;

static FixStar g_stars[FIXTURE_MAX_STARS];
static int g_nstars = 0;

/* AE 独立反投影（标准 AzimuthalEquidistant，投影中心=极点；与 ALG §2.3 同义
 * 的独立实现——不 #include / 不调用 gaia_client.c 任何符号） */
static void fix_unproject_ae(double x, double y, double cra, double cdec,
                             double *ra, double *dec) {
    double xr = x * DEG2RAD, yr = y * DEG2RAD;
    double r = sqrt(xr * xr + yr * yr);
    if (r < 1e-15) {
        *ra = cra;
        *dec = cdec;
        return;
    }
    double c = asin(cos(r));
    if (cdec < 0) c = -c;
    *ra = cra + atan2(xr * sin(r) / r, yr * sin(r) / r) * RAD2DEG;
    *dec = c * RAD2DEG;
    if (*ra < 0) *ra += 360.0;
    if (*ra >= 360.0) *ra -= 360.0;
}

/* 把一颗星写入 g_stars：先量化，再独立反演为参考真值 */
static void fix_add(const char *id, const char *file, int tree, const char *proj,
                    double center_ra, double center_dec,
                    double leaf_x0, double leaf_y0,
                    double ra, double dec, uint16_t mag_raw, int16_t dra_raw,
                    int spec_mode /*0=无,1=固定 pattern,2=伪随机*/) {
    FixStar *s = &g_stars[g_nstars];
    memset(s, 0, sizeof(*s));
    s->id = strdup(id);          /* 栈缓冲被 main 复用，必须拷贝 */
    if (!s->id) { fprintf(stderr, "fixture gen: OOM strdup star id (%s)\n", id); exit(2); }
    s->file = file; s->tree = tree; s->proj = proj;
    s->center_ra = center_ra; s->center_dec = center_dec;
    s->leaf_x0 = leaf_x0; s->leaf_y0 = leaf_y0;
    s->mag_raw = mag_raw; s->dra_raw = dra_raw;
    s->bp_raw = (uint16_t)(mag_raw > 15000 ? mag_raw - 500 : mag_raw + 700);
    s->rp_raw = (uint16_t)(mag_raw > 15000 ? mag_raw - 800 : mag_raw + 1200);
    s->has_spec = (spec_mode != 0);

    double x, y; /* 平面坐标（度）：EQ=相对 center 的 (x,y)；AE=极平面 */
    if (strcmp(proj, "Equirectangular") == 0) {
        x = (ra - center_ra);
        y = dec;
    } else {
        double theta = 90.0 - dec;            /* 北极冠余纬 */
        double phi = (ra - center_ra) * DEG2RAD;
        x = theta * sin(phi);
        y = theta * cos(phi);
    }
    s->dx = (uint32_t)llround((x - leaf_x0) / INV_SCALE);
    s->dy = (uint32_t)llround((y - leaf_y0) / INV_SCALE);

    /* 独立精确反演（与量化取整同参数，保证参考值=被测可复现值） */
    double xq = leaf_x0 + (double)s->dx * INV_SCALE;
    double yq = leaf_y0 + (double)s->dy * INV_SCALE;
    if (strcmp(proj, "Equirectangular") == 0) {
        s->ra = center_ra + xq + (double)dra_raw * INV_DRA;
        s->dec = yq;
        if (s->ra < 0) s->ra += 360.0;
        if (s->ra >= 360.0) s->ra -= 360.0;
    } else {
        fix_unproject_ae(xq, yq, center_ra, center_dec, &s->ra, &s->dec);
    }
    s->mag = (double)mag_raw * 0.001 - 1.5;

    for (int j = 0; j < WL; j++) {
        /* 波形 pattern（j=0 处 255*85-1=21674 峰值，j≥1 递减）：
         * 极冠 8 颗星幅度各异（oracle/单测遍历比对用） */
        int amp = 21674 - 7 * g_nstars;
        for (int j = 0; j < WL; j++)
            s->spectrum[j] = (uint8_t)(1 + (((amp - j * 62) % 255) + 255) % 255);
    }
    g_nstars++;
}

/* ===== 最小 LZ4 block 压缩（greedy hash；产出标准 LZ4 block 序列，
 * 由被测侧 lz4_decompress 独立解码——实现隔离：本压缩器不复制其逻辑） ===== */
static size_t fix_lz4_hash(uint32_t v) {
    return (v * 2654435761U) >> 20; /* 12-bit */
}
static uint32_t fix_load32(const uint8_t *p) {
    uint32_t v; memcpy(&v, p, 4); return v; /* LE host（x86/ARM 小端构建面） */
}
static size_t lz4_compress_min(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    uint16_t table[1u << 12];
    memset(table, 0xFF, sizeof(table));
    size_t ip = 0, anchor = 0, op = 0;
    uint8_t token;
    while (ip < n) {
        /* 找匹配 */
        size_t mlen = 0, moff = 0;
        if (ip + 4 <= n) {
            size_t h = fix_lz4_hash(fix_load32(in + ip));
            size_t cand = table[h];
            if (cand != 0xFFFFU && cand < ip && ip - cand <= 65535 &&
                fix_load32(in + cand) == fix_load32(in + ip)) {
                size_t maxm = n - ip;
                mlen = 4;
                while (mlen < maxm && in[cand + mlen] == in[ip + mlen] && mlen < 65536u + 15u)
                    mlen++;
                moff = ip - cand;
            }
            table[h] = (uint16_t)ip;
        }
        if (mlen < 4) { ip++; continue; }
        /* 发射序列：literal [anchor,ip) + match */
        size_t lit = ip - anchor;
        size_t llc = lit >= 15 ? 15 : lit;
        size_t mlc = (mlen - 4) >= 15 ? 15 : (mlen - 4);
        if (op + 1 + lit + ((lit >= 15) ? (lit - 15 + 254) / 255 : 0) + 2 +
            ((mlen - 4) >= 15 ? ((mlen - 4) - 15 + 254) / 255 : 0) > cap)
            return 0;
        token = (uint8_t)((llc << 4) | mlc);
        out[op++] = token;
        if (lit >= 15) {
            size_t r = lit - 15;
            while (r >= 255) { out[op++] = 255; r -= 255; }
            out[op++] = (uint8_t)r;
        }
        memcpy(out + op, in + anchor, lit);
        op += lit;
        uint16_t off = (uint16_t)moff;
        out[op++] = (uint8_t)(off & 0xFF);
        out[op++] = (uint8_t)(off >> 8);
        if (mlen - 4 >= 15) {
            size_t r = (mlen - 4) - 15;
            while (r >= 255) { out[op++] = 255; r -= 255; }
            out[op++] = (uint8_t)r;
        }
        ip += mlen;
        anchor = ip;
        /* 维护哈希（匹配覆盖区间内的位置） */
        for (size_t k = ip - mlen + 1; k + 4 <= ip && k < ip; k++)
            table[fix_lz4_hash(fix_load32(in + k))] = (uint16_t)k;
    }
    /* 尾部 literals */
    if (anchor < n) {
        size_t lit = n - anchor;
        size_t llc = lit >= 15 ? 15 : lit;
        if (op + 1 + lit + ((lit >= 15) ? (lit - 15 + 254) / 255 : 0) > cap)
            return 0;
        out[op++] = (uint8_t)(llc << 4);
        if (lit >= 15) {
            size_t r = lit - 15;
            while (r >= 255) { out[op++] = 255; r -= 255; }
            out[op++] = (uint8_t)r;
        }
        memcpy(out + op, in + anchor, lit);
        op += lit;
    }
    return op;
}

/* ===== 列 shuffle（byte_unshuffle 的精确逆变换） ===== */
static void fix_shuffle(uint8_t *data, size_t len, int item) {
    if (item <= 1 || len == 0) return;
    size_t n = len / (size_t)item;
    if (n == 0) return;
    uint8_t *tmp = (uint8_t *)malloc(len);
    if (!tmp) { fprintf(stderr, "fixture gen: OOM shuffle\n"); exit(2); }
    for (int i = 0; i < item; i++)
        for (size_t j = 0; j < n; j++)
            tmp[(size_t)i * n + j] = data[j * (size_t)item + (size_t)i];
    memcpy(data, tmp, len);
    free(tmp);
}

/* ===== 记录组装 ===== */
#define WL_LIMIT (WL * 85 - 1)   /* 每点 flux 上限（byte=255） */
static void fix_pack_record(const FixStar *s, int stride, uint8_t *rec) {
    memset(rec, 0, (size_t)stride);
    memcpy(rec + 0, &s->dx, 4);
    memcpy(rec + 4, &s->dy, 4);
    memcpy(rec + 20, &s->mag_raw, 2);
    if (stride == STRIDE_SP) {
        memcpy(rec + 22, &s->bp_raw, 2);
        memcpy(rec + 24, &s->rp_raw, 2);
    } else {
        uint16_t bp = 0, rp = 0; /* DR3 数据 BP/RP=0 sentinel（DATA-GAIA-001） */
        memcpy(rec + 22, &bp, 2);
        memcpy(rec + 24, &rp, 2);
    }
    memcpy(rec + 26, &s->dra_raw, 2);
    if (stride == STRIDE_SP) {
        float fmin = -1.0f, fmul = 85.0f; /* flux = byte*85 - 1 ∈ [0,21665] */
        memcpy(rec + 32, &fmin, 4);
        memcpy(rec + 36, &fmul, 4);
        memcpy(rec + 40, s->spectrum, WL);
    }
}

/* ===== 树/块描述 ===== */
typedef struct {
    const char *projection;
    double center_ra, center_dec;
    double leaf_x0, leaf_y0, leaf_x1, leaf_y1; /* 平面 bbox（度） */
    int star_begin, star_end;                  /* [begin,end) 的本树星 → 单叶块 */
    int use_lz4;                               /* 0=zlib；1=lz4；2=原样块 */
} FixTree;

/* ===== 单文件写出 ===== */
static int fix_write_xpsd(const char *dir, const char *name, const char *db_id,
                          const char *compression, int item_size, int has_spec,
                          const FixTree *trees, int ntree, char *err, size_t errsz) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", dir, name);

    /* 星按树分组装块（缓冲动态分配，truncate fixture 单块可达 MB 级） */
    uint64_t block_off[16];
    uint32_t block_size[16], comp_size[16];
    uint8_t *rawbuf[16] = {0}, *compbuf[16] = {0};
    size_t data_bytes = 0;

    for (int t = 0; t < ntree; t++) {
        int ns = trees[t].star_end - trees[t].star_begin;
        int stride = has_spec ? STRIDE_SP : STRIDE_NOSP;
        size_t raw_len = (size_t)ns * (size_t)stride;
        uint8_t *raw = (uint8_t *)malloc(raw_len);
        uint8_t *comp = (uint8_t *)malloc(raw_len + 4096); /* 压缩不赢→原样 */
        rawbuf[t] = raw; compbuf[t] = comp;
        if (!raw || !comp) { snprintf(err, errsz, "OOM block buffers"); return -1; }
        for (int i = 0; i < ns; i++)
            fix_pack_record(&g_stars[trees[t].star_begin + i], stride,
                            raw + (size_t)i * (size_t)stride);
        int shuffle = (strstr(compression, "+sh") != NULL);
        if (shuffle) fix_shuffle(raw, raw_len, item_size);

        size_t clen = 0;
        if (strstr(compression, "lz4") != NULL) {
            clen = lz4_compress_min(raw, raw_len, comp, raw_len + 4096);
            if (clen == 0 || clen >= raw_len) { /* 压缩不赢 → 原样 */ clen = 0; }
        } else {
            uLongf bound = compressBound((uLong)raw_len);
            if (compress2(comp, &bound, raw, (uLong)raw_len, Z_BEST_COMPRESSION) != Z_OK) {
                snprintf(err, errsz, "zlib compress failed");
                return -1;
            }
            clen = (size_t)bound;
            if (clen >= raw_len) clen = 0;
        }
        block_off[t] = data_bytes;
        block_size[t] = (uint32_t)raw_len;
        comp_size[t] = clen ? (uint32_t)clen : (uint32_t)raw_len;
        data_bytes += comp_size[t];
    }

    /* XML（宽度固定字段 → 长度两遍确定） */
    int file_nstars = 0;
    for (int t = 0; t < ntree; t++)
        file_nstars += trees[t].star_end - trees[t].star_begin;
    size_t node_base = 16; /* 先按 0 长度 XML 试算，再迭代一次收敛 */
    char xml[4096];
    size_t xml_len = 0;
    for (int pass = 0; pass < 3; pass++) {
        size_t dp = 16 + xml_len + (size_t)ntree * 48;
        int o = 0;
        o += snprintf(xml + o, sizeof(xml) - (size_t)o,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<XPSDVersion=\"1.0\">\n"
            "<Data magnitudeRange=\"10.0,20.0\" position=\"%010llu\" compression=\"%s\" "
            "itemSize=\"%d\"%s%s/>\n",
            (unsigned long long)dp, compression, item_size,
            has_spec ? " parameters=\"spectrumStart=336,spectrumStep=1,"
                       "spectrumCount=343,spectrumBits=8\"" : "",
            has_spec ? "" : "");
        o += snprintf(xml + o, sizeof(xml) - (size_t)o,
            "<Statistics totalSources=\"%d\"/>\n", file_nstars);
        o += snprintf(xml + o, sizeof(xml) - (size_t)o,
            "<DatabaseIdentifier>AstroCS-Test-Fixture-%s-Seed%llu</DatabaseIdentifier>\n",
            db_id, (unsigned long long)FIX_SEED);
        for (int t = 0; t < ntree; t++)
            o += snprintf(xml + o, sizeof(xml) - (size_t)o,
                "<Tree projection=\"%s\" center=\"%017.12f,%017.12f\" "
                "rootPosition=\"%010llu\" nodeCount=\"1\"/>\n",
                trees[t].projection, trees[t].center_ra, trees[t].center_dec,
                (unsigned long long)(16 + xml_len + (size_t)t * 48));
        o += snprintf(xml + o, sizeof(xml) - (size_t)o, "</XPSD>\n");
        if ((size_t)o == xml_len) break;
        xml_len = (size_t)o;
        (void)node_base;
    }

    FILE *f = fopen(path, "wb");
    if (!f) { snprintf(err, errsz, "open %s failed", path); return -1; }

    uint32_t header_len = (uint32_t)xml_len;
    fwrite("XPSD0100", 1, 8, f);
    fwrite(&header_len, 4, 1, f);
    uint32_t pad = 0;
    fwrite(&pad, 4, 1, f);
    fwrite(xml, 1, xml_len, f);
    /* 节点区 */
    for (int t = 0; t < ntree; t++) {
        uint8_t nd[48];
        double x0 = trees[t].leaf_x0, y0 = trees[t].leaf_y0;
        double x1 = trees[t].leaf_x1, y1 = trees[t].leaf_y1;
        memcpy(nd + 0, &x0, 8);
        memcpy(nd + 8, &y0, 8);
        memcpy(nd + 16, &x1, 8);
        memcpy(nd + 24, &y1, 8);
        uint64_t bo = 0x8000000000000000ULL | (uint64_t)block_off[t];
        memcpy(nd + 32, &bo, 8);
        memcpy(nd + 40, &block_size[t], 4);
        memcpy(nd + 44, &comp_size[t], 4);
        fwrite(nd, 1, 48, f);
    }
    /* 数据块 */
    for (int t = 0; t < ntree; t++) {
        if (comp_size[t] == block_size[t])
            fwrite(rawbuf[t], 1, block_size[t], f);
        else
            fwrite(compbuf[t], 1, comp_size[t], f);
    }
    fclose(f);
    for (int t = 0; t < ntree; t++) { free(rawbuf[t]); free(compbuf[t]); }
    return 0;
}

/* ===== manifest ===== */
static int fix_write_manifest(const char *dir) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/manifest.ndjson", dir);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "{\"kind\":\"manifest\",\"id\":\"TEST-GAIA-001\",\"seed\":%llu,"
               "\"generator\":\"tests/unit/gaia_xpsd_fixture_gen.c\","
               "\"alg_ref\":\"docs/algorithms/GAIA_QUERY.md s2/s5\"}\n",
            (unsigned long long)FIX_SEED);
    for (int i = 0; i < g_nstars; i++) {
        FixStar *s = &g_stars[i];
        fprintf(f,
            "{\"kind\":\"star\",\"id\":\"%s\",\"file\":\"%s\",\"tree\":%d,"
            "\"proj\":\"%s\",\"cx\":%.17g,\"cy\":%.17g,\"x0\":%.17g,\"y0\":%.17g,"
            "\"dx\":%u,\"dy\":%u,\"mag_raw\":%u,\"dra_raw\":%d,"
            "\"ra\":%.17g,\"dec\":%.17g,\"mag\":%.17g",
            s->id, s->file, s->tree, s->proj, s->center_ra, s->center_dec,
            s->leaf_x0, s->leaf_y0, s->dx, s->dy, s->mag_raw, s->dra_raw,
            s->ra, s->dec, s->mag);
        if (s->has_spec) {
            fprintf(f, ",\"bp_raw\":%u,\"rp_raw\":%u"
                       ",\"flux_min\":%.9g,\"flux_mul\":%.9g,\"spectrum\":[",
                    s->bp_raw, s->rp_raw, -1.0, 85.0);
            for (int j = 0; j < WL; j++)
                fprintf(f, "%s%u", j ? "," : "", (unsigned)s->spectrum[j]);
            fprintf(f, "]");
        }
        fprintf(f, "}\n");
    }
    fclose(f);
    return 0;
}

/* ===== 截断 fixture（性能 + I5 max results）：1 文件 1 树 1 叶 ===== */
static int fix_write_truncate(const char *dir, int nstars) {
    /* g_stars 已含主 fixture 星；复用"mag 窗口外 → 置 mag_raw=0 (m=-1.5)"
     * 技巧填充，但这里要 200001 颗 → 独立组装，不经 fix_add（避免 manifest 爆炸）。
     * 设计：全部星 m=12.0（raw=13500），dx=i, dy=0，投影 EQ center(0,0)，
     * 叶 [-1, 8.9]x[-1,1)；查询 cone(0,0,r=8.5°,mag 11.9..12.1) 命中全部，
     * 验证 out_count==200000 截断（I5）。 */
    int stride = STRIDE_NOSP;
    size_t raw_len = (size_t)nstars * (size_t)stride;
    uint8_t *raw = (uint8_t *)malloc(raw_len);
    uint8_t *comp = (uint8_t *)malloc(raw_len + 4096);
    if (!raw || !comp) return -1;
    memset(raw, 0, raw_len);
    for (int i = 0; i < nstars; i++) {
        uint8_t *rec = raw + (size_t)i * stride;
        uint32_t dx = (uint32_t)i, dy = 0;
        uint16_t mag_raw = 13500; /* m=12.0 */
        memcpy(rec + 0, &dx, 4);
        memcpy(rec + 4, &dy, 4);
        memcpy(rec + 20, &mag_raw, 2);
    }
    fix_shuffle(raw, raw_len, stride);
    uLongf bound = compressBound((uLong)raw_len);
    if (compress2(comp, &bound, raw, (uLong)raw_len, Z_BEST_SPEED) != Z_OK) {
        free(raw); free(comp); return -1;
    }
    size_t clen = (size_t)bound;
    int stored = clen >= raw_len; /* 压缩不赢 → 原样 */
    if (stored) clen = raw_len;

    /* 平面跨度 nstars*2µas；nstars=200001 → ~0.111°，bbox [-1, 0.112) 可控 */
    double x1 = -1.0 + (double)nstars * INV_SCALE + 1e-9;
    char path[2048];
    snprintf(path, sizeof(path), "%s/truncate.xpsd", dir);
    FILE *f = fopen(path, "wb");
    if (!f) { free(raw); free(comp); return -1; }

    /* XML 两遍收敛（position/rootPosition 依赖 xml_len，宽度定长 → 2 遍收敛） */
    char xml[1024];
    size_t xml_len = 0;
    size_t node_pos = 0, data_pos = 0;
    int o = 0;
    for (int pass = 0; pass < 3; pass++) {
        node_pos = 16 + xml_len;
        data_pos = node_pos + 48;
        o = 0;
        o += snprintf(xml, sizeof(xml),
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<XPSDVersion=\"1.0\">\n"
            "<Data magnitudeRange=\"10.0,20.0\" position=\"%010llu\" compression=\"zlib+sh\" "
            "itemSize=\"32\"/>\n"
            "<Statistics totalSources=\"%d\"/>\n"
            "<DatabaseIdentifier>AstroCS-Test-Fixture-GaiaDR3-Trunc-Seed%llu</DatabaseIdentifier>\n"
            "<Tree projection=\"Equirectangular\" center=\"%017.12f,%017.12f\" "
            "rootPosition=\"%010llu\" nodeCount=\"1\"/>\n"
            "</XPSD>\n",
            (unsigned long long)data_pos, nstars, (unsigned long long)FIX_SEED,
            0.0, 0.0, (unsigned long long)node_pos);
        if ((size_t)o == xml_len) break;
        xml_len = (size_t)o;
    }

    uint32_t header_len = (uint32_t)xml_len;
    fwrite("XPSD0100", 1, 8, f);
    fwrite(&header_len, 4, 1, f);
    uint32_t pad = 0;
    fwrite(&pad, 4, 1, f);
    fwrite(xml, 1, xml_len, f);
    uint8_t nd[48];
    double x0 = -1.0, y0 = -1.0, y1 = 1.0;
    memcpy(nd + 0, &x0, 8);
    memcpy(nd + 8, &y0, 8);
    memcpy(nd + 16, &x1, 8);
    memcpy(nd + 24, &y1, 8);
    uint64_t bo = 0x8000000000000000ULL; /* offset 0（相对 data_position） */
    memcpy(nd + 32, &bo, 8);
    uint32_t bs = (uint32_t)raw_len, cs = (uint32_t)clen;
    memcpy(nd + 40, &bs, 4);
    memcpy(nd + 44, &cs, 4);
    fwrite(nd, 1, 48, f);
    fwrite(stored ? raw : comp, 1, clen, f);
    fclose(f);
    free(raw); free(comp);
    return 0;
}

/* ===== 星集 ===== */
static void fix_populate(void) {
    char id[64];
    /* ---- gaia_sp.xpsd（DR3SP，zlib+sh，itemSize 384） ---- */
    /* T0: Equirectangular center (10,0)，叶 [-1,1)x[20,22)，256 星（含 mag 边界/dra/边缘星） */
    fix_add("sp_t0_maglo", "gaia_sp.xpsd", 0, "Equirectangular",
            10.0, 0.0, -1.0, 20.0, 10.0, 21.0, 11500, 0, 1);
    fix_add("sp_t0_maghi", "gaia_sp.xpsd", 0, "Equirectangular",
            10.0, 0.0, -1.0, 20.0, 10.0, 21.5, 21500, 0, 1);
    fix_add("sp_t0_maghi2", "gaia_sp.xpsd", 0, "Equirectangular",
            10.0, 0.0, -1.0, 20.0, 10.25, 21.0, 21499, 0, 1);
    fix_add("sp_t0_dra_pos", "gaia_sp.xpsd", 0, "Equirectangular",
            10.0, 0.0, -1.0, 20.0, 10.5, 20.5, 16000, 1, 1);
    fix_add("sp_t0_dra_neg", "gaia_sp.xpsd", 0, "Equirectangular",
            10.0, 0.0, -1.0, 20.0, 10.5, 21.5, 16000, -1, 1);
    fix_add("sp_t0_dra_big", "gaia_sp.xpsd", 0, "Equirectangular",
            10.0, 0.0, -1.0, 20.0, 10.75, 20.25, 16000, 1000, 1);
    for (int i = 6; i < 256; i++) {
        snprintf(id, sizeof(id), "sp_t0_%03d", i);
        double ra = 9.0 + rng_uniform(0.0, 2.0) - 1e-9;
        double dec = 20.0 + rng_uniform(0.0, 2.0) - 1e-9;
        if (ra < 9.0) ra = 9.0;
        if (ra >= 11.0) ra = 10.999999;
        if (dec < 20.0) dec = 20.0;
        if (dec >= 22.0) dec = 21.999999;
        uint16_t raw = (uint16_t)(11500 + (int)(rng_uniform(0.0, 10000.0)));
        int spec = (i % 5 == 0) ? 2 : 1;
        fix_add(id, "gaia_sp.xpsd", 0, "Equirectangular",
                10.0, 0.0, -1.0, 20.0, ra, dec, raw, 0, spec);
    }
    /* T1: Equirectangular center (180,0)，叶 [-0.75,0.75)x[-30,-28)，128 星（负 dec） */
    for (int i = 0; i < 128; i++) {
        snprintf(id, sizeof(id), "sp_t1_%03d", i);
        double ra = 179.25 + rng_uniform(0.0, 1.5);
        double dec = -30.0 + rng_uniform(0.0, 2.0);
        if (dec >= -28.0) dec = -28.000001;
        uint16_t raw = (uint16_t)(11500 + (int)(rng_uniform(0.0, 10000.0)));
        fix_add(id, "gaia_sp.xpsd", 1, "Equirectangular",
                180.0, 0.0, -0.75, -30.0, ra, dec, raw, 0, 1);
    }
    /* T2: Equirectangular center (360,0)，叶 [-1,1)x[40,42)，32 星（RA 环绕 359..361） */
    for (int i = 0; i < 32; i++) {
        snprintf(id, sizeof(id), "sp_t2_%03d", i);
        double rel = (i < 16) ? rng_uniform(-1.0, 0.0) : rng_uniform(0.0, 1.0);
        double ra = 360.0 + rel; /* >360 → 解码归一化到 [0,360) */
        double dec = 40.0 + rng_uniform(0.0, 2.0);
        if (dec >= 42.0) dec = 41.999999;
        uint16_t raw = (uint16_t)(11500 + (int)(rng_uniform(0.0, 10000.0)));
        fix_add(id, "gaia_sp.xpsd", 2, "Equirectangular",
                360.0, 0.0, -1.0, 40.0, ra, dec, raw, 0, 1);
    }
    /* T3: AzimuthalEquidistant center (0,90)，叶 [-1.05,1.05)^2，8 极冠星 */
    {
        static const double th[] = {0.0, 0.25, 0.5, 0.75, 1.0, 1.0, 0.8, 0.3};
        static const double ph[] = {0.0, 72.0, 144.0, 216.0, 288.0, 0.0, 36.0, 108.0};
        for (int i = 0; i < 8; i++) {
            snprintf(id, sizeof(id), "sp_t3_%03d", i);
            double dec = 90.0 - th[i];
            double ra = fmod(360.0 + ph[i], 360.0);
            fix_add(id, "gaia_sp.xpsd", 3, "AzimuthalEquidistant",
                    0.0, 90.0, -1.05, -1.05, ra, dec, (uint16_t)(15000 + i), 0, 1);
        }
    }

    /* ---- gaia_dr3.xpsd（DR3，无光谱，lz4 / 原样，itemSize 32） ---- */
    /* T0: Equirectangular center (60,0)，叶 [-1,1)x[5,7)，160 星（80 零星供 lz4） */
    for (int i = 0; i < 160; i++) {
        snprintf(id, sizeof(id), "dr3_t0_%03d", i);
        if (i < 80) { /* 零星：dx=dy=mag=0 重复 pattern（真实 LZ4 压缩比）；
                       * 位置全同会破坏真值 1:1 匹配 → y 网格错开 */
            fix_add(id, "gaia_dr3.xpsd", 0, "Equirectangular",
                    60.0, 0.0, -1.0, 5.0, 59.0, 5.0 + i * 1e-5, 0, 0, 0);
            continue;
        }
        double ra = 59.0 + rng_uniform(0.0, 2.0);
        double dec = 5.0 + rng_uniform(0.0, 2.0);
        if (ra >= 61.0) ra = 60.999999;
        if (dec >= 7.0) dec = 6.999999;
        uint16_t raw = (uint16_t)(11500 + (int)(rng_uniform(0.0, 10000.0)));
        fix_add(id, "gaia_dr3.xpsd", 0, "Equirectangular",
                60.0, 0.0, -1.0, 5.0, ra, dec, raw, 0, 0);
    }
    /* T1: Equirectangular center (200,0)，叶 [-0.5,0.5)x[-10,-8)，96 星（原样块路径） */
    for (int i = 0; i < 96; i++) {
        snprintf(id, sizeof(id), "dr3_t1_%03d", i);
        double ra = 199.5 + rng_uniform(0.0, 1.0);
        double dec = -10.0 + rng_uniform(0.0, 2.0);
        if (dec >= -8.0) dec = -8.000001;
        uint16_t raw = (uint16_t)(11500 + (int)(rng_uniform(0.0, 10000.0)));
        fix_add(id, "gaia_dr3.xpsd", 1, "Equirectangular",
                200.0, 0.0, -0.5, -10.0, ra, dec, raw, 0, 0);
    }
}

/* ===== 头文件模式（单测编译期嵌入参考真值，运行时零解析） ===== */
static int fix_write_header(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f,
        "/* 自动生成：tests/unit/gaia_xpsd_fixture_gen.c --header（seed %llu）。\n"
        " * 参考真值 = 量化精确反演（独立公式，与被测实现同源 ALG s2）。\n"
        " * 值由 oracle (tests/oracle/gaia_oracle.py) 二次独立复算。 */\n"
        "#ifndef GAIA_CAT_MANIFEST_H\n"
        "#define GAIA_CAT_MANIFEST_H\n"
        "#define GAIA_CAT_WL 343\n"
        "typedef struct {\n"
        "    const char *id; const char *file; int tree;\n"
        "    double ra, dec, mag;\n"
        "    unsigned mag_raw, bp_raw, rp_raw;\n"
        "    int dra_raw;\n"
        "    int has_spec;\n"
        "    unsigned char spectrum[GAIA_CAT_WL];\n"
        "} GaiaCatRefStar;\n"
        "static const GaiaCatRefStar G_MANIFEST[] = {\n",
        (unsigned long long)FIX_SEED);
    for (int i = 0; i < g_nstars; i++) {
        FixStar *s = &g_stars[i];
        fprintf(f,
            "{\"%s\",\"%s\",%d,%.17g,%.17g,%.17g,%uu,%uu,%uu,%d,%d,{",
            s->id, s->file, s->tree, s->ra, s->dec, s->mag,
            s->mag_raw, s->bp_raw, s->rp_raw, s->dra_raw, s->has_spec);
        for (int j = 0; j < WL; j++)
            fprintf(f, "%u,", (unsigned)s->spectrum[j]);
        fprintf(f, "}},\n");
    }
    fprintf(f, "};\n#define G_MANIFEST_N %d\n#endif\n", g_nstars);
    fclose(f);
    return 0;
}

/* ===== main（可作库：GAIA_FIXTURE_GEN_AS_LIBRARY） ===== */
static const FixTree g_trees_sp[4] = {
    {"Equirectangular",      10.0,  0.0, -1.00,  20.0,  1.00, 22.0,   0, 256, 0},
    {"Equirectangular",     180.0,  0.0, -0.75, -30.0,  0.75,-28.0, 256, 384, 0},
    {"Equirectangular",     360.0,  0.0, -1.00,  40.0,  1.00, 42.0, 384, 416, 0},
    {"AzimuthalEquidistant",  0.0, 90.0, -1.05, -1.05,  1.05, 1.05, 416, 424, 0},
};
static const FixTree g_trees_dr3[2] = {
    {"Equirectangular",  60.0,  0.0, -1.00,   5.0, 1.00,  7.0, 424, 584, 1},
    {"Equirectangular", 200.0,  0.0, -0.50, -10.0, 0.50, -8.0, 584, 680, 2},
};

int gaia_fixt_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: gaia_xpsd_fixture_gen <out_dir> [--header <path>]\n");
        return 2;
    }
    const char *dir = argv[1];
    const char *header_path = NULL;
    for (int i = 2; i + 1 < argc; i += 2)
        if (strcmp(argv[i], "--header") == 0) header_path = argv[i + 1];
    fix_populate();
    char err[2048] = {0};
    if (fix_write_xpsd(dir, "gaia_sp.xpsd", "GaiaDR3SP", "zlib+sh", STRIDE_SP, 1,
                       g_trees_sp, 4, err, sizeof(err)) != 0) {
        fprintf(stderr, "fixture gen: %s\n", err);
        return 2;
    }
    if (fix_write_xpsd(dir, "gaia_dr3.xpsd", "GaiaDR3", "lz4", STRIDE_NOSP, 0,
                       g_trees_dr3, 2, err, sizeof(err)) != 0) {
        fprintf(stderr, "fixture gen: %s\n", err);
        return 2;
    }
    if (fix_write_manifest(dir) != 0) {
        fprintf(stderr, "fixture gen: manifest write failed\n");
        return 2;
    }
    if (fix_write_truncate(dir, 200001) != 0) {
        fprintf(stderr, "fixture gen: truncate fixture failed\n");
        return 2;
    }
    if (header_path && fix_write_header(header_path) != 0) {
        fprintf(stderr, "fixture gen: header write failed\n");
        return 2;
    }
    printf("gaia_xpsd_fixture_gen: seed=%llu stars=%d dir=%s (truncate fixture included)\n",
           (unsigned long long)FIX_SEED, g_nstars, dir);
    return (g_nstars == 680) ? 0 : 3;
}

#ifndef GAIA_FIXTURE_GEN_AS_LIBRARY
int main(int argc, char **argv) { return gaia_fixt_main(argc, argv); }
#endif
