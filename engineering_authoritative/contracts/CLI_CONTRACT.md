# AstroCS CLI Contract — FROZEN

> **状态：FROZEN（冻结）**
> **冻结日期：2026-07-30**
> **冻结任务：I-001**
> **权威源（SSOT）：本文件 + README §14**
> **实现位置：`lib/orchestrator/cpp/`**

---

## 0. FROZEN 声明

本契约自 2026-07-30 起冻结。已实现命令的签名、参数、JSONL 事件格式与错误码**不得修改**。扩展只能通过新增可选参数或新增命令（不得改已有命令语义）。

---

## 1. 已实现命令（FROZEN）

### 1.1 `stage1` — 单帧科学处理

```
orchestrator stage1 --frame <fits_path> --output <hiss_path>
    [--gaia-data <dir>]
    [--calibration-dir <dir>]
    [--filter <name>]
    [--config <json>]
    [--log-level <LEVEL>]
    [--request <file>]
    [--cancel-on-signal]
```

| 参数 | 必填 | 说明 |
|------|------|------|
| `--frame` | 是 | 输入 FITS Light 路径 |
| `--output` | 是 | 输出 HISS 文件路径 |
| `--gaia-data` | 否 | Gaia DR3SP 数据目录 |
| `--calibration-dir` | 否 | Master Bias/Dark/Flat 目录 |
| `--filter` | 否 | 滤镜名（覆盖 FITS header） |
| `--config` | 否 | stage1 配置 JSON |
| `--log-level` | 否 | 日志级别（DEBUG/INFO/WARN/ERROR） |
| `--request` | 否 | request JSON 模式（优先于其他参数） |
| `--cancel-on-signal` | 否 | 启用 Ctrl+C 取消（SIGINT→request_cancel） |

**输出**：stdout 输出 JSONL 事件流，stderr 输出日志。成功退出码 0。

### 1.2 `stage2` — 多帧合并

```
orchestrator stage2 --frames <hiss_dir> --output <hcsd_path>
    [--config <json>]
    [--log-level <LEVEL>]
    [--request <file>]
    [--cancel-on-signal]
```

| 参数 | 必填 | 说明 |
|------|------|------|
| `--frames` | 是 | 输入 HISS 文件目录 |
| `--output` | 是 | 输出 HCSD 文件路径 |
| `--config` | 否 | stage2 配置 JSON |
| `--log-level` | 否 | 日志级别 |
| `--request` | 否 | request JSON 模式 |
| `--cancel-on-signal` | 否 | 启用 Ctrl+C 取消 |

### 1.3 `inspect` — 元数据检查（不执行任务）

```
orchestrator inspect (--hiss <path> | --hcsd <path> | --frame <path> | --request <file>)
```

互斥参数优先级：`--hiss` > `--hcsd` > `--frame` > `--request`。

输出 JSON 到 stdout，含文件元数据（nside/n_pix/has_snr/provenance 等）。

### 1.4 `capabilities` — 能力查询

```
orchestrator capabilities
```

无参数。输出 JSON 到 stdout，包含：
- `schema_version`: 1
- `version`: "1.0.0"
- `modules[]`: 10 个模块（name/version/capabilities）
- `stages[]`: ["READ_FITS","CALIBRATE","PLATESOLVE","PSF","PHOTOMETRIC","SNR","DRIZZLE","STACK"]
- `commands[]`: ["run","run-batch","stage1","stage2","inspect","capabilities","status"]
- `request_commands[]`: ["stage1","stage2","inspect"]
- `schema_versions`: 各契约文件版本
- `exit_codes[]`: 11 个主退出码三元组（numeric_code/name/code）

---

## 2. JSONL 事件输出（FROZEN）

stdout 输出 JSONL（每行一个 JSON 事件）。事件类型：

| 事件 | 触发时机 | 关键字段 |
|------|---------|---------|
| `job_started` | 任务开始 | job_id, command |
| `stage_started` | 阶段开始 | job_id, stage, progress=0.0 |
| `stage_progress` | 阶段进度 | job_id, stage, progress |
| `stage_completed` | 阶段完成 | job_id, stage, progress=1.0 |
| `resource_wait` | 资源等待 | job_id, reason |
| `resource_spill` | 内存溢写 | job_id, chunk_id |
| `warning` | 警告 | job_id, message |
| `error` | 错误 | job_id, stage, error{code,numeric_code,message} |
| `job_completed` | 任务完成 | job_id, result |

扩展事件（`_ex` 后缀，含额外字段）：
- `stage_start` / `stage_end`: 含时间戳
- `result`: 含输出摘要
- `failed`: 失败终止

普通日志输出 stderr 或日志文件，不混入 stdout。

---

## 3. 退出码（FROZEN）

| numeric_code | name | string_code | 含义 |
|---|---|---|---|
| 0 | SUCCESS | ASTROCS_SUCCESS | 成功 |
| 1 | GENERIC_ERROR | ASTROCS_INTERNAL | 内部错误 |
| 2 | DLL_LOAD_FAILED | ASTROCS_MODULE_MISSING | 模块 DLL 加载失败 |
| 3 | BLOCK_MISSING | ASTROCS_BLOCK_MISSING | PipelineFrame 缺块 |
| 4 | CALIBRATE_FAILED | ASTROCS_CALIBRATION_MISSING | 校准失败 |
| 5 | PLATESOLVE_FAILED | ASTROCS_PLATESOLVE_FAILED | 板解失败 |
| 6 | DRIZZLE_FAILED | ASTROCS_DRIZZLE_FAILED | Drizzle 失败 |
| 7 | CONFIG_ERROR | ASTROCS_CONFIG_INVALID | 配置错误 |
| 8 | FILE_IO_ERROR | ASTROCS_FILE_IO_ERROR | 文件 I/O 错误 |
| 9 | TIMEOUT | ASTROCS_TIMEOUT | 操作超时 |
| 10 | CANCELLED | ASTROCS_CANCELLED | 用户取消 |

模块特定非退出码（20-29，出现在 JSONL error.numeric_code，不直接作为进程退出码）：

| 20 | STAR_DETECT_FAILED | 星点检测失败 |
| 21 | PSF_FAILED | PSF 拟合失败 |
| 22 | PHOTOMETRIC_FAILED | 测光失败 |
| 23 | SNR_FAILED | SNR 估计失败 |
| 24 | STACK_FAILED | 叠加失败 |
| 25 | HISS_INVALID | HISS 文件无效 |
| 26 | HCSD_INVALID | HCSD 文件无效 |
| 27 | MODULE_ABI_UNSUPPORTED | 模块 ABI 不兼容 |
| 28 | INPUT_INVALID | 输入无效 |

100+ 预留模块扩展码。

---

## 4. Planned 命令（未实现，不冻结）

README §14.1 要求但当前未实现的命令，标记为 PLANNED，不在本契约冻结范围：

| 命令 | 状态 | 说明 |
|------|------|------|
| `validate` | PLANNED | WCS 闭环/格式校验 |
| `browser` | PLANNED | 打开球面浏览器（独立 exe `healpix_browser_qt`） |
| `cancel` | PLANNED | 取消运行中任务（当前通过 `--cancel-on-signal` 实现） |
| `resume` | PLANNED | 从检查点恢复 |
| `benchmark` | PLANNED | 性能基准（当前通过 `browser_cli --benchmark` 实现） |

这些命令实现后须另行冻结。

---

## 5. 配置优先级（FROZEN）

参数解析优先级（高→低）：
1. `cli` — 命令行参数
2. `overrides` — request JSON 中的 cli_overrides
3. `config` — --config 指定的 JSON 文件
4. `default` — 内置默认值

---

**— END OF FROZEN CONTRACT —**
