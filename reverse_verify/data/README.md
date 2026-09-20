# reverse_verify/data —— 数据类型矩阵（真实数据实例 + 合成数据配方）

> **建立说明（诚实登记）**：本文件由 `M16-SCENE` 分片建立 —— 派单要求「把 M16 登记进
> `reverse_verify/data/README.md` 的真实数据实例列」，但该文件在派单时**尚不存在**
> （`DATA-TYPE-MATRIX` 分片已落地 `data/synthetic/{datasets.json,generate.py}`，
> 矩阵 README 未落地）。本文件**不替代** `DATA-TYPE-MATRIX` 的产出：
> 该分片落地后请**在本文件基础上扩展**（矩阵单元、判据清单），不要另建第二个矩阵文件。
>
> 依据：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.47（论文工作区须按「数据类型」组织
> 真实 + 合成测试）、§9.41（合成必须是真实物理噪声过程）。

## 1. 组织方式

```
reverse_verify/data/
├── README.md                     ← 本文件：数据类型矩阵
├── real/                         ← 真实数据实例（登记，不入库大产物）
│   ├── README.md                 ← 真实实例索引说明
│   └── m16_scene_index.json      ← HST M16 三帧 + 有效域掩膜 + 场景配方索引
└── synthetic/
    ├── datasets.json             ← 合成数据集注册表（单元 → 配方 → 产物 → 判据）
    └── generate.py               ← 可复跑生成入口（按 renderer 键派发）
```

- **大产物**（掩膜 FITS、合成帧 FITS）一律落 `run/`（gitignore），**不入库**；
- 仓库内只保留**可复跑入口 + 小体积元数据索引**；
- 每个矩阵单元 = **真实数据实例**（real_instance）× **合成数据配方**（scene），
  两者都登记，缺一不可（§9.47 第 4 条）。

## 2. 矩阵单元 × 真实数据实例

| 维度 | 矩阵单元 | **真实数据实例** | 合成配方（scene） | 产物目录（run/，不入库） |
|---|---|---|---|---|
| 目标 | `target:bright_nebula_core` | **HST_M16/F657N**（星云核心窗口 `[5504,896,1024,1024]`，块中位 1.93 e/s = 全帧最亮）；`L4-rebuild/norm/t3_m2_red` | `m16_nebula_core.json`、`nebula_core_m42_analytic.json`、`nebula_core_m42_realbase.json` | `run/reverse_verify/data_matrix/m16_nebula_core` 等 |
| 目标 | `target:starfield` | **HST_M16/F502N**（平坦星场窗口 `[3136,768,1024,1024]`，块中位峰峰/中位 0.27） | `m16_starfield.json`、`starfield_general.json` | `.../m16_starfield`、`.../starfield_general` |
| 目标 | `target:dark_lowsnr` | **HST_M16/F673N**（最暗窗口 `[7360,6784,1024,1024]`，中位 0.0115 e/s；F673N 全帧中位 0.0213 e/s 为三波段最低） | `m16_dark_lowsnr.json`、`high_sky_low_snr.json` | `.../m16_dark_lowsnr`、`.../high_sky_low_snr` |
| 目标 | `target:sparse_field` | —（**未覆盖**，见 §5） | `sparse_field.json` | `.../sparse_field` |
| 指向 | `pointing:common_mode_overlap` | **HST_M16 三帧**（CRVAL/CRPIX/CD **逐位相同**，同一天区同输出网格） | `m16_band_matrix.json`、`common_mode_overlap.json` | `.../m16_band_matrix`、`.../common_mode_overlap` |
| 指向 | `pointing:mosaic_diff_pointing` | `L4-rebuild/norm`（6 板块） | `mosaic_diff_pointing_*.json` | `.../mosaic_diff_pointing_*` |
| 条件 | `condition:high_sky_moon` | —（**未覆盖**） | `high_sky_low_snr.json`、`mosaic_diff_pointing_highsky.json`、`sweep_sky.json` | 对应目录 |
| 条件 | `condition:seeing_1.5_to_5.0px` | **HST_M16**（实测星点 FWHM：F673N 中位 2.28 px / F657N 亮星云区 4.6 px，**逐区不同**） | `sweep_seeing.json`、`m16_nebula_core.json`（A6 实验扫 1.0/2.0/3.0 px） | `.../sweep_seeing`、`.../m16_scene/a6_seeing` |
| 条件 | `condition:exposure_30_to_600s` | HST_M16 真实 EXPTIME 9600/14400/16000 s（**与合成曝光无关**，只用于解释底图速率） | `sweep_exposure.json` | `.../sweep_exposure` |
| 仪器 | `instrument:HST_WFC3_UVIS` | **HST_M16**（WFC3/UVIS，0.0400 arcsec/px，8400×8000，NDRIZIM=32） | `m16_*.json` | `.../m16_*` |
| 仪器 | `instrument:0.2m_cmos` … `1.0m` | —（**未覆盖**） | `sweep_instrument.json` | `.../sweep_instrument` |
| 波段 | `band:Ha` | **HST_M16/F657N**（PHOTPLAM 6566.61 Å，PHOTFLAM 2.2290e-18） | `m16_nebula_core.json`、`m16_band_matrix.json` | `.../m16_*` |
| 波段 | `band:SII` | **HST_M16/F673N**（PHOTPLAM 6765.92 Å，PHOTFLAM 2.2397e-18） | `m16_dark_lowsnr.json`、`m16_band_matrix.json` | `.../m16_*` |
| 波段 | `band:OIII` | **HST_M16/F502N**（PHOTPLAM 5009.64 Å，PHOTFLAM 5.2676e-18） | `m16_starfield.json`、`m16_band_matrix.json` | `.../m16_*` |
| 波段 | `band:Red/Green/Blue` | `L4-rebuild/norm/t3_m2_red` | `sweep_band.json`、`nebula_core_m42_*` | 对应目录 |
| 负例 | `negative_control:true` | —（真值「无效应」由**同一真值、逐位相同的重复渲染**实现） | `common_mode_overlap.json`；A6 实验的 P3a/P3b 臂 | `.../common_mode_overlap`、`.../m16_scene/a6_seeing` |

**M16 三波段本身就是「不同数据类型」**（§9.47 第 3 条「按波段」）：
同一片天空、同一 WCS 网格，但谱线结构完全不同 ——
Hα（F657N）亮星云丝状结构 + 密集星场；[S II]（F673N）极暗（中位 0.0213 e/s，比 Hα 暗 10.9×）；
[O III]（F502N）弥散平坦、星点为主。

## 3. 真实实例的「可复跑」入口

| 入口 | 作用 |
|---|---|
| `reverse_verify/synthetic/m16_mask.py` | 由 HST_M16 三帧生成**有效域掩膜**（valid + flags + meta），落 `run/reverse_verify/m16_scene/masks/` |
| `reverse_verify/synthetic/m16_scene.py` | **前向渲染接口**：真实 M16 结构 + PHOTFLAM 星等尺度 + §9.41 物理噪声链 |
| `reverse_verify/data/synthetic/generate.py` | 统一生成入口（`renderer` 键派发；M16 场景走 `m16_scene`） |
| `run/reverse_verify/m16_scene/make_index.py` | 重建 `reverse_verify/data/real/m16_scene_index.json` |
| `reverse_verify/experiments/m16_scene/exp_a6_seeing_aperture.py` | 示范判据（A6 seeing 红绿例 + 负例） |

## 4. 判据登记（每类数据都要能红能绿）

| 判据 id | 内容 | 脚本 | 结果 |
|---|---|---|---|
| `exp1_sky_poisson_snr` | 天光 ↑ ⇒ 散粒方差 ↑ ⇒ SNR ↓（**不得**用加常数代替） | `experiments/data_matrix/exp1_sky_poisson_snr.py` | DATA-TYPE-MATRIX |
| `exp2_mosaic_shape_difference` | 不同指向马赛克 ⇒ 帧间空间形状差异（§9.46 场景） | `.../exp2_mosaic_shape_difference.py` | DATA-TYPE-MATRIX |
| `exp3_variance_closure` | 逐像素方差闭合到解析预测 | `.../exp3_variance_closure.py` | DATA-TYPE-MATRIX |
| `exp_variance_closure` | **M16 合成帧**逐像素方差闭合（含平场乘性响应显式分解 + 掩膜传播） | `experiments/m16_scene/exp_variance_closure.py` | **M16-SCENE，已跑（9/9 PASS）** |
| `exp_a6_seeing_aperture` | **固定真值通量、只改 seeing** ⇒ PSF 域口径孔径无关；5x5 盒和域**不**无关；负例归零 | `experiments/m16_scene/exp_a6_seeing_aperture.py` | **M16-SCENE，已跑（4/4 PASS）** |

## 5. 未覆盖项（诚实登记）

1. `target:sparse_field`、`condition:high_sky_moon`、`instrument:0.2m_cmos…1.0m`
   **尚无真实数据实例**（只有合成配方）；
2. M16 **只有窄带三帧**，无宽带测光帧 ⇒ 无法在真实数据上做「按波段 Red/Green/Blue」的对照；
3. M16 三帧**同指向**，因此 `pointing:mosaic_diff_pointing` 的真实实例只能由 L4 多板块提供；
   M16 只能覆盖 `common_mode_overlap`；
4. M16 三帧是 **drz 合成品**（NDRIZIM=32），**不是原始帧** ⇒ 噪声已相关化，
   不得用于标定噪声相关长度类判据（详见 `real/README.md` 与 `synthetic/README.md` 的诚实边界）。
