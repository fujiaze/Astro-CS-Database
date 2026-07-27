# P06-003 HCSD 输出与独立读取 - 测试报告

- 任务编号：P06-003
- 执行日期：2026-07-27
- 测试人：子 Agent（GLM-5.2）
- 合约：`engineering/contracts/hcsd_format_v1.md`（FROZEN v1.0）

---

## 1. 测试环境

- 操作系统：Windows
- Python：3.x（zstandard 0.25.0）
- orchestrator.exe：`lib/orchestrator/cpp/orchestrator.exe`（4126364 bytes）
- DLL：全部加载成功（AIO/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE/GRADIENT_SPHERE/STACK）
- 工作目录：`f:\Astro dev\Astro CS Normalization Database`

## 2. 测试项与结果

### 2.1 子叶索引 leaf_index 验证

**测试 1a：T1_baseline.hcsd leaf_index 结构**

| 输入 | 命令 | 期望 | 实际 | 结果 |
|---|---|---|---|---|
| T1_baseline.hcsd | `python parse_hcsd_binary.py <file> 200000` | n_leaves=49152, leaf_ipix 一致, sum(data_length)=n_pix | n_leaves_read=49152, leaf_ipix_consistent=True, non_empty=78, sum_data_length=15522966=n_pix | **PASS** |

**测试 1b：T6_run1.hcsd leaf_index 结构**

| 输入 | 命令 | 期望 | 实际 | 结果 |
|---|---|---|---|---|
| T6_run1.hcsd | `python parse_hcsd_binary.py <file> 100000` | n_leaves=49152, leaf_ipix 一致, sum(data_length)=n_pix | n_leaves_read=49152, leaf_ipix_consistent=True, non_empty=5, sum_data_length=1566=n_pix | **PASS** |

**测试 1c：非空子叶数与 P00-003 baseline 一致**

| 输入 | 期望 | 实际 | 结果 |
|---|---|---|---|
| T1_baseline.hcsd | 78/49152（P00-003 baseline） | 78/49152 | **PASS** |

### 2.2 metadata 验证

**测试 2a：T1_baseline 必填字段**

| 字段 | 期望 | 实际 | 结果 |
|---|---|---|---|
| nside | 32768 | 32768 | **PASS** |
| nested | true | true | **PASS** |
| n_pix | ~15522966 | 15522966 | **PASS** |
| has_snr | false | false | **PASS** |

**测试 2b：T1_baseline caller 元数据**

| 字段 | 期望 | 实际 | 结果 |
|---|---|---|---|
| filter | Red | Red | **PASS** |
| n_frames | 2 | 2 | **PASS** |
| total_exposure_s | 360.000 | 360.000 | **PASS** |
| sigma_clip.sigma | 3.0 | 3.0000 | **PASS** |
| sigma_clip.max_iter | 5 | 5 | **PASS** |
| stack_stats.mean_pixel_count | ~1.98 | 1.9850 | **PASS** |
| stack_stats.median_exposure | ~180 | 180.000 | **PASS** |

**测试 2c：已知缺口确认**

| 缺口 | 期望 | 实际 | 结果 |
|---|---|---|---|
| format_version 字段 | 不存在（§9.1） | 不存在 | **PASS** |
| 校验和 | 不存在（§9.2） | 不存在 | **PASS** |

### 2.3 输入追溯验证

**测试 3a：n_frames 与输入 HISS 数一致**

| HCSD 文件 | HCSD n_frames | 输入 HISS 文件 | 一致 | 结果 |
|---|---|---|---|---|
| T1_baseline.hcsd | 2 | frame1.hiss + frame2.hiss = 2 | 是 | **PASS** |

**测试 3b：n_pix 与 stage2 日志一致**

| HCSD 文件 | HCSD n_pix | stage2 日志唯一像素数 | 一致 | 结果 |
|---|---|---|---|---|
| T1_baseline.hcsd | 15522966 | 15522966（`[hp_stack_hiss] 第一遍扫描完成: 15522966 唯一像素`） | 是 | **PASS** |

**测试 3c：mean_pixel_count 与 stage2 日志一致**

| HCSD 文件 | HCSD mean_pixel_count | stage2 日志 | 一致 | 结果 |
|---|---|---|---|---|
| T1_baseline.hcsd | 1.9850 | 1.9850（`mean_pixel_count=1.9850`） | 是 | **PASS** |

### 2.4 inspect --hcsd 独立读取验证

**测试 4a：T1_baseline inspect --hcsd**

| 项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| exit_code | 0 | 0 | **PASS** |
| DLL 加载 | 全部成功 | 全部成功（9/9 模块） | **PASS** |
| 输出事件 | result + completed | result + completed（JSONL） | **PASS** |
| magic | HCSD | HCSD | **PASS** |
| nside | 32768 | 32768 | **PASS** |
| n_pix | 15522966 | 15522966 | **PASS** |
| meta_json | 存在 | 存在且完整 | **PASS** |

命令：`orchestrator.exe inspect --hcsd engineering/evidence/P06-002/T1_baseline/output/T1_baseline.hcsd`

**测试 4b：T6_run1 inspect --hcsd**

| 项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| exit_code | 0 | 0 | **PASS** |
| magic | HCSD | HCSD | **PASS** |
| nside | 2048 | 2048 | **PASS** |
| n_pix | 1566 | 1566 | **PASS** |

**测试 4c：P04-003 复验**

P04-003 已测 inspect --hiss/--hcsd/--frame（317 集成测试 PASS）。本任务复验 inspect --hcsd 成功，**PASS**。

### 2.5 HCSD 字节级结构验证

**测试 5a：T1_baseline 字节级结构**

| 项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| Magic | "HCSD" (0x48 0x43 0x53 0x44) | "HCSD" | **PASS** |
| uncomp_json_len | >0 | 228 | **PASS** |
| comp_json_len | >0 | 178 | **PASS** |
| leaf_index_bytes | 1179648 | 1179648 | **PASS** |
| sorted_ipix 升序 | 是（抽样 20 万项） | is_ascending=True | **PASS** |
| sorted_ipix 按 (leaf_ipix, ipix) 排序 | 是 | is_leaf_sorted=True | **PASS** |
| file_size | 12+178+1179648+15522966*12 = 187455430 | 187455430 | **PASS** |

**测试 5b：T6_run1 字节级结构**

| 项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| Magic | "HCSD" | "HCSD" | **PASS** |
| uncomp_json_len | >0 | 333 | **PASS** |
| comp_json_len | >0 | 231 | **PASS** |
| leaf_index_bytes | 1179648 | 1179648 | **PASS** |
| sorted_ipix 升序 | 是（全量 1566 项） | is_ascending=True | **PASS** |
| sorted_ipix 按 (leaf_ipix, ipix) 排序 | 是 | is_leaf_sorted=True | **PASS** |
| file_size | 12+231+1179648+1566*12 = 1198683 | 1198683 | **PASS** |

### 2.6 按子叶读取 aio_hcsd_read_leaf 验证

**测试 6a：T1_baseline 按子叶读取**

| 项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| 非空子叶检查数 | 78 | 78 | **PASS** |
| 空子叶检查数 | 1 | 1 | **PASS** |
| 总检查数 | 79 | 79 | **PASS** |
| pass_count | 79 | 79 | **PASS** |
| fail_count | 0 | 0 | **PASS** |
| 每个子叶 ipix 与全量读取一致 | 是 | 全部一致 | **PASS** |
| 每个子叶 pixel 与全量读取一致 | 是 | 全部一致 | **PASS** |

命令：`python verify_read_leaf.py <T1_baseline.hcsd>`

**测试 6b：T6_run1 按子叶读取**

| 项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| 总检查数 | 6 | 6 | **PASS** |
| pass_count | 6 | 6 | **PASS** |
| fail_count | 0 | 0 | **PASS** |

### 2.7 HCSD 字节级可重现

**测试 7：T1_baseline 与 P00-003 baseline SHA-256 一致**

| 项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| T1_baseline.hcsd SHA-256 | 2A9BD12E...4122C37 | 2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37 | **PASS** |
| P00-003 stage2_baseline.hcsd SHA-256 | 2A9BD12E...4122C37 | 2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37 | **PASS** |
| SHA-256 一致 | 是 | 是 | **PASS** |
| 文件大小一致 | 187455430 | 187455430 | **PASS** |

## 3. 测试汇总

| 类别 | 测试项数 | PASS | FAIL |
|---|---|---|---|
| 子叶索引 | 3 | 3 | 0 |
| metadata | 3 | 3 | 0 |
| 输入追溯 | 3 | 3 | 0 |
| inspect 独立读取 | 3 | 3 | 0 |
| 字节级结构 | 2 | 2 | 0 |
| 按子叶读取 | 2 | 2 | 0 |
| 字节级可重现 | 1 | 1 | 0 |
| **总计** | **17** | **17** | **0** |

**VERDICT: PASS**
