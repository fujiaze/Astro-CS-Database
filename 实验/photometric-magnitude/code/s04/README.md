# S04 · P1 真实帧光度与颜色误差归因（可复跑脚本）

本目录是本单元 S04 的全部执行脚本。执行报告、逐星表、图与全部读数在
`run/ACSDSDOC-02/S04/`（`run/` 与 `实验/**/results/` 均按 `.gitignore:17/187` 不入库，
故入库的可复算面 = 这 7 个脚本 + `../../REPORT_experiment.md` §11 追加章节）。

基线：`audit/acsd-science-docs-p5`，`HEAD = e31b2be66ffe37d547e67a4ba007e7257e49789d`。

## 复跑（按依赖顺序，全部从仓库根执行）

```bash
cd <repo root>
python3 实验/photometric-magnitude/code/s04/s04_inventory.py     # §13.1 合规选帧（2.9 s）
python3 实验/photometric-magnitude/code/s04/s04_measure.py       # 旧读数复现 + 8 帧逐星（99 s）
python3 实验/photometric-magnitude/code/s04/s04_strata.py        # 分层 / 训练-留出（3.4 s）
python3 实验/photometric-magnitude/code/s04/s04_sigmaflat.py     # σ_flat 独立测量（88 s）
python3 实验/photometric-magnitude/code/s04/s04_linearity.py     # 线性区 / 饱和序列（27 s）
python3 实验/photometric-magnitude/code/s04/s04_negatives.py     # P1-D 负例（42 s）
python3 实验/photometric-magnitude/code/s04/s04_acceptance.py    # 验收格式汇总（<1 s）
```

## 脚本职责

| 脚本 | 职责 |
|---|---|
| `s04_common.py` | 公共层：路径重定向（`sc.RUN/CACHE/RESULTS`、`sg.CACHE/DUMP_BIN` → `run/ACSDSDOC-02/S04/`）、FITS 三种读法、单帧引导测光 + 逐星表、双边界预算、分层统计工具（按星聚类 bootstrap） |
| `s04_inventory.py` | 从 `实验/shared/data_catalog.json` 枚举 + 逐帧读头几何 + CRVAL 天区一致性断言；不合格帧/缺几何帧逐帧登记 |
| `s04_measure.py` | `git show` 取出 `ceec03552` 与 HEAD 两版 `step8_real_frame.py` 原样运行；合规 8 帧两套方差口径逐帧测量；原生 MC tag 交叉核验 |
| `s04_strata.py` | 逐星表落盘；颜色/星等/位置/SNR 分层系数 + bootstrap CI；同帧与跨仪器训练–留出；判据样本存活率诊断 |
| `s04_sigmaflat.py` | 极简 XISF 单块读取器；平场母版高通/低阶结构；真实星位置 PSF 加权直接口径；重复位置检验；σ_ceiling(σ_flat) 灵敏度；直接施加母版平场的 Δσ_obs；σ_pix_e 审计 |
| `s04_linearity.py` | 同夜同指向 300/600 s 通量比 vs 信号；同曝光零点；饱和序列；线性区判定（`NOT_MEASURED` + 已扫范围） |
| `s04_negatives.py` | P1-D 五个负例（错通带/QE、G 双计、颜色项截断三腿、空间场阶数扫描、FITS 标度三种读法），每个都先写声明方向 |
| `s04_acceptance.py` | 从 `results/*.json` 读值组装「验收格式」记录与快照哈希 |

## 纪律约束（脚本内已实现，勿放宽）

1. **不改任何既有实现**：`s04_common` 只 import `实验/photometric-magnitude/code/` 的既有模块，
   不做 monkeypatch；两版历史脚本用 `git show` 取出到 `run/.../cache/hist_step8/` 后运行。
2. **判据式与阈值一字不改**；`σ_flat` 只接受 `SIGMA_FLAT_HF_CANONICAL` 或独立测量值，
   绝不用 `calibrate()['delta_after_m']` 回灌（自指会造出恒 PASS 的假通过，见负例 N3b）。
3. **同一颗星跨帧重复不算独立样本**：所有 bootstrap 一律按 `star_index` 聚类。
4. **跨变体比较固定 MC tag** `s04`（`σ_ceiling` 的 tag 依赖 0.32%，见 `s04_measure.py` 的交叉核验）。
5. 全部写出重定向到 `run/ACSDSDOC-02/S04/**`；不写 `实验/**/results/`（`.gitignore:187` 明禁回流）。

## 环境

Python 3.13.5 / numpy 2.2.4 / scipy 1.15.3 / astropy 7.0.1 / matplotlib 3.11.2；
无 photutils。`gcc` 可用（`scia_gaia.build_dumper` 会编译 `gaia_xp_dump` 到
`run/ACSDSDOC-02/S04/cache/bin/`，**不写** `run/SCI-401/`）。
