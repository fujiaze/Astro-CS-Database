# 所有权、借用与生命周期

本合同依据[系统架构](../architecture/ARCHITECTURE.md)和[公共接口](../api/PUBLIC_API.md)，规定数组、模型、句柄与异步任务的寿命。跨动态库的分配器与布局由[C ABI](../api/abi/ABI.md)细化，Normalize 对象版本由[数据合同](NORMALIZE_DATA.md)定义。

## 1. 所有权必须能从接口读出

| 形式 | 语义 |
|---|---|
| borrowed/const view | 调用方保有对象，被调方只在声明借用期访问 |
| caller-allocated output | 调用方分配并传容量，被调方填充；不转移所有权 |
| library-allocated result | 按本族指定 free/close 释放，不跨分配器或接口族 |
| opaque handle | 由 create/open/build 取得，经配对 destroy/close/finalize/abort 结束 |
| Runtime owned payload | 发布后只读，由真实消费者寿命管理回收 |
| persisted artifact | 文件完成与内存对象寿命分开，由 AIO 和产品发布合同管理 |

不能一律说“所有输出都由调用方分配”。检测和 PSF 的部分接口分配结果，session inspect 使用 host allocator；容量填充 API 又由调用方提供数组。nullable、借用、别名/in-place 与失败后出参状态逐函数声明。

## 2. 句柄与释放器配对

| 对象 | 取得 | 配对结束 |
|---|---|---|
| 检测句柄与结果 | sdet_create、detect_ex 系 | sdet_destroy、sdet_free_detect_ex |
| 分配型 PSF 批结果 | dpsf_fit_batch 系 | dpsf_free_results |
| IPV solver | ipv_solve_create | ipv_solve_destroy |
| HiPS 产品句柄 | aio_hips_product_begin | finalize 或 abort，按接口状态避免重复释放 |
| sky-plane 模型 | p2_sky_plane_build/open 系 | p2_sky_plane_close |
| 库 UPM 模型 | p2_upm_build/open 系 | p2_upm_close |
| session | p1/p2/p3_session_create | 对应 destroy |
| session inspect 文本 | host allocator | 同一 host free |

表用于辨认释放域，不替代公开头的精确返回和失败语义。coverage 的 free 清零 POD，不释放调用方数组；不能从名字 free 推断释放了所有下层缓冲。AIO 的特定动态接口还可能使用 new[]，其返回需按原声明 delete[]，不能用普通 free。

## 3. 阶段内对象与异步任务

大数组、模型与 frame 在生产者发布后保持相应只读或独占状态，直到最后一个实际消费者结束。同步调用结束后才可结束借用；异步任务须持有显式 owned/shared lease，不能捕获已离开作用域的栈对象或悬垂裸指针。

RAII 保证离开作用域时清理，不代表任意提前析构都是安全的。取消与异常先停止/收尾在途任务，随后释放视图、写句柄与预算。source frame 的 header 元数据也受同一寿命约束，不能图像写完就销毁仍会被 staging 读取的 frame。

当前 CLI 的 RunContext 是 run_pipeline 局部对象，覆盖同步 Runtime::run；原始对象版本、typed 模型仓与最后消费者回收的目标和实现差异见[Runtime](RUNTIME.md)、[命名块](PIPELINE_BLOCK.md)和[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 4. 并发与失败清理

同一 handle 的并发性逐函数定义；不同 handle 能否并行还取决于共享缓存、全局表与文件服务。thread_local 错误文本避免部分线程污染，但不自动使整个库线程安全。错误字符串只用于诊断，返回码与具名状态才决定调用结果。

部分分配失败时释放已经取得的资源，并明确出参是重置、部分有效还是未写入。destroy/close 是否接受 null、是否幂等由接口声明决定，不能对所有族统一假定。模型发布失败和合法 precision unavailable 具有不同原因，按[原子发布](ATOMIC_PUBLISH.md)处理。

验证重点为分配/释放配对、非首项失败、容量不足、重复释放负例、跨族释放错误、取消等待、异步借用及成功/失败后的可见产品。已读源码发现的具体风险集中记录，未运行的生命周期检查不宣称已通过。
