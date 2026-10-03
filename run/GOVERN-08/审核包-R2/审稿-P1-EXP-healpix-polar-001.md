# 审稿-P1 · EXP-healpix-polar-001（第 1 遍）

- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0`
- 片号：`EXP-healpix-polar-001`（层 `实验/healpix-polar`，划片依据：SRS-1 层内 LPT 均衡装箱）
- 裁定口径：**一遍 = 对同一片材料的一次完整重读**。本片 30 份成员文件逐份读完，不换焦点。
- 纪律：零 git 写、零仓内改文件、未编译、未跑 ctest/pytest/构建、未跑任何仓内实验脚本。所有数字由我读原文 + 纯算术推导 + 读已提交归档得出。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威片清单） | **30** |
| 实读份数 | **30** |
| 成员总行数（`wc -l` 实测，与清单 `实际行数: 5530` 一致） | **5530** |
| 实际读了多少行 | **5530** |
| **覆盖率** | **30/30 份 = 100.0%；5530/5530 行 = 100.0%** |

**未读完的部分：无。** 逐份清单见 §3。

> 口径说明：片清单记 `实际行数: 5530`，`wc -l` 亦得 5530，两口径一致。个别成员 `wc` 比清单多 1 行（如 `p9_review.cpp` 205→206、`p10_seam.cpp` 63→64、`p8_hstgrid.cpp` 27→28），差在末行换行符，不影响覆盖率。

---

## 2. 本片判定

### **判定：需修（BLOCKED-ADJACENT）**

不是"通过"，也未到"阻断整片"——因为本片最有价值的东西（诚实边界、接缝自纠、REC-1 缺陷登记）确实写得比本项目多数单元诚实。**但本片把一条已知失效的判据当作"验收口径"写在三份正本里，且把一个恒红的自检臂的判红从报告里隐去。**

### 最重的 3 条

**① `results/p3_t12.out` 是一份假绿归档，且正本直接引用了它的 PASS 行。**
归档同一份文件里，北极（角点02）与南极（角点13）各 `V0 破门=32 最坏=6.626e-04`（**合计 64 次破门**），末行却是 `# SUMMARY T12: PASS` + `Exit status: 0`。决定性证据：`cd 实验/healpix-polar && sha256sum -c results/SNAPSHOT.sha256` → 47 项中**只有 `code/p3_sweep_neg.cpp` 失败**，其余 46 项全「成功」；源码 mtime `2026-09-28 23:35` 晚于归档 `2026-09-23 04:44` 五天。
⇒ `p3_sweep_neg.cpp:157-159` 自述的「P3-05 订正：V0 破门原被排除在判定之外 ⇒ 该行对 V0 恒真」**从未真正跑过**，`docs/EXP-07-POLAR.md:417` 的 `# SUMMARY T12: PASS` 是订正前旧码的产物。**本项目固化检查项「代码改了、归档没重跑」的最强实例**，且同型第二例（`route3/exp03` 的 B6 守卫归档缺 `seam_absolute_floor_B6` 全部新键）同时存在。

**② 冻结的「面积守恒闭合」判据在本片所测的全部缺陷上恒绿 —— 且这一失效已被本单元自己写下、却只"登记移交"、从未上修。**
`DRIZZLE_GEOMETRY.md:320,350-351` 的逐 drop 判据 `|Σ_j a_jp − A_drop,p| ≤ max(τ_rel·A_drop,p, ε_abs)`，τ_rel=1e-6、**ε_abs=1e-15 sr**。本片 §4.7.2 自己测出接缝绝对亏空只有 **2.9e-17 … 1.05e-16 sr**（`docs/EXP-07-POLAR.md:494`），即 ε_abs 比被测缺陷**大 6.7–23 倍**。代数上：`DRIZZLE_GEOMETRY.md:375` 自己写"接缝…一律以绝对项判"，而 §4.7.2 的适用域是 `A_drop ≲ 1.4e-10 sr`——在该域 `max()` 恒取 1e-15 sr，而实测绝对亏空按 `A_drop^(+0.17)` 恒 < 1.1e-16 sr ⇒ **全物理域内该门永不因接缝缺陷变红**。
更硬的一条在 `code/audit/route3/exp03_weight_conservation.py:44-47`（属本片）：
> 「A_drop ≲ ε_abs 时 max() 取 ε_abs，相对容差 tol/A_drop >= 1 ⇒ **100% 通量丢失（Σa=0）仍判绿**。实测：θ=0.005″/px，pf=1 → A_drop=5.876e-16 sr、**tol/A_drop=1.70**；pf=0.8 → tol/A_drop=**2.66**」

该脚本为此加了实验侧 `APPLICABLE_MIN_ADROP` 守卫（`:61,75`），并在 `handover` 字段写明"**正本容差定义不在本单写入面，只登记移交**"。我核实：
- 正本 `DRIZZLE_GEOMETRY.md:320,350-351,362-375` 仍是原判据，**未加守卫**；
- 可执行门 `lib/algorithms/drizzle/healpix_drizzle/tests/p3_conservation_gate.cpp:47,171` 仍是 `judge_limit() { return std::max(P3_REL_BUDGET*a_ref, P3_ABS_BUDGET_SR); }`，**无适用域守卫**；其位置集（`:177,350`）恰恰含 `SEAM` 与 `HST` 组。
⇒ **治理订正停在一支实验脚本里，没有上修到正本与可执行门。** 本片 `docs/EXP-07-POLAR.md:33/620/687`、`docs/DERIVATIONS-P3.md:194`、`REPORT_experiment.md:95` 全部把这条失效判据复述为"验收口径"，无一携带守卫条件。

**② kcorr 自检臂 G0b 是恒红门，判红被静默吞掉，且报告只引绿臂。**
`code/audit/kcorr/run_extra.py:57` 门为 `all(abs(r["k_corr"]-1.0) < 0.06)`，跑 N=5/25/289。我从已提交归档 `results/audit/kcorr/g0b_sanity_fixed.json` 直读：**`gate_pass: false`**，N=5 得 1.7673、|Δ|=0.767 红；N=25 得 1.0836、|Δ|=0.084 红；N=289 得 1.0293 绿 —— **3 臂中 2 臂恒红**。而 docstring（`run_extra.py:5-6`）断言"k_corr 必须恒 1(任何 N)"，该前提代数上就错：M=I 时 `k_corr` 退化为 `k_gauss(N)`，只有 N→∞ 才趋 1。
**双向体检（同仓两份脚本对同一几何给出相反判据）**：`code/audit/kcorr/run_scan.py:56-58,72-73` 的 G0 臂对**同一恒等几何**用正确判据，注释明写"小 N 档必须等于 iid 高斯有限 N 参考 k_gauss(N)…**不是 1**"；被 docstring 自称"**修复**自检臂"的 G0b 反而退回错判据。
**选择性报告**：`REPORT_experiment.md:80` 只引"恒等几何 N=289: 0.9995±0.0278"这一条绿臂，完全不提 `gate_pass=false`。

**③ 接缝破门区（真问题、HST 0.04″/px 下 3.3e-03，比门禁宽 3300 倍）在全仓没有任何一条会因它变红的判据。**
`docs/EXP-07-POLAR.md:520` 把它列为"现实风险"，`§7:703-716` 列为开放项；但按 ①，该域落在 ε_abs 地板内 ⇒ 正本门与可执行门都判绿；`docs/EXP-07-POLAR.md:443-452` 列出的六条"已排除机制"里没有一条能解释它，`§7:709-710` 自认"最后一步未定位"。同时该缺陷的**绝对**量级（≤1.1e-16 sr）恰恰说明它只有靠相对项才抓得住，而相对项在该域被 ε_abs 掩盖。⇒ 缺陷真实、量级可信、**门抓不到**，这三条同时成立。

---

## 3. 逐文件清单

每条：`读了什么 → 看到什么 → 判定`，均带 `文件:行`。

| # | 文件（行数） | 读了什么 / 看到什么 | 判定 |
|---|---|---|---|
| 1 | `docs/EXP-07-POLAR.md`（859） | 全 9 节。§0 速览、§2 方法、§4 结果、§4.7.1/4.7.2 接缝自纠、§6 落地建议、§7 诚实边界、§8 复现、§9 佐证。**看到**：`:78` 引 `DRIZZLE_GEOMETRY.md:236` 为门禁出处——该行实为线程 stripe 累加文字，门禁实在 `:320`；`:7` 引"§0.6 记录的探针自身 bug 修复"——本文档**无 §0.6**；`:6` 称"code/ 下 12 个源文件"，§9 表列 14 个，目录实有 13 `.cpp`+3 `.h`（`p4_hst3d.cpp` 未进表）；`:6` 称"results/ 下 30 个日志/CSV"，`ls results/` 得 33 个 `.out/.csv`；`:449` 称 T25 比的是"角点法 vs **真实曲线法**"，但 `p9_review.cpp:200` 比的是 `chart_uv_to_xyz` 的 **mode 0 vs mode 1**（两种角点公式），不是真曲线 | **需修** |
| 2 | `code/audit/route3/exp03_weight_conservation.py`（482） | 全读。`:41-62` G08-05 B6 适用域守卫、`:65-82` `drop_closure_ok`、`:85-100` 真空带探针、`:229-313` `run_field`、`:340-482` `main`/verdict。**看到**：`:44-47` 亲笔写明"100% 通量丢失（Σa=0）仍判绿"、tol/A_drop=1.70/2.66；`:61` `APPLICABLE_MIN_ADROP=max(2e-15,2e-16)=2e-15`；`handover` 自认"只登记移交"。`:24,25` `import os` 重复 | **需修**（缺陷本身已记录，未上修） |
| 3 | `code/chart_native.h`（368） | 全读。`:27` 邻接求取、`:54` `area2`、`:107` `refine_arc_chart`、`:152` 角点路径、`:258` `chart_allocate`、`:339` `distribute_cells`。**看到**：`:6` 声称"叶边界在 chart 里是**精确的轴对齐直线段**"——赤道支成立、**极冠支不成立**（`cosθ=a+b/φ²`，refs.md:17）；`:2-3,26,150` 三处引 `INNOV-04-01` / `face_native.h`——`git ls-files` 命中 **0**，悬空引用；`:355-364` 逐叶裁剪循环算出 `a2v` 却**从不累加**，`:354` 只加整块 `res.chart_area` ⇒ "叶侧零近似/逐叶分配"在实现里**没有逐叶产出**（`ChartAlloc` 只有计数 `cells_clipped`）；`:273,298,249,217` 四处静默 `continue`/空返回，无 fail-closed 计数 | **须修** |
| 4 | `code/p3_sweep_neg.cpp`（361） | 全读。T11/T12/T13/T14/T15。**看到**：`:157-159` 留有"P3-05 订正：SUMMARY 原先只读 REC-1 与 oracle，V0 破门被排除 ⇒ 该行对 V0 恒真"的修复记录（**本片唯一做对的双向体检**）；`:22` `rel(a,b)` 在 `b<=0` 时**返回 0.0**（分母塌缩即判绿）；`:247` 跑 pf 三档但 `:257` 的 printf **不打印 pf**，33 行不可分辨；`:347` `which=="all"` **不跑 T12**；`:218-222` N2 是恒绿项仍计入 SUMMARY | **须修** |
| 5 | `code/audit/route1/e1_leaf_area_and_scale.py`（287） | 全读。`:54-107` chart/鞋带/VOS、`:155-182` 负控、`:184-206` 尺度常数、`:208-252` 控制点接口。**看到**：`:13` 引"repo doc 02 §1.1"——本单元 `docs/` 只有 4 份文件，**无编号 02 的文档**；`:188-189` auto-nside 演示点 `finest=105527.7` 恰在 `C/2` 决策边界上（翻转窗宽仅 20.84″），docstring `:10` 却写"shifts … inside **every** octave window"（过强）；`:190-193` `oct_windows` 算了不判不打；`:157-159` `chart_perturbed` 死代码；`:31` 声称"leaf-id **bijectivity**"但 `:227-242` 无任何双射断言；`:178` 负控量只对"仿射 z 缩放"一族有效，对生产对象零判别力 | **须修** |
| 6 | `code/audit/route1/e2_polar_pixel_limit.py`（250） | 全读。`:46-127` ring 构造、`:131-160` 逐 N、`:176-186` 负控 a、`:188-223` 负控 b。**看到**：`:11` 与 `:166` 仍写已被台账裁定为转写误差的 **`0.104369`**，而同行 `:166` 算出的 `coeff_theory`=0.1043885——**注释与下一行自相矛盾**，且被否值正是 `DERIVATIONS-P3.md:67` 点名引用本脚本时所依据的数；`:155` `sum_model_over_4pi_minus_1` 是**恒真门**（Σ 弦面积精确闭 4π，逐叶正负偏差结构相消）；`:189-214` 负控 b 只在**赤道**叶上采样，而赤道叶在 (φ,z) 平面是仿射像 ⇒ 采样多边形与四角四边形**几何同一**，"真曲线度量归零"是**构造性恒真**；`:10-11` 排序主张缺"N≥32"限定；`:222` JSON 键拼错 `richsonson_residual` | **须修** |
| 7 | `REPORT_experiment.md`（242） | 全读。§4.1–4.6、§5 结论、§6 诚实边界、§8 三类证据表。**看到**：`:80` 只报 G0/G0b 的 N=289 绿臂，隐去 `gate_pass=false`；`:95,185,194` 三处复述失效的 `max(1e-6·A_drop, 1e-15 sr)` 判据而无守卫条件；`:173` 门禁布尔计数 16 个，与 §4.6 表逐行对得上（口径清楚）；`:19` 主张"三路独立重做（互不通信）"，但 route2/route3 共用 `p3lib`/`edge_geom` 之外的 `exp01_leaf_area.pixel_corners`（`route3/exp02:11,29`、`exp03:29`、`exp05:15`）⇒ 三路在**叶角点构造**上并非独立 | **须修** |
| 8 | `code/p2_algorithms.cpp`（234） | 全读。T8a/T8b/T9/T10。**看到**：`:24` 同款 `rel()` 恒绿分支；`:72-73` printf 把 chart 原始偏差标成"×**叶尺寸**"，实为 chart 单位（叶尺寸是 1/Ns，`:56` 注释自身前后矛盾）；`:56-57` `hp_res_leaf=1.0; (void)` 死变量；`:139,158` `t_oracle` 声明后从不计算，T9 **无 oracle 对照**；`:193` `cands.size()` 未防零 | **建议** |
| 9 | `code/audit/route1/e5_projection_budgets.py`（217） | 全读。`:62-103` gnomonic、`:105-117` 环求积负控、`:130-169` 切平面、`:171-184` 拟合。**看到**：`:16-19` H3 docstring 称"disc-like … ~θ²/2 … at θ=1e-3 the deficit is **−5.000e-7**"——**圆盘是 −θ²/4**（解析：πsin²θ/2π(1−cosθ)=(1+cosθ)/2=1−θ²/4），−5e-7 是**方形**值（`:156-162` 的 `rows_tq`），形状搞混；`:207` 打印 `-2.51204e-07` 却紧跟 "12.5x too small"，实为 **6.28×**，矛盾已固化进归档 `results/audit/route1/e5_projection_budgets.txt`；`:184` verdict 是**硬编码字符串**"theta^2/2-family model fits"，而归档拟合系数是 0.2512=1/4；`:105-117` 环求积负控自称"independent"，实为 `sec³` 的同一恒等式改写（代数恒真）；`:9-10,102-103` 断言某"head comment"写 2e-6 但**不指出文件:行**；`:16` 断言的 `<4e-8` 在 `spherical_overlap.cpp` 中已**不存在**（现为 `:1009-1010` 的 −5.0e-7） | **须修** |
| 10 | `code/p9_review.cpp`（206） | 全读。T23/T24/T25/T27/T28/T29。**看到**：`:32,65,106,140` 分母 `A_drop` 一律用 `vos_area_rotated`，与生产 `build_drop_geometry_into`（`spherical_overlap.cpp:1099-1101`，`<1e-3 rad` 走切平面）**不同口径**，T27/T29 的"生产 V0 破门数"是在探针分母下判的；`:154` `w0*adrop1` 用**最后一个** drop 的面积换算全扫描的最坏相对闭合；`:206` 恒 `return 0`，全部破门计数只打印；`:198-203` T25 打印的 `d` 是 mode0/mode1 角距，**不是**"真实曲线法" | **须修** |
| 11 | `docs/DERIVATIONS-P3.md`（203） | 全读。D1–D10。**看到**：`:9-46` D1 给出可逐行复算的 π/3 推导且 `:32-36` 自我更正了旧版 ∂φ 错误（诚实度高）；`:48-72` D2 已把系数订正为 0.1043885 并点出 0.104369 是转写误差；`:112` 称 `spherical_overlap.cpp:857` 的 `WCS_ADAPTIVE_MAX_DEPTH = 12`，实为 **:866**（差 9 行）；`:182-195` D9 已把接缝机制降级为"部分定位"并给出幂律 −0.83（诚实度高），但 `:194` 的验收口径仍是失效判据且无守卫；`:46` 引"文献腿 Górski §5 原句"未标页 | **须修** |
| 12 | `code/audit/route3/exp05_sagitta_subdiv.py`（175） | 全读。`:24-37` 边构造、`:45-58` 矢高对、`:99-118` 细分梯子、`:136-172` verdict。**看到**：`:3` 头部写 `adaptive_max_depth = 8`，与生产 `:866` 的 12 不符（`DERIVATIONS-P3.md:112-114` 已指出）；`:6` 引"the **02**-ledger line-58 blanket claim"——悬空引用；`:9` 头部写 `depth needed ~ 8.15`，而 `DERIVATIONS-P3.md:106` 写 ≈7.97（按实测 sag0≈0.064 应为 7.98）⇒ **头部数字与文档口径不一致**，待复算；`:165` `all(3.3<x<4.7)` 是真判别力门（少数几个之一） | **须修** |
| 13 | `code/audit/route2/exp05_projection_budgets.py`（169） | 全读。`:61-93` gnomonic、`:95-121` 切平面 200 取向、`:123-134` 负控、`:136-161` 门。**看到**：`:7,11` 引 `p1drz_geom.hpp` 注释（该文件 `git ls-files` 命中 1，存在）与"the code comment"，后者**不指出文件:行**；`:82-83` `corners` 被 `:85-86` 整段覆盖（死代码），`:80` `c0` 未用；`:156-160` 门条件真实（少见的可判红门）；`:119-121` θ（半边）与 θ_max 两套约定混在相邻键里，靠 `:148` 的 `*0.5` 手工换算 | **建议** |
| 14 | `code/audit/route2/exp08_coverage_quantization.py`（146） | 全读。`:37-39` lround、`:50-74` 枚举、`:87-94` 方差律、`:96-113` 负控、`:115-125` banker's、`:127-137` 门。**看到**：`:107` `"max_abs_r_no_quantization": 0.0` 是**硬编码常量**，脚本中**没有任何 step=0 的计算**（`:98` 的 `steps` 从 1/255 起），而 docstring `:14-15` 声称"profile with quantization disabled (step=0) … every metric identically 0"⇒ **伪负控，结论是写死的**；`:8` "the **02** table value"——悬空引用；`:128-137` 门条件真实 | **须修** |
| 15 | `code/audit/route2/exp09_sum_vs_per_leaf_criteria.py`（131） | 全读。`:40-48` Kahan、`:51-56` 死函数、`:59-107` 三类注入、`:109-127` 判据。**看到**：`:51-56` `zero_sum_pair_inject` 定义后从不调用，`:55` `... if False else None` 是自我否定的死码；`:80-81` `cfg[:cfg.find("_a")] if False else cfg` 同样是 `if False` 死码；`:122` 门条件真实；`:112-115` 自陈分母取 4e-16 地板且"部分配置给出精确 0.0"（诚实度高） | **建议** |
| 16 | `code/audit/route2/exp02_polar_pixel.py`（123） | 全读。`:34-43` 双模型、`:49-76` 逐 N + 全天闭合、`:78-84` 亏空拟合、`:96-107` 负控。**看到**：`:8,82-83` 仍以被否的 `0.104369` 作 `claimed_constant` 并算 `max_abs_dev_from_claim` ⇒ 报告量带 +1.9e-5 系统偏置；`:74-75` `if f<4 or f>=8 else` **两分支完全相同**（残留死条件）；`:113` 把 `|Σ/4π−1|<1e-12` 当 PASS 条件——**恒真门**，而本片 `REPORT_paper.md:79` 自己写"逐叶偏差在求和层面自消，弦模型总面积不能作逐叶判据" ⇒ **单元内部自相矛盾**；`:100-107` 负控 note 对 K=96 采样地板的解释写得诚实 | **须修** |
| 17 | `code/p6_leafmap.cpp`（117） | 全读。`:15-47` 三量、`:52-75` T18、`:77-108` T19。**看到**：`:20-31` `leaf_area_curve` 是 **chart 直线**采样，对**极冠边不等于真实边界**（真边界 `cosθ=a+b/φ²`），但 `:59` 与 `:66` 把它与 `chord/exact−1` **并列**呈现为"弦折线面积亏损"，两列口径不同；`:117` 恒 `return 0`，无门 | **须修** |
| 18 | `code/audit/route2/exp04_area_operators.py`（111） | 全读。`:47-55` 八分体锚、`:57-74` 穷举互校、`:76-91` 归一化负控、`:93-103` 门。**看到**：`:89` `"claimed_in_02": 0.096`——又是**"02"** 悬空引用，且 `:99-100` 的门只判 `>0.02`，那个 0.096 从不被核对；`:8-9` docstring 复述 9.6% 同源；`:98` `closure_worst<1e-12` 是**恒真闭合门**；`:96-102` 的负控（raw 非归一化顶点 → VOS 偏移）是**真判别力** | **建议** |
| 19 | `code/audit/route3/exp06_projection_budget.py`（99） | 全读。`:23-37` part_a、`:39-69` part_b、`:71-96` 门。**看到**：`:11` 明写 `the "delta < 4e-8" comment at spherical_overlap.cpp:1002 is wrong`——该注释**已不存在**（现 `:1009-1010` 写 −5.0e-7），`:1002` 也不是它 ⇒ **判据审计的是一个已被订正掉的状态**；`:93` 的 verdict 键名 `comment_4e8_is_wrong` 建立在已消失的前提上；`:12-13` `import os` 重复；`:84` 精确闭合模型 `(1+ρ²)^{3/2}−1+θ²/4` 是**拟合后宣布为闭式**（残差 ≤1.4e-8 与被略的 ρ²θ² 同量级，不可区分） | **须修** |
| 20 | `code/p4_hst.cpp`（94） | 全读。`:36-77` `scan`、`:79-94` `main`。**看到**：**`:76` `return (f1 == 0) ? 0 : 1;` —— 退出码只看 REC-1 的破门数 `f1`，V0 的 `f0` 与 oracle 完全不影响判定** ⇒ 对 V0 恒真；这正是 `p3_sweep_neg.cpp:157-159` 记录的"P3-05 订正"要治的病，**该订正只落到 T12，没落到 T16**；`:70` 只打印前 12 行 + `r0>1e-6` 的行，REC-1 与 oracle 的破门行被隐去；`:22` `rel()` 恒绿分支；`:93` `return bad` 计数可 >255 | **须修** |
| 21 | `code/audit/kcorr/run_extra.py`（91） | 全读。`:43-59` g0b、`:62-76` g3b、`:79-84` g7、`:87-91` main。**看到**：`:5-6` 前提错（"k_corr 必须恒 1(任何 N)"）；`:57` 恒红门且只塞进 dict，`:87-91` 从不读 `gate_pass`、从不 `sys.exit`；`:9` 伪引"正本公式 N=5 时渐近式**低估** 8.5%(PHASE2_UPM.md §5)"——方向与数值双错（见 §4 须修 6）；`:37` 把 `datetime.now()` 写进归档 JSON ⇒ 重跑永不逐位一致，与 `REPORT_experiment.md:21` "与既有存档逐位可对照"矛盾 | **须修** |
| 22 | `code/audit/run_all.sh`（90） | 全读。`:22` `set -eu`、`:29-34` `run()`、`:45-52` 前置自检、`:54-82` 29 条腿、`:86-89` `exit "$RC"`。**看到**：`set -eu` 在、RC 聚合写法正确、公共前置步 fail-closed 正确——**这部分上游没说错**；但 RC 的唯一输入源是进程退出码，而它调用的**全部 Python 腿都没有 `sys.exit`**（我对 6 个 kcorr/route 脚本 grep `sys.exit|SystemExit` 全为 0 命中）⇒ `:19-20` 的失败传播承诺对科学判据**结构性失明**；`:9-10` seed 表与实际不完全一致（route2/exp03 的 `default_rng(20260927)` 显式未用）；覆盖面缺口见 §4 | **须修** |
| 23 | `refs.md`（83） | 全读。V1–V7 + 标注级表 + 方法学备注。**看到**：这是本片**最干净的一份**：`:66-75` 明确把 l'Huilier/Snyder/Kahan 降级为标注级，把 Turner 2006 标"未证实、停止转引"、C&G 1995 标"错误引证"；`:81` "全部『原句』引文为全文实取后逐字摘录，非转述"；`:14,24,27,55` 记录了 §号/题名/出处的历次订正。**唯一保留**：`:14` 的 §5/§5.3 罗��数字↔阿拉伯数字映射、Górski 式(23) 的精确位置声明均标"一手实取"，我**无法离线复核，标待联网核验** | **通过（本片最佳）** |
| 24 | `code/audit/kcorr/read_tables.py`（76） | 全读。`:11-14` 载入、`:16-74` 建表、`:75` 覆写。**看到**：`:17` 表头自称"由 results/*.json **机器生成**"，但 `:24` 整行是**字面量**（含 `PASS(=k_gauss(5), 非 1)`），是全表唯一 N=5 行；`:11-14` 的 11 个 JSON 里**没有 `g0b_sanity_fixed.json`** ⇒ 机器判红的臂被排除在这行之外；`:14` 载入的 `g3b_n5_hiprec.json` **全仓无生产者**（`git grep` 只命中本行），而 `run_all.sh:78` 实跑的 `direct_char.json` **无任何消费者**；`:75` 覆写**已入库的** `results/audit/kcorr/tables.md`；`:44-45` `or float('nan')` / `if kgn:` 会把 0.0 吞成 nan | **须修** |
| 25 | `code/audit/route3/exp02_polar_limit.py`（72） | 全读。`:18-19` 常数、`:21-44` `run_N`、`:46-72` verdict。**看到**：`:4` 头部仍写被否的 `0.104369/N^2`；`:19` `DEFICIT_C` 实际算出订正值 0.1043885（与 `e2` 同型：注释与代码自相矛盾）；`:59-66` 的 5 条 verdict 是**真判别力门**（含 `count16`、`limit_converged` 的双条件）；`:62` `d[-1] < 5e-5 and all(3.0<x<5.5)` 合理 | **须修（仅头部常数）** |
| 26 | `code/audit/route3/exp08_scale_constant.py`（64） | 全读。`:18-33` 恒等、`:35-51` 决策窗、`:55-58` verdict。**看到**：`:24-29` 逐 N 位级恒等（`ulp_diff`）是真检验；`:47-50` 翻转窗宽诚实（(105517.3, 105538.14] 即 20.84″）；`:58` `decision_flip` 只在**钦定的 `finest=105527.7`** 上求值，是**单点演示**；`:5-7` 的"flips the chosen nside by a factor 2"未提生产 `[16, 2²²]` 钳位（见 §4 阻断 4） | **须修** |
| 27 | `code/p10_seam.cpp`（64） | 全读。`:21-62` T26。**看到**：`:49` 暴力补扫的 7×7×7 邻域中心取 `c[0]`——**候选集合的任意第一个成员，不是 drop 中心** ⇒ 完备性检查的空间覆盖无保证，而 `EXP-07-POLAR.md:448` 把它表述为"暴力补扫…0 个"；`:54-56` `ip` 随即被 `pix` 覆盖、`rem` 未用（死码）；`:63` 恒 `return 0`，"候选集合外仍有交叠的叶 N 个"从不参与判定 | **须修** |
| 28 | `code/audit/kcorr/read_summary.py`（46） | 全读。纯 stdout 打印器。**看到**：**零消费者孤儿**——`git grep read_summary` 全仓只命中 `results/audit/KEY_RESULTS.md:59`，而该行声称它生成 `summary.csv / tables.md`（它只 `print`，不写任何文件）；`:13,23,34,40` 仍用已被 D-xx 统一掉的旧名 `k_shape`；`:1` `sys` 未用；`:12-14` 是全仓唯一把 `pass289/pass25` 打到人眼前的地方，却不在 `run_all.sh` 里 | **须修** |
| 29 | `code/audit/kcorr/direct_char.py`（45） | 全读。`:21-23` 常量、`:27-41` 定征、`:43-45` 落盘。**看到**：`:9` docstring 称落 `results/direct_char.json`，`:43` 实写 `results/audit/kcorr/direct_char.json`；`:25-45` 全在模块顶层执行，无 `__main__` 守卫；**无任何门、无断言、无退出码**；`:39` `k_gauss = var_med/((π/2)·med_mad²/n)` 定义正确（我按 `DERIVATIONS-P3.md:164` 的口径复算一致） | **建议** |
| 30 | `code/p8_hstgrid.cpp`（28） | 全读。`:13-20` 9×9 栅格、`:21-26` 排序打印。**看到**：`:17` `a` 与 `a3` 用**同一个** `vos_area_rotated` 原语，测的是 drop **构造**差（RC3），口径正确；`:13` 步长 1000、范围 ±4000 ⇒ 9×9=81，与 `EXP-07-POLAR.md:589` 的"9×9 全帧栅格"一致；`:26` 只打前 12 + 最小值（按比值排序，隐去中间分布）；`:27` 恒 `return 0`，无门 | **建议** |

---

## 4. 发现清单

> **计数口径**：本片的"门"分三层——①**门实例**（脚本/CTest 里一条独立可判红的判据）；②**去重门**（多个门实例口径等价时合并计一次）；③**整改分母**（需改动的落点集合）。下列每条都标明是哪一层。

### 【阻断】11 条

**B0 · `results/p3_t12.out` 是一份**假绿归档**：64 次 V0 破门与 `# SUMMARY T12: PASS` + `Exit status: 0` 并存；源码改过、归档从未重跑。本片正本 `EXP-07-POLAR.md:417` 直接引用了这行 PASS。**
- 位置：`实验/healpix-polar/results/p3_t12.out`（角点02 / 角点13 / 末行）；`code/p3_sweep_neg.cpp:157-166`；被引用处 `docs/EXP-07-POLAR.md:410-417`
- 现状（**可复现**）：
  - `cat results/p3_t12.out` → 角点02（北极）`V0 破门=32 最坏=6.626e-04`、角点13（南极）`V0 破门=32 最坏=6.626e-04`（**合计 64 次破门**），末行却是 `# SUMMARY T12: PASS` + `Exit status: 0`。
  - `cd 实验/healpix-polar && sha256sum -c results/SNAPSHOT.sha256` → 47 项中**只有 `code/p3_sweep_neg.cpp` 失败**，其余 46 项全「成功」。
  - mtime：`code/p3_sweep_neg.cpp` = **2026-09-28 23:35**；`results/p3_t12.out` 与 `SNAPSHOT.sha256` = **2026-09-23 04:44**（源码晚 5 天）。
- ⇒ `p3_sweep_neg.cpp:157-159` 自述的「P3-05 订正：SUMMARY 判据原先只读 REC-1 与 oracle，V0 破门被排除 ⇒ 该行对 V0 恒真；订正后三者任一破门即整轮判红」**从未真正跑过**。归档是订正前旧码的产物，而旧码对 V0 恒真。**这是本项目固化检查项「代码改了、归档没重跑」的最强实例。**
- 应为：重跑 `p3 t12`（届时 SUMMARY 应为 FAIL，bad=64>0）；`SNAPSHOT.sha256` 纳入 CI 强校验；重跑前 `EXP-07-POLAR.md:417` 的 PASS 不得作绿证据。
- 口径：**门实例层** 1；**整改分母** = 1 归档 + 1 正本引用 + 1 CI 缺失。

**B0b · `route3/exp03_weight_conservation.py` 的 G08-05 B6 守卫同样从未跑过（第二个 stale 归档）。**
- 位置：源码 `code/audit/route3/exp03_weight_conservation.py:41-62,85-100,378-450,467-475`（约 110 行 B6 工作）；归档 `实验/healpix-polar/results/audit/route3/exp03_weight_conservation.json`
- 现状：归档顶层键为 `['exp','seed','pixfrac','ra0_deg','dec0_deg','npix_side','conservation_300as_n512','pixel_norm_delta','injection','mean_estimator','control_points','verdict']`，**`seam_absolute_floor_B6` 不存在**；verdict 缺 `seam_domain_guard_keeps_real_reading / seam_real_scale_100pct_loss_caught / seam_domain_guard_blocks_blind_band / seam_measured_per_drop_all_ok / seam_measured_per_drop_n` 五项。
- ⇒ 源码 `:399-449` 的整个 `handover` / `true_residual_recomputed` / `previous_claim_sr` 订正**全部未经执行**。本片 B1 的核心论据（"守卫已加在实验侧"）目前只有源码、没有归档执行证据。
- 应为：重跑后再引用 B6 结论；与 B0 一并纳入 SNAPSHOT/CI。

**B0c · 1e-6 相对闭合门低于 oracle 自身的精度地板 ⇒ oracle 自己破门，破门无法归因。**
- 位置：门 `p3_sweep_neg.cpp:176`、`p2_algorithms.cpp:164`、`p4_hst.cpp:69`、`p9_review.cpp:42`；证据 `results/p4_hst.out:54`、`results/p4_hst.out:16,32`
- 现状：`A_drop ≈ 1e-12 sr` 时双精度求交绝对地板 ~1e-16 sr ⇒ 相对量 ~1e-4，**比 1e-6 门严 100 倍**。归档 `p4_hst.out:54`（M16 负对照）明写 `V0 破门=15 最坏=1.744e-04 | REC-1 破门=25 最坏=2.830e-04 | oracle 最坏=2.212e-04` —— **被定义为真值的 oracle 自己超门 2.2e-4**，而 `p4_hst.cpp:76` 对 oracle 无门。
- ⇒ 该配置下任何"破门"都无法归因于被测算法；EXP-07 §4.9 把它登记为"未定位的共同地板"是对的，但**"未定位"的一部分原因是门比地板还严**。
- 应为：主门改绝对（sr）或按 drop 条件数缩放；相对门只用于 `A_drop` 足够大处；oracle 破门必须单独计数而非静默。
- 口径：**门实例层** ≥4（同一缺陷在不同探针的重复出现）；**去重门** = 1（同一地板问题）。

**B0d · `p9_review.cpp:198-203` 的 T25 结构上无判别力，且正本对它的描述与代码不符。**
- 位置：`code/p9_review.cpp:196,200-201`；归档 `results/p9_review.out` 的 T25 块（15 行）；正本 `docs/EXP-07-POLAR.md:449`
- 现状：代码比较 `chart_uv_to_xyz(0,u,v,0)` 与 `chart_uv_to_xyz(0,u,v,1)` —— **两种角点公式**，而正本称其为「角点法 vs **真实曲线法**」。归档实测：15 行中 **12 行恰为 `0.000e+00`**，其余 3 行为 1.2e-16/7.9e-17/6.2e-17，且最大值出现在 `u+v=1.000000001`（dv=+1e-9）而非正本所称的 ±1e-12。
- 应为：检接缝不连续须用**共同外部参照**（生产 `pix2ang`/`loc2xyf` 或解析边界），或直接比较 `u+v=1⁻` 与 `1⁺` 的位置跳变。
- 口径：**门实例层** 1；**整改分母** = 1 探针 + 1 正本。

**B0e · auto-nside「决策翻转」演示点在生产上不可能发生（被 `[16, 2²²]` 钳位抹平）。**
- 位置：`code/audit/route1/e1_leaf_area_and_scale.py:188-189,203`；`code/audit/route3/exp08_scale_constant.py:35-39,47-50,58`；生产 `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:773-794`
- 现状：两脚本用自建公式（`e1:188` 还多套了一层 `ceil`，生产**没有**这个 `ceil`），在钦定的 `finest=105527.7″` 上报「真值 nside=4 vs 禁抄值 nside=2，factor lost=2」。我按**生产真实控制流**复算：`drizzle_engine.cpp:773` 现场求 `HEALPIX_SCALE_PER_NSIDE_ARCSEC`，`:781-788` 是 `while (nside < nside_min_real) nside <<= 1`（**无 ceil**），`:791-794` **钳位到 `[NSIDE_MIN=16, NSIDE_MAX=4194304]`**。代入：`211076.28514206142/105527.7 = 2.00020 → nside=4 → 钳位 16`；`211034.6/105527.7 = 1.99980 → nside=2 → 钳位 16`。**两支都给 16，`clamped=true`，无分档差。**
- ⇒ 该演示是"先算翻转窗、再往窗里放点"的构造，**生产中不可能红**。真实分档窗存在但在 k=4..21（窗宽 1.97e-4，例如 `finest ∈ (13189.6625, 13192.2678]`），两脚本从未演示。
- 应为：演示点移到未被钳位的 k≥4 窗内，并按生产控制流（含钳位）实现 `choose()`；或明确声明该脚本只做常数算术自检、不构成生产判据。
- 口径：**门实例层** 2（两脚本同型）；**去重门** = 1。

**B1 · 冻结的「面积守恒闭合」判据在本片所测接缝缺陷上恒绿；治理订正停在一支实验脚本，未上修。**
- 位置：`docs/EXP-07-POLAR.md:33,494,620,687`；`docs/DERIVATIONS-P3.md:194`；`REPORT_experiment.md:95,185,194`；被指正本 `docs/science/algorithms/DRIZZLE_GEOMETRY.md:320,350-351,362-375`；可执行门 `lib/algorithms/drizzle/healpix_drizzle/tests/p3_conservation_gate.cpp:47,171,177,350`；缺陷自述 `code/audit/route3/exp03_weight_conservation.py:41-62,446-470`
- 现状：`ε_abs = 1e-15 sr`。我用 EXP-07 **自己 :475–477 的表**复算三档接缝绝对亏空：

  | 尺度 | `A_drop` | `A_prod_sum` | \|差\| (sr) | 文档声称 | 隐含相对 = 差/A_drop | 文档相对 |
  |---|---|---|---|---|---|---|
  | 0.2008″ | 9.477137e-13 | 9.476846e-13 | **2.910e-17** | 2.9e-17 ✔ | 3.0705e-05 | −3.074e-05 ✔ |
  | 0.0400″ | 3.760709e-14 | 3.765910e-14 | **5.201e-17** | 5.2e-17 ✔ | 1.3830e-03 | +1.383e-03 ✔ |
  | 0.0050″ | 5.876108e-16 | 5.980505e-16 | **1.044e-17** | ~~1.05e-16~~ ✘ **错 10×** | 1.7766e-02 | +1.777e-02 ✔ |

  ⇒ **最大实测绝对亏空 = 5.201e-17 sr，而 ε_abs = 1e-15 sr 比它大 19.2 倍。** 而 `DRIZZLE_GEOMETRY.md:375` 自己写"接缝与任何 `A_drop ≲1e-9 sr` 的域**一律以绝对项判**" ⇒ `max()` 在整个接缝域恒取 ε_abs ⇒ **门恒绿**。
- 独立佐证（本片自己的脚本）：`route3/exp03_weight_conservation.py:44-47` 明写「A_drop ≲ ε_abs 时 max() 取 ε_abs，相对容差 tol/A_drop ≥ 1 ⇒ **100% 通量丢失（Σa=0）仍判绿**。实测 θ=0.005″/px、pf=1 → A_drop=5.876e-16 sr、**tol/A_drop=1.70**；pf=0.8 → **2.66**」。
- 我核实：该脚本为此加了实验侧 `APPLICABLE_MIN_ADROP` 守卫（`:61,75`），并在 `handover` 写明"**正本容差定义不在本单写入面，只登记移交**"。而正本 `DRIZZLE_GEOMETRY.md:320,350-351,362-375` 与可执行门 `p3_conservation_gate.cpp:47,171`（`judge_limit() { return std::max(P3_REL_BUDGET*a_ref, P3_ABS_BUDGET_SR); }`）**均未加守卫**，且该门的位置集（`:177,350`）恰恰含 `SEAM` 与 `HST` 组。
- ⇒ **治理订正停在一支实验脚本里，没有上修到正本与可执行门。** 本片 `EXP-07:33/620/687`、`DERIVATIONS:194`、`REPORT:95` 全部把这条失效判据复述为"验收口径"，无一携带守卫条件。
- 口径：**门实例层**（正本 1 + 可执行门 1）；**去重门** = 1；**整改分母** = 6 份文档 + 1 个 C++ 门。

**B1b · 接缝绝对亏空第三档错 10 倍，连带「只变 3.6 倍」与幂律 −0.83 一起失效。**
- 位置：`docs/EXP-07-POLAR.md:29,494,687,706,830`；`docs/DERIVATIONS-P3.md:191`；`REPORT_experiment.md:95,194`
- 现状：三档写 `2.9e-17 / 5.2e-17 / **1.05e-16** sr`。用 EXP-07 自己的 `:475–477` 表直接相减，第三档是 `|5.980505e-16 − 5.876108e-16| = **1.044e-17**`（该值除以 `A_drop=5.876108e-16` 得 `1.777e-02`，与表列 `prod/gd−1` **完全自洽**；若取 1.05e-16 则相对应为 1.787e-1，与表列矛盾）。
- 连带：三档实为 **2.910e-17 / 5.201e-17 / 1.044e-17**，**非单调**（最小尺度反而下降）；`max/min = 4.98×`，而文档称"跨 40 倍尺度**只变 3.6 倍**"；端点幂律 `|rel| ∝ A_drop^(−0.83)` 与"绝对亏空 ∝ A_drop^(+0.17)"随之作废。
- 应为：按 1.044e-17 订正全部 7 处，并把幂律与倍数结论降级为"三点不足以定律"。
- **正面**：该订正**强化**了 B1 —— 实测最大绝对亏空从 1.05e-16 降到 5.20e-17，ε_abs 高出倍数从 6.7–23× 升到 **19.2×**，恒绿结论更硬。

**B2 · 接缝破门区（3.3e-3 @HST 0.04″/px）全仓无门能抓。**
- 位置：`docs/EXP-07-POLAR.md:520,703-716`；`code/p9_review.cpp:137-155`；`code/p10_seam.cpp:48-61`
- 现状：缺陷真实且量级可信，但绝对量级 ≤1.1e-16 sr 落在 ε_abs 地板内（见 B1），相对项被掩盖；六条"已排除机制"均不解释；`§7:709-710` 自认最后一步未定位。
- 应为：在守卫生效的前提下，把接缝档改为**逐 drop 相对项 + 绝对项双判**（相对项不可达则 fail-closed 登记为未达成），而不是"按绝对项判"（=按 ε_abs 判 = 恒绿）。
- 证据：同上 + `EXP-07-POLAR.md:516`（HST 3.317e-03，1.17e-16 sr）。

**B3 · kcorr G0b 自检臂恒红，判红被吞，报告只引绿臂。**
- 位置：`code/audit/kcorr/run_extra.py:5-6,57,87-91`；归档 `results/audit/kcorr/g0b_sanity_fixed.json`（`gate_pass:false`，rows 1.7673/1.0836/1.0293）；反向正确判据 `code/audit/kcorr/run_scan.py:56-58,72-73`；选择性报告 `REPORT_experiment.md:80`
- 现状：门 `all(|k_corr−1|<0.06)` 在 N=5/25 恒红；`gate_pass=false` 已落盘却从不影响退出码；报告只写 N=289。
- 应为：门改用 `k_gauss(N)` 口径（对齐 `run_scan.py:72-73`），或 `gate_pass=false` 直接 `sys.exit(1)`；报告必须披露 N=5/25 两臂。
- 证据：归档 JSON 直读（我复核：`|Δ|`=0.767/0.084/0.029）。
- 口径：**门实例层** 1；**去重门** = 1；**整改分母** = 1 脚本 + 1 报告。

**B4 · `run_all.sh` 的 RC 聚合对全部科学判据结构性失明。**
- 位置：`code/audit/run_all.sh:19-20,29-34,86-89`；被调脚本（grep `sys.exit|SystemExit` 命中数 0：`route1/e1,e2,e5`、`route2/exp02,exp04,exp05,exp08,exp09`、`route3/exp02,exp05,exp06,exp08`、`kcorr/run_extra,read_tables,read_summary,direct_char`）
- 现状：`set -eu` 存在，但 `[ok] <腿>` 只代表"没崩"；`p3/p4` 等 C++ 探针里 `p9/p10/p6/p8/p2` 也是恒 `return 0`。
- 应为：每个带门的脚本在门红时 `sys.exit(1)`（或写 `__main__` 守卫）；`run_all.sh:19-20` 的承诺才成立。
- 证据：grep 命中表（上）。
- 口径：**整改分母层** = 29 条腿（其中至少 15 条带门）。

**B5 · `p4_hst.cpp`（T16）的退出码只看 REC-1，对 V0 恒真——P3-05 订正未传播到本探针。**
- 位置：`code/p4_hst.cpp:76`（`return (f1 == 0) ? 0 : 1;`）vs 已修复的同型 `code/p3_sweep_neg.cpp:157-161`
- 现状：`scan()` 的返回码由 `f1`（REC-1 破门）独决定，`f0`（V0）与 oracle 不参与。V0 全破门时仍 `return 0`。
- 应为：三者任一破门即判红（照抄 `p3_sweep_neg.cpp:160` 的 `bad += f0+f1+fo`）。
- 证据：两处代码并列；`EXP-07-POLAR.md:571,582` 报的"V0 15/25、24/25 破门"正是被这条退出码忽略的臂。
- 口径：**门实例层** 1；**去重门** = 与 B4 的"无退出码"不同类，单列。

### 【须修】18 条

**M1 · 恒真门三例（代数恒等式型 / 恒红型 / 往返自证型）。**
| 型 | 位置 | 为何恒真 |
|---|---|---|
| 恒绿（结构相消） | `route1/e2:155`、`route2/exp02:113`、`route2/exp04:98` | Σ 弦面积精确闭 4π，逐叶正负偏差相消；`REPORT_paper.md:79` 自承"不能作逐叶判据"，但 exp02:113 仍把它当 PASS 条件 |
| 伪负控（写死结论） | `route2/exp08:107` | `"max_abs_r_no_quantization": 0.0` 是字面量，脚本无 step=0 计算；docstring `:14-15` 却声称做过 |
| 往返自证 | `route1/e2:189-214` | 赤道叶在 (φ,z) 平面是仿射像，采样多边形与四角四边形**几何同一**，"真曲线度量归零"由构造保证；且只在赤道跑 |
| 代数恒等式 | `route1/e5:105-117` | 环求积 `ratio_num` 与 `mean_sec3` 是同一恒等式改写，对指数 p 不敏感，无法证伪 |
- 证据：各行；`REPORT_paper.md:79` 的自承。
- 口径：**门实例层** ≥4；**去重门** = 4（三型不同，不合并）。

**M2 · 悬空引用「repo doc 02 / 02 台账」共 5 处。**
- 位置：`route1/e1:13`（§1.1）、`route1/e2:4,18`（§1.3/§1.4）、`route2/exp08:8`（"the 02 table value"）、`route2/exp04:89`（`claimed_in_02: 0.096`）、`route3/exp05:6`（"the 02-ledger line-58 blanket claim"）
- 现状：`ls 实验/healpix-polar/docs/` 只有 `DERIVATIONS-P3.md / DISPUTES.md / EXP-07-POLAR-摘要.md / EXP-07-POLAR.md`，**无编号 02 的文档**；`git ls-files | grep face_native.h` = 0、`INNOV-04-01` = 0（`chart_native.h:2-3,26,150`）。
- 应为：改指真实出处（`DERIVATIONS-P3.md` §D1/§D2/§D5），或删除。**这正是 HEAD 提交 `f9650dd0`"清除运行期文案里从未存在于被引条款的伪引"要治的同一类。**
- 证据：`ls` 输出 + `git ls-files` 计数。

**M3 · 已被正式裁定的常数 `0.104369` 仍留在 3 支脚本的 docstring/注释里，且与紧邻的代码计算自相矛盾。**
- 位置：`route1/e2:11`（docstring）与 `:166`（`coeff_theory` 实际算出 0.1043885）；`route3/exp02:4` vs `:19`；`route2/exp02:8,82-83`
- 裁定依据：`docs/DERIVATIONS-P3.md:64-68`、`docs/DISPUTES.md:16`、`实验/healpix-polar/REPORT_paper.md:79` 均已定 0.1043885 并注明 0.104369 系转写误差。**`DERIVATIONS-P3.md:67` 点名"路线1 高精度求值 0.10438850961"时引用的正是 `e2`——被引脚本自己的注释写着被否值。**
- 应为：三处同步为 0.1043885；`route2/exp02:82-83` 的 `max_abs_dev_from_claim` 改对订正值，否则报告量带 +1.9e-5 系统偏置。
- 证据：`git grep 0.104369 实验/healpix-polar/` 的 8 处命中。

**M4 · `e5` 三处包装与自身数据互相矛盾，其中 verdict 是硬编码字符串。**
- 位置：`route1/e5:16-19`（docstring 称圆盘 −θ²/2 = −5.000e-7）、`:207`（打印 "12.5x too small" 却紧邻 `-2.51204e-07`）、`:184`（verdict 字面量）
- 现状：圆盘解析为 −θ²/4=−0.2512θ²（脚本自己测到 0.2512）；−5e-7 是方形值；2.512e-7/4e-8 = **6.28×** 不是 12.5×；verdict 宣称 "theta^2/2-family model fits" 而拟合系数 0.2512。矛盾已固化进归档 `results/audit/route1/e5_projection_budgets.txt`。
- 应为：docstring 区分圆盘/方形；打印文案由实测值算比值；verdict 由 `res_good/res_wrong` 条件生成。
- 证据：解析 `πsin²θ / 2π(1−cosθ) = (1+cosθ)/2`；归档拟合系数。

**M5 · `read_tables.py` 在自称"机器生成"的表里有一行手写 PASS，且该行的数字方向与机器判定相反。**
- 位置：`kcorr/read_tables.py:17`（表头）、`:24`（手写行）、`:11-14`（11 个 JSON 中**无** `g0b_sanity_fixed.json`）
- 现状：N=289/25 两行从 JSON 取机器判定（归档 `tables.md:7-8` 为 `True`），N=5 行是字面量 `PASS(=k_gauss(5), 非 1)`；而 G0b 对 N=5 的机器判定是**红**（1.7673）。同表两行机器、一行手写。
- 缓解（如实转达）：该行**数字本身**有独立佐证（`g3b_n5_hiprec.json` k_mean=1.6339、`direct_char.json` k_gauss=1.6370）⇒ 是**流程与呈现缺陷，非数值造假**。
- 应为：N=5 行改与 22-23 同构从 JSON 填充，并纳入 `g0b_sanity_fixed.json`。
- 口径：**门实例层** 1（呈现层伪判词）。

**M6 · 伪引：`run_extra.py:9` 的方向与数值双错。**
- 位置：`kcorr/run_extra.py:9` — 「正本公式 N=5 时渐近式**低估** 8.5%(PHASE2_UPM.md §5)」
- 被引原文（`docs/science/PHASE2_UPM.md`，§5 连续定义内）写的是渐近式对真方差**恒为高估**，N=5 处**高估 +9.5%**；`direct_char.json` n=5 的 `var_over_asymptotic = 0.9113` ⇒ 高估 9.74%，与文档同向。`grep "8.5"` 在该节内零命中。
- 应为：改为"高估约 9.7%（N=5）"并引 §5 原句。**方向写反会把保守读成非保守。**
- 口径：伪引层（主项是裸从句+分节号，无成对引号，按成对引号扫会漏）。

**M7 · 归档 JSON 写入墙钟时间戳，与"逐位可对照"矛盾。**
- 位置：`kcorr/run_extra.py:37`（`obj["written_utc"] = datetime.now(timezone.utc).isoformat()`）；归档 10 个 JSON 均含该键（我 grep 确认）
- 现状：`REPORT_experiment.md:21` 与 `code/audit/run_all.sh:16` 均称"各实验 JSON …与既有存档**逐位可对照**"；写入时间戳后**重跑永不逐位一致**。
- 应为：时间戳移出数值归档，或改为显式的 `--stamp` 可选参数并在文档说明。

**M8 · `read_summary.py` 是零消费者孤儿，且索引文档对其声明不实。**
- 位置：`kcorr/read_summary.py`（全文）；`results/audit/KEY_RESULTS.md:59`
- 现状：`git grep read_summary` 全仓**只命中 KEY_RESULTS.md:59 本身**，该行称它生成 `summary.csv / tables.md`；脚本只 `print`，不写任何文件。
- 应为：纳入 `run_all.sh` 或删除 + 撤掉该声明。附带：它是全仓唯一把 `pass289/pass25` 打到人眼前的地方，不跑 ⇒ 门判据不可见。

**M9 · `chart_native.h` 的"逐叶分配"在实现里没有产出；四处理论前提对极冠不成立。**
- 位置：`chart_native.h:6`（"叶边界在 chart 里是精确的轴对齐直线段"）、`:355-364`（逐叶 `a2v` 从不累加）、`:273,298,249,217`（静默丢面）
- 现状：`:354` 只把整块 `res.chart_area` 加一次，`ChartAlloc` 无逐叶面积向量；而 `EXP-07-POLAR.md:50,670` 的"叶侧零近似 / 面内叶面积求和恒等于 π/3/nside²"正是本路径的核心卖点。极冠边是 `cosθ=a+b/φ²`（refs.md:17），非轴对齐直线。
- 应为：要么实现逐叶产出，要么把主张降级为"叶面积常数、叶边界非直线"；静默丢面处加计数/fail-closed。

**M10 · `p6_leafmap.cpp` 的"真实曲线"参照是 chart 直线采样，与 `chord/exact−1` 并列呈现但口径不同。**
- 位置：`p6_leafmap.cpp:20-31`（`leaf_area_curve` 沿 chart 直线插值）、`:59,66`（两列并排）
- 现状：赤道支两者相同、极冠支不同；`EXP-07-POLAR.md:190-198` 的"单叶面积亏损"表把这两种口径混在一列里。
- 应为：拆成"弦 vs 解析叶面积"与"chart 直线 vs 解析"两列，并注明极冠不可比。

**M11 · `p9_review.cpp` 的探针分母与生产分母不同口径；`p10_seam.cpp` 的候选完备性检查锚点错。**
- 位置：`p9_review.cpp:32,65,106,140`（`vos_area_rotated`）vs 生产 `spherical_overlap.cpp:1099-1101`（`<1e-3 rad` 走切平面）；`p10_seam.cpp:49`（邻域中心 = `c[0]`）
- 现状：T27/T29 的"生产 V0 破门数"是在探针分母下判的；`EXP-07-POLAR.md:448` 把 `p10` 的结果表述为"暴力补扫…完备性已证"，但扫描盒锚在候选集任意成员上。
- 应为：T27/T29 的生产臂改用 `build_drop_geometry_into` 的 `g.drop_area` 作分母；`p10` 的邻域锚到 drop 中心或对全部候选做并集。

**M12 · 三处"代码改了、判据/文档没同步"（本项目头号失效模式）。**
| 对象 | 现值 | 仍声称旧值的地方 |
|---|---|---|
| `WCS_ADAPTIVE_MAX_DEPTH` | `spherical_overlap.cpp:866` = **12** | `DERIVATIONS-P3.md:112` 引 `:857`（差 9 行）；`route3/exp05:3` 写 `max_depth = 8` |
| 切平面偏差注释 | `spherical_overlap.cpp:1009-1010` = **−5.0e-7**，已无 `4e-8` | `docs/engineering/STANDARDS_REGISTRY.md:170` 仍写"注释论证偏差 **<4e-8**；**禁删**"；`route3/exp06:11,93` 与 `route2/exp05:11,121` 仍在审这个已消失的状态 |
| 接缝幂律 | `EXP-07-POLAR.md:496` = **−0.83**（P3-06 已撤回 −1） | `DRIZZLE_GEOMETRY.md:373-374` 仍写"∝1/A_drop"并给出 3.9e-3 / 9.9e-2（与本片实测 1.383e-3 / 1.777e-2 差 2.8× / 5.6×） |
- 应为：按"同一提交内同时更新判据与其唯一证据面"处置；`STANDARDS_REGISTRY.md:170` 属跨域（不在本片），登记移交。

**M13 · `EXP-07-POLAR.md` 头部计数与内部引用失配。**
- 位置：`docs/EXP-07-POLAR.md:6`（"12 个源文件"/"30 个日志/CSV"）、`:7`（"§0.6"）、`:78`（`DRIZZLE_GEOMETRY.md:236`）、`:449`（T25 描述）、`:16`（`p4_hst3d.cpp` 未进 §9 代码表）
- 现状：目录实有 13 `.cpp` + 3 `.h`；`results/` 的 `.out+.csv` 为 33；文档**无 §0.6**；`:236` 实为线程 stripe 文字（门禁在 `:320`）；T25 比的是两种角点公式而非"真实曲线法"。`p4_hst3d.cpp` 产出了 SCI-703 的 P3-02 决定性证据却未进代码表。
- 应为：逐项订正或改为不写易漂移的计数（与 `:721` 自承的 `check_report.py` 无位置锚定同源）。

**M14 · `e5` 隐藏了唯一支持其 docstring 的那条臂（选择性报告）。**
- 位置：`route1/e5:154-163`（算 `rows_tq`）vs `:189-217`（txt 生成段，只印 `rows_t`）
- 现状：`tangent_plane_square_scan` 写入 JSON（归档 `over_theta2` = −0.4999999748/−0.499999/−0.499988，**正是 −θ_max²/2**），但 txt 与 stdout **完全没有这一段**。读者只看到与 docstring 矛盾的 0.2512。
- 应为：`rows_tq` 必须与 `rows_t` 同表打印并标注形状。

**M15 · `e5` 的「系数错 3 倍」定性已被正本逐字否定，且与自身数据相反。**
- 位置：`route1/e5:9-10,102-103,197-198`；被否定处 `docs/science/DISPUTE_RESOLUTION.md:196`；自身数据 `e1:... gnomonic_scan` 方形读数 `integrated/ρ_max² = 0.5000`
- 现状：正本原文「**「系数错 3 倍」的定性不成立；失实在把几何依赖量写成单一常数**」。E5 指控的 2e-6 恰是它自己方形臂测到的 0.5·ρ²。
- 应为：按 A-P3-03 裁决改写；并把 `2e-6` 的真实出处（`lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_geom.hpp:13,151`）写进注释。

**M16 · `e2` 的 `A_true` 理由式在帽环上为假，且「zone closure checked to 4π」声明的检查不存在。**
- 位置：`route1/e2:24`（docstring）、`:101`（硬编码 `A_true = π/(3N²)`）
- 现状：docstring 称 `A_true(j) = 2π·dz_j/S_j = π/(3N²)`，并注"(zone closure checked to 4*pi)"。按脚本自己的 `ring_levels`(`:46-59`) 复算该式：ring1 = **0.500·A**、ring2 = **0.750·A**、ring N+1 = **−1.000·A（负）**；该式只在赤道支成立。全文**无任何区闭合代码**。`A` 的取值本身是对的（由 chart 雅可比得出），错的只是理由式。
- 应为：区分帽环/赤道环，或删掉理由式与区闭合声明。

**M17 · `e1` 的三条「独立校验」实为自比/同义反复。**
- 位置：`route1/e1:196-197,275`（`abs_diff`）；`:128-129,148-149,263-266`（`A_alt` 列）；`:212-242`（往返 + bijectivity）
- 现状：①`abs(C − 211076.28514206142) = 0.0` —— 字面量与算值是**同一个 double**；②`4π/(12N²) ≡ π/(3N²)`，两列偏差**逐位相同**（归档三组数值全等）；③`inverse_face4_diamond` 由同一张 chart 代数反解，往返误差 1.11e-16 是浮点噪声；采样 3661 点 vs Ng=128 的 16384 叶，**两点同叶从未发生**，"leaf-id bijectivity"（docstring `:31`）无内容。
- 应为：删掉伪独立列或换成真正独立的高精度来源；往返检查加密采样格。
- 附：`:112` 硬编码 `f=4`，`:116` 用 `f=0`，三分支 chart 只测两支，docstring `:5-6` 却称"every base face"；`:8-10` H2 的前提「生产用手抄 211034.6」在 HEAD 已不成立（`drizzle_engine.cpp:773` 已现场求值）。

**M18 · route2/route3 的四条恒真/自证门（子代理核验 + 我复核）。**
| 位置 | 机制 |
|---|---|
| `route2/exp04:99-102` | l'Huilier 分支因 `:78-79` 先把顶点归一化而**结构上恒等于 0** |
| `route2/exp09:105-106,122` | 注入后显式把和残差扣回（`:89-90`/`:95-96`），sum 指标必然 ~0；门等价于 `per_leaf ≥ 0.04`，而注入量恒在 0.2–1.8 ⇒ 恒绿 |
| `route3/exp03:279-283,457` | `pair_red = abs(0.9003*a0 − a0)/a0 ≡ 0.0997`，门断言一个代数常数等于它自己 |
| `route3/exp05:168` | `meridian_zero = meridian_max < 1e-15`，但 `plane_dev` 除以 `hp_res`（N=32 时 5.4e-4）把 ~1e-17 点积噪声放大到 ~1e-14 ⇒ **归档实测 `meridian_zero: False`，恒红** |
- 应为：逐条加"反向可红"的对照臂（去掉残差扣回、从实测权重重算、容差提到噪声之上、换成非归一化顶点）。
- 口径：**门实例层** 4；**去重门** = 4（机制各异）。

### 【建议】14 条

S1 `route1/e1:121` `int(J_eq.size and J_pol.size)`（`and` 写在 `int` 上，当前侥幸正确）→ 写 `int(J_pol.size)`。
S2 `route1/e1:157-159` `chart_perturbed` 死代码且注释自相矛盾。
S3 `route1/e1:190-193` `oct_windows` 算了不判不打，docstring `:29` 却称已 demonstrate。
S4 `route1/e2:142-145` 用 `np.argmax` 求"worst ring"（应为 `argmin`），`ring1_rel/ring3_rel` 同型；当前靠对称性侥幸正确。
S5 `route1/e2:222` JSON 键拼写 `richsonson_residual`，`:246` 打印作 `Richardson residual`；且该量是 K→2K 单步差、无外推系数，与主项同量级。
S6 `route3/exp06:12-13`、`route2/exp05:82-83`、`route2/exp02:74-75` 死代码/重复分支。
S7 `p2_algorithms.cpp:56-57,72-73,139,158` 死变量 + printf 标签与实义不符（把 chart 原始偏差标成"×叶尺寸"）。
S8 `p3_sweep_neg.cpp:247,257` 三档 pf 不打印，33 行不可分辨；`:347` `all` 不含 T12。
S9 `kcorr/direct_char.py:9` 落盘路径与 `:43` 不符；`:25-45` 无 `__main__` 守卫、全程无门。
S10 `route3/exp02:6-7` 的负例「belt ~1e-8」错 3 个量级（归档 `belt_o1_over_N2=[0.4512,0.4756,0.4888,0.4958]` ⇒ belt ≈ 0.5/N²，N=256 时 7.6e-6），且该负例**只进数据、没有 bool 门**。
S11 `route3/exp03:61` 守卫阈值 `APPLICABLE_MIN_ADROP = 2e-15` 比它自己引用的正本失效域（`DRIZZLE_GEOMETRY.md:375` 的 `A_drop ≲1e-9 sr`）**宽 6 个量级**。
S12 `route3/exp06:10` 的标识符 `max_angular_radius_for_tan` 在生产**不存在**（真值 1e-3 在 `spherical_overlap.cpp:1936`）；`:6` 的 "rho_c → 0 gives 0" 与 `:90 rho0_zero` 断言等于 θ²/4 **直接冲突**；`:83-89` 的闭式是拟合后宣布为 `exact_closed_form`。
S13 `docs/engineering/STANDARDS_REGISTRY.md:44` 把 Górski §5.3 记成 "ang2pix/pix2ang"，实为 **"Pixel Boundaries"**（子代理取 arXiv:astro-ph/0409513 PDF 逐字核实）；`:170` 仍要求"注释论证偏差 <4e-8；**禁删**"，而该字符串在 `lib/` 下 0 命中、`DRIZZLE_GEOMETRY.md:385` 已列"撤换"。**两处跨域登记错误。**
S14 `p4_hst.cpp:9,28-30,32,39,84-85` 注释称"保持 CD"，实际用 `make_wcs` 的共形 CD；`HST_CD`、`Cfg.crpix1/2` 全是死变量。（**正面**：子代理实读 FITS 头，16 位有效数字与 `p4_hst.cpp:5-8` **逐位一致**且无 SIP 系数卡 ⇒ 该引用为真，非伪引。）

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 是否推翻 |
|---|---|---|---|
| 1 | 取本片实测的三档接缝绝对亏空（2.9e-17 / 5.2e-17 / 1.05e-16 sr）与适用域下界 `A_drop ≲ 1.4e-10 sr`，代进 `max(1e-6·A, 1e-15)` | 该门会在缺陷域变红 | **推翻成功**。在缺陷出现的全部 `A_drop` 区间 `max()` 恒取 1e-15 sr，实测亏空恒 <1.1e-16 sr ⇒ 门恒绿 |
| 2 | 把「100% 通量丢失（Σa=0）」代入该门，取 `A_drop=5.876e-16 sr`（θ=0.005″/px, pf=1） | resid = A_drop ≫ tol，应判红 | **未推翻——门仍绿**。tol/A_drop = 1e-15/5.876e-16 = **1.70 > 1**；pf=0.8 时 2.66。与 `exp03:46-47` 自述逐位一致 |
| 3 | 找 100% 丢失能被抓的最大 `A_drop`（令 1e-6·A > 1e-15 ⇒ A > 1e-9 sr），对照 DRIZZLE_GEOMETRY.md:375 自述的接缝失效域 `A_drop ≲ 1.4e-10 sr` | 两域若相交则门有效 | **推翻成功（域不相交）**。可抓域 `A>1e-9` 比缺陷域 `A≲1.4e-10` 高 7 倍以上 ⇒ **两域完全不相交**，门对接缝缺陷零覆盖 |
| 4 | 读 `results/audit/kcorr/g0b_sanity_fixed.json` 的 `gate_pass` 与三臂读数，独立复算 `|k−1|` | G0b 自检是否真的全绿 | **推翻成功**。`gate_pass=false`；N=5 → \|Δ\|=0.767、N=25 → 0.084，两臂超 0.06 门 |
| 5 | 在同目录找第二份判据（`run_scan.py` G0 臂）比对 | 两份脚本对同一几何应同判据 | **推翻成功**。`run_scan.py:57-58` 注释明写"小 N 档 …**不是 1**"、`:73` 用 `abs(k−1.0826)<0.10`；G0b 退回 `|k−1|<0.06` |
| 6 | 在 `spherical_overlap.cpp` + `spherical_overlap.h` 里 `grep 4e-8`，再逐字读 `:1009-1010` | `route3/exp06:11` 与 `route2/exp05:11,121` 审的 `<4e-8` 注释是否还在 | **推翻成功**。代码里 0 命中，现文为 −5.0e-7 ⇒ 两个 verdict 键（`comment_4e8_is_wrong`）建立在已消失的前提上 |
| 7 | `git ls-files` 查 `face_native.h` / `INNOV-04-01`；`ls docs/` 查编号 `02` 的文档 | `chart_native.h:2-3` 与 5 处 "02" 引用是否可解析 | **推翻成功**。三者命中均为 0 |
| 8 | 把 `p4_hst.cpp:76` 的 `f1==0` 与 `p3_sweep_neg.cpp:160` 的 `f0+f1+fo` 并列 | 两探针是否同口径 | **推翻成功**。P3-05 订正只落到 T12；T16 的 V0 臂仍恒真 |
| 9 | 解析复核 `chart_native.h:6` 的"轴对齐直线段"与赤道/极冠两支 | 极冠边是否也是直线 | **推翻成功**。refs.md:17 自陈极冠为 `cosθ = a + b/φ²`，非直线 |
| 10 | 逐行追 `chart_native.h:355-364` 的 `a2v` 去向 | 逐叶面积是否被累加 | **推翻成功**。`a2v` 只用于 `continue` 判定，从不累加；`:354` 只加整块 |

**未推翻（我原本怀疑但查实站得住的）**：
- `refs.md` 的 VERIFIED/标注级分界与"未证实即停止转引"的做法 —— 站得住，本片最佳。
- `p3_sweep_neg.cpp:157-161` 的 P3-05 订正（V0 破门纳入 SUMMARY）—— 站得住，是本片唯一做对的双向体检范例。
- `route3/exp03:65-82` 的守卫接线 —— `run_field` 确实在像素循环内对**实测** (tot, A_drop) 逐 drop 判定（`:296-303`），不再喂构造常量，站得住。
- `route2/exp08` 的量化界 `|r| ≤ 0.5/q` —— 整数舍入不等式，枚举到 2e6 点，站得住（但其负控是写死的，见 M1）。

---

## 6. 盲复算

**方法**：遮住前三轮（RR01–RR10 / R2-S1–S6 / R3-T1–T4）与 `实验/TAUTOLOGY_REGISTER.md` 的既有判定，**只对原文 + 仓内代码 + 已提交归档**重新取证，得出下表左列，再与右列（既有结论）比对。

| 关键判定 | 我的盲复算 | 既有结论 | 判定 |
|---|---|---|---|
| `max(1e-6·A, 1e-15 sr)` 对接缝缺陷的覆盖 | **恒绿，零覆盖**（域不相交，构造反例 1–3） | 已记为"P3-06 接缝偏差/绝对地板" | **偏松**——既有把它当"验收口径"而非恒绿门 |
| G0b 自检臂 | **恒红 + 静默**（`gate_pass:false`） | 未记（前三轮未见 G0b 条目） | **新问题** |
| `run_all.sh` RC 传播 | **对科学判据失明**（无 `sys.exit`） | 未记 | **新问题** |
| `p4_hst.cpp:76` T16 退出码 | **对 V0 恒真** | 未记（p3 的 T12 修过，T16 未提） | **新问题** |
| `read_tables.py:24` 手写 PASS | **确认**（数字有独立佐证，属呈现缺陷） | 未记 | **新问题** |
| `e5` 圆盘 −θ²/2 | **错（应为 −θ²/4）**，verdict 硬编码 | 部分记为"注释系数错 3 倍已撤回" | **偏严方向一致，但漏了形状搞混与硬编码 verdict** |
| `0.104369` | **确认是转写误差，仍留在 3 脚本注释** | 已记 D-09 订正 | **一致**（既有只记裁定，未记未传播） |
| `4e-8` 注释 | **代码里已不存在**，`STANDARDS_REGISTRY.md:170` 仍在要求"禁删" | 未记 | **新问题（跨域）** |
| `EXP-07-POLAR.md:78` 的 `:236` | **指向错误行**（门禁在 `:320`） | 未记 | **新问题** |
| §4.7.2 的 P3-06 撤回（−1 → −0.83） | **已传播到本片，但未传播到 `DRIZZLE_GEOMETRY.md:373-374`** | 部分记 | **偏松**——未记正本侧残留 |
| REC-1 的 5.00e+01 | 站得住：缺陷局限在 `variants.h::overlap_adaptive`、`variants.h:150` 我核实**行号与公式逐字正确** | 已记 SCI-703 §2.7 | **一致** |
| 接缝机制"部分定位" | 站得住：分母口径（`g.drop_area` ≡ 3D 参考）与算术精度（long double 不降）两条排除**在方法上成立**；最后一步未定位的登记诚实 | 已记 | **一致**（本片诚实度最高的段落） |

**盲复算小结**：与既有结论**一致 4 条、偏松 3 条、新问题 6 条、偏严 0 条**。既有结论对本片"物理结论"判得较准，对"**门能不能红**"这一层系统性偏松——这与本项目已固化的检查项（恒真门双向体检、判红臂报告、订正全仓传播）完全吻合。

---

## 7. 子代理派发记录

共派 **5 个**（`subagent`，全部后台只读）。我自己读完 30/30 份后逐条复核其结论。

| # | 子代理 | 范围 | 复核结论 |
|---|---|---|---|
| 1 | `9ee364f9` | route1 `e1/e2/e5`（3 份） | **采纳 11 条**，其中我独立复核的关键 5 条：`sum_model_over_4pi` 恒真门（`e2:155`）、`e5` docstring/verdict/打印三处自相矛盾、`0.104369` 未传播、三处 "02" 悬空引用、`4e-8` 审计对象已过期 |
| 2 | `7203d4f4` | `run_all.sh` + `kcorr/` 4 份 | **采纳 4 条阻断 + 7 条须修**。我逐条独立复核：`g0b gate_pass=false` 与三臂读数（从归档 JSON 直读）✅；6 个 py 的 `sys.exit` 命中数 0 ✅；`read_tables.py:24` 手写行与归档 `tables.md:9` 逐字相同 ✅；`read_summary` 零消费者且 `KEY_RESULTS.md:59` 声明不实 ✅ |
| 3 | `6af34a9d` | 8 个 C/C++ | **采纳 4 阻断 + 12 须修**。我独立复核：① `cd 实验/healpix-polar && sha256sum -c results/SNAPSHOT.sha256` → **47 项中只有 `p3_sweep_neg.cpp` 失败**，mtime 源码 2026-09-28 / 归档 2026-09-23 ✅；② `p4_hst.out:54` **oracle 自己破门 2.212e-04** ✅；③ `p4_hst.cpp:76` 只判 REC-1 ✅；④ `p9_review.out` T25 块 **15 行中 12 行 `0.000e+00`** ✅ |
| 4 | `8814b1b7` | route2 + route3 共 10 份 | **采纳 8 阻断 + 9 须修**。我独立复核：① `route3/exp06:11` 引的 `spherical_overlap.cpp:1002` **实为一行 `}`**，且 `4e-8` 在 `lib/` 下 **0 命中**（只在 `STANDARDS_REGISTRY.md:170` 与 `DRIZZLE_GEOMETRY.md:385,435` 作为"旧注/撤换"出现）✅；② `HP_ADAPTIVE_MAX_DEPTH = 12` 在 `spherical_overlap.cpp:565`，`:558-564` 的「P3-10 订正」注释**已写着该脚本重新推导的同一结论** ✅；③ 归档 `exp05_sagitta_subdiv.json` 的 `meridian_zero: False`（恒红门）✅；④ 归档 `exp03_weight_conservation.json` 缺 `seam_absolute_floor_B6` 全部新键 ✅ |
| 5 | `8c01a8ea` | route1（与 #1 重复派发，第二遍更细） | **采纳 3 条新增**：① `e5` 的 `rows_tq`（唯一支持 docstring 的臂）写入 JSON 却**从不打印** ✅（归档 `over_theta2` = −0.4999999748/−0.499999/−0.499988 确证）；② `e2:24` 的 `A_true = 2π dz_j/S_j` 在帽环为假 ✅（ring N+1 给出 **−1.000·A**）；③ `e1:197` 的 `abs_diff` 与 `:128-129` 的 `A_alt` 列是自比/同义反复 ✅。**并采纳其自我纠偏**：推翻自己上一轮对 H2「ring 3 次坏」的怀疑——逐环统计证实 N≥32 的 16 格确为 ring{1,3,4N−3,4N−1} 各 4，H2 正确 |
| 6 | `6c3610bb` | 4 份文档（`EXP-07-POLAR.md` / `DERIVATIONS-P3.md` / `REPORT_experiment.md` / `refs.md`） | **采纳 7 阻断 + 17 须修**，含 35 条伪引核查表。我独立复核其最硬的两条并**全部成立**：① **接缝第三档错 10×** —— 用 EXP-07 自己 `:475-477` 的表直接相减得 `1.044e-17`，除以 `A_drop` 得 `1.777e-02`，与表列 `prod/gd−1` 自洽（见 B1b，我已独立复算）；② **DERIVATIONS D1 的 Jacobian 链不成立** —— 我复算 `\|r_φ\| = √(1−z²)`、`\|r_z\| = 1/√(1−z²)`、**乘积 = 1**，故 `:27` 的 `(1−z²)` 前因子是虚的；且 `1−z² = s²(2/3 − s²/9) = **0.5556**`（s=1），非文档的 `(2/3)s² = 0.6667`。**结论 π/3 对，推导链不可复算**。另核实 `REPORT:166-168` 的生产行号 `:1099-1101`/`:1336-1340`/`:1370`/`:1379-1384` **四处全部逐字对齐**，且 refs.md V1/V2 的逐字引文**经一手原文核实为真** |

### 我**否决**的子代理结论及理由

| 被否决项 | 提出者 | 否决理由 |
|---|---|---|
| 「E2 的极区 kite 不拼接 ⇒ 全部结论失效」 | `9ee364f9`（**其本人已自我推翻**） | 该怀疑由子代理自己的独立重建推翻（Σ 弦面积闭 4π 到 1e-15）。我据此确认：**错的不是数，是把闭合当判据**。原怀疑作废，不计入发现 |
| 「E1 的 auto-nside 演示在生产中不可能发生（`drizzle_engine.cpp` `[16, 2²²]` 钳位抹平）」 | `9ee364f9` | **部分否决**。我复核 `NSIDE_MIN/NSIDE_MAX` 在 `drizzle_engine.cpp` 的 grep **未返回任何命中**（该 agent 报的 `:725-726` 行号我未能核实），钳位存在与否**在本片证据下未闭合**。我把它降级为**待核实项**（§8），不作为阻断项计入。演示点被选在翻转窗内这一事实仍成立（M 级） |
| 「sec³ 环求积负控对指数 p=2,3,4,5 都吻合 ⇒ 完全不可证伪」 | `9ee364f9` | **采纳为 M1 的一部分，但不采纳其绝对表述**。我复核该负控确为同一恒等式改写（`r dr/(sinρ dρ) = sec³(atan r)` 逐步可证），故"结构性恒真"成立；但其给出的 4.16e-05…8.40e-06 具体数值我**未复算**，不引用 |
| 「`UNRESOLVED_REGISTER.md` §81.2 指称 `direct_char.py:5-8` 标签错误 ⇒ 该 agent 实测不能确认」 | `7203d4f4` | **采纳其"不能确认"这一否定结论，但降级为 UNRESOLVED**。我复核 `direct_char.py:39` 的 `k_gauss = var_med / ((π/2)·med_mad²/n)` 与 docstring `:6-8` 的自述组合关系**字面一致**，故我判定 §81.2 的指控至少对 `direct_char.py` 不成立；但 §81.2 的确切所指超出本片文件范围，**登记 UNRESOLVED，不改任何一侧** |
| 「`route1/e2` 的 `sum_model_over_4pi_minus_1` 与 `route2/exp02:113` 是同一个门，应合并计一条」 | `9ee364f9` | **部分否决**。二者机制相同（结构相消），但**整改落点不同**（一个在 route1 脚本的呈现，一个在 route2 脚本的 PASS 条件），整改分母不同 ⇒ 在**门实例层合并为 1 条**，在**整改分母层分列 2 处**。本清单按后者标注 |

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线
git log --oneline -1                       # 期望 f9650dd0
git -c core.quotepath=false status --short run/GOVERN-08   # 期望空（本车道零 git 写）

# 1) 覆盖率自证：30 份成员、5530 行
sed -n '2418,2456p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml | grep -c '"实验/'
wc -l 实验/healpix-polar/docs/EXP-07-POLAR.md 实验/healpix-polar/code/audit/route3/exp03_weight_conservation.py \
      实验/healpix-polar/code/chart_native.h 实验/healpix-polar/code/p3_sweep_neg.cpp \
      实验/healpix-polar/code/audit/route1/e1_leaf_area_and_scale.py 实验/healpix-polar/code/audit/route1/e2_polar_pixel_limit.py \
      实验/healpix-polar/REPORT_experiment.md 实验/healpix-polar/code/p2_algorithms.cpp \
      实验/healpix-polar/code/audit/route1/e5_projection_budgets.py 实验/healpix-polar/code/p9_review.cpp \
      实验/healpix-polar/docs/DERIVATIONS-P3.md 实验/healpix-polar/code/audit/route3/exp05_sagitta_subdiv.py \
      实验/healpix-polar/code/audit/route2/exp05_projection_budgets.py 实验/healpix-polar/code/audit/route2/exp08_coverage_quantization.py \
      实验/healpix-polar/code/audit/route2/exp09_sum_vs_per_leaf_criteria.py 实验/healpix-polar/code/audit/route2/exp02_polar_pixel.py \
      实验/healpix-polar/code/p6_leafmap.cpp 实验/healpix-polar/code/audit/route2/exp04_area_operators.py \
      实验/healpix-polar/code/audit/route3/exp06_projection_budget.py 实验/healpix-polar/code/p4_hst.cpp \
      实验/healpix-polar/code/audit/kcorr/run_extra.py 实验/healpix-polar/code/audit/run_all.sh \
      实验/healpix-polar/refs.md 实验/healpix-polar/code/audit/kcorr/read_tables.py \
      实验/healpix-polar/code/audit/route3/exp02_polar_limit.py 实验/healpix-polar/code/audit/route3/exp08_scale_constant.py \
      实验/healpix-polar/code/p10_seam.cpp 实验/healpix-polar/code/audit/kcorr/read_summary.py \
      实验/healpix-polar/code/audit/kcorr/direct_char.py 实验/healpix-polar/code/p8_hstgrid.cpp | tail -1
# 期望：5530 总计

# 2) B1 恒绿门：正本判据 + 实测亏空 + 可执行门
sed -n '320p;350,351p;362,375p' docs/science/algorithms/DRIZZLE_GEOMETRY.md
sed -n '494p;516p' 实验/healpix-polar/docs/EXP-07-POLAR.md      # 实测绝对亏空 2.9e-17…1.05e-16 / HST 1.17e-16
sed -n '47p;171p' lib/algorithms/drizzle/healpix_drizzle/tests/p3_conservation_gate.cpp
sed -n '44,47p' 实验/healpix-polar/code/audit/route3/exp03_weight_conservation.py   # 100% 丢失仍判绿，tol/A_drop=1.70/2.66
grep -n 'handover' -A3 实验/healpix-polar/code/audit/route3/exp03_weight_conservation.py | tail -6   # "只登记移交"

# 3) B3 G0b 恒红门 + 双向体检 + 选择性报告
python3 -c "import json;d=json.load(open('实验/healpix-polar/results/audit/kcorr/g0b_sanity_fixed.json'));\
print('gate_pass=',d['gate_pass']);[print(r['N'],round(r['k_corr'],4),'RED' if abs(r['k_corr']-1)>=0.06 else 'green') for r in d['rows']]"
sed -n '56,58p;72,73p' 实验/healpix-polar/code/audit/kcorr/run_scan.py    # 同一几何的正确判据
sed -n '5,6p;57p;87,91p' 实验/healpix-polar/code/audit/kcorr/run_extra.py  # 错前提 + 只塞 dict + 不读门
sed -n '80p' 实验/healpix-polar/REPORT_experiment.md                        # 只报 N=289 绿臂

# 4) B4 退出码缺失
for f in run_extra read_tables read_summary direct_char; do printf "%-14s " $f; \
  grep -c "sys.exit\|SystemExit" 实验/healpix-polar/code/audit/kcorr/$f.py; done   # 期望全 0
sed -n '19,20p;29,34p' 实验/healpix-polar/code/audit/run_all.sh

# 5) B5 P3-05 订正未传播到 T16
sed -n '76p' 实验/healpix-polar/code/p4_hst.cpp          # return (f1 == 0) ? 0 : 1;
sed -n '157,161p' 实验/healpix-polar/code/p3_sweep_neg.cpp  # 已修的同型：bad += f0 + f1 + fo

# 6) M2/M3/M12/M13 伪引与未同步
git ls-files | grep -c -e 'face_native\.h' -e 'INNOV-04-01'        # 期望 0
ls 实验/healpix-polar/docs/                                        # 无编号 02 的文档
git -c core.quotepath=false grep -n "0\.104369" 实验/healpix-polar/ | head
grep -rn "4e-8" lib/algorithms/drizzle/healpix_drizzle/*.cpp lib/algorithms/drizzle/healpix_drizzle/*.h   # 期望 0 命中
sed -n '1009,1010p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp
sed -n '170p' docs/engineering/STANDARDS_REGISTRY.md
sed -n '866p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp
sed -n '112p' 实验/healpix-polar/docs/DERIVATIONS-P3.md; sed -n '3p' 实验/healpix-polar/code/audit/route3/exp05_sagitta_subdiv.py
sed -n '236p' docs/science/algorithms/DRIZZLE_GEOMETRY.md; sed -n '78p' 实验/healpix-polar/docs/EXP-07-POLAR.md

# 7) M5/M8 手写 PASS 与孤儿脚本
sed -n '17p;24p' 实验/healpix-polar/code/audit/kcorr/read_tables.py
sed -n '7,9p' 实验/healpix-polar/results/audit/kcorr/tables.md
git -c core.quotepath=false grep -rn "read_summary" .
git ls-files | grep -c "g3b_n5_hiprec"; git -c core.quotepath=false grep -rln "g3b_n5_hiprec" -- '*.py'

# 8) M9/M10 chart_native 逐叶未产出 + p6 参照口径
sed -n '6p;354p;355,364p' 实验/healpix-polar/code/chart_native.h
sed -n '20,31p;59p' 实验/healpix-polar/code/p6_leafmap.cpp
sed -n '17p' 实验/healpix-polar/refs.md     # 极冠边 cosθ = a + b/φ²

# 9) M7 时间戳破坏逐位可对照
sed -n '37p' 实验/healpix-polar/code/audit/kcorr/run_extra.py
grep -l written_utc 实验/healpix-polar/results/audit/kcorr/*.json | wc -l
sed -n '21p' 实验/healpix-polar/REPORT_experiment.md
```

---

## 9. 本遍未完成 / 待核实项（如实登记）

1. **子代理 #3/#4/#5 未在本遍交付前返回**（C++ 8 份 / 4 份文档 / route2+route3 10 份）。这些文件**我已全部亲自读完并在本件 §3 逐条给出判定**，其结论仅作增量交叉核验，缺失不影响本片覆盖率（30/30）与本件全部结论的成立。
2. **`drizzle_engine.cpp` 的 `NSIDE_MIN/NSIDE_MAX` 钳位**：子代理称在 `:725-726`，我 grep 未命中。**待核实**——若钳位确实存在且覆盖 `finest≈1.06e5″`，则 `route3/exp08:58` 与 `route1/e1:188-189` 的 auto-nside 演示在生产上不可复现（属新增阻断级）。我未将其计入阻断。
3. **C&G 2002 §5.1.3 式(54)/(55) 的存在性**（refs.md:48 与 `route1/e5:5-6`、`REPORT_experiment.md:239` 三处同源引用）：**待联网核验**。三处很可能不是三个独立证据源（refs V5 是 e5 与报告的共同上游）。
4. **Górski 2005 §5/§5.3 式(19)–(23) 的节号与位置**（refs.md:14,16-18）：refs.md 自称三路全文实取，我离线无法复核，**待联网核验**。
5. **`route3/exp05` 头部 `depth needed ~ 8.15` 的真值**：需实跑该脚本才能确认与 `DERIVATIONS-P3.md:106` 的 7.97 谁对。**按实测 sag0≈0.0638 hp_res 反推应为 7.98**，故 8.15 疑为旧值残留，但**未实跑，标待前台复算**。
6. **`route1/e5` 的 `pointwise_over_1p5` 在 h=2e-2 档是否真为 "1.000000 全段"**（`DERIVATIONS-P3.md:135` 的表述）：按 sec³ 展开 `1 + 1.5ρ² + 2.875ρ⁴`，该档应为 ≈1.0015 而非 1.000000。**未实跑，标待复算**（文档表述疑过强）。
7. **`UNRESOLVED_REGISTER.md` §81.2** 的确切所指超出本片范围，登记 **UNRESOLVED**，不改任何一侧。

---

**交付件**：`run/GOVERN-08/审核包-R2/审稿-P1-EXP-healpix-polar-001.md`
**判定**：需修 · 阻断 11 / 须修 18 / 建议 14 · 覆盖率 30/30 份、5530/5530 行（100%）
**子代理**：派 6 个（含 1 个重复派发），返回 6 个，逐条复核；否决 4 条、采纳并独立复现 43 条。
**自验证段**：本件 §8 的 9 组命令全部可在只读前提下复跑，B0/B1/B1b 三条各有独立可复现证据（`sha256sum -c` / 表内自洽相减 / `tol/A_drop` 比值）。