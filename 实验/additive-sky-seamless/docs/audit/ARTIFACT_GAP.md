# 产物缺口登记：C1–C7 读数

本单元 `results/` 根目录的 `c1_additive.json`…`c7_realdata.json` 一度只剩 `c8_boundary_step.json`
与 `c9_sky_surface_compare.json`。本文登记缺口的范围、可证实的原因、已恢复的部分与仍然缺的部分。
凡属推测一律显式标注「推测」；无法证实的归因不写。

## 1. 缺口范围

`README.md` §4.2、§4.3、§11 引用的 C1–C7 招牌读数原先无落盘证据，即
`results/c1_additive.json`…`results/c7_realdata.json` 七个文件全部不在盘上。
其中受影响最大的三条：

- M1「Phase1 后帧间乘性比偏离 1 = 5.89e-4（未做 0.560）」；
- M4「纯加性 UPM 后接缝未做/做了 = 27.37 / 6.31 e⁻（4.33×）」；
- M5「10% 幅度恢复 0.693 ± 0.005（58σ）」。

`README.md` 自己在这几处标注「该读数需重跑取得，当前不可离线核验」，与盘上状态一致。

## 2. 原因（可证实）

**这些文件曾被 git 跟踪，并在一次归档移除提交中被删除。** 证据：

```
git log --all --oneline -- 实验/additive-sky-seamless/results/c1_additive.json
  1fa477a7 移除实验域运行结果归档，报告迁入单元文档目录
  4a2bd78c 前台重跑 P5 三条旧判据腿，覆盖归档 JSON（闭环车道按约束未跑）
  9a2b5d11 refactor: 根目录整合 + 收编 RELEASE-04 在飞改动

git show --name-status 1fa477a7 | grep results/c[1-7]
  D 实验/additive-sky-seamless/results/c1_additive.json
  D 实验/additive-sky-seamless/results/c2_multiplicative.json
  D 实验/additive-sky-seamless/results/c3_public_plane.json
  D 实验/additive-sky-seamless/results/c4_seam_criterion.json
  D 实验/additive-sky-seamless/results/c5_weights.json
  D 实验/additive-sky-seamless/results/c6_sparse_dense.json
  D 实验/additive-sky-seamless/results/c7_realdata.json
```

该提交删除实验域 `results/` 共 290 个文件。此后 `.gitignore:187` 忽略 `/实验/**/results/`，
配套注释写明「实验域的运行结果归档层已整层移除 ⇒ 各单元 results/ 不再入库，重跑产物不得回流」。
因此从该提交起，`results/` 只在磁盘上存在、不再有版本控制兜底：**任何一次重跑产物若未及时另存，
就只剩磁盘副本**。`c8`/`c9` 后来被重新跑出（磁盘上时间戳晚于该提交），C1–C7 没有。

**未被证实、故不写为原因的**：C1–C7 具体在哪一次操作后从磁盘消失、是否由某次 `run/` 清理连带删除，
本文无法证实。可证实的相关事实只有两条：一是 `run_all.sh` 自身的输出目录 `run/SCI-403/`
在本轮开工前**不存在**（该入口若完整跑过会留下 `logs/`、`results_prior/`、`results_head/`；
本轮为复跑才新建，见 §4）；二是 `README.md` 提到「原备份指针随 run/ 与 reports/ 清退」。
这两条都只说明「有过清理」，不足以指认清理者或时间。**推测**：C1–C7 的磁盘副本随某次实验域清理一并消失。

## 3. 权威副本仍然存在

三处 `run/` 快照保存着这些读数，且相互可对照：

| 来源 | 路径 | 覆盖 | 与 git 最后提交版的关系 |
|---|---|---|---|
| git 最后提交版 | `1fa477a7^`（= `bf25c085`） | C1–C7 全部 | 权威；本文比对基准 |
| 并发校准工作区 | `run/P1-CONCURRENCY-CALIB-01/wsrc/实验/additive-sky-seamless/results/` | C1–C7 全部 | C2/C3/C4/C6/C7 逐位相同；C1/C5 为更早版本 |
| 审稿基线 | `run/FINAL-07/gate-reds/baseline/实验/additive-sky-seamless/results/` | C1–C7 全部 | 同上 |
| P5 订正轮 | `run/FINAL-07/审核包/科研审查/P5_订正/{baseline,rerun_HEAD}/` | C1/C3/C7 | baseline 与审稿基线逐位相同；rerun_HEAD 为 `6808a7e4` 复跑 |

`run/P1-CONCURRENCY-CALIB-01` 与 `run/FINAL-07/gate-reds` 两份快照对 C1–C7 **逐位相同**
（md5 全等），说明这批读数在两次独立保存之间没有变化。

**版本对齐（关键）**：git 最后提交版中 C1、C5 与两份磁盘快照**不同**，且更新：

| 文件 | git `bf25c085` | 磁盘快照 | 说明 |
|---|---|---|---|
| `c1_additive.json` | 16749 B, md5 `e62fc5a7…` | 12630 B, md5 `c44765ce…` | git 版含 `A6p`/`A9p`/`A9q`/`A6p_A9p_discrimination`，即订正后的判据面；快照版是订正前的旧判据面 |
| `c5_weights.json` | 6712 B, md5 `14225526…` | 5649 B, md5 `863e4b47…` | 同上，git 版为订正后 |
| `c2/c3/c4/c6/c7` | — | — | 逐位相同 |

git 版 C1/C5 对应 `4a2bd78c`（前台重跑 P5 三条旧判据腿）之后的修订，晚于
`code/c1_additive.py`（09-30 20:58）与 `code/c5_weights.py`（09-30 21:01）的改动时刻，
即**这两个文件与当前脚本修订一致**。

## 4. 已恢复的部分

本轮按 `run_all.sh` 的方式（同命令形式、同日志名、同 `mem_guard` 看门狗）重跑 C1–C7。
可跑通与不可跑通的分布如下：

| 腿 | 结果 | 说明 |
|---|---|---|
| C4 `c4_seam_criterion` | **跑通**（rc=0，6/6 判据 PASS） | 纯 Python，不依赖探针 |
| C1 `c1_additive` | 跑不动 | 需 `run/SCI-403/sky_probe`、`upm_probe` |
| C2 `c2_multiplicative` | 跑不动 | 需 `upm_probe` |
| C3 `c3_public_plane` | 跑不动 | 需 `sky_probe` |
| C5 `c5_weights` | 跑不动 | 需 `sky_probe`、`upm_probe` |
| C6 `c6_sparse_dense` | 跑不动 | 需 `upm_probe`、`sky_probe` |
| C7 `c7_realdata` | 跑不动 | 需 `sky_probe` |

探针无法构建，原因是构建入口引用的路径与库名已经失效：

```
$ python3 eng/tools/monitoring/mem_guard.py --max-rss-gb 8 --timeout 3600 \
      bash 实验/additive-sky-seamless/code/build_probes.sh
== build upm_probe
[build_probes] build/libastrocs_phase2.a 不存在 ⇒ 先 ninja -C build
ninja: Entering directory `build'
ninja: error: loading 'build.ninja': No such file or directory
[mem_guard] peak_rss_gb=0.003 limit_gb=8.0 label=bash exit=1
```

`build_probes.sh` 期望 `$ROOT/build/libastrocs_*.a` 与 `build/build.ninja`；实际生产静态库是
`$ROOT/build/linux-control/libacsd_*.a`（Makefile 构建，无 `build.ninja`）——库名（`astrocs` → `acsd`）
与构建目录都变了。`code/**` 本轮未改动。

各腿的真实报错（`run/SCI-403/logs/{c1..c7}.log` 尾部）一致形态：

```
FileNotFoundError: [Errno 2] No such file or directory:
  '/workspace/Astro CS Database/run/SCI-403/sky_probe'      # c1/c3/c5/c7
  '/workspace/Astro CS Database/run/SCI-403/upm_probe'      # c2/c6
```

C4 的重跑与 `bf25c085` 提交版逐项对照：**判词完全一致（6/6 PASS，`n_fail=0`）**，
数值只在浮点末几位不同（最大相对差约 4e-14，例：`N1_deg_no_power` 0.09234725368755825 →
0.09234725368756219）。这说明该腿可复现到浮点舍入级。

## 5. 仍然缺的部分

- **C1/C2/C3/C5/C6/C7 的「当前修订重跑」读数不存在**。盘上恢复的是 `bf25c085` 提交版
  （见 §6），它与当前脚本修订的差距：`code/sci_c_common.py` 的改动时刻（10-02 22:57）
  晚于这批读数的产出时刻（09-30 21:27），因此严格说这批读数**不是**当前共享库修订下的产物。
  要拿到当前修订的证据，必须先修复探针构建路径再重跑。
- **`README.md` §11.3 的 A6p/A9p/A9q 读数**只存在于 `bf25c085` 提交版 C1 中，磁盘快照没有；
  已在 §6 随恢复一并回到盘上。
- **`P5_订正/rerun_HEAD`（`6808a7e4`）与 baseline 的差异未收敛**：C1 有两条判据判词翻转
  （`A6_subset_invariance`、`A9_final_gauge_near_noop`），C3/C7 数值差异大但判词不变
  （如 `c7` 的 `kappa` 3.16e7 → 4.93e3）。这是订正轮本身的对照材料，本轮未重新裁决。

## 6. 盘上现状与来源标注

| 文件 | 盘上来源 | 与 `bf25c085` 提交版 |
|---|---|---|
| `results/c1_additive.json` | git `bf25c085` 恢复 | 逐位相同 |
| `results/c2_multiplicative.json` | git `bf25c085` 恢复 | 逐位相同 |
| `results/c3_public_plane.json` | git `bf25c085` 恢复 | 逐位相同 |
| `results/c4_seam_criterion.json` | **本轮重跑** | 判词相同，数值差 ≤ 4e-14 |
| `results/c5_weights.json` | git `bf25c085` 恢复 | 逐位相同 |
| `results/c6_sparse_dense.json` | git `bf25c085` 恢复 | 逐位相同 |
| `results/c7_realdata.json` | git `bf25c085` 恢复 | 逐位相同 |

**恢复不等于重跑**：上表六个「git 恢复」文件是删除前最后提交的固化读数，按字节还原，
**不是**本轮跑出来的；只有 C4 是本轮重跑。二者不要混读。恢复动作只是把 `1fa477a7` 对
本单元的删除还原回磁盘，不改变任何数值。

## 7. 判据面与 `README.md` 的对照结果

对照基准取 `bf25c085` 提交版（权威），C4 另用本轮重跑。

### 7.1 `README.md` §4.2（C2 乘性世界）

| 判据 | README 声称值 | `c2_multiplicative.json` 实测 | 一致 | 备注 |
|---|---|---|---|---|
| M1 | 5.89e-4（未做 0.560） | `after=0.000589200073179974`、`before=0.5599394542162011` | **一致** | 5.892e-4 / 0.5599 |
| M1b | 0.560 | `0.5599394542162011` | **一致** | |
| M2 | 0.0376（无梯度）/ 0.107（有梯度） | `flat=0.037605932855678814`、`gradient=0.10730025204558769` | **一致** | |
| M3 | 0.0295 / 0.0477 | `flat=0.029493998310297864`、`gradient=0.04769656270421807` | **一致** | |
| M4 | 27.37 / 6.31（**4.33×**） | `none=27.36711950826256`、`loworder=6.313933085541166`、`ratio=4.334401257899446` | **一致** | |
| M5（10%） | 0.693 ± 0.005（58σ） | `amp10=0.6925973534384495`、`err10=0.005262883226255595`、`n_sigma10=58.40955106660412` | **一致** | PSD 峰 `k=26` vs 预期 `21.333` 也一致 |
| M5（1%） | **未检出**（amp −0.0004 ± 0.038），3σ 检测限 0.069 | `hf_matched_filter.amp_hat=0.6720891171838539`、`amp_err=0.023018733227673917`、`n_sigma=14.2453922017707`；`detection_limit_3sigma=0.06905619968302175`；`detected_at_1pct=false` | **不一致** | 见 §7.4 |
| M6 | max\|Δseam\| 2.14 < 50% 基线 17.50；ρ=0.50 | `max_abs_dseam=2.1385042537276036`、`baseline_seam_max_hf0=17.495663462866332`、`spearman_vs_hf=0.49999999999999994` | **一致** | 50%×17.4957=8.7479 |

### 7.2 `README.md` §11（判据变更登记）

| 判据 | README 声称值 | JSON 实测 | 一致 |
|---|---|---|---|
| A2 | slope 0.7681；现判据比 0.975 | `A2_curve_calibrated.ratio=0.9747277829127695`（`slope_pred=0.8235`） | 比值一致；slope 0.7681 未落盘 |
| A10 | Pearson 0.8964 | `c1`(快照版) `pearson=0.8964021920950578` | **一致**（快照版，见 §7.4） |
| M2 | 0.0376 / 0.107 | 同上 | **一致** |
| M3 | 0.0295 | 同上 | **一致** |
| M4 | 4.334 | `ratio=4.334401257899446` | **一致** |
| M6 | ρ 0.10→0.50；max\|Δseam\| 2.14 < 8.75 | `spearman_vs_hf=0.5`、`max_abs_dseam=2.1385`、50%×17.4957=8.7479 | **一致**（「8.75」即 50% 门限，「17.50」是基线，两者不矛盾） |
| N4 | A* 6.975 | `N4_detection_limit.lim=6.975350326829212` | **一致** |
| W1 | vs uniform 仅 8.45% | `margin_uniform=0.08455244978776011` | **一致** |

### 7.3 `README.md` §11.3（判据重写登记）

| 声称值 | `c1_additive.json`（`bf25c085` 提交版）实测 | 一致 |
|---|---|---|
| A6p = 0.752 σ_control（子集项 0.752、gauge 项 1.6e-15） | `A6p_subset_and_gauge_consistency.worst_sigma=0.7523263017597124`、`subset_sigma=0.7523263017597124`、`gauge_sigma=1.638584401559019e-15` | **一致** |
| A9p = 0.13272 ≤ 0.30136 | `A9p_terminal_gauge_margin.max_abs_G=0.13271928566723545`、`tol_M=0.3013559430274131` | **一致** |
| A9q \|ΔC\| = 0.0 | `A9q_final_gauge_zero_effect_on_solve.dC=0.0`（`obj_same=true`） | **一致** |
| A9p 对绝对容差档判红（3.09e-4 > 自声明 1e-6） | `A6p_A9p_discrimination[abs_tolerance_arm]`：`max_abs_G=0.00030926509680284653`、`tol_M=1e-06`、`red=true` | **一致** |
| A6p gauge 项对逐帧 gauge 注入判红 | `A6p_A9p_discrimination[gauge_frame_dependent_injection]`：`max_dev=4.878360717760814 > tol_2sigma=1.0840801595024028`、`red=true` | **一致** |
| W1_prime：snr2 +0.142914±0.069540（z=+2.0551）；uniform −0.011721±0.046178（z=−0.2538） | 见 §7.4 | **未能核验** |

### 7.4 不一致与未能核验项

**(a) M5 的 1% 幅度行与 JSON 不符（不一致，需订正）。**
`README.md` §4.2 写「1% 幅度：**未检出**（amp −0.0004 ± 0.038）」。该 JSON 中：

- 找不到 `−0.0004` 与 `0.038` 这两个数（`c2_multiplicative.json` 全文件无此二值）；
- 1% 档的匹配滤波读数是 `amp_hat=0.6720891171838539 ± 0.023018733227673917`，`n_sigma=14.25`
  ——这是**检出**，不是「未检出」；
- JSON 与 README 一致的部分只有：3σ 检测限 `0.06905619968302175`，以及 PSD 峰不在注入频率
  （`hf_component.psd_peak_at_injected=false`、`detected_at_1pct=false`）。

**推测**：README 该行保留了 §11.1 所述「M5 改为对模板做比 + 匹配滤波」之前的旧值
（旧值 −0.0004±0.038），而 10% 行已更新到新值；即订正只落到 10% 行，1% 行漏改。
该推测未能证实——无法从现存文件判定旧值来源。

**(b) `A10` 的引用版本与最新提交版不符（不一致，需订正）。**
`README.md` §4.1 与 §11 引用的 `slope 0.798`、`Pearson 0.8964`、`端点比 ×5.07` 来自
**订正前**的 C1（即磁盘快照版，md5 `c44765ce…`）；`bf25c085` 提交版 C1 的同一判据为
`slope=0.7687065969853147`、`pearson=0.8822086109818777`、`ratio=6.139029101773245`。
同一现象也见于 A3（0.0087 vs 0.007712）、A6（0.0236 vs 0.407791）、A8b（0.02317 vs 0.400113）、
A9（0.00372 vs 0.132719）。即 README 的 §4.1 行对应订正前判据面，§11.3 行对应订正后判据面，
两套读数在 README 中并存而未标注各自对应的修订。

**(c) W1_prime 的误差棒未能核验。**
`README.md` §11.3 的 `W1_prime` 表（`snr2 +0.142914±0.069540`、`uniform −0.011721±0.046178`）
在 `c5_weights.json` 中**不存在**：盘上 `bf25c085` 提交版 C5 只给点估计
（`margin_snr2=0.25916157215795865`、`margin_uniform=0.08455244978776011`）与
`honest_boundary_W1` 文字，没有 `W1_prime` 字段、也没有 ±SE 与 z 值。
**推测**：该表来自比 `bf25c085` 更晚、未入库的一次 C5 运行。

## 8. 建议

1. 修复 `code/build_probes.sh` 的库路径与库名（`build/libastrocs_*.a` → `build/linux-control/libacsd_*.a`，
   `ninja -C build` → 对应的 Makefile 入口），再按 `run_all.sh` 重跑 C1–C7，
   取得**当前修订**下的读数；这是唯一能消除 §5 第一条缺口的方式。
2. 订正 `README.md` §4.2 的 M5 1% 行（与 JSON 对齐，或说明该值的来源）。
3. 在 `README.md` §4.1 与 §11 的相应行标注各自对应的修订（订正前/订正后），
   避免同一判据出现两个未标注版本的读数。
4. 若要让 `results/` 不再依赖磁盘，需要一条与 `.gitignore` 策略相容的固化路径
   （例如把固化读数落到单元 `docs/` 下的证据目录）；这属策略决定，不在本单元自行改定。

## 9. 探针构建入口修复

`code/build_probes.sh` 原先引用 `build/libastrocs_*.a` 与 `ninja -C build`，两者都已不存在。
现改为按实际布局解析：

- **构建目录**：`$ACSD_BUILD_DIR` > `build/linux-control` > `build`（不写死绝对路径）；
- **静态库名**：`libacsd_*.a`，兼容历史名 `libastrocs_*.a`；
- **触发库构建**：用该目录自带的 `Makefile`（或 `build.ninja`），且**在锁内**做；
- **全流程持 `flock ${ACSD_BUILD_LOCK:-/tmp/astrocs_build.lock}`**，等待上限 `$ACSD_LOCK_WAIT`
  （默认 7200 s）；取不到锁即放弃，不并发链接；
- 去掉只在 `ASTROCS_PROBES=ON` 时才存在的探针库（两个探针源都不使用 `ACSD_PROBE_*` 宏）；
- 新增 `$ACSD_PROBE_SRC_DIR`：探针源目录可指向一份同源副本，用于在不改动 `code/**` 的前提下构建。

真实输出尾部：

```
[build_probes] 已取构建锁 /tmp/astrocs_build.lock
== build upm_probe
[build_probes] 构建树 = …/build/linux-control
[build_probes] 源目录 = …/results/rerun_1f6d0a64/probe_src
[build_probes] 静态库 = …/libacsd_phase2.a …/libacsd_identifiability.a …/libacsd_hips.a …/libacsd_aio.a …/libacsd_common.a …/libacsd_cfitsio.a
== build sky_probe
…
built -> …/run/SCI-403/{upm_probe,sky_probe}
[mem_guard] peak_rss_gb=0.323 limit_gb=8.0 label=bash exit=0
```

## 10. 探针驱动源的陈旧点（未改 `code/**`）

`code/upm_probe.cpp` 有 **3 处** `astrocs::healpix::`，而 `lib/algorithms/shared/healpix/healpix_core.h`
的命名空间已改为 `acsd::healpix`（该头文件第 20–21 行），故原样编译报
`error: 'astrocs' has not been declared`。`code/sky_probe.cpp` 无此问题，原样编译通过。

`code/**` 本轮只允许改 `build_probes.sh`，故**未改动驱动源**；改为在本单元写面内保留一份
同源副本 `results/rerun_1f6d0a64/probe_src/`：`sky_probe.cpp` 与原文件逐位相同，
`upm_probe.cpp` 仅上述 3 处命名空间不同（`diff` 只有这 3 行）。用 `$ACSD_PROBE_SRC_DIR`
指向该副本构建，已实测两个探针都能编出（见 §9）。

**待裁决**：是否授权把 `code/upm_probe.cpp` 的 3 处命名空间就地订正，使 `code/` 本身
可直接构建（避免长期依赖副本）。

## 11. 当前修订重跑（读数 `results/rerun_1f6d0a64/`）

命令形式与 `run_all.sh:86-95` 一致（从仓库根、`mem_guard` 看门狗、日志名 `${step%%_*}.log`），
由 `results/rerun_1f6d0a64/run_leg.sh` 逐腿执行；新读数落 `results/rerun_1f6d0a64/`，
`results/` 根目录里的 `bf25c085` 固化读数原样放回，两套并存。

| 腿 | rc | peak_rss_gb | 判词翻转 | 备注 |
|---|---|---|---|---|
| C1 `c1_additive` | 0 | 0.554 | 无 | 14 门全绿；`A7b` iters 300 → 3283 |
| C2 `c2_multiplicative` | 0 | 0.571 | 无 | 7 门全绿；`M4` ratio 4.3344 → 4.3367 |
| C3 `c3_public_plane` | 0 | 0.646 | 无 | 7 门全绿 |
| C4 `c4_seam_criterion` | 0 | 0.549 | 无 | 6 门全绿 |
| C5 `c5_weights` | **1** | 0.602 | **`W5_enum_gap` PASS → FAIL** | 见下 |
| C6 `c6_sparse_dense` | 0 | 0.569 | 无 | 4 门全绿；`E1` 3.1e-15 → 0.0 |
| C7 `c7_realdata` | 0 | 0.257 | 无 | 5 门全绿 |

- **C5 是本轮唯一判红的腿**：`W5_enum_gap` 的 `stalled_case` 由 0 变 1、`iters` 由
  [3, 1, 300] 变 [3, 3496, 1361]。同一 seed、未改判据、未调参。
- **C4 两次独立重跑逐位相同**（md5 `5513aa64…`），且与 `bf25c085` 判词一致、数值仅差浮点末位
  （最大相对差约 4e-14）；说明该腿确定可复现。
- C1/C2/C3/C6/C7 与 `bf25c085` 逐门对照**无判词翻转**，数值差异见 §11 与 README 各节的来源标注。

## 12. 对 §7.4 的更正（上轮结论有误）

上轮把 `W1_prime` 记为「C5 里不存在该字段 ⇒ 未能核验」，**该结论错误**：
`W1_prime` 是 `c5_weights.json` 的**顶层键**，不是 `gates.rows` 里的判据 id
（判据 id 为 `W1p_arms_statistically_indistinguishable`）。上轮只在 `gates.rows` 里找 id，
故漏判。实际情况：

- `bf25c085` 的 `c5_weights.json` **有** `W1_prime` 顶层键，内容与 `README.md` §11.3 的表一致
  （`snr2` margin 0.1429135268084416 / se 0.0695398598814922 / z 2.05513107…；
  `uniform` margin −0.011721… / se 0.046178… / z −0.2538…）；
- 当前修订重跑给出**逐位相同**的 `W1_prime`；
- 旧判据面快照（md5 `863e4b47…`）的 `W1_prime` 为 `null`。

因此 `README.md` §11.3 的 `W1_prime` 表**有据可核**，本轮已把 README 的字段名与读数文件
标注为实际值（`W1p_arms_statistically_indistinguishable` / `results/rerun_1f6d0a64/c5_weights.json`）。

## 13. `README.md` 本轮改动（依据）

| 位置 | 改动 | 依据读数 |
|---|---|---|
| §4.2 `M5` 行 | 1% 幅度由「未检出（−0.0004 ± 0.038）」改为「匹配滤波恢复 **0.672 ± 0.023**（14σ），PSD 峰不在注入频率，3σ 检测限 0.069」 | `results/rerun_1f6d0a64/c2_multiplicative.json` 的 `hf_matched_filter`（`amp_hat`/`amp_err`/`n_sigma`）与 `hf_component`（`psd_peak_at_injected=false`、`detection_limit_3sigma`） |
| §11.1 `M5` 行 | 同步订正 1% 幅度表述 | 同上 |
| §4.1 表下 | 新增「读数来源」注：本表为旧判据面读数，并列出与当前修订重跑的逐项对应值 | 旧判据面 `run/FINAL-07/gate-reds/baseline/…/c1_additive.json`；当前 `results/rerun_1f6d0a64/c1_additive.json` |
| §4.5 表下 | 新增「读数来源与当前修订差异」注：RMS/漏入为旧判据面读数，给出当前修订值；登记 `W5_enum_gap` 判红 | 旧判据面 `…/gate-reds/baseline/…/c5_weights.json`；当前 `results/rerun_1f6d0a64/c5_weights.json` |
| §11.3 `W1_prime` | 标注实际字段名与读数文件 | `results/rerun_1f6d0a64/c5_weights.json` 的 `W1_prime` |
| §4.1–§4.7 小节标题 | 「当前不可离线核验」改为指向 `results/rerun_1f6d0a64/` 的当前修订读数 | 本轮重跑 |
| §4.5 标题 | 「5/5 PASS」改为「4/5 PASS」 | 本轮重跑 `n_fail=1` |

`§8.1`/`§8.2`/`§1` 中指向 `evidence_lit.json`/`evidence_code.json`/`audit_rework_summary.json`
的「不可离线核验」标签**未改**：这三个文件不在盘上，标签仍准确。

## 14. 仍未收敛

- **C5 `W5_enum_gap` 判红**：当前修订下 `stalled_case=1`、`iters=[3, 3496, 1361]`，
  与旧判据面的 `stalled_case=0`、`iters=[3, 1, 300]` 不一致。未裁决该判红属实现缺陷
  还是判据口径需更新。
- **`P5_订正` rerun_HEAD（`6808a7e4`）与 baseline 的 A6/A9 判词翻转**：仍未裁决。
- **`code/upm_probe.cpp` 的 3 处命名空间**：未授权改动，暂以 `results/rerun_1f6d0a64/probe_src/`
  的同源副本构建。
- **`sci_c_common.py` 的改动时刻晚于本批读数**：本轮重跑已把该差距消除（重跑用的就是当前
  共享库），但 `bf25c085` 固化读数与当前共享库的对应关系仍未逐项核对。
