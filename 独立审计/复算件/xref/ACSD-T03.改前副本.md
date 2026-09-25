# 任务：ACSD-T03 零离散度假绿通道、对插值算子恒 0 的重建误差判据、求和型守恒门的错分盲区

> 波次 `W0` ｜ 杠杆分档 `P0` ｜ 整改域 门禁+科学 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `docs/science/PHOTOMETRY.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-19.md×23、AUD-201-测光核验.md×19、AUD-101-DA01-根规范与科学.md×7、AUD-101-DB-12.md×6、AUD-101-DB-18.md×6、AUD-101-DB-20.md×6、AUD-301-文献池P1.md×5、AUD-101-DB-03.md×4、AUD-301-文献复算-旧判批.md×4、AUD-101-DB-15.md×3、AUD-101-DB-16.md×3、AUD-101-D1残余.md×2、AUD-101-DA02-算法推导.md×2、AUD-101-DB-14.md×1、AUD-101-DB-17.md×1、AUD-101-DB02.md×1、论文1-回执.md×1 | AUD201·V4[PASS] AUD201·V5[PASS] | 入库 |
| `eng/ci/fixtures/provenance/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD201·V5[PASS] | 入库 |
| `eng/contracts/schemas/` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-04.md×4、AUD-101-DB-09.md×4、AUD-101-DB01.md×2、AUD-402-判读-A2.md×1 | AUD201·V5[PASS] 合同层·W3[PASS] | 入库 |
| `lib/infrastructure/pipeline/orchestrator/memory.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD201·V5[PASS] | 入库 |
| `lib/algorithms/integration/v6/src/weight_chain.cpp` | — | 曾与 DEV-01 捆成一条头条，称"两条臂各中一次：权重在信号维退化为常数" | 臂 B 只在 `ivar` 缺失时进入（`ivar_missing>0`），且**显式声明** `uncertainty_available=false` ＋ `weight_source="frame_snr"` ＋ `weight_basis="frame_snr_ivar"` ⇒ 属**已声明的降级**，与逐帧标量权重在业界（SWarp `sigfac`、PixInsight 帧权）同型，*（同对象另有 1 条 D3 主张） | AUD-202-SNR核验.md×2 | AUD202-补·V5[PASS] | 入库 |
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 4 条 D3 主张） | AUD-202-SNR核验.md×2、AUD-301-文献复算-旧判批.md×2、AUD-401-架构对齐.md×1、AUD-402-判读-BD1.md×1 | AUD202-补·V5[PASS] | 入库 |
| `docs/plugins/algorithms_phase1/13_integration.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | — | 入库 |
| `docs/plugins/algorithms_phase2/13_integration.md` | — | manifest 的 `node_reproduction_max_abs` 登记为「重建误差」；`weight_chain.h:167` 自注"应 ~0" | 二者对同一字段给出互斥定性（一名为误差量、一称为应恒 0）；该字段在可达输入集上恒 0 ⇒ **无证据资格**；合同要求的逐像素**预测方差**从未实现（字段面缺位、`git log -S` 两条命中均在实验侧）。另：`EXP-04-RECONSTRUCTION.md:648`（判据 S4）本仓已自登记"容差 0.5 dex 对精确插值类过松，不具举证资格"，但 manifest 名面未跟着改 | AUD-101-DB-04.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-202-SNR核验.md×1 | AUD202-补·V5[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `lib/algorithms/drizzle/healpix_drizzle/tests/reference_overlap.cpp` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD204·W1[PASS] | 入库 |
| `docs/algorithms/DRIZZLE_GEOMETRY.md` | — | 生产面积算法唯一命名 = 球面逐边裁剪 ＋ "Van Oosterom & Strackee 扇形三角剖分" | 该文献在台账已判 `关联错`；按"平面三角形立体角公式"用于球面三角面积看似正确用法，但与判错面不同 ⇒ 适用性未复核（同对象另有 3 条 D3 主张） | D9-工单对账.md×8、AUD-204-面积交叠核验.md×4、AUD-301-文献复算-旧判批.md×3、AUD-101-DA01-根规范与科学.md×2、AUD-101-DA02-算法推导.md×2、AUD-101-DB-03.md×2、AUD-101-DB-17.md×1、AUD-101-DB01.md×1、AUD-301-文献池P1.md×1 | AUD204·W1[PASS] AUD204·W2[PASS] | 入库 |
| `实验/shared/synthetic/noise_model.py` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD202-补·V5[PASS] | 入库 |
| `lib/algorithms/drizzle/healpix_drizzle/v6_spherical_overlap.h` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | AUD204·W1[PASS] | 入库 |

## 2 依据

「复核-AUD201」V5（判定：确认，独立构造可达、判据实读放行）；「复核-AUD202-补」V5②③（判定：确认，18 组换源自测逐位 0，唯一能红面已被上游 `:479-486` 门接管 ⇒ 可达输入集上恒 0）；「复核-AUD204」W1（判定：确认 — 面积闭合是代数恒等，「1e-15 闭合」无证据资格）；《链路间口径对表》§8 形态一与合案建议④；标准 05 §2；AGENTS.md §5（判据必须非退化）。

## 3 改法（具体动作，动词开头）

1. 给 `sigma_residual` 类只卡上界与有限性的判据补退化侧守卫：零离散度 ⇒ 判红或显式具名「不可估计」
2. 把 `node_reproduction_max_abs` 改为「n=0 即红」守卫 + 换源必红断言（第②层给出的补项）
3. 求和型守恒判据补「定位句 + 同堂错分负例」：构造总量不变、逐叶/逐源错分的夹具，判据必须判红
4. 撤销「构造闭合 ⇒ 面积口径正确」的证据资格：在 `DRIZZLE_GEOMETRY.md` 与各报告里写明该判据只判代数恒等
5. 把测光 `sigma_residual` 与天光 `civar`/`cvar` 的「不可估计/零散度」哨兵统一到同一具名退化点（复用 `upm.cpp:2954-2962` `p2_upm_control_variance()` 的 `return 1` 写法）

## 4 文件域（本任务允许触碰的路径集合）

```text
docs/science/PHOTOMETRY.md
eng/ci/fixtures/provenance/
eng/contracts/schemas/
lib/infrastructure/pipeline/orchestrator/memory.md
lib/algorithms/integration/v6/src/weight_chain.cpp
lib/algorithms/noise_snr/cpp/src/snr_science.cpp
docs/plugins/algorithms_phase1/13_integration.md
docs/plugins/algorithms_phase2/13_integration.md
lib/algorithms/drizzle/healpix_drizzle/tests/reference_overlap.cpp
docs/algorithms/DRIZZLE_GEOMETRY.md
实验/shared/synthetic/noise_model.py
lib/algorithms/drizzle/healpix_drizzle/v6_spherical_overlap.h
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 与 ACSD-T24（X5 哨兵表示）互为依据：哨兵形态与本族的退化侧守卫须同批定名
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - 与 ACSD-T04（未注册可执行件）必须同批：本任务写的错分负例要落在真正在册的门上，否则负例无判红主体

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 注入「总量守恒、逐叶错分」夹具 ⇒ 守恒判据必须判红（现状恒过）
- 把插值算子的重建误差换成另一实现源 ⇒ `node_reproduction_max_abs` 必须非 0（现状逐位 0）
- 把 `sigma_residual` 置为 0 ⇒ 判红而非判绿（现状放行）
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以「再加一个上界」替代退化侧守卫；不得把恒真判据降格为「信息性输出」以规避红灯

## 8 登记与边界

测光 V3 的「改锚分箱轴」路径已被第②层判为无效整改路径，本任务不采纳；分箱轴对色项缺陷趋零属「度量与缺陷不同源」，改走 T31。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `docs/science/PHOTOMETRY.md` | AUD-101-D1残余.md:185;AUD-101-D1残余.md:186;AUD-101-DA01-根规范与科学.md:100;AUD-101-DA01-根规范与科学.md:326;AUD-101-DA01-根规范与科学.md:330;AUD-101-DA01-根规范与科学.md:337;AUD-101-DA01-根规范与科学.md:58;AUD-101-DA01-根规范与科学.md:699;AUD-101-DA01-根规范与科学.md:745;AUD-101-DA02-算法推导.md:645;AUD-101-DA02-算法推导.md:688;AUD-101-DB-03.md:464… | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） ‖ AUD201·V5：确认**（我独立构造可达、判据实读放行；并给成稿补三条它没有的事实，其中一条把证据级别从"构造"抬到"库内实跑日志"） |
| `eng/ci/fixtures/provenance/` | — | AUD201·V5：确认**（我独立构造可达、判据实读放行；并给成稿补三条它没有的事实，其中一条把证据级别从"构造"抬到"库内实跑日志"） |
| `eng/contracts/schemas/` | AUD-101-DB-04.md:172;AUD-101-DB-04.md:173;AUD-101-DB-04.md:542;AUD-101-DB-04.md:546;AUD-101-DB-09.md:120;AUD-101-DB-09.md:207;AUD-101-DB-09.md:230;AUD-101-DB-09.md:250;AUD-101-DB01.md:200;AUD-101-DB01.md:454;AUD-402-判读-A2.md:113 | AUD201·V5：确认**（我独立构造可达、判据实读放行；并给成稿补三条它没有的事实，其中一条把证据级别从"构造"抬到"库内实跑日志"） ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |
| `lib/infrastructure/pipeline/orchestrator/memory.md` | — | AUD201·V5：确认**（我独立构造可达、判据实读放行；并给成稿补三条它没有的事实，其中一条把证据级别从"构造"抬到"库内实跑日志"） |
| `lib/algorithms/integration/v6/src/weight_chain.cpp` | AUD-202-SNR核验.md:103;AUD-202-SNR核验.md:374 | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 |
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp` | AUD-202-SNR核验.md:196;AUD-202-SNR核验.md:311;AUD-301-文献复算-旧判批.md:227;AUD-301-文献复算-旧判批.md:357;AUD-401-架构对齐.md:184;AUD-402-判读-BD1.md:43 | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 |
| `docs/plugins/algorithms_phase1/13_integration.md` | — | — |
| `docs/plugins/algorithms_phase2/13_integration.md` | AUD-101-DA01-根规范与科学.md:456;AUD-101-DB-04.md:80;AUD-101-DB-04.md:84;AUD-202-SNR核验.md:224 | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `lib/algorithms/drizzle/healpix_drizzle/tests/reference_overlap.cpp` | — | AUD204·W1：确认（并订正成稿两处口径） |
| `docs/algorithms/DRIZZLE_GEOMETRY.md` | AUD-101-DA01-根规范与科学.md:103;AUD-101-DA01-根规范与科学.md:470;AUD-101-DA02-算法推导.md:1527;AUD-101-DA02-算法推导.md:250;AUD-101-DB-03.md:511;AUD-101-DB-03.md:521;AUD-101-DB-17.md:296;AUD-101-DB01.md:724;AUD-204-面积交叠核验.md:115;AUD-204-面积交叠核验.md:147;AUD-204-面积交叠核验.md:6;AUD-204-面积交叠核验.md:66… | AUD204·W1：确认（并订正成稿两处口径） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） |
| `实验/shared/synthetic/noise_model.py` | — | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 |
| `lib/algorithms/drizzle/healpix_drizzle/v6_spherical_overlap.h` | — | AUD204·W1：确认（并订正成稿两处口径） |

