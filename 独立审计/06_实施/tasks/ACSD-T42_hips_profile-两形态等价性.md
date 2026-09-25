# 任务：ACSD-T42 `hips_profile=0` 与 `=1` 在默认参数下不等价，误差来自 uint8 支撑格（前置：负责人裁-5）

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学+代码 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` | — | （见 §2 依据） | （同对象另有 1 条 D3 主张） | AUD-101-DA01-根规范与科学.md×1、AUD-101-DB02.md×1、AUD-204-面积交叠核验.md×1 | AUD204·W1[PASS] | 入库 |
| `lib/algorithms/coverage/src/sampler.cpp` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 6 条 D3 主张） | AUD-101-DB-05.md×1、AUD-101-DB01.md×1、AUD-203-天光无缝核验.md×1、AUD-402-判读-A3.md×1 | AUD203·V1[PASS/P1] AUD204·W3[PASS] 架构接线·W3[PASS] | 入库 |
| `eng/tools/e2e/seam_footprint.py` | — | （见 §2 依据） | （同对象另有 1 条 D3 主张） | AUD-101-DB-20.md×3、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-203-天光无缝核验.md×1 | AUD203·V2[PASS/P1] AUD204·W3[PASS] | 入库 |
| `eng/tools/e2e/render_vis.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-18.md×2、AUD-101-DB-20.md×1 | AUD204·W3[PASS] | 入库 |
| `eng/tools/quality/frame_qc_grid.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD204·W3[PASS] | 入库 |
| `eng/tests/validation/release02/q2_snr_smoothness/realdata/realdata_seam.log` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD204·W3[PASS] | 入库 |
| `docs/design/PRODUCT_STORAGE_FORM.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-08.md×9、AUD-101-DB-12.md×3、AUD-101-DB-10-补.md×2、AUD-101-DA01-根规范与科学.md×1 | 合同层·W4[PASS] 合同层·W5[PASS] | 入库 |

## 2 依据

「复核-AUD204」W3（判定：确认，并补两条成稿未报的更大分支；负责人待裁事项可据此定案）：`=1` 一侧除以按 8 位格点量化的支撑面积，误差随支撑变小而放大（典型量级千分之一点几、低支撑处可达百分之二以上，比 fp32 累加噪声差约四个数量级），支撑低于一个量化格时该像素按「未覆盖」发布、信号值为 NaN；`profile=0` 无偏移（已结案）；《UNRESOLVED 清单》裁-5。

## 3 改法（具体动作，动词开头）

1. 取裁-5：若判「必须等价」⇒ 把该腿改为除以精确面积或补归一，并加一条「两形态逐像素一致」的门
2. 若判「允许不同」⇒ 在设计（`PRODUCT_STORAGE_FORM.md`）与合同里写清两形态适用域与精度差，并解释 NaN 像素在产品侧如何被消费
3. 无论哪条路：补「低支撑 ⇒ 未覆盖 + NaN」的可驱动负例夹具

## 4 文件域（本任务允许触碰的路径集合）

```text
lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
lib/algorithms/coverage/src/sampler.cpp
eng/tools/e2e/seam_footprint.py
eng/tools/e2e/render_vis.py
eng/tools/quality/frame_qc_grid.py
eng/tests/validation/release02/q2_snr_smoothness/realdata/realdata_seam.log
docs/design/PRODUCT_STORAGE_FORM.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：负责人裁-5
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `seam_footprint.py` 与 ACSD-T35 共改 ⇒ 同批

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 同一输入两形态逐像素比对 ⇒ 差异复现并判红（若选等价路线）；选允许不同路线时，文档未写精度差 ⇒ 口径门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以默认 profile 掩盖差异；不得自行裁定两形态是否必须等价

## 8 登记与边界

我方倾向「必须等价」（差异由量化格式而非物理引入），但该选择属产品方向，未回前登记 BLOCKED。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` | AUD-101-DA01-根规范与科学.md:473;AUD-101-DB02.md:226;AUD-204-面积交叠核验.md:177 | AUD204·W1：确认（并订正成稿两处口径） |
| `lib/algorithms/coverage/src/sampler.cpp` | AUD-101-DB-05.md:264;AUD-101-DB01.md:580;AUD-203-天光无缝核验.md:93;AUD-402-判读-A3.md:54 | AUD203·V1：确认 ‖ AUD204·W3：确认（并补两条成稿未报的更大分支；负责人待裁事项可据此定案） ‖ 架构接线·W3：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低） |
| `eng/tools/e2e/seam_footprint.py` | AUD-101-DA01-根规范与科学.md:665;AUD-101-DB-10-补.md:89;AUD-101-DB-20.md:300;AUD-101-DB-20.md:338;AUD-101-DB-20.md:429;AUD-203-天光无缝核验.md:66 | AUD203·V2：确认 ‖ AUD204·W3：确认（并补两条成稿未报的更大分支；负责人待裁事项可据此定案） |
| `eng/tools/e2e/render_vis.py` | AUD-101-DB-18.md:251;AUD-101-DB-18.md:96;AUD-101-DB-20.md:299 | AUD204·W3：确认（并补两条成稿未报的更大分支；负责人待裁事项可据此定案） |
| `eng/tools/quality/frame_qc_grid.py` | — | AUD204·W3：确认（并补两条成稿未报的更大分支；负责人待裁事项可据此定案） |
| `eng/tests/validation/release02/q2_snr_smoothness/realdata/realdata_seam.log` | — | AUD204·W3：确认（并补两条成稿未报的更大分支；负责人待裁事项可据此定案） |
| `docs/design/PRODUCT_STORAGE_FORM.md` | AUD-101-DA01-根规范与科学.md:106;AUD-101-DB-08.md:240;AUD-101-DB-08.md:267;AUD-101-DB-08.md:296;AUD-101-DB-08.md:49;AUD-101-DB-08.md:51;AUD-101-DB-08.md:53;AUD-101-DB-08.md:69;AUD-101-DB-08.md:76;AUD-101-DB-08.md:79;AUD-101-DB-10-补.md:288;AUD-101-DB-10-补.md:444… | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） |

