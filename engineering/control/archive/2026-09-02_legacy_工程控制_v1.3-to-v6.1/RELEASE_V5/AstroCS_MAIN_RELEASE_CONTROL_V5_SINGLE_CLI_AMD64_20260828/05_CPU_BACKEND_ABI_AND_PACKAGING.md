# amd64 CPU Backend ABI、ISA 与安全加载

## 1. 设计结论

可以实现 baseline、SSE4.1、AVX、AVX2+FMA、AVX-512 等多个计算路径，但不得机械复制所有算法。先由 profile 找到热点与向量化可行性，再为有收益的 kernel 实现变体。每个变体必须先过独立 Oracle；benchmark 只在正确变体之间选最快者。

AVX-512 不是天然更快，可能因频率、数据规模或内存带宽落后；没有实测收益就不选，甚至不发布。

## 2. 编译隔离

- baseline backend 不得要求 AVX，必须能在项目支持的最低 amd64 CPU 上加载。
- 可选 backend/translation unit 用局部编译选项；主 CLI 和 baseline 不得被 AVX 指令污染。
- 禁止全局 `-march=native`、`/arch:AVX2`、`-mavx*`。
- CI 用反汇编/对象检查确认 baseline 中没有超出最低合同的 opcode。
- SIMD 变体与 baseline 必须共享同一公共数学合同，禁止复制后独立漂移。

## 3. CPU 与 OS 状态检测

加载前必须同时检查：

1. CPUID feature bits；
2. OSXSAVE；
3. XGETBV 中 OS 是否保存 XMM/YMM/ZMM 状态；
4. 当前进程 affinity/cgroup/Job Object 的可用 CPU，而非机器总核数；
5. backend manifest 的 required_features；
6. backend 文件 hash 和 ABI 版本。

不支持的 backend 不得尝试执行。Windows 采用受限 DLL 搜索目录；Linux 只从发行包相对私有目录加载。默认禁止任意 `PATH/LD_LIBRARY_PATH` 插件注入；开发覆盖需显式危险开关并记录到 manifest。

## 4. 稳定 C ABI v1

不得跨 DLL/SO 边界暴露 C++ STL、异常、RTTI、编译器分配的所有权。唯一入口示意：

```c
int astrocs_backend_get_api_v1(
    uint32_t host_abi_version,
    uint32_t host_struct_size,
    const astrocs_host_services_v1* host,
    astrocs_backend_api_v1* out_api);
```

ABI 结构必须含：

- `abi_version, struct_size`；
- `backend_id, backend_build_id, backend_sha256`；
- required/detected feature bits；
- 对齐、precision、determinism 和 aliasing 合同；
- allocator/log/cancel/thread-budget host callbacks；
- kernel capability 表与函数指针；
- `self_test()`、`warmup()`、`shutdown()`；
- 结构化错误码；不得跨边界抛异常。

内存由分配方释放，或全部使用 host allocator。并发合同逐函数写明：可重入、线程安全、是否内部并行、是否允许嵌套并行。Host 传入一个全局 thread budget；backend 不得私自创建不受预算控制的线程池。

## 5. Kernel 注册粒度

至少按下列 kernel 独立注册与选路，而非一个全局“AVX2 模式”：

- calibration pixel transform；
- noise/SNR reductions；
- WCS/PSF 批量计算（若为热点）；
- drizzle overlap/accumulate/normalize；
- UPM SpMV/residual/weight update；
- rejection statistics；
- integration accumulate；
- HiPS bulk transform/encode（只作为吞吐，不改变科学值）。

每个 kernel 记录 `science_contract_id,algorithm_id,kernel_version,precision,determinism_class`。

## 6. 失败与回退

- 启动前可选 backend 预检失败：发 warning/backend event，回退 baseline，再开始该 run。
- 计算中 backend 失败：安全中止整个 stage，不得静默改 backend 后继续混合结果。
- profile 指向的 backend hash/ABI/kernel version 不匹配：profile 整体或对应项失效，使用保守路线。
- baseline 自检失败：不得运行，返回 5。

## 7. 发布检查

每个平台生成 `backends.manifest.json`：文件名、hash、ABI、编译器、局部 flags、required_features、kernel 清单、自测结果。正式包只含 manifest 内文件，且 CLI `doctor` 对每个 shipped backend 做“安全检测但不执行不支持指令”的核查。

