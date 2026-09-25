# 任务：ACSD-T02 两条入库门规：门的汇总读数与科学文档证据路径必须命中跟踪件

> 波次 `W0` ｜ 杠杆分档 `P1` ｜ 整改域 门禁+文档 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `artifacts/ci/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-12.md×3 | AUD201·V6[PASS] 门禁入口·W1[PASS] 门禁入口·W2[PASS] | 不可复核（读数件未入库） |
| `artifacts/evidence/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DA02-算法推导.md×18、AUD-101-DB-19.md×9、AUD-101-DB-12.md×7、AUD-101-DB01.md×7、AUD-101-D1残余.md×5、AUD-101-DB-10-补.md×5、AUD-101-DB-11.md×5、AUD-403-注释与README.md×4、AUD-101-DA01-根规范与科学.md×3、AUD-101-DB-17.md×3、AUD-101-DB-20.md×3、AUD-101-DB-03.md×2、AUD-101-DB-08.md×2、AUD-101-DB-09.md×2、AUD-101-DB-16.md×2、AUD-101-DB-04.md×1、AUD-101-DB-10.md×1、AUD-101-DB-13.md×1、AUD-101-DB-15.md×1、AUD-101-DB-18.md×1、AUD-201-测光核验.md×1、AUD-402-判读-A1.md×1、AUD-501-门禁现状审计.md×1 | 结果层与收口层·R1[PASS/P1] 门禁入口·W2[PASS] | 不可复核（读数件未入库） |
| `docs/science/PHOTOMETRY.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-19.md×23、AUD-201-测光核验.md×19、AUD-101-DA01-根规范与科学.md×7、AUD-101-DB-12.md×6、AUD-101-DB-18.md×6、AUD-101-DB-20.md×6、AUD-301-文献池P1.md×5、AUD-101-DB-03.md×4、AUD-301-文献复算-旧判批.md×4、AUD-101-DB-15.md×3、AUD-101-DB-16.md×3、AUD-101-D1残余.md×2、AUD-101-DA02-算法推导.md×2、AUD-101-DB-14.md×1、AUD-101-DB-17.md×1、AUD-101-DB02.md×1、论文1-回执.md×1 | AUD201·V4[PASS] AUD201·V5[PASS] | 入库 |
| `docs/science/STAR_DETECTION.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DA01-根规范与科学.md×4、AUD-101-DA02-算法推导.md×2、AUD-101-DB-03.md×1、AUD-101-DB-10-补.md×1、AUD-201-测光核验.md×1 | AUD201·V6[PASS] | 入库 |
| `docs/algorithms/GATES_AND_TOLERANCES.md` | — | 曾按"某条科学门的归档红灯"呈报 | 产生该读数的脚本已被门表明文降级为诊断脚本（`GATES_AND_TOLERANCES.md:77`，`eng/ci/checks.json` 对 `gate2` 0 命中）；现行两条登记门的收口载体**晚于该红读数落地** ⇒ 现状既不能判红也不能判绿 | AUD-101-DA02-算法推导.md×2 | AUD201·V6[PASS] | 入库 |
| `docs/science/PHASE2_UPM.md` | — | §1 把 `ivar=1/variance` 命名为"Phase2 逐像素科学权重"；§11 同一句既写"适用域=空背景随机分量"又写"直接入加权"（同句自相矛盾）；§4.6 称 HiPS 里"只存"帧级 SNR 与稀疏绝对 SNR | 三处都是**指称越界**，不是数学分歧：`ivar` 对天光建模与 UPM 控制点拟合是正确权重（那一组样本按 §5b 排异分层只取源掩膜外，其总方差即 `σ_bg²`，见 `PHASE2_UPM.md:21,:76`）；对阶段二叠加则须由 §5c 加权方差面给权。`07_noise_snr.md:201` 的"只存"与已定案的 variance/ivar 子产品（`DATA_SEMANTICS`（同对象另有 7 条 D3 主张） | AUD-101-DA01-根规范与科学.md×14、AUD-101-DA02-算法推导.md×10、AUD-101-DB-04.md×9、AUD-203-天光无缝核验.md×8、AUD-101-DB-20.md×5、AUD-101-DB-09.md×3、AUD-301-文献池P1.md×3、AUD-101-DB-18.md×2、AUD-101-D1残余.md×1、AUD-301-文献复算-旧判批.md×1、论文3-回执.md×1 | AUD202·V3[VOID] AUD203·V1[PASS/P1] AUD203·V2[PASS/P1] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] | 入库 |
| `docs/algorithms/PHASE2_SAMPLER.md` | — | `sigma = (s0>0)? s0 : 1e-12` ⇒ 以数值保护量的平方生成有限方差，`civar ≈ 1.31e26` 进 UPM 加性面求解 | 正本 `PHASE2_UPM.md:84-85` §8 与 `PHASE2_SAMPLER.md §5.4`：`σ_bg_raw = 0 ⇒ control_ivar **必须为 0**`、`control_variance` 标为**无尺度信息（非有限）**，禁止以数值保护量生成有限方差发布。机制定性不是"除零未设守卫"，而是**有显式钳位**：正因有 1e−12，`:877` 的 0 分支与 （同对象另有 2 条 D3 主张） | AUD-101-D1残余.md×4、AUD-101-DA02-算法推导.md×2、AUD-301-文献复算-旧判批.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-14.md×1、AUD-301-文献池P1.md×1 | AUD203·V1[PASS/P1] DB13·W4[PASS/P1] | 入库 |
| `实验/additive-sky-seamless/README.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-301-文献池P1.md×6、AUD-101-DB-17.md×2、AUD-101-DB-20.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-04.md×1、AUD-203-天光无缝核验.md×1 | AUD203·V2[PASS/P1] 结果层与收口层·R4[PASS] | 入库 |
| `实验/absolute-snr/README.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-13.md×8、AUD-101-DB-17.md×2、AUD-101-DB-19.md×2、AUD-101-DB-16.md×1、AUD-101-DB-20.md×1 | 结果层与收口层·R3[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `lib/algorithms/psf/tests/p1psf/p1psf_centroid_gate.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD201·V6[PASS] | 入库 |
| `lib/algorithms/psf/tests/p1psf/CMakeLists.txt` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-401-架构对齐.md×1 | AUD201·V6[PASS] | 入库 |
| `lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/evidence/gate2_result.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-201-测光核验.md×1 | AUD201·V6[PASS] | 入库 |
| `artifacts/evidence/audit-2026-01/FIX_LEDGER.csv` | — | 证据指针 `lib/photometric_calib/**`；标定读数"各支路 p95 ≤0.040 px"只活在正文 | `git ls-files lib/photometric_calib` = 0 ⇒ 跟踪台账的证据指针指向未跟踪旧路径；该标定读数的原始件在 `run/ci/ctest/`（不入库） | AUD-101-DA02-算法推导.md×2、AUD-101-D1补三份.md×1、AUD-101-DB01.md×1、AUD-201-测光核验.md×1、D9-工单对账.md×1 | AUD201·V6[PASS] 负责人面与索引·V1[PASS] | 不可复核（读数件未入库） |
| `.gitignore` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | — | 入库 |

## 2 依据

《链路间口径对表》§8 合案建议①②（四链同族的「证据落档缺失」同一根因）；「复核-AUD201」V6（判定：降级 — 读数、跟踪性、台账措辞复现，但「该读数属一条科学门」定性不成立 ⇒ 现状既不能判红也不能判绿）；《天光无缝核验报告》DEV-03（`.gitignore:17` = `run/*`，跟踪面 `git ls-files` 计数 0）；标准 05 §2。

## 3 改法（具体动作，动词开头）

1. 新增门规 A：在册门的汇总读数 JSON（数百字节级）登记进 `artifacts/ci/<sha>/` 或 `实验/*/results/`，文档只引用跟踪路径
2. 新增门规 B：`docs/science/**`、`docs/algorithms/**`、`docs/plugins/**` 中形如 `run/…`、`evidence/…` 的证据指针必须命中跟踪件，否则判红
3. 把 `PHASE2_SAMPLER.md:292,:297` 两处「实测」字样改指到入库读数件，或如实标「不可复核」
4. 质心门 p95 超容差那条改标「该读数不属科学门，既不能判红也不能判绿」，并按 `docs/science/STAR_DETECTION.md:15`/`:135` 与门表 `GATES_AND_TOLERANCES.md:52`、`:77` 的锚订正
5. 台账 `artifacts/evidence/audit-2026-01/FIX_LEDGER.csv` 与 `lib/photometric_calib/**` 全量悬空证据路径改指或删除
6. 把 `orchestrator/memory.md`、`pipeline/orchestrator/cpp/include/star_coord_contract.h` 等过程件按标准 04 §4 移出生产目录

## 4 文件域（本任务允许触碰的路径集合）

```text
artifacts/ci/
artifacts/evidence/
docs/science/PHOTOMETRY.md
docs/science/STAR_DETECTION.md
docs/algorithms/GATES_AND_TOLERANCES.md
docs/science/PHASE2_UPM.md
docs/algorithms/PHASE2_SAMPLER.md
实验/additive-sky-seamless/README.md
实验/absolute-snr/README.md
lib/algorithms/psf/tests/p1psf/p1psf_centroid_gate.cpp
lib/algorithms/psf/tests/p1psf/CMakeLists.txt
lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/evidence/gate2_result.json
artifacts/evidence/audit-2026-01/FIX_LEDGER.csv
.gitignore
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T01 的读数件字段名；与 ACSD-T06 同族，本任务的指针门复用其解析器
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - 与 ACSD-T06 必须同批或 T06 先：两者都判「指针可解析」，两侧解析器不同即口径分裂
  - 与 ACSD-T09 共改 `FIX_LEDGER.csv` 与 `docs/KNOWN_LIMITATIONS.md`

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 在任一科学文档写 `run/NOTHING/evidence/x.json` 作为证据指针 ⇒ 判红
- 删除某在册门的汇总读数件 ⇒ 判红
- 以零字节文件充当证据 ⇒ 判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得用 waiver 让「证据缺失」继续判绿；不得把不可复核的数字写成实测值

## 8 登记与边界

同时消化《链路间口径对表》措辞 W6：`78%` vs `72.6%`、`0.459 dex`/`0.0107 mag` 一类只有历史或包内读数、无跟踪副本的数字，引用时改标「不可复核/口径未记」，并只写有跟踪副本的那一个。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `artifacts/ci/` | AUD-101-DB-12.md:166;AUD-101-DB-12.md:168;AUD-101-DB-12.md:183 | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ 门禁入口·W1：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `artifacts/evidence/` | AUD-101-D1残余.md:114;AUD-101-D1残余.md:241;AUD-101-D1残余.md:243;AUD-101-D1残余.md:283;AUD-101-D1残余.md:361;AUD-101-DA01-根规范与科学.md:275;AUD-101-DA01-根规范与科学.md:514;AUD-101-DA01-根规范与科学.md:751;AUD-101-DA02-算法推导.md:1048;AUD-101-DA02-算法推导.md:1074;AUD-101-DA02-算法推导.md:1096;AUD-101-DA02-算法推导.md:1146… | 结果层与收口层·R1：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现 ‖ 门禁入口·W2：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立； |
| `docs/science/PHOTOMETRY.md` | AUD-101-D1残余.md:185;AUD-101-D1残余.md:186;AUD-101-DA01-根规范与科学.md:100;AUD-101-DA01-根规范与科学.md:326;AUD-101-DA01-根规范与科学.md:330;AUD-101-DA01-根规范与科学.md:337;AUD-101-DA01-根规范与科学.md:58;AUD-101-DA01-根规范与科学.md:699;AUD-101-DA01-根规范与科学.md:745;AUD-101-DA02-算法推导.md:645;AUD-101-DA02-算法推导.md:688;AUD-101-DB-03.md:464… | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） ‖ AUD201·V5：确认**（我独立构造可达、判据实读放行；并给成稿补三条它没有的事实，其中一条把证据级别从"构造"抬到"库内实跑日志"） |
| `docs/science/STAR_DETECTION.md` | AUD-101-DA01-根规范与科学.md:554;AUD-101-DA01-根规范与科学.md:558;AUD-101-DA01-根规范与科学.md:569;AUD-101-DA01-根规范与科学.md:70;AUD-101-DA02-算法推导.md:2175;AUD-101-DA02-算法推导.md:890;AUD-101-DB-03.md:407;AUD-101-DB-10-补.md:57;AUD-201-测光核验.md:265 | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） |
| `docs/algorithms/GATES_AND_TOLERANCES.md` | AUD-101-DA02-算法推导.md:1473;AUD-101-DA02-算法推导.md:511 | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） |
| `docs/science/PHASE2_UPM.md` | AUD-101-D1残余.md:111;AUD-101-DA01-根规范与科学.md:108;AUD-101-DA01-根规范与科学.md:193;AUD-101-DA01-根规范与科学.md:360;AUD-101-DA01-根规范与科学.md:421;AUD-101-DA01-根规范与科学.md:425;AUD-101-DA01-根规范与科学.md:436;AUD-101-DA01-根规范与科学.md:626;AUD-101-DA01-根规范与科学.md:63;AUD-101-DA01-根规范与科学.md:664;AUD-101-DA01-根规范与科学.md:666;AUD-101-DA01-根规范与科学.md:695… | AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ AUD203·V1：确认 ‖ AUD203·V2：确认 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 |
| `docs/algorithms/PHASE2_SAMPLER.md` | AUD-101-D1残余.md:103;AUD-101-D1残余.md:371;AUD-101-D1残余.md:393;AUD-101-D1残余.md:99;AUD-101-DA01-根规范与科学.md:416;AUD-101-DA02-算法推导.md:1856;AUD-101-DA02-算法推导.md:2281;AUD-101-DB-14.md:42;AUD-301-文献复算-旧判批.md:116;AUD-301-文献复算-旧判批.md:342;AUD-301-文献池P1.md:632 | AUD203·V1：确认 ‖ DB13·W4：确认（可结案，不必上呈） |
| `实验/additive-sky-seamless/README.md` | AUD-101-DA01-根规范与科学.md:668;AUD-101-DB-04.md:37;AUD-101-DB-17.md:169;AUD-101-DB-17.md:178;AUD-101-DB-20.md:201;AUD-101-DB-20.md:206;AUD-203-天光无缝核验.md:8;AUD-301-文献池P1.md:124;AUD-301-文献池P1.md:144;AUD-301-文献池P1.md:240;AUD-301-文献池P1.md:259;AUD-301-文献池P1.md:849… | AUD203·V2：确认 ‖ 结果层与收口层·R4：确认（三处不一致全部复现），但定级按"是否进判据/对外主张"分档；成稿的 1 处对照口径需订正 |
| `实验/absolute-snr/README.md` | AUD-101-DB-13.md:243;AUD-101-DB-13.md:247;AUD-101-DB-13.md:259;AUD-101-DB-13.md:282;AUD-101-DB-13.md:283;AUD-101-DB-13.md:326;AUD-101-DB-13.md:327;AUD-101-DB-13.md:50;AUD-101-DB-16.md:94;AUD-101-DB-17.md:143;AUD-101-DB-17.md:159;AUD-101-DB-19.md:1445… | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `lib/algorithms/psf/tests/p1psf/p1psf_centroid_gate.cpp` | — | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） |
| `lib/algorithms/psf/tests/p1psf/CMakeLists.txt` | AUD-401-架构对齐.md:396 | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） |
| `lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/evidence/gate2_result.json` | AUD-201-测光核验.md:264 | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） |
| `artifacts/evidence/audit-2026-01/FIX_LEDGER.csv` | AUD-101-D1补三份.md:69;AUD-101-DA02-算法推导.md:916;AUD-101-DA02-算法推导.md:985;AUD-101-DB01.md:111;AUD-201-测光核验.md:268;D9-工单对账.md:569 | AUD201·V6：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿） ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `.gitignore` | — | — |

