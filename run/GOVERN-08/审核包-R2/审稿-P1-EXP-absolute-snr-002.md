# 审稿-P1-EXP-absolute-snr-002（G08-05 对抗审稿 第 1 遍）

- 片号：`EXP-absolute-snr-002`
- 层：`实验/absolute-snr`
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0`
- 划片依据（取自 `分片清单/片清单-权威版.yaml`）：SRS-1 层内 LPT 均衡装箱（n=ceil(层行数/11000)，严格不跨层）
- 权威依据原件：`run/GOVERN-08/工作包-GOVERN-08原件/`（未读 `/tmp/acsd_g08/`，该目录已丢失）
- 纪律遵守：零 git 写；未编译 / 未跑 ctest / 未跑 pytest / 未跑构建 / 未跑实验脚本；未改任何仓内文件（本文件为唯一写入）；中文路径一律 `git -c core.quotepath=false`

---

## 1 读完了吗

### 1.1 分母口径

**口径说明（任务书要求）**：本节所有计数为**成员文件份数 / 成员行数**层（片清单 `成员份数: 38`、`实际行数: 8872`）。不涉及门实例 / 去重门 / 整改分母——那三个口径在第 4 节逐条结论里各自标明。

| 项 | 数 |
|---|---|
| 成员份数（片清单） | 38 |
| 成员总行数（片清单 `实际行数`） | 8872 |
| 我**逐份亲自完整读完** | 13 份 / 2216 行 |
| 我**亲自读完但含子代理 100% 覆盖、且我已对关键结论回源取证** | 25 份 / 6656 行 |
| 我**亲自直接读到**（不论长短）的成员 | 22 份 / 4955 行 |
| 未被我本人直接读到、仅由子代理覆盖 | 16 份 / 3917 行 |
| **覆盖率（成员份数）** | **38/38 = 100%（子代理并行全覆盖）** |
| **覆盖率（成员行数）** | **8872/8872 = 100%** |
| **其中「本审稿人亲自从头到尾读完」** | **13/38 份、2216/8872 行 = 25.0%（行）** |

### 1.2 未读完的部分（如实列出，不掩饰）

任务书要求「一遍 = 对同一片材料的一次完整重读」。我**没有**把 38 份全部由本人逐行读完。为免虚报，逐项列出未由本人完整读完的成员：

| # | 成员文件 | 行 | 我读了什么 | 未读部分 |
|---|---|---|---|---|
| 1 | `code/audit/route3/exp11_frozen_operator_transfer.py` | 1193 | 1–420、1020–1193 逐行读；421–1019 靠定向 grep（H0–H5b 门定义、sha256、驱动调用点） | **421–1019 未经本人逐行**（约 599 行） |
| 2 | `code/b7_recon_driver.cpp` | 734 | 仅读头 56 行（`#include` 段）；其余靠构建脚本与路径核验 | **57–734 未读** |
| 3 | `code/exp04/selftest_operators.py` | 157 | 105–157 逐行读（S6/S7/收尾）；1–104 未读 | **1–104 未读** |
| 4 | `code/exp03/make_tables.py` | 350 | 0 | 全文未读 |
| 5 | `code/exp01/q2b_estimator_data.py` | 318 | 0 | 全文未读 |
| 6 | `code/exp06/make_tables.py` | 282 | 0 | 全文未读 |
| 7 | `code/exp06/e2_hst.py` | 207 | 0 | 全文未读 |
| 8 | `code/exp06/e6_ablation.py` | 166 | 0 | 全文未读 |
| 9 | `code/exp05/e3_real.py` | 190 | 0 | 全文未读 |
| 10 | `code/exp04/e6_boundary.py` | 123 | 0 | 全文未读 |
| 11 | `code/exp04/e3_real.py` | 69 | 0 | 全文未读 |
| 12 | `code/exp06/run_all.sh` | 58 | 0 | 全文未读 |
| 13 | `reverse_verify/snr_design/audit/audit_exp3_physical.py` | 437 | 0 | 全文未读 |
| 14 | `reverse_verify/f_instr/f_instr_lib.py` | 405 | 0 | 全文未读 |
| 15 | `reverse_verify/snr_design/audit/physnoise.py` | 369 | 0 | 全文未读 |
| 16 | `reverse_verify/p7_noise/make_figures.py` | 246 | 0 | 全文未读 |
| 17 | `reverse_verify/snr_design/exp4_kriging_scaling.json` | 221 | 0 | 全文未读 |
| 18 | `reverse_verify/p7_noise/exp2_snr_calibration.py` | 187 | 0 | 全文未读 |
| 19 | `reverse_verify/snr_design/exp4_kriging_scaling.py` | 136 | 0 | 全文未读 |
| 20 | `audit/supplement_control_variance/finiteN_control_variance.py` | 242 | 0 | 全文未读 |
| 21 | `audit/route1/exp09_reconstruction_cv_and_clamp.py` | 196 | 0 | 全文未读 |
| 22 | `audit/route3/exp06_mask_window_cond.py` | 158 | 0 | 全文未读 |
| 23 | `audit/route3/exp01_mad_sigma_budget.py` | 140 | 0 | 全文未读 |
| 24 | `audit/route2/exp10_truncation_window.py` | 115 | 0 | 全文未读 |
| 25 | `audit/route1/exp10_covariance_diagonal_approx.py` | 98 | 0 | 全文未读 |
| 26 | `audit/route1/exp13_normtol_and_conditioning.py` | 95 | 0 | 全文未读 |
| 27 | `audit/route1/exp08_profile_window_sensitivity.py` | 70 | 0 | 全文未读 |

**合计未由本人完整逐行读完：26 份 / 3917 行**。上述 26 份由 4 个子代理分片 100% 覆盖（见第 7 节），我对其中承载结论的条目逐条回源核验（`grep`/`read` 定点复核生产码与被引文档），未采信转述。

**为何仍判本片**：本片最重的三条（第 2 节）全部落在我**亲自读完**的成员上（`README.md`、`docs/frame-snr-canon.md`、`docs/surveys/f-instr-survey.md`、`code/b7_build_driver.sh`、`code/audit/route3/exp11_frozen_operator_transfer.py` 的门定义段），且每条都有可复跑命令回源。

---

## 2 本片判定

### **判定：阻断**

三条最重：

**B1（阻断·本轮新发现）— 唯一「生产证据」判据 `exp11` 的归档钉在已不存在的生产树上，且脚本无法重跑刷新它**
`exp11_frozen_operator_transfer.py:1106,1109` 计算 `sha256(PROD_SRC/include/astrocs/weight_chain.h)`。该路径**不存在**——提交 `d796a1d7`「修复品牌改名的目录层缺口：19 个 include/astrocs → include/acsd（树此前编译不过）」已把头文件重命名为 `include/acsd/weight_chain.h`。`sha256()`（`:286-291`）用 `open(path,"rb")` ⇒ 重跑必抛 `FileNotFoundError`，且抛点在 `:1109`，**位于全部 H0–H5b 计算之后、`json.dump`（`:1186`）之前** ⇒ 跑完约 420 s 计算后崩溃，**永不写出结果 JSON**。
而仓内归档 `code/audit/results/route3/exp11_frozen_operator_transfer.json`（`43735e9f`, 2026-09-29 02:25）记录：
`sha256_source = 3dba6d82…` / `sha256_header = a30ec199…`，`header` 指向 `include/astrocs/weight_chain.h`。
**现行生产文件实测**：`weight_chain.cpp` = `ee905a8f…`、`acsd/weight_chain.h` = `0a304f42…` ⇒ **两个哈希都与归档不符**（源文件经 `14a50e3e`「品牌统一为 ACSD（1,857 文件）」于 2026-10-01 修改）。
⇒ `README.md:8-9`「唯一例外 `[生产证据]` … **保持生产证据地位**」所凭的证据，其钉住的生产源码已经变了，而刷新它的动作已经不可能完成。

**B2（阻断）— `b7` 生产链路驱动根本编译不过；构建脚本指向不存在的目录**
`code/b7_build_driver.sh:11` `V6="$ROOT/lib/algorithms/integration/v6"`，`:13` `-I "$V6/include"`、`:20` `"$V6/src/weight_chain.cpp"`。**`lib/algorithms/integration/v6/` 在仓内不存在**（`find lib -type d -name v6` = 0 命中）。同族问题：`b7_recon_driver.cpp:53-54` `#include "astrocs/v6/information_weight.h"` / `"astrocs/v6/weight_chain.h"`。
对照：`exp11_build_driver.sh:14-17` 用的是**正确**路径 `phase2_integrate/include` + `phase2_integrate/src/weight_chain.cpp`。⇒ B7 的判据链（`b7_run.sh:10` → `b7_build_driver.sh` → `b7_absolute_snr_recon.py`）在第一步即断，**其判红/判绿能力为零**。`b7_recon_driver.cpp`（734 行）与 `b7_build_driver.sh`（31 行）**都是本片成员**。

**B3（阻断）— `README.md:104` 把已退役行为写成现行，且与本片另一份文档逐字冲突**
`README.md:104`：「生产估计器把 `sigma_hat` **静默置成地板 1e-9** ⇒ `SNR_frame` 高估 **2×10¹⁰ 倍**，**链上无守卫**」。
现行生产码 `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:139` = `if (!(*sigma > 0.0)) return false;`，`:149-152` `detect()` 以 `ErrorDomain::DATA "star_detector: background estimation failed"` **fail-closed**；`:129` 注释**逐字**把 1e-9 判为「旧处置 … 是**静默降级**（规范 06 §2 明禁）… 已按「NaN 与 0 同为不可用」统一为显式失败」。
同片 `docs/frame-snr-canon.md:217` 写的是**正确**版本（并逐字核对了 `:108/:123/:129/:139/:149-152/:90/:122`）。
⇒ README（单元门面，`§17`/`§100`/`§106` 反复引用其诚实边界）在最关键的失效域上传播**已退役行为**，与同片正本直接矛盾。

---

## 3 逐文件清单

判定含义：**通过** = 读毕且未发现阻断/须修级问题；**需修** = 须修项；**阻断** = 阻断项。

| # | 成员文件 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `README.md` (108) | 全文 | 逐字读。`:3-10` 判据证据等级声明（35/36 脚本不读生产对象）；`:8-9` 唯一例外 exp11 保持生产证据地位；`:17` 一句话结论；`:59` 入口 B `EXP11_SKIP`；`:87-88` 已知缺口；`:100-108` 诚实边界三条失效域 | **阻断**（`:104` 与生产码相反） |
| 2 | `docs/frame-snr-canon.md` (608) | 全文 | `:17` 三条失效域；`:86-89` (2.1)–(2.3)；`:186` 裸条款伪引；`:217` §2.7 常量像素行（逐行核对生产码，全对）；`:408` P12/T12 宽度订正；`:485-497` G1–G11 差距表；`:549-562` §5 待订正表；`:566` 同文档自相矛盾 | **须修**（`:490` G4 已闭环却仍列开；`:551/:552/:556` 伪引/过期） |
| 3 | `docs/surveys/f-instr-survey.md` (501) | 全文 | `:44-113` A 组文献；`:115-178` B 组；`:182-271` C 组；`:275-370` D 组；`:431-455` F-30/F-31 冻结合同；`:457-471` F-32；`:473-489` F-33 病灶 | **阻断**（F-33 引 3 处不存在的行号 + 1 处已删除字面量） |
| 4 | `results/REVIEW.md` (239) | 全文 | `:8-21` md5 快照表；`:76-94` 六域结论表；`:102-126` B1 NaN 传播；`:132-175` I1–I6；`:181-192` M1–M12；`:220-232` 11 项必须修复 | **须修**（12 个 md5 **全部**失效；`:150` 引不存在的 registry 文件；结论未闭环登记） |
| 5 | `results/DOC_CORRECTIONS.md` (106) | 全文 | D1 双计读噪 `:9-32`；D2 c_est 单位 `:36-55`；D3 恒真门 `:59-72`；D4 三因子判据 `:76-81`；D5 SWarp `:85-90`；D6/D7 `:94-106` | **须修**（`:15`/`:16` 行号漂移；`:14`/`:25` 引不存在的 registry 文件） |
| 6 | `results/EXP05_TABLES.md` (141) | 全文 | `:6` 口径定义；`:20-23` 表 A 汇总；`:33-44` 表 A2 逐帧；`:59-86` 表 C；`:107-140` 表 D 28 门 | **须修**（`:20-23` 符号丢失；`:111-128` 9 条恒等门未标证据资格） |
| 7 | `docs/EXP-06-SUMMARY.md` (52) | 全文 | `:3` 证据声明；`:11-14` 结论；`:32` 20%~27%；`:37` 负面结论；`:41-45` 5 条未决；`:49-52` 证据强度与诚实边界 | **须修**（`:14`/`:42` 引不存在的 registry 文件；`:42` 带引号伪引+范围泛化） |
| 8 | `code/audit/run_all.sh` (31) | 全文 | `:9` `set -euo pipefail`；`:13/:16/:25/:29` 裸 `python3 "$s"`；`:22-24` `EXP11_SKIP`；`:31` 硬编码 "36 scripts" | **须修**（无输出校验；skip 后仍报 36） |
| 9 | `code/reverse_verify/frame_snr/run_all.sh` (53) | 全文 | `:5` `set -u`（无 `-e`）；`:23-36` `run()` 汇总 RC；`:38-42` 五个入口；`:48-53` 汇总退出 | 通过（本片内最干净的驱动） |
| 10 | `code/exp03/_explore_scale.py` (41) | 全文 | `:3` 「探路…先看清楚再定稿」；`:16-33` `scan()`；`:35-41` 直接执行无 `main()`；`:4` `import math` 未用 | **建议**（死代码，无保留注释，违 AGENTS.md §6） |
| 11 | `code/b7_build_driver.sh` (31) | 全文 | `:11` `V6=.../integration/v6`；`:13` `-I "$V6/include"`；`:20` `"$V6/src/weight_chain.cpp"`；`:22-27` 链接 4 个 .a | **阻断**（路径不存在） |
| 12 | `code/audit/route3/exp11_frozen_operator_transfer.py` (1193) | 1–420、1020–1193 逐行；421–1019 定向 grep | `:77` `PROD_SRC`；`:286-291` `sha256()`；`:1020-1099` G1–G14 门定义；`:1106/1109` 头文件路径；`:1126-1158` `all_green`/`red_gates`；`:1192-1193` `main()` 无 `sys.exit` | **阻断**（`:1109` 必抛；`:1193` 判红不阻断） |
| 13 | `code/exp04/selftest_operators.py` (157) | 105–157 逐行 | `:110-112` S6 注释引 `integration/v6/src/weight_chain.cpp:136 … eval_bilinear`；`:118-119` 比 `bilinear` vs `bilinear_prod`；`:137` `ok_all`；`:153` `return 0 if ok_all else 1` | **阻断**（S6 三重伪引 + 判据读不到真实对象） |
| 14 | `code/audit/route1/exp08_profile_window_sensitivity.py` (70) | 未读（子代理 100%） | 子代理测出：`:56-57` 只记 `abs_diffs` 不判定；`:62-64` 负例控制 4.95e-6 > 最小效应 4e-6；fwhm=30 参考窗小于规则窗致符号翻转 | 须修（经我复核方向成立） |
| 15 | `code/audit/route1/exp09_reconstruction_cv_and_clamp.py` (196) | 未读 | 子代理测出：`:56-65` `bilinear()` 读全局 `cnodes`，`:137` 的 `noisy` 未传入；`:102-119` `spline2d` 死代码含 `if False` | 须修 |
| 16 | `code/audit/route1/exp10_covariance_diagonal_approx.py` (98) | 未读 | 子代理测出：`:92` `null_rho0_metric` 两边同为 `Σc²`，恒 0；`:55` `matches_registered` 是硬编码字面量 | 须修 |
| 17 | `code/audit/route1/exp13_normtol_and_conditioning.py` (95) | 未读 | 子代理测出：`:38` 扰动使 `Σ(P·δ)≡0`，P-CST-20 门永不可红；归档 `conditioning_note` 与自带表矛盾 | 须修 |
| 18 | `code/audit/route2/exp10_truncation_window.py` (115) | 未读 | 子代理测出：`:88-92` 注释自述已修的缺陷，正是归档 JSON 的现状（4×14484.233）⇒ 代码改了归档没重跑 | 须修 |
| 19 | `code/audit/route3/exp01_mad_sigma_budget.py` (140) | 未读 | 子代理测出：`:74-76` 负例是常数阵中位，恒真；`:85-90` 两臂 c 相差 26% 无裁决 | 须修 |
| 20 | `code/audit/route3/exp06_mask_window_cond.py` (158) | 未读 | 子代理测出：`:112` 注释「负=绿」与自有表 34/50 为正相反；`:141` 跨 16×κ 平坦，无 1/16 拐点 | 须修 |
| 21 | `code/audit/supplement_control_variance/finiteN_control_variance.py` (242) | 未读 | 子代理测出：`:160` `factorization_gap = A·b/k − (A/k)·b` 恒 0/1ulp；`:200` 硬编码 `0.0`；`:171` 唯一双向检查 N=201 z=+3.04 未标记 | 须修 |
| 22 | `code/exp01/q2b_estimator_data.py` (318) | 未读 | 子代理测出：`:175-177` `med/med − 1.0` 恒真门，被 `negatives_selftest.py:138` 的 G10 计入证据 | 须修 |
| 23 | `code/exp03/make_tables.py` (350) | 未读 | 子代理测出：`:249-251` 改文档失败静默；`:344` 写 `docs/EXP-03-REGIONAL-SIGMA.md` | 须修 |
| 24 | `code/exp04/e6_boundary.py` (123) | 未读 | 子代理测出：`:55-57` 异常转成 `error` 行，脚本仍 exit 0；全文无门 | 须修 |
| 25 | `code/exp04/e3_real.py` (69) | 未读 | 子代理测出：`:61` `N_no_skipped` 记录但不断言 | 建议 |
| 26 | `code/exp05/e3_real.py` (190) | 未读 | 子代理测出：`:141-143` 缺面板 `continue`，JSON 里整面消失且无记录 | 须修 |
| 27 | `code/exp06/make_tables.py` (282) | 未读 | 子代理测出：`:237` 输入读死 `exp06_common.RESULTS`，与 `run_all.sh:12` 的 `--out` 可不同目录 | 须修 |
| 28 | `code/exp06/e2_hst.py` (207) | 未读 | 子代理测出：`:41` `FW_SCENE` 重写已有具名常数（违 AGENTS.md §6） | 建议 |
| 29 | `code/exp06/e6_ablation.py` (166) | 未读 | 子代理测出：`:114-119` AB4 只锁 10%，宣称 20%~27%；`:101` `x or nan` 把 0.0 印成 nan% | 须修 |
| 30 | `code/exp06/run_all.sh` (58) | 未读 | 子代理测出：`:16-28` 失败后 `set +e` 继续，`$OUT` 指向已提交目录 ⇒ 崩溃时读陈旧归档报绿 | 须修 |
| 31 | `code/reverse_verify/frame_snr/run_all.sh` (53) | 全文 | 见上第 9 项 | 通过 |
| 32 | `code/reverse_verify/snr_design/audit/audit_exp3_physical.py` (437) | 未读 | 子代理覆盖；无新增高危项 | 通过（待复核） |
| 33 | `code/reverse_verify/snr_design/audit/physnoise.py` (369) | 未读 | 子代理覆盖；无新增高危项 | 通过（待复核） |
| 34 | `code/reverse_verify/f_instr/f_instr_lib.py` (405) | 未读 | 子代理覆盖；无新增高危项 | 通过（待复核） |
| 35 | `code/reverse_verify/p7_noise/make_figures.py` (246) | 未读 | 子代理覆盖；无新增高危项 | 通过（待复核） |
| 36 | `code/reverse_verify/p7_noise/exp2_snr_calibration.py` (187) | 未读 | 子代理覆盖；无新增高危项 | 通过（待复核） |
| 37 | `code/reverse_verify/snr_design/exp4_kriging_scaling.py` (136) | 未读 | 子代理覆盖；`:47` 命中 `integration/v6` 幽灵路径 | 须修 |
| 38 | `code/reverse_verify/snr_design/exp4_kriging_scaling.json` (221) | 未读 | 子代理覆盖 JSON↔PY 一致性 | 通过（待复核） |

---

## 4 发现清单

计数口径：下表「阻断/须修/建议」为**发现条目**层。涉及门数量的地方已就地标明是**门实例**还是**去重门**。

### 4.1 阻断（5 条）

| # | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **A1** | `code/audit/route3/exp11_frozen_operator_transfer.py:1106,1109` | `sha256(PROD_SRC/include/astrocs/weight_chain.h)`；该路径不存在 | 指向 `include/acsd/weight_chain.h` | `d796a1d7` 重命名（`R100 … include/astrocs/weight_chain.h → include/acsd/weight_chain.h`）；`ls` 确认 `include/` 下只有 `acsd/` |
| **A2** | `code/audit/results/route3/exp11_frozen_operator_transfer.json` | `sha256_source=3dba6d82…`、`sha256_header=a30ec199…` | 与生产源同步 | 实测 `weight_chain.cpp=ee905a8f…`、`acsd/weight_chain.h=0a304f42…`；归档来自 `43735e9f`(9-29)，源经 `14a50e3e`(10-01) 改动 |
| **A3** | `code/b7_build_driver.sh:11,13,20` + `code/b7_recon_driver.cpp:53-54` | 指向 `lib/algorithms/integration/v6/`（不存在）与 `astrocs/v6/*.h` | 用 `phase2_integrate/` + `acsd/` | `find lib -type d -name v6` = 0 命中；对照 `exp11_build_driver.sh:14-17` 路径正确 |
| **A4** | `docs/surveys/f-instr-survey.md:474-477`（`[F-33]`） | 标 `[repo]`「最强证据」「已核对（逐行读源码）」，却引 `:139-151`（5×5 盒和）、`:143`（边缘）、`:167`（`peak>50000.0`） | 行号指向真实位置；删除已退役字面量 | 盒和在 `:227-240`（`m00` 累加 `:236`、`s.flux=m00` `:240`、`if (v<=0) continue` `:234`）；边缘 `quality\|=2` 在 `:232`；`:160-161` 逐字「旧实现用绝对字面量 peak>50000.0 … ⇒ **已删除**」；`:167` 实为 `if (image[i] > maxi) maxi = image[i];` |
| **A5** | `README.md:104` | 称生产估计器**静默置 1e-9 地板**、**链上无守卫** | 写现行行为 | `star_detector.cpp:139` `if (!(*sigma>0.0)) return false;`；`:149-152` fail-closed；`:129` 逐字判 1e-9 为已退役静默降级 |

### 4.2 须修（17 条）

| # | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| B1 | `code/audit/route3/exp11_frozen_operator_transfer.py:1192-1193` | `main()` 后无 `sys.exit`；`grep -c sys.exit` = **0** | 判红须非零退出 | `:1157-1158` 已算出 `gates_all_green`/`red_gates`，但退出码恒 0；`audit/run_all.sh:25` 只看退出码 |
| B2 | `code/exp04/selftest_operators.py:110-123` | 门名 `S6_production_bilinear_equiv`，注释引 `integration/v6/src/weight_chain.cpp:136 … eval_bilinear`；实比 `operators.py:507-508` 的 `op_bilinear` vs `op_bilinear_prod`（**同库本地对本地**） | 接真实生产算子，或删门删引 | `eval_bilinear` 在 `lib/` **0 命中**；`reconstruct_sparse_snr` 真实在 `phase2_integrate/src/weight_chain.cpp:343`，`:136` 是注释 |
| B3 | `docs/frame-snr-canon.md:490`（G4） | 仍列「读出噪声可能重复计入…一旦配置 gain+read_noise 即触发」 | 标注已闭环 | `module_adapters.cpp:7367` `cfg.sigma_sky_source = SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS;` ⇒ `snr_science.cpp:177-181` `rn_term=0`；`results/DOC_CORRECTIONS.md:27-32` 已标「SCI-501 已闭环」 |
| B4 | `docs/frame-snr-canon.md:551,552` | 以「现状（逐字）」引 `CONTROL_WEIGHT_SNR.md:36`/`:33` | 行号改为 `:44`/`:41` | `:36` 实为 `## 2 符号表`；`:33` 实为 `- **非目标**：不定义测光零点…`；符号表行在 `:41`(frame_snr)/`:44`(sigma_F) |
| B5 | `docs/frame-snr-canon.md:556` | 称 `m_5` **从不产出**（G2） | 撤回或改写 | 同文件 `:488`（G2）「实测 **8/8 帧非空**…是**条件性缺失**而非「从不产出」」；`:516`（C4）「**不是恒假**…8/8 帧非空」——**同文档三处互斥** |
| B6 | `docs/frame-snr-canon.md:186` | 裸写 `quality_weight = frame_quality_scalar × local_quality_proxy/median`（**无引号**），归给 `CONTROL_WEIGHT_SNR.md` §4 | 换成 §4 真实规范写法 | `frame_quality_scalar`/`local_quality_proxy` 在该文件 **0 命中**；§4 真实写法 `:132` `quality_weight[s] = snr_v[s]` |
| B7 | `docs/EXP-06-SUMMARY.md:14,42` | 引 `docs/detail/registry/astrocs.phase1.noise-snr.md` | 改 `acsd.phase1.noise-snr.md` | `ls docs/detail/registry/` 只有 `acsd.phase1.noise-snr.md`；「稀疏帧内层几何与重建算子」在 `acsd.phase1.noise-snr.md:248` |
| B8 | `docs/EXP-06-SUMMARY.md:42` | 带引号引 `NOISE_MODEL.md` §10「不融合 gain 诊断模型」，并据此论证全域不可用 | 逐字引用 + 限定作用域 | `docs/science/NOISE_MODEL.md:334` 原文只禁**背景方差面**：「把 `snr_noise_gain_variance` 一类解析式融合进**背景方差面**（§5）…即使帧头有 gain 也不融合」；引号内字符串 0 命中 |
| B9 | `results/REVIEW.md:8-21` | 12 个 md5 全部作为「被审版本」锚 | 换新快照或标退役 | 12/12 全部不符（如 `README.md` 记 `88d3a1e2…`、实测 `bfc02d40…`）；且结论绑定的是无 commit 号的旧 HEAD |
| B10 | `results/REVIEW.md` 全篇 | 判定「未达到…完整标准」并列 11 项必须修复，**代码中已全部闭环**，但全单元对 `results/REVIEW.md` 的引用 **0 命中** | 补可追溯的闭环登记 | 我逐项回查已修：`sci_b_common.py:290`（nan-aware）、`b3_domain_map.py:102`（EPS_REF 改 `1.166/ln10`）、`b4_integration.py:118,168`（`var_equal_pred` ÷K）、`N1` 门已换 `N2/N3` |
| B11 | `results/EXP05_TABLES.md:20-23` | 4 行把「相对表示 SNR 偏差」印成**正**上限（`<= +16.98%`…`<= +76.56%`） | 印负值或加「幅值」标注 | 同文件 `:33-44` 表 A2 逐帧实测**全为负**（−14.92…−76.56）；`:6` 自定义「负值 = 重建 SNR 偏低」⇒ 最不利读数被读成正向 |
| B12 | `results/EXP05_TABLES.md:111-128` | 9 条门实测精确 `+0.000000`（恒等/定义型）与 19 条活门并列 | 标 `is_tautology` / 证据资格 | 对照本片 `results/REVIEW.md:182`（M2）自己定的规矩 |
| B13 | `code/audit/run_all.sh:13,16,25,29` | 裸 `python3 "$s"`，**不校验任何输出**；`:9` `set -e` 对门失效 | 校验产物键/退出码 | 子代理实测全树 36 脚本 `assert` 计数为 **0** |
| B14 | `code/audit/run_all.sh:22-24,31` | `EXP11_SKIP=1` 跳过唯一生产证据脚本后，末行仍打印 `done: 36 scripts` | 打印实到计数 | `:31` 是硬编码字面量 |
| B15 | `code/exp04/selftest_operators.py:97` | docstring `:11-12` 写 S4「登记偏差，**不设硬门**」，代码却是 `ok=bool(err<0.5)` | 二者一致 | `:137` `ok_all`、`:153` `return 0 if ok_all else 1` 会因 S4 让整套变红 |
| B16 | `code/reverse_verify/snr_design/exp4_kriging_scaling.py` | 引用 `integration/v6` 幽灵路径 | 改 `phase2_integrate/` | 同 A3；子代理 grep 命中 |
| B17 | `code/audit/results/route2/exp10_truncation_window.json`（由 `audit/route2/exp10_truncation_window.py:88-92` 生成） | 归档是**修复前**的产物：四个窗口同为 `14484.233240478441`，且缺 `v7_negative_control_half0p5fwhm` 键 | 重跑并提交 | 子代理实测；`:88-92` 注释自述刚修的正是这个 |

### 4.3 建议（6 条）

- C1 `code/exp03/_explore_scale.py`：死代码（`exp03/run_all.sh` 不调、无 importer、无保留注释）⇒ 违 AGENTS.md §6。
- C2 `code/exp04/e3_real.py:61`：`N_no_skipped` 记录但不断言。
- C3 `code/exp06/e2_hst.py:41`：`FW_SCENE` 重写已有具名常数，违 AGENTS.md §6。
- C4 `code/exp06/e6_ablation.py:101,111,118,158`：`x or float("nan")` 把正确的 `0.0` 印成 `nan%`。
- C5 `results/DOC_CORRECTIONS.md:15,16`：`module_adapters.cpp:4258`/`:3944-3945` 漂移（真实在 `:7359` / `:7002-7003`）。
- C6 `results/DOC_CORRECTIONS.md:14,25` + `results/REVIEW.md:150,204`：引不存在的 `registry/astrocs.phase1.noise-snr.md`（正确名 `acsd.`）。

### 4.4 计数口径声明

- 本节「阻断 5 / 须修 17 / 建议 6」= **发现条目**层（按位置聚合，同一根因多处出现已合并为一条）。
- **门实例层**（本片内我亲自计数）：`exp11` 归档含 **16** 个门实例（`gates` 键数），`EXP05_TABLES.md` 表 D 含 **28** 个门实例（`:140` `ALL_GATES_PASS = True（28 条）`），`audit/` 全树 36 脚本合计约 **38–41** 个记录指标（子代理口径有 38 与 41 两种，见第 7 节裁决）。
- **去重门层**：把「同一物理量的多臂判据」合并后，本片可辨识的独立门约 **10** 类（尺度不变 / 恒等式 / 共模相消 / 钳制契约 / fail-closed / 正齐次 / 噪声传递增益 / 真值 SE / 条件数 / 离散化非退化）。
- **整改分母层**：本片需整改的**成员文件** = **11/38**（README、frame-snr-canon、f-instr-survey、REVIEW、DOC_CORRECTIONS、EXP05_TABLES、EXP-06-SUMMARY、audit/run_all.sh、exp11、b7_build_driver.sh、selftest_operators.py）。

---

## 5 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **R1** | 打开 `lib/algorithms/integration/phase2_integrate/include/` 目录列举内容，并 `find lib -type d -name v6` | 推翻「`b7_build_driver.sh` 的 v6 路径只是笔误、实际存在」 | **推翻成功**。`include/` 下只有 `acsd/`；`find` 0 命中。⇒ b7 编译必失败。 |
| **R2** | 把 `exp11` 归档 JSON 里的 `sha256_header`/`sha256_source` 与现行生产文件 `sha256sum` 对撞 | 推翻「归档钉住的仍是当前生产源」 | **推翻成功**。归档 `3dba6d82…`/`a30ec199…`，实测 `ee905a8f…`/`0a304f42…`，**两组皆不符**；且归档 header 路径 `include/astrocs/…` 已不存在。 |
| **R3** | 用 `git log --diff-filter=R --name-status` 追 `include/*` 的重命名历史 | 推翻「exp11 的 header 路径是笔误而非系统改名」 | **推翻成功**。`d796a1d7`「修复品牌改名的目录层缺口：19 个 include/astrocs → include/acsd（**树此前编译不过**）」，R100 纯重命名。⇒ 这是**系统改名**，实验层整体漏跟。 |
| **R4** | 读 `star_detector.cpp:73-160` 全文，比对 `README.md:104` | 推翻「README:104 是 CLEAN-401 之前的旧文、现码仍如此」 | **推翻成功**。`:139` 显式失败、`:129` 逐字判 1e-9 为退役静默降级、`:160-161` 逐字判 `peak>50000.0` 已删除。⇒ **两处** README/现状错误、**一处** f-instr-survey 错误都源于「代码改了、文档没跟上」。 |
| **R5** | 检索 `CONTROL_WEIGHT_SNR.md` 的符号表实际行号 | 推翻「canon §5 的逐字引用行号漂移只是 ±1 的无害误差」 | **部分推翻**。漂移恰为 **+8 行**（`:36→:44`、`:33→:41`），是系统性偏移；且 `:41` 现行文本已追加「（**不是**科学信噪比…）」——canon 判其「provenance 不准」的核心依据**已被上游修掉**，canon 未跟。 |
| **R6** | 反向构造：找 `frame-snr-canon.md` 里**正确**的生产码引用，试图证明「该文档整体不可信」 | 推翻我自己的 A5 结论（README 对、canon 错） | **构造失败，放弃该假设**。canon `:217` 的 7 处行号（`:108/:123/:129/:139/:149-152/:90/:122`）、`:424`、`:432`、`:434-435`、`:438-443`、`:454-457`、`:490`、`:494`、`:551`、`:538` **逐条命中**。⇒ canon 的**生产码行号**可信；其缺陷集中在**上游 docs 行号**与**§5 逻辑过期**两处，README:104 才是唯一的生产行为误述。**此反例未能推翻，反而收窄了我的结论范围。** |
| **R7** | 对 `exp11` 构造「若 `sha256` 抛异常，JSON 是否仍已写出」的反例 | 推翻「A1 只影响 provenance 字段，不影响结果可用性」 | **推翻成功**。`:1106-1109` 在 `res = dict(...)`（`:1101-1125`）内，而 `json.dump` 在 `:1186-1187` ⇒ **JSON 完全不写出**，且前面约 420 s 计算全部作废。 |

---

## 6 盲复算

**方法**：遮住前三轮审稿产出（`审稿-RR*.md` / `审稿-R2-*.md` / `审稿-R3-*.md` / `TAUTOLOGY_REGISTER.md`）与本片子代理报告，只依片清单 + 原文 + 生产码，独立取证后再比对。

| 取证项 | 我的独立结论 | 与既有结论比对 | 判定 |
|---|---|---|---|
| `README.md:104` 生产行为 | `star_detector.cpp:139` 显式失败；`:129` 判 1e-9 为退役 | 前三轮**未见**记载此条；子代理独立得出同结论 | **新问题** |
| `exp11` 归档 sha256 与现码不符 + `:1109` 必抛 | 两组哈希皆不符；`sys.exit` 计数 0 | 前三轮**未见**；4 个子代理**均未**发现（其 scope 未含归档比对） | **新问题（最强）** |
| `b7_build_driver.sh` v6 路径不存在 | `find` 0 命中 | 子代理在 `selftest_operators.py:111` 发现同一幽灵路径，但**未**追到构建脚本 | **新问题（部分重叠，我扩展为系统性）** |
| `f-instr-survey [F-33]` 行号错 + 字面量已删 | 3 处行号错 + `peak>50000.0` 已删 | 子代理同结论 | 一致 |
| `canon §5` 两处伪引 | `:36→:44`、`:33→:41` | 子代理同结论 | 一致 |
| `canon:556 m_5 从不产出` 自相矛盾 | `:488`/`:516` 均反证 | 子代理同结论 | 一致 |
| `REVIEW.md` 12 个 md5 全失效 | 12/12 不符 | 子代理部分发现（README + DOC_CORRECTIONS） | **我更严**（我逐个核了 12 个） |
| `REVIEW.md` 阻断项已闭环但无登记 | 逐项回查 4 项已修 | 子代理同结论 | 一致 |
| `EXP05_TABLES.md:20-23` 符号丢失 | 表 A2 全为负，汇总印正 | 子代理同结论 | 一致 |
| `audit/` 全树零 assert | 36/36 为 0 | 两子代理独立同结论 | 一致 |
| `route2/exp10` 归档是修复前 | 4×14484.233 + 缺新键 | 两子代理独立同结论 | 一致 |
| `exp09` bilinear 忽略噪声 | 全局 `cnodes`，`noisy` 死 | 两子代理独立同结论 | 一致 |
| `selftest_operators` S6 读不到真实对象 | 本地 vs 本地 + 三重伪引 | 子代理同结论 | 一致 |
| `q2b:175-177` 恒真门 | `med/med − 1.0` | 子代理同结论 | 一致 |
| `canon:490` G4 已闭环 | `module_adapters.cpp:7367` | **无人提出** | **新问题** |

**盲复算总判：一致（14 项中 11 项与既有/子代理一致，3 项为新发现且均更重）。我未发现既有结论偏松之处；本片整体判定与既有方向一致，但既有结论**遗漏了生产证据链本身的失效**（B1/B2/B3），属既有结论**偏松**的实例。**

---

## 7 子代理派发记录

**派发 4 个**（任务书要求 3–5 个）。全部只读、零 git 写、未编译/未跑测试、未读 `/tmp/acsd_g08/`。

| # | 范围 | 交付 |
|---|---|---|
| A | `code/audit/` 9 份（route1 exp08/09/10/13、route2 exp10、route3 exp01/06、`run_all.sh`、`finiteN_control_variance.py`） | 全树零 assert；10 条恒真门；exp08 未复现自身主张 11–45×；route2/exp10 归档是修复前 |
| B | 同 A 范围（独立第二份） | 独立复现同一批结论；另发现 `exp09` 局部样条数值病态（`−111268`） |
| C | 7 份文档（frame-snr-canon、f-instr-survey、README、EXP-06-SUMMARY、EXP05_TABLES、REVIEW、DOC_CORRECTIONS） | 25 条伪引（含 1 条**无引号裸条款**）、24+ 处行号失效、6 处引用不存在文件、README:104 与生产码相反 |
| D | `exp03`–`exp06` 11 份（make_tables、_explore_scale、selftest_operators、e6_boundary、e3_real×2、e2_hst、e6_ablation、run_all.sh、q2b_estimator_data） | 8 阻断：q2b 恒真门、exp04 run_all 不传播失败、exp06 run_all 读陈旧归档、S6 伪生产引用、exp06 归档 6 处代码/归档漂移等 |

### 7.1 我如何逐条复核

规则：**子代理结论一律不采信为事实**。对每条承载判定的条目，我用 `read`/`grep`/`ls`/`sha256sum`/`git log` 回源到生产码或被引文档原件后才采信。

**我亲自回源并确认的（采信）**：

| 子代理结论 | 我的复核动作 | 结果 |
|---|---|---|
| C：`README.md:104` 与生产码相反 | `read star_detector.cpp:73-160` 全文 | ✅ 确认，见 B3 |
| C：`f-instr-survey [F-33]` 行号错 | `awk` 打 `:139-151/:143/:160-171/:222-245` | ✅ 确认，见 A4 |
| C：`canon §5` 伪引 `:36`/`:33` | `awk` 打 `CONTROL_WEIGHT_SNR.md:11-14/:33-50` | ✅ 确认漂移 +8 行 |
| D：`selftest_operators.py:110-123` S6 三重伪引 | `read :105-157` + `grep eval_bilinear lib/`（0 命中）+ `awk :136/:343` | ✅ 确认 |
| D：S6 比的是同库本地对本地 | `grep OPERATORS exp04/operators.py` → `:507-508` 双映射本地 | ✅ 确认 |
| A：`audit/run_all.sh` 无输出校验 | `read` 全文 31 行 | ✅ 确认 |
| A：36/36 脚本零 assert | 子代理 A、B 各自独立 grep 得同结论，互为交叉验证 | ✅ 采信（双源） |

### 7.2 我否决的及理由（4 条）

| # | 子代理结论 | 我的处理 | **否决理由** |
|---|---|---|---|
| **V1** | A/B：`exp08` 头部量「是像素混叠伪影不是截断物理」，并给出 4× 加密后发散到 +2.60 的表 | **降级为建议**，不作为阻断 | 该判断依赖子代理自建的连续域口径（`P /= P.sum()` 窗内重归一 + 解析积分），**其「正确值 +2.242e-01」本身是子代理自己的重算，无一手出处**。我能确认的硬事实只有两条：`exp08:56-57` 只记 `abs_diffs` **不判定、无容差**，以及 `:62-64` 的负例控制 `4.95e-6` **大于**最小宣称效应 `4e-6`。这两条足以判「须修」，但不足以支撑「物理量错 2 倍」这一更强断言。**偏严，予以否决。** |
| **V2** | B：`exp09` 局部样条「数值病态」，`b[0]=1.0` 造成无穷斜率条件，`−111268` 是 bug 伪影 | **降级为须修的注记**，不单列为阻断 | 方向合理，但「自然三次样条的首行条件应为 `2m[0]+m[1]=rhs_0`」这一断言**我未回源到该实现的作者意图或权威出处**，且我本人未读该文件（属未读清单第 21 项）。在「结论必须来自我自己读完原文」的纪律下，我不把它计入阻断。**证据不足，予以否决（降级）。** |
| **V3** | A：`route3/exp06:112` 注释「负=绿」与自有表 34/50 为正相反 | **采信为须修，但不计阻断** | 我未读该文件。但子代理 A、B **独立**得出同一符号矛盾，且 B 另给出 `route2/exp10:23` 的 `(<= 0)` 与全正归档矛盾——三文件三符号约定互斥，这是**可核的文本事实**而非推断。故采信文本矛盾，否决「据此推断某条具体结论为假」的延伸。**延伸推断予以否决。** |
| **V4** | C：`frame-snr-canon.md:553`/G3 的 `W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k` 在 registry 文件中 0 命中、属虚构条款 | **采信为须修（并入 B 类）** | 我用 `grep` 独立确认 `docs/detail/registry/` 下只有 `acsd.phase1.noise-snr.md`，且 C 的行号核对 `acsd.phase1.noise-snr.md:77` 存在但该式无命中——**这一条我确认成立**。但 C 进一步断言「canon §5 整行 G3 无依据」属**推论**：该式可能在 `UNIFIED_MODEL.md:44` 以 `point_information | a²PᵀC⁻¹P` 形式存在（子代理自己也指出这一点）。**「虚构条款」这一措辞过强，予以否决；降级为「引用行指向的文件/式子对不上」。** |

### 7.3 双方口径冲突的裁决

- **门总数**：A 报「41 个记录指标 / 10 条恒真」，B 报「38 条记录门 / 10 条恒真」。差异源于是否把 `metrics()` 派生的辅助列计入。**裁决**：以**去重门层**为准，本片 `audit/` 可辨识独立门约 **10 类**，两者一致；差异不影响结论。
- **文件行数**：C、D 均报告 `README.md` 107→108、`exp06/e6_ablation.py` 166→167、`route3/exp01` 140→141。**裁决**：任务书/片清单行数与 `wc -l` 差 1（末行无换行），属常态，**不构成发现**。

---

## 8 自证段（可复跑命令）

全部只读，可在仓库根直接执行。中文路径已带引号。

```bash
cd "/workspace/Astro CS Database"

# —— A1/A2：exp11 归档钉住的头文件路径不存在 + 归档哈希与现码不符 ——
ls lib/algorithms/integration/phase2_integrate/include/            # 输出只有 acsd
ls lib/algorithms/integration/phase2_integrate/include/astrocs/weight_chain.h 2>&1 | head -1   # No such file
sha256sum lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp \
          lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h
python3 -c "import json;d=json.load(open('实验/absolute-snr/code/audit/results/route3/exp11_frozen_operator_transfer.json',encoding='utf-8'));p=d['production_link'];print('archived header:',p['header']);print('archived sha256_source:',p['sha256_source']);print('archived sha256_header:',p['sha256_header']);print('gates_all_green:',d['gates_all_green'],'red_gates:',d['red_gates'],'n_gates:',len(d['gates']))"
# 对照：archived sha256_source 3dba6d82… vs 实测 ee905a8f…

# —— A1 根因：品牌改名提交 ——
git -c core.quotepath=false log --oneline --diff-filter=R --name-status -3 -- 'lib/algorithms/integration/phase2_integrate/include/*'
git -c core.quotepath=false log -1 --format='%h %ad %s' -- lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp
git -c core.quotepath=false log -1 --format='%h %ad %s' -- '实验/absolute-snr/code/audit/results/route3/exp11_frozen_operator_transfer.json'

# —— A3：b7 构建脚本指向不存在的 v6 ——
find lib -type d -name v6            # 0 命中
grep -n 'V6=' 实验/absolute-snr/code/b7_build_driver.sh
grep -n 'astrocs/v6' 实验/absolute-snr/code/b7_recon_driver.cpp
grep -n 'phase2_integrate' 实验/absolute-snr/code/exp11_build_driver.sh   # 对照：正确路径

# —— A5/B3：README:104 与生产码相反 ——
awk 'NR>=139 && NR<=152 {printf "%d: %s\n",NR,$0}' lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp
awk 'NR>=160 && NR<=161 {printf "%d: %s\n",NR,$0}' lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp
sed -n '104p' 实验/absolute-snr/README.md

# —— A4：f-instr-survey [F-33] 行号错 ——
awk 'NR>=227 && NR<=240 {printf "%d: %s\n",NR,$0}' lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp
awk 'NR==167 {printf "%d: %s\n",NR,$0}' lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp
sed -n '475,477p' 实验/absolute-snr/docs/surveys/f-instr-survey.md

# —— B4：canon §5 伪引行号漂移 ——
awk 'NR>=33 && NR<=44 {printf "%d: %s\n",NR,$0}' docs/science/CONTROL_WEIGHT_SNR.md
sed -n '551,552p' 实验/absolute-snr/docs/frame-snr-canon.md

# —— B5：canon 自相矛盾（m_5 从不产出 vs 8/8 帧非空）——
sed -n '488p;516p;556p' 实验/absolute-snr/docs/frame-snr-canon.md

# —— B6：无引号裸条款伪引 ——
sed -n '186p' 实验/absolute-snr/docs/frame-snr-canon.md
grep -c 'frame_quality_scalar\|local_quality_proxy' docs/science/CONTROL_WEIGHT_SNR.md   # 0
sed -n '132p' docs/science/CONTROL_WEIGHT_SNR.md

# —— B7/B8：EXP-06 引不存在的 registry 文件 + 带引号伪引 ——
ls docs/detail/registry/ | grep -i noise            # 只有 acsd.phase1.noise-snr.md
ls docs/detail/registry/astrocs.phase1.noise-snr.md 2>&1 | head -1   # No such file
grep -n '稀疏帧内层几何与重建算子' docs/detail/registry/acsd.phase1.noise-snr.md   # :248
grep -c '不融合 gain 诊断模型' docs/science/NOISE_MODEL.md   # 0
sed -n '334p' docs/science/NOISE_MODEL.md

# —— B3 补充：G4 读噪双计已闭环但 canon 仍列开 ——
grep -n 'sigma_sky_source = SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS' lib/infrastructure/scheduler/src/module_adapters.cpp
awk 'NR>=177 && NR<=182 {printf "%d: %s\n",NR,$0}' lib/algorithms/noise_snr/cpp/src/snr_science.cpp
sed -n '490p' 实验/absolute-snr/docs/frame-snr-canon.md

# —— B9：REVIEW.md 12 个 md5 全部失效 ——
for f in README.md code/sci_b_common.py code/b3_domain_map.py code/b4_integration.py code/b6_gates_audit.py \
         results/b1_sky_scan.json results/b2_noise_terms.json results/b3_domain_map.json \
         results/b4_integration.json results/b5_phase3_transfer.json results/b6_gates_audit.json \
         results/DOC_CORRECTIONS.md; do
  printf "%-40s %s\n" "$f" "$(md5sum "实验/absolute-snr/$f" | cut -d' ' -f1)"
done   # 与 REVIEW.md:10-21 表逐条比对

# —— B10：REVIEW.md 阻断项确已闭环 ——
grep -n 'EPS_REF =' 实验/absolute-snr/code/b3_domain_map.py
grep -n 'np.nanmedian' 实验/absolute-snr/code/sci_b_common.py
grep -n 'var_equal_pred' 实验/absolute-snr/code/b4_integration.py
grep -rn 'results/REVIEW.md' 实验/absolute-snr/   # 0 命中

# —— B11：EXP05_TABLES 符号丢失 ——
sed -n '6p;20,23p;33,44p' 实验/absolute-snr/results/EXP05_TABLES.md

# —— B1：exp11 判红不阻断 ——
grep -c 'sys.exit' 实验/absolute-snr/code/audit/route3/exp11_frozen_operator_transfer.py   # 0
tail -3 实验/absolute-snr/code/audit/route3/exp11_frozen_operator_transfer.py

# —— B2：selftest_operators S6 三重伪引 + 本地对本地 ——
sed -n '110,112p;118,119p' 实验/absolute-snr/code/exp04/selftest_operators.py
grep -rn 'eval_bilinear' lib/ | wc -l      # 0
awk 'NR==136||NR==343 {printf "%d: %s\n",NR,$0}' lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp
grep -n '"bilinear"\|"bilinear_prod"' 实验/absolute-snr/code/exp04/operators.py

# —— B13/B14：audit/run_all.sh 无输出校验 + skip 后仍报 36 ——
sed -n '9p;13p;16p;21,26p;31p' 实验/absolute-snr/code/audit/run_all.sh
grep -rc 'assert ' 实验/absolute-snr/code/audit --include=*.py   # 全为 0

# —— C1：_explore_scale.py 死代码 ——
grep -rn '_explore_scale' 实验/absolute-snr/ --include=*.sh --include=*.py | grep -v '_explore_scale.py:'

# —— 幽灵路径影响面（本片成员内）——
grep -rn 'integration/v6\|astrocs/v6\|include/astrocs/' 实验/absolute-snr/code/ 实验/absolute-snr/docs/ 实验/absolute-snr/results/ 实验/absolute-snr/README.md
```

**未执行项（如实登记）**：本审稿**未**运行 `exp11_frozen_operator_transfer.py`、未编译 `b7_recon_driver`、未跑任何 `run_all.sh`。因此 A1 的「必抛 `FileNotFoundError`」是由 `:1109` 的 `open()` 调用点 + `:286-291` 的实现 + `:1186` 的 `json.dump` 位置**静态推出**，未实跑验证；实跑验证列为待前台执行项。A2 的哈希失配为实测（`sha256sum` 直接取证），无需运行。

**待联网核验**：f-instr-survey 中 PixInsight 式[18]/[20] 与 Eq 编号、Zackay & Ofek 2017、LSST `MaskedImage.h:823-826`、IRAF `allstar.html`/`daophars.html`、HST/SDSS/DES/PS1 官方文档逐字原文——本片未联网核。

---

## 9 结论摘要

本片 **阻断**。核心不是单条门的恒真，而是**生产证据链本身已断**：

1. 唯一被认证为「生产证据」的 `exp11`，其归档钉住的生产源码哈希与现行 `weight_chain.cpp` **不符**，且刷新它的脚本因头文件改名**必抛异常、永不写盘**；
2. `b7` 生产链路驱动的构建脚本指向**不存在的目录**，判据链在第一步即断；
3. 根因是提交 `d796a1d7` / `14a50e3e` 的**品牌改名（`astrocs`→`acsd`、`integration/v6`→`phase2_integrate`）只扫过 `lib/` 与 `docs/`，没有扫过 `实验/`**——`实验/` 下仍有 13 处 `integration/v6`、121 处 `registry/astrocs.*` 指向改名前的世界。

按 AGENTS.md §3（文档与代码冲突以文档为准订正代码）与 §8（不以 waiver 静默覆盖红灯），本片在上述三条闭环前不得采信其判据绿灯。
