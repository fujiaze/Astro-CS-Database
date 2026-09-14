# STAGE-02 ACR 底座建设期与 Phase1 HiPS 换代收口

## 时间窗与提交数
- seq 272–550（共 279 条；merge 9 条，L 级 30 条）。日期 2026-08-02 至 2026-08-10。
- 并行谱系显著：ACR 线 / R10–R13 编排-双精度线 / HiPS V3→V6 线多链交错聚合（H-S06/S07/S09/S10 开篇声明；seq 增而日期回退属拓扑聚合非回退）。
- 对应分片：H-S05（尾部）、H-S06、H-S07、H-S08、H-S09、H-S10。

## 该阶段要解决的问题（引自提交消息，标注自述）
1. 为 ACR（AstroCompute Runtime）建立底座并冻结架构：ADR-001~009（seq 272/273）→ 固定路由模型（seq 284 | bdefe797）→ ADR-010 反转冻结 + routing/ 整体删除（seq 305 | 447c5ea5、306 | b6891124 净删 1884 行）→ 目标像素操作聚焦（seq 406 | 0030c3a5 ACR_FOCUSED V1）→ 成本单位修正（seq 438 V3）→ 架构+加权积分冻结（seq 460 | 38b85702 V4）→ route profile v2 子系统（seq 471 | 8563c8d4，+2232 行）。
2. Python 生产层下线、C++ 编排器单入口化：seq 310 | 23d07ed4 删 32 个生产 Python 封装（-13993 行）→ seq 322 | 8653bd54 tracked 再删 40（保留件加 NON_PRODUCTION_TOOL_ONLY 标记）；seq 320 | 85fa651c 删 REPL 与旧命令，唯一入口 orchestrator.exe <stage1.json>，订正 CFG-011（output_ahpx_path→output_hiss_path）。
3. Phase1 输出格式换代：HISS→HiPS。seq 481 | 30293051（自述 HISS deprecated、AIO 唯一写入器）→ seq 498 | 5d5be0ac CFITSIO 4.6.4 vendored 导入（+265118 行，186 文件）+ HiPS 直写生产链（V3 起点）。
4. R07–R13 审查轮缺陷修复与归并裁决：ACCEPT_PRODUCTION 三笔（seq 297–299）/FAST_RESEARCH_ONLY 五笔历史归并（seq 300–304，-s ours 主树零改动——按影子树登记，证据价值仅消息裁决文本）；22/23/24/25 号纠正计划连跳（seq 323/344/367/384）。

## 具体工作内容（按模块）
- 科学算法：Drizzle 性能阶段 3→7（seq 361 基线 21.7s → 383 阶段 7 9.5×，阶段号消息内显式递增，H-S07）；HISS/PREC 双精度族（seq 362/368/374/375/377/378 08-05 上午集中）；R12→R13 独立 Oracle 方法学（seq 440 | a50a97a2 CAND-001，oracle_independent_test）；流水线阶段序契约变更 seq 479 | ab486b62（PSF=2 先于 PLATESOLVE=3、star_id 贯穿，482 Gate8 验收）。
- 管线与 CLI：见上第 2 点；编排器 AIO ABI 握手硬失败 + HISS 解压 64MiB/NSIDE≤2^22 资源上限（seq 470 | 11bbfd41）。
- 构建与 CI：固定种子 dataflow_fuzz 四类各 5000–10000 例（seq 470）；ACR_BUILD_SANITIZER 构建选项成合并前硬门禁（seq 308 | 038db00a）；"每 Commit 一证据目录 + merge_to_main=false + SKIPPED"模式（seq 312/319）；证据策略反转——seq 353 一次性删除全部 19 个仓内证据文件、改仓外单 HEAD 生成（23号计划 §7），此后测试计数全部退化为消息自述（本片口径分界，H-S07）。
- 控制包与治理：工程控制/tasks/acr 控制包换代（seq 293 | 49ea5c1e 旧 spec/tasks 删除并入 2026-08-02 包，"控制包无版本号"声明）；ACR_FOCUSED V1→V4 四连（406→429→438→460，评审件随包入库呈"评审→改版→修复"节奏）；台账状态字面量 ARCHITECTURE_DIRECTION_FROZEN=true 且 READY_FOR_BUSINESS_ADAPTER=false（seq 466 | 323b441f）→ 转 benchmark 驱动路由；V3→V6 审核包 SHA 更替链（seq 538/543/544）、PHASE1=FROZEN 字面量移除（seq 542）、ACR 基座 merge 休眠并入 main（seq 550 | 198d69e0，自述"无业务调用"）。P13-003 证据目录启用（seq 518 | 686f7959）；IMG-001/CFG-001 契约切换（seq 529/530：output.hips 权威、pixfrac 默认 0.8、内嵌 schema SHA 冻结）。
- 文档与报告：wiki 指针推进"Final Freeze"（seq 422–428，424 drizzle_freeze_test 39/39）→ 状态反转 NOT_SYNCED（seq 437 | 0f451b28，Phase1 未闭合，H-S08）；memory.md 台账内测试计数全为自述。
- 测试：drizzle_freeze_test（424）、qualification/focused 目录族（408）、mixed 路由实测仅测试自述（H-S06/S07）。

## 交付与验证留痕
- 审核包 V3–V6 的 SHA256 记录链（seq 538/543/544）；ACR 四轮控制包标识更替（A9766B99→426D9A51→CE288DBF→50127981，seq 497/521/531/549，台账标题为证）；V4 sanitizer 证据归档（seq 523，V5 对应归档缺失见缺口）；seq 289/296/312/319 有入库证据文件，其余通过数（623/224/14/14、9003/9003、5502/5502 等）均为自述（H-S06/S07/S08/S10）。

## 结构与规范变化
- 目录：lib/acr 新增（272）→ routing/ 删除（306）→ qualification/focused/ 新增（408）→ 共享 lib/common/healpix core（514 | 6d810a37 删 AIO 私有 ang2ipix_nest）；third_party/cfitsio 入库（498）；证据目录去版本号统一（296）。
- 编号族：ADR-001..010、R07..R13、F-fix 1–10 与 Commit A–G 并存、CFG-1xx、PREC-1xx、SNR-101、CAND-001、P13-003、IMG/CFG-001、V3–V6 审核轮。
- 门禁：path_guard.ps1 新增 tools/_* 排除（296）；"28/28"等 B 批收口尚未开始（本阶段无 QA-V19R7）。

## 与下一阶段的衔接
- seq 550（ACR 休眠并入）+ seq 551/552（合流登记 + Phase2 控制包 V1 开工、lib/phase2 从零建模块）为直接交接点（H-S11）；本阶段末 HiPS 直写链与 CFITSIO vendored 是 Phase2 mosaic/HiPS 浏览器的地基。
- 未收口项带入：drizzle INT 缺失线（至 STAGE 内后续）、V5 sanitizer 证据归档缺失、SEQ-012（303 的 FAST_RESEARCH_ONLY）后效、F-fix 10 于 seq 331–333 落地（H-S07 关闭 H-S06 缺口）。

## 证据缺口
- seq 305 消息自述"合并控制包 32 文件"与变更面（仅 lib/acr 7 文件）不符，实际合并在 307（口径差）；seq 322 自述删 421 个 Python 文件 vs tracked 仅 40（archive/ 不可核）；seq 424→437"冻结→未闭合"回退的原因文档在 wiki 子模块（本仓不可核）；全部测试通过数无 CI 日志交叉（353 之后仓内无产物）；链 A/B 分叉点与合流点在片外（H-S09）；MICROFIX #1、REV-102/103 编号缺口未证实；wiki 指针提交（507/526/537/542）正文物料在子模块仓库不可核（H-S10）。
