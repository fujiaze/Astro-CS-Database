/* B13-R13-5 共址测试: gaia_client.c byte_unshuffle malloc 失败静默忽略。
 *
 * 修复前: uint8_t *tmp = malloc(data_len); if (!tmp) return; —— 分配失败
 * 静默返回, 调用方 (read_leaf_block 两处) 把未逆置换的字节序当合法科学
 * 数据继续解析 = 静默数据损坏。
 * 修复后: 返回码协议 (0 成功 / -1 分配失败), 两调用点非 0 → fprintf+NULL。
 *
 * 技术: #include 源文件以访问 static byte_unshuffle; OOM 注入用
 * malloc(SIZE_MAX) —— C 标准规定该尺寸分配必须失败返回 NULL, 无需
 * malloc interposition (hook 转发在 glibc 有递归陷阱)。
 */
#include "gaia_client.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

/* ---- roundtrip: 列主序 [a0..a2|b0..b2|c0..c2|d0..d2] → 原始 3 记录 ×4 字段 ---- */
static void test_roundtrip(void) {
    uint8_t shuffled[12] = {0,1,2,  10,11,12,  20,21,22,  30,31,32};
    const uint8_t expect[12] = {0,10,20,30, 1,11,21,31, 2,12,22,32};
    uint8_t buf[12];
    memcpy(buf, shuffled, 12);
    int rc = byte_unshuffle(buf, 12, 4);
    CHECK(rc == 0);
    CHECK(memcmp(buf, expect, 12) == 0);
}

/* ---- 无操作边界: item_size<=1 / data_len==0 / n==0 → rc=0 且数据原样 ---- */
static void test_noop_boundaries(void) {
    uint8_t data[4] = {9, 8, 7, 6};
    uint8_t ref[4];
    memcpy(ref, data, 4);

    CHECK(byte_unshuffle(data, 4, 1) == 0);   /* item_size<=1 */
    CHECK(memcmp(data, ref, 4) == 0);
    CHECK(byte_unshuffle(data, 4, 0) == 0);
    CHECK(memcmp(data, ref, 4) == 0);
    CHECK(byte_unshuffle(data, 0, 4) == 0);   /* data_len==0 */
    CHECK(memcmp(data, ref, 4) == 0);
    CHECK(byte_unshuffle(data, 3, 4) == 0);   /* n=3/4=0 → 无完整记录 */
    CHECK(memcmp(data, ref, 4) == 0);
    /* 完整倍数长度才逆置换: data_len=8, item_size=2, n=4 (记录2字节);
     * 字段0={0,1,10,11}, 字段1={20,21,30,31} → 记录 [0,20][1,21][10,30][11,31] */
    uint8_t d2[8] = {0,1, 10,11, 20,21, 30,31};
    CHECK(byte_unshuffle(d2, 8, 2) == 0);
    const uint8_t e2[8] = {0,20, 1,21, 10,30, 11,31};
    CHECK(memcmp(d2, e2, 8) == 0);
}

/* ---- OOM 注入: malloc(SIZE_MAX) 按标准必失败 → rc=-1 (修复前静默), 数据不写 ---- */
static void test_oom_returns_error(void) {
    uint8_t data[12] = {0,1,2, 10,11,12, 20,21,22, 30,31,32};
    const uint8_t ref[12] = {0,1,2, 10,11,12, 20,21,22, 30,31,32};
    const int rc = byte_unshuffle(data, (size_t)-1, 4);
    CHECK(rc == -1);                   /* 关键断言: 失败必须可见 */
    CHECK(memcmp(data, ref, 12) == 0); /* 失败路径不得写 data */
}

int main(void) {
    test_roundtrip();
    test_noop_boundaries();
    test_oom_returns_error();
    if (failures == 0) {
        printf("B13-R13-5 GAIA UNSHUFFLE TESTS PASS\n");
        return 0;
    }
    fprintf(stderr, "B13-R13-5 GAIA UNSHUFFLE TESTS FAIL (%d)\n", failures);
    return 1;
}
