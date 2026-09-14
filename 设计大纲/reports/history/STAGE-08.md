# STAGE-08 宪章定纲、真实数据与 CI 转绿、RQS 全量普查收口

## 时间窗与提交数
- seq 1786–1988（共 203 条；merge 0，L 级 25 条）。日期 2026-09-10 至 2026-09-14（拓扑尾段；1844 自述 BASE=1827 而实际 git 父=1843、8 对同秒时间戳——共享工作树并行创作、串行落库，H-S34）。
- 对应分片：H-S33（尾）、H-S34、H-S35、H-S36、H-S37。

## 该阶段要解决的问题（引自提交消息，标注自述）
1. 以冻结宪章替代历史约束文件并逐面落地：seq 1786 | d8c821db ASTROCS_PROJECT_CONSTITUTION.md 首入库（+674，DRAFT_FOR_OWNER_REVIEW→FROZEN，§18 四项负责人裁决）；seq 1787/1788 V8.1 tracked 镜像 56 文件 RENAME50 归档 + 工程控制/ACTIVITY_STATE.md 首现（唯一活动源）；seq 1796/1797 AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909 控制包整体入库（TASK_LEDGER.csv 首现、REAL-000/001）。
2. 宪章逐面落地链（1793–1813）：AIO-001→AIO-002→P1-001→P2-001/002→WCS-003→P3-001/002→RT-001→MOD-001→CLI-001→CI-001；lib/core/src/module_adapters.cpp 五度成为最大落点（+1144/+1505/+32/+779/+168，"唯一真实 operation 委托"三 Phase 各一次）；此后 12 条消息以"宪章 §x.y"为规格锚。
3. CI 转绿攻坚：CI-001/REAL-000 建设期（1816 起）→ 裁决集 R-01..R-14 首发（seq 1828 | e6b1ea87 rev6 重编）→ CI Batch 3 真启用、ctest 173→286 阶跃、Phase2 门首进 CI（seq 1872 | c21eec50）→ known_failures.json 删 UT-CLI 基线条目 + removals[] 留痕（seq 1944 | ccc7a933，自述"Linux CI 取得 conclusion=success 的最后一步"）。
4. 全项目只读审计（RQS）：seq 1859 | 43c5c488 问题扫描/工作区建立 → 叶子入档波（1861–1884）→ 定稿+前台自纠波（1885 撤回 F00-07a）→ 十域合并完成 333 条/P0 68（seq 1896）→ 27 轴+14 域全收工 INDEX v3（seq 1922 | f4f88d54）→ FIX_LEDGER 机器化账本开张（seq 1945 | 1d2f0736，521 条/P0 82、判定列只读/处置列可写）→ B/V/P 三波修复-复核闭环。

## 具体工作内容（按模块）
- 科学算法：owner 裁决回退 WIN-* 资源门旗标（seq 1813）；P1..P11 批次性能与科学线（seq 1955 起 bd4bc23b 换段，perf/science/fix 前缀，间隔 20–60 分钟大 diff）；WCS header_pointing 尾修（seq 1981 | 910d671f——全史最后一个代码面提交）。
- 管线与 CLI：tests/realdata + tools/realdata 首现（seq 1815 | NEWDIR×2，重心转真实数据审计）；lib/snr_estimator 断链摘除 + TASK-RESULT-SCHEMA 归档排除（seq 1814）。
- 构建与 CI：CI fail-closed 收紧（seq 1812）；B2 批修复 + AGENT_PLAN 扩轴（1864/1867，第二波横向轴 L19–L28）；ci/known_failures.json 引入 C1822 → 末改 C1944；ci/checks.json 末改 C1956（6d74046d）。
- 控制包与治理：RESCUE-V3 登记唯一 ACTIVE、V1 对齐包转 ARCHIVED_SUPERSEDED（seq 1850 | 5a250999，治理/CI 期与 RESCUE 修复期分界）；40_OWNER_DECISIONS.md 新建（seq 1895 | e84b10a3，负责人裁决容器从无到有，其后 A/B/C/D 编号族挂靠）；astrocs-frontdesk 代提交线首现（seq 1826/1827）→ 双前台事故登记（seq 1847 附录 D）→ 交接说明（1849）；问题扫描/ 新顶级目录未见 AGENTS.md 落位登记旁证（H-S34 存疑）。
- 文档与报告：FRONT_ACTION_LIST.md"隔壁工单"+ 建议条款 C-13..C-17（seq 1985 | 521095b8，审计输出转可执行工单）；轴骨架 V7..V15 与 _cache 机械普查产物两笔万行级入库（seq 1986/1987，+10778/+16578）；S35 段 RQS 卷宗 45/55 条为审计面。
- 测试：UT-BACKEND 回归修复（seq 1923/1924）；ANCHOR_VERIFY_REPORT 三时点数字互异（1926/1942/1946，"漂移不判错"登记）。

## 交付与验证留痕
- 宪章 §18 裁决入库（1786）；控制包 rev3（1797）；C1–C7 检查点/CHECKPOINTS 链延续；FIX_LEDGER 554→555 行（seq 1988 抽取器 V\d→V\d+ 修复后账本完备——此前对 V10+ 编号系统性缺行，引用旧账本须先排除抽取器缺陷，账本完备性硬边界）；RQS 定稿数 1893"51 条"vs 1895"43 条"口径互斥（H-S35 未证实）；全部量化自述延续（本阶段无任何独立运行产物可核，除 ci 配置与台账文本本身）。

## 结构与规范变化
- 目录：工程控制/ACTIVITY_STATE（1787）、问题扫描/（1859 起 _cache/_verify/审计卷宗三层）、tests/realdata + tools/realdata（1815）、contracts/schemas（1782）。
- 编号族：宪章 §x.y → AIO/P/WCS/RT/MOD/CLI/CI 落地号 + REAL-00x → R-01..14 裁决 → RESCUE/FD 系列 → RQS/L 轴/域 → FIX_LEDGER V{n}-N-xx → P1..P11 批次。
- 自述锚法换代："BASE=<父 sha8>/结论绑定 BASE=" 自 seq 1795 起成常态（1795–1815 共 15 条）；证据 run 会话号锚（1786 Rmtucy2cqced995→1803 Rmtvuwx3i0c5cb8）。

## 与下一阶段的衔接
- 本阶段即本史终点（seq 1988 | 5617a512，2026-09-14 23:23）。未收口项（转交控制包取证线/后续历史）：FOLLOW-UP-FD05-03、UT-BACKEND 的 FIX-BACKEND 整改、40 文件待裁 B/C/D 项、L24 各 A-G 执行清单、p3003 掩码函数 DOC-CLOSE（H-S35）；RESCUE-FD-01/FD-02、F-SCI-F3-001-01/-02 残差（H-S34）；D-02 "TRACE NF 13-16 fixed next round"（H-S33）；B2-A11 编号缺席（H-S34/S35）。

## 证据缺口
- 1863/1866/1867、1880/1892/1915/1916/1919 等无消息正文，仅凭首行+变更面；1984 自述"20 条"vs 变更面 6 行；1987 消息 P1 vs 1988 账本 P2 且 producer=V5；1988"已记"记录落点不可见；负责人"两裁定"（seq 1925）授权时点无原始记录；astrocs-agent 与 AstroCS Agent 两署名是否同一执行体未证实；问题扫描/_cache 是否计账本分母未证实（H-S36/S37）。
