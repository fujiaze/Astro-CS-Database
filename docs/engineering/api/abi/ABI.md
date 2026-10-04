# 公共 C ABI 基础层

上游：最高设计的模块与 ABI、CPU 后端与资源两章；公共 API 规则见 `../PUBLIC_API.md`。

单一头 `lib/include/acsd/common_abi_v1.h`，C 与 C++ 双可编译；跨边界不出现标准库类型、异常、
运行时类型信息与编译器私有类型。

本文件定义公共 C ABI 的基础层：类型与逐字段单位 / 所有权、并发合同模板、
头文件独立性验证合同。公共 ABI 的命名与版本握手规则同最高设计 的模块与 ABI 条款；
host 服务（allocator / logger / cancel / thread budget）与 `../../architecture/ARCHITECTURE.md`
的顶层结构同构。

## 命名与版本

- 前缀 `acs_`（函数）/ `ACS_`（类型 / 常量）；所有 struct 首两字段
 `uint32_t struct_size; uint32_t abi_version;`，作为握手字段，与 `../../architecture/ARCHITECTURE.md`
 的结构握手同规；
- `ACS_ABI_VERSION_V1 = 1u`；失配即拒绝（返回 `ACS_ERR_ABI_MISMATCH`），不猜布局。

## 类型与逐字段单位 / 所有权

```c
/* 基础 POD(逐字段单位注释为合同一部分, 由 ABI layout 测试核对) */
typedef struct { uint32_t struct_size, abi_version; } acsd_head;

/* CPU-001: 所有跨边界结构带 head(struct_size+abi_version); span 构造经 ACS_SPAN_* 宏 */
typedef struct acsd_span_f32 { acsd_head head; float* data; uint64_t count; } acsd_span_f32; /* count=元素数, data 所有权=外部分配方 */
typedef struct acsd_span_f64 { acsd_head head; double* data; uint64_t count; } acsd_span_f64;
typedef struct acsd_span_u8 { acsd_head head; uint8_t* data; uint64_t count; } acsd_span_u8;
#define ACS_SPAN_U8(ptr, n) { { sizeof(acsd_span_u8), ACS_ABI_VERSION_V1 }, (ptr), (n) }
#define ACS_SPAN_F32(ptr, n) { { sizeof(acsd_span_f32), ACS_ABI_VERSION_V1 }, (ptr), (n) }
#define ACS_SPAN_F64(ptr, n) { { sizeof(acsd_span_f64), ACS_ABI_VERSION_V1 }, (ptr), (n) }

/* opaque handle: 不透明指针, 生命周期仅经 create/destroy 对 */
typedef struct acsd_handle_s* acsd_handle;

/* 错误码(结构化, 禁异常) */
typedef enum {
 ACS_OK=0, ACS_ERR_PARAM=1, ACS_ERR_ABI_MISMATCH=2, ACS_ERR_NOMEM=3,
 ACS_ERR_IO=4, ACS_ERR_UNSUPPORTED=5, ACS_ERR_CANCELLED=6, ACS_ERR_STATE=7,
 ACS_ERR_BUDGET=8, ACS_ERR_SELFTEST=9
} acsd_status;

/* host allocator: 所有跨边界内存经此(分配方释放或全 host alloc, 二选一由函数合同标注) */
typedef struct {
 uint32_t struct_size, abi_version;
 void* (*alloc)(void* ud, uint64_t size, uint64_t align);
 void (*free)(void* ud, void* p); /* p 可为 NULL */
 void* user_data;
} acsd_allocator;

/* logger: 线程安全由宿主保证; level 常量 ACS_LOG_DEBUG/INFO/WARN/ERROR */
typedef struct {
 uint32_t struct_size, abi_version;
 void (*log)(void* ud, int level, const char* component, const char* msg);
 void* user_data;
} acsd_logger;

/* cancel: 单向置位(宿主→backend), 原子语义; backend 只读轮询 */
typedef struct {
 uint32_t struct_size, abi_version;
 int (*is_cancelled)(void* ud); /* 0/1, 原子读 */
 void* user_data;
} acsd_cancel;

/* thread budget: 只读快照+租借; backend 禁自建线程池, 取值源见 ../../architecture/DATA_FLOW.md */
typedef struct {
 uint32_t struct_size, abi_version;
 uint32_t available_cpus; /* affinity∩cgroup∩Job Object */
 uint32_t max_workers; /* 本次调用允许的 worker 上限 */
 int (*acquire)(void* ud, uint32_t n); /* 原子租借, 0=成功 */
 void (*release)(void* ud, uint32_t n);
 void* user_data;
} acsd_thread_budget;
```

`acsd_status` 是 ABI 层的粗粒度返回码，只区分「成功 / 哪一类失败」；
面向用户的分类、退出码与阶段 ID 分别由 「并发合同模板」一节 / 「头文件独立性验证合同」一节 / 「落点映射」一节
与 「落点映射」一节 定义，两层不得互相替代。

## 并发合同模板（逐函数必填字段）

每个跨边界函数头注释必含：`reentrant: yes|no; threadsafe: yes|no; internal_parallel: none|omp(budget); aliasing: in/out 不重叠|允许 in-place`；
内存去向（谁分配谁释放）；取消点粒度（帧 / 行带 / 迭代 / 整模型 / 整文件）。

## 头文件独立性验证合同

- 单头 `common_abi_v1.h` 以 `gcc -x c -std=c11` 与 `g++ -x c++ -std=c++17` 独立编译通过（无 STL 依赖）；
- ABI layout 测试：静态断言 `sizeof` / `offsetof` 全字段（双平台同布局，amd64 LP64 / LLP64 差异仅指针宽度，已避用 `long`）；
- 无 exception 跨边界：`-fno-exceptions` 可编译 backend 翻译单元。

## 落点映射

| 本文件 | 落点 |
|---|---|
| 「类型与逐字段单位 / 所有权」一节 类型 | `lib/include/acsd/common_abi_v1.h`（实现头 + ABI layout 测试） |
| 「并发合同模板」一节 并发合同 | 「头文件独立性验证合同」一节 逐函数定义沿用；分阶段 API 面见 `../PUBLIC_API.md`、`../PUBLIC_API.md`、`../PUBLIC_API.md` |
| 「类型与逐字段单位 / 所有权」一节 budget / cancel | `../../architecture/DATA_FLOW.md`（唯一执行预算对象）、`../../architecture/ARCHITECTURE.md`（顶层结构） |
| 「类型与逐字段单位 / 所有权」一节 loader 侧契约 | `SECURE_LOADER.md` |

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/api/PUBLIC_API.md`，同层相关正本。
[3] 内部文档 `docs/engineering/contracts/RUNTIME.md`，同层相关正本。
