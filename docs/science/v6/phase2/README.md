> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# SCI-P2-001 — Phase2 三模式集成复核（人读正文索引）

> 上游：ASTROCS_DESIGN.md §2（核心科学方法）、§3（数据对象与配置）

任务：`工程控制/旧 V6 控制包（ROOT-007 已删除）/tasks/SCI-P2-001.md`
基线：`HEAD = main = 4b508f28bbcada66c417a8a9324aca7eb92869ff`（本机 `git rev-parse` 实测）
写域：`docs/science/v6/phase2/`、`run/v6/sci-p2/`（仅此两处）
建议状态：**PASS**

## 文件

| 文件 | 内容 |
|---|---|
| `SCI-P2-001_THREE_MODE_REVIEW.md` | 主正文：三模式目标函数 / 输出 / 最优性边界 / 验证，逐项条款锚与文献锚 |
| `COVARIANCE_AND_EFFECTIVE_PSF.md` | 三模式 covariance 从**实际组合系数**传播的公式，及 effective PSF 定义 |
| `WEIGHT_PROVENANCE_GATE.md` | "median SNR/support/coverage/FWHM/residual 不得单独冒充权重"的机器判据（R0–R8） |
| `VERIFICATION_AND_EVIDENCE.md` | 独立 Oracle、结构探针、负向 mutation 的命令、rc 与结果 |

## 机器证据（`run/v6/sci-p2/`）

| 产物 | 说明 |
|---|---|
| `oracle/phase2_three_mode_oracle.py` | 独立 Oracle（纯 NumPy，**不调用生产实现**），32 项 check |
| `oracle/weight_provenance_gate.py` | 权重来源机器判据门（独立合同门，只读 JSON） |
| `oracle/probe_production_surface.py` | HEAD 生产面结构证据探针，16 项 check |
| `oracle/run_gate_and_mutations.py` | 5 正向控制 + 17 门 mutation + 3 Oracle 自我 mutation |
| `oracle/run_all.sh` | 一键复跑（单一 rc） |
| `summary.json` | 机器可读汇总（基线 SHA、计数、rc、建议状态） |
| `logs/` | 命令日志（含 rc 行） |
| `mutations/` | 每个被注入错误的记录/脚本 |

## 基线分歧声明（强制）

工作树**不是** HEAD 的干净副本：16 个 tracked 文件回退到祖先 blob、10 个 tracked 文件被删除
（`run/v6/base/baseline_freeze.json.worktree_vs_head`，BASE-OWN-001 机器证据）。
本任务一切**生产面判定以已提交 HEAD 为准**，并在 `VERIFICATION_AND_EVIDENCE.md` 显式标注
"基线分歧未裁决"。工作树中 `lib/infrastructure/scheduler/src/module_adapters.cpp` 相对 HEAD 少 135 行/多 27 行，
故该文件只以 `git show HEAD:...` 读取。
