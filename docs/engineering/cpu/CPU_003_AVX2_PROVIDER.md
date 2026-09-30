# CPU AVX2/FMA provider（热点 kernel 后端）

> 上游：docs/ASTROCS_DESIGN.md §9（CPU 后端与资源）、docs/ASTROCS_DESIGN.md §9（CPU 后端与资源））、
>       docs/engineering/CPU_BACKEND_ARCH.md、docs/engineering/ISA_VARIANTS.md（逐 kernel 选路）、
>       docs/engineering/cpu/CPU_001_CAPABILITY_PROBE.md（os_safe 能力平面）

## 1. 目标与验收

AVX2/FMA provider 建立 AMD64 **AVX2/FMA 后端**：只迁移 profile 指定的热点 kernel；
target 单独 `/arch:AVX2`（Linux `-mavx2 -mfma`）；函数入口由 provider 表查询，
不复制科学模块；其余 kernel 回落 baseline。

- 实现面：`lib/infrastructure/benchmark/cpu/avx2/`（`include/astrocs/cpu/avx2_provider_v1.h` +
  `src/avx2_provider.cpp`）。
- 测试面：`eng/tests/cpu/avx2/`（gate stub 负测 + handshake + so_load + 对照 oracle runner）。

验收项：
1. 非支持 CPU 不加载：`required=(AVX|AVX2|FMA) ⊆ os_safe` 失败 → 拒绝；
2. CPUID/XGETBV negative：硬件缺 AVX2/FMA / OS 不保存 YMM → 拒绝（模拟负测）；
3. baseline 对照容差：热点输出与 baseline 相对差 ≤ 2e-4（科学 oracle 同规）；
4. FMA 是否改变归约顺序的记录见 §6；
5. 性能不是通过条件：本 provider 只对正确性/确定性/加载门负责；逐 kernel 性能决策属
   benchmark profile 面。

## 2. 注册热点与选路（冻结）

注册热点 = `calibration-pixel-transform` 与 `hips-bulk-transform`，**恰 2 个 kernel**；
其余 kernel 不注册（回落 baseline）。逐 kernel 选路正本 = `docs/engineering/ISA_VARIANTS.md`
§1/§2：这两个 kernel 登记 avx2 变体；`drizzle-accumulate` 保持 baseline；
`noise-snr-reductions`（排序型）、`upm-spmv`（gather 型）、`integration-accumulate`
保持 baseline；AVX（无 FMA）与 AVX-512 不登记变体。逐 kernel 计时读数、增益与反汇编计数见
`实验/engineering-evidence/prerelease-v5/`（`ISA-001` / `ISA-002` / `ISA-003` 子目录）；
本文件只承载结构决策与冻结容差。

## 3. 非热点回落 baseline（不复制科学模块）

本 provider 只注册 §2 的 2 个热点。`run_kernel` 对任何非注册索引（baseline 内核表里的
其余 kernel）返回 `ACS_ERR_UNSUPPORTED`；host 按 `kernel_id` 粒度在 provider 间逐 kernel
选路，未注册 kernel 自动回落 baseline —— 高级 provider 不复制/不重复实现科学 kernel，
防三份科学算法漂移。

测试判据：
- `NONHOT_AVX2 NOT_FOUND`：`noise-snr-reductions` 在 avx2 表查不到（oracle runner 输出）；
  `NONHOT_BASE FOUND`：同 kernel 在 baseline 表可查；
- handshake/so_load 测试断言：`run_kernel(表外索引)` → `ACS_ERR_UNSUPPORTED`。

## 4. 加载门（非支持 CPU 不加载；CPUID/XGETBV negative）

query 期执行 `acs_cpu_avx2_cap_gate`：真实 `acs_cap_detect_v1`（CPUID + OSXSAVE +
XGETBV）+ `acs_cap_os_safe_satisfies_v1(cap, required)`，其中 `required = AVX | AVX2 | FMA`
（`ACS_CPU_AVX2_REQUIRED_FEATURES`）。os_safe 平面由 capability classify 组包含语义保证：
AVX 家族整组仅在 `OSXSAVE=1 且 XCR0.XMM|YMM (0x6)` 时进入 os_safe
（`lib/infrastructure/benchmark/cpu/common/` 的 `capability_v1.h` 位语义）。

| 场景 | 证据面 | 判定 |
|---|---|---|
| 非 amd64 / 探测失败 | detect 返回 UNSUPPORTED | 拒（`ACS_ERR_UNSUPPORTED`） |
| CPUID negative（硬件缺 AVX2/FMA） | hw 无 AVX 家族位 | 拒 |
| XGETBV negative（硬件有 AVX2 但 OS 不保存 YMM） | osxsave=0 / xcr0 缺 0x6 → os_safe 清除 AVX 家族 | 拒 |
| AVX2 机（OS 保存 XMM\|YMM） | os_safe 含 AVX\|AVX2\|FMA | 通过 |

负测以 stub 探测注入（`eng/tests/cpu/avx2/provider_avx2_capability_gate_test.c` 链接期替换
`acs_cap_detect_v1`/`acs_cap_os_safe_satisfies_v1`，同 baseline gate 测试法）；
正测经真实 CPUID/XGETBV。

加载成功后才提供 kernel 服务；`self_test` 失败 / ABI 失配按 provider 装载合同拒绝
（`docs/engineering/CPU_BACKEND_ARCH.md`）。编译隔离：本 provider TU 仅以 `-mavx2 -mfma`
编译，`-mavx*` 不作用于 baseline/主 CLI（核对方式见 §8）。

## 5. 函数入口由 provider 表查询

host/测试不硬编码「baseline 内核索引映射」，而是以 **kernel_id** 查 `kernel_list()` 得索引
再 `run_kernel()`（oracle main 的 `find_kernel`）；provider 描述表（`acs_kernel_desc_v1`）
与 baseline 用**同一科学 kernel 身份**（`kernel_id` 与 `sci_contract_id` 逐条相同），
只是该 kernel 的 AVX2+FMA ISA 实现。注册表只含热点子集是 avx2 provider 与 baseline
provider 的契约差异，kernel_id 语义两 provider 一致（逐 kernel 路由的事实源）。

## 6. FMA 与归约顺序（验收项 4）

| kernel | 公式形态 | 归约 | FMA 是否改变归约顺序 |
|---|---|---|---|
| calibration-pixel-transform | `o[i] = (a[i]−b[i]−k·c[i])·d[i]` | 无（每元素独立标量表达式） | 否——`-mfma` 把乘加链收缩为 vfnmadd（一次舍入替代两次中间舍入），无任何求和/项序改动 |
| hips-bulk-transform | `o[i] = (1−fx)(1−fy)v00 + fx(1−fy)v10 + (1−fx)fy·v01 + fx·fy·v11` | 固定 4 项序标量累加（源码序，无 `-ffast-math`/重排） | 否——`-mfma` 收缩乘加链、不重排项序；输出逐元素独立 |

结论：两个注册热点均**无跨项/跨线程归约**（每输出元素独立）；FMA 只减少中间舍入次数，
**不改变归约顺序**，每元素舍入差落在 ULP 量级，判据 = §7 的 baseline 对照容差。来源：
IEEE 754-2019 的 fusedMultiplyAdd 条款（融合乘加只做一次舍入）；对照实现 = baseline provider
的同一 kernel 源（`lib/infrastructure/benchmark/backend_host/baseline_kernels_impl.inc`
共享源，仅编译旗标不同）；实测读数见 `实验/engineering-evidence/prerelease-v5/`。

## 7. 容差冻结

- **baseline 对照容差 = 2e-4 相对**（`|a−b|/max(1,|b|) ≤ 2e-4`），与 baseline provider 的
  科学 oracle（Python f64 参考实现）同规 —— baseline 已与 f64 独立参考一致，故 avx2 相对
  baseline 在容差内 ⇔ 相对科学参考在容差内。
- 容差依据：`calibration-pixel-transform` 与 `hips-bulk-transform` 离散公式的 f32 数值
  路径；与 `docs/engineering/ISA_VARIANTS.md` §2 的 variant Oracle 口径一致（与 baseline
  同公式同序、共享源，只允许 FMA 舍入差 2e-4 相对）。
- 确定性：无跨线程归约 ⇒ 输出不随 worker 数变化，无需并行归约容差。

## 8. 编译隔离核对

- 本 provider TU 以 `-mavx2 -mfma` 整 TU 局部旗标编译；baseline provider TU 与主 CLI 不带
  `-mavx*`。核对方式 = 对两个 TU 各做一次反汇编扫描：avx2 TU 含 FMA 特征指令与 256-bit
  VEX，baseline TU 不出现 VEX/ymm/zmm ⇒ AVX2/FMA 指令只存在于 avx2 provider 目标；
- 本 provider 无全局 SIMD 静态初始化（全局对象仅 POD/字符串表；query 前不执行任何 AVX 指令）。

## 9. 测试证据

`eng/tests/cpu/avx2/`（runner：`python3 eng/tests/cpu/avx2/run_provider_avx2_checks.py`）：
1. capability gate stub 负测：非 amd64 / 无 AVX2 hw / OS 禁 YMM → 拒；AVX2 机 → 过
   （`provider_avx2_capability_gate_test`）；
2. handshake：真实 query OK + ABI 负测 + `kernel_list` 列出 2 个热点 kernel_id +
   表外索引 unsupported（`provider_avx2_handshake_test`）；
3. so_load：dlopen 唯一导出（module/backend 入口为空）+ `kernel_list` 2 项 + `self_test` +
   calibration 冒烟 + 表外 unsupported（`provider_avx2_so_load_test`）；
4. 对照 oracle：dlopen baseline 与 avx2 两个 DSO，按 kernel_id 查询、worker 预算 1/4 双跑
   → 逐位判据、1/N 一致性、baseline 相对差 ≤ 2e-4、ULP 上界记录
   （`eng/tests/cpu/avx2/provider_avx2_oracle_main.cpp`）。

## 10. 边界

- 本 provider 只注册 2 个热点；其余 kernel 必须由 host 回落 baseline（回落是 host 路由语义，
  provider 自身不做回落执行）；
- Windows `/arch:AVX2` 实机验证属发行验证面；Linux 侧同源 `-mavx2 -mfma` 编译与反汇编
  核对在本面完成；
- `drizzle-accumulate` 变体更慢 ⇒ 不注册（保持 baseline）；若未来主机/编译器表现不同，
  由 benchmark profile 重新决策（本文件不推断）。
