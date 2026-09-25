# 任务：ACSD-T25 常数出处登记为配置键的六条死键与 CFG001/CFG002 判据退化

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学+配置 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/packaging/config/defaults.json` | — | 常数出处登记为配置键；`authority_status: "sourced"` | 六键在 CI 基线里被自登为 `dead_config_key:photometry.*`，值实为代码字面量；同一文档 `:218` 指代码、`:220` 指死键 ⇒ 互相指认。**逐键历史判定：4 键为锚漂移（曾成立、随文档重排失效），`tukey_c` 为登记即错（8/8 个历史版本从未成立）**；4/6 回链行号指错（＋4/＋7/＋147/＋147）；`sourced` 的"有无权威出处"这（同对象另有 4 条 D3 主张） | AUD-101-DB01.md×3、AUD-101-DB-03.md×2、AUD-101-DB-05.md×2、AUD-101-DB-06-07.md×2、AUD-202-SNR核验.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-101-DB-18.md×1、AUD-201-测光核验.md×1、AUD-402-判读-A1.md×1、AUD-402-判读-A2.md×1、AUD-402-判读-A3.md×1、AUD-402-常数台账.说明.md×1 | AUD201·V4[PASS] | 入库 |
| `docs/science/PHOTOMETRY.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-19.md×23、AUD-201-测光核验.md×19、AUD-101-DA01-根规范与科学.md×7、AUD-101-DB-12.md×6、AUD-101-DB-18.md×6、AUD-101-DB-20.md×6、AUD-301-文献池P1.md×5、AUD-101-DB-03.md×4、AUD-301-文献复算-旧判批.md×4、AUD-101-DB-15.md×3、AUD-101-DB-16.md×3、AUD-101-D1残余.md×2、AUD-101-DA02-算法推导.md×2、AUD-101-DB-14.md×1、AUD-101-DB-17.md×1、AUD-101-DB02.md×1、论文1-回执.md×1 | AUD201·V4[PASS] AUD201·V5[PASS] | 入库 |
| `eng/tests/config/check_cfg002_registry.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD201·V4[PASS] 合同层·W3[PASS] | 入库 |
| `eng/tests/config/test_cfg001_contracts.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-09.md×1 | AUD201·V4[PASS] | 入库 |
| `eng/ci/prod_wiring_baseline.json` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_ | AUD-201-测光核验.md×2 | AUD201·V4[PASS] AUD202·V4[PASS] | 入库 |
| `eng/ci/ledgers/dead_config_keys.json` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 1 条 D3 主张） | AUD-202-SNR核验.md×1 | AUD202·V2[PASS] AUD202·V4[PASS] 合同层·W5[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `docs/algorithms/GATES_AND_TOLERANCES.md` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿 | AUD-101-DA02-算法推导.md×2 | AUD201·V6[PASS] | 入库 |

## 2 依据

《链路间口径对表》§9 X6（含 `tukey_c` 的「登记即错」例外与 CFG001/CFG002 判据退化）；「复核-AUD201」V4（判定：确认 — 四条子 claim 全部独立复算成立：`PHOTOMETRY.md:218` 与 `:220` 权威回链行号大面积指错、`:220` 把「证据出处」写成配置键 `photometry.mag_tolerance`、死键 6/6 成立、`sourced` 状态字段本身没说谎）。

## 3 改法（具体动作，动词开头）

1. 逐键定案六条 `dead_config_key:photometry.*`：接线或删除；删除须同批摘除合同、登记册、文档三处引用
2. 把常数出处载体定为单一面（代码字面量 或 配置键，二选一），并在 `PHOTOMETRY.md:218`/`:220` 写清该面
3. `tukey_c` 的「登记即错」例外按第②层口径写入判据例外表，不再当活键
4. 修 CFG001/CFG002 的判据退化：从「键名存在」升级为「值与文档回链同号同值」
5. `PHOTOMETRY.md` 的权威回链行号逐条按实测订正

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/packaging/config/defaults.json
docs/science/PHOTOMETRY.md
eng/tests/config/check_cfg002_registry.py
eng/tests/config/test_cfg001_contracts.py
eng/ci/prod_wiring_baseline.json
eng/ci/ledgers/dead_config_keys.json
docs/algorithms/GATES_AND_TOLERANCES.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 同批：ACSD-T05（值级一致性门）—— 门与数据同批改，否则新数据触发锁旧值的门
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - T05/T25/T41 共改 `defaults.json`、`dead_config_keys.json`、两个 cfg check ⇒ 必须同一提交

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 把某常数在文档里的回链改指到一个不存在的键 ⇒ CFG 判据判红（现状：判绿）
- 在 `defaults.json` 写一个值、文档写另一个值 ⇒ 值级判据判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以「配置里有这个键」充当科学证据出处

## 8 登记与边界

第②层已就「造假 vs 漂移」下判断（成稿未做这一层），本任务按该结论登记处置面，不再上呈。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/packaging/config/defaults.json` | AUD-101-D1残余.md:48;AUD-101-DA01-根规范与科学.md:527;AUD-101-DB-03.md:486;AUD-101-DB-03.md:520;AUD-101-DB-05.md:238;AUD-101-DB-05.md:239;AUD-101-DB-06-07.md:158;AUD-101-DB-06-07.md:42;AUD-101-DB-10-补.md:110;AUD-101-DB-10.md:48;AUD-101-DB-18.md:182;AUD-101-DB01.md:420… | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） |
| `docs/science/PHOTOMETRY.md` | AUD-101-D1残余.md:185;AUD-101-D1残余.md:186;AUD-101-DA01-根规范与科学.md:100;AUD-101-DA01-根规范与科学.md:326;AUD-101-DA01-根规范与科学.md:330;AUD-101-DA01-根规范与科学.md:337;AUD-101-DA01-根规范与科学.md:58;AUD-101-DA01-根规范与科学.md:699;AUD-101-DA01-根规范与科学.md:745;AUD-101-DA02-算法推导.md:645;AUD-101-DA02-算法推导.md:688;AUD-101-DB-03.md:464… | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） ‖ AUD201·V5：确认**（我独立构造可达、判据实读放行；并给成稿补三条它没有的事实，其中一条把证据级别从"构造"抬到"库内实跑日志"） |
| `eng/tests/config/check_cfg002_registry.py` | — | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |
| `eng/tests/config/test_cfg001_contracts.py` | AUD-101-DB-09.md:224 | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） |
| `eng/ci/prod_wiring_baseline.json` | AUD-201-测光核验.md:160;AUD-201-测光核验.md:172 | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） ‖ AUD202·V4：确认（并补两条成稿没给的硬证据：结果件里 `snr` 出现 0 次；dense 两条生产者都不可达） |
| `eng/ci/ledgers/dead_config_keys.json` | AUD-202-SNR核验.md:257 | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） ‖ AUD202·V4：确认（并补两条成稿没给的硬证据：结果件里 `snr` 出现 0 次；dense 两条生产者都不可达） ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `docs/algorithms/GATES_AND_TOLERANCES.md` | AUD-101-DA02-算法推导.md:1473;AUD-101-DA02-算法推导.md:511 | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） |

