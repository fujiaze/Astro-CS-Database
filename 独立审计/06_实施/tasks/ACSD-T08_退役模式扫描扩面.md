# 任务：ACSD-T08 被禁 token 仍列生产模式：退役门扫描面扩到登记件与验证档案

> 波次 `W0` ｜ 杠杆分档 `P1` ｜ 整改域 门禁+文档 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/ci/check_psfsw_retired.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-501-门禁现状审计.md×1 | DB13·W1[PASS] | 入库 |
| `eng/contracts/data/v6_clause_registry_v1.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | D9-工单对账.md×4、AUD-101-DB-04.md×2、AUD-101-DB-06-07.md×1、AUD-101-DB-13.md×1 | DB13·W1[PASS] 合同层·W1[PASS] | 入库 |
| `eng/contracts/schemas/unified/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-09.md×4、AUD-101-DB-03.md×2、AUD-101-DB-06-07.md×2、D9-工单对账.md×2、AUD-101-DB-04.md×1 | DB13·W1[PASS] | 入库 |
| `docs/validation/v6/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | D9-工单对账-补.md×2、AUD-101-DB-11.md×1、AUD-101-DB-13.md×1 | DB13·W1[PASS] | 入库 |
| `docs/science/PSF_SIGNAL_WEIGHT.md` | — | 双计使 `σ_F` 高估 `+12.8%`（基准点）至 `+34.0%`（RN=50 最坏点） | 头条取的是 **N=1000 单次 MC 实现值**（`b2_noise_terms.py:184` 的分母是该 seed 那 1000 帧的实测散布），而同一 JSON 里就躺着 seed 无关的闭式 `pred_doublecount_bias = +14.5009%`。"同一物理点两个数"的成因**不是口径分歧**，是分母的样本噪声：四条同点轴（非两条）的分子极差 0.027%、分母极差 4（同对象另有 1 条 D3 主张） | AUD-101-DB-16.md×12、AUD-101-DA01-根规范与科学.md×6、AUD-202-SNR核验.md×4、AUD-101-DB-04.md×2、AUD-101-DB-12.md×2、AUD-101-DB-13.md×2、AUD-101-DB-03.md×1、AUD-101-DB-10.md×1 | DB13·W1[PASS] | 入库 |
| `eng/ci/checks.json` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿（同对象另有 1 条 D3 主张） | AUD-501-门禁现状审计.md×17、AUD-101-D1补三份.md×11、AUD-101-DB-19.md×6、D9-工单对账-补.md×6、AUD-101-DB-10-补.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-12.md×4、AUD-101-DA02-算法推导.md×3、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-20.md×1、AUD-401-架构对齐.md×1、AUD-403-注释与README.md×1 | AUD201·V6[PASS] AUD204·W2[PASS] DB13·W1[PASS] DB13·W2[PASS] 合同层·W3[PASS] 合同层·W4[PASS] 结果层与收口层·R1[PASS/P1] 负责人面与索引·V1[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `lib/algorithms/coverage/tests/weight_mode_retire_negative_test.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | DB13·W1[PASS] | 入库 |
| `docs/contracts/DATA_SEMANTICS.md` | — | 同一份文档体系对**同一符号**给两种分母：代码与 FROZEN 正本 `docs/science/DRIZZLE.md:44-50` 为 `w_jp = a_jp/A_drop,j`（其 §10 禁止项逐字写着"把核权重写回 `a_jp/A_pixel,j`（FZ-COND-FLUX-CONSERV 判红）"）；而 `DATA_SEMA | 唯一实现口径是 `a_jp/A_drop`；`w'_jp = a_jp/A_pixel` 只作为**等价参数化**成立，且必须同时换分母（`N'_p = Σ w'·A_pixel = D_p`）。等价性已独立复核：发布 `S_p` 与 `variance_p` 对两种参数化**不变**（分子分母各乘 `pf⁴` 相消）；但 `Σ_p w_jp = 1`（⇒ `Σ_p F_p = Σ_j x_j`、（同对象另有 1 条 D3 主张） | AUD-101-DA02-算法推导.md×9、AUD-101-DB01.md×9、AUD-101-DB-06-07.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-04.md×3、AUD-101-DB-10-补.md×3、AUD-101-DA01-根规范与科学.md×2、AUD-101-DB-03.md×2、AUD-204-面积交叠核验.md×2、AUD-101-DB-13.md×1、AUD-402-判读-BD1.md×1、AUD-403-注释与README.md×1 | DB13·W1[PASS] DB13·W4[PASS/P1] 合同层·W1[PASS] | 入库 |
| `docs/DOCUMENT_INDEX.yaml` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-03.md×28、AUD-101-DB-11.md×20、AUD-101-DB-19.md×18、AUD-101-DA01-根规范与科学.md×13、AUD-101-D1残余.md×11、AUD-101-DB-12.md×11、AUD-101-DB01.md×11、AUD-101-DB-08.md×9、AUD-101-DB-10-补.md×8、AUD-101-DA02-算法推导.md×6、AUD-101-DB-05.md×6、AUD-101-DB-04.md×4、AUD-101-DB-20.md×4、AUD-101-D1补三份.md×3、AUD-101-DB-06-07.md×3、AUD-101-DB-14.md×3、AUD-101-DB-16.md×3、AUD-101-DB-17.md×3、AUD-101-DB-13.md×2、AUD-101-DB-18.md×2、AUD-101-DB-09.md×1、AUD-101-DB-15.md×1、AUD-402-判读-A3.md×1 | 负责人面与索引·V3[PASS] | 入库 |

## 2 依据

「复核-DB13」W1（判定：确认）：被科学正本与两份现行合同判为「禁用 token、接受面 = 空」的 `psfsw_robust`，在验证档案层 6 件（在册 `ACTIVE_NORMATIVE`）与其机器源里仍列为生产模式；合同机器登记件自身三处相反（`weight_modes.production` 2 值 vs 同名 FROZEN 条款 3 值 vs `G-EPSF-PRESENT` 条件仍写「三生产模式」）；退役门扫描面只及 `eng/contracts/schemas/unified` ⇒ 拦不住；标准 04 §4。

## 3 改法（具体动作，动词开头）

1. 扩 `check_psfsw_retired.py` 扫描面到 `eng/contracts/data/**`、`docs/validation/v6/**` 与全部登记件
2. 把 `v6_clause_registry_v1.json` 三处取值改为同一值（或指向同一 value 源）
3. 把 `FZ-MODE-RETIRED` 正式入册 `eng/ci/checks.json`
4. 验证档案 6 件按「就地改成 2 模式」或「降级出正式文档层并扩退役门扫描面」二选一执行（第②层推荐后者，但三条取证路互斥，属负责人取向选择）

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/ci/check_psfsw_retired.py
eng/contracts/data/v6_clause_registry_v1.json
eng/contracts/schemas/unified/
docs/validation/v6/
docs/science/PSF_SIGNAL_WEIGHT.md
eng/ci/checks.json
lib/algorithms/coverage/tests/weight_mode_retire_negative_test.cpp
docs/contracts/DATA_SEMANTICS.md
docs/DOCUMENT_INDEX.yaml
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 判据扩面须晚于 ACSD-T00 批 2（否则新门字段不齐）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `PSF_SIGNAL_WEIGHT.md`、`DATA_SEMANTICS.md` 与 T21/T36 共改 ⇒ 同批或由同一簇领改；`docs/DOCUMENT_INDEX.yaml` 与 T10 同批

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 在任一登记件把 `psfsw_robust` 写回 production 分类 ⇒ 退役门判红
- 修好判据：跟踪集内 `psfsw_robust` 不再出现在任何 production 分类；登记件三处同值；`FZ-MODE-RETIRED` 入册
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得只改说明层不改机器登记件；不得以「档案已声明历史」免除扫描

## 8 登记与边界

整改路线（就地订正 vs 降级出层）已进 UNRESOLVED（该复核件「必须负责人裁的」节），不阻塞判据扩面半边。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/ci/check_psfsw_retired.py` | AUD-501-门禁现状审计.md:642 | DB13·W1：确认 |
| `eng/contracts/data/v6_clause_registry_v1.json` | AUD-101-DB-04.md:547;AUD-101-DB-04.md:94;AUD-101-DB-06-07.md:97;AUD-101-DB-13.md:157;D9-工单对账.md:235;D9-工单对账.md:258;D9-工单对账.md:331;D9-工单对账.md:332 | DB13·W1：确认 ‖ 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `eng/contracts/schemas/unified/` | AUD-101-DB-03.md:377;AUD-101-DB-03.md:540;AUD-101-DB-04.md:293;AUD-101-DB-06-07.md:78;AUD-101-DB-06-07.md:80;AUD-101-DB-09.md:192;AUD-101-DB-09.md:199;AUD-101-DB-09.md:239;AUD-101-DB-09.md:263;D9-工单对账.md:63;D9-工单对账.md:65 | DB13·W1：确认 |
| `docs/validation/v6/` | AUD-101-DB-11.md:54;AUD-101-DB-13.md:218;D9-工单对账-补.md:330;D9-工单对账-补.md:72 | DB13·W1：确认 |
| `docs/science/PSF_SIGNAL_WEIGHT.md` | AUD-101-DA01-根规范与科学.md:117;AUD-101-DA01-根规范与科学.md:361;AUD-101-DA01-根规范与科学.md:364;AUD-101-DA01-根规范与科学.md:368;AUD-101-DA01-根规范与科学.md:374;AUD-101-DA01-根规范与科学.md:60;AUD-101-DB-03.md:431;AUD-101-DB-04.md:92;AUD-101-DB-04.md:96;AUD-101-DB-10.md:64;AUD-101-DB-12.md:201;AUD-101-DB-12.md:203… | DB13·W1：确认 |
| `eng/ci/checks.json` | AUD-101-D1残余.md:165;AUD-101-D1补三份.md:102;AUD-101-D1补三份.md:105;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:197;AUD-101-D1补三份.md:48;AUD-101-D1补三份.md:51;AUD-101-D1补三份.md:57;AUD-101-D1补三份.md:58;AUD-101-D1补三份.md:83;AUD-101-D1补三份.md:86;AUD-101-D1补三份.md:92… | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ DB13·W1：确认 ‖ DB13·W2：确认（头条数字降级） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `lib/algorithms/coverage/tests/weight_mode_retire_negative_test.cpp` | — | DB13·W1：确认 |
| `docs/contracts/DATA_SEMANTICS.md` | AUD-101-DA01-根规范与科学.md:359;AUD-101-DA01-根规范与科学.md:607;AUD-101-DA02-算法推导.md:1052;AUD-101-DA02-算法推导.md:1055;AUD-101-DA02-算法推导.md:1323;AUD-101-DA02-算法推导.md:1750;AUD-101-DA02-算法推导.md:2134;AUD-101-DA02-算法推导.md:2204;AUD-101-DA02-算法推导.md:450;AUD-101-DA02-算法推导.md:772;AUD-101-DA02-算法推导.md:803;AUD-101-DB-03.md:38… | DB13·W1：确认 ‖ DB13·W4：确认（可结案，不必上呈） ‖ 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `docs/DOCUMENT_INDEX.yaml` | AUD-101-D1残余.md:127;AUD-101-D1残余.md:145;AUD-101-D1残余.md:160;AUD-101-D1残余.md:335;AUD-101-D1残余.md:387;AUD-101-D1残余.md:401;AUD-101-D1残余.md:406;AUD-101-D1残余.md:407;AUD-101-D1残余.md:435;AUD-101-D1残余.md:66;AUD-101-D1残余.md:97;AUD-101-D1补三份.md:128… | 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |

