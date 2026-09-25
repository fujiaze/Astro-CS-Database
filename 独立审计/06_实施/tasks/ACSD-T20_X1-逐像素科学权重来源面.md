# 任务：ACSD-T20 阶段二逐像素权重来源面定案：叠加权须含源项，实现侧新增含源项加权方差面

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `lib/infrastructure/scheduler/src/module_adapters.cpp` | — | （见 §2 依据） | （同对象另有 9 条 D3 主张） | AUD-101-DB-03.md×8、AUD-401-架构对齐.md×7、AUD-101-DB-19.md×6、AUD-202-SNR核验.md×6、AUD-101-DB-20.md×4、AUD-403-注释与README.md×3、D9-工单对账.md×3、AUD-101-D1残余.md×2、AUD-201-测光核验.md×2、AUD-101-DB-08.md×1、AUD-101-DB-11.md×1、AUD-101-DB-12.md×1、AUD-101-DB-18.md×1、AUD-402-判读-A3.md×1 | AUD202-补·V5[PASS] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] 架构接线·W1[PASS] 架构接线·W3[PASS] 测光默认阶数·三[PASS] | 入库 |
| `docs/science/NOISE_MODEL.md` | — | Phase2 逐像素科学权重 = Phase1 落盘的 `ivar`（空背景方差面倒数） | 叠加权应为 `w = 1/σ_w²`，`σ_w² = σ_bg² + S_src/g`（`NOISE_MODEL.md` §5c 的加权方差面）。现状把**带适用域的近似**当**定义**：`1/σ_bg²` 只在 `S_src ≡ 0` 或 `S_src/g ≪ σ_bg²` 时等于最优权（同对象另有 5 条 D3 主张） | AUD-101-DA01-根规范与科学.md×12、AUD-101-DA02-算法推导.md×6、AUD-202-SNR核验.md×6、D9-工单对账.md×6、AUD-101-DB-03.md×5、AUD-101-DB-19.md×4、AUD-101-DB-20.md×3、AUD-101-DB-13.md×1、AUD-101-DB-14.md×1、AUD-101-DB-16.md×1、AUD-101-DB-17.md×1、AUD-301-文献复算-旧判批.md×1 | 结果层与收口层·R3[PASS] | 入库 |
| `ASTROCS_DESIGN.md` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_（同对象另有 11 条 D3 主张） | — | — | 入库 |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` | — | 帧级 SNR／稀疏控制点／深度 `m_5` 全部走对角（白噪声）形式 | Phase1 产品是 drizzle 重采样后的 HEALPix 叶（相关长度 1–2 px），属 `07_noise_snr.md:130` 的"相关噪声"分支，却未用完整信息核；完整核**已实现、零接入**（同对象另有 5 条 D3 主张） | AUD-202-SNR核验.md×11、AUD-101-DB-03.md×6、AUD-101-DB-19.md×5、AUD-101-DB-16.md×3、AUD-101-DB-20.md×3、AUD-101-DB-13.md×2、AUD-101-DB-14.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-402-判读-A3.md×1 | AUD202-补·V5[PASS] AUD202·V3[VOID] 结果层与收口层·R3[PASS] | 入库 |
| `docs/science/CONTROL_WEIGHT_SNR.md` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_（同对象另有 2 条 D3 主张） | AUD-101-DA01-根规范与科学.md×10、AUD-101-DB-15.md×7、AUD-202-SNR核验.md×7、AUD-101-DB-13.md×4、AUD-101-DB-16.md×4、AUD-101-DB-04.md×1、AUD-101-DB-12.md×1、AUD-101-DB-14.md×1、AUD-101-DB-19.md×1、AUD-101-DB-20.md×1 | AUD202·V2[PASS] DB13·W4[PASS/P1] 结果层与收口层·R3[PASS] | 入库 |
| `docs/design/UNIFIED_MODEL.md` | — | §1 把 `ivar=1/variance` 命名为"Phase2 逐像素科学权重"；§11 同一句既写"适用域=空背景随机分量"又写"直接入加权"（同句自相矛盾）；§4.6 称 HiPS 里"只存"帧级 SNR 与稀疏绝对 SNR | 三处都是**指称越界**，不是数学分歧：`ivar` 对天光建模与 UPM 控制点拟合是正确权重（那一组样本按 §5b 排异分层只取源掩膜外，其总方差即 `σ_bg²`，见 `PHASE2_UPM.md:21,:76`）；对阶段二叠加则须由 §5c 加权方差面给权。`07_noise_snr.md:201` 的"只存"与已定案的 variance/ivar 子产品（`DATA_SEMANTICS` | AUD-101-DB-09.md×4、AUD-101-DA01-根规范与科学.md×2、AUD-101-DB-10-补.md×2、AUD-101-DB-03.md×1、AUD-101-DB-14.md×1、AUD-101-DB-16.md×1、AUD-101-DB-20.md×1、D9-工单对账.md×1 | AUD202·V3[VOID] DB13·W1[PASS] 合同层·W2[PASS] | 入库 |
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 4 条 D3 主张） | AUD-202-SNR核验.md×2、AUD-301-文献复算-旧判批.md×2、AUD-401-架构对齐.md×1、AUD-402-判读-BD1.md×1 | AUD202-补·V5[PASS] | 入库 |
| `eng/ci/check_no_weight_mode.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-06-07.md×1 | DB13·W4[PASS/P1] 合同层·W2[PASS] | 入库 |

## 2 依据

《链路间口径对表》§9 真互斥 X1（科学方向已定：叠加权必须含源项，非两篇文档打架、不需裁决 ⇒ 待定案的只是「两侧各改什么」）；「复核-AUD202」V1（判定：确认，两臂定性不同须拆开定级；臂 B 只在 `ivar` 缺失时进入且显式声明 `uncertainty_available=false`/`weight_source=frame_snr` ⇒ 属已声明降级、非缺陷）；《SNR链路核验报告》R-1、DEV-01/DEV-02。

## 3 改法（具体动作，动词开头）

1. 实现侧：新增按含源项 σ 求值的逐像素加权方差面（`module_adapters.cpp:11936` 所在权臂）
2. 文档侧：给 `NOISE_MODEL.md:9`、`:328` 两句补适用域；把「降级路径且无逐像素权」与「权重链闭合」分开陈述，不得并列声称科学完备
3. 先跑 R-1 复测钉死现行生产走哪条臂：查 `p1_final.json#products` 是否含 `variance`/`ivar`、`p2_integrated.json` 的 `weight_basis`/`weight_source`/`uncertainty_unavailable_reason`，并按 `08_drizzle.md:70` 附 `n_variance_tiles>0` 的磁盘证据
4. 把 `07_noise_snr.md:138`（scheduler 未挂 variance 块 ⇒ 恒走帧级常量）与 `module_adapters.cpp:7880-8230`（variance 块已挂载且 fail-closed）两条互斥陈述由复测读数收敛为一条

## 4 文件域（本任务允许触碰的路径集合）

```text
lib/infrastructure/scheduler/src/module_adapters.cpp
docs/science/NOISE_MODEL.md
ASTROCS_DESIGN.md
docs/plugins/algorithms_phase1/07_noise_snr.md
docs/science/CONTROL_WEIGHT_SNR.md
docs/design/UNIFIED_MODEL.md
lib/algorithms/noise_snr/cpp/src/snr_science.cpp
eng/ci/check_no_weight_mode.py
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T01（复测读数需入库面）、ACSD-T05（同一批配置键的值级断言）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `module_adapters.cpp` 被 T20/T24/T29/T30/T31/T36/T41 七簇点到 ⇒ 同一文件的口径改动必须串行或合成一个提交（见总览 §5 冲突表）
  - 文档与实现同批：改权臂数据会触发一条锁旧口径的门（`check_no_weight_mode.py` 族），不同批改必红
  - `NOISE_MODEL.md` 与 T32/T40 共改 ⇒ 同批

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 把控制点权重的 σ 换成不含源项的背景 σ ⇒ 含源项断言必须判红
- 在文档把「降级路径」与「权重链闭合」并列声称 ⇒ 口径门判红
- 把稀疏层接上但控制点仍不按含源项 σ 求值 ⇒ 判红（第②层明示：接稀疏层不解决源项缺失）
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得为规避实现工作量而反向放宽文档（科学方向已定）；不得引 Zackay & Ofek 2017 支撑源受限域结论（其题名限定语 = 背景主导噪声极限）

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | AUD-101-D1残余.md:158;AUD-101-D1残余.md:295;AUD-101-DB-03.md:147;AUD-101-DB-03.md:158;AUD-101-DB-03.md:502;AUD-101-DB-03.md:61;AUD-101-DB-03.md:619;AUD-101-DB-03.md:636;AUD-101-DB-03.md:735;AUD-101-DB-03.md:88;AUD-101-DB-08.md:152;AUD-101-DB-11.md:493… | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 ‖ 架构接线·W1：确认**（成稿 AUD401-005 的事实面与计数全部独立复现成功；我的定级理由与改法方向与成稿不同，见"与成稿差异"） ‖ 架构接线·W3：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低） ‖ 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `docs/science/NOISE_MODEL.md` | AUD-101-DA01-根规范与科学.md:101;AUD-101-DA01-根规范与科学.md:112;AUD-101-DA01-根规范与科学.md:383;AUD-101-DA01-根规范与科学.md:387;AUD-101-DA01-根规范与科学.md:393;AUD-101-DA01-根规范与科学.md:61;AUD-101-DA01-根规范与科学.md:627;AUD-101-DA01-根规范与科学.md:652;AUD-101-DA01-根规范与科学.md:697;AUD-101-DA01-根规范与科学.md:701;AUD-101-DA01-根规范与科学.md:719;AUD-101-DA01-根规范与科学.md:768… | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `ASTROCS_DESIGN.md` | — | — |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` | AUD-101-DA01-根规范与科学.md:375;AUD-101-DB-03.md:310;AUD-101-DB-03.md:313;AUD-101-DB-03.md:318;AUD-101-DB-03.md:476;AUD-101-DB-03.md:480;AUD-101-DB-03.md:499;AUD-101-DB-10-补.md:123;AUD-101-DB-10.md:61;AUD-101-DB-13.md:285;AUD-101-DB-13.md:48;AUD-101-DB-14.md:130… | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `docs/science/CONTROL_WEIGHT_SNR.md` | AUD-101-DA01-根规范与科学.md:102;AUD-101-DA01-根规范与科学.md:104;AUD-101-DA01-根规范与科学.md:345;AUD-101-DA01-根规范与科学.md:349;AUD-101-DA01-根规范与科学.md:377;AUD-101-DA01-根规范与科学.md:380;AUD-101-DA01-根规范与科学.md:59;AUD-101-DA01-根规范与科学.md:649;AUD-101-DA01-根规范与科学.md:694;AUD-101-DA01-根规范与科学.md:727;AUD-101-DB-04.md:39;AUD-101-DB-12.md:202… | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） ‖ DB13·W4：确认（可结案，不必上呈） ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `docs/design/UNIFIED_MODEL.md` | AUD-101-DA01-根规范与科学.md:314;AUD-101-DA01-根规范与科学.md:323;AUD-101-DB-03.md:356;AUD-101-DB-09.md:215;AUD-101-DB-09.md:217;AUD-101-DB-09.md:237;AUD-101-DB-09.md:264;AUD-101-DB-10-补.md:254;AUD-101-DB-10-补.md:426;AUD-101-DB-14.md:42;AUD-101-DB-16.md:347;AUD-101-DB-20.md:347… | AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ DB13·W1：确认 ‖ 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp` | AUD-202-SNR核验.md:196;AUD-202-SNR核验.md:311;AUD-301-文献复算-旧判批.md:227;AUD-301-文献复算-旧判批.md:357;AUD-401-架构对齐.md:184;AUD-402-判读-BD1.md:43 | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 |
| `eng/ci/check_no_weight_mode.py` | AUD-101-DB-06-07.md:97 | DB13·W4：确认（可结案，不必上呈） ‖ 合同层·W2：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区 |

