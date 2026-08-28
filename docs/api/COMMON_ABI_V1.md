# AstroCS 公共 C ABI 基础层 v1 (Common Foundation — API-001 冻结)

> ID: API-COMMON-001  状态: FROZEN (V5 API-001, 2026-08-28)  上游: ARCH-002/ARCH-003(backend host services 同构)  下游: API-002..005, ABI-001, CLI-001..003
> 头文件: `include/astrocs/common_abi_v1.h`(单一头, C/C++ 双可编译, 无 STL/exception/RTTI 跨边界)。

## 1 命名与版本

- 前缀 `acs_`(函数)/`ACS_`(类型/常量);所有 struct 首两字段 `uint32_t struct_size; uint32_t abi_version;`(handshake, ARCH-003 §4 同规)。
- `ACS_ABI_VERSION_V1 = 1u`;失配即拒绝(返回 `ACS_ERR_ABI_MISMATCH`),不猜布局。

## 2 类型与逐字段单位/所有权

```c
/* 基础 POD(逐字段单位注释为合同一部分, 由 ABI layout 测试核对) */
typedef struct { uint32_t struct_size, abi_version; } acs_head;

typedef struct acs_span_f32 { float*  data; uint64_t count; } acs_span_f32;  /* count=元素数, data 所有权=外部分配方 */
typedef struct acs_span_f64 { double* data; uint64_t count; } acs_span_f64;
typedef struct acs_span_u8  { uint8_t* data; uint64_t count; } acs_span_u8;

/* opaque handle: 不透明指针, 生命周期仅经 create/destroy 对 */
typedef struct acs_handle_s* acs_handle;

/* 错误码(结构化, 禁异常) */
typedef enum {
  ACS_OK=0, ACS_ERR_PARAM=1, ACS_ERR_ABI_MISMATCH=2, ACS_ERR_NOMEM=3,
  ACS_ERR_IO=4, ACS_ERR_UNSUPPORTED=5, ACS_ERR_CANCELLED=6, ACS_ERR_STATE=7,
  ACS_ERR_BUDGET=8, ACS_ERR_SELFTEST=9
} acs_status;

/* host allocator: 所有跨边界内存经此(分配方释放或全 host alloc, 二选一由函数合同标注) */
typedef struct {
  uint32_t struct_size, abi_version;
  void* (*alloc)(void* ud, uint64_t size, uint64_t align);
  void  (*free)(void* ud, void* p);            /* p 可为 NULL */
  void* user_data;
} acs_allocator;

/* logger: 线程安全由宿主保证; level 常量 ACS_LOG_DEBUG/INFO/WARN/ERROR */
typedef struct {
  uint32_t struct_size, abi_version;
  void (*log)(void* ud, int level, const char* component, const char* msg);
  void* user_data;
} acs_logger;

/* cancel: 单向置位(宿主→backend), 原子语义; backend 只读轮询 */
typedef struct {
  uint32_t struct_size, abi_version;
  int   (*is_cancelled)(void* ud);             /* 0/1, 原子读 */
  void* user_data;
} acs_cancel;

/* thread budget: 只读快照+租借(ARCH-004 §1); backend 禁自建线程池 */
typedef struct {
  uint32_t struct_size, abi_version;
  uint32_t available_cpus;                     /* affinity∩cgroup∩Job Object */
  uint32_t max_workers;                        /* 本次调用允许的 worker 上限 */
  int      (*acquire)(void* ud, uint32_t n);   /* 原子租借, 0=成功 */
  void     (*release)(void* ud, uint32_t n);
  void* user_data;
} acs_thread_budget;
```

## 3 并发合同模板(逐函数必填字段)

每个跨边界函数头注释必含: `reentrant: yes|no; threadsafe: yes|no; internal_parallel: none|omp(budget); aliasing: in/out 不重叠|允许 in-place`;内存去向(谁分配谁释放);取消点粒度(帧/行带/迭代/整模型/整文件, 与 ALG 5c 对齐)。

## 4 头文件独立性验证合同

- 单头 `common_abi_v1.h` 以 `gcc -x c -std=c11` 与 `g++ -x c++ -std=c++17` 独立编译通过(无 STL 依赖);
- ABI layout 测试: 静态断言 `sizeof/offsetof` 全字段(双平台同布局, amd64 LP64/LLP64 差异仅指针宽度已避用 long);
- 无 exception 跨边界: `-fno-exceptions` 可编译 backend TU。

## 5 任务映射

| 本文件 | 落点 |
|---|---|
| §2 类型 | ABI-001(实现头+layout tests) |
| §3 并发合同 | API-002..005 逐函数定义沿用 |
| budget/cancel | ARCH-003 host services/ARCH-004 §1 |
