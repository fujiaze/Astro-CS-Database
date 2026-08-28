/* AstroCS 公共 C ABI v1 (05 §4 / docs/api/COMMON_ABI_V1.md) — ABI-001 实现
 * 单一头文件: C11 与 C++17 双可编译; 禁 STL/异常/RTTI 跨边界。
 * 合同: 结构前两字段恒 struct_size+abi_version(handshake, 失配即拒); 内存分配方释放
 * 或全经 host allocator; 并发合同逐函数注释(可重入/线程安全/内部并行/嵌套并行)。
 */
#ifndef ASTROCS_COMMON_ABI_V1_H
#define ASTROCS_COMMON_ABI_V1_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ACS_ABI_VERSION_V1 1u

/* ───────── 基础 POD(逐字段单位/所有权为合同一部分) ───────── */

typedef struct acs_head {
    uint32_t struct_size;   /* sizeof(具体结构) */
    uint32_t abi_version;   /* ACS_ABI_VERSION_V1 */
} acs_head;

typedef struct acs_span_f32 {
    float*   data;          /* 所有权=外部分配方; 边界内不释放 */
    uint64_t count;         /* 元素数(非字节) */
} acs_span_f32;

typedef struct acs_span_f64 {
    double*  data;
    uint64_t count;
} acs_span_f64;

typedef struct acs_span_u8 {
    uint8_t* data;
    uint64_t count;
} acs_span_u8;

/* opaque handle: 生命周期仅经 create/destroy 对; 内部实现不可见 */
typedef struct acs_handle_s* acs_handle;

/* 结构化错误码(禁异常); 数值稳定, v1 冻结 */
typedef enum acs_status {
    ACS_OK = 0,
    ACS_ERR_PARAM = 1,
    ACS_ERR_ABI_MISMATCH = 2,
    ACS_ERR_NOMEM = 3,
    ACS_ERR_IO = 4,
    ACS_ERR_UNSUPPORTED = 5,
    ACS_ERR_CANCELLED = 6,
    ACS_ERR_STATE = 7,
    ACS_ERR_BUDGET = 8,
    ACS_ERR_SELFTEST = 9,
    ACS_ERR_INTERNAL = 70   /* 未分类; 等价 CLI 退出码 70 语义 */
} acs_status;

/* 日志级别 */
enum { ACS_LOG_DEBUG = 0, ACS_LOG_INFO = 1, ACS_LOG_WARN = 2, ACS_LOG_ERROR = 3 };

/* ───────── host services(宿主注入, backend 只持指针) ───────── */

/* allocator: 所有跨边界内存经此或"分配方释放"(逐函数标注); 计数可验证 */
typedef struct acs_allocator {
    uint32_t struct_size;
    uint32_t abi_version;
    void* (*alloc)(void* ud, uint64_t size, uint64_t align);  /* 失败返 NULL; align 为 2 的幂 */
    void  (*free)(void* ud, void* p);                         /* p=NULL 允许(空操作) */
    void* user_data;
} acs_allocator;

/* logger: 线程安全由宿主保证; backend 内部并行时可直接并发调用 */
typedef struct acs_logger {
    uint32_t struct_size;
    uint32_t abi_version;
    void (*log)(void* ud, int level, const char* component, const char* msg);
    void* user_data;
} acs_logger;

/* cancel: 单向置位(宿主→backend); backend 只读轮询, 在 ALG 5c 冻结的安全点检查 */
typedef struct acs_cancel {
    uint32_t struct_size;
    uint32_t abi_version;
    int (*is_cancelled)(void* ud);   /* 0/1, 原子读 */
    void* user_data;
} acs_cancel;

/* thread budget: 只读快照+原子租借(ARCH-004 §1); backend 禁自建线程池 */
typedef struct acs_thread_budget {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t available_cpus;         /* affinity ∩ cgroup ∩ Job Object */
    uint32_t max_workers;            /* 本次调用允许的 worker 上限 */
    int (*acquire)(void* ud, uint32_t n);   /* 原子租借 n 个 worker; 0=成功, 非 0=预算不足 */
    void (*release)(void* ud, uint32_t n);  /* 归还 */
    void* user_data;
} acs_thread_budget;

typedef struct astrocs_host_services_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    acs_allocator     allocator;
    acs_logger        logger;
    acs_cancel        cancel;
    acs_thread_budget budget;
} astrocs_host_services_v1;

/* ───────── kernel 注册表(05 §5 粒度) ───────── */

/* precision/determinism_class 枚举(v1 冻结) */
enum { ACS_PRECISION_F32 = 0, ACS_PRECISION_F64 = 1 };
enum { ACS_DET_BITWISE = 0, ACS_DET_FIXED_ORDER = 1, ACS_DET_THREADLOCAL_MERGE = 2 };

typedef struct astrocs_kernel_entry_v1 {
    char     science_contract_id[32];  /* 如 "ALG-P3-003"; NUL 结尾 */
    char     algorithm_id[32];         /* 如 "drizzle-accumulate" */
    char     kernel_version[16];       /* 如 "1.0.0" */
    uint8_t  precision;                /* ACS_PRECISION_* */
    uint8_t  determinism_class;        /* ACS_DET_* */
    /* v1 通用签名: params/in 所有权=调用方, out 由调用方分配; io 不得重叠(除标注 in-place) */
    acs_status (*fn)(const astrocs_host_services_v1* host,
                     const void* params, uint32_t params_bytes,
                     const void* in, void* out);
} astrocs_kernel_entry_v1;

/* ───────── backend API(05 §4) ───────── */

typedef struct astrocs_backend_api_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    char     backend_id[48];           /* 如 "baseline" */
    char     backend_build_id[48];     /* 编译指纹(编译器/构建类型) */
    char     backend_sha256[65];       /* 文件 hash(ABI-002 manifest 填充), 未知为空串 */
    uint64_t required_features;        /* required CPU feature bits */
    uint64_t detected_features;        /* 加载时检测(ABI-002) */
    uint32_t alignment_bytes;          /* io 缓冲建议对齐 */
    uint8_t  precision_class;          /* ACS_PRECISION_* */
    uint8_t  determinism_class;        /* ACS_DET_* */
    uint8_t  aliasing_contract;        /* 0=in/out 不重叠 */
    uint8_t  nested_parallel_allowed;  /* 恒 0(ARCH-004 §3 禁嵌套) */
    uint32_t kernel_count;
    const astrocs_kernel_entry_v1* kernels;   /* 静态表, 所有权=backend */
    acs_status (*self_test)(const astrocs_host_services_v1* host);  /* 失败→5, 不得运行 */
    acs_status (*warmup)(const astrocs_host_services_v1* host);
    acs_status (*shutdown)(const astrocs_host_services_v1* host);
} astrocs_backend_api_v1;

/* 唯一入口(05 §4 示意): handshake 失配返回 ACS_ERR_ABI_MISMATCH, 不猜布局。
 * 并发合同: reentrant=yes; threadsafe=yes(只填静态表); internal_parallel=none。 */
int astrocs_backend_get_api_v1(uint32_t host_abi_version,
                               uint32_t host_struct_size,
                               const astrocs_host_services_v1* host,
                               astrocs_backend_api_v1* out_api);

/* ABI 边界验证入口(测试/自检用, 非科学接口): 内部异常被捕获转 acs_status,
 * 证明异常不跨边界。reentrant=yes; threadsafe=yes。 */
int astrocs_abi_boundary_probe(int mode);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASTROCS_COMMON_ABI_V1_H */
