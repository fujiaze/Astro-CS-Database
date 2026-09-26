# 实验单元：healpix-polar（P3 守恒映射算子 / 极区面积交叠）

## 这是什么

ACSD 科学链第 3 点（最高设计 §2 ALG-DRZ）：mosaic 阶段把源帧 drizzle 到 HEALPix 网格时，
精确计算"源像元 drop 足迹 × HEALPix 叶单元"的球面交叠面积，实现平面到球面的通量守恒映射。
本单元是 **k_corr 常数的归属单元**（D-08 终裁，供 P5 引用，正确表述见 REPORT_paper.md §3.4）。

## 成稿文件（2026-09 总编对账收口）

| 文件 | 内容 |
|---|---|
| REPORT_paper.md | 正式论文（三路证据整合定稿；每个数字标注 [文献]/[实验:脚本]/[推导]） |
| REPORT_experiment.md | 实验报告（假说/方法/数据/结果/结论/诚实边界/复现命令/seed 说明） |
| refs.md | 文献核验台账（只收一手 VERIFIED；标注级/未证实条目单列） |
| docs/DERIVATIONS-P3.md | 支撑推导（A_leaf、亏缺律、外接半径双口径、矢高深度、量化界、k_corr 两因子等 10 条） |
| docs/EXP-07-POLAR.md（+摘要） | **历史正本**：极区破门根因与候选算法对照（保留；与台账无冲突数值，其 §4.7/§4.8 自我更正在原文内） |
| code/ | 历史探针（C++17，p0–p10，固定几何无随机）＋ code/audit/（三路审计与补实验脚本收编） |
| code/audit/run_all.sh | 三路＋补实验一键复现（各脚本 seed 写死：route1=20050709、route2=20260927、route3=20260926、kcorr SEED_BASE=20260816） |
| run_all.sh（根） | 历史探针一键复现（quick/full，逐位一致） |
| results/ | 历史探针日志/CSV 存档＋ SNAPSHOT.sha256 |
| results/audit/{route1,route2,route3,kcorr}/ | 三路＋补实验关键结果 JSON 存档（索引见 results/audit/KEY_RESULTS.md，注明来源路线与台账编号） |

## 一句话结论（按分歧台账订正后）

P3 的守恒是**构造性精确**：drop 归一权重 Σw=1 逐位、全局闭合 8.2e-15、
A_leaf = π/(3N²) 双式互证 ≤3.3e-12（F&H 2002 **§2 式(2)–(5)** 的 s² 表面亮度守恒——出处按 D-03 终裁）。
球面几何偏差全部闭式化：极像元弦亏缺律 **0.1043885/N²**（D-09 订正，旧值 0.104369 系转写误差）、
矢高 sag0 = 8.094e-2·**ρ₁**（D-02 订正单位）⇒ max_depth 须 ≥9（生产 cpp=12 已满足，只改文档）、
外接半径全天 sup ≈ **1.0415·hp_res**（1.25 裕量 ≥20%；路线1 的 1.1284 按 D-01 改记账为极冠叶对角常数 2/√π）、
量化界 |r| ≤ 0.5/q（A-P3-05 判决方向）。验收必须**逐叶**（求和判据对恰保总量注入失明 ≥14.88 个数量级）。
k_corr = k_gauss(N) × k_geo 两因子＋几何查表取代冻结 1.4（声明域两端低估 32%/2 倍，D-08，负责人已批）。

## 极区实现（历史正本核心结论，维持成立）

极点破门三条根因：RC1 叶边界"4 角＋大圆弧弦"（弦偏差 0.0639 hp_res、nside 无关、单叶亏损 9.97e-2）、
RC2a acos(z) 角点构造（RC1 的前置暴露条件）、RC3 TAN 逆投影极点量化（不破门闭合判据、须单独设检）。
闭合判据对 RC1 近乎失明（破门全部来自快路径假阳性），真判据是**逐叶误差**。
另有 u+v=1 接缝绝对面积地板 ≈1.4e-16 sr（像元尺度 ≲2.4″/px 破门；HST 0.04″/px 实测 3.3e-3）。
最小改动面修复（REC-1：稳定角点式＋自适应细分＋覆盖自检）预算 1e-7·A_drop 下极点 0/1089、最坏 2.79e-8，非极区零代价。

## 一键复现

- 三路审计＋补实验：bash code/audit/run_all.sh（Python，seed 写死，输出到 run/healpix-polar-audit-logs/，与 results/audit/ 存档逐位对照）
- 历史探针：bash run_all.sh [quick|full]（约 15/60 分钟，日志落 run/EXP-07-POLAR/logs/）

## 环境

- Python ≥3.10 + numpy（audit 脚本，无仓库内 import、无网络）；Linux amd64，g++ -O2 -std=c++17（历史探针）。
- 历史探针不链接产品二进制（只读编译生产两个源文件做交叉核对），峰值 RSS < 130 MB。

## 与生产代码的关系

本单元**只读** lib/algorithms/drizzle/healpix_drizzle/ 与 lib/algorithms/shared/healpix/（探针交叉核对），
不修改任何生产文件；修复方案以伪代码形式写在 docs/EXP-07-POLAR.md §6.1，落地属于单独任务。
需要订正的生产/文档条目清单见 REPORT_paper.md §5 与五单元简报"需订正的文档条目"（本单元只登记，不改仓库其余部分）。

## 目录约定

沿用仓库既有实验单元约定 实验/<kebab-topic>/{code,docs,results,README.md}；2026-09 成稿新增
REPORT_paper.md / REPORT_experiment.md / refs.md / code/audit/ / results/audit/（收编自
独立审计/实验重做/P3守恒映射算子/ 三路＋补实验，原文件名保持）。
分歧裁决一律以 独立审计/实验重做/总编对账/分歧台账.md 为准（D-01/02/03/08/09/11、A-P3-01…12）。
