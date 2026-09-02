> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/architecture/ERROR_MODEL.md

# AstroCS 错误分类与错误码 (V19)

> 本文件是故障诊断与机器一致性检查的权威来源。
> `tools/astrocs_diagnose.py` 按本表的正则模式归类日志信号。

## 阶段 ID (stage IDs)

```text
READ_FITS   CALIBRATE   PLATESOLVE   PSF   PHOTOMETRIC
SNR         NSIDE       DRIZZLE      HIPS_WRITE   HIPS_VERIFY
STAGE2_DISCOVER  STAGE2_COVERAGE  STAGE2_UPM  STAGE2_REJECT
STAGE2_INTEGRATE STAGE2_HIPS
```

## 错误分类表

| 代码 | 名称 | 阶段 | 含义 | 修复路径 |
|---|---|---|---|---|
| E100 | DLL_LOAD_FAILED | 任意 | 动态库未加载 (依赖/路径) | `.\toolchain.ps1 check`; 检查 mingw64 PATH 与 DLL 搜索路径 |
| E200 | BLOCK_MISSING | 任意 | 必需命名块缺失 | 核对上游阶段顺序与 stop_after 配置 |
| E300 | CALIBRATE_FAILED | CALIBRATE | 校准失败 | 核对 master 文件、BINNING/FILTER |
| E400 | PLATESOLVE_FAILED | PLATESOLVE | WCS 求解失败 | 检查初值/Gaia 目录/SIP order |
| E500 | PSF_FAILED | PSF | PSF 拟合失败 | 检查星点块/拟合半径/饱和 |
| E600 | PHOTOMETRIC_FAILED | PHOTOMETRIC | 测光定标失败 | 检查 filters/QE/Gaia 检索 |
| E700 | SNR_FAILED | SNR | SNR/噪声模型失败 | 检查 psf 块、sigma_residual、gain/readnoise |
| E800 | DRIZZLE_FAILED | DRIZZLE | Drizzle 失败 | 检查 WCS/SIP、pixfrac∈(0,1]、nside 2 幂 |
| E900 | HIPS_WRITE_FAILED | HIPS_WRITE | HiPS 直写失败 | 检查输出目录/tile_depth=9/nside>=512 |
| E910 | HIPS_VERIFY_FAILED | HIPS_VERIFY | HiPS 验证失败 | 用 aio_hips_reader 检查各产品语义 |
| E920 | STAGE2_FAILED | STAGE2_* | Phase2 失败 | 检查 stage2 JSON/gate 日志; rejection_cli 复现 |
| E950 | CONFIG_ERROR | 任意 | 配置非法 | 对照 docs/CONFIG_REFERENCE.md |
| E960 | FILE_IO_ERROR | 任意 | 文件 IO 失败 | 检查路径/权限/完整性 |
| E970 | GENERIC_ERROR | 任意 | 通用错误 | 按日志定位; 保留现场 |
| E980 | TIMEOUT | 任意 | 阶段超时 | ASTROCS_DRIZZLE_FINE_PROFILE=1 定位热点 |

## 退出码 (orchestrator AstroCsExitCode)

```text
0 SUCCESS          1 GENERIC_ERROR       2 DLL_LOAD_FAILED
3 BLOCK_MISSING    4 CALIBRATE_FAILED    5 PLATESOLVE_FAILED
6 DRIZZLE_FAILED   7 CONFIG_ERROR        8 FILE_IO_ERROR
```

Phase2 状态码:

```text
P2_INTEGRATE_OK=0 / NO_CANDIDATES=1 / ALL_REJECTED=2 /
ZERO_VALID_WEIGHT=3 / INVALID_INPUT=4
P2_STATUS_OK / UNDERDETERMINED / MIN_SAMPLES / INVALID_METHOD /
INVALID_CONFIGURATION
```

## 每阶段诊断记录字段 (DIAGNOSTICS.md)

```text
run_id frame_id stage_id input_hash config_hash output_hash
wall_cpu RSS bytes IO_bytes threads status error upstream_cause artifacts
```
