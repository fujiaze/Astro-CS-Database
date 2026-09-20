# F-INSTR — 星点通量口径调研与定案 (RELEASE-02 裁决 A6)

> 逆向验收工作区工作项 **F-INSTR-SURVEY**。对应报告：
> `reverse_verify/docs/f-instr-canon.md`（定案）与 `reverse_verify/references/f-instr-survey.md`（逐条文献/软件记录）。

## 这是什么

Phase1 用 `F_instr`（单帧实测星点通量）与 Gaia XP 合成通量 `F_syn` 之比
`r_i = log10(F_instr,i/F_syn,i)` 的 Tukey-IRLS 稳健位置 `location` 定 `k_photo = 10^(-location)`。
本工作项回答：**`F_instr` 该用哪种口径**，并给出孔径无关性的数值证据。

## 结论一句话

现生产的 `F_instr` 是 **5×5 固定盒 + 正性截断**（`star_detector.cpp:139-151` 的 `m00`），
它把**视宁度**读成了**乘性增益**：seeing 2.0→4.0 px 时回收通量变化 **−0.72 mag（假增益 0.52×）**，
且该变化随孔径剧烈改变 —— 与 Q1 的 P1-3「跨望远镜偏差 ~0.5 等」和「孔径依赖峰峰 7.78/4.93/3.81/4.25%」同源。
定案：改用 **PSF 总通量（PSF 加权最优提取 / PSF 拟合总通量）**，实测孔径无关性 ≤ **0.005 mag**。

## 文件

| 文件 | 作用 |
|---|---|
| `f_instr_lib.py` | 物理正向渲染器（Poisson/Gaussian/增益/饱和/量化/平场响应/天光梯度）+ 9 种通量口径估计器 + 度量 |
| `exp0_scene_and_noise.py` | 真实帧作底：选帧、干净窗口、经验 PSF、噪声模型自校验 |
| `exp1_recovery.py` | 回收精度 vs 峰值 S/N；噪声参数敏感性；天光梯度；PSF 模型误差 |
| `exp2_aperture_dependence.py` | 孔径依赖曲线 dmag(r) × seeing（任务书 ③-14） |
| `exp3_seeing_null.py` | seeing 负例：真值无效应 ⇒ 度量必须归零（能红能绿，判据写死在代码里） |
| `exp4_gain_recovery.py` | 真乘性增益回收（A4 验收前提）：正对照 + 负例 + 联合 + 小尺度结构响应 |
| `exp5_psf_shape.py` | PSF **形状族**自由度对总通量回收的影响（PSF 测光的主要风险面） |
| `run_all.sh` | 一键复跑（含 `TMPDIR=/dev/shm/astrocs_finstr`） |

中间产物：`run/reverse_verify/f_instr/`（gitignore，不入库）。

## 复跑

```bash
export TMPDIR=/dev/shm/astrocs_finstr
bash reverse_verify/experiments/f_instr/run_all.sh
```

全程约 2.5 分钟（exp0–exp5）。依赖 `numpy`/`scipy`/`astropy`（本机已具备；`photutils`/`sep` 未安装，故 PSF 拟合自实现，见报告 §1 核对状态）。

## 底数据声明

本仓**无哈勃/HST 数据**（全仓检索确认）。按负责人令的替代条款，
以 `run/RELEASE-02/L4-rebuild/norm/*/calibrated_*.fts`（49 帧真实标定帧）作底。
真实帧头**无 GAIN/RDNOISE 键**，故 `g`/`RN` 为声明式取值，并做了敏感性扫描（exp1）。
