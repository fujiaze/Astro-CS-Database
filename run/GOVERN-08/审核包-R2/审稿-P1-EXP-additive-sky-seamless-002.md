# G08-05 对抗审稿 第1遍 · 片 `EXP-additive-sky-seamless-002`

- 片号：`EXP-additive-sky-seamless-002`
- 层：`实验/additive-sky-seamless`
- 依据片清单：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2227-2284`
- 基线 HEAD：`f9650dd0aed97d7f261e5e6547f4fdb505bc313b`（工作树干净，`git status --short` 空）
- 划片依据：SRS-1 层内 LPT 均衡装箱（n=ceil(层行数/11000)，严格不跨层）
- 审稿人立场：红队，默认现行结论是错的

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威清单） | **50** |
| 实际读完份数 | **50** |
| 成员总行数（权威清单 `实际行数`） | **7875** |
| 实际读入行数 | **7875** |
| 覆盖率 | **100.0%**（份数 50/50，行数 7875/7875） |

**行数口径**：以 `wc -l` 对 50 个成员文件逐一实测求和 = 7875，与权威清单 `实际行数: 7875` 逐位一致（无缺号、无幽灵路径、无重复项）。

**未读完的部分**：无。50 份全部以 `read` 工具逐份完整读入（分 12 个批次；其中 `exp1_sky_poisson_snr.py`、`gen_synth.py`、`exp_variance_closure.py`、`ea_507_scale_scan.py` 分两段读完，末段已读至 EOF）。

**为取证额外读取的仓外/依赖文件（不计入覆盖率）**：
- `eng/tools/e2e/seam_footprint.py:240-360, 478-547`（生产门本体）
- `实验/additive-sky-seamless/code/sci_c_common.py:28-33, 46-70, 77-155, 163-166, 320-480`（共用库，`Gates` / `seam_steps` / `degenerate_steps`）
- `lib/algorithms/coverage/include/astro/phase2/sky_plane.h:8-13, 419-475`（生产天光面 API 契约）
- `实验/additive-sky-seamless/results/{seam_gate_coverage,c3_public_plane,c7_realdata}.json`、`results/audit_rework/{p3_kcorr/tables.md, route1/c6_identifiability_dof.json, route1/c7_share_vs_abs_weight.json, route2/e1_control_variance_median.json, route3/q3_median_variance.json, supp_control_variance/*.json}`（固化证据面）

**纪律自证**：未编译、未跑 ctest/pytest/构建、未执行任何仓内实验脚本主流程、未读 `/tmp/acsd_g08/`、零 git 写（无 add/commit/checkout/reset/stash/rm --cached）、未修改任何仓内文件（本交付件为唯一写入，且落在 `run/GOVERN-08/审核包-R2/`，与被审材料分离）。

---

## 2. 本片判定

### **阻断**

最重的 3 条：

| # | 位置 | 一句话 |
|---|---|---|
| **B-1** | `实验/additive-sky-seamless/code/seam_gate_coverage.py` ↔ `实验/additive-sky-seamless/results/seam_gate_coverage.json` | **代码改了、归档没重跑**：判据 2026-10-02 改（283+/79-），归档停在 2026-09-30；同一提交 `ef3dc516` 更新了 README:500 与 TAUTOLOGY_REGISTER:481/487 却没有更新唯一证据面 |
| **B-2** | `实验/additive-sky-seamless/code/reverse_verify/**`（10 份成员）↔ `实验/additive-sky-seamless/code/reverse_verify/m16_scene/README.md:11-13` | **实测数字零归档证据**：产物全部写进被 gitignore 的 `run/**`，而 README 给出 12 个具体读数并引用两份**仓内不存在**的证据文件 |
| **B-3** | `实验/additive-sky-seamless/code/c3_public_plane.py:109-115`（B2）、`:143-159`（B6） | **恒真门（代数恒等式型）**：B2 按 API 契约必然为 0；B6 的比较双方**逐位相同**，判据无判别力，却计入 7 门并被 README:179「7/7 PASS」引用 |

---

## 3. 逐文件清单（50 份）

> 每条给「读了什么 → 看到什么 → 判定」，全部带 `文件:行`。

### 3.1 根目录 c1–c7 与门探针（12 份）

| 文件（行数） | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `code/seam_gate_coverage.py` (573) | 全 573 行；并核对生产门 `seam_footprint.py:300-360`/`480-517` | ① `:214` `null_img=None` 默认把零台阶对照回退为被测图本身 ⇒ `cov_null≡cov` ⇒ 任何 `cov≤0.5` 必落 `zone=none`；② `:191/196/200` 检出限 `thr/bg` 在 `ctrl_mad≥1.0 ADU @ BG=300` 即 `blind=True`；③ `:232` `coverage_ok` 算了从不消费；④ `:341-343` `... if False else None` 死分支；⑤ `:461` docstring 仍写旧门名 `COV1`；⑥ `:484-487` COV4 在无噪臂是夹具解析恒等式（见 §5 反例 1）；⑦ `:552` COV10 与 COV8 增量重复 | **需修**（非阻断：全仓无外部调用者，见 §7-否决 1） |
| `code/seam_gate_gradient_scan.py` (227) | 全 227 行 | 夹具 `image():61-74` 造的是**严格 x 仿射、y 向常数**剖面，与 `:23-26` 的一阶推导前提完全重合 ⇒ `:138/154/166` 的「恢复」断言是闭式恒等式；`:196-198` 噪声 MC 的 σ 比门低 10× ⇒ `exceed_rate==0.0` 空过；`:189` 算了生产判据超门率却从不被断言 | **需修** |
| `code/c1_additive.py` (475) | 全 475 行 | `:17-18` docstring 的 A7/A8 声明与 `:372`/`:376-377`/`:385-387` 实现**结论相反**（A8 是否定关系）；`:93-94` `np.mean(x and [x])` 算出的 `sep` 从未使用；`:201`/`:328` `all([])` 空真；`:290`+`:308` 使 A6p 严格蕴含 A6；`:320-321` 声称 `final_gauge=0` 那一支由 A6p gauge 项承担，但 G≡0 时该项也是 0；`:378-382` A7b 是「钉死缺陷」反向门 | **需修** |
| `code/c2_multiplicative.py` (489) | 全 489 行 | `:18-22` docstring 与实现 6 条门中 5 条不一致（M2 5e-3→0.15、M3 2%→5%、M4 5×→3×、M5 ±3bin→±6bin、M6 命题反转）；`:52-53` 注入 `m_true` 与 `:127-128` 的 `m̂` **同为 degree-2 多项式** ⇒ 生成器落在估计器函数类内；`:269-275` 覆写 `wF` 的 sky/frames 却未重算 `est`，`:278` 的"吸收臂"除的是错配归一；`:421` `n_sigma>20` 度量的是**对注入幅度的偏离**而非检出显著性 | **需修** |
| `code/c3_public_plane.py` (181) | 全 181 行 | `:111-115` B2 恒真（见 B-3）；`:143-159` B6 恒真（见 B-3）；`:45` 未用 import、`:118` 未用变量；`:162-169` B7 用 `s["step"]` 而非 `excess`，与 `sci_c_common.py:387-388`「强结构背景必须看 excess」口径相反 | **阻断**（B2/B6） |
| `code/c4_seam_criterion.py` (166) | 全 166 行 | `:37-39` 注入 ramp 同时写进 `frames["B"]` 与 `sky["B"]`，`sci_c_common.py:427` 再相减 ⇒ 注入被**代数精确抵消** ⇒ N1 恒真（作者已在 desc 自陈）；`:14`/`:125` 与 `:15`/`:127` docstring 与代码阈值不一致（[1,5] vs [0.5,8]；「3×检测限」vs 硬编码 10 e-）；`:142` N5 用单边谓词而 `:85/:88` 声明双边；`:93-94` 死代码；`:154` N6 用 N=120 的偏度（SE=√(6/120)=0.224，约 3% 自发假红） | **需修** |
| `code/c5_weights.py` (279) | 全 279 行 | `:225-239` 的 `state_machine` 对 `:247` 的字面量期望 ⇒ W4 是纯定义自测且从不调生产；`:264-267` W5 断言生产**必须**分不清 max_iter/stalled（desc 自写「登记 FIX」）⇒ 修好即红；`:193-195` W2 用无误差棒点估计（与 W1p 的整改方向不一致）；`:198` 完整链路只取 `i=0` 单样本 | **需修** |
| `code/c7_realdata.py` (303) | 全 303 行 | `:178-183` R1 门条件只有 `all(isfinite(...))`（实测 `level_ratio` 跨度 0.939–1.050，幅度无任何断言）；`:290-291` R4 断言 `:286` 刚 `savefig` 的文件存在 ⇒ 构造性为真；`:225` `o.get("rank")==o.get("n_nodes")` 两键都缺时 `None==None` 为真；`:235` O(n) 反查、`:236` 死赋值、`:240` 每 cell 权重取「最后一个样本」静默覆盖 | **需修** |
| `code/sky_probe.cpp` (174) | 全 174 行 | 只读链接生产天光面，`:34` 引 `sky_plane.h:221`（实为 `:224`），`:35/:37` 的 `sky_plane.cpp:890/1861` 逐字为真；`:161-168` `p2_upm_control_variance` 直拍路径存在 | 通过（1 处行号陈旧） |
| `code/seam_gate_gradient_scan.py` 的归档面 `results/seam_gate_gradient_scan.json` | 比对提交时间 | 代码与归档同为 `54823d6f`(09-29)，**同步**，无漂移 | 通过 |

### 3.2 audit_rework 三路 + 补实验（21 份）

| 文件（行数） | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `code/audit_rework/run_all.sh` (78) | 全 78 行 | `:16` `set -eu` 正确；`:29-49` 列的 37 个脚本逐个存在、无幽灵调用；`:7` 自述「重跑不覆盖正本」为真（已核实 38 个 `OUT` 常量全指向 gitignored 目录）；`:54` 「无一 sys.exit(1)」为真（实测全子树零命中） | 通过 |
| `code/audit_rework/results/.gitignore` (6) | 全 6 行 | `:5 *` + `:6 !.gitignore`；`git ls-files` 核实该目录**只入 `.gitignore` 一个**，磁盘 39 个 JSON 全未入库；与自述一致，**无伪装** | 通过 |
| `code/audit_rework/supp_507_relstep/p5c_common.py` (337) | 全 337 行 | 独立重实现，纪律良好；`:5` 声明的上游 `docs/science/11_upm.md` **仓内不存在**（全仓只在 `run/*` 归档快照里）；`:5` 引 §7a/§9a 存在、`:174` 的 §4.3 与 `:5` 的 §4 口径不一；`:205` `np.interp` 遇 NaN 会静默传播 | 需修（伪引） |
| `code/audit_rework/supp_507_relstep/ea_507_scale_scan.py` (133) | 全 133 行 | `:103-118` 把 README §4.1 的 5.07 倍率当**对照值硬编**进归档；`:117` 自陈序列非单调（400/50 px）；`:120` 写 `P.RESULTS`（gitignored）；全程无门、无退出码 | 建议 |
| `code/audit_rework/supp_control_variance/production_chain_control_variance.py` (129) | 全 129 行 | `:47-83` 逐句复刻 `sampler.cpp:837-878`（**判据读的是复刻而非生产实现**，但已诚实标注）；`:98` `clip_frac = (nks < n).mean()` 的 `R*0+` 是冗余；无门、无退出码 | 通过（须登记为「复刻面」） |
| `code/audit_rework/route1/c2_kcorr_drizzle.py` (140) | 全 140 行 | `:116-125` 5 条 `structural_checks` 只记录不消费；`:126-132` 诚实标注「不构成对 1.3883 的复现声明」；`:4` 引 `11_upm.md §4.1`（不存在） | 建议 |
| `code/audit_rework/route1/c3_seam_gate.py` (147) | 全 147 行 | **全文件零门、恒 exit 0**（`:143-147`）；`:117-118` `Dlt − g2d_over_bg ≡ 0` ⇒ (e)「漏检面复现」是恒等式；`:134` `"median_rel_step": 0.0` 是**写死字面量**却作为 (f) 读数落盘；`:55-59` docstring 的 measured-vs-analytic 比较（两列相差 12.7×）**从未执行** | 需修 |
| `code/audit_rework/route1/c4_variance_ratio.py` (91) | 全 91 行 | `:73-79` 三条 conclusion 只记录不消费、恒 exit 0；`:5` 引 `11_upm.md`（不存在）；A/B/C 三案设计正确且双向（电平 vs 方差互补） | 建议 |
| `code/audit_rework/route1/c5_huber_efficiency.py` (92) | 全 92 行 | 理论腿与 MC 腿对照良好，`:74-78` 的 `unc=+inf ⇒ huber_w=1` 是诚实闭式；无门、无退出码 | 通过 |
| `code/audit_rework/route1/c6_identifiability_dof.py` (139) | 全 139 行 | `:102-105` 「H_solve 上恒绿」被明确识别为恒真门（**诚实登记**）；`:89-90` `kappa_identical` 实测为 **False**（归档 `results/audit_rework/route1/c6_identifiability_dof.json` 的 `global_weight_invariance.kappa_identical == false`），即该条是真的红；`:127-129` 方向订正诚实 | 通过（红已被 rollup 披露） |
| `code/audit_rework/route1/c7_share_vs_abs_weight.py` (98) | 全 98 行 | `:75` `exclusive = share > 1-1e-20` 实测 **False**（真红）；`:61` `abs(theta_abs-5.0) if False else ...` 死分支；`:87` 负例门 `abs(th_excl)<0.5` 无种子无关性保证 | 建议 |
| `code/audit_rework/route1/c8_scale_tolerance.py` (81) | 全 81 行 | `:50` `total_seq = math.fsum([]) if False else 0.0` 死分支且变量从未使用；`:42` 相对/绝对等价性统计**零处消费** | 建议 |
| `code/audit_rework/route2/e1_control_variance_median.py` (114) | 全 114 行 | `:59-61` 把「正本称低估 8.5%」记为 `doc_claim_underestimate_pct_at_N5` **待检验主张**（非既成事实）；`:80-83` 端到端腿方向诚实；`:101-107` chain 臂是解析算术 | 通过 |
| `code/audit_rework/route2/e2_kcorr_drizzle_mc.py` (105) | 全 105 行 | `:93-98` 负例（未相关分支 k≈1）方向正确；`:80-81` `s_bg` 用 `allsamp[::50]` 的全局 MAD 而 patch 统计用 `pool`，两口径不同源；无门 | 建议 |
| `code/audit_rework/route2/e7_smoothing_lambda_biasvariance.py` (121) | 全 121 行 | `:4-9` **幻觉锚取证**：grep 出 `lambda_bend` 零命中、`PHASE2_UPM §6` 是「假设」节，判定 `05_正向规格.md 判据01` 幻觉锚 —— 这是本片质量最高的取证之一；`:67-68` `off` 用外层 `rng` 而噪声用 `r2`，跨 rep 不独立；无门 | 通过（取证可信） |
| `code/audit_rework/route2/e9_node_spacing_representation.py` (105) | 全 105 行 | `:9-12` 第二个幻觉锚取证（`alpha/beta/h_absolute_min` 引 `11_upm §4.2` 无对应常数）；`:71` `y0 = ...` 死变量（`:73` 立刻覆盖）；`:66` 只用 64×200 采样点；无门 | 通过（取证可信） |
| `code/audit_rework/route3/exp03_median_variance.py` (127) | 全 127 行 | `:59` 明确把「正本低估 8.5%」标为**待检验**并给出正确方向（0.913x）；`:104-117` 负例 S=0 的 `1.314e26` 伪 ivar 复算准确；`:117` `"zero_effect_metric_zero": 0.0 == 0.0` 是恒真自证字段 | 通过 |
| `code/audit_rework/route3/exp04_kcorr_mc.py` (116) | 全 116 行 | `:12-15` **诚实登记**：明写「不宣称复现 1.3883 本身」；`:91-95` 恒等映射负例 k=1；`:102-105` 缩放不变性用不同 reps 与不同 seed 却逐位比较并附 MC 波动说明 | 通过 |
| `code/audit_rework/route3/exp05_huber_efficiency.py` (136) | 全 136 行 | 理论腿闭式 + MC 对拍；无门 | 通过 |
| `code/audit_rework/route3/exp09_zero_anchor.py` (85) | 全 85 行 | `:41-55` 三档权重口径（share 1.0 / 0.2 / absolute 1e-24）的岭回归收缩因子，设计正确；`:7-9` 引 `upm.cpp:562-564` 行号需另核 | 通过 |
| `code/audit_rework/route3/exp10_weight_arms.py` (158) | 全 158 行 | `:118-120` `ordering_matches_doc_16.1.3` 实测 **False**（真红）；`:75-89` 下游读数先扣掉真值天光项 `sky_term` 再度量 ⇒ 判据读的是**已知真值扣除后的残差**，比生产 `rel_step` 多一步；`:134` `negative_ok` 阈值 1e-3 无判红臂 | 需修（口径须登记） |
| `code/audit_rework/route3/exp11_scale_invariance.py` (100) | 全 100 行 | `:47` `sigma_eff` 在 `unc=+inf` 时走 else 分支给 `abs(inf)` ⇒ `:48` 判 inf ⇒ z=0；`:53-55` 三条 `floor_dead_*` 只记录不消费；`:87` 负例 `0.0 < 1e-6` 构造性为真 | 建议 |
| `code/audit_rework/route3/exp12_quality_factor.py` (79) | 全 79 行 | `:61-67` `PARTIAL_EXEMPT` 论证诚实且与 D-56 处置对齐；`:60` `monotone_in_q` 只记录不消费 | 通过 |

### 3.3 reverse_verify（13 份）

| 文件（行数） | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `code/reverse_verify/data_matrix/dtmlib.py` (299) | 全 299 行 | `:28` `ROOT = HERE.parents[5]` 路径深度正确；`:266-299` `skip_if_inputs_missing` 返回 **2** 并写 `all_pass: None`，**fail-closed 正确**；`:176-179` `fwhm=None` 后立即 `or` 的死赋值；`:34-35` `RESULT_DIR` 指向 gitignored `run/` | 须修（并入 B-2） |
| `code/reverse_verify/data_matrix/exp1_sky_poisson_snr.py` (265) | 全 265 行 | `:51-57` 「两轮才定案」的环形统计区设计有实测依据；`:113` 解析预测把同一估计量作用在**无噪声期望帧**上（消掉三个系统源），口径诚实；`:217-218` N1 门阈 1e-12 是纯代数恒等（`(f+δ)-(f+δ)`）；`:258-261` `all_pass` 与退出码正确 | 通过（并入 B-2） |
| `code/reverse_verify/data_matrix/exp2_mosaic_shape_difference.py` (271) | 全 271 行 | `:16-17` docstring **自己证明**共模场景下 `std_f≡0, R_SP0≡1` 精确成立 ⇒ `:233-236` G2 恒真；`:237` 因 `d_com→0` 走 `else inf` ⇒ `:238-240` G3 必过；`:253-261` G6 判据与 `reference_dark_samefield.note`「非判据」**自相矛盾**；`:30-34` 阈值历史诚实登记（但 `:19`「阈值设计先于看结果」按字面为假） | 须修（TAUTOLOGY_REGISTER:400 已登记 G2） |
| `code/reverse_verify/data_matrix/exp3_variance_closure.py` (215) | 全 215 行 | `:164` `((f1+δ)-(f2+δ))` vs `(f1-f2)` ⇒ E3 测的是 IEEE 减法；`:154-157` `dv`/`var_before` 是被弃原实现的死脚手架；`:203-206` E5 断言生产估计量**必须**偏差 >5%（修好即红）；`:176-179` `mean_only` 臂预测用 `src_e=0` | 须修（TAUTOLOGY_REGISTER:406 已登记 E3） |
| `code/reverse_verify/m16_scene/README.md` (58) | 全 58 行 | `:11-13` 给 12 个具体实测读数（0.00381 mag / 0.3358 mag / 88.2× / +0.078% / +0.028% / +0.074% / 0.5606 / 0.8615 / 0.8659 / +137.2 / +130.7 / +132.1 ADU²）——**仓内零归档**；`:7`/`:22`/`:27` 引 `run/RELEASE-02/paper/data/{STACKN32/M16FIX}/*.md` **两份都不存在**；`:11` 引 `star_detector.cpp:151`（实为 `:240`） | **阻断**（B-2） |
| `code/reverse_verify/m16_scene/exp_a6_seeing_aperture.py` (294) | 全 294 行 | `:41-50` 判据 P1/P2/P3 写死且含双向负例，设计良好；`:254`+`:261` P3a 用**同场景同种子渲染两次**断言逐位相同 ⇒ 测的是渲染器确定性（对 RNG 固定是有价值的回归测试，但被标为「负例判据」）；`:192-193` 增益 1.5 硬编而非读场景；`:284` 结果写 gitignored `run/` | 须修（并入 B-2） |
| `code/reverse_verify/m16_scene/exp_variance_closure.py` (181) | 全 181 行 | `:12-38` 四条判据的**估计量偏差归因**（ADU 取整致配对差分偏低 ~2%）写得非常扎实；`:66-68` `flat_blocks()` 直接 `return None`（死函数）；`:158-163` C1–C4 四门齐备且退出码正确；`:174` 结果写 gitignored `run/` | 通过（并入 B-2） |
| `code/reverse_verify/smooth_lambda/CRITERIA.md` (36) | 全 36 行 | `:3` 上游 `reverse_verify/README.md §3.3` **不存在**；`:31` C7 要求 A_base 与 A_scale(×1e9) 逐点相对差 ≤1e-6，但 `gen_synth.py:179/186-188` 的 `--scale` 是**仿真后纯标签缩放**（`help="输出单位缩放（仅标签，物理不变）"`），且生成器**没有缩放 λs** ⇒ 该判据测不到它声称的「λs 最优值对标度不变」；`:26` C2 的分母可被反向操作 | 须修（伪引 + 判据空转） |
| `code/reverse_verify/smooth_lambda/gen_synth.py` (207) | 全 207 行 | `:179` `--scale` help 逐字「仅标签，物理不变」；`:186-190` 缩放在仿真**之后**施加 ⇒ 确认 C7 是浮点舍入级自比；`:117-122` N_retained 取整个 patch（`PATCH×PATCH`）而非裁剪后，符合合同式；`:94-98` `tex` 固定纹理场决定 patch 空间 MAD | 须修（支撑 C7 判据失效） |
| `code/reverse_verify/smooth_lambda/convert_real.py` (44) | 全 44 行 | 纯二进制转换，无判据；`:10-13` 源是 gitignored `run/RELEASE-02/L4-rebuild/mosaic_out/p2_samples.json` | 建议（输入不可复现） |
| `code/reverse_verify/smooth_lambda/probe_real.py` (71) | 全 71 行 | `:57` `kc = 1.4` 第五处硬编；`:13` `json.load(open(SRC))` 无存在性守卫；`:62` 注释里的像素尺度是推测（`tile ~ 512*0.9586 arcsec?` 带问号）；无判据 | 建议 |
| `code/reverse_verify/smooth_lambda/describe_real.py` (53) | 全 53 行 | 纯描述性统计，无判据；`:11` 硬编 `real49.upmb` | 通过 |
| `code/reverse_verify/smooth_lambda/real_fidelity.py` (67) | 全 67 行 | `:5` 的 `removal_X = 1 - median(std_f(z))/median(std_f(y))` 与 `CRITERIA.md:10` 的 `removal_frac` 定义**逐字一致**（正向核对通过）；`:53-59` 双份落 gitignored `run/` | 通过（口径一致） |

### 3.4 单元根文档与定案（4 份）

| 文件（行数） | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `data/README.md` (55) | 全 55 行 | **逐条核对全部为真**：`:10` HST 文件在盘；`:12` 五项参数与 `sci_c_common.py:46/47/33/28/57-60` 逐字相符；`:18-19` `SEED_BASE=20260923` 与 sha256 派生相符；`:24` `COVER_GX` 与 `BOUNDARIES` 相符；`:39` 「实测位移 [0,0]/[0,0]/[8,6]/[10,8]」与 `results/c7_realdata.json` **四值逐个命中**；`:45-48` 15 个入口符号全部在头文件命中 | 通过（质量最高的一份） |
| `docs/control-variance-adjudication.md` (98) | 全 98 行 + 全部被引固化 JSON | ✓ `:27/:36-41` N=5 高估 9.53% 与 `c1_median_variance.json` 逐位相符；✓ `:47/:48/:49/:50` 四路读数逐位相符；✓ `:69` T3 列头与三个读数在 `tables.md:28,45` 逐字命中；✓ `:97` smooth-lambda 订正注真实存在；✗ `:95` `c3_public_plane.py:43-72` 无 `k_corr=1.4` 字面量（真值在 `sci_c_common.py:31`）；✗ `:22` 拉普拉斯 `0.335×` 四路均未复现（0.318/0.349/0.353/0.387），且它是 JSON 里的 `doc_claim_*` 键名即**待检验旧主张**；✗ `:31/:50` 「端到端 ±1.5%」只在 N=5 成立（N=20 为 +5.0%）；✗ `:50` 「低估 1.3–3.2%」实为 1.22–3.19% 且 N=20 是 +0.92% 高估；✗ `:57` 「N≈225–251」无 251 出处；✗ `:69` 「k_gauss N=5→1.63」与其强制的 k_gauss 列（1.6620）矛盾 | 须修（6 处） |
| `results/REVERSE_VERIFY_CANON.md` (41) | 全 41 行 | ✓ `:8-10` 诚实登记「结论速览从未回填」（`docs/smooth-lambda.md:18` 确为占位符）；✓ `:35-39` 四条复跑命令逐条对得上脚本；✗ `:3` `docs/ASTROCS_DESIGN.md` 与 `ENGINEERING_SPEC.md` **均不存在**（真名 `docs/ACSD_DESIGN.md`，且 §2.3 主题是 P3 守恒映射，与 λs 无关，λs 在 §2.5）；✗ `:41` `run/ROOT-CONSOLIDATION/logs/migration_rerun.md` **不存在** | 须修（3 处伪引） |
| `code/reverse_verify/smooth_lambda/build.sh` (20) / `sweep_all.sh` (19) | 全 20+19 行 | `build.sh:9-19` 真编译真 `lib/algorithms/coverage/src/upm.cpp`，`ROOT` 五级上溯正确；`sweep_all.sh:5` 同；`:9` 默认 `LAM` 与 CANON:37 给的 `SW_LAM` 逐字相同；**`:16-19` 子壳内 `echo` 吃掉退出码 ⇒ 脚本恒 exit 0**，与 `audit_rework/run_all.sh:14-15` 批评的正是同一失效形态 | 须修（恒 0 退出码） |

---

## 4. 发现清单

### 4.1 阻断（3 条）

---

#### **B-1　代码改了、归档没重跑：`seam_gate_coverage.json` 停在整改前，而 README 与恒真门台账都按整改后陈述**

- **位置**：`实验/additive-sky-seamless/code/seam_gate_coverage.py`（判据）↔ `实验/additive-sky-seamless/results/seam_gate_coverage.json`（唯一证据面）；被引用面 `实验/additive-sky-seamless/README.md:500`、`实验/TAUTOLOGY_REGISTER.md:481,487`
- **现状**：判据最后修改 `ef3dc516`(2026-10-02 05:16)，`git show --stat` 为 **283 insertions / 79 deletions**；归档最后修改 `751329ef`(2026-09-30 21:01)，**早 2 天**。同一提交 `ef3dc516` 同时改了 `README.md` 与 `TAUTOLOGY_REGISTER.md`，**一个 results 文件都没动**。
- **归档实测缺什么**（本人 `python3 -c` 逐键核对，非推测）：
  - 归档 `gates` 数组 **9 条**，id 为 `COV1_mirror_faithful / COV2_dilution_is_real / COV3_production_blind_verdict_green / COV4_coverage_reads_fraction / COV5_gate_fails_closed_on_low_coverage / COV6_gate_green_on_no_step_control / COV7_gate_red_on_full_amplitude / COV8_coverage_diagnostic_noise_limited / COV9_amplitude_independence`
  - 改后代码的门是 **10 条**，含 **`COV6b_noiseless_no_step_is_certified_pass`** 与 **`COV10_blind_arm_rejects_legacy_false_pass`** —— 归档**一条都没有**
  - 归档的 `COV6`/`COV8` 是**旧名**（`_gate_green_on_no_step_control` / `_coverage_diagnostic_noise_limited`），改后为 `_no_step_control_never_red` / `_noisy_arm_is_unresolved_not_pass`
  - 顶层键缺 **`selfchecks`**（`seam_gate_coverage.py:557` 会写）、缺 **`coverage_amplitude_grid_noisy`**（`:405`）、缺 **`legacy_vs_new_noisy`**（`:450`）
  - 归档**没有 `unresolved`/`UNRESOLVED`/`blind_to_gate`/`detection_floor_rel`/`blind_ratio`/`ctrl_side` 任何一个键**
- **应为**：同一提交内同时更新判据与其唯一证据面；判据改名时全仓 grep 旧门名并同步报告（`seam_gate_coverage.py:461` 的 docstring 至今仍写旧名 `COV1`，`grep -rn COV1 实验/` 命中 3 处：`TAUTOLOGY_REGISTER.md:335/487`、归档 JSON:922、代码 docstring:461）。
- **证据**：
  ```bash
  git -c core.quotepath=false log -1 --format='%h %ad %s' --date=short -- 实验/additive-sky-seamless/code/seam_gate_coverage.py   # ef3dc516 2026-10-02
  git -c core.quotepath=false log -1 --format='%h %ad %s' --date=short -- 实验/additive-sky-seamless/results/seam_gate_coverage.json # 751329ef 2026-09-30
  python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/seam_gate_coverage.json',encoding='utf-8'));print(list(d.keys()));print([r['id'] for r in d['gates']])"
  ```
  危害面：`README.md:500` 明写「可跑，**10 门**（`COV2`–`COV10`）+ 1 项不计门数的镜像自检，退出码 0」，`TAUTOLOGY_REGISTER.md:481` 明写「正确实现 10 门全 PASS、退出码 0；注入 A ⇒ **COV5/COV8/COV10 三门红**、退出码 1」。这两条陈述所依赖的证据面**不存在于仓内**——`COV10` 这条回归门在归档里根本没有。
- **口径**：整改分母层面，本片 1 个脚本的证据面 100% 陈旧；门实例层面，`seam_gate_coverage` 的整改分母声明为 10，实际归档只有 9 且名字全旧。

---

#### **B-2　reverse_verify 全套零归档证据，而 README 用 12 个具体读数与两份不存在的文件背书**

- **位置**：10 份成员代码 + `实验/additive-sky-seamless/code/reverse_verify/m16_scene/README.md:11-13`
- **现状**：
  - `dtmlib.py:34-35` `OUT_ROOT = ROOT/"run/reverse_verify/data_matrix"`、`RESULT_DIR = OUT_ROOT/"results"`
  - `exp_a6_seeing_aperture.py:87` `OUT = ROOT/"run/reverse_verify/m16_scene/a6_seeing"`；`:284` 写 `result.json`
  - `exp_variance_closure.py:54` `OUT = ROOT/"run/reverse_verify/m16_scene/closure"`；`:174` 写 `variance_closure.json`
  - `sweep_all.sh:8` `R=run/reverse_verify/smooth_lambda`
  - `run/*` 被 `.gitignore:17` 排除（本人实测 `git check-ignore -v` 命中 `.gitignore:17:run/*`）
  - `git ls-files` 在 `实验/additive-sky-seamless/results/` 下**只有 13 个文件**，无任何一个 exp1/exp2/exp3/exp_a6/exp_variance_closure/smooth_lambda 的产物
- **README 却陈述**（`m16_scene/README.md:11-13`）：`4/4 PASS`、PSF 域跨度 **0.00381 mag**、盒和域 **0.3358 mag**、比值 **88.2×**、`9/9 PASS`、C4 **+0.078/+0.028/+0.074%**、C1 **+1.92/+0.72/+1.03%**、σ 比 **0.5606/0.8615/0.8659**、r = **0.887/0.988/0.987**、K2 增量 **+137.2/+130.7/+132.1 ADU²**（解析预测 132.40）。
- **且它引的两份证据文件都不存在**：`run/RELEASE-02/paper/data/STACKN32/STACKN32-001.md`（`:7`）、`run/RELEASE-02/paper/data/M16FIX/M16-SCENE-FIX-001.md`（`:7`/`:27`）。`run/RELEASE-02/` 目录本身存在，但这两个子路径 `ls` 均 MISSING。
- **应为**：归档产物到 `实验/additive-sky-seamless/results/`（与 c1–c7 一致的正本位置），或把 README 的数字明确降级为「未归档的历史运行读数，不可复算」。附带把 `:7`/`:22`/`:27` 的死引用改指现存文件。
- **危害面**：`TAUTOLOGY_REGISTER.md` §2.8 登记了 exp2/exp3 的 11 条门，但**没有登记 exp1/exp_a6/exp_variance_closure/smooth_lambda 这 4 组共 32 条判据**——本片的判据登记存在系统性缺口，且缺口最大的那组连证据都没有。
- **口径**：判据分母层面，本片 `reverse_verify` 目录贡献 **32 条判据**（exp1:5 + exp2:6 + exp3:5 + exp_a6:4 + exp_variance_closure:4×3场景=12），其中 **0 条**有仓内可复算证据。

---

#### **B-3　`c3_public_plane.py` 的 B2/B6 是恒真门（代数恒等式型），却计入 7 门并被报告引用**

- **位置**：`实验/additive-sky-seamless/code/c3_public_plane.py:109-115`（B2）、`:143-159`（B6）
- **现状**：
  - **B2**（`:112-115`）：`bref = B − D`；`dev = max|bref − bref[0]|`；门 `dev < 1e-9`。生产 API 契约写死 `b_k(x) = B_ref(x) + δ_k(x)`（`lib/algorithms/coverage/include/astro/phase2/sky_plane.h:13`）与 `b_k(ra,dec) = B_ref + δ_k + gauge_shift`（同文件 `:460`）⇒ **`B − D ≡ B_ref` 按构造恒等于帧无关**，与拟合质量无关。归档实测 `max_dev = 5.684341886080802e-14`，而 `Bref_median = 297.33105962109374` ⇒ 纯 float64 舍入（相对 1.9e-16），**比 1e-9 阈低 4 个数量级**。
  - **B6**（`:158-159`）：门是 `span < 1e-9 and dseam < 1e-9`。归档实测 `per_frame_const_spread = 5.329070518200751e-15`、`delta_seam = 0.0`，且本人核对 `g["seam_ref"] == g["seam_sum"]` 为 **`True`** —— 归档里存的是**逐位相同的两个列表**。原因：gauge 常数平移经 cell 加权叠加变成逐 cell 常数，被 `sci_c_common.py:366-368` 的 order-2 多项式基线完全吸收。
- **应为**：两条都降为 `selfcheck`（正确范式就在同仓 `seam_gate_coverage.py:458-471` 的 `add_selfcheck`，`TAUTOLOGY_REGISTER.md:487` 对 `COV1` 做过同样的处置，c3 未同步）；B2 若要保留为科学判据，应拿 `build_world` 的 `sky` 真值去比 B_ref，而不是拿 `B − D`。
- **证据**（本人实测）：
  ```bash
  python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/c3_public_plane.json',encoding='utf-8'));\
  print(d['B_ref_frame_independence']);print(d['gauge']['per_frame_const_spread'],d['gauge']['delta_seam']);\
  print('seam lists identical:', d['gauge']['seam_ref']==d['gauge']['seam_sum'])"
  ```
  危害面：`README.md:179` 写「C3 …（`results/c3_public_plane.json`，**7/7 PASS**）」，7 门里有 2 门无判别力 ⇒ 实际有判别力的门是 5 条。
- **口径**：C3 的整改分母声明为 7，去重后有判别力门 **5**（B1/B3/B4/B5/B7），恒真门 **2**。

---

### 4.2 须修（14 条）

| # | 位置 | 现状 → 应为 |
|---|---|---|
| **M-1** | `sci_c_common.py:473-480` | `Gates.add(level=...)` 记录 `level` 但 `summary()` **无条件**计入 `n`/`n_fail`；`c4_seam_criterion.py:162`、`c5_weights.py:275` 均以 `n_fail==0` 决定退出码 ⇒ **`level="meta"` 是死字段**。自认非经验的 meta 门（`c4:103` N1、`c5:248` W4）仍计入整改分母并卡流水线。**改这一处同时解 N1/W4 两条。** |
| **M-2** | `c4_seam_criterion.py:37-39` + `sci_c_common.py:427` | N1 恒真（注入 ramp 同写 `frames["B"]` 与 `sky["B"]`，`degenerate_steps` 相减 ⇒ 精确抵消）**且反向**：若把 `sky_truth` 换成生产真实路径（`patch_estimate` 估出的天光，更正确），`sep50_deg` 升破 2 ⇒ **N1 判红、脚本 exit 1**。即「把代码改对 → 流水线红」。 |
| **M-3** | `c5_weights.py:245-248` | W4 测的是本文件 `:225-239` 定义的 `state_machine` 对 `:247` 字面量期望 `{converged:1, max_iter:0, stalled:2, invalid:3}`，**从不调用生产代码**，也不可能因生产缺陷而红。 |
| **M-4** | `c5_weights.py:264-267`、`c1_additive.py:378-382`、`exp3_variance_closure.py:203-206`、`seam_gate_gradient_scan.py:143` | 四处「**钉死缺陷**」门：断言已知缺陷**必须仍在**，修好即判红。W5 的 desc 自己写着「登记 FIX」。应改为「缺陷已修 ⇒ 期望反转」的双向门，或降级为 observation 不参与退出码。 |
| **M-5** | `c1_additive.py:201-202`、`c3_public_plane.py:103-104`、`c5_weights.py:211-212`、`c7_realdata.py:226-227` | `all(v is not None for v in out.get("frame_delta_coeffs", {}).values())` 四处 **空集合恒真**：`all([])==True`。若生产改名该键或走 `allow_additive_only_single_frame` 路径，A1/B1/W3/R2 仍绿而**零帧**带 δ_k。⚠️ `seam_gate_coverage.py:474-475` 已把这个危害写进注释并修了，副本未同步。 |
| **M-6** | `c1_additive.py:290` + `:302-308` vs `:361-364` | A6 被 A6p **严格包含**：`worst_dev = max(dev_subset, dev_gauge) ≥ dev_subset`，A6p 过 ⇒ A6 必过。A6 是零增量的重复门。 |
| **M-7** | `c1_additive.py:17-18`、`c2_multiplicative.py:18-22` | 模块 docstring 的判据清单与实现**结论相反或大幅漂移**：c1 A8「退化 ≥10×」vs 实现「无影响」（**否定关系**）；c1 A7「α=1 不收敛」vs `:372`「α=1 亦收敛」；c2 M2 5e-3→0.15、M3 2%→5%、M4 5×→3×、M5 ±3bin→±6bin、M6 命题反转。docstring 是审计第一入口。 |
| **M-8** | `c1_additive.py:318-325` + `:332-333` vs `:359` | `:321` 声称 `final_gauge=0` 那一支「由 A6p 的 gauge 项与 §6 的边界承担」——但 G≡0 时 `dev_gauge = max|g[0]−g[0]| = 0`，**A6p 的 gauge 项同样无信号**。覆盖声明与实际盲区不符。 |
| **M-9** | `c2_multiplicative.py:269-279` | `wF` 的 `sky`/`frames` 在 `:270-275` 被覆写并重合成，但 **`est`（k_photo/m_hat）仍是带天光梯度的旧世界的**；`:278` 的 `apply_phase1(wF,"loworder")` 除的是错配归一。M2/M3 读的正是 `ma_flat` ⇒ 「吸收臂」不是夹具本意的那一臂。 |
| **M-10** | `c7_realdata.py:178-183` | R1 全门只有 `all(isfinite(lv)) and all(isfinite(slopes))`，对 `level_ratio` **无任何阈值**。实测跨度 0.939–1.050；完全匹配的帧（ratio≡1）同样绿。docstring:11 声称「被量化（含不确定度）」——`robust_linfit:114` 只返两个标量，**全程无不确定度**。 |
| **M-11** | `c7_realdata.py:290-291` | R4 门条件是 `Path(S.FIGS/"fig_c7_real_crop.png").exists()`，该文件由 `:286` 刚 `savefig` 写出 ⇒ **构造性为真**（save 抛异常会直接中止脚本）。docstring:14 声称「目检确有可见台阶」——代码从未检查台阶可见性。 |
| **M-12** | `seam_gate_coverage.py:205-219` | `edge_with_coverage(..., null_img=None)` 默认把零台阶对照回退为被测图本身 ⇒ `cov_null ≡ cov` ⇒ 任何 `cov ≤ 0.5` 必落 `zone="none"`/`PASS`，`FAIL_COVERAGE` 分支**在该调用形态下永不可达**。本人算术复核：cov=0.30 → `cov_null_hi = 0.3397` → PASS；同一 cov 在配对对照下（hi=0.0033）是 `diluted`→`FAIL_COVERAGE`。⚠️ 现状是**潜伏**（全仓 5 个调用点都传了 `nul`，无外部消费者），但这是 `TAUTOLOGY_REGISTER.md:470` 评为「全域最高」的那个缺陷被 API 默认值原样放回。 |
| **M-13** | `seam_gate_coverage.py:191/196/200` + `:512-516` | 检出限 `blind_to_gate = thr/bg ≥ gate`。本人复算：`MAD(ctrl) ≈ σ√2` ⇒ 触发条件 `σ/bg ≥ 0.2357%`，BG=300 时 **σ ≥ 0.707 ADU**；本单元 RN=10 e⁻，脚本自带带噪臂 σ=20 高出 28 倍。⇒ 任何真实天光帧上 coverage 层只能给 `UNRESOLVED`，**`PASS` 不可达**；而 COV6 把 `verdict in ("PASS","UNRESOLVED")` 当「从不判红」放行，等于把恒红追认成绿。⚠️ 文件 `:60-63` **已诚实披露**此局限 ⇒ 判定为「覆盖缺口」而非「隐瞒缺陷」。 |
| **M-14** | `docs/control-variance-adjudication.md`（6 处）+ `results/REVERSE_VERIFY_CANON.md:3,41` + `reverse_verify/smooth_lambda/CRITERIA.md:3` + `p5c_common.py:5` + `m16_scene/README.md:11` | **伪引/引用不精确 10 条**：① `:95` `c3_public_plane.py:43-72` 无 `k_corr=1.4`（真值 `sci_c_common.py:31`）；② `:22` 拉普拉斯 `0.335×` 四路均未复现（0.318/0.349/0.353/0.387，且它是 JSON 的 `doc_claim_*` 键名＝待检验旧主张）；③ `:31/:50` 「端到端 ±1.5%」只在 N=5 成立（N=20 为 +5.0%）；④ `:50` 「低估 1.3–3.2%」实为 1.22–3.19% 且 N=20 是 +0.92% 高估；⑤ `:57` 「N≈225–251」无 251 出处（16 相位全 225）；⑥ `:69` 「k_gauss N=5→1.63」与其强制的 k_gauss 列（1.6620）矛盾；⑦ `CANON:3` `docs/ASTROCS_DESIGN.md`（真名 `ACSD_DESIGN.md`，且 §2.3 主题是 P3 守恒映射、λs 在 §2.5）；⑧ `CANON:3` `ENGINEERING_SPEC.md §9`（该文件已删，工程正本拆成 ~70 份，**16 个文件各有一个 §9**，无唯一指代）；⑨ `CANON:41` `migration_rerun.md` 不存在；⑩ `CRITERIA.md:3` 上游 `reverse_verify/README.md` 不存在；另 `p5c_common.py:5` 引 `docs/science/11_upm.md` 不存在（该路径只在 `run/*` 归档快照里，且被 route1 全部 7 份脚本引用）；`m16_scene/README.md:11` `star_detector.cpp:151` 实际在 `:240`；`sky_probe.cpp:34` `sky_plane.h:221` 实际在 `:224`。 |

### 4.3 建议（10 条）

| # | 位置 | 内容 |
|---|---|---|
| S-1 | `seam_gate_coverage.py:424` | `nul = make_image(**dict(kw, step_adu=0.0))` 对两个 `no_step_*` case 使 `nul` 与 `img` **数值完全相同** ⇒ `cov_excess` 构造性 ≤ 0，「与零台阶对照不可分辨」在这两行是空洞的。（分档扫描 `:378/401/411` 用的是同 seed 去台阶的真配对 null，无此问题。） |
| S-2 | `seam_gate_coverage.py:484-487` | COV4 在无噪臂是夹具解析恒等式（见 §5 反例 1）。作者已把**同源**的幅度极差恒等式（COV9 无噪支）降为自检（`:393-395`、`:539-540`），却把**另一种写法**的同一恒等式留作计门。 |
| S-3 | `seam_gate_coverage.py:547-554` vs `:527-536` | COV10 的第二子句 `all(new_verdict != "PASS" ... if blind_to_gate)` 与 COV8 的子句**逐字相同**；且 `legacy_verdict`（`:434-440`）是测试内重写的旧规则，不是被删掉的 `:172` ⇒ COV10 相对 COV8 零增量判别力，门数虚增 1。 |
| S-4 | `seam_gate_coverage.py:232`、`:341-343`；`c1_additive.py:93-94`、`:104`；`c3_public_plane.py:45`、`:118`；`c7_realdata.py:235-236`；`c8_scale_tolerance.py:50`；`c7_share_vs_abs_weight.py:61` | 死代码/未用符号：`coverage_ok` 算了从不消费且把 `unresolved` 记为 OK（误导下游）；两处 `... if False else None`；`max_subsets=256` 未用；`binary_dilation`/`grid` 未用；`gx,gy = None,None` 死赋值；`total_seq` 死分支。 |
| S-5 | `c3_public_plane.py:162-169` | B7 用 `s["step"]`（被真结构污染）而非 `excess`，与 `sci_c_common.py:386-388`「强结构背景必须看 excess」口径相反 ⇒ B7 在很大程度上在量真结构而非接缝。 |
| S-6 | `c2_multiplicative.py:49-61`/`:127-128` | 注入的 `m_true`（`:52-53` 6 个系数）与 Phase1 拟合的 `m̂`（6 阶基）**函数类完全重合** ⇒ M1「帧间乘性比 → 1，|ratio−1|<5e-3」在 `hf_amp=0` 的 wP 上是生成器落在估计器类内的结构性恒真（仍非严格恒等，有真实噪声，但判别力对「模型类不匹配」为零）。 |
| S-7 | `c4_seam_criterion.py:85/88/113/142` | `:142` N5 用**单边、未对 mu 中心化**的谓词，而 `:85/:88` 明写 `two_sided=True`；且 N5 把 k=0 的 mu/sd/thr 套到 `:133-136` 改造过的**另一个总体**上（阈值未重导）。 |
| S-8 | `reverse_verify/smooth_lambda/sweep_all.sh:16-19` | 子壳内 `echo "SWEEP_DONE $name rc=$?"` 使 `wait` 与脚本末行 `echo ALL_SWEEPS_DONE` 吃掉真实退出码 ⇒ **脚本恒 exit 0**。这正是 `audit_rework/run_all.sh:14-15` 明文批评的同一失效形态，却留在另一棵子树里。 |
| S-9 | `audit_rework/route1/{c2,c3,c4,c5,c6,c7,c8}_*.py`、`route2/{e1,e2,e7,e9}_*.py`、`route3/{exp03,exp04,exp05,exp09,exp10,exp11,exp12}_*.py`（本片 19 份） | 这些脚本**全部无门聚合、无 `sys.exit`** ⇒ 退出码恒 0，docstring 列出的 claim 只被记录、从不消费。`run_all.sh:52-64` 的自述与 `rollup.py` 是唯一兜底，兜底有效性依赖 `GATE_DISCLOSURE.json` 的人工维护。 |
| S-10 | `m16_scene/README.md:1-58` | 通篇含日期、任务编号（`STACKN32-001（2026-09-20）`、`M16-SCENE-FIX-001`）、退役报告指针（`:38`「该报告已退役，见仓库 git 历史」）与「更早修复前 0.0127 / 27×」历史叙事 —— 与 AGENTS.md §5「正文无日期、版本号、任务流水编号、commit、『旧版/作废/曾/原』等历史叙事」冲突。（实验 README 的前后对照有其证据价值，故只列建议，不升级。） |

---

## 5. 我主动构造的反例

### 反例 1：COV4 是夹具的代数恒等式（**推翻成功**）

- **构造什么**：COV4 断言 `|cov − frac| ≤ 0.03`，被测对象是 `cov = mean(|seam_i − lv| > thr)`。
- **期望推翻什么**：期望它能在判据失效时翻红（真门），或至少能在判据**实现**改动时翻红。
- **推导**（本人逐步代数，非模拟）：`ctrl` 是生产 `_ctrl_at`（`seam_footprint.py:274-278`）返回的**差分** `I(+d)−I(−d)`，故 `lv = median(ctrl) ≈ 0`；无噪夹具下 `mad(ctrl) ≡ 0` ⇒ `thr = max(3·0, 0.5·gate·bg) = 0.5×0.01×300 = 1.5 ADU`；`seam_i = 12 ADU`（前 f 段）或 `0`（其余）⇒ `|seam_i − 0| > 1.5` 对**全部**样本成立？否——`seam_i=0` 时 `0 > 1.5` 为假 ⇒ 只有前 f 段计入 ⇒ `cov ≡ f` **精确成立**。`frac ∈ {0.05,…,1.00}` 全部 `|cov−frac| = 0.0 ≤ 0.03`。
- **是否推翻**：**推翻成功**。COV4 在无噪臂是构造性恒真，只能在双线性采样几何变化时红，不能在判据出问题时红。作者对**同源**的幅度极差恒等式（COV9 无噪支）已在 `:393-395` 与 `:539-540` 自认并降级为自检，唯独 COV4 未同步 ⇒ 整改只做了一半。

### 反例 2：A6 是否被 A6p 蕴含（**推翻成功**）

- **构造什么**：`c1_additive.py:290` `worst_dev = max(dev_subset, dev_gauge)`；`:308` A6p 门 `worst_dev <= tol_A6`；`:364` A6 门 `dev_subset <= tol_A6`。
- **期望推翻什么**：期望 A6 有独立判别力（即 A6p 绿而 A6 红是可能的）。
- **推导**：`dev_gauge = max_i max|g[i] − g[0]| ≥ 0` ⇒ `worst_dev ≥ dev_subset` ⇒ `worst_dev ≤ tol` ⇒ `dev_subset ≤ tol`。反向不成立。
- **是否推翻**：**推翻成功**。A6 是 A6p 的严格弱蕴含 ⇒ 零增量判别力，门数虚增 1（整改分母 14 → 有效 13）。

### 反例 3：`null_img` 默认形态能否产出 `FAIL_COVERAGE`（**推翻成功**）

- **构造什么**：`seam_gate_coverage.py:214` `cn = coverage(null_img if null_img is not None else img, ...)`。
- **期望推翻什么**：期望省略 `null_img` 时判决面仍能区分「有台阶被稀释」与「真无台阶」。
- **推导**（本人纯算术，n=1200）：`cn["cov"] ≡ cov` ⇒ `pnull = min(max(cov, 1/n), 0.5)`；cov=0.30 ⇒ pnull=0.30 ⇒ `hi = 0.30 + 3√(0.30·0.70/1200) = 0.3397` ⇒ `cov ≤ hi` ⇒ `zone="none"` ⇒ `verdict="PASS"`。同一 cov=0.30 在真配对对照下（cov_null=0，hi=0.0033）是 `diluted` → `FAIL_COVERAGE`。
- **是否推翻**：**推翻成功**（在该调用形态下 `FAIL_COVERAGE` 不可达）。**但我把它降级为须修而非阻断**——见 §7 否决 1。

### 反例 4：覆盖率层的绿臂在有噪数据上是否可达（**推翻成功，但严重度被我自己下调**）

- **构造什么**：`blind_to_gate = thr/bg ≥ gate`，`thr = max(3·MAD(ctrl), 0.5·gate·bg)`。
- **期望推翻什么**：期望在「有噪声、无缺陷」的真实情形下 coverage 层能出具 PASS（绿臂存在）。
- **推导**（本人）：`ctrl` 是两次独立采样的差分 ⇒ `std(ctrl) = σ√2` ⇒ `median|·| = 0.6745·σ√2` ⇒ `MAD(ctrl) = 1.4826×0.6745·σ√2 ≈ σ√2`。`blind ⟺ 3σ√2/bg ≥ 0.01 ⟺ σ/bg ≥ 0.2357%`。BG=300 时 **σ ≥ 0.707 ADU**。真实天光帧 σ/bg ≈ 1/√(sky_counts) ≈ 3–10%。
- **是否推翻**：**推翻成功**（绿臂在真实工作域不可达）。**但严重度下调为须修**，理由见 §7 否决 2。

### 反例 5：exp2/exp3 的负例能否被证伪（**与我的预期相反，如实登记**）

- **构造什么**：试图证伪 `exp2:233-236` G2 共模归零。
- **期望推翻什么**：期望共模场景下 `std_f(delta)` 能非零。
- **结果**：**未能推翻**。文件自己的 docstring `:16-17` 已证明该恒等式成立，`TAUTOLOGY_REGISTER.md:400` 亦已登记为 B 型（逐位恒等）。我的独立结论与既有登记**一致**，不新增发现。

### 反例 6（未完成，诚实登记）：`CRITERIA.md:31` C7 尺度不变性到底在测什么

- **构造什么**：核对 `gen_synth.py:179/186-190` 是否真的把物理标度放大 1e9。
- **期望推翻什么**：期望找到「A_scale 静默没建成」或「C7 是恒红门」二选一的判别证据。
- **结果**：**部分推翻、部分未决**。已确证 `--scale` 是仿真**之后**的纯标签缩放（`:179` help 逐字「仅标签，物理不变」），且 `:186-190` 只乘 `value` 与 `control_variance` ⇒ 若所有判据量都是比值，C7 是浮点舍入级自比；但**生成器没有缩放 λs**，而 `control_variance` 被乘了 1e18，若粗糙度项作用在 `control_variance` 上，同一 λs 在 A_scale 里含义不同 ⇒ C7 可能既不自比也测不到它声称的东西。**需实跑裁决**（我按约束未跑），标 **UNRESOLVED**。

---

## 6. 盲复算

**方法**：对 B-3（唯一我认定为「阻断且可完全算术裁决」的一条）先不看子代理结论，独立从 `sky_plane.h` 的 API 契约 + 归档 JSON 取证，再与结论比对；对其余阻断/须修项，逐条独立算术复核。

| 项 | 既有结论 | 我的独立取证 | 判定 |
|---|---|---|---|
| B-2 的 B_ref 独立性 | 「恒真，dev=5.68e-14」 | 我先读 `sky_plane.h:13`（契约 `b_k(x)=B_ref(x)+δ_k(x)`）与 `:460`（`b_k = B_ref + δ_k + gauge_shift`），推出 `B−D ≡ B_ref`；再读归档 `max_dev=5.68e-14`、`Bref_median=297.33`（相对 1.9e-16 = 1 个 ulp 量级） | **一致**（且我的推理链更上游：不依赖归档数值，只依赖契约 + 实测量级） |
| B-3 的 B6 | 「两个逐位相同的列表」 | 我跑 `d['gauge']['seam_ref']==d['gauge']['seam_sum']` → `True`；`delta_seam = 0.0` | **一致** |
| M-12 `null_img` 默认 | 「FAIL_COVERAGE 不可达」 | 我用 n=1200 手算 cov=0.05/0.30/0.48 三点 | **一致** |
| M-13 `blind_to_gate` 阈值 | 「σ/bg ≥ 0.2357%」 | 我从 `mad()` 的 1.4826 系数推出 `MAD(ctrl)≈σ√2`，解 `3σ√2/bg ≥ gate` | **一致**（两位子代理分别给出 0.2357% 与 28×，与我的 0.707 ADU @BG=300 自洽） |
| M-6 A6 ⊂ A6p | 「零增量」 | 我从 `:290` 与 `:308`/`:364` 直接推出单向蕴含 | **一致** |
| `control-variance-adjudication.md:79` 「标准误低估 14%」 | 「口径错误，14% 实为 k_gauss 腿」 | 我算 `1 − 1.4/1.6370 = 14.48%`；若真指 MAD 尺度则生产链 N=5 的 `sqrt(E[σ̂²]/σ²) ≈ 0.9515` ⇒ 4.85%，`E[σ̂²]/σ² ≈ 0.9053` ⇒ 9.47% | **一致**（采纳，但降为须修，因该文档整体是裁决台账而非代码判据） |
| `control-variance-adjudication.md:59` MC 带口径 | 「16 相位均值而非 128 样本」 | 我用 `range/sd = 0.15205/0.0416 = 3.655`，对照 iid 正态 `E[R]/σ`：n=16 → 3.532、n=128 → 5.189 ⇒ 贴 n=16 | **一致**（算术自洽性论证成立） |
| M-13 的严重度 | 子代理评为「阻断」 | 我按 `README`/报告是否引用、生产门是否消费、是否已披露三项判据，判为须修 | **偏严（我更宽）**，见 §7-否决 2 |
| `M-12` 的严重度 | 子代理评为「阻断」 | 我按「是否有活调用者」判为须修 | **偏严（我更宽）**，见 §7-否决 1 |
| exp2/exp3 的 SKIP 状态 | 「固化 JSON 是 status:SKIP、11 条门一次都没被求值」 | 我 `ls 实验/additive-sky-seamless/results/`：**没有任何 exp1/exp2/exp3 的产物文件**（它们写 gitignored `run/`）⇒ 该子代理所指的「固化 JSON」我**无法定位** | **无法核实**，不采纳为事实（已在本片改写为 B-2「零归档证据」） |

**盲复算总判**：与既有结论**一致**；对两条严重度**我的判定更宽**（理由已记）；对一条**推翻子代理的取证前提**（exp2/exp3 的 SKIP 记录在我可见范围内不存在）。

---

## 7. 子代理派发记录

派出 **4 个** subagent（另有 2 次因 harness 重复投递产生的同题实例，内容一致，不另计）。全部要求：亲读指定文件全文、只读、零 git 写、不编译、不跑仓内实验脚本、每条带 `文件:行`。

| # | 派发主题 | 指派范围 | 我的复核结论 |
|---|---|---|---|
| A | 恒真门三型 / 恒红 / 自愈 / 判据读不到真实对象 / 选择性报告 | `seam_gate_coverage.py`、`seam_gate_gradient_scan.py`、`c4_seam_criterion.py`、`c1/c2/c3/c5/c7`、`route1/c3_seam_gate.py`、`CRITERIA.md`、`data_matrix/exp2`、`exp3` + 补读生产门 `seam_footprint.py` 与 `sci_c_common.py` | **采纳** M-1/M-2/M-3/M-5/M-6/M-8/M-10/M-11/M-17/M-19/M-20 等主体；**独立复核**其 B-3/B-4（B2/B6 恒真）并用本人算术确认；**下调**其 B-1、B-2 两条的严重度 |
| B | 伪引 / 文档-代码冲突 / 退役治理 / `.gitignore` 伪装 | `control-variance-adjudication.md`、`REVERSE_VERIFY_CANON.md`、`data/README.md`、`c3_public_plane.py`、`CRITERIA.md`、三个 shell、`m16_scene/README.md`、smooth_lambda 小脚本 | **采纳** 10 条伪引（其中 6 条 `control-variance-adjudication.md`、2 条 `CANON`、1 条 `CRITERIA.md`、1 条 `star_detector.cpp:151`）+ `.gitignore` 无伪装的反向结论；**采纳**「`docs/smooth-lambda.md:3-8` 订正注真实存在」「T3 列头逐字命中」「`run_mosaic_ls.sh` 参数逐字相符」等正向核实 |
| C | 盲复算：数值独立重算 | `control-variance-adjudication.md` + 四路 median-variance 脚本 + `p3_kcorr/` 全部固化读数 | **采纳** §3.1 统计口径错（range/sd=3.655 贴 n=16）、`:79` 的 14% 是 k_gauss 腿、`:69` 的 k_gauss(5) 四个不同值（1.0973/1.2087/1.6370/1.6620）；**采纳** §2 推导腿完全成立（Var(x₍₃₎)=0.2868336616，密度归一化 1.0000000000，「低估 8.5%」方向订正确立）；**采纳**「κ 四点须双边合成 MC 误差」的审计陷阱 |
| D | 归档漂移 / 门名一致性 / `run_all.sh` 覆盖 | `results/**` 全部入库 JSON + 38 个脚本的 `OUT` 常量 + `run_all.sh` | **采纳** 决定性证据「36/37 工作树重跑产物与正本 md5 逐位一致，唯一差异 c10」——这一点**避免了我把 22 个 route2/route3 脚本误判为缺陷**；**采纳** `audit_rework_summary.json` 是无产出脚本的孤儿归档；**采纳** 6 条 `open_red` 当前为红而 `REPORT_experiment.md:278` 仍写「退出码 0」；**本人独立复现**三条 open_red（`c6/kappa_identical`、`c7/exclusive`、`q10/ordering_matches_doc_16.1.3` 均为 `False`） |

### 逐条否决（3 条）

1. **否决子代理 A 的「B-1 覆盖率层在真实数据上恒红 → 阻断」**（子代理编号重复出现两次，两次都提）。
   - **理由**：该局限在 `seam_gate_coverage.py:60-63` **已由作者本人逐字披露**（「实测带噪臂 ctrl_mad≈22 ⇒ thr/bg≈0.22，是门阈的 22 倍：比门瞎 22 倍的诊断量，在 cov 与零台阶对照不可分辨时给出的是『看不见』，不是『没有』」），并由 `COV8`（`:527-536`）作为门强制执行；`UNRESOLVED` 在 `:85` 的注释与 `_VERDICT_RANK`（`:238`）中被明确定义为「证据不足，**不计通过**也不谎称有台阶」。此外该脚本的输出**无生产消费者**（生产门是 `eng/tools/e2e/seam_footprint.py`，不引用 coverage 层；本人 `grep -rn edge_with_coverage\|gate_decision_with_coverage` 全仓只命中该文件自身与文档）。
   - **改判**：算术我完全采纳（`σ/bg ≥ 0.2357%`、BG=300 时 σ≥0.707 ADU，与子代理的 28× 同量级），但严重度由**阻断降为须修 M-13**，定性为「绿臂覆盖缺口」而非「隐瞒缺陷」。

2. **否决子代理 A 的「`null_img=None` fail-open → 阻断」**。
   - **理由**：缺陷本身真实且我独立复算无误，但**当前不可达**——全仓 5 个调用点（`:379/387/402/412/425`）**全部显式传了 `nul`**，且 `grep` 证实 `edge_with_coverage` / `gate_decision_with_coverage` **无任何仓外调用者**。把它定为阻断会与本片真正的阻断项（B-1/B-2/B-3）争夺注意力。
   - **改判**：降为**须修 M-12**，并明确标注「潜伏：API 默认值 fail-open，当前无活调用者」。

3. **否决子代理 B/C/D 关于 `route3/q3_median_variance.json` 是「文档引用错名」的怀疑**。
   - **理由**：route3 的命名规范是脚本 `expNN_*` → 产物 `qN_*`（我在片内 `exp03_median_variance.py:19` 亲眼读到 `OUT = .../q3_median_variance.json`）。`control-variance-adjudication.md:49` 引的正是 `route3/q3_median_variance.json`，**引用正确，不是笔误**。
   - **采纳子代理的更正**并据此把该项从我的疑似清单中划掉。

---

## 8. 自证段（可复跑命令）

> 全部只读；中文路径已带 `-c core.quotepath=false`；不编译、不跑实验脚本。

```bash
cd "/workspace/Astro CS Database"

# ── 0. 基线 ───────────────────────────────────────────────
git -c core.quotepath=false rev-parse HEAD                       # f9650dd0aed97d7f261e5e6547f4fdb505bc313b
git -c core.quotepath=false status --short                        # 空输出

# ── 1. 覆盖率自证：50 份成员 / 7875 行，与权威清单逐位一致 ──
sed -n '2234,2284p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
  | sed 's/^      - //' | tr -d '"' | grep -v '成员文件:' > /tmp/s002.txt
wc -l /tmp/s002.txt
while IFS= read -r f; do wc -l < "$f"; done < /tmp/s002.txt | paste -sd+ | bc   # 7875

# ── 2. B-1：判据改了、归档没重跑 ────────────────────────────
git -c core.quotepath=false log -1 --format='%h %ad %s' --date=short -- 实验/additive-sky-seamless/code/seam_gate_coverage.py
git -c core.quotepath=false log -1 --format='%h %ad %s' --date=short -- 实验/additive-sky-seamless/results/seam_gate_coverage.json
git -c core.quotepath=false show --stat ef3dc516 -- 实验/additive-sky-seamless/code/seam_gate_coverage.py
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/seam_gate_coverage.json',encoding='utf-8'));print('has_selfchecks:', 'selfchecks' in d);print('gates_n:',len(d['gates']));print([r['id'] for r in d['gates']])"
# 期望：代码 ef3dc516(10-02) / 归档 751329ef(09-30)；has_selfchecks=False；gates_n=9；无 COV6b/COV10

# ── 3. B-2：reverse_verify 零归档 ──────────────────────────
git -c core.quotepath=false check-ignore -v run/reverse_verify/m16_scene/a6_seeing/result.json   # .gitignore:17:run/*
git -c core.quotepath=false ls-files 实验/additive-sky-seamless/results/ | grep -icE 'a6_seeing|exp1_|exp2_|exp3_|variance_closure|smooth_lambda'  # 0
ls run/RELEASE-02/paper/data/STACKN32/STACKN32-001.md run/RELEASE-02/paper/data/M16FIX/M16-SCENE-FIX-001.md  # 均 No such file

# ── 4. B-3：B2/B6 恒真门 ──────────────────────────────────
sed -n '13p;460p' lib/algorithms/coverage/include/astro/phase2/sky_plane.h    # b_k = B_ref + δ_k (+gauge_shift)
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/c3_public_plane.json',encoding='utf-8'));\
b=d['B_ref_frame_independence'];g=d['gauge'];\
print('B2 max_dev=',b['max_dev'],'Bref_median=',b['median']);\
print('B6 spread=',g['per_frame_const_spread'],'dseam=',g['delta_seam']);\
print('seam lists identical:', g['seam_ref']==g['seam_sum'])"

# ── 5. M-1：level="meta" 是死字段 ─────────────────────────
grep -rn 'level="meta"' 实验/additive-sky-seamless/code/ ; grep -rn '\.level\b\|level ==' 实验/additive-sky-seamless/code/ | grep -v 'level=' # 零命中
sed -n '473,480p' 实验/additive-sky-seamless/code/sci_c_common.py            # add 存 level，summary 不分 level

# ── 6. M-5：all([]) 空集合恒真（四处） ───────────────────
grep -n 'all(v is not None for v in out.get("frame_delta_coeffs", {}).values())' 实验/additive-sky-seamless/code/{c1_additive,c3_public_plane,c5_weights}.py
sed -n '226p' 实验/additive-sky-seamless/code/c7_realdata.py

# ── 7. M-12/M-13：coverage 层 fail-open 默认值与检出限 ──────
sed -n '205,219p;191,201p' 实验/additive-sky-seamless/code/seam_gate_coverage.py
python3 -c "
import math
n=1200; gate=1e-2; bg=300.0
for cov in (0.05,0.30,0.48):
    pnull=min(max(cov,1.0/n),0.5); hi=pnull+3*math.sqrt(pnull*(1-pnull)/n)
    print('cov=%.2f -> cov_null_hi=%.4f -> cov<=hi? %s' % (cov,hi,cov<=hi))
sig=1.0*math.sqrt(2)*0.6745*1.4826   # MAD(ctrl) ≈ σ√2
print('blind threshold: MAD(ctrl) >= %.4f ADU @ BG=300  <=>  sigma/bg >= %.4f%%' % (gate*bg/3, 100*gate/(3*math.sqrt(2))))
"
grep -rn 'edge_with_coverage\|gate_decision_with_coverage' --include=*.py --include=*.md . | grep -v 'seam_gate_coverage.py'   # 无外部调用者

# ── 8. M-14：伪引逐条核实 ─────────────────────────────────
for p in docs/ASTROCS_DESIGN.md docs/ACSD_DESIGN.md ENGINEERING_SPEC.md docs/science/11_upm.md \
         实验/additive-sky-seamless/code/reverse_verify/README.md \
         run/ROOT-CONSOLIDATION/logs/migration_rerun.md; do [ -e "$p" ] && echo "EXISTS $p" || echo "MISSING $p"; done
grep -n 'K_CORR = 1.4' 实验/additive-sky-seamless/code/sci_c_common.py          # :31 定义
grep -n 'S.K_CORR' 实验/additive-sky-seamless/code/c3_public_plane.py            # :67 消费点，43-72 无字面量
grep -n 's.flux = m00' lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp   # 240，不是 151
sed -n '151p' lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp

# ── 9. 正向核实（本片也有关键项经得起核） ─────────────────
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/c7_realdata.json',encoding='utf-8'));print([f['phase_shift'] for f in d['data']])"  # [0,0]/[0,0]/[8,6]/[10,8] 与 data/README.md:39 逐字相符
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/c1_median_variance.json',encoding='utf-8'));g=d['gaussian_N5_exact'];print(g['var_exact_quadrature'],g['asymptotic'],g['asymptotic_over_true'])"  # 0.2868336616 / 0.3141592654 / 1.0952663770
grep -rn 'sys.exit(1)\|exit(1)' 实验/additive-sky-seamless/code/audit_rework/ | grep -v rollup.py   # 零命中，与 run_all.sh:54 自述相符
```

---

## 9. 交付件内计数口径声明

| 口径 | 含义 | 本片数值 |
|---|---|---|
| **门实例** | 以「一次可独立判红的聚合判定」为单位：`Gates.add` / `add` / `L.verdict` / `rec["C?_pass"]` / `CRITERIA.md` 的 C1–C8 | **110 条**（c1:14、c2:7、c3:7、c4:6、c5:5、c7:5、seam_gate_coverage:10、gradient_scan:16〔5 case × 3–4 断言 + 1 noise〕、exp1:5、exp2:6、exp3:5、exp_a6:4、exp_variance_closure:12〔C1–C4 × 3 场景〕、CRITERIA.md:8） |
| **去重门** | 扣除恒真门、恒红门、被蕴含的重复门、本地桩门后仍有判别力的门 | **≈ 93 条**。扣除明细：恒真/恒等 8（c3 B2、c3 B6、c4 N1、c5 W4、exp2 G2、exp2 G3、exp3 E3、c7 R4）；反向（修好即红）4（c5 W5、c1 A7b、exp3 E5、gradient_scan F2）；重复 2（c1 A6 被 A6p 蕴含、seam_gate_coverage COV10 与 COV8 重叠）；空真 4（c1 A1、c3 B1、c5 W3、c7 R2 的 `all([])`/缺键）；弱门 3（c7 R1 只查有限性、c1 A8b 分母自指、seam_gate_coverage COV4）；桩门 2（c1 A6p 的 gauge 注入负控、route1/c3_seam_gate 的 (e)） |
| **整改分母** | 报告/台账实际声称的门数 | seam_gate_coverage 声称 10（`README.md:500`）**实际归档 9 且全为旧名**（B-1）；c3 声称 7（`README.md:179`）；c4 声称 6；c5 声称 5；c7 声称 5；CRITERIA.md 声称 8；exp1/2/3/exp_a6 共 20；exp_variance_closure 12 |

---

*本件由 G08-05 第 1 遍对抗审稿（片 `EXP-additive-sky-seamless-002`）产出。全程只读，零 git 写，未编译、未跑 ctest/pytest/构建、未执行仓内实验脚本，未读 `/tmp/acsd_g08/`。仓内唯一写入为本交付件。*