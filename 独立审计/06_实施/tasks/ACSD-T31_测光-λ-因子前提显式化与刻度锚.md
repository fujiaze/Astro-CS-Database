# 任务：ACSD-T31 λ 因子成立前提写成约束 + 绝对刻度锚改述为「仓库内无可复核锚」

> 波次 `W1` ｜ 杠杆分档 `P2` ｜ 整改域 科学 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp` | — | `λ` 因子无条件成立，成立前提未写成约束 | `λ` 因子成立 ⟺ 解码后的 `F_λ` 为**能量**谱辐照度；容器若给光子谱通量则必须去掉 `λ`。该前提须显式化 | AUD-201-测光核验.md×4 | — | 入库 |
| `lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_production_validation.py` | — | "绝对刻度由本轮真实数据实证锚定" | 该锚的证据件不在跟踪面（`run/` 六条全悬空、脚本结果件不入库），且唯一在库的 1050 星对拍表对该缺陷**零灵敏度** ⇒ 应表述为"绝对刻度目前无仓库内可复核锚" | AUD-201-测光核验.md×2 | — | 入库 |
| `docs/science/PHOTOMETRY.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-19.md×23、AUD-201-测光核验.md×19、AUD-101-DA01-根规范与科学.md×7、AUD-101-DB-12.md×6、AUD-101-DB-18.md×6、AUD-101-DB-20.md×6、AUD-301-文献池P1.md×5、AUD-101-DB-03.md×4、AUD-301-文献复算-旧判批.md×4、AUD-101-DB-15.md×3、AUD-101-DB-16.md×3、AUD-101-D1残余.md×2、AUD-101-DA02-算法推导.md×2、AUD-101-DB-14.md×1、AUD-101-DB-17.md×1、AUD-101-DB02.md×1、论文1-回执.md×1 | AUD201·V4[PASS] AUD201·V5[PASS] | 入库 |
| `lib/algorithms/photometry/README.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB02.md×1 | 测光默认阶数·三[PASS] | 入库 |
| `docs/plugins/algorithms_phase1/06_photometry.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-19.md×5、AUD-101-DB-03.md×2、AUD-101-DB-14.md×1、AUD-101-DB-18.md×1 | 测光默认阶数·三[PASS] | 入库 |

## 2 依据

《测光链路核验报告》DEV-01/DEV-02；「复核-AUD201」V1（判定：降级 —「存活」确认、「可达产品路径」推翻、「星等计两次」机理与 0.459 dex 判待证；生产路径调用者 = 0，`pc_api.cpp:385` 已注释「不再使用 uint8*10^(-0.4G) 猜测公式」）、V2（判定：推翻 — 推翻的是「25× 裕度」这条论证；λ 因子本身的正确性由别的路径成立，正本无需订正）、V3（判定：降级 — 「色项型缺陷在星等分箱下趋零」复现，但「判据无证据资格」结论过宽，且成稿给的整改路径无效）。

## 3 改法（具体动作，动词开头）

1. 在正本 §2a.2 把「`λ` 因子成立 ⟺ 解码后的 `F_λ` 为能量谱辐照度」写成正向约束（常数须有推导，不得只写结论）
2. 把 §2a.6/:50/:408 的「绝对刻度由本轮真实数据实证锚定」改述为「目前无仓库内可复核锚」，并删去对该锚的引用
3. 色项判据的落点改到容器解码谱上：跟踪 CSV 两臂同源、不经容器解码 ⇒ 对该缺陷恒绿，不可作整改落点
4. 删除 `spectrum_integrator.cpp:279` 虚报生产可达的注释（「被 pc_api.cpp OpenMP 并行循环调用」与 grep 实数矛盾），并按标准 04 §5 处理退役叙述
5. 处置该两函数的死代码面：不在任何公共头/导出面、调用者全在测试与工具树（含 CMake 里无目标的 `test_spectrum_integrator.cpp`），按标准 04 §4 删除或标退役
6. 不再引用 0.459 dex 作为独立测得值（其来源目录被 `.gitignore:17` 排除、跟踪面 0 文件）

## 4 文件域（本任务允许触碰的路径集合）

```text
lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp
lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_production_validation.py
docs/science/PHOTOMETRY.md
lib/algorithms/photometry/README.md
docs/plugins/algorithms_phase1/06_photometry.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T02（读数入库面就绪才能补刻度锚证据）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `PHOTOMETRY.md` 由 T02/T03/T05/T25/T31 共改 ⇒ 同批；`filters.json` 与 T05 共改

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 在能量/光子两支约定上注入反向选择 ⇒ 缺判据时不得判绿（现状：唯一判据缺位）
- 把色项判据建在跟踪 CSV 上 ⇒ 去掉 `λ` 因子后仍判绿 ⇒ 该判据不得入册
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得因 V2 论证被推翻而改动 λ 因子本身（正本无需订正）；不得以「注释里有说明」替代可达性事实

## 8 登记与边界

历史可达性：生产切换到 XPSD 解码发生在 `70edb842`（2026-08-08，「fluxMin/fluxMul 线性解码替代 uint8*10^-0.4G」）⇒ 该式曾是生产通道，早于该提交的产物才可能带偏差，HEAD 不带。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp` | AUD-201-测光核验.md:107;AUD-201-测光核验.md:44;AUD-201-测光核验.md:59;AUD-201-测光核验.md:72 | — |
| `lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_production_validation.py` | AUD-201-测光核验.md:141;AUD-201-测光核验.md:410 | — |
| `docs/science/PHOTOMETRY.md` | AUD-101-D1残余.md:185;AUD-101-D1残余.md:186;AUD-101-DA01-根规范与科学.md:100;AUD-101-DA01-根规范与科学.md:326;AUD-101-DA01-根规范与科学.md:330;AUD-101-DA01-根规范与科学.md:337;AUD-101-DA01-根规范与科学.md:58;AUD-101-DA01-根规范与科学.md:699;AUD-101-DA01-根规范与科学.md:745;AUD-101-DA02-算法推导.md:645;AUD-101-DA02-算法推导.md:688;AUD-101-DB-03.md:464… | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） ‖ AUD201·V5：确认**（我独立构造可达、判据实读放行；并给成稿补三条它没有的事实，其中一条把证据级别从"构造"抬到"库内实跑日志"） |
| `lib/algorithms/photometry/README.md` | AUD-101-DB02.md:156 | 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `docs/plugins/algorithms_phase1/06_photometry.md` | AUD-101-DB-03.md:457;AUD-101-DB-03.md:461;AUD-101-DB-14.md:89;AUD-101-DB-18.md:119;AUD-101-DB-19.md:122;AUD-101-DB-19.md:1362;AUD-101-DB-19.md:1397;AUD-101-DB-19.md:90;AUD-101-DB-19.md:92 | 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |

