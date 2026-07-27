# P06-003 HCSD 输出与独立读取 - 独立复核报告

- 任务编号：P06-003
- 复核日期：2026-07-27
- 复核人：子 Agent（GLM-5.2，独立复核视角）
- 复核对象：`engineering/evidence/P06-003/` 全部证据
- 提交基线：4ccb507（P06-002 球面梯度与稳健叠加证据 VERDICT: PASS）

---

## 1. 复核范围

本复核独立审查 P06-003 任务的所有证据，验证：

1. **子叶索引结构**：leaf_index 49152 项的 leaf_ipix 一致性、data_offset/data_length 合理性、sum(data_length)=n_pix。
2. **metadata 完整性**：JSON 头必填字段（nside/nested/n_pix/has_snr）与 caller 元数据。
3. **输入追溯**：HCSD meta 的 n_frames 与源 HISS 文件数一致。
4. **inspect --hcsd 独立读取**：DLL 加载、JSONL 输出、统计与 stage2 日志一致。
5. **字节级结构**：Magic/JSON头/leaf_index/sorted_ipix 升序/文件大小。
6. **按子叶读取正确性**：aio_hcsd_read_leaf 定位逻辑与全量读取一致。
7. **字节级可重现**：T1 SHA-256 与 P00-003 baseline 一致。
8. **是否未修改业务源码**（只读验证任务约束）。

## 2. 复核方法

1. **结构化 JSON 审查**：逐项检查 `hcsd_validation_results.json` 的 7 项结果字段一致性。
2. **inspect 输出核对**：核对 `inspect_hcsd.json` 与 `inspect_hcsd_T6.json` 的 magic/nside/n_pix/meta_json。
3. **字节级解析核对**：核对 `hcsd_binary_parse_T1.json` 与 `hcsd_binary_parse_T6.json` 的 leaf_index 统计与升序检查。
4. **按子叶读取核对**：核对 `verify_read_leaf_T1.json`（79/79）与 `verify_read_leaf_T6.json`（6/6）的 pass_count。
5. **SHA-256 完整性**：核对 T1_baseline.hcsd 与 P00-003 stage2_baseline.hcsd 的 SHA-256 一致。
6. **stage2 日志一致性**：核对 P06-002 T1_baseline 的 stage2_stderr.log 中 n_frames/n_pix/mean_pixel_count。
7. **Git 状态验证**：确认未修改业务源码（仅新增证据目录 + 修改任务注册表/控制文件）。

## 3. 复核结果

### 3.1 子叶索引结构

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| T1 leaf_index 项数 | 49152 | 49152 | ✓ |
| T1 非空子叶数 | 78（= P00-003 baseline） | 78 | ✓ |
| T1 leaf_ipix 一致 | leaf_ipix[i] == i | True | ✓ |
| T1 sum(data_length) | = n_pix = 15522966 | 15522966 | ✓ |
| T1 空子叶 data_offset | = 0 | True | ✓ |
| T6 leaf_index 项数 | 49152 | 49152 | ✓ |
| T6 非空子叶数 | >0 | 5 | ✓ |
| T6 sum(data_length) | = n_pix = 1566 | 1566 | ✓ |

### 3.2 metadata 完整性

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| T1 nside | 32768 | 32768 | ✓ |
| T1 nested | true | true | ✓ |
| T1 n_pix | 15522966 | 15522966 | ✓ |
| T1 has_snr | false（HCSD 强制） | false | ✓ |
| T1 filter | 存在 | Red | ✓ |
| T1 n_frames | = 输入 HISS 数 = 2 | 2 | ✓ |
| T1 sigma_clip | {sigma, max_iter} | {3.0, 5} | ✓ |
| T1 stack_stats | {mean_pixel_count, median_exposure} | {1.9850, 180.000} | ✓ |
| 无 format_version | 不存在（§9.1） | 不存在 | ✓ |
| 无校验和 | 不存在（§9.2） | 不存在 | ✓ |

### 3.3 输入追溯

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| HCSD n_frames | = 输入 HISS 数 | 2 = 2（frame1.hiss + frame2.hiss） | ✓ |
| HCSD n_pix | = stage2 日志唯一像素数 | 15522966 = 15522966 | ✓ |
| HCSD mean_pixel_count | = stage2 日志统计 | 1.9850 = 1.9850 | ✓ |

### 3.4 inspect --hcsd 独立读取

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| T1 exit code | 0 | 0 | ✓ |
| T1 DLL 加载 | 全部成功 | 9/9 模块 | ✓ |
| T1 输出事件 | result + completed | result + completed（JSONL） | ✓ |
| T1 magic | HCSD | HCSD | ✓ |
| T1 nside/n_pix | 32768/15522966 | 32768/15522966 | ✓ |
| T6 exit code | 0 | 0 | ✓ |
| T6 magic/nside/n_pix | HCSD/2048/1566 | HCSD/2048/1566 | ✓ |

### 3.5 字节级结构

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| T1 Magic | "HCSD" | "HCSD" | ✓ |
| T1 leaf_index_bytes | 1179648 | 1179648 | ✓ |
| T1 sorted_ipix 升序 | 是 | True（抽样 20 万项） | ✓ |
| T1 sorted_ipix 按 leaf 排序 | 是 | True | ✓ |
| T1 file_size | 187455430 | 187455430 | ✓ |
| T6 Magic | "HCSD" | "HCSD" | ✓ |
| T6 sorted_ipix 升序 | 是 | True（全量 1566 项） | ✓ |
| T6 file_size | 1198683 | 1198683 | ✓ |

### 3.6 按子叶读取正确性

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| T1 非空子叶检查数 | 78 | 78 | ✓ |
| T1 空子叶检查数 | 1 | 1 | ✓ |
| T1 pass_count | 79 | 79 | ✓ |
| T1 fail_count | 0 | 0 | ✓ |
| T6 pass_count | 6 | 6 | ✓ |
| T6 fail_count | 0 | 0 | ✓ |
| 子叶 ipix/pixel 与全量一致 | 是 | 是 | ✓ |

### 3.7 字节级可重现

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| T1 SHA-256 | 2A9BD12E...4122C37 | 2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37 | ✓ |
| P00-003 baseline SHA-256 | 2A9BD12E...4122C37 | 2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37 | ✓ |
| SHA-256 一致 | 是 | 是 | ✓ |
| 文件大小一致 | 187455430 | 187455430 | ✓ |

### 3.8 业务源码未修改

| 复核项 | 期望 | 实际 | 结果 |
|---|---|---|---|
| .cpp/.h 文件修改 | 无 | 无（仅新增 evidence 脚本 + 报告） | ✓ |
| 控制文件更新 | 仅任务注册表/状态 | 仅 MASTER_TASK_REGISTER.csv/PROJECT_STATE.yaml/CURRENT_TASK.md | ✓ |

## 4. 风险评估

| 风险 | 等级 | 缓解 | 是否阻塞 PASS |
|---|---|---|---|
| 无 format_version 字段 | 低 | contract §9.1 v1.1+ 待修复 | 否 |
| 无校验和 | 中 | contract §9.2, §7, 损坏检测依赖 magic + JSON 解压 | 否 |
| meta 无 input_hiss_files | 低 | n_frames 已足够追溯, contract §4.3 不强制 | 否 |
| N_LEAVES 硬编码 49152 | 低 | contract §9.4 设计现状 | 否 |
| DLL 路径需用 lib/orchestrator/cpp/ | 低 | P06-002 已确认, 非本任务引入 | 否 |

## 5. 复核结论

所有 7 项验证（17 个子测试）全部 PASS：
1. 子叶索引结构正确（leaf_ipix 一致, sum=n_pix, 非空子叶数与 baseline 一致）
2. metadata 完整（必填字段 + caller 元数据齐全, 已知缺口确认）
3. 输入追溯有效（n_frames/n_pix/mean_pixel_count 与 stage2 日志一致）
4. inspect --hcsd 独立读取成功（DLL 全加载, JSONL 输出完整）
5. 字节级结构符合契约（Magic/JSON头/leaf_index/sorted_ipix 升序/文件大小）
6. 按子叶读取正确（aio_hcsd_read_leaf 定位逻辑与全量读取逐子叶一致）
7. HCSD 字节级可重现（T1 SHA-256 = P00-003 baseline SHA-256）

**VERDICT: PASS**

本任务证明 HCSD 输出符合 FROZEN v1.0 格式契约，支持子叶索引按需读取与独立 inspect 验证，可作为 P07 阶段的输入。
