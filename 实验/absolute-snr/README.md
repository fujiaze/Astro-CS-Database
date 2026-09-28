# 实验/absolute-snr — P2 跨帧绝对 SNR（SCI-B）· 单元入口

> **体系化整理定稿（总编对账后）**。唯一事实源：`独立审计/实验重做/总编对账/分歧台账.md` 与 `五单元成稿简报.md`。
> 单元定位：科学链第 2 创新点（最高设计 §2），P1 测光零点 → **P2 绝对 SNR 标尺** → P3/P4/P5。

## 一句话结论

跨帧绝对 SNR 把单帧测光零点提升为可传递的绝对信噪比标尺：全部 25 个登记常数三腿闭合——双计偏差 +14.5009%/+38.2524% 逐位复现（闭式 2.35×10⁻¹⁴）、对角欠估 36.3% 获闭式 1+ρ(M_eff−1)、9216 天空预算与直接管线测量差 1.2%、γ=2 由恒等式唯一确定（4.4×10⁻¹⁶ = 2 ulp）、1.152 经 D-04 终裁——稀疏控制点 sparse_snr_value = F_ref/σ_F 由此成为 P3 上球的无量纲硬通货。

## 交付物导航

| 文件 | 内容 |
|---|---|
| `REPORT_paper.md` | 正式论文（摘要/引言/链条位置/方法/数据/结果/结论/诚实边界/参考文献；数字带 [文献]/[实验]/[推导] 腿标注） |
| `REPORT_experiment.md` | 实验报告（假说/方法/数据/结果/台账订正/复现命令/诚实边界 + 【链条位置】节） |
| `refs.md` | 文献核验台账（只收一手 VERIFIED；标注级与拒绝采信项单列） |
| `results/AUDIT_KEY_RESULTS.json` | 关键结果 JSON 汇总（逐条注明来源路线与台账裁决号） |
| `code/` | 单元主实验 b1–b7（seed 20260921，`run_all.sh`）+ exp01–06 + reverse_verify |
| `code/audit/` | 独立审计三路重做 + 补实验脚本与结果快照（seed 20260926 / 20260601，`run_all.sh`，见其 README） |
| `docs/` | 支撑推导（DERIVATIONS_P2.md）、台账订正记录（LEDGER_CORRECTIONS_P2.md）、历史正本存档（LEGACY_*） |
| `results/` | 单元主实验 JSON（b*.json、exp0*.json）+ `DOC_CORRECTIONS.md` + `figs/` |

## 固定 seed

- 单元主实验：`SEED_BASE = 20260921`（`code/sci_b_common.py`；无时间/环境相关随机源）。
- 审计三路：`20260926`（写死于脚本）；补实验：`20260601`（主）/`20260605`（生产链臂）/`20260602–04`（独立复核）。
- 两套 seed、两套实现互为独立复现；全部读数为固定 seed 复现值，不重跑即引用 `results/` 既有 JSON。

## 复现

```bash
bash 实验/absolute-snr/code/run_all.sh                                        # 主实验 b1–b7 + ctest（需构建/联网，25–35 min）
bash 实验/absolute-snr/code/audit/run_all.sh                                  # 审计三路 + 补实验（36 脚本，全量实测约 55 min）
bash 实验/absolute-snr/code/reverse_verify/f_instr/run_all.sh                 # f_instr exp0–exp5（需真实 L4 标定帧，约 2.5 min）
bash 实验/absolute-snr/code/reverse_verify/frame_snr/run_all.sh               # 解析/物理红线 + 外部对拍（无构建，约 82 s）
bash 实验/absolute-snr/code/reverse_verify/snr_design/run_all.sh              # snr_design exp1–exp5（无构建，约 50 s）
( cd 实验/absolute-snr/code/reverse_verify/snr_design/audit && bash run_all_audit.sh )   # 4 项审计复算（无构建，约 10 min）
```

> **前置条件（第 3–6 条命令）**：需要真实 L4 标定帧产品树
> `run/RELEASE-02/L4-rebuild/norm/<tile>/{calibrated_*.fts, p1_sources.json, p1_snr.json}`。
> 该产品树已随轮次 GC 回收（`run/<轮次>/out/` 回收策略），本机当前不存在；重建需先重跑 RELEASE-02
> 的 L4 normalize（原始素材见 `testdata/M42_T2T3_mosaic_Flying_dutchman/`，本轮未重建）。
> **实跑状态**列区分「本次订正实跑」与「未实跑」，未实跑项写明阻塞条件；原始日志见
> `run/FINAL-07/审核包/科研审查/P2_订正/evidence/logs/`，逐条证据与判定见
> `run/FINAL-07/审核包/科研审查/P2_订正/P2-m7_m9_订正说明.md`。

| # | 命令 | 产物落点 | 耗时 / 构建 / 网络 | 本次实跑状态 |
|---|---|---|---|---|
| 1 | `bash 实验/absolute-snr/code/run_all.sh` | 单元 `results/`（b*.json、`figs/`、`tables/`）+ `run/SCI-402/`（逐步日志） | 约 25–35 min；**需先构建**（`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build`；脚本内再 g++ 直编生产驱动、跑 `ctest -R p1snr_science\|p1noise_numpy_oracle`，写 `build/`）；第 1 步 `fetch_evidence.py` **需联网** | **未实跑**（注①） |
| 2 | `bash 实验/absolute-snr/code/audit/run_all.sh` | 单元 `code/audit/results/<route>/*.json`（就地覆盖）；`EXP11_SKIP=1` 可跳 route3/exp11 | 全量 **实测约 55 min**（3281 s，`EXP11_SKIP=1` 跳过 route3/exp11 时 35 个路线脚本全绿 exit 0）；含 `route3/exp11` 另需约 8 min（需 g++ 与 testdata M42 帧）；单脚本 `route2/exp02` 单独实测 442 s；无构建、无网络 | **实跑**：先按 1800 s 上限跑 → 超时（16/36，exit 124，peak 2.09 GB）；改为 3600 s 上限后 **35/35 全绿 exit 0，3281 s**（peak 2.07 GB；注②） |
| 3 | `bash 实验/absolute-snr/code/reverse_verify/f_instr/run_all.sh` | `run/reverse_verify/f_instr/{logs/*.log, scene.npz, exp*.json}` | 约 2.5 min；无构建；需 numpy/scipy/astropy | **未跑通**（exp0 抛 IndexError；注③） |
| 4 | `bash 实验/absolute-snr/code/reverse_verify/frame_snr/run_all.sh` | `run/reverse_verify/frame_snr/{redlines,redlines_physical,external_crosscheck}.json` + `实验/absolute-snr/run/reverse_verify/frame_snr/{branch_discriminator,p1_snr_inventory}.json` | 约 82 s；无构建（T12/P12 内用 `g++` 直编只读生产 TU） | **实跑**（exit 1：T8–T11/P8–P11/P14 绿；T12/P12/P13、branch_discriminator 红；注④⑤） |
| 5 | `bash 实验/absolute-snr/code/reverse_verify/snr_design/run_all.sh` | `run/reverse_verify/snr_design/exp*.{json,log}`，并**回拷覆盖** `code/reverse_verify/snr_design/exp*.json` | 约 50 s；无构建；exp2/exp3 需真实帧 | **实跑**（exp1/2/4/5 绿；exp3 红：缺 `t2_m2_red/p1_sources.json`） |
| 6 | `bash 实验/absolute-snr/code/reverse_verify/snr_design/audit/run_all_audit.sh` | `run/reverse_verify/snr_design/audit/*.{json,log}` | 约 10 min（实测 588 s）；无构建；**CWD 必须 = 该 audit 目录**（脚本内数据路径为相对路径） | **实跑**（3/4 绿；`audit_sim_validation` 红，注⑥） |

- 注① `code/run_all.sh` 未实跑，三个独立阻塞条件：(a) 需 `cmake`/`ninja` 构建产物与 `ctest`（`ctest` 写仓库根 `build/`）；(b) 第 1 步 `fetch_evidence.py` 走 `urllib` 取 6 处外部一手佐证（pixinsight / crossref / arxiv / gitlab / github），无网络必然失败；(c) 产物写 `run/SCI-402/`，整轮 25–35 min。审查报告 §4.1 同样登记为「未跑」。
- 注② 第 2 条以 `python3 eng/tools/monitoring/mem_guard.py --max-rss-gb 8 --timeout 1800 -- bash run_all.sh` 实跑：1800 s 被杀（exit 124），完成 16/36 个脚本，被杀时停在 `route2/exp02_robust_scale_mad.py`（单独计时 442 s）。故脚本注释与 `code/audit/README.md` 的「单脚本 CPU 秒级」不适用于全量：`route1/*` 多为分钟级，前 16 个脚本就用满 1800 s，全量实测 3281 s。**两次实跑（超时版与全绿版）跑后与跑前快照比较：35 个 `results/<route>/*.json` 逐字节 0 差异**（幂等）。`route3/exp11`（`exp11_frozen_operator_transfer.py`）本轮以 `EXP11_SKIP=1` 排除——该脚本为同期另一 worker 新增、仍在开发，本代理未实跑它。**该目录同期正被另一 worker 并发编辑**（工作树有 `M code/audit/run_all.sh`、`?? code/audit/route3/exp11_frozen_operator_transfer.py` 等），36 脚本数与耗时随其改动漂移。
- 注③ 第 3 条：`exp0_scene_and_noise.py:91` 抛 `IndexError: list index out of range` —— 它从 `run/RELEASE-02/L4-rebuild/norm/*/calibrated_*.fts` 选帧，该树已回收（本机仅存 1 帧 smoke 产物，且它在 `run/AUTONOMOUS-01/` 下、不在该 glob 路径内）。**另：该组 6 个脚本把 `ROOT` 写死为绝对路径 `/workspace/Astro CS Database`（`exp0:18`、`exp1:17`、`exp2:18`、`exp3:22`、`exp4:20`、`exp5:52`），违反机器手册 §3「在仓库内工作，不写死服务器绝对路径」，并使该 runner 无法在镜像副本内运行**（它会无视副本路径、直接读写工作仓库）。属既有缺陷，本次未修（超出 P2-m7/P2-m9 订正范围），已上报。
- 注④ 第 4 条 runner 末尾对 `run/reverse_verify/frame_snr/` 的 3 个 JSON 做丢字段检查，缺 p1 产品即整轮 FAIL，故 exit 1。T12/P12 需在**工作仓库**（`lib/` 生产 TU 在位）条件下才真跑；镜像内跑会退化为 `SKIP: production source not found`，因此本次另用 `--repo <工作仓库> --out <证据目录>` 单独补跑（不写仓库）。
- 注⑤ T12/P12 红是**判据传参口径缺陷**，不是生产与 canon 不一致：生产 `snr_source_snr_f64` 的 `fwhm_px` 字段语义是「检测块高斯 FWHM」（`lib/algorithms/noise_snr/cpp/src/snr_science.cpp:53` `detectionSigmaFromFwhm = fwhm/2.3548200`，`:110-112` 明示跨块禁止混用），而 `run_redlines.py`/`run_redlines_physical.py` 把 canon 的 **Moffat4 FWHM**（`FWHM = 1.230310σ`，`docs/science/PSF.md:66`）原样灌入该字段。按正确口径（`fwhm = 2.3548200·σ`）复算，生产与 canon 一致到 `1.3e-14`（`sum_p2`）/`5.8e-15`（SNR），见 `evidence/logs/t12_attribution.log`。`P13` 红因无真实帧（`SKIP: no real frame found`）；`branch_discriminator.py:28`、`inventory_p1_snr.py:27` 的 `REPO` 只上溯 3 级（指到 `实验/absolute-snr` 而非仓库根），故永远扫不到 `run/` 下的 p1 产品、恒报 0 产品。三项均为既有缺陷，本次未修，已上报。
- 注⑥ 第 6 条：`audit_sp0` / `audit_exp3_physical` / `audit_exp1245` 绿；`audit_sim_validation` 写出部分 JSON 后 `KeyError: 'G_noise_affinity_dimensionless'` —— G 段需要 ≥2 帧同夜真实帧，本机替代数据仅 1 帧。该 runner 同时是 P2-m9 改前/改后逐位对比的载体：4 个产物除 `elapsed_s` 计时字段外**值域逐位一致**（`evidence/logs/m9_after_compare.log`）。
- 通用：第 3–6 条落 `run/reverse_verify/**`；`eng/tools/run_keep.txt` 未登记该族（`grep reverse_verify` 无命中），产物可能被 `run_gc` 回收。第 5 条会把产物**回拷覆盖**单元内既有 `code/reverse_verify/snr_design/*.json` 快照——复核时建议先备份或在副本内运行。

## 与台账/历史正本的关系

- 本单元历史正本（整理前的 README 与论文精读报告）原样存档于 `docs/LEGACY_README_SCI-B_v1.md` 与 `docs/LEGACY_REPORT_paper_v1.md`，未删除；仍成立部分已吸收进现行文件。
- 与台账冲突处按裁决改写并逐条注明 D-xx/A-P2-xx，明细见 `docs/LEDGER_CORRECTIONS_P2.md`（1.152 终裁与 5811→5816.6、“seed 无关闭式”错误标签、对角欠估闭式统一、γ 恒等门 4 ulp 规则、control_variance N=5 方向词、k_corr 改查表等）。
- 负责人已批事项在本单元的落实：k_corr 查表由 P3 承载（本单元只引机制腿）；插值设置配置化＋运行日志输出不落盘（P4 承载）；掩膜 k=0.1 等规范选择登记为“项目约定，不注文献出处”；反方差口径＋Aitken 1935（标注级）引用进入科学文档。

## 诚实边界速览

1.152 标定登记出处待补登；m_ref=6.0 为单位制锚点（冻结纪律）；k_corr 查表网格属 P3 交付件；P-CST-23 声称系列生成配置欠定；生产链被估量 y 的合同语义待负责人裁决；对 MC 真值的偏置数字带 ±3 pp MC 噪声（低噪证据是臂比值 vs 闭式预言 ≤1.25 pp）。详见 `REPORT_paper.md` §6 与 `REPORT_experiment.md` §7。