# 工程控制 / RELEASE-02 启动文档（00_README）

## 1. 目的

RELEASE-01 已把 R 通道两组真实数据全流程跑通并交付了两个成品帧，但视觉验收（接缝）与性能验收（资源门）判 FAIL，且 mosaic 以显式等权降级运行、未实现设计要求的逆方差叠加。负责人已裁决：

- **接缝必须修复，属发布阻断项**；
- 稀疏天光面、逐帧方差/逆方差链等新文档语义**属本预览版必须闭合的范围**，不带病发布；
- 两项冻结科学文档的 P0 订正**已获负责人批准**，按 ENGINEERING_SPEC §3 走变更 claim 落地；
- 版本号（`0.0.1alpha`）在全部验收通过后再改，本控制包不触碰版本号、不执行 FIN-001。

RELEASE-02 的目标：闭合 RELEASE-01 审计出的全部 P0 代码缺口与两项科学订正，使视觉门、性能门、机器门全部 PASS，重新交付两个 R 通道成品帧给负责人检查。

## 2. 权威来源与科学立场

- **权威来源**：负责人随本控制包提供的**最新文档包**（36 篇，已含 PSFSNR 公式订正、天光口径澄清、U-1/U-2 修复、两个检查器补登记）；
- RELEASE-01 的审计与报告是修复输入，不是权威：`工程控制/RELEASE-01/GAP_AUDIT.md`（P0-01..P0-18 清单）、`reports/RELEASE-01/`（science / tests / perf / vis 各报告）；
- **科学立场（最高优先级）**：设计意图在大纲里，但负责人提出的方法不一定对，科学文档也不一定对。修复过程中凡涉及天光面建模、逆方差/协方差传播、PSF 加权、插值与重采样的方法选择，必须对照真实论文与真实开源实现（SWarp、Siril、SExtractor/SEP、photutils、properimage、SCAMP 等，见 `docs/research/SNR_WEIGHT_RESEARCH_PACK.md`），依据充分才动手；发现文档方法有问题，走变更 claim 并在报告中给出推导与出处，不得盲从文档也不得盲从现有代码（AGENTS.md §8）。GPL 代码只读对照、不进仓库。

## 3. 文档包替换

- 负责人先将最新文档包 zip 解压替换仓库文档集；第 1 个任务 `DOC-101` 核验替换正确性（哈希清单、引用可达、治理痕迹清零）；
- 本次文档包相对 RELEASE-01 入库版的实质变更：
  1. `docs/plugins/algorithms_phase1/07_noise_snr.md`：PSFSNR 公式分子订正为 `(Σ_j f_j)²`（官方文档式[18]，和的平方）；新增通量型 `F_ref/σ_F` 与功率比型 `(Σf)²/σ_n²` 的口径澄清；天光措辞订正（信号项不被加性背景虚高，天光散粒噪声计入 σ_n）；c3/c4 补版本漂移注记；
  2. `docs/research/SNR_WEIGHT_RESEARCH_PACK.md`：同步公式订正；开源代码表许可证与源码行号订正（DeepSkyStacker = BSD-3-Clause、SExtractor = GPL-3.0、SEP = LGPL-3.0、properimage = BSD-3-Clause、SWarp/Siril/SCAMP = GPL-3.0）；COAAD I/II 书目订正并新增 ZOGY（ApJ 830,27）消歧条目；
  3. `docs/design/UNIFIED_MODEL.md`、`ASTROCS_DESIGN.md`：frame_snr 口径与天光措辞同步；
  4. `ENGINEERING_SPEC.md` §7：根固定条目补 `ACCEPTANCE_SPEC.md`（修复 U-1 / ENG-CONSTRAINTS 红）；
  5. `docs/ci/01_CHECKS.md` §2：补登记 `CHK-E2E-REPRO` 与 `CHK-EXIT-CONSISTENCY`（修复 U-2 / CHK-REGISTRY-DOC-SYNC 红；负责人已认可两个检查器的价值并批准保留）。

## 3a. 自主裁决授权（负责人明确授权，不等盯办）

负责人不逐项盯办。执行 agent 按"**发现问题 → 派子 agent 研究 → 订正文档 → 修改代码 → 补测试 → 独立验证 → 统一提交**"的闭环自行推进，一个问题走到闭环再进下一个，不把技术问题原样上抛。

**agent 可自行裁决并直接落地（事后在报告中留痕）**：

1. **科学方法问题**：发现科学文档/算法文档的公式、口径、推导有误或不完整，派子 agent 检索一手论文与开源实现（SWarp/Siril/SExtractor/SEP/photutils/properimage/SCAMP 等）、必要时做合成数据实验，给出出处与推导后，直接走 ENGINEERING_SPEC §3 变更 claim 订正文档并改代码，无需等待负责人批准；
2. **门禁/判据问题**：发现门禁判据本身不合理（判据与科学目标不符、阈值无依据、误报/漏报、fail-open），派子 agent 研究合理判据，**改进门禁本身**（同步更新 `docs/ci/`、`ci/checks.json` 与判据文档，配可执行正例/负例与 --self-test），而不是绕过门禁、删检查或挂 waiver；
3. **工程实现选型**：文档框架内的具体算法、数据结构、线程模型、缓存策略、插值/拟合方法、单位口径、模块 manifest 登记，由 agent 依据证据自决；
4. **P1/P2 差距与测试欠账**：随分片一并闭合，不等单独授权；
5. **开源对照**：需要数值对拍时可引入 BSD/MIT/LGPL 类工具做对照（GPL 代码只读不进仓库），对照过程与结论入报告。

**必须上呈负责人、不得自决的事项（穷尽列出）**：

- 发布/不发布、交付物是否合格的最终判定；
- 版本号与发布时间（本控制包明确不触碰）；
- 改变顶层合同：三命令（normalize/mosaic/export）划分、JSON 输入输出合同的结构性变更、HiPS 产品数据模型的破坏性变更；
- 许可证存疑事项（某代码/数据能否进入仓库无把握时）；
- 删除或覆盖 testdata、用户数据、交付物；
- 穷尽文献与实验后仍无法收敛、且存在互斥产品方向取舍的科学争议。

上呈时必须同时给出：已查证的证据、各方案代价、agent 的明确推荐——不把裸问题丢给负责人。

**每个自决闭环的留痕要求**：问题登记（GAP_AUDIT 增量）→ 研究证据（文献编号/代码文件:行/实验脚本与结果）→ 文档订正（变更 claim 编号 + diff 摘要）→ 代码提交 → 新增 Oracle/负例测试 → 前台独立复跑结果。缺证据的"我觉得"不算闭环。

## 4. 任务与执行入口

- 触发：负责人指示"执行 RELEASE-02"；
- 前台 = 执行 agent（调度 + 独立验证 + 统一提交）；SubAgent = 干活的手（**零 git 写权限**）；不同修复分片派不同次级 SubAgent 并行；
- 任务依赖见 `TASK_LIST.md`：`DOC-101` 最先；随后 `SCI-101` 与 `FIX-A..FIX-E` 并行；`TST-101` 紧随修复滚动进行；全部修复完成后 `BLD-101`；通过后 `E2E-102`；再 `VIS-102` 与 `PERF-102`；两者 PASS 后 `DEL-102` 交付；
- 每个任务独立验收，PASS 由前台独立复跑后写入 `ACCEPTANCE.md`，不复用 SubAgent 自述；不用 waiver 掩盖红灯；
- 本控制包**不含 FIN-001**：版本号、README 发布收尾、发布包等全部留待负责人检查成品帧并认可后，另开控制包执行。

## 5. 最终交付物

1. 闭合后的代码（P0-02..P0-14 逐条有提交与证据），机器门全绿；
2. 两项冻结科学文档的变更 claim 记录 + 一致性回归证据；
3. 补齐后的测试（负例面、零测试模块、fixture 解锁）与全量 ctest 结果；
4. 重跑的 L3/L4 日志、1/N worker 一致性证据、性能计时与优化对比、视觉验收图（整幅 + 4×4 分块 + 可疑区域裁剪放大）；
5. 两个重新交付的 R 通道平面 FITS（M42 与 Galaxy Center），**只交付负责人检查**；
6. 填写完整的 `ACCEPTANCE.md` 与 `SUMMARY.md`。

## 6. 环境前置

- L4 全量 R 通道中间产物峰值约 70 GB，运行前工作盘可用空间 ≥ 100 GiB（脚本内置 df 余量闸 <30 GiB ABORT）；
- 本机 `/tmp` 为悬空挂载点，临时目录用 `TMPDIR=/dev/shm/astrocs_tmp` 或源码树外可写目录；
- 真实 HiPS fixture 相关的 6 个用例在 P0-03 修复后应自动解锁，TST-101 负责复验。
