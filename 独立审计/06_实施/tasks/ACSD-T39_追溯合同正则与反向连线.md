# 任务：ACSD-T39 ID 正则与被审对象、与检查器互斥；反向权威 `INDEX.yaml` 承载不了被委派的那几层

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 合同+文档 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `docs/contracts/INDEX.yaml` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×9、AUD-101-DB-09.md×5、AUD-101-DB01.md×4、AUD-101-DB-05.md×2、AUD-101-DB-06-07.md×2、AUD-101-DB-13.md×2、AUD-101-DA02-算法推导.md×1、AUD-101-DB-04.md×1、AUD-101-DB-12.md×1 | DB13·W2[PASS] | 入库 |
| `eng/contracts/schemas/traceability_matrix.schema.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | DB13·W2[PASS] | 入库 |
| `docs/TRACEABILITY.csv` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×10、AUD-101-DB-04.md×5、AUD-101-DB-13.md×4、AUD-101-DA01-根规范与科学.md×2、AUD-101-DB-09.md×2、AUD-101-DA02-算法推导.md×1、AUD-101-DB-06-07.md×1、D9-工单对账-补.md×1 | DB13·W2[PASS] 负责人面与索引·V3[PASS] | 入库 |
| `docs/traceability/TRACEABILITY_MATRIX.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-13.md×7、AUD-101-DB-11.md×1、AUD-101-DB01.md×1 | DB13·W3[PASS/P1] | 入库 |
| `docs/traceability/TRACEABILITY_MATRIX.csv` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-13.md×3、AUD-101-DB-09.md×2、AUD-101-DB-11.md×2 | DB13·W3[PASS/P1] | 入库 |
| `eng/tools/traceability/check_traceability_matrix.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | DB13·W2[PASS] | 入库 |
| `eng/ci/checks.json` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿（同对象另有 1 条 D3 主张） | AUD-501-门禁现状审计.md×17、AUD-101-D1补三份.md×11、AUD-101-DB-19.md×6、D9-工单对账-补.md×6、AUD-101-DB-10-补.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-12.md×4、AUD-101-DA02-算法推导.md×3、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-20.md×1、AUD-401-架构对齐.md×1、AUD-403-注释与README.md×1 | AUD201·V6[PASS] AUD204·W2[PASS] DB13·W1[PASS] DB13·W2[PASS] 合同层·W3[PASS] 合同层·W4[PASS] 结果层与收口层·R1[PASS/P1] 负责人面与索引·V1[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `docs/design/UNIFIED_MODEL.md` | — | §1 把 `ivar=1/variance` 命名为"Phase2 逐像素科学权重"；§11 同一句既写"适用域=空背景随机分量"又写"直接入加权"（同句自相矛盾）；§4.6 称 HiPS 里"只存"帧级 SNR 与稀疏绝对 SNR | 三处都是**指称越界**，不是数学分歧：`ivar` 对天光建模与 UPM 控制点拟合是正确权重（那一组样本按 §5b 排异分层只取源掩膜外，其总方差即 `σ_bg²`，见 `PHASE2_UPM.md:21,:76`）；对阶段二叠加则须由 §5c 加权方差面给权。`07_noise_snr.md:201` 的"只存"与已定案的 variance/ivar 子产品（`DATA_SEMANTICS` | AUD-101-DB-09.md×4、AUD-101-DA01-根规范与科学.md×2、AUD-101-DB-10-补.md×2、AUD-101-DB-03.md×1、AUD-101-DB-14.md×1、AUD-101-DB-16.md×1、AUD-101-DB-20.md×1、D9-工单对账.md×1 | AUD202·V3[VOID] DB13·W1[PASS] 合同层·W2[PASS] | 入库 |
| `docs/traceability/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB02.md×1 | DB13·W2[PASS] 负责人面与索引·V4[PASS] | 入库 |

## 2 依据

「复核-DB13」W2（判定：确认，头条数字降级）：30/30 行 `module_id` 对规范非法、对门合法；规范承诺的「两表关系」一节内容不存在（全文 `requirement_id` 0 次）；「八层 vs 九层」同文档两处；被指定的反向权威 `INDEX.yaml` 承载不了它被委派的那几层，且它与追溯矩阵之间没有任何在册门做反向连线（`CONTRACT-GRAPH`、`TRACEABILITY-MATRIX` 各自成立、互不相读）；反向面默认只 WARN、CI 不带 `--strict` ⇒ fail-open。定性＝主体缺规则、缺值只 10 条。

## 3 改法（具体动作，动词开头）

1. 统一规范正则 == 检查器正则 == 30/30 实际取值（三处同批）
2. 补规范 §5「两表关系」一节的实际内容；「八层 vs 九层」取一处
3. 给 SRC 规则与反向索引指认补内容；把 4 条超册项入基线或补锚
4. 把 `INDEX.yaml` ↔ 追溯矩阵的反向连线入册为新门，并让 CI 侧带 `--strict`（默认 WARN = fail-open）
5. 第②层口径入档：非占位 ID / 零命中 / VERIFIED 三个头条数不采信，改用复核口径（159 / 91 / 140 / 75）并写明统计口径

## 4 文件域（本任务允许触碰的路径集合）

```text
docs/contracts/INDEX.yaml
eng/contracts/schemas/traceability_matrix.schema.json
docs/TRACEABILITY.csv
docs/traceability/TRACEABILITY_MATRIX.json
docs/traceability/TRACEABILITY_MATRIX.csv
eng/tools/traceability/check_traceability_matrix.py
eng/ci/checks.json
docs/design/UNIFIED_MODEL.md
docs/traceability/
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T10（C3 覆盖 `实验/`、`工程控制/` 后，追溯对象集才与索引同源）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `docs/TRACEABILITY.csv` 与 `docs/traceability/**` 同时被 ACSD-T10 的迁移行指向 ⇒ 与 W4 迁移波同批或先于它

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 修好判据：规范正则 == 检查器正则 == 30/30 取值；`--strict` 跑通且基线条目数 == 实际 WARN 数
- 删一条反向连线 ⇒ 新门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得把反向面留在默认 WARN；不得沿用未被复算的 137/64/53 一组数（已进作废表）

## 8 登记与边界

成稿举例 `ALG-P1-WR-001`、`ALG-P2-RES-001`、`ALG-P2-SMP-001`、`ALG-P3-005` 四条「VERIFIED 零命中」已被推翻，见总览作废表。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `docs/contracts/INDEX.yaml` | AUD-101-DA02-算法推导.md:1061;AUD-101-DB-04.md:175;AUD-101-DB-05.md:381;AUD-101-DB-05.md:432;AUD-101-DB-06-07.md:171;AUD-101-DB-06-07.md:172;AUD-101-DB-08.md:113;AUD-101-DB-08.md:29;AUD-101-DB-08.md:319;AUD-101-DB-08.md:332;AUD-101-DB-08.md:376;AUD-101-DB-08.md:84… | DB13·W2：确认（头条数字降级） |
| `eng/contracts/schemas/traceability_matrix.schema.json` | — | DB13·W2：确认（头条数字降级） |
| `docs/TRACEABILITY.csv` | AUD-101-DA01-根规范与科学.md:450;AUD-101-DA01-根规范与科学.md:509;AUD-101-DA02-算法推导.md:2057;AUD-101-DB-04.md:383;AUD-101-DB-04.md:388;AUD-101-DB-04.md:427;AUD-101-DB-04.md:461;AUD-101-DB-04.md:499;AUD-101-DB-06-07.md:186;AUD-101-DB-09.md:150;AUD-101-DB-09.md:157;AUD-101-DB-11.md:111… | DB13·W2：确认（头条数字降级） ‖ 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |
| `docs/traceability/TRACEABILITY_MATRIX.json` | AUD-101-DB-11.md:450;AUD-101-DB-13.md:117;AUD-101-DB-13.md:355;AUD-101-DB-13.md:79;AUD-101-DB-13.md:84;AUD-101-DB-13.md:88;AUD-101-DB-13.md:92;AUD-101-DB-13.md:93;AUD-101-DB01.md:653 | DB13·W3：确认（范围与严重度上修） |
| `docs/traceability/TRACEABILITY_MATRIX.csv` | AUD-101-DB-09.md:181;AUD-101-DB-09.md:252;AUD-101-DB-11.md:165;AUD-101-DB-11.md:166;AUD-101-DB-13.md:354;AUD-101-DB-13.md:70;AUD-101-DB-13.md:74 | DB13·W3：确认（范围与严重度上修） |
| `eng/tools/traceability/check_traceability_matrix.py` | — | DB13·W2：确认（头条数字降级） |
| `eng/ci/checks.json` | AUD-101-D1残余.md:165;AUD-101-D1补三份.md:102;AUD-101-D1补三份.md:105;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:197;AUD-101-D1补三份.md:48;AUD-101-D1补三份.md:51;AUD-101-D1补三份.md:57;AUD-101-D1补三份.md:58;AUD-101-D1补三份.md:83;AUD-101-D1补三份.md:86;AUD-101-D1补三份.md:92… | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ DB13·W1：确认 ‖ DB13·W2：确认（头条数字降级） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `docs/design/UNIFIED_MODEL.md` | AUD-101-DA01-根规范与科学.md:314;AUD-101-DA01-根规范与科学.md:323;AUD-101-DB-03.md:356;AUD-101-DB-09.md:215;AUD-101-DB-09.md:217;AUD-101-DB-09.md:237;AUD-101-DB-09.md:264;AUD-101-DB-10-补.md:254;AUD-101-DB-10-补.md:426;AUD-101-DB-14.md:42;AUD-101-DB-16.md:347;AUD-101-DB-20.md:347… | AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ DB13·W1：确认 ‖ 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |
| `docs/traceability/` | AUD-101-DB02.md:318 | DB13·W2：确认（头条数字降级） ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |

