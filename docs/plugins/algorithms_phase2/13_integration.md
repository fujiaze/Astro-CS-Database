# 插件文档：integration（科学目标集成）

## 1. 职责与边界

- **职责**：按明确科学目标把归一化+排异后的帧集成为马赛克：扩展源 GLS、点源 Q/W、或显式 psfsw_robust 集成。
- **不是**：不产"一个万能 weight"；普通像素 ivar coadd 在 PSF 不同时**不保证**最大点源 SNR，不得宣称等价；不宣称 PSFSW 为 Fisher 最优。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.3（SNR 层检测与**逆方差叠加**）、§4.4（两种目标产品，不能混用权重）
- `docs/design/UNIFIED_MODEL.md`（frame_snr、sparse_snr_layer）
- `docs/science/INTEGRATION.md`、`docs/science/PSF_SIGNAL_WEIGHT.md`
- `docs/design/PHASE2_DETAILED_DESIGN.md` §6

## 3. 输入/输出数据合同

- **输入**：归一化产品组、UPM 参数、rejection、PSF/信息层、帧级 SNR（文件头）、[稀疏帧内 SNR 层]、配置（含权重口径 （已按 §9.73 A44 作废：该概念不存在；权重是阶段二按该天球像素对应帧集合现场算出的派生量））。
- **SNR 路径（三条，配置文件 JSON 显式指定；默认稀疏）**：
  - `dense`（稠密面，精度基准）/ `sparse_reconstruct`（**默认**：稀疏层重建稠密）/ `frame_reconstruct`（帧级重建稠密）；
  - `sparse_reconstruct` 且输入**有**稀疏层 → 实际 SNR = 帧级 × 帧内，参与科学运算；
  - 输入**无**稀疏层而路径为默认/`sparse_reconstruct` → 按帧级执行并**显式记录实际路径**（`snr_path_effective=frame_reconstruct` + 计数），**不得静默**；稀疏层存在但损坏/不可重建 → 明确失败（§7）；
  - 三条路径精度对比为论文核心实验（判据 SP-0；**不得预设稀疏一定最好**，何时哪种最优由实验回答）。（**2026-09-20 订正**：原文「实测同条件帧级标量已最优到 0.06%」**已作废**——§9.48 判「方向对、数字不成立」；SP-0 为诊断量、不作默认开关。）
  - **逆方差叠加**：每个天球像素接收多个源像素输入，用每个源像素的 SNR 计算对应权重（SNR → 逆方差权重），得到最优检测/测光功率——**不是直接用 SNR 加权**。
- **输出**（同一马赛克包可含多个明确产品族）：
  1. `surface_brightness`：signal、variance、correlation、effective PSF；
  2. `point_source`：Q、W、flux、detection statistic、effective/proper PSF；
  3. `psfsw_integration`（被选择时）：四分量、相对权重、conventional coadd、variance/correlation、effective PSF、基线比较；
  4. support、coverage、validity、rejection；UPM 参数/协方差/残差；manifest。
- 参考：`contracts/schemas/mosaic_product.schema.json`。

## 4. 算法与公式要点

### 4.0 SNR 重建与逆方差权重

- `sparse_reconstruct`（默认）→ 实际 SNR = 帧级 × 帧内（由稀疏控制点插值/重建为稠密，重建算子与误差入 manifest）；
- `frame_reconstruct` → 帧级 SNR 重建/直接参与（等权重面）；
- 路径由配置显式选择（默认 `sparse_reconstruct`），是 Phase2 **标准行为**；无稀疏层时按帧级执行并**显式记录实际路径**（不静默）；
- **逆方差叠加**：每个天球像素的多个源像素输入，由各自 SNR 计算对应权重（SNR → 逆方差权重），非直接 SNR 加权；
  **换算口径**：`w_k = SNR_k²/F_ref,k² = 1/σ_F,k²`——`F_ref,k` 是**逐帧**参考通量（`frame_independent_fixed_magnitude`），配对性只要求**同一帧内** SNR 与 `F_ref` 同源（§9.60 / §9.66 A `WEIGHT-FREF-PERFRAME-001`；`07_noise_snr.md` §4.1）；
- **稠密权重是数学表示，工程按需计算**：叠加分块进行（最小单元可为单个像素），**不预计算稠密、不全部加载内存**，用到哪个像素的 SNR 算哪个；全稠密 = 数学等价描述，工程用节省资源的实现并按需权衡 CPU 与内存。

### 扩展源/面亮度

```text
s_hat = (AᵀC⁻¹A)⁻¹AᵀC⁻¹d
```

工程近似独立样本才退化到像素 ivar 加权平均；Drizzle 相关、共同 master、UPM 参数、重叠重采样协方差纳入 `C` 或以相关核/低秩近似。

### 点源最优

```text
Q_k = a_kP_kᵀC_k⁻¹d_k,    W_k = a_k²P_kᵀC_k⁻¹P_k
独立帧: Q=ΣQ_k, W=ΣW_k, F_hat=Q/W, Var(F_hat)=1/W
```

### psfsw_robust 模式

- 显式 `psfsw_robust` 口径 （已按 §9.73 A44 作废：该概念不存在；权重是阶段二按该天球像素对应帧集合现场算出的派生量）：用 Phase1 的 psfsw_robust_weight 做 conventional integration；
- 必须用共同星集/selection-function 门，传播实际线性组合 covariance，输出 effective PSF，与等权/exposure/pixel-ivar/`W_info` 基线比较；
- **不得宣称 Fisher 最优**，除非专项证明。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `weight_mode` | `point_information` | —— | point_information / surface_gls / psfsw_robust （已按 §9.73 A44 作废：键不存在；权重是派生量）（本行仅因 config/config_registry.json 的 plugin_knobs 登记锚点未同步而保留；注销归 FIX-203/FIX-207） |
| `target_product` | 全 | —— | 输出产品族选择 |
| `correlation_approx` | —— | —— | 相关噪声近似方式 |
| `baseline_compare` | true | —— | 是否输出基线比较 |

## 6. 接口/ABI

- entrypoint：归一化产品组 → 马赛克产品族；
- 输出被 export 消费（不要求来自同一进程）。

## 7. 错误与边界

- PSF 不同的输入用像素 ivar coadd 且宣称点源最优 → 禁止；
- 相关噪声无描述 → variance 不完备，标记；
- `psfsw_robust` 口径 （已按 §9.73 A44 作废：该概念不存在；权重是阶段二按该天球像素对应帧集合现场算出的派生量） 缺共同星集/selection function → fail-closed；
- 稀疏 SNR 层存在但损坏/不可重建 → 明确失败（不得静默回退帧级）。

## 8. 测试与 Oracle

- 独立高精度矩阵/NumPy oracle 验证 GLS、Q/W、covariance；
- 注入点源满足 `SNR_combined² ≈ ΣSNR_k²`（独立、模型正确）；
- 不同 seeing/透明度/背景组合下点源检测功率 ≥ 普通 ivar 叠加；
- **SNR 路径**：`dense` / `sparse_reconstruct` / `frame_reconstruct` 三条路径输出正确，精度对比入 SP-0；
- **SNR 重建**：稀疏→稠密重建与帧级铺满重建分别验证；帧级×帧内权重正确；无稀疏层时实际路径被显式记录（负例：静默降级判红）；
- 扩展源常量场、梯度、总通量、方差无偏；
- psfsw_robust 模式与基线比较 + covariance 传播正确；
- M42/银心真实数据检查。
