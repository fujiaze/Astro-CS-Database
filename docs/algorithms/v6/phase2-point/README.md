> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# ALG-P2-POINT-001 — Phase2 点源算法规格（人读正文索引）

- 任务：`工程控制/旧 V6 控制包（ROOT-007 已删除）/tasks/ALG-P2-POINT-001.md`（wave 3，depends_on SCI-ADJ-001）
- 写域：`docs/algorithms/v6/phase2-point/`（tracked）+ `run/v6/alg-p2-point/`（gitignore 工作区：机器规格/独立 Oracle/证据/命令日志）
- 基线：`HEAD = main = 125bc0999363be1a42a1f2df3254601e0cc7b8fb`
- 建议状态：PASS（见本任务返回；由控制器验收）

## 文件

| 文件 | 内容 |
|---|---|
| `ALG-P2-POINT-001_SPEC.md` | 主规格：符号/单位、Q/W 合并、proper/effective PSF、点源检测/测光、输出帧重算、independent-frame `SNR_combined²=ΣSNR_k²` 验证、fail-closed 门、provenance、验证矩阵、移交登记。每条 clause 带 SCI/ALG/DATA/DESIGN/文献锚 |
| `ALG-P2-POINT-001_GATES.md` | 门目录与负向 mutation 目录（与机器规格 `gates`/`mutations` 一一对应） |
| `README.md` | 本索引 |

## 机器证据（run/v6/alg-p2-point/）

| 产物 | 说明 |
|---|---|
| `spec/phase2_point_spec.json` | 机器可读规格（units/formulas/clauses/gates/mutations/required_freeze_ids） |
| `oracle/phase2_point_oracle.py` | 独立 NumPy Oracle（O1–O12 正向 + OM1–OM3 自我 mutation；不调用生产实现） |
| `oracle/check_alg_p2_point_spec.py` | 规格结构门（量纲代数/门引用/条款-锚/冻结条目引用；M01–M15 负向 mutation） |
| `oracle/run_all.sh` | 一键复跑（单一 rc） |
| `summary.json` | 机器可读汇总（基线 SHA、计数、rc、建议状态） |
| `logs/` | 全部命令 stdout/stderr 与 rc |
| `mutations/` | 每条注入错误的记录/脚本 |

## 与本包其他任务边界

- `surface_gls`（GLS、pixel-ivar 误差门、UPM/rejection/covariance）→ `ALG-P2-SURF-001`
- `psfsw_robust`（共同星集、四分量、稳健归一、conventional coadd、基线比较、fail-closed）→ `ALG-P2-PSFSW-001`
- Phase1 `W_info`/PSFSW 分量生产 → `ALG-P1-001`；Phase3 传播与 Q/W 输出平面重算 → `ALG-P3-001`
- schema/字段词表归一 → `DATA-DESIGN-001` + `SCHEMA-INTEGRATE-001`（W6；本任务只引用不发明词表）
- 正式冻结 → `CONTRACT-FREEZE-001`（W4，写入 `docs/algorithms/v6/frozen/`）

## 基线分歧声明（强制）

工作树相对 HEAD 存在未裁决回退/删除（CTRL-F1）；本任务一切生产面判定以已提交 **HEAD=125bc099** 为准，
Oracle 不读取工作树生产实现，不构成同实现自证。
