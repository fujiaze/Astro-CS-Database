# 任务：ACSD-T24 把「零散度/不可估计」从伪装有限值改回具名退化，并抬到帧级门与产品声明面

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学+代码 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `lib/algorithms/coverage/src/sampler.cpp` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 6 条 D3 主张） | AUD-101-DB-05.md×1、AUD-101-DB01.md×1、AUD-203-天光无缝核验.md×1、AUD-402-判读-A3.md×1 | AUD203·V1[PASS/P1] AUD204·W3[PASS] 架构接线·W3[PASS] | 入库 |
| `lib/algorithms/coverage/src/sky_plane.cpp` | — | `sigma = (s0>0)? s0 : 1e-12` ⇒ 以数值保护量的平方生成有限方差，`civar ≈ 1.31e26` 进 UPM 加性面求解 | 正本 `PHASE2_UPM.md:84-85` §8 与 `PHASE2_SAMPLER.md §5.4`：`σ_bg_raw = 0 ⇒ control_ivar **必须为 0**`、`control_variance` 标为**无尺度信息（非有限）**，禁止以数值保护量生成有限方差发布。机制定性不是"除零未设守卫"，而是**有显式钳位**：正因有 1e−12，`:877` 的 0 分支与 （同对象另有 2 条 D3 主张） | AUD-203-天光无缝核验.md×1 | — | 入库 |
| `lib/algorithms/photometry/cpp/src/star_matcher.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | — | 入库 |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | — | （见 §2 依据） | （同对象另有 9 条 D3 主张） | AUD-101-DB-03.md×8、AUD-401-架构对齐.md×7、AUD-101-DB-19.md×6、AUD-202-SNR核验.md×6、AUD-101-DB-20.md×4、AUD-403-注释与README.md×3、D9-工单对账.md×3、AUD-101-D1残余.md×2、AUD-201-测光核验.md×2、AUD-101-DB-08.md×1、AUD-101-DB-11.md×1、AUD-101-DB-12.md×1、AUD-101-DB-18.md×1、AUD-402-判读-A3.md×1 | AUD202-补·V5[PASS] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] 架构接线·W1[PASS] 架构接线·W3[PASS] 测光默认阶数·三[PASS] | 入库 |
| `lib/algorithms/coverage/src/upm.cpp` | — | `sigma = (s0>0)? s0 : 1e-12` ⇒ 以数值保护量的平方生成有限方差，`civar ≈ 1.31e26` 进 UPM 加性面求解 | 正本 `PHASE2_UPM.md:84-85` §8 与 `PHASE2_SAMPLER.md §5.4`：`σ_bg_raw = 0 ⇒ control_ivar **必须为 0**`、`control_variance` 标为**无尺度信息（非有限）**，禁止以数值保护量生成有限方差发布。机制定性不是"除零未设守卫"，而是**有显式钳位**：正因有 1e−12，`:877` 的 0 分支与 （同对象另有 7 条 D3 主张） | AUD-203-天光无缝核验.md×5、AUD-101-DB-03.md×3、AUD-401-架构对齐.md×2、AUD-101-DB-04.md×1、AUD-101-DB-17.md×1、AUD-101-DB-20.md×1 | AUD203·V3[PASS/P2] AUD203·V5[PASS/P2] 架构接线·W3[PASS] | 入库 |
| `docs/science/PHASE2_UPM.md` | — | §1 把 `ivar=1/variance` 命名为"Phase2 逐像素科学权重"；§11 同一句既写"适用域=空背景随机分量"又写"直接入加权"（同句自相矛盾）；§4.6 称 HiPS 里"只存"帧级 SNR 与稀疏绝对 SNR | 三处都是**指称越界**，不是数学分歧：`ivar` 对天光建模与 UPM 控制点拟合是正确权重（那一组样本按 §5b 排异分层只取源掩膜外，其总方差即 `σ_bg²`，见 `PHASE2_UPM.md:21,:76`）；对阶段二叠加则须由 §5c 加权方差面给权。`07_noise_snr.md:201` 的"只存"与已定案的 variance/ivar 子产品（`DATA_SEMANTICS`（同对象另有 7 条 D3 主张） | AUD-101-DA01-根规范与科学.md×14、AUD-101-DA02-算法推导.md×10、AUD-101-DB-04.md×9、AUD-203-天光无缝核验.md×8、AUD-101-DB-20.md×5、AUD-101-DB-09.md×3、AUD-301-文献池P1.md×3、AUD-101-DB-18.md×2、AUD-101-D1残余.md×1、AUD-301-文献复算-旧判批.md×1、论文3-回执.md×1 | AUD202·V3[VOID] AUD203·V1[PASS/P1] AUD203·V2[PASS/P1] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] | 入库 |
| `docs/algorithms/PHASE2_SAMPLER.md` | — | `sigma = (s0>0)? s0 : 1e-12` ⇒ 以数值保护量的平方生成有限方差，`civar ≈ 1.31e26` 进 UPM 加性面求解 | 正本 `PHASE2_UPM.md:84-85` §8 与 `PHASE2_SAMPLER.md §5.4`：`σ_bg_raw = 0 ⇒ control_ivar **必须为 0**`、`control_variance` 标为**无尺度信息（非有限）**，禁止以数值保护量生成有限方差发布。机制定性不是"除零未设守卫"，而是**有显式钳位**：正因有 1e−12，`:877` 的 0 分支与 （同对象另有 2 条 D3 主张） | AUD-101-D1残余.md×4、AUD-101-DA02-算法推导.md×2、AUD-301-文献复算-旧判批.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-14.md×1、AUD-301-文献池P1.md×1 | AUD203·V1[PASS/P1] DB13·W4[PASS/P1] | 入库 |
| `lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp` | — | `snr_estimator.cpp:148,:322,:596` 返回码 2「退化」，旧接口**把整幅 SNR 填 1.0**；`orchestrator.cpp:4493-4494` 记日志后跳过 `snr_model` 块并 `return true` | 下游"认得"0 属另一模块的内部退化，既不使测光帧判红也不在产品面声明"此帧散度不可信" ⇒ 不构成 DEV-07 的降级理由，但应登记为整改可复用的**具名退化点**（哨兵与 rc 已存在，缺的是抬到帧级门与产品声明面）（同对象另有 1 条 D3 主张） | AUD-202-SNR核验.md×2、AUD-401-架构对齐.md×2 | — | 入库 |
| `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | — | 入库 |
| `docs/science/NOISE_MODEL.md` | — | Phase2 逐像素科学权重 = Phase1 落盘的 `ivar`（空背景方差面倒数） | 叠加权应为 `w = 1/σ_w²`，`σ_w² = σ_bg² + S_src/g`（`NOISE_MODEL.md` §5c 的加权方差面）。现状把**带适用域的近似**当**定义**：`1/σ_bg²` 只在 `S_src ≡ 0` 或 `S_src/g ≪ σ_bg²` 时等于最优权（同对象另有 5 条 D3 主张） | AUD-101-DA01-根规范与科学.md×12、AUD-101-DA02-算法推导.md×6、AUD-202-SNR核验.md×6、D9-工单对账.md×6、AUD-101-DB-03.md×5、AUD-101-DB-19.md×4、AUD-101-DB-20.md×3、AUD-101-DB-13.md×1、AUD-101-DB-14.md×1、AUD-101-DB-16.md×1、AUD-101-DB-17.md×1、AUD-301-文献复算-旧判批.md×1 | 结果层与收口层·R3[PASS] | 入库 |

## 2 依据

《链路间口径对表》§9 X5（测光与 UPM 同族）；「复核-AUD203」V1（判定：确认，定级 P1）：采样侧 1e-12 数值保护量把「零方差」伪装成 7.609e-27 的有限方差、逆方差 1.314e26，绕过下游 rc=2 的 fail-closed 门并独取该 cell 权重；《天光无缝核验报告》DEV-01 与反证行（同模块正确写法 `upm.cpp:2954-2962`，整改可直接复用该具名函数）；《测光链路核验报告》DEV-08（`snr_estimator.cpp:148,:322,:596` 返回码 2 把整幅 SNR 填 1.0；`orchestrator.cpp:4493-4494` 记日志后跳过 `snr_model` 块并 `return true`）。

## 3 改法（具体动作，动词开头）

1. 删除 `sampler.cpp:863-877` 与 `sky_plane.cpp:296`、`:306-307` 同款第二处的 1e-12 钳位，改为显式不可估计具名退化（方差非有限、逆方差 0）
2. 复用 `p2_upm_control_variance()` 的 `return 1` 写法作为唯一退化点，禁止再造第二套口径
3. 把已有的 rc/哨兵抬到帧级门与产品声明面：`sigma_residual ≤ 0` 时测光帧不得静默判绿，产品面须声明「此帧散度不可信」
4. 撤销 `snr=1.0` 作为缺省填充值（与 ACSD-T28 术语面同批），改 NaN 或合同侧显式允许
5. 同步订正 `docs/science/PHASE2_UPM.md`、`docs/algorithms/PHASE2_SAMPLER.md` 中把 1e-12 当物理量的表述

## 4 文件域（本任务允许触碰的路径集合）

```text
lib/algorithms/coverage/src/sampler.cpp
lib/algorithms/coverage/src/sky_plane.cpp
lib/algorithms/photometry/cpp/src/star_matcher.cpp
lib/infrastructure/scheduler/src/module_adapters.cpp
lib/algorithms/coverage/src/upm.cpp
docs/science/PHASE2_UPM.md
docs/algorithms/PHASE2_SAMPLER.md
lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp
lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
docs/science/NOISE_MODEL.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置/同批：ACSD-T03 的退化侧守卫定义（同一族，须同批定名）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `module_adapters.cpp` 同文件簇（与 T20/T29/T30/T31/T36/T41）⇒ 同批或串行
  - `PHASE2_UPM.md` 与 T02/T29/T35 共改 ⇒ 同批
  - 与 ACSD-T28 必须同批：哨兵语义与术语面一处改、一处留即自相矛盾

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 常数天光块（零方差）⇒ `control_ivar == 0` 且 `control_variance` 非有限（第②层给出的修好判据）；若仍得 1.314e26 ⇒ 判红
- 注入 `sigma_residual = 0` ⇒ 帧级门判红（现状：记日志后跳过并 `return true`）
- 删除 `1e-12` 而不同时补显式退化 ⇒ 负例夹具判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以「数值保护」为名保留伪装有限方差；不得改冻结容差而不走变更流程

## 8 登记与边界

跨链传导风险由测光报告 DEV-08 登记：一个看起来合法的假 SNR 会流入下游，故帧级声明与产品面声明都是必要落点。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `lib/algorithms/coverage/src/sampler.cpp` | AUD-101-DB-05.md:264;AUD-101-DB01.md:580;AUD-203-天光无缝核验.md:93;AUD-402-判读-A3.md:54 | AUD203·V1：确认 ‖ AUD204·W3：确认（并补两条成稿未报的更大分支；负责人待裁事项可据此定案） ‖ 架构接线·W3：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低） |
| `lib/algorithms/coverage/src/sky_plane.cpp` | AUD-203-天光无缝核验.md:106 | — |
| `lib/algorithms/photometry/cpp/src/star_matcher.cpp` | — | — |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | AUD-101-D1残余.md:158;AUD-101-D1残余.md:295;AUD-101-DB-03.md:147;AUD-101-DB-03.md:158;AUD-101-DB-03.md:502;AUD-101-DB-03.md:61;AUD-101-DB-03.md:619;AUD-101-DB-03.md:636;AUD-101-DB-03.md:735;AUD-101-DB-03.md:88;AUD-101-DB-08.md:152;AUD-101-DB-11.md:493… | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 ‖ 架构接线·W1：确认**（成稿 AUD401-005 的事实面与计数全部独立复现成功；我的定级理由与改法方向与成稿不同，见"与成稿差异"） ‖ 架构接线·W3：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低） ‖ 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `lib/algorithms/coverage/src/upm.cpp` | AUD-101-DB-03.md:104;AUD-101-DB-03.md:116;AUD-101-DB-03.md:652;AUD-101-DB-04.md:466;AUD-101-DB-17.md:240;AUD-101-DB-20.md:202;AUD-203-天光无缝核验.md:119;AUD-203-天光无缝核验.md:153;AUD-203-天光无缝核验.md:166;AUD-203-天光无缝核验.md:21;AUD-203-天光无缝核验.md:22;AUD-401-架构对齐.md:126… | AUD203·V3：确认 ‖ AUD203·V5：确认 ‖ 架构接线·W3：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低） |
| `docs/science/PHASE2_UPM.md` | AUD-101-D1残余.md:111;AUD-101-DA01-根规范与科学.md:108;AUD-101-DA01-根规范与科学.md:193;AUD-101-DA01-根规范与科学.md:360;AUD-101-DA01-根规范与科学.md:421;AUD-101-DA01-根规范与科学.md:425;AUD-101-DA01-根规范与科学.md:436;AUD-101-DA01-根规范与科学.md:626;AUD-101-DA01-根规范与科学.md:63;AUD-101-DA01-根规范与科学.md:664;AUD-101-DA01-根规范与科学.md:666;AUD-101-DA01-根规范与科学.md:695… | AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ AUD203·V1：确认 ‖ AUD203·V2：确认 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 |
| `docs/algorithms/PHASE2_SAMPLER.md` | AUD-101-D1残余.md:103;AUD-101-D1残余.md:371;AUD-101-D1残余.md:393;AUD-101-D1残余.md:99;AUD-101-DA01-根规范与科学.md:416;AUD-101-DA02-算法推导.md:1856;AUD-101-DA02-算法推导.md:2281;AUD-101-DB-14.md:42;AUD-301-文献复算-旧判批.md:116;AUD-301-文献复算-旧判批.md:342;AUD-301-文献池P1.md:632 | AUD203·V1：确认 ‖ DB13·W4：确认（可结案，不必上呈） |
| `lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp` | AUD-202-SNR核验.md:123;AUD-202-SNR核验.md:259;AUD-401-架构对齐.md:236;AUD-401-架构对齐.md:354 | — |
| `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp` | — | — |
| `docs/science/NOISE_MODEL.md` | AUD-101-DA01-根规范与科学.md:101;AUD-101-DA01-根规范与科学.md:112;AUD-101-DA01-根规范与科学.md:383;AUD-101-DA01-根规范与科学.md:387;AUD-101-DA01-根规范与科学.md:393;AUD-101-DA01-根规范与科学.md:61;AUD-101-DA01-根规范与科学.md:627;AUD-101-DA01-根规范与科学.md:652;AUD-101-DA01-根规范与科学.md:697;AUD-101-DA01-根规范与科学.md:701;AUD-101-DA01-根规范与科学.md:719;AUD-101-DA01-根规范与科学.md:768… | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |

