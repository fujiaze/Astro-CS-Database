# BASE-UTIL-001｜P2 utilization_p75_low 独立归因诊断（负责人裁决 2，选择 A）

## 背景（owner 裁决 2026-09-09）
全量 pytest 预存失败集含 p2001/p2002/p2006 的 `utilization_p75_low`。资源利用率已是冻结宪章门禁的一部分，**不得仅凭"属于 BASE 预存失败"进入审核包豁免**。本任务只归因，不修实现。

## 目标
用 BASE 主构建二进制（当前 main HEAD 干净构建产物）复现 p2001/p2002/p2006，**不得使用在途修改生成的二进制混淆归属**。

## 诊断记录要求（逐项）
1. 有效 CPU / affinity / cgroup 配额；
2. workload 与计算区间时长；
3. granted / active workers；
4. CPU 平均值、P75、连续低利用率窗口；
5. queue depth、progress、I/O wait、内存带宽；
6. 1 worker 与 N worker 时间和加速比；
7. 二进制、配置、输入 hash（绑定结论到 SHA）。

## 分类（按冻结宪章 §10.5 与裁决 1 CPU 门禁：有效 CPU≥2 且计算区间>10s 时，平均利用率≥85%；任何连续 10s<60% 或单活跃线程即失败）
- **计算区间不足 10 秒** → 判定「门禁不适用」（NOT_APPLICABLE），不是"豁免"；
- **环境或监控计算错误** → 归类 ENV/MONITOR 错误，登记修复建议（修复本身不在本任务域）；
- **BASE 真实性能缺陷** → 建立独立整改任务建议（不在本任务修）；
- **当前提交新增回归** → 指认对应 commit/任务，退回修复。

## 依赖与边界
- 依赖 `P1-001`（用已提交 main 的干净构建）；read-only lane，零代码写入（构建产物落 `run/`）。
- **不得顺手修改 P2 科学实现或任何生产/测试代码。**
- 不阻断无依赖的合同与只读工作，但必须在 `RT-001`、最终资源门与真实数据终验前闭环（RT-001 已挂本节点依赖）。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。

## 验收
- p2001/p2002/p2006 各自归因结论明确（四类之一），证据链完整（记录项 1-7 全覆盖）；
- 结论绑定 BASE 构建 SHA 与二进制 hash；
- 前台复验复现后才可 PASS；本任务若产生登记面修改（memory/finding 登记），单独原子 commit。

## 返回证据
schema 要求 scope/acceptance/provenance + artifacts（监控采样、复现日志、hash 清单、分类结论表）；不得伪造 PASS。
