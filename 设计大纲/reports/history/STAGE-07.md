# STAGE-07 V8.1 CI 控制包、W1 模块合同冻结波与 CI 治理化

## 时间窗与提交数
- seq 1521–1785（共 265 条；merge 3 条，L 级 27 条）。日期 2026-09-05 至 2026-09-09。
- 对应分片：H-S28、H-S29、H-S30、H-S31、H-S32（前半）。全段单亲线性链为主；1619 谱系分叉链 A/链 B 经 1624/1625 双 merge 收拢、1634/1635 为 git stash 快照对旁支（H-S30）。

## 该阶段要解决的问题（引自提交消息/台账，标注自述）
1. V8.1 CI 控制包的建立与 waves 推进：seq 1521 V81-ADOPT-001 整包注册（evidence/v8_1_ci_control 首现，触及 189 次）→ V8-CI-001..012 以"分发租约（WRITE_LEASE.json+任务 JSON）→实现"成对推进（1529–1562）→ seq 1562 TASK_STATE 一次性补 010/011/012 CLOSED（tasks 204→207、CLOSED 57→60）收口点 → 关闭后补丁波 + CIQA 独立攻击审查（1568–1571 连关两个 P1-GAP）。
2. 双平台 CI 收敛：hosted 五轮修复（V8-CI-010）→ deep/coverage 深水区（012）→ GitHub Actions 采用波（ADOPT 族，1541 起）→ R9→R19 轮次带外部 run 号单调（34178712916…34204130361，可作外部时钟锚，H-S31）。
3. 模块合同冻结 W1 批次：控制包 k/140 显式计数，1/140→18/140 每"docs 冻结 + CLOSED 同步"成对推进（seq 1600–1627 十连、1652–1704 八连），CLOSED 聚合 61→81（1692 一次性 +13 改判）（H-S30/S31）。
4. 台账对账机制稳态化：COMMIT_LEDGER.jsonl"追赶循环"（1604–1643 多处缺登补齐）→ seq 1646 固化守则"每次 commit 前 append 当前 HEAD sha"→ 1649/1650 收敛 missing=={HEAD} 豁免态 → seq 1668 | b05288b8 治理规则换轨（reconcile_state.py 改三条非递归不变量：sha 存在性/四字段 schema/subject 与 git log 一致；此后"单行补登"归零）（H-S30/S31 片内最强断点）。

## 具体工作内容（按模块）
- 科学算法：P2/P3 五域冻结合同链收口（1727 前，控制包 17→21/140、DATA §24–§29 每域+1，H-S32）；bug 狩猎 R3→R13（p1-batchN 五连同修止于 1712；批次字母与 seq 序不一致，属发现轮次非落地顺序，H-S31）；重计算治理实施波 CPU-006/007/008 + MON-001/002（1728 起，evidence/v6_1_rework 成主要落点）；batchR 全局分配审计（1747，173 处 grep、17 处修、在飞域只审不修）。
- 管线与 CLI：模块独立化 IMPL 波（L 级三连：gaia 1748、drizzle 1756、calibration 1758；TEST→IMPL→INT 三段链，gaia/calibration 片内闭环、drizzle 缺 INT，H-S32）；lib/phase3_proj 骨架首现 seq 1722（后被 S33 追认为 1806 三枚文件的真实引入点）；p3 session 相关线延续。
- 构建与 CI：fatduck-admin.yml 运维通道新建（seq 1702 | 21ce603a，1703 四分钟内 shell pwsh→cmd）；Windows 产品代码/ctest 修复波 + MON/QA/CPU/LNX 复验波（seq 1587–1595）；"（subagent 交付）"后缀自 seq 1576 | 3a4b265b 起覆盖 1576..1595 全部 20 条（此前 35 条为 0）——交付组织方式突变边界（H-S29）。
- 控制包与治理：控制包 k/140 节拍 + CLOSED 同步对（每 5–30 秒紧邻成对，与 ≥25 分钟 CI 等待断层并存，H-S31/S32）；V8.1 台账记账线在 seq 1780 终结（chore(state)/COMMIT_LEDGER 类提交自 1781 绝迹，H-S33）；根目录整洁基线三连（seq 1775/1782/1785：.gitignore 收紧、AGENTS.md 目录规范强制化、368 项数据/二进制出库、contracts/schemas 与 packaging/launch 归位）。
- 文档与报告：memory.md 会话交接指针族（seq 1768 第 7/8 条：cp_run 串行单飞、completion≠PASS 需五门机器复跑、SubAgent 零 git——控制包执行模式沉淀）；BASS DR3 用户数据区全目录 gitignore（seq 1785）。
- 测试：固定种子与契约 pytest 延续；量化验收 30+ 条仅消息自述、仓库面无产物（H-S31 通则）。

## 交付与验证留痕
- evidence/v8_1_ci_control 台账面（引入 C1521，末改 C1780，触及 189——本阶段最重的证据目录）；CIQA 入档（seq 1569，evidence/ciqa 首现 C1569）；GitHub run 号外部锚（9 个，1679–1700）；V8.1 控制包 tracked 镜像 56 文件（至 seq 1787 归档时清点）。

## 结构与规范变化
- 目录：evidence/v8_1_ci_control（1521）、ci/checks.json（1527 首现）、ci/tests（1528）、evidence/ciqa（1569）、lib/phase3_proj（1722）、contracts/schemas + packaging/launch（1781/1782 搬家）。
- 编号族：V81-ADOPT/V8-CI-0xx → W1 模块冻结（PSF/SESSION/STAR/PHOT/WCS/…k/140）→ CFG/R 轮/batch 字母 → CPU/MON 治理号。
- 交付组织：subagent 交付后缀、双克隆并行→merge 收拢（1619–1625，冲突文件 COMMIT_LEDGER.jsonl）、stash 快照旁支入库（1634/1635）——工作流形态直接进提交历史。

## 与下一阶段的衔接
- 本段末（seq 1785）根目录整洁基线完成后，seq 1786 | d8c821db 宪章冻结入库开启 STAGE-08（前台订正：原文指针 5a250999 与 seq 1786 不符，已按 _evidence/commits/index.jsonl 对账修正）；W1 冻结批次（140 号任务集）与 CI 修复常驻线跨入下一阶段（1780 记账线终结≠W1 结束，1781 起改以宪章为规格锚）。
- 带入项：package exit 5 修复本体（1631/1636 疑落 1706+，H-S30）；MON-001 门阈值（U≥0.75/连续 10s<0.50）与宪章 §17.6 文本（85%/60%）口径差无解释映射（H-S32 登记，不作评价）；P1 编号序≠实现序、drizzle INT 缺失。

## 证据缺口
- 全部 ctest/pytest/TSAN/ASAN 计数与 GitHub 结论未独立核验（gitignore 实跑产物不入库）；台账 date 字段与提交时刻背离（1641–1651 整点手写值，seq 1669 才对齐 committer date——按台账排序会假性回退，H-S31）；紧凑卡"代码面文件"把模块 README/memory/module.yaml 计入，8 条 DOC 冻结引用该字段需换算（H-S31）；1681 卡片聚合与逐文件行数不一致；批次字母 E/F/M/N 未见；若干跨片引用 sha（37ed737d、93ce026c、9e8ba103、4f6e2b65 等）不在本片证据集。
