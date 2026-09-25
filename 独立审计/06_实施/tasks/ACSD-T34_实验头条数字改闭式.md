# 任务：ACSD-T34 归档头条取自 N=1000 单次 MC 实现值：改引用 seed 无关的闭式并补判据

> 波次 `W1` ｜ 杠杆分档 `P2` ｜ 整改域 实验 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `实验/absolute-snr/REPORT_paper.md` | — | 双计使 `σ_F` 高估 `+12.8%`（基准点）至 `+34.0%`（RN=50 最坏点） | 头条取的是 **N=1000 单次 MC 实现值**（`b2_noise_terms.py:184` 的分母是该 seed 那 1000 帧的实测散布），而同一 JSON 里就躺着 seed 无关的闭式 `pred_doublecount_bias = +14.5009%`。"同一物理点两个数"的成因**不是口径分歧**，是分母的样本噪声：四条同点轴（非两条）的分子极差 0.027%、分母极差 4 | AUD-101-DB-13.md×5、AUD-101-DB-20.md×3、AUD-101-DB-16.md×1、AUD-301-文献复算-旧判批.md×1 | AUD202-补·V6[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `实验/absolute-snr/code/b2_noise_terms.py` | — | 双计使 `σ_F` 高估 `+12.8%`（基准点）至 `+34.0%`（RN=50 最坏点） | 头条取的是 **N=1000 单次 MC 实现值**（`b2_noise_terms.py:184` 的分母是该 seed 那 1000 帧的实测散布），而同一 JSON 里就躺着 seed 无关的闭式 `pred_doublecount_bias = +14.5009%`。"同一物理点两个数"的成因**不是口径分歧**，是分母的样本噪声：四条同点轴（非两条）的分子极差 0.027%、分母极差 4 | AUD-202-SNR核验.md×1 | AUD202-补·V6[PASS] | 入库 |
| `实验/absolute-snr/results/b2_noise_terms.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-202-SNR核验.md×1 | AUD202-补·V6[PASS] 结果层与收口层·R3[PASS] | 入库 |
| `实验/absolute-snr/results/ctest_evidence.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-17.md×1 | AUD202-补·V6[PASS] | 入库 |
| `实验/absolute-snr/results/EXP06_TABLES.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-17.md×1 | 结果层与收口层·R3[PASS] | 入库 |
| `实验/absolute-snr/results/REVIEW.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-17.md×2、AUD-101-DB-16.md×1 | 负责人面与索引·V4[PASS] | 入库 |

## 2 依据

「复核-AUD202-补」V6（判定：确认，定级 P2 — 头条 +12.8% 是 N=1000 单次实现值，分母样本噪声 sd 2.94pp；同物理点四条轴四个数、极差 4.84pp；seed 无关的闭式 +14.5009% 就在同一 JSON 里；约束它的 `max_abs_diff`（7.00pp）判定方 0 处；本仓 `REPORT_paper.md:225/:371` 已自登记）。

## 3 改法（具体动作，动词开头）

1. 把引用面的数字改为同一 JSON 内的闭式值
2. 为 `max_abs_diff` 补一条真判红的面（判定方现为 0 处 = 恒真）
3. 在报告里点名四条轴与极差，不把单轴读数当物理量
4. 补「真值无双计 ⇒ 归零」负例（AGENTS.md §5 判据非退化）

## 4 文件域（本任务允许触碰的路径集合）

```text
实验/absolute-snr/REPORT_paper.md
实验/absolute-snr/code/b2_noise_terms.py
实验/absolute-snr/results/b2_noise_terms.json
实验/absolute-snr/results/ctest_evidence.md
实验/absolute-snr/results/EXP06_TABLES.md
实验/absolute-snr/results/REVIEW.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 无前置
- 无跨任务硬依赖，可独立执行与验收。

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 把闭式值换回单次实现值 ⇒ 判据必须判红（现状无判定方，不红）
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以「如实登记偏差」代替改正引用面；不得另立一条「未披露」性质的 P1（第②层已把成因降级）

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `实验/absolute-snr/REPORT_paper.md` | AUD-101-DB-13.md:186;AUD-101-DB-13.md:251;AUD-101-DB-13.md:255;AUD-101-DB-13.md:260;AUD-101-DB-13.md:329;AUD-101-DB-16.md:94;AUD-101-DB-20.md:183;AUD-101-DB-20.md:242;AUD-101-DB-20.md:247;AUD-301-文献复算-旧判批.md:96 | AUD202-补·V6：确认**（"头条取自单次 MC 实现值"成立；"最大偏差无人判"成立）。 ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `实验/absolute-snr/code/b2_noise_terms.py` | AUD-202-SNR核验.md:341 | AUD202-补·V6：确认**（"头条取自单次 MC 实现值"成立；"最大偏差无人判"成立）。 |
| `实验/absolute-snr/results/b2_noise_terms.json` | AUD-202-SNR核验.md:358 | AUD202-补·V6：确认**（"头条取自单次 MC 实现值"成立；"最大偏差无人判"成立）。 ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `实验/absolute-snr/results/ctest_evidence.md` | AUD-101-DB-17.md:34 | AUD202-补·V6：确认**（"头条取自单次 MC 实现值"成立；"最大偏差无人判"成立）。 |
| `实验/absolute-snr/results/EXP06_TABLES.md` | AUD-101-DB-17.md:31 | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `实验/absolute-snr/results/REVIEW.md` | AUD-101-DB-16.md:94;AUD-101-DB-17.md:310;AUD-101-DB-17.md:33 | 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |

