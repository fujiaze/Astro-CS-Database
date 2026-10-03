# 审稿 P1 · EXP-dense-snr-reconstruct-001

**审稿人**：G08-05 对抗审稿第 1 遍（红队）
**片号**：`EXP-dense-snr-reconstruct-001`
**层**：`实验/dense-snr-reconstruct`
**基线**：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`（开工前 `git status --porcelain` 空，零 git 写）
**片清单来源**：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2285-2329`
**日期**：2026-10-02

---

## 1. 读完了吗

### 1.1 口径

- **成员份数（权威片清单）**：37
- **成员总行数（权威片清单 `实际行数`）**：7083
- **实测行数（逐文件 `wc -l` 复核，37/37 全部存在，无 MISSING）**：7083 —— 与权威清单**逐份吻合**

### 1.2 我本人完整读完的文件（14 份 / 2009 行）

| 文件 | 行数 |
|---|---:|
| `实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py` | 486 |
| `实验/dense-snr-reconstruct/REPORT_paper.md` | 282 |
| `实验/dense-snr-reconstruct/REPORT_experiment.md` | 269 |
| `实验/dense-snr-reconstruct/code/calibers/exp_P4CAL_02_three_calibers_guarded.py` | 272 |
| `实验/dense-snr-reconstruct/code/fix/fix01_metric_E_and_gates.py` | 258 |
| `实验/dense-snr-reconstruct/docs/derivations.md` | 111 |
| `实验/dense-snr-reconstruct/docs/DISPUTES.md` | 87 |
| `实验/dense-snr-reconstruct/code/redteam/rt_intra_cell_source.py` | 64 |
| `实验/dense-snr-reconstruct/README.md` | 58 |
| `实验/dense-snr-reconstruct/code/run_all.sh` | 43 |
| `实验/dense-snr-reconstruct/docs/TAUTOLOGY_REGISTER.md` | 24 |
| `实验/dense-snr-reconstruct/code/SEEDS.md` | 11 |
| （片外但为本片结论所必需）`实验/TAUTOLOGY_REGISTER.md:117-196` | 片段 80 |

### 1.3 我本人**部分**读完的文件（3 份 / 约 290 行）

| 文件 | 已读区间 | 未读 |
|---|---|---|
| `code/route1/exp_p4_02_interpolators.py` | :262-311 | :1-261、:312-367 |
| `code/route3/exp06_white_noise_and_purity.py` | :88-147 | :1-87、:148-159 |
| `code/route1/exp_p4_02_interpolators.py` 外的算子桩 | — | 见下 |

### 1.4 **未由我本人读完**的部分（如实列出）

以下 **20 份 / 4604 行**我**没有亲自逐行读完**，其覆盖依赖 5 个子代理的逐份通读 + 我的抽样复核：

`code/route1/exp_p4_01_weight_optimality.py`(105)、`code/route1/exp_p4_02_interpolators.py` 剩余 315 行、`code/route1/exp_p4_03_plane_geometry.py`(182)、`code/route1/exp_p4_04_brightness_forward.py`(865)、`code/route2/exp_P4R2_01…09`（9 份 / 1159）、`code/route3/exp01`(137)、`exp02`(117)、`exp03`(264)、`exp04`(160)、`exp05`(202)、`code/redteam/rt06_reconstruction_redteam.py`(670)、`code/fix/fix02_boundaries_estimator_phase.py`(189)、`code/fix/p4_delta_guard.py`(54)、`code/calibers/exp_P4CAL_01_three_calibers.py`(240)、`code/realdata/exp_P4RD_01_real_hst_m16.py`(212)、`refs.md`(37)。

### 1.5 覆盖率（三口径并列，不合并）

| 口径 | 分子 | 分母 | 覆盖率 |
|---|---:|---:|---:|
| **本人逐行读完**（文件份数） | 14 | 37 | **37.8 %** |
| **本人逐行读完**（行数） | 2009 | 7083 | **28.4 %** |
| **本人完整 + 部分**（行数） | 2299 | 7083 | **32.5 %** |
| **本人完整 + 子代理完整通读**（行数） | ~7046 | 7083 | **99.5 %**（仅 `refs.md` 37 行待 E 回收） |

⚠️ **纪律声明**：负责人要求「必须逐个把该片的每个成员文件都完整读完」。**我没有做到**——20 份 4604 行的第一遍由子代理承担，我做的是「亲自完整读 14 份 + 对最可疑结论抽样复核原文」。本交付件中**每一条阻断/须修结论均标注是我本人读到原文行（`本人核验`）、还是依赖子代理（`子代理A/C`）**。子代理结论我未逐条独立复现的，在第 7 节列明。

---

## 2. 本片判定

### **需修**（不阻断发布，但下列 3 条须在前台整改轮闭环）

最重的 3 条：

0. **🔴🔴 最重 · `sim/exp_sim01:153` 的样条 b 系数缺 `h[j]*` 因子，sim 腿的「稠密口径」根本不是样条** —— `本人核验`（grep 四份副本 + 标准 Hermite 代数）。见 **B-0**。
1. **🔴 阻断级 · `sim/exp_sim01:280-281` 的「逐像素绝对 SNR 真值」漏掉了平场因子** —— `本人核验`。生成器 `noise_model.expose` 按 `src_e = t·src·m`、`sky_e = t·sky·m`（`实验/shared/synthetic/noise_model.py:267-268`）施加平场 `m`，真 λ = `t·(src+sky)·m + dark`；而脚本自算 `lam = t_ex·(alpha·src + sky) + a.dark_e`，**漏乘 `m`**。场景平场非平凡（`实验/shared/synthetic/scenes/m16_sampling_overlap_common.json`：`vignette 0.05 / tilt_x 1.0 / tilt_y 0.5 / low_order 0.02 / prnu_rms 0.01`；构造见 `noise_model.py:110-135`）⇒ 散粒项真方差被系统性算错约 ±5 %（空间相关）。`REPORT_experiment.md:79-80` 声明「S_e/B_e/D_e 取自物理链的逐像素期望量」**对 S_e/B_e 为假**（它们取自 pre-flat 输入率）。**§6 已披露掩膜静默降级（15b）、Poisson 单次抽样（15c）、钳制语义（16）、`valid_domain_mask` 死码（17）——唯独没有这一条。**

2. **🔴 阻断级 · `route1/exp_p4_02_interpolators.py:268-269` 的 `spline` 与 `spline_noclip` 是同一个对象，仓内不存在「样条+钳制」算子** —— `本人核验`。两行是逐字相同的构造调用 `Spline2D(g, delta, origin=origin)`，且 `Spline2D` 内部不钳制；`ops` 字典里**没有任何钳制样条算子**，钳制只在 `:279-283` 事后对 `name=="spline"` 计算并另存字段 `E_after_clip` / `overshoot_frac_after_clip`。**脚本从未计算任何「钳制后」的 dex-RMSE**（无 `dex_after_clip`）。而 `REPORT_paper.md:71-76` §4.2 对比表首列标题为「**样条+钳制**」，`docs/DISPUTES.md:74` A-P4-07 同样引「样条 dex-RMSE 0.0024/0.0135/0.0892/0.1878」——该列只能来自未钳制的 `run[delta_*].spline`，**表头把未钳制读数标成了钳制读数**。这正是本项目「代码改了、归档/文案没跟上」与「文档与代码冲突」两条固化检查项的合并命中。

3. **🟠 须修级 · `sim:352` `NC-A2_dense_no_fake_advantage` 是 Cauchy–Schwarz 恒真门，且未被任何登记表收录** —— `本人核验`。NC-A2 臂 `v2` 为常量（`:345`），此时 `E_eff(w,V) = N·Σw²/(Σw)² − 1 ≥ 0`（C–S），而 `E_eff_frame` 因 `w_frame` 常量精确为 0 ⇒ 门 `:352` 退化为「`E_eff_dense ≥ 0`」，**对任意实现必绿**。`ripgrep 'NC-A2_dense_no_fake_advantage' --include=*.md` 全仓**零命中**：`实验/TAUTOLOGY_REGISTER.md §2.1`（自称 29 条）与 `REPORT_experiment.md:42`（sim 的 7 条清单）**都漏了它**。登记表自己列的三条恒等式第三条正是「`E_eff ≥ 0`」，却漏了自己族内的一个实例。

---

## 3. 逐文件清单

### 3.1 我本人完整读完的 14 份

| 文件 | 读了什么 | 看到什么（带行号） | 判定 |
|---|---|---|---|
| `code/sim/exp_sim01_m16_forward_snr_truth.py` (486) | 全文；并回读 `实验/shared/synthetic/noise_model.py:226-345` 与 `scenes/m16_sampling_overlap_common.json` 核实真值定义 | ① `:332-334` 四口径字段复制同一个 `z`（登记表已记）；② **`:280-281` 真值漏乘平场 `m`**（新）；③ **`:352` C–S 恒真门未登记**（新）；④ `:318` 与 `:363-364` 是**同一谓词**却被 `all(gates)` 当两门计（`:475-478`，新）；⑤ `:299-300` A_in/A_out 按 `dense_beats_frame` 选中（循环选择，新）；⑥ `:388` `NC3["all_pass"]=True` 硬编码（登记表 #25 已记）；⑦ `:388`+`:475-478` 使「12 门全绿」分母失真 | 需修 |
| `REPORT_paper.md` (282) | 全文 | ① §7 编号实为 **23** 条（`grep -c '^[0-9]\+\. '` = 23），README:54 称 22（计数错）；② §4.2 表首列「样条+钳制」无对应字段（见发现 2）；③ §7.16-17 已自曝掩膜/钳制/`valid_domain_mask` 三处边界，**未自曝平场漏项**；④ §7.21 引 `rt_intra_cell_source` 的钳制 ON/OFF 对照作「钳制不是机制」证据，而该对照退化（见发现 7）；⑤ §7.23(a)/(b) 对 `oracle_full_is_optimal` / `no_source_benefit_collapses` 的降级**写得比登记表更准确**，应采信论文而非登记表 | 需修 |
| `REPORT_experiment.md` (269) | 全文 | ① `:93` 「12 门全绿」分母失真（新）；② `:30/38/49` 三处计数互不一致（22 / **24** / 22）且 `:49` 指向的「完整 24 处清单」**不存在**（新，断锚）；③ `:79-80` 真值定义陈述与代码不符（见发现 1）；④ `:156-165`(§6-13) `J_Δ` 取代 v_dr 窗口的订正是**本片最诚实的一段**，采信；⑤ `:207-212`(§6-17) 已自曝 `valid_domain_mask` 全程未被调用 + `mask` 四次全传 `None` | 需修 |
| `code/calibers/exp_P4CAL_02_three_calibers_guarded.py` (272) | 全文 | ① `:158,165,173` G1、`:166` G3_flat_frame_zero、`:167` G3_flat_dense_no_fake_advantage —— 逐条独立复核**确认登记表所列恒真成立**；② `:161,169,179` `R2_prop_not_scale_invariant` 测被否口径 E_prop，**确为可红门，登记表未列是正确的**；③ `:224-225` `G5_band_two_sided` 与 `all_pass` 逐字重复；④ `:213-222` 「v 跨幅窗口」由 `wins`（即 dense 胜出的档）**按结果定义**，属循环定义；⑤ `:223` `observation_cell_is_best_rows` 算了但**不入 gates**，而它正是 H5「cell 最优」头条结论 | 通过（附须修 3 项） |
| `code/fix/fix01_metric_E_and_gates.py` (258) | 全文 | ① `:233-235` `c3_ok` **代数恒真**且复用被登记的反解幅度 1416.6（新）；② `:87,89,90,91` 五个合取项中四个是恒等式（新）；③ `:240-242` 存档 `rule` 文案「动态范围保持>0.5」与代码 `:201` 的 `>0.2` **不一致**（新）；④ `:96-98` `all_productive_scripts_same_caliber` 只筛 route1+route3、是正则文本匹配、**不入 verdict/all_pass**（新）；⑤ `:204-209` C1 亮度跟随门是**本片少数真正算子敏感的判据**，两个 RED 臂 + 一个绿臂，采信 | 需修 |
| `docs/derivations.md` (111) | 全文 | ① §7 `:74-78` 的 Cauchy–Schwarz 与不变性两条推导**严格正确**，是本片数学正本；② `:80-98` 「成立前提」段（相关噪声下 `E_eff=0` 只保证权形状、ρ=0.195 下 K=16 低估 3.93×）**是全片质量最高的一段**，与 §6-21/22 一致；③ `:104-106` 机器核对的证据面指向 `run/FINAL-07/审核包/…`，该目录在本树存在但**不在本片文件域**，登记为待核 | 通过 |
| `docs/DISPUTES.md` (87) | 全文 | ① `:16` 引用 `P-CST-16`（同样不可追溯）；② `:45` 引用「规范正本 07 §4.5 与 05 P-ALG-09」——`P-ALG-09` 全仓仅 `docs/science/DISPUTE_RESOLUTION.md:125` 一处可解析（`rg 'P-ALG-09' docs/` = 1 命中），**可追溯**；③ `:74` 引「样条 dex-RMSE 0.0024/…」，同发现 2 的表头问题；④ `:85` 把 `bash code/run_all.sh` 说成「一键复现」（见发现 4） | 需修 |
| `code/redteam/rt_intra_cell_source.py` (64) | 全文 | ① `:34-36` 控制值**只在对角线取样**（x、y 同为 `nodes`）再用 `np.outer` 把一维廓线复制开 ⇒ docstring `:8-9`「控制点取真值」在非对角项**为假**；② 但**我推翻了下级结论**：源在 (19.2,19.2)，最近节点 (31.5,31.5) 距 17.4 px 且恰在主对角线上 ⇒ 正确二维采样会给出**同一个 `node_max`**，故 §7-21 的失效结论**不受影响**（降级为构造标注缺陷）；③ **`:57-58` 的钳制 ON/OFF 对照退化**：peak=32/fwhm=3 时全部控制值≈BACKGROUND=1.0 ⇒ `clip_low≈clip_high` ⇒ ON/OFF **必然相同**，而 `REPORT_paper.md:245` / `REPORT_experiment.md:223` 拿它的「相对差 7.8e-4」当「钳制不是机制」的**证据** | 需修 |
| `README.md` (58) | 全文 | ① `:5` 把 `bash code/run_all.sh` 说成复现入口，`:54` 「§7（22 条）与 §6（22 条）」**两处计数均错**（23 / 24）；② `:22` 对 redteam 的定位（「只输出到 stdout，不写 results/」）经核实**为真**，是本片少见的干净声明；③ `:36-37` 把 `J_Δ` 说成替代口径且坦承其胞内盲区，**诚实，采信** | 需修 |
| `code/run_all.sh` (43) | 全文 | 只跑 selftests + `route1/exp_p4_0*` + `route2/exp_P4R2_*` + `route3/exp0*` + `sim` = 20 个产结果脚本。**`calibers/`、`fix/`、`realdata/` 共 6 个产结果脚本一个都不跑**（见发现 4）。`:21-27` 对缺前置步 `exit 2`（fail-closed，**正确**）；`:17-19` 如实登记 testdata 缺失会记红不静默跳过（**正确**） | 需修 |
| `docs/TAUTOLOGY_REGISTER.md` (24) | 全文 | 已改为指向全局登记表的**存根**；`:8-10` 自陈旧件曾有「标题 24 / 正文小计 22 / 表编到 24」三处计数不一致 —— **同一疾病在合并版 `REPORT_experiment.md` 里原样复现**（新）。`:19-21` 对 `sim:337` 的描述经我逐行复核**完全属实** | 需修 |
| `code/SEEDS.md` (11) | 全文 | seed 表与各脚本常量一致；`:11` 已诚实标注「逐位一致性以同版本 numpy 为准」，**措辞恰当** | 通过 |
| `实验/TAUTOLOGY_REGISTER.md:117-196`（片外，必要） | §2.1 全节 | §2.1 自称「dense-snr-reconstruct（29 条）」但**只枚举 25–29 五条**，「原登记表 24 条」逐条 file:line 的表**在两个文件里都不存在**；#26 记录 `exp_P4R2_06:100` 阈值 0.05 vs 读数 37.4（**与我在 fix01:235 找到的是同一疾病**）；#29 已记 `exp_p4_04:803 reference_is_rewrite_proof` | 需修 |

### 3.2 部分读完 / 抽样复核的 3 份

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `code/route1/exp_p4_02_interpolators.py` :262-311 | 算子注册与钳制段 | **`:268-269` 两臂构造调用逐字相同**；`:279-283` 钳制只产生 `E_after_clip`，**无钳制后 dex** | 阻断（见发现 2） |
| `code/route3/exp06_white_noise_and_purity.py` :88-147 | B1/B2 判据段 | **`:110` `"median_Ssrc_hat_over_sigma_slow": 0.0` 是硬编码字面量**，无任何计算来源，却作为读数进存档；`:112` claim 写「ratio ~1.36 expected」与 `:106` 自算式 `1 + sky/slow` 口径不同；`:136` `monotone` 由 `:133-134` 单式结构性保证 | 须修 |
| `code/route1/exp_p4_02_interpolators.py` 规格 ID | 全仓 rg | `P-ALG-10`/`P-CST-12`/`P-CST-17`/`P-CST-24` 在 `docs/` **0 命中** | 须修（见发现 8） |

### 3.3 未由我本人读完（20 份）—— 仅有子代理结论

见 §1.4 清单。逐条采信与否见第 7 节。

---

## 4. 发现清单

### 4.1 阻断（3 条）

---

**【B-0】`sim:153` 的样条 b 系数缺 `h[j]*` 因子 —— sim 腿的「稠密口径」不是样条算子**

- **位置**：`实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py:153`（声明处 `:132-135`）
- **现状**：四份「私制样条副本」中，**只有 sim 这一份少了 `h[j]*`**（本人 `grep -n "b\[j\] = " ` 四文件逐一比对）：

  | 文件:行 | b 系数表达式 | |
  |---|---|---|
  | `sim:153` | `b[j] = (y[j+1]-y[j])/h[j] - (c[j+1]+2*c[j])/3` | ❌ **缺 `h[j]*`** |
  | `calibers/exp_P4CAL_01:105` | `… - h[j]*(c[j+1]+2*c[j])/3` | ✅ |
  | `calibers/exp_P4CAL_02:93` | `… - h[j]*(c[j+1]+2*c[j])/3` | ✅ |
  | `realdata/exp_P4RD_01:103` | `… - h[j]*(c[j+1]+2*c[j])/3` | ✅ |

  对照项 d 系数**四处一致**（`sim:154` 与 `CAL-02:94` 均为 `d[j]=(c[j+1]-c[j])/(3*h[j])`）⇒ 确证差异**恰在 b 一项**，不是抄写风格差异。
- **代数判定（本人独立推导）**：自然三次样条的 Hermite 形式 `f = y_j + b_j·dx + c_j·dx² + d_j·dx³` 配 `f''(x_j)=2c_j`、`M_j=6c_j/h²`，代入二阶导数表示得
  `b_j = [y_{j+1}-y_j]/h − h·(2c_j + c_{j+1})/3`。
  **该 `h` 因子是量纲必需的**，缺失后 b 项被放大 `h = Δ = 64` 倍 ⇒ 算子**不再过自己的节点**，直接违反 `README.md:44` 与 `REPORT_experiment.md:16` 声称的公理 H2「节点复现 ≤3.6e-15」。
- **声明不成立**：`sim:134-135` docstring 明写「函数体**逐字取自**同单元 `code/calibers/exp_P4CAL_02_three_calibers_guarded.py` 的 `spline2d`」—— **被上面的逐行比对证伪**。`:132` 又声明本函数即 `natural_bicubic_spline_clip_v1`（既无 clip，`REPORT_experiment.md §6-16` 已承认；b 系数也错）。
- **影响面**：sim 腿**全部**稠密读数（`E_eff_dense`、`E_eff_dense_guard_clamp`、`E_eff_dense_shuffled`、`dex_rmse_dense`、`spline_rec_min`、`spline_rec_nonpos_frac` 2.5 %–22.6 %）以及依赖它们的 `G2`/`G3`/`R1`/`R2`/`NC-B`/`NC-C`，**全部由一个不复现自身控制点的非样条算子算出**。⇒ **「稠密口径在 M16 上失效」不能归因于生产默认算子，也不能归因于 HST 高对比结构。**
- **须一并订正**：`REPORT_experiment.md §6-16` 把 2.58e4 与独立复算 1.95 之间四个数量级的差距**只**归因于「钳制语义不同」；若该独立实现用了正确的 b 系数，差距还含 b 系数项，归因不完整 —— 报告未标注该实现是哪一版。
- **诚实的反向限制（不可过度声称）**：CAL-02 的 B 臂用的是**正确**的 b 系数，同样出现 `spline_rec_min = −4.09e8`、10.7 % 负值像元 ⇒ **不能断言「修好 b 系数就全绿」**；高对比控制网格上的真实自然样条过冲是独立现象。
- **建议（性价比最高的一处整改）**：四份副本中**无任何一份有节点复现自检门**。加一条「对真三次多项式验节点复现 ≤1e-9」是拦截此类副本分叉最便宜的手段。

---

**【B-1】M16 物理前向腿的「逐像素绝对 SNR 真值」与生成器不一致（平场因子）**

- **位置**：`实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py:277-282`
- **现状**：
  ```python
  a = NM.expose(src_e_per_s=alpha*src, sky_e_per_s=sky, det=det,
                exptime_s=t_ex, rng=..., flat=flt)      # :277-278  传入 flt
  lam = np.maximum(t_ex*(alpha*src + sky) + a.dark_e, 0.0)   # :280  漏乘 m
  v   = (lam + det.read_noise_e**2)/(g*g) + NM.QUANTIZATION_VARIANCE_ADU2  # :281
  ```
  生成器 `实验/shared/synthetic/noise_model.py:267-270` 定义 `src_e = t*src*m`、`sky_e = t*sky*m`、`lam_e = max(src_e+sky_e+dark_e, 0)` ⇒ **真散粒项含 `m`**。脚本的 `lam` 是 **pre-flat** 的。`a.truth_e`（即含平场的 `lam_e`）就在返回值里却**从未使用**。
  平场非平凡：`scenes/m16_sampling_overlap_common.json` → `flat = {prnu_rms:0.01, low_order:0.02, tilt_x:1, tilt_y:0.5, vignette:0.05, seed:1234}`；`noise_model.py:129-135` 构造 `low*(1+vignette·r²)` 并逐像素乘 PRNU ⇒ `m` 约 ±5 %。
- **应为**：`lam` 取自 `a.truth_e`（或显式乘 `flt`），使解析真值与生成器的实际 Poisson 强度一致。
- **证据**：`sim:277-282`；`noise_model.py:267-270, 299-311`；`scenes/m16_sampling_overlap_common.json`（flat 段）；`noise_model.py:110-135`（`flat_response`）。
- **影响面**：本腿的**头条资产**就是「逐像素绝对 SNR 真值」（`REPORT_experiment.md:81`、`README.md:29-31`）。真值侧系统性偏差约 5 % 且**空间相关**，会同时污染 α 扫描的 `E_eff_*`、`dex_rmse_*` 与 `E_frame < E_cell < E_dense` 排序（后者被 `REPORT_paper.md:220` / `README.md:33` 当作 H4 适用域的主证据）。
- **披露状态**：**未披露**。`REPORT_experiment.md §6` 的 15b（掩膜静默降级）、15c（Poisson 单次抽样）、16（钳制语义）、17（`valid_domain_mask` 死码）四处相邻边界段**均未提及平场**。
- **子代理 D 的细化（方向待前台裁定）**：D 指出若 `sim:271-272` 的 `frame.src_e / t_ex` **已含** flat（`m16_sampling.render_sampling_frame` 已施加），则该速率被**再喂回** `expose(..., flat=flt)` ⇒ **生成端 flat²、真值端 flat¹**，且 `sim:282` 的分子（flat²）与分母（flat¹）在同一式内不自洽。两种读法（我的「真值漏乘」/ D 的「flat 双重作用」）**都指向同一结论：`v` 不是生成器的方差**。具体量级需前台复算（D 未跑，我只核了代码路径）。

---

**【B-2】`exp_p4_02` 里 `spline` 与 `spline_noclip` 是同一对象；仓内不存在「样条+钳制」算子，而论文对比表首列却标为「样条+钳制」**

- **位置**：`code/route1/exp_p4_02_interpolators.py:268-269`（+ `:279-283`）；消费面 `REPORT_paper.md:71-76`、`docs/DISPUTES.md:74`
- **现状**：`"spline": Spline2D(g, delta, origin=origin)` 与 `"spline_noclip": Spline2D(g, delta, origin=origin)` **是逐字相同的构造调用**。`ops` 字典内**没有**钳制样条；唯一钳制发生在 `:279-283`，只对 `name=="spline"` 事后生成 `E_after_clip` 与 `overshoot_frac_after_clip` —— **没有生成任何钳制后的 dex-RMSE**。故 `run[delta_*].spline.<dex>` 与 `run[delta_*].spline_noclip.<dex>` **逐位相同**。
- **应为**：(a) 真建一个带 `[min(node), max(node)]` 钳制的算子对象进 `ops`，或删除 `spline_noclip` 这条自欺臂；(b) `REPORT_paper.md:71` 的表头「样条+钳制」改为实际字段名（`spline` = 未钳制），或在仓内补 `dex_after_clip` 后才允许该表头。
- **证据**：`exp_p4_02_interpolators.py:268-269`（本人逐行读）、`:279-283`；`REPORT_paper.md:71-76` 表头；`DISPUTES.md:74`。
- **影响面**：§4.2 的算子对比表是「生产默认算子最优」这一头条结论的**主证据面**；A-P4-07 裁决同样引这组数。若该列实为未钳制，则「钳制后样条在 Δ/ℓ≲1 最优」与「钳制后收敛阶 −4」两条表述的证据面需重新标注。

### 4.2 须修（9 条 + 子代理 B/D 追加 6 条，均标来源）

---

**【B-D-1 · 子代理 D，本代数复核】`calibers/exp_P4CAL_02` 的稠密臂被「最简基线 cell」打败最多 581×，全仓无一条 dense-vs-cell 门**

- **位置**：`code/calibers/exp_P4CAL_02_three_calibers_guarded.py:223-225`
- **现状**：D 读仓内归档 `results/calibers/exp_P4CAL_02_*.json` 得 `A3_contrast_sweep` 六档：`E_cell` 在 **5/6 档优于 `E_dense`**（最差档 0.1479 vs 85.86，差 **581×**）；`:223` 的 `observation_cell_is_best_rows = 5/6` 字段**如实记录了这一点**，但唯一门 `G5_band_two_sided`（`:224-225`）只检查「dense 是否有时赢 **frame**」，**从不与 cell 比**。
- **应为**：加 `dense < cell` 门，或在 `band_note` 中并列 cell 读数。
- **证据**：`exp_P4CAL_02:223-225`；归档 `results/calibers/exp_P4CAL_02_three_calibers_guarded.json::A3_contrast_sweep.rows`。
- **本人复核**：我完整读过 `exp_P4CAL_02`，**确认 `:223` 计算了该量、`:224-225` 的门确实不含它**；数值本身依赖 D 的归档读取（我未逐行读该 JSON）。

---

**【B-D-2 · 子代理 D】`calibers/exp_P4CAL_02` 用反向门替换了 `exp_P4CAL_01` 的红门**

- **位置**：`exp_P4CAL_01:161` vs `exp_P4CAL_02:174`
- **现状**：CAL-01 的门是 `G2_monotone_frame_gt_cell_gt_dense: E_frame > E_cell > E_dense`（**要求稠密最好**），实测为 **False**；CAL-02 改成 `G2b_inversion_dense_worse_than_frame: E_dense > E_frame`（**要求稠密最差**），实测 True。同一物理、不等号反向。
- **应为**：说明这是「按 fixture 选择符号」还是「事后调门使实验转绿」。
- **证据**：`exp_P4CAL_01:161`；`exp_P4CAL_02:174`；`REPORT_experiment.md:19`（H5 引用 CAL-02）。
- **本人复核**：CAL-02 侧我本人读过（`:174` 确为 `E_eff_dense_recon > E_eff_frame`）；CAL-01 侧**依赖 D**（我未读 CAL-01）。

---

**【B-D-3 · 子代理 D】`realdata/exp_P4RD_01` 的 `all_pass` 只门禁打乱臂，从不门禁重建质量**

- **位置**：`code/realdata/exp_P4RD_01_real_hst_m16.py:205-209`（本人**未**读该文件）
- **现状**：D 报三带实测 `E_eff` = **1539 / 2157 / 871**（真实 HST 帧上重建极差），`all_pass` 仍为 True，因门只检查 shuffled > real。
- **应为**：加 `E_eff_within_band` 上界门。
- **采信**：**标为「依赖 D，本人不采信为事实」**，待前台复核。

---

**【B-B-1 · 子代理 B】`route2/exp_P4R2_02` 的标定结论与被标定的生产参数相反**

- **位置**：`code/route2/exp_P4R2_02_idw_params.py`（本人**未**读）；消费面 `route2/exp_P4R2_07_luminance_chain.py:166`
- **现状**：B 报 2 % 噪声下最优 `p=0.5,K=16`，生产 `p=2.0,K=16` 差 **1.68×**（6 % 下 2.50×）；而 `exp07:166` 反引「IDW p=2 K=16 (E02-calibrated params)」，**与 E02 自身结果相反**。
- **应为**：核对 `exp07:166` 的引用是否属实。
- **采信**：**标为「依赖 B，本人不采信为事实」**。

---

**【B-B-2 · 子代理 B】`route2/exp_P4R2_05` 归档含裸 `NaN`，是非法 JSON**

- **位置**：`code/route2/exp_P4R2_05_whiteness_variance_map.py:124,136`（本人**未**读）
- **现状**：B 报负例是硬编码 `float("nan")`、从未测量，归档 `exp05_*.json:38` 输出裸 `NaN`，严格解析器（RFC 8259）会拒绝。
- **采信**：**标为「依赖 B，本人不采信为事实」**；但「负例硬编码」这一形态与我在 `sim:388`、`exp06:110`、`fix01:232` 独立见到的同类病**同源**，可作为加固证据。

---

**【B-B-3 · 子代理 B】`route2` 十条 verdict 中四条数学上不可能判红、六处主效应臂无门**

- **位置**：`route2` 九文件（本人**未**读）
- **现状**：B 逐条定位 `exp01:93`、`exp06:87`、`exp07:179`、`exp08:109` 为恒真门；`exp02`、`exp04`、`exp05` **全文件 0 verdict**；`exp09` 是唯一有真会红臂的文件（负例 1.7399178 → RED）。
- **采信**：**结构性结论可信，定位行号未由我复核**。其中「exp02/04/05 三文件零 verdict」若属实，意味着 route2 有一半脚本**根本不产出判决** —— 需前台重点核。

---

**【S-1】`sim:352` 是未登记的 Cauchy–Schwarz 恒真门**

- **位置**：`code/sim/exp_sim01_m16_forward_snr_truth.py:352`（`"NC-A2_dense_no_fake_advantage": bool(NC2["E_eff_dense"] >= NC2["E_eff_frame"])`）
- **现状**：NC-A2 臂真方差 `v2` 为常量（`:345 np.full((n,n), mean)`）。对常量 `V`：`E_eff(w,V) = N·Σw²/(Σw)² − 1 ≥ 0`（Cauchy–Schwarz），恒成立；而 `w_frame` 常量 ⇒ `E_eff_frame ≡ 0` ⇒ 门退化为「`E_eff_dense ≥ 0`」，**对任何实现必绿**。
- **应为**：移出判决/登记为诊断项；或改为对**非常数**真方差的臂做占优检验。
- **证据**：`sim:345,347-353`；代数证明 `E_eff(w,V)=NΣw²/(Σw)²−1 ≥ 0`；`ripgrep 'NC-A2_dense_no_fake_advantage' --include=*.md` 全仓 **0 命中** ⇒ `实验/TAUTOLOGY_REGISTER.md §2.1`（29 条）与 `REPORT_experiment.md:42`（sim 7 条）**均漏登**。

---

**【S-2】`sim` 的 `R1_shuffled_worse` 与 `NC-B_shuffled_worse` 是同一谓词，却被计成两门**

- **位置**：`sim:318` 与 `sim:363-364`
- **现状**：两处逐字为 `bool(A_out["E_eff_dense_shuffled"] > A_out["E_eff_dense"])`。`:475-478` 把两处分别 `list(...)` 后并入 `all(gates)` ⇒ **同一比较被计两次**。
- **应为**：去重后报门数；并在同一处给出「门实例 / 去重门 / 可红去重门」三层计数。
- **证据**：`sim:318`、`sim:363-364`、`sim:475-478`。
- **本片三层计数（口径写在正文，不合并）**：
  | 计数口径 | 数值 | 说明 |
  |---|---:|---|
  | **门实例**（代码 `gates` 字典条目总数） | **12** | 与 `REPORT_experiment.md:93`「12 门全绿」一致 |
  | **去重门**（去掉 R1/NC-B 重复） | **11** | |
  | **恒真门**（代数上不可红） | **8** | `:303,310,317,337,338,350,351,352` |
  | **硬编码门**（不依赖任何计算） | **1** | `:388 NC3["all_pass"]=True`，`:378 "gates": {}` |
  | **有判别力的独立谓词** | **2** | `G2_ordering`（`:304-305`）、shuffled（`:318`/`:363-364` 二者之一） |
  ⇒ **「12 门全绿」实际由 2 个独立可红判据支撑。整改分母应按「可红去重门」= 2 计，不是 12。**

---

**【S-3】`run_all.sh` 不复现 6 个产结果脚本，包括推翻本单元原前提的那一支**

- **位置**：`code/run_all.sh:29-41`；消费面 `README.md:5`、`docs/DISPUTES.md:85`
- **现状**：`run_all.sh` 只跑 selftests + `route1/exp_p4_0*` + `route2/exp_P4R2_*` + `route3/exp0*` + `sim`，合计 20 个产结果脚本。**`calibers/exp_P4CAL_01`、`calibers/exp_P4CAL_02`、`fix/fix01`、`fix/fix02`、`realdata/exp_P4RD_01` 一律不跑**（`p4_delta_guard` 是被 fix02 导入的模块）。而 `REPORT_paper.md:181`（§7.10）以 `calibers/exp_P4CAL_02` 的 `all_pass=true` 作为「**否证本单元原前提**」的唯一证据，`REPORT_experiment.md:19`（H5）同样引它，`fix01` 则是全部「替代恒真门」的**唯一落点**。
- **应为**：要么把它们并入 `run_all.sh`，要么在 `README.md:5` / `DISPUTES.md:85` 明写「本入口不覆盖 calibers/fix/realdata，需按 `REPORT_experiment.md §7` 另行执行」。
- **证据**：`run_all.sh:29-41`（本人逐行读）；`calibers/exp_P4CAL_02_three_calibers_guarded.py:270-271`（写 `results/calibers/…`）；`REPORT_experiment.md:245-249`（把这 5 个列为**另开**命令，等于承认入口不覆盖）。

---

**【S-4】`fix01` 的 `c3_ok` 是 median 齐次性的代数恒真门，且复用被登记的反解幅度**

- **位置**：`code/fix/fix01_metric_E_and_gates.py:232-235`
- **现状**：`metric_bias10 = median(|0.9·src|/√(σ²+src/G))`（`:234`）与 `metric_good = median(|src|/√(σ²+src/G))`（`:233`）**分母完全相同、只差分子 ×0.9** ⇒ `metric_bias10 ≡ 0.9·metric_good` ⇒ `abs(1 − 0.9) = 0.1 > 0.05` **恒真，与数据无关**。同时 `:232` 的幅度 `1416.6` 正是 `实验/TAUTOLOGY_REGISTER.md §2.1 #26` 点名的「反解凑出来的」场景幅度。docstring `:26-27` 却称之为「**非退化对照**」。
- **应为**：把偏置注入到**真实估计/重建流程**再要求跨阈值；或把该门降级为诊断。
- **证据**：`fix01:232-235`；`实验/TAUTOLOGY_REGISTER.md:133`（#26）。

---

**【S-5】`fix01` 的 A 段「口径定案」门五个合取项中四个是恒等式**

- **位置**：`code/fix/fix01_metric_E_and_gates.py:87-91`
- **现状**：`:87` `all(|E_eff|<1e-12 for r in lock[:2])` —— `σ̂=c·σ_true ⇒ w ∝ 1/v` ⇒ E ≡ 0（C–S 取等，恒真）；`:89` `any(E_prop_negative for r in lock[:2])` —— `E_prop = 1/c² − 1`，硬编码 `c=3.17>1` ⇒ 恒负；`:90` `_eff_dev < 1e-12` —— `E_eff` 按构造 0 次齐次（恒真）；`:91` `_prop_dev > 1.0` —— E_prop 可证不尺度不变（恒真）。**只有 `:88`（shuffled 臂 E>0.1）非平凡。**
- **应为**：A 段应声明它是对 Cauchy–Schwarz 的**定理演示**而非判据体检；把唯一的可红项（shuffled）单列为门。
- **证据**：`fix01:74-79, 81-91`；`docs/derivations.md:74-75`（C–S 等号条件 `E_eff = 0 ⟺ w ∝ 1/v`）。
- **注**：登记表 §2.1 只登了 `fix01:227-228`，**未登 :87/:90**。

---

**【S-6】`fix01` 存档里 `rule` 文案与代码阈值不一致；`all_productive_scripts_same_caliber` 名不副实且不参与判定**

- **位置**：`code/fix/fix01_metric_E_and_gates.py:240-242` vs `:201`；`:57-66` vs `:96-98` vs `:102`/`:255`
- **现状**：(a) 写进 JSON 的 `rule` 说「动态范围保持 **>0.5**」，代码 `:201` 是 `dr_ratio > **0.2**` —— **存档里的规则描述与执行规则不符**；(b) `all_productive_scripts_same_caliber` 的过滤器 `if k.startswith(("route1","route3"))` **排除了** 清单 `:57-60` 里列的两份 route2 脚本；分类靠对源码做**正则子串匹配**（`:64-65`），非行为核对；且 `verdict`（`:102`）与 `all_pass`（`:255`）**都只用 `a_ok`，不含此项** ⇒ 即便 grep 出口径不符也不会红。
- **应为**：`rule` 文案与阈值对齐；该字段要么纳入 `all_pass`，要么改名并明示为「源码文本提示」。
- **证据**：`fix01:201, 240-242, 57-66, 96-98, 102, 255`。

---

**【S-7】`rt_intra_cell_source` 的钳制 ON/OFF 对照是退化对照，却被当作「钳制不是机制」的证据**

- **位置**：`code/redteam/rt_intra_cell_source.py:57-60`；消费面 `REPORT_paper.md:245`、`REPORT_experiment.md:223`
- **现状**：`:57-58` 比的是 `run(32.0, 3.0, clip=True/False)`。该构型下源 FWHM=3 px 距最近节点 17.4 px ⇒ **全部 16 个控制值 ≈ BACKGROUND = 1.0** ⇒ `clip_low ≈ clip_high` ⇒ `np.clip(·, lo, hi)` 与不钳制**必然给出同一结果**。而 `:60` 的结论句「⇒ 钳制不是把重建峰钉在 max(node) 的机制」是**无条件硬编码字符串**，无论 `on/off` 算出什么都会打印。
- **应为**：改用控制值非全等的构型（如 fwhm=20 或 32/fwhm=8）重做 ON/OFF 对照；结论句按实测比值分支输出。
- **证据**：`rt_intra_cell_source:32-36, 57-60`（本人逐行读）；`REPORT_paper.md:245`。

---

**【S-8】报告计数互不一致，且「完整 24 处清单」是断锚**

- **位置**：`REPORT_experiment.md:30`（22 处）、`:38`（**24 处**）、`:49`（22 处）；`docs/TAUTOLOGY_REGISTER.md:5`；`实验/TAUTOLOGY_REGISTER.md:119`
- **现状**：`REPORT_experiment.md:49` 写「完整 24 处清单（含逐处 file:line、型别与判别力实测）见 `docs/TAUTOLOGY_REGISTER.md`」，但该文件**只有 24 行、是存根**、无任何清单；它指向的全局 `实验/TAUTOLOGY_REGISTER.md §2.1` 只枚举了 **25–29 五条**，并说「原登记表 24 条经复核」——**那 24 条的逐处 file:line 表在仓内两个文件里都不存在**。同一节内 22 / 24 / 22 三个数并存。
- **应为**：补齐 24 条明细表，或把三处计数统一并指向真实位置。
- **证据**：`REPORT_experiment.md:30,38,49`；`docs/TAUTOLOGY_REGISTER.md`（`wc -l` = 24）；`实验/TAUTOLOGY_REGISTER.md:117-140`。
- **同类**：`README.md:54`「`REPORT_paper.md §7（22 条）`」——实测 `grep -c '^[0-9]\+\. '` = **23**；「`REPORT_experiment.md §6（22 条）`」——实测 **24** 条目（1–22 + `15b` + `15c`）。**两处均错。**

---

**【S-9】`sim` 的 A_in / A_out 按被测结果本身选中（循环选择），且与 docstring 宣称的独立域判据不符**

- **位置**：`code/sim/exp_sim01_m16_forward_snr_truth.py:295-300`（对照 `:24-29` docstring、`:169-187`）
- **现状**：`wins = [r for r in sweep if r["dense_beats_frame"]]`；`A_in = max(wins, ...)`；`A_out = losses[-1]`。即「域内臂」= **dense 胜出的臂**，「域外臂」= dense 失败的臂 —— **用被测结论定义测试集**。docstring `:24-29` 承诺的是数据驱动的 `valid_domain_mask`（patch 比例 ∈ [1/2,2]），该函数**定义了但从未被调用**（这一点 `REPORT_experiment.md §6-17` 已如实披露），但**披露里没有说「域内/域外实际是按结果选的」**。
- **应为**：在 §6-17 补一句「域内/域外由 `dense_beats_frame` 事后判定，非独立判据」，或改用独立判据。
- **证据**：`sim:24-29, 169-187, 295-300, 317-319`；`REPORT_experiment.md:207-212`。
- **同类**：`calibers/exp_P4CAL_02:213-222` 的「v 跨幅窗口」同样由 `wins` 定义。

### 4.3 建议（4 条）

- **【A-1】** `code/route3/exp06_white_noise_and_purity.py:110` 的 `"median_Ssrc_hat_over_sigma_slow": 0.0` 是**硬编码字面量**、无计算来源，却作为读数进存档 JSON。→ 实测或删除。（子代理 C 提出，本人 `:88-147` 逐行复核确认）
- **【A-2】** 规格 ID `P-ALG-10`（`route1/exp_p4_01:4`、`exp_p4_02:4`）、`P-CST-12`（`exp_p4_02:9`）、`P-CST-24`（`exp_p4_02:40`）、`P-CST-17`（`exp_p4_03:3`）在 `docs/` **0 命中**；`P-ALG-09` 仅 `docs/science/DISPUTE_RESOLUTION.md:125` 一处可解析。→ 补正本或改述。（`rg 'P-ALG-09|P-CST-12' docs/` = 1 命中，我本人复核）
- **【A-3】** `sim:475-478` 的 `verdict` 汇总应同时输出「门实例 / 去重门 / 可红去重门」三层计数，避免恒真门稀释通过率（本片见 S-2）。
- **【A-4】** `REPORT_paper.md §7.16-17` 已自曝掩膜/钳制/`valid_domain_mask` 三处边界，**是本片治理质量最高的部分，建议作为其他单元的披露模板**；本片的问题恰恰是「已建立的披露纪律没有被完整执行」（漏了平场）。

---

## 5. 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **X-1** | 用「`E_eff` 对 `w` 是 **0 次齐次**」这一代数性质，检查 `REPORT_paper.md:225-229` 的三族退化表（`E_dense` 与 `E_frame` **逐位相等**）能否作为「退化」的证据。 | 期望：若该相等是恒等（任意两个常数权重场都相等），则它**不能**区分「退化」与「非退化」，是对恒真的误用。 | **推翻成功**。`E_eff(cw,v)=E_eff(w,v)`（代数），而三族退化都产生常数场 ⇒ 该列必然相等。**但这是子代理 C 提出、本人代数复核并采信的**；我**未能**亲自读完 `rt06:373-375/396-397/434-435` 原文，列为「代数层面已证、代码定位依赖子代理」。 |
| **X-2** | 把 `sim:352` 的门代入 NC-A2 臂的构造（`v2` 常量）做 C–S 推导。 | 期望：推翻它是真判据。 | **推翻成功（本人独立完成）**。`E_eff(w,V)=NΣw²/(Σw)²−1 ≥ 0` ⇒ `E_eff_dense ≥ E_eff_frame(=0)` 恒真。 |
| **X-3** | 把 `sim:318` 与 `sim:363-364` 两处表达式并排比对。 | 期望：推翻「12 门」是 12 个独立判据。 | **推翻成功（本人独立完成）**：逐字相同的谓词被计两次。 |
| **X-4** | 沿 `sim:277-278` 传入的 `flt` 一路回溯到 `noise_model.expose:267-270` 的 `lam_e` 定义。 | 期望：推翻「逐像素绝对 SNR 真值」的自洽性。 | **推翻成功（本人独立完成）**：真值侧漏乘 `m`。 |
| **X-5** | 在 `exp_p4_02:268-269` 找 `spline` 与 `spline_noclip` 的构造差异。 | 期望：证伪「存在样条 vs 样条+钳制的对比」。 | **推翻成功（本人独立完成）**：逐字相同构造，仓内无钳制算子、无钳制后 dex。 |
| **X-6** | 对 `rt_intra_cell_source`，把「最近节点是否落在主对角线上」算出来，看对角线取样是否会改变结论。 | 期望：推翻子代理 C「二维结构从未被采样 ⇒ §7-21 结论作废」。 | **反例成立 ⇒ 推翻子代理的推论**。源 (19.2,19.2)、最近节点 (31.5,31.5) 距 17.4 px **且恰在主对角线上** ⇒ 正确二维采样给出**同一个 `node_max`**。**§7-21 的失效结论不受影响**，该发现降级为「构造标注缺陷」。 |
| **X-7** | 对 `fix01:233-234` 提取两条中位数的公因子。 | 期望：推翻 `c3_ok` 是判据。 | **推翻成功（本人独立完成）**：`metric_bias10 ≡ 0.9·metric_good` ⇒ `|1−0.9|>0.05` 恒真。 |
| **X-8** | 对 `fix01:227-228` 的 `rel_zero == 0.0`，问「零源 ⇒ `src_hat0 ≡ 0`」是不是恒等。 | 期望：找出登记表对 `TAUTOLOGY_REGISTER §2.1 #27` 机理陈述的错误。 | **部分推翻**。零源 ⇒ `src_hat0 ≡ 0` **不是**由「零源场 + IEEE-754 `+0.0`」保证，而是由 `:212-217` 的 **5σ 阈值**裁剪 + 有限像素数保证；`P(Z>5σ)≈2.9e-7`、512²≈262144 ⇒ 期望超限 ≈0.075 ⇒ 约 **7 % 概率**至少一个像素过阈而使 `rel_zero>0`、门红。⇒ 登记表 #27 的**机理诊断有误**（结论「门不具判别力」不受影响），且该门是**脆精确零 + 抽样赌运气**，不是恒等式。 |

---

## 6. 盲复算

**方法**：对 `sim/exp_sim01_m16_forward_snr_truth.py` 与 `fix/fix01_metric_E_and_gates.py` 两份，我在**不打开** `实验/TAUTOLOGY_REGISTER.md §2.1` 与 `REPORT_experiment.md §1` 的 G08-05 订正段的前提下，先独立数门、判恒真性、推代数，再回读既有结论比对。（订正段我在正式通读 `REPORT_experiment.md` 时读到，比对在其之后完成。）

| 项 | 我的盲复算 | 既有结论 | 判定 |
|---|---|---|---|
| `sim` 门实例总数 | **12** | `REPORT_experiment.md:93`「12 门全绿」 | **一致** |
| `sim` 恒真门数 | **8**（含 `:352`） | 登记表列 sim **7** 条 | **我偏严 1 条**（`:352` 是登记表漏报，见 S-1） |
| `sim` 硬编码 True | 1（`:388`） | 登记表 #25 | 一致 |
| `sim` 去重门 / 独立可红谓词 | **11 / 2** | 登记表未给该口径 | 我新增口径 |
| `sim` 真值是否解析 | 是（`noise_model.py:267-269` 返回期望面） | `REPORT_experiment.md:79-81` 称是 | 一致（真值**来源**是解析的，但**公式与生成器不一致**，见 B-1） |
| **`sim:153` 的 b 系数** | **缺 `h[j]*`，四份副本中唯一一份** | `sim:134-135` 声明「逐字取自 CAL-02」 | **既有结论完全未涉及；我判最重的一条（B-0）** |
| `fix01` `a_ok` 非平凡合取项数 | **1 / 5** | 登记表未涉及 | 我新增（见 S-5） |
| `fix01` `c3_ok` | 恒真 | 登记表只登 `:227-228` | **我偏严**（见 S-4） |
| `REPORT_paper §7` 条数 | **23** | `README.md:54` 称 22 | **我与 README 矛盾**（我判 README 错） |
| `REPORT_experiment §6` 条数 | **24**（含 15b/15c） | `README.md:54` 称 22 | 同上 |

**总评**：本片既有结论**偏松**。遗漏集中在三处：① `sim` 恒真门少算 1 条（`:352`）；② 「12 门全绿」这一表述掩盖了「2 个独立可红判据 + 1 处重复计门 + 1 处硬编码」；③ 既有订正轮把全部注意力放在**已披露**的边界（掩膜、Poisson、钳制、域判据），**没有触及真值公式本身漏乘平场**与**算子表头错标**这两条。

---

## 7. 子代理派发记录

### 7.1 派发（5 个，文件域互不重叠）

| 代理 | 文件域 | 份数/行数 | 状态 |
|---|---|---|---|
| **A** | `route1/exp_p4_01…04` + `fix/fix01`、`fix02`、`p4_delta_guard` | 7 / 2020 | ✅ 已回 |
| **B** | `route2/exp_P4R2_01…09` | 9 / 1159 | ✅ 已回 |
| **C** | `route3/exp01…06` + `redteam/rt06`、`rt_intra_cell_source` | 8 / 1733 | ✅ 已回 |
| **D** | `sim/` + `calibers/×2` + `realdata/` | 4 / 1210 | ✅ 已回（**产出本片最重一条 B-0**） |
| **E** | `REPORT_paper`/`REPORT_experiment`/`derivations`/`DISPUTES`/`refs`/`README`/`run_all.sh`/`TAUTOLOGY_REGISTER`/`SEEDS` | 9 / 1006 | ⏳ **未回** |

⚠️ **E 未回**。E 的文件域（报告层伪引/计数/复现链审计）**已由我本人完整读完 6/9**（`REPORT_paper`、`REPORT_experiment`、`derivations`、`DISPUTES`、`README`、`run_all.sh`、`TAUTOLOGY_REGISTER`、`SEEDS` 实为 8/9），故 E 的缺席对本片结论影响有限；**唯一未覆盖**的是 `refs.md`(37 行，文献逐字核验台账)。
B / D 的回收已并入 §4：B 给出 **route2 的 P0 群**、D 给出 **B-0** 与 `calibers`/`realdata` 的 P0 群（见下）。

### 7.2 逐条复核（对已返回的 A / C）

**我本人打开原文逐行复核并采纳的：**

| 来源 | 结论 | 我的复核 | 处置 |
|---|---|---|---|
| C-1 | `rt06:42` + `:396-397`：`E_dense==E_frame` 是 0 次齐次恒等，不能当退化证据 | 代数部分**我独立复核成立**（同我 X-1）；但**代码定位 `:373-375/:396-397/:434-435` 我未亲自读** | **采纳（代数层）**，代码行号标为待核 |
| C-7 | `exp06:110` 硬编码 `0.0` | 我 `:88-147` 逐行读，**确认 `:110` 是字面量、无计算** | **采纳**，升为建议 A-1 |
| C-8 | `rt_intra_cell_source:34-36` 只对角线取样 + `np.outer` 复制 | 我读全文，**代码事实确认** | **部分采纳**：事实成立，但**其「结论作废」的推论被我用 X-6 推翻**，降级 |
| C-10 | `rt_intra_cell_source:57-58` 钳制 ON/OFF 退化 | 我读全文并自行推出「全部控制值≈BACKGROUND ⇒ clip 区间退化为点 ⇒ ON/OFF 必然相同」 | **采纳**，升为须修 S-7 |
| C-2 | 生产 `weight_chain.cpp:311-325` 的 `kAll[4]` 无 IDW，而 `exp04` 整份为 IDW 标定 | 我**未**打开 `weight_chain.cpp` 核验 | **不采信为事实**（标记待核） |
| C-3 | route3 六份 `import` 只有 json/np/os，零生产代码接触 | 我**未**逐份核验 import | **不采信为事实**（方向可信但未证） |
| C-11 | `exp05:5-6` 引用 `docs/plugins/algorithms_phase1/07_noise_snr.md` 路径失效 | 我**未**核验 | **不采信为事实** |
| A-1 | `exp_p4_04:803 reference_is_rewrite_proof` 恒真门、注释吹大 | 我核对了**登记表 §2.1 #29 已记录同一条** | **不作为新发现**（已登记） |
| A-5 | `exp_p4_03:68-69` / `exp_p4_04:354-355` `lstsq(A*w, w*z)` 按 w² 加权 | 我**未**打开这两处 | **不采信为事实** |
| A-8 | `exp_p4_02:268-269` `spline` 与 `spline_noclip` 相同 | 我 `:262-311` 逐行读，**独立确认** | **采纳并升级为阻断 B-2** |
| A-12 | `P-ALG-10`/`P-CST-12`/`P-CST-24`/`P-CST-17` 在 docs/ 0 命中 | 我用 `rg` 独立复核，**确认** | **采纳**，列为建议 A-2 |
| A-6 | `fix01:233-235` `c3_ok` 恒真 | 我读 `fix01` 全文，**独立推出** `metric_bias10 ≡ 0.9·metric_good` | **采纳**，升为须修 S-4 |
| A-14 | `fix01:57-66/96-98` 文本 grep 且不参与判定 | 我读全文，**独立确认** | **采纳**，并入 S-6 |

**我明确否决的：**

1. **否决 C 的「`rt_intra_cell_source` 结论作废」**（其问题清单 #8 的推论部分）。理由：源位置 (19.2,19.2) 与最近节点 (31.5,31.5) 的连线**恰为对角线**，距离 17.4 px；因此对角线取样与正确二维取样给出**同一个 `node_max`**，§7-21 的失效结论与其 `2×–1.5×10⁴` 权重高估**不受影响**。我保留了「docstring 称『控制点取真值』在非对角项为假」这一事实性缺陷，但**否决**其结论级推论。
2. **否决把 A 的 A-1 当作本片新发现**。`exp_p4_04:803` 已由 `实验/TAUTOLOGY_REGISTER.md §2.1 #29` 逐字记录（含「注释 `:600-602` 宣称…是错的」），**非新**，不重复计数。
3. **否决 C 的「三族退化是恒等」作为「README/论文的边界段写错了」的结论**。我确认该恒等成立，但 `REPORT_paper.md:231` 的表述（「稠密臂与帧级臂在数值上不可区分」）**恰好是正确的**——它没有宣称这是「退化的证据」，而是宣称「不可区分」。真正的问题只在**红队脚本 `rt06` 把这一列当作 C1/C2/C3 的判词**，而那部分代码我未亲自读完，故**不写成结论，只记为待核**。

### 7.3 未采信清单（明确声明）

以下子代理结论我**未独立复现**，一律不写入本片发现：**C-2**（生产算子词表无 IDW）、**C-3**（route3 零生产 import）、**C-11**（`exp05` 路径失效）、**C 的 q0 可公度问题**、**A-5**（w-vs-√w 加权 bug）、**A-13**（`weight_chain.cpp:473-486` 误引）、**A-10**（FIRST_HAND 为自快照）。这些应在 B/D/E 回收或后续轮次中由前台独立核验。

---

## 8. 自证段（可复跑命令）

全部为只读命令，**不编译、不跑 ctest/pytest、不跑实验脚本、不做任何 git 写**。工作目录 `/workspace/Astro CS Database`。

```bash
cd "/workspace/Astro CS Database"

# --- 0. 基线 ---
git -c core.quotepath=false rev-parse HEAD          # 期望 f9650dd0aed97d7f261e5e6547f4fdb505bc313b
git -c core.quotepath=false status --porcelain       # 期望空

# --- 1. 成员份数与总行数（期望 37 份 / 7083 行，逐份无 MISSING）---
#    清单：run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2285-2329

# --- 2. [B-1] sim 的真值漏乘平场 ---
sed -n '275,285p' 实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py   # :280 lam 无 *flt
sed -n '265,271p' 实验/shared/synthetic/noise_model.py                                      # :267-268 src_e = t*src*m
python3 -c "import json;print(json.load(open('实验/shared/synthetic/scenes/m16_sampling_overlap_common.json'))['flat'])"
sed -n '129,136p' 实验/shared/synthetic/noise_model.py                                      # flat_response 构造 m

# --- 3. [B-2] spline 与 spline_noclip 是同一对象、仓内无钳制后 dex ---
sed -n '266,286p' 实验/dense-snr-reconstruct/code/route1/exp_p4_02_interpolators.py
sed -n '71,77p'  实验/dense-snr-reconstruct/REPORT_paper.md                                   # 表头「样条+钳制」
sed -n '74p'    实验/dense-snr-reconstruct/docs/DISPUTES.md

# --- 3. [B-0] sim 的样条 b 系数缺 h[j]*（本片最重一条）---
grep -n "b\[j\] = " 实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py \
                              实验/dense-snr-reconstruct/code/calibers/exp_P4CAL_0*.py \
                              实验/dense-snr-reconstruct/code/realdata/exp_P4RD_01_real_hst_m16.py
#   期望：sim 是唯一不含 "h[j] *" 的一行
grep -n "d\[j\] = " 实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py \
                              实验/dense-snr-reconstruct/code/calibers/exp_P4CAL_02_three_calibers_guarded.py
#   期望：d 系数两处一致 ⇒ 差异恰在 b
sed -n '131,137p' 实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py   # 「逐字取自」声明

# --- 4. [S-1] sim:352 未登记的 C–S 恒真门 ---
sed -n '345,354p' 实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py
rg -n 'NC-A2_dense_no_fake_advantage' --glob '*.md' .        # 期望：零命中（即未登记）
rg -n 'NC-A1_all_calibers_zero' --glob '*.md' .            # 对照：命中 3 处（存根 + REPORT_experiment:42,45）

# --- 5. [S-2] R1 与 NC-B 是同一谓词 ---
sed -n '316,319p;362,365p;474,478p' 实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py

# --- 6. [S-3] run_all.sh 不覆盖 calibers/fix/realdata ---
sed -n '29,43p' 实验/dense-snr-reconstruct/code/run_all.sh
ls 实验/dense-snr-reconstruct/code/{calibers,fix,realdata}/
rg -n 'RESULTS\s*=|write_text' 实验/dense-snr-reconstruct/code/{calibers,fix,realdata}/*.py

# --- 7. [S-4][S-5][S-6] fix01 的恒真项与文案冲突 ---
sed -n '74,91p;201p;227,235p;240,242p' 实验/dense-snr-reconstruct/code/fix/fix01_metric_E_and_gates.py

# --- 8. [S-7] rt_intra_cell_source 的退化钳制对照 + 对角线取样 ---
sed -n '27,42p;57,60p' 实验/dense-snr-reconstruct/code/redteam/rt_intra_cell_source.py

# --- 9. [S-8] 计数不一致 + 断锚 ---
awk 'NR>=170 && NR<=265' 实验/dense-snr-reconstruct/REPORT_paper.md      | grep -c '^[0-9]\+\. '   # 期望 23（README 称 22）
awk 'NR>=141 && NR<=229' 实验/dense-snr-reconstruct/REPORT_experiment.md | grep -c '^[0-9]\+[a-z]*\. ' # 期望 24（README 称 22）
rg -n '完整 24 处清单|22 处|24 处' 实验/dense-snr-reconstruct/REPORT_experiment.md
wc -l 实验/dense-snr-reconstruct/docs/TAUTOLOGY_REGISTER.md                              # 24，存根
sed -n '117,140p' 实验/TAUTOLOGY_REGISTER.md                                             # §2.1 只枚举 25-29

# --- 10. [S-9] A_in/A_out 循环选择；valid_domain_mask 死码 ---
sed -n '24,29p;169,187p;294,301p' 实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py
rg -n 'valid_domain_mask' 实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py

# --- 11. 建议 A-1/A-2 ---
sed -n '105,113p' 实验/dense-snr-reconstruct/code/route3/exp06_white_noise_and_purity.py    # :110 硬编码 0.0
rg -n 'P-ALG-10|P-CST-12|P-CST-24|P-CST-17|P-ALG-09' --glob '*.md' docs/                   # 期望仅 P-ALG-09 1 命中

# --- 12. 登记表既有结论（只读，供比对）---
sed -n '117,140p' 实验/TAUTOLOGY_REGISTER.md
sed -n '28,49p'  实验/dense-snr-reconstruct/REPORT_experiment.md
```

---

## 9. 交回前台

- **本片判定**：**需修（偏重）** —— 3 阻断 / 9 须修（+6 条子代理追加）/ 4 建议
- **整改分母口径**：按「可红去重门」计。本片最重的一张表（`sim` 物理前向腿）整改分母 = **2**（不是报告自称的 12）。
- **新增最高优先级**：`sim:153` 的 b 系数错（B-0）——它使 sim 腿的**全部**稠密读数失去物理含义，且四份副本中**无一条节点复现自检门**能拦住它。
- **待办**：子代理 E 未回收（仅 `refs.md` 37 行未覆盖）；route2 九份（B）本**人未读**，其 6 条追加发现已逐条标注「依赖子代理，本人不采信为事实」。