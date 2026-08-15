# Phase2 UPM Science

## 目的

多帧覆盖并集上建立**一个**联合加性光度模型（UPM），消除逐帧背景/零点差。

## 科学定义

对帧 f 在像素 p：

```text
calibrated_f(p) = raw_f(p) − C_f(p)
```

C_f 为帧 f 的空间加性校正场（8×8 control cell 双线性）。

求解：Huber IRLS + SNR/ivar 感知权重 + 弱零锚 + 连通分量独立 gauge
（每分量参考帧 = 最小 frame_id）。

## 权重（V19 SNR_REDESIGN_CONTRACT）

```text
w_UPM ∝ quality × geometric_reliability / Var(control_estimator)
ivar 优先（obs->ivar>0），否则 1/uncertainty²（SCI-NOISE-015）
```

legacy snr²/(1+snr²) 仅 ablation/诊断（use_ivar_weight=0）。

## 持久化绑定（SCI-UPM-PERSIST-001 / ALG-UPM-FRAME-BIND-001）

```text
parameter_rows[index] ↔ frame_id_by_index[index]     # 同长、无重复
```

绑定只由稳定 frame_id 决定；保存/重开不得改变 frame_id→θ 映射；
禁止从有序容器遍历重建（DATA-UPM-MODEL-001）。

## 变量/单位

- C：加性场（信号单位）；theta：control 系数；ivar：信号⁻²；
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

SCI-UPM-001..010；SCI-UPM-PERSIST-001；ALG-UPM-FRAME-BIND-001；
DATA-UPM-MODEL-001。
