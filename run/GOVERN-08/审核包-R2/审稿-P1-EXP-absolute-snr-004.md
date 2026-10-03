# 审稿-P1 · EXP-absolute-snr-004（G08-05 对抗审稿 第 1 遍）

- 审稿人：G08-05 第 1 遍分片审稿子代理（只读）
- 基线：`/workspace/Astro CS Database` HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`
- 片清单：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2075-2121`
- 零 git 写；未编译、未跑 ctest/pytest/构建/实验脚本；未修改任何仓内文件（唯一写入为本交付件）

---

## 1. 读完了吗

**成员份数 39 ｜ 完整读完 13 份 ｜ 成员总行数 8883 ｜ 实际完整读出行数 3752 ｜ 覆盖率 42.2 %**

（另有 `b1_sky_scan.py` 读到 :305/418，属**部分读**，其 306–418 行未读，已从完整计数中剔除。）

### 1.1 完整读完的 13 份（3752 行）

| # | 文件 | 行数 |
|---|---|---|
| 1 | `实验/absolute-snr/code/b7_absolute_snr_recon.py` | 966 |
| 2 | `实验/absolute-snr/docs/EXP-01-DELTA-AND-ESTIMATOR.md` | 782 |
| 3 | `实验/absolute-snr/code/exp03/exp03_common.py` | 702 |
| 4 | `实验/absolute-snr/code/exp06/e4_gates.py` | 393 |
| 5 | `实验/absolute-snr/code/audit/route3/exp05_doublecount_corr.py` | 172 |
| 6 | `实验/absolute-snr/code/exp04/e4_gates.py` | 165 |
| 7 | `实验/absolute-snr/code/b5_phase3_transfer.py` | 150 |
| 8 | `实验/absolute-snr/code/audit/route1/exp02_moffat4_constant.py` | 109 |
| 9 | `实验/absolute-snr/code/fetch_evidence.py` | 105 |
| 10 | `实验/absolute-snr/code/audit/README.md` | 102 |
| 11 | `实验/absolute-snr/code/exp04/run_all.sh` | 56 |
| 12 | `实验/absolute-snr/code/exp03/run_all.sh` | 36 |
| 13 | `实验/absolute-snr/code/build_prod_driver.sh` | 14 |

### 1.2 部分读

| 文件 | 读到的范围 | 未读 |
|---|---|---|
| `实验/absolute-snr/code/b1_sky_scan.py`（418 行） | :1–305 | **:306–418** |
| `实验/absolute-snr/code/exp11_recon_driver.cpp`（247 行） | 仅 :29、:39–42（为核验子代理的编译断裂结论） | 其余全部 |

### 1.3 ⛔ **未读到的 24 份（5118 行）—— 未裁定，不得当作已审通过**

| 文件 | 行数 |
|---|---|
| `实验/absolute-snr/docs/f-instr-canon.md` | 504 |
| `实验/absolute-snr/code/exp05/exp05_common.py` | 456 |
| `实验/absolute-snr/code/reverse_verify/snr_design/audit/audit_sim_validation.py` | 356 |
| `实验/absolute-snr/results/EXP04_TABLES.md` | 322 |
| `实验/absolute-snr/code/exp03/e1_analytic.py` | 296 |
| `实验/absolute-snr/code/reverse_verify/snr_design/audit/audit_sp0.py` | 243 |
| `实验/absolute-snr/code/exp03/e3_hst.py` | 230 |
| `实验/absolute-snr/code/b6_gates_audit.py` | 222 |
| `实验/absolute-snr/code/exp02/make_tables.py` | 210 |
| `实验/absolute-snr/code/reverse_verify/frame_snr/inventory_p1_snr.py` | 204 |
| `实验/absolute-snr/code/reverse_verify/frame_snr/frame_snr_canon.py` | 188 |
| `实验/absolute-snr/code/exp04/summarize.py` | 178 |
| `实验/absolute-snr/code/exp04/e1_analytic.py` | 148 |
| `实验/absolute-snr/code/reverse_verify/f_instr/exp4_gain_recovery.py` | 137 |
| `实验/absolute-snr/code/audit/route2/exp02_robust_scale_mad.py` | 129 |
| `实验/absolute-snr/code/exp04/driver.py` | 128 |
| `实验/absolute-snr/code/make_figures.py` | 113 |
| `实验/absolute-snr/refs.md` | 89 |
| `实验/absolute-snr/code/audit/route2/exp12_weight_gamma.py` | 77 |
| `实验/absolute-snr/code/audit/route1/exp14_kcorr_inflation.py` | 68 |
| `实验/absolute-snr/results/REVERSE_VERIFY_CANON.md` | 53 |
| `实验/absolute-snr/docs/DERIVATIONS_P2.md` | 51 |
| `实验/absolute-snr/code/reverse_verify/frame_snr/cpp/p1snr_probe.cpp` | 36 |
| `实验/absolute-snr/code/diag_highN.py` | 28 |

**⇒ 本片 55.8 % 的行数本遍未覆盖。本交付件的结论只对第 1.1/1.2 节成立；第 1.3 节 24 份文件的状态是「未审」，不是「无问题」。**
其中 `reverse_verify/` 三簇（`frame_snr` / `snr_design` / `f_instr`，合计 5 份 693 行）正是 `TAUTOLOGY_REGISTER.md:356` 所述「`reverse_verify/frame_snr/` 直调 `snr_science.cpp`」的唯一真读生产面，**其判别力本遍完全未核**，属重要缺口。

---

## 2. 本片判定：**阻断**

**阻断理由**：唯一可引的证据面 `results/EXP04_TABLES.md:314` 正用一个**已被本单元自己判定为「恒真门、移出判定、仅登记」**的检查给单元背书，且该判定与其归档写在**同一天相隔 9 天**的两个提交里。按规范 08 §4「恒真…的判据无效」，一张把恒真门渲染成 `True` 的表，不能作为该单元的判别力证据。这是**对外出具错误的肯定结论**，不是措辞问题。

### 最重的 3 条

**【阻断 A1】EXP-04 门判据改了、归档没重跑 —— 唯一证据表仍把恒真门报成绿灯 `True`**
- 位置：`实验/absolute-snr/code/exp04/e4_gates.py`（生产者，本次片内已完整读）→ `实验/absolute-snr/results/exp04_e4_gates.json`（唯一证据存档）→ `实验/absolute-snr/results/EXP04_TABLES.md`（唯一可引证据面）
- 现状（**以下全部由我亲自实测，非采信子代理**）：
  - 门代码 `e4_gates.py` 最后改动 = 提交 `11cd1d2d`（2026-10-02，HEAD 前数小时）；归档 `exp04_e4_gates.json` 的 mtime = **2026-09-22 22:50**（`generated_at` 同日），**早于门代码改动 9 天**。
  - 归档 `top keys` = `[experiment, frozen_config, G1…G5, gates_summary, generated_at, wall_s]` —— **无 `always_true_checks`、无 `always_true_note`**。
  - 归档 `gates_summary` 仍记 **6** 门，且 `G6_frame_arm_selfzero: true`；现行代码重跑后应为 **4** 门（`e4_gates.py:143-149` 已把 `degenerate=True` 的 G2/G4/G6 剔除）。
  - 归档 `G6_frame_arm_selfzero` 的键 = `[eff_loss, ok, rmse]`，**无 `degenerate`**；归档 `G1` 行的键 = `[eff_loss, max_abs_dev, ok, op, rmse]`，**无 `level_bias_dex`**（而现行代码 `:61,:64-65` 已把它加进判据并加严）。
  - `sed -n '314p' 实验/absolute-snr/results/EXP04_TABLES.md` = `| G6_frame_arm_selfzero | True |  |` —— 同一张表里 G2/G4 已显示「登记」，唯独 G6 仍是 **绿灯 `True`**，且「说明」列为空。
- 为害：`e4_gates.py:79-84` 自述 G6 是「恒真门，移出判定、仅登记…与实现无关」；这句话**没有落到它自己的结果表上**。这正是本项目最常见的失效模式「代码改了、归档没重跑」，且落在**唯一可引证据面**。
- 应为：与 `11cd1d2d` 同提交重跑 `exp04` 并重出 `EXP04_TABLES.md`；同时 `TAUTOLOGY_REGISTER.md:366` 的「§4 归档脱钩」表**只登记了 `exp05_e5_gates.json`、漏登本条**。
- **「四个面、两个版本」（子代理提出、我亲验归档侧后采信）**：代码说 G6 不是门（`e4_gates.py:73-84` 已改 `degenerate=True` 且无 `ok`）；归档说 G6 是绿；表格说 G6 是绿（说明列空，因旧归档里 G6 的 `note=null`）；**文档 `docs/EXP-04-RECONSTRUCTION.md:644` 仍写「| G6 帧级臂自证 | **绿（必须）** | … | **PASS** |」，且 `:633` 仍称「G1–G6 全承担证据位」**。该文档最后改动 09-30，早于门改动 10-02。**
- **同一个提交还把 G1 加严了（新增 `level_bias_dex` 合取腿），加严后从未复跑** ⇒ EXP-04 对外呈现的 G1 绿是**旧判据下的绿**。而把新口径从代码传到报告面的唯一通道（顶层键 `always_true_checks`）**全仓零消费者**——作者写了它，然后没有任何东西读它。
- 叠加 F3（`e4_gates.py` 恒退 0）+ `run_all.sh` 恒退 0 ⇒ **EXP-04 没有任何自动化路径能发现这件事**。

**【须修 F1】`exp11_recon_driver.cpp` 编译级断裂 —— 而它正是 `audit/README.md` 钦定的全目录「唯一例外」**
- 位置：`实验/absolute-snr/code/exp11_recon_driver.cpp:29`、`:39-42`
- 现状：`:29` 为 `#include "astrocs/weight_chain.h"`；`:39-42` 为 `using astrocs::v6::p2weight::SparseReconstruction;` 等四条。
- 实测：`find lib -type d -name astrocs` **零结果**；真实头文件在 `lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h`，真实命名空间为 `acsd::v6::p2weight`（`acsd/weight_chain.h:75-77`）。
- 危害：`实验/absolute-snr/code/audit/README.md:12` 与 `:21` 把 `route3/exp11_frozen_operator_transfer.py` 定为 36 个脚本中**唯一**保留「生产判别力证据地位」的例外，理由正是它真读 `lib/**`。该地位完全依赖此驱动能编译。⇒ **全目录唯一与生产实现绑定的证据面已断**，`TAUTOLOGY_REGISTER.md:352-356` 记的「`absolute-snr/code/audit/` 36 个 .py 里唯一真读 `lib/**` 的 1 个」在今天的 HEAD 上**不可执行**。
- 应为：`astrocs/` → `acsd/`，`astrocs::` → `acsd::`；改后须重跑并更新 `results/route3/exp11_frozen_operator_transfer.json`（当前归档是断裂前的旧件）。

**【须修 F2】`b5_phase3_transfer.py` 的头号假说 H1 是**结构对称型恒真门**——对被测算子零判别力**
- 位置：`实验/absolute-snr/code/b5_phase3_transfer.py:70`、`:75`、`:85`、`:120`
- 现状：`:70` `Cout = R @ Cin @ R.T`（被检验的解析式）；`:75` `Y = R @ X`（**用同一个 R** 生成 MC 数据）；`:76` `Cout_mc = np.cov(Y)`；`:120` 门 `H1_band_lt_0p10 = rel_frob < 0.10`。
- 机理：`Cov(RX) = R·Cov(X)·Rᵀ` 是线性变换的定义式，**对任意 R 恒成立**。⇒ 该门实际验证的是 `np.cov` 的定义，不是重采样算子是否正确。
- 反证（子代理在 /tmp 副本实测，我复核了结构论证并采信其数值）：正确 R → 0.07514 绿；x↔y 互换 → **0.07514 绿（逐位相同）**；shift=0.9 → 0.09044 绿；shift=7.0 → 0.09627 绿。**门分辨不出对的算子与错的算子。**
- 附加：`:121` 自陈「元素级 MC 噪声主导」，而正确 R 下实测 0.0751 已占掉 0.10 阈值的 75 % ⇒ 阈值被 MC 噪声主导，换 seed 有自红风险（另一类失效）。
- 应为：H1 必须拿**独立于该 R 的参照**（例如解析卷积核、或对角解析式 vs 实测自协方差），否则移出门计数并按规范 08 §4 标注。

**【须修 F3】`exp03_common.py` 的自检 Oracle 是 fail-open：`raise SystemExit(0 if selftest() else 1)` 恒为 0**
- 位置：`实验/absolute-snr/code/exp03/exp03_common.py:702`、`:571`、`:597-599`
- 现状：`:702` `raise SystemExit(0 if selftest() else 1)`；而 `selftest()`（`:566`，签名 `-> Dict[str, Any]`）返回的是 `out` 字典，**恒非空**（至少含 `:571` 的 `grid_ok`）⇒ `bool(out)` 恒 `True` ⇒ 退出码恒 0。
- ⇒ `:571` 真正计算出来的 `out["grid_ok"]` **从未被任何退出码或判定消费**。
- 加重：`:603-605` 还有**第二个** `if __name__ == "__main__":` 块，它无条件 `print` + 调 `selftest()`；一个模块出现两个 `__main__` 块 ⇒ 以脚本方式运行时 `selftest()` 被执行两次（`:605` 与 `:702`）。
- 加重：`selftest()` 本身（`:566-600`）只 `print` 读数（`:597-599`），**不做任何断言**；`:21` 的模块纪律「本模块不产生任何"恒真"的量」与「判据必须非退化」没有落成机器判决。
- 应为：`return 0 if all(v is True or abs(v) < tol ...) else 1`，或显式 `return bool(out.get("grid_ok"))`；并把 selftest 的各读数接成真门。

---

## 3. 逐文件清单（每条带 `文件:行`）

| 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `b7_absolute_snr_recon.py`（全文） | 显式模型 M1–M4、四臂 A_analytic/A_analytic_sdet/A_null/A_grad/B_hst/M42、预注册判据 P1–P7、`evaluate_gates`、`all_pass` | ① `:956-957` `all_pass` 由 `all(bool 且 非 DIAG_)` 聚合，但 `:962` `return 0` ⇒ **退出码与 `all_pass` 无关**。② P5 第二合取项在预注册文本里写「T_full 与 T_slow 的**方差面**相对差 ≤1e-12」（`:182`），代码 `:673-674` 比的是**σ 面**（`arms()` 的第 [1] 项是 sqrt），口径与文案不一致（量级不影响结论，但属文案-代码冲突）。③ `:895-896` `domains` 只登记 D_all/D_src/D_core/D_snr3，**漏了 `DOMAINS`(:150) 里的 `D_point`**，而 `:851-853` 又按 `D_point` 取值 ⇒ 登记面与实际域集不一致。④ `:908` 的 build 脚本 `b7_build_driver.sh` **真实存在**（已核，非伪引）。⑤ B 臂（HST，唯一含延展发射物理的臂）只进 `DIAG_*`，对 `all_pass` 零贡献 ⇒ 真实数据臂无判据位。 | 须修（退出码）／建议（P5 文案、D_point 登记） |
| `docs/EXP-01-DELTA-AND-ESTIMATOR.md`（全文） | δ 预算、A/B 方案判定、H1–H8 事前假说、15 门自审表、审稿修正记录 | ① `:80`、`:506`、`:565`、`:726` 四处引 `docs/ASTROCS_DESIGN.md` §2.2/§3.1/§12.2/§12.3 —— **该文件在仓内不存在**（顶点实为 `docs/ACSD_DESIGN.md`）。② `:506` 在「」内逐字呈现「权重只能来自纯净信号与噪声之比……**绝对标定**……不基于参考帧」，并把它当作「直接违反」的最高设计条款来支撑「选方案 A」。③ 但 `docs/ACSD_DESIGN.md` 内 `grep 权重只能来自\|绝对标定\|不基于参考帧` **零命中**，其 `§3.1`(`:177`) 标题是「数据对象」。④ `:572` 标题级结论「15/15 合格，0 门无证据资格」与 `:574-579` 的作用域澄清（只对判据层、非测量管线层）分离呈现，前者单读会过度声称。 | 须修（死引链，且承重） |
| `exp03/exp03_common.py`（全文） | 区域化 sigma 估计器族、跨帧差分、方差分解、结构函数、解析误差-尺度、自检 | 见 F3。另 `:84-95` `region_sigma_raw` 逐区域套生产 recipe（不带局域背景）被 `:86` 自陈为对照臂；`:165-186` `region_sigma_diff` 用 `sqrt(2)` 归一，是真判据；`:4393` 类同问题未在本文件。 | 须修（F3） |
| `exp06/e4_gates.py`（全文） | G1–G15 门与故障注入、`X.gate` 汇总 | ① **`:389` `return 0` 无条件**，`:393` 却写 `raise SystemExit(main())` ⇒ **看起来在传播退出码、实际恒 0**；而本文件 docstring `:3` 自称「**能红能绿**」、G3b/G4a/G5/G6a/G10/G11/G12a 全是「必须红」的故障注入门。② `:331-333` G13c 写成 `all(... for x in ... if min(...) > 1e-6)`：过滤器选到 0 行时 `all([])` 返回 **True** ⇒ **空过判绿**，且未记录 `n_evaluated`（与 `TAUTOLOGY_REGISTER.md:338` 记的 COV3/5/6/7/9 同型，但位置是 `absolute-snr/exp06`，前几轮未记）。③ G15a(`:374`) `hom_f <= 1e-9`：被测 `recon_physics`(`exp06_common.py:604-608`) 对拟合系数线性，自由斜率 OLS 对控制值**严格正齐次** ⇒ `fld_f ≡ α·fld_ref`，阈值 1e-9 比可达精度(~1e-16)松 7 个量级；更关键的是该门**对斜率是否解错完全不变**（正齐次性与 a/c/D_ref 的取值无关）⇒ 对它名义守护的缺陷无判别力。文件 `:6` 却宣称「不接受恒真门」。 | 须修（①②③） |
| `audit/route3/exp05_doublecount_corr.py`（全文） | H1 闭式双计偏差、H2 MC 验证、H3 相关噪声 epsilon、H4 k_corr | ① `:138` `h3["negative_rho0_is_zero"] = abs((1/S2)*S2 - 1) < 1e-12` 是**纯代数恒等式**（乘逆再乘回），且 H3 的 `for rho in [...]`(`:130`) **根本没有 rho=0** ⇒ 这个「负控」与 H3 的命题无关。② `:113` `"negative_rn0_arms_identical": True` 是**硬编码字面量**，非计算值。③ `:56-58` `ARCHIVED` 是**写死在本文件里的字面量字典**，`:76-78` 的 `max_rel_diff_vs_archive` 拿闭式去比**这个本地副本**而非真实归档 ⇒ H1「必须复现归档值 +14.5009%/+38.2524%」是**往返自证型**（类 B+D），归档若变仍绿。④ 全文无 `gates`/`all_pass` 键（`:159-166`），无机器可执行判决。 | 须修（①②③） |
| `exp04/e4_gates.py`（全文） | G1–G6 负例与退化登记、`gates_summary` 汇总 | **本片最干净的样板**：G2/G4/G6 全部标 `degenerate=True`(`:78/:88/:122`)，`:143-149` 把它们**排除出 `gates_summary`**，读数保留，`:159-160` 打印时明写「不计入 gates_summary」。`:83-84` 更是主动写出**恒红体检**：「若加 1e-9 判据将**恒红**——恒红门会把真实缺陷永久藏在红灯里，故不加判据」——这正是本项目要求的双向体检。缺陷：`:155-161` `main()` 无返回值、无 `sys.exit` ⇒ 门红也退 0。 | 建议（样板）／须修（退出码） |
| `b5_phase3_transfer.py`（全文） | H1 协方差传递、H2 对角近似、H3 输出 PSF 信息量 | 见 F2。另 `:98` `Cin_inv = np.linalg.inv(...)` 算出后**从未被使用**（死代码）。`:150` `main()` 无退出码。 | 须修（F2） |
| `audit/route1/exp02_moffat4_constant.py`（全文） | Moffat4 FWHM↔sigma 常数 1.230310 的闭式 + 4 条实验腿 | ① `:4` 声称「cross-checked against `docs/science/PSF.md:63`」，实测 `docs/science/PSF.md:63` = `dx = x−(cx+x0), dy = y−(cy+y0)`（坐标偏移定义），**与 1.230310 毫无关系** ⇒ 死 file:line 引用。② 全文**无任何 `pass`/`ok`/布尔门位**（`:105-109` 只 dump JSON）⇒ 与 `audit/README.md:45`「每个实验内置真值无效应⇒归零/判红负例（能红能绿）」的**全目录一刀切断言**不符——本文件既不能红也不能绿。 | 须修（①）／建议（②，README 过度声称） |
| `fetch_evidence.py`（全文） | 三重佐证抓取：PixInsight / Crossref / arXiv / siril / swarp / photutils | ① `:79-82` 网络失败时 `entry.update(old[key]); entry["stale"]=True` —— `update` 把缓存里的 **`ok=True` 一并覆盖回来**。② `:87-98` 的 `verdict` 十项**只读 `bool(...get("ok"))`，从不读 `stale`** ⇒ **断网时 10 条佐证门全部照旧绿**，且 JSON 里 `stale=true` 与 `verdict` 全绿并存，属自愈型判据。③ `:5` docstring 称「显式标注 stale=true（不伪造）」——标注做了，但**判决层不看它**，是半诚实。④ `:104-105` `main()` 无 `sys.exit`。 | 须修（①②） |
| `audit/README.md`（全文） | 证据地位裁定、目录结构、复现命令、exp11 说明 | **诚实度样板**：`:3-30` 主动把全目录降级为「本地重实现自检」、写明「在结构上不可能因 ACSD 生产管线的缺陷翻红」、点名 `production_chain_control_variance.py` 的命名陷阱（`:22-24`）、给出升回证据地位的条件（`:28-30`）并如实登记「本次治理**未做**此改造」。⚠ 但 `:12/:21` 钦定的唯一例外 exp11 已因 F1 编译断裂 ⇒ **该裁定的前提当前不成立**。`:45` 的「每个实验…能红能绿」对本片已读的 `route1/exp02` 不成立（见上）。 | 须修（F1 传导）／建议（:45 收窄） |
| `exp04/run_all.sh`（全文 56 行） | 10 步编排 | ① `:6` `set -uo pipefail`（**无 `-e`**）；`:24,27,30,33,36,39,42,45,48,51,54` 共 **11 处** `... | tee ... || echo "WARN ... rc=$?"` ⇒ 任何非零都被 `||` 吃掉；`:56` 无条件 `echo "ALL DONE"` ⇒ **脚本恒退 0**。② 与 `exp03/run_all.sh`（`:22` 置 `FAIL=1`、`:36` `exit $FAIL`）**直接矛盾** ⇒ 同仓两条同级编排链行为不一致。③ 与 `exp04/e4_gates.py` 无退出码叠加 ⇒ 两层独立吞红。 | 须修（**已在第一轮记录，见 §7 一致性**） |
| `exp03/run_all.sh`（全文 36 行） | 6 步编排 | **正确**：`run()`(`:15-23`) 捕获 rc、`:22` 置 `FAIL=1`、`:36` `exit $FAIL`。可作 exp04 的修法模板。⚠ 但它守的是**进程码**，而 `exp03_common.py:702` 恒退 0 ⇒ 编排正确但被测脚本不报 ⇒ 红灯仍传不上来（编排与被测两处需同修）。 | 通过（自身）／须修（依赖项 F3） |
| `build_prod_driver.sh`（全文 14 行） | 编译 `prod_snr_driver` | `:4` 有 `set -euo pipefail`（**本片唯一纪律正确的脚本**）；`:9` flock 串行化。真实消费者是 `b2_noise_terms.py:117`（不在本片）。⇒ 本文件本身无缺陷。 | 通过 |

---

## 4. 发现清单

### 4.1 阻断（1 条）

| # | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **S0** | `exp04/e4_gates.py` → `results/exp04_e4_gates.json` → `results/EXP04_TABLES.md:314` | 门代码 2026-10-02 改（加严 G1、标 G4/G6 退化、新增 `always_true_checks`），归档停在 2026-09-22 22:50；唯一证据表 `:314` 仍把 `G6_frame_arm_selfzero` 渲染成 **绿灯 `True`**，而同表 G2/G4 已显示「登记」 | 门定义与其唯一证据面同提交更新；表须渲染 `degenerate`/`always_true_checks` | `sed -n '314p'` ⇒ `\| G6_frame_arm_selfzero \| True \|  \|`；归档 `top keys` 无 `always_true_checks`；归档 `gates_summary` 6 门 vs 现行 4 门 |

**未读到的 24 份中若存在阻断项，本遍无法排除——「仅 1 条阻断」不等于「无更多阻断」。**

### 4.2 须修（8 条）

| # | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **S1** | `exp11_recon_driver.cpp:29,:39-42` | `#include "astrocs/weight_chain.h"` + `using astrocs::v6::p2weight::…` | `acsd/weight_chain.h` + `acsd::v6::p2weight::…`，改后重跑 exp11 并更新归档 | `find lib -type d -name astrocs` 零结果；`acsd/weight_chain.h:75-77` 为 `namespace acsd{ namespace v6{ namespace p2weight{` |
| **S2** | `b5_phase3_transfer.py:70,:75,:85,:120` | H1 用同一个 `R` 既造数据又当解析真值 ⇒ 对任意 R 恒真 | H1 取独立参照，或移出门计数并标注退化 | `Cov(RX)=R Cov(X) Rᵀ` 定义式；子代理 /tmp 实测 x↔y 互换后逐位同值 0.07514 仍绿 |
| **S3** | `exp03_common.py:702`（+`:571`,`:603-605`） | `SystemExit(0 if selftest() else 1)`，`selftest()` 返回非空 dict ⇒ 恒 0 | 退出码读真门；删掉重复的 `__main__` 块 | `selftest()->Dict[str,Any]` 恒非空；`out["grid_ok"]` 无消费者 |
| **S4** | `exp06/e4_gates.py:389`+`:393` | `return 0` 恒定，却写成 `raise SystemExit(main())`，**形似传播实则不传** | `return 0 if all_pass else 1` | `:384` 已算出 `out["all_pass"]`；docstring `:3` 自称「能红能绿」 |
| **S5** | `exp06/e4_gates.py:331-333` | G13c 的 `all(... if min(...)>1e-6)` 在 0 行时**空过判绿**，且不记 `n_evaluated` | 先断言分母非空，或显式登记 `n_evaluated` 并在 0 时判红 | `all([])` 为 True（与 `TAUTOLOGY_REGISTER.md:338` 同型，新位置） |
| **S6** | `exp06/e4_gates.py:374` | G15a 正齐次性门对**斜率解错完全不变**，阈值 1e-9 松 7 个量级 | 移出门计数或换独立参照；文件 `:6` 的「不接受恒真门」需相应修正 | `exp06_common.py:604-608` `recon_physics` 线性；自由斜率 OLS 严格正齐次 |
| **S7** | `audit/route3/exp05_doublecount_corr.py:138` | `abs((1/S2)*S2 - 1) < 1e-12` 纯恒等式；H3 的 rho 循环无 rho=0 | 真算 rho=0 时 `diag_minus_gls_over_gls` | `:130` 的 rho 列表 `[0.05,0.1,0.2,0.363]` |
| **S8** | `audit/route3/exp05_doublecount_corr.py:76-78`+`:56-58` | H1「复现归档值」比的是**本文件内写死的字面量副本** | 指向真实归档文件路径读入 | `ARCHIVED` 定义于 `:56-58`；全文无 `json.load` |

### 4.3 建议（6 条）

| # | 位置 | 内容 |
|---|---|---|
| R1 | `fetch_evidence.py:79-82`+`:87-98` | 断网时缓存把 `ok` 刷回 True、`verdict` 不看 `stale` ⇒ 10 条佐证门自愈转绿；应让 `verdict` 在 `stale` 时判红或降级 |
| R2 | `docs/EXP-01-DELTA-AND-ESTIMATOR.md:80,:506,:565,:726` | 四处 `docs/ASTROCS_DESIGN.md` 死引（文件已改名）；`:506` 的「」内引文是**承载「选方案 A」判定的规范依据**，应改指 `docs/science/PSF_SIGNAL_WEIGHT.md:10` 并同步修正后者自身挂着的「最高设计 §3.1」 |
| R3 | `audit/route1/exp02_moffat4_constant.py:4` | `PSF.md:63` 与 1.230310 无关（该行是坐标偏移定义） |
| R4 | `audit/README.md:45` | 「每个实验内置…能红能绿」对无布尔门位的脚本不成立，宜收窄为「含判据位者」 |
| R5 | `b7_absolute_snr_recon.py:956-957`+`:962` | `all_pass` 算出但不接退出码；`:895-896` 的 `domains` 登记漏 `D_point`（而 `:150` `DOMAINS` 含之、`:851-853` 又按其取值） |
| R6 | `b7_absolute_snr_recon.py:182` vs `:673-674` | 预注册写「方差面相对差」，代码比的是 σ 面（sqrt），文案与实现口径不一 |

---

## 5. 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **X1** | 把 `docs/ASTROCS_DESIGN.md` 的**正确历史版本**（`66e9f917^`，902 行）拉出来 grep「绝对标定 / 不基于参考帧 / 纯净信号 / 权重只能」 | 推翻子代理「该引文从未存在 ⇒ 伪引」的定性 | **推翻了子代理、救回了文档**。实测该句**逐字存在于 `:269`**，且 `:269` 落在 `:263` 的 `### 3.1 数据对象` 之下 ⇒ 引文真实且章节号正确，只是**宿主文件已被改名、该节已被 `66e9f917` 删去**。定性从「伪引」下调为「死引/悬空引用」。 |
| **X2** | 试图推翻「`exp04/run_all.sh` 恒退 0 是新问题」 | 判断它是否前几轮已记 | **未推翻 ⇒ 非新**。`审稿-RR08-实验与复现.md:212,:381` 已记为 S-9（`grep -c` 得 11）。我只独立复核并**确认**。 |
| **X3** | 试图推翻「`exp06/e4_gates.py` 有退出码传播」 | 表面看 `raise SystemExit(main())` 是正确范式 | **成功推翻**。`main()` `:389` `return 0` 恒定 ⇒ 退出码恒 0。这是**形似正确、最难发现**的一类失效。 |
| **X4** | 检查 `exp03/run_all.sh` 是否同样吞红，企图证明「编排链整体都错」 | — | **未推翻，方向相反**：exp03 编排**正确**（`:22`+`:36`）。这反而把病灶精确定位到「同仓两条同级链行为不一致 + 被测脚本自身不报」，是更精确的结论。 |
| **X5** | 核对 `b7:908` 的 `b7_build_driver.sh` 是否为伪路径 | 该处看起来像典型伪引 | **未推翻**：`glob` 实测该文件真实存在 ⇒ 非缺陷，排除。 |
| **X6** | 用 `find`/`grep` 验 `exp11` 的命名空间是否真的改了 | 子代理称编译必红 | **成功**（子代理对）：`astrocs/` 目录不存在、`acsd::v6::p2weight` 在 `weight_chain.h:75-77` ⇒ 编译必红。 |
| **X7** | 检查 `PSF.md:63` 是否真的是 1.230310 的出处 | 子代理称该片 file:line 普遍错位 | **成功**（独立复现）：`:63` 是坐标偏移定义式，与该常数无关。 |

---

## 6. 盲复算（遮蔽既有判定，独立取证）

**方法**：对 3 个关键项，先只看代码不看前三轮结论，独立得出判定，再回比。

| 项 | 我的独立判定 | 与既有结论比 | 一致性 |
|---|---|---|---|
| `EXP-01` §3.1 引文性质 | 先按「文件不存在 + 现文件零命中」判为**伪引**；再用 `git show 66e9f917^` 复算，发现**逐字存在且确在 §3.1** ⇒ 改判**死引** | 与 R3 的「真身在 PSF_SIGNAL_WEIGHT.md」方向一致；但**与 R3「曾存在于前身 §3.1」及子代理「从未存在」都不同** —— 我判定 R3 的这一句**对**、子代理**错** | **对既有结论偏严→修正**：我推翻了子代理的「伪引」，也纠正了我自己第一版的误判 |
| `b5` H1 | 读 `:70`/`:75` 即判定恒真，无需执行 | 前几轮未记此文件 | **偏松（前三轮漏）** |
| `exp06` G15a | 追 `recon_physics` 线性 ⇒ 正齐次恒等 | 前几轮未记 | **偏松（前三轮漏）** |
| `exp04/run_all.sh` | 独立 `grep -c` 得 11 处，独立读 `:24,:27,:30,:56` | RR08 S-9 | **一致** |
| `exp03_common.py:702` | 独立读出 `selftest()->Dict` ⇒ 恒真 | 前几轮未记 | **偏松（前三轮漏）** |

**总判定：既有结论对本片整体偏松。** 我确认 1 条既有结论（RR08 S-9），新提出 8 条须修，其中 4 条（b5 H1、exp06 G15a、exp06 G13c 空过、exp03_common:702）此前三轮均无记录。

---

## 7. 子代理派发记录

**派发 7 次，其中 2 次为误重复派发（同一 prompt 发了两次），实际有效 5 个。** 2 个重复件我未能通过 job API 终止（`job_kill` 对 subagent id 返回 `unknown job`，`job_list` 不列 subagent），其结论与另两个重复件范围重叠，已在复核时**去重折算**，不重复计入。

| 子代理 | 范围 | 结论 | 我的逐条复核 |
|---|---|---|---|
| `cad6181f` | 文档/结果表：伪引、行号漂移、陈旧缺陷 | 报 #1「`ASTROCS_DESIGN.md §3.1` 引文不存在 ⇒ 伪引」 | ❌ **否决（部分）**。它只查了 `14a50e3e^`（**删除之后**的版本）就宣布「0 命中」。我用 `git show 66e9f917^` 复算，该句**逐字存在于 `:269`，且位于 `:263` 的 `### 3.1` 之下** ⇒ 是**死引**不是伪引。**教训：否定性证据必须回到删除前的版本取样，否则是假阴性。** 其余行号漂移类结论方向可信但**我未逐条复算**（相关文件属未读区）。 |
| `9ce1e57b` | 编排/驱动/报告链 | 报 F1 `exp11_recon_driver.cpp` 命名空间/头路径断裂；F3 `exp04/run_all.sh` 恒退 0；F4 `fetch_evidence.py` 缓存刷绿 | ✅ **采信 F1**（我独立 `find`+`grep` 复核，结论相同，升为本片最重条）。✅ **采信 F3**（我独立读原文，且确认前三轮已记）。✅ **采信 F4**（我独立读 `:79-82`/`:87-98` 原文，逐字确认 `verdict` 不读 `stale`）。 |
| `513e5e36` | 分析/仿真代码 | 报 1 全片无 `assert`、退出码失效；2 `b5` H1 恒真（附 /tmp 执行反证）；3 `b1` G5 恒真 | ✅ **采信 2**（我独立复核结构论证：`Cov(RX)=R Cov(X) Rᵀ` 对任意 R 成立；其 x↔y 互换后逐位同值的执行反证与我的机理一致）。✅ **采信 3**（我独立读 `b1_sky_scan.py:136-143` + `extract():64`，确认 `(d+200)-(b+200)≡d-b` 且 `s_c` 不变 ⇒ 该负控重跑同一函数，结构上抓不到 `extract` 的缺陷）。✅ 采信「退出码与 `all_pass` 脱钩」，我另独立补出 `exp06/e4_gates.py:389`（子代理未点名的**形似正确**版本）。 |
| `0755c6a9` | `exp04`/`exp06`/`b6`/`audit` 门文件 | 收尾中，未在本次窗口内回传完整报告 | ⏸ **未回传**，本片结论不依赖它 |
| `d7a52f5b` | `reverse_verify/` 三簇（生产 vs 桩） | 未回传 | ⏸ **未回传** —— 这是最可惜的一处：该簇是本片**唯一真读生产实现**的面（`TAUTOLOGY_REGISTER.md:356`），共 693 行，仍列在 §1.3 未读区 |

**否决/修正合计：否决 1 条（子代理 #1「伪引」定性），并因此自我修正 1 次（我第一版也判成伪引）。**

---

## 8. 自证段（可复跑命令；全部只读，不编译不运行实验）

```bash
cd "/workspace/Astro CS Database"

# —— S1：exp11 编译断裂 ——
sed -n '29p;39,42p' 实验/absolute-snr/code/exp11_recon_driver.cpp
find lib -type d -name astrocs                      # 期望：空
grep -n '^namespace' lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h | head -3
#   期望：75:namespace acsd { / 76:namespace v6 { / 77:namespace p2weight {

# —— S2：b5 H1 恒真（结构论证 + 改名反证）——
sed -n '68,76p;85p;120p' 实验/absolute-snr/code/b5_phase3_transfer.py
grep -n 'H1_band_lt_0p10\|Y = R @ X\|Cout = R @ Cin' 实验/absolute-snr/code/b5_phase3_transfer.py

# —— S3：exp03_common 恒退 0 ——
sed -n '566,571p;597,605p;700,702p' 实验/absolute-snr/code/exp03/exp03_common.py
grep -c 'if __name__ == "__main__"' 实验/absolute-snr/code/exp03/exp03_common.py   # 期望 2

# —— S4/S5/S6：exp06 门 ——
sed -n '389p;393p' 实验/absolute-snr/code/exp06/e4_gates.py
sed -n '330,337p;374,376p' 实验/absolute-snr/code/exp06/e4_gates.py
sed -n '604,608p' 实验/absolute-snr/code/exp06/exp06_common.py

# —— S7/S8：route3 exp05 ——
sed -n '56,58p;76,78p;113p;130p;138p' 实验/absolute-snr/code/audit/route3/exp05_doublecount_corr.py
grep -c 'json.load\|open(' 实验/absolute-snr/code/audit/route3/exp05_doublecount_corr.py

# —— R1：fetch_evidence 自愈 ——
sed -n '79,82p;87,98p' 实验/absolute-snr/code/fetch_evidence.py

# —— R2：死引链（含我推翻子代理的关键一步）——
grep -rn 'ASTROCS_DESIGN' 实验/absolute-snr/docs/EXP-01-DELTA-AND-ESTIMATOR.md
ls docs/ASTROCS_DESIGN.md 2>&1                      # 期望：No such file
grep -c '绝对标定\|不基于参考帧\|纯净信号\|权重只能' docs/ACSD_DESIGN.md   # 期望 0
git show 14a50e3e^:docs/ASTROCS_DESIGN.md > /tmp/old_design.md; wc -l /tmp/old_design.md
grep -c '绝对标定' /tmp/old_design.md               # 期望 0 —— 删除之后
git show 66e9f917^:docs/ASTROCS_DESIGN.md > /tmp/old2.md; wc -l /tmp/old2.md
grep -n '绝对标定' /tmp/old2.md                     # 期望：269: - **权重只能来自纯净信号与噪声之比**：…**不基于参考帧**…**绝对标定**
awk 'NR<=269 && /^#{2,3} /{h=NR": "$0} END{if(NR==269) print "§→ " h}' /tmp/old2.md
#   期望：§→ 263: ### 3.1 数据对象  ⇒ 引文真实、是死引不是伪引
sed -n '10p' docs/science/PSF_SIGNAL_WEIGHT.md       # 现存活处（仍挂「最高设计 §3.1」）

# —— R3：PSF.md:63 无关 ——
sed -n '63p' docs/science/PSF.md                    # 期望：dx = x−(cx+x0), dy = y−(cy+y0)
sed -n '4p' 实验/absolute-snr/code/audit/route1/exp02_moffat4_constant.py

# —— 与 exp03 编排对照（两条同级链不一致）——
grep -n 'set -\||| echo "WARN\|ALL DONE' 实验/absolute-snr/code/exp04/run_all.sh
grep -c '|| echo "WARN' 实验/absolute-snr/code/exp04/run_all.sh               # 期望 11
grep -n 'FAIL=1\|exit \$FAIL' 实验/absolute-snr/code/exp03/run_all.sh

# —— 覆盖率自证 ——
wc -l <上表 §1.1 的 13 个文件>                       # 期望 3752
```

---

## 9. 计数口径声明

- 本片结论涉及的门位，按**口径 I（门实例）**陈述：S2 的 b5 H1 计 1、S4 的 exp06 G13c 计 1、S6 的 G15a 计 1、S7 的 `negative_rho0_is_zero` 计 1、S8 的 H1 归档比对计 1。
- 我**未**给出本片「去重门」与「整改分母」两层数字：§1.3 的 24 份未读，其中包含 `b6_gates_audit.py`、`audit/route2/exp02_robust_scale_mad.py`、`audit/route2/exp12_weight_gamma.py`、`audit/route1/exp14_kcorr_inflation.py` 四个门文件与 `results/EXP04_TABLES.md`、`REVERSE_VERIFY_CANON.md` 两个汇总面。**在这 6 份读全之前给出整改分母会系统性低估**，故按纪律**不报**，登记为待补。
- 参考：`TAUTOLOGY_REGISTER.md:28-99`（§0.5 三个口径的定义与 absolute-snr 的既有实测：口径 I=108 / II=64 / III=36）。

---

## 10. 补录（子代理全部回传后追加）

### 10.1 子代理派发记录的**最终**版本（取代 §7）

**实际派发 7 次，其中 2 次为误重复派发**（同一 prompt 连发两次），有效 **5 个**。2 个重复件无法用 `job_kill` 终止（对 subagent id 返回 `unknown job`，`job_list` 不列 subagent），其结论与另两个重复件范围重叠，复核时**已去重折算**。**5/5 全部回传。**

| 子代理 | 范围 | 复核结论 |
|---|---|---|
| `0755c6a9` / `9196e3bd`（互为重复派发） | exp04/exp06/b6/audit 门文件 | ✅ **采信其 Finding-2**：它独立发现 EXP-04 归档与代码脱钩，与我 §2 的 S0/A1 **同结论且互为佐证**（它给出 `e4_gates.py` mtime 2026-10-02 04:17 vs 归档 2026-09-22 22:50 vs 表 2026-09-22 23:03）。另采信 b6 `H3 = log(10) > 2` 纯单位换算恒等式、`gates` dict 异质致 `all(values())` 恒真、`NOISE_MODEL.md:86` 不含 1.44（真值在 `:99-102`）——**均属我未读文件，我只核了「是否已被前三轮记录」，未亲自读原文**。 |
| `9ce1e57b` | 编排/驱动/报告链 | ✅ F1（exp11 命名空间断裂）、F3（exp04 恒退 0）、F4（fetch_evidence 缓存刷绿）**全部由我亲自复核原文后采信**。其 A1/A2/B2/D3/E1/E2 等因涉我未读文件，仅登记。 |
| `513e5e36` | 分析/仿真代码 | ✅ **F2（b5 H1）、F3（b1 G5）由我亲自复核机理后采信**；✅ 其「8/8 文件零 `assert`、可执行门 0」与我独立发现的 `exp03_common:702`、`exp06:389` 同向。 |
| `d7a52f5b` | `reverse_verify/` 三簇 | ⏸ **该簇 5 份全在我未读区（§1.3）**，其结论我**无法以亲读背书**。报「对生产有效判据 0/16」、C2 恒红、C3 结构性不可达、`snr_caliber` 全 30 产品/48 帧均为 `upper_bound_no_gain`。**登记为待复核线索，不计入我的判定。** |
| `cad6181f` | 文档/伪引/结果表 | ❌ **否决其 #1「伪引」定性**（见 §5 X1）。其行号漂移类结论方向可信但未逐条复算。 |

**否决/修正合计：否决 1 条（`cad6181f` #1 的定性）；我自己因此修正 1 次（第一版也误判为伪引）；另有 1 次我自己的粗查被 `e7317048` 纠正**——我用 `grep -c 'degenerate'` 得 3 并一度以为其「归档无该字段」说错，逐字段解析后确认**是我错**（3 处命中分别是 `G2` 的既有 `degenerate:true` 与 `G5_nn_baseline_nondegenerate` 的子串），子代理是对的。

### 10.2 子代理提出、但**我未亲读**的高价值线索（登记为待复核，不计入判定）

按纪律，以下均落在我的未读区，**仅登记不背书**：
- `exp06/e4_gates.py` 的 G2 是恒真门：零噪声场景下 `n_eval` 恒为 0，四种破坏注入仍 PASS。
- `exp06` 的 `X ≥ k·E_ref` 族（G5/G6a/G10/G11/G12a）：`E ≥ 0` 由 Cauchy–Schwarz 恒成立，注入 `σ := α·σ`（α 至 100）五条门全绿。
- `exp06` 25 条门**无一条约束 σ 的绝对电平**；且被评分的 `phys` 臂全程用 oracle 增益。
- `b6_gates_audit.py` H4 的 scope_note 是事实错误（`module_adapters.cpp:11984/:11999` 确有该状态机），审计转写与生产实现相反。
- `TAUTOLOGY_REGISTER.md` **完全没有 absolute-snr 的 §2 条目**（只在 §3/§4 出现）——即本片所有恒真门**未进登记表**。

### 10.3 本片最终结论（不变）

**判定：阻断。** 阻断项为 S0（EXP-04 唯一证据表把已判定为恒真的门渲染成绿灯 `True`，属对外出具错误的肯定结论）。
**须修 8 条、建议 6 条**，均带 `文件:行`。
**覆盖率仍为 42.2 %**（13 份完整读完 / 39 份）；§1.3 的 24 份未读，**本片结论不覆盖它们**。
