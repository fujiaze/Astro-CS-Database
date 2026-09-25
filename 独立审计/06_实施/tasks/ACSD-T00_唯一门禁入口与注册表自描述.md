# 任务：ACSD-T00 消灭双入口分叉与隐式继承：注册表逐单元自描述

> 波次 `W0` ｜ 杠杆分档 `P0` ｜ 整改域 门禁 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/ci/run.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-501-门禁现状审计.md×8、AUD-101-DB-20.md×3、AUD-101-DB-12.md×1 | 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `eng/ci/run_checks.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-501-门禁现状审计.md×36、AUD-101-DB-10-补.md×2、AUD-101-DB-11.md×1、D9-工单对账.md×1 | 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `eng/ci/checks.json` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿（同对象另有 1 条 D3 主张） | AUD-501-门禁现状审计.md×17、AUD-101-D1补三份.md×11、AUD-101-DB-19.md×6、D9-工单对账-补.md×6、AUD-101-DB-10-补.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-12.md×4、AUD-101-DA02-算法推导.md×3、AUD-101-DB-08.md×3、AUD-101-DB-04.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-20.md×1、AUD-401-架构对齐.md×1、AUD-403-注释与README.md×1 | AUD201·V6[PASS] AUD204·W2[PASS] DB13·W1[PASS] DB13·W2[PASS] 合同层·W3[PASS] 合同层·W4[PASS] 结果层与收口层·R1[PASS/P1] 负责人面与索引·V1[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `eng/ci/checks.schema.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-501-门禁现状审计.md×4 | 门禁入口·W1[PASS] | 入库 |
| `eng/ci/validate_registry.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-501-门禁现状审计.md×3 | 门禁入口·W1[PASS] | 入库 |
| `eng/ci/tests/test_deep_profiles.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 门禁入口·W1[PASS] | 入库 |
| `eng/ci/tests/test_runner_selection.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | 门禁入口·W1[PASS] | 入库 |
| `docs/ci/CI_SPEC.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-11.md×8、AUD-501-门禁现状审计.md×7、AUD-101-DB-08.md×5、AUD-101-DB-10-补.md×2、AUD-101-DA01-根规范与科学.md×1 | 合同层·W4[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `docs/ci/01_CHECKS.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-501-门禁现状审计.md×15、AUD-101-DB-11.md×7、AUD-101-D1残余.md×3、AUD-101-D1补三份.md×3、AUD-101-DB-20.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-08.md×1、AUD-101-DB-10-补.md×1、D9-工单对账-补.md×1 | 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `docs/ci/03_GATES.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-501-门禁现状审计.md×8、AUD-101-DB-08.md×4、AUD-101-DB-11.md×4、AUD-101-DB-12.md×3、D9-工单对账-补.md×3、D9-工单对账.md×1 | 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 入库 |
| `ACCEPTANCE_SPEC.md` | — | manifest 的 `node_reproduction_max_abs` 登记为「重建误差」；`weight_chain.h:167` 自注"应 ~0" | 二者对同一字段给出互斥定性（一名为误差量、一称为应恒 0）；该字段在可达输入集上恒 0 ⇒ **无证据资格**；合同要求的逐像素**预测方差**从未实现（字段面缺位、`git log -S` 两条命中均在实验侧）。另：`EXP-04-RECONSTRUCTION.md:648`（判据 S4）本仓已自登记"容差 0.5 dex 对精确插值类过松，不具举证资格"，但 manifest 名面未跟着改 | — | — | 入库 |
| `.github/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | — | 入库 |

## 2 依据

《门禁体系设计》(D7) §2.1–2.6 与 §2.6 五批事务表；「复核-门禁入口」W1（判定：确认，机制与「agent 入口可零执行报绿」两条主链全部成立；成稿规模数 345/93% 待订正）；标准 05 §2（判据须有判别力）、§3（fail-closed）、§5（注册表与实际可执行检查一致）；CONTROL_PACK_SPEC §5（文件域互斥）。

## 3 改法（具体动作，动词开头）

1. 建共享模块并实现 `index_steps(registry) -> [unit]`，输出解析后自描述的单元（每单元带全量字段、不含「待定」）
2. 把两入口（`run.py` / `run_checks.py`）改为调用同一 `index_steps` 与同一零有效执行守卫
3. 让 `--strict` 校验器新增「逐单元全字段必填」规则，但只判已存在的形态
4. 注册表字段扩表：schema + validator + 执行器三方同批改，并逐 step 回填 `waivable/heavy/requires_monitor/mutates_workspace/outputs/inputs/optional_inputs/prerequisite_tools/changed_paths/timeout_seconds/platform/profiles/command`，同批删除 `INHERIT_FIELDS` 路径
5. 把 `platform` 值域收敛为 `{any, linux, windows}`；投递渠道（`fatduck`）改挂档位属性并从 `platform` 值域摘除
6. 将 workflow 改指唯一执行器；删除 `eng/ci/run.py` 的裁决路径；其单测迁移到共享模块
7. 把 `01_CHECKS.md §2` 命令列切换为生成器产物；`ACCEPTANCE_SPEC.md` 两处「通过标准 = `run_checks.py --check … exit 0`」补限定为「由唯一执行器在同平台执行且 effective>0」
8. 把 R12 反转为 R12b；清理聚合项顶层 command

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/ci/run.py
eng/ci/run_checks.py
eng/ci/checks.json
eng/ci/checks.schema.json
eng/ci/validate_registry.py
eng/ci/tests/test_deep_profiles.py
eng/ci/tests/test_runner_selection.py
docs/ci/CI_SPEC.md
docs/ci/01_CHECKS.md
docs/ci/03_GATES.md
ACCEPTANCE_SPEC.md
.github/
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - D7 §2.6 表内 5 批为硬顺序：批 2 的 schema/validator/executor/数据回填四改必须同提交（任一先动即全红，D7 §3.4 白名单死结）；批 3 晚于批 2；批 4 晚于批 3；批 5 最后
  - 本任务是 W0 的根节点：ACSD-T01/T04/T05/T08/T10 的门名与字段登记都挂在其注册表扩表之后
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - 与 ACSD-T07、ACSD-T10 共改 `eng/ci/checks.json`：注册表结构性改动必须同一提交，否则出现「改了校验器数据全红」或「改了数据校验器全红」的中间态落 main
  - 与 ACSD-T01 共用「有效执行数」定义：入口未唯一化前，普查门的「通过」无可信语义

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 删掉任一 step 的 `platform` ⇒ 注册表校验判红（现状：一个红、一个继承、一个当 any）
- 任一档的 `platforms` 为空，或其成员在该平台「有效执行数 = 0」 ⇒ `GATE-PROFILE-PLATFORM` 判红
- `platform: fatduck` 之类非平台值出现 ⇒ 判红（C-c）
- 把 `CHK-ALGO-WIRING`（`platform: linux`、进 `windows-main`、`waivable: false`）原样留着 ⇒ 该门在两读数面不一致，须判红；改数据与改判据不同提交 ⇒ 提交门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以 waiver 盖住分叉红灯；不得保留第二入口「零有效执行仍报绿」的路径
- 不得在批 2 之前单独回填数据或单独改校验器

## 8 登记与边界

成稿给的规模数（345 step / 93%）被第②层判为待订正，本任务按其复算口径登记，不沿用原数。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/ci/run.py` | AUD-101-DB-12.md:179;AUD-101-DB-20.md:109;AUD-101-DB-20.md:413;AUD-101-DB-20.md:49;AUD-501-门禁现状审计.md:195;AUD-501-门禁现状审计.md:196;AUD-501-门禁现状审计.md:197;AUD-501-门禁现状审计.md:224;AUD-501-门禁现状审计.md:254;AUD-501-门禁现状审计.md:400;AUD-501-门禁现状审计.md:669;AUD-501-门禁现状审计.md:774 | 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `eng/ci/run_checks.py` | AUD-101-DB-10-补.md:204;AUD-101-DB-10-补.md:205;AUD-101-DB-11.md:341;AUD-501-门禁现状审计.md:159;AUD-501-门禁现状审计.md:161;AUD-501-门禁现状审计.md:179;AUD-501-门禁现状审计.md:180;AUD-501-门禁现状审计.md:194;AUD-501-门禁现状审计.md:199;AUD-501-门禁现状审计.md:200;AUD-501-门禁现状审计.md:201;AUD-501-门禁现状审计.md:202… | 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `eng/ci/checks.json` | AUD-101-D1残余.md:165;AUD-101-D1补三份.md:102;AUD-101-D1补三份.md:105;AUD-101-D1补三份.md:123;AUD-101-D1补三份.md:197;AUD-101-D1补三份.md:48;AUD-101-D1补三份.md:51;AUD-101-D1补三份.md:57;AUD-101-D1补三份.md:58;AUD-101-D1补三份.md:83;AUD-101-D1补三份.md:86;AUD-101-D1补三份.md:92… | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ DB13·W1：确认 ‖ DB13·W2：确认（头条数字降级） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 ‖ 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `eng/ci/checks.schema.json` | AUD-501-门禁现状审计.md:177;AUD-501-门禁现状审计.md:178;AUD-501-门禁现状审计.md:370;AUD-501-门禁现状审计.md:674 | 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 |
| `eng/ci/validate_registry.py` | AUD-501-门禁现状审计.md:169;AUD-501-门禁现状审计.md:205;AUD-501-门禁现状审计.md:673 | 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 |
| `eng/ci/tests/test_deep_profiles.py` | — | 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 |
| `eng/ci/tests/test_runner_selection.py` | — | 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 |
| `docs/ci/CI_SPEC.md` | AUD-101-DA01-根规范与科学.md:228;AUD-101-DB-08.md:164;AUD-101-DB-08.md:170;AUD-101-DB-08.md:189;AUD-101-DB-08.md:195;AUD-101-DB-08.md:321;AUD-101-DB-10-补.md:408;AUD-101-DB-10-补.md:412;AUD-101-DB-11.md:316;AUD-101-DB-11.md:318;AUD-101-DB-11.md:337;AUD-101-DB-11.md:344… | 合同层·W4：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据 ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `docs/ci/01_CHECKS.md` | AUD-101-D1残余.md:296;AUD-101-D1残余.md:297;AUD-101-D1残余.md:302;AUD-101-D1补三份.md:47;AUD-101-D1补三份.md:68;AUD-101-D1补三份.md:92;AUD-101-DA01-根规范与科学.md:670;AUD-101-DB-08.md:175;AUD-101-DB-10-补.md:467;AUD-101-DB-11.md:145;AUD-101-DB-11.md:287;AUD-101-DB-11.md:291… | 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `docs/ci/03_GATES.md` | AUD-101-DB-08.md:174;AUD-101-DB-08.md:176;AUD-101-DB-08.md:189;AUD-101-DB-08.md:345;AUD-101-DB-11.md:299;AUD-101-DB-11.md:325;AUD-101-DB-11.md:329;AUD-101-DB-11.md:669;AUD-101-DB-12.md:110;AUD-101-DB-12.md:113;AUD-101-DB-12.md:295;AUD-501-门禁现状审计.md:109… | 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `ACCEPTANCE_SPEC.md` | — | — |
| `.github/` | — | — |

