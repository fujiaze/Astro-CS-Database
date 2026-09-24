# Phase2 目标态详细设计

> 上游：ASTROCS_DESIGN.md §5（mosaic：相对定标·排异·集成）

使命：把一组合同兼容的 Phase1 球面产品相对定标、排异并合成为可继续测量的马赛克；对不同科学目标提供明确的最优统计量，而不是一个万能 weight。

## 1. 输入与兼容性

输入可来自不同 Phase1 运行，但必须兼容：天球 frame、滤镜/波段、signal 物理语义、通量尺度可变换、PSF/噪声模型可解释、产品 schema 和 provenance 完整。混合积分通量/面亮度、未知单位、缺少必要响应或损坏 manifest 时拒绝。

输入的 signal 是**线性**面亮度（单位随产品 `BUNIT`，Phase1 面亮度产品为 `ADU/sr`；逐帧相对测光零点由 `PHOTSCAL`/`PHOTAPPL` 承载，量纲正本见 `docs/contracts/DATA_SEMANTICS.md` §31.1a）：本阶段的全部运算是**线性**的——UPM 加性天光校正（`y_k = s + C_k + ε_k`）与逆方差加权求和（`signal = Σ w_i·x_i / Σ w_i`）——因此**不做星等换算**（星等是对数量，星等的加权平均在物理上无意义；星等只在派生/展示时按 `m = ZP_k − 2.5·log10 F` 换算，见 `docs/design/PHASE1_DETAILED_DESIGN.md` §7.1）。

输入产品的**落盘形态不进入科学语义**：`hips_paths` 的元素可以是裸 `<name>.hips/` 或归档 `<name>.hips.zst`，调用方不感知形态（形态由落盘名判定，见 `docs/design/PRODUCT_STORAGE_FORM.md`）。**`hips_paths` 的元素保持字符串**（不做元素对象化）：逐帧产品级索引路径由命名规则派生 —— `<name>.hips` / `<name>.hips.zst` → `<name>.hips.index.json`（与 `hips_path` 同父目录）。按天区查输入帧走**块级覆盖索引**（数据集级 `coverage.index.json`，不压缩；由**加性可选键** `coverage_index` 显式引用，缺失时由各产品级索引现场倒排），不逐瓦片探测：索引给出块 → 候选帧集合与覆盖分数，**像素级裁决仍由 support/validity/排异语义执行**。输入合同**不设** `storage_form` 键——阶段二产物固定裸形态（服务面），出现该键（含 `archive` 值）一律 REJECT。

## 2. 固定科学流程

`admit → coverage graph → control sampling → global relative photometry/background model → apply normalization → rejection inference → science-objective integration → product validation → atomic publish`。

每步产物持久或可重建，七个 operation 逐一具名调用。

SNR 重建口径由 JSON 显式指定：`dense`（稠密帧内 SNR）、`sparse_reconstruct`（默认，稀疏控制点插值重建）、`frame_reconstruct`（仅帧级）；实际生效口径记录在 `snr_path_effective`。三条口径都**直接**产出同一物理量 `SNR = F_ref/σ_F` 的稠密表示，只在重建方式上不同：`sparse_reconstruct` 由稀疏**绝对** SNR 控制点重建为稠密场（控制点值即绝对信噪比本身，不再乘/除帧级标量）；叠加权重由 SNR **现场换算**为逆方差 `w = SNR²/F_ref²`（`F_ref` 为逐帧参考通量），不由上游落盘（`ASTROCS_DESIGN.md` §5.3）。

## 3. Coverage 与重叠图

建立帧/区域重叠图，记录有效面积、信息量和连通分量。无连接分量不能假装在同一零点/背景基准；输出分组件或 fail-closed。coverage 是几何/有效域，不是权重。

## 4. UPM：联合相对模型

对重叠区域拟合每帧**加性**天光背景（UPM 加性校正场）：

```text
y_k(x) = s(x) + C_k(x) + epsilon_k(x)      # 纯加性（g_k ≡ 1）
```

- `C_k(x)` 是加性天光背景（校正场）；**不引入乘性 `g_k`**（恒等；乘性残留归 Phase1 低阶空间增益，见 `docs/science/PHASE2_UPM.md` §14a）；
- 星点掩膜之外每帧取稀疏背景采样点，采样点权重取**噪声逆方差 `control_ivar`**：被估量是变化的背景电平，`SNR²` 在该处不是有效逆方差代理；SNR 只作 veto/质量门（`ASTROCS_DESIGN.md` §5.4）；
- 约束 gauge，报告**可辨识性判决与读数**（判在**未正则化**的列均衡数据信息矩阵上，唯一相对阈值；欠定与病态是同一条不等式的两种读法）、连通性、残差和参数协方差；
- 参考天光面的节点间距由**输入几何**导出（上界 = 重叠带宽度与指向间距的一半取小，下界 = 数据自身分辨率极限），并作为自适应回路的**唯一旋钮**；几何量缺失 ⇒ fail-closed，取值只来自输入几何；
- 控制点避开源、饱和、坏点和高结构区域；
- 校准参数不确定度必须传播到最终 covariance；
- 欠定、断图、不可辨识或显著模型失配显式失败/分组件；判据**只**设在未正则化的求解矩阵上（正则化后条件数有上界 ⇒ 恒真门），正则化矩阵的条件数只作诊断。

## 5. Rejection

排异是潜在污染状态的估计，不是把异常值变成零：

- cosmic ray、卫星线、坏列、移动源、云/梯度、失焦/拖线分类型；
- 阈值使用预测残差方差，包含 Phase1 噪声和 UPM 参数不确定度；
- 小样本规则、迭代上限和方法版本化；
- 输出 rejection mask/count/reason/probability；
- 移动源等科学信号可选择保留到独立层，不默认当缺陷删除；
- NaN 采用**样本级掩膜**：污染样本掩除后**重归一**、覆盖级缺数置 NaN 并**强制计数**，掩膜动作逐条计数。

### 5.1 逐像素排异路由（最终五档表）

路由依据 `N` = 该输出像素的**几何覆盖帧数**（coverage footprint 一次解析），与掩膜后存活数、整组帧数都无关：

| N（几何覆盖帧数） | 算法 |
|---|---|
| 1 ≤ N ≤ 3 | none：不排异，直接逆方差加权积分 |
| 4 ≤ N ≤ 5 | percentile clipping |
| 6 ≤ N ≤ 15 | winsorized sigma clipping |
| N ≥ 16 | linear fit clipping |

生产排异算法集 = none / percentile / winsorized / linear fit；min/max 极值法**不用于生产**。实际方法、参数与 N 写入 `rejection` provenance（权威表见 `ASTROCS_DESIGN.md` §5.5；算法出处、合法性窗口与合成 Oracle 正负例见 `docs/science/REJECTION.md`）。

## 6. 两类目标产品，不能混用权重

### 6.1 扩展源/面亮度马赛克

对同一输出 sky element 的估计：

```text
s_hat = (Aᵀ C⁻¹ A)⁻¹ Aᵀ C⁻¹ d
```

工程近似为独立样本时才退化到像素 ivar 加权平均。Drizzle 相关噪声、共同 master、UPM 参数和重叠重采样导致的协方差必须纳入 `C` 或以 correlation kernel/低秩项近似。输出 signal、variance、相关描述、effective PSF、support、coverage、rejection。

### 6.2 点源最优检测与测光

每帧充分统计量：

```text
Q_k = a_k P_kᵀ C_k⁻¹ d_k,    W_k = a_k² P_kᵀ C_k⁻¹ P_k
```

独立帧时：

```text
Q = Σ_k Q_k,    W = Σ_k W_k,    F_hat = Q/W,    Var(F_hat) = 1/W
```

这条产品线显式消费 Phase1 的 PSF、光度响应和噪声/协方差。普通像素 ivar coadd 在 PSF 不同时不保证最大点源 SNR；两者不等价，声明面按此区分。目标产品可采用：

- detection statistic / score map；
- point-source information map `W`；
- flux estimator map (Q/W)；
- effective/proper coadd PSF；
- 或经证明信息保持的 proper coadd 表示。

### 6.3 权重的来源与产生链

Phase2 **不消费**任何来自 Phase1 的相对权重产品：权重一律**按该天球像素对应的帧集合现场算出**（派生量）；
Phase1 与 Phase3 **不产生、不消费**权重。PSF 拟合质量代理（`q_psf`、残差尺度）**只作诊断**，**权重面排除**该项（`ASTROCS_DESIGN.md` §2、§3.1）。

**产生链固定为两步、没有可选择项**：

~~~text
阶段一  只生产信噪比：稀疏 SNR 控制点（控制点存绝对 SNR，不乘/除帧级标量）
阶段二  叠加前先算真实信号面：用每帧的稀疏控制点重建稠密控制点 / 稠密 SNR 面
        → 取逆方差（最优功率）定权  w(x,y) = SNR(x,y)^2 / F_ref^2 = 1 / sigma_F(x,y)^2
        → 叠加
~~~

§2 的三条 SNR 重建口径（`dense` / `sparse_reconstruct`（默认）/ `frame_reconstruct`）是**重建方式**的选择，不是权重口径的选择：三者都产出同一物理量的稠密表示，都走同一条逆方差定权式；实际生效口径记入 `snr_path_effective`。**不存在**可选的权重口径、口径选择键、口径枚举或口径配置项；越界 token 一律 fail-closed（`FZ-WEIGHT-SINGLE-PATH` / `FZ-MODE-RETIRED` / `FZ-FIELD-WEIGHTMODE`）。

## 7. 空间变化与压缩

Phase1 的 PSF/information/noise 若为空间模型，Phase2 必须在输出位置求值。只有误差门证明标量近似对最终 `W`、flux bias 和 detection power 的损失低于阈值，才可用一帧一个数。压缩误差进入 manifest。

## 8. 确定性与并行

分块/并行只改变执行，不改变归约次序或科学结果。跨 tile source/rejection 状态有确定边界协议。内存不足时重新分块，不静默降低统计模型。

## 9. Phase2 输出

同一马赛克包可含彼此明确的产品族：

1. `surface_brightness`：signal、variance、correlation、effective PSF；
2. `point_source`：Q、W、flux、detection statistic、effective/proper PSF；
3. support、coverage、validity、rejection；
4. UPM 参数、协方差和残差诊断；
5. manifest：输入列表/哈希、目标函数、权重模型、近似、排异和 provider。

manifest 写全每张图，且每个 weight 具名。

Phase2 产物是**服务面天球数据库**，落盘形态固定为**裸 `<name>.hips/`**（被随机读取，不引入解压延迟）；同样写出产品级索引与数据集级覆盖索引，供 Phase3 按天区查。

## 10. 验收

- 独立高精度矩阵/NumPy oracle 验证 GLS、Q/W 与 covariance；
- 注入点源满足 `SNR_combined² ≈ Σ_k SNR_k²`（独立、模型正确条件下）；
- 不同 seeing/透明度/背景组合优于或等于普通 ivar 图像叠加的点源检测功率；
- 扩展源常量场、梯度、总通量与方差无偏；
- UPM 断图/欠定/不可辨识、排异小样本、零信息量和相关噪声失配能红；拟合不收敛或判红时产品**照出**、`warning_codes` 非空、构建 rc **不变**；
- M42/银心真实数据检查接缝（**有符号**电平台阶门槛 + 适用域，见 `ACCEPTANCE_SPEC.md` §6.2）、背景、星形、卫星线、黑洞和预测/实测噪声。
