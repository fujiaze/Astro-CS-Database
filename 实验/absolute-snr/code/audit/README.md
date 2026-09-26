# code/audit — 独立审计三路重做 + 补实验（P2 跨帧绝对 SNR）

本目录收录独立审计"实验重做"阶段的可复现脚本与结果快照，来源如下（脚本文件名保持原名，逐字复制，未改算法）：

| 子目录 | 来源 | 内容 | 固定 seed |
|---|---|---|---|
| `route1/` | `独立审计/实验重做/P2跨帧绝对SNR/路线1/code/` | 14 个实验（exp01–exp14），P-CST-01…25 + 幻觉锚专项，26/26 | `20260926`（写死于脚本） |
| `route2/` | `独立审计/实验重做/P2跨帧绝对SNR/路线2/code/` | 12 个实验（exp01–exp12），25/25 三腿覆盖 | `20260926`（写死于脚本） |
| `route3/` | `独立审计/实验重做/P2跨帧绝对SNR/路线3/code/` | 6 个实验（exp01–exp06），26/26 | `20260926`（写死于脚本） |
| `supplement_control_variance/` | `独立审计/实验重做/P2跨帧绝对SNR/补实验-control_variance/code/` | 3 个脚本：有限 N 解析+MC、生产链忠实臂、独立 seed 抽检 | `20260601`（主）/ `20260605`（生产链臂）/ `20260602–04`（复核，写死于脚本） |

## 环境与纪律

- 依赖：Python 3 + numpy（补实验另用 `scipy.special.erf`）；零仓库 import；不构建、不运行 `eng/**`。
- 每个实验内置"真值无效应 ⇒ 度量归零/判红"负例（能红能绿）。
- 单脚本 CPU 时间 1–10 s 量级（上限 5 min），无重计算、无内存风险。

## 复现

```bash
bash code/audit/run_all.sh          # 顺序跑全部 35 个脚本，日志与 stdout 打印到终端
```

或分目录运行：

```bash
for s in code/audit/route1/*.py; do python3 "$s"; done
for s in code/audit/route2/*.py; do python3 "$s"; done
for s in code/audit/route3/*.py; do python3 "$s"; done
for s in code/audit/supplement_control_variance/*.py; do python3 "$s"; done
```

## 结果快照

`results/route{1,2,3}` 与 `results/supplement_control_variance` 为各路 `results/` 的 JSON 逐字快照
（`*.stdout` 文本日志未收录，见源目录）。关键读数的来源路线标注汇总见
`../../results/AUDIT_KEY_RESULTS.json`；报告正文见 `../../REPORT_experiment.md` 与 `../../REPORT_paper.md`。

## 与单元主实验（seed 20260921）的关系

本目录脚本均为**独立审计重做**产物，与本单元原 `code/`（b1–b7，seed `20260921`）互为独立复现：
两套 seed、两套实现路径覆盖同一常数体系，全部关键量在 MC 误差内一致；
分歧与订正以 `独立审计/实验重做/总编对账/分歧台账.md`（D-xx）为准，详见 `../../docs/LEDGER_CORRECTIONS_P2.md`。
