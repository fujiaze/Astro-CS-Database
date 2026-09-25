# 任务：ACSD-T28 术语表把合法测量值当缺失标记：一行改口径 + 改锚，可结案一条待裁

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学+文档 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `docs/GLOSSARY.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | D9-工单对账.md×14、AUD-101-DB-11.md×6、AUD-101-DB-13.md×3、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-05.md×1 | DB13·W4[PASS/P1] | 入库 |
| `docs/science/CONTROL_WEIGHT_SNR.md` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_（同对象另有 2 条 D3 主张） | AUD-101-DA01-根规范与科学.md×10、AUD-101-DB-15.md×7、AUD-202-SNR核验.md×7、AUD-101-DB-13.md×4、AUD-101-DB-16.md×4、AUD-101-DB-04.md×1、AUD-101-DB-12.md×1、AUD-101-DB-14.md×1、AUD-101-DB-19.md×1、AUD-101-DB-20.md×1 | AUD202·V2[PASS] DB13·W4[PASS/P1] 结果层与收口层·R3[PASS] | 入库 |
| `docs/algorithms/PHASE2_SAMPLER.md` | — | `sigma = (s0>0)? s0 : 1e-12` ⇒ 以数值保护量的平方生成有限方差，`civar ≈ 1.31e26` 进 UPM 加性面求解 | 正本 `PHASE2_UPM.md:84-85` §8 与 `PHASE2_SAMPLER.md §5.4`：`σ_bg_raw = 0 ⇒ control_ivar **必须为 0**`、`control_variance` 标为**无尺度信息（非有限）**，禁止以数值保护量生成有限方差发布。机制定性不是"除零未设守卫"，而是**有显式钳位**：正因有 1e−12，`:877` 的 0 分支与 （同对象另有 2 条 D3 主张） | AUD-101-D1残余.md×4、AUD-101-DA02-算法推导.md×2、AUD-301-文献复算-旧判批.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-14.md×1、AUD-301-文献池P1.md×1 | AUD203·V1[PASS/P1] DB13·W4[PASS/P1] | 入库 |
| `docs/contracts/DATA_SEMANTICS.md` | — | 同一份文档体系对**同一符号**给两种分母：代码与 FROZEN 正本 `docs/science/DRIZZLE.md:44-50` 为 `w_jp = a_jp/A_drop,j`（其 §10 禁止项逐字写着"把核权重写回 `a_jp/A_pixel,j`（FZ-COND-FLUX-CONSERV 判红）"）；而 `DATA_SEMA | 唯一实现口径是 `a_jp/A_drop`；`w'_jp = a_jp/A_pixel` 只作为**等价参数化**成立，且必须同时换分母（`N'_p = Σ w'·A_pixel = D_p`）。等价性已独立复核：发布 `S_p` 与 `variance_p` 对两种参数化**不变**（分子分母各乘 `pf⁴` 相消）；但 `Σ_p w_jp = 1`（⇒ `Σ_p F_p = Σ_j x_j`、（同对象另有 1 条 D3 主张） | AUD-101-DA02-算法推导.md×9、AUD-101-DB01.md×9、AUD-101-DB-06-07.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-04.md×3、AUD-101-DB-10-补.md×3、AUD-101-DA01-根规范与科学.md×2、AUD-101-DB-03.md×2、AUD-204-面积交叠核验.md×2、AUD-101-DB-13.md×1、AUD-402-判读-BD1.md×1、AUD-403-注释与README.md×1 | DB13·W1[PASS] DB13·W4[PASS/P1] 合同层·W1[PASS] | 入库 |
| `docs/contracts/PUBLIC_API.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-09.md×6、AUD-101-DB01.md×5、AUD-101-DA02-算法推导.md×2、AUD-101-DB-05.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-11.md×1、AUD-402-判读-BD1.md×1、AUD-501-门禁现状审计.md×1、D9-工单对账.md×1 | DB13·W3[PASS/P1] DB13·W4[PASS/P1] | 入库 |
| `lib/algorithms/coverage/include/astro/phase2/upm.h` | — | 两处生产入口的 `tolerance_relative` 取值不同 | 定义层 `upm.h:78-80`（语义）＋ `:113`（默认 0）；赋值层是两个装配点；消费层 `upm.cpp:1036-1040` 唯一 ⇒ **不是"配置两侧默认不同"，是装配点一处显式设、另一处漏设回落默认**。正本 `PHASE2_UPM.md:98` 标题行即"容差随观测尺度归一，**禁绝对容差**"、`:114-115` 明写"生产必须 `tolerance_relative=1（同对象另有 2 条 D3 主张） | AUD-101-DB-03.md×1、AUD-101-DB-09.md×1 | DB13·W4[PASS/P1] | 入库 |
| `lib/algorithms/coverage/tests/synthetic_gate.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-09.md×1、D9-工单对账.md×1 | DB13·W4[PASS/P1] | 入库 |
| `eng/ci/check_no_weight_mode.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-06-07.md×1 | DB13·W4[PASS/P1] 合同层·W2[PASS] | 入库 |

## 2 依据

「复核-DB13」W4（判定：确认，可结案不必上呈）：`docs/GLOSSARY.md:15` 的「snr=1.0 一律按 unknown 显式标注」与正本 §5/§6/§7、两份现行合同、算法文档、实现头 `upm.h:51-54`、测试 `synthetic_gate.cpp:3938` 六面逐字相反（把合法测量值当缺失标记 = 正本明列的不可接受变化）；其自证锚 `#42` 只定义符号别名，同行还把被否定的 `support×snr²` 通道当活定义。

## 3 改法（具体动作，动词开头）

1. 把 `docs/GLOSSARY.md:15` 一行改为与正本六面一致的表述（1.0 是合法测量值；不可估计另用具名哨兵）
2. 改自证锚 `#42` → `#153`，并把同行 `support×snr²` 的活定义改指现行通道
3. 与 ACSD-T24 的哨兵改法同批落地（同一语义、两处承载面）
4. 撤下负责人待裁清单中「snr=1.0 两篇相反」一项

## 4 文件域（本任务允许触碰的路径集合）

```text
docs/GLOSSARY.md
docs/science/CONTROL_WEIGHT_SNR.md
docs/algorithms/PHASE2_SAMPLER.md
docs/contracts/DATA_SEMANTICS.md
docs/contracts/PUBLIC_API.md
lib/algorithms/coverage/include/astro/phase2/upm.h
lib/algorithms/coverage/tests/synthetic_gate.cpp
eng/ci/check_no_weight_mode.py
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 同批：ACSD-T24
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `DATA_SEMANTICS.md`/`PUBLIC_API.md` 与 T27/T36/T38 共改 ⇒ 同批

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 修好判据：全仓 `snr=1.0` 语义单向（`git grep` 无第二读法）；任一文档仍把 1.0 当 unknown ⇒ 术语一致性门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得在改口径的同时改默认容差

## 8 登记与边界

该条同时消解一条已登记待裁事项，交付后需在 UNRESOLVED 清单勾销。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `docs/GLOSSARY.md` | AUD-101-DA01-根规范与科学.md:450;AUD-101-DB-05.md:304;AUD-101-DB-11.md:116;AUD-101-DB-11.md:120;AUD-101-DB-11.md:527;AUD-101-DB-11.md:547;AUD-101-DB-11.md:588;AUD-101-DB-11.md:658;AUD-101-DB-13.md:277;AUD-101-DB-13.md:336;AUD-101-DB-13.md:373;D9-工单对账.md:33… | DB13·W4：确认（可结案，不必上呈） |
| `docs/science/CONTROL_WEIGHT_SNR.md` | AUD-101-DA01-根规范与科学.md:102;AUD-101-DA01-根规范与科学.md:104;AUD-101-DA01-根规范与科学.md:345;AUD-101-DA01-根规范与科学.md:349;AUD-101-DA01-根规范与科学.md:377;AUD-101-DA01-根规范与科学.md:380;AUD-101-DA01-根规范与科学.md:59;AUD-101-DA01-根规范与科学.md:649;AUD-101-DA01-根规范与科学.md:694;AUD-101-DA01-根规范与科学.md:727;AUD-101-DB-04.md:39;AUD-101-DB-12.md:202… | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） ‖ DB13·W4：确认（可结案，不必上呈） ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `docs/algorithms/PHASE2_SAMPLER.md` | AUD-101-D1残余.md:103;AUD-101-D1残余.md:371;AUD-101-D1残余.md:393;AUD-101-D1残余.md:99;AUD-101-DA01-根规范与科学.md:416;AUD-101-DA02-算法推导.md:1856;AUD-101-DA02-算法推导.md:2281;AUD-101-DB-14.md:42;AUD-301-文献复算-旧判批.md:116;AUD-301-文献复算-旧判批.md:342;AUD-301-文献池P1.md:632 | AUD203·V1：确认 ‖ DB13·W4：确认（可结案，不必上呈） |
| `docs/contracts/DATA_SEMANTICS.md` | AUD-101-DA01-根规范与科学.md:359;AUD-101-DA01-根规范与科学.md:607;AUD-101-DA02-算法推导.md:1052;AUD-101-DA02-算法推导.md:1055;AUD-101-DA02-算法推导.md:1323;AUD-101-DA02-算法推导.md:1750;AUD-101-DA02-算法推导.md:2134;AUD-101-DA02-算法推导.md:2204;AUD-101-DA02-算法推导.md:450;AUD-101-DA02-算法推导.md:772;AUD-101-DA02-算法推导.md:803;AUD-101-DB-03.md:38… | DB13·W1：确认 ‖ DB13·W4：确认（可结案，不必上呈） ‖ 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `docs/contracts/PUBLIC_API.md` | AUD-101-DA02-算法推导.md:1057;AUD-101-DA02-算法推导.md:116;AUD-101-DB-05.md:382;AUD-101-DB-09.md:195;AUD-101-DB-09.md:261;AUD-101-DB-09.md:3;AUD-101-DB-09.md:43;AUD-101-DB-09.md:45;AUD-101-DB-09.md:49;AUD-101-DB-10-补.md:147;AUD-101-DB-11.md:102;AUD-101-DB01.md:125… | DB13·W3：确认（范围与严重度上修） ‖ DB13·W4：确认（可结案，不必上呈） |
| `lib/algorithms/coverage/include/astro/phase2/upm.h` | AUD-101-DB-03.md:107;AUD-101-DB-09.md:57 | DB13·W4：确认（可结案，不必上呈） |
| `lib/algorithms/coverage/tests/synthetic_gate.cpp` | AUD-101-DB-09.md:165;D9-工单对账.md:124 | DB13·W4：确认（可结案，不必上呈） |
| `eng/ci/check_no_weight_mode.py` | AUD-101-DB-06-07.md:97 | DB13·W4：确认（可结案，不必上呈） ‖ 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |

