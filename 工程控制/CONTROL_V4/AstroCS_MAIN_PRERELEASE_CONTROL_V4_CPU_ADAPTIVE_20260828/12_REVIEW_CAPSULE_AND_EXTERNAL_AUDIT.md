# 每 commit 审查胶囊与外部科学审核

## 1. 每个 commit 自动生成胶囊

目录：`run/review_capsules/<task_id>_<commit12>/`，不提交 Git。

必须包含：

- `metadata.json`：task、parent/commit/tree SHA、时间、Agent、host；
- `commit.patch`：该 commit 单独 patch；
- `changed_files/`：该 commit 所有变更文件的**提交后完整版本**；
- `tests.json`：命令、timeout、exit、结果摘要、日志 hash；
- `contracts_affected.json`：SCI/ALG/API/TEST IDs；
- `resource_summary.json`：涉及重计算时必需；
- `large_artifacts.csv`：超限内容只引用。

单胶囊目标5 MiB、上限10 MiB。不得打入 build、binary、数据、完整日志。

## 2. 最新源码提交方式

- 每个 commit 的胶囊提供 patch + 完整变更文件，保证外部审核人看到最新代码而不是只有 diff片段。
- SCI/ALG/ARCH/API commit 额外附带该层**全部最新文档和机器合同**，避免跨文件推导缺上下文。
- 模块阶段完成时附带该模块相关 source/header/tests 的当前版本快照。
- 不反复打整个仓库；未变更模块通过 commit SHA 和 manifest定位。

## 3. 科学审查包 REV-001

必须包含：

- 全部最新 `docs/science`、`docs/algorithms`、核心 `docs/architecture`；
- science/algorithm/API contracts 和 traceability；
- 公式对应 source symbol、测试和 Oracle；
- `SCIENCE_CLAIMS.csv`；
- `REFERENCES.bib` 或等价引用库；
- 所有相关 commit胶囊。

`SCIENCE_CLAIMS.csv` 字段：

```text
claim_id,science_id,document,section,formula_or_claim,unit,assumptions,
primary_source,doi_ads_arxiv,source_section,project_choice,source_symbol,test_ids,review_status,reviewer_note
```

引用要求：

- 优先原始论文、标准、官方技术文档；禁止以博客/二手摘要作为公式 authority；
- 引用必须定位章节、公式或定理，不得只列论文名；
- 区分“文献结论”和“AstroCS工程选择”；
- Drizzle/HEALPix/稳健统计/噪声模型/误差传播/球面映射等核心定义必须有主来源；
- 项目自行设计的 UPM/权重必须给完整推导、维度分析和独立实验，不伪造文献背书。

外部审核人职责：

- 重新推导核心公式和单位；
- 核实主文献是否真的支持该说法；
- 对照实现和 Oracle；
- 给出 `ACCEPT / CORRECT / USER_DECISION_REQUIRED`。

Agent 不得替外部审核人填写 `review_status`。

## 4. 代码抽查

外部审核覆盖：

- 100% SCI/ALG/ARCH/API 文档和核心公式；
- 100% CPU feature/ISA安全选择、autotune cache、资源监控门禁；
- 100% AIO、Sampler、UPM、Integration、Drizzle 并行关键路径；
- 100% 独立 Oracle 的真值生成；
- 其他变更文件按 final commit SHA 做确定性抽样：以 `sha256(final_sha + relative_path)` 排序取前20%，Agent不得自选样本。

若抽样发现系统性问题，扩大到同类文件100%审核。

## 5. 不阻塞原则

- commit胶囊在后台生成并排队，Agent继续下一项机器任务；
- REV-001等待时继续CPU/MON/构建；
- 外部意见以新的原子修复Task落地；
- 只有科学定义存在两个合理但互斥选择时才请求用户；
- 最终发布前，所有 `CORRECT` 必须闭环，`USER_DECISION_REQUIRED` 必须解决。

