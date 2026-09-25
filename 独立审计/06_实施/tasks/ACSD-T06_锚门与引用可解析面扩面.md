# 任务：ACSD-T06 三道锚门的结构性盲区：把任意路径形态与 doc_globs 覆盖面纳入判据

> 波次 `W0` ｜ 杠杆分档 `P1` ｜ 整改域 门禁+文档 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `docs/algorithms/anchors/check_doc_line_anchors.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-D1补三份.md×5、AUD-101-DA02-算法推导.md×1、AUD-101-DB-11.md×1 | 合同层·W2[PASS] 负责人面与索引·V2[PASS] | 入库 |
| `docs/algorithms/anchors/anchor_contract.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DA02-算法推导.md×2、AUD-101-DB02.md×2、AUD-101-D1残余.md×1、AUD-101-D1补三份.md×1、AUD-101-DB-12.md×1 | 负责人面与索引·V2[PASS] | 入库 |
| `docs/algorithms/anchors/unresolved_registry.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DA02-算法推导.md×3、AUD-101-DB-11.md×2、AUD-101-DB-12.md×2、AUD-101-DB-13.md×1 | 合同层·W2[PASS] | 入库 |
| `eng/ci/check_registration_anchors.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W2[PASS] | 入库 |
| `eng/ci/ledgers/registration_anchor_ledger.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-501-门禁现状审计.md×1 | 合同层·W2[PASS] | 入库 |
| `eng/ci/check_no_weight_mode.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-06-07.md×1 | DB13·W4[PASS/P1] 合同层·W2[PASS] | 入库 |
| `eng/tools/quality/check_path_domain_anchors.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W3[PASS] | 入库 |
| `docs/research/SNR_WEIGHT_RESEARCH_PACK.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-13.md×11、AUD-101-DB-16.md×5、AUD-202-SNR核验.md×2、AUD-301-文献复算-旧判批.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-11.md×1 | 合同层·W2[PASS] | 入库 |
| `reports/v6/contract-review/02_CONFLICT_AND_GAP_AUDIT.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W2[PASS] | 待核：该路径不在仓库跟踪集，先定对象再派工 |

## 2 依据

「复核-合同层」W2（判定：确认存在（引用悬空成立）＋定性降级为「锚号/出处失效」，不是「依据丢失」；并确认该引用面处在三道锚门的结构性盲区）；标准 05 §2、§5。

## 3 改法（具体动作，动词开头）

1. 把 `anchor_contract.json` 的 `doc_globs` 从只认 `docs/**` 扩到 `lib/**`、`实验/**`、`工程控制/**` 与根治理件（与索引门 C10 同一定义）
2. `EXTERNAL_REFERENCE` 类条目改判「指向的锚号/出处是否存在」，不再用「依据丢失」定性
3. `unresolved_registry.json` 回填实测未决项，注册即判红可见并计数
4. 把 `check_registration_anchors.py` 与 `check_path_domain_anchors.py` 合并到同一路径解析器，避免两侧口径
5. 为 `docs/research/SNR_WEIGHT_RESEARCH_PACK.md` 的外引指针补跟踪副本，或改标不可复核

## 4 文件域（本任务允许触碰的路径集合）

```text
docs/algorithms/anchors/check_doc_line_anchors.py
docs/algorithms/anchors/anchor_contract.json
docs/algorithms/anchors/unresolved_registry.json
eng/ci/check_registration_anchors.py
eng/ci/ledgers/registration_anchor_ledger.json
eng/ci/check_no_weight_mode.py
eng/tools/quality/check_path_domain_anchors.py
docs/research/SNR_WEIGHT_RESEARCH_PACK.md
reports/v6/contract-review/02_CONFLICT_AND_GAP_AUDIT.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 无前置；建议早于 ACSD-T02，令其指针门复用本任务产出的解析器
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - 与 ACSD-T02 同族（同一解析器）⇒ 同批或 T06 先；`check_no_weight_mode.py` 与 T20/T28 的权臂口径互相触发 ⇒ 同批

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 在 `lib/` 或 `工程控制/` 下写一条指向不存在行的 `路径:行` ⇒ 判红（现状：`doc_globs` 只含 `docs/` 而失明）
- 把锚号改成不存在的节号 ⇒ C7/C8 判红
- 清空扫描面 ⇒ 判红而非判绿（零对象守卫）
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以「已注册为未决」当作免罪：未决条目必须计数可见

## 8 登记与边界

`工程控制/RELEASE-02/GAP_AUDIT.md` 一类正文需从历史提交取回（`git show ae0f1e63^:工程控制/RELEASE-02/GAP_AUDIT.md`），属取证动作，不是整改对象。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `docs/algorithms/anchors/check_doc_line_anchors.py` | AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:131;AUD-101-D1补三份.md:21;AUD-101-D1补三份.md:76;AUD-101-D1补三份.md:80;AUD-101-DA02-算法推导.md:351;AUD-101-DB-11.md:113 | 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 ‖ 负责人面与索引·V2：降级**（锚确实指到无关内容、真定义点在别处，已我自己定位 3 组；但**成稿给的根因不成立**： |
| `docs/algorithms/anchors/anchor_contract.json` | AUD-101-D1残余.md:55;AUD-101-D1补三份.md:87;AUD-101-DA02-算法推导.md:2056;AUD-101-DA02-算法推导.md:280;AUD-101-DB-12.md:210;AUD-101-DB02.md:234;AUD-101-DB02.md:248 | 负责人面与索引·V2：降级**（锚确实指到无关内容、真定义点在别处，已我自己定位 3 组；但**成稿给的根因不成立**： |
| `docs/algorithms/anchors/unresolved_registry.json` | AUD-101-DA02-算法推导.md:2054;AUD-101-DA02-算法推导.md:2195;AUD-101-DA02-算法推导.md:319;AUD-101-DB-11.md:578;AUD-101-DB-11.md:94;AUD-101-DB-12.md:210;AUD-101-DB-12.md:232;AUD-101-DB-13.md:301 | 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |
| `eng/ci/check_registration_anchors.py` | — | 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |
| `eng/ci/ledgers/registration_anchor_ledger.json` | AUD-501-门禁现状审计.md:491 | 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |
| `eng/ci/check_no_weight_mode.py` | AUD-101-DB-06-07.md:97 | DB13·W4：确认（可结案，不必上呈） ‖ 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |
| `eng/tools/quality/check_path_domain_anchors.py` | — | 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |
| `docs/research/SNR_WEIGHT_RESEARCH_PACK.md` | AUD-101-DA01-根规范与科学.md:381;AUD-101-DB-11.md:637;AUD-101-DB-13.md:25;AUD-101-DB-13.md:280;AUD-101-DB-13.md:288;AUD-101-DB-13.md:29;AUD-101-DB-13.md:303;AUD-101-DB-13.md:312;AUD-101-DB-13.md:313;AUD-101-DB-13.md:33;AUD-101-DB-13.md:335;AUD-101-DB-13.md:352… | 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |
| `reports/v6/contract-review/02_CONFLICT_AND_GAP_AUDIT.md` | — | 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |

