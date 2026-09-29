# 档位判词判据书（TIER VERDICT CRITERIA）

> 上游：`docs/ASTROCS_DESIGN.md` §12.4（四层验收与验证层级）、`ACCEPTANCE_SPEC.md` §5/§6（L3/L4）
> 配套机器实现：`eng/tools/acceptance/tier_verdict_gate.py`（本文件 §3 逐条对应，TV-01..TV-12）
> 配套盘点与清单：`docs/engineering/FROZEN_GATE_INVENTORY.md`、`docs/engineering/REL790_VISUAL_EVIDENCE_CHECKLIST.md`
> 纪律：本文件**不改任何科学公式、阈值或冻结定义**，只规定「档位判词这句话怎么写才算有据」；
> 科学判据的数值正本仍在 `docs/science/` 与各 SCI/ALG 条款。

## 0 本文件解决什么

现状事实（不再论证）：

1. 49 帧档当前判词 = **NOT_VERIFIED**。5 帧小集 5 收（38/0.102、42/0.119、42/0.101、54/0.123、53/0.139）**不足以重判档位**，重判需全量端到端；
2. 端到端五帧小集现状：帧 1 由 28 对/0.6554 REJECT 变为 38 对/0.102 ACCEPT；
3. 一个已知门被冻结产物触发过：E2E 档 3 的 1/5 帧触发冻结门（DISP-WCS-001 / RESCUE F-9）；

4. 「块在首帧中止」易被误读成「5 帧全拒」，判定依据见
`run/FINAL-07/审核包/端到端/五帧越闸定性报告.md` §8.1（该处已给出以 5 次独立单帧运行为准的结论）。

本文件把那四条事实变成**可红可绿的判据**：以后任何人写档位判词，写错形态、写错分母、把子集证据当档位证据、把拒绝分不清产品行为还是缺陷，机器都会判红。

## 1 两条轴与两套判词

| 轴 | 键 | 分母 | 判词对象 |
|---|---|---|---|
| 帧数轴 | `frame_count` | `planned_frames`（计划帧 = 实际入册帧数） | 逐帧 ACCEPT/REJECT/NOT_EVALUATED |
| 覆盖度轴 | `coverage` | `total_px`（输出平面总像素） | `covered_px / total_px`、`finite_px / total_px` |

**两条轴必须同时报，且分母必须显式点名**。允许的分母词表（封闭，改词表=改判据）：
`evaluated_frames / planned_frames / total_px / covered_px / finite_px`。

### 1.1 帧数档阶梯（正本 = `run/FINAL-07/审核包/端到端/M42双平台端到端等价报告.md` §2.2）

| 档键 | 规模 | 阶段面 |
|---|---|---|
| `e2e_tier1_single_frame` | 单帧 normalize | normalize |
| `e2e_tier2_8f_mosaic` | 8 帧 normalize + mosaic | normalize + mosaic |
| `e2e_tier3_full` | 49 帧全量 normalize + mosaic + export | 三段全覆盖 |

## 2 判定条件（逐档）

| 判词 | 充分必要条件（全部满足） |
|---|---|
| `VERIFIED` | ① `n_evaluated == planned_frames`；② 阶段面覆盖该档要求；③ 无被拒帧；④ 无未消冻结门命中；⑤ 每个主张都在 `guaranteed_scope` 内 |
| `NOT_VERIFIED` | 证据不足以支撑 VERIFIED（如首帧中止、未跑满、阶段面缺段）——**这是默认值**，不是例外 |
| `BLOCKED` | 有具体阻塞条件（环境/上游门/负责人明令），且阻塞面已按原因码登记 |
| `NOT_ATTEMPTED` | 0 帧被求值，且 `not_attempted_reasons` 非空 |

**子集判词是独立词表**，不与档位判词混用：`SUBSET_VERIFIED / SUBSET_NOT_VERIFIED / SUBSET_REJECTED`。
子集证据**只能**支撑子集判词；把子集证据写成档位 `VERIFIED` 判红（TV-04）。

## 3 形态处置规则：「全部失败」还是「首帧中止」

这是本判据书最关键的一条。块以帧序执行、首帧失败即中止 ⇒ **未求值的帧不是「失败的帧」**。

| `block_outcome` | 硬性要求 |
|---|---|
| `first_frame_abort` | 必须给 `aborted_at.frame_id` + `frame_index`；`n_evaluated < planned`；`rejected != planned` |
| `all_failed` | **`n_evaluated == planned`**（每一帧都被实际求值并拒绝）、`accepted == 0`、`rejected == planned`，且 `aborted_at` 留空 |
| `partial` | 存在未评估帧且已评估部分有结论 |
| `complete` | 全部求值、`not_attempted == 0` |
| `not_attempted` | 0 帧求值 + 原因码齐全 |

写错形态的判红点：把首帧中止写成 `all_failed`（TV-03 N1）、把 `n_eval < planned` 的记录写成 `VERIFIED`（TV-04 N2）。

## 4 未尝试面怎么报

未尝试 ≠ 失败，也 ≠ 不提。逐项必填：`item / reason_code / owner / evidence`。
原因码封闭词表：`BLOCKED_UPSTREAM / OWNER_STANDDOWN / ENV_MISSING / RESOURCE / SCOPE_EXCLUDED / OTHER`（`OTHER` 必须在 `note` 写明）。`counts.not_attempted` 必须与逐帧 `NOT_EVALUATED` 权重对账。

## 5 证据面

每个 `evidence` 指针必须**存在、非空**；落在 `run/` 下的（过程产物可再生）**必须带内容 sha256**。
逐帧可按 `frame_ids` + `frame_count` 显式分组（组内同判词），但权重必须与 `counts` 对账。

## 6 主张范围

`claim_scope ⊆ guaranteed_scope` 是硬规则（TV-11）。判据能断言的范围必须与实际能保证范围一致：5 帧小集能保证「这 5 帧全 ACCEPT」，不能保证「49 帧档通过」。

## 7 12 条规则与可执行证据

| 规则 | 内容 | 注入负例 |
|---|---|---|
| TV-01 | 分母声明齐（planned/指纹/分母词表/阶段面/轴/档） | N3 |
| TV-02 | 计数守恒 accepted+rejected+not_attempted == planned | N4 |
| TV-03 | 形态判定自洽（首帧中止 ≠ 全部失败） | N1 |
| TV-04 | 子集证据只支撑子集判词，档位判词仍为 NOT_VERIFIED | N2 |
| TV-05 | 逐帧记录完备（frame_id/frame_ids + verdict + 证据） | — |
| TV-06 | 拒绝记录：门 ID / 原文原因 / 分类(非 unknown) / 出口 / 确定性 | N5 N6 N7 |

| 规则 | 内容 | 注入负例 |
|---|---|---|
| TV-07 | 未尝试面：原因码 + owner + 证据 | N8 |
| TV-08 | 证据指针可解析且非空；run/ 下带 sha256 | N9 |
| TV-09 | 覆盖度轴：covered∈(0,total]、finite<=total、total_px 已声明 | — |
| TV-10 | 报出比率可按声明分母复算 | N10 |
| TV-11 | claim_scope ⊆ guaranteed_scope | N11 |
| TV-12 | 存在未消冻结门命中或被拒帧时，VERIFIED 判红 | N12 |

## 8 怎么跑

~~~bash
python3 eng/tools/acceptance/tier_verdict_gate.py --self-test          # 自证 + 12 条注入负例
python3 eng/tools/acceptance/tier_verdict_gate.py --record <record.json>  # 裁决一份判词记录
python3 eng/tools/acceptance/tier_verdict_gate.py --record <record.json> --json-out <out.json>
~~~

真实事实夹具（改编自本轮 49 帧档事实）：
`eng/tools/acceptance/fixtures/tier_verdict_real_good.json` → 判绿（形态分清、判词 NOT_VERIFIED）；
`eng/tools/acceptance/fixtures/tier_verdict_real_bad.json` → 判红（形态误报 + 子集冒充档位 + 分类 unknown + 未证确定性）。

## 9 未决 / 交接

- 本门未注册进 `eng/ci/checks.json`（本轮硬约束禁改该文件）⇒ 由前台登记后再进 CI；
- `planned_frame_list_sha256` 的生成命令待定（候选：`find <root> -name '*Red.fts' | sort | sha256sum`），裁定后写入本文件 §5。
