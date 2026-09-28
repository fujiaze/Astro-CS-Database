# CPU Backend 架构（C ABI v1 · Loader · Per-Kernel Dispatcher）

> 上游：ASTROCS_DESIGN.md §9（CPU 后端与资源）、§8.5（模块与 ABI）

## 1 设计结论

- 变体集合：baseline / AVX / AVX2+FMA / AVX-512，**profile 先行**——只对实测热点 kernel 做变体；每变体独立 Oracle；benchmark 只在正确变体间选最快；AVX-512 无实测收益则不选（原因、测量读数与代码去留见 `docs/architecture/ISA_VARIANTS.md`）。逐 kernel 选路结论为现行口径的唯一正本，本文件不复制读数。

## 2 编译隔离

- baseline 必须在最低 amd64 合同 CPU 上加载；变体 TU 局部编译选项；主 CLI 与 baseline 零 AVX 污染；**编译选项限于变体 TU 局部**（全局 `-march=native`/`/arch:AVX2`/`-mavx*` 属全局设置）；CI 反汇编/对象扫描 baseline opcode；变体与 baseline 共享同一数学合同头（唯一副本 = `lib/infrastructure/benchmark/backend_host/baseline_kernels.h`）。

## 3 CPU / OS 状态检测与信任边界

加载前六查（全过才执行）：① CPUID feature bits；② OSXSAVE；③ XGETBV（XMM/YMM/ZMM 保存）；④ 进程可用 CPU = affinity ∩ cgroup ∩ Job Object（非机器总核）；⑤ manifest `required_features`；⑥ 文件 sha256 + ABI 版本。

**信任边界**：Windows 受限 DLL 搜索目录；Linux 仅发行包相对私有目录；解析只走这两个可信来源（`PATH`/`LD_LIBRARY_PATH` 的值不参与）；开发覆盖 = 显式危险开关 + manifest 记录；host 只执行已过六查的 backend（无 illegal instruction 可能）。

## 4 稳定 C ABI v1

- 跨边界面：只传稳定 C ABI 类型；C++ STL / 异常 / RTTI / 编译器分配所有权留在边界内；异常处理限于边界内（测试断言）。
- 唯一入口：`astrocs_backend_get_api_v1(host_abi_version, host_struct_size, host*, out_api*)`；`struct_size` / version handshake 失配拒绝。
- 结构必含：`abi_version, struct_size`；`backend_id, backend_build_id, backend_sha256`；required/detected feature bits；对齐 / precision / determinism / aliasing 合同；allocator/log/cancel/thread-budget host callbacks；kernel capability 表 + 函数指针；`self_test()/warmup()/shutdown()`；结构化错误码。
- 内存：分配方释放或全部 host allocator；并发合同逐函数写明（可重入 / 线程安全 / 内部并行 / 嵌套并行）；host 传全局 thread budget，**backend 禁私有线程池**。

## 5 Kernel 注册粒度

逐 kernel 独立注册与选路（**禁一个全局「AVX2 模式」**），七类：calibration pixel transform / noise-SNR reductions / WCS-PSF 批量（热点才注册）/ drizzle overlap-accumulate-normalize / UPM SpMV-residual-weight / rejection statistics / integration accumulate / HiPS bulk transform-encode（仅吞吐不改科学值）。每 kernel 记录 `science_contract_id, algorithm_id, kernel_version, precision, determinism_class`（科学链与算法链一一对应，见 `docs/science/` 与 `docs/science/algorithms/`）。

## 6 失败与回退

1. 启动前预检失败 → warning / backend event → 回退 baseline → run 开始；
2. 计算中失败 → **安全中止整个 stage，禁静默换 backend 混合结果**；
3. profile hash / ABI / kernel version 不匹配 → 该项或整体失效，走保守路线（baseline + 动态 worker）；
4. baseline 自检失败 → 返回错误并停止执行（退出码唯一源 = `lib/infrastructure/cli/exit_codes.h`，表见 `docs/ASTROCS_DESIGN.md` §7.2；baseline 自检失败映射内部错误码）。

## 7 发布检查

每平台 `backends.manifest.json`：文件名 / hash / ABI / 编译器 / 局部 flags / required_features / kernel 清单 / 自测结果；正式包只含 manifest 内文件；`doctor` 对每个 shipped backend「安全检测但不执行不支持指令」。

## 8 条款落点映射

| 条款 | 落点 |
|---|---|
| §1 变体策略 | `docs/architecture/ISA_VARIANTS.md`（逐 kernel 选路结论）、`docs/architecture/ISA_BIT_MANIP_VARIANTS.md`；读数与决策原表 = `实验/engineering-evidence/prerelease-v5/` |
| §2 编译隔离 | `docs/architecture/abi/ABI_003_SECURE_LOADER.md`、`eng/tests/backend/test_isa_variants.py`（opcode 扫描与变体证明） |
| §3 六查 + 信任边界 | `docs/architecture/abi/ABI_003_SECURE_LOADER.md`、`eng/tests/backend/test_abi_loader.py`（fake manifest / hash / ISA / path injection 负例） |
| §4 C ABI v1 | `docs/architecture/abi/ABI_003_SECURE_LOADER.md`、`eng/tests/backend/test_abi_v1.py`（ABI layout 与异常不跨边界） |
| §5 kernel 表 | `eng/tests/backend/test_abi_kernels.py`（baseline 全 kernel + affinity 多线程） |
| §6 回退 | `eng/tests/backend/test_cpu_profile.py`（profile 失效 / fallback）+ `docs/ASTROCS_DESIGN.md` §7.2（退出码） |
| §7 发布 | `eng/packaging/verify_install_tree.py`、`docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv`（打包面登记） |
