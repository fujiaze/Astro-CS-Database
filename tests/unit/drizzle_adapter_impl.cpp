/* drizzle_adapter_impl.cpp - adapter 测试直接路径实现 (C++, 含生产源)
 *
 * 由 drizzle_adapter_test.c include。提供:
 *   - helper (api_desc_str/api_cfg_str/drz_host_*)
 *   - direct 路径 legacy 调用 (hp_drizzle_run / hp_drizzle_reverse_run, 统计面)
 *   - BITWISE 比对实现
 * 编译单元与生产源一致: drizzle_engine.cpp 等 9 TU 直接在本测试可执行内
 * 重编译 (非 PIC STATIC 消费, 语义与 legacy 链路面完全一致)。
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>   /* mkstemp/close/remove */
#endif

#include "hp_drizzle_api.h"
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/host_api_v1.h"

/* ── str 构造 helper ── */
static acs_str_v1 api_desc_str(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s; v.size = (uint64_t)strlen(s);
    return v;
}
#define api_cfg_str(s) api_desc_str(s)

/* ── host executor stub (租借计数) ── */
typedef struct {
    acs_host_api_v1 api;
    acs_executor_v1 executor;
    acs_allocator_v1 allocator;
    int acquire_calls, release_calls;
    int fail_acquire;
} drz_test_host;

static void* drz_alloc(void* user, uint64_t size, uint64_t align) {
    (void)user; (void)align;
    return size ? malloc((size_t)size) : NULL;
}
static void drz_free(void* user, void* p) { (void)user; free(p); }

static int drz_acquire(void* user, uint32_t n) {
    drz_test_host* h = (drz_test_host*)user;
    h->acquire_calls++;
    if (h->fail_acquire) return -1;
    (void)n;
    return 0;
}
static void drz_release(void* user, uint32_t n) {
    drz_test_host* h = (drz_test_host*)user;
    h->release_calls++;
    (void)n;
}
static void drz_host_init(drz_test_host* h) {
    memset(h, 0, sizeof(*h));
    h->api.head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    h->api.head.abi_version = ACS_ABI_VERSION_V1;
    h->executor.head.struct_size = (uint32_t)sizeof(acs_executor_v1);
    h->executor.head.abi_version = ACS_ABI_VERSION_V1;
    h->executor.available_cpus = 4;
    h->executor.max_workers = 4;
    h->executor.acquire = drz_acquire;
    h->executor.release = drz_release;
    h->executor.user_data = h;
    h->api.executor = &h->executor;
    h->allocator.head.struct_size = (uint32_t)sizeof(acs_allocator_v1);
    h->allocator.head.abi_version = ACS_ABI_VERSION_V1;
    h->allocator.alloc = drz_alloc;
    h->allocator.free = drz_free;
    h->allocator.user_data = h;
    h->api.allocator = &h->allocator;
}
static const acs_host_api_v1* drz_host_api(drz_test_host* h) { return &h->api; }

/* ── 合成 fixture: 64x64 f32 高斯斑块 + TAN WCS (CRVAL/CRPIX/CD) ── */
static void drz_make_plane_f32(std::vector<float>& img, int W, int H,
                               double* crval, double* crpix, double* cd) {
    img.assign((size_t)W * H, 0.0f);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double dx = (double)x - 32.0, dy = (double)y - 32.0;
            img[(size_t)y * W + x] =
                (float)(1000.0 * std::exp(-(dx * dx + dy * dy) / 18.0) + 10.0);
        }
    (void)crval; (void)crpix; (void)cd;   /* 头键由 manifest 直接承载 */
}

/* manifest JSON 组装 (行数据 base64); 用 b64 编码 f32/f64 数组 */
static void drz_b64_encode(const uint8_t* src, size_t n, std::string& out) {
    static const char* tab =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    out.clear();
    size_t i = 0;
    for (; i + 3 <= n; i += 3) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i+1] << 8) | src[i+2];
        out += tab[(v >> 18) & 63]; out += tab[(v >> 12) & 63];
        out += tab[(v >> 6) & 63];  out += tab[v & 63];
    }
    size_t rem = n - i;
    if (rem == 1) {
        uint32_t v = (uint32_t)src[i] << 16;
        out += tab[(v >> 18) & 63]; out += tab[(v >> 12) & 63]; out += "==";
    } else if (rem == 2) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i+1] << 8);
        out += tab[(v >> 18) & 63]; out += tab[(v >> 12) & 63];
        out += tab[(v >> 6) & 63];  out += '=';
    }
}

static std::string drz_build_drizzle_manifest(const std::vector<float>& img,
                                              int W, int H) {
    std::string b64;
    drz_b64_encode((const uint8_t*)img.data(), img.size() * sizeof(float), b64);
    char buf[256];
    std::string m = "{\"op\":\"drizzle\",\"width\":" + std::to_string(W) +
        ",\"height\":" + std::to_string(H) + ",\"dtype\":\"f32\",\"data_base64\":\"" + b64 + "\","
        "\"header\":{"
        "\"CRVAL1\":\"202.5\",\"CRVAL2\":\"47.2\","
        "\"CRPIX1\":\"32.0\",\"CRPIX2\":\"32.0\","
        "\"CD1_1\":\"-0.0002777778\",\"CD1_2\":\"0.0\","
        "\"CD2_1\":\"0.0\",\"CD2_2\":\"0.0002777778\","
        "\"CTYPE1\":\"RA---TAN\",\"CTYPE2\":\"DEC--TAN\","
        "\"PHOTSCAL\":\"1.0\",\"PHOTAPPL\":\"1.0\","
        "\"FILTER\":\"g\",\"EXPTIME\":\"30.0\","
        "\"DATE-OBS\":\"2026-01-01T00:00:00\","
        "\"OBJECT\":\"unit\",\"TELESCOP\":\"astrocs\",\"INSTRUME\":\"sim\"}}";
    (void)buf;
    return m;
}

/* reverse manifest: nside=64 (LEGACY 小 nside 反向允许), 简单 leaf 集 */
static std::string drz_build_reverse_manifest(const std::vector<uint64_t>& ipix,
                                              const std::vector<float>& sig,
                                              const std::vector<double>& sup,
                                              double pixfrac) {
    std::string ip64, sg64, sp64;
    drz_b64_encode((const uint8_t*)ipix.data(), ipix.size() * 8, ip64);
    drz_b64_encode((const uint8_t*)sig.data(), sig.size() * 4, sg64);
    drz_b64_encode((const uint8_t*)sup.data(), sup.size() * 8, sp64);
    std::string m = "{\"op\":\"reverse\",\"nside\":64,\"nested\":1,"
        "\"pixfrac\":" + std::to_string(pixfrac) + ",\"output_fp64\":0,"
        "\"no_data_as_zero\":1,"
        "\"crval\":[202.5,47.2],\"crpix\":[32.0,32.0],"
        "\"cd\":[-0.0002777778,0.0,0.0,0.0002777778],"
        "\"sip_order\":0,\"sip_ap_order\":0,"
        "\"n_leaf\":" + std::to_string(ipix.size()) + ","
        "\"leaf_dtype\":\"f32\","
        "\"leaf_ipix_base64\":\"" + ip64 + "\","
        "\"leaf_signal_base64\":\"" + sg64 + "\","
        "\"leaf_support_base64\":\"" + sp64 + "\"}";
    return m;
}

/* ── 模块 vtable 引用 (execute 走 plugin 面) ── */

/* 直接路径: legacy hp_drizzle_run (output_path=NULL 纯统计) */
static int drz_direct_run(PipelineFrame* frame, int nside, int nested,
                          double pixfrac, HpDrizzleResult* res) {
    return hp_drizzle_run(frame, nside, nested, pixfrac, NULL, res, 0);
}

/* PipelineFrame 组装 (与 adapter 内一致) */
static PipelineFrame* drz_direct_frame(const std::vector<float>& img, int W, int H) {
    PipelineFrame* f = aio_pipeline_frame_create();
    if (!f) return NULL;
    int dims[2] = { H, W };
    if (aio_frame_add_block(f, "data", AIO_BLOCK_FLOAT32, (void*)img.data(),
                            (int64_t)(size_t)W * H, dims, 2, "test") != 0) {
        aio_pipeline_frame_destroy(f);
        return NULL;
    }
    struct KV { const char* k; const char* v; };
    static const KV kvs[] = {
        {"CRVAL1", "202.5"}, {"CRVAL2", "47.2"},
        {"CRPIX1", "32.0"}, {"CRPIX2", "32.0"},
        {"CD1_1", "-0.0002777778"}, {"CD1_2", "0.0"},
        {"CD2_1", "0.0"}, {"CD2_2", "0.0002777778"},
        {"CTYPE1", "RA---TAN"}, {"CTYPE2", "DEC--TAN"},
        {"PHOTSCAL", "1.0"}, {"PHOTAPPL", "1.0"},
        {"FILTER", "g"}, {"EXPTIME", "30.0"},
        {"DATE-OBS", "2026-01-01T00:00:00"},
        {"OBJECT", "unit"}, {"TELESCOP", "astrocs"}, {"INSTRUME", "sim"}
    };
    for (size_t i = 0; i < sizeof(kvs)/sizeof(kvs[0]); i++)
        aio_frame_kv_set(f, "header", kvs[i].k, kvs[i].v);
    return f;
}

/* BITWISE: drizzle op — direct 统计 vs plugin execute 输出统计。
 * plugin 路径 output_path 指向临时 .hiss (事务 sink 输出), 统计不受
 * 文件 history elapsed 影响。 */
static int drz_bitwise_drizzle(acs_module_instance_v1* inst,
                               const acs_host_api_v1* hapi,
                               const acs_module_api_v1* api) {
    const int W = 64, H = 64;
    std::vector<float> img;
    double crval[2] = {202.5, 47.2}, crpix[2] = {32.0, 32.0};
    double cd[4] = {-0.0002777778, 0.0, 0.0, 0.0002777778};
    drz_make_plane_f32(img, W, H, crval, crpix, cd);

    /* direct */
    PipelineFrame* df = drz_direct_frame(img, W, H);
    if (!df) return 0;
    HpDrizzleResult dres;
    memset(&dres, 0, sizeof(dres));
    int rc = drz_direct_run(df, 512, 1, 1.0, &dres);
    aio_pipeline_frame_destroy(df);
    if (rc != 0) { printf("  direct run rc=%d err=%s\n", rc, dres.error_msg); return 0; }

    /* plugin: 临时 hips_dir (hp_drizzle_run_hips 语义: 直写 HiPS 产品集,
     * hips_dir 必填 (-12); legacy_hiss_path 可选, 不传) */
    char dtmpl[] = "/tmp/drz_adapter_test_hips_XXXXXX";
    char* hips_dir = mkdtemp(dtmpl);
    if (!hips_dir) return 0;

    std::string manifest = drz_build_drizzle_manifest(img, W, H);
    std::string cfg = std::string("{\"op\":\"drizzle\",\"nside\":512,\"nested\":1,"
                                  "\"pixfrac\":1.0,\"precision_mode\":0,"
                                  "\"hips_dir\":\"") + hips_dir + "\"}";
    acs_strbuf_v1 ob; char obuf[1024];
    memset(&ob, 0, sizeof(ob)); ob.data = obuf; ob.cap = sizeof(obuf);
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = api->execute(inst, api_cfg_str(manifest.c_str()),
                                 api_cfg_str(cfg.c_str()), &ob, &err);
    if (st != ACS_OK) {
        printf("  plugin execute st=%d msg=%s\n", (int)st,
               err.message_utf8 ? err.message_utf8 : "?");
        return 0;
    }
    /* 统计逐位一致 (直接路径 vs 模块输出) */
    int ok = strstr(obuf, "\"n_healpix_pixels\":") && strstr(obuf, "\"n_source_pixels\":");
    if (ok) {
        long long d_h = (long long)dres.n_healpix_pixels;
        long long d_s = (long long)dres.n_source_pixels;
        char expect[128];
        snprintf(expect, sizeof(expect), "\"n_healpix_pixels\":%lld", d_h);
        ok = strstr(obuf, expect) != NULL;
        if (ok) {
            snprintf(expect, sizeof(expect), "\"n_source_pixels\":%lld", d_s);
            ok = strstr(obuf, expect) != NULL;
        }
        if (!ok)
            printf("  stats mismatch: direct(%lld,%lld) out=%.120s\n",
                   d_h, d_s, ob.data);
    }
    /* 清理临时 hips 产品集 */
    {
        std::string cmd = std::string("rm -rf '") + hips_dir + "'";
        if (system(cmd.c_str()) != 0) { /* 清理失败不影响判定 */ }
    }
    return ok;
}

/* BITWISE: reverse op — direct 输出平面 vs plugin 输出平面 base64 解码 memcmp */
static int drz_bitwise_reverse(acs_module_instance_v1* inst,
                               const acs_host_api_v1* hapi,
                               const acs_module_api_v1* api) {
    const int32_t NSIDE = 64;
    const int W = 32, H = 32;
    /* 合成 leaf: 均匀网格 12*NSIDE^2 太大; 取每 quadrant 首 pixel 子集 256 leaf */
    std::vector<uint64_t> ipix;
    std::vector<float> sig;
    std::vector<double> sup;
    for (uint64_t p = 0; p < 256; p++) {
        ipix.push_back(p * 733ull % (12ull * NSIDE * NSIDE));
        sig.push_back((float)(100.0 + (double)p));
        sup.push_back(1.0);
    }
    std::string manifest = drz_build_reverse_manifest(ipix, sig, sup, 0.5);
    std::string cfg = std::string("{\"op\":\"reverse\",\"width\":") +
        std::to_string(W) + ",\"height\":" + std::to_string(H) + "}";
    acs_strbuf_v1 ob;
    size_t plane_bytes = (size_t)W * H * 4;
    size_t obuf_cap = plane_bytes * 2 * 4 / 3 + 4096;
    std::vector<char> obuf(obuf_cap);
    ob.data = obuf.data(); ob.cap = (uint64_t)obuf_cap;
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    acs_status st = api->execute(inst, api_cfg_str(manifest.c_str()),
                                 api_cfg_str(cfg.c_str()), &ob, &err);
    if (st != ACS_OK) {
        printf("  reverse plugin st=%d msg=%s\n", (int)st,
               err.message_utf8 ? err.message_utf8 : "?");
        return 0;
    }
    /* 解码 signal_plane_base64 与 direct 对比 */
    const char* key = "\"signal_plane_base64\":\"";
    const char* p = strstr(ob.data, key);
    if (!p) { printf("  reverse out missing plane\n"); return 0; }
    p += strlen(key);
    std::vector<uint8_t> plane_direct(plane_bytes), plane_plugin(plane_bytes);
    /* direct 路径 */
    HpReverseDrizzleInput rin;
    memset(&rin, 0, sizeof(rin));
    rin.nside = NSIDE; rin.nested = 1;
    rin.target_width = W; rin.target_height = H;
    rin.pixfrac = 0.5; rin.output_fp64 = 0;
    rin.crval[0] = 202.5; rin.crval[1] = 47.2;
    rin.crpix[0] = 32.0; rin.crpix[1] = 32.0;
    rin.cd[0] = -0.0002777778; rin.cd[1] = 0.0;
    rin.cd[2] = 0.0; rin.cd[3] = 0.0002777778;
    rin.sip_order = 0; rin.sip_ap_order = 0;
    rin.leaf_ipix = ipix.data();
    rin.n_leaf = (int64_t)ipix.size();
    rin.leaf_signal_f32 = sig.data();
    rin.leaf_support = sup.data();
    rin.no_data_as_zero = 1;
    HpReverseDrizzleResult rres;
    memset(&rres, 0, sizeof(rres));
    int rc = hp_drizzle_reverse_run(&rin, plane_direct.data(), plane_plugin.data(), &rres);
    if (rc != 0) { printf("  direct reverse rc=%d err=%s\n", rc, rres.error_msg); return 0; }
    /* plugin b64 → bytes */
    static int8_t rev[256];
    static bool init = false;
    if (!init) {
        memset(rev, -1, sizeof(rev));
        const char* tab =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; i++) rev[(unsigned char)tab[i]] = (int8_t)i;
        init = true;
    }
    size_t o = 0;
    uint32_t acc = 0; int bits = 0;
    const char* q = p;
    while (*q && *q != '"' && o < plane_bytes) {
        if (*q == '=') break;
        int8_t v = rev[(unsigned char)*q];
        if (v < 0) break;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; plane_plugin[o++] = (uint8_t)((acc >> bits) & 0xFF); }
        q++;
    }
    if (o != plane_bytes) { printf("  reverse plane b64 short: %zu\n", o); return 0; }
    return memcmp(plane_direct.data(), plane_plugin.data(), plane_bytes) == 0;
}
