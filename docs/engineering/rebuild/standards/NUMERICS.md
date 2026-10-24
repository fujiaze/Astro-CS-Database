# 共享数值标准

本篇是所有process共用的数值原语与证书标准，不是调度节点。由src/numerics唯一实现；process声明其采用的具体算术/停止条件。框架只调度和计费，不决定科学rank、gauge或likelihood。

**认证前置门槛：** 2026-10-24第二轮源码核对发现基本四则调用前已求值、周期极值遗漏、一般幂与atan2分支切线等包含性阻碍，详见[当前实施状态](../../../audit/IMPLEMENTATION-STATUS.md)。下述是必须实现的合同，不是当前interval.cpp已可信的声明。四则通过不推出超越函数、积分、SVD或复合证书通过；依赖未闭合时上游不得发布certified。

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

## 严格区间原语的执行合同

一个interval表示闭实数集合[L,U]，L≤U；输入binary64按其精确二进制有理值解释，十进制字符串另有解析外包。NaN/颠倒端点为invalid_model。定义域内的值必须全被包住；遇到分母跨0、非法log/幂/角度原点，不把NaN转成零宽区间。内部可表达扩展界；需要有限ExactInterval/JSON而无法给有限界时返回domain/precision_exhausted及无证书，不能序列化假有限端点。

首期认证路径固定为MPFR/GMP有向算术：MPFR以RNDD/RNDU分别求下/上界，GMP保存精确dyadic/有理量。binary64快速路径只在相同原语、定义域、编译器/选项和硬件配置的包含性验证通过后启用；默认普通libm不具有认证能力。MPFR的舍入语义和可用函数以[官方手册](https://www.mpfr.org/mpfr-current/mpfr.html#Rounding)为依据；库提供点值正确舍入，区间定义域划分和极值包含仍由本实现负责。

| 原语 | 必须执行的范围算法 | 停止/域边界 |
|---|---|---|
| +、− | 下界对对应精确端点表达式RNDD，上界RNDU；舍入环境/模式在表达式求值前生效 | `down(a+b)`接收已经舍入的double不构成有向求和；封装须接收操作数或延迟callable |
| ×、÷ | 四个端点组合逐项双向舍入后取min/max；除数严格同号才除 | 分母包含0返回domain；不能跳过不便计算的端点或用epsilon替代 |
| neg、scale、abs | neg=[−U,−L]；scale按factor符号交换端点并向外乘，abs跨0下界0、上界max(−L,U) | 非有限factor不进入有限模型；不能把全部端点正值当abs最小值 |
| hull、intersect、disjoint | hull取最小下界/最大上界，交取最大下界/最小上界；先用精确比较判空 | 无空集表示的内部交helper只接受已证明非空交；上层在空交用Result明确domain/no_support，不造反序区间继续算 |
| sqrt、log、exp、erf、atan | 验证定义域，以单调性在相应端点向外求值 | sqrt需L≥0，log需L>0；overflow不能变有限成功 |
| sin、cos | 向外算两端点，并判定区间是否包含全部可能的极大/极小相位；sin的π/2+2kπ、−π/2+2kπ，cos的2kπ、π+2kπ，以有向π和整数ceil/floor证明存在性 | 不用固定k枚举；相位判定未决就加入±1，宽度≥2π可直接[-1,1]；巨大角精度不足可保该粗界，不伪造窄界 |
| atan2(y,x) | 原点不在矩形时按坐标轴和负实轴切分、逐象限单调求界；结果使用主值[-π,π]的闭包 | 负实轴跨切线的两段若公开类型只能存一个区间，返回[-π_U,π_U]；包含(0,0)为domain，不能以四角最值漏掉±π |
| pow(X,Y) | X严格为正时，统一用exp(Y·log(X))的包含运算；可用已证明等价的单调分段收紧 | X含负值只接受精确单点整数指数，按奇偶和0临界点；负指数需0不在X。X含0且Y严格正可含0；0^0或混合非法实域具名domain，不能只取交叉两端幂 |
| J0/J1及派生Airy | 点值有向求值后，区间通过Taylor/导数范围细分包含；接近0用带余项幂级数 | 两个端点的Bessel值不包围区间极值；不能把点值MPFR能力误登记为完整区间Bessel能力 |

每个操作保存调用前舍入状态并在全部出口恢复；不得更改其它任务的算术环境。若使用硬件fenv，必须证明环境线程局部、禁止编译器重排/常量折叠跨越guard，认证翻译单元禁fast-math和隐式FMA。未验证的平台直接用显式MPFR舍入，不靠一次启动加法检查放行整库。中间MPFR/GMP位长、limb、临时对象和算术work都进入调用预算。

### 必需原语反例与独立Oracle

| case | 冻结输入 | 必须包含/拒绝 |
|---|---|---|
| N-ADD | binary64 a=0.1、b=0.2，另测抵消、次正规数、最大有限数邻域、±0 | GMP按a.as_integer_ratio+b.as_integer_ratio给精确和；下界不能高于它，上界不能低于它。不能拿十进制3/10替代该精确和 |
| N-PERIOD | sin[1000,1002]、cos[1000,1004]、跨很多周期及π/2附近相邻dyadic | 前者上界≥1；临界点存在性由独立高精度π包含证明，不以有限网格采样作为证明 |
| N-POW | [2,3]^[2,3]、[1/2,2]^[-1,2]、[-2,3]^2、[-2,-1]^[1,2] | 分别包含[4,27]、[1/4,4]、[0,9]；最后一个包含非整数负底实域，拒绝 |
| N-ANGLE | x=[-2,-1],y=[-1,1]；另测四象限、±0、原点 | 跨切线的包围覆盖趋近±π；含原点具名失败 |
| N-COMPOSE | WCS导数/平方根、Airy原点/尾、score Hessian和矩阵残差链 | 逐个依赖原语及复合函数都有独立包含测试，不能用生产interval反造expected |

四则用独立GMP/有理数oracle；超越函数用独立驱动的高精度MPFR点值和解析极值/单调域证据，oracle精度至少高于被测且增加精度检查稳定。随机/密集点只能补回归覆盖，不能证明区间全域。保存最坏包含违例、输入精确编码、域拆分、精度、compiler/flags及MPFR版本。资源/溢出/缺库导致不能认证时，调用不得继续返回严格证书。

## 认证能力不能用一个全局bool代表

构建/验收清单按原语族和域分别记录`basic/elementary/periodic/angle/power/bessel/quadrature/linalg/root`的实现hash、环境、已证实定义域、限制和独立case证据。`interval_primitive_status()`即使报告directed_rounding也只描述其实际检测范围，不能授予整个清单certified。公共Code枚举不新增自由字符串；这里的unresolved等原因放既有Issue.reason/Assessment。

上游证书依赖图显式记录：WCS/geometry→实际用到的四则/周期/angle；PSF→模型原语/积分/Bessel；photometry→模板/exp/线性解/根验证；秩/PSD→linalg向外残差。任何依赖未验证或超域，保可用名义结果及conditional状态；若请求必须认证则返回precision_exhausted/unsupported而不发布假证书。代码修复后先重跑原语，再重跑受影响上游；旧产物仅标待影响追踪，不未经分析宣称所有产品污染。

### SVD/eigen与线性解证书的实际计算

名义分解允许成熟浮点QR/SVD，但认证检查器读取原始输入区间及分解数字作为精确点，独立向外重算每项重建残差、正交缺陷、Frobenius平方和、sqrt及谱界。不能复用普通double残差再最后nextafter一下；计入输入矩阵半径和解码/单位变换。对多RHS、分块和transpose，所有必需子解证书做AND，不能被最后一列成功覆盖。

rank_relative只定义数值分类，certified的rank-nullspace须注明精确结构或约定的条件阈值；小但非零奇异值不能被叫物理零。对PSD，显著负下界给invalid_covariance，跨零给未决；只有结构性零才用于精确约束。验证固定可估子空间后再用前述扰动界，必要时直接验证KKT残差。N-LINALG需覆盖精确rank1矩阵、diag(1,2^-40)、[[1,1],[1,1+2^-52]]、精确零与小负特征值、行列缩放和多RHS；独立高精度谱/有理约束必须被最终界包含。无法证明的输出保持conditional，不因求解器报告converged而置rank_certified。

## PSD非线性约束与根盒的统一补充

PSD噪声产生的无噪声方程若非线性，不得仅对白化正谱作无约束GN。可证明的可行参数化存在时在该坐标求解；否则构建所属process明确的KKT根f(w,λ)，用其完整Jacobian求Newton步。控制算法固定为缩放KKT残差merit=||S f||²/2的Armijo回溯（1e-4、最多32次），零步先检查；这是求根的数值merit，不向科学likelihood添加ridge或噪声。结束必须验证原约束可行、完整驻点和约束切空间上的Lagrangian Hessian正性；失败不给局部极小证书。此约束路线的恢复阶段不要求原无约束likelihood逐步下降，不能套用无约束段落的单一停止条件。

根盒以名义候选为中心，初始每坐标半径取max(32个该点ulp、2|Gf|)并有向外包；零点ulp取已声明精度的最小有效步。先做Krawczyk收缩/内部包含检查，不通过时先提高候选解精度；需要更大盒时按倍增尝试，受同一max_iterations/work/位长上限限制。不能把“扩大盒直到能包”当唯一性证明；只有实际包含/收缩及可估条件成立才认证。KKT Jacobian可不定，极小条件在约束切空间验证，不要求整个KKT矩阵SPD。精确约束的数值输入若不确定，须证明固定零空间/可行结构在该输入盒稳定，否则precision_unresolved。

KKT先认证约束Jacobian的独立行；只有存在结构性恒等/已证明局部依赖时才删除冗余约束，并保留所有原方程的验证。仅一次数值rank不足不证明非线性方程冗余。乘子非唯一不直接等于科学参数不可估：在独立约束系统/可估参数子空间认证，再检查完整原约束；不能证明独立/冗余结构时给constraint_rank_unresolved的条件状态，不伪造科学nullspace。PSD约束profile必须由完整KKT消元得到Lagrangian梯度和Hessian，不套无约束gθ/普通H的Schur补。
