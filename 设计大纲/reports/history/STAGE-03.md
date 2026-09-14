# STAGE-03 Phase2 开工与 v6→v19 快速迭代期

## 时间窗与提交数
- seq 551–715（共 165 条；merge 0，L 级 12 条，"改动全在噪声区"1 条 seq 615）。日期 2026-08-10 至 2026-08-16。
- 对应分片：H-S11、H-S12、H-S13。本段为单亲线性链（550 合流后无分叉，seq 序=时间序，H-S11/S12/S13 开篇声明）。

## 该阶段要解决的问题（引自提交消息，标注自述）
1. Phase2（马赛克叠加/rejection）从零建设：seq 552 Phase2 控制包 V1（SHA 34A532A2…B2EB308）开工，lib/phase2 新目录（seq 553 | 1a38f400 首现，W2 接口冻结 P2_API/P2_REJECT_* 枚举、stage2.schema 首版）；seq 558→559 五小时断档后从合成骨架（gate 12/12）切真实 AIO 链（559–563，astrocs-stage2 真实入口）。
2. 休眠 ACR 转入业务消费：seq 574 | f9a06ac6 首个真实 CUDA mosaic_reject kernel（自述 RTX 3060 Ti，CPU/GPU 等价 gate 21/21）；seq 577 逐 tile ACR 路由（自述 61.6M px diff 7.45e-9）。
3. 审核-修复轮驱动的版本阶梯 v2→v19：seq 581 | 32d84795 前缀 phase2→phase2-v2 + AuditFix V2 控制包点名、R 编号族开启；v3/v4 轮 R 编号两度重启（seq 592、599）；gate 计数单调 22→41 作轮内标尺（H-S11）。
4. HiPS 可视化与真实数据基座：seq 612 | 9ad94dc1 BASS DR3 数据集入库（365 文件 + .gitignore 治理）；seq 616 | 42c801de phase2-v9 浏览器 campaign（healpix_browser_qt core/widgets/tests/tools 四层新建，USER_VISUAL_ACCEPTANCE=PENDING）。

## 具体工作内容（按模块）
- 科学算法：codeql-v1 P1 收口（26 告警修复，seq 606 | 5571e9bc）；v12r2 缺陷族 HOLE-001/SEAM-001/002 临时法（最近邻 C 外推+MAD 清洗，seq 620/621）在 v13 被明言撤销、改 DBE-like 采样 + Laplacian 延拓（seq 622 | 83471979）；v14 拒绝语义与 STF 三控制（seq 624–646，typed params/WBPP 2.9.1 策略版本化 profile）；v19 三层噪声模型（seq 688 | f6727773 NoiseWeightModelV1/PsfFitQuality/PhotometricCalibrationQuality，689 方差传播、690 ivar 权重退役 raw-SNR）；v18r3 极面剪枝由实测自证升级为双-Lipschitz 可证明保守（seq 684 | 348ca1e8，GAIA_CACHE_VERSION 1→2）；v18 Drizzle profiling +5548 行（seq 671 | df5cbb5d，healpix_core/snr_evaluator/nanoflann 内化）。
- 管线与 CLI：G6/G7 门禁销项（seq 628/635）；性能线（seq 627、665 tools/phase1_e2e_bench.py）。
- 构建与 CI：v17 定稿工具族（seq 667 tools/gen_repo_source_manifest.py 4197 文件 SHA256、assemble_v17_review_pkg.py）；配置一致性机器门（seq 624/684 check_baseline_opcodes 前奏、api_doc_consistency seq 664）；docs_machine_consistency v1.0.0 工具首产（seq 695 | c0753e33）。
- 控制包与治理：Phase2 控制包 V1→AuditFix V2→v3/v4 轮（H-S11）；self_review round0–6 方法学（seq 647 | 4b92989f 起，FINALIZATION_SELF_REVIEW=PASS seq 653 | 8a772cae）；V14 审计摘要 FROZEN_CANDIDATE（seq 626，G7 挂起→628 PASS）；v19r2 全仓盘点 25 字段审计 schema（seq 698 | 729bc3b9）+ docs L0-L5 权威层级 64 合同 ID（seq 700 | 8ab2efbd）。
- 文档与报告：docs/contracts、docs/architecture、docs/development、docs/browser、docs/validation 等目录族首现（seq 624 | 5a9eadcf，provenance 引入 C624）；reports/ 交付报告波（v14 六篇 626、v15 九篇 651、v16 四篇 658、v17 六篇 668、v18r2 五篇 683、资源时间线 677）；CHANGELOG 类文档与 docs/BUILD_RELEASE 等预发布文档族（seq 691 | 1d3a34c2 v19 全站文档 17 新目录）。
- 测试：合成门计数 38/38→45/45（seq 606/607，v5 跳档无卡）；静态分析 311 units 全覆盖收口（seq 715 | 23c11c19）；v19r3 步骤族 s0→s10（执行序重排 s6 先于 s4/s5，属任务调度非谱系分叉，H-S13）。

## 交付与验证留痕
- 审核包登记：AstroCS_Review_Phase2_V1.zip（首见 seq 564–567）；V1 递进终点 SHA 6EC2A920（seq 580）；v17 freeze PASS 自述 74/74 清洁构建（seq 669 | c9b8dfc3）；self_review 六轮报告入库（seq 647–653）；v17 交付报告组（seq 668 | d00b9f42）。所有量化（RMSE 1.07×、13× failure、7.45e-9、0.0002 bias）均自述；581 之后无 EXECUTION_LOG 台账条目（H-S11 缺口）。

## 结构与规范变化
- 目录：lib/phase2（553）、healpix_browser_qt 四层（616）、BASS DR3/（612）、self_review/（647）、docs 六分层（624/700）、tools/quality（698）、docs/TRACEABILITY.csv（697 首建）。
- 编号族：v6–v19 版本前缀、R/P0-xx（581 起）、HOLE/SEAM、G4–G7、QA-*（695/701 起）、v19r2-s*/v19r3-s*。
- 事故模式：dll.bak_v18r2 误入库-即删对在 seq 713/714 与 724/725 两次同型复现（H-S13/S14 共同证实，git log --name-status 核）；"审计 §12 假 PASS"被 712 与 718 先后两次自述修复。

## 与下一阶段的衔接
- seq 715 为 V19R3 质量收口候选边界，seq 716 起（H-S14）进入 v19r3-s12→v19r4 生产接线；本阶段末的 docs/TRACEABILITY、tools/quality、机器门体系是 STAGE-05 QA-V19R7/R8 的直接地基；seq 713→715 的产物收口问题在 STAGE-05 被"假 PASS 订正"追责。

## 证据缺口
- v4→v6 跳档（无 v5 提交、gate 38→45 间 7 门建立点无对应卡，seq 607）；581 之后 v2/v3/v4 控制包 SHA/任务表与 P0-01..10 全集未入库；559–563 gate 中段计数不可复原；591 泄漏引入者、603 官方 harness 落位、574/577 GPU 路由构建形态未证实（H-S11）；658 的 17 文件中 8 个路径未展开、649/643 stf_engine 引入点矛盾、614 构建尾注（H-S12）；701 自述 354 文件 vs 卡 353 差 1、706 三步合一提交、707 操作对象台账未点名（H-S13）。
