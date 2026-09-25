# 任务：ACSD-T33 合同要求的逐像素预测方差在 `SparseReconstruction` 里无字段，manifest 以它物顶替其名

> 波次 `W1` ｜ 杠杆分档 `P1` ｜ 整改域 科学+合同 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `eng/contracts/schemas/unified/sparse_snr_layer.schema.json` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象** | AUD-101-DB-16.md×1、AUD-202-SNR核验.md×1 | AUD202·V2[PASS] | 入库 |
| `docs/science/CONTROL_WEIGHT_SNR.md` | — | 三种重建口径由 JSON 显式选定、均产出同一物理量，实际生效口径记 `snr_path_effective`；§8b 图谱为其选型依据 | ①`snr_path` 是**死键**（`git grep "snr_path" -- lib` 的 6 处命中全为同名 FITS 形参、CLI 白名单串与帮助键表 ⇒ 配置读取面 0）；②`snr_path_effective` 在 `lib` **0 命中** ⇒ "不静默降级"无载体；③`dense` 不是"没有生产者"，而是**两个生产者都不可达且产物不被消费**（`hp_drizzle_（同对象另有 2 条 D3 主张） | AUD-101-DA01-根规范与科学.md×10、AUD-101-DB-15.md×7、AUD-202-SNR核验.md×7、AUD-101-DB-13.md×4、AUD-101-DB-16.md×4、AUD-101-DB-04.md×1、AUD-101-DB-12.md×1、AUD-101-DB-14.md×1、AUD-101-DB-19.md×1、AUD-101-DB-20.md×1 | AUD202·V2[PASS] DB13·W4[PASS/P1] 结果层与收口层·R3[PASS] | 入库 |
| `eng/ci/ledgers/dead_config_keys.json` | — | 稀疏控制点存 `F_ref/σ_F(x,y)`（合同 const 语义），由 `sparse_reconstruct` 重建稠密 SNR 场 | 准确形态是：**合同对象 `sparse_snr_layer` 在 `lib/**` 零生产者**（`git grep -c "sparse_snr" -- lib` 共 25 处命中，全为消费侧结构体/重建器/自检与合同门夹具/CLI 键表，无一处写产品；`eng/ci/ledgers/dead_config_keys.json:110` 自证）；生产里写逐源 SNR 的是**另一个在册对象**（同对象另有 1 条 D3 主张） | AUD-202-SNR核验.md×1 | AUD202·V2[PASS] AUD202·V4[PASS] 合同层·W5[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `实验/absolute-snr/docs/snr-propagation-design.md` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-301-文献池P1.md×6、AUD-101-DB-16.md×5、AUD-301-文献复算-旧判批.md×4、AUD-101-DB-15.md×2 | AUD202·V2[PASS] | 入库 |
| `docs/plugins/algorithms_phase2/13_integration.md` | — | manifest 的 `node_reproduction_max_abs` 登记为「重建误差」；`weight_chain.h:167` 自注"应 ~0" | 二者对同一字段给出互斥定性（一名为误差量、一称为应恒 0）；该字段在可达输入集上恒 0 ⇒ **无证据资格**；合同要求的逐像素**预测方差**从未实现（字段面缺位、`git log -S` 两条命中均在实验侧）。另：`EXP-04-RECONSTRUCTION.md:648`（判据 S4）本仓已自登记"容差 0.5 dex 对精确插值类过松，不具举证资格"，但 manifest 名面未跟着改 | AUD-101-DB-04.md×2、AUD-101-DA01-根规范与科学.md×1、AUD-202-SNR核验.md×1 | AUD202-补·V5[PASS] 负责人面与索引·V4[PASS] | 入库 |
| `lib/algorithms/integration/v6/src/weight_chain.cpp` | — | 曾与 DEV-01 捆成一条头条，称"两条臂各中一次：权重在信号维退化为常数" | 臂 B 只在 `ivar` 缺失时进入（`ivar_missing>0`），且**显式声明** `uncertainty_available=false` ＋ `weight_source="frame_snr"` ＋ `weight_basis="frame_snr_ivar"` ⇒ 属**已声明的降级**，与逐帧标量权重在业界（SWarp `sigfac`、PixInsight 帧权）同型，*（同对象另有 1 条 D3 主张） | AUD-202-SNR核验.md×2 | AUD202-补·V5[PASS] | 入库 |
| `docs/contracts/DATA_SEMANTICS.md` | — | 同一份文档体系对**同一符号**给两种分母：代码与 FROZEN 正本 `docs/science/DRIZZLE.md:44-50` 为 `w_jp = a_jp/A_drop,j`（其 §10 禁止项逐字写着"把核权重写回 `a_jp/A_pixel,j`（FZ-COND-FLUX-CONSERV 判红）"）；而 `DATA_SEMA | 唯一实现口径是 `a_jp/A_drop`；`w'_jp = a_jp/A_pixel` 只作为**等价参数化**成立，且必须同时换分母（`N'_p = Σ w'·A_pixel = D_p`）。等价性已独立复核：发布 `S_p` 与 `variance_p` 对两种参数化**不变**（分子分母各乘 `pf⁴` 相消）；但 `Σ_p w_jp = 1`（⇒ `Σ_p F_p = Σ_j x_j`、（同对象另有 1 条 D3 主张） | AUD-101-DA02-算法推导.md×9、AUD-101-DB01.md×9、AUD-101-DB-06-07.md×5、D9-工单对账.md×5、AUD-101-DB-11.md×4、AUD-101-DB-04.md×3、AUD-101-DB-10-补.md×3、AUD-101-DA01-根规范与科学.md×2、AUD-101-DB-03.md×2、AUD-204-面积交叠核验.md×2、AUD-101-DB-13.md×1、AUD-402-判读-BD1.md×1、AUD-403-注释与README.md×1 | DB13·W1[PASS] DB13·W4[PASS/P1] 合同层·W1[PASS] | 入库 |

## 2 依据

「复核-AUD202」V2（判定：确认，定性从「写错对象」收窄为「生产根本没有该对象 + 消费侧按已作废的相对语义实现」）；「复核-AUD202-补」V5（判定：确认②③ — 合同要求的逐像素预测方差在 `SparseReconstruction` 里无字段，而 manifest 用 `node_reproduction_max_abs` 顶替「重建误差」之名）。

## 3 改法（具体动作，动词开头）

1. 在 `SparseReconstruction` 侧新增合同所要求的逐像素预测方差字段；或把合同条款改为指向实际存在的对象（二选一并登记）
2. manifest 停止用 `node_reproduction_max_abs` 冒名「重建误差」；`sparse_snr_spacing_px`/`snr_path` 死键按 ACSD-T05 定案
3. 消费侧的已作废相对语义实现改为按正本绝对语义（与 `CONTROL_WEIGHT_SNR.md` 对齐）
4. 在文档里补 `predicted_variance_adu2` 的对象辨析：实验侧同名字段不是合同对象（第②层给的三类命中辨析）

## 4 文件域（本任务允许触碰的路径集合）

```text
eng/contracts/schemas/unified/sparse_snr_layer.schema.json
docs/science/CONTROL_WEIGHT_SNR.md
eng/ci/ledgers/dead_config_keys.json
实验/absolute-snr/docs/snr-propagation-design.md
docs/plugins/algorithms_phase2/13_integration.md
lib/algorithms/integration/v6/src/weight_chain.cpp
docs/contracts/DATA_SEMANTICS.md
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 同批：ACSD-T05（死键处置）、ACSD-T32（三臂）
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `CONTROL_WEIGHT_SNR.md`/`DATA_SEMANTICS.md` 与 T28/T32/T36 共改 ⇒ 同批

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 合同声明字段而产品无该字段 ⇒ 合同一致性门判红（现状：结果件里 `snr` 出现 0 次仍判绿）
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得以顶替名继续出具「重建误差」读数

## 8 登记与边界

根因与 T20 同源（预测方差未实现、`sparse_reconstruct` 生产链缺失）：第②层建议按同一工单合并处置、不重复计数，两任务须同批交付。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `eng/contracts/schemas/unified/sparse_snr_layer.schema.json` | AUD-101-DB-16.md:289;AUD-202-SNR核验.md:128 | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） |
| `docs/science/CONTROL_WEIGHT_SNR.md` | AUD-101-DA01-根规范与科学.md:102;AUD-101-DA01-根规范与科学.md:104;AUD-101-DA01-根规范与科学.md:345;AUD-101-DA01-根规范与科学.md:349;AUD-101-DA01-根规范与科学.md:377;AUD-101-DA01-根规范与科学.md:380;AUD-101-DA01-根规范与科学.md:59;AUD-101-DA01-根规范与科学.md:649;AUD-101-DA01-根规范与科学.md:694;AUD-101-DA01-根规范与科学.md:727;AUD-101-DB-04.md:39;AUD-101-DB-12.md:202… | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） ‖ DB13·W4：确认（可结案，不必上呈） ‖ 结果层与收口层·R3：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯） |
| `eng/ci/ledgers/dead_config_keys.json` | AUD-202-SNR核验.md:257 | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） ‖ AUD202·V4：确认（并补两条成稿没给的硬证据：结果件里 `snr` 出现 0 次；dense 两条生产者都不可达） ‖ 合同层·W5：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足） ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `实验/absolute-snr/docs/snr-propagation-design.md` | AUD-101-DB-15.md:185;AUD-101-DB-15.md:58;AUD-101-DB-16.md:185;AUD-101-DB-16.md:27;AUD-101-DB-16.md:304;AUD-101-DB-16.md:31;AUD-101-DB-16.md:449;AUD-301-文献复算-旧判批.md:134;AUD-301-文献复算-旧判批.md:175;AUD-301-文献复算-旧判批.md:245;AUD-301-文献复算-旧判批.md:255;AUD-301-文献池P1.md:301… | AUD202·V2：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"） |
| `docs/plugins/algorithms_phase2/13_integration.md` | AUD-101-DA01-根规范与科学.md:456;AUD-101-DB-04.md:80;AUD-101-DB-04.md:84;AUD-202-SNR核验.md:224 | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ 负责人面与索引·V4：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**： |
| `lib/algorithms/integration/v6/src/weight_chain.cpp` | AUD-202-SNR核验.md:103;AUD-202-SNR核验.md:374 | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 |
| `docs/contracts/DATA_SEMANTICS.md` | AUD-101-DA01-根规范与科学.md:359;AUD-101-DA01-根规范与科学.md:607;AUD-101-DA02-算法推导.md:1052;AUD-101-DA02-算法推导.md:1055;AUD-101-DA02-算法推导.md:1323;AUD-101-DA02-算法推导.md:1750;AUD-101-DA02-算法推导.md:2134;AUD-101-DA02-算法推导.md:2204;AUD-101-DA02-算法推导.md:450;AUD-101-DA02-算法推导.md:772;AUD-101-DA02-算法推导.md:803;AUD-101-DB-03.md:38… | DB13·W1：确认 ‖ DB13·W4：确认（可结案，不必上呈） ‖ 合同层·W1：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"） |

