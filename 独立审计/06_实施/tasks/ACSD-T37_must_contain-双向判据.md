# 任务：ACSD-T37 `must_contain` 与唯一源互斥：改双向判据，不采纳会引入新 fail-open 的指针判据

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 合同+门禁 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/contracts/schemas/hips_storage_form.schema.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×4 | 合同层·W4[PASS] 合同层·W5[PASS] | 入库 |
| `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×8、AUD-101-DB-04.md×2、AUD-101-DB-12.md×2、AUD-101-DB01.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-101-DB-14.md×1 | 合同层·W4[PASS] 合同层·W5[PASS] | 入库 |
| `docs/design/PRODUCT_STORAGE_FORM.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×9、AUD-101-DB-12.md×3、AUD-101-DB-10-补.md×2、AUD-101-DA01-根规范与科学.md×1 | 合同层·W4[PASS] 合同层·W5[PASS] | 入库 |
| `eng/tools/hipsform/check_hips_storage_form.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×3、AUD-101-DB-10-补.md×1 | 合同层·W4[PASS] 合同层·W5[PASS] | 入库 |
| `eng/ci/checks.json` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿（同对象另有 1 条 D3 主张） | AUD-501-门禁现状审计.md×17、AUD-101-D1补三份.md×11、AUD-101-DB-19.md×6、D9-工单对账-补.md×6、AUD-101-DB-10-补.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-12.md×4、AUD-101-DA02-算法推导.md×3、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-20.md×1、AUD-401-架构对齐.md×1、AUD-403-注释与README.md×1 | AUD201·V6[PASS] AUD204·W2[PASS] DB13·W1[PASS] DB13·W2[PASS] 合同层·W3[PASS] 合同层·W4[PASS] 结果层与收口层·R1[PASS/P1] 负责人面与索引·V1[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `docs/ci/CI_SPEC.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×8、AUD-501-门禁现状审计.md×7、AUD-101-DB-08.md×5、AUD-101-DB-10-补.md×2、AUD-101-DA01-根规范与科学.md×1 | 合同层·W4[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `docs/design/PHASE1_DETAILED_DESIGN.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×4、AUD-101-DB-10-补.md×2、AUD-101-DB-10.md×2 | 合同层·W4[PASS] | 入库 |
| `docs/design/PHASE2_DETAILED_DESIGN.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×3、AUD-101-DB-10.md×1 | DB13·W3[PASS/P1] 合同层·W4[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `docs/design/PHASE3_DETAILED_DESIGN.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×2、AUD-101-DB-10.md×1、AUD-101-DB01.md×1 | 合同层·W4[PASS] | 入库 |
| `docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | D9-工单对账-补.md×4、AUD-101-DB-08.md×2、AUD-101-DB01.md×2 | 合同层·W4[PASS] | 入库 |
| `docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB01.md×5、AUD-101-DB-08.md×2、AUD-101-DB-10-补.md×1 | 合同层·W4[PASS] | 入库 |

## 2 依据

「复核-合同层」W4（判定：确认（互斥成立、触发面缺口成立）＋处置方案降级 —— 成稿给的「换成指针判据」会净失一项现行判据强度，引入新 fail-open；须改为双向判据）；标准 05 §3。

## 3 改法（具体动作，动词开头）

1. 把判据改为双向：说明层声明的必备字段 ⊆ 机器层实际约束，且机器层约束 ⊆ 说明层声明（两侧各一条判红）
2. 补触发面：`check_hips_storage_form.py` 缺声明输入、配置缺失时按合同判红或按声明路径处理，不静默取默认
3. 订正 `PRODUCT_STORAGE_FORM.md` 与合同对同一约束的两种表述

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/contracts/schemas/hips_storage_form.schema.json
docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md
docs/design/PRODUCT_STORAGE_FORM.md
eng/tools/hipsform/check_hips_storage_form.py
eng/ci/checks.json
docs/ci/CI_SPEC.md
docs/design/PHASE1_DETAILED_DESIGN.md
docs/design/PHASE2_DETAILED_DESIGN.md
docs/design/PHASE3_DETAILED_DESIGN.md
docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md
docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T36（先定唯一源，才能判「子集」方向）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - 与 ACSD-T38 共改 `HIPS_STORAGE_FORM_CONTRACT.md` 与同一 schema ⇒ 必须同批

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 机器层多出一个说明层未声明的约束 ⇒ 反向判据判红；说明层声明一个机器层未约束的字段 ⇒ 正向判据判红（现状：只一侧有判据）
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以「指针判据」替代双向判据（第②层已判其净失一项现行判据强度）

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/contracts/schemas/hips_storage_form.schema.json` | AUD-101-DB-08.md:235;AUD-101-DB-08.md:277;AUD-101-DB-08.md:375;AUD-101-DB-08.md:70 | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |
| `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md` | AUD-101-DA01-根规范与科学.md:678;AUD-101-DB-04.md:168;AUD-101-DB-04.md:170;AUD-101-DB-08.md:244;AUD-101-DB-08.md:266;AUD-101-DB-08.md:287;AUD-101-DB-08.md:300;AUD-101-DB-08.md:318;AUD-101-DB-08.md:334;AUD-101-DB-08.md:40;AUD-101-DB-08.md:46;AUD-101-DB-10-补.md:104… | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |
| `docs/design/PRODUCT_STORAGE_FORM.md` | AUD-101-DA01-根规范与科学.md:106;AUD-101-DB-08.md:240;AUD-101-DB-08.md:267;AUD-101-DB-08.md:296;AUD-101-DB-08.md:49;AUD-101-DB-08.md:51;AUD-101-DB-08.md:53;AUD-101-DB-08.md:69;AUD-101-DB-08.md:76;AUD-101-DB-08.md:79;AUD-101-DB-10-补.md:288;AUD-101-DB-10-补.md:444… | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |
| `eng/tools/hipsform/check_hips_storage_form.py` | AUD-101-DB-08.md:251;AUD-101-DB-08.md:283;AUD-101-DB-08.md:72;AUD-101-DB-10-补.md:106 | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |
| `eng/ci/checks.json` | AUD-101-D1残余.md:165;AUD-101-D1补三份.md:102;AUD-101-D1补三份.md:105;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:197;AUD-101-D1补三份.md:48;AUD-101-D1补三份.md:51;AUD-101-D1补三份.md:57;AUD-101-D1补三份.md:58;AUD-101-D1补三份.md:83;AUD-101-D1补三份.md:86;AUD-101-D1补三份.md:92… | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ DB13·W1：确认 ‖ DB13·W2：确认（头条数字降级） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `docs/ci/CI_SPEC.md` | AUD-101-DA01-根规范与科学.md:228;AUD-101-DB-08.md:164;AUD-101-DB-08.md:170;AUD-101-DB-08.md:189;AUD-101-DB-08.md:195;AUD-101-DB-08.md:321;AUD-101-DB-10-补.md:408;AUD-101-DB-10-补.md:412;AUD-101-DB-11.md:316;AUD-101-DB-11.md:318;AUD-101-DB-11.md:337;AUD-101-DB-11.md:344… | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `docs/design/PHASE1_DETAILED_DESIGN.md` | AUD-101-DB-08.md:241;AUD-101-DB-08.md:297;AUD-101-DB-08.md:310;AUD-101-DB-08.md:312;AUD-101-DB-10-补.md:424;AUD-101-DB-10-补.md:443;AUD-101-DB-10.md:24;AUD-101-DB-10.md:44 | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 |
| `docs/design/PHASE2_DETAILED_DESIGN.md` | AUD-101-DB-08.md:242;AUD-101-DB-08.md:298;AUD-101-DB-08.md:309;AUD-101-DB-10.md:26 | DB13·W3：确认（范围与严重度上修） ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `docs/design/PHASE3_DETAILED_DESIGN.md` | AUD-101-DB-08.md:243;AUD-101-DB-08.md:299;AUD-101-DB-10.md:28;AUD-101-DB01.md:35 | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 |
| `docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md` | AUD-101-DB-08.md:246;AUD-101-DB-08.md:302;AUD-101-DB01.md:349;AUD-101-DB01.md:351;D9-工单对账-补.md:294;D9-工单对账-补.md:297;D9-工单对账-补.md:298;D9-工单对账-补.md:300 | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 |
| `docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md` | AUD-101-DB-08.md:247;AUD-101-DB-08.md:303;AUD-101-DB-10-补.md:287;AUD-101-DB01.md:258;AUD-101-DB01.md:388;AUD-101-DB01.md:390;AUD-101-DB01.md:414;AUD-101-DB01.md:920 | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 |

