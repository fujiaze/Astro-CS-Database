# CPU Backend 架构 (C ABI v1 · Loader · Per-Kernel Dispatcher)

> ID: ARCH-BACKEND-001  状态: FROZEN (V5 ARCH-003, 2026-08-28)  上游: ARCH-002/ALG-001..007  下游: ABI-001/002, ISA-001..005, BENCH-001..005
> 权威来源: 控制包 05 全条目(§1–§7)逐节落成架构; 禁止项与 05 完全一致。

## 1 设计结论(05 §1)

- 变体集合: baseline / SSE4.1 / AVX / AVX2+FMA / AVX-512,**profile 先行**:仅对实测热点 kernel 做变体;每变体独立 Oracle;benchmark 只在正确变体间选最快;AVX-512 无实测收益则不选甚至 NOT_SHIPPED(原因+数据+代码去留登记)。

## 2 编译隔离(05 §2)

- baseline 必须在最低 amd64 合同 CPU 上加载;变体 TU 局部编译选项;主 CLI 与 baseline 零 AVX 污染;**禁止全局 `-march=native`/`/arch:AVX2`/`-mavx*`**;CI 反汇编/对象扫描 baseline opcode;变体与 baseline 共享同一数学合同头(单一 `lib/backend_host/baseline_kernels.h`,禁止复制漂移;V6.1 实际实现,CPU-BACKEND-ARCH-001)。

## 3 CPU/OS 状态检测与信任边界(05 §3)

加载前六查(全过才执行): ① CPUID feature bits; ② OSXSAVE; ③ XGETBV(XMM/YMM/ZMM 保存); ④ 进程可用 CPU=affinity∩cgroup∩Job Object(非机器总核); ⑤ manifest `required_features`; ⑥ 文件 sha256+ABI 版本。
**信任边界**: Windows 受限 DLL 搜索目录;Linux 仅发行包相对私有目录;**禁止任意 `PATH`/`LD_LIBRARY_PATH` 注入**;开发覆盖=显式危险开关+manifest 记录;host 永不执行未过六查的 backend(无 illegal instruction 可能)。

## 4 稳定 C ABI v1(05 §4)

- 跨边界禁: C++ STL/异常/RTTI/编译器分配所有权;异常不得跨边界(测试断言)。
- 唯一入口: `astrocs_backend_get_api_v1(host_abi_version, host_struct_size, host*, out_api*)`;`struct_size`/version handshake 失配拒绝。
- 结构必含: `abi_version, struct_size`;`backend_id, backend_build_id, backend_sha256`;required/detected feature bits;对齐/precision/determinism/aliasing 合同;allocator/log/cancel/thread-budget host callbacks;kernel capability 表+函数指针;`self_test()/warmup()/shutdown()`;结构化错误码。
- 内存: 分配方释放或全部 host allocator;并发合同逐函数写明(可重入/线程安全/内部并行/嵌套并行);host 传全局 thread budget,**backend 禁私有线程池**。

## 5 Kernel 注册粒度(05 §5)

逐 kernel 独立注册与选路(**禁一个全局"AVX2 模式"**),七类: calibration pixel transform / noise-SNR reductions / WCS-PSF 批量(热点才注册) / drizzle overlap-accumulate-normalize / UPM SpMV-residual-weight / rejection statistics / integration accumulate / HiPS bulk transform-encode(仅吞吐不改科学值)。每 kernel 记录 `science_contract_id, algorithm_id, kernel_version, precision, determinism_class`(SCI-XXX↔ALG-XXX 链,与 ALG 文档一一对应)。

## 6 失败与回退(05 §6)

1. 启动前预检失败→warning/backend event→回退 baseline→run 开始;
2. 计算中失败→**安全中止整个 stage,禁静默换 backend 混合结果**;
3. profile hash/ABI/kernel version 不匹配→该项或整体失效,走保守路线(baseline+动态 worker);
4. baseline 自检失败→不得运行,返回错误(exit 5 语义随 API-002 冻结)。

## 7 发布检查(05 §7)

每平台 `backends.manifest.json`: 文件名/hash/ABI/编译器/局部 flags/required_features/kernel 清单/自测结果;正式包只含 manifest 内文件;`doctor` 对每个 shipped backend"安全检测但不执行不支持指令"。

## 8 与 V5 任务的映射

| 05 条目 | 落点 |
|---|---|
| §1 变体策略 | ISA-001..005(profile 证明/NOT_SHIPPED/NOT_APPLICABLE 证据) |
| §2 编译隔离 | ABI-001(+CI opcode 扫描) |
| §3 六查+信任边界 | ABI-002(fake manifest/hash/ISA/path injection 测试) |
| §4 C ABI v1 | ABI-001(ABI layout tests/异常不跨边界) |
| §5 kernel 表 | ABI-003(baseline 全 kernel+affinity 多线程) |
| §6 回退 | BENCH-004(profile 失效/fallback)+API-002(exit) |
| §7 发布 | BENCH-005(doctor)+09 打包 |
