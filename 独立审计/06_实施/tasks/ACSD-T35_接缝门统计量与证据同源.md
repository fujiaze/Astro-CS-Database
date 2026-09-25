# 任务：ACSD-T35 出厂接缝门含可相消的法向梯度项，与验收证据用的是两个统计量

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学+门禁 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/tools/e2e/seam_footprint.py` | — | （见 §2 依据） | （同对象另有 1 条 D3 主张） | AUD-101-DB-20.md×3、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-203-天光无缝核验.md×1 | AUD203·V2[PASS/P1] AUD204·W3[PASS] | 入库 |
| `实验/additive-sky-seamless/code/sci_c_common.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-18.md×2、AUD-203-天光无缝核验.md×1 | AUD203·V2[PASS/P1] | 入库 |
| `实验/additive-sky-seamless/README.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-301-文献池P1.md×6、AUD-101-DB-17.md×2、AUD-101-DB-20.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-04.md×1、AUD-203-天光无缝核验.md×1 | AUD203·V2[PASS/P1] 结果层与收口层·R4[PASS] | 入库 |
| `实验/additive-sky-seamless/REPORT_paper.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-301-文献池P1.md×3、AUD-101-DB-17.md×2、AUD-101-DB-20.md×1、AUD-301-文献复算-旧判批.md×1 | AUD203·V5[PASS/P2] 结果层与收口层·R5[PASS] | 入库 |
| `docs/science/PHASE2_UPM.md` | — | §1 把 `ivar=1/variance` 命名为"Phase2 逐像素科学权重"；§11 同一句既写"适用域=空背景随机分量"又写"直接入加权"（同句自相矛盾）；§4.6 称 HiPS 里"只存"帧级 SNR 与稀疏绝对 SNR | 三处都是**指称越界**，不是数学分歧：`ivar` 对天光建模与 UPM 控制点拟合是正确权重（那一组样本按 §5b 排异分层只取源掩膜外，其总方差即 `σ_bg²`，见 `PHASE2_UPM.md:21,:76`）；对阶段二叠加则须由 §5c 加权方差面给权。`07_noise_snr.md:201` 的"只存"与已定案的 variance/ivar 子产品（`DATA_SEMANTICS`（同对象另有 7 条 D3 主张） | AUD-101-DA01-根规范与科学.md×14、AUD-101-DA02-算法推导.md×10、AUD-101-DB-04.md×9、AUD-203-天光无缝核验.md×8、AUD-101-DB-20.md×5、AUD-101-DB-09.md×3、AUD-301-文献池P1.md×3、AUD-101-DB-18.md×2、AUD-101-D1残余.md×1、AUD-301-文献复算-旧判批.md×1、论文3-回执.md×1 | AUD202·V3[VOID] AUD203·V1[PASS/P1] AUD203·V2[PASS/P1] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] | 入库 |
| `eng/tests/backend/test_p2003_seam_oracle.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD203·V5[PASS/P2] | 入库 |
| `实验/additive-sky-seamless/results/c1_additive.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD203·V5[PASS/P2] | 入库 |
| `实验/additive-sky-seamless/results/c5_weights.json` | — | 曾按"W5 已验证四态"的乐观读法呈报 | 归档取值分布穷举（跟踪结果件 22 处命中）⇒ 观测到的收敛态只有 **{0,1}**；其中 4 个"2"**全部不是观测值**（`REVIEW.md:101/:171` 是叙述；`c5_weights.json:107` 在 `convergence_enum_spec` 图例里、`:186` 在 `W4_convergence_enum_spec` 门的 `value` 里且该门自带 `"le | — | — | 入库 |

## 2 依据

「复核-AUD203」V2（判定：确认，定级 P1）、V5（判定：确认，P2 证据缺口）；《天光无缝核验报告》DEV-02（门判据含可正可负的法向梯度项 ⇒ 相干结构读数可达门限 72.6%；分子是有符号量，梯度项与台阶代数相加、反号相消 ⇒ 实测门自己声明的确定性下限 1.005% 台阶被判绿，漏检面上推到 Δ/L ≈ 1.73%；「结构假阳性可区分」的证据属另一统计量 C4 N5 off-locus excess，而出货门 v2 已主动移除 off-locus）；AGENTS.md §9（能红能绿）。

## 3 改法（具体动作，动词开头）

1. 把「相干梯度无台阶必须绿」与反号相消算例补进 `REQUIRED_SELFTEST_CASES`（不动门限）
2. §17.3 增列「梯度反号相消」漏检面，并把「Δ/L>1.005% 必判红」限定为「梯度项为零时」
3. 验收证据里把「结构可区分」限定为需与 `step_net`、d 扫描联合判读，明写 C4 N5 的 off-locus 结论不自动迁移到 boundary-only 门
4. 订正 docstring 的 78% 与正本 §17.4 的 72.6%（同一处两个写法 = 口径未同步，取正本值）
5. 补 `stalled=2` 的可驱动 stall 夹具（正/负例）并复跑 c5 归档（跟踪结果件现观测到的收敛态只有 {0,1}，且四态码比归档数据晚一天落地）

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/tools/e2e/seam_footprint.py
实验/additive-sky-seamless/code/sci_c_common.py
实验/additive-sky-seamless/README.md
实验/additive-sky-seamless/REPORT_paper.md
docs/science/PHASE2_UPM.md
eng/tests/backend/test_p2003_seam_oracle.py
实验/additive-sky-seamless/results/c1_additive.json
实验/additive-sky-seamless/results/c5_weights.json
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T02（读数入库）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `PHASE2_UPM.md` 与 T02/T24/T29 共改 ⇒ 同批；`seam_footprint.py` 与 T42 共改

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 构造反号梯度与真台阶代数相消的夹具 ⇒ 门必须判红（现状静默判绿）
- 构造相干梯度无台阶 ⇒ 必须判绿（否则门误红）
- `stalled=2` 无夹具驱动 ⇒ 该态无证据资格，登记为待补
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得放松门限来掩盖相消面；不得把另一统计量的结论直接迁移为本门的证据

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/tools/e2e/seam_footprint.py` | AUD-101-DA01-根规范与科学.md:665;AUD-101-DB-10-补.md:89;AUD-101-DB-20.md:300;AUD-101-DB-20.md:338;AUD-101-DB-20.md:429;AUD-203-天光无缝核验.md:66 | AUD203·V2：确认 ‖ AUD204·W3：确认（并补两条成稿未报的更大分支；负责人待裁事项可据此定案） |
| `实验/additive-sky-seamless/code/sci_c_common.py` | AUD-101-DB-18.md:248;AUD-101-DB-18.md:95;AUD-203-天光无缝核验.md:83 | AUD203·V2：确认 |
| `实验/additive-sky-seamless/README.md` | AUD-101-DA01-根规范与科学.md:668;AUD-101-DB-04.md:37;AUD-101-DB-17.md:169;AUD-101-DB-17.md:178;AUD-101-DB-20.md:201;AUD-101-DB-20.md:206;AUD-203-天光无缝核验.md:8;AUD-301-文献池P1.md:124;AUD-301-文献池P1.md:144;AUD-301-文献池P1.md:240;AUD-301-文献池P1.md:259;AUD-301-文献池P1.md:849… | AUD203·V2：确认 ‖ 结果层与收口层·R4：确认（三处不一致全部复现），但定级按"是否进判据/对外主张"分档；成稿的 1 处对照口径需订正 |
| `实验/additive-sky-seamless/REPORT_paper.md` | AUD-101-DB-17.md:170;AUD-101-DB-17.md:207;AUD-101-DB-20.md:261;AUD-301-文献复算-旧判批.md:315;AUD-301-文献池P1.md:323;AUD-301-文献池P1.md:388;AUD-301-文献池P1.md:592 | AUD203·V5：确认 ‖ 结果层与收口层·R5：确认（占位与 5/7 结论行逐位复现），并补一条比成稿更重的观察 |
| `docs/science/PHASE2_UPM.md` | AUD-101-D1残余.md:111;AUD-101-DA01-根规范与科学.md:108;AUD-101-DA01-根规范与科学.md:193;AUD-101-DA01-根规范与科学.md:360;AUD-101-DA01-根规范与科学.md:421;AUD-101-DA01-根规范与科学.md:425;AUD-101-DA01-根规范与科学.md:436;AUD-101-DA01-根规范与科学.md:626;AUD-101-DA01-根规范与科学.md:63;AUD-101-DA01-根规范与科学.md:664;AUD-101-DA01-根规范与科学.md:666;AUD-101-DA01-根规范与科学.md:695… | AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ AUD203·V1：确认 ‖ AUD203·V2：确认 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 |
| `eng/tests/backend/test_p2003_seam_oracle.py` | — | AUD203·V5：确认 |
| `实验/additive-sky-seamless/results/c1_additive.json` | — | AUD203·V5：确认 |
| `实验/additive-sky-seamless/results/c5_weights.json` | — | — |

