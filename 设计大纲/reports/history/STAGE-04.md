# STAGE-04 V19R4–R8 质量闭合、v20 预发布与 REAUDIT_V3 启动

## 时间窗与提交数
- seq 716–990（共 275 条；merge 0，L 级 6 条）。日期 2026-08-16 至 2026-08-27。两处日历空窗：729→730 约 5 天、964→965 约 2.7 天、965→966 约 2 天。
- 对应分片：H-S14、H-S15、H-S16、H-S17、H-S18。全段单亲线性链，seq 序=时间序（H-S14/S17/S18 声明）。

## 该阶段要解决的问题（引自提交消息，标注自述）
1. Windows/真实数据 Tier-A blocker 关闭：seq 730 | e91e06f5 "Tier-A 7 blockers closure（CAL window、AIO truncation、GAIA zero/dangling、ORCH FP…）"，V19R4→V19R6R2-W1 轮次切换 + 5 天空窗（H-S14）。
2. 以控制包母体组织质量收尾：seq 735 | 23662d63 QA-V19R7 控制包建档（Spec/Checklist/Task + 95 任务注册），此后所有任务号出自该母体（H-S14）。
3. 文档-代码一致性锚定与逐批销账：A 阶段（架构）→ B1/B2（L1 docs/science、L2 docs/algorithms 对齐）→ B4/B5（drizzle/ivar/shim 锚点）→ C 批（SKIP+证据工件）→ D 批复盘（H-S15/S16/S17）。
4. V19R8 新一代立项：seq 816/818/820 三件套（Spec 255L + checklist 185L + tasks 421L）、seq 873 ADR-QA-V19R8（六阶段 S0–S6、commit 预算 90+10、每 10 commits 一 checkpoint）（H-S15/S16）。
5. v20 Linux 关闭与预发布：seq 965 | 535e7387 L 级 squash（108 文件，tools/quality 52 件合同检查器体系 + 全站文档重排，自述"all fixes from prerelease/v20-linux-closure squashed"）→ 并行化 REAUDIT_V3（CON-001..010）。

## 具体工作内容（按模块）
- 科学算法：B4 锚点为注释级生产码+文档成对（826–868，无科学语义改动证据，H-S16）；SHA-256 shim 归一（seq 791 | a4452d89）；HEALPix 去重净 -501 行（seq 817）；orchestrator 错误码（seq 794）；D 批 C-06 编译告警长尾跨 5 个代码提交（H-S17）。
- 管线与 CLI：stage2 运行攻坚（seq 944–962，sampler parallel+streaming+frame_id cache 自述 14min→~90s，seq 944 | b8d215d0）；stage2 V17 typed rejection 配置迁移（seq 935 | 754abc99，脱离 QA 链的 [P0] 独立线）。
- 构建与 CI：machine 9/9 检查器 JSON 首落盘（seq 790 | 7340e69c checkpoints）；docs_machine_consistency 全绿（9/9 PASS 自述）。
- 控制包与治理：PROJECT_STATE 控制面跃迁（seq 869 | 8370d143：v1.3 p13-stage1/G12 → V19R8 quality-closure/G-QA）；B3 Gate PASS 42/95（seq 789）→ B4 28/28（seq 868）→ C 11/11 89/95（seq 913）→ D-06 95/95（seq 934）；V19R8 final 但维持 PRE_RELEASE PASS FINAL PENDING（seq 933/934，发布未决口径）。
- 文档与报告：三阶段文档审查 Stage A（science 10+2 PASS，seq 936 | 10b91990）/B（algorithms 11/11，seq 937）/C（architecture 锚点三件套 ARCHITECTURE+API_REFERENCE+CODE_CHANGE_MAP，seq 938 | a6c88115）；self_review round1/2 报告（seq 942/943，0 P0/0 P1 终止）。
- 测试：p1noise 等六域可复用验证面（seq 1769 后移属下一段；此处为 C 批 SKIP 模式）；静态分析 311 units（收口在 seq 715 前段）。
- REAUDIT_V3：seq 966–990 CON-001..009 PASS + CON-010 首个 FAIL 字面量（2T SIGSEGV，seq 986 | 53a1cc04 自述 TSan 实锤 cfitsio 并发读），至 990 以 Amdahl ~1.34x "G2 停止点"收尾；结构搬家三连（seq 978/979/980，REAUDIT_V3 355 文件导入/333 改名/reports 126 归档，reports 按版本分目录）。

## 交付与验证留痕
- QA-V19R7 控制包三件套 + 95 任务台账（735）；checkpoints JSON 工件组（round03/07/25/28/37/40/44/47/64/66/68/80/83，done 53→95/95，H-S15/S16/S17）；evidence/QA-V19R7-A2-01..08（seq 743/744）；reports/v19r7_quality、v19r8_quality 落盘目录切换（seq 821 | f1cf87a6）；证据封包 manifest+SHA256SUMS（seq 922 | b0e06eb4，22 文件哈希，927/930 两轮订正）；reports/stage2_fix_report.md（944）；docs/release、docs/phase2、docs/audit（965）。

## 结构与规范变化
- 目录：reports/ 版本化分目录（980）、工程控制/REAUDIT_V3（978）、tools/quality 52 件（965 入库）。
- 编号族：V19R4→V19R6R2-W1→QA-V19R7（A/B*/C/D）→V19R8-S*→CON-00x。
- 作者身份切换：seq 731→732（fujiaze@users.noreply→付家泽@126.com）+ vm-bj 工作目录基线入 memory（H-S14）。
- 假 PASS/证据振荡问题：C-04 链（899 SKIP→924 真实 82/82→925 空面重复销账→926 回退→929 恢复→930 哈希终订）为本阶段最显著治理事件（H-S17）；"实现短哈希与卡面哈希系统性错位 12 处"（H-S15）+"消息引用 SHA 无法解析为当前仓对象"（seq 720 起，H-S14 用 git cat-file 全局核）——锚点来源判为"未证实"。

## 与下一阶段的衔接
- seq 986 CON-010 FAIL（cfitsio 并发读 SIGSEGV）跨段悬置：STAGE-05 开头（seq 992–1003 CON-010 取证组 12 条）接续，最终成为 V5 期 PAR-002 BLOCKED 的同一根因（seq 1152，H-S19/S21 互证）；seq 1036 "执行侧全部停止、控制包打回重做"再引出 V4/V5 换代（H-S19）。

## 证据缺口
- hotfix 链与审查报告引用的十余个 7 位 sha（756805f、d2420f6 等）与主线 sha8 不同源，映射未建立（H-S18）；12 处 checkpoint 消息引用实现短哈希与卡面值全不匹配（成因不可判，H-S15）；965 squash 内部变更序不可复原；978 的 355→333 差集清单未全列；P1..P5 攻坚无"根因确诊"收口；"PASS/9/9/89/89"类多为自述；B3-04 无改即 DONE 巡检无落盘（H-S15）。
