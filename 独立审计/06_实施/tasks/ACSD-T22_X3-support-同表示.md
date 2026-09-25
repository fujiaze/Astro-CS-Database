# 任务：ACSD-T22 support 分子分母同表示与逐叶分配的在册容差登记

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学+门禁 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp` | — | （见 §2 依据） | （同对象另有 1 条 D3 主张） | AUD-204-面积交叠核验.md×2 | — | 入库 |
| `lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-D1残余.md×1、AUD-101-DB-09.md×1、D9-工单对账-补.md×1 | — | 入库 |
| `docs/algorithms/DRIZZLE_GEOMETRY.md` | — | 生产面积算法唯一命名 = 球面逐边裁剪 ＋ "Van Oosterom & Strackee 扇形三角剖分" | 该文献在台账已判 `关联错`；按"平面三角形立体角公式"用于球面三角面积看似正确用法，但与判错面不同 ⇒ 适用性未复核（同对象另有 3 条 D3 主张） | D9-工单对账.md×8、AUD-204-面积交叠核验.md×4、AUD-301-文献复算-旧判批.md×3、AUD-101-DA01-根规范与科学.md×2、AUD-101-DA02-算法推导.md×2、AUD-101-DB-03.md×2、AUD-101-DB-17.md×1、AUD-101-DB01.md×1、AUD-301-文献池P1.md×1 | AUD204·W1[PASS] AUD204·W2[PASS] | 入库 |
| `eng/ci/checks.json` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿（同对象另有 1 条 D3 主张） | AUD-501-门禁现状审计.md×17、AUD-101-D1补三份.md×11、AUD-101-DB-19.md×6、D9-工单对账-补.md×6、AUD-101-DB-10-补.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-12.md×4、AUD-101-DA02-算法推导.md×3、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-20.md×1、AUD-401-架构对齐.md×1、AUD-403-注释与README.md×1 | AUD201·V6[PASS] AUD204·W2[PASS] DB13·W1[PASS] DB13·W2[PASS] 合同层·W3[PASS] 合同层·W4[PASS] 结果层与收口层·R1[PASS/P1] 负责人面与索引·V1[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |

## 2 依据

《链路间口径对表》§9 X3（support 的分子分母同表示问题：弦面积 ÷ 真叶面积）、X7（逐叶分配是否有在册门 + 逐叶几何门的容差登记缺失，`1e-6/1e-5/1e-7` 三处口径）；标准 05 §5。

## 3 改法（具体动作，动词开头）

1. 把 `astro_sphere_sink.cpp:330-332`、`:520-521` 的 support 分子分母统一到同一表示
2. 在 `DRIZZLE_GEOMETRY.md §9` 登记逐叶几何门的容差唯一值，消掉三处口径
3. 与 ACSD-T04 的注册动作同批提交（门名与判据同批，防自红）

## 4 文件域（本任务允许触碰的路径集合）

```text
lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp
lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt
docs/algorithms/DRIZZLE_GEOMETRY.md
eng/ci/checks.json
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T03；与 ACSD-T21 同批
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - T21/T22/T04 共改 `DRIZZLE_GEOMETRY.md` 与 `tests/CMakeLists.txt` ⇒ 三者同批或由 T21 统一领改

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 构造「分子弦面积、分母真叶面积」的错配注入 ⇒ support 判据判红
- 把逐叶容差写成第二个值 ⇒ 容差登记一致性门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得只改一处容差而留下另两处

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp` | AUD-204-面积交叠核验.md:135;AUD-204-面积交叠核验.md:141 | — |
| `lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt` | AUD-101-D1残余.md:115;AUD-101-DB-09.md:164;D9-工单对账-补.md:148 | — |
| `docs/algorithms/DRIZZLE_GEOMETRY.md` | AUD-101-DA01-根规范与科学.md:103;AUD-101-DA01-根规范与科学.md:470;AUD-101-DA02-算法推导.md:1527;AUD-101-DA02-算法推导.md:250;AUD-101-DB-03.md:511;AUD-101-DB-03.md:521;AUD-101-DB-17.md:296;AUD-101-DB01.md:724;AUD-204-面积交叠核验.md:115;AUD-204-面积交叠核验.md:147;AUD-204-面积交叠核验.md:6;AUD-204-面积交叠核验.md:66… | AUD204·W1：确认（并订正成稿两处口径） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） |
| `eng/ci/checks.json` | AUD-101-D1残余.md:165;AUD-101-D1补三份.md:102;AUD-101-D1补三份.md:105;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:197;AUD-101-D1补三份.md:48;AUD-101-D1补三份.md:51;AUD-101-D1补三份.md:57;AUD-101-D1补三份.md:58;AUD-101-D1补三份.md:83;AUD-101-D1补三份.md:86;AUD-101-D1补三份.md:92… | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ DB13·W1：确认 ‖ DB13·W2：确认（头条数字降级） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |

