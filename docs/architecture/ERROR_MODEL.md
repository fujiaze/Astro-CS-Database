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

## 进程退出码（唯一源）

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

**判据 = 与 `lib/infrastructure/cli/exit_codes.h` 的 `astrocs::ExitCode` 枚举逐名逐值一致。**

阶段 stage IDs：P1.READ/P1.CALIBRATE/P1.STAR/P1.PSF/P1.PLATESOLVE/
P1.PHOTOMETRIC/P1.NOISE/P1.DRIZZLE/P1.HIPS_WRITE/P2.*。

## 契约

ERR-* 族（S2 注册，含 ERR-P2-UPM-001 畸形模型）。

ERR-P2-UPM-001 见 `lib/algorithms/coverage/src/upm.cpp:~890` frames 唯一/类型/C 行数校验。
