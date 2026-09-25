# 任务：ACSD-T40 同一物理量两值未标口径、手写报告字面量与自家 JSON 矛盾、未填充骨架占 `docs/` 位

> 波次 `W1` ｜ 杠杆分档 `P2` ｜ 整改域 实验+文档 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `docs/science/NOISE_MODEL.md` | — | Phase2 逐像素科学权重 = Phase1 落盘的 `ivar`（空背景方差面倒数） | 叠加权应为 `w = 1/σ_w²`，`σ_w² = σ_bg² + S_src/g`（`NOISE_MODEL.md` §5c 的加权方差面）。现状把**带适用域的近似**当**定义**：`1/σ_bg²` 只在 `S_src ≡ 0` 或 `S_src/g ≪ σ_bg²` 时等于最优权（同对象另有 5 条 D3 主张） | AUD-101-DA01-根规范与科学.md×12、AUD-101-DA02-算法推导.md×6、AUD-202-SNR核验.md×6、D9-工单对账.md×6、AUD-101-DB-03.md×5、AUD-101-DB-19.md×4、AUD-101-DB-20.md×3、AUD-101-DB-13.md×1、AUD-101-DB-14.md×1、AUD-101-DB-16.md×1、AUD-101-DB-17.md×1、AUD-301-文献复算-旧判批.md×1 | 结果层与收口层·R3[PASS] | 入库 |
| `docs/science/CONTROL_WEIGHT_SNR.md` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_（同对象另有 2 条 D3 主张） | AUD-101-DA01-根规范与科学.md×10、AUD-101-DB-15.md×7、AUD-202-SNR核验.md×7、AUD-101-DB-13.md×4、AUD-101-DB-16.md×4、AUD-101-DB-04.md×1、AUD-101-DB-12.md×1、AUD-101-DB-14.md×1、AUD-101-DB-19.md×1、AUD-101-DB-20.md×1 | AUD202·V2[PASS] DB13·W4[PASS/P1] 结果层与收口层·R3[PASS] | 入库 |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` | — | 帧级 SNR／稀疏控制点／深度 `m_5` 全部走对角（白噪声）形式 | Phase1 产品是 drizzle 重采样后的 HEALPix 叶（相关长度 1–2 px），属 `07_noise_snr.md:130` 的"相关噪声"分支，却未用完整信息核；完整核**已实现、零接入**（同对象另有 5 条 D3 主张） | AUD-202-SNR核验.md×11、AUD-101-DB-03.md×6、AUD-101-DB-19.md×5、AUD-101-DB-16.md×3、AUD-101-DB-20.md×3、AUD-101-DB-13.md×2、AUD-101-DB-14.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-402-判读-A3.md×1 | AUD202-补·V5[PASS] AUD202·V3[VOID] 结果层与收口层·R3[PASS] | 入库 |
| `实验/additive-sky-seamless/docs/smooth-lambda.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-17.md×6 | 结果层与收口层·R5[PASS] | 需复测（对象不在跟踪集） |
| `实验/absolute-snr/README.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-13.md×8、AUD-101-DB-17.md×2、AUD-101-DB-19.md×2、AUD-101-DB-16.md×1、AUD-101-DB-20.md×1 | 结果层与收口层·R3[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `实验/absolute-snr/results/b1_sky_scan.json` | — | 双计使 `σ_F` 高估 `+12.8%`（基准点）至 `+34.0%`（RN=50 最坏点） | 头条取的是 **N=1000 单次 MC 实现值**（`b2_noise_terms.py:184` 的分母是该 seed 那 1000 帧的实测散布），而同一 JSON 里就躺着 seed 无关的闭式 `pred_doublecount_bias = +14.5009%`。"同一物理点两个数"的成因**不是口径分歧**，是分母的样本噪声：四条同点轴（非两条）的分子极差 0.027%、分母极差 4 | AUD-202-SNR核验.md×2 | 结果层与收口层·R3[PASS] | 入库 |
| `实验/absolute-snr/results/b2_noise_terms.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-202-SNR核验.md×1 | AUD202-补·V6[PASS] 结果层与收口层·R3[PASS] | 入库 |
| `实验/absolute-snr/results/EXP06_TABLES.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-17.md×1 | 结果层与收口层·R3[PASS] | 入库 |
| `实验/healpix-polar/docs/EXP-07-POLAR-摘要.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-17.md×2 | 结果层与收口层·R3[PASS] | 入库 |
| `实验/healpix-polar/results/t17_extent.csv` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 结果层与收口层·R2[PASS] | 入库 |
| `实验/additive-sky-seamless/REPORT_paper.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-301-文献池P1.md×3、AUD-101-DB-17.md×2、AUD-101-DB-20.md×1、AUD-301-文献复算-旧判批.md×1 | AUD203·V5[PASS/P2] 结果层与收口层·R5[PASS] | 入库 |
| `eng/tools/doccheck/doc_fact_authority.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 结果层与收口层·R3[PASS] 结果层与收口层·R4[PASS] | 入库 |
| `eng/ci/check_doc_unverified_cite.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-19.md×2 | 结果层与收口层·R5[PASS] | 入库 |
| `eng/tools/doccheck/check_doc_index.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-D1补三份.md×2、AUD-101-DB-19.md×2、AUD-101-DB-20.md×2、AUD-101-D1残余.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-101-DB-11.md×1 | 结果层与收口层·R5[PASS] 负责人面与索引·V3[PASS] | 入库 |
| `实验/healpix-polar/docs/EXP-07-POLAR.md` | — | （见 §2 依据） | 若仍主张"chart 路线在缝上不达标"，需一条**不依赖 face 归属选择**的逐叶对照（球面精确构造可当 oracle）并给跨 face 解析延拓后的重复数字；另登记"文档冻结 1e−6 与可执行门 1e−7/1e−6 两档并存"为独立的门表登记一致性缺陷（P2） | AUD-301-文献池P1.md×4、AUD-101-DB-17.md×2、AUD-204-面积交叠核验.md×1 | AUD204·W1[PASS] 结果层与收口层·R2[PASS] | 入库 |

## 2 依据

「复核-结果层与收口层」R2（判定：降级 —— 4 条中 3 条成立、第 4 条推翻；SNAPSHOT 子核确认 47/47 通过）、R3（判定：确认 —— 差因＝口径未声明非取数错；SCI-B 读噪双计正式文档 34% 低估，现行正确值 = 最坏 +36.6%；healpix 同页 1.12e-8 与 2.79e-8 两值都真、差在样本面未写）、R4（判定：确认 —— 三处不一致全部复现，定级按是否进判据/对外主张分档）、R5（判定：确认 —— 占位与 5/7 结论行逐位复现，并补一条比成稿更重的观察）。

## 3 改法（具体动作，动词开头）

1. 在每个数值引用点补口径标注（样本面/统计量/实现值 vs 闭式），或改指唯一有跟踪副本的那一个
2. 把 SCI-B 读噪双计比例在正式文档改为最坏 +36.6%（现行 34% 低估缺陷量级）
3. 让手写报告层的三处字面量与 `doc_fact_authority.json` 对齐，并为该判据补实读断言（不一致即判红）
4. `实验/additive-sky-seamless/docs/smooth-lambda.md` 按 R5 处置：迁回 `实验/additive-sky-seamless/docs/` 或补全后转正，不留未填充骨架占正式文档层位
5. R2 的第 4 条不执行（已被推翻，进作废表）

## 4 文件域（本任务允许触碰的路径集合）

```text
docs/science/NOISE_MODEL.md
docs/science/CONTROL_WEIGHT_SNR.md
docs/plugins/algorithms_phase1/07_noise_snr.md
实验/additive-sky-seamless/docs/smooth-lambda.md
实验/absolute-snr/README.md
实验/absolute-snr/results/b1_sky_scan.json
实验/absolute-snr/results/b2_noise_terms.json
实验/absolute-snr/results/EXP06_TABLES.md
实验/healpix-polar/docs/EXP-07-POLAR-摘要.md
实验/healpix-polar/results/t17_extent.csv
实验/additive-sky-seamless/REPORT_paper.md
eng/tools/doccheck/doc_fact_authority.json
eng/ci/check_doc_unverified_cite.py
eng/tools/doccheck/check_doc_index.py
实验/healpix-polar/docs/EXP-07-POLAR.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T02（读数入库面），否则口径标注仍无跟踪副本
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `NOISE_MODEL.md`/`CONTROL_WEIGHT_SNR.md` 与 T20/T32/T33 共改 ⇒ 同批；`check_doc_index.py` 与 T10 共改 ⇒ 同批

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 报告字面量与其自家 JSON 不一致 ⇒ 判据判红；无跟踪副本的数字被当实测引用 ⇒ `check_doc_unverified_cite.py` 判红
- 未填充骨架（5/7 结论行）留在 `docs/` ⇒ 角色/骨架判据判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以删除整段冒充订正（判已修要看被点名的行本身）

## 8 登记与边界

M42 帧级 σ 两份结果差约 4 倍：第②层判为口径未声明、两侧数值各自可回溯，属登记口径缺陷而非取数错。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `docs/science/NOISE_MODEL.md` | AUD-101-DA01-根规范与科学.md:101;AUD-101-DA01-根规范与科学.md:112;AUD-101-DA01-根规范与科学.md:383;AUD-101-DA01-根规范与科学.md:387;AUD-101-DA01-根规范与科学.md:393;AUD-101-DA01-根规范与科学.md:61;AUD-101-DA01-根规范与科学.md:627;AUD-101-DA01-根规范与科学.md:652;AUD-101-DA01-根规范与科学.md:697;AUD-101-DA01-根规范与科学.md:701;AUD-101-DA01-根规范与科学.md:719;AUD-101-DA01-根规范与科学.md:768… | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `docs/science/CONTROL_WEIGHT_SNR.md` | AUD-101-DA01-根规范与科学.md:102;AUD-101-DA01-根规范与科学.md:104;AUD-101-DA01-根规范与科学.md:345;AUD-101-DA01-根规范与科学.md:349;AUD-101-DA01-根规范与科学.md:377;AUD-101-DA01-根规范与科学.md:380;AUD-101-DA01-根规范与科学.md:59;AUD-101-DA01-根规范与科学.md:649;AUD-101-DA01-根规范与科学.md:694;AUD-101-DA01-根规范与科学.md:727;AUD-101-DB-04.md:39;AUD-101-DB-12.md:202… | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） ‖ DB13·W4：确认（可结案，不必上呈） ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` | AUD-101-DA01-根规范与科学.md:375;AUD-101-DB-03.md:310;AUD-101-DB-03.md:313;AUD-101-DB-03.md:318;AUD-101-DB-03.md:476;AUD-101-DB-03.md:480;AUD-101-DB-03.md:499;AUD-101-DB-10-补.md:123;AUD-101-DB-10.md:61;AUD-101-DB-13.md:285;AUD-101-DB-13.md:48;AUD-101-DB-14.md:130… | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `实验/additive-sky-seamless/docs/smooth-lambda.md` | AUD-101-DB-17.md:235;AUD-101-DB-17.md:239;AUD-101-DB-17.md:247;AUD-101-DB-17.md:249;AUD-101-DB-17.md:308;AUD-101-DB-17.md:352 | 结果层与收口层·R5：确认（占位与 5/7 结论行逐位复现），并补一条比成稿更重的观察 |
| `实验/absolute-snr/README.md` | AUD-101-DB-13.md:243;AUD-101-DB-13.md:247;AUD-101-DB-13.md:259;AUD-101-DB-13.md:282;AUD-101-DB-13.md:283;AUD-101-DB-13.md:326;AUD-101-DB-13.md:327;AUD-101-DB-13.md:50;AUD-101-DB-16.md:94;AUD-101-DB-17.md:143;AUD-101-DB-17.md:159;AUD-101-DB-19.md:1445… | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `实验/absolute-snr/results/b1_sky_scan.json` | AUD-202-SNR核验.md:370;AUD-202-SNR核验.md:45 | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `实验/absolute-snr/results/b2_noise_terms.json` | AUD-202-SNR核验.md:358 | AUD202-补·V6：确认**（"头条取自单次 MC 实现值"成立；"最大偏差无人判"成立）。 ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `实验/absolute-snr/results/EXP06_TABLES.md` | AUD-101-DB-17.md:31 | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `实验/healpix-polar/docs/EXP-07-POLAR-摘要.md` | AUD-101-DB-17.md:268;AUD-101-DB-17.md:279 | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `实验/healpix-polar/results/t17_extent.csv` | — | 结果层与收口层·R2：降级（4 条中 3 条成立；第 4 条推翻。SNAPSHOT 子核 = 确认 47/47 通过） |
| `实验/additive-sky-seamless/REPORT_paper.md` | AUD-101-DB-17.md:170;AUD-101-DB-17.md:207;AUD-101-DB-20.md:261;AUD-301-文献复算-旧判批.md:315;AUD-301-文献池P1.md:323;AUD-301-文献池P1.md:388;AUD-301-文献池P1.md:592 | AUD203·V5：确认 ‖ 结果层与收口层·R5：确认（占位与 5/7 结论行逐位复现），并补一条比成稿更重的观察 |
| `eng/tools/doccheck/doc_fact_authority.json` | — | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） ‖ 结果层与收口层·R4：确认（三处不一致全部复现），但定级按"是否进判据/对外主张"分档；成稿的 1 处对照口径需订正 |
| `eng/ci/check_doc_unverified_cite.py` | AUD-101-DB-19.md:437;AUD-101-DB-19.md:484 | 结果层与收口层·R5：确认（占位与 5/7 结论行逐位复现），并补一条比成稿更重的观察 |
| `eng/tools/doccheck/check_doc_index.py` | AUD-101-D1残余.md:356;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:133;AUD-101-DB-10-补.md:438;AUD-101-DB-10.md:71;AUD-101-DB-11.md:33;AUD-101-DB-19.md:584;AUD-101-DB-19.md:666;AUD-101-DB-20.md:32;AUD-101-DB-20.md:346 | 结果层与收口层·R5：确认（占位与 5/7 结论行逐位复现），并补一条比成稿更重的观察 ‖ 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |
| `实验/healpix-polar/docs/EXP-07-POLAR.md` | AUD-101-DB-17.md:269;AUD-101-DB-17.md:287;AUD-204-面积交叠核验.md:71;AUD-301-文献池P1.md:830;AUD-301-文献池P1.md:831;AUD-301-文献池P1.md:832;AUD-301-文献池P1.md:834 | AUD204·W1：确认（并订正成稿两处口径） ‖ 结果层与收口层·R2：降级（4 条中 3 条成立；第 4 条推翻。SNAPSHOT 子核 = 确认 47/47 通过） |

