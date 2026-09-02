/* ABI-002 生命周期语义参考实现探针（C11 + C++17 双编译）
 *
 * 角色: 本文件是 lifecycle_v1.h 冻结语义的 reference 实现 + 自检探针, 供
 * tests/abi/test_abi002_lifecycle.py 编译执行并做穷举/负测判定; 同时是 ABI-003
 * loader 生产实现与 ABI-005 noop module 的语义对齐参考。
 *
 * 实现并导出（与 lifecycle_v1.h 声明一一对应）:
 *   acs_lc_transition_allowed_v1 / acs_lc_op_error_v1
 *   acs_abi_compat_v1 / acs_struct_ext_ok_v1 / acs_negotiate_v1
 *   acs_module_selftest_v1 / acs_lc_state_name_v1 / acs_module_op_name_v1
 *
 * 自检模式（无参数运行）: 输出 [PASS]/[FAIL] 行 + 状态机穷举表 + 汇总 ALL_OK;
 * 任一项失败返回非 0。pytest 以参数化编译/运行调用本探针。
 *
 * 纯 C 参考实现: 不含平台库; 仅依赖头族（C11/C++17 双可编译, -fno-exceptions 亦可）。
 */
#include "astrocs/abi/lifecycle_v1.h"

#include <stdio.h>
#include <string.h>

/* ═══════════════ 参考实现 ═══════════════ */

int acs_lc_transition_allowed_v1(int state, int op)
{
    /* 只受理实例级 op 与合法 state; 其余一律拒绝（含 module 级 op 误用与非法值） */
    switch (state) {
    case ACS_LC_STATE_CREATED:
        if (op == ACS_OP_EXECUTE || op == ACS_OP_INSPECT ||
            op == ACS_OP_REQUEST_CANCEL || op == ACS_OP_DESTROY) {
            return 1;
        }
        return 0;
    case ACS_LC_STATE_EXECUTING:
        if (op == ACS_OP_INSPECT || op == ACS_OP_REQUEST_CANCEL) {
            return 1;
        }
        return 0;
    case ACS_LC_STATE_CANCELLING:
        if (op == ACS_OP_INSPECT || op == ACS_OP_REQUEST_CANCEL) {
            return 1;
        }
        return 0;
    case ACS_LC_STATE_DESTROYED:
    default:
        return 0; /* DESTROYED: 无允许转移; 非法 state 拒绝 */
    }
}

acs_status acs_lc_op_error_v1(int state, int op)
{
    return acs_lc_transition_allowed_v1(state, op) ? ACS_OK : ACS_ERR_STATE;
}

int acs_abi_compat_v1(uint32_t host_abi)
{
    return host_abi == ACS_ABI_VERSION_V1 ? 1 : 0;
}

int acs_struct_ext_ok_v1(uint64_t peer_size, uint64_t self_size)
{
    if (self_size == 0u) {
        return 0; /* 自身尺寸 0 无意义（防御） */
    }
    return peer_size >= self_size ? 1 : 0;
}

acs_status acs_negotiate_v1(uint32_t host_abi,
                            uint64_t host_struct_size,
                            uint64_t api_struct_size)
{
    if (!acs_abi_compat_v1(host_abi)) {
        return ACS_ERR_ABI_MISMATCH;
    }
    if (!acs_struct_ext_ok_v1(host_struct_size, sizeof(acs_host_api_v1))) {
        return ACS_ERR_ABI_MISMATCH;
    }
    if (!acs_struct_ext_ok_v1(api_struct_size, sizeof(acs_module_api_v1))) {
        return ACS_ERR_ABI_MISMATCH;
    }
    return ACS_OK;
}

acs_status acs_module_selftest_v1(const acs_module_api_v1* api)
{
    if (api == NULL) {
        return ACS_ERR_SELFTEST;
    }
    if (api->head.abi_version != ACS_ABI_VERSION_V1) {
        return ACS_ERR_SELFTEST;
    }
    /* 必填回调非空（request_cancel 可空 = 不支持取消; 空表项按 NULL_CALLBACK 拒绝） */
    if (api->describe == NULL || api->validate_config == NULL || api->plan == NULL ||
        api->create == NULL || api->execute == NULL || api->inspect == NULL ||
        api->destroy == NULL) {
        return ACS_ERR_SELFTEST;
    }
    return ACS_OK;
}

const char* acs_lc_state_name_v1(int state)
{
    switch (state) {
    case ACS_LC_STATE_CREATED:
        return "CREATED";
    case ACS_LC_STATE_EXECUTING:
        return "EXECUTING";
    case ACS_LC_STATE_CANCELLING:
        return "CANCELLING";
    case ACS_LC_STATE_DESTROYED:
        return "DESTROYED";
    default:
        return NULL;
    }
}

const char* acs_module_op_name_v1(int op)
{
    switch (op) {
    case ACS_OP_QUERY:
        return "QUERY";
    case ACS_OP_DESCRIBE:
        return "DESCRIBE";
    case ACS_OP_VALIDATE_CONFIG:
        return "VALIDATE_CONFIG";
    case ACS_OP_PLAN:
        return "PLAN";
    case ACS_OP_SELF_TEST:
        return "SELF_TEST";
    case ACS_OP_CREATE:
        return "CREATE";
    case ACS_OP_EXECUTE:
        return "EXECUTE";
    case ACS_OP_INSPECT:
        return "INSPECT";
    case ACS_OP_REQUEST_CANCEL:
        return "REQUEST_CANCEL";
    case ACS_OP_DESTROY:
        return "DESTROY";
    default:
        return NULL;
    }
}

/* ═══════════════ 自检驱动 ═══════════════ */

static int g_fail = 0;

static void expect(int cond, const char* what)
{
    if (cond) {
        printf("[PASS] %s\n", what);
    } else {
        printf("[FAIL] %s\n", what);
        g_fail = 1;
    }
}

/* 无状态判定穷举（与 pytest 参考矩阵同源; 输出机器表供日志留档） */
static void dump_matrix(void)
{
    static const int states[] = {
        ACS_LC_STATE_CREATED, ACS_LC_STATE_EXECUTING,
        ACS_LC_STATE_CANCELLING, ACS_LC_STATE_DESTROYED
    };
    static const int ops[] = {
        ACS_OP_EXECUTE, ACS_OP_INSPECT, ACS_OP_REQUEST_CANCEL, ACS_OP_DESTROY
    };
    size_t si, oi;
    printf("state_matrix(rows=state,cols=op order EXECUTE,INSPECT,REQUEST_CANCEL,DESTROY):\n");
    for (si = 0; si < sizeof(states) / sizeof(states[0]); ++si) {
        printf("  %s:", acs_lc_state_name_v1(states[si]));
        for (oi = 0; oi < sizeof(ops) / sizeof(ops[0]); ++oi) {
            printf(" %d", acs_lc_transition_allowed_v1(states[si], ops[oi]));
        }
        printf("\n");
    }
}

static void selftest(void)
{
    /* 1. 合法调用时序被接受: CREATED 上 execute/inspect/request_cancel/destroy */
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_CREATED, ACS_OP_EXECUTE) == 1,
           "CREATED+EXECUTE allowed");
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_CREATED, ACS_OP_DESTROY) == 1,
           "CREATED+DESTROY allowed");
    expect(acs_lc_op_error_v1(ACS_LC_STATE_CREATED, ACS_OP_EXECUTE) == ACS_OK,
           "CREATED+EXECUTE -> ACS_OK");

    /* 2. 非法调用时序被拒绝（状态机性质测试核心）: */
    /*    2a. EXECUTING 上再 execute（同实例并发 execute）→ 拒绝 */
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_EXECUTING, ACS_OP_EXECUTE) == 0,
           "EXECUTING+EXECUTE rejected (同实例并发 execute)");
    /*    2b. EXECUTING 上 destroy（执行中销毁）→ 拒绝 */
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_EXECUTING, ACS_OP_DESTROY) == 0,
           "EXECUTING+DESTROY rejected (执行中销毁)");
    /*    2c. CANCELLING 上 execute → 拒绝 */
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_CANCELLING, ACS_OP_EXECUTE) == 0,
           "CANCELLING+EXECUTE rejected");
    /*    2d. DESTROYED 上一切实例 op（含 double destroy 检测）→ 拒绝 */
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_DESTROYED, ACS_OP_DESTROY) == 0,
           "DESTROYED+DESTROY rejected (double destroy 检测)");
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_DESTROYED, ACS_OP_INSPECT) == 0,
           "DESTROYED+INSPECT rejected (destroy 后访问)");
    expect(acs_lc_op_error_v1(ACS_LC_STATE_DESTROYED, ACS_OP_EXECUTE) == ACS_ERR_STATE,
           "DESTROYED+EXECUTE -> ACS_ERR_STATE");

    /* 3. 非法 state/op 值拒绝 */
    expect(acs_lc_transition_allowed_v1(0, ACS_OP_EXECUTE) == 0, "state=0 rejected");
    expect(acs_lc_transition_allowed_v1(99, ACS_OP_DESTROY) == 0, "state=99 rejected");
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_CREATED, 0) == 0, "op=0 rejected");
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_CREATED, 99) == 0, "op=99 rejected");
    expect(acs_lc_op_error_v1(0, 0) == ACS_ERR_STATE, "非法 state+op -> ACS_ERR_STATE");

    /* 4. module 级 op 不经实例状态机: 本判定拒绝（host 不得用它拦 module 级 op） */
    expect(acs_lc_transition_allowed_v1(ACS_LC_STATE_CREATED, ACS_OP_VALIDATE_CONFIG) == 0,
           "module-level op 不经实例状态机判定");

    /* 5. 状态名/操作名（非法值 NULL） */
    expect(strcmp(acs_lc_state_name_v1(ACS_LC_STATE_CREATED), "CREATED") == 0,
           "state name CREATED");
    expect(acs_lc_state_name_v1(0) == NULL, "illegal state name NULL");
    expect(strcmp(acs_module_op_name_v1(ACS_OP_DESTROY), "DESTROY") == 0,
           "op name DESTROY");
    expect(acs_module_op_name_v1(0) == NULL, "illegal op name NULL");

    /* 6. ABI major/host_abi 协商负测: 唯一合法 v1 值 = 1 */
    expect(acs_abi_compat_v1(1u) == 1, "host_abi=1 compatible");
    expect(acs_abi_compat_v1(2u) == 0, "host_abi=2 rejected (major mismatch)");
    expect(acs_abi_compat_v1(0u) == 0, "host_abi=0 rejected");
    expect(acs_abi_compat_v1(0xFFFFFFFFu) == 0, "host_abi garbage rejected");

    /* 7. struct_size 旧/新尾部扩展兼容 */
    expect(acs_struct_ext_ok_v1(sizeof(acs_host_api_v1), sizeof(acs_host_api_v1)) == 1,
           "peer==self struct_size compatible");
    expect(acs_struct_ext_ok_v1(sizeof(acs_host_api_v1) + 32u, sizeof(acs_host_api_v1)) == 1,
           "peer newer (尾部扩展) compatible");
    expect(acs_struct_ext_ok_v1(sizeof(acs_host_api_v1) - 8u, sizeof(acs_host_api_v1)) == 0,
           "peer older (缺尾部字段) rejected");
    expect(acs_negotiate_v1(1u, sizeof(acs_host_api_v1), sizeof(acs_module_api_v1)) == ACS_OK,
           "negotiate full v1 ok");
    expect(acs_negotiate_v1(2u, sizeof(acs_host_api_v1), sizeof(acs_module_api_v1)) ==
               ACS_ERR_ABI_MISMATCH,
           "negotiate host_abi=2 -> ABI_MISMATCH");
    expect(acs_negotiate_v1(1u, sizeof(acs_host_api_v1) + 16u, sizeof(acs_module_api_v1)) ==
               ACS_OK,
           "negotiate host newer tail-ext ok");
    expect(acs_negotiate_v1(1u, sizeof(acs_host_api_v1) - 16u, sizeof(acs_module_api_v1)) ==
               ACS_ERR_ABI_MISMATCH,
           "negotiate host older -> ABI_MISMATCH");

    /* 8. self_test: 空表/空回调/缺回调 → SELFTEST; 完整表 → OK */
    expect(acs_module_selftest_v1(NULL) == ACS_ERR_SELFTEST, "selftest NULL api -> SELFTEST");

    {
        static acs_module_api_v1 g_api;
        (void)memset(&g_api, 0, sizeof(g_api));
        g_api.head.struct_size = sizeof(acs_module_api_v1);
        g_api.head.abi_version = ACS_ABI_VERSION_V1;
        expect(acs_module_selftest_v1(&g_api) == ACS_ERR_SELFTEST,
               "selftest 空回调表 -> SELFTEST (空回调负测)");
        /* 填满必填回调后通过（request_cancel 可空） */
        g_api.describe = (acs_status (*)(const acs_module_api_v1*, acs_str_v1,
                                         acs_module_descriptor_v1*))1;
        g_api.validate_config = (acs_status (*)(const acs_module_api_v1*, acs_str_v1,
                                                acs_error_info_v1*))1;
        g_api.plan = (acs_status (*)(const acs_module_api_v1*, acs_str_v1, acs_str_v1,
                                     acs_strbuf_v1*, acs_error_info_v1*))1;
        g_api.create = (acs_status (*)(const acs_module_api_v1*, acs_str_v1,
                                       const acs_host_api_v1*, acs_module_instance_v1**,
                                       acs_error_info_v1*))1;
        g_api.execute = (acs_status (*)(acs_module_instance_v1*, acs_str_v1, acs_str_v1,
                                        acs_strbuf_v1*, acs_error_info_v1*))1;
        g_api.inspect = (acs_status (*)(const acs_module_instance_v1*, acs_strbuf_v1*,
                                        acs_error_info_v1*))1;
        g_api.request_cancel = NULL; /* 可选 */
        g_api.destroy = (void (*)(acs_module_instance_v1*))1;
        expect(acs_module_selftest_v1(&g_api) == ACS_OK, "selftest 完整必填回调 -> OK");
        g_api.destroy = NULL;
        expect(acs_module_selftest_v1(&g_api) == ACS_ERR_SELFTEST,
               "selftest 缺 destroy -> SELFTEST");
    }

    dump_matrix();
}

int main(void)
{
    printf("abi002_lifecycle_probe selftest begin\n");
    selftest();
    if (g_fail) {
        printf("RESULT: FAIL\n");
        return 1;
    }
    printf("RESULT: ALL_OK\n");
    return 0;
}
