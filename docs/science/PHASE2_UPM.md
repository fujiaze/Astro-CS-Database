# Phase2 UPM Science

## 目的

多帧覆盖并集上建立**一个**联合加性光度模型（UPM），消除逐帧背景/零点差。

## 科学定义

对帧 f 在像素 p：

```text
calibrated_f(p) = raw_f(p) − C_f(p)
```

C_f 为帧 f 的空间加性校正场（8×8 control cell 双线性）。

求解：Huber IRLS + control-ivar 感知权重 + 弱零锚 + 连通分量独立 gauge
（每分量参考帧 = 最小 frame_id）。

## 权重（V19R3 冻结：SCI-UPM-WEIGHT-001 / ALG-UPM-CONTROL-IVAR-001 /
DATA-UPM-CONTROL-UNC-001）

```text
w_UPM = quality_factor × geometric_reliability × control_ivar
# raw=quality·control_ivar；normalized=raw/sum·geom（per-control，见UPM_SOLVER.md:45与p2_upm_normalized_weights）
control_ivar = 1 / control_variance
control_variance = k_corr × (π/2) × sigma_bg² / N_retained
```

含义：
- control estimator = background-clean patch **median**（不是单像素 leaf）；
- control_variance 是其统计方差：Drizzle 输出协方差由 k_corr>=1 表征
  （N_eff = N_retained/k_corr < N_retained）；
- N_retained 用 clipping 后保留样本（不是裁剪前 n_total）；
- k_corr 由当前 Drizzle synthetic noise/covariance MC 校准（UPMW-005
  control_median_mc_test：pixfrac=0.8 生产默认，2000 实现，
  k_corr_empirical=1.3883，N_eff≈181/251），冻结保守值 1.4；per-frame覆盖 frames[frame_id].kcorr>0 ? per-frame : cfg.control_k_corr（sampler.cpp:672），缺省回退1.4；
- 禁止 production science 模式乘 star SNR / snr²/(1+snr²) / support^p
  （support 只作 eligibility/coverage 诊断）；legacy snr²/(1+snr²)/unc²
  仅 use_ivar_weight=0 的 ablation/诊断（SNR-015）；
- obs.ivar（单 leaf Phase1 ivar）V19R3 弃用为诊断字段，禁止进入科学权重。

几何可靠性（geometric_reliability）在 per-control 归一化中施加
（p2_upm_normalized_weights × control_reliability）。

硬门：UPMW-001（snr 扰动不变）、UPMW-002（control_ivar 1:4 → weight
1:4）、UPMW-003（星群不变）、UPMW-004（独立 Gaussian Var(median)
≈ πσ²/2N）、UPMW-005（Drizzle 相关 MC）、UPMW-006（无 legacy SNR
consumer；production 缺 control_ivar 显式 rc=2）、UPMW-007（patch
estimator vs truth）。

## 持久化绑定（SCI-UPM-PERSIST-001 / ALG-UPM-FRAME-BIND-001）

```text
parameter_rows[index] ↔ frame_id_by_index[index]     # 同长、无重复
```

绑定只由稳定 frame_id 决定；保存/重开不得改变 frame_id→θ 映射；
禁止从有序容器遍历重建（DATA-UPM-MODEL-001；见 `lib/phase2/src/upm.cpp:6` 与 `lib/astro_image_io/src/aio_upm.cpp:4` 锚点）。
payload含signal/support tile float32 LE bytes裸字节，跨endian理论不同id，当前仅Linux x86_64路径（sampler.cpp:250-364），DATA-FRAME-ID-001。

## 变量/单位

- C：加性场（信号单位）；theta：control 系数；
- control_variance：信号²；control_ivar：信号⁻²；
- frame_id：稳定科学 payload 标识（uint64）。

## 假设

- 帧间无乘性尺度差（乘性 photometric scale 已撤销）；
- 控制点 SNR 与几何解耦（V4 R6 snr_available 语义）。

## 有效域

- ≥2 clean 帧共同覆盖控制点；单帧区由几何节点 harmonic continuation。

## 不保证

- 不保证跨滤镜统一（filter 分组由调用方保证）。

## 失效条件

- 无重叠/无控制点 → NO_DATA；畸形模型文件 → ERR-P2-UPM-001。

## 数值精度

FP64；dense cache 与 sparse 求值 1e-12 等价门。

## 参考文献

工程控制/docs/PHASE2_INTERFACE_FREEZE（W2 冻结）；SNR_REDESIGN_CONTRACT。

## ID

SCI-UPM-001..010；SCI-UPM-PERSIST-001；SCI-UPM-WEIGHT-001；
ALG-UPM-FRAME-BIND-001；ALG-UPM-CONTROL-IVAR-001；
DATA-UPM-MODEL-001；DATA-UPM-CONTROL-UNC-001；ACR-IVAR-001；
TEST-UPMW-001..007；DATA-FRAME-ID-001（frame_id = truncated-64
canonical SHA-256 of science payload；见 docs/contracts/DATA_SEMANTICS.md）。
