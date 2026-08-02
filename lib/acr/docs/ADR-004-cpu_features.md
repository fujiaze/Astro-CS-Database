# ADR-004: google/cpu_features v0.9.0 ISA 安全门禁

| 项目 | 值 |
| --- | --- |
| 状态 | Accepted |
| 日期 | 2026-08-02 |
| 依赖锁 | `lib/acr/docs/dependency-lock.json` |
| 版本 | google/cpu_features v0.9.0（commit `v0.9.0`） |
| 许可证 | Apache-2.0 |
| 平台 | Windows / Linux |
| 形态 | 编译型库 |

## 状态

Accepted。google/cpu_features 作为 ACR 唯一的运行时 CPU ISA 检测来源，承担安全门禁职责：加载任何 ISA 专用插件前必须先经 cpu_features 校验，禁止非法指令执行。

## 背景

ACR 支持多档 ISA 插件（SSE / AVX / AVX2 / FMA / AVX-512 子集）。直接编译 `-mavx2` 等指令集后运行在不含该指令集的 CPU 上会触发 `SIGILL` / `EXCEPTION_ILLEGAL_INSTRUCTION`，导致进程崩溃。ACR 必须在加载 ISA 插件前精确检测目标 CPU 支持哪些指令集子集。

关键约束：**AVX-512 子集必须精确匹配，不能用单一 bool 表达**。AVX-512F / AVX-512CD / AVX-512VL / AVX-512BW / AVX-512DQ 等子集相互独立，CPU 可能支持其中任意组合。若用单 bool（"支持 AVX-512"）会导致在仅支持部分子集的 CPU 上加载错误插件。

`__builtin_cpu_supports` 是 GCC/Clang 专属，MSVC 不支持；手写 cpuid 在不同 ISA 子集的 leaf/位掩码上极易出错，且不可移植到非 x86。

cpu_features **只回答"能不能"运行，不回答"是不是最快"**——性能最优路径由 ACR 标定阶段实测决定。

## 决策

1. 引入 google/cpu_features v0.9.0 作为 ACR CPU ISA 检测的唯一来源。
2. 检测项：SSE / SSE2 / SSE3 / SSSE3 / SSE4.1 / SSE4.2 / AVX / AVX2 / FMA / AVX-512 各子集（F/CD/VL/BW/DQ/IFMA/VBMI 等）。
3. 安全门禁：**加载任何 ISA 插件前必须调用 cpu_features 校验**，不匹配则拒绝加载并回退 baseline。
4. AVX-512 子集以独立 bool 集合表达，禁止合并为单一 "AVX-512" 标志。
5. 检测结果作为设备指纹的一部分序列化。
6. baseline 路径（无任何扩展）永远可用，不依赖任何 ISA 检测。

## 理由

- Google 维护，跨 x86/ARM/loongarch 等多架构，覆盖 ACR 未来平台扩展。
- Apache-2.0 许可证宽松。
- API 简洁（`GetX86Features()` 返回结构体），结果稳定。
- 编译期不强制 `-mavx*`，插件可独立编译为多档，运行时按检测结果加载。

## 集成边界

- **职责内**：检测 SSE/AVX/AVX2/FMA/AVX-512 子集、安全门禁（拒绝非法插件加载）、设备指纹输出。
- **职责外**：性能最优路径选择（由 ACR 标定阶段实测，cpu_features 只回答"能不能"）、GPU 能力检测（由 CUDA/HIP runtime API）。
- **门禁边界**：所有 ISA 插件加载入口必须经过 cpu_features 校验函数，无例外；baseline 插件无需校验。
- **降级边界**：检测失败或 CPU 不支持任何扩展时，baseline 路径必须可用，不阻塞运行。
- **API 边界**：`X86Features` 等类型不得出现在公共 API 签名中，封装为 ACR 自有 ISA mask 类型。

## 替代方案

1. **`__builtin_cpu_supports`**：
   - 未采用：GCC/Clang 专属，MSVC 不支持，跨平台性差。
2. **手写 cpuid 解析**：
   - 未采用：AVX-512 子集 leaf（7,0）位掩码复杂，极易出错；ARM 无 cpuid；维护成本高且不可靠。
3. **`/proc/cpuinfo` 解析**：
   - 未采用：Linux 专属，Windows 不可用；flags 字符串格式不稳定，AVX-512 子集覆盖不全。
4. **操作系统 API（`IsProcessorFeaturePresent` 等）**：
   - 未采用：Windows API 不覆盖 AVX-512 子集；Linux 无等价 API。

## 未采用原因

`__builtin_cpu_supports` 与手写 cpuid 在跨平台与 AVX-512 子集精度上不达标；`/proc/cpuinfo` 与 OS API 在 Windows 与 AVX-512 子集覆盖上均有缺口。

## 验收实验

| 实验 | 目标 | 通过条件 |
| --- | --- | --- |
| 能力 mask | 检测当前 CPU ISA | 与 `lscpu` / `coreinfo` 一致（含 AVX-512 子集） |
| 拒绝非法插件 | 在无 AVX2 的 CPU 上加载 AVX2 插件 | 拒绝加载，回退 baseline，无 SIGILL |
| baseline 可用 | 禁用所有 ISA 插件 | 程序正常运行，输出正确结果 |
| 跨平台编译 | Windows + Linux 均编译通过 | MSVC + GCC/Clang 均无错误 |
| AVX-512 子集精度 | 仅支持部分子集的 CPU | 仅加载匹配子集的插件，不因部分支持而误判 |

实验日志写入 `run/logs/acr/cpu_features/<YYYYMMDD>/`。
