# ACSD 五篇科学论文整改 · DeepSeek 执行交接包

**交接基线：** `e31b2be66ffe37d547e67a4ba007e7257e49789d`  
**仓库：** `https://github.com/fujiaze/Astro-CS-Database`  
**分支：** `audit/acsd-science-docs-p5`  
**状态：** 五篇科学论文、共享方法学协议、参考文献与先例矩阵、科学规范纠偏已推送；真实数据和生产计算测试未在本轮由 ChatGPT 运行。

## 为什么交付包不复制仓库全文

科学正文及文献的**唯一最新正本在远端工作分支**。本包固定完整 SHA 并精确指向真实文件，避免离线副本覆盖新版本。DeepSeek 在带有仓库及真实 `testdata/` 的工作节点上 fetch，并以该 SHA 为起点。单独打开 ZIP 不代表全部科学论文正文已离线打包；这是一套可执行的交接控制包，而不是第二份并行科学文档体系。

## 用户委托中已经直接完成的部分

1. 五篇原有 `实验/<unit>/REPORT_paper.md` **原位重写**，包括摘要、数学模型与单位、可辨识条件、现有先例、实验矩阵、限制与参考文献。历史超长稿可用 Git 历史追溯；不可直接作为当前科学成功结论。
2. 五篇 `README.md` 与实验根索引整理；五份历史 `REPORT_experiment.md` 插入证据状态前言，原观测记录未删。
3. 新建统一实验协议 `实验/shared/FIVE_PAPER_EXPERIMENT_PLAN.md`，所有论文用解析 Oracle / HST 物理前向 / 真实观测 / 生产代码四条不同证据腿。
4. 15 项主干参考资料和 BibTeX：`实验/shared/references/CORE_REFERENCES_VERIFIED.md`、`CORE_REFERENCES_VERIFIED.bib`、`PRIOR_ART_CLAIM_MATRIX.md`；书目信息核对不等于逐页研究已完成。
5. 修订最高设计 `docs/ACSD_DESIGN.md`、P3 规范 `docs/science/drizzle/DRIZZLE.md`、P5 规范 `docs/science/sky/UPM.md`，纠正“理想守恒等于数值无误”“参考 SNR 等于场景源 SNR”“接缝平滑等于天光真实恢复”等无条件主张。
6. 已建好细粒度任务书 `实验/engineering-evidence/acsd-science-docs-p5/DEEPSEEK_SCIENCE_EXECUTION.md`，**该文件是服务器任务最完整的正式执行入口**。

## 包中文件

- `01_START_PROMPT.md`：直接粘贴给 DeepSeek 的启动指令与操作红线。
- `02_TASK_MATRIX.md`：按 S00–S09 划分的实验与实现任务、前置条件、交付物。
- `03_SCIENTIFIC_ACCEPTANCE.md`：五篇的数学/单位/Oracle、反例、统计、论文验收门。
- `04_LITERATURE_GUIDE.md`：核验后的核心学术引用 DOI，指定精读方向。
- `05_REPO_FILE_MAP.md`：实际仓库中的文件位置和 Git 改动边界。
- `06_REPORT_TEMPLATE.md`：统一真实回传报告模板；不得将未测算结果编造为 PASS。
- `VERIFY_BRANCH.sh`：只读检查仓库、完整 SHA、文档存在性和工作区状态，不会更改文件。
- `MANIFEST.json` 和 `MANIFEST.sha256`：控制包清单及哈希。后者涵盖除其自身外的全部文件。

## 执行与交付顺序

**第 1 步：** DeepSeek 在其已有仓库中执行 `git fetch origin`，按文件 `01_START_PROMPT.md` 切到分支，先确认 Git 工作区清洁、HEAD 不早于交接 SHA。  
**第 2 步：** 先读远端 `DEEPSEEK_SCIENCE_EXECUTION.md` 与 `FIVE_PAPER_EXPERIMENT_PLAN.md`，再执行各项，不创建与既有五篇争权的并行“论文”。  
**第 3 步：** 每个 science 模块按有真值的 Oracle、生产故障注入、合成数据、真实数据的顺序交付，每阶段分别提交并推送本工作分支。  
**第 4 步：** 报告 SHA + 复现命令 + 输入哈希 + 实测结果/未决项，再交回 ChatGPT 复核；**不合并 main，不需要 PR**。
