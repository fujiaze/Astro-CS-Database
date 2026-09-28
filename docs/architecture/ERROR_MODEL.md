# Error Model

> 上游：docs/ASTROCS_DESIGN.md §7.3（错误传播与运行日志）、§8（软件架构）

## 类别

CONFIG / INPUT_CORRUPT / DEPENDENCY / NUMERIC / NO_DATA / RESOURCE /
TIMEOUT / IO / SCIENCE_GATE / INTERNAL（口径 = `docs/standards/LOGGING_DIAGNOSTICS_STANDARD.md`）。

## 阶段 ID

每阶段一个阶段 ID 前缀，逐节点一枚，节点名与注册表节点一致：

| 阶段 | 阶段 ID |
| --- | --- |
| normalize | `P1.READ` / `P1.CALIBRATE` / `P1.STAR` / `P1.PSF` / `P1.PLATESOLVE` / `P1.PHOTOMETRIC` / `P1.NOISE` / `P1.DRIZZLE` / `P1.HIPS_WRITE` |
| mosaic | `P2.COVERAGE` / `P2.SAMPLER` / `P2.UPM` / `P2.REJECTION` / `P2.INTEGRATE` / `P2.HIPS_WRITE` |
| export | `P3.PROPERTIES` / `P3.WCS` / `P3.RESAMPLE` / `P3.WRITE` / `P3.VERIFY` |

## 稳定错误语义

- C API：0=success；非 0=hard error（类别由调用上下文/troubleshooting 定位）。
- 可恢复科学状态经 status 字段（UNDERDETERMINED / NO_CANDIDATES /
  ALL_REJECTED / ZERO_VALID_WEIGHT / INVALID_INPUT）。
- 每个 high-risk error → troubleshooting 条目（docs/diagnostics/）。

## 进程退出码（唯一源）

> **退出码唯一源 = `lib/infrastructure/cli/exit_codes.h`**（11 码，与最高设计 §7.2 同源）：
>
> ```text
> OK=0  ARGS=2  INPUT=3  SCIENCE=4  BACKEND=5  COMPUTE=6
> IO=7  INTEGRITY=8  CANCELLED=9  RESOURCE=10  INTERNAL=70
> ```
>
> 凡需要退出码数值处**一律引用该头文件**；本文档与任何下级文档的数值表只有这一套
> （最高设计 §7.2：码值与含义只有一份）。

模块特定非进程退出码（JSONL error.numeric_code，20-29）：

```text
STAR_DETECT_FAILED=20  PSF_FAILED=21  PHOTOMETRIC_FAILED=22
SNR_FAILED=23  STACK_FAILED=24  HISS_INVALID=25  HCSD_INVALID=26
MODULE_ABI_UNSUPPORTED=27  INPUT_INVALID=28
MODULE_SPECIFIC_BASE=100
```

**机器判据 = 「进程退出码（唯一源）」节 11 码与 `lib/infrastructure/cli/exit_codes.h` 的 `astrocs::ExitCode` 枚举逐名逐值一致，由 `eng/tools/docs_machine_consistency.py`（error_taxonomy_exit_codes）执行；上方模块特定码为 JSONL `error.numeric_code` 面，与进程退出码分立。**

## 契约

稳定错误码族 `ERR-*` 的登记面 = `docs/TRACEABILITY.csv` 的 `error_codes` 列。

以 `ERR-P2-UPM-001`（UPM 模型文件畸形）为例：f
rames 非数组、frames 内重复、帧数与 C 行数不等，都在模型打开时判错并沿 `p2_upm_open` 上行
（实现 `lib/algorithms/coverage/src/upm.cpp` 的 frames 校验）。
