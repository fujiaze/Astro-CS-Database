# 审稿-P1 · EXP-absolute-snr-003（第 1 遍 · 完整重读）

- 审稿人：P1 审稿车道（对同一片的一次完整重读）
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0`
- 纪律：亲自 `read` 全部成员原文；零 git 写；未编译、未跑 ctest/pytest、未跑构建、未跑任何实验脚本；未读 `/tmp/acsd_g08/`
- 计数口径说明（全文统一）：
  - **门实例** = 运行期能被求值并进入 `gates`/`injs`/pass-fail 判定的判定点（循环展开后计）
  - **去重门** = 门实例中删去逐位重复（同一表达式、同一阈值、同一取值）后的独立判据
  - **整改分母** = 需整改的独立判据总数（= 去重门 − 已合格者）

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威清单） | **38** |
| 实际读完份数 | **38** |
| 成员总行数（权威清单「实际行数」） | **8870**（清单声明 8870，`wc -l` 逐份复算亦为 8870，一致） |
| 实际读了多少行 | **8870** |
| **覆盖率** | **100.0 %** |
| 未读完的部分 | **无**（0 份、0 行） |

复核命令（可复跑）：

```bash
cd "/workspace/Astro CS Database"
sed -n '2029,2074p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml   # 取本片成员清单
while IFS= read -r f; do wc -l < "$f"; done < <(sed -n '2037,2074p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml | sed 's/^ *- "//; s/"$//') | paste -sd+ | bc
```

**说明**：本片 38 份全部为**纯静态阅读**。凡涉及「跑一遍看结果」的判断，本单一律未执行；涉及数值复核的，一律改为直接读仓内归档 JSON（读档案 ≠ 跑脚本），并在条目中注明该读数来自哪个归档。

---

## 2. 本片判定

### 判定：**阻断**

理由一句话：**本片的主结论性文件 `EXP-05-ABSOLUTE-SNR.md` 的核心判决「28 条判据全绿」建立在一份被现行代码自己宣告 EXPIRED 的归档上，而那 28 条里有 7 条是现行代码明文标注「不得作为实现正确性的证据」的纯恒等门；同时产生该判决的门脚本 `return 0` 无条件，缺陷永远不能让复现命令失败。**

### 最重的 3 条

| # | 位置 | 一句话 |
|---|---|---|
| **B1** | `实验/absolute-snr/code/exp05/e5_gates.py:425-443` + `实验/absolute-snr/results/exp05_e5_gates.json` + `docs/EXP-05-ABSOLUTE-SNR.md:35,522-547,657` | **代码改了、归档没重跑、报告还引用旧门名**。现行代码把 7 条纯恒等门移出计数并写明「**不得被引用**」，归档 `meta.n_gates=28/all_pass=true` 仍是那套旧门；报告 §0 第 9 行与 §7.1 三处仍按**旧短名**（G5a/G5b/G5c/G6a/G6b/G7b/G8）给实测值，而 `G8` 在现行代码里**已不存在**。门数实测 **26**（门实例口径），非 28。 |
| **B2** | `code/exp05/e5_gates.py:451`（`return 0`） | **判据永远不能让复现命令失败**。`main()` 无条件 `return 0`，`run_all.sh:27` 仍打印 `ALL DONE`；`exp05/run_all.sh:24` 又把产物写回被判据引用的同一路径 `results/exp05_e5_gates.json`。即：**缺陷被捕获后，复现命令不失败；第一跑红、第二跑就把唯一的证据面覆写掉。** |
| **B3** | `code/reverse_verify/snr_design/exp5_error_budget.py:83-87,128` + 其已入库归档 `exp5_error_budget.json:32` | **同片内自相矛盾**：EXP-05 报告 §2.5 亲自判定「`1.3127 ⇒ 帧级 SNR 偏低 23.8%` 不成立（归因错误）」，并把订正列为补强项 A4；但同片的 `exp5_error_budget.py` 仍把「the production estimator is a whole-frame **UNCLIPPED MAD**」当作 `MEASURED` 证据写死，且把 `1.3127` 顶替 `eps_p1` 进入「current production」场景，给出 **132.82 %** 的 SNR 误差预算。A4 的整改范围**只点了 `snr-propagation-design.md` 与 `REPORT.md`，漏掉了本片这两处活代码与已入库证据**。 |

---

## 3. 逐文件清单

38 份全读。下表「门实例 / 去重门」均按第 0 节口径。

### 3.1 文档（4 份）

| 文件（行） | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `docs/EXP-03-REGIONAL-SIGMA.md`（973） | 全文三遍：§0 结论页 → §1–§9 主体 → §10 + 附录 A/B | 与归档逐条对拍：§9.1 表 7 门的 6 个关键数字（−0.09 %/3σ 0.03 %、1.91 %/2.4 %、1.93 %/2.0 %、21.1 % vs 3.6 %/1.67×、6.66 %、+5.48 %、9.12）**全部与 `results/exp03_e4_gates.json` 一致**；唯一不符见 F3。§1.2 的代码锚（`star_detector.cpp:135`、`module_adapters.cpp:2486/4629-4630`、`snr_frame_science.cpp:89-90`）需在代码侧核（属他片）。 | **须修 1**（F3） |
| `docs/EXP-02-STRUCTURE-CONTAMINATION.md`（762） | 全文 | §8.1 门清单 14 条与归档 `results/exp02_e4_gates.json` **逐条同名同值**（80 ok / 125 fail_closed / 115 认证 / 0 假绿 / HST 7 ok·12 fail_closed·5 excess 假绿 / 最差 1.75 %·2.47 %）；§8.3 负例读数（+2.246e-05、3σ 6.03e-05、max 3.27e-04）逐位一致。**本片文档中归档保真度最高的一份。** | **通过** |
| `docs/EXP-05-ABSOLUTE-SNR.md`（710） | 全文三遍 | §0 第 9 行「**28 条判据全绿（`ALL_GATES_PASS = True`）**」、§7 `:522-523` 同数字、`:527-547` 的门表、附录 A `:657`「28 条」**四处全部指向已过期归档**；§7.1 表里 G5a/G5b/G5c/G6a/G6b/G7b/G8 恰是现行代码 `:432-440` 列出的 `obsolete_gate_names`。§2.5 自身的归因订正是对的（支持 B3 的指控）。 | **阻断（B1）** |
| `docs/SNR_WEIGHT_RESEARCH_PACK.md`（229） | 全文 | 首行「已按 §9.73 A44 作废 …… GAP_AUDIT.md §9.73 A44；**ASTROCS_DESIGN.md** §2.1」：仓根**无** `GAP_AUDIT.md`（`ls` 已核），仓内只有 `docs/ACSD_DESIGN.md`、**无** `docs/ASTROCS_DESIGN.md`。**此非新问题**：`docs/engineering/UNRESOLVED_REGISTER.md:4114-4117`（第 81.1 条）已明文登记「`§9.73 A44` 的所指不是顶点条款，而是一份**已不在仓内**的审计件」。§8.1/§8.5/§8.8 的行锚表本身自洽，且 `:195-207` 诚实标注了两条「语义漂移」。 | **建议**（已登记，仅补片内落点） |
| `data/README.md`（38） | 全文 | `:26` 记 testdata 为 `{M1,M2,M4} …**三帧**`，而 EXP-02/03 用 16 帧、EXP-05 §4.1 用 **12 帧**（M1/M2/M4/M5）——本 README 是 SCI-402 期的 B3 指针，与本片三个实验单元的实情不符。`:35` 的复现命令 `bash 实验/absolute-snr/code/run_all.sh` **存在**（`ls` 已核，1790 B）。 | **建议** |

### 3.2 判据与实验代码（26 份）

| 文件（行） | 门实例 / 去重门 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| `exp05/e5_gates.py`（455） | 21 个 `X.gate` 调用点 → **运行期 26 门实例** / **去重 25**（G1d 与 G1c 逐位重复）/ 整改分母 **25** | 全文；逐门回溯其取值来源至 `e4_weight.py`、`exp05_common.py` | **B1 的代码侧**：`:425-443` 自宣告归档 EXPIRED、列 8 个 `obsolete_gate_names`、`:441` 写「本单不得自行覆写 `results/`」——**而 `:113` 的默认 `--out` 就是该同一路径**，`run_all.sh:24` 亦然 ⇒ 自我指涉。恒真门：G1b（`:127`，归一化加权均值对权重公共缩放恒等）、G1c（`:131`，`var_w=Σw²v/(Σw)²` 对 `w→w/c²` 不变 ⇒ 恒 0）、G1d（`:134`，G1c 逐位重复）、G5d（`:351`，`amb>0` 由中位数序统计量保证恒非负）、G9a（`:377`，`cross_frame_E(K=2,σ≡1)` 与其闭式特例代数恒等）。结构对称型：G6（`:270` 的 `xf_ok` 与 `:219` 的 `rel_ratio` 逐字相同；交换臂与正确臂的零集相同）、G5c（`:250` 的 `abs_fault` 与 `:187` 的 `abs_ratio` 逐字相同，且 `recon_absolute` 是单参函数，结构上进不了 `sf`）。容差矛盾：`:191` 的 `lvl_dev` 与 `:288-289` 的 `lvl2` 是**同一个数**，却用 `TOL_TRUTH_LEVEL=1e-1`（`:56`）与 `TOL_TRUTH_REL=2.5e-2`（`:52`），而 `:54` 自记实测 1.49e-2 ⇒ G7 裕度仅 1.68×。fail-open：`:374`/`:385` 的 `if e_case:` 使 M5 缺失时 G9a/G9b/G10 **根本不生成**。死导入 `:25 import e4_weight as E4`。 | **阻断** |
| `exp05/e4_weight.py`（210） | 0 | 全文 | fail-open：`:121-124` e3 缺失时静默回落到硬编码 `[2.937,2.940,2.704]`，唯一痕迹 `measured_c_source="builtin"`（`:154`）**从不被 `e5_gates` 检查**（`:373` 只取 `case=="M5"`），而文件头 `:12` 承诺「用**真实帧实测的 c_k** 定量」。`:100` 的 `sigma` 形参在 `:127`/`:142` 恒为 `ones` ⇒ 死参，一般权重情形从未被测。`:124` 的 `negative_control_equal_c=[1.5,1.5,1.5]` 若被接进 G10 会**立刻假红**（`bias_common=0, bias_xframe=0` ⇒ `0 > 10×0` 为假）⇒ 埋雷。 | **须修** |
| `exp05/make_tables.py`（198） | 0 | 全文 | `:48` 读 `exp05_e5_gates.json`、`:189` 把 `e5["meta"]["all_pass"]` 与 `n_gates` **原样写进 `EXP05_TABLES.md`** ⇒ **表是过期判决的二手转述，且不带任何过期标记**（已核：全 `results/` 目录 grep `EXPIRED` 零命中）。`:55` 的口径行把 `F_ref=1、A_NEA=1` 写在表头，掩盖了 A_NEA 是**尺寸相关量**。 | **阻断（随 B1）** |
| `exp05/run_all.sh`（27） | — | 全文 | `set -euo pipefail` 正确、相对层级正确、**无** `\|\| true`。但 `:24` 覆写被判据引用的归档（B2）；`:10` 的 `EXP05_OUT` 可重定向，而 G 门与 `make_tables` 硬编码 `X.RESULTS` ⇒ 一旦设 `EXP05_OUT`，门判的是**旧归档**而非本轮产物。 | **阻断（随 B2）** |
| `exp04/make_tables.py`（215） | 0 | 全文 | 纯展示层：`:21-26` `load()` 缺文件返回 `None`、各处 `if eN:` 静默跳过 ⇒ **缺归档时表会少一整节而无人察觉**（`:167` 的 `e7`、`:179` 的 `e4` 同）。无判定。 | 建议 |
| `exp04/e7_crossframe.py`（123） | **0 门**（仅 1 个 fail-closed 输入守卫 `:98`） | 全文 | docstring `:7` 承诺「并与真值的跨帧离散（**=0**）对比」——**该对比从未实现**，且真值散布按构造就是 0（M=8 次实现共用同一 `sigma` 面，`:53`/`:58`/`:66`），即使实现也只会得到另一个恒真门。docstring `:10` 的 C3「与空间臂对比」同样无实现（`:64-70`、`:108-111` 算了两组数但无比值/阈值/门）。`:74` `if os.path.exists(E.M16)` 静默跳过整个 C1b 块且输出无标记——**与同文件 `:98` 的 fail-closed 纪律直接矛盾**。`:38` `np.log10(np.abs(r))` 用 `abs` 折叠符号。 | **须修** |
| `exp04/e5_cost.py`（103） | **0 门** | 全文 | `BUDGET_S`（`:25`）只被 `:52-54` 的 `print` 引用，**没有 `break`/`continue`** ⇒ 打印的「后续 Δ 不再测该算子」**为假**。`finite`（`:48`）与 `over_budget`（`:83/86/90`）**算而不判**；且 `over_budget` 是**比值不是布尔** ⇒ 消费方 `if row["over_budget"]:` 会把所有行当真超预算。`--quick`（`:36`）只给 2 个 Δ ⇒ `:70` 的 `len(rr)>=3` 永不满足 ⇒ 每个算子的 `loglog_slope_vs_N` 全为 `None`，而 `:92` 的 `frozen_config` **不记 `quick` 标志**。N 标度 3 点即报 `t∝N^x`，无 R²/不确定度。 | **须修** |
| `exp03/e4_gates_selftest.py`（360） | **12 门实例**（7 gate + 5 inj）/ 去重 12 / 整改分母 **10** | 全文；逐 inj 回溯 | `:253-256` `r1/max(r1,1e-12)` ⇒ A2≡1 ⇒ `certify_region(1.0)` 恒 `"ok"` ⇒ `pass` 恒真；且 **`:250` docstring 说「认证必须判红」，`:256` 代码断言的恰是相反**；512² 的 `r1` 算完即弃。`:285-286` `shorted="ok"` 字面量 ⇒ `(shorted=="ok")` 恒真，门退化为 `true_err > DELTA`；**`:283` 的 `a2` 算完从不使用**（死变量，是「注入从未施加到被检函数」的直接物证），`certify_region` 全程未被短路调用过。`:191-193` 两条 HST 场景缺失 `continue`、`:206-207` `if nul:` 使无结构负例整段跳过 ⇒ 同一门内两条 **fail-open**。`:148` 的 `e_r1`（EXP-03 的主产物 R1）**算完不进判据**。`:269` 用 Python `and` 做除零守卫。`:238` 在 `reg`/`glob` 为空时 `max()` 抛 ValueError 而非判红。 | **须修**（恒真门已登记在案） |
| `exp02/e4_gates_selftest.py`（258） | **≤14 门实例（条件化）** / 去重 14 / 整改分母 **13** | 全文 | `:36` `_load` 缺文件返回 `None`；`:208/214/216` 的 `if d1:/if d2:/if d3:` 使门列表**条件追加**，`:241 ALL_GATES_PASS=all(...)` 在**更短**的列表上求值 ⇒ **删任一归档即 exit 0、门数无声减少**（同仓正确范式就在邻片 `exp03/e4_gates_selftest.py:186,222`：同情形返回 `pass=False`）。`:147` `v_disabled="ok"` 字面量恒真，且 `:148` 的 `+0.013` 是**硬编码 S_clip 基线**（与 `:43` 文档的 1.38~1.43%、`:63` 的实测 `abs(base)` 三处三个口径）。`:225-234` 三条门直接读上游脚本预算好的布尔、零重算。`:212` 把 `crosscheck_mirror` 拷进输出但**从不检查其 `available`**。`:141-146` 的 `v_ok`（正确变体）**从不进 gates** ⇒ 注入门只测「坏变体判红」，不测「好变体判绿」。 | **须修** |
| `exp02/e1_analytic_scan.py`（242） | 0 独立门 | 全文 | `:190` `"G_gate_disabled_turns_green_wrongly": True` —— **字面量常量 `True`**，`:192-196` 的 note 诚实说明「本自检不执行该注入」，但 `exp02/e4_gates_selftest.py:212` 把整个 `red_injections` 拷进归档、读者会把 `True` 当读数。`:187` `G_fix_within_budget = abs(fix_rel + 0.013) <= DELTA` 的 `0.013` 同为硬编码。`main()` 恒 `return 0`。 | **须修** |
| `exp02/exp02_common.py`（510） | 0 assert；3 个 fail-closed 守卫（`:77/:167/:235`） | 全文 | `:448-450` `classify` 把缺失的 `A1/A2/D` 替换为**完美值 1.0**（而 `tol=1.014`）⇒ **漏传诊断量 = 门变绿**，其中 `A2` 是其 docstring `:393/:433` 自称的「判据主量」。`:334-348` `convergence_gate` 的 `C = |σ̂(box)/σ̂(2box) − 1|` 分子分母**同出 `mesh_sigma`** ⇒ 对一切公共乘性偏置零信息。`:501-504` `rms_of` docstring 自称「与生产 noise_sigma 同口径」，实为**无裁剪、绕中位数**的总体 RMS，与生产（2 轮裁剪绕**上中位数**、`:95-99`）差的恰是 B3 里登记的 1.31~1.57×。`:172-176` 与 `:180-182` 对**同一 bug** 给了两个数（13000 % 与 +2.1 %）。`:152-168` `_median_filter_nan` 名实不符（遇非有限即 `raise`）。`:56-103` 的生产 recipe 是手抄，`:106-139` 的独立核对只与**另一份手抄**互校 ⇒ **sigma 根只对着自己验证**。 | **须修** |
| `exp01/q2a_estimator_pairing.py`（428） | **0 门**（`:424` 恒 `return 0`） | 全文 | `:343-344` `analytic_table` 以**逐字相同实参**调两次 ⇒ 四个 `spurious_*` 恒为 `f(x)/f(x)−1 ≡ 0`；docstring `:22` 把它列为「负例 N3：跨帧伪差必须**严格 0**」。`:311` `sigma_opt_equals_sigma_box` 按定义恒真。`:351-357` N4 **不调 `C.var_optimal`**，只重写 `1/a` 并用自写的 `np.where` 守卫 ⇒ 测的是守卫副本不是被测对象。docstring `:13-23` 声明的判据 (a)–(d) 与 N1–N4 **全部只 dump 进 JSON，无人判定**。`:79/:82/:83` 死变量。 | **须修** |
| `exp01/hst_sim.py`（97） | 0 | 全文 | `:78-85` 重写了 docstring `:11` 点名的 `noise_model.predicted_variance_adu2`，`:28` 导入了 `NM` 却**从不调它** ⇒ HST 臂「真值」是共享模型的第二份手抄。`:92-97` `truth_sigma_adu` docstring 自称「与生产 recipe 的全局尺度同口径」，实为无裁剪绕中位数。饱和时 `V[m]` 为空 ⇒ 返回 NaN 无守卫。 | 建议 |
| `exp01/real_products.py`（138） | 1（`:53` 解析守卫，非科学判据） | 全文 | `:26-27/:56` 帧级标量用**正则刮取**、同名键取**最后一次**匹配 ⇒ 若生产 JSON 在 `"psf_params"` 前出现嵌套 `noise_sigma`，`meta["noise_sigma"]` **静默取错且无门会响**（同文件数组侧已用正确的 `raw_decode`）。`:132-134` 配对未命中 `continue` 且**不返回 `n_unmatched`**；`src["id"]` 是 `dtype=object`（`:98`）⇒ id 空间不一致时**全 miss 并静默清空**。`:114` `find_products` 无产品时返回 `[]`，调用方无法区分「无数据」与「偏小」。手写扫描器无往返校验。 | 建议 |
| `reverse_verify/frame_snr/branch_discriminator.py`（191） | 3 门实例 / 去重 2 + 1 退出码门 | 全文 | `:17` docstring 承诺「PASS 当且仅当 同 FWHM bin 内 sigma_f 的**相对散布 < 1e-6**」；`:32 TOL=1e-6` 是**死常数**（只在 `:163` 被抄进输出，**不进任何谓词**）；实际门在 `:93` 是 `abs(dark10/bright10 − 1) < 1e-3` ⇒ **量不同、容差宽 1000 倍**。`:92` 注释宣布的「正确判据（通量维度）」**未被实现**。红对照 `:143` 用**被测的 `sigma_f_adu`** 反推 `sig_sky` ⇒ `:156` 的「必须变红」由代数保证，**对生产零证据力**；`:144` `* 0.0` 与 `:150` `** 0` 是被关掉的增益项留下的**死项指纹**。`:119 GAIN=1.5` 自述「假设值」。`:95/:157` 的 `break` 使每产品只取第一帧、只取第一个合格 case；`:184` 判定只要求 `len(cases)>0 and n_pass==len(cases)`。**正面**：`:184` 的空集判红是 fail-closed，正确。 | **须修** |
| `reverse_verify/snr_design/exp5_error_budget.py`（187） | **0 门**（无 assert/raise/退出码） | 全文 | **B3 的代码侧**。另：`:79-116` 的 7 项全是硬编码字面量，evidence 串引 `NOISE_MODEL.md 5a` / `UNCERTAINTY_AND_COVARIANCE.md` / `RELEASE-02 unc-prop-audit.md`（自述「report **retired**」，按 `AGENTS.md` §4 非可核证据）/ `CALIBRATION.md:459`——**脚本从不读这些文件**。`:43` `--frame-snr-spread` 默认 2.22 自称「49 个 L4 帧的 max/min」，**那 49 帧从不读取**。`:120-122` 从**同一张字面表**取回自己的两个元素（只能防表内字面漂移）。`:146` 键名 `snr_error_pct` 与 `:174-177` 表头「error of the REPORTED sigma」**都不带** docstring `:23-28` 自己强调的「`eps_master=0` 是未测量、**不是**无误差、总计**不是**上界」限定。`:8` 承诺的通用权重式 `Σw²v/(Σw)²` **从未实现**。**正面**：`:109-115`/`:181-182` 对 `eps_master` 的「未测量 ≠ 无误差」标注做得对。 | **阻断（B3）** |
| `reverse_verify/snr_design/run_all.sh`（51） | — | 全文 | `rc` 累计与 `exit "$rc"` 正确，**无** `\|\| true`，`:9` 上溯层级正确。`:29` `cp -f` 把复现产物**回写源码目录**、覆写受版本管理的 `exp*.json` ⇒ 自愈源头（复现动作覆写被引证据面）。`:12` 不检查 `$NORM` 存在性，错误延后到 astropy。`:5/14/16` 无 `-e` 且 `mkdir -p` 失败被吞。 | **须修** |
| `reverse_verify/snr_design/audit/run_all_audit.sh`（36） | — | 全文 | `:23` 循环 4 个脚本；同目录另有 `audit_mosaic_shape.py`（229 行）**从未被任何 `run_all.sh` 调用**（子代理全仓 grep 佐证，7 处命中无一是 sh 引用）——**该孤儿是裁决级证据**。本入口**不回拷** ⇒ `audit/*.json` 永不刷新（3 脚本 09-30 改过、归档停 09-21）。`:5/12/14` 同 `mkdir` 吞错。 | **须修** |
| `reverse_verify/p7_noise/exp1_injection_recovery.py`（380） | 6 判据（R1–R5）**全部只判定不生效** | 全文 | `:305/:351/:354/:355/:361` 算出 `R1_pass`…`R5_*_pass` 并打印，但 `:376 main()` **无条件 `return 0`** ⇒ 六条判据**无任何退出码出口**。`:29-33` docstring 写「判据（写死，不事后放宽）」，与「无出口」冲突。`:336-343` 的 `[RETRACTED]` 解释串里**硬编码了 0.4345 / 1.0945 两个实测值**（同文件 `:319-329` 的另一段用 `%` 格式化）⇒ 数据变了这两句不会跟着变。**正面**：`:252-254` 如实登记零噪声负例的成立条件（`flat_base=True` 等），`:336-343` 显式标注 `[RETRACTED-DIRECTION]`。 | **须修** |
| `reverse_verify/p7_noise/p7lib.py`（321） | 0 | 全文 | 共享库，`:5` 自陈「判据先行：各 exp 脚本顶部写死阈值；本库只提供估计量与解析预测，不含阈值」——纪律清晰。`:222-246` `paired_var_adu2` 把「必须用均值不能用中位数（median(χ²₁)=0.4549 ⇒ 偏 54.5 %）」的踩坑写进注释，是好范式。`:312` `det_of` 强制 `stack_n` 默认 32。**无缺陷**。 | **通过** |
| `reverse_verify/f_instr/exp1_recovery.py`（145） | 0 | 全文 | 纯数据生产 + 打印表，`main()` 无返回值。`:98` `np.load(os.path.join(OUT,"scene.npz"))` 依赖 `OUT` 已存在 ⇒ 缺件时 `FileNotFoundError` 裸崩（与 `snr_design` 侧同型，但不在本片必查面）。`:50` `encircled_energy` 口径未在文中声明。 | 建议 |
| `reverse_verify/f_instr/exp5_psf_shape.py`（107） | 0 | 全文 | `:96` `pass = abs(bias) <= 0.02` 写进 JSON 但**无退出码、不入任何入口判定**。`:10` docstring 的 0.02 mag 阈值**无推导**。`:61` 的 `sig` 用 `1.4826·MAD`、与同目录 `exp1_recovery.py:54-59` 的 `robust_sky_stats` 口径不同却同名混用。 | 建议 |
| `audit/route1/exp01_robust_statistics_constants.py`（155） | 0 | 全文 | `:124` `"null_zero_noise_metric": 0.0 if np.allclose(np.clip(np.zeros(10),0,1),0) else 1.0` —— **拿全零数组和自己比**，恒真；这是 P-CST-06 唯一的「真值无效应」负例。`:141` 的注释（截断偏置 2.5e-4 低于 MC 分辨率、只解析确立、MC 只查公式）**诚实**，但 `:149` 仍把 `mc_z_vs_closed` 与判定并列输出。`:48-51` P-CST-03 的 null 是真算（常数数组的 MAD 确为 0）。 | **须修** |
| `audit/route1/exp05_mref_and_reference_flux.py`（165） | 0 | 全文 | **正面范式**：`:102-108` 把 `w = SNR²/F_ref² ≡ 1/σ_F²` 明确标注 `is_tautology: True, evidence_eligible: False` 并说明「原版以字面常数 `sigma_F=12.0` 复现之并当作证据，已按标准 01 §7 作废」；`:113-132` 把跨帧 w 比值漂移改成**可假判据**并配负例。**不一致**：`:136-145` 的 `fref_kphoto_identity`（`F_ref,k·k_photo,k = F0`）**同样是无上界的代数恒等**（`ZP_k = ZP_SYN − 2.5·log10(kp)` ⇒ `F_ref_k = F0/kp` ⇒ 乘积恒 `F0`），却写成 `"identity_is_exact_up_to_float": True` 而**未标 `evidence_eligible: False`**。 | **须修**（同文件内纪律不统一） |
| `audit/route1/exp07_weight_and_coadd_identities.py`（57） | 0 | 全文 | `:22-25` `w_ident = snr²/F_ref²` vs `w_direct = 1/σ_F²`，`snr = F_ref/σ_F` ⇒ **代数恒等**，读数必为 1-ulp 舍入；脚本 docstring `:7` 自己写「Registered gate value: deviation ~ 2.22e-16 (1 ulp)」。`:34-37` 负例（丢掉 `F_ref²` ⇒ O(1) 偏离）证明**测试非空**但被测公式仍是脚本内自重写，**不 import 任何生产代码**。`:36` `o1_detected`、`:31`/`:50` `within_4_ulp` 写进 JSON **无退出码**。 | **须修** |
| `audit/route1/exp12_gamma_weight_scale.py`（55） | 0 | 全文 | `:24-30` `F_ref=100g`、`sigma_F=5g` ⇒ `snr` 与 g 无关、`w = 0.04/g²` ≡ `ivar` ⇒ **`gamma2_identity` 是纯代数恒等**，结构上抓不到任何实现缺陷。docstring `:6-12` 的推导自相矛盾：「w must scale as 1/sigma_F_k^2 **~ g_k^0**」——`1/σ_F²` 随 g⁻² 缩放，`g^0` 是错的；同段又写「w(gamma=2)*sigma_F,k^2 = 1 to machine precision」——两句无法同时成立。`:43-46` 负例 `g=1` 使任意指数都通过 ⇒ 恒真。`:49` 结论称「numeric verification at 1e-15」但代码无容差门。 | **须修** |
| `audit/route2/exp03_closed_form_constants.py`（132） | 0 | 全文 | docstring `:9` 承诺「root-find the half-max points of a numeric Gaussian」，但 `:60 val = 2·sqrt(2 ln2)` 与 `:62 r_half = sqrt(2 ln2)`、`:67 numeric = 2·r_half` ⇒ **「数值腿」就是闭式本身**，两者恒等。`:36-53` 的 Acklam 逆正态自实现 `:37` 自称 ~1e-15 但**无对拍**。`:122` 明确写「diagnostic-only（production zero consumers）」——退役治理**诚实**，正面。 | 建议 |
| `audit/route2/exp04_moffat4_sigma.py`（80） | 0 | 全文 | docstring `:13` 承诺「and a **bisection** for FWHM」、`:12` 承诺「radial quadrature of the second moment」——`:41-43 moffat_fwhm_numeric` 直接 `return 2.0*alpha*sqrt(2^(1/beta)−1.0)`，**闭式，无二分**；故 `:64 numeric_fwhm_over_sigma` 与 `:52 closed_form` 恒等，二矩对拍才是真腿。`:69-71` 的 β=2 发散负控制是**真负例**，正面。`:51 reg=1.230310` 与闭式 `1.230307652590` 差 +1.908e-06（已复算），文件如实登记。 | 建议 |
| `audit/route2/exp07_correlation_diagonal.py`（125） | 0 | 全文 | `:68-83` MC 对拍 GLS 方差是**真实验**（显式协方差 + Cholesky + 40 万次）。`:96-107` ρ=0 负控制是真负例。`:117-119` `k_corr` 登记值 1.3883 被「reproducible_at {n_eff:3, rho:0.19415}」**反解凑出**——即该常数被**定义**为那个点，而非被实验确定；docstring `:26` 已自认「toy model, original calibration grid not archived」，**诚实登记到位**。全程无退出码。 | 建议 |
| `audit/route3/exp02_profile_constants.py`（160） | 0 | 全文 | `:64/:75` 的「dense numerical half-max crossing」是**真腿**（400001/800001 点插值），与闭式独立。`:84-85` 用**精确逆 CDF 采样**的径向密度算二矩，方法学正确。`:108-113` 负控制是交叉套用常数。`:113 wrong_constant_detected: True` 是**字面量常量**而非判据（唯一瑕疵）。`:41-48 norm_ppf` 是 200 次二分、无收敛校验与端点夹持。 | 建议 |
| `audit/route3/exp03_median_se_identity.py`（89） | 0 | 全文 | `:47-48 h1["negative_sigma0"] = {... "is_zero": True}` —— `is_zero` 是**硬编码字面量**，不是由 `se` 推出的判定；且未调 `C.var_optimal`。`:11-12/:60-64` 的 H2 与 `route1/exp07` 同一恒等。`:69-74` 的 H3（γ 指数）诚实给出 `theory_bound_SNR_pow` 作为参照，方法学可取。docstring `:12` 自陈「no literature/experiment leg needed」——对恒等式成立，但对「实现正确性」不成立。 | 建议 |
| `audit/route3/exp04_refmag_chain.py`（296） | 0（判据以 `verdict` 字符串落盘） | 全文 | **本片最诚实的门注记范式**，应被推广：`:14-25` 逐条声明每条判据的**证据资格**——G2a 的绿「是**解析可证**的，不是实测得来的」（`:135` 再重复一次）；G3' 的 4.44e-16「是**纯 IEEE-754 舍入**，**不是物理效应**」「阈值只高出底噪 2.3 倍，裕度偏薄」。`:164-171` 把 `w = SNR²/F_ref² ≡ 1/σ_F²` 标 `is_tautology: True, evidence_eligible: False` 并注明「原版以字面常数复现之并当作证据，已作废」。`:37-41` 明写 IDW 是**代理算子**、不在冻结词表内、只报数量级。`:226` 循环变量用 `cx/cy` 而 `:248` 索引成 `[cy][cx]`——顺序正确。**无缺陷**。 | **通过（范式）** |
| `audit/supplement_control_variance/spotcheck_independent_seed.py`（62） | 0 | 全文 | **真独立 seed**：`:19 SEEDS=[20260602,20260603,20260604]`，`:43` 逐 seed 独立重跑 ⇒ 「独立种子复核」名副其实（我亲自核实子代理关于「复用同 seed ⇒ 门必绿」的担忧**不成立**）。`:24 ch = clamp(4e7/n, 1e4, 2e5)`、`:34` 用原始矩算方差，标准正态下数值稳定。`:45-46` `ratio_B = (π/2)·c_mad2/kappa_mc` 与 `route1/exp01` 的 P-CST-06/07 互补。**无退出码**。 | **通过** |
| `docs/…` 见 3.1 | | | | |

---

## 4. 发现清单

> 口径：位置 / 现状 / 应为 / 证据。阻断 = 使结论性陈述不成立或使判据失去证据资格；须修 = 缺陷确实存在且当前实现不可接受；建议 = 改进项。

### 4.1 阻断（5 条：B1–B4 + B5；B5 为 S5 报告后由我复算升级）

**B1｜exp05 门—归档—报告三方脱钩；报告的 7 条「证据门」是纯恒等门**
- 位置：`code/exp05/e5_gates.py:425-443`；`results/exp05_e5_gates.json`（`meta.n_gates=28`、28 个旧门名）；`docs/EXP-05-ABSOLUTE-SNR.md:35`、`:522-523`、`:527-547`、`:657`
- 现状：现行代码把 7 条纯恒等门移入 `identity_checks`（`:309-346`）并写明「**验证定义自洽，不构成实现正确性的证据，不得被引用**」，同时把该归档标为 `EXPIRED`；归档本身仍是旧 28 门；报告四处仍写「28 条全绿」并按**旧短名**给实测值，而 `G8` 在现行代码里已不存在、`G5a/G5b/G5c/G6a/G6b/G7b` 在现行代码里指向**语义完全不同**的门。
- 应为：同一次提交内**同时**①按现行代码重跑生成新归档与新表；②把报告 §0/§7/附录 A 的门数与门名改成现行定义；③在报告里显式记「7 条旧恒等门已退役、不计入门数」。二者必须同提交。
- 证据：门数清点 —— `grep -c "X.gate(gates" e5_gates.py` = **21** 个调用点，其中 `:399/:406/:413` 在循环内，按归档枚举展开为 2+2+4 = 8 ⇒ **门实例 26**；与 28 自洽（28 − 8 退役 + 6 新增 = 26）。归档实测：`meta.n_gates=28`，门名含 `G5a_frame_scalar_fault_leaves_absolute_bitwise`…`G8_degenerate_bitwise_identity`。
- 计数口径：**门实例**。

**B2｜exp05 门脚本恒 `return 0` ⇒ 缺陷不能让复现命令失败；且复现动作覆写唯一证据面**
- 位置：`code/exp05/e5_gates.py:451`；`code/exp05/run_all.sh:24,27`
- 现状：`main()` 无条件 `return 0`；`ALL_GATES_PASS=false` 只体现在 JSON 的 `all_pass` 字段。`run_all.sh:24` 把 `e5_gates.py` 的输出指向 `$OUT/exp05_e5_gates.json`，而 `e5_gates.py:113` 的默认 `--out` 与 `:441`「本单不得自行覆写 `results/`」**指的是同一路径** ⇒ 守卫生效的同时自我指涉。
- 应为：`return 0 if X.all_pass(gates) else 1`（同单元邻片 `exp02/e4_gates_selftest.py:254` 已是正确写法）；或把默认输出改为 gitignored 目录并在报告里显式区分「本轮新档」与「仓内快照」。
- 证据：`e5_gates.py:447-451`；同片正面/反面对照 `exp02/e4_gates_selftest.py:254`。
- 计数口径：门实例（退出码门）。

**B3｜`exp5_error_budget.py` 仍断言 EXP-05 自己已撤回的归因；A4 整改范围漏掉本片两处活代码 + 已入库证据**
- 位置：`code/reverse_verify/snr_design/exp5_error_budget.py:83-87`、`:128`；已入库归档 `code/reverse_verify/snr_design/exp5_error_budget.json:32`；对照 `docs/EXP-05-ABSOLUTE-SNR.md:225-236`（§2.5）与 `:500`（A4 补强项）
- 现状：EXP-05 §2.5 已判定「`1.3127 ⇒ 帧级 SNR 偏低 23.8%` **不成立（归因错误）**」，并把订正列为 A4；但 `exp5_error_budget.py:86` 仍写「**the production estimator is a whole-frame UNCLIPPED MAD**」，`:128` 仍把 `1.3127` 顶替 `eps_p1` 进入「current production」场景，输出 **132.82 %**。A4 点名的整改位置只有 `snr-propagation-design.md` 与 `run/SNR-DESIGN-01/REPORT.md`。
- 应为：①把 `:83-87` 的归因改为 EXP-05 §2.5 的订正写法（分子来自 `p1_snr.json.frames[].variance` 的逐像素模型 A `robust_sigma`，与 `frame_snr` 所用的整帧裁剪 RMS 是**两个生产者互相矛盾**，不是 `frame_snr` 的偏差）；②场景 1 的 `eps_p1` 改用 EXP-02/03/05 实测的**真实帧电平偏差**（SNR 偏低 15 %~77 % ⇒ 相对 σ 误差 1.18×~4.35×）或标 `UNRESOLVED`；③把 `exp5_error_budget.json:32` 一并订正；④**同步扩 A4 的范围**。
- 证据：`docs/EXP-05-ABSOLUTE-SNR.md:236`「由它推出「帧级 SNR 偏低 23.8%」是**归因错误**」；归档实测 `current production … eps_p1=1.3127  tot=1.3282  132.82%`；`git -c core.quotepath=false grep -n "UNCLIPPED MAD"` 命中 4 处，全部仍活（`exp5_error_budget.py:86`、其 `.json:32`、`audit/audit_exp3_physical.py:86`、`audit/physnoise.py:103`——后两处在 `audit/` 目录，属他片，须一并 grep）。
- 计数口径：门实例（该文件**零门**）。

**B4｜`EXP05_TABLES.md` 是过期判决的二手转述且无过期标记**
- 位置：`code/exp05/make_tables.py:48,184-189`；`results/EXP05_TABLES.md`
- 现状：`:184-188` 逐条转写 `e5["gates"]`、`:189` 写 `` `ALL_GATES_PASS = %s`（%d 条）``，**不携带任何过期/退役标记**。
- 应为：表头固定加一行 `> 门集合版本：<commit>；过期归档不得引用`；或表只从**新档**生成。
- 证据：`grep -rn "EXPIRED\|stale_archive\|不得作为证据引用" 实验/absolute-snr/results/` → **零命中**。
- 计数口径：门实例（展示层，不计门）。

**B5｜`route3/exp03` 的 `within_gate` 是**恒红门**，且仓内已存在**提交态红灯**——同一恒等式在 `route1/exp07` 用 4 ulp 阈值则绿**
- 位置：`code/audit/route3/exp03_median_se_identity.py:60-64`；**已入库**归档 `code/audit/results/route3/exp03_median_se_identity.json`
- 现状：`:56` `sigma_f = f_ref/snr` ⇒ `:57 w_direct = 1/sigma_f²` 在**定义上**等于 `:58 w_identity = snr²/f_ref²`（往返自证，零物理内容）。阈值 `:64 within_gate = rel_dev.max() <= 2.22e-16`（**恰 1 ulp**）。归档实测 `max_rel_dev = 6.661338147750939e-16`（**恰 3 ulp**），**`"within_gate": false`** —— 正确实现下**永远红**，把良性舍入报成失败。
- 同一恒等式在 `route1/exp07:31/50` 用的是 **4 ulp** 阈值、实测 `5.561e-16`（2.5 ulp）⇒ `within_4_ulp: true`（绿）。**同一命题、同一物理、两个不同阈值、相异判决。**
- 应为：①阈值统一为 `max(2 ulp, 实测基线)` 或直接对齐 `route1/exp07` 的 4 ulp；②该门按 §4.2/C2 的纪律移入恒等门登记，**不得计为通过的门**；③更根本的是——这条门在**正确实现下就红**，违反「判据须在正确实现下绿、注入缺陷时红」；④仓内已提交的红灯须在台账登记为**已知红灯**，不得沉默入库。
- 证据：
  ```bash
  python3 -c "import json;d=json.load(open('实验/absolute-snr/code/audit/results/route3/exp03_median_se_identity.json'));print(d['H2_weight_identity'])"
  # -> {'eps_double': 2.22e-16, 'archived_gate': 2.22e-16, 'max_rel_dev': 6.661e-16, 'within_gate': False}
  python3 -c "import json;d=json.load(open('实验/absolute-snr/code/audit/results/route1/exp07_weight_and_coadd_identities.json'));print(d['identity_w_snr2_over_fref2'])"
  # -> {'max_rel_dev': 5.561e-16, 'within_4_ulp': True, 'archived_v4_max_dev': 3.01e-16}
  ```
- 计数口径：门实例。

### 4.2 须修（12 条）

| # | 位置 | 现状 → 应为 |
|---|---|---|
| S1 | `exp02/e4_gates_selftest.py:36,208,214,216,241` | 门列表**条件追加**；删任一归档 ⇒ 门数无声减少而 `ALL_GATES_PASS` 仍可能 True ⇒ 应：`d1/d2/d3` 缺失时生成一条 `prerequisite_present` 的 **FAIL** 门（对照 `exp03/e4_gates_selftest.py:186,222` 的正确写法） |
| S2 | `exp05/e5_gates.py:374,385` | `if e_case:` 使 M5 缺失时 G9a/G9b/G10 **不生成**，`all_pass` 仍在短列表上为真 ⇒ 同上，改为空即 FAIL |
| S3 | `exp03/e4_gates_selftest.py:283-286` | `shorted="ok"` 字面量恒真；`:283` 的 `a2` 是**死变量**（`certify_region` 从未被短路调用）⇒ 应：照 `exp01/negatives_selftest.py:114` 的正确范式，注入**被测量本身**的替身（`v = 2.0 if fault else d[...]`） |
| S4 | `exp02/e4_gates_selftest.py:147-158` + `e1_analytic_scan.py:190` | `v_disabled="ok"` 字面量恒真；上游更直接写死 `"G_gate_disabled_turns_green_wrongly": True` ⇒ 同 S3；另 `:148` 的 `+0.013` 硬编码基线须统一到 `_cert_audit:63` 的 `abs(base)` |
| S5 | `exp03/e4_gates_selftest.py:191-193,206-207` | 两条 HST 场景缺失 `continue`、`if nul:` 使无结构负例整段跳过 ⇒ 同一门内两条 fail-open ⇒ 应改 fail-closed |
| S6 | `exp05/e5_gates.py:191` vs `:288-289` | **同一个数**（`|median(recon_absolute(spf))/median(Tf)−1|`，`Tf` 为常数场）却用 `1e-1` 与 `2.5e-2` 两个容差，`:54` 自记实测 1.49e-2 ⇒ **G7 裕度仅 1.68×，是边红候选** ⇒ 应统一容差或给出两个量确不同的推导 |
| S7 | `exp05/e5_gates.py:250,270` | `abs_fault` 与 `:187` 的 `abs_ratio` **逐字相同**；`:270` 的 `xf_ok` 与 `:219` 的 `rel_ratio` **逐字相同** ⇒ G5c/G6 的部分子句是 G5a/G5b 的重复且更松 ⇒ 应去重 |
| S8 | `exp02/exp02_common.py:448-450` | 缺失的 `A1/A2/D` 被替换为**通过值 1.0**（`tol=1.014`）⇒ 漏传 = 门变绿，`A2` 更是自称的「判据主量」 ⇒ 应缺省 `float("inf")` |
| S9 | `exp05/e4_weight.py:121-124,154` + `e5_gates.py:373` | e3 缺失时静默回落到硬编码 `c_k`，`measured_c_source="builtin"` **从不被检查** ⇒ G9a/G9b/G10 可在字面量上判绿，而文件头承诺「真实帧实测」 ⇒ 应在 e5 侧断言 `!= "builtin"`，或在 e4 侧 raise |
| S10 | `reverse_verify/snr_design/run_all.sh:29` | `cp -f` 把复现产物**回写源码目录**、覆写受版本管理的 `exp*.json` ⇒ 复现动作覆写被引证据面（自愈源头） ⇒ 应默认写 gitignored 目录，仓内快照显式回拷 |
| S11 | `branch_discriminator.py:17,32,93,92-93` | docstring 承诺「相对散布 < 1e-6」，实现是「`dark10/bright10` 偏离 < 1e-3」；`TOL=1e-6` 是**死常数**（只进 `:163` 输出）；`:92` 注释宣布的「正确判据」未实现 ⇒ 应实现通量维度判据，并把 `TOL` 接入谓词或删除 |
| S12 | `reverse_verify/frame_snr/branch_discriminator.py:143-150,156` | 红对照用**被测的 `sigma_f_adu`** 反推 `sig_sky` ⇒ 「必须变红」由代数保证，**对生产零证据力**；`:144 * 0.0`、`:150 ** 0` 是被关掉的增益项留下的死项指纹 ⇒ 应改用独立重算的 `sigma_f`，或明确标注「仅度量活性」 |

### 4.3 建议（11 条）

| # | 位置 | 建议 |
|---|---|---|
| C1 | `exp03/e4_gates_selftest.py:253-256` vs docstring `:250` | `r1/r1` ⇒ A2≡1 ⇒ 恒 `"ok"`；且**docstring 说「必须判红」而代码断言相反** ⇒ 修 docstring 或改注入点（真恒等门，报告 §9.1 已如实登记「如实登记该门自身恒绿」，但代码内文档仍反着写） |
| C2 | `exp05/e5_gates.py:127,131,134,351,377` | 5 条纯恒等门：G1b / G1c / G1d（G1c 的重复）/ G5d（`amb>0` 由中位数序统计量保证）/ G9a（`cross_frame_E(K=2,σ≡1)` 与其闭式特例恒等）⇒ 移入 `algebraic_identity_checks`，`n_gates` 相应减 |
| C3 | `exp01/q2a_estimator_pairing.py:343-344,311,351-357` | N3 是同函数调两次（恒 0）、N1 子项按定义恒等、N4 不调被测函数；`main():424` 恒 `return 0` ⇒ 判据零出口 |
| C4 | `exp02/exp02_common.py:334-348,501-504,172-182` | `convergence_gate` 的 `C` 分子分母同源（对公共乘性偏置零信息）；`rms_of` 自称「与生产同口径」实为无裁剪绕中位数；`_expand_mesh_map` 同一 bug 两个数（13000 % / +2.1 %） |
| C5 | `audit/route1/exp01_robust_statistics_constants.py:124` | 零噪声负例是 `allclose(zeros, 0)` ⇒ 恒真；这是 P-CST-06 唯一的「无效应归零」证据 |
| C6 | `audit/route1/exp05_mref_and_reference_flux.py:136-145` | `F_ref,k·k_photo,k = F0` 同为无上界恒等，却未按本文件 `:102-108` 的自家纪律标 `evidence_eligible: False` ⇒ 同文件内标准不统一 |
| C7 | `audit/route1/exp07:22-25`、`exp12:24-30`、`audit/route3/exp03:11-12,47-48` | 三处恒等门均**不 import 任何生产代码**，只重写被测公式 ⇒ 结构上不可能因生产缺陷翻红；`exp12` 的 docstring `:6-12` 推导自相矛盾（`1/σ_F² ~ g^0` 错） |
| C8 | `audit/route2/exp03:60-67`、`exp04:41-43` | docstring 承诺的「数值 root-find / bisection」**未实现**，「数值腿」就是闭式本身 ⇒ 二者恒等；`exp04` 的二矩对拍是真腿 |
| C9 | `exp04/e7_crossframe.py:7,10,74,38` | docstring 承诺的「与真值跨帧离散(=0) 对比」与 C3「与空间臂对比」**均未实现**；`:74` 静默跳过 C1b 与同文件 `:98` 的 fail-closed 矛盾；`:38` 用 `abs` 折叠符号 |
| C10 | `exp04/e5_cost.py:52-54,83,36,92` | 死预算（只 print 不截断）；`over_budget` 是比值不是布尔；`--quick` 静默产出全 `None` 的标度段且 `frozen_config` 不记 `quick` |
| C11 | `data/README.md:26`、`exp01/real_products.py:26-27,56,132-134,114`、`exp01/hst_sim.py:78-85`、`audit/route3/exp02:113`、`audit/route3/exp03:47-48` | 分别是：README 的帧数（3 vs 12/16）与本片实验不符；帧级标量用正则刮取且同名键取最后一次；配对未命中静默 `continue`；HST 真值是共享模型的手抄副本；两处 `wrong_constant_detected` / `is_zero` 是硬编码字面量 |

### 4.4 伪引与「现状已反转」型引用（本人已逐条复核，见 §7）

| # | 位置 | 现状 → 应为 |
|---|---|---|
| P1 | `docs/EXP-03-REGIONAL-SIGMA.md:46-47`、`:7`、`:830`；`docs/EXP-02-STRUCTURE-CONTAMINATION.md:5,36-37` | 逐字引 `AGENTS.md` §5/§8/§6 的三句话（**带引号**）在 `AGENTS.md` 中**逐字 0 命中**：实测 `grep -c "查证\|质疑前提\|负例" AGENTS.md` = **0**；且节号错——`grep -n "^## " AGENTS.md` 得 §4=科学自治、§5=文档维护、§6=代码纪律、§8=提交纪律。相关要求在 §4。**应**：改引 §4 的实际措辞或删除引号。 |
| P2 | `docs/EXP-05-ABSOLUTE-SNR.md:128,445,686` | 三处逐字引 `docs/ASTROCS_DESIGN.md` §3.1「**全程只有 SNR**，HiPS 里存的是帧级 SNR 与稀疏控制点上的绝对 SNR」。该路径**不存在**（真实为 `docs/ACSD_DESIGN.md`），且其 §3.1 不含该长句（`:182-183` 实为「两个独立对象…**不做相对归一**」）。**应**：改引 `docs/ACSD_DESIGN.md:183`。 |
| P3 | `docs/EXP-05-ABSOLUTE-SNR.md:227-228` | 冒号后的**裸从句**（无引号，正是任务警告的形态）把「⇒ 帧级 SNR 偏低 23.8 %」归给 `snr-propagation-design.md §1019`，而该文 `:56/:1048-1049` 明写该归因**已删**、真实偏差是**偏低 15 %~77 %**。**这与 B3 是同一处缺陷的文档侧**。 |
| P4 | `docs/EXP-05-ABSOLUTE-SNR.md:195` | 代码符号 `ASTROCS_FRAME_SNR` **全仓只存在于本实验的 3 个文件**（本文档、`snr-propagation-design.md`、`exp5_error_budget.py`）；`lib/` 下的真实键是 **`ACSD_FRAME_SNR`**。**应**：改键名。（`git grep -c "ASTROCS_FRAME_SNR"` 与 `git grep -l "ACSD_FRAME_SNR" -- lib/` 已实测。） |
| P5 | `docs/EXP-05-ABSOLUTE-SNR.md:499`（§6.2 A3）、`:501`（A5） | **这两条「需补强」在 HEAD 已无对象**：①`docs/science/CONTROL_WEIGHT_SNR.md:218`（§8a 第 7 条）**已含**「方向性约束（若未来要求两者一致）：只允许 `frame_snr := median_p(SNR_c)`…`SNR_c := frame_snr × 相对场` 的反向…**不可逆**」；②`lib/infrastructure/scheduler/src/module_adapters.cpp:7360-7366` 的 `cfg.sigma_sky_source` 注释**已是订正后的版本**（原文「来自 **StarDetector::estimate_background 的整帧 2 轮 median±3\*1.482602218505602\*MAD 裁剪后 RMS**…noise_model 的稳健尺度是**另一个**生产者」）。**应**：删除 A3/A5。**注意反差**：A4 却**范围不足**，漏掉了本片活着的 `exp5_error_budget.py`（见 B3）⇒ **同一张补强清单里，两条已失效、一条范围不足**。 |
| P6 | `docs/EXP-02-STRUCTURE-CONTAMINATION.md:57-58`、`docs/EXP-05-ABSOLUTE-SNR.md:242-245` | 称代码注释写「noise_sigma 来自 noise_model 的**空天稳健尺度** (1.4826\*MAD)」并据此列为待订正项。该措辞在 `module_adapters.cpp` **已消失**（见 P5②）；`lib/` 下仅 `snr_estimator.h` 尚存一处，且语境不同。**应**：撤下该「现状」描述。 |
| P7 | `docs/EXP-03-REGIONAL-SIGMA.md:37,701-706,681,720-724` | §1.1/§8.3 通篇按「**帧级 × 帧内相对因子**」「稀疏层存中位归一相对 SNR」建模并判「必须改」，而现行 registry 已改口径（`docs/detail/registry/acsd.phase1.noise-snr.md:117/121/262`：`sparse_reconstruct` **不乘帧级标量**、控制点存**绝对** SNR）。**EXP-03（seed 20260925）与 EXP-05（seed 20260926）§6.1「绝对表示已落地、不提出任何回退」直接冲突** ⇒ EXP-03 §8 整节的「现状」栏已失效。**应**：整段按现行 registry 重写。 |
| P8 | `docs/EXP-05-ABSOLUTE-SNR.md:396` | 「绝对 **−0.92 %** vs 相对 **−45.70 %**…撑大 **84 %**」与本文档自己的表 A（`:377`）及归档 `results/EXP05_TABLES.md` 均不符（归档为 **−1.01 % / −44.12 %**；`b_f=1.7897` ⇒ **+78.97 %**）。`grep -rn "45.70" 实验/absolute-snr/{docs,results}/` 仅命中该行自身。**应**：改三个数。 |
| P9 | `docs/EXP-05-ABSOLUTE-SNR.md:30,400` vs `:448,698` | 「≤0.2 %」对「≤0.25 %」自相矛盾；表 A 的 `hst_struct_varsky` 绝对偏差 = **+0.25 %** ⇒ 「≤0.2 %」为假。 |
| P10 | `docs/EXP-05-ABSOLUTE-SNR.md:459` vs `:534` | G3 门 gap：`:534` 记 **3.7e-4**、`:459` 记 **7.7e-5**；归档 `exp05_e5_gates.json` 实为 **3.6945e-4** ⇒ `:459` 无归档支撑。**应**：统一为 3.7e-4。 |
| P11 | `docs/EXP-03-REGIONAL-SIGMA.md:56,57,58`（及 `:693,694`） | 代码行号锚三条全失效：`star_detector.cpp:135` 实为函数体内的 `if (!(*sigma > 0.0)) return false;`（`estimate_background` 定义在 `:73`）；`module_adapters.cpp:2486` 实为 `uv.message + " (light frame: …"`（写点在 `:4258`）；`:4629-4630` 实为 `// ── P9: header_pointing 初始指向 + 板尺度`（读点在 `:7359`）。**对照**：`:59` 的 `snr_frame_science.cpp:89-90` **正确**。**应**：删行号只留符号名（EXP-02 的 `:39` 已如此声明，EXP-03 未继承）。 |
| P12 | 五篇文档的 `run/` 证据面 | `run/SCI-402`、`run/SCI-B-EXP-02`、`run/SCI-B-EXP-03`、`run/SNR-ABS-DERIVE-01`、`run/SNR-DESIGN-01`、`run/DOC-404`、`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md` **一律不存在** ⇒ `EXP-05:583-589`「两次独立运行逐位一致、cmp 通过」、`SNR_PACK:222`「期望 anchors=36 ok=36 bad=0」、`EXP-03:909` 的 `sources/**` 等**全部核验声明在仓内不可复核**。**应**：要么入库，要么把不可核声明降级为「报告人自述」。 |

### 4.5 已核实为**真**（防误报，明确不改）

以下项经本片亲自对拍**全部成立**，不得在整改中误伤：EXP-02 的 16 帧 fail_closed、1.16~3.66 倍、240 条中 115 条认证、负例 Δ=+2.246e-05±2.011e-05；EXP-03 的 7 门 5 注入与表 D1 的 6 个关键量、R1 曲线 +46.6 %→+91.2 %、16 帧；EXP-05 的 28 门与 §7.1 的 11 个实测值逐位相符、表 A/B/E 与 `EXP05_TABLES.md` 一致、M1+M2+M4+M5=12 帧、§8 分臂命令与 `run_all.sh` 完全一致；`data/README.md` 的 12 项数值逐条 grep 相符、其 `:35` 的 `run_all.sh` 存在且编排准确；`spotcheck_independent_seed.py` 确为真独立 seed（与主实验 seed 20260601 无重叠）。



B1（门—归档—报告三方脱钩，且报告的 7 条「证据门」是纯恒等门）在本片是**新**的：仓内 `docs/engineering/UNRESOLVED_REGISTER.md` 记录的是 `results/exp05_e5_gates.json` 脱钩本身（由 G08-05 整改代码引入），但**「报告 §7.1 仍按旧短名引用已退役门、并把 7 条纯恒等门计入『28 条全绿』」这一层**未被登记。B2 的「`return 0` 无条件 + 复现动作覆写唯一证据面」是**新**的。B3 的「A4 整改范围漏掉 `exp5_error_budget.py` 与其已入库 JSON」是**新**的。B4（新表无过期标记）是**新**的。S6（G7 与 G5a 是同一个数却两个容差、裕度 1.68× 边红）是**新**的。S7（G5c/G6 子句是 G5a/G5b 的逐位重复）是**新**的。C6（`exp05` 同文件内 tautology 标注标准不统一）是**新**的。

---

## 5. 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| R1 | 假设「报告 §7.1 的 `G5a=0.0` 指的是现行代码的 `G5a`」——按短名去现行代码里找 `G5a` | 期望它**不是**同一条门 | **已推翻**。现行 `G5a_absolute_arm_matches_analytic_truth`（`e5_gates.py:198`）是「跨帧比值 + 逐帧电平 + 逐 patch 形态」三合一、锚在解析真值 `T_k=1/v_k` 上的门；旧 `G5a_frame_scalar_fault_leaves_absolute_bitwise` 是「帧级标量 ×2 后绝对重建逐位不变」。**同名不同义**，`G8` 更是完全不存在。 |
| R2 | 假设「28 这个数字仍然对」——清点现行代码的门 | 期望能数出 28 | **已推翻**。21 个 `X.gate` 调用点 + 3 个循环展开（e1 2 行 / e2 非 `hst_null` 2 行 / e3 4 panel = 8）⇒ **26 门实例**。与归档自洽：28 − 8 退役 + 6 新增 = 26。 |
| R3 | 假设「EXP-05 §9.1 的 7 个门数字与归档一致」——逐个对拍 | 期望全部一致 | **部分推翻**。6 个一致（−0.09 %/0.03 %、1.91 %、1.93 %、21.1 % vs 3.6 %、6.66 %、5.48 %、9.12 全部逐位吻合）；**1 个不符**：`:795` 写「结构场 12 次判定 = **6 ok** / 2 diagnostic_only / 3 fail_closed」，**6+2+3=11≠12**，而归档 `structured_verdicts` 计数为 **{ok:7, fail_closed:3, diagnostic_only:2}**，n=12 ⇒ **ok 应为 7**。 |
| R4 | 假设「§3.3 的 2.7e-5 与 §9.1 的 2.7e-5 是同一个数」 | 期望自洽 | **已推翻（轻微）**。归档 `a4_formula.rows`（25 行）中 `abs_diff` 最小值 = **2.7493e-05**（slope 3.25 / box 64）⇒ §9.1 的「2.7e-5」是**截断**该最小值，§3.3 表 `:269` 的「**+3.0e-5**」是**错误进位**（2.75e-5 进到 3.0e-5 应为 2.7e-5 或 2.8e-5）。同一数字在同一文档内两处写法不一致。 |
| R5 | 假设「`spotcheck_independent_seed.py` 声称独立种子但可能复用同 seed ⇒ 门必绿」 | 期望抓到同 seed | **未推翻**。`:19 SEEDS=[20260602, 20260603, 20260604]`，`:43` 逐 seed 独立 `np.random.default_rng(seed)` ⇒ 三个 seed 真实不同，跨 seed `kappa_sd_across_seeds` 有意义。（我**否决**了子代理基于「可能是同 seed」的疑虑——该疑虑在派发时被我列为待核项，核后不成立。） |
| R6 | 假设「`exp1_analytic_scan.py:190` 的 `True` 只是注释旁注」 | 期望它不是门 | **已推翻**。`:190` 是 `res[name] = {... "G_gate_disabled_turns_green_wrongly": True}` 的**字典成员**，会原样进 `results/exp02_e1_analytic.json`；`exp02/e4_gates_selftest.py:212` 把整个 `red_injections` 拷进归档。`:192-196` 的 note 说「本自检不执行该注入」，但**值仍是 True**。 |
| R7 | 假设「`audit/route2/exp04` 的 `moffat_fwhm_numeric` 是真的数值二分」 | 期望它是闭式的独立对拍 | **已推翻**。`:41-43` 直接 `return 2.0*alpha*sqrt(2**(1/beta)−1.0)`，**与 `:48-50` 的闭式逐字相同** ⇒ `:64 numeric_fwhm_over_sigma` 与 `:52 closed_form_fwhm_over_sigma` 恒等。docstring `:13` 承诺的「a bisection for FWHM」不存在。 |
| R8 | 假设「`exp5_error_budget.py` 的 `phase1_source_contamination_bias` 项值 0.0 ⇒ 1.3127 没进预算」 | 期望它没进 | **部分推翻**。项值确为 `0.0`（`:83`），但 `1.3127` 以**另一个名字**（`eps_p1`）进入了场景 1（`:128`）⇒ 它不但进了预算，还**顶替**了「估计器噪声」这一项，并在场景 2–6 中被 `0.015` 彻底替换（`:129-138`）。输出的 term 表与 scenario 表因此**互相脱钩**。 |
| R9 | 假设「`branch_discriminator.py:93` 的门在正确实现下是绿的」 | 期望它是可假判据 | **未能确证（留 UNRESOLVED）**。该量 `dark10/bright10` 在 gain-free 下受 bin 内 fwhm 细变污染（bin 宽 0.01 px，`σ_f = σ_sky·√A_NEA(fwhm)`），我手算 bin 内相对离散约 `0.5×(0.01/fwhm) ≈ 1.7e-3`（fwhm≈3）> 阈值 1e-3 ⇒ **可能是恒红**。但**本单禁跑脚本**，未取到 `:178` 打印的真实 `sigma_f 相对散布` 实测值，故**不下结论**，标 UNRESOLVED。 |

---

## 6. 盲复算

方法：对「门数」「文档—归档一致性」「恒真门判定」三件事，先遮住既有判定独立重算，再与本片结论比对。

| 项 | 盲复算路径 | 与既有结论比 |
|---|---|---|
| exp05 门数 | 只看 `e5_gates.py` 的 `X.gate(` 出现位置（不看任何报告/归档），用 `grep -n` 得 21 个调用点，逐个判断是否在循环内（`:399/:406/:413` 在循环）⇒ 18 固定 + 8 展开 = 26 | **一致**（既有结论亦为 26）。**未发现偏松/偏严。** |
| exp02 门数 | 只数 `gates.append` 的调用点（11 处，含 3 个在 2 元循环内）⇒ 静态上限 8 + 6 = 14；再看 `:208/214/216` 的条件装载 | **一致**（≤14，且条件化）。**未发现偏松/偏严。** |
| exp03 门数 | 只数 `gates = [...]`（7）与 `injs = [...]`（5）⇒ 12 | **一致**。 |
| exp02 报告↔归档 | 遮住报告，只读 `results/exp02_e4_gates.json` 取 `{ok:80, diagnostic_only:35, fail_closed:125}`、认证 115、假绿 0/0、最差 1.7487 %/2.4718 %、负例 +2.2462e-05 与 3σ 6.0327e-05 | **与报告 §0/§8.1/§8.3 逐项一致 ⇒ 判「一致」，无偏松偏严。** |
| exp03 报告↔归档 | 同法 | **1 项偏严（即报告偏松）**：`gate_not_degenerate` 的 ok 计数报告写 6、归档 7。**判：既有结论（首轮报告「全部一致」）在这一点上偏松，我判更严。** 其余 6 项一致。 |
| 恒真门 | 遮住子代理结论，逐条自己代数复核 | 与子代理**基本一致**；我把 C2 从「3 条」扩到「5 条」（新增 G5d、G9a），**比子代理更严**。 |

**结论**：盲复算与本片结论**一致**，唯二处偏差已在 §5 R3 记录，方向为「既有结论偏松、本片更严」。

---

## 7. 子代理派发记录

派发 **5 个**（要求 3–5），全部在后台并行；本人对每一条都回到原文逐行复核后才采纳。

| # | 名字 | 做什么 | 结果 |
|---|---|---|---|
| S1 | `9a50074d`（判据体检） | 12 份判据/库文件的恒真门三型 + 恒红门双向体检、阈值出处、读不到真对象 | 交回 12 节长报告（30 条编号发现 + 阈值出处汇总表） |
| S2 | `28f53c86`（判据体检·并行第二份） | 同口径交叉体检，作为 S1 的**独立第二意见** | 交回 7 条可证明恒真门 + 3 处 fail-open + 1 处最锋利盲区 |
| S3 | `8b6132ba`（伪引与文档↔代码交叉） | 5 份文档的伪引（含**裸从句**扫法）+ 文档声明的函数/键名/行号/数值在代码里是否真实存在 + 归档产物是否存在 + run_all 编排 | **后到、在报告收尾时交付**。逐条复核后**全部采纳** P1–P12（见 §4.4），其中 P1/P2/P4/P5/P6 由我**亲自 grep 复验** |
| S4 | `02753ec5`（复现编排与自愈） | 3 个入口的编排完整性、自愈判据、代码改了归档没重跑、旧门名全仓 grep、产物新鲜度、shell 卫生 | 交回 5 节长报告（含 `git log` 提交号取证） |
| S5 | `bce6ff3d`（审计路线常数与文献） | 15 份 `audit/`+`reverse_verify/` 脚本的常数出处、恒真门三型、seed 独立性、读不到真对象 | 交回 15 节长报告（28 个判据门、9 项待联网核验清单）。**其「route3/exp03 恒红门 + 已提交红灯」我亲自复算后升级为 B5** |

### 7.1 我**采纳**的子代理结论（已自行复核）

| 出处 | 结论 | 我的复核 |
|---|---|---|
| S1/S2 | `exp03/e4_gates_selftest.py:285-286`、`exp02/e4_gates_selftest.py:147-158` 的「门被短路」注入是字面量恒真；`:283` 的 `a2` 是死变量 | **亲自读原文确认**，并补出 S1/S2 都没提的一条：上游 `exp02/e1_analytic_scan.py:190` 直接写死 `"G_gate_disabled_turns_green_wrongly": True` ⇒ 升级为 S4 |
| S1/S2 | `exp01/q2a_estimator_pairing.py:343-348` N3 是同函数调两次 | **亲自确认**：`:343` 与 `:344` 实参逐字相同、纯函数 ⇒ 四个 `spurious_*` 恒 0 |
| S1/S2 | `exp05/e5_gates.py:131-137` G1c 与 G1d 逐位重复、且被门控量恒等于 0 | **亲自确认**：`:131-133` 与 `:134-137` 条件、阈值、取值三行全同；`var_w=Σw²v/(Σw)²` 在 `w→w/c²` 下分子分母同乘 `c⁻⁴` ⇒ 差恒 0 |
| S1 | `exp02/e4_gates_selftest.py:208,214,216` fail-open 门集；同仓正确范式在 `exp03:186,222` | **亲自确认**两条路径，并亲自把同仓对照行号读出来 |
| S1 | `branch_discriminator.py:32 TOL=1e-6` 从未参与任何比较、`:93` 实际阈值 1e-3 | **亲自确认**：`TOL` 只出现在 `:163` 的输出字典 |
| S1 | `exp05/e5_gates.py:191` 与 `:288-289` 是同一个量、容差差 4× | **亲自确认并加强**：`Tf` 是常数场 ⇒ `level_dev(...)["median_ratio"]` 与 `|median(recon_absolute(spf))/median(Tf)−1|` 恒等 ⇒ 不是「几乎相同」而是**同一个数** |
| S4 | exp05 门归档来自 `6a2dba59`(09-23)、门代码改于 `11cd1d2d`(10-02)，落后 9 天 | **亲自复核**归档内容（28 个旧门名 + `meta.n_gates=28`）与代码的 `obsolete_gate_names`，独立得出同一结论；提交号未自行复跑 git 复核，**标为「采信 S4 的 git 取证」** |
| S4 | `EXP05_TABLES.md:140` 仍写 28 条且无过期标记 | **亲自复核**：全 `results/` 目录 grep `EXPIRED|stale_archive|不得作为证据引用` **零命中** |
| S4 | 三个入口全部「缺陷被捕获后复现命令不失败」 | **亲自确认** `e5_gates.py:451`、并补出 S4 已指出但我强调的自指涉：`e5_gates.py:113` 默认 `--out` = 它自己宣告过期的那条路径 |

### 7.2 我**否决 / 降级**的子代理结论

| 出处 | 结论 | 我的处置与理由 |
|---|---|---|
| S2（S11） | 「把 `median(1/s)` 换成 `1/median(s)` 在 ramp 场上只改变 c_eff 约 0.6 %，远在 2e-1 之内 ⇒ G5 全部漏检」 | **否决该量化，保留该担忧为定性条目。** 子代理自己声明「我手工推算，**未执行**」。本单同样禁跑脚本，**无实测支撑的数字不进交付件**。降级为 S9（`builtin` 回落）的相邻建议。 |
| S2（S14） | 「I5 的 green 由脚手架的 `d − median(d)` 保证，而非被测 recipe 的鲁棒性」 | **降级为建议。** 现象属实（`exp03:315` 做了减法、`:299` 没做），但 `exp02_common.production_clip_sigma:96` **内部本就以中位数为裁剪中心**，故该减法是冗余而非伪造。作为「特异性」测试仍有效，但**不该按子代理的措辞记为构造恒绿**。 |
| S1（S13） | 「G7 裕度仅 1.7×」 | **采纳但改为自算**（1.49e-2 / 2.5e-2 ⇒ 1.68×），并把结论从「裕度偏薄」**加强**为「G5a 与 G7 是同一个数、两个容差」。 |
| S1（阈值表中的 `exp01/negatives_selftest.py` 条目） | 把 `exp01/negatives_selftest.py` 当作「全仓阈值出处最好的范例」并要求其余文件照办 | **不采信为结论**：该文件**不在本片 38 份成员内**，我未读，不据未读之文件下结论。S1/S2 的这条只作为**线索**记入 §7.3 的待前台项。 |
| S2（S30） | 「MC 噪声用 `pixel_variance` 造、报告的 σ 也由同一个 `pixel_variance` 配 `var_optimal` ⇒ 配对判据只验证矩代数自洽」 | **机理正确**（`q2a:77` 与 `:85-87` 我已亲自读到同源），但**该配对判据在 q2a 内没有出口**（`main():424` 恒 `return 0`），所以「自洽地绿」这件事**已经是 C3 的一部分**，不单列。 |
| S4 | 「`run/reverse_verify/...` 日志尾部有 `FileNotFoundError`/`IndexError`」 | **不采信为事实、只采信为线索。** S4 读的是仓内既有日志（未执行脚本，合规），但那些日志**不在我本片的 38 份成员内**，我未读，且**这些文件所依赖的 `run/RELEASE-02/L4-rebuild/norm/**` 输入既被 gitignore 也已不在磁盘**。降级为 §8 的待前台执行项。 |

### 7.3 采纳 S3/S5 的补充复核（我亲自跑过的命令）

```bash
grep -c "查证\|质疑前提\|负例" AGENTS.md            # -> 0  ⇒ P1 成立
grep -n "^## " AGENTS.md | head -12                # §4=科学自治 §5=文档维护 §8=提交纪律 ⇒ P1 成立
git grep -c "ASTROCS_FRAME_SNR" -- .               # -> 仅 3 个 absolute-snr 文件，无 lib/ ⇒ P4 成立
git grep -l "ACSD_FRAME_SNR" -- lib/ | head -3     # -> lib/ 下真实键名
git grep -c "空天稳健尺度" -- lib/                  # -> 仅 snr_estimator.h 1 处；module_adapters.cpp 已无
sed -n '7358,7367p' lib/infrastructure/scheduler/src/module_adapters.cpp   # -> 注释已是订正版 ⇒ P5② 成立
grep -n "方向性约束" docs/science/CONTROL_WEIGHT_SNR.md                     # -> :218 方向性约束已存在 ⇒ P5① 成立
python3 -c "import json;d=json.load(open('实验/absolute-snr/code/audit/results/route3/exp03_median_se_identity.json'));print(d['H2_weight_identity'])"
# -> {'max_rel_dev': 6.661338147750939e-16, 'archived_gate': 2.22e-16, 'within_gate': False} ⇒ B5 成立
python3 -c "import json;d=json.load(open('实验/absolute-snr/code/audit/results/route1/exp07_weight_and_coadd_identities.json'));print(d['identity_w_snr2_over_fref2'])"
# -> {'max_rel_dev': 5.561195462496615e-16, 'within_4_ulp': True} ⇒ 同一恒等式两阈值两判决
```

### 7.4 我**否决 / 降级**的子代理结论（补）

| 出处 | 结论 | 处置 |
|---|---|---|
| S5 | 「`code/audit/` 36 个脚本中 35 个不链接生产 `lib/**` ⇒ 结构上不可能因真实管线缺陷翻红」 | **不作为本片结论采纳。** 该裁定出自 `docs/DERIVATIONS_P2.md:3-10`（G08-05 已定），属**既有裁定**而非本片新发现；且它引用的文件不在本片 38 份成员内，我未读。**降级为 §8 待前台项**：请把该裁定显式挂到 `audit/` 下每个脚本头上（本片 11 份 `audit/` 文件都受影响）。 |
| S3 | 「`run/` 下 6 个证据目录 + `docs/references/` 全失 ⇒ 五篇文档所有实测声明不可复核」 | **采纳其核验方法，否决其推论强度。** 我只确认「路径不存在」这一事实（`ls`/`git grep`）；**「所有实测声明均不可复核」是过强推论**——本片已用仓内 `results/*.json` 逐条复核了 EXP-02/03/05 的门与表，**那些是可复核的**。被复核不成立的部分限于引用 `run/` 侧证据的声明（`EXP-05:583-589` 的复现一致性、`SNR_PACK` §8 的行锚表、`EXP-03:906-909` 的 research/sources）。**收窄后采纳为 P12。** |
| S3 | 「EXP-05 `:396` 的 −0.92 %/−45.70 %/84 % 与归档不符」 | **采纳为 P8。** 我另行确认了同段的第二处自相矛盾（P9：≤0.2 % vs ≤0.25 %）与第三处（P10：3.7e-4 vs 7.7e-5），这两处 S3 也已列出，一并采纳。 |
| S5 | 「route2/exp03 引 Stigler 1977 与 Huber 1981 在 `docs/` 全仓零命中；仓内正本应为 Cramér 1946 §28.5」 | **不采信为结论。** 我未独立 grep 该两项文献名（`docs/SCIENCE_REFERENCES` 不在本片成员内），且**待联网核验**的文献存在性本就超出本单范围。降级为 §8 待联网核验项。 |
| S5 | 「`route3/exp04:59` 的 `MOFFAT4_FWHM_FACTOR` 是死变量且与生产 1.230310 冲突」 | **采纳为建议。** 我读原文时注意到该常量在本文件未被使用，但**未独立 grep 生产冻结值**，故只登记「死变量」这一半（已亲自确认），另一半标待核。 |



- **S3（`8b6132ba`）与 S5（`bce6ff3d`）在本窗口内未回报**。其中伪引方向我在本人阅读中**自行**做了一轮定点核验：`SNR_WEIGHT_RESEARCH_PACK.md:1` 的 `GAP_AUDIT.md §9.73 A44`（仓根无此文件）与 `docs/ASTROCS_DESIGN.md`（仓内只有 `docs/ACSD_DESIGN.md`）——**两条均已被仓内 `docs/engineering/UNRESOLVED_REGISTER.md:4114-4117` 第 81.1 条登记**，故本片**不**将其计为新发现（§4.4 已注明）。EXP-02/03/05 三份报告引用的 `docs/ASTROCS_DESIGN.md` 与 `docs/detail/registry/astrocs.*` 的**逐条伪引核验未完成**，标为**待前台续派**。
- `audit/route1`、`route2`、`route3` 三条路线的**常数一手出处**（如 `k_corr=1.4`、`1.230310`、`0.7316727929211932` 在仓外的一手文献锚）本片**未做联网核验**，一律标「待联网核验」。

---

## 8. 待前台执行 / 待联网核验（不由本单执行）

1. **重跑 exp05 门与表**：`bash 实验/absolute-snr/code/exp05/run_all.sh`（输入 `testdata/**` 在仓内，可跑）——必须与报告 §0/§7/附录 A 的门数与门名**同提交**更新（B1、B2、B4）。
2. **补退出码**：`e5_gates.py:451`、`p7_noise/exp1_injection_recovery.py:376`、`f_instr/exp5_psf_shape.py:107`、以及 `reverse_verify/snr_design` 与 `audit/` 下 10 个脚本（照抄同片 `exp02/e4_gates_selftest.py:254` 的现成写法）。
3. **把既有裁定显式挂到 `audit/` 每个脚本头上**：`docs/DERIVATIONS_P2.md:3-10` 已裁定 `code/audit/` 中 35/36 个脚本不链接生产 `lib/**`、绿灯不构成生产判别力证据——本片 11 份 `audit/` 文件全在此裁定内，但脚本头部无此声明（S5 提出，本单未采信为结论）。
4. **`exp05/run_all.sh:10` 的 `EXP05_OUT` 重定向后门仍读硬编码 `X.RESULTS`** ⇒ 门判旧档；需给 `e5_gates.py`/`make_tables.py` 加 `--in-dir`。
5. **`branch_discriminator.py:93` 的恒红判断**（R9）：需取一次真实产品的 `sigma_f 相对散布` 实测值定案；本单禁跑脚本，未取。
6. **待联网核验**（9 项，S5 给出，本单未执行）：Stigler 1977、Huber 1981 的存在性与被引内容；`p7lib.py:9-27` 的 6 个 DOI；`Fruchter & Hook 2002`；`archived_v4_max_dev=3.01e-16` 的出处；Rousseeuw & Croux 1993 的两个 DOI 冲突；M16 场景 `zp_ab` 实测值 vs `exp1` docstring 硬编码 22.635；`SKY=15.9/61.7` 是否真有对应 P1 产物；P-CST-16 的 `tile_width=512` 页内定位。
7. **`audit/route1|2|3` 的常数一手锚**（`k_corr=1.4`、`1.230310`、`0.7316727929211932`）在仓外的一手文献锚未做联网核验。
8. **AGENTS.md 改名面**：本片 5 份文档共约 30 处 `ASTROCS→ACSD` 路径与 `ASTROCS_FRAME_SNR→ACSD_FRAME_SNR` 符号未同步（P2、P4）。属跨片系统性问题，建议前台统一处置而非逐片改。

---

## 9. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
export LC_ALL=C

# ── 0) 基线
git -c core.quotepath=false log --oneline -1            # 期望 f9650dd0

# ── 1) 成员清单与行数（片 EXP-absolute-snr-003）
sed -n '2029,2034p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml
sed -n '2037,2074p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
  | sed 's/^ *- "//; s/"$//' > /tmp/members_sid003.txt
wc -l < /tmp/members_sid003.txt                          # 期望 38
xargs -d '\n' wc -l < /tmp/members_sid003.txt | tail -1   # 期望 8870 total

# ── 2) B1：exp05 门—归档—报告三方脱钩
#   2a) 现行代码自宣告归档 EXPIRED
grep -n "EXPIRED\|obsolete_gate_names\|stale_archive_notice" \
  实验/absolute-snr/code/exp05/e5_gates.py
#   2b) 归档仍是旧 28 门
python3 -c "import json;d=json.load(open('实验/absolute-snr/results/exp05_e5_gates.json'));\
print(d['meta']['n_gates'], len(d['gates']));print([g['gate'] for g in d['gates']][:20])"
#   2c) 现行代码门实例数（21 调用点，3 个在循环内）
grep -c "X\.gate(gates" 实验/absolute-snr/code/exp05/e5_gates.py   # 期望 21
#   2d) 报告仍写 28 与旧短名
grep -n "28 条\|ALL_GATES_PASS = True" 实验/absolute-snr/docs/EXP-05-ABSOLUTE-SNR.md
#   2e) 新表无过期标记
grep -rn "EXPIRED\|stale_archive\|不得作为证据引用" 实验/absolute-snr/results/ || echo "零命中"

# ── 3) B2：门脚本恒 return 0
sed -n '447,451p' 实验/absolute-snr/code/exp05/e5_gates.py
sed -n '111,115p'  实验/absolute-snr/code/exp05/e5_gates.py   # 默认 --out 同一路径
sed -n '24p'       实验/absolute-snr/code/exp05/run_all.sh
#   同片正确写法对照
sed -n '254p'      实验/absolute-snr/code/exp02/e4_gates_selftest.py

# ── 4) B3：1.3127 归因在全仓的存活点
git -c core.quotepath=false grep -n "UNCLIPPED MAD" -- .
git -c core.quotepath=false grep -n "1\.3127" -- .
sed -n '225,236p'  实验/absolute-snr/docs/EXP-05-ABSOLUTE-SNR.md   # EXP-05 自己的订正
sed -n '83,87p;128p' 实验/absolute-snr/code/reverse_verify/snr_design/exp5_error_budget.py
python3 -c "import json;d=json.load(open('实验/absolute-snr/code/reverse_verify/snr_design/exp5_error_budget.json'));\
print([(s['scenario'][:40],s['eps_phase1'],round(s['eps_total'],4),round(s['snr_error_pct'],2)) for s in d['partB_reported_sigma_budget']['scenarios'][:1]])"
grep -n "assert\|sys.exit\|raise " 实验/absolute-snr/code/reverse_verify/snr_design/exp5_error_budget.py || echo "零门"

# ── 5) F3：exp03 报告 gate_not_degenerate 计数与归档不符
sed -n '795p' 实验/absolute-snr/docs/EXP-03-REGIONAL-SIGMA.md
python3 -c "
import json,collections
d=json.load(open('实验/absolute-snr/results/exp03_e4_gates.json'))
for g in d['gates']:
    if g['gate']=='analytic.gate_not_degenerate':
        v=g['structured_verdicts']; print('n=',len(v),dict(collections.Counter(v)))"
#   期望 {ok:7, fail_closed:3, diagnostic_only:2}，而报告写 6/2/3（和为 11）

# ── 6) R4：exp03 同一数字两处写法
python3 -c "
import json
r=json.load(open('实验/absolute-snr/results/exp03_e1_analytic.json'))['a4_formula']['rows']
m=min(r,key=lambda x:x['abs_diff']); print('min abs_diff =',m['abs_diff'],'slope',m['slope'],'box',m['box'])"
sed -n '269p;781p' 实验/absolute-snr/docs/EXP-03-REGIONAL-SIGMA.md   # 期望 2.7493e-05

# ── 7) 恒真门定点复核（S4）
sed -n '283,286p' 实验/absolute-snr/code/exp03/e4_gates_selftest.py
sed -n '147,148p;157,158p' 实验/absolute-snr/code/exp02/e4_gates_selftest.py
sed -n '190p'     实验/absolute-snr/code/exp02/e1_analytic_scan.py
sed -n '253,256p' 实验/absolute-snr/code/exp03/e4_gates_selftest.py
sed -n '343,348p' 实验/absolute-snr/code/exp01/q2a_estimator_pairing.py
sed -n '127,137p' 实验/absolute-snr/code/exp05/e5_gates.py
sed -n '351,354p' 实验/absolute-snr/code/exp05/e5_gates.py
sed -n '448,450p' 实验/absolute-snr/code/exp02/exp02_common.py
sed -n '41,43p'   实验/absolute-snr/code/audit/route2/exp04_moffat4_sigma.py
sed -n '124p'     实验/absolute-snr/code/audit/route1/exp01_robust_statistics_constants.py
sed -n '47,48p'   实验/absolute-snr/code/audit/route3/exp03_median_se_identity.py
sed -n '17p;32p;93p' 实验/absolute-snr/code/reverse_verify/frame_snr/branch_discriminator.py
sed -n '143,144p;150p;156p' 实验/absolute-snr/code/reverse_verify/frame_snr/branch_discriminator.py

# ── 8) fail-open 门集
sed -n '35,36p;205,217p;240,241p' 实验/absolute-snr/code/exp02/e4_gates_selftest.py
sed -n '372,375p;384,386p'       实验/absolute-snr/code/exp05/e5_gates.py
sed -n '191,193p;205,207p'       实验/absolute-snr/code/exp03/e4_gates_selftest.py

# ── 9) 自愈：复现动作覆写证据面
sed -n '28,29p' 实验/absolute-snr/code/reverse_verify/snr_design/run_all.sh
sed -n '25p;35p' 实验/absolute-snr/code/reverse_verify/snr_design/audit/run_all_audit.sh

# ── 10) 已登记、不计为新发现的伪引落点
grep -n "GAP_AUDIT\|ASTROCS_DESIGN" 实验/absolute-snr/docs/SNR_WEIGHT_RESEARCH_PACK.md
ls GAP_AUDIT.md docs/ASTROCS_DESIGN.md 2>&1
sed -n '4114,4118p' docs/engineering/UNRESOLVED_REGISTER.md   # 已登记第 81.1 条
```

**本单的禁行清单（已在执行中遵守）**：未 `git add/commit/checkout/reset/stash`，未 `git rm --cached`；未编译、未跑 `ctest`/`pytest`/构建/任何 `实验/absolute-snr` 下的 `.py`；未读 `/tmp/acsd_g08/`；未修改仓内任何文件（唯一写入为本交付件）。
