# 插件文档：calibration（定标）

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

## 1. 职责与边界

- **职责**：把原始 light 帧转换为定标后的信号（减偏置、暗流、平场），并传播不确定度。
- **不是**：不做源检测、不做背景估计、不做 WCS；不把校准与背景/UPM 混成同一层。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.6（硬约束：校准方差传播）
- `docs/science/NOISE_MODEL.md`（噪声模型与方差传播公式）
- `docs/design/PHASE1_DETAILED_DESIGN.md` §4（校准与方差传播）

## 3. 输入/输出数据合同

- **输入**：原始 light（float 语义）、master bias/dark/flat、cosmetic map、曝光/增益/读出噪声/饱和/非线性/温度/滤镜/观测站元数据（输入为参数 + 路径，见最高设计 §4.3）。
- **输出**：定标信号 `y`（单位声明）、`V(y)`/ivar、validity 标志、写入 manifest 的校准口径。
- 参考：`contracts/schemas/data_light.schema.json`、`contracts/schemas/calibration_output.schema.json`。

## 4. 算法与公式要点

典型 bias/dark 分离路径：

```text
y_p = [r_p - b_p - alpha (d_p - b_p)] / f_p,    alpha = t_light / t_dark
```

独立近似下方差传播（目标态）：

```text
V(y_p) = {V(r_p)+V(b_p)+alpha²[V(d_p)+V(b_p)]+y_p²V(f_p)} / f_p²
```

- **禁止**裁切负值、加未声明 pedestal、夹紧负值；
- 共享 master 的相关性用低秩/相关核/共同 master ID 保留，**不得误当独立**；
- gain、Poisson、read noise、量化、master 方差分别标识。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `bias_path` | —— | —— | master bias 路径 |
| `dark_path` | —— | —— | master dark 路径 |
| `dark_exposure` | —— | s | master dark 曝光 |
| `flat_path` | —— | —— | master flat 路径 |
| `gain` | —— | e⁻/ADU | 由元数据或显式覆盖 |
| `read_noise` | —— | e⁻ | 由元数据或显式覆盖 |
| `clip_negative` | false | —— | 禁止 true（除非负责人批准降级并记录） |

## 6. 接口/ABI

- entrypoint：单一定标入口，输入 light+master 组，输出定标产品；
- 端口引用有效 DATA 合同；所有权与释放按 C ABI 规则。

## 7. 错误与边界

- 缺关键单位/gain/read-noise 口径 → fail-closed；
- master 与 light 尺寸/帧身份不一致 → 拒绝；
- NaN/Inf 输入 → 标记 validity，不静默置零。

## 8. 测试与 Oracle

- 独立解析/Monte Carlo Oracle：注入已知 bias/dark/flat，验证 `y` 与 `V(y)` 与理论一致；
- 共同 master 相关性不变量（不能因重复减同一 master 而方差被低估）；
- 1 worker vs N worker 一致；ISA 等价；
- 注入点源信息权重理论 `σ_F=1/√W_psf` 与实测一致（配合 psf/noise_snr）。
