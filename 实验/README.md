# ACSD 科学实验与五篇论文

本目录管理科学链 P1–P5 的**论文及可复算实验**。科学定义和设计合同仍以 `docs/ACSD_DESIGN.md`、`docs/science/**` 为权威；实验论文是检验、讨论和发表稿，不得以未经验证的论文文字反向覆盖科学规范。

## 五篇当前论文

| 论文 | 主题 | 当前主要未闭合问题 |
|---|---|---|
| [P1：Gaia XP 逐帧测光标定](photometric-magnitude/REPORT_paper.md) | 合成通量、稳健零点、空间标度与独立预算 | 真实帧残差、平场假设及色差归因 |
| [P2：跨帧参考信噪比](absolute-snr/REPORT_paper.md) | 条件 PSF 通量噪声、自由背景、相关噪声和实际覆盖率 | 真实星误差乐观、稀疏生产者 |
| [P3：HEALPix drizzle 守恒映射](healpix-polar/REPORT_paper.md) | drop 球面面积、局部完备性、极区与面边界误差 | face-center/apex 反例与独立几何 oracle |
| [P4：稀疏 SNR 场重建](dense-snr-reconstruct/REPORT_paper.md) | 参考物理量、样条/IDW、有效支持与偏差传播 | 样条过冲、域外和参考/场景对象混用 |
| [P5：加乘性天光与接缝](additive-sky-seamless/REPORT_paper.md) | 加性差模、残余乘性、重叠分量与低频保真 | S9b 分量失败、增益接线、真实边界覆盖 |

- **[五篇共用实验协议与矩阵](shared/FIVE_PAPER_EXPERIMENT_PLAN.md)**：四种独立证据腿、可证伪反例、数据索引、论文图表和科学验收门。
- **[核验主干文献与适用边界](shared/references/CORE_REFERENCES_VERIFIED.md)**、[BibTeX](shared/references/CORE_REFERENCES_VERIFIED.bib)：只把在期刊出版社/作者官网确认的信息列为“书目已核验”，不以 DOI 代替逐页方法核对。
- **[第一轮交接报告](engineering-evidence/acsd-science-docs-p5/HANDOFF_REPORT.md)**、[后续科学裁决](engineering-evidence/acsd-science-docs-p5/REVIEW_DECISIONS.md)：过程证据与改动方向；它们不是对外论文。

## 实验单元标准

一个单元维护 `README.md`、`REPORT_paper.md`、`REPORT_experiment.md`、`code/`、`docs/`、`data/`（可只记录索引）及必要的 `results/`。正式论文写方法、推导、可辨识性、对照、结果级别与科学限制；实验报告保留真实发生过的数值/命令，不因结论变化而事后篡改原观察。大型 FITS 与运行时日志留在 `testdata/` / `run/` 或原用户设备上，不随 Git 分发；入库的是复现脚本、轻量哈希索引、统计结果摘要与许可信息。

**证据等级：** `BIB_VERIFIED` 只保证书目信息；`SELF_CHECK` 是数学独立重现；`ORACLE_CHECKED` 有独立真值；`PRODUCTION_CHECKED` 已链接当前生产算子且故障注入可判红；`RELEASE_VERIFIED` 还需真实数据和 CI/端到端验收。任何未执行的实验标 `NOT_RUN`，代理报告但缺独立复算的数字标 `AGENT_REPORTED/UNVERIFIED`。

四种必备数据腿包括解析 oracle、HST 非平凡结构的物理前向仿真、本地校准真实帧以及当前生产代码/HiPS 端到端。真实帧不是外部绝对物理真值；HST 叠加成品亦不能冒充未处理原始探测器噪声真值。用于星表与真帧的许可证和可再分发性须分开登记。

## 其他实验与证据

- `cone-search-constants/`：锥形搜索相关数值方法。
- `m42-realdata/`：M42 真实数据集和校验任务。
- `P5-signal-extraction/`：与 P5 相关的探索性历史验证，引用须说明与第五篇主实验单元的角色不同。
- `shared/`：合成数据器、共享数据引用和引用文献。
- `engineering-evidence/`：带时点的工程质量与科学审查证据。
- `reviews/`、`裁决台账.md`：历史审稿与决议面，不自动赋予数值科学成功地位。

## 上游合同与参照

上游：[最高设计](../docs/ACSD_DESIGN.md)、[科学文档](../docs/science/README.md)、[文档治理](../docs/engineering/governance/DOCUMENT_GOVERNANCE.md)。任何论文结论与科学规范不一致时，须重新核验文献、算式、生产实现和实验真值，完成同批订正与测试后再改科学正本，不得以“论文为准”掩盖合同冲突。
