# Orchestrator（Stage1 编排器）

版本：AstroCS Orchestrator 2.0（Phase1 JSON 唯一入口）| C++17 | MSYS2 MinGW64

## 唯一运行方式

正式科学运行只有一条命令：

```powershell
orchestrator.exe <stage1.json>
```

所有输入、参数、精度模式、输出路径与日志位置只能来自该 JSON 文件。**不支持**任何子命令、长参数叠加、CLI 覆盖、REPL、`run` / `run-batch`。

非运行命令（仅诊断/校验）：

```powershell
orchestrator.exe --help
orchestrator.exe --version
orchestrator.exe --print-schema
orchestrator.exe --validate <stage1.json>
orchestrator.exe --inspect <file.hiss>   # LEGACY 诊断 (HISS 已 DEPRECATED)
```

- `--validate`：只做 Schema 校验，不执行流水线（输出 `VALID` 退出码 0；`INVALID` 退出码 1）
- `--inspect`：**LEGACY** 诊断 — 查看 legacy HISS 文件 metadata（precision_mode、signal_dtype、Tile 信息等），
  仅用于调试（HISS 已 DEPRECATED，正式产物为 HiPS）

## JSON Schema 与模板

- Schema（权威）：`configs/stage1.schema.json`，也可用 `orchestrator.exe --print-schema` 打印内嵌副本
- 模板（含全部路径、精度与输出字段）：`configs/stage1.template.json`
- 校验模板：`orchestrator.exe --validate configs/stage1.template.json`

## 顶层字段

| 字段 | 说明 |
| --- | --- |
| `schema_version` | 固定 `"1.0"` |
| `pipeline` | 固定 `"stage1"` |
| `precision` | `"fp32"`（默认）或 `"fp64"`，全链路同一精度 |
| `input` | `light` 亮场路径；`master_bias` / `master_dark` / `master_flat` 母版路径（可为 null） |
| `calibration` | `mode`（standard / optimal / exposure_ratio）、曝光时长、`fallback` |
| `platesolve` | Gaia 星表目录、最大星数、可选初始 RA/Dec |
| `psf` | 拟合半径、最大迭代、容差 |
| `photometric` | Gaia 光谱目录、滤光响应、QE 曲线 |
| `snr` | 估计器 id、采样尺度 |
| `drizzle` | `mode="precise"`、`pixfrac ∈ (0,1]`、`nside`（auto / explicit 16..4194304）、`ordering="nested"` |
| `output` | `hiss`（legacy 对比，可选）、`hips` 生产输出、`log` JSONL 日志、`diagnostics_dir`、`overwrite` |
| `execution` | `stop_after`（read / calibrate / platesolve / psf / photometric / snr / nside / drizzle / hips_verify / hiss_verify / browser_verify）、`threads`、`stage_timeout_sec` |

## 路径解析规则

- JSON 中所有相对路径以 **JSON 文件所在目录**为基准解析为绝对路径
- 绝对路径原样使用（统一规范化）
- 输出路径同样按此规则解析，运行前自动创建所需目录

## 校验与错误码

Schema 严格校验（`additionalProperties: false`，缺字段、未知字段、类型错误均拒绝）。进程退出码：

| 码 | 字符串码 | 含义 |
| --- | --- | --- |
| 0 | ASTROCS_SUCCESS | 成功 |
| 1 | ASTROCS_INTERNAL | 内部错误 |
| 2 | ASTROCS_MODULE_MISSING | DLL 加载失败 |
| 3 | ASTROCS_BLOCK_MISSING | 数据块缺失 |
| 4 | ASTROCS_CALIBRATION_MISSING | 校准失败 |
| 5 | ASTROCS_PLATESOLVE_FAILED | PlateSolve 失败 |
| 6 | ASTROCS_DRIZZLE_FAILED | Drizzle 失败 |
| 7 | ASTROCS_CONFIG_INVALID | 配置/Schema 无效 |
| 8 | ASTROCS_FILE_IO_ERROR | 文件 I/O 错误 |
| 9 | ASTROCS_TIMEOUT | 阶段超时 |
| 10 | ASTROCS_CANCELLED | 已取消 |

## Stage1 流水线（Phase1 V4 生产链）

```
READ_FITS → CALIBRATE → PSF → PLATESOLVE → PHOTOMETRIC → SNR → NSIDE
         → DRIZZLE (PRECISE) → HiPS 直写 (Drizzle→AIO, 无 HISS 中转) → HIPS_VERIFY
```

- 生产末端为 **HiPS 1.4**（signal/support 图像 + SNR Catalogue 三个独立子产品），
  HISS 已 deprecated，仅在 `validation.legacy_hiss_compare=true` 时作为对比产物写出；
- Browser 后端为 HiPS→AIO Reader（`browser_cli --hips <root>`）。

每阶段受 `stage_timeout_sec` 保护；超时/取消时按原子性规则清理部分输出。HIPS_VERIFY 遍历全部 Tile 与 SNR Catalogue 并完成完整性校验（HISS 路径已废弃）。

## 编译

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cd lib\orchestrator\cpp
make        # orchestrator.exe（动态加载各模块 DLL，静态链接 C++ 运行时）
```

## 目录

- `src/` — main.cpp、orchestrator.cpp、dll_loader.cpp、json_config.cpp、logger.cpp 等
- `include/` — orchestrator.h、dll_loader.h、json_config.h、logger.h 等
- `tests/` — 单元测试
- `configs/` — Schema 与 JSON 模板
- `Makefile` — 编译配置
