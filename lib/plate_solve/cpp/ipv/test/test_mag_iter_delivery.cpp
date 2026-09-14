// ============================================================================
// test_mag_iter_delivery.cpp - P14-N-10 (RQS V2-N-10 缺陷 1) 交付面回归锁
//
// 锁: 极限星等割线迭代的**失效面必须落在交付结构 StarSelection**——
//   converged / query_failed / capped / n_queries(=m_lim_iterations) /
//   mag_lim_final / alpha 全部可观测。
// 旧实现只把 capped/alpha/m_lim_final/query_count 写进 StarSelection,
// converged 与 query_failed 丢失, 且触顶被记成 converged=true。
//
// 被测面: ipv::mag_iter_apply_to_selection (ipv_select.cpp 生产源, 经
// astrocs_p1_ipv 链接); 生产 4 条 ipv_select 路径都经 gaia_query_mag_iterative
// 调它 —— 本锁直接验证该唯一映射点。
// 编译面与 test_mag_iter.cpp 同 (不依赖网络/Gaia 数据/真实图像)。
// ============================================================================
#include "ipv_select.h"

#include <cstdio>
#include <cstring>
#include <cstdint>

using namespace ipv;

static int g_fail = 0;
static int g_check = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++g_check;                                                             \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);    \
            ++g_fail;                                                          \
        } else {                                                               \
            std::printf("ok  : %s\n", (msg));                                  \
        }                                                                      \
    } while (0)

static uint64_t bits(double v) {
    uint64_t u = 0;
    std::memcpy(&u, &v, sizeof(u));
    return u;
}

// 收敛成功路径: 全部字段逐项落到交付面。
static void test_mapping_converged() {
    StarSelection sel{};
    MagIterOutcome mi;
    mi.m_lim_final = 12.9850849855;
    mi.query_count = 2;
    mi.n_returned = 178;
    mi.n_target = 60;
    mi.converged = true;
    mi.capped = false;
    mi.query_failed = false;
    mi.valid = true;
    mi.alpha_final = 0.2885;

    mag_iter_apply_to_selection(mi, sel);

    CHECK(bits(sel.m_lim_final) == bits(mi.m_lim_final),
          "交付面: mag_lim_final 逐位一致");
    CHECK(sel.m_lim_iterations == 2, "交付面: n_queries(=m_lim_iterations)==2");
    CHECK(sel.n_gaia_final == 178, "交付面: n_returned 落到 n_gaia_final");
    CHECK(sel.m_lim_converged, "交付面: converged=true 可见 (旧实现丢失)");
    CHECK(!sel.m_lim_capped, "交付面: capped=false 可见");
    CHECK(!sel.m_lim_query_failed, "交付面: query_failed=false 可见 (旧实现丢失)");
    CHECK(bits(sel.m_lim_alpha_final) == bits(mi.alpha_final),
          "交付面: alpha_final 逐位一致");
}

// 触顶路径: capped 可见且 converged 必须为 false (旧实现错记 true)。
static void test_mapping_capped() {
    StarSelection sel{};
    MagIterOutcome mi;
    mi.m_lim_final = 13.0;
    mi.query_count = 1;
    mi.n_returned = 200000;
    mi.converged = false;
    mi.capped = true;
    mi.query_failed = false;
    mi.valid = true;

    mag_iter_apply_to_selection(mi, sel);

    CHECK(sel.m_lim_capped, "交付面: capped=true 可见");
    CHECK(!sel.m_lim_converged, "交付面: 触顶时 converged=false 可见 (不得记 true)");
    CHECK(sel.m_lim_iterations == 1, "交付面: 触顶 n_queries==1");
}

// 查询失败路径: query_failed 可见, 末次成功结果仍交付。
static void test_mapping_query_failed() {
    StarSelection sel{};
    MagIterOutcome mi;
    mi.m_lim_final = 11.5;
    mi.query_count = 2;
    mi.n_returned = 100;
    mi.converged = false;
    mi.capped = false;
    mi.query_failed = true;
    mi.valid = true;

    mag_iter_apply_to_selection(mi, sel);

    CHECK(sel.m_lim_query_failed, "交付面: query_failed=true 可见 (旧实现丢失)");
    CHECK(!sel.m_lim_converged, "交付面: 查询失败不是收敛");
    CHECK(sel.m_lim_iterations == 2, "交付面: 失败时 n_queries==2");
    CHECK(bits(sel.m_lim_final) == bits(11.5), "交付面: 末次成功 mag_lim 仍交付");
}

int main() {
    std::printf("=== P14-N-10 mag-iter 交付面回归锁 ===\n");
    test_mapping_converged();
    test_mapping_capped();
    test_mapping_query_failed();
    std::printf("=== checks=%d fail=%d ===\n", g_check, g_fail);
    if (g_fail != 0) { std::printf("RESULT: FAIL\n"); return 1; }
    std::printf("RESULT: PASS\n");
    return 0;
}
