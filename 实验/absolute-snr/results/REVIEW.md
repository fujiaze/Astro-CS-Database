# SCI-B 独立审稿记录

> **审稿人**：独立审稿子代理（非作者，零 git 写权限，未修改本单元以外的任何文件）
> **审稿对象**：`实验/absolute-snr/`（跨帧绝对 SNR 传递链，SCI-402）
> **审稿时刻**：2026-09-21T05:20Z（CST 13:20）
> **被审版本（md5，快照于 05:19–05:20Z）**

| 文件 | md5 |
|---|---|
| README.md | `88d3a1e21ec9d22dd9ff066704be5013` |
| code/sci_b_common.py | `487492282d475161bdcace33b9d0641d` |
| code/b3_domain_map.py | `d0c12c0e7aa62206b11daf1e3bb1240a` |
| code/b4_integration.py | `dd045789d58a1a34d7ab65f062c4a2f2` |
| code/b6_gates_audit.py | `8d6a20ac859cb212d2dc0e34539c09ec` |
| results/b1_sky_scan.json | `e6effb9f157ae1ea748889639744bd25` |
| results/b2_noise_terms.json | `3b5fa2ed34084c089337a527bd853099` |
| results/b3_domain_map.json | `e80bbfa759bca0e1596e9fb238a13336` |
| results/b4_integration.json | `eaa47c0e6e71e98ada12684e733f654e` |
| results/b5_phase3_transfer.json | `27e031f3db000fb6dc2ef74a8230fb23` |
| results/b6_gates_audit.json | `78ad1d1ee6f5d5e1a35243650ee4651d` |
| results/DOC_CORRECTIONS.md | `cd9297c6a9ee169acf45cf0587426294` |

> ⚠ **审稿过程本身的条件**：本单元在我审稿期间被**作者代理持续改动**（13:01 新增 DOC_CORRECTIONS.md；13:04/13:08 改 PSF_SIGNAL_WEIGHT.md / CONTROL_WEIGHT_SNR.md；13:09 改 b6 源码并加 `--self-test`；13:09/13:12 重跑 b1/b2/b3/b4/b5/b6；13:19 再改 README）。上表是我核对结论时依据的版本；**任何"可复跑复现"的结论都只对该版本负责**。见 §3 次要问题 M12。

---

## 1 审稿范围与方法

### 1.1 实际执行的命令（全部只读；唯一写入 = 本文件）

```bash
# ① 备份 + 版本快照（不改仓库）
cp -a 实验/absolute-snr/results /tmp/SCI-B-backup/results
md5sum 实验/absolute-snr/results/*.json

# ② 可复现性抽检（实跑；脚本会覆写 results/*.json）
cd 实验/absolute-snr && python3 code/b6_gates_audit.py          # 0.25 s
cd 实验/absolute-snr && python3 code/b5_phase3_transfer.py      # 3.5 s
python3 code/b6_gates_audit.py --self-test               # 新增自检，exit 0，9/9 PASS
# 语义比对（去掉 generated_at/wall_s/elapsed_s）：
diff <(python3 -c "import json;d=json.load(open('/tmp/SCI-B-backup/results/b6_gates_audit.json'));d.pop('generated_at');d.pop('wall_s');print(json.dumps(d,indent=1,sort_keys=True))") \
     <(python3 -c "import json;d=json.load(open('results/b6_gates_audit.json'));d.pop('generated_at');d.pop('wall_s');print(json.dumps(d,indent=1,sort_keys=True))")

# ③ 数值自洽性独立复算（自写脚本，不 import 实验单元代码；/tmp/verify*.py）
python3 /tmp/verify_sci_b.py    # SNR 比值、斜率、dense 字节、H4 比值、c_est、B5 比值
python3 /tmp/verify3.py         # 双计读噪闭式预言 vs 实测臂
python3 /tmp/verify5.py         # b4 part_e 量纲端到端 MC
python3 /tmp/verify6.py         # EPS_REF 与 B6 "电平偏差"语义
python3 /tmp/verify9.py         # b3 边界 / HST bias / part_c 预言
python3 /tmp/verify10.py        # b3 四臂齐全性
python3 /tmp/verify11.py        # 复现 b3 缺失 sparse 行
python3 /tmp/verify12.py        # 定位 sigma_field_fast NaN 机制
python3 /tmp/verify13.py        # 修复估计量后重评 b3 边界（monkeypatch，不改仓库）
python3 /tmp/verify14.py        # 量化 NaN bug 对 dense 臂/s_field/ℓ 的影响
python3 /tmp/verify15.py        # 用 README 的 eps_ref 重算校正 RMSE

# ④ 生产码只读核对
awk 'NR>=149 && NR<=238' lib/algorithms/noise_snr/cpp/src/snr_science.cpp
git show HEAD:lib/infrastructure/scheduler/src/module_adapters.cpp | awk 'NR>=4250 && NR<=4272'
awk 'NR>=65 && NR<=88' lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp
grep -rn "noise_sigma" lib/ | head
awk 'NR>=78 && NR<=96' docs/science/NOISE_MODEL.md
grep -rn "sparse_reconstruct\|snr_path_effective\|FZ-SNR" lib/ lib/include/ eng/contracts/

# ⑤ 越权核查
git status --short ; git diff --stat ; git status --short docs/research/ docs/science/ lib/
```

### 1.2 未执行但做了静态审查的代码
`b1_sky_scan.py`、`b2_noise_terms.py`、`b3_domain_map.py`、`b4_integration.py`、`b5_phase3_transfer.py`、`b6_gates_audit.py`、`sci_b_common.py` 全文逐行读；`b1/b3` 的关键分支另做了**独立复现实验**（见 §3 B1、I1）。

---

## 2 逐项核验结论

| 条目 | 结论 | 证据（我自己跑出来的） |
|---|---|---|
| **1 可复现性**：b6 重跑 | **PASS** | 重跑与备份逐字段相同（仅 `generated_at`/`wall_s` 变）；`diff` 除时间戳外**零差异** |
| **1 可复现性**：b5 重跑 | **PASS** | 同上；且作者 13:12 的第三次重跑仍与备份逐字段相同 ⇒ 三次独立运行同值 |
| **1 可复现性**：b1/b2/b4/b3 | **PASS** | 作者重跑后我做了语义 diff：b1（05:09）、b2（05:09）、b3（13:12）、b4 均**仅时间戳/耗时字段变化**，全部科学量逐位相同 |
| **1 无 facade / 硬编码真值 / 跳过分支** | **部分 FAIL** | b1/b2/b4/b5/b6 无硬编码真值；**b3 存在静默跳过分支**（`b3_domain_map.py:141-142` `if m.sum()<16: continue`）并已实际触发 → §3 **B1** |
| **2 判据非退化**：`FINDING_doublecount_*` 为 False 是否"预期为红" | **PASS（真发现，非改名掩饰）** | 闭式预言独立复算：B1 亮源 B=0 预言 **+12.62%** vs 实测臂/定义式 **+12.61%**；暗源预言 **+36.90%** vs 实测 **+36.86%**；B2 `G4c_pred_vs_arm_ratio_max_abs_diff=0.0125`。生产码链路亦证实（`module_adapters.cpp:4258@HEAD` 填经验总 rms + `snr_science.cpp:171-200` 再加 `(RN/g)²`；`noise_sigma` = `estimate_background` 的裁剪后 RMS，含读噪） |
| **2 恒真门清点** | **FAIL** | 至少 8 个门在任何数据下恒真：`N1_flat_field_sparse_never_wins`（帧级 RMSE **精确为 0**，实测 0.000e+00 ×5 面 ⇒ "sparse<0" 不可能）、`b1 G1_strict_monotone_def`、`b4 H1_identity_lt_1e-12`、`b4 H5_identity_lt_1e-12`、`b4 H4_equal_leverage_loss_zero`、`b4 H5_mismatch_zero_spread_zero_loss`、`b6 H3_e11_dex_constant_overestimates`（因子按构造 = ln10）、`b6 H4_all_cases_match`（自指玩具状态机）→ §3 I2/I3/I5 |
| **2 恒假门** | **PASS（未发现）** | 逐门检查 6 个 JSON 的全部 gates：不存在"永远为假、靠命名假装发现"的项；B1 的 2×2 个 `FINDING_doublecount_*` False 有独立证据支撑 |
| **3 数值自洽**：SNR(10⁶)/SNR(0) | **PASS** | 复算 0.9130772/43.319582 = **0.0210777**（亮）、0.0304423/2.8436633 = **0.0107053**（暗）= README 2.11%/1.07% |
| **3 数值自洽**：斜率 −0.4879/−0.4972 | **PASS** | 用 B≥3000 段重做 log-log 最小二乘：**−0.4879107345007782 / −0.49719839095455826**，与 JSON 逐位相同 |
| **3 数值自洽**：dense 存储 67,108,864 B | **PASS** | 4×4096² = 67,108,864 B = 64 MiB = 67.11 MB；= 1 MiB 预算 64 倍 |
| **3 数值自洽**：H4 naive/optimal 1.523 vs 1.504 | **PASS** | 用 JSON 的 4 个杠杆 h 独立重算预言 = **1.504406285804784**（与 JSON 逐位同）；实测 1.5225715 |
| **3 数值自洽**：B6 c_est 因子 2.303 | **PASS** | 0.045/(1.44/ln10/√1024) = **2.302585092994046 = ln10**；实测相对 SE 0.036229 vs 1.166/√N=0.0364375；dex 实测 0.015760 ≈ 0.036229/ln10=0.015734 |
| **3 数值自洽**：B5 1.373 vs 1.389 | **PASS** | 1+0.75·ρ_out = 1+0.75×0.51909103 = **1.389318271478706**（JSON 逐位同）；实测 1.3729973 |
| **4 科学正确性**：σ_F 定义、天光只进噪声、F_ref 锚定、ΣSNR²、Q/W、C_out | **PASS** | σ_F⁻²=ΣP_i²/σ_i²（Horne 1986）实现正确（`sci_b_common.py:175-181`，与生产 `snr_science.cpp:191-200` 同式）；天光只在 `var_i`；`w=SNR(F_ref)²/F_ref²≡1/σ_F²` 复算偏差 2.2e-16；ΣSNR² 为代数恒等（成立）；Q/W 与 C_out=R C_in Rᵀ 是标准式 |
| **4 量纲（重点核查 b4 part_e）** | **PASS（无错）** | `b4_integration.py:288` `(sky+DARK+rn²+f_ref·g·P)/g²` ≡ 正确式 `(sky+DARK+rn²)/g² + f_ref·P/g`（ADU²），两式**逐位相同**；自建端到端 MC（F_e=f_ref·g 电子域 Poisson + 高斯读出，n=8000）：code/MC−1 = **−0.04% / +0.15% / +0.05%**，在 MC 噪声内 ⇒ 量纲正确 |
| **5 越权与边界** | **PASS（有说明）** | `docs/research/SNR_WEIGHT_RESEARCH_PACK.md` **零改动**（`git status` 空、`git diff` 空）；`docs/science/PSF_SIGNAL_WEIGHT.md`/`CONTROL_WEIGHT_SNR.md` 有改动，但**属 SCI-402 允许改的文件域**（新增 §7a/§8a 定案结论，纯追加）；SCI-B 全部脚本的写目标只有 `results/`（`C.save_json`/`savefig`）与 `run/SCI-402/`（日志）——**未发现 SCI-B 写生产码或科学文档**。生产码 `lib/algorithms/noise_snr/*`、`module_adapters.cpp` 的改动来自并发 FIX-405/DOC-402（见 §4） |
| **6 与 EXP-205 冲突（README §8）** | **存疑** | 结论**方向正确但我复核后认为"未被所交证据充分建立"**：`s≥0.03 时 Δ 到 512 仍胜` 中，s=0.03（5 面）与 s=0.10 ℓ=256 有真实数据支撑；**其余 9 个 (面,Δ=512) 组合的 sparse 行是缺失的**，`delta_star=None` 由缺数据产生。我用修正估计量重算后结论**仍然成立**（sparse 0.2294 vs frame 0.2307 等），但**所交 JSON 不支持该主张** → §3 B1/I4 |

---

## 3 发现的问题

### 阻断（必须修复后才可采信 b3 的结果表）

#### **B1｜`sigma_field_fast` 的 NaN 传播使 b3 大面积丢格、并在大 Δ 上整臂消失；缺失被当成"未出现边界"**

- **位置**：`code/sci_b_common.py:284-295`（`finite_all` 只按输入判定一次，`medf=np.median` 在第二轮遇到第一轮裁剪产生的 NaN 时返回 NaN ⇒ 该 cell 全灭）；受害者 `code/b3_domain_map.py:132-142`（`if m.sum() < 16: continue` **静默丢弃整条臂**）与 `code/b3_domain_map.py:167-182`（`delta_star()`：`sp is None` ⇒ 不判定交叉 ⇒ 返回 `None`）。
- **复现命令**：
  ```bash
  cd /tmp && python3 /tmp/verify12.py    # 打印 round0 保留 98%、round1 保留 0.0000
  cd /tmp && python3 /tmp/verify11.py    # 打印 cell_finite=0/256, est_finite=0/4096, m.sum=0
  cd /tmp && python3 /tmp/verify10.py    # 列出 13 个 (面,Δ) 组合缺 sparse 臂
  ```
- **期望 vs 实际**：
  - 期望：每个 (面,Δ) 四条臂齐全，或缺失被显式记录并判红。
  - 实际：`b3_domain_map.json` 中 **13 个 (面,Δ) 组合没有 sparse 行**（s=0.30 的 ℓ=16/32 在 Δ≥128、ℓ=64/128/256 在 Δ≥256/512；s=0.10 的 ℓ=16/32/64/128 在 Δ=512；HST 在 Δ=512），JSON 中**没有任何字段记录这次跳过**；`delta_star_px=None` 因此同时表示"确实没交叉"和"根本没算"。
  - dense 臂同样受害：`s=0.30, ℓ=16` 只剩 **1321/4096 = 32.3%** 的 cell，其 `dense` RMSE 报 **0.0258**，用修正估计量重算是 **0.0791**（乐观 **3.1×**）；`ℓ=32` 75.2%（0.0234 → 0.0373）；HST 96.2%（0.0896 → 0.0940）。`s_field`（0.2185 → 0.2314）与 `ℓ_meas`（23.0 → 26.6）同样被偏。
- **后果与我的独立重评**（`/tmp/verify13.py`：进程内 monkeypatch 成 nan-aware 归约，**不改仓库**）：
  ```
  s=0.30 ℓ=16  orig: D64=0.2304 D128/256/512=缺失   fixed: D64=0.1899 D128=0.2193 D256=0.2279 D512=0.2294 (frame=0.2307)
  s=0.30 ℓ=32  orig: D64=0.2616 D128+=缺失          fixed: D64=0.1593 … D512=0.2721 (frame=0.2770)
  s=0.30 ℓ=64  orig: D64=0.2003 D256+=缺失          fixed: D64=0.0815 … D512=0.2852 (frame=0.2934)
  s=0.30 ℓ=128 orig: D256=0.2999 D512=缺失          fixed: D512=0.2513 (frame=0.2999)
  s=0.30 ℓ=256 orig: D512=缺失                      fixed: D512=0.1937 (frame=0.2998)
  HST          orig: D512=缺失                      fixed: D512=0.1773 (frame=0.0485, Δ*=32 不变)
  ```
  ⇒ **§8/D4 的"无源污染合成面未复现 Δ/ℓ≈1 边界"这一结论在修正后仍然成立**（sparse 始终 < frame，但 ℓ=16 处余量只有 0.6%），**但报告里的数字与"0 次交叉"的判据是建立在缺失数据上的**——这正是 AGENTS.md §9「SKIP 充数 / 检查器静默退化」所禁止的形态。
- **连带影响**：`results/b3_domain_map.json` 中受影响面的 `at_delta64`、`delta_star`、`ell_px`、`s_field_log10_std` 全部需要重算；README §3.3 表里"s=0.30 行"的 dense/sparse 数值、§8 表、DOC_CORRECTIONS D4 都要重写。
- **修复方向**：`sigma_field_fast` 每轮都用 nan-aware 归约（或按轮重判 finite）；`eval_arms` 缺臂时写入 `skipped=true + reason` 并让 `delta_star` 区分"未交叉"与"未计算"（缺臂必须判红）。

---

### 重要

#### **I1｜eps_ref 代码与报告不一致 2.846 倍，且代码用的是本单元自己批判过的单位错误**

- **位置**：`code/b3_domain_map.py:100` `EPS_REF = 1.44/np.sqrt(512)` = **0.063640 dex**；README:63 与 README:173 写 `eps_ref=0.506/√512=0.0224 dex`。
- **复现**：`python3 /tmp/verify15.py`
- **期望 vs 实际**：正确 dex 常数 = 1.166/ln10/√512 = **0.022362**（README 的 0.506 就是这个）；代码用 1.44 直接当 dex（漏 /ln10，且用保守的 1.44 而非 1.166），偏大 **2.846×**。这正是 B6 判 EXP-205 有罪的**同一类单位错误**。
- **后果**：§3.3 表括号里的"扣除真值噪声后的值"全部来自这个偏大的 eps_ref：
  | 面/臂 | rmse | 现报告 corr(0.0636) | 用 README 的 0.0224 重算 |
  |---|---|---|---|
  | M42 M1 sparse | 0.0440 | **0.0000** | **0.0379** |
  | M42 M2 sparse | 0.0825 | 0.0525 | **0.0794** |
  | M42 M4 sparse | 0.0413 | **0.0000** | **0.0347** |
  | HST sparse | 0.0994 | 0.0764 | **0.0969** |
  ⇒ §5.2「M1/M4 的校正后 RMSE 触零（估计量已达真值噪声地板）」是**由错误的 eps_ref 造成的假象**；真实结论应为"仍高于真值噪声地板"。

#### **I2｜b6 的 fail-closed "对拍"是自指玩具，无被测对象**

- **位置**：`code/b6_gates_audit.py:99-145`（`path_state_machine` 定义在 b6 内部，用例与期望值同表硬编码）。
- **复现**：`grep -rn "sparse_reconstruct\|snr_path_effective\|FZ-SNR" lib/ lib/include/ eng/contracts/` ⇒ 命中仅 `lib/infrastructure/cli/session_commands.h`（模板字符串）、`eng/contracts/schemas/phase_config_mosaic.schema.json`（description），**lib/ 实现代码中没有任何路径状态机**。
- **期望 vs 实际**：期望"对拍"= 与独立实现/真实调用路径比对；实际是作者把自己的转写与自己的期望表比对，`H4_all_cases_match` **在任何数据下恒真**（只有改源码才可能失败）。语义转写与 `07_noise_snr.md` §4.2 一致（这点我核对了，一致），但它**不能支撑 README §3.6「fail-closed 路径对拍」与 H14「语义正确」的强度**——应表述为"按规范转写的语义自检，实现侧尚未落地"。

#### **I3｜`N1_flat_field_sparse_never_wins` 是恒真门，却被计入"非退化负例 PASS"**

- **位置**：`code/b3_domain_map.py:277-289`。
- **复现**：`python3 -c "import json;j=json.load(open('实验/absolute-snr/results/b3_domain_map.json'));print(j['gates']['N1_flat_field_sparse_never_wins'])"`；再查 5 个 s=0 面的 `frame_median.rmse_log_rho` ⇒ 全部 **精确 0.000000**。
- **期望 vs 实际**：s=0 时真值场恒为常数 1.0 ⇒ 常数臂中位归一后 RMSE ≡ 0 ⇒ `sparse < 0` 在数学上不可能，**任何数据都不可能让它翻红**。报告在 README:124 脚注与 DOC_CORRECTIONS D3 已**诚实承认**该场景判据退化，但 README:266 附录 B 仍把它列为通过的门、README §4"非退化负例 PASS"仍以"平坦场归零"作为四处归零之一 ⇒ **同一文档内自相矛盾**。

#### **I4｜§8「未复现 Δ/ℓ≈1 边界」未被所交 JSON 支持（详见 B1）**

- **位置**：README:128/229；`results/b3_domain_map.json` → `faces.synthetic_grf[*].delta_star`。
- **复现**：`python3 /tmp/verify10.py`（列出缺臂组合）；`python3 /tmp/verify13.py`（修正后重评）。
- **期望 vs 实际**：期望"20 个合成面上 Δ 到 512 都未见交叉"是**算出来的**；实际有 9 个 (面,Δ=512) 根本没算。修正后结论仍成立（所以我判"结论对、证据不足"而非"结论错"），但**报告不应把缺失计为阴性**。

#### **I5｜b4 `part_c` 的等权解析预言错了 K=3 倍**

- **位置**：`code/b4_integration.py:168` `var_equal_pred=float(np.mean([1/Ws[c["name"]] for c in cfgs]))`（3 帧平均的方差应为 `mean(σ²)/K`，漏除 K）。
- **复现**：`python3 -c "import json;c=json.load(open('实验/absolute-snr/results/b4_integration.json'))['part_c'];s=[f['sigma_f'] for f in c['frames']];import numpy as np;print('JSON pred',c['mc']['var_equal_pred'],'正确 pred',np.mean(np.square(s))/3,'实测',c['mc']['var_equal'])"`
  ⇒ JSON pred **11494.99**、正确 **3831.66**、实测 **3953.29**。
- **期望 vs 实际**：同一 JSON 里 `var_qw_pred`（1260.41 vs 实测 1275.12）与 `var_single_pred`（1794.26 vs 1751.09）都对，只有等权预言差 3 倍且无门把关；README §3.4 新版已改成只讲"等权 3953.29（3.1×）"而不再引用该预言，但机器可读结果里仍留着错值。

#### **I6｜附录 B 的 ctest "6/6 passed" 与它自己引用的日志矛盾**

- **位置**：README:269（"6/6 passed … 日志 `run/SCI-402/ctest_snr.log`"）。
- **复现**：`tail -25 run/SCI-402/ctest_snr.log`
- **期望 vs 实际**：日志实际是 `5/6 … p1noise_numpy_oracle ***Failed`（并发 FIX-405 把 `noise_model.cpp` 改到不可编译中间态）。README §5.10 已诚实披露这次红属并发生产码、非本单元回归；但附录 B 仍写 6/6 并指向该日志 ⇒ 内部不一致，验收附录不应保留已被自己推翻的 PASS 句。

---

### 次要

- **M1｜"算术常数"负例近乎恒零**：`b1_sky_scan.py:136-143` 对全帧 +200 ADU 后**重新估背景并相减**，对任何"扣局部背景"的估计量都是解析恒等变换（中位平移协变、MAD 平移不变）⇒ ΔSNR 只能是浮点舍入（实测 6.7e-16/4.4e-16）。它被列在 §2.3「真值无效应⇒归零」与 §4「非退化负例 PASS」里，但判别力接近 0；建议改用"不重估背景的常数偏置"或"局部背景估不到的空间变化"作负例。
- **M2｜定义型/代数型门被当作 PASS 证据**：`b1 G1_strict_monotone_def`（σ_F 对 B 严格单调是构造性质）、`b4 H1_identity_lt_1e-12` 与 `H5_identity_lt_1e-12`（代数恒等）、`b4 H4_equal_leverage_loss_zero` 与 `H5_mismatch_zero_spread_zero_loss`（代数恒等）、`b6 H3_e11_dex_constant_overestimates`（因子按定义 = ln10）、`b6 H2_null_proportional_zero` 与 `H2_unbiased_green` 是**同一代码路径**（`b6:64-73` 两处都令 `est=truth*1.0`）。这些不是错，但不携带数据信息，不应计入"非退化"计数。
- **M3｜"+10% 电平偏差"名不副实**：`b6_gates_audit.py:71` 注入的是**按中位数分半的 ±10% 乘性偏差**，不是均匀电平偏差。复现：`python3 /tmp/verify6.py` ⇒ 均匀 `est=truth×1.10` 与 `×0.50` 的 E **都恒为 0**（尺度相消），只有分半偏差才给 0.0274。README §3.6/§2.3 与 b6 `--self-test` 的用例名 `"E: +10% level bias -> RED"` 都会让读者以为 E 能抓均匀电平错误——**它不能**（这是设计使然，但表述必须改）。
- **M4｜README §3.3 表行标签错**："合成 GRF ℓ=16 s=0.30" 一行填的是 **ℓ=64 s=0.30** 的数（dense 0.0184/sparse 0.2003/frame 0.2934）；ℓ=16 的真实值是 dense 0.0258/sparse 0.2304/frame 0.2307。另外该列把 M42/HST 的**实测** s_field 与合成的**标称** s 混在同一列。
- **M5｜"16 px 棋盘 hold-out"与代码不符**：`b3_domain_map.py:106` `fam=(yy+xx)%2` 是 **1 px** 棋盘（README:63/173 写 16 px）。1 px 棋盘在 32×32 cell 内恰好给 512 个真值像素（与 `EPS_REF` 的 N 自洽），所以实现选择合理，是文档写错。
- **M6｜"最坏 +34.0%"低估了自己的最坏值**：B1 暗源 B=0 的 `G4c_empirical_rn_max_rel_dev = 0.3662`（+36.6%）> B2 的 +34.0%；同处"天光主导时偏差 <1%"对暗源实为 −1.29%。
- **M7｜死代码含量纲错误**：`b2_noise_terms.py:93` `v_skyonly = ((B+D)/g² + (RN/g)² + F*Pf/g)` 的源泊松项少了 /g（应为 `F*Pf/g²`）。该变量**从未被使用**（`pred_doublecount_bias` 只用 `v_corr`/`v_emp`），不影响结果，但是埋着的雷。
- **M8｜真实数据面缺文件时静默跳过**：`b3_domain_map.py:228-229` `if not os.path.exists(path): continue`，无计数、无 JSON 记录；若 testdata 缺失，报告会静默少一个面。
- **M9｜行号引用对 HEAD 正确、对工作树漂移**：README:109 与 DOC_CORRECTIONS D1 引 `module_adapters.cpp:4258`/`3944-3945` —— 我核对了 `git show HEAD` 版本**完全正确**；但工作树因并发改动已漂到 **4267/3953-3954**。建议引用带 commit 或改用符号名。
- **M10｜B5 的"重采样"是按构造成立的**：`b5:74-76` `Y=R@X`，所以 `C_out=R C_in Rᵀ` 由线性代数**必然**成立，MC 只测采样误差（`H1_band_lt_0p10`/`H1_diag_within_2pct` 因此是 MC 收敛检查）。它验证了实现，不验证"drizzle 的方差确实这样传播"这一物理命题；§3.5 的措辞应限定。同理 `1+0.75ρ_out` 是启发式近邻式（门限放到 10%）。
- **M11｜并发 ctest 红的披露是加分项，但 `run/SCI-402/` 日志不入库**：`run/` 被 gitignore，报告里所有 `run/SCI-402/*.log` 证据在提交后不可复核；建议把关键日志摘要落进 `results/`。
- **M12｜审稿期间单元在持续变动**：13:01–13:23 之间 README/代码/结果被反复重写（我至少观察到 README 4 个版本：12:12 版 → 13:08 版 → 13:19 版 `88d3a1e2…` → 13:23 版 `fa1d5f7a0723b1a949b13808b56ed604`；b3 JSON 2 个版本；b6 源码 1 次功能性变更；13:17 新增 `code/make_tables.py` 与 `results/COMPARISON_TABLES.md`）。对"独立审稿"而言这是**流程问题**：审稿结论只能绑定到 §0 表中的 md5，README 其后任何改动均不在本审稿覆盖范围内；建议冻结后再送审。

---

## 4 未能核验的部分

1. **未复跑 b1/b2/b3/b4**（耗时 30 s–3 min，但作者在同一时段反复重跑同一批脚本，我若并行运行会与其结果互相覆盖、破坏双方证据）。我以"作者重跑 + 我逐字段语义 diff"替代：b1/b2/b3/b4 在多次重跑间**科学量逐位一致**，可信度高；但我**没有亲自从零跑出** b1/b3 的 JSON。b1 的核心物理我改用自建独立脚本对拍（闭式预言/斜率/1/ΣP²），结论一致。
2. **未逐行核验 `results/figs/*.png` 与图件生成脚本的正确性**（只确认 `make_figures.py` 只读 JSON、只写 `results/figs`）。
3. **未核验 `results/evidence_web.json` 中的外部佐证**（DOI/arXiv/Siril/photutils/PixInsight 链接）——需要联网逐条抓取；本次只确认该文件存在且被 README §7 引用。研究包中 SWarp 行号未核验一事，作者已在 §5.8/D5 自认，我未独立复核。
4. **未核验 `code/diag_highN.py`、`diag_noise_terms.py`、`make_tables.py`、`fetch_evidence.py`** 的逻辑（非主链路）。
5. **无法归因工作树中生产码/科学文档改动的作者**：`实验/` 整个目录未被 git 跟踪，`lib/algorithms/noise_snr/*`、`module_adapters.cpp`、`docs/science/NOISE_MODEL.md` 的改动我判断来自并发任务（FIX-405 / DOC-402，有 `run/DOC-402/`、`run/CLEAN-401/` 与代码内注释为证），但**没有工具能证明 SCI-B 作者从未碰过它们**。可核验的只有：SCI-B 全部脚本的写目标都在 `results/` 与 `run/SCI-402/` 内。
6. **未做 b1 的更大 N_MC 收敛性复核**（N_MC=1000 的 z 检验余量：max|z|=2.06/2.68，接近 3σ 门；我没有独立提高统计量去检验这些点是否只是"刚好过关"）。
7. **未复核 `07_noise_snr.md` 之外的上游合同**（如 Phase2 侧是否真的按 `snr_path` 消费）。

---

## 5 总体判定

**未达到"可判定结论 + 非退化证据 + 诚实边界"的完整标准。**

三块是好的、而且是硬的：
- **核心物理（B1/B2）经我独立复算成立**——闭式双计预言与实测臂在 B=0 处吻合到 **0.01–0.04 pp**（+12.62% vs +12.61%；+36.90% vs +36.86%），1/ΣP²=10.7893 复算一致，斜率/比值/存储字节逐位一致；
- **"发现指示项为红"是真发现而非改名掩饰**——生产码链路（经验总 rms → `sigma_sky_adu` → 再加 `(RN/g)²`）我在 HEAD 版本上逐行确认；
- **量纲（含被点名怀疑的 b4 part_e）无错**——代码式与正确式逐位等价，并通过我自建的端到端 MC；
- 可复现性极强（多次独立重跑逐位同值），恒真门问题（帧级 RMSE 门）被主动识别并给出替代判据，诚实边界写得比一般实验单元好。

但 **b3 这一块目前不可采信**：一个共享库里的 NaN 传播缺陷让 13 个 (面,Δ) 组合**静默丢掉整条 sparse 臂**、让部分面的 dense 臂只剩 32% 的格（RMSE 乐观 3.1 倍），而 `delta_star()` 把"没算"与"没交叉"混为一谈——恰好把缺失转成了报告想要的阴性结论（我用修正估计量重算，结论侥幸仍成立，但所交证据不支持它）。叠加 eps_ref 的 2.846 倍单位错误（且是本单元自己批判过的同一类错误），§3.3 表与 §8 表的数字需要重做。

### 必须修复项清单（按优先级）

1. **[阻断] 修 `sci_b_common.sigma_field_fast` 的 NaN 传播**（每轮 nan-aware 归约），重跑 b3，重出 `b3_domain_map.json`。
2. **[阻断] `eval_arms` 缺臂必须显式记录并判红**；`delta_star()` 区分 `not_computed` 与 `no_crossing`。
3. **[阻断] 重写 README §3.3 表（受影响面的 dense/sparse/ℓ/s_field）、§8 表、DOC_CORRECTIONS D4**，并把"s≥0.03 时 Δ 到 512 仍胜"改为"修正后仍胜，但 ℓ=16 处余量仅 0.6%"。
4. **[重要] 统一 eps_ref**：代码改用 `1.166/ln10/√N`（或文档值 0.506/√512），重算全部 `rmse_log_rho_corrected`，撤回"M1/M4 触零"的表述。
5. **[重要] b6 fail-closed 与 H14 的措辞降级**为"按规范转写的语义自检（实现侧未落地）"；或补一个真实调用路径的用例。
6. **[重要] `N1_flat_field_sparse_never_wins` 从"非退化负例/PASS 门"中移除**（保留为"判据退化区"的说明即可）。
7. **[重要] 修 `b4:168` 的 `var_equal_pred`（除 K）并重跑 b4**。
8. **[重要] 附录 B 的 ctest 6/6 改为与 `ctest_snr.log` 一致的表述**（5/6，红属并发 FIX-405，附日志摘要入 `results/`）。
9. **[次要] 改"+10% 电平偏差"为"中位分半 ±10% 乘性偏差"**，并明说 E 对均匀电平误差不敏感；同步 b6 `--self-test` 用例名。
10. **[次要] 修 §3.3 行标签（ℓ=16→ℓ=64）、"16 px 棋盘"→"1 px 棋盘"、"+34.0%"→"+36.6%"**；删 `b2:93` 死代码；给 b3 的 testdata 缺失加显式失败。
11. **[流程] 冻结版本后再送审**：本次审稿期间 README/代码/结果被反复重写，审稿结论只对 §0 的 md5 负责。

---

### 附：本审稿自身留下的痕迹

- 我运行 `b6_gates_audit.py` 与 `b5_phase3_transfer.py` 覆写了这两个 JSON；比对确认**除 `generated_at`/`wall_s` 外与运行前逐位相同**，且作者随后（13:12）的第三次重跑与二者仍逐位相同 ⇒ 我的运行**未改变任何数值**，故**未做还原**（还原反而会覆盖作者更新的时间戳版本）。
- 除本文件外，我未创建、修改或删除仓库内任何文件；所有复算脚本写在 `/tmp/verify*.py`。
