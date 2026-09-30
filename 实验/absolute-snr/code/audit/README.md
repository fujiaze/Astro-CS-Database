# code/audit — 独立审计三路重做 + 补实验（P2 跨帧绝对 SNR）

本目录是 P2 三路重做与补实验的可复现脚本与结果快照（脚本文件名保持原名，逐字复制，未改算法）：

| 子目录 | 脚本快照位置 | 内容 | 固定 seed |
|---|---|---|---|
| `route1/` | `route1/`（本目录内） | 14 个实验（exp01–exp14），P-CST-01…25 + 幻觉锚专项，26/26 | `20260926`（写死于脚本） |
| `route2/` | `route2/`（本目录内） | 12 个实验（exp01–exp12），25/25 三腿覆盖 | `20260926`（写死于脚本） |
| `route3/` | `route3/`（本目录内） | 6 个实验（exp01–exp06），26/26；**另加 `exp11_frozen_operator_transfer.py`（P2-M5 闭环，见下节，非路线3 原件）** | `20260926`（写死于脚本） |
| `supplement_control_variance/` | `supplement_control_variance/`（本目录内） | 3 个脚本：有限 N 解析+MC、生产链忠实臂、独立 seed 抽检 | `20260601`（主）/ `20260605`（生产链臂）/ `20260602–04`（复核，写死于脚本） |
| `../redteam/` | `absolute-snr/code/redteam/` | 帧级 `sigma_sky` 估计器的适用域反例（结构/噪声比阈值、常量像素 fail-open），逐条镜像生产裁剪循环、不调用生产二进制 | 无（确定性种子内联） |

## 环境与纪律

- 依赖：Python 3 + numpy（补实验另用 `scipy.special.erf`）；零仓库 import；不构建、不运行 `eng/**`。
- 每个实验内置"真值无效应 ⇒ 度量归零/判红"负例（能红能绿）。
- 单脚本 CPU 时间 1–10 s 量级（上限 5 min），无重计算、无内存风险。

## 复现

```bash
bash code/audit/run_all.sh          # 顺序跑全部 36 个脚本（含 exp11；EXP11_SKIP=1 跳过它）
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

## P2-M5 闭环脚本（`route3/exp11_frozen_operator_transfer.py`）

对抗审查 finding **P2-M5**（`run/FINAL-07/审核包/科研审查/SCI-702_P2_跨帧绝对SNR_审查报告.md:215`）指出
exp04 的 H3 用 **IDW(power=2,k=8)** 代理算子证明"1.5% 控制点精度约定完整穿过 P4 重建接口"，
而 IDW 既不是冻结默认算子 `natural_bicubic_spline_clip_v1`、也不在冻结词表内，且只用 64 个控制点的
512² 合成场，未覆盖生产域。`exp11` 是该 finding 的闭环证据：

- **不重写算子**：逐点重建全部由生产 `SparseSnrReconstructor` 给出（只读编译
  `lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp` 为独立驱动
  `code/exp11_recon_driver.cpp`）；
- 4 个冻结 token 全测；真实 M42 帧 4096² 全帧 × 真实控制网格（Δ=64 冻结默认 4096 点、Δ=32 加密 16384 点）；
- 含"常数网格 ⇒ 归零"负例、未知 token（含 IDW token）与越界的 fail-closed 负例、钳制值域核验、
  脉冲 Σw² 解析闭式门与线性叠加检验。

**与路线3 约定的已知偏差**：route3 其余脚本遵循"零仓库 import、不构建 eng/**、单脚本 1–10 s"；
exp11 必须 import 实验侧 `sci_b_common` 并编译生产源为只读驱动，单轮墙钟约 8 min（<900 s 上限）。

```bash
bash code/audit/run_all.sh                 # 会一并跑 exp11（需 testdata M42 帧与 g++）
bash 实验/absolute-snr/code/exp11_build_driver.sh
python3 eng/tools/monitoring/mem_guard.py --max-rss-gb 4 --timeout 900 -- \
    python3 实验/absolute-snr/code/audit/route3/exp11_frozen_operator_transfer.py
# 只跑非真实帧部分（约 2 min）：
python3 实验/absolute-snr/code/audit/route3/exp11_frozen_operator_transfer.py --skip-real
```

结果：`results/route3/exp11_frozen_operator_transfer.json`；
证据说明：`run/FINAL-07/审核包/科研审查/P2_订正/evidence/exp11_frozen_operator_transfer.md`。

## 与单元主实验（seed 20260921）的关系

本目录脚本均为**独立审计重做**产物，与本单元原 `code/`（b1–b7，seed `20260921`）互为独立复现：
两套 seed、两套实现路径覆盖同一常数体系，全部关键量在 MC 误差内一致；
分歧与订正以 `实验/裁决台账.md`（D-xx）与 `../../docs/DISPUTES.md`（A-P2-xx）为准，详见 `../../docs/LEDGER_CORRECTIONS_P2.md`。
