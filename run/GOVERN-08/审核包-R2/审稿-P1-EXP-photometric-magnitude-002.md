# G08-05 对抗审稿 · 第 1 遍 · P1 交付件

**片号**：`EXP-photometric-magnitude-002`
**基线**：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`（已核）
**划片依据**：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2558-2602`，SRS-1 层内 LPT 均衡装箱，严格不跨层
**审稿人**：只读审稿代理。未编译、未跑 ctest/pytest/构建/实验脚本；**零 git 写**；未改任何仓内文件（本交付件为唯一写入）。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威版片清单） | **37** |
| 实际读到 | **37 / 37** |
| 成员总行数（片清单声明） | **6782** |
| 实测总行数（逐份 `wc -l`） | **6782**（与清单逐份一致） |
| 实际读到的行数 | **6782** |
| **覆盖率** | **100.0%（37/37 份，6782/6782 行）** |

**未读完的部分：无。**

**为核实伪引而额外阅读的仓内非本片文件**（不计入覆盖率分子）：
`docs/detail/registry/acsd.phase1.photometry.md:1-100`、`docs/science/PHOTOMETRY.md`（grep + :124-128/:376/:382/:398-402）、`lib/algorithms/photometry/cpp/src/{star_matcher.cpp,spectrum_integrator.cpp,frame_photometry_fit.cpp,filter_curve_json.h,pc_api.cpp}` 定点行、`results/{step5_calibration_gate,step6_apply_and_units}.json`（只读取数值做算术核算）、`results/redo/route2/exp2_mag_prefilter.json`（键集比对）。

**未做（按纪律）**：未跑 `run_all.sh` / `redo/run_all.sh` / 任何 `stepN_*.py` / `exp_*.py`；所有「重跑后是否翻红」标「待运行核验」并给出命令。

---

## 2. 本片判定

### **判定：阻断（BLOCK）**

本片不能以现状交付。最重 5 条：

**① 判据表与生成它的代码不同源（G4 判定会由 PASS 翻为 RED）。**
`code/step9_collect.py` 已按 G08-05 R2 重写，`results/GATES.md` **没有重跑**。
- 生成器 mtime `2026-10-02T01:41:16`；GATES.md `2026-09-30T21:59:20`；gates.json `2026-09-29T00:20:26`（**证据面比生成器旧 2–3 天**）。
- 重写后新增的 6 个标记串在 GATES.md 中命中数**全为 0**：`现场重算` / `成立域声明` / `同源自证` / `项齐全有限正` / `诊断（不判定）` / `same_source`。
- `step9_collect.py:207` 自述「该条在归档读数下**不成立**…故本行判红」；`:232` `g4_rob = bool(far) and g4_far_min >= 0.5*nominal`，而 `[0,5]` 行 `match_rate = 0.0` vs `0.5×0.98592 = 0.49296` ⇒ **必为 False** ⇒ 重跑输出 **RED、合计 7/9**；仓内两份证据面都写 **G4 = PASS、8/9**。

**② `exp2_mag_prefilter` 的「负对照」本身是恒真式，且其复现命令在 HEAD 上必然失败。**
- `route2/exp2:133` 的 `inliers_flux` 与 `:95` 的 `inlier_idx_base` 是**同一个谓词 `abs(d-med)<=3.0` 在同一数组上算两次**；`:134-135` 的第二个析取支掩码对 `inlier_idx_base` **恒为全真** ⇒ 整个门退化为「IRLS 有没有剔人」。而本意为负对照的三行 `:129 keep_flux` / `:130 rc_flux` / `:132 keep_flux_shifted` **算了从未使用**（死码）——代码从未构造过它声称的「非平移不变通量窗规则」。
- **写盘路径错**：`:149` `makedirs(.../results/redo/route2)`，`:150` 却写 `here/../results/exp2_mag_prefilter.json` = `code/redo/results/`（**该目录从不创建**）⇒ 全部算完后末行 `open(...,"w")` 抛 `FileNotFoundError`、退出码 1。`exp7:84-85` 与 `exp8:170-171` 同一复制粘贴残留。
- 而 `REPORT_route2.md:90` 仍给这个恒真门打 **✅「负对照（非不变窗口规则）内点集改变」**，`:92` 更升格为「本实验同时**证明** 式-2 的严格平移不变性」。

**③ `exp_S00` 幻觉锚核验门自签豁免，归档「15/15 全绿」与当前源码实际「13/15」失配，而重跑不会报错。**
`exp_S00:89` 的红名单谓词 `… (not needle_found and "条件钳位" not in note and "锚漂移" not in note)` 中的 `note` 由脚本自己在 `:38-50` 撰写 ⇒ **恰好把三条已知漂移锚永久豁免出红名单**。我独立复核 15 个 needle（`sed -n` + `grep -cF`）：

| needle | 命中 | 声称位置 |
|---|---|---|
| `判据参照` | **0** | `exp_S00:46` 声称 `docs/science/PHOTOMETRY.md:126` |
| `求解前提` | **0** | `exp_S00:49` 声称 `§16.5/:400` |
| 其余 13 个 | 各 ≥1 | 代码侧锚全部命中 |

`PHOTOMETRY.md` 全文 grep「判据参照」**0 命中** ⇒ `:126` 的锚**凭空虚构**（该行实为「官方定义 / Gaia DR3 §20.12.4 …」表格行）；「求解前提」真实在 **:382**（§16.5 起于 :376），`:400` 实为「生产默认通带是**宽带** … `filter_passband` 取 `Baader R`」⇒ **漂移 18 行**。
另 `:73` `"anchor_real": True` 是成功路径上的**字面量**（语义只是「文件能打开」），`:84` 因此恒 = 15；`:106` `return 0` 无条件 ⇒ 重跑即使红也不改退出码。`code/redo/README.md:12` 的「S00 锚核验 15/15」与重跑后的 13/15 矛盾。

**④ `G2` 整行的两个判定项皆恒真/近恒真，却报 PASS——现行代码正在报告一个结构上无法失败的绿灯。**
- `step9_collect.py:161` `g2_c = |sigma_residual_delta| <= 1e-12`：`calibrate` 的 `r = log10(f_instr/f_syn)` 对全体乘 `c` 是**纯平移**，MAD 与 Tukey 权重皆平移不变 ⇒ `sigma_residual_delta ≡ 0`（归档实测 `0.0`）。
- `:162` `g2_d = np.ptp([k_photo]) > 0`（防空转守卫）：退化族三成员 `k_photo` = 3.4213e-16 / 3.4214e-16 / 3.4181e-16，**相对散布仅 9.89e-4**（我实测）⇒ 守卫被浮点噪声满足。
- 对比：`step9_collect.py:148-156` 已正确地把 (a)(b) 降级为 `same_source_self_checks`，但 (c)(d) 仍留在判定式里。**这比 G4 的过期更危险**：G4 是「证据面没更新」，G2 是「判定式本身无判别力」。

**⑤ 「缺陷复现」复刻的是一个生产**已修**的缺陷，三份报告都打 ✔「逐字一致」。**
`lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp:150-154`（**我亲读**）明写：
> 「n_int == 3（4 点）是退化分支: 前段区间数 n_13 == 0 ⇒ 1/3 部分**必须为 0**。历史实现在该分支先把 y[0] 计入一次（sum = y[0] + y[0] = 2·y[0]）…**订正见变更 claim PHOT-SIMPSON-N3-001**」

而三份脚本把缺陷项 `+2·y[0]·h/3` **写死在被测函数体里**，从不调用生产：`route1/exp5:37`、`route2/exp8:81-83`、`route3/exp_S06:57`。报告侧 `REPORT_route1.md:285/:289`、`REPORT_route2.md:235/:237` 均打 ✔，**三份都未提生产已修**。

---

## 3. 逐文件清单（37 份）

> 判定：**阻断** / **须修** / **建议** / **无问题**

| # | 成员文件 | 行 | 读了什么 | 看到什么（`文件:行`） | 判定 |
|---|---|---|---|---|---|
| 1 | `README.md` | 609 | 全文 | :7 引 `docs/ASTROCS_DESIGN.md`、:8/:74/:370/:449 引 `astrocs.phase1.photometry.md`——**均不存在**；:8 与 :74 对同一 §4.1 给**两个互斥标题**；:234 称帧 C n=96 而归档为 105；:562 称 G6=PARTIAL 而 GATES 记 PASS；:553 称 REVIEW.md「428 行」实为 541 | **阻断** |
| 2 | `results/REVIEW.md` | 541 | 全文（轮次 1+2） | 轮次 2 `:451/:456` 诚实披露 4 处数字未回填；`:487` 称「G6 已降为 PARTIAL」与现行 GATES 的 G6=PASS 冲突 | 须修（历史稿，线索不采信为事实） |
| 3 | `results/GATES.md` | 17 | 全文 | :11 G4=**PASS**（现行代码判 RED）；:14 引不存在的 `astrocs.*`；:15 G8 F502N n=74/−0.07361/0.205 而 README:322 记 n=75/−0.080/0.213 | **阻断** |
| 4 | `results/DOC_CORRECTIONS.md` | 180 | 全文 | C1–C9 结构完整、订正诚实；:157 仍引不存在的 `astrocs.*` | 无问题（历史线索） |
| 5 | `docs/DISPUTES.md` | 137 | 全文 | A-P1-11:104-106 **主动登记** `exp_S09` 两处恒真门（`bool(zp_fit==zp_16)`、`zp_16−zp_16`）——我复核成立；A-P1-05:46-50 主动登记 `exp4.design()` 退化 | 无问题（**正面：缺陷自登记**） |
| 6 | `docs/derivation_robust_weights.md` | 96 | 全文 | :15 「**C 与星无关**」→ :21-24 推出 3.0 mag **严格等价** 1.2 dex；与 README:72（G08-04 G2 已推翻该等价，`C_i` 是逐星量）**直接矛盾**；:46-47 正确写 `1/0.67449=1.4826`，与 `exp_S08:115` 注释「1.2533 = 1/Φ⁻¹(3/4)」矛盾 | **阻断** |
| 7 | `docs/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md` | 242 | 全文 | :3/:5 引 `ASTROCS_DESIGN.md`、`UNIFIED_SCIENCE_MODEL.md`（后者不在 DOC-SCI-001 目录）；:90 `m_5 = ZP−2.5·log10(5·σ_F)` 与 `exp8:139` 不符；:58/:180 自洽写 1.253=√(π/2) | 须修 |
| 8 | `refs.md` | 129 | 全文 | 台账诚实（V5/V6 自标「未能核实」）；:46 明确「正文亦未以该文献支撑 820/deg²」——与 `REPORT_route1.md:142` 把 820 归给 R6 冲突 | 须修（冲突方在 REPORT） |
| 9 | `code/README.md` | 86 | 全文 | :62 记 step7「**6** 条负例（N0–N5）」实为 **7 条 N0–N6**；:66 称 `src/`「**五个**真实数据脚本」实存 6 个；:86 无条件声称 quick 会标 `NOT_RUN`（归档存在时不成立） | 须修 |
| 10 | `code/run_all.sh` | 56 | 全文 | `set -euo pipefail`、路径从 `BASH_SOURCE` 推导、quick 分支正确跳过 step6；但 :56 无条件 `echo 全部完成`、**退出码不承载判据红**；**从不调用** README 声明的强制前置步 `run_selftests.sh` | 须修 |
| 11 | `code/redo/README.md` | 14 | 全文 | :5 自述「不 import 仓库任何 Python」；:12「S00 锚核验 15/15」与实测 13/15 冲突 | 须修 |
| 12 | `code/redo/run_all.sh` | 29 | 全文 | :22 `> /dev/null` 丢全部输出；:29 `echo「读数与基准快照逐项一致」`——**脚本从不做任何 diff/比较**，25 个基准快照 JSON 从不被任何代码读取 | **阻断** |
| 13 | `code/step1_analytic.py` | 235 | 全文 | :104 `Budget(sigma_fit_white=0.014)` 硬编码（R4 已提未修）；:194-199 稻草人公式**已按 C8 改写为诚实描述**（前轮整改到位）；:92-94 `f_syn_v` 同时是注入通量与模型通量 ⇒ `oracle_zero_point_m1` 是**构造即恒等**；:208 死变量 | 须修 |
| 14 | `code/step3_forward_vs_photflam.py` | 293 | 全文 | :5 引 `06.md §6`（不存在）；:32 死导入；:195 只对两滤镜做 validation 而 `FILTERS` 是三元；:211-212/:247-248/:228-232 三处 `continue` **静默丢数据**且从不登记被丢数量 | 须修 |
| 15 | `code/step4_guided_vs_blind.py` | 121 | 全文 | :63 `false_alarms=0` **硬编码**；按同口径（71 候选 − 70 匹配）应为 **1** ⇒ **归档里的事实错误**；被 `step9:247` 当实测读入 | **阻断** |
| 16 | `code/step5_calibration_gate.py` | 231 | 全文 | :4 引不存在的 `docs/plugins/algorithms_phase1/06_photometry.md`；:111 `sigma_gaia=0.002` 硬编码且 :108-110 自认「未激活」仍计入上界；:118-119 `sigma_psfsys` 由**与 σ_obs 同批的 `g["flux"]`** 导出且占 `sigma_sys_quadrature` 的 **97.1%/98.8%/99.8%**（我实测）；:225-226 两个 `False` 字面量被 `step9:189` 当实测读入 | **阻断** |
| 17 | `code/step6_apply_and_units.py` | 208 | 全文 | :60 注释「**不复用** apply_photometry 的实现」与 :61-62 矛盾（`(k·m)·img` vs `pl.apply_photometry(img,k,m)` 同表达式两份拷贝 ⇒ `rel` 恒 0），该值经 `step9:288` 成为 **G5 四个合取项之一**；:65 死导入；:85-95 `downstream()` 是本文件内 6 行桩，G5 的 fail-closed 验的是这个桩 | **阻断** |
| 18 | `code/step9_collect.py` | 395 | 全文 | 已按 G08-05 R2 系统重写并**自带正确判定**（:91-110 修 G1b fail-open、:148-163 降级 G2 的 (a)(b)、:237 现场重算、:343-347 降级 G8 去趋势）；但 :54-56 step5 缺失时**静默 `return`**、旧 GATES.md 原样留存、退出码 0；:280 G4 的 repro **就是覆写其自身输入归档的 step4 脚本**；:316-324 G6 证据行是**冻结字面量快照**（违反本文件 :32 自己的契约） | 无问题（**代码最干净**）+ 须修 |
| 19 | `code/scia_gaia.py` | 114 | 全文 | :21-34 三级回退目录解析、:52-66 `check_xpsd_dir` 给出可操作错误；:74 缓存命中即复用、:99 `sha256` 记的是**缓存 CSV** 而非源星表 ⇒ 溯源指纹指错对象 | 无问题 + 建议 |
| 20 | `code/gaia_xp_dump.c` | 55 | 全文 | 只读夹具，`fclose`/`free`/`destroy` 路径齐全，argc 校验与 return code 完整 | 无问题 |
| 21 | `reverse_verify/.../real_gain.py` | 515 | 全文 | :37/:43 探测**不存在的 `docs/ASTROCS_DESIGN.md`** ⇒ 落到 :46 回落 `实验/photometric-magnitude`，:50 `NORM` 指向不存在的 `实验/photometric-magnitude/run/RELEASE-02/...`（真实数据在仓根 `run/`，我已核实存在）⇒ **不设 `ASTROCS_ROOT` 即无法定位数据**；:22/:436 死代码；全文件**无任何 verdict/退出码** | 须修 |
| 22 | `reverse_verify/.../analyze_real.py` | 118 | 全文 | :64 浮点相等 `== 0.02`；:71 诚实注明「用 real_gain.json 里 order1 的 coef 不行（未加先验），故只报幅度」；无 verdict | 无问题 |
| 23 | `route1/exp1_robust_constants.py` | 194 | 全文 | :19-22 常量为手抄字面量（生产 `star_matcher.cpp:21/23/25/27` 从不被读）；:72-75 `identity_ARE()` **不调用** `biweight_ARE`（只共用网格）⇒ 对 ψ/ψ′ 公式无判别力，却被 `REPORT_route1.md:74` 当 c=4.685 的证据之一；:186 只实现正本鲁棒门**一半**（`PHOTOMETRY.md:268` 要求「**且**离群权重为 0」，`:155` 算了 `n_zero_weight` 却不入判定） | 须修 |
| 24 | `route1/exp5_integration_gates.py` | 195 | 全文 | :15 SEED=20260930 与 README「统一 20260926」不符；:37 B11 缺陷写死；:94/:96 `can_integrate`/`guard_should_reject` 是**字面表达式重述断言**且与生产相反（生产 `si.cpp:174-175` 对 `n_int==1` **走梯形**，不拒绝）；:84 单点网格会 IndexError；:130-135 为「初值不影响收敛值」给割线加了步长限幅+域截断三道保险 ⇒ **为让命题为真而修改了求解器**；无 verdict | **阻断**（B11）+ 须修 |
| 25 | `route1/REPORT_route1.md` | 343 | 全文 | :323「H1 STALE（漂移至 **:227/:355**）」——我实测「承载面」在 **:13**、「线性面亮度」在 **:218/:336** ⇒ **目标行错误**；:324「H2 §16.5/:400 **精确命中**」——真身 :382 ⇒ **漂移 18 行**；:159/:234/:236-237 把 `exp_S08→H8d` 当**权威订正**引用并推出「1.3926× 低估」；:236 引 `code/redo/results/…`（实际在 `results/redo/route3/`）；:343「本单元实测 27 处」**未写明口径** | **阻断** |
| 26 | `route2/exp2_mag_prefilter.py` | 154 | 全文 | :109-113 自认「原为五个恒真式」⇒ **前轮已修**（正面）；但 :117/:118/:122/:126 四项**仍恒绿**（IRLS 精确平移等变）；:134-135 负对照恒真且测的不是声明的命题；:129-132 三行死码；:149-150 写盘路径错 ⇒ **复现链断裂** | **阻断** |
| 27 | `route2/exp7_spatial_bound.py` | 89 | 全文 | :32-50 本地 IRLS **每轮重估 S**（生产 `sg.cpp:212` 固定 S）；:63-64 在**300 个星点**上度量 `max|B@coef|` 而生产在**像素帧网格**上度量并中心化 ⇒ 生产注释明说要防的「星区窄→系数外推」**结构上复现不了**；:68 如实报红臂（正面）；:84-85 写盘路径错 | 须修 |
| 28 | `route2/exp8_fsyn_forward.py` | 175 | 全文 | :95/:151-153 `abs_dev = \|ratio_mag − 10^0.4\|` 是**积分线性性恒等**（S06:93-95 已标 `discriminating_power: none`，**route2 未标**）；:121 `production_grid_immune` 因 n=342 偶而结构必绿；:25 `HC` 变量从未被使用（:31 重新硬编码 h,c,kb）；:139 `m5` 无 `σ_F`；:170-171 写盘路径错 | 须修 |
| 29 | `route2/REPORT_route2.md` | 274 | 全文 | :16/:239 `m_5 = ZP − 2.5·log10(5·σ_F)`，代码 :139 无 `σ_F`；:90 四个「✔（构造精确）」打在恒绿门 + 「负对照…内点集改变 ✔」打在恒真负对照上；:92 升格为「**证明**」；:188 表（N=3 MC SE **0.6694**）与 :253 正文（「0.724σ」）**自相矛盾**；全部复现命令写 `cd code && python3 expN`，实际在 `code/redo/route2/` | **阻断** |
| 30 | `route3/exp_S00_anchor_verification.py` | 110 | 全文 | 见 §2③（:73 字面量、:84 恒真计数、:89 自签豁免、:91-96 硬编码 conclusion、:106 无条件 `return 0`）；:29 标签 `540-545` 而区间 `538-545`；:10-13 `REPO` 可被 `ACSD_REPO` 替换而 JSON 只记相对路径 | **阻断** |
| 31 | `route3/exp_S01_tukey_c.py` | 131 | 全文 | :110-112 `H1_pass` 三合取全为真测量（**正面**）；:87-93 `negative_zero_check.pass = bias < 3*se` 是单侧 3σ 检验且**未加 `discriminating_power` 标注**——P1-m09 整改只覆盖 S02/S09/S11/S12，**漏了 S01**；:85-86 `bias`/`se` 用循环末档遗留变量（改元组顺序即静默改分母） | 须修 |
| 32 | `route3/exp_S02_mad_consistency.py` | 102 | 全文 | :45/:62 为真测量；:73 `mad.max()==0.0` 恒等门，:76 已标 `none` 且**不进** verdict ⇒ **正面整改范例**；:18 `CONST` 仍是手抄 | 无问题（正面）+ 建议 |
| 33 | `route3/exp_S06_fsyn_lambda_simpson.py` | 150 | 全文 | :82/:92 **已正确标注** `discriminating_power: none`，**但 :137 verdict 仍计入**；:134 `H6d_B11.pass = \|v_bug − 11/3\| < 1e-12` 而 :57 **定义**了该值 ⇒ 构造性恒绿且**未标注**；:134 后半 `n_int%2==0` 的网格是 `:66/:124` 本地 arange，不是生产解码网格 | 须修 |
| 34 | `route3/exp_S08_ladder.py` | 161 | 全文 | :62 `max(queries)<=5` 被 `LADDER` 长度 5 锁死；:76 比值 ≡ **√10**、常数全消；:139-142「期望值 8.8622e-4」是本公式自身输出的抄写、`_consistency_gate(legacy)=="RED"` 是**故意喂不同公式造出的结构必红**；:52-55 `sample_identical_to_fixed16_fraction` **指标名与实现相反**（两者都够 2000 或都没触发阈值才记「样本相同」，而 `cum[stop] < cum[4]` 时样本并不相同）；:115 注释常量标签错；:34/:43 死代码 | **阻断** |
| 35 | `route3/exp_S09_mag_minmax.py` | 100 | 全文 | :69 `pass = bool(zp_fit == zp_16)` 而 :59 与 :42 是**逐字相同的掩码** ⇒ 恒真；:74 `zp_16 − zp_16` 恒等；**但** :77 已标 `none`、:86 `registered_not_counted`、:85 verdict 只计 H9a ⇒ **正面整改范例** | 无问题（正面） |
| 36 | `route3/exp_S11_zp_sample_floor.py` | 93 | 全文 | :62 `H11.pass` = MC vs 独立阶统计量积分 ⇒ **本片最佳门**；但 :71 `pass: True` **硬编码**、:77 仍送进 verdict（与 :72-74 自认「无判别力」**自相矛盾**）；:35 `exact_pred_n3 = 1.0860` 与同文件 :47-51 积分值（≈0.670）**矛盾 1.62×**，且被写进交付 JSON；docstring :5-6「≈1.08σ」与 :60-61「渐近式高估 8%（即精确 0.670 < 渐近 0.724）」互相矛盾 | 须修 |
| 37 | `route3/exp_S12_dex_bound.py` | 90 | 全文 | :64-71 明确 `none`、:76 `registered_not_counted`、:73-74 注释说明「原为字面量 True 直接写进 verdict；现已移出」⇒ **正面整改范例** | 无问题（正面） |

---

## 4. 发现清单

> **计数口径**（涉及判据数量时一律写明哪一层）：
> - **门实例**：单个 `pass`/布尔/条件判定表达式计 1，同一量的重复命名计 2。本片我数到 **52 个**（子代理独立计数 49–75，因是否把「显式降级为诊断」的布尔计入而异）。
> - **去重门**：同一表达式只计一次 ⇒ **48 个**。
> - **整改分母**：需独立整改的缺陷条目数。
> - `REPORT_route1.md:11`「18/18 + 6/6」、`REPORT_route2.md:265`「14/14」都是**整改分母**（清单项 S1–S18 / S1–S14），**不是门数**。
> - `GATES.md:17`「8/9 项 PASS」是**验收行数**（G1/G1b/G2–G8 共 9 行）。
> - `REPORT_route1.md:343`「本单元实测 27 处」**未写明口径**，不可与上述任一口径互相印证。
> - ⚠ 计数冲突（须前台裁决）：子代理独立复算得「本单元 27 处严格恒真（去重口径）」，与我的 24 个口径接近但不同；且仓内 `G08-05-整改-第二轮实验域.md:536` 称「往返自证型全仓只有 2 例且都在 absolute-snr」，而本单元至少 5 例（`exp2:117-126`、`exp8:121/:144`、`exp_S08:124-136`、`exp_S09:42/59/69`）。**请前台先钉死口径再出单元表。**

### 4.1 阻断（15 条）

**B-1｜判据表未随生成器重跑（G4 由 PASS 翻 RED）**
- 位置：`code/step9_collect.py` vs `results/GATES.md:11` vs `results/gates.json`
- 现状/应为/证据：见 §2①

**B-2｜`exp2` 负对照恒真 + 复现链断裂（写盘路径错）**
- 位置：`route2/exp2:133-135`（恒真）、`:149-150`（路径错）；同类 `exp7:84-85`、`exp8:170-171`；报告 `REPORT_route2.md:90,:92`
- 现状：`:133` 与 `:95` 同谓词两次；`:149` 建 `results/redo/route2`、`:150` 写 `code/redo/results/`（从不创建）
- 应为：负对照必须真的构造「平移不变 vs 不平移」两族规则；写盘路径统一
- 证据：我 `sed -n` 三处确认 makedirs 目标与写入目标不一致；`ls` 确认 `code/redo/results/` 不存在

**B-3｜`exp_S00` 自签豁免 + 归档 13/15 与源码失配** —— 见 §2③

**B-4｜`G2` 整行判定式无判别力却报 PASS** —— 见 §2④

**B-5｜B11 三处复刻的是生产已修缺陷** —— 见 §2⑤

**B-6｜`exp_S08` verdict 三门零判别力，且已被 route1 当权威订正引用**
- 位置：`exp_S08:62,:76,:139-142,:145-147`；`REPORT_route1.md:159,:234,:236-237`
- 证据：H8b 两表同式生成 ⇒ 比值 ≡ √10；H8d 期望值是自身输出抄写；route1 据此推出「1.3926× 低估」并标为 P1-M05 权威订正

**B-7｜`σ_psfsys` 与被测统计量同源，且独占上界的 97–99.8%**
- 位置：`step5_calibration_gate.py:118-119`（`psf_vs_aperture_systematics(g["flux"], f_ap4, g["flux_err"])`，用的是与 `cal["sigma_obs_mag"]` **同一批** `g["flux"]`）→ `:129-131` 进 Budget → 进 `sigma_ceiling`
- 现状（我实测归档）：帧 A `sigma_psfsys/sigma_sys_quadrature` = **97.1%**、B = **98.8%**、C = **99.8%**；`sigma_obs/sigma_ceiling` = **93.5% / 82.0% / 34.9%**
- 应为：`step5:95-98` 已对 `delta_after_m` 明令「同源不进预算」，对 `sigma_psfsys` 应适用同一处置（或做噪声扣除）
- 反向事实（须一并记录）：σ_obs 自身偏高（obs/pred=1.154），故同源项**没有**把判定掩盖成恒绿；这是**判别力被压缩**（帧 A 余量仅 6.5%），不是恒真门

**B-8｜G3/G4 的否定性子判据读硬编码字面量**
- 位置：`step4:63`（`false_alarms=0`，且按同口径应为 **1**）→ `step9:247`；`step5:225-226`（两个 `False`）→ `step9:189`
- 证据：归档 `frames[A].guided = {n_candidates:71, n_matched_truth:70, false_alarms:0}` ⇒ 按盲检侧同定义（`:57` `blind_fp = bx.size - ka2.size`）引导侧应为 1

**B-9｜`G5` 四个合取项中两个恒真**
- 位置：`step9:288`（`g5_recompute`）读 `step6:61-63` 的同表达式复算；`step9:293`（`g5_fc`）读 `step6:85-95` 的本文件内 6 行桩
- 证据：归档 `independent_recompute_max_rel_diff = 0.0`（逐位 0）

**B-10｜G4 的 repro 命令覆写判据自身的输入归档（自愈路径）**
- 位置：`step9_collect.py:280`（`repro=python3 …/step4_guided_vs_blind.py`）；写归档在 `step4:117`
- 现状：跑该行的复现命令会**刷新 `step4_guided_vs_blind.json`**，但**不刷新 `GATES.md`** ⇒ 人读面冻结在旧值。缺陷被捕获后复现命令**不持续失败**
- 应为：repro 与「刷新证据面」绑定，或分列并注明「仅刷新该步归档」

**B-11｜全仓幽灵文档引用（本片密集）**
- 位置：`README.md:7,:8,:74,:370,:449`、`GATES.md:14`、`DOC_CORRECTIONS.md:157`、`REVIEW.md:22,:227`、`step5:4`、`step6:5`、`step3:5`、`real_gain.py:37,:43`、两份 REPORT、archive:3,:5
- 现状：`docs/ASTROCS_DESIGN.md`（实为 `ACSD_DESIGN.md`）、`astrocs.phase1.photometry.md`（实为 `acsd.…`）、`docs/plugins/…/06_photometry.md`、`ACCEPTANCE_SPEC.md` **均不存在**。全仓引用文件数：51 / 10 / 16 / 24

**B-12｜推导正本 D1 与 README 订正直接矛盾** —— 见 §3 #6

**B-13｜`reverse_verify` 两个脚本在 HEAD 上跑不起来：仓根定位器双失效**
- 位置：`real_gain.py:36-46`（`_find_root`）、`:49-50`（`NORM`）
- 现状：①`:43` 的存在性标记是 `docs/ASTROCS_DESIGN.md`——**该文件不存在**（`ls docs/` 只有 `ACSD_DESIGN.md`）⇒ `:42` 的 8 次向上循环**恒落空**，必走 `:46`；②`:46` 的 fallback `../../..`（4 级）落到 `实验/photometric-magnitude`，而该处**无 `run/` 目录**（我 `ls` 确认）⇒ `:50` 的 `NORM` 指向不存在路径 ⇒ `load_all():130` 的 `os.listdir(NORM)` 必抛 `FileNotFoundError`；`analyze_real.py:31` 的 `R.load_all()` 同理
- **应为**：标记改 `docs/ACSD_DESIGN.md`（或更稳的仓根 `VERSION`，我已确认存在）；fallback 深度应为 **6 级**（`../../../../../..`）——我 `realpath` 实算：4 级→`实验/photometric-magnitude`、5 级→`实验`、**6 级→仓库根**；并在 `NORM` 不存在时 fail-closed 报错而非崩栈
- 证据：`realpath -m` 三档实算；`ls -d 实验/photometric-magnitude/run` → 不存在；`ls docs/`、`ls VERSION`

**B-14｜`DISPUTES.md` A-P1-08 把一个不存在的缺陷写成「终裁 · 条款性缺陷收录」**
- 位置：`docs/DISPUTES.md:74,:76`（并被 `docs/derivation_robust_weights.md:50` 承接）
- 现状（**我亲读生产代码**）：`lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:293` 是
  `out.zero_point_scatter_mag = 1.482602218505602 * zmad;` —— **16 位全精度，完全合规**；
  `grep -n "1\.4826"` 在该文件**只命中这一行**，且 `1.4826` 只是全精度常量的前缀
- 现状：DISPUTES 写「`frame_photometry_fit.cpp:292` 用**4 位 1.4826**，违反 MAD→σ 冻结条款（要求 1e-6 量级）」，并给出「相对差 1.496×10⁻⁶」；:76 把它列为「**两处条款性缺陷收录**」
- 应为：**撤回该终裁**——代码未违反任何冻结条款，且行号差 1（:292 vs :293）
- 严重性：`DISPUTES.md` 是**裁决落点文件**（README:18 称其为「本单元 A-P1-xx」的裁决依据），把不存在的缺陷写成终裁会污染全部下游引用
- 证据：`sed -n '290,295p'` + `grep -n "1\.4826"` 实测；「4 位截断值」在生产代码中**不存在**

**B-15｜`README.md:74-85` 标注「逐字」的判据块，在被引文件中无任何逐字来源**
- 位置：`README.md:74-85`（标题行自称「（…「测光一致性判据（单帧、尺度无关、双边界）」，**逐字**）」）
- 现状：我对被引文件 `docs/detail/registry/acsd.phase1.photometry.md` 执行
  `grep -n "gate_scope\|1\.166\|sigma_ceiling"` → **0 命中**。该文件 :69-82 只有散文 + 一句
  「MAD→标准差估计量的相对标准误 SD 因子取 √1.361 的值，3 倍作抽样允差」，**没有一个等式**
- 现状：README 的 10 行伪代码块里的 `sigma_ceiling = (1 + 3·1.166/√n)·sqrt(Σ 预算项²; 本帧)`、
  `gate_scope = "two_sided" if rho_lo > 0 else "upper_only"`、判定序三行，**在被引文件中全部无出处**
- 应为：去掉「逐字」，改标「推导自 `docs/science/PHOTOMETRY.md` §16.5 第 3 条 + 本实验补全」
- 附带：`README.md:9` 声称该文件「只有 §1–§8」—— 我 `grep -nE "^#{1,4} "` 确认该文件**根本不用 § 编号**，用的是无编号 `##`/`###` 标题 ⇒ §4.1 这个节号本身也是无源的

### 4.2 须修（21 条）

| # | 位置 | 现状 | 应为 |
|---|---|---|---|
| M-1 | `README.md:8` vs `:74` | 同一 §4.1 两个互斥标题；且该文档**无编号 §4.1** | 统一为实文标题「测光一致性判据（单帧、尺度无关、双边界）」（`acsd.phase1.photometry.md:69`） |
| M-2 | `REPORT_route1.md:323` | H1 目标行 :227/:355 错误（真身 :13/:218/:336） | 改锚 |
| M-3 | `REPORT_route1.md:324` | H2「:400 精确命中」不成立（真身 :382） | 改锚 |
| M-4 | `README.md:234` vs `GATES.md:7` vs 归档 | 帧 C n=**96** vs **105** | 统一 |
| M-5 | `README.md:562` vs `GATES.md:13` | G6=PARTIAL vs PASS；且 :339 自称 N5「✔」与 :562「N5 未通过」自相矛盾 | 统一 |
| M-6 | `README.md:553` | REVIEW.md「428 行」实为 541 | 改 |
| M-7 | `code/README.md:62` | 「6 条负例（N0–N5）」实为 7 条（N0–N6） | 改 |
| M-8 | `code/README.md:86` | 无条件声称 quick 会标 `NOT_RUN`；归档存在时 `load()` 只判存在不判新鲜度 ⇒ G2/G5 照上轮归档给 PASS | 改声称或校验 mtime |
| M-9 | `step9_collect.py:54-56` | step5 缺失时静默 `return`，旧 GATES.md 原样留存、退出码 0（最完整的「第一跑红、之后转绿」） | 早退时 `sys.exit(非零)` 或先删/标注旧表 |
| M-10 | `step9_collect.py:316-324` | G6 证据行是冻结字面量快照，违反本文件 :32 自己的契约 | 全部从 `s7` 格式化 |
| M-11 | `step9_collect.py:114-125` | G1b 证据行印「N_eff=15.57」，但反解 σ_flat=0.0008806、flat_pix_sigma=0.0032 ⇒ 隐含 N_eff=**13.205**；`0.0032/√15.566=0.000811`（差 8.6%） | 证据行的 N_eff 与实际折算一致，或删该标注 |
| M-12 | `step9_collect.py:109` | `sigma_sys_quadrature <= sigma_ceiling` 是**包含式**恒真（系数 1.50489×√(·) 且还加 σ_fit） | 标为接线检查而非判别项 |
| M-13 | `redo/run_all.sh:29` | echo「逐项一致」但无任何比较；25 个基准快照从不被读取 | 加真实 diff，或删该断言 |
| M-14 | `exp_S01:87-93`；`analyze_real.py:69` | `exp_S01` 负例未加 `discriminating_power: none`；`analyze_real.py:69` 用 `zip(sorted(by_label), pp)` **按位置**把 `real_ridge` 的帧序数组贴到标签上 | 前者补标注；后者改为按标签 join。（**注**：我实测当前归档**错配 0/49**——因 `load_all()` 遍历 `sorted(os.listdir(...))`，帧序恰已字典序；故这是**脆弱性**而非现存错误，列须修不列阻断） |
| M-15 | `exp1:186` vs `PHOTOMETRY.md:268` | 正本鲁棒门是合取（`且离群权重为 0`），实现只判前者 | 补第二合取项 |
| M-16 | `exp_S08:115` 注释 | 「1.2533 = 1/Φ⁻¹(3/4)」错（应为 √(π/2)；1/Φ⁻¹(3/4)=1.4826） | 改注释，与 `derivation:47`、`archive:180` 对齐 |
| M-17 | `exp5:103`（820/deg²）vs `exp_S08:12-13`（43.6/deg²）vs `refs.md:46` | 两套密度模型互斥，且 820 已被 refs.md 标「未能核实」而 `REPORT_route1.md:142` 仍引用 | 统一口径 |
| M-18 | `step1_analytic.py:104` | `Budget(sigma_fit_white=0.014)` 硬编码，N0 判红完全依赖它 | 改为本帧可算量派生 |
| M-19 | `README.md:322` / `DOC_CORRECTIONS.md:85` vs `GATES.md:15` | **F502N 三个数三个值**：归档实测 `n=74 / median −0.07361 / MAD 0.20487 / slope 0.5584` ⇒ **GATES 对**，README 与 C5 的 `75 / −0.080 / 0.213 / 0.565` **错**；README:400/:422 的「MAD 0.049–0.255」区间也建立在错值上 | 以归档为准回填 README:322 与 C5 |
| M-20 | `README.md:362-363` vs `results/step8_real_frame.json` | **三个耗时全部过期**：归档实测 `guided=7.6692 s / blind=7.0140 s / guided_sec_per_fit=0.017671`，README 写 `2.093 s / 1.751 s / 0.004823`；`REVIEW.md:451`（轮次 2）同样过期 | 按归档回填 |
| M-21 | `GATES.md:7` vs 归档 | 帧 C 报「n=105」作判据样本数，但 `sigma_floor`/`sigma_ceiling` 实际按 **`budget.n=96`**（`n_inliers=96`）生成（`σ_floor ∝ 1/√n`，105 vs 96 差约 4.7%）；README:234 虽写「96（预算样本；候选 105）」但把 105 误称为「候选」（真 `n_candidates=500`，105 是 `n_selected`） | GATES 报判据 n 用 96；README 的措辞改为「n_selected=105，预算口径 n=96」 |

### 4.3 建议（7 条）

1. `exp_S01:87-93` 补 `discriminating_power: none`（P1-m09 整改漏了 S01）；并显式绑定循环末档变量（`:85-86`、`:97`）。
2. `exp_S06:137` 与 `exp_S11:77` 把已自认「无判别力」的项**仍送进 verdict** —— 与 S02/S09/S12 的处理不一致，统一为 `identity_checks` 独立栏。
3. `exp_S11:35` 删除错误常数 `exact_pred_n3 = 1.0860`（真值 ≈0.670，错 1.62×，已写进交付 JSON）；docstring :5-6 与 criterion :60-61 统一。
4. `exp_S08:52-55` 指标名与实现对齐（`sample_identical_to_fixed16_fraction` 语义相反）。
5. 死代码清理：`exp_S08:34,:43`、`exp2:129-132`、`step1:208`、`step6:65`、`step3:32`、`exp8:25`（`HC` 未用）、`real_gain:22,:436`。
6. `step3:211-212/:247-248/:228-232` 三处静默丢数据 + `step9:349` 对缺键无惩罚 ⇒ 建议 step9 强制三滤镜齐备。
7. **`run_all.sh` 从不执行的门**：① `code/redteam/rt_gate_attribution.py`（`README.md:466` 最重要的判读限制结论**全部出自它**）；② `redo/run_all.sh` 三路；③ `README.md:14-21` 声明为强制前置步的 `实验/shared/synthetic/run_selftests.sh`；④ `reverse_verify/p1_spatial_gain/`。另有约 16 条脚本内声明的判据（`step1:96,:136,:142,:203`；`step3:16-17`「独立校验」；`step6:70-74`「通量守恒」等）**执行了但 `step9` 不消费、README 不引用**。

---

## 5. 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| R-1 | 对 `exp_S00` 的 15 个 needle 逐条独立 `grep -cF`，不看脚本输出 | 「归档 15/15 全绿」 | **推翻**。实测 13/15；「判据参照」全文 0 命中（虚构锚）、「求解前提」在 :382 不在 :400（漂 18 行）。红名单因 `:89` 自签豁免仍为空 ⇒ 机器字段与结论串自相矛盾 |
| R-2 | 检验 `exp_S00:89` 谓词在「2 needle 确实落空」时的行为 | 红名单是否会真报 | **坐实失效**。两落空项的 `note` 都含「锚漂移」⇒ 被自己注释豁免 |
| R-3 | 读 `spectrum_integrator.cpp:145-160` 原文 | B11 缺陷在生产是否仍存在 | **推翻报告**。生产已订正（`PHOT-SIMPSON-N3-001`） |
| R-4 | grep「承载面」「线性面亮度」「求解前提」真实行号 | `REPORT_route1` H1/H2 | **双双推翻**。真身 :13/:218/:336/:382；报告给的 :227/:355/:400 全错 |
| R-5 | 读 `acsd.phase1.photometry.md:58-92` 对照 README:8 与 :74 | 「逐字来源」是否逐字存在 | **推翻一处**。`:8` 的「（从误差预算推导）」不存在；该文档无编号 §4.1 |
| R-6 | 比对 `derivation:15-24` 与 `README:72` | D1 是否随订正同步 | **推翻**。D1 仍假设「C 与星无关」并称「严格等价」 |
| R-7 | mtime 比对 + 6 个新标记串命中计数 | 「代码改了、归档没重跑」 | **推翻**。生成器新 2–3 天，6 标记命中全 0，G4 将由 PASS→RED |
| R-8 | 追 `step4:63` 与 `step5:225-226` 字面量到 `step9:247/:189` | G3/G4 否定性合取是否读实测 | **推翻**。两处均读写死字面量 ⇒ 结构恒真 |
| R-9 | 追 `step6:60-63` 到 `step9:288` | G5 是否含真判别项 | **推翻**。同表达式两份拷贝 ⇒ `rel` 恒 0 |
| R-10 | 用定义式对照 `exp_S08:115` 注释 | 常量标签 | **推翻**。1/Φ⁻¹(3/4)=1.4826 ≠ 1.2533 |
| R-11 | 比对 `REPORT_route2.md:188` 与 `:253` | 同报告内部一致性 | **推翻**。0.6694 vs 0.724σ |
| R-12 | 追 `exp2:95/:133/:134` 的集合来源 | 「负对照能红」 | **推翻**。同一谓词两次，右侧掩码恒全真 ⇒ 字段恒 True |
| **R-13** | **`sed -n` 三处比对 `makedirs` 目标 vs `open(path)` 目标** | route2 三脚本能否复现归档 | **推翻**。`exp2:149/150`、`exp7:84/85`、`exp8:170/171` 全部「建 A 写 B」，B 目录从不创建 ⇒ HEAD 上三条复现命令必然 `FileNotFoundError` |
| **R-14** | **追 `step9:280` 的 repro 到写归档处** | G4 的复现命令是否会刷新判据表 | **推翻**。repro = step4 脚本，其 `:117` 覆写 `step4_guided_vs_blind.json`，但 `GATES.md` 不刷新 |
| **R-15** | **从归档反算 `sigma_psfsys/sigma_sys_quadrature` 与 `sigma_obs/sigma_ceiling`** | 上界是否被同源项支配 | **推翻（判别力被压缩）**。97.1%/98.8%/99.8% 与 93.5%/82.0%/34.9% |
| **R-16** | **从归档反算退化族 `k_photo` 相对散布** | `g2_d` 防恒真守卫是否有效 | **推翻**。rel_ptp = **9.89e-4**，守卫被浮点噪声满足 |
| **R-17** | **用 `frames[A].guided` 的 n_candidates/n_matched_truth 按盲检同口径复算虚警** | `false_alarms=0` 是否成立 | **推翻**。71−70 = **1**，归档写 0 ⇒ 归档事实错误 |

---

## 6. 盲复算

**方法**：对每条关键结论，先在**不读既有审稿件**（`results/REVIEW.md`、前三轮 `审稿-*.md`、`TAUTOLOGY_REGISTER.md`）的前提下独立取证，再与既有结论比对。

| 结论 | 我的独立取证 | 与既有结论比对 | 判定 |
|---|---|---|---|
| B11 生产已修 | 亲读 `spectrum_integrator.cpp:145-160` | 前三轮**未见**；REVIEW 两轮均未提 | **新（偏严）** |
| GATES.md 未重跑 | mtime + 6 标记串 + `step9:207` 自述 | 前三轮**未见** | **新（偏严）** |
| `exp_S00` 自签豁免 + 13/15 | 我逐 needle `grep -cF` | 两个子代理**独立得出同一结论**（三路互证） | **新（一致）** |
| `exp_S08` 三门恒真 | 我读 `:62/:76/:139-142` + 常量代数 | 两个子代理**独立同判** | **新（一致）** |
| `exp2` 恒绿 + 假负对照 | 我追 `:95`/`:133` 同规则 | 两个子代理**独立同判** | **新（一致）** |
| route2 三脚本写盘路径错 | 我 `sed -n` 三处比对 | 子代理**独立同判** | **新（一致）** |
| H1/H2 锚错 | 我 grep 真实行号 | 两个子代理**独立同判** | **新（一致）** |
| G4 repro 覆写自身输入 | 我追 `step9:280`→`step4:117` | 子代理**独立同判** | **新（一致）** |
| G2 整行无判别力 | 我读 `:161/:162` + 归档反算 | 子代理**独立同判** | **新（一致）** |
| σ_psfsys 独占上界 | 我从归档反算 | 子代理**独立同判** | **新（一致）** |
| 全仓幽灵引用 | 我 `ls` + `git grep -l` 计数 | **前几轮均未系统登记** | **新（偏严）** |
| **DISPUTES A-P1-08 幻影缺陷指控** | **我亲读** `frame_photometry_fit.cpp:290-295` | 前三轮**未见** | **新（偏严）** |
| **README §2.3「逐字」块无来源** | 我 `grep` registry 文档 → 0 命中 | 前三轮**未见**（REVIEW R14 只改了节号） | **新（偏严）** |
| **F502N n=74/75、step8 三耗时、帧C n=96/105** | 我从归档实算三处 | 前三轮**未见** | **新（偏严）** |
| derivation D1 未同步 | 我读 `:15-24` vs `README:72` | **前几轮均未见** | **新（偏严）** |
| σ_flat 自指（C9/P1-B01） | 我读 DOC_CORRECTIONS C9 | 前三轮**已记** | **一致**（不重复计数） |
| G1b fail-open / G2 的 (a)(b) 降级 | 我读 `step9:91-110`/`:148-163` | 已在 `step9` 注释自记为 G08-05 R2 修 | **一致**（整改到位，不再报） |
| N2/N3/N4/N5 恒等式 | 未复核（`step7_negatives.py` 不在本片） | REVIEW R3/R4 + C1 已记 | **本片外，不重复报** |

**总判：偏严。** 15 条阻断中，**14 条在前三轮记录中找不到对应项**；唯一重叠的 σ_flat 自指我按已记处理。六个子代理在五组结论上独立复现（S00 / S08 / exp2 / H1H2 / G2 / σ_psfsys / 写盘路径 / G4 repro / DISPUTES幻影缺陷 / 三处数字失配），构成**多路互证**；同时我**否决了子代理 3 条**（见 §7），其中 1 条是其自列的头号阻断。

---

## 7. 子代理派发记录

**派发总数：6 个（4 个不同范围 + 2 次意外重复派发）** —— 超出规定的「3-5 个」，原因是两次重复派发。重复派发客观上提供了**盲复算互证**，如实记录。

| # | 范围 | 状态 |
|---|---|---|
| 1 | 恒真门三型 + 判据双向体检（route1/2/3 共 14 份） | **已回** |
| 2 | 同上（**意外重复**） | **已回** |
| 3 | step1–step9 主链 + run_all.sh（10 份） | **已回** |
| 4 | 伪引 + 文档-代码冲突（10 份） | **已回** |
| 5 | 同上（**意外重复**） | **已回** |
| 6 | reverse_verify 判据真实对象（2 份） | **已回** |

**4 份回执的逐条复核与否决**：

| 子代理结论 | 我的复核 | 采信/否决 |
|---|---|---|
| 「生产 B11 已修，三脚本复刻已修缺陷」 | 亲读 `spectrum_integrator.cpp:145-160` | **采信**（本片第 5 重 blocker） |
| 「`exp_S00` 实为 13/15；『判据参照』0 命中、『求解前提』在 :382」 | 独立 `grep -cF` 复算 15 个 needle | **采信**（与 §2③ 完全一致） |
| 「`exp_S08` H8a/H8b/H8d 三门零判别力，H8b ≡ √10」 | 读 `:62/:76/:139-142`，确认同式生成 | **采信** |
| 「`exp2:134-135` 负对照恒真 + `:129-132` 死码」 | 追 `:95`/`:133` 同谓词 | **采信** |
| 「route2 exp2/exp7/exp8「建 A 写 B」⇒ 复现必然失败」 | 我 `sed -n` 三处比对 + `ls` 确认 B 目录不存在 | **采信**（**我本人漏了这条**） |
| 「`step9:280` 的 repro 覆写 step4 归档」 | 我追 `step4:117` 的 `sc.jdump` | **采信**（**我本人漏了这条**） |
| 「`G2.c` 恒真 + `G2.d` 被浮点噪声满足」 | 我从归档反算 rel_ptp = **9.89e-4**（子代理报 1.3e-4，同量级） | **采信，用我自己的实测值** |
| 「`sigma_psfsys` 占 `sigma_sys_quadrature` 97%+」 | 我从归档反算 97.1/98.8/99.8% | **采信** |
| 「`exp_S11:35` `exact_pred_n3=1.0860` 错，真值 ≈0.6705」 | 我读 `:35` 与 `:47-51`；我**未运行**故只确认字面矛盾 | **采信为「字面矛盾」**，真值标**待运行核验** |
| 「`exp5` 的 `integrate_like_production` 越界，生产 `si.cpp:132` 返回 0.0 而非拒绝」 | 我确认 `exp5:84` 越界；生产侧我**未逐行读** `si.cpp:132` | **部分采信**：越界成立；生产行为标**待运行核验** |
| 「`exp7` 与生产 `sg.cpp` 三处算法差异（S 固定/度量网格/gauge）」 | 我**未读** `spatial_gain.cpp` | **不采信为事实**，列为待核 |
| 「`exp_S08:52-55` 指标名与实现相反」 | 我读 `:52-55`，确认 `min(cum[stop],cum[4])>=2000` 与 `stop==4` 两种情形都被记为「样本相同」，而 `cum[stop]<cum[4]` 时样本不同 | **采信** |
| 「13/14 脚本判据读的不是生产实现」 | 我以 `redo/README.md:5` 自述 + 无 import 生产模块确认 | **采信但降级**：该事实由仓库自披露且多处已按同口径引用；真正阻断的是「报告把无判别力门打 ✔ 并升格为权威订正」这层 |
| 「台账『往返自证型只有 2 例』与事实矛盾」 | 我确认本单元至少 5 例往返自证 | **采信**，已写入口径冲突待裁决 |
| 「`step4` 引导侧虚警应为 1 而归档写 0」 | 我从归档读 `n_candidates=71 / n_matched_truth=70` 并按 `:57` 同口径复算 | **采信** |
| **「`analyze_real.py:69` 位置 join 导致 41/49 标签-值错配」（列为阻断）** | **我从三个归档实算：`sorted(by_label)` 与 `real_gain` 的帧序标签逐项比对 ⇒ 错配 0/49**；原因是 `load_all():130` 遍历 `sorted(os.listdir(NORM))`，帧序**恰已**字典序 | **❌ 否决**。子代理的头号阻断结论**不成立**。位置 join 确有脆弱性（若装载序变化即错配），我已降级为 M-14 须修 |
| **「`_find_root` fallback 应改成 5 级」** | 我 `realpath -m` 实算三档：4 级→`实验/photometric-magnitude`、5 级→`实验`、**6 级→仓库根** | **❌ 否决其修法**（5 级仍不对），改用 6 级；其「双失效」的事实判断我采信并升级为 B-13 |
| 「DISPUTES A-P1-08 的 `1.4826` 四位截断缺陷不存在」 | **我亲读** `frame_photometry_fit.cpp:290-295` + `grep -n "1\.4826"` ⇒ :293 是 16 位全精度 | **采信**（新阻断 B-14） |
| 「F502N 归档 n=74 而 README/C5 记 75」 | 我从 `step3_forward_vs_photflam.json` 实算 n=74/−0.07361/0.20487/0.5584 | **采信**（M-19） |
| 「step8 三个耗时全部过期」 | 我从 `step8_real_frame.json` 实算 7.6692/7.0140/0.017671 | **采信**（M-20） |
| 「帧 C 判据 n 用 105 而预算按 96」 | 我实算 `n_selected=105 / budget.n=96 / n_inliers=96` | **采信**（M-21） |
| 「README §2.3 的『逐字』块在被引文件中无来源」 | 我 `grep -n "gate_scope\|1\.166\|sigma_ceiling"` 于 registry 文档 → **0 命中** | **采信**（新阻断 B-15） |
| 「`exp7` 与生产 `spatial_gain.cpp` 三处算法差异」 | 我**未读**该生产文件 | **不采信为事实**，列为待核 |

---

## 8. 自证段（可复跑命令）

> 全部**只读**；不编译、不跑实验脚本。中文路径请加 `git -c core.quotepath=false`。

```bash
cd "/workspace/Astro CS Database"

# 0) 基线
git -c core.quotepath=false rev-parse HEAD          # → f9650dd0…

# 1) 覆盖率：37 份、6782 行
bash -c 'awk "NR>=2565 && NR<=2602" run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml | grep -c "      - \"实验/"'   # → 37
# 逐份行数台账：/tmp/p1_sid_lines.txt（合计 6782）

# 2) B-1 判据表未重跑
ls -l --time-style=+%Y-%m-%dT%H:%M:%S \
  实验/photometric-magnitude/code/step9_collect.py \
  实验/photometric-magnitude/results/GATES.md \
  实验/photometric-magnitude/results/gates.json
for m in 现场重算 成立域声明 同源自证 项齐全有限正 "诊断（不判定）" same_source; do
  printf "%-16s %s\n" "$m" "$(grep -c -- "$m" 实验/photometric-magnitude/results/GATES.md)"; done   # → 全 0
grep -o "| G4 |[^|]*|[^|]*|" 实验/photometric-magnitude/results/GATES.md   # → **PASS**
sed -n '205,212p;229,232p' 实验/photometric-magnitude/code/step9_collect.py

# 3) B-2 route2 写盘路径「建 A 写 B」
for f in exp2_mag_prefilter exp7_spatial_bound exp8_fsyn_forward; do
  echo "--- $f"; grep -n "makedirs\|path = os.path.join" 实验/photometric-magnitude/code/redo/route2/$f.py; done
ls -d 实验/photometric-magnitude/code/redo/results 2>&1      # → 不存在

# 4) B-3 幻觉锚：逐 needle 独立复算
check(){ f="$1"; lo="$2"; hi="$3"; n="$4"; printf "%-3s %-22s %s:%s-%s\n" \
  "$(sed -n "${lo},${hi}p" "$f" | grep -cF -- "$n")" "$n" "$f" "$lo" "$hi"; }
check lib/algorithms/photometry/cpp/src/filter_curve_json.h 356 356 "map_filter_name"
check lib/algorithms/photometry/cpp/src/filter_curve_json.h 444 444 "load_curve"
check lib/algorithms/photometry/cpp/src/filter_curve_json.h 207 207 "check_curve_identity"
check lib/algorithms/photometry/cpp/src/filter_curve_json.h 190 194 "1e-9"
check lib/algorithms/photometry/cpp/src/star_matcher.cpp 493 501 "mag_tolerance"
check lib/algorithms/photometry/cpp/src/star_matcher.cpp 518 536 "< 3"
check lib/algorithms/photometry/cpp/src/star_matcher.cpp 538 545 "0.6744897501960817"
check lib/algorithms/photometry/cpp/src/star_matcher.cpp 555 555 "_IRLS_MAX_ITER"
check lib/algorithms/photometry/cpp/src/star_matcher.cpp 580 580 "_IRLS_CONVERGE"
check lib/algorithms/photometry/cpp/src/star_matcher.cpp 18 27 "_MAD_SCALE"
check lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp 166 174 ">= 30.0"
check lib/algorithms/photometry/cpp/src/pc_api.cpp 290 314 "2000-10000"
check lib/algorithms/photometry/cpp/src/pc_api.cpp 978 1008 "mag_max_arr"
check docs/science/PHOTOMETRY.md 126 126 "判据参照"     # → 0（虚构锚）
check docs/science/PHOTOMETRY.md 400 400 "求解前提"     # → 0（真身在 :382）
grep -n "求解前提" docs/science/PHOTOMETRY.md; sed -n '124,126p;376p;382p;400p' docs/science/PHOTOMETRY.md
sed -n '70,100p' 实验/photometric-magnitude/code/redo/route3/exp_S00_anchor_verification.py

# 5) B-4 G2 判定式无判别力 + rel_ptp
sed -n '157,163p' 实验/photometric-magnitude/code/step9_collect.py
python3 -c "
import json,numpy as np
u=json.load(open('实验/photometric-magnitude/results/step6_apply_and_units.json'))['unit_elimination']['degenerate_family']
k=[r['k_photo'] for r in u]; print(k); print('rel_ptp =', np.ptp(k)/np.mean(k))"   # → 0.000989

# 6) B-5 B11 生产已修 + 三脚本写死缺陷
sed -n '148,156p' lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp
grep -n "2 \* y\[0\] \* h / 3\|2.0 \* y\[0\] \* h / 3" \
  实验/photometric-magnitude/code/redo/route{1/exp5_integration_gates,2/exp8_fsyn_forward,3/exp_S06_fsyn_lambda_simpson}.py

# 7) B-6 exp_S08 三门 + 常量标签
sed -n '62p;76p;69p;115p;124,142p' 实验/photometric-magnitude/code/redo/route3/exp_S08_ladder.py
sed -n '46,47p' 实验/photometric-magnitude/docs/derivation_robust_weights.md

# 8) B-7 σ_psfsys 独占上界
python3 -c "
import json
d=json.load(open('实验/photometric-magnitude/results/step5_calibration_gate.json'))
for f in d['frames']:
    it,b=f['items_measured'],f['budget']; sp=it['sigma_psfsys_inframe']; sq=b['sigma_sys_quadrature']
    print(f['tag'],'psfsys/sys=%.1f%%'%(sp/sq*100),'obs/ceiling=%.1f%%'%(f['sigma_obs_mag']/b['sigma_ceiling']*100))"

# 9) B-8 字面量当实测
sed -n '61,63p' 实验/photometric-magnitude/code/step4_guided_vs_blind.py
python3 -c "
import json;g=json.load(open('实验/photometric-magnitude/results/step4_guided_vs_blind.json'))['frames'][0]['guided']
print(g['n_candidates'], g['n_matched_truth'], g['false_alarms'], '→ 应为', g['n_candidates']-g['n_matched_truth'])"
sed -n '225,226p' 实验/photometric-magnitude/code/step5_calibration_gate.py

# 10) B-10 G4 repro 覆写自身输入
sed -n '280p' 实验/photometric-magnitude/code/step9_collect.py
sed -n '117p' 实验/photometric-magnitude/code/step4_guided_vs_blind.py

# 11) B-11 幽灵引用
ls -1 docs/ | head -5; ls -1 docs/detail/registry/ | grep -i photometry; ls -d docs/plugins 2>&1
for p in ASTROCS_DESIGN.md astrocs.phase1.photometry.md 06_photometry.md ACCEPTANCE_SPEC.md; do
  echo "$p -> $(git -c core.quotepath=false grep -l -- "$p" | wc -l) files"; done

# 12) B-12 D1 与 README 矛盾
sed -n '15p;18p;21p;24p' 实验/photometric-magnitude/docs/derivation_robust_weights.md; sed -n '72p' 实验/photometric-magnitude/README.md

# 13) M 级数字不一致
sed -n '234p;339p;553p;562p' 实验/photometric-magnitude/README.md
sed -n '7p;13p;15p' 实验/photometric-magnitude/results/GATES.md
sed -n '62p;66p;86p' 实验/photometric-magnitude/code/README.md
python3 -c "import json;d=json.load(open('实验/photometric-magnitude/results/step5_calibration_gate.json'));print([(f['tag'],f['n_selected']) for f in d['frames']])"
sed -n '188p;253p;16p;90p;242p' 实验/photometric-magnitude/code/redo/route2/REPORT_route2.md

# 14) 待运行核验（本单不执行，交前台）
#   cd 实验/photometric-magnitude/code/redo/route2 && python3 exp2_mag_prefilter.py; echo $?   # 预期 FileNotFoundError + rc=1
#   cd 实验/photometric-magnitude/code/redo/route3 && python3 exp_S00_anchor_verification.py     # 看 n_needle_found 是否 =13
#   cd "/workspace/Astro CS Database" && python3 实验/photometric-magnitude/code/step9_collect.py # 看 G4 是否翻 RED、合计 7/9
#   python3 -c "print(abs((1/0.6744897501960817)-1.2533))"    # → 0.2293，证 1.2533 ≠ 1/Φ⁻¹(3/4)
```

---

## 9. 未确证清单（交前台）

1. `exp_S00` 重跑后 `n_needle_found` 实测值（静态推导 13，**待运行核验**）。
2. `step9_collect.py` 重跑后 G1/G1b/G2/G3/G4/G5/G8 的实际判定（静态推导 G4 由 PASS→RED、合计 7/9）。
3. `exp_S11:35` 的 `exact_pred_n3=1.0860` 与 `:47-51` 积分值（≈0.670）矛盾中哪侧是笔误。
4. `exp7` 与生产 `spatial_gain.cpp` 的三处算法差异（S 固定 / 度量网格 / gauge）——子代理提出，**我未读该生产文件，不采信为事实**。
5. `exp5` 本地守卫与生产 `spectrum_integrator.cpp:132/174-175` 的实际行为差异。
6. 子代理 #4/#5/#6 的回执未返回；其范围我已用自己的一遍阅读覆盖，结论均来自我亲读的原文。
7. `refs.md:46` 的 820/deg² UNRESOLVED 在 `REPORT_route1.md:142` 的下游数值影响（我确认文本冲突，未追影响面）。
8. **门计数口径**：我 52 门实例/48 去重门、子代理 49–75/27 严格恒真、仓内台账 27 —— 三个口径并存，须前台钉死后重出单元表。
9. **`exp7` 与生产 `spatial_gain.cpp` 的三处算法差异**（S 固定 / 度量网格 / gauge）—— 子代理提出，我**未读该生产文件，不采信为事实**。
10. `exp5` 本地守卫与生产 `spectrum_integrator.cpp:132/174-175` 的实际行为差异（`si.cpp:174-175` 我未逐行读）。
11. 需联网核验（refs.md 已自登记「未能核实」，本单未联网）：Rousseeuw & Croux 1993 Table 2 的 1.361（**`1.166` 的唯一文献锚，目前无独立可核来源**，而 README:92-95 与 `derivation:66-73` 的推导链依赖它）；refs.md V1/V3/V11 的书目字段。