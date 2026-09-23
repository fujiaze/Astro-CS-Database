# DR3SP 数据库 Schema 审计 (Gate 4, Phase1 Full Freeze v2)

## 数据来源与版本

| 项 | 值 |
| --- | --- |
| 数据源 | PixInsight XPSD 格式 Gaia DR3/SP 数据库 (`XPSD0100`) |
| 文件 | `gaia/GaiaDR3SP/gdr3sp-1.0.0-*.xpsd` × 20, 每文件 ~3.2 GB, 约 1127 万源/文件 |
| 数据库标识 | `GaiaDR3SP`, `DatabaseVersion=1.0.0` |
| 创建 | 2022-11-09, PCL 2.4.30 (Linux) |
| 基础数据 | Gaia DR3 (Gaia Collaboration 2016b/2022k; Babusiaux et al. 2022), BP/RP 平均光谱 |

## 二进制记录布局 (STAR_STRIDE_SP = 40 + 343)

| 偏移 | 字段 | 说明 |
| --- | --- | --- |
| +0 | dx | 投影 X (uint32, 比例 1/(3600·1000·500)) |
| +4 | dy | 投影 Y (uint32, 同比例) |
| +8..+19 | (保留) | 视差/自行等 (未在光谱路径解析) |
| +20 | mag_raw (uint16) | `magG = mag_raw*0.001 - 1.5` |
| +22/+24 | magBP/magRP (uint16) | 发布 BP/RP 星等 |
| +26 | dra_raw (int16) | RA 修正 |
| +28..+31 | (保留) | 标志等 |
| +32 | fluxMin (float32) | 逐星量化参数（PCL `EncodedStarSPData`） |
| +36 | fluxMul (float32) | 逐星量化参数 |
| +40 | spectrum | BP/RP 采样光谱 uint8 × 343 |

## 光谱网格 (header 属性, 与 gaia_client.c 解析一致)

```text
spectrumStart=336, spectrumStep=2, spectrumCount=343, spectrumBits=8
压缩: zlib+sh (byte shuffle), itemSize=1
```

即波长网格 `[336, 338, ..., 1020] nm`, 343 点, uint8 值。

## 关键审计结论

1. **XPSD 不保存 Gaia source_id**: `gaia_client.c` 中 `source_id` 恒置 0
   (行内无 source_id 字段)。因此 per-star DR3SP lineage 不能使用官方 source_id;
   本包采用位置量化哈希 `dr3sp_id` (ra/dec 量化到 1e-4 deg 后 64-bit 混合哈希),
   并在 photometric_match 块中记录。
2. **uint8 由逐星量化参数解码, 绝对刻度已实证锚定**: XPSD 每条记录自带 float32
   `fluxMin`/`fluxMul` (偏移 +32/+36), 解码 `F(λ) = byte·fluxMul + fluxMin` 给出
   **绝对谱辐照度** (W·m⁻²·nm⁻¹)。实证 (任务 SCI-PHOT-FORMULA-01, 26211 颗真实 XPSD 星):
   用官方 Gaia G 通带 (Riello et al. 2021) + GaiaXPy `Gaia_DR3_Vega` 零点 −26.4899 复算合成星等,
   `median(m_syn − magG) = −0.0037 mag`、MAD `0.0033 mag` (G∈[6,18]、解码谱处处为正, n=11272)。
   证据 `run/SCI-PHOT-FORMULA-01/evidence/a1_xpsd_absolute_check.json`、`a2_bandpass_and_absolute.json`。
   **`byte` 数组本身不是与星无关的相对谱形** (量化参数逐星不同, 实测跨 6 个数量级), 因此
   "把 uint8 当相对 SED 再乘 `10^(−0.4·magG)`" 的写法不成立 (见下节)。
3. **无 covariance/quality flag 暴露**: XPSD 仅提供光谱字节与星等, 不提供 XP 协方差。

## 与 GaiaXPy / Gaia DR3 官方数据的关系

- 官方 XP 连续系数 (XP_CONTINUOUS) 可从 ESA Datalink 下载 (本包 `download_xp_sample.py`),
  含 `bp_coefficients/rp_coefficients/errors/correlations/n_parameters` 等完整字段。
- GaiaXPy 2.1.4 (BSD-3-Clause) 为官方 DPAC 合成测光工具; `generate()` 输出
  Gaia_DR3_Vega G/BP/RP 合成星等, 与 Gaia DR3 发布测光 median|Δ| ≤ 0.008 mag (自洽校验)。
- 官方通带: Gaia EDR3/DR3 passband.dat (Riello et al. 2021, A&A 649 A3),
  G/BP/RP 响应曲线 + 零点点 (GaiaXPy XpFilter XML: G=-26.4899, BP=-25.9655, RP=-27.2164)。

## 生产积分约定 (权威: docs/science/PHOTOMETRY.md §2a, claim PHOT-FSYN-CANON-001)

```text
F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ            # W·m⁻²·nm  —— 不含 10^(-0.4·magG)
F_λ(λ_i) = byte_i·fluxMul + fluxMin         # W·m⁻²·nm⁻¹ (XPSD 逐星量化解码)
```

- F_λ: XPSD uint8 经 `byte·fluxMul + fluxMin` 解码的**绝对**谱辐照度 (336–1020 nm @2 nm, 343 点)
- T(λ): 滤光片响应 (Akima 插值到光谱网格, 区间外 0)
- Q(λ): CCD QE (Akima 插值; 未配置时 Q≡1, 显式未建模项; **Q 是通带组成部分**)
- 积分: Simpson 1/3 复合 (末尾奇数区间 Simpson 3/8), 在**谱网格本身**上积分
- 光子计数约定: 权重 λ。依据: Gaia DR3 官方文档 §5.4.1 式 (5.41)
  `⟨f_λ⟩ = ∫f_λ S λ dλ / ∫S λ dλ`; Sirianni et al. 2005 §5 式 (3) 脚注 5
  ("multiplied by λ/hc … as appropriate for a photon-counting detector");
  Bessell & Murphy 2012 式 (A30)。本合同省略与星无关的分母 `∫TQλdλ` (被 location/ZP_syn 吸收)
  与常数 `1/(hc)` (同上)。
- 通带外: T=0 (passband.dat 的 99.99 哨兵值置 0); 完全无重叠 ⇒ F_syn≡0 ⇒ 不能定标

**历史相对口径 (非生产, 仅数值对拍)**: `compute_f_syn`/`compute_f_syn_cached` 曾按
`∫uint8·T·Q·λdλ × 10^(−0.4·magG)` 使用。该写法给逐星 `r_i` 注入 `+0.4·G_i` (dex) 的加性项,
单标量零点吸收不掉 (真实 M42 样本实测 MAD-σ = 0.459 dex = 1.147 mag);
且量化参数逐星不同, uint8 数组不可当相对谱形。**生产定标路径是 `compute_f_syn_cached_xpsd`。**

Gate 4 对比结论 (详见 `gate4_result.json`):
同一 XP 源上, AstroCS 积分约定 (passband.dat + 零点点 + 光子加权平均通量)
与 GaiaXPy 官方合成测光逐星等吻合:

| 带 | median \|Δmag\| | p95 \|Δmag\| |
| --- | ---: | ---: |
| G | ~0.0007 | ~0.010 |
| BP | ~0.0006 | ~0.002 |
| RP | ~0.0012 | ~0.012 |

颜色 (BP-G / G-RP / BP-RP) median \|Δ\| ≤ 0.001 mag。
