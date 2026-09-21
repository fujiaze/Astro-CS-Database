/* M9-H-1 回归锁: Gaia 模块 manifest 栈缓冲容量边界 (B1-memory)
 *
 * 根因: lib/infrastructure/gaia_xpsd_client/src/module_entry.c::gaia_execute 用 512 字节栈
 * 缓冲 head[512] 拼 manifest, ::gaia_inspect 用同型 buf[512]:
 *   - json_append_escaped 无容量入参, 逐字节自增越界写;
 *   - 三处 hw += snprintf(...) 用"本应写入数"推进指针 (截断即越界);
 *   - schema 用无界 memcpy;
 *   - 越界长度再经 (hw-head) 作为 total / memcpy 回读进交付 JSON。
 * catalog_dir 配置上限 1023 字符 ⇒ 可造成约 1900 字节栈越界写/读。
 *
 * 修复契约 (M9-H-1 建议处置①②): 所有拼接点显式带容量并夹紧; 触边即
 * ACS_ERR_PARAM + ACS_DIAG_ECODE_BUFFER_TOO_SMALL, 不产出产品, 尺寸查询
 * 阶段同样拒绝。长度 119/120/121 的合法目录仍须成功且可回读。
 *
 * 本 TU 共址 #include module_entry.c 直接驱动 static gaia_execute/gaia_inspect:
 * 修复前 1023 字符 catalog_dir 与 400×'"' 返回 ACS_OK (ASan 下则栈越界);
 * 修复后这两处返回 ACS_ERR_PARAM 且不写产品 → 红/绿分离。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <direct.h>
#define ACS_TEST_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define ACS_TEST_MKDIR(p) mkdir((p), 0777)
#endif

#include "module_entry.c"   /* 被测适配器 (共址直接路径; gaia_client.c 另编译链接) */

static int g_fail = 0;
static const uint64_t kSizeSentinel = 0xDEADBEEFULL;

static void check(int cond, const char* what) {
    if (cond) {
        fprintf(stderr, "ok   %s\n", what);
    } else {
        fprintf(stderr, "FAIL %s\n", what);
        g_fail++;
    }
}

static void write_dr3_xpsd(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot create %s\n", path); exit(2); }
    uint8_t hdr[16];
    memcpy(hdr, "XPSD0100", 8);
    uint32_t zero = 0;
    memcpy(hdr + 8, &zero, 4);
    memcpy(hdr + 12, &zero, 4);
    fwrite(hdr, 1, sizeof(hdr), f);
    const char* xml =
        "<XPSD>"
        "<Data magnitudeRange=\"0,20\" position=\"0\" compression=\"raw\" itemSize=\"32\" />"
        "<Statistics totalSources=\"0\"/>"
        "<DatabaseIdentifier>GaiaDR3</DatabaseIdentifier>"
        "</XPSD>";
    fwrite(xml, 1, strlen(xml), f);
    fclose(f);
}

static void build_cfg(char* out, size_t cap, const char* dir) {
    snprintf(out, cap,
             "{\"op\":\"cone_search\",\"catalog_dir\":\"%s\","
             "\"ra\":10.0,\"dec\":20.0,\"radius_deg\":0.5,"
             "\"mag_low\":0.0,\"mag_high\":20.0}",
             dir);
}

static acs_status call_execute(gaia_inst* inst, const char* cfg, char* buf,
                               uint64_t cap, acs_strbuf_v1* out,
                               acs_error_info_v1* err) {
    acs_str_v1 in = acs_str_from("{}");
    acs_str_v1 cj = acs_str_from(cfg);
    memset(out, 0, sizeof(*out));
    out->head.struct_size = (uint32_t)sizeof(*out);
    out->head.abi_version = ACS_ABI_VERSION_V1;
    out->data = buf;
    out->cap = cap;
    out->size = kSizeSentinel;
    return gaia_execute((acs_module_instance_v1*)inst, in, cj, out, err);
}

static acs_status call_inspect(const gaia_inst* inst, char* buf, uint64_t cap,
                               acs_strbuf_v1* out, acs_error_info_v1* err) {
    memset(out, 0, sizeof(*out));
    out->head.struct_size = (uint32_t)sizeof(*out);
    out->head.abi_version = ACS_ABI_VERSION_V1;
    out->data = buf;
    out->cap = cap;
    out->size = kSizeSentinel;
    return gaia_inspect((const acs_module_instance_v1*)inst, out, err);
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : ".";
    ACS_TEST_MKDIR(dir);
    char xpsd[1024];
    snprintf(xpsd, sizeof(xpsd), "%s/catalog.xpsd", dir);
    write_dr3_xpsd(xpsd);

    GaiaClient* cli = gaia_client_create_ex(dir, GAIA_DB_AUTO);
    if (!cli) { fprintf(stderr, "FAIL cannot open catalog dir %s\n", dir); return 2; }

    gaia_inst inst;
    memset(&inst, 0, sizeof(inst));
    inst.state = ACS_LC_STATE_CREATED;
    inst.client = cli;
    inst.host = NULL;

    static char cfg[4096];
    static char buf[65536];
    acs_strbuf_v1 out;
    acs_error_info_v1 err;

    /* ── 合法长度 119/120/121: 必须成功且 catalog_dir 原样回读 ── */
    const int lens[3] = { 119, 120, 121 };
    for (int i = 0; i < 3; i++) {
        static char d[128];
        memset(d, 'a', (size_t)lens[i]);
        d[lens[i]] = '\0';
        build_cfg(cfg, sizeof(cfg), d);
        buf[0] = 'Z';
        acs_status st = call_execute(&inst, cfg, buf, sizeof(buf), &out, &err);
        char what[96];
        snprintf(what, sizeof(what), "execute catalog_dir=%d -> ACS_OK", lens[i]);
        check(st == ACS_OK, what);
        snprintf(what, sizeof(what), "execute catalog_dir=%d round-trips", lens[i]);
        check(st == ACS_OK && strstr(buf, d) != NULL, what);
    }

    /* ── 转义正例: 2 个反斜杠须翻倍为 4 且不越界 ── */
    build_cfg(cfg, sizeof(cfg), "aa\\\\bb");   /* 配置文本含 2 反斜杠 */
    buf[0] = 'Z';
    acs_status st_esc = call_execute(&inst, cfg, buf, sizeof(buf), &out, &err);
    check(st_esc == ACS_OK && strstr(buf, "aa") != NULL
          && strstr(buf, "\\\\\\\\") != NULL,
          "execute escaped backslashes -> ACS_OK + doubled");

    /* ── 越界: 1023 字符 catalog_dir, 写阶段必须 PARAM 且不写产品 ── */
    static char big[1024];
    memset(big, 'a', 1023);
    big[1023] = '\0';
    build_cfg(cfg, sizeof(cfg), big);
    buf[0] = 'Z';
    acs_status st_long = call_execute(&inst, cfg, buf, sizeof(buf), &out, &err);
    check(st_long == ACS_ERR_PARAM, "execute catalog_dir=1023 -> ACS_ERR_PARAM");
    check(out.size == kSizeSentinel, "execute catalog_dir=1023 -> no product (size untouched)");
    check(buf[0] == 'Z', "execute catalog_dir=1023 -> output buffer untouched");

    /* ── 越界: 尺寸查询阶段 (data=NULL, cap=0) 同样拒绝, 且不报尺寸 ── */
    acs_status st_sz = call_execute(&inst, cfg, NULL, 0, &out, &err);
    check(st_sz == ACS_ERR_PARAM, "execute size-query catalog_dir=1023 -> ACS_ERR_PARAM");
    check(out.size == kSizeSentinel, "execute size-query -> no size leaked");

    gaia_client_destroy(cli);

    /* ── inspect 第二站点: 手工构造实例, 精确控制 catalog_dir ── */
    gaia_inst inst2;
    memset(&inst2, 0, sizeof(inst2));
    inst2.state = ACS_LC_STATE_CREATED;

    snprintf(inst2.cfg.catalog_dir, sizeof(inst2.cfg.catalog_dir), "short/dir");
    buf[0] = 'Z';
    acs_status st_i1 = call_inspect(&inst2, buf, sizeof(buf), &out, &err);
    check(st_i1 == ACS_OK && strstr(buf, "short/dir") != NULL,
          "inspect short catalog_dir -> ACS_OK round-trips");

    snprintf(inst2.cfg.catalog_dir, sizeof(inst2.cfg.catalog_dir), "x\"y\\z");
    buf[0] = 'Z';
    acs_status st_i2 = call_inspect(&inst2, buf, sizeof(buf), &out, &err);
    check(st_i2 == ACS_OK && strstr(buf, "x\\\"y\\\\z") != NULL,
          "inspect escaped quote/backslash -> ACS_OK + correct escape");

    memset(inst2.cfg.catalog_dir, '"', 400);
    inst2.cfg.catalog_dir[400] = '\0';
    buf[0] = 'Z';
    acs_status st_i3 = call_inspect(&inst2, buf, sizeof(buf), &out, &err);
    check(st_i3 == ACS_ERR_PARAM, "inspect 400x'\"' -> ACS_ERR_PARAM");
    check(out.size == kSizeSentinel, "inspect 400x'\"' -> no product");

    memset(inst2.cfg.catalog_dir, 'a', 1023);
    inst2.cfg.catalog_dir[1023] = '\0';
    acs_status st_i4 = call_inspect(&inst2, buf, sizeof(buf), &out, &err);
    check(st_i4 == ACS_ERR_PARAM, "inspect catalog_dir=1023 -> ACS_ERR_PARAM");
    check(out.size == kSizeSentinel, "inspect catalog_dir=1023 -> no product");

    if (g_fail) {
        fprintf(stderr, "gaia_module_manifest_bounds: %d failure(s)\n", g_fail);
        return 1;
    }
    fprintf(stderr, "gaia_module_manifest_bounds: PASS\n");
    return 0;
}
