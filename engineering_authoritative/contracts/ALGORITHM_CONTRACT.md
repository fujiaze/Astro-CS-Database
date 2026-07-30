# AstroCS Algorithm Contract — FROZEN

> **状态：FROZEN（冻结）**
> **冻结日期：2026-07-30**
> **冻结任务：I-001**
> **权威源（SSOT）：本文件 + README §5/§7**
> **实现位置：`lib/healpix_db/healpix_stack/python/e_chain/` + `g_chain/`**

---

## 0. FROZEN 声明

本契约冻结 Stage1 产出契约与 Stage2 算法链的数学模型、参数边界与权重规则。冻结后不得修改已有算法语义。扩展只能通过新增可选参数（不得改变已有参数默认语义）。

---

## 1. Stage1 产出契约（FROZEN）

### 1.1 固定流水线（8阶段）

```
READ_FITS → CALIBRATE → PLATESOLVE → PSF → PHOTOMETRIC → SNR → DRIZZLE → HISS_WRITE
```

每个可观测阶段必须对应真实工作，不得保留空壳状态节点。

### 1.2 PlateSolve 路径（FROZEN）

```
PlateSolve 内部检测一次
├── 用于求解
└── 同步导出同一 detections
    └── PSF 直接复用
```

禁止编排器第二次星点检测。capability 名：`internal_detection_shared_export`。

### 1.3 测光成功判定（FROZEN）

正式成功必须同时满足：
- 有效 Gaia 合成流量
- 合理空间匹配数量
- 匹配唯一性（双向最近邻）
- `fit_used > 0`
- 有限且合理的 `scale_factor`（scale > 0，无上限）
- `sigma_residual > 0`
- 重投影残差无系统偏移

Stage2 不再自由拟合乘性尺度。

### 1.4 HISS 产出（FROZEN）

遵循 HISS_FORMAT_V2.md。四要素：
- signal: float32，禁止量化为 uint8
- support: uint8（0/1），无覆盖不得写成零
- SNR: 稀疏控制点（SoA 三通道），不得全量存储
- provenance: JSON，含 format_version/nside/filter/exposure_s/obs_time/wcs/drizzle/fits_meta/source

---

## 2. Stage2 输入约束（FROZEN）

一个 Stage2 任务默认只接受：
- 同一规范滤镜
- 兼容 HISS V2 格式
- 有效测光定标
- 有效 SNR 模型
- 有效 support
- 能形成连通重叠图的天空覆盖

不同滤镜必须分别生成数据库。重叠图不连通时分成独立分量求解。

---

## 3. 全局加性共识曲面（FROZEN）

### 3.1 只允许加性梯度

```
I_corrected = I_original + C_i
```

`C_i` 为第 i 帧的加性校正曲面。禁止：低频乘性梯度、Stage2 再次自由改变光度尺度、同时拟合乘法和加法。

### 3.2 共识曲面定义

联合拟合全部帧后，在局部 SNR²/逆方差权重下，使全部帧相对于该曲面的加性偏移总体最小的平滑球面共识曲面。

整体零点使用全体帧 SNR² 加权零均值约束，不选择单一参考帧。

### 3.3 数学模型（FROZEN）

观测方程（每个控制点 i 属于帧 f(i)）：
```
signal_i = Σ_m c_m × B_m(ra_i, dec_i) + off_{f(i)} + noise_i
```

- 曲面基底 B：`[1, ra', dec', ra'×dec', ra'², dec'²]`（中心化，D=2 阶）
- 参数向量 x = [c_0..c_M, off_0..off_F]
- 零均值约束：`Σ off_f = 0`（大权重等式行 `1e6 × max_data_w`）
- 加权最小二乘：`min Σ w_i × (signal_i - A_i×x)² + λ×(Σ off_f)²`
- 求解器：`scipy.sparse.linalg.lsqr`（稀疏，稳健，处理秩亏）

### 3.4 禁止项

- 不得使用无权重梯度（必须 W^(1/2)A x = W^(1/2)b）
- 不得选单一参考帧
- 不得加入乘性项
- 不得用梯度模块补偿 Stage1 测光失败

---

## 4. 梯度控制点（FROZEN）

### 4.1 有效区域采样

控制点只从以下区域采样：
- 真实观测覆盖（support=1）
- 非 NaN/Inf
- 非填零
- 非危险边缘

采样策略：分层随机按 dec 分箱，每片目标 800 点。

### 4.2 星点和异常掩膜（FROZEN）

| 掩膜类型 | 方法 | 参数 |
|---------|------|------|
| 星点掩膜 | median + N×MAD-sigma | N=5 |
| 饱和掩膜 | p99.9 百分位 | — |
| 异常掩膜 | NaN/Inf/≤0 | — |

掩膜半径根据 FWHM、流量、饱和程度动态扩展。

### 4.3 稀疏窗口统计

每个控制点来自球面局部窗口，至少包括：
- sigma-clipped median / biweight location
- MAD / 局部方差
- 有效像素比例
- SNR
- support
- 局部结构强度
- 污染比例

---

## 5. SNR² 权重（FROZEN）

### 5.1 联合权重公式

```
w = SNR² × inverse_variance
```

- variance 由该帧有效像素 MAD-sigma² 估计（背景噪声方差）
- variance 下限 = (0.01 × median)² 防止权重爆炸
- 每帧归一化（w_med=1.0）保证数值稳定

### 5.2 SNR IDW 插值

- power=2.0
- k=8（最近邻数）
- 覆盖率必须 100%

### 5.3 权重规则

- 低信噪比帧不能凭借大量噪声样本拖偏高信噪比帧
- 梯度拟合权重与最终叠加权重来源相关，但不得直接复用为同一结果

---

## 6. 稳健排异（FROZEN）

### 6.1 排异时机

梯度校正完成后，逐球面像素执行排异。

### 6.2 MAD 排异算法（FROZEN）

```
reject_sigma = 3.0
reject_mad_factor = 1.4826
threshold = reject_sigma × reject_mad_factor × MAD
```

- 每帧独立计算 MAD
- 排异仅基于残差，与 SNR 无关（高 SNR 坏值仍被拒绝）
- 非支持区域不参与排异

### 6.3 排异输入

- 已校正信号（梯度校正后）
- 局部 SNR
- support
- 覆盖数
- 边缘置信度

---

## 7. SNR² 连续加权融合（FROZEN）

### 7.1 融合公式

```
fused_signal[p] = Σ_f (signal_f[p] × w_f[p]) / Σ_f w_f[p]
w_f[p] = SNR_f[p]² × support_f[p]
```

- 每像素独立计算
- 连续加权（非硬阈值），避免硬边
- 权重和为零时输出无数据，不输出零亮度

### 7.2 融合要求

- 权重随位置连续
- 单覆盖到多覆盖无亮度跳变
- 面板边缘无硬切换
- 无覆盖区不参与

---

## 8. HCSD 产出（FROZEN）

### 8.1 生产数据

- 最终球面信号
- 球面索引（ipix 升序）
- 格式和观测元数据
- 输入 HISS 摘要
- 梯度与叠加参数摘要
- 紧凑质量统计

### 8.2 调试质量层

开关：`write_debug_layers`（默认 ON，生产可关）

开启后保存：
- coverage count
- rejected count
- total stack weight
- gradient confidence
- global target surface
- 每帧加性校正曲面
- 梯度控制点
- 控制点残差
- seam diagnostic
- support

### 8.3 连续性要求

HCSD 不应出现：
- 面板边界跳变
- 覆盖数变化造成的块状断层
- 权重突然变化
- 梯度外推断层
- 无数据区零值污染
- 排异数量变化造成的亮度突变

---

## 9. 实现文件索引（FROZEN）

| 文件 | 说明 |
|------|------|
| `lib/healpix_db/healpix_stack/python/e_chain/e_common.py` | E 链公共约定 |
| `lib/healpix_db/healpix_stack/python/e_chain/e_masks_sampling.py` | E-001 掩膜+采样 |
| `lib/healpix_db/healpix_stack/python/e_chain/e_weights.py` | E-002 SNR²权重 |
| `lib/healpix_db/healpix_stack/python/e_chain/e_solver.py` | E-003 全局曲面求解 |
| `lib/healpix_db/healpix_stack/python/g_chain/g_common.py` | G 链公共约定 |
| `lib/healpix_db/healpix_stack/python/g_chain/g001_reject.py` | G-001 排异 |
| `lib/healpix_db/healpix_stack/python/g_chain/g002_fusion.py` | G-002 融合 |
| `lib/healpix_db/healpix_stack/python/g_chain/g003_hcsd.py` | G-003 HCSD |

---

## 10. 验证基线（FROZEN）

- E-004: 已知梯度/SNR/异常注入恢复测试
- G-004: 银心 Red 30帧（11+9+10）正式叠加
- G-005: 接缝/连续性/排异/权重量化

30帧验证结果（G-004/G-005）：
- 排异率 3.426%
- 融合像素 10,253
- 接缝相对偏差 12.51%
- 连续性突变率 12.80%
- SNR_eff 中位数 450.0

---

**— END OF FROZEN CONTRACT —**
