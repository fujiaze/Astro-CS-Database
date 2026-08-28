# V4 工作流与门禁

## 检查点链（C0–C9，连续执行，不得跳过）

| CP | 名称 | 进入条件 | 退出判据（04 详表） |
|---|---|---|---|
| C0 | 控制与起点 | 控制包 validate PASS | 起点冻结+工具链盘点+继承债务登记 |
| C1 | Linux 静态分析 | C0 完成 | 机器检查器 0 FAIL + 硬编码清单 + race 债务清单 |
| C2 | SCI 科学定义 | C1 快照 | Phase3 SCI 全定义 + Phase1/2 复核冻结 |
| C3 | ALG 算法推导 | C2 快照 | Phase3 几何/映射/采样推导 + backend/线程/benchmark/门禁算法 |
| C4 | ARCH/API 合同 | C3 快照 | CLI/进程内/backend/profile/监控/门禁/Phase3 合同 |
| C5 | CODE 实现 | C4 快照 | 全部实现 + race 修复 + 硬编码清零 |
| C6 | 测试与 Oracle | C5 快照 | Oracle 对拍 + 全部测试 PASS + TSan 干净 |
| C7 | Linux 构建 alpha | C6 快照 | build 0 错 0 警 + alpha 包 + capsule |
| C8 | Windows 正式验证 | C7 包 + Fatduck 在线 | Windows 构建/测试/benchmark/合成/2R + 门禁判定 |
| C9 | 32R 与发布候选 | C8 包 | 当前候选唯一一次 32R + 门禁判定 + 终态冻结 + AWAITING_EXTERNAL_RELEASE_REVIEW |

## 连续执行纪律

- Fatduck 离线时不得整体停工：继续 Linux 侧任务（C1–C7 全部可做），Windows 任务挂起计时等待。
- Fatduck 在线窗口（北京时间 07:00–23:30，最早 06:30）内执行 C8 与 C9。
- 每个 Task：实现/验证 → 原子 commit → 立即 push → review capsule 刷新 → 账本置 PASS。
- 禁止在未通过前置检查点时启动后续重计算（规则 10）。

## 资源门禁（强制，判定权在机器指标）

一切生产重计算（stage1/stage2/Phase3/32R）必须：
1. 全程资源监控（CPU%/内存/I/O/线程/阶段时间序列，30s 间隔或更细）；
2. 门禁判定全部通过：均值 CPU 利用率 ≥ 阈值（见 05）、内存无异常增长、无 race 证据、报告完整；
3. 串行段 >1s 的重计算阶段即 FAIL（规则 9）；
4. 监控缺失/报告缺失 → 该运行不得 PASS。

## 历史代码使用

历史锚仅 `git archive` 导出仓库外只读构建（对照用途）；禁止在历史锚提交/建分支；
禁止"反复运行历史版本"——历史运行以 V3 审核包为证据，不重跑。
