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

## 15. 三项裁决的落地

**裁决 1（探针驱动源）**：已就地订正 `code/upm_probe.cpp` 的 3 处
`astrocs::healpix::` → `acsd::healpix::`（第 34、187、194 行），依据
`lib/algorithms/shared/healpix/healpix_core.h` 第 20–21 行的现命名空间 `acsd::healpix`。
订正后用**仓库自身源**重建，真实输出尾部：

```
[build_probes] 已取构建锁 /tmp/astrocs_build.lock
[build_probes] 源目录 = …/实验/additive-sky-seamless/code
== build upm_probe … == build sky_probe
built -> …/run/SCI-403/{upm_probe,sky_probe}
[mem_guard] peak_rss_gb=0.323 limit_gb=8.0 label=bash exit=0
```

`$ACSD_PROBE_SRC_DIR` 兼容口子**已不再需要**：仓库源现在可直接构建，且
`results/rerun_1f6d0a64/probe_src/upm_probe.cpp` 与 `code/upm_probe.cpp` 已逐位相同。
保留该口子的唯一价值是「让某次重跑把自己的驱动副本钉在记录里」；若以最小面为准可删。
本轮**保留**（不改动已交付的 `build_probes.sh` 行为），并在 `code/upm_probe.cpp` 的注释里
不引入任何副本依赖。

**裁决 2（越界写出的图）**：`docs/figs/fig_c7_real_crop.png` 由 C7 脚本派生写出、在写面外，
但它是**当前修订的派生图**且**未被 git 跟踪**（`.gitignore:60 *.png`）⇒ 回滚无意义、
新版更正确。**经复核保留新版**，不回滚；新图另存一份 in-scope：
`results/rerun_1f6d0a64/fig_c7_real_crop.png`。

**裁决 3（两套读数的版本归属口径）**：
`results/` 根目录的固化读数是 `bf25c085` 版（删除前固化），`results/rerun_1f6d0a64/`
是**当前修订**读数（探针构建已修、与生产库同修订）；引用任何数字时必须写明取自哪一套；
两套并存不是歧义，是版本归属。

## 16. C5 `W5_enum_gap` 翻红归因

### 16.1 判据本身是什么

`code/c5_weights.py` 第 250–267 行：三例收敛枚举，判据条件是

```python
res["production_convergence"]["maxiter_case"] == 0 and
res["production_convergence"]["stalled_case"] == 0
```

即**它断言的正是「缺陷存在」**——生产 `p2_upm_convergence` 只返回 0/1、max_iter 与
stalled 不可区分（登记 FIX-2）。这是一条**缺陷登记门**：缺陷在 ⇒ 绿；缺陷被修 ⇒ 红。
`lib/algorithms/coverage/include/astro/phase2/upm.h` 第 445 行给出枚举口径
`0=max_iter 1=converged 2=stalled 3=invalid`。

### 16.2 候选改动清单与证据

`bf25c085`（固化读数产出于 2026-09-30 21:27）之后、与 C5 权重/枚举相关的改动：

| 路径 | `bf25c085..HEAD` 的提交 | 是否候选 |
|---|---|---|
| `实验/…/code/c5_weights.py` | 无提交 | 否（判据与场景未动） |
| `实验/…/code/sci_c_common.py` | 仅 `1fa477a7`（删归档，不涉求解） | 否 |
| `lib/algorithms/coverage/src/upm.cpp` | **`36267e1a`**、`e0997984`、`918325a6`、`303cc74f`、`bdc350df`、`40088560`、`a24a8d2f` … | **是** |

**根因提交：`36267e1a`**（2026-10-07 00:27:32）
「UPM自适应收敛：迭代预算按κ保守收敛率追加（下界100/上界5000可配），判敛阈值不动；
修M42 converged=0顶满导致的x2030残余缝」。其 diff 的关键行：

```diff
+ cfg.max_iterations_cap = 5000;
+ // 自适应迭代预算：max_iterations 为"下界/试跑预算"语义（小 κ 早停，行为逐位不变）；
+ // max_iterations_cap 为可配上界（默认 5000，防病态发散）。
+ int iter_budget = cfg.max_iterations;
- for (int iter = 0; iter < cfg.max_iterations; ++iter) {
+ for (int iter = 0; iter < iter_budget; ++iter) {
+   // 自适应追加：预算耗尽、未收敛、未 stall、且改善仍显著时，按 κ 保守收敛率
+   // 估算剩余轮数并延长 budget（可重估，上限 cap）。
```

即 `max_iterations` 的**语义由「上限」改为「下界/试跑预算」**，另加可配 `max_iterations_cap`
（默认 5000）。这同时解释了两个观测：

- 固化读数 `iters=[3, 1, 300]`（上限被遵守）→ 当前修订 `iters=[3, 3496, 1361]`；
- `maxiter_case` 的 `max_iterations=1` 现在跑出 3496 轮 —— 预算被自适应延长到 `[1, 5000]` 内。

### 16.3 上限实验：是「卡住」还是「触上限」

复现脚本 `results/rerun_1f6d0a64/c5_w5_attrib.py`（只读，世界构造与 c5 一致：
`S.build_world(seed_tag="c5_0", blur=12.0, level=20.0)`），读数
`results/rerun_1f6d0a64/c5_w5_attrib.json`。

**(i) stall 档（o3）迭代数上限扫描 —— iters 恒定 ⇒ 不是触上限**

| `max_iterations` | 50 | 100 | 300 | 600 | 1200 | 2400 | 4800 | 9600 | 20000 |
|---|---|---|---|---|---|---|---|---|---|
| `iters` | 1361 | 1361 | 1361 | 1361 | 1361 | 1361 | 1361 | 1361 | 1361 |
| `converged` | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |

上限从 50 提到 20000，迭代数**一位不变**、判词恒为 1 ⇒ 求解器是**按自身判据停下**的，
不是被上限截断。

**(ii) stall 档目标函数平台证据**

| `tolerance` | 1e-12 | 1e-9 | 1e-6 | 1e-3 |
|---|---|---|---|---|
| `iters` | 1361 | 984 | 824 | 30 |
| `objective` | 39.2927482808 | 39.2927258943 | 39.2923694520 | 52.7709677785 |
| `Δobj/obj` | 0 | 5.70e-07 | 9.64e-06 | 3.43e-01 |

容差放宽六个数量级，目标函数相对变化 ≤ 9.7e-6 ⇒ 目标函数**已落到平台**。

**(iii) o2 档：`max_iterations` 已不再是上限**

| `max_iterations` | 1 | 2 | 5 | 10 | 50 | 300 |
|---|---|---|---|---|---|---|
| `iters` | 3496 | 3496 | 3491 | 3497 | 3493 | 3497 |
| `converged` | 0 | 0 | 0 | **2** | 0 | **2** |

上限 1→300，迭代数恒在 ~3491–3497；且 `converged` 出现 **2（stalled）**——证明生产返回值
**已不止 0/1**，即 FIX-2 所述缺陷已不复存在。

### 16.4 结论：**(b) 判据口径需更新**

理由：

1. `W5_enum_gap` 是一条**缺陷登记门**，其断言（`maxiter_case == 0 and stalled_case == 0`）
   成立的前提是「生产只给 0/1」。当前修订实测出现 0、1、**2** 三种取值（§16.3(iii)），
   前提已不成立 ⇒ 门翻红是**门的前提被修好**，不是实现退化。
2. 不是「触上限」：stall 档在上限 50→20000 下 iters 恒为 1361（§16.3(i)），且目标函数
   在容差放宽六个数量级下相对变化 ≤ 9.7e-6（§16.3(ii)）——它按自身判据停下。
3. 因此不需要（也不允许）为变绿而调参或改判据；正确的处置是**更新这条门**：
   它应改为断言「三例在枚举上可区分」（例如 `converged_case=1`、`maxiter_case∈{0}`、
   `stalled_case∈{2}` 或至少 `maxiter_case != stalled_case`），或直接退役。
4. **附带需一并更新**：`o2` 场景（`max_iterations=1, tolerance=1e-9`）的**语义已失效**——
   `max_iterations` 不再是上限（§16.3(iii)），该场景已无法"用 1 轮强制 max_iter"；
   `o3` 场景在当前求解器下判为 `converged=1`（不再是 stalled），要真正行使 `stalled(2)`
   分支需重新设定容差/平滑档。这两点属**判据口径更新**，不在本轮改动（未授权改 `c5_weights.py`）。

**未裁决的残余**：`o3` 判为 `converged=1` 是否符合
`docs/science` 中「converged 状态枚举」一节的口径（`1 = converged` 需 `tol_step` 与
`tol_obj` 同时满足），本轮只给出「生产自洽且按自身判据停下」的证据，未逐条比对阈值定义。

## 17. 仍未收敛（照旧登记）

- **`P5_订正` rerun_HEAD（`6808a7e4`）与 baseline 的 `A6_subset_invariance` /
  `A9_final_gauge_near_noop` 判词翻转**：仍未裁决。
- **C5 `W5_enum_gap` 的门与场景更新**：归因已定（§16），但改 `code/c5_weights.py`
  未在本轮授权内，待派单。
- **`o3` 档 `converged=1` 与 science 枚举口径的逐条比对**：见 §16.4 残余。

## 18. P1：`W5_enum_gap` 退役为显式诊断读数

**为什么退役而不是放宽**：该门断言 `maxiter_case == 0 and stalled_case == 0`——断言的正是
「缺陷存在」（缺陷在则绿、缺陷修好则红），**其绿红与质量方向相反**。按 `AGENTS.md` §13
「本项目不设门禁；已有的门禁式机制按退役处置——删除，或改为显式的诊断/校验读数（记录、上报、
可复核），但不得阻塞」，处置为**退役为记录**。

**改动点**（`code/c5_weights.py`）：

- 删去 `g.add("W5_enum_gap", …, ok=…)` 这条判据行 ⇒ 它不再进入 `gates.rows`，
  因而不参与 `n_fail`、不影响 `main()` 的退出码；
- 新增 `res["convergence_enum_diagnostic"]`：逐例记录
  `scenario / converged / converged_name / iterations / objective / tolerance /
  tolerance_relative / max_iterations / max_iterations_cap / rc_build /
  dobj_over_obj_vs_converged`，并给出诊断结论字段
  `distinguishable`（三例枚举是否两两不同）、`distinct_enum_values`、`enum_names`；
- 输出新增 `[DIAG]` 行（不判 PASS/FAIL）；
- 模块 docstring 的 W5 条目同步改写。

**新读数样例**（真实输出尾部，`results/rerun_1f6d0a64/c5_weights.json`）：

```
  [PASS] W4_convergence_enum_spec     {'converged': 1, 'max_iter': 0, 'stalled': 2, 'invalid': 3}
  [DIAG] 收敛状态枚举（非门禁，不阻塞） distinguishable=True distinct=[0, 1, 2]
  [DIAG]   converged  enum=1(converged) iters=3      obj=0.0121118916393  Δobj/obj=0            tol=0.001    max_iter=300 cap=5000
  [DIAG]   max_iter   enum=0(max_iter ) iters=3496   obj=0.012059161484   Δobj/obj=0.004354     tol=1e-12    max_iter=300 cap=5000
  [DIAG]   stalled    enum=2(stalled  ) iters=1493   obj=39.7455159559    Δobj/obj=3281         tol=1e-15    max_iter=300 cap=5000
[mem_guard] peak_rss_gb=0.795 limit_gb=8.0 label=python3 exit=0
```

退出码由原来的 1（门翻红）变为 **0**：这不是「把门改绿」，而是**退役了一条与质量方向相反的门**，
且**未调任何参数、未改任何阈值**。`gates` 现为 4 条（`W1p`/`W2`/`W3`/`W4`）全绿。

**`max_iterations_cap` 的取值依据**：探针不覆写该键（`upm_probe.cpp:131` 为
`P2UpmBuildConfig cfg{}` 值初始化，其后的字段赋值列表里没有 `max_iterations_cap`），
而 `upm.h:89` 给出默认成员初值 `= 5000` ⇒ 逐次运行的取值确定为 5000。诊断行如实记该值，
并在 `c5_weights.py` 注释里写明这条依据（不是猜测）。

## 19. P2：`o2`/`o3` 场景语义修复（每条分支都要被真正行使）

原场景的语义已被 `36267e1a` 破坏：`o2` 用 `max_iterations=1` 想「强制 max_iter」，
但该键自该提交起是**下界/试跑预算**，1 会被自适应延长；`o3` 原为
`tolerance=1e-12 + smoothing_lambda=50`，实测判为 `converged=1`，并未行使 stalled。

按 `upm.cpp:1649-1670` 的三条判定分支重新构造场景，并用
`results/rerun_1f6d0a64/c5_branch_probe.py`（只扫探针已暴露的 cfg 键）做了
`smoothing_lambda × tolerance` 扫描以确认分支可稳定行使：

| `smoothing_lambda` | `tolerance` | 枚举 | iters |
|---|---|---|---|
| 0 | 1e-9 | 2 stalled | 3497 |
| 0 | **1e-12** | **0 max_iter** | 3496 |
| 0 | 1e-15 | 2 stalled | 3497 |
| 50 / 200 / 1000 / 5000 / 20000 | 1e-9 | 1 converged | 984–988 |
| 50 / 200 / 1000 / 5000 / 20000 | 1e-12 | 1 converged | 1361–1362 |
| 50 / 200 / 1000 / 5000 / 20000 | **1e-15** | **2 stalled** | 1493 |

重复性：对 6 个配置各跑两遍，`enum` 与 `iters` **逐位相同**（稳定），例：
`l0_t1e-12 → 0/0 @3496`、`l1000_t1e-15 → 2/2 @1493`。

**采纳的场景**：

- `converged`：`tolerance=1e-3, tolerance_relative=1, max_iterations=300` → **enum=1**
- `max_iter` ：`tolerance=1e-12, tolerance_relative=1, smoothing_lambda=0.0` → **enum=0**
- `stalled`  ：`tolerance=1e-15, tolerance_relative=1, smoothing_lambda=1000.0` → **enum=2**

**未达项（如实登记）**：`3=invalid`（目标非有限 / 无有效数据项）**未由配置构造出**——
探针已暴露的 cfg 键无法稳定制造非有限目标。不硬凑，登记为未行使。

**硬约束遵守**：本项**没有为了让任何判词变绿而调参**——`o1`（converged 档）的容差未动，
`o2`/`o3` 改的是「行使哪条分支」的场景构造；`W1p`/`W2`/`W3`/`W4` 的判据与阈值一字未改。

## 20. P3：`o3` 与 science 枚举口径逐条比对

规范来源：`docs/science/algorithms/mosaic/PHASE2_UPM_IMPL.md`
「收敛容差与报告字段（现行登记）」一节（第 571–578 行）。逐条对照：

| 规范条目 | 规范要求 | 生产实现（`lib/algorithms/coverage/src/upm.cpp`） | 本次实测 | 判定 |
|---|---|---|---|---|
| 配置面 | 只有 `tolerance`（默认 1e-6）与 `tolerance_relative`（默认 0）；**不存在** `tol_step`/`tol_obj`/`tolerance_obj` | 同（`upm.h:77-81/:113`） | — | 生产自洽 |
| 判据（`tolerance_relative=0`） | `max_dM < tolerance ∧ max_dC < tolerance` | `:1639-1640` | — | 生产自洽 |
| 判据（`tolerance_relative=1`） | `max_dM < tolerance·max(scale_obs,1) ∧ max_dC < tolerance·max(scale_obs,1)` | `:1642-1644`（`s_eff = max(scale_obs,1.0)`） | — | 生产自洽 |
| `scale_obs` 定义 | `median(\|value\|)` | `:1140-1148`（`nth_element` 取 `size/2`） | — | 生产自洽 |
| `1=converged` 成立条件 | 「`tol_step` 与 `tol_obj` **同时**满足」 | `:1659`：`max_dM < tol_M && max_dC < tol_C` | `converged` 档 → enum=1 | **生产自洽**（按规范正文的 `max_dM/max_dC` 口径）；但规范正文用词与 `upm.h:166` 注释的 `tol_step/tol_obj` 不一致，见下 |
| `2=stalled` 成立条件 | `\|Δobj\|/max(\|obj_old\|,1e-300) < 1e-12` 连续 5 轮 | `:1150-1151`（`kStallPatience=5`、`kObjImproveFloor=1e-12`）、`:1661-1670` | `stalled` 档 → enum=2 | 生产自洽 |
| `3=invalid` 成立条件 | 目标非有限 | `:1655-1656` | 未行使 | 未行使（见 §19） |
| `0=max_iter` 成立条件 | 迭代耗尽未达容差 | 循环自然落出（`:1716` 之后） | `max_iter` 档 → enum=0 | 生产自洽 |
| 枚举定义 | `0=max_iter / 1=converged / 2=stalled / 3=invalid` | `upm.h:294-296`、`:445` | `distinct=[0,1,2]` | 生产自洽 |
| 访问器 | `p2_upm_convergence` 只读、不写任何出参（null model） | `:2703-2713` | 三例均正常返回 | 生产自洽 |

**结论：生产自洽，无缺陷线索。** `o3` 判为 `converged=1` 与规范不矛盾——规范把 `1` 定义为
「步长判据同时满足」，而该场景在 1493 轮确实满足了步长判据（其目标函数已在平台上，
`Δobj÷obj ≤ 9.7e-6` 是对容差放宽的响应，与步长判据是两条独立通道）。原 `o3` 之所以
「看起来该是 stalled」，是因为旧场景的**设计意图**而非规范要求；按 §19 重设场景后
`2=stalled` 可被稳定行使。

**顺带登记两处非行为性不一致（文档/注释层面，不改生产实现）**：

1. `lib/algorithms/coverage/include/astro/phase2/upm.h:166` 的注释写
   「`1 = converged（tol_step 与 tol_obj 同时满足）`」，而规范明写**不存在**
   `tol_step`/`tol_obj` 字段、实际量是 `max_dM`/`max_dC` 对 `tol_M`/`tol_C` ⇒ 注释用词陈旧。
2. 规范同节引用的行号 `upm.cpp:1028-1052`（枚举）与 `:1577-1589`（访问器）**已漂移**：
   当前实际位置是 `:1649-1670` 与 `:2703-2713`。

## 21. 仍未收敛（照旧登记）

- **`P5_订正` rerun_HEAD（`6808a7e4`）与 baseline 的 `A6_subset_invariance` /
  `A9_final_gauge_noop` 判词翻转**：仍未裁决。
- **`3=invalid` 分支未被行使**：需目标非有限，配置面无法稳定构造。
- **`upm.h:166` 注释用词**与**规范行号漂移**：属文档/注释订正，未在本轮授权内。

## 22. 裁决：`A6_subset_invariance` / `A9_final_gauge_near_noop` 的判词翻转

本节**关闭**此前在 §17 与 §21 登记为「仍未裁决」的那一项。

### 22.1 判据表达式的原句（文件:行）

两版读数都用**旧判据面**（`A6`/`A9`），取自 `6808a7e4:实验/additive-sky-seamless/code/c1_additive.py`：

| 判据 | 量 | 阈值 | 原句位置 |
|---|---|---|---|
| `A6_subset_invariance` | `ua["full_4elem"]["subset"]["max_dev"]`（= `subset_dev(world, corr)["max_dev"]`） | `max_dev < 0.05` [e⁻] | `c1_additive.py:245-247`；`subset_dev` 定义在 `:71-92` |
| `A9_final_gauge_near_noop` | `dev_fg = float(np.max(np.abs(c0 - corr_store["full_4elem"])))`，即 `max\|C(final_gauge=0) − C(final_gauge=1)\|`（同 `CFG_FULL`） | `dev_fg < 0.01` [e⁻] | `c1_additive.py:271-279`（`dev_fg` 在 `:273`，`g.add` 在 `:276`） |

臂配置 `CFG_FULL = dict(UPM_BASE, gs_damping=0.5, m_full_frame=1, final_gauge=1,
tolerance=1e-3, tolerance_relative=1)`（`:40-43`）——注意 **`tolerance_relative=1`**。

两条阈值都是**绝对**口径（e⁻）；A9 的判据描述文字却写「`< 0.01 e- ≈ 3e-5 相对`」，
即阈值原本是按「观测尺度 ~300 e⁻ 的 3e-5」标定的（`0.01 / 300 ≈ 3.3e-5`）。

### 22.2 两版读数

两套读数：`run/FINAL-07/审核包/科研审查/P5_订正/baseline/c1_additive.json`（blob md5 `c44765ce…`，
对应提交 **`9a2b5d11`**，2026-09-21）与同目录 `rerun_HEAD/c1_additive.json`（blob md5 `b76c33e3…`，
HEAD = **`6808a7e4`**，2026-09-28 22:59 复跑）。

| 判据 | `9a2b5d11` 版 | `6808a7e4` 版 | 判词 |
|---|---|---|---|
| `A6_subset_invariance` | **0.02361920603851786** | **0.4077910086047609** | PASS → **FAIL**（×17.3） |
| `A9_final_gauge_near_noop` | **0.0037192998514683495** | **0.13271928566723545** | PASS → **FAIL**（×35.7） |
| `A8b`（同源 A6 量） | same 0.02361920603851786 / diff 0.02317449298794827 | same 0.4077910086047609 / diff 0.4001129358054527 | PASS（比值不变） |

同一 JSON 里的逐臂读数（这是定位根因的关键）：

| 臂 | `9a2b5d11` iters / subset max_dev | `6808a7e4` iters / subset max_dev | `max_abs_C` |
|---|---|---|---|
| `legacy_alpha1_mff0_fg0`（**绝对**容差） | 300 / **0.0164212** | 300 / **0.0164212** | 8.20032 → 8.20032（**逐位相同**） |
| `legacy_rel_tol`（相对容差） | 4 / 0.0164327 | 3 / 0.0164327 | 8.43919 → 8.44026 |
| `damp_only` | 11 / 0.0227896 | 6 / **0.219665** | 8.60594 → 8.40858 |
| `full_4elem` | **18** / **0.0236192** | **5** / **0.407791** | 8.30922 → 8.03426 |

**关键事实**：用**绝对**容差的那条臂在两版之间**逐位相同**（300 轮、0.0164212、`max_abs_C` 8.20032）
⇒ 世界、单位、数据与绝对判敛路径在该窗口内**没有变**；变的只有**相对**容差路径，
且相对容差的臂**迭代数显著减少**（`full_4elem` 18 → 5，`damp_only` 11 → 6）。

### 22.3 候选改动清单 + 证据

窗口 `9a2b5d11..6808a7e4` 共 318 个提交。与这两条判据相关的候选：

| 路径 | 窗口内提交 | 判定 |
|---|---|---|
| `实验/…/code/c1_additive.py` | 仅 `bb508dea`（修根目录整合引入的**脚本路径回归**） | **非候选**：只改路径，未动 A6/A9 表达式与阈值 |
| `实验/…/code/sci_c_common.py` | 仅 `bb508dea`（同上） | **非候选** |
| `lib/algorithms/coverage/src/upm.cpp` | **`d77fd11f`**、`7cde39cb`、`a22b3890`、`139a2bc4`、`e44652c4` | **候选** |
| `lib/algorithms/coverage/include/astro/phase2/upm.h` | 同上 + `eccf07e6`、`19867470` | 候选 |

**根因提交：`d77fd11f`**（2026-09-22 01:35:32）
「SCI-502：UPM 三个生产缺陷修复（相对容差 / 四态收敛 / 近奇异自适应）」。
是 `6808a7e4` 的祖先、且不在 `9a2b5d11` 之内 ⇒ 落在窗口内。其 FIX-1 的 diff 要点：

```diff
-            double scale_M = 0.0, scale_C = 0.0;
-                scale_M = std::max(scale_M, std::fabs(M[k]));
-                    scale_C = std::max(scale_C, std::fabs(m->C[f][k]));
-            tol_M = cfg.tolerance * std::max(scale_M, 1.0);
-            tol_C = cfg.tolerance * std::max(scale_C, 1.0);
+            const double s_eff = std::max(m->scale_obs, 1.0);
+            tol_M = cfg.tolerance * s_eff;
+            tol_C = cfg.tolerance * s_eff;
```

提交说明原文：「相对判据的分母改为**观测量稳健尺度** scale_obs（`|value|` 中位数），
**不再用 max|M|**（`11_upm.md` §4.6 明令）」——即该改动是**规范明令的订正**，
不是失误。分母由「场量级」（≈1，故有效绝对容差 ≈ `tolerance`）改为「观测量量级」
（该合成世界天光 ~300 e⁻）⇒ 同一 `CFG_FULL`（`tolerance=1e-3`）的**有效绝对容差变松约
`scale_obs` 倍（≈300×）**，求解更早停，C 场更不收敛。

**排除其它候选**：`legacy_alpha1_mff0_fg0` 臂逐位相同（§22.2）说明单位口径提交
`7cde39cb`（ADU/sr）等**未**影响本测量；`eccf07e6`「收敛枚举与门槛清理收尾」只动枚举/门槛登记，
不改相对容差分母（分母改动只出现在 `d77fd11f`）。

### 22.4 最小判别实验

脚本 `results/rerun_1f6d0a64/c1_a6a9_attrib.py`（只读复用 `code/` 的助手，不新建判据、
不改阈值），读数 `results/rerun_1f6d0a64/c1_a6a9_attrib.json`。判别思路：把 `full_4elem`
臂的 `tolerance_relative` 关掉（回到绝对口径）扫 `tolerance`，看 A6/A9 是否在足够紧的
绝对容差下回到基线量级。

| 配置 | conv | iters | A6 max_dev | A6 | A9 diff | A9 |
|---|---|---|---|---|---|---|
| `CFG_FULL`（现行，相对 1e-3） | 1 | **5** | **0.407791** | **FAIL** | **0.132719** | **FAIL** |
| 绝对 1e-3（= 旧分母下的有效口径） | 1 | 25 | 0.0164221 | PASS | 0.000535339 | PASS |
| 绝对 1e-4 | 1 | 1432 | 0.0164107 | PASS | 9.98734e-05 | PASS |
| 绝对 1e-5 | 1 | 3738 | 0.0164074 | PASS | 9.98791e-06 | PASS |
| 绝对 1e-6 | 0 | 5000 | 0.0164071 | PASS | 2.83283e-06 | PASS |
| 绝对 1e-7 | 0 | 5000 | 0.0164071 | PASS | 2.83283e-06 | PASS |
| 相对 1e-5 | 1 | 21 | 0.0178601 | PASS | 0.00162168 | PASS |
| 相对 1e-6 | 1 | 327 | 0.016418 | PASS | 0.000301039 | PASS |

**读数表明**：绝对 1e-3（旧分母下的有效口径）下 A6 = **0.0164221**，与基线里绝对容差臂的
**0.0164212** 同量级；继续收紧则 A6 **收敛到 0.016407**、A9 **收敛到 ~1e-6** 并**稳定不再变**
⇒ 两条量在**充分收敛**下都健康，没有证据指向场本身出错。相对口径把 `tolerance` 收紧同样能恢复。

### 22.5 裁决

**(b) 判据口径需更新。**

机制：`d77fd11f`（规范明令的订正）把相对容差分母由 `max(scale_M/scale_C, 1)` 改为
`max(scale_obs, 1)`。`CFG_FULL` 带着 `tolerance=1e-3, tolerance_relative=1`，在旧分母下
**实际退化为绝对 1e-3**（因为场量级 ≈1），在新分母下变成 `1e-3 × ~300 ≈ 0.3` 的绝对阈值
⇒ 求解由 18 轮缩到 5 轮。两条判据的**绝对**阈值（0.05 e⁻ / 0.01 e⁻）是在**旧的有效收敛深度**
下标定的，前提（「该 cfg 下解已充分收敛」）被**正确地**改掉了，故判词翻转。

**不是 (a)**：充分收敛下 A6 稳定在 0.016407、A9 稳定在 ~1e-6（表 §22.4），且绝对容差臂
在两版之间逐位相同（§22.2）——没有引入错误行为的机制，也没有可复现的缺陷读数。

**该改哪份文档的哪一条**（本轮只登记，不改文档）：

1. **`README.md` §11.3** 的「`A6_subset_invariance` / `A9_final_gauge_near_noop`」那一行
   （旧形态 → 新形态表里的一条）：应补记本次裁决的根因——旧 `A6`/`A9` 的**绝对**阈值
   在 `d77fd11f` 之后不再对应同一收敛态；正确的口径是**相对**（A9 判据文字里的
   「≈ 3e-5 相对」正是这个意思，但实现成了绝对 0.01 e⁻）。
2. **`README.md` §4.1** 表下的「读数来源」注（上一轮所加）：可补一句根因指向 `d77fd11f`。
3. **`docs/science` 侧无需改**：`PHASE2_UPM_IMPL.md`「收敛容差与报告字段」一节规定的
   `scale_obs` 分母是本次翻转的**依据**，规范本身正确。

### 22.6 仍未收敛

- 上面 22.5 列的两条 README 位置**本轮未改**（写面不含 README），待派单。
- `A6`/`A9` 在新判据面（`A6p`/`A9p`/`A6_subset_invariance_scale`）下的阈值是否也应改成
  相对口径：`A6p` 已用 `2·σ_control` 的**相对**口径（见 `c1_additive.py:291-308`），
  `A9p` 用求解器自声明的 `tol_M`（同样是当次口径）⇒ 新判据面**已**规避该问题；
  旧判据面的阈值无需再改，只需在文档里记清根因。

## 23. 归因脚本的留存位置

§22.4 的归因脚本原先只落在 `results/rerun_1f6d0a64/c1_a6a9_attrib.py`。`results/` 在
`.gitignore` 内（`/实验/**/results/`）⇒ **不会被入库**，而按 `AGENTS.md` §13.2
「实验代码一并保留」，脚本属必须保留的过程物。故已**复制**（不是移动）一份到：

- **`code/c1_a6a9_attrib.py`** —— 随单元入库的留存位置（脚本正本所在）；
- `results/rerun_1f6d0a64/c1_a6a9_attrib.py` —— 原位置，**原样保留**，其读数
  `results/rerun_1f6d0a64/c1_a6a9_attrib.json` 亦未改动。

两份的差别只有三处、且都**不涉及判据、阈值、扫描配置与测量逻辑**：

1. 只读助手目录的定位由 `parents[2]/"code"` 改为**向上搜索**含 `sci_c_common.py` 的目录
   （原写法从 `code/` 运行会指错目录；改后两处位置都能运行）；
2. 输出文件名加 `_codecopy` 后缀、并支持 `C1_A6A9_OUT` 覆盖，避免覆盖既有读数；
3. 删去两个**未被使用**的符号（`UPM_BASE` 的 import、局部变量 `wts`），纯死码清理。

**可运行性已冒烟确认**（从 `code/` 位置运行，输出重定向到 `/tmp`，**未写仓库内任何文件**）：

```
$ C1_A6A9_OUT=/tmp/c1_a6a9_smoke.json python3 code/c1_a6a9_attrib.py
== A6/A9 翻转的最小判别实验（阈值 A6<0.05  A9<0.01）==
  配置                             conv  iters  A6 max_dev  A6 ok       A9 diff A9 ok       max_abs_C
  CFG_FULL（现行，相对口径）              1     5      0.407791    False       0.132719    False  8.03426
  绝对口径 tolerance=0.001           1     25     0.0164221   True        0.000535339 True   8.30643
  …
```

读数与 §22.4 原先的一次运行**逐位相同**，即复制未引入任何行为差异。
