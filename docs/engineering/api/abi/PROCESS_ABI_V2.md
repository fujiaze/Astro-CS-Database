# ARS-02｜Process ABI v2 头级冻结与 Calibration 双 Operation 纵向切片

**级别：可编译的目标 ABI 参考合同（ABI design frozen for ARS-02 first slice）；不是已安装头文件、动态库或生产执行能力。** 2026-10-16 以远端 main `14721442de7b4e1b34cc4f35a465c9e7376b3332`、[#6](https://github.com/fujiaze/Astro-CS-Database/issues/6) 复审、[#24](https://github.com/fujiaze/Astro-CS-Database/issues/24) ARS-02 与现有 [ARS 总体架构](../../architecture/PROCESS_RUNTIME_ARS.md)、[可编码框架](../../architecture/PROCESS_RUNTIME_DETAILED_DESIGN.md)、[公共 ABI](ABI.md) 为依据。以下 **单个 C11 代码块就是唯一 v2 头部签名权威**，实现者将其拆为 `status_codes_v2.h → typed_store_v2.h → host_api_v2.h → module_api_v2.h`，保留全部字段顺序、值、调用约定、结构尺寸；不能再次自行重选接口。测试时也可以直接抽出代码块作为单头编译。科学量/合法输入/功能范围受 [CALIBRATION](../../../science/calibration/CALIBRATION.md) 与 [Normalize modules](../../../detail/normalize/modules.md) 约束。

## 1. 本次明确选定、不再让执行者选择的原则

1. **第一纵向切片的 module**：现有 `lib/algorithms/calibration/` 的 `acsd_p1_calibration`。该 target 在 `lib/algorithms/calibration/CMakeLists.txt` 中已经通过 `src/module_entry.cpp` 导出 `acsd_module_query_v1`，从生产 `src/{ac_api,calibrator,cosmetic_corrector,master_generator}.cpp` 编译。新 v2 导出**同一 DLL 内**并存 `acsd_module_query_v1`（完全保留）和 `acsd_module_query_v2`（新增）；Linux version-script、Windows `.def` 与 secure loader 的导出白名单为这两个已协商符号，legacy 科学函数仍非对外暴露。现有 v1 虽在 config 中列出十个操作，但只有一个 module 级 execute，**不能冒充 v2 的双 Operation 表**。
2. **仅两个固定 op**：`module_id=acsd.p1.calibration`、`operation_id=calibrate_frame` 与 `operation_id=correct_frame`；`operation_at(0/1)` 分别返回校准/坏点操作，以 `schema_version=2` 登记。底层分别调用真实 `ac_calibrate_frame` 和 `ac_correct_frame`（**FP32，不暗中提升成 f64**）。两个 v2 内核嵌入正式 Normalize `cal → cos` 科学生产链，而不是另造“演示滤波器”。现有 CLI 注册表使用 `acsd.phase1.calibration` 与 `acsd.phase1.cosmetic` **两个旧 node-module_id**；runtime_client 的显式适配表把它们分别映射到同一 DLL 的 `(acsd.p1.calibration, calibrate_frame)` 和 `(acsd.p1.calibration, correct_frame)`，不暗中改变旧 module_id/现有 pipeline IR 合同。其他同 module 的既有操作继续走 v1，待该纵向切片独立验证后再逐个迁移。尤其 `p1_op_cosmetic` 还调用 **`ac_correct_columns_ex2` 坏列修复及其标记/统计逻辑**，首个 v2 `correct_frame` 仅替换原坏点修复内核，坏列处理仍经现有经预算控制的科学调用序执行，绝不可直接跳过或当成等价新节点。
3. **并发唯一策略**：同一 `acsd2_operation_instance` **绝对串行执行**，`partition` 在第一纵向切片只能生成一个 `FULL_FRAME` work unit；不同 Frame 的不同 instance 可以并行但每个由 Host 分配恰好一个 CPU worker，内部 OMP 限制到 1（具体通过同 worker 的 `ac_set_num_threads(1)`），不在实例内自行 spawn/等待线程。不采用“线程安全共享同实例”或“每分片 instance”二选一。取消在单次同步科学 C API 返回后的安全点响应，不谎称能抢占不可中断内核。
4. **数据/内存边界**：Host 创建并持有 `writer` 与 `ReadLease` 的实际所有权；进 DLL 的图像仅是借用 view，返回前 release；写出指针只在 writer map→unmap 期间有效。`writer_publish` 只有 Runtime 在 `execute_unit` 与 `reduce` 均成功且未取消、所有任务静止时调用；失败一律 `writer_abort`，不会发布半成品。第一纵切片中插件不直接 submit AIO；完整 Frame revision/persistent manifest 仍归后续 ARS-07/08。
5. **ABI v1 共存**：v1 query 所接收的 Host 仅允许在 query 期间使用，v2 query 也如此；真正持有 Host v2 服务表只发生在 `create`，并通过 Host 和 DLL pin 保证到 destroy/回调/ticket 收尾。v1 `acsd_module_query_v1` 的静态表/manifest 语义不因 v2 增加而被重写；需要 v1→v2 的 Base64 解码/复制桥时按照额外物理 allocation 独立计费，不能称零拷贝。`common_abi_v1.h` 不与新模块化 ABI 共同 include。
6. **ARS-02 的范围**：冻结 API 和“可从 CLI 验证的真实双 Operation”工程合同；**不宣称** #23 资源/科学基线、ARS-03 typed DAG、ARS-04 完整 BlockStore、ARS-05 严格外部 C++ 分配记账、ARS-06 Frame rollback 或全 HiPS revision 发布已实现。它们是独立后续工作，先维持现行 CLI 默认科学等价。

## 2. 可编译 C11 头（唯一冻结签名）

注意：为了完全避免 v1 同名类型在单个 TU 中的定义冲突，本 v2 基础类型全部使用 `acsd2_` 命名。`acsd2_status` 的整数值与现有模块化 v1 `acsd_status` 对应值相同，但**类型并不混用**；唯一外部新增入口仍命名 `acsd_module_query_v2`。64 位目标只允许 Windows amd64 / Linux amd64，遵循各平台默认 struct alignment，禁止 `#pragma pack`。本 C 头可作为独立 `C11`/`C++17` 编译单元，不需要 v1 include。

```c
/* ACSD Process ABI v2 / ARS-02: frozen target C11 header specification.
 * DESIGN REFERENCE ONLY: not installed, not a production implementation.
 * Standalone; never include v1 module headers or legacy common_abi_v1.h here.
 */
#ifndef ACSD_PROCESS_ABI_V2_REFERENCE_H
#define ACSD_PROCESS_ABI_V2_REFERENCE_H
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32)
# define ACSD2_CALL __cdecl
# if defined(ACSD2_BUILD_MODULE)
#  define ACSD2_EXPORT __declspec(dllexport)
# else
#  define ACSD2_EXPORT __declspec(dllimport)
# endif
#else
# define ACSD2_CALL
# define ACSD2_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define ACSD2_ABI_MAJOR 2u
#define ACSD2_API_REQUIRED_FLAGS 0u
#define ACSD2_CAP_STORE          (UINT64_C(1)<<0)
#define ACSD2_CAP_CREDITS        (UINT64_C(1)<<1)
#define ACSD2_CAP_SCRATCH        (UINT64_C(1)<<2)
#define ACSD2_CAP_LOGGER         (UINT64_C(1)<<3)
#define ACSD2_CAP_CANCEL         (UINT64_C(1)<<4)
#define ACSD2_CAP_AIO           (UINT64_C(1)<<5)
#define ACSD2_CAP_TASKS         (UINT64_C(1)<<6)
#define ACSD2_CAP_CATALOG       (UINT64_C(1)<<7)
#define ACSD2_CAP_KERNEL        (UINT64_C(1)<<8)
#define ACSD2_REQUIRED_SLICE_CAPS (ACSD2_CAP_STORE|ACSD2_CAP_CREDITS|ACSD2_CAP_CANCEL)
#define ACSD2_SCOPE_FRAME 1u
#define ACSD2_DTYPE_F32 1u
#define ACSD2_DTYPE_F64 2u
#define ACSD2_PLAN_BOUNDED 1u
#define ACSD2_PLAN_UNKNOWN 2u
#define ACSD2_OP_INSTANCE_SERIAL 1u
#define ACSD2_UNIT_FULL_FRAME 1u
#define ACSD2_PORT_INPUT 1u
#define ACSD2_PORT_OUTPUT 2u
#define ACSD2_PORT_OPTIONAL 1u
#define ACSD2_COORD_PIXEL_0 1u
#define ACSD2_CREDIT_OUTPUT 1u
#define ACSD2_CREDIT_SCRATCH 2u
#define ACSD2_CREDIT_AIO 3u
#define ACSD2_IO_PENDING 1u
#define ACSD2_IO_COMPLETED 2u
#define ACSD2_IO_CANCELLED 3u
#define ACSD2_IO_FAILED 4u

typedef enum acsd2_status {
  ACSD2_OK=0, ACSD2_ERR_PARAM=1, ACSD2_ERR_ABI_MISMATCH=2,
  ACSD2_ERR_NOMEM=3, ACSD2_ERR_IO=4, ACSD2_ERR_UNSUPPORTED=5,
  ACSD2_ERR_CANCELLED=6, ACSD2_ERR_STATE=7, ACSD2_ERR_BUDGET=8,
  ACSD2_ERR_SELFTEST=9, ACSD2_ERR_EXCEPTION=10, ACSD2_ERR_INTERNAL=70
} acsd2_status;
/* These integer values intentionally agree with the modular v1 status family. */
typedef struct acsd2_head {uint32_t struct_size; uint32_t abi_version;} acsd2_head;
typedef struct acsd2_str {acsd2_head head; const char *data; uint64_t size;} acsd2_str;
typedef struct acsd2_strbuf {acsd2_head head; char *data; uint64_t capacity; uint64_t size;} acsd2_strbuf;
typedef struct acsd2_error {
  acsd2_head head; int32_t status; int32_t domain;
  uint32_t detail_code; uint32_t reserved; acsd2_str message;
} acsd2_error;
typedef struct acsd2_call {
  acsd2_head head; acsd2_str run_id; acsd2_str scope_id;
  acsd2_str occurrence_id; acsd2_str node_id; acsd2_str operation_id;
  uint64_t attempt_id;
  uint64_t cancel_generation; uint32_t scope_kind; uint32_t worker_grant;
} acsd2_call;
typedef struct acsd2_tensor_meta {
  acsd2_head head; uint64_t width; uint64_t height; uint64_t row_stride_bytes;
  uint32_t dtype; uint32_t flags; acsd2_str schema_id; acsd2_str unit;
  acsd2_str coordinate_id; uint32_t pixel_origin; uint32_t status;
} acsd2_tensor_meta;
typedef struct acsd2_const_view {
  acsd2_head head; const uint8_t *data; uint64_t size_bytes;
  acsd2_tensor_meta meta;
} acsd2_const_view;
typedef struct acsd2_mut_view {
  acsd2_head head; uint8_t *data; uint64_t size_bytes;
  acsd2_tensor_meta meta;
} acsd2_mut_view;
typedef struct acsd2_plan {
  acsd2_head head; uint64_t work_units; uint64_t output_bytes;
  uint64_t scratch_fixed_bytes; uint64_t scratch_per_worker_bytes;
  uint64_t aio_new_bytes; uint64_t staging_new_bytes;
  uint32_t min_workers; uint32_t max_workers;
  uint32_t estimate_kind; uint32_t partition_axis;
} acsd2_plan;
typedef struct acsd2_work_unit {
  acsd2_head head; uint64_t ordinal; uint64_t begin; uint64_t end;
  uint32_t axis; uint32_t reserved;
} acsd2_work_unit;

/* Handles are Host-allocated and strictly opaque across the DLL. */
typedef struct acsd2_read_lease_s acsd2_read_lease;
typedef struct acsd2_writer_s acsd2_writer;
typedef struct acsd2_block_s acsd2_block;
typedef struct acsd2_grant_s acsd2_grant;
typedef struct acsd2_scratch_s acsd2_scratch;
typedef struct acsd2_io_ticket_s acsd2_io_ticket;
typedef struct acsd2_task_s acsd2_task;
typedef struct acsd2_operation_instance_s acsd2_operation_instance;

/* Context points at a Host-managed lifetime object, not the DLL's storage. */
typedef struct acsd2_credit_service {
  acsd2_head head; void *context;
  acsd2_status (ACSD2_CALL *reserve)(void *, const acsd2_call *, uint64_t bytes,
                   uint32_t purpose, acsd2_grant **out, acsd2_error *err);
  acsd2_status (ACSD2_CALL *grant_bytes)(void *, const acsd2_grant *,
                   uint64_t *out_bytes, acsd2_error *err);
  void (ACSD2_CALL *grant_release)(void *, acsd2_grant *);
} acsd2_credit_service;

typedef struct acsd2_scratch_service {
  acsd2_head head; void *context;
  acsd2_status (ACSD2_CALL *allocate)(void *, acsd2_grant *grant,
                   uint64_t bytes, uint64_t alignment, acsd2_scratch **out,
                   acsd2_error *err);
  acsd2_status (ACSD2_CALL *map)(void *, acsd2_scratch *,
                   uint8_t **out_ptr, uint64_t *out_len, acsd2_error *err);
  acsd2_status (ACSD2_CALL *unmap)(void *, acsd2_scratch *, acsd2_error *err);
  void (ACSD2_CALL *release)(void *, acsd2_scratch *);
} acsd2_scratch_service;

typedef struct acsd2_store_service {
  acsd2_head head; void *context;
  acsd2_status (ACSD2_CALL *borrow)(void *, const acsd2_call *, acsd2_str input_port,
                   acsd2_read_lease **out, acsd2_error *err);
  acsd2_status (ACSD2_CALL *read_view)(void *, const acsd2_read_lease *,
                   acsd2_const_view *out, acsd2_error *err);
  void (ACSD2_CALL *read_release)(void *, acsd2_read_lease *);
  acsd2_status (ACSD2_CALL *begin_output)(void *, const acsd2_call *,
                   acsd2_str output_port, const acsd2_tensor_meta *,
                   acsd2_grant *grant, acsd2_writer **out, acsd2_error *err);
  acsd2_status (ACSD2_CALL *writer_map)(void *, acsd2_writer *,
                   acsd2_mut_view *out, acsd2_error *err);
  acsd2_status (ACSD2_CALL *writer_unmap)(void *, acsd2_writer *, acsd2_error *err);
  acsd2_status (ACSD2_CALL *writer_publish)(void *, acsd2_writer *,
                   acsd2_block **out, acsd2_error *err);
  void (ACSD2_CALL *writer_abort)(void *, acsd2_writer *);
  void (ACSD2_CALL *block_release)(void *, acsd2_block *);
} acsd2_store_service;

typedef struct acsd2_cancel_service {
  acsd2_head head; void *context;
  uint32_t (ACSD2_CALL *is_cancelled)(void *, const acsd2_call *);
} acsd2_cancel_service;

typedef struct acsd2_logger_service {
  acsd2_head head; void *context;
  void (ACSD2_CALL *log)(void *, uint32_t severity, acsd2_str component, acsd2_str message);
} acsd2_logger_service;

/* AIO is optional for the ARS-02 first slice; required for ARS-07/08. */
typedef struct acsd2_aio_service {
  acsd2_head head; void *context;
  acsd2_status (ACSD2_CALL *write_immutable)(void *, const acsd2_call *,
                   const acsd2_block *, acsd2_str artifact_key,
                   acsd2_grant *slot_grant, acsd2_io_ticket **out, acsd2_error *err);
  acsd2_status (ACSD2_CALL *wait)(void *, acsd2_io_ticket *, uint32_t timeout_ms,
                   uint32_t *out_state, acsd2_error *err);
  acsd2_status (ACSD2_CALL *request_cancel)(void *, acsd2_io_ticket *, acsd2_error *err);
  void (ACSD2_CALL *ticket_release)(void *, acsd2_io_ticket *);
} acsd2_aio_service;

typedef void (ACSD2_CALL *acsd2_task_entry)(void *user_data);
typedef struct acsd2_tasks_service {
  acsd2_head head; void *context;
  acsd2_status (ACSD2_CALL *enqueue)(void *, const acsd2_call *,
                acsd2_task_entry fn, void *user_data, acsd2_task **out, acsd2_error *err);
  acsd2_status (ACSD2_CALL *await)(void *, acsd2_task *,
                uint32_t timeout_ms, acsd2_error *err);
  void (ACSD2_CALL *task_release)(void *, acsd2_task *);
} acsd2_tasks_service;

typedef struct acsd2_host_api {
  acsd2_head head; uint64_t capabilities;
  const acsd2_credit_service *credits;  /* required for first slice */
  const acsd2_store_service *store;      /* required for first slice */
  const acsd2_cancel_service *cancel;    /* required for first slice */
  const acsd2_scratch_service *scratch;  /* optional; NULL iff bit absent */
  const acsd2_logger_service *logger;    /* optional */
  const acsd2_aio_service *aio;          /* optional */
  const acsd2_tasks_service *tasks;      /* optional */
} acsd2_host_api;

typedef struct acsd2_port_desc {
  acsd2_head head; acsd2_str name; acsd2_str schema_id; acsd2_str unit;
  acsd2_str coordinate_id; uint32_t dtype; uint32_t direction;
  uint32_t flags; uint32_t pixel_origin;
} acsd2_port_desc;
typedef struct acsd2_operation_desc {
  acsd2_head head;
  acsd2_str operation_id;
  const acsd2_port_desc *ports; /* static table pinned with module */
  uint32_t port_count; uint32_t schema_version;
  uint32_t instance_flags; uint32_t reserved;
  uint64_t required_host_caps;
} acsd2_operation_desc;

typedef struct acsd2_operation_api {
  acsd2_head head;
  acsd2_status (ACSD2_CALL *validate_config)(acsd2_str config, acsd2_error *err);
  acsd2_status (ACSD2_CALL *plan)(const acsd2_host_api *,
                 const acsd2_call *, const acsd2_tensor_meta *input,
                 acsd2_str config, acsd2_plan *out, acsd2_error *err);
  acsd2_status (ACSD2_CALL *create)(const acsd2_host_api *, acsd2_str config,
                 acsd2_operation_instance **out, acsd2_error *err);
  acsd2_status (ACSD2_CALL *partition)(acsd2_operation_instance *,
                 const acsd2_tensor_meta *input, acsd2_work_unit *out,
                 uint64_t capacity, uint64_t *required, acsd2_error *err);
  acsd2_status (ACSD2_CALL *execute_unit)(acsd2_operation_instance *,
                 const acsd2_call *, const acsd2_work_unit *,
                 acsd2_writer *output_writer, acsd2_error *err);
  acsd2_status (ACSD2_CALL *reduce)(acsd2_operation_instance *,
                 const acsd2_call *, acsd2_writer *output_writer,
                 acsd2_error *err);
  acsd2_status (ACSD2_CALL *result_meta)(acsd2_operation_instance *,
                 acsd2_strbuf *out, acsd2_error *err);
  acsd2_status (ACSD2_CALL *request_cancel)(acsd2_operation_instance *);
  void (ACSD2_CALL *destroy)(acsd2_operation_instance *);
} acsd2_operation_api;

typedef struct acsd2_module_api {
  acsd2_head head; acsd2_str module_id; acsd2_str semver; acsd2_str build_digest;
  uint32_t operation_count; uint32_t reserved;
  acsd2_status (ACSD2_CALL *operation_at)(uint32_t index,
                 const acsd2_operation_desc **out_descriptor,
                 const acsd2_operation_api **out_api, acsd2_error *err);
} acsd2_module_api;

ACSD2_EXPORT acsd2_status ACSD2_CALL
acsd_module_query_v2(uint32_t host_abi, const acsd2_host_api *host,
                     const acsd2_module_api **out_api);

#if defined(__cplusplus)
# define ACSD2_ASSERT(C,M) static_assert((C),M)
#else
# define ACSD2_ASSERT(C,M) _Static_assert((C),M)
#endif
#if UINTPTR_MAX == UINT64_MAX
ACSD2_ASSERT(sizeof(acsd2_head)==8,"v2 head layout");
ACSD2_ASSERT(sizeof(acsd2_str)==24,"v2 str layout");
ACSD2_ASSERT(sizeof(acsd2_call)==152,"v2 call layout");
ACSD2_ASSERT(sizeof(acsd2_tensor_meta)==120,"v2 tensor meta layout");
ACSD2_ASSERT(sizeof(acsd2_port_desc)==120,"v2 port layout");
ACSD2_ASSERT(sizeof(acsd2_host_api)==72,"v2 host layout");
ACSD2_ASSERT(sizeof(acsd2_operation_api)==80,"v2 operation table layout");
ACSD2_ASSERT(sizeof(acsd2_module_api)==96,"v2 module table layout");
ACSD2_ASSERT(offsetof(acsd2_host_api,credits)==16,"v2 host credit offset");
ACSD2_ASSERT(offsetof(acsd2_store_service,writer_publish)==64,"v2 publish offset");
ACSD2_ASSERT(offsetof(acsd2_module_api,operation_at)==88,"v2 operation enumerator offset");
#else
# error "ARS-02 v2 frozen target currently supports only 64-bit platforms"
#endif
#ifdef __cplusplus
} /* extern C */
#endif
#endif /* ACSD_PROCESS_ABI_V2_REFERENCE_H */
```

## 3. 协商、ABI Layout 与两阶段缓冲协议

### 3.1 加载和版本

Host 经安全根、内容摘要及 allowlist 取到 `acsd_module_query_v2`，按照目标 CPU/ABI 版本先建 Host service tables。`query(host_abi=2,host,&out)` 对 `host_abi`、Host/其强制表的 `head.abi_version=2`、`head.struct_size`（必须覆盖 Host `cancel` 字段和各表**本版本全部必需回调**）、`capabilities` 与三个必要服务表函数指针逐个核验。`*out` 先写 NULL，成功才置为 DLL 静态 module table。Host 收回 module table 后再次验证 `head`、`operation_count==2`（首个 v2 DLL）、`operation_at(0/1)`、每个 desc 的所有 port 描述、非重复 operation ID、`schema_version=2`、op API 必需函数指针非 NULL；还需校验 `module_id/build_digest` 对齐可信 manifest。任何不合法布局、旧 host 版本或 v1 内部结构被误当 v2 都返回 `ACSD2_ERR_ABI_MISMATCH`，不尝试读取较短结构的尾部。

v2 **固定 major=2**，对已知表 `struct_size` 大于当前期望可忽略扩展尾部，必须拒绝未知 **mandatory** capability。`required_host_caps` 中本轮只接受 `ACSD2_REQUIRED_SLICE_CAPS`；AIO、TASKS、CATALOG、KERNEL 的 capability 位和 Host 追加子表仅在另行扩展/测试对应正式合同后才可声明 enabled；缺可选服务用 NULL+bit=0 表示，调用到缺失服务返回 `ACSD2_ERR_UNSUPPORTED`，不是 NULL 调用。对未知 required flags 也应拒绝，禁止静默“降级后执行另一个算法”。C11 与 C++17 兼容只在本次明确支持的 64 位小端 IEEE-754 binary32/binary64 平台成立，启动时验证平台属性；不同编译器须跑 layout 对照与 loader 调用测试，不以 Linux 一个平台证明 Windows ABI。

**64 位 LP64/LLP64 实测参考布局（本批独立 gcc C11 + g++ C++17）**：

| 类型 | sizeof (B) | 注意 |
|---|---:|---|
| `acsd2_head` / `acsd2_str` / `acsd2_error` | 8 / 24 / 48 | 嵌套 `acsd2_str.head` 也要求 version=2 |
| `acsd2_call` / `acsd2_tensor_meta` | 152 / 120 | 身份/元信息仅在当前 callback 内借入 |
| `acsd2_const_view` / `acsd2_mut_view` | 144 / 144 | 引用/映射期间才可使用指针 |
| `acsd2_plan` / `acsd2_work_unit` | 72 / 40 | 宽度/字节数量必须 checked multiply |
| `acsd2_store_service` / `acsd2_credit_service` / `acsd2_scratch_service` | 88 / 40 / 48 | Host 拥有 service context |
| `acsd2_host_api` / `acsd2_port_desc` / `acsd2_operation_desc` | 72 / 120 / 64 | 多 op 静态 port table |
| `acsd2_operation_api` / `acsd2_module_api` | 80 / 96 | 固定回调表布局 |

冻结的关键偏移（B）：`host.credits=16`、`host.store=24`、`host.cancel=32`、`store.writer_publish=64`、`module.operation_at=88`；头内有 `static_assert/_Static_assert` 直接约束这些 ABI 尺寸与偏移，Windows/MSVC x64 构建应以自己的检查结果交叉确认。`acsd2_str` 为 `ptr+size` 的 UTF-8 字节 span，不保证 NUL；输入 `head` 与 pointer/length 一致性由调用方保证，callee 必须验证。所有 `acsd2_str` 若要跨回调存储必须复制进 module 私有受管 metadata；从 module table 返回的字符串/desc/port/vtable 内存由 DLL 静态持有，引用期到**最后** module pin 解除，不由 Host free。

`acsd2_strbuf`：`data=NULL,capacity=0` 的 probe 返回 `ACSD2_OK` 并在 `size` 填**所需 UTF-8 字节，不含 NUL**；fill 时 `capacity≥size+1` 一次性填满并写末尾 NUL，`size` 仍为字节长度。容量不足返回 `ACSD2_ERR_PARAM` 且 `detail_code=1001 (BUFFER_TOO_SMALL)`、`size=required`，**不写任何前缀/输出缓冲**，调用方扩容再取；`size==UINT64_MAX` 时 checked `size+1` 溢出返回 PARAM。此规则适用 `result_meta`，不会让 manifest 含截断 JSON。`acsd2_error` 的 message span 有效至本次错误回调返回；由 Host 立即复制到自己的错误日志。不存在“非零错误但 out 指针保持旧有效 handle”的行为；所有创建型回调入参有效时先设 `*out=NULL`，失败不留下半分配。

### 3.2 操作描述符与唯一端口

首个 DLL module table `module_id="acsd.p1.calibration"`，`operation_count=2`，静态 `operation_at` 下标 **0**=`calibrate_frame`，**1**=`correct_frame`，越界返回 `ACSD2_ERR_PARAM`。两 op `schema_version=2`、`instance_flags=ACSD2_OP_INSTANCE_SERIAL`、`required_host_caps=STORE|CREDITS|CANCEL`，
所有端口为 F32 row-major `[height,width]`，像元中心 0-based、与输入同物理 signal 单位，`coordinate_id="pixel-center-0"`；

| Operation | 输入端口（名称/可缺性） | 输出端口 | 额外 manifest metadata |
|---|---|---|---|
| `calibrate_frame` | `light` 必需；`master_dark`, `master_flat`, `master_bias` 可缺（仅遵从显式配置的 NULL 语义） | `calibrated` 必需 | `actual_k`, `dark_optimization`, `master_presence`, `kernel_rc` |
| `correct_frame` | `calibrated` 必需；`master_dark`,`master_bias` 可缺 | `cleaned` 必需 | `hot_count`, `cold_count`, `method`, `kernel_rc` |

模块 C 描述符含每个 `acsd2_port_desc` 的 name/schema_id/unit/coordinate_id/dtype/direction/flags/pixel_origin。表指针不可被 Host 修改。first slice `schema_id="acsd.image-plane/v2"` 的具体物理 `unit` 从上游科学合同继承，绝不硬编码 `ADU` 代替电子数或光度归一 signal；若 master unit 与 light 不同且不存在科学认可转换，进入内核前拒绝。Master 与 Light 的 width/height 必须相同，row_stride_bytes 必须满足 `width * sizeof(float)`；首个科学内核要求**连续** row-major，不能把 padded stride 指针传给旧 int×int 核，非连续输入由 Host 在入场时明确复制并计入 credit 或拒绝，不能默默误读。

## 4. 内存与引用服务的完整可执行状态转换

### 4.1 MemoryCredit/Buffer ownership

`credits.reserve(context,call,bytes,purpose,&grant,&err)` 对本阶段 Manager **原子**取得增量 credit，目的只能 `OUTPUT|SCRATCH|AIO`，一笔 grant 属于一个 run/scope/attempt；超出余额返回 `ACSD2_ERR_BUDGET`，无可信上界返回有名的 resource error。`grant_bytes` 查询不可使其再使用；`grant_release` 幂等的设计不允许对已移交 grant 重新释放：**成功移交后调用方必须把本地变量置 NULL**，避免 double free。`scratch.allocate(grant,bytes,alignment,&scratch)` 与 `store.begin_output(...,grant,&writer)` **成功时消费 grant**（reserved→live，无第二次计费），失败不消费，原调用方负责 grant_release；scratch 只在 map→unmap 期间借出裸指针，`scratch.release` 归还其实际驻留（若 pool 仍物理持有按 pool 的 live 账本保留）。

**本 slice 保证**：最终 F32 output bytes = `checked_mul(width,height,4)`，`width/height>0` 且 legacy `int` 能表达 width、height 与 product，否则 `ACSD2_ERR_PARAM`；Cal 的旧输入和新 output 在转换期间都存活，OutputGrant 只计新输出，绝不把输入/被 pin 的 master 重新加一遍。`correct_frame` 会在现行 `cosmetic_corrector.cpp` 内部产生 STL 动态数组，暂时未由 Host 分配器严格归口；ARS-02 先在 `plan.scratch_fixed_bytes` 保守包络估计并用 RSS/ASan 观测偏差，**不能因此宣称 ARS-05 的严格防 OOM 已完成**。后续 ARS-05 要么将内核 scratch 接入 Host allocator，要么提供可信尺寸上界和保留量并作故障注入验证。

### 4.2 Typed read lease 与 writer

`store.borrow(context,call,"light"/&co,&lease)` 解析当前 run、node、operation、attempt、port 和真实上游 input revision；缺**必需**端口报数据错误，缺已声明 optional master 则输出 NULL 表示合法缺席，绝不把错误/损坏 master 当 NULL。`read_view` 生成 **不可变** `const data`、width/height/strides/unit/schema；从 `borrow` 到 `read_release` 期间 Host 保持真实 backing allocation pinned；不能读 raw pointer 之后立即 release，再传入科学内核。失败时 `out=NULL`，不会给过期指针。

Runtime 在调用 op 前持 OutputGrant 并执行 `store.begin_output(call,port,tensor_meta,grant,&writer)`；成功时 writer 私有且不可被其他消费者 borrow。`writer_map(writer,&view)` 仅一次独占 mutable 映射，内核出参必须写满有效二维区域；`writer_unmap` 是之后 `writer_publish` 的必要前置。DLL 的 `execute_unit` 可以 map→科学函数→unmap；`reduce` 核对实际统计、未取消的作业结果并生成小型 `result_meta`。随后 Runtime（**不是 DLL**）调用 `writer_publish` 原子发布 Block；返回 `acsd2_block*` 给 Host 内部持有，caller 依本服务 `block_release` 配对。失败/取消无条件调用 `writer_abort`，不发布部分像素。

引用回收顺序：

```text
Host.reserve(output) → Host.begin_output(consumes grant) → Op.execute_unit
  ├─ Host.borrow inputs → get read_view → science AC_API → read_release
  └─ Host.writer_map → Host.writer_unmap
all units success → Op.reduce → Op.result_meta → Host.writer_publish
failure/cancel → Host.writer_abort (no visible Block)
last read lease / I/O pin / declared consumer → free physical allocation → release credit
```

内存复制只允许在 Host 显式 bridge 和兼容适配位置发生；v1 Base64 decoded buffers 与新 Host-owned input blocks 属**两次实际分配时**必须分别有 credit。若 Host 以真正所有权 transfer 取代复制，则只转 allocation_id owner 不额外扣相同 RAM。AIO `write_immutable` 接受已发布且只读 block，成功返回 ticket 并保留 pin 到 terminal completion + ticket_release；`request_cancel` 只是请求取消，`wait` 返回终态以前 **不能**释放 block/loader/Host，IO 后端最终释放取决于票据完成。首 slice `CAP_AIO=0`，绝不利用不存在的 ticket API 假装写产品。

## 5. Operation 的状态、科学调用与取消

### 5.1 生命周期与线程安全（第一切片固定）

```text
query v2 → operation_at(0,1) → validate_config → plan (read metadata only)
→ create(inst) → partition(probe,fill) → Host.begin_output
→ execute_unit [exactly one FULL_FRAME unit] → reduce → result_meta
→ Host.publish_writer → destroy(inst) → unload when all calls/leases/tickets quiescent
```

每个 `create` 生成独占实例，唯一 Run/Frame/Operation attempt 不复用，`destroy` 不带仍在执行的 callback，不在 destroy 内发起异步写；`request_cancel` 可与正在运行的 instance 并发，仅原子设置 cancellation 标志，`execute_unit/reduce/partition/result_meta` 与同 instance 的其他操作**均串行**。Host 若 worker grant 不是1，第一 slice fail `ACSD2_ERR_BUDGET` 而非让 OpenMP 借全部可用核心；调用旧 `ac_set_num_threads(1)` 仅在正在执行这个 instance 的同一 CPU worker 内操作，避免进程-wide 更改生产科学默认。Internal kernel（尤其热/冷 mask 连通分量）内部可能临时申请 STL scratch，必须在上述安全边界保证 pointer 有效。

`partition` 协议：probe 传 `out=NULL,capacity=0,required!=NULL`，成功设置 `*required=1`；fill 需要 `capacity>=1`，恰写一个 `{ordinal=0,begin=0,end=width*height,axis=FULL_FRAME}` 并设置 required=1；不足返回 `ACSD2_ERR_PARAM/detail=1001`，**不得写半个 WorkUnit**；错误的元信息/shape/输出指针报 PARAM。计划 `work_units=1,min_workers=max_workers=1,partition_axis=FULL_FRAME`。`result_meta` 仅含纯字符串/标量 JSON，不得内联图像或把配置 metadata 代替真实科学观测。未执行/失败时 result_meta 返回 `ACSD2_ERR_STATE`，不给旧上一轮结果。

Host 在 `execute_unit` 前及科学调用返回后检查 `cancel.is_cancelled(call)`；内核本体没有分片取消能力，不为它伪造“每 256 行检查”。一旦取消即跳过 `reduce/publish`，TaskGroup join 确认调用退出，read lease/scratch/writer/ticket 静止后才 destroy/unload。无论 kernel C API 是否抛 C++ 异常，DLL 所有导出 callback 全部 catch-all：`bad_alloc→ACSD2_ERR_NOMEM`、其他异常→`ACSD2_ERR_EXCEPTION`；Host callback 自身也要 catch-all，跨 C ABI 不得传播异常。legacy AC API `AC_OK=0` 映射 ACSD2_OK；`AC_ERR_PARAM=-1` 映射 PARAM，`AC_ERR_MEMORY=-2` 映射 NOMEM，`AC_ERR_INTERNAL=-3` 映射 INTERNAL；不把负数原值直接穿透。

### 5.2 两个真实 operation 的参数/数学与来源

**calibrate_frame → `ac_calibrate_frame`**：入参 `const float* light, int width,height, const float* master_dark, master_flat, master_bias, float* out,int dark_optimization,float dark_scale_factor,float* actual_k`；F32 `[H,W]`，按 [calibrator.cpp](../../../../lib/algorithms/calibration/src/calibrator.cpp) 当下实现：当 `dark_optimization=0`（标准式，dark 已扣 bias）时

\[
Y=(L - \mathbf 1_B B - K\mathbf 1_D D) / \max(F,0.1),\quad K=	exttt{dark_scale_factor}.
\]

当 `dark_optimization=1` 且 dark/bias 均提供（兼容式，dark 含 bias）时

\[
Y=(L-B-K(D-B))/\max(F,0.1).
\]

缺 flat 时分母为1，dark_opt=1 但缺 dark/bias 时按旧内核退回标准式，仍保持 K；master_dark/bias/flat NULL 只能是明示科学允许的缺席，不能暗中替换损坏文件。实际 K 不由 DLL 读取 EXPTIME/自选1，而由**上游已通过配置与数据校验**的调用者给定；需要暗场却不能推导 t_light/t_dark 的情况必须保持现有 fail-closed 合同，未要求 dark 时默认分支仍保持旧科学行为。避免把代码旧注释中的“黄金分割搜索”当成现行运行事实，当前内核用传入 K。输出 `actual_k` 随 result_meta 与 input occurrence 绑定。`flat` 的零/负值按当前内核 floor，不改变科学算法。

**correct_frame → `ac_correct_frame`**：入参 `const float* data,int width,height,const float* master_dark,master_bias,float* out,float hot_sigma,cold_sigma,int method,max_structure_size,int* out_hot,out_cold`。当前 `cosmetic_corrector.cpp`：dark 的热像素和 bias 的冷像素各以 robust median ± sigma×(1.482602218505602 MAD) 判定，8 邻域组件尺寸 `size>=max_structure_size` 排除；method=0 采用 5×5 有效邻域 median，method=1 实现是**四正交方向最近好像元的反距离权重**，不是数学双线性插值。缺 master_dark/bias 不得把错误文件读失败变成“零修复”，但明示 optional NULL 仍执行合法遗留语义。输出 clean 与 `out_hot/out_cold` 需要全部写入；坏点是否影响后续噪声的相关性，仍由 Normalize 科学合同负责，ARP 不新增 variance 场或更改 default。

**生产接线目标**：

```text
acsd CLI normalize (现有 runtime_client.cpp / build_pipeline_ir)
  → existing cal node selects v2 calibration module + operation_at(0)
  → typed F32 Light/Master read leases → ac_calibrate_frame → calibrated Block
  → existing cos node selects SAME DLL's operation_at(1)
  → typed calibrated/master read leases → ac_correct_frame → point-corrected Block
  → pre-existing ac_correct_columns_ex2 + masks/statistics retained → cleaned Block
  → existing WCS / photometry / noise / drizzle / writer remain unchanged
```

现有 `module_adapters.cpp` 的 `p1_op_calibration` 和 `p1_op_cosmetic` 是内建正式科学生产调用点；v2 接入应将这两个节点的 frame 输入/输出改接 shared typed adapter **同时保持**下游现有文件桥与注册的科学依赖，避免它们突然断链。**明确采用配置中一个受 schema 校验的 runtime backend 选择项** `runtime.process_backend="v2_calibration_slice"` 开启该条链；未显式选择时不改变既有 v1/静态默认，缺 v2 DLL 不静默退回另一科学实现。该配置项及解析合法值必须同 C++ CLI schema 和测试一并落地；在未落地前，文档不宣称此 key 已可运行。生产确证用 loader v2 entry 指针与 per-op execute 调用计数，而不是从 module.yaml 的 `node_operations` 文本判断已动态执行。

## 6. 构建落点、功能证据、正负测试及依赖

| 目标路径/代码 | 一次性确定的变更方向 |
|---|---|
| `lib/include/acsd/abi/status_codes_v2.h`、`typed_store_v2.h`、`host_api_v2.h`、`module_api_v2.h` | 从上述单头按 typedef 顺序拆分，**签名、枚举和指针/释放规则保持**；每个头独立 C11/C++17 可编译 |
| `lib/algorithms/calibration/src/module_entry.cpp` | 增加 v2 module/2×operation tables 与两个实例 adapter；共用现有 `ac_calibrate_frame/ac_correct_frame` 内核，不复制科学代码 |
| `lib/algorithms/calibration/src/{module_exports.map,acsd_p1_calibration.def}`、`lib/algorithms/calibration/CMakeLists.txt` | 导出 v1 与 v2 两个 query，隐藏 legacy AC_API 科学符号，保持原编译源与 OpenMP/数学后端 |
| `lib/infrastructure/pipeline/module_loader/{secure_loader.c,module_registry.c}` | 多表版本识别、OP descriptor + Host service 验证、内容哈希/allowlist、loader pin 与旧 v1 兼容 |
| `lib/infrastructure/cli/runtime_client.cpp` 及 scheduler 的 cal/cos 适配层 | 在真实 CLI + 严格配置开关下动态执行 cal→cos，两节点数据边、主帧预处理及坏列修复与现有算法保持数值一致，旧默认路径不受影响 |
| 模块自身现有 `module.yaml` / `module_ports.registry.json` 与正式接口描述 | 实际 v2 完成后同步两个新 entry 和 port identity；不能先改 registry 假装生产已接入 |

**ARS-02 代码 Agent 可以接收的完整正反核验**：

- C11/C++17/x64：单头和拆分头均 `-std=c11/-std=c++17 -Werror` 编译，通过 `offsetof/sizeof` 断言；Windows MSVC x64 必须单独构建验证（不在本次环境假定 PASS）。
- ABI：旧 v1 query 与新 v2 同 DLL，operation_at 0/1 和越界、相同 ID/重复 port、错误 struct_size/version、缺必需 callback、未知 required capability、NULL/非 UTF-8、嵌套字符串版本、过大容量与所需 size+1 溢出全部有负例；失配时不访问尾部且出参清 NULL。
- 生命周期：module table 静态到 unload、query Host 不得异步保存、create 时绑定有效 Host、同实例第二 execute 并发拒绝、不同实例并行可证明；读租约直到旧计算完成、writer 只能 Host publish、map/unmap 后指针禁止使用、取消后 TaskGroup join、正在访问的 lease/dynamic DLL 不能 unload、所有分配/释放配对。AIO optional NULL 时安全返回 UNSUPPORTED。
- 真实科学：使用同份小型有主帧/缺主帧、坏点/冷点、K!=1、flat floor、负像素、非连续 row-stride 拒绝、整数溢出，比较新 DLL/旧内核 **同一输入、同一 F32、同线程数**的 pixel array 与统计结果；必须记录版本、科学参数及 FPU/ISA，允许的浮点误差在正式科学合同中声明，不得仅比较图像 hash。
- 真实 CLI：在 `runtime.process_backend=v2_calibration_slice` 时通过 loader 调到两个不同真实 v2 Operation，然后真实 wcs/PSF 等 downstream 消费；形成可读 sidecar/产品，查看产物/trace、与旧路径作科学数值对照，包括 bad-column mask/统计和 cleaned 面。只会 query/plan 但无 execute 调用不算完成。
- Resource：OOM/failure injection 在 reserve、materialize/ begin_output、science kernel、writer_unmap、publish、取消分别试，确保不出现半成品、无悬垂 read lease/double free；区分本轮可检查的 credit 状态与未来 ARS-05 真实 STL 分配受管。
- #19 既有测试 `26 failed / 56 passed` 的旧锚点漂移需由 ARS-01 [#23](https://github.com/fujiaze/Astro-CS-Database/issues/23) 分类为旧基线失败，不掩盖或写成本轮成功；#19 的现阶段混阶 fail-closed 不能误写成已实现面积保真的 HiPS 重映射。

**本批本地静态 ABI 验证证据**：从本页单头抽出，在独立临时目录用 GCC C11、G++ C++17 `-Wall -Wextra -Wpedantic -Werror` 编译通过，且测得 §3 表中 LP64 尺寸；不构建仓库、不测试 MSVC、未调用真实 DLL，不声称 ARS-02 或 Issue #6 的科学 E2E 完成。当前 #23 仍 Open，#24 只有在基线、配置 schema 和这份头/Host 合同同步到位后才适合启动正式代码实施。本篇不代替全原191篇信息保全工作，后续继续 Issue #6 其他主题。


## 7. ARS-02 本批独立静态复核及唯一路径落实（2026-10-17）

**结论：C11 头与双 Operation 的目标架构选择维持不变；本批源码审查不等于已安装/已执行。** 源基线 `d09371c7f40ed8518fe819fcd311a433e42902cb`。

- 现行 CLI 真正内建调用点为 `lib/infrastructure/scheduler/src/module_adapters.cpp::p1_op_calibration`（约 2084–3114；实际 `ac_calibrate_frame` 约 2934）、`p1_op_cosmetic`（3115 起，`ac_correct_frame` 约 3304）。校准底层 C ABI 符号实现在 `lib/algorithms/calibration/src/ac_api.cpp` 约 141/155 行，函数声明在 `include/astro_calibration.h` 约 101/124 行；并非凭 YAML 伪造。坏列处理由 `module_adapters.cpp::p1_op_cosmetic` 约 3329 的 **`ac_correct_columns_ex2`** 完成，不能在 v2 correct_frame 返回后提前结束当前 cosmetic 节点。
- `lib/algorithms/calibration/src/module_entry.cpp` 当前导出 `acsd_module_query_v1`（约 1515）；它的 `ac_calibrate_frame* / ac_correct_frame*` （约 1036–1068）调用证明内核可复用，但**不**证明 ABI v2、two-operation table、typed credits、CLI 调度已存在。两个 op 维持冻结顺序 0=`calibrate_frame`、1=`correct_frame`，共用一个 `acsd.p1.calibration` DLL，不再让 Agent 选择新的 module 拆分方案。
- v2 用冻结 C11 `acsd2_*` 类型和静态操作描述符，运行时 adapter 显式将 CLI `acsd.phase1.calibration`/ `acsd.phase1.cosmetic` 映射同一 DLL 两个 op；保持原 `cal -> cos -> WCS/science` 的上游/下游数据身份、F32 维度与坏点统计；`correct_columns_ex2` 仍在 cosmetic node 内，加入实际 v2 execute trace 与正确输出归属。
- `runtime.process_backend="v2_calibration_slice"` 必须连 schema 解析、loader 符号协商、错误传播、真实 CLI 选路一起实现；缺 DLL 明确失败而不悄悄 fallback。校准 master 缺失、flat floor、dark scaling、cosmetic 形态学掩膜及标记统计使用**现有** CALIBRATION 科学有效域，不能为新 ABI 更改校准方程。内存：最大新输出 `checked(width*height*4)`、读 lease/credits/取消/异常释放；内部 STL 暂不能宣称严格 Host OOM 保护。
- ARS-01 的 S0 真实 CLI 输入/命令/数值产物作为**唯一对照基线**，做同帧 v1 静态与 v2 插件调用、像素逐项/容差、坏点坏列计数和文件 hashes 对拍。C11/C++17 头独立编译、LP64/LLP64 layout、Windows MSVC 真 DLL loader、v1-v2 符号白名单和双 op execute 计数分别留证。依次实施：头拆分→两 op vtable→loader+schema→CLI adapter/credits→完整真实 E2E；所有未运行步骤 **TARGET/NOT_RUN**。本批不扩张为 ARS-03~08。
