# 任务：ACSD-T38 裸形态是否允许没有产品级索引：五处口径 + 同一判据两侧相反，合同自身条款互斥到不可同时满足

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 合同 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×8、AUD-101-DB-04.md×2、AUD-101-DB-12.md×2、AUD-101-DB01.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-101-DB-14.md×1 | 合同层·W4[PASS] 合同层·W5[PASS] | 入库 |
| `docs/design/PRODUCT_STORAGE_FORM.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×9、AUD-101-DB-12.md×3、AUD-101-DB-10-补.md×2、AUD-101-DA01-根规范与科学.md×1 | 合同层·W4[PASS] 合同层·W5[PASS] | 入库 |
| `eng/contracts/schemas/hips_storage_form.schema.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×4 | 合同层·W4[PASS] 合同层·W5[PASS] | 入库 |
| `eng/tools/hipsform/check_hips_storage_form.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×3、AUD-101-DB-10-补.md×1 | 合同层·W4[PASS] 合同层·W5[PASS] | 入库 |
| `eng/ci/ledgers/dead_config_keys.json` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 1 条 D3 主张） | AUD-202-SNR核验.md×1 | AUD202·V2[PASS] AUD202·V4[PASS] 合同层·W5[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `lib/infrastructure/cli/session_commands.h` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-402-判读-A3.md×2、AUD-402-判读-A1.md×1 | 合同层·W5[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `eng/tools/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×1、AUD-101-DB-17.md×1、AUD-401-架构对齐.md×1、AUD-501-门禁现状审计.md×1 | 合同层·W5[PASS] | 入库 |

## 2 依据

「复核-合同层」W5（判定：确认 —— 冲突成立且比成稿说的更硬：不是「三方」而是「五处口径 + 同一判据两侧相反」，其中合同自身条款互斥到不可同时满足）；标准 05 §3（读端不识别必要结构时 fail-closed）。

## 3 改法（具体动作，动词开头）

1. 先消掉合同自身互斥的两条条款（不可同时满足 = 结构缺陷，不是方向选择）
2. 把五处口径归一为一句正向约束：裸形态在 {允许|禁止} 无产品级索引，并点名读端行为
3. 把 `check_hips_storage_form.py` 的两侧相反判据改为同一方向的单一判据
4. 把 `session_commands.h` 的读端实现与合同声明比对，缺结构时判红

## 4 文件域（本任务允许触碰的路径集合）

```text
docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md
docs/design/PRODUCT_STORAGE_FORM.md
eng/contracts/schemas/hips_storage_form.schema.json
eng/tools/hipsform/check_hips_storage_form.py
eng/ci/ledgers/dead_config_keys.json
lib/infrastructure/cli/session_commands.h
eng/tools/
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 同批：ACSD-T37（同一 schema、同一文件）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `HIPS_STORAGE_FORM_CONTRACT.md` + 该 schema 由 T37/T38 必须同批；若归一出现互斥产品方向（HiPS 产品数据模型的破坏性变更）⇒ 按 AGENTS.md §10 上呈，不自行取舍

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 构造「裸形态 + 无产品级索引」输入 ⇒ 判据必须给出与声明一致的单一结论（现状两侧相反）
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得保留两侧相反判据中的任一侧

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md` | AUD-101-DA01-根规范与科学.md:678;AUD-101-DB-04.md:168;AUD-101-DB-04.md:170;AUD-101-DB-08.md:244;AUD-101-DB-08.md:266;AUD-101-DB-08.md:287;AUD-101-DB-08.md:300;AUD-101-DB-08.md:318;AUD-101-DB-08.md:334;AUD-101-DB-08.md:40;AUD-101-DB-08.md:46;AUD-101-DB-10-补.md:104… | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |
| `docs/design/PRODUCT_STORAGE_FORM.md` | AUD-101-DA01-根规范与科学.md:106;AUD-101-DB-08.md:240;AUD-101-DB-08.md:267;AUD-101-DB-08.md:296;AUD-101-DB-08.md:49;AUD-101-DB-08.md:51;AUD-101-DB-08.md:53;AUD-101-DB-08.md:69;AUD-101-DB-08.md:76;AUD-101-DB-08.md:79;AUD-101-DB-10-补.md:288;AUD-101-DB-10-补.md:444… | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |
| `eng/contracts/schemas/hips_storage_form.schema.json` | AUD-101-DB-08.md:235;AUD-101-DB-08.md:277;AUD-101-DB-08.md:375;AUD-101-DB-08.md:70 | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |
| `eng/tools/hipsform/check_hips_storage_form.py` | AUD-101-DB-08.md:251;AUD-101-DB-08.md:283;AUD-101-DB-08.md:72;AUD-101-DB-10-补.md:106 | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |
| `eng/ci/ledgers/dead_config_keys.json` | AUD-202-SNR核验.md:257 | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） ‖ AUD202·V4：确认（并补两条成稿没给的硬证据：结果件里 `snr` 出现 0 次；dense 两条生产者都不可达） ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `lib/infrastructure/cli/session_commands.h` | AUD-402-判读-A1.md:25;AUD-402-判读-A3.md:273;AUD-402-判读-A3.md:42 | 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `eng/tools/` | AUD-101-DB-08.md:48;AUD-101-DB-17.md:277;AUD-401-架构对齐.md:295;AUD-501-门禁现状审计.md:276 | 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |

