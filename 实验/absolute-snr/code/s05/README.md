# S05 / P2 实验代码（唯一来源）

- `s05_common.py`：PSF / 正本 §3.3 噪声模型 / GLS / 覆盖率与 Wilson 区间 / MAD / seed 派生。
- `s05_drizzle_geom.py`：可分离方格 Drizzle 线性算子与输出协方差（精确 1D⊗1D 构造）。
- `s05_arms_oracle.py`：四臂独立 Oracle（白噪+光子 / 自由背景 GLS / 线性 R 重采样 / 点源-弥散源）。
- `s05_kcorr_units.py`：`k_corr` 单位证据、标定域 MC、生产权重交叉核对、P3 前后 covariance 对照。
- `s05_real117.py`：真实帧重测（§13.1 清单枚举 + CRVAL 断言 + 训练/留出 + 分层 coverage + 经验地板层）。
- `s05_figures.py` / `s05_evidence.py`：图件与紧凑机器证据。
- `run_all.sh`：一键复跑（写入 `run/ACSDSDOC-02/S05/`）。
