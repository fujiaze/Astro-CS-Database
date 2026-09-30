# 实验/absolute-snr — P2 跨帧绝对 SNR（SCI-B）· 单元入口

> **体系化整理定稿（总编对账后）**。裁决唯一事实源：`实验/裁决台账.md`（跨单元 D-xx）与 `docs/DISPUTES.md`（本单元 A-P2-xx）；科学正本同步件 `docs/science/DISPUTE_RESOLUTION.md`；成稿口径摘要见本单元 `REPORT_paper.md` 与 `docs/LEDGER_CORRECTIONS_P2.md`。
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
| `docs/` | 支撑推导、分册实验报告、定案与调研（见 `docs/README.md`） |
| `results/` | 单元主实验 JSON（b*.json、exp0*.json）+ `figs/` |
| `data/` | 数据指针（只读声明，本单元不新增、不复制数据） |

## 固定 seed

- 单元主实验：`SEED_BASE = 20260921`（`code/sci_b_common.py`；无时间/环境相关随机源）。
- 审计三路：`20260926`（写死于脚本）；补实验：`20260601`（主）/`20260605`（生产链臂）/`20260602–04`（独立复核）。
- 两套 seed、两套实现互为独立复现；全部读数为固定 seed 复现值，不重跑即引用 `results/` 既有 JSON。

## 复现

所有命令都从**仓库根**执行。脚本内部从自身位置推导仓库根（`Path(__file__).resolve().parents[N]`），
不写死任何机器绝对路径，也不依赖调用者的工作目录。

### 公共前置步：合成数据物理链自检

```bash
bash 实验/shared/synthetic/run_selftests.sh
```

先证明合成器链本身可信（能红能绿），再让本单元跑真实验；任一组件判红时退出码为 1，
实验读数不得作为证据。**不得只调 `noise_selftest.py`**——它只验独立重实现，对生产实现零判别力；
生产面的门禁责任在 `m16_scene --selftest` 与 `m16_sampling --selftest`。

### 本单元入口

| # | 命令 | 产物落点 | 耗时 / 构建 / 网络 |
|---|---|---|---|
| A | `bash 实验/absolute-snr/code/run_all.sh` | 单元 `results/`（b*.json、`figs/`、`tables/`）+ `run/SCI-402/`（逐步日志） | 约 25–35 min；**需先构建**（`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build`）；第 1 步 `fetch_evidence.py` **需联网** |
| B | `bash 实验/absolute-snr/code/audit/run_all.sh` | `code/audit/results/<route>/*.json`（就地覆盖）；`EXP11_SKIP=1` 可跳 route3/exp11 | 全量约 55 min；`route3/exp11` 另需 g++ 与 testdata M42 帧；无构建、无网络 |
| C | `bash 实验/absolute-snr/code/reverse_verify/frame_snr/run_all.sh` | `run/reverse_verify/frame_snr/*.json` | 约 82 s；无构建（T12/P12 内用 `g++` 直编只读生产 TU） |
| D | `bash 实验/absolute-snr/code/reverse_verify/snr_design/run_all.sh` | `run/reverse_verify/snr_design/exp*.{json,log}`，并回拷覆盖 `code/reverse_verify/snr_design/exp*.json` | 约 50 s；无构建 |
| E | `bash 实验/absolute-snr/code/reverse_verify/snr_design/audit/run_all_audit.sh` | `run/reverse_verify/snr_design/audit/*.{json,log}` | 约 10 min；无构建 |
| F | `bash 实验/absolute-snr/code/reverse_verify/f_instr/run_all.sh` | `run/reverse_verify/f_instr/{logs/*.log, scene.npz, exp*.json}` | 约 2.5 min；无构建；需 numpy/scipy/astropy |

### 真实标定帧产品树

入口 D、E、F 与入口 C 的 P13 项需要真实标定帧产品树，默认落点

    run/RELEASE-02/L4-rebuild/norm/<tile>/{calibrated_*.fts, p1_sources.json, p1_snr.json}

可用环境变量 `P2_NORM_DIR` 指向等价的 normalize 产品树（须含 `<tile>/calibrated_*.fts`）：

```bash
P2_NORM_DIR=/path/to/norm bash 实验/absolute-snr/code/reverse_verify/snr_design/run_all.sh
```

产品树缺位时，相关项以**明确诊断**退出（f_instr 为退出码 2 并打印期望布局），
不再以 `IndexError` 形式失败。重建该产品树需先跑 normalize（原始素材见
`testdata/M42_T2T3_mosaic_Flying_dutchman/`）。

注：入口 D 会把产物**回拷覆盖**单元内既有 `code/reverse_verify/snr_design/*.json` 快照——
复核时建议先备份或在副本内运行。入口 B 就地覆盖 `code/audit/results/`。`run/reverse_verify/**`
未被产物保留清单登记，可能被 `run_gc` 回收。

### 已知缺口

- `code/reverse_verify/snr_design/audit/audit_mosaic_shape.py` 有脚本与结果快照，但**未接入**入口 E 的
  脚本列表；补齐需前台决定是否纳入一键入口。

## 与台账的关系

- 整理前的历史正本不再随单元保存；其仍成立部分已吸收进现行 `README.md`、`REPORT_paper.md` 与 `REPORT_experiment.md`，失效部分按台账订正。
- 与台账冲突处按裁决改写并逐条注明 D-xx/A-P2-xx，明细见 `docs/LEDGER_CORRECTIONS_P2.md`（1.152 终裁与 5811→5816.6、「seed 无关闭式」错误标签、对角欠估闭式统一、γ 恒等门 4 ulp 规则、control_variance N=5 方向词、k_corr 改查表等）。
- 负责人已批事项在本单元的落实：k_corr 查表由 P3 承载（本单元只引机制腿）；插值设置配置化＋运行日志输出不落盘（P4 承载）；掩膜 k=0.1 等规范选择登记为「项目约定，不注文献出处」；反方差口径＋Aitken 1935（标注级）引用进入科学文档。

## 诚实边界速览

1.152 标定登记出处待补登；m_ref=6.0 为单位制锚点（冻结纪律）；k_corr 查表网格属 P3 交付件；P-CST-23 声称系列生成配置欠定；生产链被估量 y 的合同语义待负责人裁决；对 MC 真值的偏置数字带 ±3 pp MC 噪声（低噪证据是臂比值 vs 闭式预言 ≤1.25 pp）。

帧级 `sigma_sky` 估计器的适用域以 `docs/frame-snr-canon.md` §2.7 表为唯一登记面，三条失效域：

- **饱和**：跨膝点时单调性严格反转（`sigma_hat` 124.6→3.9 ADU，`SNR_frame` 升一个量级以上）⇒ 处置是**整帧 fail-closed 拒收**，不是剔除饱和像素后继续（原处方本身产生反转）。
- **结构主导**：判据应是「`sigma_sky` 估计器对天光单调」，且只在结构 rms / 天光噪声 rms **≲ 2** 时成立——实测 `d ln sigma_hat / d ln B` 为 `r=0 → 0.4898`、`r=1 → 0.2459`、`r=3 → 0.0782`、`r=10 → 0.0101`、`r=30 → 0.0011`；真实 M42 亮帧的 `sigma_hat_prod/sigma_hat_fix = 2.44–3.66` 正在该衰减带内 ⇒ 整帧标量口径在该域**不适用**，须改区域化 `sigma_sky`。
- **常量（掩膜 / 零填充 / 过曝置零）像素 ≥ 0.40**：第 2 轮 `MAD` 恰为 0 ⇒ 只留等值像素 ⇒ 生产估计器把 `sigma_hat` **静默置成地板 1e-9** ⇒ `SNR_frame` 高估 **2×10¹⁰ 倍**，链上无守卫；0.30–0.38 另有无告警退化带（SNR 高估 1.8×–8.9×）。处置同为整帧 fail-closed 拒收；**修法属生产码治理**（`lib/`，本单元只登记）。

其余口径的反例核查为阴性：源主导、`F_instr` 正性截断、掩膜边界 `r_i` 收缩、逐像素 vs 整帧口径——前三条未找到违反，第四条是定义性恒等式而非实验结论，但两口径必须在正文标注。

复现：`python3 code/redteam/rt_sigma_hat_applicability.py`（逐条镜像生产裁剪循环，不调用生产二进制、不写 `results/`）。详见 `REPORT_paper.md` §6 与 `docs/frame-snr-canon.md` §2.7。