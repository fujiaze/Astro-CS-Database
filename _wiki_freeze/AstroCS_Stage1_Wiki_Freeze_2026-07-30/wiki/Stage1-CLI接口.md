# Stage1 CLI 接口

## 状态

**范围已冻结；JSON 配置格式为草案，待用户确认后冻结。**

## 1. 设计原则

CLI 是底层运算接口：

- 参数显式；
- 默认少；
- 无交互；
- 无软警报；
- 确定性；
- 错误码稳定；
- 适合 GUI 调用。

## 2. 命令格式

```text
astrocs stage1 --config <config.json>
```

所有参数通过单一 JSON 配置文件传入。CLI 不接受散列命令行参数（--light / --master-dark 等），避免参数过长和难以复用。

## 3. JSON 配置格式（草案）

```json
{
  "light": "path/to/light.fts",
  "output": "path/to/output.hiss",
  "output_suffix": "",
  "calibration": {
    "mode": "standard",
    "master_dark": "path/to/master_dark.fts",
    "master_flat": "path/to/master_flat.fts",
    "master_bias": null
  },
  "nside": null,
  "gaia_data_dir": "GaiaDR3SP",
  "filter": "Red",
  "extra": {}
}
```

### 3.1 字段说明

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `light` | string | 是 | 单色 Light FITS 文件路径 |
| `output` | string | 是 | 输出 HISS 文件路径 |
| `output_suffix` | string | 否 | 输出文件附加后缀（如 `_calibrated`），为空则不加 |
| `calibration.mode` | string | 是 | `"standard"` 或 `"dark-optimization"` |
| `calibration.master_dark` | string | 是 | Master Dark 路径（已包含 Bias） |
| `calibration.master_flat` | string | 是 | Master Flat 路径（已校准归一化） |
| `calibration.master_bias` | string\|null | 条件必填 | Master Bias 路径；`mode="dark-optimization"` 时必填，`mode="standard"` 时可为 null |
| `nside` | int\|null | 否 | HEALPix NSIDE，2 的幂。null 时自动选择（1-2×过采样） |
| `gaia_data_dir` | string | 否 | Gaia 星表目录，默认 `"GaiaDR3SP"` |
| `filter` | string | 否 | 滤镜名（LUM/RED/GREEN/BLUE/HA/OIII），用于测光定标 |
| `extra` | object | 否 | 扩展字段，供未来使用 |

### 3.2 校准模式

**标准模式** (`mode: "standard"`)：

```
I_cal = (L - D) / F
```

- Master Dark 已包含 Bias，不再次减 Bias
- master_bias 可为 null

**暗场优化模式** (`mode: "dark-optimization"`)：

```
I_cal = (L - B - k*(D - B)) / F
```

- k = t_light / t_dark（曝光时间比例，从 FITS 头 EXPTIME 读取）
- master_bias 必填
- 若 FITS 头无 EXPTIME，返回硬错误

## 4. 不进入 CLI 的逻辑

CLI 不执行：

- 文件搜索；
- 自动匹配；
- 分组；
- session/date；
- 设备判断；
- GUI 警报；
- Master 生成；
- 裁切；
- Overscan；
- CFA；
- 自动切换校准模式；
- 自动接受或拒绝欠采样。

## 5. 硬错误

CLI 必须检查：

- JSON 配置可解析；
- 文件可读；
- 图像为单通道；
- 尺寸兼容；
- 必需 Master 存在；
- Master 数据有限；
- Flat 可除；
- NSIDE 合法（2 的幂）；
- WCS/PlateSolve 可用；
- 暗场优化模式有 EXPTIME；
- HISS 可原子写入。

CLI 返回：

- 稳定错误码；
- 机器可读错误字段；
- 简短人类说明；
- 不静默降级。

## 6. 事件输出

CLI 输出 JSONL 事件流到 stdout：

```jsonl
{"event":"job_started","frame":"light.fts","timestamp":"..."}
{"event":"stage_started","stage":"CALIBRATE","timestamp":"..."}
{"event":"stage_progress","stage":"CALIBRATE","percent":50,"timestamp":"..."}
{"event":"stage_completed","stage":"CALIBRATE","elapsed_ms":1234,"timestamp":"..."}
{"event":"warning","stage":"CALIBRATE","message":"...","timestamp":"..."}
{"event":"error","stage":"CALIBRATE","code":12,"message":"...","timestamp":"..."}
{"event":"job_completed","status":"PASS","elapsed_ms":5678,"timestamp":"..."}
```

事件类型：

| 事件 | 说明 |
|------|------|
| `job_started` | 任务开始 |
| `stage_started` | 某阶段开始 |
| `stage_progress` | 阶段内进度（0-100%） |
| `stage_completed` | 某阶段完成 |
| `warning` | 算法警告（仅算法事实，非用户决策警报） |
| `error` | 错误 |
| `job_completed` | 任务完成 |

像素和 Tile 数据不通过 JSON 输出。
