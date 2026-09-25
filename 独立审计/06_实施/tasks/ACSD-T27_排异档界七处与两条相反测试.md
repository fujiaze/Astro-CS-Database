# 任务：ACSD-T27 排异三档正本跟改 7 处对侧口径，并修一条与正本相反的测试断言

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学+测试 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `docs/plugins/algorithms_phase2/12_rejection.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DA01-根规范与科学.md×5、AUD-101-DB-04.md×3、AUD-101-DB-05.md×2、AUD-101-DB-10-补.md×1、AUD-101-DB-13.md×1、AUD-101-DB02.md×1 | DB13·W3[PASS/P1] | 入库 |
| `docs/science/REJECTION.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DA02-算法推导.md×8、AUD-101-DB-04.md×6、AUD-101-DA01-根规范与科学.md×4、AUD-301-文献池P1.md×3、AUD-101-DB-06-07.md×1 | DB13·W3[PASS/P1] | 入库 |
| `docs/algorithms/PHASE2_REJECTION.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DA02-算法推导.md×2、AUD-301-文献复算-旧判批.md×2、AUD-301-文献池P1.md×2、AUD-101-DB02.md×1 | DB13·W3[PASS/P1] | 入库 |
| `docs/contracts/PUBLIC_API.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-09.md×6、AUD-101-DB01.md×5、AUD-101-DA02-算法推导.md×2、AUD-101-DB-05.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-11.md×1、AUD-402-判读-BD1.md×1、AUD-501-门禁现状审计.md×1、D9-工单对账.md×1 | DB13·W3[PASS/P1] DB13·W4[PASS/P1] | 入库 |
| `docs/design/PHASE2_DETAILED_DESIGN.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×3、AUD-101-DB-10.md×1 | DB13·W3[PASS/P1] 合同层·W4[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `docs/modules/phase2_rej.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB02.md×2、AUD-101-DB-04.md×1 | DB13·W3[PASS/P1] | 入库 |
| `docs/modules/registry/astrocs.phase2.reject.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB02.md×1 | DB13·W3[PASS/P1] | 入库 |
| `eng/tests/unit/p2_rejection_test.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DA02-算法推导.md×1 | DB13·W3[PASS/P1] | 入库 |
| `eng/tests/unit/p2002_unc_rej_prov_test.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | DB13·W3[PASS/P1] | 入库 |
| `lib/algorithms/coverage/src/rejection.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×1、AUD-202-SNR核验.md×1 | DB13·W3[PASS/P1] | 入库 |
| `docs/traceability/TRACEABILITY_MATRIX.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-13.md×7、AUD-101-DB-11.md×1、AUD-101-DB01.md×1 | DB13·W3[PASS/P1] | 入库 |
| `docs/traceability/TRACEABILITY_MATRIX.csv` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-13.md×3、AUD-101-DB-09.md×2、AUD-101-DB-11.md×2 | DB13·W3[PASS/P1] | 入库 |
| `ASTROCS_DESIGN.md` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_（同对象另有 11 条 D3 主张） | — | — | 入库 |

## 2 依据

「复核-DB13」W3（判定：确认，范围与严重度上修：对侧 7 处而非成稿的 2 处，含现行合同 `PUBLIC_API.md:1376-1378`、两份模块页、`PHASE2_REJECTION.md:90` 同文件自我否定、`rejection.cpp:1129-1131` 注释与代码互否）；唯一正本 = `12_rejection.md` §9 三档（`ASTROCS_DESIGN.md:422` 点名并自证「本节不复制档界与取值」），现行代码走三档 ⇒ 口径不需裁决。

## 3 改法（具体动作，动词开头）

1. 按正本跟改 7 处对侧口径（文档 + 合同 + 模块页 + 注释）
2. 修 `p2002_unc_rej_prov_test.cpp:550-551` 的相反断言（系 M3 提交 `139a2bc4` 漏改），使其与 `p2_rejection_test.cpp:212-218` 对同一 profile 同一入口一致
3. 订正成稿给出的 `ASTROCS_DESIGN.md:420 → :422` 锚（`:420` 实为「路由依据 N = 该输出像素的几何可贡献帧数」）
4. 登记实跑前置（本只读阶段禁跑）：`python3 eng/tools/monitoring/mem_guard.py --max-rss-gb 4 -- ctest --test-dir build -R ^(p2002_unc_rej_prov|p2_rejection)$ --output-on-failure`

## 4 文件域（本任务允许触碰的路径集合）

```text
docs/plugins/algorithms_phase2/12_rejection.md
docs/science/REJECTION.md
docs/algorithms/PHASE2_REJECTION.md
docs/contracts/PUBLIC_API.md
docs/design/PHASE2_DETAILED_DESIGN.md
docs/modules/phase2_rej.md
docs/modules/registry/astrocs.phase2.reject.md
eng/tests/unit/p2_rejection_test.cpp
eng/tests/unit/p2002_unc_rej_prov_test.cpp
lib/algorithms/coverage/src/rejection.cpp
docs/traceability/TRACEABILITY_MATRIX.json
docs/traceability/TRACEABILITY_MATRIX.csv
ASTROCS_DESIGN.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 无前置（口径六面单向，第②层已定案，不上呈）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `PUBLIC_API.md` 由 T27/T28/T36 共改 ⇒ 同批；`ASTROCS_DESIGN.md` 由 T00/T20/T21/T27 共改 ⇒ 见冲突表

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 修好判据：`git grep` 对侧 0 命中；两条 ctest 同跑全绿（现状对同一入口断言相反，必有一条红）
- 在任一文档写回两档口径 ⇒ 排异档界一致性门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得把「两条测试相反」以 SKIP 消化；不得为让测试变绿而改档界取值

## 8 登记与边界

第②层定级口径：实跑确认前 P1，确认为红则 P0 —— 本任务书按 P1 登记并挂复测项。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `docs/plugins/algorithms_phase2/12_rejection.md` | AUD-101-DA01-根规范与科学.md:487;AUD-101-DA01-根规范与科学.md:488;AUD-101-DA01-根规范与科学.md:489;AUD-101-DA01-根规范与科学.md:493;AUD-101-DA01-根规范与科学.md:494;AUD-101-DB-04.md:61;AUD-101-DB-04.md:65;AUD-101-DB-04.md:72;AUD-101-DB-05.md:242;AUD-101-DB-05.md:243;AUD-101-DB-10-补.md:72;AUD-101-DB-13.md:145… | DB13·W3：确认（范围与严重度上修） |
| `docs/science/REJECTION.md` | AUD-101-DA01-根规范与科学.md:478;AUD-101-DA01-根规范与科学.md:482;AUD-101-DA01-根规范与科学.md:66;AUD-101-DA01-根规范与科学.md:747;AUD-101-DA02-算法推导.md:129;AUD-101-DA02-算法推导.md:130;AUD-101-DA02-算法推导.md:154;AUD-101-DA02-算法推导.md:1639;AUD-101-DA02-算法推导.md:1643;AUD-101-DA02-算法推导.md:1679;AUD-101-DA02-算法推导.md:2169;AUD-101-DA02-算法推导.md:2284… | DB13·W3：确认（范围与严重度上修） |
| `docs/algorithms/PHASE2_REJECTION.md` | AUD-101-DA02-算法推导.md:1632;AUD-101-DA02-算法推导.md:2207;AUD-101-DB02.md:75;AUD-301-文献复算-旧判批.md:209;AUD-301-文献复算-旧判批.md:325;AUD-301-文献池P1.md:79;AUD-301-文献池P1.md:83 | DB13·W3：确认（范围与严重度上修） |
| `docs/contracts/PUBLIC_API.md` | AUD-101-DA02-算法推导.md:1057;AUD-101-DA02-算法推导.md:116;AUD-101-DB-05.md:382;AUD-101-DB-09.md:195;AUD-101-DB-09.md:261;AUD-101-DB-09.md:3;AUD-101-DB-09.md:43;AUD-101-DB-09.md:45;AUD-101-DB-09.md:49;AUD-101-DB-10-补.md:147;AUD-101-DB-11.md:102;AUD-101-DB01.md:125… | DB13·W3：确认（范围与严重度上修） ‖ DB13·W4：确认（可结案，不必上呈） |
| `docs/design/PHASE2_DETAILED_DESIGN.md` | AUD-101-DB-08.md:242;AUD-101-DB-08.md:298;AUD-101-DB-08.md:309;AUD-101-DB-10.md:26 | DB13·W3：确认（范围与严重度上修） ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `docs/modules/phase2_rej.md` | AUD-101-DB-04.md:17;AUD-101-DB02.md:358;AUD-101-DB02.md:71 | DB13·W3：确认（范围与严重度上修） |
| `docs/modules/registry/astrocs.phase2.reject.md` | AUD-101-DB02.md:354 | DB13·W3：确认（范围与严重度上修） |
| `eng/tests/unit/p2_rejection_test.cpp` | AUD-101-DA02-算法推导.md:1671 | DB13·W3：确认（范围与严重度上修） |
| `eng/tests/unit/p2002_unc_rej_prov_test.cpp` | — | DB13·W3：确认（范围与严重度上修） |
| `lib/algorithms/coverage/src/rejection.cpp` | AUD-101-DB-11.md:416;AUD-202-SNR核验.md:411 | DB13·W3：确认（范围与严重度上修） |
| `docs/traceability/TRACEABILITY_MATRIX.json` | AUD-101-DB-11.md:450;AUD-101-DB-13.md:117;AUD-101-DB-13.md:355;AUD-101-DB-13.md:79;AUD-101-DB-13.md:84;AUD-101-DB-13.md:88;AUD-101-DB-13.md:92;AUD-101-DB-13.md:93;AUD-101-DB01.md:653 | DB13·W3：确认（范围与严重度上修） |
| `docs/traceability/TRACEABILITY_MATRIX.csv` | AUD-101-DB-09.md:181;AUD-101-DB-09.md:252;AUD-101-DB-11.md:165;AUD-101-DB-11.md:166;AUD-101-DB-13.md:354;AUD-101-DB-13.md:70;AUD-101-DB-13.md:74 | DB13·W3：确认（范围与严重度上修） |
| `ASTROCS_DESIGN.md` | — | — |

