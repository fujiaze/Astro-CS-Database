# AstroCS CPU 能力探测公共层 (CPU-001)

> ID: MOD-CPU-CAPABILITY-001 · owner: SA-CPU-09 · 状态: FROZEN (CPU-001)
> 上游: BLD-003 安装骨架 / 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md §2
> 下游: CPU-002 baseline provider、CPU-003/004 AVX2/AVX-512 provider、
>       CPU-005 逐 kernel 路由（加载前 capability 判定）

## 1. 标识

| 项 | 值 |
|---|---|
| 组件 ID | `astrocs.cpu.capability` (CPU-001) |
| 交付形态 | 纯 C11 源码单元 `src/capability_detect.c` + 公共头 `include/astrocs/cpu/capability_v1.h`（本轮不建 SHARED target；CPU-002 起 provider DLL 链接本单元） |
| ABI | C ABI v1（`ACS_CAP_ABI_VERSION_V1`，POD 前两字段 struct_size+abi_version） |
| 平台 | AMD64（Linux 实测；Windows `_M_X64` 同源契约，实机验证由 WIN-* 承担） |
| 科学变更 | 无（纯探测/判定基础设施，`scientific_change=false`） |

## 2. 负责 / 不负责

负责：
- AMD64 CPUID 叶 0/1/7.0/80000000-4 只读探测：feature 位、厂商、family/model/stepping、brand；
- OSXSAVE + XGETBV(0) 实测 XCR0（仅当 OSXSAVE=1 才执行 XGETBV，探测自身永不触发非法指令）；
- **双平面判定**：`hw_features`（硬件支持 = CPUID 位）与 `os_safe`（硬件 ∩ OS 状态位 ∩ 组包含后可安全执行）；
- AVX/AVX2/FMA 组：OS 须保存 XMM|YMM（XCR0 0x6）；AVX-512 组：须保存 opmask|ZMM_Hi256|Hi16_ZMM（XCR0 0xE0）且 **F/CD/BW/DQ/VL 五子集全部硬件置位**（缺任一子集 → 组整体不在 os_safe 平面，拒绝）；
- BMI2 检测（HEALPix 位操作能力，不要求独立 DLL；C 约束 §C3）；
- `require_*` 判定 API：provider/调用方以 `required_features` 掩码查询 os_safe 平面子集满足；
- 稳定 JSON 序列化：Windows/Linux 同一 schema（`schemas/cpu_capability.schema.json`），无第三方 JSON 依赖；
- 探测执行不读取任何硬编码核心数（无逻辑核计数/线程建议，见 §7）。

不负责：
- 不实现 provider/self-test/kernel 路由（CPU-002/005）；
- 不做线程/worker 建议（CPU-008，host ThreadBudget）；
- 不做 cgroup/Job/affinity 有效核计算（CPU-008 / MON 系列）；
- 不读配置文件/不产生 profile（CPU-006/007）。

## 3. 双平面判定规则（15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md §2）

```text
启动时固定顺序: CPUID feature → OSXSAVE → XGETBV XMM/YMM/ZMM state
                → (CPU-002+) DLL ABI/hash → provider self-test
硬件支持但 OS 不保存寄存器时拒绝; AVX-512 至少检查所需 F/CD/BW/DQ/VL 子集,
不能只看一个 AVX512F。
```

实现映射（`cap_classify`）：
- 无需 OS 状态：SSE2/SSE4.1/SSE4.2/BMI1/BMI2 → `os_safe = hw`；
- AVX 家族（AVX/AVX2/FMA）：`osxsave=1 且 XCR0&0x6==0x6` 时进入 os_safe；
- AVX-512 五子集：`osxsave=1 且 XCR0&0xE0==0xE0 且 hw 含全部五子集` 时整体进入 os_safe；
- `os_safe_features_bitmask` 是加载判定的唯一依据；`hw_features_bitmask` 仅用于诊断（硬件有但 OS 拒绝的精确原因）。

## 4. 合同链接

| 链 | ID | 落点 |
|---|---|---|
| ARCH | DOC-ARCH-CPU-001 | `docs/architecture/cpu/CPU_001_CAPABILITY_PROBE.md` |
| API | API-CPU-001 | `providers/cpu/common/include/astrocs/cpu/capability_v1.h` |
| DATA | CPU-CAP-JSON-001 | `providers/cpu/common/schemas/cpu_capability.schema.json` |
| TEST | TEST-CPU001-* | `tests/cpu/dispatch/`（probe + feature matrix 模拟 + schema 校验） |
| 标准 | 15 §2 / C §C6 | 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md / AstroCS_ENGINEERING_CONSTRAINTS.md §C |

## 5. 公共 API 与主要符号

| 符号 | 语义 |
|---|---|
| `acs_cap_detect_v1` | 探测本机 → `acs_cap_result_v1`（原始 CPUID/OS 证据 + 双平面） |
| `acs_cap_feature_name_v1` | feature 位 → 冻结名称（JSON feature_names 事实源） |
| `acs_cap_os_safe_satisfies_v1` | required ⊆ os_safe（加载判定） |
| `acs_cap_hw_satisfies_v1` | required ⊆ hw（诊断用） |
| `acs_cap_os_saves_avx512_state_v1` | XCR0 0xE0 查询 |
| `acs_cap_serialize_json_v1` | 稳定 JSON（返回需要字节；缓冲不足可重试） |

主要实现符号：`cap_cpuid` / `cap_xgetbv0_impl` / `cap_classify` / `cap_append*`。

## 6. JSON schema（Windows/Linux 同一 schema）

`acs_cap_serialize_json_v1` 输出顶层字段固定：`schema_version`（const 1）、
`kind`（`astrocs_cpu_capability`）、`architecture`（`amd64`）、`vendor`/`brand`/
`family`/`model`/`stepping`、`cpuid{max_leaf,leaf1_ecx,leaf1_edx,leaf7_ebx,
leaf7_ecx,leaf7_edx}`、`os_state{osxsave,xcr0}`、
`features{hw[],os_safe[]}`、`hw_features_bitmask`、`os_safe_features_bitmask`、
`judgement{os_saves_xmm_ymm,os_saves_opmask_zmm,avx_family_os_safe,
avx512_subset_os_safe}`。
无浮点；全整数/字符串；键序固定 → 同机同构逐字节稳定。

## 7. 并发 / 线程 / 资源

- `reentrant=yes`；`threadsafe=yes`；`internal_parallel=none`；无分配、无锁、无全局可变状态；
- **不读取硬编码核心数**：本层不含逻辑核计数/affinity/cgroup 读取；可用核由 host
  executor/ThreadBudget 经 CPU-008 计算（约束 §D3/D4、15 §4 effective_available_workers）。

## 8. 验证命令（Linux 控制节点）

```bash
# 纯 C11 编译 (gcc/clang 严格 warning)
gcc   -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Iproviders/cpu/common/include -c providers/cpu/common/src/capability_detect.c
# probe + feature matrix + schema 校验 (tests/cpu/dispatch)
python3 tests/cpu/dispatch/run_cpu_capability_checks.py
```

Windows：`_M_X64` 分支经 `__cpuidex`/`_xgetbv` 同源编译面存在，实机验证由
WIN-* 在 Fatduck 执行（10 §5 PLATFORM_SCOPE）；JSON schema 与 Linux 同一
（验收：Windows/Linux 输出同一 schema）。

## 9. 已知限制 / 未实现项

- 仅 AMD64 语义权威；非 x86 返回 `ACS_CAP_ERR_UNSUPPORTED`；
- 本机 Linux amd64（Intel Xeon Gold 6148，KVM）实测含 AVX2/FMA/BMI2 与 AVX-512
  全五子集 → os_safe 全覆盖；**缺 AVX/缺 OS state/缺子集拒绝路径由 feature
  matrix 模拟负测覆盖**（真实无-AVX512 硬件由 WIN-*/CI 矩阵补）；
- Windows DLL/加载集成在 CPU-002 交付（本任务不建 SHARED target、不改根 CMake/安装布局）；
- 不包含扩展叶（TSX/ERMS/RDSEED 等）——本轮 provider 无需，属 CPU-006 指纹范围。
