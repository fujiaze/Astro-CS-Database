# 审稿-P1 · EXP-engineering-evidence-001 · G08-05 对抗审稿第 1 遍

- 片号：`EXP-engineering-evidence-001`
- 层：`实验/engineering-evidence`
- 基线 HEAD：`f9650dd0aed97d7f261e5e6547f4fdb505bc313b`（任务给定值已实测一致）
- 清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2330-2417`
- ⛔ 全程未读 `/tmp/acsd_g08/`；零 git 写（未 add/commit/checkout/reset/stash/rm --cached）；未编译、未跑 ctest/pytest/构建/实验脚本；未改任何仓内既有文件。

---

## 1. 读完了吗

**结论：未达到"一遍 = 对同一片材料的一次完整重读"的完整标准。个人完整读完 22/80 份；余 58 份由 5 个子代理逐行读完后经我抽验复核。**

| 项 | 数值 | 说明 |
|---|---|---|
| 成员份数（权威版清单） | 80 | 全部实测存在，0 缺失 |
| 清单声明行数 / 实测行数 | 8403 / **8412** | 差 +9（`wc -l` 与 `sum(1 for _ in f)` 对末行无换行文件的计数差） |
| **本人完整 read 份数** | **22 / 80（27.5%）** | 逐行 read，非脚本扫描 |
| **本人完整 read 行数** | **4811 / 8412（57.2%）** | 见 §3 逐文件清单的"读了多少行"列 |
| 子代理完整 read 份数（并集） | 80 / 80（100%） | QA-A×2 / QA-B×2 / QA-C / QA-D / QA-E 共 7 个子代理（5 条独立车道） |
| 本人对子代理结论的独立复核 | 见 §7 | 凡阻断级结论均由我亲自 grep / 只读 git 复验 |

### 未读完的部分（如实列出）

以下 **58 份**我**未逐行亲自读完**，依赖子代理报告 + 我的抽验：

- `audit-2026-01/FIX_LEDGER.csv` 的**后 759 行**（我读完前 26 行全文，并对全部 785 行做了列级字段提取与路径存在性复算）
- `compress-01/code/` 18 个脚本中的 17 个：`fits_bench.c`、`analyze.py`、`zstd_bench.c`、`extract_corpus.py`、`product_level.py`、`fits_loss.c`、`rt_diff.c`、`fill_scan.py`、`trim_scan.py`、`diag_cfitsio.c`、`py_bench.py`、`run_bench.sh`、`run_bench2.sh`、`make_filelist.py`、`run_bench3.sh`、`disk_bench.sh`、`build.sh`
- `v19r7-quality/` 18 份：`audit_findings_{science,standards,architecture,io,drizzle,phase2}.md` + 6 份 `EVIDENCE_INDEX.md` + 6 份 TASK/REVIEW/TEST 报告
- `audit-2026-01/OWNER_DECISIONS.md`
- `release-05/` 4 份：`FAILCLOSED_SURVEY.md`、`AUDIT_PACKAGE.md`、`D-14_STALE_PATH_CLEANUP.md`、`G2-9_CI_INCREMENTAL_STATUS.md`
- `v6/` 15 份：`review-audit/{00,01,02,03,04,05,06}`、`science-adjudication/SUMMARY.md`、`contract-review/{03,04}`、`release-review/01`、`real-science/REPRODUCE.md`
- `reaudit-v3/v3_exec/{CON010,CON006,CON009}` 3 份
- `release-01/` 3 份：`science/exp_math.py`、`VIS-001/l4/profile_and_crop.py`、`VIS-001/l4/profile2.py`

**这一条本身是本片的交付缺陷**：本片有 9 条阻断级发现集中在我未逐行读完的 58 份里（尤其 compress-01 的 17 个脚本、v19r7 的 18 份、release-05 的 4 份）。**建议对本片再补一遍由本人主读的完整重读**，尤其是 `compress-01/code/analyze.py` 与 `v19r7-quality/evidence/`。

---

## 2. 本片判定

### 判定：**阻断**（9 条阻断 · 11 条须修 · 6 条建议）

本片是「工程实测留档」层。判定阻断的三条最重：

**① 归档的自校验清单在本片干净 HEAD 上失败，唯一数字证据面不在清单内。**
`compress-01/MANIFEST.sha256` 自称"覆盖本目录下全部归档文件（不含本清单自身）"，实测 `sha256sum -c` **2/25 失败**，且被 `COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md` 之外的唯一权威数字源 `final_numbers.json` 双双落榜。这两个文件与其清单是**同一提交 `4b0308b4` 一次落盘**、工作树干净 ⇒ **清单在入库那一刻就是错的**，即"同一提交内必须同时更新判据与其唯一证据面"被违反。（`MANIFEST.sha256:3` 的校验路径 `cd artifacts/evidence/compress-01` 也是迁移前的旧路径。）

**② 本片自称"机器生成的原始数字表"的 810 行表，被合成载荷污染，最高差 226×，且污染方向单向有利于被推荐方案。**
`code/analyze.py` 是**唯一不排除 `.dense.bin` 的汇总脚本**（`grep -n 'dense' code/analyze.py` → 零命中），而 `verify/final_numbers.py`(5 处)、`product_level.py`(2)、`py_bench.py`(2) 全部排除。`extract_corpus.py:51` 把 `<tag>.dense.bin` 写进与真实瓦片**同一目录**，`run_bench.sh:9` 却宣称"188 瓦片"而实跑 `data/*.bin`。审阅者能在 md 里逐格看到的数字，一半是"只保留 finite 且非零的载荷"的变长合成体。

**③ 本片的核心判据资产（`v6/qa-design/oracle/`）在结构上不可能因任何真实缺陷翻红，且机械断言一条已被生产退役的事实。**
35 个 check 中 **27 个实测值恒为 0 或 1**（`evidence/oracle_baseline.json` 直接可证）；全分片 **零处打开生产源码**；`validate_spec.py:157` 强制 `production_modes == {point_information, surface_gls, psfsw_robust}`，而 `lib/algorithms/integration/phase1_product/include/acsd/phase1_product.h:31-32` 逐字写「`FZ-MODE-PRODUCTION {point_information, surface_gls}`（原集合里的 psfsw_robust 已退役）」⇒ **把规格订正成真相会使门变红**。这是本片最危险的一条：它不是判据偏弱，是判据在阻止真相入库。

---

## 3. 逐文件清单（22 份本人完整 read）

| # | 文件 | 读了多少行 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|---|
| 1 | `compress-01/COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md` | 810 | 6 节全表 | `:3` 上游写 `ASTROCS_DESIGN.md §8.3（异步与压缩）、§9（内存极简化）`——该文件不存在；`docs/ACSD_DESIGN.md:469` §8.3 实为「三个阶段调度器」。`:4` 声称由 `analyze.py` 生成但该脚本 `dense` 零命中。`:217` 整 .fits n=152 与 `:11` 数据段 n=7、`:651` python 侧 leaf n=24 三套语料不可比。`:434-474` RICE_1_q0 整族 `FAIL(rc=413)` 41 行。`ratio(数据段)` 实为 `comp/1MiB` 而非比值（`:239` 305280/1048576=0.2911↔印 0.2911）。 | 需修 |
| 2 | `audit-2026-01/FIX_LEDGER.csv` | 786（前 26 行全文 + 全 785 行列级提取） | 表头/前 26 行长记录/全量 `fix_state`/`verified_state`/路径存在性 | **去重文件路径引用 1308 个，仅 28 个存在 → 97.9% 悬空**。`fix_state`: OPEN 754 / FIXED 26 / PARTIAL 4 / IN_PROGRESS 1。6 个 `fix_commit` 全是 HEAD 祖先（真），但 **26 条 FIXED 中 6 条 `fix_commit` 为空**且 `fix_note` 自认"待前台提交回填"从未回填。`evidence_file` 785 行全指向已删的 `问题扫描/`。 | 须修 |
| 3 | `v6/qa-design/oracle/qa_oracle.py` | 576 | 35 个 `@check` 全文 | `:137` 与 `:141` 是**逐字符相同的表达式** `a*a*float(P@P)/(sg*sg)`。`:253-255` 两项皆 `x/x-1`。`:245-246` `Bj=np.array([B0])` 被**构造成**答案必为 B0。`:401` `Wtrue=0.79845` 硬编码常量。`:446-451` `bias=0.0` 字面量。`:475-480` `CHK-BASE-05` 无任何 `B.get` ⇒ 结构性恒绿。`:570` 全文件唯一 `open()` 是写自己的 JSON。 | 阻断 |
| 4 | `v6/qa-design/oracle/validate_spec.py` | 304 | 43 条结构规则 | `:157-160` 强制三生产模式含已退役的 `psfsw_robust`。`:69-74` 注释宣称的 `units=={}` 豁免是**死分支**（`pass` 后无条件 `out.append`）。`:26-29` `ANCHOR_RE` 含 `K[0-9]|M[0-9]`，任意含「M1」的散文即可满足 frozen 锚。`:44-46` 账本缺失即**静默跳过**全部零用例规则。`:212-237` 账本规则在 `run_all.py` 首跑不带 `--ledger` 时完全不执行。 | 阻断 |
| 5 | `v6/qa-design/SUMMARY.md` | 86 | 全文 | `:13-18` 列出的 6 份 `docs/validation/v6/*.md` 交付物**全部不存在**。`:34` "独立 Oracle 35/35 PASS" 未披露 7/44 门无可执行 check。`:37` "人读/机读逐字一致" 对当前树不可复现。`:6,78` 基线 HEAD 过期。`:40` "run_all.py rc_total=0" 在 HEAD 已不成立（`run_mutations.py:54` open 的是 `实验/docs/validation/v6/QA_MATRIX.md`，该路径由 `:19` 三层 dirname 算出，不存在）。 | 阻断 |
| 6 | `v6/qa-design/oracle/run_all.py` | 115 | 7 步编排 | `:91` 首跑 `validate_spec` 不带 `--ledger` ⇒ 账本规则整块跳过。`:92` `--write` 紧接 `:93` `check_docs` ⇒ **自愈**（写者=校验者）。`:18` `P0_EXEC` 硬编码 6 值**逐个等于** `min_cases`。`:62` G-RD-03 `executed_cases=8` 硬编码，`:65` note 声称"testdata/index.json 八个数据集逐一存在性"而全分片**零次读 testdata**。`:33-40` 账本由 oracle 自己的绿 check 计数签发。 | 阻断 |
| 7 | `v6/qa-design/oracle/run_mutations.py` | 101 | 三类驱动 | `:17` `import qa_oracle`，`:28-37` 从 `qa_oracle.MUT_BUGS` 取 flag、调 `qa_oracle.run()` 判定检出 ⇒ **被测对象 = 判据本身**。`:53-54` 直接 `open(DOCS/QA_MATRIX.md)`，HEAD 上必抛 `FileNotFoundError`（无 try）。`:12` `copy` 死导入。 | 阻断 |
| 8 | `v6/qa-design/oracle/check_docs.py` | 80 | 一致性门 | `:36-41` 拿 `R.render_gate_table(qm)` 与由同一 `qa_matrix.json` 渲染出的文件逐字比 ⇒ 往返自证。`:13` `NEG` 死常量（唯一出现处即定义处）。`:52-55` 禁止项扫描只匹配首列为 `psf_snr_power` 的表格行。 | 阻断 |
| 9 | `v6/qa-design/oracle/render_docs.py` | 167 | 渲染器 | `:152` `makedirs(exist_ok=True)` **凭空创建**被判定的文档目录 ⇒ "文档缺失"结构性不可检出。`:46` 向人读文档渲染 `executed=0 或 skip-only -> rc=2`，而 `run_all.py:105` 的 `rc_total` 只取 0/1，代码从不产生该 rc。`:13-14` 含日期化历史叙事（违 AGENTS.md §5）。`:39-84` 渲染的**全是规格元数据**，无任何实测值/判定/rc 列 ⇒ 选择性报告。 | 阻断 |
| 10 | `v6/qa-design/oracle/assemble.py` | 68 | 组装器 | `:5` 声明「退出码 0/2」，`:68` 恒 `sys.exit(0)`。全程零校验。`:32` 未知 lit key 静默丢弃。`:20-61` 每次运行覆写入库的 `qa_matrix.json`。 | 须修 |
| 11 | `实验/engineering-evidence/README.md` | 40 | 全文 | `:32-36` "机器消费锚（迁后改接）"5 条，**5 条全部断裂**：`eng/ci/check_frozen_gate.py`/`check_worker_balance.py`（`eng/ci/` 整目录不存在）、`docs/validation/v6/`（不存在）、`docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md`（实为 `docs/engineering/…`）、`docs/science/NOISE_MODEL.md` 引 `exp_math.py`（`grep exp_math docs/science/NOISE_MODEL.md` → **零命中**）。`:4,15,40` 含日期化历史叙事。 | 须修 |
| 12 | `compress-01/code/acc_roundtrip.py` | 193 | 全文 | `:44-46` 负例扰动的是被比较的第三个数组 `acc3`，**不是被测对象 `np.add.at`** ⇒ `red` 被 `ok` 蕴含，`:71` `assert ok and red` 退化为 `assert ok`（恒真门）。`:150-157` 只计时，**解压结果丢弃，全程无往返校验**。`:4` 口径来源 `run/MEM-DESIGN-01/verify/compress_faithful.py` 不存在。`:24` 默认语料 `run/PERF-401/out/real16_w1` 不存在。`:165-181` 稠密/稀疏两行**加权基准不同**却并排。`:33-43` 的正向自检（`np.add.at` vs 顺序循环 + 负例）是**本片唯一做对的双向体检**，应予肯定。 | 阻断 |
| 13 | `compress-01/verify/final_numbers.py` | 117 | 全文 | `:102` `if rows and "bitwise" in rows[0]` —— CSV 列名实为 **`bitwise_eq`**（`code/fits_bench.c:58`），dict 键精确匹配 ⇒ **恒 False**，`:103` 同错名会被打开即 `KeyError`。⇒ TRUTHFUL-CONCLUSION-01 的整改**从未生效**且被伪装成"fail-closed 不猜数"（`:109`）。`:44` `comp/FITS_BYTES*(FITS_BYTES/NPIX)` 代数恒等于 `comp/NPIX`，`FITS_BYTES` 是死常量。`:7` `EV="run/COMPRESS-01/evidence"` 与依赖的 `zstd_tiles_shuf0/4.csv`、`fits_codecs.csv` 全不入库 ⇒ 归档不可重生成。`:10` `BW_W,BW_R=419,359` 是硬编码本机常量且无归档产物。`:87-90` 有 TRUTHFUL-CONCLUSION-01 整改说明（应予肯定，但如上所述整改本身是坏的）。 | 阻断 |
| 14 | `audit-2026-01/README.md` | 16 | 全文 | `:11` 计数（785 / P0 93 / P1 449 / P2 240 / P? 3）经我独立复算**逐条为真**。`:15-16` 明文"历史审计证据，不是现行整改依据"——**这句是本目录最重要的诚实性锚**。但 `:15` `docs/KNOWN_LIMITATIONS.md` 与 `:16` `工程控制/RELEASE-04/` **两条"现行"指针全部悬空**（前者已迁 `artifacts/evidence/known-limitations-ledger/LIMITATIONS.md`，后者整目录不存在）。`:16` `git log -- 问题扫描/` = 193 commits，指针真实可用。 | 须修 |
| 15 | `audit-2026-01/ROOT_CAUSES.md` | 211 | 11 簇 + 残余风险表 + 机制⑪-⑭ | `:3` 把已删的 `问题扫描/findings/` 声明为"**唯一权威**"，无退役限定语。`:5` 指向 `40_OWNER_DECISIONS.md`（现名 `OWNER_DECISIONS.md`）。`:4` `node 问题扫描/_tools/verify_anchors.js` 复跑命令不可执行。`:4` 计数 68+189+76=333 自洽。`:202` 时点 `d7f46dd0` 合规声明做得规范（应予肯定）。`:171` 日期化历史叙事。`:40` 公开记录自身归因撤回——**诚实性样本**。 | 须修 |
| 16 | `v19r7-quality/audit_findings.md` | 232 | 全文 | `:6,29,208-216,231` 全指向不存在的 `reports/v19r7_quality/`。`:54` SC-01 以 `CALIBRATION.md §失效条件"flat_norm=0→显式拒绝"` 为 P0 判据——我实测该句在现行 `docs/science/CALIBRATION.md` **全文不存在**（现行 `:78` 是"median<=0 保持原样不归一（避免除零）"、`:97` 是 `flat_norm = max(flat/median(flat), 0.1)`）⇒ **幽灵 P0**，`can_enter_B2=false` 失去依据。`:17` "9 checks 0 broken" 已被 `audit_stats.json` 订正为 12，md 未同步（裂脑）。`:16,25,44,46` 的 64/4/24/36 计数我独立复算**为真**（应予肯定）。 | 阻断 |
| 17 | `v19r7-quality/audit_findings_noise.md` | 87 | 全文 | `:62` "按文档 NOISE_MODEL 7 + UNCERTAINTY 3 + 跨域 1 = 11"，但 `:60` 声明合计 **10** ⇒ 求和 ≠ 声明。`:63` "按归口 B1-05 6 + B1-06 3 + B5-06/A1-02 2 = 11"，同矛盾。`:46` NO-07（P2）写"**P0 已修**"，而本表唯一 P0 是 NO-01 ⇒ 无源状态声明。全部 `lib/snr_estimator/**` 路径已迁死。 | 须修 |
| 18 | `v6/real-science/REAL-SCIENCE-001_REPORT.md` | 297 | 全文 | `:296` "所有数值来自真实运行并可经 §11 命令复现" —— §11 的 **7 条命令路径实测 7/7 全部不存在**（`run/v6/real-science/*`、`artifacts/v6/real-science/`、`reports/v6/real-science/`、`cli/v6_runtime_contract.h`）。`:6` CLI 版本串 `0.11.0-alpha.2` 与仓内 `VERSION`（`0.1.0-alpha.1`）不符。`:174` "**本项目未发现实现缺陷**"是全局断言，而 `:96-102` 登记了 5 项未测项（含"真实 FITS 产品链未运行"）。`:165` Oracle 第 3 项交叉校验"R C_in Rᵀ 复现 1/ΣW：R_k=W_k/ΣW、C_in=diag(1/W_k)"——**把 C_in 假定为权的倒数，该式成为构造性恒等式**，是对"独立交叉校验"的伪称。`:185-191` 如实登记真实星负 PSF 样本与处置（应予肯定）。 | 阻断 |
| 19 | `v6/performance/PERF-SCALE-001.md` | 276 | 全文 | 引用面全灭（`eng/tools/v6/*`、`artifacts/v6/performance/*`、`runtime/v6_budget.py`、`cli/commands.cpp` 实测全缺）。`:198` "23/23 生产三模式放行"已被上游退役推翻。**但 `:195` 如实登记"B: OMP_NUM_THREADS 注入未限制成功 → 非检出"、`:91-92` 如实登记 `queue_depth` 与 `worker_balance` 是配置回填值非观测、`:164` 如实登记 16w CPU 65.1% < 85% 门限并只记录不裁决**——这三处是本片最好的双向体检与选择性报告反面教材，应予肯定。 | 须修 |
| 20 | `release-05/GATE-501_VERIFICATION.md` | 73 | 全文 | §1 十门的宿主脚本 `eng/ci/*` 全部不存在 ⇒ 10 条验收门不可复现。`:4` 称判据单一实现点 `eng/ci/monitor_evidence.py`，而 FAILCLOSED_SURVEY 称 `eng/ci/run_checks.py::evidence_verdict`——**两文档对同一判据给出两个实现点**。`:52-55` 如实登记 `utilization_pct` 生产侧恒 50.0 的缺陷且未修。`:59-61` 如实登记仅 22/78 有自测。`:65` `--check@@` 文本损坏。**`:11` 的逐字数字引用经独立复算为真**（`run/ci/l2-frozen-gate/replay.json` `real16_w16_gate.json` avg=0.2453/p50=0.0388/frac=0.0693/lowwin=142.05，`evidence_count=11`、`now_red=9` 逐字相符）——本片唯一经机器证据坐实的引用。 | 阻断 |
| 21 | `l2_performance/README.md` | 106 | 全文 | `:63-65` 判 bitwise **PASS**、`real_differences = {}`、"唯一被点名的非科学差异是 `p2_samples.json`"；而同目录 `real16_bitwise_bitwise.json` 判 **FAIL**（16 项 `non_fits_diff`）、`real16_bitwise_cmp/compare_report.json` 判 **DIFFER**（`differing=14`，含 5 个 `p2_*.json` Phase2 科学产物）⇒ **同一比对三份归档三个判决**，且 README 隐去 5 个科学产物差异。`:84` 复现命令依赖仓外 `/tmp/astrocs_build.lock`。`:72-78` 如实登记 `worker_balance.csv` 的 `utilization_pct` 恒 50.00 退化（应予肯定）。 | 阻断 |
| 22 | `l2_performance/real16_bitwise_cmp/compare_summary.md` | 70 | 全文 | `:14` 公有 3093，但 `:15-20` 逐字节一致 3077 + 仅墙钟 4 + 容差 0 + 实质差异 14 = **3095** ≠ 3093（`differences` 前两项 `missing_in_b`/`extra_in_b` 不在 common 内却并入 `differing:14`）⇒ 口径混用。`:38-51` 14 项实质差异含 5 个 Phase2 科学产物。`:3-4` A/B 路径硬编码仓内绝对路径 + 已 gitignore 工作域。`:53-59` 容差依据推导写得规范（应予肯定）。 | 须修 |

---

## 4. 发现清单

### 🔴 阻断（9）

| ID | 位置 | 现状 | 应为 | 证据 | 验证 |
|---|---|---|---|---|---|
| **B-01** | `compress-01/MANIFEST.sha256` | 自称"覆盖本目录下全部归档文件"，实测 `sha256sum -c` **2/25 失败**；`COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md` 完全不在清单内；`:3` 校验路径是迁移前旧址 `artifacts/evidence/compress-01` | 清单覆盖全部归档件（含 md）且 `sha256sum -c` 全绿；校验路径改现址 | 实测 `337ec132…` vs 清单 `03f9cbb0…`；`3f684bb7…` vs `c8f2e582…`；三件同属提交 `4b0308b4`、工作树干净 | **本人** |
| **B-02** | `compress-01/code/analyze.py`；`COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md:7-223` | `analyze.py` 是唯一不排除 `.dense.bin` 的汇总脚本（`grep -n 'dense' code/analyze.py` 零命中），而 `final_numbers.py`(5)/`product_level.py`(2)/`py_bench.py`(2) 全排除；`extract_corpus.py:51` 把 dense 载荷写进同一目录，`run_bench.sh:9` 宣称"188 瓦片"却实跑 `data/*.bin` | `analyze.py` 加 `".dense." not in r["file"]`；dense 载荷另立一节；重生成并纳入 MANIFEST | md 的 `n` 值（2,4,4,6,6,6,6,6,6,48）= 真实+dense 逐项吻合；`statistics.median` 对 2 值取平均，`signal\|hier0\|zstd−7=0.5037` 反解 dense≈1.0049 | **本人 + QA-B** |
| **B-03** | `compress-01/verify/final_numbers.py:102-103` | 查 `"bitwise" in rows[0]`，CSV 列名实为 `bitwise_eq`（`code/fits_bench.c:58`）⇒ 恒 False ⇒ **恒红门**；`:103` 同错名被打开即 `KeyError` | 改 `"bitwise_eq" in rows[0]` + `r["bitwise_eq"]`，并重跑归档 | 归档 `final_numbers.json` 仍是旧硬编值 `20/103/188` 且 `bitwise_unavailable_reason=None` ⇒ 新代码从未跑过 | **本人** |
| **B-04** | `v6/qa-design/oracle/` 全目录 | 35 个 check 中 **27 个实测恒 0/1**；`qa_oracle.py` 全文件零处打开生产源码；`validate_spec.py:157` 强制三生产模式含已退役的 `psfsw_robust` ⇒ **订正成真相会使门变红** | 判据读生产产物；`production_modes` 与 `phase1_product.h:31` 对齐 | `oracle_baseline.json` 27/35 退化；`qa_oracle.py:570` 唯一 `open()` 是写；`phase1_product.h:31-32` 逐字 | **本人 + QA-A×2** |
| **B-05** | `audit-2026-01/FIX_LEDGER.csv` | **去重文件路径引用 1308 个，仅 28 个存在 → 97.9% 悬空**；`evidence_file` 785 行全指向已删的 `问题扫描/` | 台账的证据面必须可打开，否则 785 条整改无法复核 | `test -e` 逐条复算；最高频悬空路径 `ci/checks.json`(74 次)、`docs/contracts/DATA_SEMANTICS.md`(14)、`tests/unit/CMakeLists.txt`(13) | **本人** |
| **B-06** | `v6/real-science/REAL-SCIENCE-001_REPORT.md:296`；`v6/performance/PERF-SCALE-001.md:198`；`v6/qa-design/` `meta.json` | 声称"可经 §11 命令复现"，但 **7/7 路径实测不存在**；"23/23 生产三模式放行"与三模式架构的 PASS 结论已被上游推翻 | 复现命令可用，或改标退役时点 | `run/v6/real-science/*`、`artifacts/v6/real-science`、`reports/v6/real-science`、`cli/v6_runtime_contract.h` 全 MISS；`fc34a9cf`(2026-09-23, HEAD 祖先)「权重收敛为单一逆方差路径，删除全部模式选择机制」；HEAD `runtime_contract.h:113-114` 对 `psfsw_robust` 返回 `FZ-MODE-RETIRED` | **本人** |
| **B-07** | `l2_performance/README.md:63-65` | 同一次 1-worker vs 16-worker 逐位比对，三份归档给出**三个判决**：`verdict.json`=PASS(`real_differences={}`)、`real16_bitwise_bitwise.json`=FAIL(16 项)、`compare_report.json`=DIFFER(14 项，含 5 个 Phase2 科学产物)；README 只点名 `p2_samples.json` 一个 | 三份归档同源同步；README 必须披露 `compare_summary.md` 的 DIFFER 与 5 个科学产物差异 | 三份 JSON 同目录同 tag，解析实测 | **本人** |
| **B-08** | `compress-01/code/acc_roundtrip.py:44-46,71` | 负例扰动的是被比较的第三个数组而非被测对象 `np.add.at` ⇒ `red` 被 `ok` 蕴含，`assert ok and red` 退化为 `assert ok`（恒真门）；`:150-157` 解压结果丢弃，全程无往返校验 | 注入到 `np.add.at` 调用本身，使 `ok` 同步转红 | 静态推导：no-op 替换下 `ok=False`（缺陷被抓住）而 `red` 仍 True（"注入缺陷后判红"照样响）⇒ 无鉴别力。`docs/engineering/COMPRESSION_CODEC_RESEARCH_PACK.md:544` 与 §6 末「没有一条恒真门」随之不成立 | **本人读 + QA-B 推导** |
| **B-09** | `实验/engineering-evidence/README.md:32-36` | 5 条"机器消费锚（迁后改接）"**5 条全断** | 消费锚要么补齐要么删 | `eng/ci/` 整目录不存在；`docs/validation/v6/` 不存在；`docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md` 实为 `docs/engineering/`；`grep exp_math docs/science/NOISE_MODEL.md` 零命中 | **本人** |

### 🟠 须修（11）

| ID | 位置 | 现状 → 应为 | 证据 |
|---|---|---|---|
| M-01 | `COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md:3` | 上游写 `ASTROCS_DESIGN.md §8.3（异步与压缩）`；该文件不存在，`docs/ACSD_DESIGN.md:469` §8.3 实为「三个阶段调度器」→ 改引 `ACSD_DESIGN.md` 正确节号 | `ls docs/ASTROCS_DESIGN.md` MISS |
| M-02 | `FIX_LEDGER.csv` L3/L8/L36/L39/L44/L316 | 26 条 FIXED 中 **6 条 `fix_commit` 为空**（其中 5 条 P0），`fix_note` 自认"待前台提交回填"从未回填 → 回填 SHA 或降级 `PARTIAL` | `csv.DictReader` 全量复算 |
| M-03 | `FIX_LEDGER.csv` 6 个 `fix_commit` | 全部为 HEAD 祖先（真）→ 但其余 97.9% 锚点悬空，须补基线 sha 定位表 | `git merge-base --is-ancestor` 逐条 YES |
| M-04 | `audit-2026-01/README.md:15-16` | 两条"**现行**"指针全悬空（`docs/KNOWN_LIMITATIONS.md` 已迁 `artifacts/evidence/known-limitations-ledger/LIMITATIONS.md`；`工程控制/RELEASE-04/` 整目录不存在）→ 改指现址 | `test -e` 实测 |
| M-05 | `audit-2026-01/ROOT_CAUSES.md:3,5` | `:3` 把已删 `问题扫描/findings/` 声明为"唯一权威"无退役限定；`:5` 指向旧文件名 `40_OWNER_DECISIONS.md` → 加退役限定语并更新文件名 | `ls 问题扫描` MISS |
| M-06 | `v19r7-quality/audit_findings.md:54` | SC-01 P0 的判据"文档约定 flat_norm=0→显式拒绝"在现行 `docs/science/CALIBRATION.md` 全文不存在 → 幽灵 P0，`can_enter_B2=false` 失去依据 | 我实测 `grep flat_norm\|显式拒绝\|除零 docs/science/CALIBRATION.md` 12 处，无该句 |
| M-07 | `v19r7-quality/audit_findings_noise.md:62-63` | "按文档 7+3+1=11"、"按归口 6+3+2=11"，而 `:60` 声明合计 **10** → 计数自相矛盾 | 求和复核 |
| M-08 | `v19r7-quality/audit_findings_noise.md:46` | NO-07（P2）写"**P0 已修**"，本表唯一 P0 是 NO-01 → 无源状态声明 | 同文 `:57` P0 清单 |
| M-09 | `release-05/GATE-501_VERIFICATION.md:4` vs `FAILCLOSED_SURVEY.md:4` | 对同一判据函数给出**两个**实现点（`monitor_evidence.py` vs `run_checks.py::evidence_verdict`） → 须先定位真源 | 两文件互指 |
| M-10 | `release-05/GATE-501_VERIFICATION.md:65`；`D-14_STALE_PATH_CLEANUP.md:45`；`G2-9_CI_INCREMENTAL_STATUS.md:9` | 三处 `@@` 文本损坏（`"` 被系统性替换成 `@@`） | `grep -rn '@@' release-05/*.md` → 恰好 3 行 |
| M-11 | `l2_performance/real16_bitwise_cmp/compare_summary.md:14-20` | 公有 3093 ≠ 3077+4+0+14=3095（两项 `missing_in_b`/`extra_in_b` 不在 common 内却并入 differing） → 口径分列 | `compare_report.json` counts 实测 |

### 🟡 建议（6）

1. `compress-01/code/` 6 条 stale path（`run/COMPRESS-01/`、`run/PERF-401/out/real16_w1`、`run/MEM-DESIGN-01/verify/compress_faithful.py` 等），`§7` 重建流程第 1 步即断链 → 登记 UNRESOLVED 或补可入库小语料。
2. `analyze.py:51` 算出的 `ok`（zstd_bench 每档 `memcmp` 逐位校验）**算了不用**，md 既不显示也不据此过滤 → 与 pack §1.4「每档做完整往返 + memcmp 校验」直接冲突。
3. `run_bench.sh:30` default-tile 跑批**删掉 GZIP_2**（它在 tile=512 节有 41 行、default 节零行），而 GZIP_2 的 `ivar|leaf`=0.1703 是全部 FITS 臂最好的 ⇒ 选择性删臂，使读者高估 zstd 优势。
4. `build.sh:7-9` 只构建 2/5 个 C 程序，`rt_diff`/`fits_loss`/`diag_cfitsio` 无构建路径，而 pack §7 步骤 5 正是调 `./bin/rt_diff`。
5. `final_numbers.py:10` `BW_W,BW_R=419,359` 硬编码本机常量，`disk_bench.sh` 只 print 不落盘 ⇒ 无溯源。
6. `COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md:217/:651` 与 `:11` 三套语料 n 互不相等（152 / 7 / 24），表间不可比，须在表头标明各自语料。

---

## 5. 你主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 | 依据 |
|---|---|---|---|---|
| E1 | 对 `MANIFEST.sha256` 跑 `sha256sum -c` 并比对清单条目数与目录实际文件数 | 归档自校验是否通过 | **推翻成功** | 2/25 失败；清单 24 条 vs 目录 25 件（md 不在清单内）；三件同属 `4b0308b4`、工作树干净 ⇒ 入库即错 |
| E2 | `grep -n 'dense'` 逐个 compress-01 汇总脚本 | `analyze.py` 生成的 md 是否与归档 JSON 同源 | **推翻成功** | `analyze.py` 零命中；其余三个消费者全排除 ⇒ md 是唯一离群者 |
| E3 | 由 md 的 `signal\|hier0\|zstd−7=0.5037`（n=2）反解 dense 载荷比率 | md 的 n 是否 = 真实+dense | **推翻成功** | `median` 对 2 值取平均 ⇒ dense = 2×0.5037 − 0.0025 ≈ 1.0049，与"dense 是散乱有限值、不可压"一致 |
| E4 | 用 `fits_bench.c:58` 的实际 CSV 列名去检验 `final_numbers.py:102` 的条件 | 该门是恒红还是恒绿 | **推翻成功（判为恒红）** | 列名 `bitwise_eq` ≠ `bitwise`；归档 JSON 仍是旧硬编值 + `reason=None` ⇒ 新代码从未跑过 |
| E5 | 把 `np.add.at` 替换为 no-op，看 `red` 是否转 False | `acc_roundtrip.py` 的负例是否有鉴别力 | **推翻成功** | `ok=False`（缺陷被抓）而 `red` 仍 True ⇒ 负例无法区分"正确"与"彻底坏掉" |
| E6 | 逐字对照 `qa_oracle.py:137` 与 `:141` | CHK-ANA-05 两侧是否同一表达式 | **推翻成功** | 两行除变量名外完全相同 |
| E7 | 检查 `CHK-ANA-11F` 的函数体是否引用 `B` | 该门是否结构性不可红 | **推翻成功** | 零 `B.get` ⇒ 任何 mutation 都不改变输出 |
| E8 | 把 `qa_matrix.json` 的 `production_modes` 订正为真实的两模式 | 判据是否反映真相 | **推翻成功（会变红）** | `validate_spec.py:157-160` + `phase1_product.h:31-32` ⇒ 判据在阻止真相入库 |
| E9 | 比对 `l2_performance` 三份归档的 `verdict` 字段 | 逐位比对是 PASS 还是 FAIL | **推翻成功（三个不同判决）** | PASS / FAIL(16) / DIFFER(14) 同目录同 tag |
| E10 | 逐条 grep `COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md:3` 的上游文档名 | 上游引用是否可解析 | **推翻成功** | `ASTROCS_DESIGN.md` 不存在；§8.3 节名也不符 |
| E11 | 在现行 `CALIBRATION.md` 中逐字搜 SC-01 引用的「失效条件 flat_norm=0→显式拒绝」 | 该 P0 是否仍成立 | **推翻成功** | 全文 0 命中；现行文已按台账自给的"文档向代码对齐"改写 |
| E12 | 按伪引假设去证伪 `GATE-501_VERIFICATION.md:11` 的逐字数字 | 该引用是否为真 | **证伪失败（引用是真的）** | `replay.json` `real16_w16_gate.json` 实测 avg=0.2453/p50=0.0388/frac=0.0693/lowwin=142.05，逐字相符；`evidence_count=11`/`now_red=9` 相符；`l2_performance/gates/*_gate.json` 实数 11 |
| E13 | 核查 `docs/science/NOISE_MODEL.md` 是否引用 `exp_math.py` | README:36 的机器消费锚是否成立 | **推翻成功** | `grep exp_math` 零命中 |

---

## 6. 盲复算

**方法**：对每条结论先不看子代理报告与任何既有判定，独立用只读命令取证，再与子代理结论比对。

| 结论 | 我的独立取证 | 与子代理比对 |
|---|---|---|
| MANIFEST 自校验失败 | 我实测 `sha256sum -c` → 2 失败；逐条比对哈希；确认 `4b0308b4` 单提交 + 工作树干净 | **一致**（QA-B F-04） |
| `analyze.py` 未排除 dense | 我实测 `grep -n dense code/analyze.py` → 零命中；`extract_corpus.py:51` 写 `.dense.bin` 同目录；`run_bench.sh:9` 宣称 188 却跑 `data/*.bin` | **一致**（QA-B F-01）；**我未独立复算 226× 的数值对账**，该量化结论采信其推导链 |
| `final_numbers.py` bitwise 恒红 | 我实测 `fits_bench.c:58` 列名 = `bitwise_eq`；归档 JSON 仍为旧值 `20/103/188` 且 `reason=None` | **一致**（QA-B F-02） |
| qa_oracle 27/35 退化 | 我实测 `oracle_baseline.json` 中 `measured ∈ {0,1}` 的 check 数 = 27；逐门推导 `ANA-05/06/11F/MC-06/ANA-03/MC-03/ANA-07A/ANA-09/ANA-10/INJ-03` 的恒等式 | **一致**（QA-A×2）；子代理的逐门 measured 数值与我独立复算的 `MC-06 loss=4.544860534134822`、`MC-07 ratio=1.01845` 逐位吻合 |
| `min_cases` 账本退化 | 我实测：44 门中 4 门 pending（`run_all.py:16`），**其余 40 门 `executed_cases == required_cases` 精确相等**（34 门因 `min_cases ≡ 绿 check 数`，2 门硬编码同值，6 个 P0 门因 `P0_EXEC` 逐个等于 `min_cases`） | **一致**（QA-A 的 40/44） |
| `psfsw_robust` 已退役 | 我实测 `phase1_product.h:31-32` 逐字 + `runtime_contract.h:113-114` + `git log -1 fc34a9cf` + `merge-base --is-ancestor` = YES | **一致**（QA-E F1、QA-A F2），且我把它与 B-04/B-06 合并为一条更重的发现 |
| l2 三判决矛盾 | 我实测三份 JSON 的 `verdict` 与 `counts` | **一致**（QA-E F2） |
| FIX_LEDGER 97.9% 悬空 | 我用严格正则重算（第一版正则把 `.cpp` 截成 `.c` 导致数字虚高，已修正后重算）→ 1308/28 | **我独有**；子代理 D 只给了抽样 |
| 复现路径批量核验 | 我实测 `run/v6/real-science/*`、`artifacts/v6/performance/*`、`cli/*`、`runtime/v6_budget.py` 等 20 余条 | **一致**（QA-E §5） |

**复算判据**：一致 10 / **偏松 0** / **偏严 1**。
唯一"偏严"处：子代理 QA-B 把 `product_level.json` 的结构当作含 `n` 字段，我实测其结构不同（`n` 分布返回空），故 QA-B 的"n 值对账"我未能在同一字段上直接复现，只复现了其**根因**（`analyze.py` 不排除 dense）。该量化部分标注为**采信推导链、未独立复算**。

---

## 7. 子代理派发记录

共派发 **7 个子代理 / 5 条独立车道**。⚠️ **QA-A 与 QA-B 各被误重复派发一次**（我在同一消息块内重复写了同一 prompt），实际并发 7 个。这 2 条重复车道提供了独立复核价值，其结论与主车道一致。

| 车道 | 派了几个 | 做什么 | 报告要点 |
|---|---|---|---|
| QA-A | 2 | `v6/qa-design/` 8 份（oracle 7 py + SUMMARY.md） | 27/35 退化实测、`docs/validation/v6/` 整目录已删、`min_cases` 40/44 退化、`render_docs` 自愈、`psfsw_robust` 退役断言 |
| QA-B | 2 | `compress-01/` 20 份 | md 被 dense 污染（226×）、bitwise 恒红、acc_roundtrip 恒真负例、MANIFEST 自校验失败、6 条 stale path、GZIP_2 选择性删臂 |
| QA-C | 1 | `v19r7-quality/` 20 份 | `reports/v19r7_quality/` 整根不存在、A2-01..04 目录缺失、SC-01 幽灵 P0、IO-001/ARC-001/STD-004 已修未销账、判据已改而 8 个 md 一字未改（裂脑）、15/15 lib 锚点失效 |
| QA-D | 1 | `audit-2026-01/` 4 份 + `release-05/` 5 份 | audit-2026-01 判定可信（README 计数逐条为真 + 退役声明）；release-05 阻断（CI 层整体不存在、被引路径 62.5% 死亡、FAILCLOSED md 230/226/4/11 与同源 JSON 357/355/2/15 互斥） |
| QA-E | 1 | `v6/` 15 份 + `l2_performance` 2 份 + `reaudit-v3` 3 份 + `release-01` 3 份 | 三模式架构已被 `fc34a9cf` 删除、l2 PASS/FAIL/DIFFER 三判决、CON-010 的锁已被撤销而 3 处活文档仍在描述、`exp_math.py:10` 错公式死代码 |

### 逐条复核：采信 / 否决 / 修正

| 子代理结论 | 我的处置 | 理由 |
|---|---|---|
| QA-A：27/35 退化 | **采信** | 我用 `oracle_baseline.json` 独立复算 = 27 |
| QA-A：`min_cases` 40/44 | **采信并修正口径** | 我独立复算：40 个**非 pending** 门 100% 退化，4 门 pending（不是"40 退化 + 4 pending"的同一句话，而是分母不同） |
| QA-A：`.docs/validation/v6` 已删 + `render_docs` 自愈 | **采信** | `git ls-files docs/validation` 空、`render_docs.py:152-155` 静默重建 |
| QA-A：`psfsw_robust` 退役断言 | **采信并升级** | 我自己读 `phase1_product.h:31-32` 确认；我把它从"一条发现"升为 **B-04 的核心**，因为它使"判据阻止真相入库"成立 |
| QA-B：md 被 dense 污染 226× | **采信根因，部分采信量化** | 根因（`analyze.py` 零命中）我实测确认；**226× 的数值对账我未能独立复算**（`product_level.json` 结构与子代理假设不符），标注为"采信推导链" |
| QA-B：bitwise 恒红 | **采信并升级为 B-03** | 我实测列名与归档值 |
| QA-B：MANIFEST 自校验失败 | **采信并升级为 B-01** | 我实测并追加了"同一提交落盘 + 工作树干净 ⇒ 入库即错"这条更强证据 |
| QA-B：acc_roundtrip 恒真负例 | **采信为 B-08** | 我自己读 `:44-46,71` 后独立推导 `red` 被 `ok` 蕴含 |
| QA-C：`reports/v19r7_quality/` 整根不存在 | **采信** | 我实测 `ls reports` → 不存在 |
| QA-C：SC-01 幽灵 P0 | **采信并独立复验** | 我亲读 `audit_findings.md:54` 并 grep 现行 `CALIBRATION.md` |
| QA-C：M5 计数错误 7 处 | **部分采信** | 我亲自复算 `audit_findings_noise.md:62-63` 的求和矛盾（M-07）；其余 6 处采信 |
| QA-D：`audit-2026-01` 判定可信 | **采信并补一条** | 我独立复算 785/P0 93/P1 449/P2 240/P? 3 全中；我补了 README 的**两条"现行"指针悬空**（子代理未提），列为 M-04 |
| QA-D：release-05 阻断 | **采信** | 与我实测的 `eng/ci/`、`ci/`、`.github/` 全不存在一致 |
| QA-D：GATE-501 `:11` 引用为真 | **采信并自己复算** | 我逐条比对了 `replay.json` 的四个数字与两个顶层计数，逐字相符 |
| QA-D：D-14 只判须修不判阻断 | **采信** | 「零消费者但有活调用者」不成立，降级正确 |
| QA-E：F1 三模式架构已删 | **采信并升为 B-06** | 我独立 `git log -1 fc34a9cf` + `merge-base --is-ancestor` + 读 `runtime_contract.h:113-114` |
| QA-E：F2 l2 三判决矛盾 | **采信并升为 B-07** | 我实测三份 JSON |
| QA-E：F3 CON-010 锁已撤销而文档仍在描述 | **采信（我未亲读该 3 份文件，列入"未读完"）** | 保留其结论但标注证据未经我复核 |
| QA-E：F10 `exp_math.py:10` 错公式死代码 | **采信（未亲读）** | 同上 |
| QA-E：S8 "REAL-SCIENCE 内部算术全自洽，不该打" | **采信其阴性结论** | 我亲读 297 行确认 §4/§5 表格内部无算术矛盾；这与我打的 M/阻断项（复现面、Oracle 第 3 项构造性恒等、版本串）不冲突 |

**否决/降级合计**：0 条完全否决；**2 条降级采信**（QA-B 的 226× 数值对账、QA-C 的 7 处计数中的 6 处）；**3 条升级**（QA-A 的 psfsw_robust、QA-B 的 bitwise/MANIFEST、QA-E 的 F1/F2 均并入我的阻断清单并附我自己的独立证据）。

---

## 8. 自证段（可复跑命令）

全部为只读命令；在仓根 `/workspace/Astro CS Database` 执行。

```bash
# 0) 基线
git -c core.quotepath=false rev-parse HEAD          # → f9650dd0aed97d7f261e5e6547f4fdb505bc313b
git -c core.quotepath=false status --porcelain -- 实验/engineering-evidence/compress-01/   # → 空（干净）

# B-01 归档自校验失败
cd 实验/engineering-evidence/compress-01 && sha256sum -c MANIFEST.sha256 2>&1 | tail -3
grep -vc '^#' MANIFEST.sha256                                   # 24 条清单
find . -type f ! -name MANIFEST.sha256 | wc -l                 # 25 件 → md 不在清单内
cd - >/dev/null
git -c core.quotepath=false log --oneline -1 -- 实验/engineering-evidence/compress-01/MANIFEST.sha256   # 4b0308b4（与另两件同提交）

# B-02 md 被合成载荷污染（唯一不排除 .dense 的汇总脚本）
cd 实验/engineering-evidence/compress-01/code
grep -c 'dense' analyze.py            # → 0
grep -c 'dense' product_level.py      # → 2
grep -c 'dense' py_bench.py           # → 2
grep -c 'dense' ../verify/final_numbers.py   # → 5
grep -n 'dense' extract_corpus.py | head -3
sed -n '9,10p' run_bench.sh            # 宣称 188 瓦片，实跑 data/*.bin
cd - >/dev/null

# B-03 bitwise 恒红门
grep -n 'bitwise' 实验/engineering-evidence/compress-01/code/fits_bench.c | head -2   # 列名 bitwise_eq
sed -n '102,103p' 实验/engineering-evidence/compress-01/verify/final_numbers.py        # 查 "bitwise"
python3 -c "import json;d=json.load(open('实验/engineering-evidence/compress-01/final_numbers.json',encoding='utf-8'));print([(k,v['bitwise_eq_corpus'],v['corpus_n'],v['bitwise_unavailable_reason']) for k,v in d['schemes'].items() if 'bitwise_eq_corpus' in v])"
# → 全是旧硬编 (20|103, 188, None) ⇒ 新代码从未跑过

# B-04 判据读不到真实对象 + 机械断言已退役事实
grep -c 'open(' 实验/engineering-evidence/v6/qa-design/oracle/qa_oracle.py     # → 1（:570 写自己的 json）
python3 -c "import json;d=json.load(open('实验/engineering-evidence/v6/qa-design/evidence/oracle_baseline.json',encoding='utf-8'));print(sum(1 for c in d['checks'] if c['measured'] in (0,0.0,1,1.0)),'/',len(d['checks']))"   # → 27 / 35
grep -n 'psfsw_robust' 实验/engineering-evidence/v6/qa-design/data/meta.json
sed -n '31,32p' lib/algorithms/integration/phase1_product/include/acsd/phase1_product.h
sed -n '157,160p' 实验/engineering-evidence/v6/qa-design/oracle/validate_spec.py
sed -n '113,114p' lib/infrastructure/cli/runtime_contract.h
git -c core.quotepath=false merge-base --is-ancestor fc34a9cf HEAD && echo YES
git -c core.quotepath=false log --oneline -1 fc34a9cf

# 零用例即红退化（40/40 非 pending 门 executed==required）
python3 - <<'PY'
import json,collections,os
R='实验/engineering-evidence/v6/qa-design'
d=json.load(open(f'{R}/evidence/oracle_baseline.json',encoding='utf-8'))
qm=json.load(open(f'{R}/qa_matrix.json',encoding='utf-8'))
cnt=collections.Counter()
for r in d['checks']:
    if r['ok']:
        for g in r['gates']: cnt[g]+=1
P0={"P0-01":2,"P0-02":2,"P0-03":2,"P0-04":2,"P0-05":5,"P0-06":2}
PEND={"G-RD-01","G-RD-02","G-RD-06","G-BASE-03"}
deg=0;tot=0
for g in qm['gates']:
    gid=g['gate_id'];req=g['zero_case_red']['min_cases']
    if gid in PEND: continue
    tot+=1
    ex = P0.get(gid, 8 if gid=='G-RD-03' else 1 if gid=='G-RD-04' else cnt.get(gid,0))
    if ex==req: deg+=1
print(f'非 pending 门 {tot}，其中 executed==required 精确相等 {deg}')
PY

# B-05 FIX_LEDGER 97.9% 悬空
python3 - <<'PY'
import csv,re,os,collections
rows=list(csv.DictReader(open('实验/engineering-evidence/audit-2026-01/FIX_LEDGER.csv',encoding='utf-8-sig')))
pat=re.compile(r'([A-Za-z0-9_][A-Za-z0-9_./\-]*\.(?:md|cc|cpp|h|hpp|py|json|txt|ps1|ya?ml|sh|cmake|def|exp|toml))(?![A-Za-z0-9])')
refs=collections.Counter()
for r in rows:
    for c in rows[0]:
        for m in pat.finditer(r.get(c) or ''): refs[m.group(1)]+=1
miss=[p for p in refs if not os.path.exists(p)]
print(f'去重路径 {len(refs)}，存在 {len(refs)-len(miss)}，缺失 {len(miss)} = {100*len(miss)/len(refs):.1f}%')
print([r['id'] for r in rows if r['fix_state']=='FIXED' and not (r['fix_commit'] or '').strip()])
PY

# B-06 复现面全灭 + 三模式已退役
for p in run/v6/real-science/extract_real.py artifacts/v6/real-science reports/v6/real-science \
         cli/v6_runtime_contract.h runtime/v6_budget.py cli/commands.cpp \
         eng/tools/v6/v6_determinism_driver.py artifacts/v6/performance/perf_harness.py \
         docs/validation/v6 reports/v19r7_quality eng/ci ci docs/ci; do
  [ -e "$p" ] && echo "EXIST $p" || echo "MISS  $p"; done
grep -c 'exp_math' docs/science/NOISE_MODEL.md      # → 0（README:36 的锚不成立）

# B-07 l2 三判决矛盾
python3 -c "
import json
L='实验/engineering-evidence/l2_performance/'
for f in ('real16_bitwise_verdict.json','real16_bitwise_bitwise.json','real16_bitwise_cmp/compare_report.json'):
    d=json.load(open(L+f,encoding='utf-8'))
    print(f, '->', d.get('verdict'), d.get('real_differences'), d.get('counts'))"

# M-01 上游伪引
ls docs/ASTROCS_DESIGN.md 2>&1 | head -1
sed -n '3p' 实验/engineering-evidence/compress-01/COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md
grep -n '^### 8.3' docs/ACSD_DESIGN.md

# M-06 SC-01 幽灵 P0
grep -n 'flat_norm\|显式拒绝\|除零' docs/science/CALIBRATION.md
grep -n 'SC-01' 实验/engineering-evidence/v19r7-quality/audit_findings.md | head -1

# E12 反例（证伪失败：GATE-501:11 的引用是真的）
python3 -c "
import json;d=json.load(open('run/ci/l2-frozen-gate/replay.json',encoding='utf-8'))
print('evidence_count',d.get('evidence_count'),'now_red',d.get('historical_pass_now_red'))
print([c['value'] for c in d['criteria'] if 'real16_w16' in str(c)])" 2>/dev/null || \
python3 -c "
import json;d=json.load(open('run/ci/l2-frozen-gate/replay.json',encoding='utf-8'))
e=[x for x in d.get('evidence',[]) if 'real16_w16' in str(x)]
print(e[:1])"

# M-10 文本损坏
grep -rn '@@' 实验/engineering-evidence/release-05/*.md
```

---

## 9. 待联网核验 / 待前台执行

**待联网核验**
- `COMPRESSION_CODEC_RESEARCH_PACK.md:3/:45/:475` 把「异步只用于能隐藏延迟的 I/O、预取与压缩」归属为 `ACSD_DESIGN.md §8.3` 原文；实测该句只出现在 `docs/engineering/EXECUTION_MODEL.md:126` 与 pack 自身，`ACSD_DESIGN.md` §8.3 无此句（QA-B 提出，我未亲读该文件）——同理 `:46` 引的「用完即释」在 `ACSD_DESIGN.md` 中 `grep -c` = 0。

**待前台执行**（本片无一件可在本车道完成）
- `compress-01` §7 重建流程（第 1 步 `extract_corpus.py` 即因语料 `run/PERF-401/out/real16_w1` 缺失而断链），因此 **B-02 修复后无法在仓内验证 md 的重生成结果**。
- 一切需要重跑的验证（`qa_oracle.py` / `run_all.py` / `run_mutations.py` / 任何 `.sh` / C 程序）。

## 10. 计数口径声明

本片涉及四种计数口径，分层如下，**不可互替**：

| 口径 | 数值 | 命令/出处 |
|---|---|---|
| **整改分母（文件层）** | **80 份 / 8412 行** | 权威版 YAML `:2330-2417`；实测 80 份全在、8412 行（清单声明 8403，差 +9 为末行无换行计数差） |
| **个人完整 read 分母** | **22 份 / 4811 行（57.2%）** | §3 表"读了多少行"列求和 |
| **子代理完整 read 分母** | **80 份 / 8412 行（并集 100%）** | 7 个子代理车道并集 |
| **门实例（qa_oracle）** | **35** | `grep -c '^@check(' qa_oracle.py` |
| **规格门（qa_matrix）** | **44** | `len(qm['gates'])`；其中 **7 门（`G-RD-04` + `P0-01..06`）零 oracle check 覆盖** |
| **退化实测值** | **27 / 35 门实例** | `oracle_baseline.json` 中 `measured ∈ {0,1}` |
| **账本退化** | **40 / 40 非 pending 规格门** | `executed_cases == required_cases` 精确相等；另 4 门为 pending |
| **mutation 目录** | **56 = science 39 + spec 14 + doc 3** | `data/mutations.json` |
| **FIX_LEDGER 整改分母** | **785 行 = P0 93 / P1 449 / P2 240 / P? 3** | 独立复算，与 `audit-2026-01/README.md:11` 逐条相符 |
| **FIX_LEDGER 锚点存活率** | **28 / 1308 去重文件路径（2.1%）** | 本人严格正则复算 |
| **l2 比对文件** | 3094/3094，公有 3093，实质差异 14（含 2 项 unpaired） | `compare_report.json` |
| **本片发现数** | **阻断 9 / 须修 11 / 建议 6** | 本文件 §4 |