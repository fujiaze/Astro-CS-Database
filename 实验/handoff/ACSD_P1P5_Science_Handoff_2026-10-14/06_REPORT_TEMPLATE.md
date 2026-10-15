# DeepSeek 回传报告模板

> 完成后在仓库 `实验/engineering-evidence/acsd-science-docs-p5/` 新增一份如 `SCIENCE_HANDOFF_REPORT.md`；不能放仓库根。报告结果必须真实来自此次运行，未知项填 UNKNOWN/NOT_RUN，禁止虚构。

## Git 与数据状态

- Repository / Branch:
- Start HEAD (40 SHA):
- End HEAD (40 SHA):
- Commits (SHA, 一句话, 文件路径):
- Toolchain (CMake/Ninja/编译器/Python/关键依赖):
- Real data SHA256 manifest file:
- HST synthetic input SHA256 manifest file:
- Linux/Windows build status (PASS, FAIL, NOT_RUN):
- `ctest` / full 3-command E2E status:

## 科学实验状态表

| Paper | Exp ID | Level (ORACLE/PHYSICAL_FORWARD/REAL_DATA/PRODUCTION) | Key metric (value/unit/denominator) | Controls | Status | Evidence path | Source SHA |
|---|---|---|---|---|---|---|---|
| P1 | | | | | NOT_RUN | | |
| P2 | | | | | NOT_RUN | | |
| P3 | | | | | NOT_RUN | | |
| P4 | | | | | NOT_RUN | | |
| P5 | | | | | NOT_RUN | | |

## 关键阻断风险复现（不能省略）

- P1 sigma_obs vs sigma_ceiling，独立 flat、色差、线性区及真实星统计：
- P2 117 星的预测/实测方差比、1σ/2σ 覆盖、相关噪声与扩展源误差：
- P3 face-center/apex −3.209e-3 几何异常的同物量确认、独立 Oracle 与 nside 扫描：
- P4 自然样条过冲、域外小网格与重建方差-权重测试：
- P5 S9b compA/compB 定标接缝、每分量规范和条件数、gain=1 阴性臂与真实 49 帧评估边界数：

## 论文交付物与文献

五篇 `REPORT_paper.md` 的主要结果是否已用新观测更新，四张图/四表各有真实执行来源吗？引用逐页式号确认了哪些，哪些仍为书目核验？具体列出。对五个贡献分别声明 SPECIFIED、IMPLEMENTED、EXPERIMENTALLY_SUPPORTED 或 RELEASE_VERIFIED，以及不满足的条件。

## 未决裁决与最后验收

按阻断等级列每条未决项：原文/实现/Oracle 的直接冲突、你尝试的三条独立核查、需要用户决定的两种可选方向及其科学后果。对真实物理缺陷，不允许为项目进度删除负例或改宽阈值。
