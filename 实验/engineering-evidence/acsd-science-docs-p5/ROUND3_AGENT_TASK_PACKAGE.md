# SCIENCE-REVIEW-03：DeepSeek 下一轮可执行任务

前置快照：8ecaac427d8d4466b5213d0dd6a6ad1c1a2ad1a9。开始前执行 git fetch origin；git switch audit/acsd-science-docs-p5；git pull --ff-only；git rev-parse HEAD，以**最新**实际 HEAD 为起点。当前分支已有 ChatGPT 审查结论、FINAL 报告状态订正及 P2 标准差术语修订，不能用上一轮工作树覆盖。不得更改 main、开 PR、force push、上传 FITS、删掉失败证据或调松阈值伪造 PASS。

## 审查与科学门

- 主审查文档：实验/engineering-evidence/acsd-science-docs-p5/INDEPENDENT_REVIEW_8ecaac427.md。
- 五篇主稿：实验/photometric-magnitude、absolute-snr、healpix-polar、dense-snr-reconstruct、additive-sky-seamless 下的 REPORT_paper.md。
- 论文/实验状态是两个独立维度：执行 DONE 不等于结果 PASS；FAIL、UNVERIFIED、UNCALIBRATED、NOT_IMPLEMENTED 不能互相当同义词。
- 项目仍未具备 RELEASE_VERIFIED，不得提出主干合并与五项创新都已成立。

## Q00 状态检查与审计（无需计算）

核实 SCIENCE_HANDOFF_REPORT.md 的 S00–S09、论文更新提交、2026-10-14 审查修订与实际 Git 提交历史。给 RESUME_NOTES.md 添加醒目的“已被后续记录取代”指针，但不删历史过程证据。生成主张-来源机器索引，五篇 Table4 逐行核对单位、样本量、分母、来源和证据持久路径。每篇旧数字、修后数字以及冒烟验证必须可独立区分。

## Q01 P1/P3 结果可携带及五篇实测图件（先做）

P1 的本轮 S04 脚本存在，但缺少随 Git 追踪的紧凑统计结果 JSON；依照报告原始数值和可复跑脚本，把每个 Table4 结果存成含来源 SHA、数据哈希、单位、分母、种子、CI 的小型证据文件，写入实验单元 docs。不得包含原始 FITS。

P3 第6、10、11行引用的 s02b_build_pair.sh 和 s02b_theta_{pre,post}.json 不在本次 Git 跟踪树内，必须补可运行脚本及轻量摘要，或将来源重指向真实含所列数字的已提交文件。第6行必须分别给“跨尺度最大值”和“同一构型 pre/post”统计，不得再拼接不同 scale 的读数。

五篇的 figures 目录只跟踪 README/manifest/生成脚本，**0 个 PNG/SVG 在远端 Git**。一份完整论文若声称有 Figure 2/3/4，必须可从洁净 checkout 重建并拿到实图。依项目不入库运行产物政策，可提交小型图表基础数据、生成脚本、内容 SHA，以及最终出版图件发布附件或经批准的版本化资源；不能仅有图名和本机哈希。

通过条件：clone + 可获得的轻量数据即可重算所有论文主表与图，任何数据缺失须明确 NOT_RUN。

## Q02 测试基线与真实 E2E（先做）

用同一 pytest 版本、相同命令分别在基线 e31b2be66... 与本轮 HEAD 运行子集，输出机器可读 pytest nodeid/status（推荐 JUnit XML），证明 new_failed=0，而非只说两边都是 29 failed / 152 passed。核实 29 个红灯确实都源于 module_adapters.cpp 锚点漂移，区分测试注册表过期与真实代码失效，不能先移除检测规则。

九个 E2E.* pytest 静态 skip 不等于已执行；先提交可运行测试车道与合同修订提案。三条 CLI rc=0 的正常路径属于冒烟层，所用 master_scale 是人为值，导出绝对量级不可信。完整科学 E2E 须以正确母版量纲/曝光/响应标定重新跑。

通过条件：有单测 nodeid 对照、明确的仍失败清单，以及科学级 E2E 和 CLI smoke 的独立结论。

## Q03 P2 k_corr 与 sparse_snr_layer（先设计，代码另批）

当前裁定：真实生产 rho≈0.83 时 k_corr 1.4 未标定，不能继续宣传通用保守。最先保证产品/证据显式输出 UNCALIBRATED，不能假装 1.4 已校准。再通过独立 MC + 真生产球面映射探针建模 k_corr 随 rho=leaf/source、pixfrac、N_retained、WCS/掩膜、nside 的变化；2.05–2.33 是待验证读数，不是允许写入生产的新常数。只有校准有效域、交叉验证和误差预算明确后才提交代码设计。

sparse_snr_layer 无生产写点：先画生产者→写盘→读回→P4 消费的真实数据流与接口，定义缺失时 fail-closed；不要用手写 sidecar 当作产品已实现。P2 经验 sigma_sys 加入后 68.27% nominal 变 86.11%，仍是过覆盖，不能称已校准。生产改动另行批准。

## Q04 P4 统计目标/参数一致性（先设计，代码另批）

当前真实生产有效 idw_power=2.0，规范写 1.0。先固定有效配置和 provenance，跨场型/真实控制/留出 CV 比较 p1/p2，并明确失效域；仅凭合成场型不能选全局最优。统一参数时必须同批改文档、入口解析、测试和 manifest。自然样条会超冲/欠冲，NaN 与支持域外不能被无提示填值且仍称无偏。生产改动另行批准。

## Q05 P5 阴性增益、跨分量锚及故障注入（先设计，代码另批）

g_true=1 的阴性世界 10/10 seeds 给显著伪增益，继续禁用 P5 乘性施加；研究星点 PSF/孔径/色差系统混淆后再做真正的外部留出验证。不要因 seam 变平而接受增益。

cross_component_comparable 在每分量内以 frame_count>=2 定义，不表示分量之间的绝对零点可比；建议改成 within-component 语义，另记 external-anchor 的真实存在状态。S9/S13 失败关闭已是当前保守产品策略，输出孤立分量独立产品需额外裁决。

ACSD_SKY_P5_FAULT 和 ACSD_DRZ_P3_FAULT 两个环境变量能在生产编译产物重新启用已知坏实现，应移到只用于测试的编译车道，或正式发行对其 fail-closed 并记录 provenance。需要真正测试运行图，而非只用注释声称“默认零开销”。

真实 P5 边界原门只算114/196，另外82条多为取样偏移导致未纳入，需构造多偏移/法向多尺度扫描，与物理扩展源的真值对比，不能称全边界已达标。生产改动另行批准。

## Q06 五论文跨篇传播与最终交付（依赖项）

跨篇 P1→P2→P3→P4→P5 故障注入尚未真正运行。需要选同一固定物理估计目标、合法校准输入和可重建 HST+相机前向世界，逐项注入 P1 标度误差、P2 方差误差、P3 漏候选、P4 域外与 P5 错规范，独立验证下游确实判红。

每任务交付 task_id、base_sha、commit_sha、脚本、环境/命令、输入内容哈希、数据分母、统计 CI、负例与控制图、PASS/FAIL/NOT_RUN/UNVERIFIED 的准确状态、局限；禁止依赖只存在本机的 run 路径。最后把 ROUND3_HANDOFF_REPORT.md 放在实验/engineering-evidence/acsd-science-docs-p5/，不放仓库根。

## 范围与明确裁决

Q00–Q02 可直接执行（文档和证据工作）；Q03–Q05 先完成科学/工程设计及负例，再向负责人提交具体实现审查，不得自行大规模重写生产；Q06 是科学发布验收前置，不允许 Agent 自动宣告五篇已可发表。
