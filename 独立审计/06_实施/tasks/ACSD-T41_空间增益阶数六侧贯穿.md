# 任务：ACSD-T41 `photometry.fit.spatial_gain_order` 六面零登记、装配层兜底取二阶（前置：负责人裁-6）

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 配置+代码 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `cfg:photometry.fit.spatial_gain_order` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | — | 入库（六面零登记本身即证据，命令见复核件 §三） |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | — | （见 §2 依据） | （同对象另有 9 条 D3 主张） | AUD-101-DB-03.md×8、AUD-401-架构对齐.md×7、AUD-101-DB-19.md×6、AUD-202-SNR核验.md×6、AUD-101-DB-20.md×4、AUD-403-注释与README.md×3、D9-工单对账.md×3、AUD-101-D1残余.md×2、AUD-201-测光核验.md×2、AUD-101-DB-08.md×1、AUD-101-DB-11.md×1、AUD-101-DB-12.md×1、AUD-101-DB-18.md×1、AUD-402-判读-A3.md×1 | AUD202-补·V5[PASS] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] 架构接线·W1[PASS] 架构接线·W3[PASS] 测光默认阶数·三[PASS] | 入库 |
| `eng/packaging/config/defaults.json` | — | 常数出处登记为配置键；`authority_status: "sourced"` | 六键在 CI 基线里被自登为 `dead_config_key:photometry.*`，值实为代码字面量；同一文档 `:218` 指代码、`:220` 指死键 ⇒ 互相指认。**逐键历史判定：4 键为锚漂移（曾成立、随文档重排失效），`tukey_c` 为登记即错（8/8 个历史版本从未成立）**；4/6 回链行号指错（＋4/＋7/＋147/＋147）；`sourced` 的"有无权威出处"这（同对象另有 4 条 D3 主张） | AUD-101-DB01.md×3、AUD-101-DB-03.md×2、AUD-101-DB-05.md×2、AUD-101-DB-06-07.md×2、AUD-202-SNR核验.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-101-DB-18.md×1、AUD-201-测光核验.md×1、AUD-402-判读-A1.md×1、AUD-402-判读-A2.md×1、AUD-402-判读-A3.md×1、AUD-402-常数台账.说明.md×1 | AUD201·V4[PASS] | 入库 |
| `eng/packaging/config/config_registry.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-03.md×2、AUD-101-DB-06-07.md×1、AUD-401-架构对齐.md×1、AUD-402-判读-A3.md×1 | AUD203·V3[PASS/P2] 合同层·W3[PASS] | 入库 |
| `docs/plugins/algorithms_phase1/06_photometry.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-19.md×5、AUD-101-DB-03.md×2、AUD-101-DB-14.md×1、AUD-101-DB-18.md×1 | 测光默认阶数·三[PASS] | 入库 |
| `lib/algorithms/photometry/README.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB02.md×1 | 测光默认阶数·三[PASS] | 入库 |
| `eng/tools/e2e/make_vis_configs.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-20.md×1 | 测光默认阶数·三[PASS] | 入库 |
| `eng/tools/quality/check_photometry_apply.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×1 | 测光默认阶数·三[PASS] | 入库 |
| `eng/tests/validation/release02/fix_p1_photometry_apply/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | — | 入库 |

## 2 依据

「复核-测光默认阶数」（判定：确认 —— 事实链全部成立 ＋ 定性与定级须修正，定级建议 P1 并给出升 P0 的唯一触发条件）；《UNRESOLVED 清单》裁-6；《链路间口径对表》§7 常数登记状态；AGENTS.md §6（不动科学公式与默认容差，除非按变更流程批准）。

## 3 改法（具体动作，动词开头）

1. 先取裁-6 的一句裁决（「验收前提是否算已过」）：未过 ⇒ 按现行条款退回零阶；已过 ⇒ 把条款改写为现状合法并补三口径复跑记录
2. 无论哪条路：补六面登记（配置模板、出厂默认、登记册、合同、CLI 协议、生产入口）＋ 值级断言（接 ACSD-T05）
3. 删除装配层「缺键静默取二阶」的兜底，改为显式报错或显式降级并写明原因（标准 04 §3）
4. 在生产启用件里显式带该键，消除「覆盖键在一条路上静默失效」的不对称

## 4 文件域（本任务允许触碰的路径集合）

```text
cfg:photometry.fit.spatial_gain_order
lib/infrastructure/scheduler/src/module_adapters.cpp
eng/packaging/config/defaults.json
eng/packaging/config/config_registry.json
docs/plugins/algorithms_phase1/06_photometry.md
lib/algorithms/photometry/README.md
eng/tools/e2e/make_vis_configs.py
eng/tools/quality/check_photometry_apply.py
eng/tests/validation/release02/fix_p1_photometry_apply/
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：负责人裁-6；ACSD-T05 的值级判据
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - 与 T05/T25 共用键与同一判据 ⇒ 必须同一提交

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 配置不带该键跑一次 normalize ⇒ 实际生效值必须与登记值一致，否则判红（现状：没有任何现存判据会因此变红）
- 六面中任一面缺登记 ⇒ 一致性门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得在裁-6 未回前自行选数；不得把六侧不一致写成「仅文档问题」；不得以「无判据会红」作为不改的理由

## 8 登记与边界

无裁决前本任务登记为 BLOCKED（AGENTS.md §10），不硬编、不预判。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `cfg:photometry.fit.spatial_gain_order` | — | — |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | AUD-101-D1残余.md:158;AUD-101-D1残余.md:295;AUD-101-DB-03.md:147;AUD-101-DB-03.md:158;AUD-101-DB-03.md:502;AUD-101-DB-03.md:61;AUD-101-DB-03.md:619;AUD-101-DB-03.md:636;AUD-101-DB-03.md:735;AUD-101-DB-03.md:88;AUD-101-DB-08.md:152;AUD-101-DB-11.md:493… | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 ‖ 架构接线·W1：确认**（成稿 AUD401-005 的事实面与计数全部独立复现成功；我的定级理由与改法方向与成稿不同，见"与成稿差异"） ‖ 架构接线·W3：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低） ‖ 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `eng/packaging/config/defaults.json` | AUD-101-D1残余.md:48;AUD-101-DA01-根规范与科学.md:527;AUD-101-DB-03.md:486;AUD-101-DB-03.md:520;AUD-101-DB-05.md:238;AUD-101-DB-05.md:239;AUD-101-DB-06-07.md:158;AUD-101-DB-06-07.md:42;AUD-101-DB-10-补.md:110;AUD-101-DB-10.md:48;AUD-101-DB-18.md:182;AUD-101-DB01.md:420… | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） |
| `eng/packaging/config/config_registry.json` | AUD-101-DB-03.md:416;AUD-101-DB-03.md:671;AUD-101-DB-06-07.md:56;AUD-401-架构对齐.md:256;AUD-402-判读-A3.md:45 | AUD203·V3：确认 ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |
| `docs/plugins/algorithms_phase1/06_photometry.md` | AUD-101-DB-03.md:457;AUD-101-DB-03.md:461;AUD-101-DB-14.md:89;AUD-101-DB-18.md:119;AUD-101-DB-19.md:122;AUD-101-DB-19.md:1362;AUD-101-DB-19.md:1397;AUD-101-DB-19.md:90;AUD-101-DB-19.md:92 | 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `lib/algorithms/photometry/README.md` | AUD-101-DB02.md:156 | 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `eng/tools/e2e/make_vis_configs.py` | AUD-101-DB-20.md:299 | 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `eng/tools/quality/check_photometry_apply.py` | AUD-101-DB-11.md:303 | 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `eng/tests/validation/release02/fix_p1_photometry_apply/` | — | — |

