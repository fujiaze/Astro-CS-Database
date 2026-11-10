# ACSD V2 科学正确性与工程可靠性彻底重构｜总任务清单

> 状态：**任务规划与实施授权说明，不代表任一任务已完成。** 2026-11-10 基于 `rebuild/acsd-v2@27bd7925b6a94a0167faa079526d17fcac5b68eb` 调查。具体执行始终读取最新分支 HEAD，保留增量及旧失败记录。任务归属：科学基础、算法文献及研究；代码/软件架构；独立论证和实验。**不引入 CI、自动门禁、文档脚本验收、分支强制策略或测试覆盖率目标。**

## 0. 目标，不可替换

ACSD 是天文观测的 `Normalize → Mosaic → Export` 科学计算系统，不是 22 个 API 的示例集。V2 完工必须实现：
- **科学正确**：天空→光电子→ADU 的前向物理模型与反向估计目标清晰；对已支持模型有正确的数学推导、已知不确定度/确定数值界/不可辨识条件；真实仪器不满足假设时如实 conditional/unknown。
- **算法有来源**：每个生产算法或关键数值原语追溯到论文、标准、一手软件/研究成果，提供 DOI、公式/章节、适用条件、合法全文存档或稳定索引；新增推导明确作者、推导、原始实验、先前技术对比与失败样本。**不能用论文标题代替对公式的理解。**
- **代码实现正确**：源码执行与推导的算法步骤、前后条件、数据边、资源/失败状态一致；独立研究者可从文档复现；不可用旧源码当数学正本。
- **组合正确**：单位、WCS、response、PSF/PRF、shared noise、geometry revision、sky/selection/权重、输出含义跨进程保持；全链最终产物可逆向审计到原始观测。
- **工程可靠**：CPU/内存/I/O统一资源账本；有界 Task/Pipe 和异步 I/O；取消、写盘失败、低内存、跨进程恢复、Windows/Linux 和实际吞吐有可重复的现实证据。
- **设计与执行分权**：科学文档先定义被估计量与数学事实；工程文档定义接口/数据/运行时；复核文档定义证据。失败不能靠修改检查器或阈值变成成功。

## 1. 工作分组与逐项完成清单（每项须有实际文件/源码/证据）

### A. 起点、历史及论文
- [ ] **A0** [01 基线、边界和冲突清理](01-BASELINE-AND-PROTOCOL.md)：按最新 SHA 逐源文件核对已实现/未接入/未认证；统一状态，不复制历史 PASS。
- [ ] **A1** [02 原始文献与算法来源](science/02-LITERATURE-AND-PROVENANCE.md)：22 Process、全部关键公共数值方法逐项建立来源—公式—适用域—代码引用；索引/依法存档论文。
- [ ] **A2** [03 五研究问题与原始实验归档](science/03-R1-R5-RESEARCH-EXPERIMENTS.md)：R1–R5逐一验证研究报告、脚本、失败、独立性和创新边界。

### B. 科学设计和数值正确性
- [ ] **B0** [04 五份科学基础正向推导](science/04-FOUNDATIONS-AND-PROOFS.md)：从模型、公理/假设、目标量出发，逐式重推并消除矛盾；不看旧源码推公式。
- [ ] **B1** [05 共享严格数值与认证](science/05-NUMERICS-AND-CERTIFICATION.md)：先解决 interval P0 和 Linalg 证书问题，再认证依赖其上游输出。
- [ ] **B2** [06 Normalize 科学链](science/06-NORMALIZE-SCIENCE.md)：校准/WCS/Gaia/PSF/响应/测量/标度的可辨识性与误差闭合。
- [ ] **B3** [07 真球面几何与 Drizzle](science/07-SPHERICAL-GEOMETRY.md)：独立全域候选、面积区间、同 R、科学误差预算及可实施性能路线。
- [ ] **B4** [08 噪声、Sky 与 Coadd](science/08-STATISTICS-SKY-COADD.md)：共享误差、同数据校正、真实低频保护、BLUE/QP、选择后 coverage。
- [ ] **B5** [09 Export 与产品科学语义](science/09-EXPORT-SCIENCE.md)：支持面积、有效核、单 signal、FITS 外部解释和恢复后科学一致性。

### C. 工程实现、完整交付与独立审核
- [ ] **C0** [10 22 Process 逐项代码重构](engineering/10-ARCHITECTURE-PROCESS-MATRIX.md)：强类型唯一实现、57操作映射、每模块签署数学符合性与资源状态。
- [ ] **C1** [11 Runtime / Memory / Storage 可靠性](engineering/11-RUNTIME-RESOURCES-STORAGE.md)：所有权、账本、单 CPU 池、有界内存管道、异步I/O、故障恢复。
- [ ] **C2** [12 三工作流、平台和产品交付](engineering/12-WORKFLOWS-PRODUCT-PLATFORMS.md)：整链单实现、磁盘三命令、Windows/Linux、独立 GUI 与真实性能。
- [ ] **D0** [13 独立证明/反例/真实数据](review/13-INDEPENDENT-REVIEW-AND-EXPERIMENTS.md)：证据从数学命题出发；审查者不依赖生产 helper；反向审计最终产品。
- [ ] **D1** [14 集成顺序、责任和最终裁决](review/14-EXECUTION-AND-CLOSEOUT.md)：逐阶段交付、阻塞、科学/工程分别签核，最终审查无必需 P0/P1/未决能力。

## 2. 实际实施次序

`A0 → A1+A2 → B0 → B1`。B2/B3/B4/B5 在基础数学条件成立且数据合同冻结后可分写域推进；先审其推导，再实现相应 `C0` 模块。`C1` 可提前修复不改变科学模型的资源错误，但所有优化均受科学不变量约束。`B2+B3+B4+B5+C0+C1 → C2 → D0 → D1`。任意科学反例发现前提错误，回到归属科学正本，不绕开前提改判据。

## 3. 三层状态与裁决

每个成果分别标：`SCIENCE_DERIVED`（指定数学假设下已有推导），`ALGORITHM_SPECIFIED`（独立开发者能实现），`IMPLEMENTATION_REVIEWED`（逐源审查实际生产路径），`INDEPENDENTLY_EXERCISED`（指定输入的独立对照），`REAL_DATA_CONDITIONAL`（真实模型尚待验证），以及 `FAIL / BLOCKED / UNKNOWN`。这些是**人类阅读的论证结论**，不是新增程序枚举、标志或脚本规则。严格认证必须有被审查的实际有向算术实现；普通单测 PASS 不产生证明。

## 4. 管理禁令

1. **禁止 CI/CD、测试覆盖率、脚本化文档格式/目录检查、自动得分、自动 PASS 门禁成为项目审查条件。** 允许编译器、调试器、Sanitizer、一次性独立实验程序作为证据；其输出由人/独立 Agent 解释，不能替代证明。
2. 不为迁移旧 V1/过时 V2 格式编写双 reader、兼容层或自设版本协议；当前 API/产品维护一套正本。保留 Git 历史和有科学价值的原始失败证据。
3. 不自动关闭旧 Issue；旧 `#113–#122` 是历史子任务材料，本计划按真缺口重新组织。执行仍在 `rebuild/acsd-v2`，不碰 `main`、不强推、不创建 PR。
4. SCIENCE、ENGINEERING、REVIEW 的主文档单向归属；不要为了“追踪”把同一公式抄成多套权威文本。文献以论文原式为参照，ACSD 使用的变体必须独立推导；引用不能替代推导。
5. GPU 后端与 SACoadd 新型求解器都不是本轮必做；首期完成可信 CPU 传统 coadd，并留接口而非预装桩。
6. 原始真实观测 FITS 如有隐私/版权/体积限制不提交公开仓库，只记录私有数据清单/hash、模型域与可复现命令；研究小型 Oracle 可入库。
   
**执行入口**：从 [01](01-BASELINE-AND-PROTOCOL.md) 开始；科研原始证据的冻结入口为 [main@adc7f33d/实验/science-foundation](https://github.com/fujiaze/Astro-CS-Database/tree/adc7f33d58359ac3d65014f239d7f53fe92e4912/%E5%AE%9E%E9%AA%8C/science-foundation)。本计划只写任务书；其清单框本次均未以“编写任务书”而勾选完成。
