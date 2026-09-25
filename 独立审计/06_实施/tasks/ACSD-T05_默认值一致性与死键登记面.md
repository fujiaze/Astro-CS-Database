# 任务：ACSD-T05 配置一致性门补数值比较，死键与键名对齐入同一条判据

> 波次 `W0` ｜ 杠杆分档 `P1` ｜ 整改域 门禁+配置 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/packaging/config/defaults.json` | — | 常数出处登记为配置键；`authority_status: "sourced"` | 六键在 CI 基线里被自登为 `dead_config_key:photometry.*`，值实为代码字面量；同一文档 `:218` 指代码、`:220` 指死键 ⇒ 互相指认。**逐键历史判定：4 键为锚漂移（曾成立、随文档重排失效），`tukey_c` 为登记即错（8/8 个历史版本从未成立）**；4/6 回链行号指错（＋4/＋7/＋147/＋147）；`sourced` 的"有无权威出处"这（同对象另有 4 条 D3 主张） | AUD-101-DB01.md×3、AUD-101-DB-03.md×2、AUD-101-DB-05.md×2、AUD-101-DB-06-07.md×2、AUD-202-SNR核验.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-101-DB-18.md×1、AUD-201-测光核验.md×1、AUD-402-判读-A1.md×1、AUD-402-判读-A2.md×1、AUD-402-判读-A3.md×1、AUD-402-常数台账.说明.md×1 | AUD201·V4[PASS] | 入库 |
| `eng/packaging/config/config_registry.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-03.md×2、AUD-101-DB-06-07.md×1、AUD-401-架构对齐.md×1、AUD-402-判读-A3.md×1 | AUD203·V3[PASS/P2] 合同层·W3[PASS] | 入库 |
| `eng/ci/checks.json` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿（同对象另有 1 条 D3 主张） | AUD-501-门禁现状审计.md×17、AUD-101-D1补三份.md×11、AUD-101-DB-19.md×6、D9-工单对账-补.md×6、AUD-101-DB-10-补.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-12.md×4、AUD-101-DA02-算法推导.md×3、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-20.md×1、AUD-401-架构对齐.md×1、AUD-403-注释与README.md×1 | AUD201·V6[PASS] AUD204·W2[PASS] DB13·W1[PASS] DB13·W2[PASS] 合同层·W3[PASS] 合同层·W4[PASS] 结果层与收口层·R1[PASS/P1] 负责人面与索引·V1[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `eng/ci/ledgers/dead_config_keys.json` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 1 条 D3 主张） | AUD-202-SNR核验.md×1 | AUD202·V2[PASS] AUD202·V4[PASS] 合同层·W5[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `eng/ci/prod_wiring_baseline.json` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_ | AUD-201-测光核验.md×2 | AUD201·V4[PASS] AUD202·V4[PASS] | 入库 |
| `eng/tests/config/check_cfg002_registry.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD201·V4[PASS] 合同层·W3[PASS] | 入库 |
| `eng/tests/config/test_cfg001_contracts.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-09.md×1 | AUD201·V4[PASS] | 入库 |
| `eng/tests/cli/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 合同层·W3[PASS] | 入库 |
| `docs/contracts/CONFIG_CONTRACT.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×4、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-DB-06-07.md×2、AUD-101-DB-10.md×1、AUD-402-判读-A2.md×1 | 合同层·W3[PASS] 合同层·W4[PASS] | 入库 |

## 2 依据

「复核-AUD201」V4（判定：确认 — 四条子 claim 全部独立复算成立，含死键 6/6；「造假 vs 漂移」这一层由第②层补做）；「复核-AUD202」V4（判定：确认 — `snr_path` 死键与三臂对象错配，结果件里 `snr` 出现 0 次、dense 两条生产者都不可达）；《链路间口径对表》§8 形态四「配置/登记键无消费者」；标准 05 §3（不静默取默认）。

## 3 改法（具体动作，动词开头）

1. 改一致性判据：从「只比键集合」升级为「逐键比数值 + 比键名对齐」，覆盖配置模板/出厂默认/登记册/合同/CLI 协议/生产入口各侧
2. 把 `defaults.json` 六条 `dead_config_key:photometry.*` 与 `snr_path`、`sparse_snr_spacing_px` 逐键定案：接线（补消费者）或删除（合同与登记面同批摘除）
3. `prod_wiring_baseline.json` 与 `dead_config_keys.json` 的基线随数据同批回填
4. 对「配置缺键时的兜底取值」补值级断言：兜底值必须等于登记值，否则判红（消除 fail-open）
5. 在判据里点名「生产入口是否落在比较集合内」，杜绝只在模板 `blocks[]` 与 CLI 骨架等面外比较的历史形态

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/packaging/config/defaults.json
eng/packaging/config/config_registry.json
eng/ci/checks.json
eng/ci/ledgers/dead_config_keys.json
eng/ci/prod_wiring_baseline.json
eng/tests/config/check_cfg002_registry.py
eng/tests/config/test_cfg001_contracts.py
eng/tests/cli/
docs/contracts/CONFIG_CONTRACT.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 无前置；与 ACSD-T25（X6 常数载体）、ACSD-T41（阶数六侧）共用同一批键与同一条判据
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - T05 / T25 / T41 三个簇共改 `defaults.json`、`dead_config_keys.json`、两个 cfg check ⇒ 必须同批或严格串行（同一提交）
  - 门与数据同批：把某键改成新值会触发一条锁旧值的门，二者不同提交即中间态全红

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 把任一侧默认值改成与登记值不同 ⇒ 一致性门判红（现状：从不比较数值，判绿）
- 新增一个无消费者的配置键 ⇒ 死键门判红
- 删掉 `photometry.fit.spatial_gain_order` 的全部登记而装配层仍兜底取二阶 ⇒ 判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以「文档已写明」替代数值比对；不得保留只判自合成夹具模式的判据充当值级门

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/packaging/config/defaults.json` | AUD-101-D1残余.md:48;AUD-101-DA01-根规范与科学.md:527;AUD-101-DB-03.md:486;AUD-101-DB-03.md:520;AUD-101-DB-05.md:238;AUD-101-DB-05.md:239;AUD-101-DB-06-07.md:158;AUD-101-DB-06-07.md:42;AUD-101-DB-10-补.md:110;AUD-101-DB-10.md:48;AUD-101-DB-18.md:182;AUD-101-DB01.md:420… | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） |
| `eng/packaging/config/config_registry.json` | AUD-101-DB-03.md:416;AUD-101-DB-03.md:671;AUD-101-DB-06-07.md:56;AUD-401-架构对齐.md:256;AUD-402-判读-A3.md:45 | AUD203·V3：确认 ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |
| `eng/ci/checks.json` | AUD-101-D1残余.md:165;AUD-101-D1补三份.md:102;AUD-101-D1补三份.md:105;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:197;AUD-101-D1补三份.md:48;AUD-101-D1补三份.md:51;AUD-101-D1补三份.md:57;AUD-101-D1补三份.md:58;AUD-101-D1补三份.md:83;AUD-101-D1补三份.md:86;AUD-101-D1补三份.md:92… | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ DB13·W1：确认 ‖ DB13·W2：确认（头条数字降级） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `eng/ci/ledgers/dead_config_keys.json` | AUD-202-SNR核验.md:257 | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） ‖ AUD202·V4：确认（并补两条成稿没给的硬证据：结果件里 `snr` 出现 0 次；dense 两条生产者都不可达） ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `eng/ci/prod_wiring_baseline.json` | AUD-201-测光核验.md:160;AUD-201-测光核验.md:172 | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） ‖ AUD202·V4：确认（并补两条成稿没给的硬证据：结果件里 `snr` 出现 0 次；dense 两条生产者都不可达） |
| `eng/tests/config/check_cfg002_registry.py` | — | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |
| `eng/tests/config/test_cfg001_contracts.py` | AUD-101-DB-09.md:224 | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） |
| `eng/tests/cli/` | — | 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |
| `docs/contracts/CONFIG_CONTRACT.md` | AUD-101-DB-04.md:189;AUD-101-DB-04.md:90;AUD-101-DB-06-07.md:174;AUD-101-DB-06-07.md:35;AUD-101-DB-08.md:245;AUD-101-DB-08.md:265;AUD-101-DB-08.md:301;AUD-101-DB-10.md:59;AUD-101-DB-11.md:408;AUD-101-DB-11.md:411;AUD-101-DB-11.md:412;AUD-101-DB-11.md:413… | 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 |

