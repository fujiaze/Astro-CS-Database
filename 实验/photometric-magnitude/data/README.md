# SCI-A · data（数据来源与生成方式）

本实验单元**不新增受版本控制的数据文件**。所有输入数据来自仓库内既有资产或公开服务，
所有中间产物落 `run/SCI-401/`（gitignore，不入库）。下表给出每个数据集的来源、
生成方式、SHA256 与复现命令。

## D1 · HST M16 真实信号模板（真实观测）

- 路径：`testdata/HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits`（仓库内既有）
- 性质：HST/WFC3-UVIS F657N HLSP drz，单 HDU，8400×8000 float32，`BUNIT=ELECTRONS/S`，
  `EXPTIME=9600`，`PHOTFLAM=2.2290223e-18`，`PHOTPLAM=6566.60545`，`PHOTBW=41.015`
- 用途：提供**真实的大尺度天光/星云结构**作为仿真背景；仅取 24×24 分块平均后的
  ~0.95"/px 模板（与 testdata FLI/KAF-16803 系统同量级采样）
- 本实验**不修改**该文件，只读

## D2 · Gaia DR3 XP 光谱（真实天体 SED）

- 来源：仓库内 `lib/infrastructure/gaia_xpsd_client`（本地 DR3SP 库）+ 仓库内
  `gaia_client.c`（`实验/SCI-A/code/gaia_xp_dump.c` 直接链接它，只读）
- 查询：M16 场 `(274.7216, −13.8415)` r=0.075°，G<21.5 → 208 源；
  真实帧场 `(83.2833557851, −6.37428025059)` r=0.30°，G<18.0
- 缓存：`run/SCI-401/data_cache/gaia_xp_*.csv`
- XP 采样：343 点、336–1020 nm @ 2 nm，uint8 + `flux_min`/`flux_mul`；
  `F(λ) = byte·flux_mul + flux_min` [W m⁻² nm⁻¹]
- 用途：注入星场的**真实 SED 形状**（星等为已知真值，SED 形状取自真实天体）

## D3 · testdata 真实帧（底参照）

- 路径：`testdata/` 内 FLI 相机帧（M42 M1 T2 Red 300 s 等），uint16 + `BZERO=32768`
- 用途：真实数据底参照——帧内仪器参数、WCS 二轮精化、引导 vs 盲检、单帧可自算预算
- 本实验**不修改**这些文件，只读

## D4 · 外部通带曲线（唯一需要网络）

- 来源：SVO Filter Profile Service，`HST/WFC3_UVIS2.{F657N,F673N,F502N}` 总系统透过率
- 复现：`bash 实验/SCI-A/code/step0_fetch_refs.sh`
- 缓存与 SHA256（写入 `run/SCI-401/data_cache/`）：

| 曲线 | SHA256 |
|---|---|
| `svo_HST_WFC3_UVIS2_F657N.txt` | `2af43d2dec10904ca2a3207cbc74a9ca1f83fe02d35b7bfd97832d032ad745cf` |
| `svo_HST_WFC3_UVIS2_F673N.txt` | `fdb18eb39936094323b90e20f06cc88c88412ce9a989c43f22e13cf8fdfa598a` |
| `svo_HST_WFC3_UVIS2_F502N.txt` | `587ef650b0660cb060af58b0267768ecb05d06ef06f17f9d7d19aa8912ac5e3c` |

**离线复现边界（诚实说明）**：无网络时 step0 直接用上述缓存；缓存缺失且无网络则 step3
无法复跑。缓存不入库，因此首次复跑需要一次网络访问。

## D5 · 本实验生成的仿真帧（非受控数据）

| 文件 | 内容 | 生成命令 |
|---|---|---|
| `run/SCI-401/sim/frame_A.npz` | HST 模板 + Gaia XP 注入，透明度 1.0 | `python3 实验/SCI-A/code/step2_hst_sim.py` |
| `run/SCI-401/sim/frame_B.npz` | 同上，透明度 0.62 | 同上 |
| `run/SCI-401/sim/frame_C.npz` | 同模板 + 500 星解析合成位置场 | 同上 |

每个 npz 内含：`img`（ADU 图像）、`m`（真实空间增益图）、`sky_map_adu`、
`mu`（期望电子数图，供精确方差用）、真值 `x/y/ra/dec/mag_gaia/mag_eff`、
模型与注入两侧的合成通量 `f_syn`/`f_syn_inject`、`inject_scale`。

## 数据使用声明

- 三类数据齐备：D1（真实观测信号模板）+ D2（真实天体 SED）+ D3（真实帧底参照）；
  纯解析代数合成见 `step1_analytic.py`（不依赖任何外部文件）。
- 不删改 `testdata/` 与 `testdata/HST_M16/` 内任何文件。
