# P06-003 HCSD 输出与独立读取 - 任务执行报告

- 任务编号：P06-003
- 阶段：P06（Stage2 真实数据验证）
- Gate：G6
- 执行日期：2026-07-27
- 执行人：子 Agent（GLM-5.2）
- 提交基线：4ccb507（P06-002 球面梯度与稳健叠加证据 VERDICT: PASS）
- 入口条件：P06-002 DONE（7/7 PASS）；P04-003 DONE（inspect --hiss/--hcsd/--frame 测试，317 集成测试）
- 合约：`engineering/contracts/hcsd_format_v1.md`（FROZEN v1.0）

---

## 1. 任务目标

验证 HCSD 输出文件的子叶索引、metadata、输入追溯和浏览器/独立读取，证明 Stage2 产出的 HCSD 文件符合格式契约且可独立解析。

## 2. 执行步骤

1. 子叶索引（leaf_index）验证：解析 49152 项索引表，统计非空子叶数，验证 leaf_ipix 一致性与 data_offset/data_length 合理性。
2. metadata 验证：检查 JSON 头必填字段（nside/nested/n_pix/has_snr）与 caller 元数据。
3. 输入追溯验证：验证 HCSD meta 中 n_frames 与输入 HISS 文件数一致。
4. inspect --hcsd 独立读取验证（P04-003 复验）：运行 inspect --hcsd，检查 JSONL 输出。
5. HCSD 字节级结构验证（加分项）：Python 脚本解析 Magic/JSON 头/leaf_index/sorted_ipix。
6. 按子叶读取（aio_hcsd_read_leaf）正确性验证：Python 模拟按子叶读取，与全量读取逐子叶比较。

## 3. 测试输入

| 文件 | 大小(bytes) | SHA-256 | 来源 | nside | n_pix |
|---|---|---|---|---|---|
| `engineering/evidence/P06-002/T1_baseline/output/T1_baseline.hcsd` | 187455430 | `2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37` | P06-002 T1（与 P00-003 baseline 一致） | 32768 | 15522966 |
| `engineering/evidence/P06-002/T6_determinism/output/T6_run1.hcsd` | 1198683 | - | P06-002 T6（梯度校正启用） | 2048 | 1566 |

选择两个文件的原因：
- T1_baseline：大文件（nside=32768, 78 非空子叶），与 P00-003 baseline 字节级一致，验证生产规模。
- T6_run1：小文件（nside=2048, 5 非空子叶），可完整读取全部 ipix/pixel 做逐子叶对比。

## 4. 验证结果总览

| 序号 | 验证项 | 结果 | 证据 |
|---|---|---|---|
| 1 | 子叶索引 leaf_index 结构 | **PASS** | T1: 78/49152 非空, leaf_ipix 一致, sum(data_length)=n_pix; T6: 5/49152 |
| 2 | metadata 必填字段 + caller 元数据 | **PASS** | nside/nested/n_pix/has_snr 全部符合; filter/n_frames/sigma_clip/stack_stats 齐全 |
| 3 | 输入追溯（HCSD ← HISS） | **PASS** | n_frames=2 = 输入 HISS 数 2; n_pix 与 stage2 日志一致; mean_pixel_count 一致 |
| 4 | inspect --hcsd 独立读取 | **PASS** | DLL 全加载, 输出 result+completed JSONL, 统计与 stage2 日志一致 |
| 5 | HCSD 字节级结构 | **PASS** | Magic/JSON头/leaf_index/sorted_ipix 升序/文件大小 全部验证通过 |
| 6 | 按子叶读取 aio_hcsd_read_leaf | **PASS** | T1: 79/79; T6: 6/6 逐子叶 ipix+pixel 与全量读取完全一致 |
| 7 | HCSD 字节级可重现 | **PASS** | T1 SHA-256 = P00-003 baseline SHA-256 |

**总计：7/7 PASS，0 FAIL**

## 5. 关键发现

### 5.1 子叶索引结构正确

- T1_baseline：78 个非空子叶（与 P00-003 baseline 记录一致），leaf_ipix 与数组下标 100% 一致，空子叶 data_offset=0/data_length=0，sum(data_length)=15522966=n_pix。
- T6_run1：5 个非空子叶（连续 33075-33079），data_offset 连续递增（0, 1544, 3968, 5112, 11304）。
- 子叶位移计算验证：nside=32768 → shift=18；nside=2048 → shift=10，与 `hio_compute_leaf_shift` 实现一致。

### 5.2 metadata 完整

- 必填字段全部存在且符合契约：nside（32768/2048）、nested（true）、n_pix（15522966/1566）、has_snr（false）。
- caller 元数据齐全：filter、n_frames、total_exposure_s、sigma_clip{sigma,max_iter}、stack_stats{mean_pixel_count,median_exposure}。
- T6_run1 额外记录了 sigma_clip.method=winsorized、winsorize_low/high、gradient_correction 配置。

### 5.3 已知缺口确认（不阻塞 PASS）

| 缺口 | 契约引用 | 影响 | 状态 |
|---|---|---|---|
| 无 format_version 字段 | §9.1 | 无法区分版本 | v1.1+ 待修复 |
| 无校验和 | §9.2, §7 | 损坏无法检测 | v1.1+ 待修复 |
| meta 无显式 input_hiss_files | §4.3 | 仅靠 n_frames 追溯 | 不强制 |
| N_LEAVES 硬编码 49152 | §9.4 | 仅支持 nside=64 子叶 | 设计现状 |
| data_offset/data_length 单位混淆 | §9.6 | 字节 vs 像素易误用 | v1.1+ 待修复 |

### 5.4 按子叶读取正确性证明

通过 Python 模拟 `aio_hcsd_read_leaf` 的定位逻辑（读取 leaf_index[leaf_ipix] → 计算 ipix/pixel 偏移 → 读取数据段），与全量读取逐子叶比较：
- T1_baseline：78 非空子叶 + 1 空子叶 = 79 项检查，全部 ipix/pixel 完全一致。
- T6_run1：5 非空子叶 + 1 空子叶 = 6 项检查，全部一致。
- 证明 leaf_index 的 data_offset（字节偏移）和 data_length（像素数）定位计算正确。

### 5.5 HCSD 字节级可重现

- P06-002 T1_baseline.hcsd 的 SHA-256 与 P00-003 stage2_baseline.hcsd 完全一致（`2A9BD12E...4122C37`）。
- 文件大小一致（187455430 bytes）。
- 证明 HCSD 输出在相同输入 + 相同配置下字节级可重现（contract §9.10）。

## 6. 兼容性与残留风险

- **未修改业务源码**：本任务为只读验证，未修改任何 .cpp/.h 文件。
- **DLL 加载路径注意**：`build/artifacts/orchestrator.exe` 的 DLL 自动推导路径错误（向上 4 级得到 `F:\Astro dev` 而非项目根目录），需使用 `lib/orchestrator/cpp/orchestrator.exe`（P06-002 已确认）。此为 orchestrator 的已知行为，非本任务引入。
- **inspect --hcsd 输出不含 leaf_index 详细统计**：inspect 仅输出 leaf_index 的固定大小（n_leaves=49152, leaf_index_bytes=1179648），不输出非空子叶数等统计。本任务通过 Python 脚本补充验证。
- **已知缺口不阻塞 PASS**：所有缺口均为 contract §9 记录的 v1.1+ 待修复项，与本任务验收无关。

## 7. 验收清单

- [x] 依赖任务 P06-002、P04-003 均已 DONE
- [x] 7 项验证均有可复现证据（日志 + JSON + 脚本）
- [x] 独立复核 VERDICT: PASS（见 REVIEW_REPORT.md）
- [x] 任务注册表、当前任务、项目状态已更新
- [x] 未修改业务源码

## 8. 交付物

- `hcsd_validation_results.json`：结构化验证结果
- `TASK_REPORT.md`：本文件
- `TEST_REPORT.md`：测试结果报告
- `EVIDENCE_INDEX.md`：证据索引
- `REVIEW_REPORT.md`：独立复核报告
- `parse_hcsd_binary.py`：HCSD 字节级解析脚本
- `verify_read_leaf.py`：按子叶读取验证脚本
- `gen_reports.py`：报告生成脚本
- `logs/inspect_hcsd.json`：T1_baseline inspect 输出
- `logs/inspect_hcsd_T6.json`：T6_run1 inspect 输出
- `logs/hcsd_binary_parse_T1.json`：T1 字节级解析结果
- `logs/hcsd_binary_parse_T6.json`：T6 字节级解析结果
- `logs/hcsd_binary_parse_summary.json`：字节级解析汇总
- `logs/verify_read_leaf_T1.json`：T1 按子叶读取验证结果
- `logs/verify_read_leaf_T6.json`：T6 按子叶读取验证结果
