# Error Model

## 类别

CONFIG / INPUT_CORRUPT / DEPENDENCY / NUMERIC / NO_DATA / RESOURCE /
TIMEOUT / IO / SCIENCE_GATE / INTERNAL（DIAGNOSTICS_STANDARD）。

## 阶段 ID

P1.READ / P1.CALIBRATE / P1.STAR / P1.PSF / P1.PLATESOLVE /
P1.PHOTOMETRIC / P1.NOISE / P1.DRIZZLE / P1.HIPS_WRITE /
P2.COVERAGE / P2.SAMPLER / P2.UPM / P2.REJECTION / P2.INTEGRATE /
P2.HIPS_WRITE。

## 稳定错误语义

- C API：0=success；非 0=hard error（类别由调用上下文/troubleshooting 定位）。
- 可恢复科学状态经 status 字段（UNDERDETERMINED / NO_CANDIDATES /
  ALL_REJECTED / ZERO_VALID_WEIGHT / INVALID_INPUT）。
- 每个 high-risk error → troubleshooting 条目（docs/diagnostics/）。

## 进程退出码（唯一源，DOC-202 R07 订正）

> ⛔ **原「编排退出码（AstroCsExitCode）」表已作废**（2026-09-20，DOC-202 / R07）：
> 该表是 `orchestrator.exe` 的**第二套**进程退出码（1/3/4/9/10 的含义与最高设计 §6.3 不同），
> 而 orchestrator **已不是入口**（最高设计 §7.1/§11「旧可执行程序不是入口」；
> §7.1 退役计划点名删除 `lib/infrastructure/pipeline/orchestrator/cpp/`）。
> 原文留痕（仅追溯，不得再被引用）：
> `SUCCESS=0 GENERIC_ERROR=1 DLL_LOAD_FAILED=2 BLOCK_MISSING=3 CALIBRATE_FAILED=4
> PLATESOLVE_FAILED=5 DRIZZLE_FAILED=6 CONFIG_ERROR=7 FILE_IO_ERROR=8 TIMEOUT=9 CANCELLED=10`
> （源：`lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h`）。
>
> **退出码唯一源 = `lib/infrastructure/cli/exit_codes.h`**（11 码，与最高设计 §6.3 同源）：
>
> ```text
> OK=0  ARGS=2  INPUT=3  SCIENCE=4  BACKEND=5  COMPUTE=6
> IO=7  INTEGRITY=8  CANCELLED=9  RESOURCE=10  INTERNAL=70
> ```
>
> 凡需要退出码数值处**一律引用该头文件**；**禁止**在本文档或任何下级文档重定义第二套数值表
> （最高设计 §6.3：码值与含义只有一份）。

模块特定非进程退出码（JSONL error.numeric_code，20-29）：

```text
STAR_DETECT_FAILED=20  PSF_FAILED=21  PHOTOMETRIC_FAILED=22
SNR_FAILED=23  STACK_FAILED=24  HISS_INVALID=25  HCSD_INVALID=26
MODULE_ABI_UNSUPPORTED=27  INPUT_INVALID=28
MODULE_SPECIFIC_BASE=100
```

V19R3 修正记录（**已随 R07 作废，仅留痕**）：此前文档误写 ARGS_ERROR/
STAGE_CONFIG_ERROR/STAGE_RUNTIME_ERROR/DATA_NOT_FOUND/SCIENCE_GATE_FAILED/
INTERNAL_ERROR（与实现不一致的假 PASS，AUDIT_DECISION §12），V19R3 曾把本表
「与 orchestrator.h 枚举 name+value 集合完全一致」当作修正依据——该依据随
orchestrator 退出（§7.1）与本节作废一并失效。**现行判据 = 与
`lib/infrastructure/cli/exit_codes.h` 的 `astrocs::ExitCode` 枚举逐名逐值一致。**

阶段 stage IDs：P1.READ/P1.CALIBRATE/P1.STAR/P1.PSF/P1.PLATESOLVE/
P1.PHOTOMETRIC/P1.NOISE/P1.DRIZZLE/P1.HIPS_WRITE/P2.*。

## 契约

ERR-* 族（S2 注册，含 ERR-P2-UPM-001 畸形模型）。

ERR-P2-UPM-001 见 `lib/algorithms/coverage/src/upm.cpp:~890` frames 唯一/类型/C 行数校验。
