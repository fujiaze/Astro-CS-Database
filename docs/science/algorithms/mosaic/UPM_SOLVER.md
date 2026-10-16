# 背景控制权重与局部 M+C 求解补充

本页说明现有UPM库的背景控制权重和M+C数值模型，科学目标与规范由[天空模型](../../sky/UPM.md)拥有。当前mosaic生产拟合/施加走p2_sky_plane_build_joint及delta接口，不能把本页旧M+C内核自动等同生产扣除链；入口定位见[Mosaic模块](../../../detail/mosaic/modules.md#upm-fit)。

## 控制估计量与权重

背景控制观测z_kc、其标准差σ_kc和quality分别给统计测量与质量信息。库的control-ivar路径取

\[
 w_{kc}^{\rm raw}=q_{kc}\,\operatorname{control\_ivar}_{kc},
 \qquad \operatorname{control\_ivar}=1/V(z_{kc}).
\]

控制中位数的既有近似为V=k_corr·(π/2)·σ_bg²/N_retained；iid高斯的渐近式、Drizzle相关和污染适用域分别解释，不能将点源SNR或单像元逆方差直接当背景控制精度。值和权重的实际单位由输入标度决定，不固定为原生ADU。

库另有use_ivar_weight=0的诊断式q·support^p·snr²/(1+snr²)/max(abs(uncertainty),sigma_floor)²，必须保留诊断身份，不重新变成产品权重模式。当前control-ivar路径遇非正/非有限civ明确返回错误。

每控制格的归一化采用

\[
 \widetilde w_{kc}=\frac{w_{kc}^{\rm raw}}{\sum_j w_{jc}^{\rm raw}}
                    \operatorname{control\_reliability}_c.
\]

这是无量纲格内份额，与原始绝对精度不是一个量。归一化改变不同控制格的相对贡献，不能因为和为reliability就宣称仍在求原绝对逆方差目标；科学解释与误差需单独论证。

## M+C与稳健迭代

局部模型z_kc≈M_c+C_kc，r=z−M−C，z_std=r/σ_eff，σ_eff=max(abs(uncertainty),sigma_floor)。Huber损失为小残差区z_std²/2、大残差区δ(abs(z_std)−δ/2)，IRLS因子分别为1与δ/abs(z_std)，既有δ=1.345无量纲。实际权重、尺度、平滑和弱锚共同决定目标，不能仅由Huber名字推出全问题最优或收敛保证。

内核按控制/帧图固定分量规范，在公共控制场和逐帧场间迭代，帧内低维系数可经CG更新，再按需要物化稠密缓存。既有弱零锚默认1e−3，选择规范与抑制不稳定不等于恢复绝对天空，也不由这个数值直接推出偏差低于0.1%。单帧区的延拓是所选模型约定，不产生独立观测信息。

M+C求值和生产raw−δ_k分开；当前生产δ相对公共连续信号面定义，不能用旧C双线性求值接口替代。同名端口或历史模型文件不是算法仍在生产的证明。

## 秩、收敛、并发和精度

有效边决定连通分量，参考帧身份和frame-index映射必须一致。构建返回成功与迭代达到容差分别报告，p2_upm_convergence提供后者，耗尽不能当已收敛。不同帧/控制的并行可改变聚合结合次序，同配置与跨worker比较分开；线程预算由宿主传入，源码存在调用内线程池不等于已经满足全部统一资源纪律。

稠密与稀疏互读是同一模型表示的一致性测试，不是独立科学真值。旧1e−12与单次误差/耗时属于相应构型，不能推广成无量纲全域误差保证。k_corr≥1及拒绝1的现有代码政策、有限样本中位数和负相关适用域的疑点见[Q31](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)，本轮不改控制方差算法、阈值或扣除模型。

独立验证应覆盖有效图断连、规范、已知加性场、真实低频保留、有限样本与相关控制统计、污染、未收敛和空观测，完整生产背景链还需其自身入口与反例。旧实测表、测试ID、逐符号锚和文献完整保存于[来源档案](../../../../实验/documentation-refactor/docs/MOSAIC_REDUCERS_SOURCE.txt)，本轮仅阅读源码与文档。
