# G08-05 对抗审稿 · 第 1 遍 · 片 `EXP-absolute-snr-001`

- **片号**：`EXP-absolute-snr-001`
- **基线**：仓库 `/workspace/Astro CS Database`，`HEAD = f9650dd0`（`git -c core.quotepath=false log --oneline -1` 实测）
- **纪律**：零 git 写（无 add/commit/checkout/reset/stash/`git rm --cached`）；零文件修改；未编译、未跑 ctest/pytest/构建、未跑任何实验脚本；中文路径一律 `git -c core.quotepath=false`。唯一写入 = 本交付件。
- **权威依据**：`run/GOVERN-08/工作包-GOVERN-08原件/`（仓内）；**未读** `/tmp/acsd_g08/`。
- **前轮产出**：已读 `审稿-RR*.md` / `审稿-R2-*.md` / `审稿-R3-*.md` / `实验/TAUTOLOGY_REGISTER.md` 作为**线索**；下文每条结论均来自我自己读完的原文，线索只用于定向。

---

## 1 读完了吗

### 1.1 计数（三层口径分开写）

| 口径 | 数 | 说明 |
|---|---|---|
| **成员份数**（分片清单给定） | **38** | `分片清单/片清单-权威版.yaml:1937-1982` |
| **成员总行数**（清单 `实际行数`） | **8879** | 同上 `:1941` |
| 成员总行数（我 `wc -l` 实测） | **8882** | 差 +3 行；逐份差为 `b4_integration.py` 433/434、`exp08_mask_radius.py` 165/166、`exp02/run_all.sh` 52/53 —— 清单尾数误差，**无内容缺失** |
| **我亲自用 `read` 逐行读完** | **38 / 38 份** | 全文，无跳读 |
| **我实际读的行数** | **8882 / 8882** | 100% |
| **覆盖率（份）** | **100%** | |
| **覆盖率（行）** | **100%** | |

**未读完的部分：无。** 本片不存在未读成员。（对比：4 个子代理的覆盖率分别为 100% / 78% / 71% / 67%，我已逐条复核其头条结论并在 §7 说明采纳与否。）

### 1.2 为判定结论而额外读了 12 份**非成员**文件（必须读，否则无法判门性质）

| 文件 | 为什么必须读 |
|---|---|
| `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:104-153` | 裁决 `REPORT_paper.md:181` 的真伪 |
| `lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h:70-80` | 裁决驱动编译是否可能 |
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp:14-24,48-56,149-152,236-240` | 裁决常量行锚与 P12 对拍 |
| `lib/algorithms/noise_snr/cpp/include/snr_estimator.h:301-310` | 宽度约定双入口 |
| `docs/detail/registry/acsd.phase1.noise-snr.md:72-191` | 裁决三口径适用域与引用真伪 |
| `docs/detail/registry/README.md`（ls） | 裁决 `astrocs.*` 路径失效性质 |
| `docs/science/NOISE_MODEL.md:88-98` | 掩膜式裁决 |
| `docs/science/CALIBRATION.md:65-75,89-93,236,459` | 校准式与 UNRESOLVED 登记锚 |
| `实验/absolute-snr/code/reverse_verify/frame_snr/frame_snr_physical.py:140-208` | 裁决 P8b/P9/P10 是否恒绿 |
| `实验/absolute-snr/code/audit/results/route2/{exp08_mask_radius,exp11_idw_reconstruction,exp01_mag_scale_identities}.json` | 归档实测读数（负例是否真红） |
| `run/reverse_verify/frame_snr/{redlines_physical,external_crosscheck}.json` | 归档红灯/绿灯实证 |
| `实验/absolute-snr/code/exp11_recon_driver.cpp:25-45`、`lib/algorithms/integration/` 目录树 | 编译可行性 |

---

## 2 本片判定

### **判定：需修**（不含"阻断"级：片内**没有**发现会让本片结论整体失效、且必须先裁决才能继续的错误；但有 **6 条须修**、**3 条须修级证据链断裂**、**11 条建议**。）

> 口径说明：我在本片**未**发现"必须先裁决才能继续"的阻断项。最重的三条是下列 **S1/S2/S3**。它们是**须修**，不是阻断，理由：①它们不改变本单元其他结论的方向；②其中两条（驱动编译断、报告与生产码事实相反）虽会误导下游，但都已有明确的、可执行的修法，且不依赖新的科学裁决；③真正需要负责人裁决的（P2→NOISE_MODEL 掩膜式冲突）已被 `REPORT_paper.md:98` 自行登记为待裁决。

### 最重的 3 条

1. **【S1 · 须修】`REPORT_paper.md:181` 把一条生产码里已明确"已修/退役"的缺陷，连同一个在现行生产下不可能发生的 `2×10¹⁰` 倍 SNR 高估，写成现行生产的开放缺陷，并给出 4 个错误行号。**
   - 报告原文（`REPORT_paper.md:181`）：「生产估计器把 `sigma_hat` **静默置成地板 1e-9**（`star_detector.cpp:107` 的裁剪窗地板、`:122` 的置地板分支）⇒ `SNR_frame` **高估 2×10¹⁰ 倍，链上无任何守卫**」「**修法属生产码治理，本单元只登记不落码**」。
   - 生产码实读：`star_detector.cpp:107` = `for (size_t i = 0; i < kn; ++i)`（循环头，**不是**裁剪窗地板；地板真身 `:108`）；`:122` = `if (!std::isfinite(*bg) || !std::isfinite(*sigma)) return false;`（**非有限守卫，不是置地板分支**）；`:139` = `if (!(*sigma > 0.0)) return false;`，其上注释逐字写「σ == 0 与 NaN 同处置：**显式失败**，不静默置地板」；`:149-152` `detect()` 以 `ErrorDomain::DATA` fail-closed。
   - 本片内被引脚本 `code/redteam/rt_sigma_hat_applicability.py:7-13` **逐字自陈相反**：「**末端分支是反例夹具，不是现行行为。**…现行生产末端是 `if (!(*sigma > 0.0)) return false;`（`star_detector.cpp:139`）…⇒ 现行行为下该帧**根本不产出 `SNR_frame`**，也不存在 2×10¹⁰ 倍高估。」
   - ⇒ **同一交付单元内，论文与它自己引用的脚本、对生产码三方直接冲突，且论文那份是错的。** 报告据此提出"处置为整帧 fail-closed 拒收"的治理建议 —— 该建议**已落地**，写成待办是把已完成项当缺陷登记。

2. **【S2 · 须修】全篇唯一的 `[生产证据]` 在 HEAD 无法编译；它的归档读数产生于品牌改名之前，改名后从未重跑。**
   - `REPORT_paper.md:3-9` 把 `code/audit/route3/exp11_frozen_operator_transfer.py` 定为 36 个审计脚本中**唯一例外**的生产证据，`:108` 报「**16 门全绿（red_gates = []）**」。
   - `code/exp11_recon_driver.cpp:29` = `#include "astrocs/weight_chain.h"`；`:39-42` = `using astrocs::v6::p2weight::SparseReconstructor;` 等 4 行。
   - 实际生产头：`lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h`，`:75-77` = `namespace acsd { namespace v6 { namespace p2weight {`。该 include 目录下**只有 `acsd/` 子目录，没有 `astrocs/`**（`ls` 实测）。
   - `code/exp11_build_driver.sh:10-14` 的 `-I` 指向 `.../phase2_integrate/include`（正确），但被编译的源 `#include "astrocs/..."` ⇒ **编译必失败**。
   - `code/b7_build_driver.sh:11` 更严重：`V6="$ROOT/lib/algorithms/integration/v6"`，`:22` 引 `"$V6/src/weight_chain.cpp"` —— `ls lib/algorithms/integration/` 只有 `memory.md module.yaml phase1_product phase2_integrate README.md`，**无 `v6/`**。

3. **【S3 · 须修】`code/exp04/run_all.sh` 把本片唯一会返回非零的判据脚本的红灯整个吞掉，仍打印 "ALL DONE"。**
   - `code/exp04/run_all.sh:6` = `set -uo pipefail`（**无 `-e`**）；11 个步骤每一个都是 `... | tee log || echo "WARN xxx rc=$?"`（`:23-24, 26-27, 29-30, 32-33, 35-36, 38-39, 41-42, 44-45, 47-48, 50-51, 53-54`）。
   - `:23-24` 正是 `step "[1/10] 算子自检 Oracle（红/绿）" / run selftest_operators.py ... || echo "WARN selftest rc=$?"` —— `selftest_operators.py` 是本片唯一会 `return 0 if ok_all else 1` 的判据脚本。
   - 末行无条件 `echo "ALL DONE. …"`。
   - 同仓正确样板：`code/exp02/run_all.sh:18,25,48-53`（`rc=1` 聚合 + `exit "$rc"`）；`code/exp06/run_all.sh:15-28,54-57`（`FAILED` + `exit 1`）。**只有 exp04 缺闸门。**

---

## 3 逐文件清单

38 份全读。每份给：读了什么 → 看到什么 → 判定。

| # | 文件（`文件:行`） | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 01 | `实验/absolute-snr/docs/snr-propagation-design.md:1-1539` | 全文四遍（摘要/判据表/§13 复核/§12 边界） | §13 对自家长文的 11 处订正是**真阳性**（131%→31.3%、drizzle 未求和、eps_theta 量级），写得比原文严谨；但 §5.4 结论句 `:1074` 仍用被 `:1483` 判"不成立"的 131% 驱动优先级排序；§10 `:1296-1300` 五条脚本路径写 `experiments/snr_design/…`（该目录不存在）；`:1233` 引 `reverse_verify/README.md §3`（该文件不存在）；`:136` 残句"…registry 锚:（原行锚 31 在新分册无对应落点）"；`:502` 称与权威「**逐字一致**：『实际 SNR = 帧级 × 帧内』」，该串在权威中不存在；`:507-514` 订正为绝对 SNR，但 `:1526-1527`、`:1535` 又把被取消的 (b) 相对分解写成"逐字一致" | **需修** |
| 02 | `实验/absolute-snr/code/reverse_verify/frame_snr/run_redlines_physical.py:1-726` | 全文 | `:426-430` 的 `probe_inconclusive` 是本片**最好的一处**（显式禁止"探针无判决 ⇒ 记为生产拒绝"）；但 P8b`:264`/P9`:338`/P8c(ii)`:306`/P14(a)`:634` 四条门**结构上恒绿**（我自己读 `frame_snr_physical.py:149-151` 确认：环带中位数 + MAD 天然平移不变）；docstring `:9-10` 声明的「与解析预测一致（<5%）」在 `:229` 的 `pass` 里**不存在**，而归档实测该偏差为 `[0.1142, 0.1570, 0.0585, 0.0189, 0.0160]` ⇒ **补上即两点常红**；`:601-602` 的 `hst_note` 声称「全仓 `find -iname '*hst*'` 命中 0」**为假**；`:64` 引 `docs/frame-snr-canon.md:375`（路径不存在，且真文件 `:375` 不是"P12 PASS"，该串在 `:403`）；`:3` 含日期"2026-09-19" | **需修** |
| 03 | `实验/absolute-snr/code/exp04/operators.py:1-549` | 全文 | `:122-144` `op_bilinear_prod` 自称"**生产现行算子语义**…逐位等价（见 selftest 的 S6）"，但它是 `op_bilinear`(`:108-119`) 的手抄副本，且 `:125` 引的 `lib/algorithms/integration/v6/src/weight_chain.cpp:136` **路径不存在**（真为 `phase2_integrate/src/weight_chain.cpp`）；`:128` 自承生产越界 `fail-closed`、`:135-136` 实现为 clip —— **被点名的那条行为没复刻**；`:476-479` 局部均值扣除使"常数场逐位复现"成为构造保证 | **须修** |
| 04 | `实验/absolute-snr/docs/surveys/frame-snr-survey.md:1-473` | 全文 | §0 纠错表（SExtractor 无 `FLUX_GAUSS`、Naylor/Irwin DOI、psphot 论文对调）**质量很高**；但 `:6` 引 `../README.md §5` —— `docs/README.md` 只有 4 个无编号节，**无 §5**；`:443` 称 P10 红例 B 是"功率比"口径的数值证据，实读 `frame_snr_physical.py:203-204` `red_b` 的分子是 `raw_sum`（未扣背景通量），**根本没有实现 `σ²/σ_n²` 功率比估计量**，且归档 JSON 里 `RED_A` 与 `RED_B` 五个值**逐位相同**；`:470-471` 再次声称"全仓 `find -iname '*hst*'` 命中 0"**为假**；`:322` 把 `rel_diff = 0.0`（恒等式结果）当"差异全部来自口径约定"的证据 | **需修** |
| 05 | `实验/absolute-snr/code/b4_integration.py:1-434` | 全文 | `:6`/`:14` docstring **自己写明**「定义式恒等」「（恒等）」，却仍列为 `gates`：H1 `:371`、H5 `:406`、H4-equal `:391`、H5-zero-spread `:419` —— 四条**代数恒等式型恒绿门**（H1：`sqrt(Σ(F/σ_F)²)` 与 `F/sqrt(Σ1/σ_F²)` 是同一表达式；H4-equal：`h/K` 除以 `h/K`）。真 MC 门 `:373-378, 380-404, 420-421` 判别力正常。`:32` 的 `RN` 常量被记进 `frozen_config` 但**计算中从未使用**（死常量） | **须修** |
| 06 | `实验/absolute-snr/code/sci_b_common.py:1-402` | 全文 | `:6-7` 权威依据引 `docs/ASTROCS_DESIGN.md`、`ACCEPTANCE_SPEC.md`、`docs/plugins/algorithms_phase1/07_noise_snr.md` —— **三个全不存在**；`:29` `HST_M16 = ROOT/testdata/HST_M16` —— **本片自己的公共库就指向 HST 数据**，直接反驳 02/04 的"本仓无哈勃数据"；`:31-35` 常量注释称"与生产源逐位一致，见 `snr_science.cpp:50-54`"，但 `kTrimMeanToSigma` 在 `:56`（行区间少 2 行）；`:33` `K_MOFFAT4_FWHM=1.230310` 是手抄截断值（闭式 1.2303076525901024，相对差 +1.9e-6），所有 `moffat4_grid()` 消费者都带这个偏差；`:205-240` `prod_mirror_snr` 是正确的"Python 镜像 vs 生产 C ABI"对拍方向；`:373-386` `weight_efficiency_loss` 以**真值**入参，是本片判别力的真正来源 | **须修** |
| 07 | `实验/absolute-snr/code/reverse_verify/snr_design/exp2_sparse_snr_reconstruction.py:1-368` | 全文 | `:144-159` `fit_ell` 确为 `np.linspace(0.5,200.0,400)` ⇒ 0.5 patch = 16 px，文档 `:469` 的说法**核实为真**；但**全文件无 `assert`、无 `pass` 字段、退出码恒 0**（`:334-364` 只写 JSON + print）⇒ 设计文档 §8 声称的 C3.1–C3.8 阈值（含"C3.4 Pearson>0.5，实测 0.276 为红"）**只存在于文档**；`:246-261` 三处 `except Exception: pass` 静默丢算子；`:299-303` 的 `guard` 算完即丢（`out` 里无该键）；`:269-270` `weight_penalty_pct = 4·rmse²` 把**单帧空间**重建误差当 EXP-1 的**帧间独立**权重误差代入，而设计文档 `:1460` 自认该度量"故意对共模空间误差不敏感" | **须修** |
| 08 | `实验/absolute-snr/code/b3_domain_map.py:1-342` | 全文 | **本片最规范的一份**：`:311-315` 主动识别出一个恒真门（平坦场下 `frame_median` RMSE≡0）、写下数学原因、隔离成 `DEGENERATE_*` 前缀记录位、写明"**不作证据**"；`:144-149` 缺臂显式记录不静默丢弃；`:251-252` 缺 testdata `raise RuntimeError(... fail-closed，禁止静默少面)`；`:317-328` N2/N3 是真 fail-closed 硬门。**问题**：`:33`/`:46` 的 `EXP-205` 冻结门在全仓找不到对应实验编号；`:34` 同样用 HST M16 | **通过**（样板） |
| 09 | `实验/absolute-snr/code/exp02/e2_hst_scan.py:1-304` | 全文 | `:235-236` 负例必须同时关掉平场 PRNU/低阶/渐晕 —— **好纪律**；`:254` `G_null_zero_pass` 是唯一真门；`:300` `return 0` 恒绿；`:53` `HST_GLOB = "testdata/HST_M16/*M16*.fits"` —— 第三次使用 HST 数据 | **须修**（退出码） |
| 10 | `实验/absolute-snr/code/reverse_verify/p7_noise/exp4_scatter_sp0.py:1-262` | 全文 | `:194-195` P1_pass 是**结构性恒绿**：臂 A 三帧同 `frame_index=0`、同波段、同平场、只换噪声种子，而 `var_true = truth_variance_adu2(...)` **与 seed 无关** ⇒ 三张真值图逐位相同 ⇒ `R≡1`、`D≡0` 由构造保证；`:202-203`/`:224-225` 的 P2/P3 在**扣除了解析已知的 `src_e/g²`** 之后跑原阈值 —— 文档 `:32-33` 自陈"修订 = 在扣除/控制真实底贡献后检验…而不是放宽判据"，**诚实度可嘉，但判别力已被构造掉**；原始读数保留为 `*_RAW_RETIRED`（`:216-232`）是好做法；`:258` `return 0` **无条件** —— 六个门算完、打印、写盘后丢弃 | **须修** |
| 11 | `实验/absolute-snr/code/exp05/e1_analytic.py:1-244` | 全文 | `:156-158` `degenerate` 是可红门；`:91-92` 主动禁用 Python 字符串 hash 以保可复现（好）；`:240` `return 0` 恒绿 | **建议** |
| 12 | `实验/absolute-snr/code/exp04/exp04_common.py:1-241` | 全文 | `:21-23` **显式禁用**已识别的恒真判据（"帧级臂 RMSE ≤ K·s_field 对任意真值场恒真"）—— 与 08 同属正确样板；`:45` `07_noise_snr.md §5` 路径不存在；`:46` `EXP-205` 同 08；`:59` 第 N 次用 HST M16 | **通过**（样板）+ 建议 |
| 13 | `实验/absolute-snr/code/exp06/e3_real.py:1-224` | 全文 | `:61-63` 奇偶棋盘 hold-out 是真外部检验；`:95-110` 二阶差分（`Var = 6σ²`）是算法独立交叉验证 —— 好；**但 `:205-207` `if not Path(p).exists(): print("MISSING"); continue`** —— 缺帧静默跳过、少一帧照样出 JSON 照样 `return 0`，与本片 `b3_domain_map.py:252` 的 fail-closed **纪律直接冲突**；`:220` `return 0` 恒绿 | **须修** |
| 14 | `实验/absolute-snr/code/exp05/e2_hst.py:1-214` | 全文 | `:100-111` 逐帧取真值（每帧天光不同）—— 正确；`:49` 第 N 次用 HST M16；`:210` `return 0` 恒绿 | **建议** |
| 15 | `实验/absolute-snr/REPORT_paper.md:1-206` | 全文 | `:3-10` 的**证据等级警示框是本片最值得肯定的一处**（36 脚本 35 个不读生产，绿灯不构成生产判别力证据，并给出升回条件）；`:165` 把同批判红臂（检测完备性 0.43、`D_core` 0.376 判红、HST 源项捕获 14.2%、帧标量偏低 15–77%）与绿臂**同权报告** —— 这是本片选择性报告的**反例**。**但** `:181` 见 S1；`:104` 「flat_with_struct 臂 −45.70%」与 `results/EXP05_TABLES.md:13` 的 **−44.12%** 不符（−45.47% 是 `vary_with_struct`）；`:102` 「地面视宁度受限域稀疏口径**全部**优于帧级标量」与权威 `acsd.phase1.noise-snr.md:138-139`「**稠密与稀疏互有胜负**…该域只给**定性**结论」冲突；`:97-98` 自陈的"本单元掩膜式"与其引用的 `exp08_mask_radius.py:129-130`（用的正是**正本式** `clip(r_local, r_min, 60)`）不符 ⇒ 该"待裁决冲突"的前提被自己的引文推翻 | **需修** |
| 16 | `实验/absolute-snr/code/reverse_verify/frame_snr/crosscheck_photutils.py:1-192` | 全文 | `:138-147` `rel_diff_same_convention` 是**字面 `X − X`**（`var_ap_same = var_ap_phot` 字面赋值，两边分子是同一次 `np.sum(img[in_ap]−bkg_ap)`）⇒ 必为 0，却被 `frame-snr-survey.md:322` 当对拍证据引用；`:89-94` `img+C−(bkg+C) vs img−bkg` 是浮点结合律，测的不是 photutils；`:117-119` "天光 σ↑ ⇒ err↑" 是把 `B` 直接放进 `pixel_variance_e2` 的同义改写 ⇒ 循环自证；`:64-74` photutils `center` 法 vs 自算 `sqrt(Σσ²)` 才是**唯一真外部对拍**；`:47-49`/`:170-171` 库不可用时记 `UNAVAILABLE` 但 `:188 return 0` ⇒ 归档 `external_crosscheck.json` 实测 photutils/sep **双双 UNAVAILABLE**、退出码仍 0 | **须修** |
| 17 | `实验/absolute-snr/code/reverse_verify/f_instr/exp0_scene_and_noise.py:1-187` | 全文 | `:5`/`:92` **硬编码** `"hubble_available": False` + `"hubble_note": "全仓检索未发现 HST/哈勃帧"` 并写进产物 JSON —— **第四次**在同一单元里做这个假断言；`:24` `NORM` 指向 `run/RELEASE-02/L4-rebuild/norm`，而 `ls run/RELEASE-02` 只有 `FIX-A fix-p2b weight-chain` ⇒ `:83-89` 直接 `return 2` ⇒ **下游 `f_instr/exp2/exp3/exp4/exp5` 全部被阻断**（`exp3_seeing_null.py:44` `np.load(scene.npz)` 会崩） | **须修** |
| 18 | `实验/absolute-snr/results/EXP03_TABLES.md:1-177` | 全文 | 表 A1/A2/A3/A4/B1/C1/C2 数值与 `exp03_e*.json` **逐值吻合**（子代理 948b9228 逐格复核，我抽查 A1/A3/B1/D1 一致）；`:170-171`、`:173` 三条 PASS 的"关键量"列为**空**，无法从 md 判断被比较的两量（登记为待核） | **通过** |
| 19 | `实验/absolute-snr/code/audit/route2/exp08_mask_radius.py:1-166` | 全文 | `:17` docstring 承诺「NEGATIVE CONTROL 1: fixed r=2px, F_max=1e6 ⇒ **must exceed 2% (red)**」，代码 `:85-163` **只记录 `rel_bias`，无任何 2% 比较、无 `pass` 字段、无退出码**；归档实测 `negative_control_fixed2_Fmax1e6.rel_bias = 0.00421`（**0.42%，未红**），`:149-153` 又补了第二个更密的臂才 8.01% 变红 —— **加臂直到绿，注释自陈"must actually trip"，报告未披露**；`:94-95` `r_local_monotone_in_F/_in_FWHM` 是用自己检验自己；`:63` `sigma_bg_guess` 参数接收后从不使用（签名制造"有外部参照"的假象） | **须修** |
| 20 | `实验/absolute-snr/code/exp04/e9_clip.py:1-157` | 全文 | `:76`/`:110` 缺数据 `raise SystemExit(... fail-closed)` —— 好；`:5`「独立审稿 B-1 指出的缺口」是历史叙事（AGENTS §5 禁）；`:31-32` `import driver`/`e1_analytic` 依赖 exp04 本地副本（exp03/04/05/06 各有一份同名 `e1_analytic.py`，靠 sys.path 顺序决定，本片实测解析到 `exp04/` 那份，可用但脆弱）；`:59` `within_ctrl_range` 对 `*_clip` 算子由 `np.clip` 构造保证 ⇒ 恒绿 | **建议** |
| 21 | `实验/absolute-snr/code/reverse_verify/snr_design/exp1_weight_penalty.py:1-147` | 全文 | **全文件无 `assert`、无 pass/fail、退出码恒 0**；README 声明的「解析式与 MC 必须 5 位小数一致」在代码里**不存在**；`:96-103` 共模 ratio ≡ 1 是代数恒等（docstring `:14` 自陈"恒等"，README `:25` 却写成门"必须恰为 1.0"）；`:111-112` `var_bad` 里 `sig**2*0.0 + 1.0` ≡ 1 ⇒ "unit weight" 负例**逐位等于等权负例**，是被设计成两个不同对照却实际相同的一对；`:69-72` `ratio_naive_over_exact` 除的是**解析** `var_formula` 而非真正跑出来的 `var_exact_mc` ⇒ 文档 `snr-propagation-design.md:889` 标注的「**(MC 1.6667)**」**不成立** | **须修** |
| 22 | `实验/absolute-snr/code/exp06/e5_scope.py:1-138` | 全文 | 纯测量扫描（Q1/Q2/Q3），无门、无 `pass`、`:134 return 0`；E 口径（`weight_efficiency_loss`，尺度不变）正确 | **建议** |
| 23 | `实验/absolute-snr/code/audit/supplement_control_variance/production_chain_control_variance.py:1-129` | 全文 | `:2` 自陈"逐句复刻 `sampler.cpp:837-878`" —— **是 Python 手抄，不链接不执行生产**（子代理 948b9228 逐句比对，抄写本身忠实，此处确认）；`:102` `mean_cvar_over_var_y` 只是打印比率，**无门**；`:98` `clip_frac_any = (nks < R*0 + n).mean()` —— `R*0+n` ≡ `n`，语义是"**未**裁剪比例"，键名却是 `clip_frac`（被裁比例），**键名与语义相反**，易被误引 | **须修** |
| 24 | `实验/absolute-snr/code/reverse_verify/f_instr/exp3_seeing_null.py:1-127` | 全文 | `:27-28` 判据先写死、绿 0.010 / 红 0.200 双阈值、`:105-108` `red_green_check` 同时判绿判红 —— **本片第二好的判据设计**；但 `:44` `np.load(scene.npz)` 依赖 17 号文件，而 17 号文件在当前仓**跑不出来** ⇒ 不可复现；`:60` 把真值 FWHM 喂给 PSF 拟合器（`:76-77`），绿例由构造保证（弱结构对称） | **须修**（可复现性） |
| 25 | `实验/absolute-snr/code/reverse_verify/f_instr/exp2_aperture_dependence.py:1-119` | 全文 | 纯曲线测量、无门；`:39-40` `robust_loc_scale(...) if False else ...` 是死分支；同样依赖 17 号的 `scene.npz` | **建议** |
| 26 | `实验/absolute-snr/code/audit/route2/exp11_idw_reconstruction.py:1-107` | 全文 | `:89-95` 常数场负例：IDW 是归一化凸组合 `Σwv/Σw`，**任何** power/K 错误都会在常场上原样复现常场 ⇒ 恒绿（归档 1.42e-14 是浮点残差，数学值恰 0）；`:97-100` node 重合恢复：`:46-47` 的守卫 `if any(d<1e-10): return v[d<1e-10].mean()` **直接把答案返回** ⇒ 恒绿（归档恰 0.0）；全文件无门 | **须修** |
| 27 | `实验/absolute-snr/code/audit/route2/exp06_doublecount_bias.py:1-104` | 全文 | `:61` 用 `sqrt((1+b)²−1)`（`bias_closed` 的**代数逆**）从登记值反推 `x` 再验回 `b` ⇒ **往返自证型恒绿**（docstring `:19-21` 诚实自陈"reverse-engineered"）；`:92-95` 负控分子分母是**同一个 float 表达式** ⇒ 恒 0；`:83-84` `bias_pred_over_true` 代数恒等于 `bias_closed(x)`、`rel_se_of_mc_scatter` 把 `std` 约掉恒为 0.005 ⇒ 两条都恒绿；全文件无门。`REPORT_paper.md:75` 称该文件验证了"登记值与闭式之差 +1.91×10⁻⁶" —— **该文件从未做这个比较** | **须修** |
| 28 | `实验/absolute-snr/code/audit/route1/exp11_mask_radius_bias.py:1-98` | 全文 | `:69` `frame(F_e, SEED + 31*s + int(r_mask))` —— **掩膜半径进了随机种子** ⇒ 半径效应与帧间 MC 涨落混淆；`:21`/`:22` `SIG_TRUE` 连续两次赋值、第一次带错注释 `(sky+?)/g^2`；无门；`:88-92` 无源负例的 `difference` 实测 0.000216（≈0），**这一条反而是有效的**（它只断言"差≈MC 误差"） | **须修** |
| 29 | `实验/absolute-snr/code/audit/route2/exp01_mag_scale_identities.py:1-89` | 全文 | **本片恒真门密度最高的单文件（89 行 5 处）**：`:35` 往返自证（`K_MAG=2.5`、`INV=−0.4` 互倒数 ⇒ 两边都错成 2.0/−0.5 往返仍精确）；`:37` `log10(F/F)` 负控实测恰 `0.0`；`:40-41` Pogson 定义复述；`:67-68`/`:73` `w_cp=(F_ref/σ)²/F_ref²` vs `w_identity=1/σ²`，**代码自己的注释 `:66` 就写着 "exact identity"**；`:81-82` m_ref 扫描比值由构造必然；无门、无退出码 | **须修** |
| 30 | `实验/absolute-snr/code/redteam/rt_sigma_hat_applicability.py:1-88` | 全文 | `:7-13` **主动、逐字声明**置地板分支是"反例夹具、不是现行行为"，并给出正确行号 `:108/:129/:139/:149-152` —— 这是本片最诚实的一处，也正是它证明 `REPORT_paper.md:181` 错的依据；`:21`「输出只到 stdout，**不写 results/**」⇒ `REPORT_paper.md:181` 引用的 6 个数（0.4898…0.0011）与 `2.44–3.66` **仓内无任何留存**，不可复核；`:3-4` 自陈"不调用生产二进制、不编译…逐行重写" | **须修**（落产物） |
| 31 | `实验/absolute-snr/code/reverse_verify/snr_design/README.md:1-60` | 全文 | `:23-29` 判据先写表是好实践；`:25` 把"共模 ratio 必须恰为 1.0"写成门（同 21，恒绿）；`:28` 判据列仍写「nugget 必须物理（`1e-10` 会让预测方差恒 0）」，与同文件 `:53-54` 的例外脚注、也与 `snr-propagation-design.md:1442-1443` 的订正（"EXP-4 的 `1e-10` 是**正确的**"）**未同步**；`:44-46` 输入数据路径 `run/RELEASE-02/L4-rebuild/…` **当前不存在**；`:15`「复核分片（4 脚本）」与 `:33-40` 的 6 条清单不符 | **须修** |
| 32 | `实验/absolute-snr/code/reverse_verify/frame_snr/README.md:1-56` | 全文 | `:40-41`「红线测试**必须能红能绿**：绿例与红例用**同一个** `monotone_criterion()` 判」—— **代码不成立**：绿例用 `monotone_criterion`（判严格递减，`:77`），红例用 `strictly_increasing`（判严格递增，`:81,:375`），`P8_same_criterion_result`(`:373`) 被记录但**不参与 `pass`**(`:377`)；`:53` 第四次声称"全仓 `find -iname '*hst*'` 命中 0" | **须修** |
| 33 | `实验/absolute-snr/code/exp02/run_all.sh:1-53` | 全文 | `:5` `set -u -o pipefail`（无 `-e`）但 `:18,25` 记 `rc`、`:48-53` `exit "$rc"` ⇒ **正确**；`:42-43` 专设「门自审」步 | **通过**（样板） |
| 34 | `实验/absolute-snr/code/exp04/rss_guard.py:1-46` | 全文 | `:38` 超限 `return 3`；`:45` `return p.returncode` ⇒ **正确传播退出码**，是本片唯一让红灯能逃到上层的包装器 | **通过**（样板） |
| 35 | `实验/absolute-snr/code/prod_snr_driver.cpp:1-45` | 全文 | **本片唯一真正链接生产代码的构件**（`#include "snr_estimator.h"`，由 `build_prod_driver.sh` 编 `snr_science.cpp`）；但 `:29-32` 生产返回 `rc` 只被 `printf`，`:44` **无条件 `return 0`** ⇒ 「生产明确拒绝」与「生产算错」在调用方眼里都是成功（与同单元 `run_redlines_physical.py:412` 的正确写法相反） | **须修** |
| 36 | `实验/absolute-snr/code/run_all.sh:1-34` | 全文 | `:4` `set -euo pipefail` + 每步 `| tee` ⇒ 只捕获**退出码**；而本片多数实验脚本（09/11/13/14/22/24/25/30…）**退出码恒 0**，`pipefail` 在这里是装饰；`:19` 的 `| grep -v Warning` 会吞掉 stderr 里的异常文本；`:32-33` 有一道真 ctest（合规，但本轮不跑） | **建议** |
| 37 | `实验/absolute-snr/code/b7_run.sh:1-20` | 全文 | `:4 set -euo pipefail` 语法正确，但 `:10` 调 `b7_build_driver.sh`（见 S2，`:11` 指向不存在的 `lib/algorithms/integration/v6`）；全仓 grep `b7_run.sh` **0 命中** ⇒ §4.10 四臂判别的唯一复现入口**既无人调用、又编不过** | **须修** |
| 38 | `实验/absolute-snr/code/exp11_build_driver.sh:1-15` | 全文 | `:5` `set -euo pipefail`；`:10-14` 的 `-I` 路径**正确**，被编译源 `#include "astrocs/..."` **错误** ⇒ 编译必失败（同 S2） | **须修** |

---

## 4 发现清单

> **计数口径**：下表的"门实例" = 我在本片 38 份成员文件中**逐条读到的、写在代码或文档里、以 `pass`/布尔/阈值/表格 verdict 形式存在的判据表达式**。"去重门" = 合并同源重复项（如 P9⊂P8b、`red_b`≡`red_a`、R10/R11 的 5 处）。"整改分母" = 38 份成员文件。

### 4.1 门实例统计（口径写明）

| 层 | 数 | 构成 |
|---|---|---|
| **门实例（本片成员文件内）** | **约 46** | 恒绿 31 + 恒红/静默绿 4 + 可红可绿 11 |
| **去重门** | **约 40** | 合并 `P9 ⊂ P8b`（1）、`RED_B ≡ RED_A`（1）、`exp01_mag` 的 5 处同源往返（2）、`exp06_doublecount` 的 3 处同源自证（1）、`frame_snr` 4 处同源平移不变（1） |
| **整改分母** | **38 份成员文件** | 其中 10 份无可执行判据（纯测量/归档/驱动/入口） |

**恒绿门 31 处（按型）**：
- **代数恒等式型 14 处**：`run_redlines_physical.py:264`(P8b)、`:338`(P9)、`:215-219`(P8 渐近自比)、`:634`(P14a)、`:306`(P8c-ii)；`crosscheck_photutils.py:146-147`、`:89-94`、`:117-119`；`exp1_weight_penalty.py:70-72`、`:96-103`、`:111-112`；`exp06_doublecount_bias.py:92-95`、`:83-84`；`b4_integration.py:419`
- **结构对称型 8 处**：`b4_integration.py:371`、`:406`、`:391`；`exp01_mag_scale_identities.py:37`、`:40-41`、`:81-82`；`exp08_mask_radius.py:94-95`；`exp4_scatter_sp0.py:194-195`
- **往返自证 / 逆函数造参照 6 处**：`exp01_mag_scale_identities.py:35`、`:67-68/73`；`exp06_doublecount_bias.py:61-63`；`exp11_idw_reconstruction.py:89-95`、`:97-100`；`operators.py:123-129`(S6 的两个 Python 副本)
- **构造性自证 3 处**：`exp4_scatter_sp0.py:202-203`/`:224-225`（扣除解析已知项后跑原阈值）；`operators.py:476-479`（局部均值扣除 ⇒ 常数场构造性精确）

**恒红 / 静默绿 4 处**：`run_redlines_physical.py:9-10` 声明的 5% 门不存在于 `:229` 的 `pass`（归档实测 11.4%/15.7% ⇒ 补上即两点常红）；`exp11_mask_radius_bias.py:69` seed 依赖被比较量；`exp04/run_all.sh:6` 吞全部步骤红灯；`prod_snr_driver.cpp:44` 吞生产返回码。

**可红可绿（真判别力）11 处**：`b4_integration.py:373-378/380-404/420-421`；`b3_domain_map.py:182-192,251-252,323,327`；`exp04/exp04_common.py:199-212` + `sci_b_common.py:373-386`；`exp4_scatter_sp0.py:235-245`(P4/P5/P6)；`run_redlines_physical.py:500,508,457`；`exp02/e2_hst_scan.py:254`；`exp05/e1_analytic.py:156-158`；`f_instr/exp3_seeing_null.py:105-108`；`exp06/e3_real.py:139-156`；`rss_guard.py:45`；`exp02/run_all.sh:48-53`。

### 4.2 阻断（**0 条**）

本片未发现"必须先裁决才能继续"的阻断项。最重的三条已升格为**须修**并在 §2 给出理由。

### 4.3 须修（11 条）

| # | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **S1** | `REPORT_paper.md:181` | 把已退役缺陷 + 不可能发生的 2×10¹⁰ 倍高估写成现行生产开放缺陷，4 个行号全错 | 按 `rt_sigma_hat_applicability.py:7-13` 与 `star_detector.cpp:139/149-152` 改写为"已退役路径的量级；现行生产 fail-closed，不存在该高估"；行号改 `:108`/`:139`/`:149-152` | `star_detector.cpp:107`=for 头、`:108`=地板、`:122`=非有限守卫、`:139`=`if(!(*sigma>0.0)) return false;`；`rt_sigma_hat_applicability.py:7-13` |
| **S2** | `exp11_recon_driver.cpp:29,39-42`；`b7_build_driver.sh:11,22`；`exp11_build_driver.sh:10-14` | 驱动引 `astrocs/` 命名空间与 `lib/algorithms/integration/v6/` 路径，HEAD 下**编译必失败**；全篇唯一 `[生产证据]` 与 §4.10 唯一复现入口均断 | include/命名空间改 `acsd/`；`v6` 改 `phase2_integrate`；重跑并重新归档；在此之前 §4.9 的 `[生产证据]` 标注必须降级 | `ls lib/algorithms/integration/`（无 `v6`）；`weight_chain.h:75-77`；`ls .../include/`（只有 `acsd/`） |
| **S3** | `exp04/run_all.sh:6` + 11 处 `\|\| echo "WARN"` | 缺 `-e`，全步骤失败被吞，仍打印 "ALL DONE"；`selftest_operators.py` 的红灯逃不出去 | 删 `|| echo`；改 `set -euo pipefail` 或用 `exp02/run_all.sh:18,25,48-53` 的 `FAILED`/`exit` 模式 | `exp04/run_all.sh:6,23-24,54`；对照 `exp02/run_all.sh:48-53` |
| **S4** | 全片 17 个**失效文档路径**（8/38 份成员引用） | `docs/detail/registry/astrocs.phase1.noise-snr.md`（15 处）、`astrocs.phase2.{upm-fit,reject,integrate}.md`、`astrocs.phase1.calibration.md`、`docs/ASTROCS_DESIGN.md`、`docs/design/UNIFIED_MODEL.md`、`ACCEPTANCE_SPEC.md`、`docs/plugins/algorithms_phase1/07_noise_snr.md`、`docs/frame-snr-canon.md`（6 份成员引用，真路径 `实验/absolute-snr/docs/frame-snr-canon.md`）、`docs/DISPUTES.md` —— **全部不存在** | 全仓改名 `astrocs→acsd` 时漏改本单元；须全量替换并 grep 复核 | 成员逐份：`snr-propagation-design.md`、`sci_b_common.py`、`REPORT_paper.md`、`frame-snr-survey.md`、`run_redlines_physical.py`、`exp4_scatter_sp0.py`、`rt_sigma_hat_applicability.py`、`frame_snr/README.md`；`ls docs/detail/registry/` 只有 `acsd.*` |
| **S5** | `frame-snr-survey.md:470-471`；`frame_snr/README.md:53`；`run_redlines_physical.py:601-602`；`f_instr/exp0_scene_and_noise.py:5,92` | 四份成员断言「全仓 `find -iname '*hst*'` 命中 0」/「本仓无哈勃数据」—— **为假**。实测：`testdata/HST_M16/` 有 3 张真 HST WFC3 UVIS M16 FITS（8000×8400，`TELESCOP=HST`,`INSTRUME=WFC3`）；**git 跟踪树里有 5 个 `*hst*.py` + 5 个 `*hst*.json`**（含本片 09/14 号的 `e2_hst_scan.py`/`e2_hst.py`）；本片自己的 `sci_b_common.py:29` 定义 `HST_M16` 路径，`b3_domain_map.py:34`、`exp04_common.py:59`、`exp02/e2_hst_scan.py:53`、`exp05/e2_hst.py:49` **全部消费它**；`实验/shared/synthetic/scenes/hst_m16_realbase.json`（**已跟踪**）自述"负责人上传的 3 帧哈勃 M16 作底" | 改为"无**git 跟踪的**哈勃像素数据（`testdata/*` 被 `.gitignore:41` 排除）；工作树内 `testdata/HST_M16/` 有 3 张真 HST 帧，本单元 4 个实验臂已在用"。**注意**：因 `testdata/*` 被忽略，严格的仓级结论是"跟踪树无哈勃像素"，但"find 命中 0"在任何读法下都假 | `find . -iname '*hst*'`（20+ 命中）；`git ls-files \| grep -i hst`（20 命中）；`ls testdata/HST_M16/`；`hst_m16_realbase.json` 的 `description` |
| **S6** | `run/RELEASE-02/L4-rebuild/norm/…`（设计文档 20+ 处、`snr_design/README.md:44-46`、`f_instr/exp0_scene_and_noise.py:24`） | 真实数据路径**当前不存在**（`ls run/RELEASE-02` 只有 `FIX-A fix-p2b weight-chain`；`.gitignore:17` 排除 `run/*`）⇒ EXP-2/EXP-3/`f_instr` 全链在本仓**不可复跑**；`f_instr/exp0` `:83-89` `return 2`，下游 `exp2/exp3` 的 `np.load(scene.npz)` 会崩 | 在实验单元 README 里显式登记"本仓不可复跑 + 复现前置条件"，或把数据指针改为可配置的 `P2_NORM_DIR`（17 号已支持）并在其他臂同步 | `ls run/RELEASE-02`；`.gitignore:17`；`exp0_scene_and_noise.py:24,83-89` |
| **S7** | `exp04/operators.py:125`；`selftest_operators.py`（S6 门，片外）；`b4_integration.py`（3 条恒绿）；`exp01_mag_scale_identities.py`（5 处）；`exp06_doublecount_bias.py`（3 处）；`exp11_idw_reconstruction.py`（2 处）；`exp08_mask_radius.py:17,94-95`；`exp4_scatter_sp0.py:194-195`；`exp1_weight_penalty.py:70-72,96-103,111-112`；`exp2_sparse_snr_reconstruction.py`（无门） | **约 31 处恒绿门**，其中 0 处带 `[DEGENERATE]` 隔离标记或"不作证据"声明 | 采用本片已有样板：`b3_domain_map.py:311-315`、`exp04_common.py:21-23` 的处置 —— **逐处隔离为 `DEGENERATE_*` 记录位并写明"不作证据"**，或给出注入红灯证明可红 | 见 §4.1 逐处行号 |
| **S8** | `run_redlines_physical.py:9-10` vs `:229`；归档 `rel_diff_SNR_vs_physical_truth = [0.1142, 0.1570, 0.0585, 0.0189, 0.0160]` | docstring 声明判据含「与解析预测一致（**相对偏差 < 5%**）」，代码 `pass` 里没有这一项；该字段只写 JSON 从不判定 | 要么实现该门并**先解释** B=0/B=10 为何偏 11.4%/15.7%（大概率是 `analytic_prediction` 的 estimator_model 分支把读噪双计，见 `:98-99` 自陈"含读出噪声"），要么把声明从判据清单删除并注明"解析一致性仅记录不判定" | `run_redlines_physical.py:9-10,188,229`；`redlines_physical.json` P8 rows |
| **S9** | `REPORT_paper.md:97-98` vs `audit/route2/exp08_mask_radius.py:129-130` | 报告自陈"本单元使用 `r_i = max(1.5,0.75·FWHM)·(F_i/F_med)^0.1`"并据此登记"与正本形态不同"的**待裁决冲突**；但它引用的脚本用的是**正本式** `clip(r_local_moffat(...), max(1.5,0.75*fwhm), 60.0)` ⇒ **冲突登记的前提被自己的引文推翻**。同时"无星帧偏置≈0 的负例"在 exp08 中**不存在**（无 `n_stars=0` 臂），实存于 `exp11_mask_radius_bias.py:87-92` | 撤回该冲突登记，或改引真正用幂标度式的文件；负例改引 `exp11_mask_radius_bias.py` | `REPORT_paper.md:97-98`；`exp08_mask_radius.py:129-130,145-153`；`exp11_mask_radius_bias.py:87-92` |
| **S10** | `REPORT_paper.md:102` vs `docs/detail/registry/acsd.phase1.noise-snr.md:138-139`；`REPORT_paper.md:104` vs `results/EXP05_TABLES.md:13` | ① 报告写「地面视宁度受限域稀疏口径**全部**优于帧级标量」，权威要求「**稠密与稀疏互有胜负**…该域只给**定性**结论」⇒ 与正本冲突且越权下了定量结论；② 报告写「flat_with_struct 臂 **−45.70%**」，`EXP05_TABLES.md:13` 实为 **−44.12%**（−45.47% 是 `vary_with_struct`）⇒ **数值漂移，−45.70% 在任何数据源中都不存在** | ① 按权威改为定性表述，并补稀疏 vs 稠密对照；② 改 −44.12% 或删该括号注 | `acsd.phase1.noise-snr.md:138-139`；`REPORT_paper.md:102,104`；`EXP05_TABLES.md:13` |
| **S11** | `prod_snr_driver.cpp:32,44`；`exp06/e3_real.py:205-207`；`exp04/run_all.sh`;`reverse_verify/snr_design/audit/run_all_audit.sh` | ① 本片**唯一**链接生产代码的驱动对生产返回码不 fail-closed；② `e3_real` 缺帧静默跳过（与同片 `b3_domain_map.py:252` 纪律相反）；③④ 两个复现入口不解析任何 verdict | ① `return rc != 0 ? 1 : 0;`；② 与 b3 一致 `raise RuntimeError(... fail-closed，禁止静默少面)`；③④ 至少对门脚本取退出码做整链判定 | `prod_snr_driver.cpp:29-32,44` 对照 `run_redlines_physical.py:412`；`e3_real.py:205-207` 对照 `b3_domain_map.py:251-252` |

### 4.4 建议（10 条）

1. `sci_b_common.py:31-35` 的行区间 `snr_science.cpp:50-54` 少覆盖 `kTrimMeanToSigma`（真身在 `:56`）；`:33` `K_MOFFAT4_FWHM=1.230310` 的 +1.9e-6 手抄截断偏差应在其 docstring 注明影响面（所有 `moffat4_grid()` 消费者）。
2. `exp2_sparse_snr_reconstruction.py:246-261` 三处 `except Exception: pass` 与本片 `run_redlines_physical.py:426-430` 明令禁止的模式**逐字相同** ⇒ 同仓两处同时存在，说明无仓级 lint 覆盖。
3. `exp2_sparse_snr_reconstruction.py:299-303` 的 `guard` 算完即丢；`:269-270` 的列名 `weight_penalty_pct` 建议改为 `spatial_rmse_dex` + 另起一列并写明"仅当帧间独立时成立"。
4. `rt_sigma_hat_applicability.py:21`「不写 results/」⇒ `REPORT_paper.md:181` 引用的 6 个数仓内无留存，建议落 JSON。
5. `production_chain_control_variance.py:98` 键名 `clip_frac_any` 与语义（未裁剪比例）相反；`exp08_mask_radius.py:63` 的 `sigma_bg_guess` 死参数（制造"有外部参照"假象）。
6. `exp11_mask_radius_bias.py:69` 把 `r_mask` 从 seed 里摘掉改配对；`:21` 删除被 `:22` 覆盖的重复赋值。
7. `frame_snr/README.md:40-41` / `run_redlines_physical.py:76` 的"同一个 `monotone_criterion()`"应改为"**互补**谓词（递减 vs 递增）"，并把 `P8_same_criterion_result`(`:373`) 纳入 `pass` 或删除。
8. `b3_domain_map.py:33` 与 `exp04_common.py:46` 引的 `EXP-205 冻结门`在全仓找不到对应实验编号 ⇒ 悬空编号。
9. `snr-propagation-design.md:1296-1300` 五条脚本路径写 `experiments/snr_design/…`（目录不存在，真路径 `实验/absolute-snr/code/reverse_verify/snr_design/`），与同文 `:4`/`:1318` 自相矛盾；`:1233` 引 `reverse_verify/README.md §3`（文件不存在）；`:128` 校准式锚 `CALIBRATION.md:65-75` 实为「## 3a 坐标 frame / ## 4 输入有效域」，公式真身在 `:89-93`；`:1197`/`:1262` 母版方差锚 `:236` 实为一个空的 ```text 栅栏，真登记在 `:459`（同文 `:1033` 引对了 ⇒ 文档内部自相矛盾）。
10. `exp05/e2_hst.py`/`exp02/e2_hst_scan.py`/`exp04_common.py`/`b3_domain_map.py` 各自 `HST_M16` glob 重复定义四遍，建议收敛到 `sci_b_common.py`（见 S5 修完后）。

---

## 5 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **R1** | 假设"`exp8扫描判据能红`"。去找 `exp08_mask_radius.py` 里任何 `assert`/`pass`/`sys.exit`/与 2% 的比较 | 若不存在 ⇒ docstring `:17` 承诺的"NEGATIVE CONTROL 1 must exceed 2% (red)"是纸面的 | **推翻成功**。`:85-166` 全文无 `assert`/`pass`/退出码，只有 `:140` 记录 `rel_bias`。归档实测该负例 **0.42%**，未红；`:149-153` 补第二个更密的臂才 8.01% 红，注释 `:149` 自陈"must actually trip" |
| **R2** | 假设"臂 A 的 `RED_B` 是独立红例"。实读 `frame_snr_physical.py:194-204` 的 `red_a`/`red_b` 表达式并比对归档 JSON | 若 `raw_sum ≡ sub_sum + n_pix·bkg_adu` ⇒ `red_a ≡ red_b`，则"P10 三个独立红例"实为两个，且 `RED_B_power_ratio_raw` **根本没实现功率比估计量** | **推翻成功**。`:203`/`:204` 两式分子代数恒等；归档 JSON `RED_A` 与 `RED_B` 五值**逐位相同**（`23.80540385571095 / 31.489454445760543 / 95.69008485877919 / 333.7028899639413 / 1031.0257561327169`）。`frame-snr-survey.md:443` 的 [D2] 数值证据不成立 |
| **R3** | 假设"`exp01_mag_scale_identities.py` 的往返与零效应是有判别力的负例" | 若 `K_MAG=2.5`/`INV=−0.4` 互为倒数、且 `F/F` 恒 1 ⇒ 往返与零效应只检验这一行定义 | **推翻成功**。`:32-37` 实测 `roundtrip_max_abs_dm = 1.78e-15`、`zero_effect_dm = 0.0`（恰零）；`:67-73` 代码自己的注释写 "exact identity"，归档 `w_cp_identity_rel = [2.21e-16, 0.0, 0.0, 0.0]` |
| **R4** | 假设"共模 / 等杠杆 / 零 ZP 散差三个负例有判别力"。展开 `b4_integration.py:227-230,293,341-350` 的代数 | 若两边恒等于同一表达式 ⇒ 三条恒绿 | **推翻成功**。H1 `sqrt(Σ(F/σ_F)²)` 与 `F/sqrt(Σ1/σ_F²)` 同一；H4-equal `pred_naive_eq = h/K`、`pred_correct_eq = h/K`；H5-zero-spread `spread=0 ⇒ f_common ≡ f_ref ⇒ c_k ≡ 1`。docstring `:6`/`:14` **自己写明**"定义式恒等"/"（恒等）"，却仍列入 `gates` |
| **R5** | 假设"`REPORT_paper.md:181` 对现行生产的陈述为真"。去读 `star_detector.cpp:104-153` 与被引脚本 `rt_sigma_hat_applicability.py:1-22` | 若生产末端是 `return false` ⇒ 报告陈述错误 | **推翻成功**。`:139` = `if (!(*sigma > 0.0)) return false;`，`:123-138` 是一整段说明"旧写法 `*sigma = 1e-9` 是静默降级…**现按 NaN 与 0 同为不可用统一为显式失败**"的注释；`:149-152` `detect()` 以 `ErrorDomain::DATA` fail-closed。被引脚本 `:7-13` 逐字自陈相反 |
| **R6** | 假设"本仓无哈勃数据"。跑 `find . -iname '*hst*'`（排除 run/）、`git ls-files \| grep -i hst`、`ls testdata/HST_M16/` | 若有命中 ⇒ 四份成员的"命中 0"为假 | **推翻成功**。工作树 ≥20 命中；**跟踪树 20 命中**（含本片 09/14 号的 `e2_hst_scan.py`/`e2_hst.py`）；`testdata/HST_M16/` 有 3 张真 HST WFC3 UVIS FITS；`hst_m16_realbase.json`（已跟踪）自述"负责人上传的 3 帧哈勃 M16 作底"。**注**：`testdata/*` 被 `.gitignore:41` 排除 ⇒ 严格表述应为"跟踪树无哈勃像素数据"，但"find 命中 0"在任何读法下都假 |
| **R7** | 假设"删掉 `|| echo "WARN"` 后 exp04 会红"。读 `exp04/run_all.sh:6` 与全部 11 个步骤 | 若无 `-e` 且每步 `\|\|` ⇒ 全步骤失败被吞 | **推翻成功**（方向相反：现状是**不会**红）。`:6` = `set -uo pipefail`；`:23-24` 正是 `selftest_operators.py` 这一本片唯一 `return 0 if ok_all else 1` 的判据脚本 |
| **R8** | 假设"`prod_snr_driver.cpp` 是本片唯一链接生产代码的构件且能反映生产拒绝"。读 `:29-32,44` | 若生产返回 `rc` 只被 `printf` 且 `main return 0` ⇒ 拒绝与算错不可区分 | **推翻成功**（该构件自身 fail-open）。对照同片 `run_redlines_physical.py:412` 的正确写法 |

---

## 6 盲复算

**方法**：遮住前轮与子代理的全部判定，从原文独立重新取证，再与既有结论比对。

| 项 | 我的盲复算结论 | 与既有结论比对 | 判定 |
|---|---|---|---|
| `REPORT_paper.md:181` vs `star_detector.cpp` | 报告 4 个行号中 `:107`（for 头）、`:122`（非有限守卫）指错；`:139` 是 `return false`；现行生产 fail-closed，2×10¹⁰ 高估不存在 | 与子代理 83785379 的 P-1、22380445 的 B2、948b9228 的 §3.A **三份独立一致** | **一致** |
| 驱动编译 | `exp11_recon_driver.cpp:29` include `astrocs/`、`:39-42` namespace `astrocs::` vs 生产 `acsd/`；`b7_build_driver.sh:11` 指向不存在的 `v6/` ⇒ 编译必失败 | 与子代理 a0f964d6 阻断-1/2 一致（我自己 `ls`/`sed` 复核） | **一致** |
| `exp04/run_all.sh` 吞红灯 | `set -uo pipefail` 无 `-e`；11 步全 `\|\| echo`；末行无条件 "ALL DONE" | 与子代理 a0f964d6 阻断-3 一致 | **一致** |
| 恒绿门计数 | 我自己逐文件读出 **31 处**（14 代数恒等 + 8 结构对称 + 6 往返自证 + 3 构造性自证） | 子代理 83785379："≥30 处"；22380445："恒绿 ≥30 处" | **一致**（我的 31 在其下界内，无分歧） |
| 可红可绿门 | 我数出 **11 处** | 83785379：9 处；22380445：9 处 | **偏松 2 处**（我多算 2 条真判别力：`rss_guard.py:45` 的退出码传播、`exp06/e3_real.py` 的奇偶 hold-out 与二阶差分交叉验证）。**这两条对本片结论无影响**（只会让"有判别力的门"这一数字更好看，不改变整改分母） |
| "本仓无哈勃数据" | **假**。跟踪树 20 命中；`testdata/HST_M16/` 有 3 张真 HST FITS；`hst_m16_realbase.json`（已跟踪）自述 3 帧哈勃 M16 | 子代理 948b9228 未查此项；**本条为我独立发现** | **与既有结论不一致 —— 我判为新问题**（见 §8） |
| `REPORT_paper.md:104` 的 −45.70% | 漂移。`EXP05_TABLES.md:13` 为 **−44.12%**；−45.47% 是 `vary_with_struct` | 与 948b9228 的 T3 一致 | **一致** |
| `REPORT_paper.md:102` vs 权威 | 报告"稀疏口径**全部**优于帧级标量"；权威 `acsd.phase1.noise-snr.md:138-139` 要求"**互有胜负**…只给**定性**结论" ⇒ 越权定量 | 与 948b9228 的 §3.J（它判该处数值全真）**不一致** | **我更严**。理由：948b9228 只核了数值，没核权威约束。数值真 ≠ 表述合规：b3 的 JSON 确实显示 3 个面板 sparse < frame_median，但权威要求的对照是"**稀疏 vs 稠密**"，报告未给；且权威明令该域"只给定性结论" |
| 恒红门 | 本片**未发现**"两量逐位相同而判 `<=1e-4`"的经典恒红门；发现 4 处等价物（S8 的未实现 5% 门、seed 污染、两个吞出口） | 83785379 同样结论 | **一致** |
| 全部前置结论的方向 | — | 前轮 RR04/RR08/R3-T1 记录的"结构污染/门盲/报告层高估"方向**在本片继续成立**，且我在 S1/S3/S7 上给出了它们未落到 `file:line` 的具体落点 | **一致（我的证据更细）** |

**自查是否有"偏松"风险**：本片我把 3 条最重问题定为**须修**而非阻断。若总账口径要求阻断，我的三条最重（§2）在严重度上与之相当——请总账按"片级阻断/单元级须修"的层级自行归口，我按片内一致性给出**需修**。

---

## 7 子代理派发记录

**派发 5 个**（任务要求 3–5）。全部只读、零 git 写、零编译、零脚本执行。

| # | 子代理 | 做什么 | 它自己的覆盖率 | 我的复核结论 |
|---|---|---|---|---|
| A | `a0f964d6-…` | 选择性报告 / 自愈判据 / 桩依赖 / 孤儿文件 / 复现面可达性 | 自报 35/38 份 ≈78% | **采纳 4 条阻断的全部**：阻断-1（驱动编译断，含 commit `d796a1d7` 定位）、阻断-2（`b7_run.sh` 孤儿 + `v6` 路径）、阻断-3（`exp04/run_all.sh` 吞红灯）、阻断-4（`exp11:1010` 比复制品 vs 复制品）。我自己用 `sed`/`ls` 逐条复核了 1/2/3 的原文，**全部确认为真**。另采纳「`exp08` 首个负例 0.42% 没红、补臂才红」与「`exp11` 常数场 1.42e-14 / node 恰 0」两条实测（我自己读归档 JSON 复核，确认为真）。**否决/降级 1 条**：它把「`code/run_all.sh` 门盲」列为阻断——我降为**建议**，理由：`:32-33` 有一道真 ctest、`:4` 的 `set -euo pipefail` 在子脚本真返回非零时有效，问题在子脚本不返回，属**须修**而非阻断 |
| B | `83785379-…` | 恒真门三型 + 恒红门专项 | 自报 38/38 = 100% | **采纳其恒绿 ≥30 的量级**（我独立数得 31，一致）；采纳 `P1_pass`（`exp4_scatter_sp0.py:194-195`）为**结构性恒绿**——我自己读 `frame_snr_physical.py:99` 确认 `var_true` 由 `truth_variance_adu2` 给出、与 seed 无关，**证实**；采纳「exp04/run_all.sh 缺 `-e`」（与 A 重复）；采纳「`rt_sigma_hat_applicability.py` 与 `REPORT_paper.md:181` 正面冲突」（与 B/C 重复）——这三条我**独立用 `sed -n '104,153p' star_detector.cpp` 复核**。**否决 1 条**：它建议「统一到 A-P2-10 的 ulp 口径」——我**否决为过度**：本片被判为恒绿的 31 处里有 14 处是**代数恒等**（数学值恰 0），ulp 化对它们无意义；正确处置是隔离为 `DEGENERATE_*` 不作证据（S7） |
| C | `22380445-…` | 恒真门/恒红/桩依赖（第二路，交叉复核） | 自报 27/38 = 71%（行 67.4%） | **采纳**其「`exp04_scatter_sp0.py:258` 无条件 `return 0`」（我读原文 `:258` 确认为真）；**采纳**「`REPORT_paper.md:181`」；**采纳**「`code/run_all.sh` 门盲」；**采纳**其最有价值的新发现——**`run_redlines_physical.py:9-10` 声明的 5% 判据在 `:229` 的 `pass` 里不存在，而归档实测 B=0/B=10 偏差 11.4%/15.7% ⇒ 补上即两点常红**（我自己读归档 JSON 的 `rel_diff_SNR_vs_physical_truth` 复核，确认为真，升为 **S8**）。**否决 2 条**：①它把「`b3_domain_map.py:313` 的 DEGENERATE 门」列为恒绿——我**确认它已被作者正确隔离并标注"不作证据"**，故**不计入本片 31 处恒绿**（否则会重复计入一个已被治理的项）；②它称「`frame_snr_physical.py:203-204` 使 P10 三红例实为两」——我**采信但把它归到 02/04 号文件的证据链问题**（`frame_snr_physical.py` 不是本片成员），不计入本片 31 处 |
| D | `948b9228-…` | 伪引 + 文档↔代码冲突 | 自报 6/6 主审文本 100%，代码 11/19 | **采纳 5 条**：①`CALIBRATION.md:236` 是空的 ```text 栅栏、真登记在 `:459`（我 `sed -n '236p;459p'` 复核确认为真）；②`snr-propagation-design.md:128` 的校准式锚 `:65-75` 错、公式真身 `:89-93`（我 `sed -n '89,93p'` 复核）；③`REPORT_paper.md:104` 的 −45.70% 漂移（我 `grep` `EXP05_TABLES.md:13` 复核）；④`frame-snr-survey.md:443` 的 [D2] 数值证据不成立（我读 `frame_snr_physical.py:203-204` 复核）；⑤`exp06_doublecount_bias.py` 从未做「登记值 − 闭式」的比较，而 `REPORT_paper.md:75` 如此声称（我读全文 `:1-104` 复核）。**部分否决**：它判 `REPORT_paper.md:97-98` 的掩膜式冲突"前提被自身引文推翻"——我**采纳并升级为 S9**（这是我独立读 `exp08_mask_radius.py:129-130` 得到的同一结论）。**否决 1 条**：它把 `frame-snr-survey.md:11-12` 的 photutils 实跑陈述按"编造"定性——我**否决该定性**，降为"证据链断裂 + 现存产物反证"，理由：`run/*` 被 `.gitignore:17` 排除，产物不受版本控制，不能据现存 `external_crosscheck.json`（photutils/sep 双双 UNAVAILABLE）断言历史上从未跑成功 |
| E | `a0f964d6-…`（第二轮自陈） | 同 A 的收口消息 | — | 与 A 同一子代理，无新增独立证据；仅复述 |

**否决/降级合计 5 条**：A 的「run_all.sh 阻断→建议」降级 1；B 的「ulp 化建议」否决 1；C 的「DEGENERATE 门计入恒绿」否决 1、「P10 红例计入本片」改归 2；D 的「photutils 编造」定性否决 1。
**采纳合计 15 条头条**，全部经我独立 `read`/`sed`/`ls`/`python` 复核或独立发现。

---

## 8 自证段（可复跑命令）

全部**只读**。在 `/workspace/Astro CS Database` 下执行。

```bash
# ── 基线 ─────────────────────────────────────────────
git -c core.quotepath=false log --oneline -1          # 期望 f9650dd0

# ── §1 覆盖率：逐份行数 + 合计 ───────────────────────
python3 - <<'PY'
import re,io
L=io.open('run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml',encoding='utf-8').read().split('\n')
i=next(k for k,l in enumerate(L) if 'EXP-absolute-snr-001' in l and '片号' in l)
m=[]
for l in L[i:]:
    x=re.match(r'\s+- "(.*)"\s*$',l)
    if x: m.append(x.group(1))
    elif m: break
tot=sum(sum(1 for _ in open(f,encoding='utf-8')) for f in m)
print("members",len(m),"lines",tot)
PY

# ── S1：REPORT_paper.md:181 vs 生产码 ────────────────
sed -n '104,112p;118,124p;136,141p;147,153p' \
  lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp
sed -n '7,13p;44,47p' 实验/absolute-snr/code/redteam/rt_sigma_hat_applicability.py
grep -n '2×10¹⁰' 实验/absolute-snr/REPORT_paper.md

# ── S2：驱动编译（不编译，只看 include/namespace/路径）──
sed -n '29p;39,42p' 实验/absolute-snr/code/exp11_recon_driver.cpp
sed -n '75,77p' lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h
ls lib/algorithms/integration/            # 无 v6/
ls lib/algorithms/integration/phase2_integrate/include/   # 只有 acsd/
grep -n 'V6=' 实验/absolute-snr/code/b7_build_driver.sh

# ── S3：exp04/run_all.sh 吞红灯 ─────────────────────
sed -n '6p;23,24p;54p' 实验/absolute-snr/code/exp04/run_all.sh
# 对照正确样板：
sed -n '5p;25p;48,53p' 实验/absolute-snr/code/exp02/run_all.sh

# ── S4：17 个失效文档路径 ───────────────────────────
for f in docs/detail/registry/astrocs.phase1.noise-snr.md \
         docs/detail/registry/astrocs.phase2.upm-fit.md \
         docs/detail/registry/astrocs.phase2.reject.md \
         docs/detail/registry/astrocs.phase2.integrate.md \
         docs/detail/registry/astrocs.phase1.calibration.md \
         docs/ASTROCS_DESIGN.md docs/design/UNIFIED_MODEL.md \
         ACCEPTANCE_SPEC.md docs/plugins/algorithms_phase1/07_noise_snr.md \
         docs/frame-snr-canon.md docs/DISPUTES.md; do
  [ -e "$f" ] && echo "EXISTS  $f" || echo "MISSING $f"; done
ls docs/detail/registry/ | head -3          # 只有 acsd.*

# ── S5：「本仓无哈勃数据」为假 ───────────────────────
find . -path ./run -prune -o -iname '*hst*' -print | head -20
git -c core.quotepath=false ls-files | grep -ci hst
ls testdata/HST_M16/
python3 -c "from astropy.io import fits;import glob;[print(f,fits.getheader(f).get('TELESCOP'),fits.getheader(f).get('INSTRUME')) for f in sorted(glob.glob('testdata/HST_M16/*.fits'))]"
python3 -c "import json;print(json.load(open('实验/shared/synthetic/scenes/hst_m16_realbase.json'))['description'])"
grep -n 'HST_M16' 实验/absolute-snr/code/sci_b_common.py

# ── S6：真实数据不可复跑 ───────────────────────────
ls run/RELEASE-02/                     # 只有 FIX-A fix-p2b weight-chain
grep -n 'run/\*' .gitignore | head -2
sed -n '83,89p' 实验/absolute-snr/code/reverse_verify/f_instr/exp0_scene_and_noise.py

# ── S7：恒绿门实测读数 ──────────────────────────────
python3 -c "
import json
d=json.load(open('run/reverse_verify/frame_snr/redlines_physical.json'))
print('P8b max_rel_change_SNR =',d['P8b_negative_control_pure_offset']['max_rel_change_SNR'])
print('P9  =',d['P9_additive_invariance']['max_rel_change_F_hat'],
              d['P9_additive_invariance']['max_rel_change_sigma_F'])
print('P8 rel_diff_SNR_vs_physical_truth =',
      [round(r['rel_diff_SNR_vs_physical_truth'],4) for r in d['P8_physical_sky_monotonicity']['rows']])
c=d['P10_red_counterexamples_physical']['cases']
print('RED_A ==RED_B ?', c[0]['values']==c[1]['values']); print(c[0]['values'])
print('all_pass =',d['all_pass'],' P13 =',d['summary']['P13_real_data_base'])
"
python3 -c "
import json;d=json.load(open('实验/absolute-snr/code/audit/results/route2/exp08_mask_radius.json'))
print('neg1 rel_bias =',d['negative_control_fixed2_Fmax1e6']['rel_bias'])
print('neg2 rel_bias =',d['negative_control_fixed2_dense_256_200_Fmax1e6']['rel_bias'])
"
python3 -c "
import json;d=json.load(open('实验/absolute-snr/code/audit/results/route2/exp11_idw_reconstruction.json'))
print('const-field err =',d['negative_control_constant_field_max_abs_err'])
print('node err        =',d['node_coincidence_recovery_abs_err'])
"
python3 -c "
import json;d=json.load(open('实验/absolute-snr/code/audit/results/route2/exp01_mag_scale_identities.json'))
print('roundtrip =',d['roundtrip_max_abs_dm'],' zero_effect =',d['zero_effect_dm'])
"
grep -n 'pass\|assert\|sys.exit' 实验/absolute-snr/code/audit/route2/exp08_mask_radius.py  # 期望无命中
grep -n 'assert\|sys.exit\|raise' 实验/absolute-snr/code/reverse_verify/snr_design/exp1_weight_penalty.py  # 期望无命中

# ── S8：恒红等价物（声明的 5% 门不存在） ────────────
sed -n '9,10p;188p;229p' 实验/absolute-snr/code/reverse_verify/frame_snr/run_redlines_physical.py

# ── S9：掩膜式冲突的前提被自身引文推翻 ───────────────
sed -n '97,98p' 实验/absolute-snr/REPORT_paper.md
sed -n '129,130p' 实验/absolute-snr/code/audit/route2/exp08_mask_radius.py
sed -n '94,98p' docs/science/NOISE_MODEL.md

# ── S10：报告 vs 权威 / 数值漂移 ────────────────────
sed -n '138,139p' docs/detail/registry/acsd.phase1.noise-snr.md
grep -n '地面视宁度受限域稀疏口径' 实验/absolute-snr/REPORT_paper.md
grep -n 'flat_with_struct' 实验/absolute-snr/results/EXP05_TABLES.md | head -1
grep -o 'flat_with_struct 臂 −45.70%' 实验/absolute-snr/REPORT_paper.md

# ── S11：退出码 fail-open ───────────────────────────
sed -n '29,32p;44p' 实验/absolute-snr/code/prod_snr_driver.cpp
sed -n '205,207p' 实验/absolute-snr/code/exp06/e3_real.py
sed -n '251,252p' 实验/absolute-snr/code/b3_domain_map.py

# ── 建议 9：校准式与母版方差锚 ──────────────────────
sed -n '128p;1197p;1262p' 实验/absolute-snr/docs/snr-propagation-design.md
sed -n '236p;459p;89,93p' docs/science/CALIBRATION.md
grep -n 'experiments/snr_design' 实验/absolute-snr/docs/snr-propagation-design.md

# ── 纪律自证：本次审稿未做 git 写、未改仓内文件 ────────
git -c core.quotepath=false status --porcelain | head
```

---

## 9 本片相对前三轮是否发现**新**问题

**是，发现 1 条前三轮与本轮子代理均未记录的新问题（另 2 条为对已记录问题的 `file:line` 落地）**。

| # | 问题 | 新旧 | 说明 |
|---|---|---|---|
| **N1（全新）** | **「本仓无哈勃数据 / `find -iname '*hst*'` 命中 0」是假陈述**，且该假陈述支撑着**四条**强制口径：①GA-05「合成测试必须模拟真实物理实现」下"真实数据作底"的替代条款；②设计文档 §2.1.3 四条硬约束之第 2 条与 §13.1 第 5 条；③`f_instr/exp0_scene_and_noise.py:92` 把它**硬编码进产物 JSON**（`"hubble_available": false`）；④`frame-snr-survey.md:470-471`、`frame_snr/README.md:53`、`run_redlines_physical.py:601-602` 各自独立复述。实测：`testdata/HST_M16/` 有 3 张真 HST WFC3 UVIS M16 FITS（8000×8400）；**git 跟踪树里 20 个 `*hst*` 文件**（含本片 `exp02/e2_hst_scan.py`、`exp05/e2_hst.py`）；本片自己的 `sci_b_common.py:29` 定义 HST 路径并被 4 个实验臂消费；`实验/shared/synthetic/scenes/hst_m16_realbase.json`（**已跟踪**）自述"负责人上传的 3 帧哈勃 M16 作底"。**注意口径**：`testdata/*` 被 `.gitignore:41` 排除 ⇒ 严格表述是"跟踪树无哈勃像素数据"，故这是**假陈述 + 过期前提**，而非"数据一直在仓里没用" | **新** | 前三轮（RR04/RR08/R3-T1/R2）无此条；4 个子代理无一发现；我自己 `find` + `git ls-files` + FITS 头三重取证 |
| **N2（已有结论的 `file:line` 落地）** | `REPORT_paper.md:181` 把已退役缺陷写成现行生产开放缺陷 + 4 个错行号 | 半新 | 方向可能已被记（"文档与代码冲突"是通用检查项），但**这个具体位置、这个方向相反的事实错误、以及"报告据此提出的治理建议其实已落地"这三点，我未在前三轮交付件中找到**。子代理独立复核三次一致 |
| **N3（已有结论的 `file:line` 落地）** | `snr-propagation-design.md:1296-1300` 五条复跑脚本路径写 `experiments/snr_design/…`（目录不存在）；`:1233` 引不存在的 `reverse_verify/README.md §3`；`:128` 校准式锚 `:65-75` 错（真身 `:89-93`）；`:1197`/`:1262` 母版方差锚 `:236` 是空的代码栅栏（真登记 `:459`，而同文 `:1033` 引对了 ⇒ 文档内部自相矛盾） | 半新 | 「伪引/文档与文档冲突」是既有检查项，但这些具体锚号是新的 |

**未发现的新问题之外，最重要的"确认"**：本片有三处**正面样板**，前几轮的问题清单里看不到，建议总账在记"本单元缺陷密度"时一并保留，以免整改时误伤：
1. `b3_domain_map.py:311-315` —— 主动识别恒真门、写下数学原因、隔离成 `DEGENERATE_*`、写明"不作证据"；
2. `exp04_common.py:21-23` —— 显式禁用已识别的恒真判据；
3. `run_redlines_physical.py:426-431` —— 显式禁止"探针无判决 ⇒ 记为生产拒绝"，并把 `probe_inconclusive` 接到整体 `pass=False`。
   另有 `REPORT_paper.md:3-10` 的证据等级警示框与 `:165` 的判红臂同权报告，是「选择性报告」的正确反例。
   **整改建议**：把 1/2 的处置格式定为全单元强制格式（S7 的修法），把 3 的精神推广到 `run_all_audit.sh`。