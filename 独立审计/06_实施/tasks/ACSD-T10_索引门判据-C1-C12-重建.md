# 任务：ACSD-T10 文档索引判据重建：12 条检查各带零对象守卫与正负例，先于一切迁移

> 波次 `W0` ｜ 杠杆分档 `P0` ｜ 整改域 门禁+文档 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/tools/doccheck/check_doc_index.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-D1补三份.md×2、AUD-101-DB-19.md×2、AUD-101-DB-20.md×2、AUD-101-D1残余.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-101-DB-11.md×1 | 结果层与收口层·R5[PASS] 负责人面与索引·V3[PASS] | 入库 |
| `docs/DOCUMENT_INDEX.yaml` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-03.md×28、AUD-101-DB-11.md×20、AUD-101-DB-19.md×18、AUD-101-DA01-根规范与科学.md×13、AUD-101-D1残余.md×11、AUD-101-DB-12.md×11、AUD-101-DB01.md×11、AUD-101-DB-08.md×9、AUD-101-DB-10-补.md×8、AUD-101-DA02-算法推导.md×6、AUD-101-DB-05.md×6、AUD-101-DB-04.md×4、AUD-101-DB-20.md×4、AUD-101-D1补三份.md×3、AUD-101-DB-06-07.md×3、AUD-101-DB-14.md×3、AUD-101-DB-16.md×3、AUD-101-DB-17.md×3、AUD-101-DB-13.md×2、AUD-101-DB-18.md×2、AUD-101-DB-09.md×1、AUD-101-DB-15.md×1、AUD-402-判读-A3.md×1 | 负责人面与索引·V3[PASS] | 入库 |
| `docs/audit/doc_classification.csv` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×5、AUD-101-DB-04.md×2、AUD-101-D1补三份.md×1、AUD-101-DB02.md×1 | 负责人面与索引·V3[PASS] | 入库 |
| `docs/TRACEABILITY.csv` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×10、AUD-101-DB-04.md×5、AUD-101-DB-13.md×4、AUD-101-DA01-根规范与科学.md×2、AUD-101-DB-09.md×2、AUD-101-DA02-算法推导.md×1、AUD-101-DB-06-07.md×1、D9-工单对账-补.md×1 | DB13·W2[PASS] 负责人面与索引·V3[PASS] | 入库 |
| `eng/tools/doccheck/dangling_ledger.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-19.md×2 | 负责人面与索引·V3[PASS] | 入库 |
| `eng/ci/checks.json` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿（同对象另有 1 条 D3 主张） | AUD-501-门禁现状审计.md×17、AUD-101-D1补三份.md×11、AUD-101-DB-19.md×6、D9-工单对账-补.md×6、AUD-101-DB-10-补.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-12.md×4、AUD-101-DA02-算法推导.md×3、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-20.md×1、AUD-401-架构对齐.md×1、AUD-403-注释与README.md×1 | AUD201·V6[PASS] AUD204·W2[PASS] DB13·W1[PASS] DB13·W2[PASS] 合同层·W3[PASS] 合同层·W4[PASS] 结果层与收口层·R1[PASS/P1] 负责人面与索引·V1[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `reports/v19r2/source_manifest.csv` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-19.md×1、D9-工单对账.md×1 | 负责人面与索引·V3[PASS] | 入库 |
| `artifacts/evidence/doc-hygiene/baseline.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 负责人面与索引·V3[PASS] | 不可复核（读数件未入库） |
| `docs/API_REFERENCE.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×1 | 负责人面与索引·V3[PASS] | 需复测（对象不在跟踪集） |

## 2 依据

《索引重建规格》(D2) §6 判据清单 C1–C12、§7 与现有门的能力差、§8 事务顺序第 1–2 条；「复核-负责人面与索引」V3（判定：降级 —「争角色」不成立，唯一索引已把 `docs/audit/doc_classification.csv` 判为历史快照并声明权威在本索引）；标准 05 §2。

## 3 改法（具体动作，动词开头）

1. 按规格 §6 逐条实现 C1–C12，每条带零对象守卫、正例、负例，`--self-test` 夹具落临时目录不改仓库文件
2. 把登记对象集唯一锚定为 `git ls-files` 派生（含 `实验/`、`工程控制/`），排除面每条带 `reason ∈ {生成物, 证据快照, 第三方}` 且命中数与声明数一致
3. `upstream` 校验补「号—标题双要素」（C7/C8），修掉现状 73 条「号错标题也错」仍能长绿的形态
4. C10 扫描面按「任意路径形态」实现（含反引号内 `路径:行`、非 `docs/` 前缀如 `lib/`、`实验/`、`工程控制/`、根文件），替换现行只认 `docs/` 的 token 判据
5. 把 `docs/audit/doc_classification.csv` 按第②层结论定角色（历史快照、权威在本索引），不删文件、不改权威
6. 先落 C1–C4（对象集与口径），再落 C6 canonical，才允许启动《迁移合并清单》W1 起的任何迁移

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/tools/doccheck/check_doc_index.py
docs/DOCUMENT_INDEX.yaml
docs/audit/doc_classification.csv
docs/TRACEABILITY.csv
eng/tools/doccheck/dangling_ledger.json
eng/ci/checks.json
reports/v19r2/source_manifest.csv
artifacts/evidence/doc-hygiene/baseline.json
docs/API_REFERENCE.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 本任务是 ACSD-T07（过程层出库）与整个 W4 迁移波的前置（《索引重建规格》§8 第 1、2 条）
  - C11 写法词表（日期形态、流水号形态、散列长度阈值）留给 D7《门禁清单》细化 ⇒ 缺件时登记为「待 D7 合入」，本任务只落「命中即红 + 给行号」
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `docs/DOCUMENT_INDEX.yaml` 同时被 ACSD-T08/T09 与本任务改：同一文件的 role/status/登记面改动必须同批，否则互相覆盖
  - `docs/TRACEABILITY.csv` 与 ACSD-T39 共改 ⇒ 同批

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 删除一条登记 ⇒ C3 判红；把口径缩回只查 `docs/**` ⇒ C3 判红（口径判定见 C4）
- 复制一份同主题文档并同样标 `canonical: true` ⇒ C6 判红；一条都不标 ⇒ C6 判红
- 在任一文档写 `` `docs/不存在的页.md:12` `` ⇒ C10 判红
- 把 `downstream: auto` 的快照 255 手工改成 3 ⇒ C12 判红并输出差值
- 删除 §8.1 的「索引：」行 ⇒ C9 判红（现状 73/0 类断点即此形态）
- 把 `实验/` 写进排除面而不带理由 ⇒ C4 判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- `DOC-INDEX` 门不可 waiver；红灯不得以「索引未跟踪」或「解析器缺失」降级为跳过 —— 这两种情形本身即 C1/C2 的红

## 8 登记与边界

本任务只重建判据；实际迁移与登记动作在 W4 波，按《迁移合并清单》W0–W6 行级顺序执行。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/tools/doccheck/check_doc_index.py` | AUD-101-D1残余.md:356;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:133;AUD-101-DB-10-补.md:438;AUD-101-DB-10.md:71;AUD-101-DB-11.md:33;AUD-101-DB-19.md:584;AUD-101-DB-19.md:666;AUD-101-DB-20.md:32;AUD-101-DB-20.md:346 | 结果层与收口层·R5：确认（占位与 5/7 结论行逐位复现），并补一条比成稿更重的观察 ‖ 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |
| `docs/DOCUMENT_INDEX.yaml` | AUD-101-D1残余.md:127;AUD-101-D1残余.md:145;AUD-101-D1残余.md:160;AUD-101-D1残余.md:335;AUD-101-D1残余.md:387;AUD-101-D1残余.md:401;AUD-101-D1残余.md:406;AUD-101-D1残余.md:407;AUD-101-D1残余.md:435;AUD-101-D1残余.md:66;AUD-101-D1残余.md:97;AUD-101-D1补三份.md:128… | 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |
| `docs/audit/doc_classification.csv` | AUD-101-D1补三份.md:134;AUD-101-DB-04.md:364;AUD-101-DB-04.md:442;AUD-101-DB-11.md:211;AUD-101-DB-11.md:215;AUD-101-DB-11.md:35;AUD-101-DB-11.md:582;AUD-101-DB-11.md:663;AUD-101-DB02.md:168 | 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |
| `docs/TRACEABILITY.csv` | AUD-101-DA01-根规范与科学.md:450;AUD-101-DA01-根规范与科学.md:509;AUD-101-DA02-算法推导.md:2057;AUD-101-DB-04.md:383;AUD-101-DB-04.md:388;AUD-101-DB-04.md:427;AUD-101-DB-04.md:461;AUD-101-DB-04.md:499;AUD-101-DB-06-07.md:186;AUD-101-DB-09.md:150;AUD-101-DB-09.md:157;AUD-101-DB-11.md:111… | DB13·W2：确认（头条数字降级） ‖ 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |
| `eng/tools/doccheck/dangling_ledger.json` | AUD-101-DB-19.md:1364;AUD-101-DB-19.md:221 | 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |
| `eng/ci/checks.json` | AUD-101-D1残余.md:165;AUD-101-D1补三份.md:102;AUD-101-D1补三份.md:105;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:197;AUD-101-D1补三份.md:48;AUD-101-D1补三份.md:51;AUD-101-D1补三份.md:57;AUD-101-D1补三份.md:58;AUD-101-D1补三份.md:83;AUD-101-D1补三份.md:86;AUD-101-D1补三份.md:92… | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ DB13·W1：确认 ‖ DB13·W2：确认（头条数字降级） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `reports/v19r2/source_manifest.csv` | AUD-101-DB-19.md:259;D9-工单对账.md:499 | 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |
| `artifacts/evidence/doc-hygiene/baseline.json` | — | 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |
| `docs/API_REFERENCE.md` | AUD-101-DB-11.md:226 | 负责人面与索引·V3：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引； |

