# 任务列表与依赖图

基线提交：`a861d8f63a1f6c17dea2f201349006f6a6bd1ad2`　任务数：30　所有任务初始状态：`NOT_STARTED`
状态列由调度员按 `OPERATOR.md §3` 维护；`PASS` 只由前台独立复跑后写入。

## 1. 任务总览

| 层 | ID | 目标 | 依赖 | 并行组 | 状态 |
|---|---|---|---|---|---|
| L0 | BASE-001 | 冻结可复现基线与预存改动边界 | — | S0 | NOT_STARTED |
| L0 | ROOT-001 | 仓库根清洁：全条目账本、处置分类与执行 | BASE-001 | S1-R | NOT_STARTED |
| L0 | ROOT-002 | 根目录整洁的长效规则与机器门 | ROOT-001 | S2-R | NOT_STARTED |
| L0 | ROOT-003 | run/ 临时产物保留策略与清运 | ROOT-001 | S3-R | NOT_STARTED |
| L0 | ROOT-004 | 旧 bug 清单按最新权威订正 + 根目录存疑文档处置 | ROOT-001 | S4-R | NOT_STARTED |
| L0 | GOV-001 | 收敛最高权威、根目录与废止治理入口 | BASE-001 | S1-A | NOT_STARTED |
| L0 | DOC-001 | 收敛活动文档的术语、状态与引用 | BASE-001 | S1-B | NOT_STARTED |
| L1 | DATA-001 | 建立统一数据对象合同链（14 个对象） | GOV-001, DOC-001 | S2-A | NOT_STARTED |
| L1 | CFG-001 | 建立 defaults/filters 与三命令配置合同 | GOV-001, DATA-001 | S3-A | NOT_STARTED |
| L1 | MOD-001 | 建立 23 插件到模块/target/清单映射门 | GOV-001, DATA-001 | S3-B | NOT_STARTED |
| L2 | ARCH-001 | 建立目标源码根与等价迁移骨架 | MOD-001 | S4-A | NOT_STARTED |
| L2 | AIO-001 | 收敛唯一 AIO 与原子产品边界 | DATA-001, ARCH-001 | S5-A | NOT_STARTED |
| L2 | RT-001 | 收敛统一调度器、typed DAG 与线程预算 | DATA-001, ARCH-001 | S5-B | NOT_STARTED |
| L3 | CLI-001 | 切换唯一用户命令树 | CFG-001, AIO-001, RT-001 | S6-A | NOT_STARTED |
| L3 | CLI-002 | 实现模板、预检、确认与 force 语义 | CFG-001, CLI-001 | S7-A | NOT_STARTED |
| L3 | CLI-003 | 收敛机器输出、退出码与取消协议 | CLI-001, AIO-001, RT-001 | S7-B | NOT_STARTED |
| L4 | P1-001 | 治理 normalize 模块与固定 DAG | DATA-001, AIO-001, RT-001 | S6-P1 | NOT_STARTED |
| L4 | P1-002 | 治理 frame_snr、稀疏层与 normalize 输出 | P1-001, CFG-001 | S7-P1 | NOT_STARTED |
| L4 | P2-001 | 治理 mosaic 固定 DAG 与产品合同 | DATA-001, AIO-001, RT-001 | S6-P2 | NOT_STARTED |
| L4 | P2-002 | 治理按需逆方差与三科学目标权重 | P2-001 | S7-P2 | NOT_STARTED |
| L4 | P3-001 | 治理八投影 registry 与独立 Oracle | DATA-001, RT-001 | S6-P3 | NOT_STARTED |
| L4 | P3-002 | 治理反向重采样、模式与流式 FITS 输出 | P3-001, AIO-001 | S7-P3 | NOT_STARTED |
| L5 | CPU-001 | 治理 CPU provider、benchmark 与 profile | RT-001 | S6-C | NOT_STARTED |
| L5 | OBS-001 | 治理观测事件、资源门与运行图 | RT-001 | S6-D | NOT_STARTED |
| L6 | INT-001 | 汇总目标构建图、注册表与产品装配 | CLI-002, CLI-003, P1-002, P2-002, P3-002, CPU-001, OBS-001 | S8 | NOT_STARTED |
| L6 | CI-001 | 重建新规范机器检查入口与治理门 | GOV-001, DOC-001, MOD-001 | S5-C | NOT_STARTED |
| L7 | QA-001 | 接通双平台 CI 与完整测试矩阵 | INT-001, CI-001 | S9 | NOT_STARTED |
| L7 | PKG-001 | 治理双平台安装树、清单与候选包 | INT-001, CI-001 | S9 | NOT_STARTED |
| L8 | REAL-001 | 执行最终 SHA 跨平台真实数据与图像验收 | QA-001, PKG-001 | S10 | NOT_STARTED |
| L9 | FINAL-001 | 独立总审计与差距闭合报告 | REAL-001 | S11 | NOT_STARTED |

## 2. 依赖图

```text
BASE-001
  ├── ROOT-001 ─┬── ROOT-002 （根目录长效机器门）
  │             └── ROOT-003 （run/ 保留策略与清运）
  ├── GOV-001 ─┐
  └── DOC-001 ─┴── DATA-001 ─┬── CFG-001 ──┐
                             ├── MOD-001 ──┼── ARCH-001 ─┬── AIO-001 ─┐
                             └── CI-001 ───┘             └── RT-001 ──┤
                                                                       ├── CLI-001 ─┬── CLI-002 ─┐
                                                                       │            └── CLI-003 ─┤
                                                                       ├── P1-001 ── P1-002 ──────┤
                                                                       ├── P2-001 ── P2-002 ──────┼── INT-001 ─┬── QA-001 ─┐
                                                                       ├── P3-001 ── P3-002 ──────┤            └── PKG-001 ─┴── REAL-001 ── FINAL-001
                                                                       ├── CPU-001 ───────────────┤
                                                                       └── OBS-001 ───────────────┘
```

> 注：CI-001 依赖 GOV-001/DOC-001/MOD-001（需要三者先把旧检查器与映射表处理完），因此它在 INT-001 之前完成，但两者的文件域在 `ci/**` 上有重叠，调度员必须串行安排。

## 3. 并行与串行规则

- **可并行**：文件域互斥且依赖已 PASS 的任务，例如 S6-P1 / S6-P2 / S6-P3 / S6-C / S6-D 五组；
- **必须串行**：`CMakeLists.txt`、中央 module registry、`ci/checks.json`、`packaging/astrocs.product.json` 属于中央集成面，只能由 INT-001 / CI-001 / QA-001 / PKG-001 串行修改；
- **禁止并行**：ARCH-001 目录迁移期间不得有任何其它任务写工作区；REAL-001 锁 SHA 期间禁止任何提交。

## 4. 提交与台账规则

- 一个任务 = 一个可独立验证的 commit，只有前台（调度员）验收后提交；
- 提交前核对 `git diff --stat` 与 BASE-001 的预存清单，禁止把预存改动混入；
- 执行与验收日志写入 `run/PROJECT-GOVERNANCE-01/<TASK>/logs/`，不入库；
- 每任务 PASS 后在 `ACCEPTANCE.md` 登记：状态 / 机器门结果 / 证据路径 / 前台结论。

## 5. 派发入口

- 提示词：`DISPATCH.md`（通用提示词 + 复核提示词 + BLOCKED 处理）
- 调度流程：`OPERATOR.md`（派发 → 独立复跑 → 边界核对 → 原子提交 → 台账）
