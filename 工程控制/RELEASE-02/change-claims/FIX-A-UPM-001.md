# 变更 claim：FIX-A-UPM-001 — PHASE2_UPM 冻结纯加性模型订正为目标乘性+稀疏天光面模型

- 控制包：RELEASE-02 / 任务 FIX-A 天光面链（P0-08/09/10）
- 变更对象：`docs/science/PHASE2_UPM.md`（SCI 冻结；现 §1/§5 为纯加性 `calibrated=raw−C_f(p)`）
- 关联条目：`OPEN-P2S-02`（`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:2425`，"UPM 乘法尺度 g_k 的生产数据面/schema 与空间模型表示"，OPEN）；`docs/science/PHASE2_UPM.md:182`（UNRESOLVED 设计-实现模型冲突登记）
- 日期：2026-09-18
- 依据条款：ENGINEERING_SPEC §3（科学正确性优先 + 变更 claim + 一致性回归）；AGENTS §8；RELEASE-02 控制包"稀疏天光面等新文档语义属本预览版必须闭合范围"
- 状态：**已否决 / SUPERSEDED by `FIX-SCI-SNR-CANON-001`（2026-09-19）**——负责人 A2 裁决 UPM = **纯加性**，与本 claim 提议方向相反；模型订正提议（§3 的「恢复 `g_k`」「空间天光面取代加性场」）**不采纳**，本文件保留为证据面与加性天光面**表示层**参考（见 `FIX-SCI-SNR-CANON-001.md` §2）

## 1 问题描述

`docs/science/PHASE2_UPM.md:182` 已自登记：

> 目标 UPM 为 **y_k(x)=g_k·s(x)+b_k(x)**（乘性响应 + 稀疏样条天光面），而本文件 §1/§5 的现行冻结模型为**纯加性** calibrated=raw−C_f(p)（8×8 control cell），并明文"乘性尺度差已撤销"。二者不可同时为真。

RELEASE-02 控制包把 P0-09（UPM 天光面 `b_k(x)` 缺失）、P0-10（`g_k` 乘法响应未接主链）列为必修，并规定本预览版必须闭合该文档语义。故需把 SCI 冻结模型订正为目标模型：

- 观测模型：`y_k(x) = g_k·s(x) + b_k(x) + ε`；
- 空间天光面：`b_k(x) = B_ref(x) + δ_k(x)`，`B_ref` 为统一参考天光面（稀疏样条），`δ_k` 为逐帧低阶多项式；
- 生产校准：`s(x) = (y_k(x) − C_k(x) − b_k(x)) / g_k`。

纯加性模型无法表达：① 帧间乘法光度响应差异（会残留 ~g 量级测光误差）；② 跨帧统一天光面（VIS-001 面板接缝/背景台阶的根因）。

## 2 证据

### 2.1 一手文献

- **Wahba, G. 1990, Spline Models for Observational Data, SIAM**（ISBN 0-89871-244-0）：张量积样条 + 粗糙度罚（P-spline）作为散乱数据曲面估计的标准方法；本实现 B_ref 用均匀 B 样条 + 二阶差分罚。
- **Duchon, J. 1977, Constructive Theory of Functions of Several Variables, 85**：薄板样条（本文件 §14a 已列）。
- **Huber, P. J. 1964, Ann. Math. Statist. 35, 73（DOI 10.1214/aoms/1177703732）** 与 **Holland & Welsch 1977（DOI 10.1080/03610927708827533）**：Huber IRLS（δ=1.345），本实现沿用。
- **Horne, K. 1986, PASP 98, 609**：最优权重 `w=1/σ²`（WLS）；本实现 `weight_mode=inverse_variance` 的统计依据（开源实现无一加权，见 2.2）。

### 2.2 成熟开源实现对照（只读，未复制）

- **SWarp 2.41.5**（GPL-3.0）`src/back.c:844-848,891-896,944`：稀疏网格上**自然三次样条**天光面（非稠密栅格）；`src/preflist.h:251` `BACK_SIZE=128`、`:253` `BACK_FILTERSIZE=3`；`src/back.c:448,452` 权重图仅作掩膜、均值不加权；`src/coadd.c:1295-1307` coadd 逆方差加权。→ 佐证"稀疏样条天光面"表示可行（本实现取其形，加权口径取 WLS/Horne）。
- **SEP v1.4.1**（BSD-3-Clause）`sep.h:91-107`：背景节点**稀疏存储**（`sep_backmap` 仅 mesh 节点）；`sep/src/background.c:32-35,334-345,540-659,682-797`：`Background2D` 中值滤波 + mesh 插值。→ 稀疏存储独立佐证。
- **Siril**（GPL-3.0）`background_extraction.c:62-65`：多项式阶 1–4 → 3/6/10/15 系数；`:371` 无加权拟合；`SAMPLE_SIZE=25 px`。→ 逐帧 δ_k 取低阶多项式的先例。
- **Montage**（BSD-3-Clause）`montageFitplane.c:486`：逐帧平面 `A·x+B·y+C`，`:488-491` 2σ 残差裁剪。→ **最接近的逐帧 δ_k 先例**（本实现默认 1 阶平面）。
- **SCAMP**（GPL-3.0）`photsolve.c:4`：只做相对光度解，**无任何 `*back*` 源**。→ **不得**引 SCAMP 支撑 `b_k`；乘法尺度 g_k 可参考其相对零点思路，天光背景须另找依据（见 2.2 SWarp/SEP/Montage）。
- **photutils 3.0.0**（BSD-3-Clause）`background_2d.py:245-246`、`core.py:34`：`SIGMA_CLIP sigma=3.0 maxiters=10`，与 sampler 现有 3σ clip 同口径。

### 2.3 模型可辨识性（独立数值推导）

见 `run/RELEASE-02/research/FIX-A-model-identifiability.md` 及 `q1_nullspace.py`…`q5_roughness.py`：

- 加性 gauge：`B_ref→B_ref+c`、`δ_k→δ_k−c` 不可辨，gauge 维 `D_add=(order+1)(order+2)/2`；若 `deg(B_ref)≥order`，**参考帧 δ_ref=0** 为最小约束（本实现 `gauge_mode=0`，另支持 sum gauge）。
- 乘法 gauge：`s→a·s, g_k→g_k/a`，锚定 `g_ref=1`。
- δ_k 阶数：默认 1（`q2_delta_order.py`）；基函数取张量积三次 B 样条（`q4_basis.py`，条件数 ~30 且与节点数无关）；P-spline 粗糙度罚必需（`q5_roughness.py`）。
- 关键可辨识性限制：**g_k 只能由真正含乘法差异的数据确定**。当帧间差异为纯加性（无乘法增益）时，自由 latent 会把加性结构吸收为伪乘法——本实现以帧间公共 control 的**直流比**做独立交叉校验，不一致即 fail-closed 取 g=1（见 `reports/RELEASE-02/FIX-A-report.md` §3.4）。

### 2.4 本仓权威目标态（已存在，与文献一致）

- `docs/plugins/algorithms_phase2/10_sampling.md` §4.2、`11_upm.md` §4.1：目标 `y_k(x)=g_k·s(x)+b_k(x)`；
- `docs/design/UNIFIED_MODEL.md` §2：统一模型（乘性 + 天光面）；
- `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:2425` `OPEN-P2S-02`（OPEN）。

## 3 订正前/后（`docs/science/PHASE2_UPM.md`）

| 位置 | before（纯加性，冻结） | after（目标乘性+天光面） |
|---|---|---|
| §1/§5 模型 | `calibrated = raw − C_f(p)`（8×8 control cell 加性场） | `s(x) = (y_k(x) − C_k(x) − b_k(x)) / g_k`，`b_k=B_ref+δ_k` |
| §5 乘性尺度 | "乘性尺度差已撤销" | 恢复 `g_k`（相对参考帧，`g_ref=1`） |
| §5 天光面 | 无空间天光面 | `B_ref(x)` 稀疏张量积 B 样条 + 逐帧 `δ_k(x)` 低阶多项式 |
| §14a:182 | UNRESOLVED 冲突登记 | 冲突已裁决；登记指向目标模型与 OPEN-P2S-02 闭合 |

## 4 影响面

- **文档**：`docs/science/PHASE2_UPM.md` §1/§5/§10（SCI 冻结）需由负责人批准订正；§14a:182 的 UNRESOLVED 登记随之更新。**本 shard 未改该文档**。
- **插件/设计**：`docs/plugins/algorithms_phase2/10_sampling.md`、`11_upm.md`、`docs/design/UNIFIED_MODEL.md` 已是目标态，无需改。
- **合同**：`OPEN-P2S-02`（g_k 生产数据面/schema 与空间模型表示）需从 OPEN 转为闭合——空间模型表示已由本 shard 实现（`sky_plane.h/.cpp`）；schema/数据面冻结登记仍待合同流程。
- **实现**：`lib/algorithms/coverage/src/sky_plane.cpp`（B_ref+δ_k+Schur）、`sampler.cpp`（天光采样点/星点掩膜）、`stage2.cpp`（生产接线 `(raw−C−b_k)/g_k` + g_k 直流比门）已按目标模型实现并测试。
- **一致性回归**：`v6_p2_sky`（13 用例，含 recovery/seam/gain/dcgain/overflow 负例）+ 全量 `ctest` 全绿；`DOC_LINE_ANCHORS_PASS` + `CONSTRAINTS_PASS`。

## 5 请求裁决

请负责人批准按 §3 订正 `docs/science/PHASE2_UPM.md` 的冻结模型描述（仅模型语义，不改 §5 的方差/权重冻结条款），并据此把 `OPEN-P2S-02` 的数据面/schema 表示纳入合同闭合。
