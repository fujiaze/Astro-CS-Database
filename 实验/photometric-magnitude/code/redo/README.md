# code/redo · 实验重做三路脚本（统一整理）

**来源**：独立审计/实验重做/P1通量积分拟合/{路线1,路线2,路线3}，原文件名保留；各路线重做报告随附为 `REPORT_route{N}.md`。
**seed**：三路统一固定 `SEED = 20260926`（脚本内写死；路线1 部分脚本派生 `SEED+1…`，路线3 同值）。RNG 一律 `numpy.random.default_rng(SEED)`。
**纪律**：纯 Python + numpy；不 import 仓库任何 Python；不跑 eng/**；未做 git 写。
**一键复跑**：`bash 实验/photometric-magnitude/code/redo/run_all.sh`（`quick` 跳过路线2 exp3/exp7）。结果写各 `routeN/results/`，基准读数快照在 `results/redo/route{1,2,3}/`。

| 目录 | 内容 | 覆盖项 |
|---|---|---|
| route1/ | exp1–exp5（seed 20260926，派生 +1…） | S1 c=4.685、S2 MAD 系数、S3 IRLS 容差、S4 mag_tolerance 边际保护、S5 FOV 三常数、S6 自适应阶梯、S7 星等窗平移、S8 匹配半径、S9 dex 界、S12 空间增益阶数、S13 1.166 因子、S14–S17 |
| route2/ | exp1–exp8（seed 20260926） | S1 ARE 三套求积互证、S2 常数截断发现、S3 容差不敏感、S4 预过滤、S5/S6 FOV 与阶梯、S8 匹配半径、S11 ZP 下限、S14 贯穿用例（ZP_syn=28.9358） |
| route3/ | exp_S00–exp_S12（seed 20260926） | S00 锚核验 15/15、S01 Kafadar 一手、S02 MAD 复算、S03 IRLS 收敛、S04 平移不变量、S05 不可估计帧、S06 F_syn/Simpson、S07 FOV 冲突、S08 阶梯、S09 mag 窗、S10 匹配半径、S11 ZP 抽样下限、S12 dex 界 |

本单元无 P1 专属补实验目录（补实验 -control_variance/-k_corr/-5.07 分别归 P2/P3/P5 承载，见五单元成稿简报）。
