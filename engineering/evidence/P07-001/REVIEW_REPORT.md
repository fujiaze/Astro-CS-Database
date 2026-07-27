# P07-001 性能与峰值内存基线 - 独立复核报告

- Reviewer mode: 独立复核（子 Agent 自复核）
- Diff reviewed: `engineering/evidence/P07-001/**`（新增性能基线证据，未修改业务源码）
- Tests rerun: 否（性能测量任务，复核证据完整性而非重跑测试）
- Commit base: 503109484933fc3d4b2459471159e1169843fdfa

## 复核项目

### 1. 证据完整性

- [x] **performance_baseline.json** 存在且结构完整（环境/stage1单帧/stage1多帧/stage2/内存泄漏/取消状态/性能异常定位/summary）
- [x] **TASK_REPORT.md** 存在且内容完整（环境/3次运行/多帧对比/stage2/内存泄漏/取消/异常定位/验收清单）
- [x] **TEST_REPORT.md** 存在且内容完整（9 个测试用例全 PASS，性能基线表，异常调查）
- [x] **EVIDENCE_INDEX.md** 存在且 SHA-256 齐全（38 个证据文件）
- [x] **logs/** 目录包含所有运行的 stdout/stderr/内存采样日志
- [x] **output/** 目录包含所有 HISS 和 HCSD 输出文件

### 2. Stage1 单帧基线复核（C003）

- [x] 三次运行均成功（exit_code=0）
- [x] wall time 中位数 77.805s（均值 78.715s，标准差 1.765s，变异系数 2.2%）
- [x] 峰值内存中位数 35470.23 MB（标准差 0.77 MB，变异系数 0.002%）
- [x] 各 stage timings 已记录（PLATESOLVE 16.67s + DRIZZLE 13.96s 为主耗时）
- [x] HISS SHA-256 已记录（三次不同，size 差 1 字节，与 P00-003 一致已知行为）

### 3. Stage1 多帧对比复核

- [x] 3 帧不同天区（C001/C003/C007）均运行成功
- [x] C001 (dec=-13°) 峰值 3.6 GB，C003 (dec=-70°) 峰值 35.5 GB，C007 (dec=-79°) 峰值 32.6 GB
- [x] PlateSolve RMS 全部 < 0.4"（C001=0.346", C003=0.124", C007=0.379"）
- [x] n_pairs 全部 > 30（C001=36, C003=47, C007=41）

### 4. Stage2 基线复核

- [x] 两次运行均成功（exit_code=0）
- [x] HCSD SHA-256 两次完全一致（2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37）
- [x] 与 P00-003 baseline SHA-256 完全一致（字节级可重现）
- [x] 峰值内存 1979 MB（与 P00-003 的 1978 MB 一致，差异 0.05%）
- [x] GRADIENT_SPHERE 5.48s（比 P00-003 的 6.63s 快 17%，在正常波动范围）

### 5. 内存泄漏检查复核

- [x] 连续 3 次 stage1 运行峰值内存稳定（35470.23/35469.87/35471.4 MB）
- [x] 峰值差异 1.53 MB（<2 MB，采样精度范围内）
- [x] 无逐次上升趋势
- [x] **结论：无内存泄漏**

### 6. 取消后状态复核

- [x] 进程已退出（exit_code=STATUS_CONTROL_C_EXIT 0xC000013A）
- [x] 无残留进程（tasklist 确认）
- [x] partial 输出已清理（无残留 HISS 文件）
- [x] 内存释放（进程退出后释放）
- [x] **结论：取消测试 PASS**

### 7. 性能异常定位复核

- [x] 异常 1（C001 内存 3.6 GB vs C003 35.5 GB）：根因已定位为 Gaia DR3 SP xpsd 南天分区文件大于赤道分区，非代码缺陷
- [x] 异常 2（C003 run1 wall time 偏高）：根因已定位为冷启动效应，非异常
- [x] 异常 3（HISS 非字节级可重现）：根因已定位为 zstd 压缩元数据/并行 drizzle 浮点非确定性，P00-003 已记录
- [x] **3 项异常均已定位根因，无未解释的异常**

### 8. 业务源码未修改确认

- [x] 未修改任何 .cpp/.h 文件
- [x] 仅新增 engineering/evidence/P07-001/ 下的证据文件
- [x] perf_runner.py 为测量脚本，非业务代码

## Contract/ABI/format findings

- 无接口/ABI/格式变更（性能测量任务）
- HCSD 输出符合 hcsd_format_v1 契约（SHA-256 与 P00-003/P06-002 baseline 一致）
- HISS 输出符合 hiss_format_v1 契约（inspect 验证已在 P05-002 完成）

## Scientific regression findings

- 无科学回归（未修改业务源码）
- PlateSolve RMS 与 P05-002 baseline 一致（C001=0.346" vs 0.346", C003=0.124" vs 0.124", C007=0.379" vs 0.379"）
- HCSD 字节级与 P00-003 baseline 一致

## Risks

1. **南天天区内存需求高**：C003/C007 峰值内存 32-35 GB，限制部署环境（需 64 GB RAM）。建议在部署文档标注。
2. **HISS 非字节级可重现**：已知行为（P00-003 记录），不影响 HCSD 字节级可重现。后续可考虑固定 zstd 压缩参数/时间戳。
3. **C001 性能差异大**：不同天区 stage1 耗时差异 4x（19s vs 78s），主要来自 Gaia xpsd mmap。长批次任务需考虑天区分布。

## 验收

- [x] 依赖任务 P05-002、P06-003 均已 DONE
- [x] 本任务目标有可复现证据（38 个证据文件 + 结构化 JSON）
- [x] 性能异常已定位根因（3 项，均非回归）
- [x] 未修改业务源码
- [x] 任务注册表、当前任务、项目状态将更新

VERDICT: PASS
