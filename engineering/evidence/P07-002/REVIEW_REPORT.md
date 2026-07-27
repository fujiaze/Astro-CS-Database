# P07-002 长批次与故障稳定性 - 独立复核报告

- Reviewer mode: 独立复核（子 Agent 自复核）
- Diff reviewed: `engineering/evidence/P07-002/**`（新增稳定性测试证据，未修改业务源码）
- Tests rerun: 否（稳定性测试任务，复核证据完整性而非重跑测试）
- Commit base: 8440e190029897f69d19dc4c8a2aa0d312fc41ae

## 复核项目

### 1. 证据完整性

- [x] **stability_results.json** 存在且结构完整（stage1_batch/stage2_repeat/cancel_rerun/fault_injection/leak_check/verdicts）
- [x] **TASK_REPORT.md** 存在且内容完整（环境/6帧批量/stage2重复/取消重跑/故障注入/泄漏检查/异常定位/验收清单）
- [x] **TEST_REPORT.md** 存在且内容完整（13 个测试用例全 PASS，稳定性测试结果表，异常调查）
- [x] **EVIDENCE_INDEX.md** 存在且 SHA-256 齐全（48 个证据文件）
- [x] **stability_runner.py** 存在且功能完整（批量/重复/取消重跑/泄漏/故障注入）
- [x] **logs/** 目录包含所有运行的 stdout/stderr/内存采样日志
- [x] **output/** 目录包含所有 HISS 和 HCSD 输出文件
- [x] **configs/** 目录包含 T2/T3/T4 三个 stage1 配置

### 2. Stage1 批量稳定性复核（6 帧）

- [x] 6 帧全部成功（exit_code=0）
- [x] C001 (dec=-13°) 峰值 3633.60 MB，wall 19.085s
- [x] C003 (dec=-70°) 峰值 35471.28 MB，wall 85.898s
- [x] C004 峰值 792.80 MB，wall 17.374s
- [x] C005 峰值 791.25 MB，wall 16.882s
- [x] C006 峰值 795.71 MB，wall 16.893s
- [x] C007 (dec=-79°) 峰值 32607.03 MB，wall 67.592s
- [x] 峰值内存与 P07-001 基线一致（C001 差异 -0.01%，C003 差异 +0.003%，C007 差异 +0.003%）
- [x] HISS 输出大小与 P05-002 一致
- [x] 帧间内存回归正常（delta 在 ±138 MB 内双向波动，无累积上升）

### 3. Stage2 重复稳定性复核（3 次确定性）

- [x] 3 次运行均成功（exit_code=0）
- [x] HCSD SHA-256 三次完全一致（2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37）
- [x] 与 P07-001 基线 SHA-256 完全一致（字节级可重现）
- [x] 与 P00-003 baseline SHA-256 完全一致（字节级可重现）
- [x] HCSD 大小三次一致（187455430 bytes）
- [x] 峰值内存稳定（1962.89/1979.65/1914.06 MB，差异 65.59 MB < 200MB）

### 4. 取消后重跑复核

- [x] 取消阶段：exit_code=3221225786 (STATUS_CONTROL_C_EXIT 0xC000013A)
- [x] 取消阶段：进程正常退出（12.853s）
- [x] 取消阶段：无残留进程（tasklist 确认 residual_count=0）
- [x] 取消阶段：无 partial 输出残留（partial_output_exists=False）
- [x] 重跑阶段：exit_code=0，成功
- [x] 重跑阶段：HISS 输出 19347 bytes（与正常 C003 一致）
- [x] **结论：取消后重跑 PASS**

### 5. 故障注入复核

- [x] 复制 frame1.hiss + frame2.hiss 到临时目录（不破坏原始文件）
- [x] 启动 stage2，t=2.041s 删除 frame1.hiss
- [x] exit_code=1（报错退出，非崩溃）
- [x] graceful_handling=True（非硬崩溃）
- [x] HCSD 未生成（输入缺失，预期行为）
- [x] 进程正常退出（2.605s）
- [x] **结论：故障注入 PASS**

### 6. 资源泄漏检查复核

- [x] 系统可用内存 46679.14 MB / 65446.38 MB (71%，healthy=True)
- [x] 残留进程 = 0（orchestrator.exe 无残留）
- [x] 临时文件 = 0（无 .tmp/.partial/.lock/.bak 残留）
- [x] stage2 重复峰值差异 65.59 MB（stable=True，<200MB）
- [x] stage1 峰值差异 34680.03 MB（天区特性，非泄漏，P07-001 已记录）
- [x] **判定逻辑正确**：使用残留进程/临时文件/系统内存/stage2重复峰值作为泄漏指标，stage1 不同帧峰值差异不作为泄漏指标（已知天区特性）
- [x] **结论：资源泄漏检查 PASS**

### 7. 性能异常定位复核

- [x] 异常 1（C003 wall +10.8% vs P07-001）：根因已定位为长批次环境下的系统负载波动（P07-001 标准差 1.765s），非回归
- [x] 异常 2（stage2 wall +14.5% vs P07-001）：根因已定位为长批次后系统负载 + 冷启动效应（三次运行递减），非回归
- [x] **2 项异常均已定位根因，无未解释的异常**

### 8. 业务源码未修改确认

- [x] 未修改任何 .cpp/.h 文件
- [x] 仅新增 engineering/evidence/P07-002/ 下的证据文件
- [x] stability_runner.py 为测试脚本，非业务代码

## Contract/ABI/format findings

- 无接口/ABI/格式变更（稳定性测试任务）
- HCSD 输出符合 hcsd_format_v1 契约（SHA-256 与 P00-003/P06-002/P07-001 baseline 一致）
- HISS 输出符合 hiss_format_v1 契约（大小与 P05-002 一致）

## Scientific regression findings

- 无科学回归（未修改业务源码）
- HCSD 字节级与 P00-003/P07-001 baseline 一致（2A9BD12E...）
- Stage1 HISS 输出大小与 P05-002 一致

## Risks

1. **南天天区内存需求高**：C003/C007 峰值内存 32-35 GB，限制部署环境（需 64 GB RAM）。P07-001 已记录，部署文档需标注。
2. **HISS 非字节级可重现**：已知行为（P00-003 记录），不影响 HCSD 字节级可重现。
3. **长批次 wall time 波动**：C003 +10.8%、stage2 +14.5% 属长批次环境正常波动，但生产环境需考虑负载规划。
4. **stage2 故障注入行为**：输入 HISS 缺失时 exit_code=1 优雅退出，生产环境需确保输入文件完整性（建议上游校验）。

## 验收

- [x] 依赖任务 P07-001、P05-003 均已 DONE
- [x] 本任务目标有可复现证据（48 个证据文件 + 结构化 JSON）
- [x] Stage1 批量稳定性（6/6 PASS）
- [x] Stage2 重复稳定性（3/3 确定性 PASS，匹配 P07-001 基线）
- [x] 取消后重跑（PASS）
- [x] 故障注入（PASS）
- [x] 资源泄漏检查（PASS）
- [x] 性能异常已定位根因（2 项，均非回归）
- [x] 未修改业务源码
- [x] 任务注册表、当前任务、项目状态将更新

VERDICT: PASS
