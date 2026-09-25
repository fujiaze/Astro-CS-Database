# 任务：ACSD-T09 状态阶梯唯一词表回正本，对外发布结论补真值判据

> 波次 `W0` ｜ 杠杆分档 `P2` ｜ 整改域 文档+门禁 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/ci/check_version.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×2、AUD-403-注释与README.md×1 | 负责人面与索引·V1[PASS] | 入库 |
| `eng/ci/checks.json` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿（同对象另有 1 条 D3 主张） | AUD-501-门禁现状审计.md×17、AUD-101-D1补三份.md×11、AUD-101-DB-19.md×6、D9-工单对账-补.md×6、AUD-101-DB-10-补.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-12.md×4、AUD-101-DA02-算法推导.md×3、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-20.md×1、AUD-401-架构对齐.md×1、AUD-403-注释与README.md×1 | AUD201·V6[PASS] AUD204·W2[PASS] DB13·W1[PASS] DB13·W2[PASS] 合同层·W3[PASS] 合同层·W4[PASS] 结果层与收口层·R1[PASS/P1] 负责人面与索引·V1[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `eng/ci/tests/test_ci001_failclosed.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-12.md×1、AUD-101-DB-20.md×1 | 负责人面与索引·V1[PASS] | 入库 |
| `eng/tools/quality/check_conclusion_truth.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 负责人面与索引·V1[PASS] | 入库 |
| `docs/standards/DOCUMENTATION_STANDARD.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DA01-根规范与科学.md×4、AUD-101-DB-11.md×2、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1 | 负责人面与索引·V1[PASS] | 入库 |
| `docs/owner/RELEASE_STATUS.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-12.md×9、AUD-101-DB-10-补.md×5、AUD-101-DA01-根规范与科学.md×4、AUD-101-DB-20.md×3、AUD-403-注释与README.md×3、AUD-101-DB-11.md×2 | 负责人面与索引·V1[PASS] 负责人面与索引·V2[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `docs/KNOWN_LIMITATIONS.md` | — | 生产 leaf = 整数格点四角的**大圆弦四边形**（`nside≥256 ⇒ nb=4`；`9≤nside<256` 亦只 4 角；生产 nside 钳位 `[16, 2²²]` 内**无任何路径**用真曲线边界）；冻结预算 `arc-chord 1e-6·hp_res`；`subdivide_healpix_edge` 注释自述"对 | 冻结预算被违反 **2–5 个数量级**且是**全天空现象**：矢高 max `8.094e−2·hp_res`（极冠，尺度不变）、`6.587e−4`（缝带）、`1.443e−4`（赤道），99.55% 的边超阈；逐叶面积误差 **−9.97%…+0.54%**（两路独立实现同值）且**不随 nside 收缩**；`:558-562` 另自述旧口径 `hp_res·1e-12` 对非大圆弧边"永 | AUD-101-DB-04.md×8、AUD-101-DB-11.md×7、AUD-101-DA01-根规范与科学.md×3、AUD-101-DB01.md×3、D9-工单对账.md×2、AUD-101-D1残余.md×1、AUD-101-DB-19.md×1、AUD-402-判读-A1.md×1、AUD-402-判读-A2.md×1、D9-工单对账-补.md×1 | AUD204·W2[PASS] 负责人面与索引·V1[PASS] | 入库 |
| `artifacts/evidence/audit-2026-01/FIX_LEDGER.csv` | — | 证据指针 `lib/photometric_calib/**`；标定读数"各支路 p95 ≤0.040 px"只活在正文 | `git ls-files lib/photometric_calib` = 0 ⇒ 跟踪台账的证据指针指向未跟踪旧路径；该标定读数的原始件在 `run/ci/ctest/`（不入库） | AUD-101-DA02-算法推导.md×2、AUD-101-D1补三份.md×1、AUD-101-DB01.md×1、AUD-201-测光核验.md×1、D9-工单对账.md×1 | AUD201·V6[PASS] 负责人面与索引·V1[PASS] | 不可复核（读数件未入库） |
| `artifacts/evidence/audit-2026-01/OWNER_DECISIONS.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 负责人面与索引·V4[PASS] | 不可复核（读数件未入库） |
| `docs/modules/MODULE_MAP.yaml` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB01.md×11、AUD-101-DB-03.md×6、AUD-101-DB-04.md×3、AUD-101-DB-11.md×3、AUD-101-D1残余.md×2、AUD-101-DB-19.md×2、AUD-101-DB-12.md×1 | 负责人面与索引·V1[PASS] | 入库 |
| `docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-12.md×3 | 负责人面与索引·V1[PASS] | 入库 |
| `docs/owner/ARCHITECTURE_OVERVIEW.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×8、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-04.md×1 | 负责人面与索引·V1[PASS] | 入库 |
| `docs/owner/PIPELINE_OVERVIEW.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×4 | 负责人面与索引·V1[PASS] 负责人面与索引·V2[PASS] | 入库 |

## 2 依据

「复核-负责人面与索引」V1（判定：降级 —「加判据」与「发布结论越界」成立；「自造状态词百级传染」与「闭环」两处口径不成立，需按实测重写，实测 `files=153 occ=517`）、V4（判定：确认 — 缺陷成立、该判据的 PASS 资格现在不成立，但「四档并存」的定性要收窄）；《索引重建规格》§7（现 `status_legal` 校验的是自定四态分类，与 §12.5 状态阶梯混名）；AGENTS.md §6（不宣布发布）。

## 3 改法（具体动作，动词开头）

1. 把索引/登记面的自造状态词与 §12.5 状态阶梯解名：`status_legal` 改名，状态词表交回 §12.5 正本供给
2. `check_conclusion_truth.py` 补「结论句必须指向跟踪读数件」的实读断言，越界发布结论判红
3. `RELEASE_STATUS.md` 里三档并存的一条审稿结论（SCI-B / I2 fail-closed 对拍）按 `OWNER_DECISIONS.md` 定案并写唯一档
4. 按第②层实测重写规模口径：登记计数 + 命令，不写「百级传染」这类未复算措辞
5. `FIX_LEDGER.csv` 只当线索（旧台账不作出证），其证据路径由 ACSD-T02 的入库门接管

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/ci/check_version.py
eng/ci/checks.json
eng/ci/tests/test_ci001_failclosed.py
eng/tools/quality/check_conclusion_truth.py
docs/standards/DOCUMENTATION_STANDARD.md
docs/owner/RELEASE_STATUS.md
docs/KNOWN_LIMITATIONS.md
artifacts/evidence/audit-2026-01/FIX_LEDGER.csv
artifacts/evidence/audit-2026-01/OWNER_DECISIONS.md
docs/modules/MODULE_MAP.yaml
docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md
docs/owner/ARCHITECTURE_OVERVIEW.md
docs/owner/PIPELINE_OVERVIEW.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T10 C5/C6（role 与 canonical 判据）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `docs/owner/*` 与 W3 批量字句波共改 ⇒ 结构改动先于措辞收口

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 把一条对外「已验收」结论接到无跟踪读数支撑的证据 ⇒ 判红
- 在索引里写一个 §12.5 词表外的状态值 ⇒ 判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得由 agent 或本任务书宣布发布与版本号（AGENTS.md §6/§10）

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/ci/check_version.py` | AUD-101-DB-11.md:487;AUD-101-DB-11.md:506;AUD-403-注释与README.md:62 | 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `eng/ci/checks.json` | AUD-101-D1残余.md:165;AUD-101-D1补三份.md:102;AUD-101-D1补三份.md:105;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:197;AUD-101-D1补三份.md:48;AUD-101-D1补三份.md:51;AUD-101-D1补三份.md:57;AUD-101-D1补三份.md:58;AUD-101-D1补三份.md:83;AUD-101-D1补三份.md:86;AUD-101-D1补三份.md:92… | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ DB13·W1：确认 ‖ DB13·W2：确认（头条数字降级） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `eng/ci/tests/test_ci001_failclosed.py` | AUD-101-DB-12.md:75;AUD-101-DB-20.md:104 | 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `eng/tools/quality/check_conclusion_truth.py` | — | 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `docs/standards/DOCUMENTATION_STANDARD.md` | AUD-101-DA01-根规范与科学.md:126;AUD-101-DA01-根规范与科学.md:175;AUD-101-DA01-根规范与科学.md:282;AUD-101-DA01-根规范与科学.md:789;AUD-101-DB-10-补.md:407;AUD-101-DB-10.md:81;AUD-101-DB-11.md:123;AUD-101-DB-11.md:145 | 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `docs/owner/RELEASE_STATUS.md` | AUD-101-DA01-根规范与科学.md:229;AUD-101-DA01-根规范与科学.md:241;AUD-101-DA01-根规范与科学.md:283;AUD-101-DA01-根规范与科学.md:284;AUD-101-DB-10-补.md:257;AUD-101-DB-10-补.md:338;AUD-101-DB-10-补.md:339;AUD-101-DB-10-补.md:342;AUD-101-DB-10-补.md:344;AUD-101-DB-11.md:204;AUD-101-DB-11.md:513;AUD-101-DB-12.md:114… | 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 负责人面与索引·V2：降级**（锚确实指到无关内容、真定义点在别处，已我自己定位 3 组；但**成稿给的根因不成立**： ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `docs/KNOWN_LIMITATIONS.md` | AUD-101-D1残余.md:264;AUD-101-DA01-根规范与科学.md:685;AUD-101-DA01-根规范与科学.md:690;AUD-101-DA01-根规范与科学.md:703;AUD-101-DB-04.md:130;AUD-101-DB-04.md:137;AUD-101-DB-04.md:149;AUD-101-DB-04.md:331;AUD-101-DB-04.md:428;AUD-101-DB-04.md:506;AUD-101-DB-04.md:570;AUD-101-DB-04.md:571… | AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `artifacts/evidence/audit-2026-01/FIX_LEDGER.csv` | AUD-101-D1补三份.md:69;AUD-101-DA02-算法推导.md:916;AUD-101-DA02-算法推导.md:985;AUD-101-DB01.md:111;AUD-201-测光核验.md:268;D9-工单对账.md:569 | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `artifacts/evidence/audit-2026-01/OWNER_DECISIONS.md` | — | 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `docs/modules/MODULE_MAP.yaml` | AUD-101-D1残余.md:159;AUD-101-D1残余.md:316;AUD-101-DB-03.md:129;AUD-101-DB-03.md:197;AUD-101-DB-03.md:201;AUD-101-DB-03.md:296;AUD-101-DB-03.md:695;AUD-101-DB-03.md:696;AUD-101-DB-04.md:208;AUD-101-DB-04.md:213;AUD-101-DB-04.md:227;AUD-101-DB-11.md:102… | 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md` | AUD-101-DB-12.md:242;AUD-101-DB-12.md:244;AUD-101-DB-12.md:31 | 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `docs/owner/ARCHITECTURE_OVERVIEW.md` | AUD-101-DA01-根规范与科学.md:247;AUD-101-DB-04.md:306;AUD-101-DB-11.md:474;AUD-101-DB-11.md:477;AUD-101-DB-11.md:481;AUD-101-DB-11.md:491;AUD-101-DB-11.md:549;AUD-101-DB-11.md:597;AUD-101-DB-11.md:614;AUD-101-DB-11.md:677 | 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `docs/owner/PIPELINE_OVERVIEW.md` | AUD-101-DB-11.md:104;AUD-101-DB-11.md:496;AUD-101-DB-11.md:500;AUD-101-DB-11.md:678 | 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 负责人面与索引·V2：降级**（锚确实指到无关内容、真定义点在别处，已我自己定位 3 组；但**成稿给的根因不成立**： |

