# P06-003 HCSD 输出与独立读取 - 证据索引

- 任务编号：P06-003
- 执行日期：2026-07-27
- 证据目录：`engineering/evidence/P06-003/`
- 执行人：子 Agent（GLM-5.2）
- 提交基线：4ccb507（P06-002 球面梯度与稳健叠加证据 VERDICT: PASS）
- 入口条件：P06-002 DONE（7/7 PASS）；P04-003 DONE（inspect --hiss/--hcsd/--frame 测试）

---

## 1. 顶层文件

| 文件 | 描述 |
|---|---|
| `TASK_REPORT.md` | 任务执行报告（7 项验证 + 结论 + 残留风险） |
| `TEST_REPORT.md` | 测试结果报告（17 项测试，全部 PASS） |
| `EVIDENCE_INDEX.md` | 本文件，证据索引 |
| `REVIEW_REPORT.md` | 独立复核报告（VERDICT: PASS） |
| `hcsd_validation_results.json` | 结构化验证结果 JSON（7 项 + 已知缺口） |

## 2. 验证脚本

| 文件 | 描述 |
|---|---|
| `parse_hcsd_binary.py` | HCSD 字节级解析脚本（Magic/JSON头/leaf_index/sorted_ipix） |
| `verify_read_leaf.py` | 按子叶读取验证脚本（模拟 aio_hcsd_read_leaf，与全量读取比较） |
| `run_binary_parse.py` | 批量运行字节级解析的 runner |
| `gen_reports.py` | hcsd_validation_results.json 生成脚本 |

## 3. 日志文件（logs/）

| 文件 | 描述 |
|---|---|
| `logs/inspect_hcsd.json` | T1_baseline.hcsd inspect --hcsd 输出（JSONL） |
| `logs/inspect_hcsd.err.log` | T1_baseline inspect stderr |
| `logs/inspect_hcsd_T6.json` | T6_run1.hcsd inspect --hcsd 输出（JSONL） |
| `logs/inspect_hcsd_T6.err.log` | T6_run1 inspect stderr |
| `logs/hcsd_binary_parse_T1.json` | T1_baseline 字节级解析结果 |
| `logs/hcsd_binary_parse_T6.json` | T6_run1 字节级解析结果 |
| `logs/hcsd_binary_parse_summary.json` | 字节级解析汇总 |
| `logs/verify_read_leaf_T1.json` | T1 按子叶读取验证结果（79/79 PASS） |
| `logs/verify_read_leaf_T6.json` | T6 按子叶读取验证结果（6/6 PASS） |

## 4. 测试输入（引用 P06-002 证据）

| 文件 | 大小 | SHA-256 | 来源 |
|---|---|---|---|
| `engineering/evidence/P06-002/T1_baseline/output/T1_baseline.hcsd` | 187455430 | `2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37` | P06-002 T1（= P00-003 baseline） |
| `engineering/evidence/P06-002/T6_determinism/output/T6_run1.hcsd` | 1198683 | - | P06-002 T6（梯度校正启用） |

## 5. 验证结果汇总

| 序号 | 验证项 | 结果 | 关键证据 |
|---|---|---|---|
| 1 | 子叶索引 leaf_index | **PASS** | 78/49152 非空（T1）, 5/49152（T6）, leaf_ipix 一致, sum=n_pix |
| 2 | metadata 必填字段 | **PASS** | nside/nested/n_pix/has_snr + caller 元数据齐全 |
| 3 | 输入追溯 | **PASS** | n_frames=2=HISS 数, n_pix=15522966=stage2 日志 |
| 4 | inspect --hcsd 独立读取 | **PASS** | DLL 全加载, JSONL result+completed |
| 5 | 字节级结构 | **PASS** | Magic/JSON头/leaf_index/sorted_ipix 升序/文件大小 |
| 6 | 按子叶读取 aio_hcsd_read_leaf | **PASS** | T1: 79/79, T6: 6/6 逐子叶一致 |
| 7 | 字节级可重现 | **PASS** | T1 SHA-256 = P00-003 baseline SHA-256 |

**总计：7/7 PASS，VERDICT: PASS**

## 6. 引用合约

- `engineering/contracts/hcsd_format_v1.md`（FROZEN v1.0）
  - §2 文件布局
  - §4 JSON 头必填字段
  - §5 子叶块索引表
  - §6 数据数组与按需读取
  - §9 已知缺口
  - §11 Round-trip 不变量

## 7. 引用上游任务

- P06-002：球面梯度与稳健叠加证据（T1_baseline + T6_determinism HCSD 来源）
- P04-003：capabilities 与 inspect 命令（inspect --hcsd 实现 + 317 集成测试）
- P01-003：HISS/HCSD 格式版本与 round-trip（HCSD 格式契约定义）
- P00-003：旧 CLI 真实数据基线（stage2_baseline.hcsd 基线 SHA-256）
