# 任务：ACSD-T36 `pixel_area_power` 机器层两事实源同名互斥；`CONFIG_CONTRACT.md` 越位成第二登记面

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 合同 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/contracts/schemas/unified/variance.schema.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-06-07.md×3 | 合同层·W1[PASS] | 入库 |
| `docs/contracts/unified_object_registry.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-09.md×3 | 合同层·W1[PASS] | 入库 |
| `eng/contracts/data/v6_clause_registry_v1.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | D9-工单对账.md×4、AUD-101-DB-04.md×2、AUD-101-DB-06-07.md×1、AUD-101-DB-13.md×1 | DB13·W1[PASS] 合同层·W1[PASS] | 入库 |
| `eng/contracts/schemas/product_family_field_constraints.schema.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W1[PASS] | 入库 |
| `docs/contracts/CONFIG_CONTRACT.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×4、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-DB-06-07.md×2、AUD-101-DB-10.md×1、AUD-402-判读-A2.md×1 | 合同层·W3[PASS] 合同层·W4[PASS] | 入库 |
| `docs/contracts/DATA_SEMANTICS.md` | — | 同一份文档体系对**同一符号**给两种分母：代码与 FROZEN 正本 `docs/science/DRIZZLE.md:44-50` 为 `w_jp = a_jp/A_drop,j`（其 §10 禁止项逐字写着"把核权重写回 `a_jp/A_pixel,j`（FZ-COND-FLUX-CONSERV 判红）"）；而 `DATA_SEMA | 唯一实现口径是 `a_jp/A_drop`；`w'_jp = a_jp/A_pixel` 只作为**等价参数化**成立，且必须同时换分母（`N'_p = Σ w'·A_pixel = D_p`）。等价性已独立复核：发布 `S_p` 与 `variance_p` 对两种参数化**不变**（分子分母各乘 `pf⁴` 相消）；但 `Σ_p w_jp = 1`（⇒ `Σ_p F_p = Σ_j x_j`、（同对象另有 1 条 D3 主张） | AUD-101-DA02-算法推导.md×9、AUD-101-DB01.md×9、AUD-101-DB-06-07.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-04.md×3、AUD-101-DB-10-补.md×3、AUD-101-DA01-根规范与科学.md×2、AUD-101-DB-03.md×2、AUD-204-面积交叠核验.md×2、AUD-101-DB-13.md×1、AUD-402-判读-BD1.md×1、AUD-403-注释与README.md×1 | DB13·W1[PASS] DB13·W4[PASS/P1] 合同层·W1[PASS] | 入库 |
| `eng/ci/check_product_contract.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-501-门禁现状审计.md×3 | 合同层·W1[PASS] | 入库 |
| `eng/packaging/config/config_registry.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-03.md×2、AUD-101-DB-06-07.md×1、AUD-401-架构对齐.md×1、AUD-402-判读-A3.md×1 | AUD203·V3[PASS/P2] 合同层·W3[PASS] | 入库 |
| `eng/tests/config/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W3[PASS] | 入库 |
| `eng/tests/contracts/test_unified_object_contract.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W1[PASS] | 入库 |
| `lib/algorithms/resample/p3_rsmp_units.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-06-07.md×1 | 合同层·W1[PASS] | 入库 |
| `lib/infrastructure/aio/v6/src/v6_bunit.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W1[PASS] | 入库 |
| `lib/algorithms/drizzle/healpix_drizzle/v6_drizzle_science.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W1[PASS] | 入库 |
| `eng/ci/checks.json` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿（同对象另有 1 条 D3 主张） | AUD-501-门禁现状审计.md×17、AUD-101-D1补三份.md×11、AUD-101-DB-19.md×6、D9-工单对账-补.md×6、AUD-101-DB-10-补.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-12.md×4、AUD-101-DA02-算法推导.md×3、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-20.md×1、AUD-401-架构对齐.md×1、AUD-403-注释与README.md×1 | AUD201·V6[PASS] AUD204·W2[PASS] DB13·W1[PASS] DB13·W2[PASS] 合同层·W3[PASS] 合同层·W4[PASS] 结果层与收口层·R1[PASS/P1] 负责人面与索引·V1[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `lib/infrastructure/aio/v6/src/v6_provenance.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W1[PASS] | 入库 |
| `eng/tests/unit/v6_p1_drz/v6_p1_drz_test.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W1[PASS] | 入库 |
| `eng/contracts/schemas/unified/examples/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | — | 入库 |

## 2 依据

「复核-合同层」W1（判定：确认，但成稿事实面不完整 —— 冲突不是「说明层 vs 机器层」而是「机器层内部两个事实源同名互斥」）、W3（判定：确认＋定级上调 —— 性质不是「抄了旧快照」，是「越位成第二套登记面」，且其复制对象自身也已漂移；按实现逐位复算，在册门 `UT-CONFIG` 在当前 HEAD 必红）。

## 3 改法（具体动作，动词开头）

1. 为每个同名键指定唯一源，其余面改为指针；`variance.schema.json` 与 `unified_object_registry.json` 二者取一并删除另一处的数值
2. 把 `CONFIG_CONTRACT.md` 的数值快照整体退位为指针 + 校验规则（合同里不该出现可漂移的计数值），并同步其复制对象 `config_registry.json` 的漂移
3. 补「合同文本不得含可漂移计数值」的机检判据（按第②层给出的可机检替代判据）
4. `UT-CONFIG` 与其数据同批改：门与数据同批，否则出现「改了校验器数据全红」或「改了数据校验器全红」的中间态

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/contracts/schemas/unified/variance.schema.json
docs/contracts/unified_object_registry.json
eng/contracts/data/v6_clause_registry_v1.json
eng/contracts/schemas/product_family_field_constraints.schema.json
docs/contracts/CONFIG_CONTRACT.md
docs/contracts/DATA_SEMANTICS.md
eng/ci/check_product_contract.py
eng/packaging/config/config_registry.json
eng/tests/config/
eng/tests/contracts/test_unified_object_contract.py
lib/algorithms/resample/p3_rsmp_units.cpp
lib/infrastructure/aio/v6/src/v6_bunit.cpp
lib/algorithms/drizzle/healpix_drizzle/v6_drizzle_science.cpp
eng/ci/checks.json
lib/infrastructure/aio/v6/src/v6_provenance.cpp
eng/tests/unit/v6_p1_drz/v6_p1_drz_test.cpp
eng/contracts/schemas/unified/examples/
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T00 批 2 的字段自描述（同一条 `STEP_REQUIRED` 面）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - 死结形态登记：`UT-CONFIG` 门 + `CONFIG_CONTRACT.md` + `config_registry.json` 三者必须同一提交
  - `DATA_SEMANTICS.md` 由 T08/T21/T22/T27/T28/T33/T36/T37/T38 共改 ⇒ 必须由一个簇领改，其余簇改指针

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 两侧事实源同名不同值 ⇒ 合同一致性门判红（现状：判绿）
- 合同文本出现计数值（如 `docs/contracts/CONFIG_CONTRACT.md` 的 field_count 一类） ⇒ 新机检判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以文档说明层「解释」机器层互斥而保留两源；不得把 `UT-CONFIG` 现有红灯以豁免覆盖

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/contracts/schemas/unified/variance.schema.json` | AUD-101-DB-06-07.md:101;AUD-101-DB-06-07.md:139;AUD-101-DB-06-07.md:143 | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `docs/contracts/unified_object_registry.json` | AUD-101-DB-09.md:194;AUD-101-DB-09.md:232;AUD-101-DB-09.md:234 | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `eng/contracts/data/v6_clause_registry_v1.json` | AUD-101-DB-04.md:547;AUD-101-DB-04.md:94;AUD-101-DB-06-07.md:97;AUD-101-DB-13.md:157;D9-工单对账.md:235;D9-工单对账.md:258;D9-工单对账.md:331;D9-工单对账.md:332 | DB13·W1：确认 ‖ 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `eng/contracts/schemas/product_family_field_constraints.schema.json` | — | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `docs/contracts/CONFIG_CONTRACT.md` | AUD-101-DB-04.md:189;AUD-101-DB-04.md:90;AUD-101-DB-06-07.md:174;AUD-101-DB-06-07.md:35;AUD-101-DB-08.md:245;AUD-101-DB-08.md:265;AUD-101-DB-08.md:301;AUD-101-DB-10.md:59;AUD-101-DB-11.md:408;AUD-101-DB-11.md:411;AUD-101-DB-11.md:412;AUD-101-DB-11.md:413… | 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 |
| `docs/contracts/DATA_SEMANTICS.md` | AUD-101-DA01-根规范与科学.md:359;AUD-101-DA01-根规范与科学.md:607;AUD-101-DA02-算法推导.md:1052;AUD-101-DA02-算法推导.md:1055;AUD-101-DA02-算法推导.md:1323;AUD-101-DA02-算法推导.md:1750;AUD-101-DA02-算法推导.md:2134;AUD-101-DA02-算法推导.md:2204;AUD-101-DA02-算法推导.md:450;AUD-101-DA02-算法推导.md:772;AUD-101-DA02-算法推导.md:803;AUD-101-DB-03.md:38… | DB13·W1：确认 ‖ DB13·W4：确认（可结案，不必上呈） ‖ 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `eng/ci/check_product_contract.py` | AUD-501-门禁现状审计.md:364;AUD-501-门禁现状审计.md:366;AUD-501-门禁现状审计.md:379 | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `eng/packaging/config/config_registry.json` | AUD-101-DB-03.md:416;AUD-101-DB-03.md:671;AUD-101-DB-06-07.md:56;AUD-401-架构对齐.md:256;AUD-402-判读-A3.md:45 | AUD203·V3：确认 ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |
| `eng/tests/config/` | — | 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |
| `eng/tests/contracts/test_unified_object_contract.py` | — | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `lib/algorithms/resample/p3_rsmp_units.cpp` | AUD-101-DB-06-07.md:114 | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `lib/infrastructure/aio/v6/src/v6_bunit.cpp` | — | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `lib/algorithms/drizzle/healpix_drizzle/v6_drizzle_science.cpp` | — | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `eng/ci/checks.json` | AUD-101-D1残余.md:165;AUD-101-D1补三份.md:102;AUD-101-D1补三份.md:105;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:197;AUD-101-D1补三份.md:48;AUD-101-D1补三份.md:51;AUD-101-D1补三份.md:57;AUD-101-D1补三份.md:58;AUD-101-D1补三份.md:83;AUD-101-D1补三份.md:86;AUD-101-D1补三份.md:92… | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ DB13·W1：确认 ‖ DB13·W2：确认（头条数字降级） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `lib/infrastructure/aio/v6/src/v6_provenance.cpp` | — | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `eng/tests/unit/v6_p1_drz/v6_p1_drz_test.cpp` | — | 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `eng/contracts/schemas/unified/examples/` | — | — |

