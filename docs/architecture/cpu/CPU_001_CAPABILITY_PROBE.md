# CPU 能力探测与安全矩阵 (CPU-001)

> ID: DOC-ARCH-CPU-001 · owner: SA-CPU-09 · 状态: FROZEN (CPU-001, 2026-09-02)
> 上游: 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md §2 / AstroCS_ENGINEERING_CONSTRAINTS.md §C6
>       / 03_TARGET_PRODUCT_AND_ARCHITECTURE.md（providers/cpu 目标树）
> 下游: CPU-002 baseline、CPU-003 AVX2/FMA、CPU-004 AVX-512、CPU-005 路由
> 实现: providers/cpu/common/（capability_v1.h + capability_detect.c）
> 测试: tests/cpu/dispatch/（probe + feature matrix 模拟负测 + schema 校验）

## 1. 目标与验收

实现 AMD64 CPUID 叶、AVX/AVX2/FMA/AVX512F/CD/BW/DQ/VL、BMI2、OSXSAVE 与
XGETBV 检查；**区分“硬件支持”(hw) 与“OS 可安全执行”(os_safe)**。能力探测
必须先于任何高级 provider 调用（不调用高级 provider 后才检测）。

验收项（04_CPU_RESOURCE_TASKS.md CPU-001）：
1. 模拟 feature matrix（合成 CPUID/XCR0 输入跑判定引擎）→ 本机实测 + 模拟双证据；
2. 缺 AVX / 缺 OS state / 缺子集 → 拒绝（负测）；
3. Windows / Linux 输出同一 JSON schema（`cpu_capability.schema.json` 唯一事实源）；
4. 不读取硬编码核心数（探测层零核心计数）。

## 2. 探测序与信任边界

加载/执行高级 ISA 前固定顺序（15 §2 原文语义）：

```text
CPUID feature → OSXSAVE → XGETBV XMM/YMM/ZMM state → (CPU-002+) DLL ABI/hash
              → provider self-test
```

- `hw_features`：CPUID 位平面（硬件支持的事实）。
- `os_safe`：`hw ∩ OS 状态位 ∩ 组包含` 后可安全执行平面。
- **provider 加载判定只使用 os_safe**：硬件支持但 OS 不保存对应寄存器 → 拒绝
  （不在 os_safe → `acs_cap_os_safe_satisfies_v1(required)` 返回 0）。
- AVX-512 至少检查所需 F/CD/BW/DQ/VL 子集与 XCR0 opmask/ZMM 状态；**不能只看
  一个 `AVX512F`**（C §C6 / 15 §2 / CPU-004 验收“缺任何子集/OS ZMM state 拒绝”）。

探测实现自身只执行 SSE2 可执行指令 + cpuid + xgetbv；XGETBV 仅在 OSXSAVE=1
后执行 → 探测永不触发非法指令。本层不读取逻辑核数、affinity、cgroup（CPU-008）。

## 3. 判定规则（cap_classify，单事实源）

| 组 | os_safe 进入条件 |
|---|---|
| SSE2/SSE4.1/SSE4.2/BMI1/BMI2 | 无需 OS 状态：`os_safe |= hw` |
| AVX 家族（AVX/AVX2/FMA） | `osxsave=1 且 XCR0&0x6==0x6`（XMM\|YMM 保存） |
| AVX-512 五子集（F/CD/BW/DQ/VL） | `osxsave=1 且 XCR0&0xE0==0xE0`（opmask\|ZMM_Hi256\|Hi16_ZMM）**且五子集 hw 全置** |

位语义（v1 冻结，只允许尾部追加）：
`SSE2=1<<0, SSE4_1=1<<1, SSE4_2=1<<2, AVX=1<<3, AVX2=1<<4, FMA=1<<5,
BMI1=1<<6, BMI2=1<<7, AVX512F=1<<8, AVX512CD=1<<9, AVX512BW=1<<10,
AVX512DQ=1<<11, AVX512VL=1<<12`。

组掩码：`AVX_FAMILY = AVX|AVX2|FMA`；`AVX512_SUBSET = F|CD|BW|DQ|VL`。

## 4. JSON schema（Windows/Linux 同一）

- 唯一事实源：`providers/cpu/common/schemas/cpu_capability.schema.json`
  （draft 2020-12；`additionalProperties:false`；全部 required 枚举冻结）。
- `acs_cap_serialize_json_v1` 输出与该 schema 一一对应；无浮点；键序固定 →
  同机同构输出逐字节稳定（可哈希/可比对）。
- 键：`schema_version/kind/architecture/vendor/brand/family/model/stepping/
  cpuid{...}/os_state{osxsave,xcr0}/features{hw[],os_safe[]}/
  hw_features_bitmask/os_safe_features_bitmask/judgement{...}`。
- Windows `_M_X64` 与 Linux `__x86_64__` 共享同一 C ABI 头与同一序列化格式
  （同源契约）；实机验证由 WIN-* 承担。

## 5. C ABI 稳定性

- `acs_cap_result_v1` 等 POD 前两字段 `struct_size + abi_version`
  （`ACS_CAP_ABI_VERSION_V1=1`）；失配返回 `ACS_CAP_ERR_ABI_MISMATCH`，不做布局猜测。
- 错误码数值与 `include/astrocs/abi/status_codes.h` `acs_status` 对齐
  （OK=0/ERR_PARAM=1/ERR_ABI_MISMATCH=2/ERR_UNSUPPORTED=5）。
- 纯 C11（extern "C" 兼容 C++17）；禁 STL/异常/RTTI；无第三方依赖；
  跨边界无托管分配、无 opaque handle。

## 6. 模拟 feature matrix（测试事实源）

`tests/cpu/dispatch/cpu_capability_matrix_test.c` 以合成
`acs_cap_result_v1`（直接置 CPUID 证据 + XCR0/OSXSAVE）驱动与生产同一的
`cap_classify` 语义（经公开 API `acs_cap_detect_v1` 后绕开不可注入的实测分支，
复用 `acs_cap_os_safe_satisfies_v1` 判定）——负测不触碰生产探测路径外的代码，
判定函数即生产函数。矩阵行（节选）：

| 行 | hw CPUID | OSXSAVE/XCR0 | 预期 os_safe | 预期判定 |
|---|---|---|---|---|
| baseline 机 | SSE2/SSE4.1/SSE4.2 | 0/0 | SSE2..SSE4_2 | 拒 AVX/AVX2/AVX512 |
| 全 AVX2 机（OS 禁 AVX） | AVX/AVX2/FMA hw | osxsave=0 | 无 AVX 家族 | **拒 AVX2（OS state）** |
| AVX2 机 | AVX/AVX2/FMA hw | osxsave=1, xcr0=0x6 | +AVX/AVX2/FMA | AVX2 过；AVX512F 拒 |
| AVX512F-only | F hw；CD/BW/DQ/VL 缺 | xcr0=0xE6 | AVX 家族（若有）；AVX-512 无 | **拒 AVX-512（缺子集）** |
| 全 AVX-512 | 五子集全 | osxsave=1, xcr0=0xE6 | +AVX512 全组 | AVX512 组过 |
| 全 AVX-512（OS 禁 ZMM） | 五子集全 | xcr0=0x6（仅 XMM/YMM） | 无 AVX-512 | **拒 AVX512F（OS ZMM state）** |

另有非法位拒绝、required=0 通过、hw-only 诊断（os_safe 拒但 hw 过）等负/正用例。

## 7. 与相邻任务边界

| 任务 | 边界 |
|---|---|
| CPU-002 baseline | 加载 `require` 判定消费本层 os_safe；本层不建 DLL |
| CPU-003/004 | provider 目标在编译/加载各自查 `required_features ⊆ os_safe`；本层不含任何 AVX 指令 |
| CPU-005 路由 | 逐 kernel provider 选择前先 capability 判定 |
| CPU-006/007 | profile 指纹含本层 CPUID/XCR0 输出；本层不读写 profile |
| CPU-008 | 有效核/线程建议不属本层（本层零核心计数） |
