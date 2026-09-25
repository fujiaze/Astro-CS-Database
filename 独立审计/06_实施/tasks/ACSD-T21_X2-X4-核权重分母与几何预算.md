# 任务：ACSD-T21 面积口径两真互斥：核权重分母 `A_drop` vs `A_pixel`、冻结几何预算被违反 2–5 个数量级

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 3 条 D3 主张） | AUD-202-SNR核验.md×1、AUD-204-面积交叠核验.md×1 | — | 入库 |
| `docs/contracts/DATA_SEMANTICS.md` | — | 同一份文档体系对**同一符号**给两种分母：代码与 FROZEN 正本 `docs/science/DRIZZLE.md:44-50` 为 `w_jp = a_jp/A_drop,j`（其 §10 禁止项逐字写着"把核权重写回 `a_jp/A_pixel,j`（FZ-COND-FLUX-CONSERV 判红）"）；而 `DATA_SEMA | 唯一实现口径是 `a_jp/A_drop`；`w'_jp = a_jp/A_pixel` 只作为**等价参数化**成立，且必须同时换分母（`N'_p = Σ w'·A_pixel = D_p`）。等价性已独立复核：发布 `S_p` 与 `variance_p` 对两种参数化**不变**（分子分母各乘 `pf⁴` 相消）；但 `Σ_p w_jp = 1`（⇒ `Σ_p F_p = Σ_j x_j`、（同对象另有 1 条 D3 主张） | AUD-101-DA02-算法推导.md×9、AUD-101-DB01.md×9、AUD-101-DB-06-07.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-04.md×3、AUD-101-DB-10-补.md×3、AUD-101-DA01-根规范与科学.md×2、AUD-101-DB-03.md×2、AUD-204-面积交叠核验.md×2、AUD-101-DB-13.md×1、AUD-402-判读-BD1.md×1、AUD-403-注释与README.md×1 | DB13·W1[PASS] DB13·W4[PASS/P1] 合同层·W1[PASS] | 入库 |
| `docs/algorithms/DRIZZLE_GEOMETRY.md` | — | 生产面积算法唯一命名 = 球面逐边裁剪 ＋ "Van Oosterom & Strackee 扇形三角剖分" | 该文献在台账已判 `关联错`；按"平面三角形立体角公式"用于球面三角面积看似正确用法，但与判错面不同 ⇒ 适用性未复核（同对象另有 3 条 D3 主张） | D9-工单对账.md×8、AUD-204-面积交叠核验.md×4、AUD-301-文献复算-旧判批.md×3、AUD-101-DA01-根规范与科学.md×2、AUD-101-DA02-算法推导.md×2、AUD-101-DB-03.md×2、AUD-101-DB-17.md×1、AUD-101-DB01.md×1、AUD-301-文献池P1.md×1 | AUD204·W1[PASS] AUD204·W2[PASS] | 入库 |
| `ASTROCS_DESIGN.md` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_（同对象另有 11 条 D3 主张） | — | — | 入库 |
| `docs/science/DRIZZLE.md` | — | （见 §2 依据） | （同对象另有 2 条 D3 主张） | AUD-101-DB01.md×11、D9-工单对账.md×9、AUD-101-DA02-算法推导.md×7、AUD-101-DA01-根规范与科学.md×6、AUD-301-文献池P1.md×6、AUD-101-DB-10-补.md×5、AUD-204-面积交叠核验.md×4、AUD-301-文献复算-旧判批.md×4、D9-工单对账-补.md×3、AUD-101-DB-03.md×2、AUD-101-DB02.md×1、AUD-402-判读-BD1.md×1 | AUD204·W2[PASS] | 入库 |
| `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp` | — | （见 §2 依据） | （同对象另有 1 条 D3 主张） | AUD-204-面积交叠核验.md×2、AUD-301-文献复算-旧判批.md×2 | — | 入库 |
| `docs/KNOWN_LIMITATIONS.md` | — | 生产 leaf = 整数格点四角的**大圆弦四边形**（`nside≥256 ⇒ nb=4`；`9≤nside<256` 亦只 4 角；生产 nside 钳位 `[16, 2²²]` 内**无任何路径**用真曲线边界）；冻结预算 `arc-chord 1e-6·hp_res`；`subdivide_healpix_edge` 注释自述"对 | 冻结预算被违反 **2–5 个数量级**且是**全天空现象**：矢高 max `8.094e−2·hp_res`（极冠，尺度不变）、`6.587e−4`（缝带）、`1.443e−4`（赤道），99.55% 的边超阈；逐叶面积误差 **−9.97%…+0.54%**（两路独立实现同值）且**不随 nside 收缩**；`:558-562` 另自述旧口径 `hp_res·1e-12` 对非大圆弧边"永 | AUD-101-DB-04.md×8、AUD-101-DB-11.md×7、AUD-101-DA01-根规范与科学.md×3、AUD-101-DB01.md×3、D9-工单对账.md×2、AUD-101-D1残余.md×1、AUD-101-DB-19.md×1、AUD-402-判读-A1.md×1、AUD-402-判读-A2.md×1、D9-工单对账-补.md×1 | AUD204·W2[PASS] 负责人面与索引·V1[PASS] | 入库 |
| `docs/science/PSF_SIGNAL_WEIGHT.md` | — | 双计使 `σ_F` 高估 `+12.8%`（基准点）至 `+34.0%`（RN=50 最坏点） | 头条取的是 **N=1000 单次 MC 实现值**（`b2_noise_terms.py:184` 的分母是该 seed 那 1000 帧的实测散布），而同一 JSON 里就躺着 seed 无关的闭式 `pred_doublecount_bias = +14.5009%`。"同一物理点两个数"的成因**不是口径分歧**，是分母的样本噪声：四条同点轴（非两条）的分子极差 0.027%、分母极差 4（同对象另有 1 条 D3 主张） | AUD-101-DB-16.md×12、AUD-101-DA01-根规范与科学.md×6、AUD-202-SNR核验.md×4、AUD-101-DB-04.md×2、AUD-101-DB-12.md×2、AUD-101-DB-13.md×2、AUD-101-DB-03.md×1、AUD-101-DB-10.md×1 | DB13·W1[PASS] | 入库 |
| `docs/algorithms/DRIZZLE_ALGORITHMS.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | — | — | 待核：该路径不在仓库跟踪集，先定对象再派工 |

## 2 依据

《链路间口径对表》§9 X2、X4 与措辞 W4；《面积交叠核验报告》DEV-04/DEV-05；「复核-AUD204」W1（确认）、W2（判定：降级 — 成稿两处口径混用、折线缝精度平台不可当作几何事实，但另有更大的真实预算违反）。

## 3 改法（具体动作，动词开头）

1. 定案核权重分母（`A_drop` 或 `A_pixel`），并在 `DATA_SEMANTICS.md:47`、`DRIZZLE_GEOMETRY.md:337-346`、`ASTROCS_DESIGN.md:209` 与 `drizzle_engine.cpp:1617` 四处写同名量与同一选择
2. 把 `k = pixfrac²` 改写为 `pf²·(1+⟨δ⟩)`，点名旧 `flux_conservation_factor = pf²` 已作废，「通量守恒」一律带量名
3. 面积口径改用真曲线边界（现成件 `subdivide_healpix_edge`，调用点从 `nb=4/samples=1` 接回）；或至少对极冠/face 角点邻域启用细分并保证共享边两侧采样一致
4. 同批登记代价：从极冠边偏差 `0.0809·hp_res` 起算逐层 ÷4，降到自身阈值 `1e-6·hp_res` 需 `d > log(8.09e4)/log(4) = 8.15` 层，而 `HP_ADAPTIVE_MAX_DEPTH = 8` ⇒ 极点邻边触底截断（残差 `1.234e-6·hp_res`，超阈 23%）、每边 256 段 = 1024 顶点/像素
5. 订正 `DRIZZLE_GEOMETRY.md:313` 的预算表述与 `:558-562` 的「对所有 NSIDE 统一使用自适应边细分」（注释与调用点相反）
6. 与 `docs/KNOWN_LIMITATIONS.md:139` 已登记但被低估（只说「与 1e-6 同阶」）的同一现象合并处理

## 4 文件域（本任务允许触碰的路径集合）

```text
lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp
docs/contracts/DATA_SEMANTICS.md
docs/algorithms/DRIZZLE_GEOMETRY.md
ASTROCS_DESIGN.md
docs/science/DRIZZLE.md
lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp
docs/KNOWN_LIMITATIONS.md
docs/science/PSF_SIGNAL_WEIGHT.md
docs/algorithms/DRIZZLE_ALGORITHMS.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T03（守恒判据的错分负例）—— 否则改面积口径无门可验
  - 与 ACSD-T22 同批执行（同文件、同节、同容差面）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `DRIZZLE_GEOMETRY.md` 由 T03/T21/T22 三簇共改 ⇒ 必须同批；`docs/KNOWN_LIMITATIONS.md` 由 T02/T09/T21 共改 ⇒ 同批
  - `DATA_SEMANTICS.md` 由 T08/T21/T28/T33/T36/T37/T38 共改 ⇒ 见冲突表，须由一个簇统一领改，其余簇以指针引用

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 构造总量不变、逐叶错分的分配夹具 ⇒ 求和型守恒门必须判红（现状恒过）
- 把 `subdivide_healpix_edge` 的调用点留在 `nb=4` ⇒ 逐叶面积误差 `-9.97%…+0.54%` 复现并判红（两路独立实现同值，99.55% 的边超阈）
- 在文档写 `k = pixfrac²` ⇒ 作废因子门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以放宽冻结阈值（如该节既有「`hp_res·1e-12` 对非大圆弧边永不收敛、已废弃」式处理）代替精确边界；改默认容差须走变更流程（AGENTS.md §6）

## 8 登记与边界

常量面亮度场的 `S_p` 因分子分母同错而不受影响：受影响面为 `support/coverage`、亚叶通量再分配、跨帧叠加的几何一致性 —— 判据须落在这三面上。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` | AUD-202-SNR核验.md:127;AUD-204-面积交叠核验.md:146 | — |
| `docs/contracts/DATA_SEMANTICS.md` | AUD-101-DA01-根规范与科学.md:359;AUD-101-DA01-根规范与科学.md:607;AUD-101-DA02-算法推导.md:1052;AUD-101-DA02-算法推导.md:1055;AUD-101-DA02-算法推导.md:1323;AUD-101-DA02-算法推导.md:1750;AUD-101-DA02-算法推导.md:2134;AUD-101-DA02-算法推导.md:2204;AUD-101-DA02-算法推导.md:450;AUD-101-DA02-算法推导.md:772;AUD-101-DA02-算法推导.md:803;AUD-101-DB-03.md:38… | DB13·W1：确认 ‖ DB13·W4：确认（可结案，不必上呈） ‖ 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |
| `docs/algorithms/DRIZZLE_GEOMETRY.md` | AUD-101-DA01-根规范与科学.md:103;AUD-101-DA01-根规范与科学.md:470;AUD-101-DA02-算法推导.md:1527;AUD-101-DA02-算法推导.md:250;AUD-101-DB-03.md:511;AUD-101-DB-03.md:521;AUD-101-DB-17.md:296;AUD-101-DB01.md:724;AUD-204-面积交叠核验.md:115;AUD-204-面积交叠核验.md:147;AUD-204-面积交叠核验.md:6;AUD-204-面积交叠核验.md:66… | AUD204·W1：确认（并订正成稿两处口径） ‖ AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） |
| `ASTROCS_DESIGN.md` | — | — |
| `docs/science/DRIZZLE.md` | AUD-101-DA01-根规范与科学.md:321;AUD-101-DA01-根规范与科学.md:459;AUD-101-DA01-根规范与科学.md:463;AUD-101-DA01-根规范与科学.md:65;AUD-101-DA01-根规范与科学.md:680;AUD-101-DA01-根规范与科学.md:731;AUD-101-DA02-算法推导.md:1534;AUD-101-DA02-算法推导.md:1542;AUD-101-DA02-算法推导.md:1595;AUD-101-DA02-算法推导.md:1622;AUD-101-DA02-算法推导.md:1802;AUD-101-DA02-算法推导.md:484… | AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） |
| `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp` | AUD-204-面积交叠核验.md:30;AUD-204-面积交叠核验.md:93;AUD-301-文献复算-旧判批.md:280;AUD-301-文献复算-旧判批.md:359 | — |
| `docs/KNOWN_LIMITATIONS.md` | AUD-101-D1残余.md:264;AUD-101-DA01-根规范与科学.md:685;AUD-101-DA01-根规范与科学.md:690;AUD-101-DA01-根规范与科学.md:703;AUD-101-DB-04.md:130;AUD-101-DB-04.md:137;AUD-101-DB-04.md:149;AUD-101-DB-04.md:331;AUD-101-DB-04.md:428;AUD-101-DB-04.md:506;AUD-101-DB-04.md:570;AUD-101-DB-04.md:571… | AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） ‖ 负责人面与索引·V1：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写； |
| `docs/science/PSF_SIGNAL_WEIGHT.md` | AUD-101-DA01-根规范与科学.md:117;AUD-101-DA01-根规范与科学.md:361;AUD-101-DA01-根规范与科学.md:364;AUD-101-DA01-根规范与科学.md:368;AUD-101-DA01-根规范与科学.md:374;AUD-101-DA01-根规范与科学.md:60;AUD-101-DB-03.md:431;AUD-101-DB-04.md:92;AUD-101-DB-04.md:96;AUD-101-DB-10.md:64;AUD-101-DB-12.md:201;AUD-101-DB-12.md:203… | DB13·W1：确认 |
| `docs/algorithms/DRIZZLE_ALGORITHMS.md` | — | — |

