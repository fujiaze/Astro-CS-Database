# STAGE-01 上游模块起源、主仓整合与第一代工程控制包

## 时间窗与提交数
- seq 1–271（共 271 条；merge 14 条，L 级 85 条）。日期 2026-05-24 至 2026-08-02（本段 seq 为谱系聚合拓扑序，非日期序；上游 12 根交错，见 H-S01/H-S02 开篇声明）。
- 子段：上游模块起源 seq 1–99（12 个上游根 + 主仓根共 13 根）；主仓整合 seq 100–110；第一代控制包 seq 111–271。
- 对应分片：H-S01、H-S02、H-S03、H-S04、H-S05（前半）。

## 该阶段要解决的问题（引自提交消息/包文本，标注自述）
1. 把散在 12 个独立小仓库的天文图像模块（Gaia XPSD、astro_image_io、star_detector、dynamic_psf、calibration、photometric_calib、data_pipeline、healpix_db、plate_solve、snr_estimator、orchestrator）整合为单一工程（seq 100 | c384de17 "chore: initialize main repository"；seq 101–110 十连 merge 按 lib/<module> 导入，每环第二父恰为对应模块仓库 tip——变更面证据，H-S02）。
2. 建立"控制包驱动"的工程治理：engineering/ v1.0（seq 118 | 1ac74251，00_START_HERE…15_ROADMAP_AND_GATES 全套）→ v1.1（seq 128 | ba4f0d34）→ v1.2（seq 162 | 0d1857de）→ v1.3（seq 175 | f4ec8b24，单提交 +446 包文件）→ 权威开发包 v2.0（seq 204 | 75b05f72，engineering_authoritative/，A-001..I-002 Gate 台账）（自述+变更面双证）。
3. Stage1/HISS 交付规范化与外部审查轮缺陷闭环：seq 224 | ccfe9a67 范围收敛（自述停 Stage2/HCSD/710 回归，唯一里程碑=冻结 HISS+Stage1 规范）；seq 226 | 78a9121d Stage1_HISS_Delivery 交付包；seq 227 | b4bcf8e7 docs/stage1_fix 17 步修复规范（WP-A~WP-I 编号族）；R04→R07 审查轮（AstroCS.wiki 子模块指针 246/253 标记轮次切换，R{n}-B/M 缺陷族）。

## 具体工作内容（按模块）
- 科学算法：上游各模块以 Python/C++ DLL 双栈起步（根 4a2750bd/802becc2/850973a7/12c55ed6/18 等，H-S01）；算法代际切换 seq 17 | 8c0762ca star_detector V5.0（Gaussian PSF 替代 Moffat4、GSL trust-region LM 替代手写 LM，符号级证据 sdet_moffat4_residual 删除/sdet_gaussian_* 新增）；Python→C++ DLL 栈切换（seq 24/29→30 calibration 双线并轨、seq 42 | photometric_calib 重写、seq 44 calibrate_data 切 DLL、seq 20 astro_image_io C++ 原生）。旧 HISS/ahpx 路线退役 seq 254 | 0f195b68（纯删除 hiss_v2*.py、aio_ahpx_*，-4530 行）。
- 管线与 CLI：orchestrator 建仓 seq 76 | 127c4a96（自述 9 节点管线）→ seq 82 两阶段 10 节点 PipelineStageV2 → seq 98/99 stage1 收缩 7 节点（GRADIENT_2D 撤销）；healpix_io 并入 astro_image_io（seq 77，seq 95 | 7604fdca 归档 -13083 行）。
- 构建与 CI：无成体系 CI 门禁证据；线程治理萌芽 seq 32/33/34（同 5 秒三连删 num_threads(16) 硬编码），seq 47 回补 -fopenmp 并证实 seq 33 曾致并行失效（H-S01）。GitHub 分支统一为 main 自述见 seq 57 摘录。
- 控制包与治理：v1.0→v1.1（seq 128，1215 文件巨型提交，59 改名归档 v1.0；注意 v1.1 重定义 P00-002/003 语义——跨片引用编号须先判包版本，H-S03）；G0–G8 Gate 翻牌链（seq 125/138/144/149/153/156/159，MASTER_TASK_REGISTER.csv 单行台账）；astro_toolkit.py（seq 151 | 5ec98660，JSON 驱动批量 git/运行/sha256）；v1.1 审计包+CORR-001..006 修正（seq 161→162）；v1.3→工程控制→权威包：seq 203 | 036a3bb5 4261 文件根目录整理（engineering_v1.3→工程控制，删除 engineering/、engineering_v1.2/、engineering_archive_v1.0/、audit/、dist/，evidence 2.9GB→26MB），seq 204 权威 v2.0 安装（旧 工程控制/ 降格 LEGACY_NON_ACCEPTANCE_EVIDENCE）；Gate A→E 单日收口链 seq 207/210/213/214/215；seq 237/238 删除 engineering_authoritative/、_v2_pack/、_wiki_freeze/ 等旧控制面（-43276 行）+ AGENT.md 建立。
- 文档与报告：模块 memory.md 台账族 seq 49–55 连续 7 笔批量建档+README（H-S01）；docs/ARCHITECTURE.md 等首现 seq 112 | d411565e；项目 README + v1.1 审计包 seq 161。
- 测试：模块级测试随上游链（H-S01 各条目）；seq 217 | 5a41e285 PROJECT_STATE 置 current_gate=I + full_regression_allowed=true + 双契约冻结 → seq 219/220 启动 710 帧三设备并行全量回归（结果越界归 H-S05）。

## 交付与验证留痕
- v1.1 审计包（seq 161，git bundle 自述 160 commits）；权威包 v2.0（seq 204，A..I 台账 + SSOT README 重写 +1535/-159）；GATE_X_REPORT.md 逐 Gate 入库（seq 207/210/213/214/215）；Stage1/HISS 交付包 MANIFEST（seq 226）；交付 ZIP 与仓内副本口径差登记于 H-S05（seq 226）。全部"测试通过数/回归结果"属消息自述，除 219/220 回归台账面外无独立产物可核。

## 结构与规范变化
- 根谱系：13 根→1（seq 100/101–110）；目录：engineering → _new_pack_v1.1 → engineering_v1.2 → engineering_v1.3 → 工程控制 → engineering_authoritative（两度整体搬家 seq 203/237）；编号族：无 → P00..P13 → A-001..I-002 Gate 链 → WP-A..I / R{n}-B/M 并存；根目录固定条目规范萌芽（seq 237 清理 + AGENT.md）。

## 与下一阶段的衔接
- seq 272 | f8d749e4（08-02 13:07，前一条 271 是 R06 交付工具脚本）新建 lib/acr + ADR-001~009 + dependency-lock.json + path_guard.ps1——AstroCompute Runtime 底座建设期开始，标志进入 STAGE-02；seq 267–275 的 R07 线也由 STAGE-02 头部（seq 276 起，H-S06 重叠区）接续。
- 宪章张力登记：seq 272 自述 worktree feature/astrocompute-runtime 与 main-only 纪律的冲突（H-S05 建议记 finding）；seq 129/130 亦有 path-b-callback-export 分支期（H-S03）。

## 证据缺口
- seq 31/39 卡片"diff 读取截断"（31 有 11 文件未列名）；seq 35 +64269 行数据构成未证实；seq 38/39/45 迁移无 git rename 机械证据；seq 44 消息带 UTF-8 BOM 成因未证实；seq 31/39 作者邮箱 outlook.com 异常无解释；seq 169"710 帧 587 resolved"vs seq 205"35 对 32 resolved"口径跳变、seq 209/210 RMS 不一致、seq 217 GATE_G"3 片→30 帧"订正均卡面无解释；P13-002 台账 DONE 收口提交缺失（H-S04）；工程控制/_control_packs/ 控制包原件与 wiki 子模块正文不可核（H-S05）。
