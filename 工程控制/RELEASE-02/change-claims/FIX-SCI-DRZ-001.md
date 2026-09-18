# 变更 claim：FIX-SCI-DRZ-001 — DRIZZLE §5/§7 归一化互斥订正为面亮度保持

- 控制包：RELEASE-02 / 任务 FIX-SCI（负责人已批准两项 P0 冻结科学文档订正）
- 变更对象：`docs/science/DRIZZLE.md`（SCI-DRZ-001，FROZEN）
- 关联条目：`FZ-FORMULA-DRIZZLE-SB` / `FZ-FORMULA-DRIZZLE-VAR` / `FZ-COND-FLUX-CONSERV` / `FZ-GATE-CONST-SB`（`docs/contracts/v6/data/02_signal.md:38-46`；`docs/science/v6/frozen/01_SEMANTIC_FREEZE.md:18-21`；裁决记录 SUP-02/SUP-03 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:2554-2580`）
- 日期：2026-09-18
- 依据条款：ENGINEERING_SPEC §3（科学正确性优先 + 变更 claim + 一致性回归）；AGENTS §8

## 1 问题描述

`DRIZZLE.md` §5 把最终信号写成 `w_jp=a_jp/A_drop,j`、`F_p=Σ_j x_j w_jp`、`D_p=Σ_j a_jp`、`S_p=F_p/D_p`；§7 又断言"常数面亮度场 B0 ⇒ `S_p=B0`"。两处不能同真：

- 令 `A_drop,j = pixfrac²·A_pixel,j`，常数面亮度场 `x_j=B0·A_pixel,j` 代入 §5 得
  `S_p = Σ_j B0·A_pixel,j·(a_jp/(pixfrac² A_pixel,j)) / Σ_j a_jp = B0/pixfrac²`。
- 即 §5 只在 `pixfrac=1` 与 §7 一致；`pixfrac=0.8` 偏 `1.5625×`，`pixfrac=0.5` 偏 `4×`。
- RELEASE-01 `GAP_AUDIT.md:108`（SCI-001-S2）登记为 **P0 级科学文档冲突**（证据 `drizzle_engine.cpp:1329/1531/1554` + 独立代数；M2a-A-1）。

## 2 证据

### 2.1 一手文献（DOI + 式号，逐字核验）

- **Fruchter, A. S. & Hook, R. N. 2002, PASP 114, 144**（"Drizzle: A Method for the Linear Reconstruction of Undersampled Images"，DOI `10.1086/338393`；arXiv:astro-ph/9808087v2 §2 式(2)–(5)，2026-09-18 逐字核验）。
  - 式(2) `W'=a·w+W`；式(3) `I'=[d·a·w·s²+I·W]/W'`，其中 `s²` "introduced to conserve surface intensity"（`s=A_out/A_in` 的线性尺度比）；
  - 式(4)(5) 用爱因斯坦求和：`I_p = [Σ_i d_i a_ip w_i s²] / [Σ_i a_ip w_i]`。
  - **同一个权重 `a_ip·w_i` 同入分子与分母**——是一致加权均值；`a_ip` 是 drop 与目标像素的分数交叠（`a_ip=overlap/A_drop`），`A_drop` 在均匀 drop 尺度下相消，不是独立除数。
  - 常数面亮度 `d_i=B0·A_in` 代入：`I_p/A_out = B0`，**与 pixfrac 无关**。
- **DrizzlePac Handbook（STScI）** §2.3.2（p.17）："the weights of the individual output pixels … are independent of the choice of p [pixfrac]"（pixfrac 不改变输出光度/权重）；§2.3.2 定义 `a_xy` 为 "fractional area overlap of the drop"。

### 2.2 开源实现对照（只读，未复制）

- **drizzlepac（STScI，BSD-3-Clause）** `src/cdrizzlebox.c`：`update_data()` L333 `output=(output·vc+dow·d)/(vc+dow)`；`do_kernel_square()` L1008 `dover=boxer(...)`、**L1012 `dover /= jaco;`**（`jaco=A_drop`，L978-981）、L1018 `dow=dover·w`——drop 面积同入分子分母，是**一致加权均值**，非本仓 legacy 混合式。等价旧 tag 1.2 `drizzle/src/cdrizzlebox.c` L952。
- **SWarp（GPL-3.0）** `src/resample.c` L845-847 以局部 `A_out/A_in`（Jacobian）面积比重采样、L875 `*(out++) *= area;`，manual §5.6/式9 "designed to conserve surface brightness per pixel"；SWarp **无 drop/pixfrac 概念**，从不用 footprint 面积归一。`src/coadd.c` COADD_WEIGHTED L1279-1305 为逆方差加权均值。

### 2.3 本仓独立复算

- 代数：`S_p^{legacy}=B0/pixfrac²`（上 §1）；目标式 `S_p=Σ_j B_j a_jp/Σ_j a_jp`（`B_j=x_j/A_pixel,j`）给 `S_p=B0` 对所有 `pixfrac∈(0,1]`。
- 等价关系：`w_SB,jp=a_jp/A_pixel,j=pixfrac²·w_legacy,jp`；`S_p^{SB}=S_p^{legacy}/pixfrac²`。
- 方差一致性：`S_p=Σ_j B_j c_jp`（`c_jp=a_jp/D_p`，`D_p=Σ a_jp`），`Var(B_j)=v_j/A_pixel,j²` ⇒ `variance_p=Σ_j v_j c_jp²/A_pixel,j²=Σ_j v_j w_SB,jp²/D_p²`。legacy 权重使 `variance_p` 额外乘 `1/pixfrac⁴`（信号乘 `1/pixfrac²`），**SNR 不变但绝对面亮度标度错**。
- 本仓实现仍为 legacy：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1531` `Scalar weight = overlap_area / drop_area;`、`:1553` `acc.sumFlux += Scalar(pixelValue * weight);`、`:1554` `acc.sumArea += Scalar(overlap_area);`；`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:707` `sig = flux / area;`。

### 2.4 权威目标态（本仓已有，且与文献一致）

- `docs/design/PHASE1_DETAILED_DESIGN.md:120-126`（TARGET_NORMATIVE）：`S_p = Σ_j B_j a_jp / Σ_j a_jp`。
- `docs/plugins/algorithms_phase1/08_drizzle.md:28-29`：同一式。
- `docs/contracts/v6/data/02_signal.md:38-46`：`FZ-FORMULA-DRIZZLE-SB` / `FZ-COND-FLUX-CONSERV`（pixfrac<1 总输出通量 `=pixfrac²·Σ x_j`，`provenance.flux_conservation_factor=pixfrac²`）。
- 裁决 SUP-02/SUP-03（`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:2554-2580`）逐字给出目标措辞。

## 3 订正前/后 diff 摘要（`docs/science/DRIZZLE.md`）

| 位置 | before | after |
|---|---|---|
| §2 符号表 | `w_jp=a_jp/A_drop,j`；无 `A_pixel,j` | `w_jp=a_jp/A_pixel,j`（`=pixfrac²·a_jp/A_drop,j`）；新增 `A_pixel,j` 行 |
| §3 单位 | `D,a,A_drop: px²` | `D,a,A_drop,A_pixel: px²` |
| §5 重建式 | `w_jp=a_jp/A_drop,j`；`S_p=F_p/D_p`（legacy） | `B_j=x_j/A_pixel,j`、`w_jp=a_jp/A_pixel,j`、`S_p=F_p/D_p=Σ_j B_j a_jp/Σ_j a_jp`；新增式(5)等价形式与 `w_SB=pixfrac²·w_legacy` 关系 |
| §5 语义固定 | 常量 ADU ⇒ `S_p=C/A_drop` | 常量 ADU ⇒ `S_p=C/A_pixel`（均匀源像素面积） |
| §5 方差 | 无权重一致性注 | 新增：信号/方差必须同用 `w_SB`；legacy 使 variance ×`1/pixfrac⁴` |
| §7 通量守恒 | `Σ_p F_p=Σ_j x_j·(a_jp/A_drop)`（无条件） | 条件不变量 `Σ_p F_p=pixfrac²·Σ_j x_j`；`flux_conservation_factor=pixfrac²` |
| §7 常量场 | `S_p=B0`（未声明 pixfrac 域） | `S_p=B0` **对全部 `pixfrac∈(0,1]`**；常量 ADU ⇒ `C/A_pixel` |
| §10 | 仅"漏 D_p 归一" | 增"legacy 归一声称绝对面亮度"为不可接受 |
| §11 Oracle | "常数场 C 的 `S_p=C`" | `FZ-GATE-CONST-SB`：`x_j=B0·A_pixel,j`、`|S_p/B0−1|<1e-3`、全 pixfrac、三类负向注入必红 |
| §14a | "未逐式引用其公式号" | 补式(5) + drizzlepac/SWarp/Handbook 逐文件:行对照 |

## 4 影响面

- **文档**：`docs/science/DRIZZLE.md`（主）；`docs/algorithms/DRIZZLE_GEOMETRY.md`（登记 DISP-DRZ-009 + 最小修复）；`docs/standards/STANDARDS_REGISTRY.md`（D.drizzle §3 由 CONFORMANT 改 PARTIAL + DISP-DRZ-009）；`docs/plugins/algorithms_phase1/08_drizzle.md`（补 legacy 禁用注 + 锚点）；`docs/design/PHASE1_DETAILED_DESIGN.md`（已一致，无需改）。
- **实现（未改，登记 DISP-DRZ-009）**：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1531,1553-1554`。`pixfrac=1`（默认 `config/defaults.json` `drizzle.pixfrac=1.0`）数值不变；`pixfrac<1` 的 signal 需乘 `pixfrac²`。
- **合同/产品**：`contracts/schemas/hips_product.schema.json` 的 signal 语义不变（面亮度）；`provenance.flux_conservation_factor=pixfrac²` 已是 v6 契约要求（`docs/contracts/v6/data/02_signal.md:27,43-45`），无需破坏性变更。**未触碰三命令划分 / JSON 结构 / HiPS 数据模型。**

## 5 一致性回归

- `python3 tools/science_contract_lint.py docs/science/DRIZZLE.md` → `SCIENCE_CONTRACT_LINT_PASS kind=sci files=1 sections=15`（rc=0）。
- `python3 ci/run_checks.py --check CHK-SCI-REF --quiet` / `CHK-DANGLING` → 见 `reports/RELEASE-02/FIX-SCI-report.md` §5（原始输出）。
- **待前台构建后复跑（本次不构建）**：`p1drz` 常量面亮度门（`FZ-GATE-CONST-SB`，覆盖 `pixfrac∈(0,1]`）、`variance_propagation_test`（`TEST-DRZ-VAR-001`）、`candidate_oracle_test` 9003 例零漏选、`ctest -R 'p1drz|drizzle'`。DISP-DRZ-009 修复落地前，pixfrac<1 的常量面亮度门应为红（当前实现 legacy）。
- 负例：按每像素常量 ADU 构造、`S_p=F_p` 漏归一、legacy `w=a/A_drop` 三法必须判红（`FZ-GATE-CONST-SB` 负向 mutation）。

## 6 状态

- 文档订正：**已落地**（本 claim）。
- 实现订正：**未落地**（登记 `DISP-DRZ-009`，最小修复 = `drizzle_engine.cpp:1531` 权重乘 `pixfrac²`；归 P1-DRZ-IMPL，需构建/回归）。
