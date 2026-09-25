# 任务：ACSD-T32 三口径图谱测的是 σ̂ 估计器而非 SNR：补一组以 F_ref 为分子的三臂对拍

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学+实验 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `docs/science/NOISE_MODEL.md` | — | Phase2 逐像素科学权重 = Phase1 落盘的 `ivar`（空背景方差面倒数） | 叠加权应为 `w = 1/σ_w²`，`σ_w² = σ_bg² + S_src/g`（`NOISE_MODEL.md` §5c 的加权方差面）。现状把**带适用域的近似**当**定义**：`1/σ_bg²` 只在 `S_src ≡ 0` 或 `S_src/g ≪ σ_bg²` 时等于最优权（同对象另有 5 条 D3 主张） | AUD-101-DA01-根规范与科学.md×12、AUD-101-DA02-算法推导.md×6、AUD-202-SNR核验.md×6、D9-工单对账.md×6、AUD-101-DB-03.md×5、AUD-101-DB-19.md×4、AUD-101-DB-20.md×3、AUD-101-DB-13.md×1、AUD-101-DB-14.md×1、AUD-101-DB-16.md×1、AUD-101-DB-17.md×1、AUD-301-文献复算-旧判批.md×1 | 结果层与收口层·R3[PASS] | 入库 |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` | — | 帧级 SNR／稀疏控制点／深度 `m_5` 全部走对角（白噪声）形式 | Phase1 产品是 drizzle 重采样后的 HEALPix 叶（相关长度 1–2 px），属 `07_noise_snr.md:130` 的"相关噪声"分支，却未用完整信息核；完整核**已实现、零接入**（同对象另有 5 条 D3 主张） | AUD-202-SNR核验.md×11、AUD-101-DB-03.md×6、AUD-101-DB-19.md×5、AUD-101-DB-16.md×3、AUD-101-DB-20.md×3、AUD-101-DB-13.md×2、AUD-101-DB-14.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-402-判读-A3.md×1 | AUD202-补·V5[PASS] AUD202·V3[VOID] 结果层与收口层·R3[PASS] | 入库 |
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 4 条 D3 主张） | AUD-202-SNR核验.md×2、AUD-301-文献复算-旧判批.md×2、AUD-401-架构对齐.md×1、AUD-402-判读-BD1.md×1 | AUD202-补·V5[PASS] | 入库 |
| `lib/algorithms/noise_snr/cpp/include/snr_estimator.h` | — | Phase2 逐像素科学权重 = Phase1 落盘的 `ivar`（空背景方差面倒数） | 叠加权应为 `w = 1/σ_w²`，`σ_w² = σ_bg² + S_src/g`（`NOISE_MODEL.md` §5c 的加权方差面）。现状把**带适用域的近似**当**定义**：`1/σ_bg²` 只在 `S_src ≡ 0` 或 `S_src/g ≪ σ_bg²` 时等于最优权 | AUD-101-DB-09.md×1 | — | 入库 |
| `实验/absolute-snr/code/b3_domain_map.py` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_ | AUD-202-SNR核验.md×1 | AUD202·V4[PASS] | 入库 |
| `实验/absolute-snr/results/b3_domain_map.json` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_ | — | AUD202·V4[PASS] | 入库 |
| `docs/science/CONTROL_WEIGHT_SNR.md` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_（同对象另有 2 条 D3 主张） | AUD-101-DA01-根规范与科学.md×10、AUD-101-DB-15.md×7、AUD-202-SNR核验.md×7、AUD-101-DB-13.md×4、AUD-101-DB-16.md×4、AUD-101-DB-04.md×1、AUD-101-DB-12.md×1、AUD-101-DB-14.md×1、AUD-101-DB-19.md×1、AUD-101-DB-20.md×1 | AUD202·V2[PASS] DB13·W4[PASS/P1] 结果层与收口层·R3[PASS] | 入库 |
| `ASTROCS_DESIGN.md` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_（同对象另有 11 条 D3 主张） | — | — | 入库 |
| `docs/science/DRIZZLE.md` | — | （见 §2 依据） | （同对象另有 2 条 D3 主张） | AUD-101-DB01.md×11、D9-工单对账.md×9、AUD-101-DA02-算法推导.md×7、AUD-101-DA01-根规范与科学.md×6、AUD-301-文献池P1.md×6、AUD-101-DB-10-补.md×5、AUD-204-面积交叠核验.md×4、AUD-301-文献复算-旧判批.md×4、D9-工单对账-补.md×3、AUD-101-DB-03.md×2、AUD-101-DB02.md×1、AUD-402-判读-BD1.md×1 | AUD204·W2[PASS] | 入库 |

## 2 依据

《SNR链路核验报告》§3 R-2 与其 DEV 表；《链路间口径对表》§9（同名不同物：帧级 `frame_snr` 分子是 `F_ref`，稀疏控制点侧被登记为同一对象的写入点分子是逐源 `F_i`；整改是「新增生产 + 改消费算子」）与措辞 W1/W3；「复核-AUD202」V1（两臂须拆开定级）。

## 3 改法（具体动作，动词开头）

1. 补一组以 `F_ref`（或 §2b 的 `S_src`）为分子的 SNR 场三臂对拍：`dense`（须先补生产者）/`sparse_reconstruct`/`frame_reconstruct` 同一输入逐像素比，报 `max|SNR_a/SNR_b − 1|`，含「真值无源 ⇒ 归零」负例
2. 若三臂不能同物理量且互差受控 ⇒ 撤销「三口径并存」的文档主张
3. 把 ADU ↔ 合成通量刻度的唯一换算式只写一次，`F_ref` 与 `k_photo` 各引一端（措辞 W1）
4. 订正 `07_noise_snr.md:201` 的「只存」列举不全（层级向下，措辞 W3）
5. 撤销等值门 `snr_phot == median(SNR_F)` 把帧级量钉在逐源中位数上的形态
6. 明确 `control_ivar`（稀疏控制点）与 `ivar`（稠密背景面）不得互换指称；「归一在消费侧」须点名在哪一层归一（措辞 W2）

## 4 文件域（本任务允许触碰的路径集合）

```text
docs/science/NOISE_MODEL.md
docs/plugins/algorithms_phase1/07_noise_snr.md
lib/algorithms/noise_snr/cpp/src/snr_science.cpp
lib/algorithms/noise_snr/cpp/include/snr_estimator.h
实验/absolute-snr/code/b3_domain_map.py
实验/absolute-snr/results/b3_domain_map.json
docs/science/CONTROL_WEIGHT_SNR.md
ASTROCS_DESIGN.md
docs/science/DRIZZLE.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T01（对拍读数须入库）；与 ACSD-T33 同批（稀疏层对象是本任务的第三臂）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `NOISE_MODEL.md` 由 T20/T32/T40 共改 ⇒ 同批；`CONTROL_WEIGHT_SNR.md` 由 T28/T32/T33/T40 共改

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 真值无源夹具 ⇒ 三臂 SNR 必须归零；若不归零 ⇒ 判红（判据非退化要求）
- 以 σ̂ 估计器充当 SNR 场 ⇒ 口径门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得用「已声明降级」替三臂缺生产者事实；不得引书目层证据支撑超出其限定语的结论

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `docs/science/NOISE_MODEL.md` | AUD-101-DA01-根规范与科学.md:101;AUD-101-DA01-根规范与科学.md:112;AUD-101-DA01-根规范与科学.md:383;AUD-101-DA01-根规范与科学.md:387;AUD-101-DA01-根规范与科学.md:393;AUD-101-DA01-根规范与科学.md:61;AUD-101-DA01-根规范与科学.md:627;AUD-101-DA01-根规范与科学.md:652;AUD-101-DA01-根规范与科学.md:697;AUD-101-DA01-根规范与科学.md:701;AUD-101-DA01-根规范与科学.md:719;AUD-101-DA01-根规范与科学.md:768… | 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` | AUD-101-DA01-根规范与科学.md:375;AUD-101-DB-03.md:310;AUD-101-DB-03.md:313;AUD-101-DB-03.md:318;AUD-101-DB-03.md:476;AUD-101-DB-03.md:480;AUD-101-DB-03.md:499;AUD-101-DB-10-补.md:123;AUD-101-DB-10.md:61;AUD-101-DB-13.md:285;AUD-101-DB-13.md:48;AUD-101-DB-14.md:130… | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp` | AUD-202-SNR核验.md:196;AUD-202-SNR核验.md:311;AUD-301-文献复算-旧判批.md:227;AUD-301-文献复算-旧判批.md:357;AUD-401-架构对齐.md:184;AUD-402-判读-BD1.md:43 | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 |
| `lib/algorithms/noise_snr/cpp/include/snr_estimator.h` | AUD-101-DB-09.md:64 | — |
| `实验/absolute-snr/code/b3_domain_map.py` | AUD-202-SNR核验.md:263 | AUD202·V4：确认（并补两条成稿没给的硬证据：结果件里 `snr` 出现 0 次；dense 两条生产者都不可达） |
| `实验/absolute-snr/results/b3_domain_map.json` | — | AUD202·V4：确认（并补两条成稿没给的硬证据：结果件里 `snr` 出现 0 次；dense 两条生产者都不可达） |
| `docs/science/CONTROL_WEIGHT_SNR.md` | AUD-101-DA01-根规范与科学.md:102;AUD-101-DA01-根规范与科学.md:104;AUD-101-DA01-根规范与科学.md:345;AUD-101-DA01-根规范与科学.md:349;AUD-101-DA01-根规范与科学.md:377;AUD-101-DA01-根规范与科学.md:380;AUD-101-DA01-根规范与科学.md:59;AUD-101-DA01-根规范与科学.md:649;AUD-101-DA01-根规范与科学.md:694;AUD-101-DA01-根规范与科学.md:727;AUD-101-DB-04.md:39;AUD-101-DB-12.md:202… | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） ‖ DB13·W4：确认（可结案，不必上呈） ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `ASTROCS_DESIGN.md` | — | — |
| `docs/science/DRIZZLE.md` | AUD-101-DA01-根规范与科学.md:321;AUD-101-DA01-根规范与科学.md:459;AUD-101-DA01-根规范与科学.md:463;AUD-101-DA01-根规范与科学.md:65;AUD-101-DA01-根规范与科学.md:680;AUD-101-DA01-根规范与科学.md:731;AUD-101-DA02-算法推导.md:1534;AUD-101-DA02-算法推导.md:1542;AUD-101-DA02-算法推导.md:1595;AUD-101-DA02-算法推导.md:1622;AUD-101-DA02-算法推导.md:1802;AUD-101-DA02-算法推导.md:484… | AUD204·W2：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反） |

