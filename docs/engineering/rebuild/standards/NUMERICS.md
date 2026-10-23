# 共享数值标准

本篇是所有process共用的数值原语与证书标准，不是调度节点。由src/numerics唯一实现；process声明其采用的具体算术/停止条件。框架只调度和计费，不决定科学rank、gauge或likelihood。

## 数值求解、可估性和停止的具体步骤

所有列先除其2范数（精确零列保零）；参数结果和nullspace映回原单位。稠密SVD的singular_error_bound至少含分解残差、矩阵输入区间界及舍入项的谱范数上界。rank_relative=1e−12只是报告分类：σ下界>threshold为可估，σ上界<threshold可列数值不可估，误差区间跨threshold则rank_unresolved并升级算术/细化。任何真实零空间须有解析/gauge结构证据或明确conditional numerical_rank；不能把任意阈值删去的小但真实非零模态称精确物理不可辨识。

SPD模型优先因子白化：C=F Fᵀ小系统Cholesky，病态用对称eigen。负特征值小于−spectral_error_bound即invalid_covariance；跨0的谱为precision_unresolved，精确零须输入结构或认证证书。PSD零空间U0先求 D0θ=y0（D0=U0ᵀD），不一致超过误差界invalid_model；求可行θp+Zη，再在正谱子空间白化GLS。exact约束可决定F并给V=0。伪逆只在已明确参数子空间中使用，不把 C+ 当总体精度。

大系统首选 sparse QR；内存不能承受时LSMR，停止同时满足缩放残差 ||Dᵀr||∞≤abs_res+rel_res||D||1||r||∞ 和已要求的输出泛函残差界，且迭代数未超。默认 rel_res=1e−10、abs_res=1e−12（输入白化无量纲空间），它们不能代替科学 tolerance；未给有限下游误差证书仅conditional。仅covariance action时KKT+MINRES，内层求解的残差/作用误差纳外层证书。预条件器不得加ridge改变目标，可以只作迭代加速。没有验证大系统nullspace能力时返回not_identifiable/geometry_solve_unresolved，不用LSMR收敛自动证明唯一性。

非线性PSF/photometry：每步按所属process的likelihood/score求GN/真实score导数，Armijo参数1e−4，回溯≤32次，先检查当前驻点，小步/梯度与输出误差满足则converged；非驻点更新必须likelihood下降；默认step=1e−10、gradient=1e−8、outer≤100。非驻点且目标不下降/overflow回溯失败nonconverged，不裁参数到任意物理值。covariance/influence只条件于报告的线性化模型，未知模型偏差不由Hessian变已知。

### BLUE、PSD和非负QP

SPD Γ：QR/Cholesky解 Γq=1，检查q/denominator误差；1ᵀq下界>0才给t=q/(1ᵀq)。PSD：先检查是否存在Γ零空间向量v且1ᵀv!=0；存在则有零方差归一解，取最小范数归一解并保存精确约束。否则在range(Γ)解q，若1在range外的分量未定则精度/秩未决；禁止用epsilon地板。权的数值区间由统计式3d传播，σ或分母太弱返回geometry_solve_unresolved。

非负QP用active-set：从equal可行解开始，在当前自由集解equality-constrained PSD QP；候选有负权时沿当前到候选线段走到最先碰0者，按frame ID打破平手并加入active；可行候选后计算KKT梯度，若active有负reduced gradient则放开最小者，反之最优。每步至多K个线性约束变化，max_iterations与work均限制；退化循环时按frame ID Bland规则。精确PSD多解取最小2范数最优权。停止验证sum=1、t≥0、stationarity和complementarity的区间残差，目标方差/权泛函误差必须在预算；max_iterations后nonconverged。Γ未知时显式QP/BLUE unavailable，automatic才equal并列原因。不同均值mixed结果的方差可算，但不是共同量最佳无偏估计。

LOO只计算一次所有候选q，从最大|q|超过Φ⁻¹(1−α/(2N))者拒一个，相等按frame ID；v_res必须有严格正下界，精确0且残差也0无需拒，0且残差被精确约束否定则模型不一致，不能除0。数值区间跨阈值是selection_unresolved：细化/升级，或显式固定实际分支conditional；默认不以任意中点决策认证成功。

## 驻点、舍入停滞和非线性失败

非线性每次迭代在线搜索前检查当前有限有效likelihood、可估空间的缩放score、参数/输出泛函误差；score与预测步均达请求阈值时允许零步收敛，likelihood不需再下降。无噪声初始化已经精确拟合的情形必须成功。仅对尚未满足停止条件的非零更新要求Armijo下降；步被舍入为零但score仍大是precision_exhausted/升级，不是收敛。非驻点且所有回溯失败才nonconverged。该判断先于Armijo，所有非线性入口复用同一停止实现。

## 可计算的SVD/eigen误差证书

对数值SVD A≈UΣVᵀ，先向外求δU=||UᵀU−I||F、δV=||VᵀV−I||F及eR=||A−UΣVᵀ||F，δU/δV必须<1。极分解正交因子的距离界取eU=δU/(1+sqrt(1−δU))、eV同式，则奇异值绝对误差可取eR+eU||Σ||2 sqrt(1+δV)+eV||Σ||2，加输入矩阵区间的Frobenius半径与全部舍入界。所有范数、sqrt、乘加向外。δ≥1或界跨rank阈值则升级/未决，不省略正交缺陷。对称eigen A≈UΛUᵀ同理用δU及重建残差，给λ区间并区分显著负/零未决；精确零约束仍须结构来源。大系统无法给证书时只conditional数值秩，不升级为certified。

## 拟合及模板数值误差不跨模块遗失

PhotoSolution数值参数包围使用可估空间的score根验证：在候选盒X内有向求score g(θ0)和Hessian H(X)，取QR/SVD近似逆G，K=θ0−Gg(θ0)+(I−GH(X))(X−θ0)。K严格在X内部且||I−GH(X)||∞<1时给局部唯一驻点包围；Hessian正下界证明局部极小。GN/IRLS声明的条件模型使用其实际score，不以步长或随机covariance替代数值证书。exp/多项式/像元积分导数用同一解析模型的区间AD，Gaussian/Moffat可解析微分，Airy的J0/J1用MPFR有向值和Bessel导数递推，0附近用幂级数及下一项余界避免0/0。latent分块可先按给定θ盒用带误差线性解消元，再验证全局score；资源不足或不能证明正秩/盒内唯一则保conditional/unconverged，不擅自给零半径。

谱v误差、PSF积分区间、counts数值包围均进入score区间；根盒再经α=exp(−hθ)和校准/重组原生区间传至x，R分子使用带符号x区间。template经signed transfer的绝对误差action在PSF结果中保存，information/sky/coadd同理消费；缺必须的绝对系数能力就返回未决/条件状态。这个数值账本与ξ根随机传播分开。所有局部验证只证明声明模型的局部解，不伪称非凸全局最优或真实物理模型已知。


## 验证线性解的矩阵扰动界

对已固定可估/gauge子空间M=M0+ΔM、|ΔM|≤EM，近似逆G满足ρ=|| |I−GM0|+|G|EM ||∞<1时，使用(1−ρ)的正下界及残差/右端误差给有限解包围；所有量向外求界。ρ跨1时升级/细化或geometry_solve_unresolved，不加ridge改目标。该共同证书由numerics提供，所属process给出其真实系统、输入区间和所请求输出泛函；非方阵先按已声明QR/KKT/子空间装配，不能直接给不存在的逆。
