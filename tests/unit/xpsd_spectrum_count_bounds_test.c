/* M9-H-2 回归锁: XPSD 文件自报 spectrumCount 边界 (B1-memory)
 *
 * 根因: lib/gaia_xpsd_client/src/gaia_client.c::load_xpsd_file 曾以
 *   xf->spectrum_count = atoi(...) 直接接受文件内 parameters 串的自报计数,
 * 该值随后用作 spec_collector_init 的步长 (capacity*count 分配)、
 * spec_collector_push 每星 memcpy 长度以及结果导出侧 malloc 尺寸:
 *   - 源侧堆越界读: 每星记录光谱起点 p+40 之后仅 344 字节可读
 *     (STAR_STRIDE_SP=384), memcpy 长度却是自报值;
 *   - OOM 放大: 容量 4096 × 100000 ≈ 410MB (单文件),
 *     导出侧 200000 × 100000 ≈ 2e10 字节。
 *
 * 修复契约 (M9-H-2 建议处置①②): 光谱文件自报计数必须落在 [1, WL_COUNT],
 * 语法非法 (负号/非数字/尾随垃圾) 或越界 → load_xpsd_file 返回 -1 (整文件
 * 拒绝); 非光谱 (DR3) 文件不受影响。
 *
 * 本 TU 共址 #include gaia_client.c 直接驱动 static load_xpsd_file——
 * 修复前: spectrumCount=0/344/100000/-1/abc 全部被接受 (rc=0) → 本测试红;
 * 修复后: 仅 343/1 与无光谱 DR3 被接受 → 绿。
 * 另附边界算术断言: WL_COUNT ≤ STAR_STRIDE_SP - 40 (343 ≤ 344)。
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

#include "gaia_client.c"   /* 被测实现 (共址直接路径, 不定义 GAIA_ALLOC_TEST) */

static int g_fail = 0;

static void write_xpsd(const char* path, const char* dbid, const char* params_attr) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot create %s\n", path); exit(2); }
    uint8_t hdr[16];
    memcpy(hdr, "XPSD0100", 8);
    uint32_t zero = 0;
    memcpy(hdr + 8, &zero, 4);
    memcpy(hdr + 12, &zero, 4);
    fwrite(hdr, 1, sizeof(hdr), f);
    char xml[1024];
    snprintf(xml, sizeof(xml),
             "<XPSD>"
             "<Data magnitudeRange=\"0,20\" position=\"0\" compression=\"raw\" itemSize=\"384\" %s/>"
             "<Statistics totalSources=\"0\"/>"
             "<DatabaseIdentifier>%s</DatabaseIdentifier>"
             "</XPSD>",
             params_attr, dbid);
    fwrite(xml, 1, strlen(xml), f);
    fclose(f);
}

static int load_once(const char* dir, const char* name, const char* dbid,
                     const char* params_attr) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    write_xpsd(path, dbid, params_attr);
    XPSDFileInternal xf;
    int rc = load_xpsd_file(&xf, path);
    if (rc == 0) close_xpsd_file(&xf);
    return rc;
}

static void expect(const char* what, int rc, int want) {
    if (rc != want) {
        fprintf(stderr, "FAIL %-46s load_xpsd_file=%d want %d\n", what, rc, want);
        g_fail++;
    } else {
        fprintf(stderr, "ok   %-46s rc=%d\n", what, rc);
    }
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : ".";
    ACS_TEST_MKDIR(dir);
    const char* SP  = "GaiaDR3SP";
    const char* DR3 = "GaiaDR3";

    /* 边界算术证据: 记录内光谱可用字节 = STAR_STRIDE_SP - 40 = 344 */
    if (WL_COUNT > (STAR_STRIDE_SP - 40)) {
        fprintf(stderr, "FAIL boundary arithmetic: WL_COUNT=%d > stride window=%d\n",
                WL_COUNT, STAR_STRIDE_SP - 40);
        g_fail++;
    } else {
        fprintf(stderr, "ok   boundary arithmetic: WL_COUNT=%d <= %d\n",
                WL_COUNT, STAR_STRIDE_SP - 40);
    }

    expect("spectrumCount=343 (valid)", load_once(dir, "sp343.xpsd", SP,
        "parameters=\"spectrumStart=336,spectrumStep=1,spectrumCount=343,spectrumBits=8\""), 0);
    expect("spectrumCount=1 (lower bound)", load_once(dir, "sp1.xpsd", SP,
        "parameters=\"spectrumStart=336,spectrumStep=1,spectrumCount=1,spectrumBits=8\""), 0);
    expect("spectrumCount=0 (reject)", load_once(dir, "sp0.xpsd", SP,
        "parameters=\"spectrumStart=336,spectrumStep=1,spectrumCount=0,spectrumBits=8\""), -1);
    expect("spectrumCount=344 (> WL_COUNT)", load_once(dir, "sp344.xpsd", SP,
        "parameters=\"spectrumStart=336,spectrumStep=1,spectrumCount=344,spectrumBits=8\""), -1);
    expect("spectrumCount=100000 (OOM amplifier)", load_once(dir, "sp100000.xpsd", SP,
        "parameters=\"spectrumStart=336,spectrumStep=1,spectrumCount=100000,spectrumBits=8\""), -1);
    expect("spectrumCount=-1 (negative)", load_once(dir, "spneg.xpsd", SP,
        "parameters=\"spectrumStart=336,spectrumStep=1,spectrumCount=-1,spectrumBits=8\""), -1);
    expect("spectrumCount=abc (non-numeric)", load_once(dir, "spabc.xpsd", SP,
        "parameters=\"spectrumStart=336,spectrumStep=1,spectrumCount=abc,spectrumBits=8\""), -1);
    expect("spectrumCount=343abc (trailing garbage)", load_once(dir, "spgarbage.xpsd", SP,
        "parameters=\"spectrumStart=336,spectrumStep=1,spectrumCount=343abc,spectrumBits=8\""), -1);
    expect("spectrumCount absent (SP)", load_once(dir, "spnone.xpsd", SP,
        "parameters=\"spectrumStart=336,spectrumStep=1,spectrumBits=8\""), -1);
    expect("DR3 no spectrum (absent ok)", load_once(dir, "dr3.xpsd", DR3, ""), 0);

    if (g_fail) {
        fprintf(stderr, "xpsd_spectrum_count_bounds: %d failure(s)\n", g_fail);
        return 1;
    }
    fprintf(stderr, "xpsd_spectrum_count_bounds: PASS\n");
    return 0;
}
