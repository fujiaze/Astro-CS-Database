# CPU AVX2/FMA provider（热点 kernel 后端）(CPU-003)

> ID: DOC-ARCH-CPU-003 · owner: SA-CPU-A11 · 状态: FROZEN (CPU-003, 2026-09-03)
> 上游: 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md §1/§2/§6 ·
>       AstroCS_ENGINEERING_CONSTRAINTS.md §C4-C6 · docs/architecture/CPU_BACKEND_ARCH.md
>       （ARCH-BACKEND-001 §1/§5/§6）· CPU_001_CAPABILITY_PROBE.md（CPU-001 os_safe 平面）·
>       baseline provider 合同（CPU-002, baseline_provider_v1.h）
> 下游: CPU-004 AVX-512 provider、CPU-005 逐 kernel 路由（以 kernel_id 选 provider）
> 实现: providers/cpu/avx2/（avx2_provider_v1.h + avx2_provider.cpp；target 单独
>       `-mavx2 -mfma`；Windows `/arch:AVX2`）
> 测试: tests/cpu/avx2/（gate stub 负测 + handshake + so_load + 对照 oracle runner）
> profile 台账: docs/architecture/ISA_VARIANTS.md（ISA-001/003 实测）·
>       artifacts/prerelease_v5/ISA-001/MEASUREMENTS.csv

## 1. 目标与验收

CPU-003 建立 AMD64 **AVX2/FMA provider**：只迁移 profile 指定的热点 kernel；
target 单独 `/arch:AVX2`（Linux `-mavx2 -mfma`）；函数入口由 provider 表查询，
不复制科学模块；其余 kernel 回落 baseline。

验收项（04_CPU_RESOURCE_TASKS.md CPU-003）：
1. 非支持 CPU 不加载：`required=(AVX|AVX2|FMA) ⊆ os_safe` 失败 → 拒绝；
2. CPUID/XGETBV negative：硬件缺 AVX2/FMA / OS 不保存 YMM → 拒绝（模拟负测）；
3. baseline 对照容差：热点输出与 baseline 相对差 ≤ 2e-4（ALG oracle 同规）；
4. FMA 是否改变归约顺序记录：本文件 §6；
5. 性能不是唯一通过条件：本任务无计时断言，只验证正确性/确定性/加载门
   （性能由 CPU-006/007 benchmark profile 决策，非 CPU-003 通过条件）。

## 2. 热点 kernel profile（ISA-001/003 实测台账，只迁移这些）

`docs/architecture/ISA_VARIANTS.md` 冻结的 ISA-001/003 实测（vm-bj，
median-of-5，best-of baseline 对变体最保守；工件
`artifacts/prerelease_v5/ISA-001/MEASUREMENTS.csv`）：

| kernel | baseline ns | avx2 变体 ns | 增益 | 决策 |
|---|---|---|---|---|
| calibration-pixel-transform (1M px) | 1 576 742 | 1 250 899 | **+20.7%** | **SHIP(avx2)** |
| hips-bulk-transform (1M px) | 16 922 530 | 12 145 714 | **+28.2%** | **SHIP(avx2)** |
| drizzle-accumulate (1M×3) | 2 587 976 | 2 981 571 | −15.2% | NOT_SHIPPED（变体更慢） |
| noise-snr-reductions (1M×3) | 35 079 711 | — | — | NOT_SHIPPED（排序型） |
| upm-spmv (512K nnz) | 1 980 485 | — | — | NOT_SHIPPED（gather 型） |
| integration-accumulate (1M×3) | 3 876 266 | — | — | NOT_SHIPPED |

ISA-003 独立复测确认：calibration +11.7%、hips +28.3% 仍 SHIP；AVX（无 FMA）
被 AVX2+FMA 严格主导（NOT_SHIPPED）；AVX-512 无额外收益（NOT_SHIPPED，
CPU-004 域另行验证）。

**CPU-003 注册热点 = `calibration-pixel-transform`（ALG-001）+
`hips-bulk-transform`（ALG-P3-002）—— 恰 2 个 kernel（v1 冻结）。**

## 3. 非热点回落 baseline（不复制科学模块）

本 provider 只注册 §2 的 2 个热点。`run_kernel` 对任何非注册索引（含
12-kernel baseline 世界里的其余 10 个 kernel）返回 `ACS_ERR_UNSUPPORTED`；
host 按 kernel_id 粒度在 provider 间逐 kernel 选路（CPU-005 语义），未注册
kernel 自动回落 baseline —— 高级 provider 不复制/不重复实现科学 kernel，
防三份科学算法漂移（02 §10.1 / 15 §1 / 约束 §C4）。

测试证据：
- `NONHOT_AVX2 NOT_FOUND`：`noise-snr-reductions` 在 avx2 表查不到
  （oracle runner 输出）；`NONHOT_BASE FOUND`：同 kernel 在 baseline 表可查；
- handshake/so_load 测试断言：`run_kernel(表外索引)` → `ACS_ERR_UNSUPPORTED`。

## 4. 加载门（非支持 CPU 不加载；CPUID/XGETBV negative）

query 期执行 `acs_cpu_avx2_cap_gate`：真实 `acs_cap_detect_v1`（CPUID +
OSXSAVE + XGETBV）+ `acs_cap_os_safe_satisfies_v1(cap, required)`，其中
`required = AVX | AVX2 | FMA`（`ACS_CPU_AVX2_REQUIRED_FEATURES`）。os_safe
平面由 CPU-001 classify 组包含语义保证：AVX 家族整组仅在
`OSXSAVE=1 且 XCR0.XMM|YMM (0x6)` 时进入 os_safe（capability_v1.h §位语义）。

| 场景 | 证据面 | 判定 |
|---|---|---|
| 非 amd64 / 探测失败 | detect 返回 UNSUPPORTED | 拒（ACS_ERR_UNSUPPORTED） |
| CPUID negative（硬件缺 AVX2/FMA） | hw 无 AVX 家族位 | 拒 |
| XGETBV negative（硬件有 AVX2 但 OS 不保存 YMM） | osxsave=0 / xcr0 缺 0x6 → os_safe 清除 AVX 家族 | 拒 |
| AVX2 机（OS 保存 XMM\|YMM） | os_safe 含 AVX\|AVX2\|FMA | 通过 |

负测以 stub 探测注入（tests/cpu/avx2/provider_avx2_capability_gate_test.c
链接期替换 `acs_cap_detect_v1`/`acs_cap_os_safe_satisfies_v1`，同 CPU-002
gate 测试法）；正测经真实 CPUID/XGETBV（本机 Xeon Gold 6148 含 AVX2+FMA）。

加载成功后才提供 kernel 服务；`self_test` 失败 / ABI 失配另按 ABI-002 拒绝。
编译隔离：本 provider TU 仅以 `-mavx2 -mfma` 编译（§8 反汇编证据），
`-mavx*` 不作用于 baseline/主 CLI（15 §6 / 约束 §C5）。

## 5. 函数入口由 provider 表查询

host/测试不硬编码"12-kernel baseline 索引映射"，而是以 **kernel_id** 查
`kernel_list()` 得索引再 `run_kernel()`（oracle main 的 `find_kernel`）；
provider 描述表（`acs_kernel_desc_v1`）与 baseline 用**同一科学 kernel 身份**
（`kernel_id`/`sci_contract_id` = ALG-001 / ALG-P3-002），只是该 kernel 的
AVX2+FMA ISA 实现。注册表只含热点子集是 CPU-003 与 CPU-002 的契约差异，
kernel_id 语义两 provider 一致（CPU-005 路由事实源）。

## 6. FMA 是否改变归约顺序记录（验收项 4）

逐 kernel 记录（对照 CPU-003 oracle：avx2 vs baseline 同输入同输出）：

| kernel | 公式形态 | 归约 | FMA 是否改变归约顺序 | 实测（本机） |
|---|---|---|---|---|
| calibration-pixel-transform | `o[i] = (a[i]−b[i]−k·c[i])·d[i]` | **无**（每元素独立标量表达式） | **否**——`-mfma` 仅把乘加链收缩为 vfnmadd（单次舍入替代两次中间舍入），无任何求和/项序 | max_rel=4.47e-06, max_ulp=89（有限 ULP 差，相对差远 < 2e-4） |
| hips-bulk-transform | `o[i] = (1−fx)(1−fy)v00 + fx(1−fy)v10 + (1−fx)fy·v01 + fx·fy·v11` | 固定 4 项序标量累加（源码序，无 -ffast-math/重排） | **否**——`-mfma` 收缩乘加链，**不重排项序**；输出逐元素独立 | max_rel=0, max_ulp=0（本样本 avx2 与 baseline 逐位一致） |

结论：两注册热点均**无跨项/跨线程归约**（每输出元素独立；对照 DET OK：
budget 1 vs 4 逐位相同）；FMA 只减少中间舍入次数，**不改变归约顺序**；
可能引入每元素 ≤ 数十 ULP 的舍入差（校准实测 max_ulp=89，对应
max_rel=4.47e-06）。科学值语义零变更（scientific_change=false）。

## 7. 容差冻结（15 §6 / 约束 §E3）

- **baseline 对照容差 = 2e-4 相对**（`|a−b|/max(1,|b|) ≤ 2e-4`），与 CPU-002
  科学 oracle（Python f64 参考实现, test_abi_kernels 同规）同数量级 ——
  baseline 已与 f64 独立参考一致（CPU-002 PASS），故 avx2 相对 baseline 在
  容差内 ⇔ 相对科学参考在容差内。
- 容差依据：ALG-001/ALG-P3-002 离散公式的 f32 数值路径（CPU-002 oracle
  2e-4 同规）；冻结于实现之前（本 provider 写码前即按 ISA_VARIANTS.md
  "variant Oracle 同式同序、允许 FMA 舍入差 2e-4" 记录）。
- 确定性：无跨线程归约 → 不随 worker 数变化（budget 1 vs 4 逐位相同，
  无需并行归约容差）。

## 8. 编译隔离证据

本机实测（g++ 14.2.0，Xeon Gold 6148）：

```text
avx2 provider TU (-mavx2 -mfma)  objdump: 43 处 ymm/vfmadd/vfnmadd 指令
baseline provider TU (无 -mavx*)  objdump: 0 处 ymm/vfmadd/vfnmadd
```

即：AVX2/FMA 指令只存在于 avx2 provider 目标；baseline/主 CLI 零 AVX 污染
（15 §6 / CPU_BACKEND_ARCH §2）。本 provider 无全局 SIMD 静态初始化
（全局对象仅 POD/字符串表；query 前不执行任何 AVX 指令，12 §7）。

## 9. 测试证据

tests/cpu/avx2/（runner: `python3 tests/cpu/avx2/run_provider_avx2_checks.py`）：
1. capability gate stub 负测：非 amd64 / 无 AVX2 hw / OS 禁 YMM → 拒；AVX2 机
   → 过（`provider_avx2_capability_gate_test` ALL PASS）；
2. handshake：真实 query OK + ABI 负测 + kernel_list=2（kernel_id 与
   ALG-001/ALG-P3-002）+ 表外索引 unsupported（`provider_avx2_handshake_test`
   ALL PASS）；
3. so_load：dlopen 唯一导出（module/backend 入口 NULL）+ kernel_list=2 +
   self_test + calibration 冒烟 + 表外 unsupported（`provider_avx2_so_load_test`
   ALL PASS）；
4. 对照 oracle：dlopen baseline.so + avx2.so，kernel_id 查询双跑 budget 1/4
   → DET OK（逐位）、ACQ 1/N、baseline 相对差 ≤ 2e-4、ULP_MAX 记录
   （`CPU-003 AVX2 PASS`）。

## 10. 已知限制

- 本 provider 只注册 2 个热点；其余 10 kernel 必须由 host 回落 baseline
  （本 provider 自身不做回落执行——回落是 host 路由语义，CPU-005 落成）；
- Windows `/arch:AVX2` DLL 实机验证在 WIN-* 域（本任务 Linux 同源
  `-mavx2 -mfma` 技术预览 + 反汇编证据）；
- drizzle-accumulate 实测变体更慢（−15.2%/−14.0%）→ 不注册（保持 baseline）；
  若未来主机/编译器表现不同，由 CPU-006/007 benchmark profile 重新决策
  （本文件不推断）。
