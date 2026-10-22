# 计算、内存、精度与并行模型

本合同执行[科学几何](../../science/foundation/SPHERICAL_DRIZZLE.md)和[统计模型](../../science/foundation/STATISTICS_SKY_COADD.md)，不改变估计量。接口和数据依赖见[API](API.md)、[对象与持久化](OBJECTS.md)。所有复杂度中的 nnz、支持、秩及精度位长是实际量，不假定真实场景永远小秩或固定候选数。

## 计算单元与对象寿命

每次命令有独立 RunContext、线程预算、object store、I/O 服务与取消域。可复用同一个基础设施实现，但不共享跨阶段会话。工作流声明不可变模型及块的输入/输出，Runtime 将其拆成有界 WorkUnit；不固定旧 Normalize 八节点或 ARS Frame/Operation 的实现层次。

对象生命周期为 builder 独占写 → validate → publish immutable → consumer leases → 最后一份租约释放 → 回收或可验证 spill。部分写入对象不可见；迭代得到新版本，不覆盖被读取模型。async I/O、排队 task、action recipe 的依赖都持租约；cancel 发出后先阻止新任务，再让在途任务到安全边界，完成/撤销 I/O，最后释放，不能先销毁数组再等待 future。

科学模块只申请 scratch/输出/任务子预算，不建线程池，不自行长期缓存数组，不直接读写文件。provider 的内存块由 I/O/runtime 承担租约与计费；纯 in-memory 独立调用由 caller 提供等价上下文。无 Runtime 也能串行执行同一 API。

## 实际算子和流式计算

R 既可保存分块 CSR，也可从不可变 overlap spool 按目标块恢复，或通过已锁定版本的 recipe 重放。构造时按 source 生成完整候选及贡献 → 有界分区 spool → 按 `(target leaf,source identity,primitive)` 排序归并；同一源重复项合并，行归一后同时生成 B/R 及误差界。source 的 A/Ω只存一份，不以每叶副本制造独立误差。

`apply` 按目标行读取触及源；`transpose_apply` 按源分区归并来自目标行的贡献；二者都用同一 R 版本。行之间共享源时 noise 使用 source identity 回溯，不按叶独立。PSF/背景基按相同 action 计算，可按需查询，不默认保存每源全图模板。unknown noise 不阻止 signal action。

几何主核为有余项界的 affine-strip cut-cell，包含完整 chart/候选分支与 GMP 精确局部裁剪。误差地板时整条端点/半空间/裁剪/ledger 路径升级 MPFR128/192/256；不是只升级 sin/cos 再降 double。通用精细 fallback 为等面积目标域包含积分 A；detector 域 B 保留独立验证/替代实现身份。所有核必须返回同一 Ω/A/a、primitive constraints 与未决域合同，不能切换到无界四角近似。

C++参考原型和Python局部高精度算例仅说明路线可执行，完整一般域 fallback 尚待后续实现和独立验证。本设计要求实施该能力；不能据局部原型宣称已经生产可用。无法在资源内收敛时正式返回 unconverged，并保留有效界与失败作用域。

## 科学预算控制器

1. 请求者给 B [L/sr]、分配/支持积分 [L]、孔径、Cov [相应量平方]、支持及可接受模型条件。分配/支持积分严格区分；request 锁定 nominal 或随几何重算的 estimator。
2. geometry 返回粗 Ω/A/a bounds 和共同约束。operators 计算 signed F/N/B/R 区间，不独立归一端点中点；需要 NL>0 才有认证归一结果。
3. 对最终泛函计算界，把强对比/病态项反推到逐 primitive 的绝对面积限额。控制器优先细化实际影响最大的区域，不以统一1%面积或中点变化量停止。
4. 通过 sky/BLUE 等求解的请求，须对可估/gauge固定系统给残差和扰动证书；ρ=‖|I−GM0|+|G|EM‖∞<1 才给该路线的有限解界。权归一分母需正下界。ρ失败 → 细化/升级或 geometry_solve_unresolved，不能 ridge 改目标。
5. 选择/秩/接触分支可能改变时，证明分支稳定、包围全部允许分支或 selection_unresolved。仅冻结实际 mask/权的结果必须标 conditional，不能冒充理想重算流程。
6. max_work/max_depth/max_bytes/precision 耗尽各自记录。返回后若序列化降精度使界增宽，重新检查 science tolerance；所有泛函分别保存 achieved/requested，不用一个 run PASS 覆盖局部失败。

跨阶段反馈通过公开refine_geometry和GeometryRevision执行，具体snapshot事务见[API](API.md)。控制器维护总配额和版本依赖；共同A/Ω或全局sky解改变时，失效并重建全部受影响域而非仅本tile。旧snapshot的消费者租约未释放前，新旧数组同时存在的峰值内存也必须预留；不足则spill旧闭包或串行重建，不先篡改旧R。

有效 covariance action 无绝对因子能力时仍能求名义方差；几何 covariance bound 不可用须明确。模型偏差与数值界分开；源值 x 本身有确定区间时同 signed numerator 一起传播。并发数、指令后端或缓存命中不改变这套停止语义。

## 数值格式

科学规范的公共实数默认 binary64。首期 native/scaled values、R、归约、所有模型与方差/控制求解默认 FP64；区间/精确账本按上述更高精度处理。这取代旧系统“大数组默认FP32”的新核心默认值，避免未计量化项消耗高动态预算。

发布 signal 可选 FP32 或 FP64，但先将量化误差加入数值包围再判断目标容差；默认选择能满足请求的格式，FP32失败提升FP64，仍不够则保高精度模型/输出不认证，绝不截断后继续标bounded。indices uint64，order uint32；极细尺度、近极点和弱支持下不按索引阶数猜可用精度。禁止 fast-math 进入包含原语；SIMD 筛选必须有有效误差界，模糊谓词升级精确算术。

归约使用固定分区、source顺序及固定二叉树，FP64补偿求和与已计舍入界；认证累计用精确/dyadic向外账本。数学上同一个 source 的几何与噪声身份不因重复分区改变。严格包含保证以所声明 IEEE/MPFR/GMP 原语为前提；不同平台 libm 或 compiler 不能自动继承该标签。

## 内存预算与背压

总硬预算 H 对可计费 payload/scratch/队列/cache/数值大整数约束；进程可用内存需另留运行时/库/不可控分配的测量余量。准入估算与运行峰值都记录，不能只统计主 signal 数组。FP64示例：单块65536叶×8帧 signal 约4 MiB，uint16 DQ约1 MiB；协方差、R、几何队列和压缩缓冲另计。

一个工作块的上界模型为

`M_block = L*K*(signal+DQ+support_state+weight_scratch) + nnz_block*(R+source_index+bound_metadata) + M_local_cov + M_shared_active + M_geometry + M_solver + M_io`。

全进程要求 `Σactive M_block + M_resident_models + M_cache + M_queued_outputs ≤ H`。bound_metadata 与 GMP/MPFR 实际分配按 bytes 计费，不假定每项固定8字节。worker 在领取任务前同时预留 input/output/scratch/spill buffer；不能先占满输入再等待输出导致死锁。预留不足先减L/K批次/并发、释放可重算 cache、spill；没有可行最小单元时 resource_exhausted。

默认块叶数目标65536，可按预算变小；它不是不可变的HiPS tile尺寸，物理tile可切片。帧数K很大时分批读取，coadd所需局部Γ或求解状态仍跨批保留；任意稠密 K²/求解 workspace 超预算时用适用的结构化 action/外存求解或拒绝资源请求，不退化为错误独立IVAR。

geometry priority queue 每worker有上限，候选爆炸保留粗域及质量并 spill。spool 分区输出一次最多占已预留字节，有界 I/O 队列提供背压。tile/source双向索引按需缓存；全图常驻 nnz_R、局部25邻域、输出N² covariance均不作为前提。磁盘配额和最小剩余空间也属于运行预算；不足时可靠失败，不循环不断spill。

## 协方差和拟合复杂度

| 任务 | 计算/存储模型 | 必须另计的项 |
|---|---|---|
| WCS点及逆 | forward O(d²)，逆O(m d²) | 连续证书/区间细分与精度位长 |
| affine几何 | O(q(d²+Kc v)+q log q)，局部队列+输出nnz | 候选Kc、精确有理位长、升级/fallback；不承诺固定像元耗时 |
| R apply/transpose | O(nnz_touched)，有界分块/spool | 排序/索引/外存I/O；跨块相同源合并 |
| C action | L C0 Lᵀ，C0=BDBᵀ+C_loc+UΛUᵀ | nnz(B/U)、局部支撑、Λ的分解/存储；任意模型不保证固定小秩 |
| 孔径/单叶V | 回溯Rᵀt及触及原生/共享因子 | signed t、绝对因子action、未知项传播 |
| photometry | 每星谱积分O(d_s)，局部拟合O(n p²+p³)，source latent分块消元 | 全局共享Schur/action、rank和GN/IRLS迭代、bootstrap |
| sky | 稀疏[H,A G]联合系统及白化/逆action；不物化Q_w或P | 内层solve误差、PSD约束、预条件与全局控制数据 |
| 稀疏actual-scene预测 | 每小chart预处理O(Mc log Mc)，查询O(log Mc) | chart/halo、holes、外部δ/M及其支持域 |
| coadd/LOO | 一般局部Γ解O(K³)；小N≤7可缓存接受mask与子块解 | N大、Γ变化/未知、非负QP、post-selection MC |
| Export | 实际相交叶数与目标边界精细度 + Q action | 支持并集/unknown subleaf形状、相同leaf合并 |

共同 Gaia/sky 全局拟合允许多遍流式读取观测/控制，参数求解后屏障统一发布解。不可估模式显式保留，不能为省内存把全局联合拟合拆为相互独立帧。保存的低秩修正可含signed block，但首选完整P C Pᵀ action保证同数据交叉项；PSD零空间作精确约束求解。

## 并行方式与可复现性

唯一CPU线程池由RunContext管理。帧级任务、源块几何、目标块action、按source的谱积分和按连通分量的sky解共享线程租约；BLAS/OpenMP默认单线程，获租约才启用内部并行，禁止嵌套超售。大precision任务也占有bytes/工作配额，不能以线程空闲为由突破内存。

source geometry可独立并行，但共享tile导数/边缓存不可变发布；每个结果写本地缓冲，按固定key归并，不对全局FP数组无序atomic累加。sky不同真实分量可并行，同一分量必须联合；最终coadd在sky解冻结后按目标块并行。Export目标块独立读取，重复叶随机身份保持相同。

同一build/后端/算术、相同输入和配置，改变线程数或完成顺序应保持规范结果内容一致；进度、计时、内存和run_id不在科学内容hash内。跨后端/平台不承诺bitwise，但必须各自保合法包围且通过预先指定的绝对/相对科学容差。数据依赖选择受舍入影响可能换支时，不以“数值接近”豁免选择状态。

验证至少覆盖1线程/多线程、正常/极低内存spill、正常/取消/写盘失败、随机任务完成顺序；各输出比较同一模型身份、共享根、支持/失败范围、signal/协方差泛函和数值界。cache关闭与开启必须保持科学状态和允差；cache只能复用有输入/hash/domain/arithmetic身份的证明。机器benchmark决定并发/块大小，不选择不同科学算法或放宽容差。

## 一个进程的资源所有权与调用预算（实现定案）

上文“每次命令独立RunContext/线程预算”意为取消域与工作账本独立，不能解释为同进程各自建池。一个进程仅一个Resources服务、Storage、CPU池与I/O队列；多个嵌入式同步调用共享总H和线程上限，每命令是子配额。命令间只通过已发布产品交换科学状态。Context是Resources/Storage/Cancellation借用，不是Runtime对象系统。默认科学配置不藏在Context中。

Resources.reserve必须原子同时预留输入pin、输出、scratch及必要I/O缓冲，失败返回resource_exhausted，不阻塞等待另一半资源。Reservation租约释放归还bytes/线程；consume_work用原子计数，失败无副作用，无调用可把总work重置。Storage解压、std::vector/共享模型控制块、大整数、排序、线程栈显式余量、事件队列均在准入估算内，实际计费超过reservation先扩预留，失败在有保证的安全边界保留未决，不让std::bad_alloc破坏包含记录。无法截获的库分配以实测峰值+20%余量预留；OS RSS不是硬受控payload上限，二者同时报告。

取消检查不超过4096 work或100ms可中断批（先到者）；MPFR/精确裁剪单步按最大位长/顶点预留，无法保证可中断时先缩小块。I/O chunk≤8MiB，最多 min(threads,4) 在途读及2个写缓冲，每任务自己的预算也约束。LRU cache默认≤H/8，key含输入hash、domain、algorithm、precision、condition；pin计费、逐出只影响性能。无队列可无限增长；work调度队列≤2*threads，spool有磁盘配额，任务产生速度由reserve控制。

大模型的dense_max_bytes默认min(H/4,64MiB)，小系统稠密QR/SVD；超过选稀疏/迭代路线，若任一必要分解（例如局部K² Γ）无结构化方法且超预算，返回resource_exhausted，不假定独立或改权。解完成前保留所需观测根多遍读能力；没有重读能力的provider在准入拒绝而非读一半后丢依赖。

## 数值求解、可估性和停止的具体步骤

所有列先除其2范数（精确零列保零）；参数结果和nullspace映回原单位。稠密SVD的singular_error_bound至少含分解残差、矩阵输入区间界及舍入项的谱范数上界。rank_relative=1e−12只是报告分类：σ下界>threshold为可估，σ上界<threshold可列数值不可估，误差区间跨threshold则rank_unresolved并升级算术/细化。任何真实零空间须有解析/gauge结构证据或明确conditional numerical_rank；不能把任意阈值删去的小但真实非零模态称精确物理不可辨识。

SPD模型优先因子白化：C=F Fᵀ小系统Cholesky，病态用对称eigen。负特征值小于−spectral_error_bound即invalid_covariance；跨0的谱为precision_unresolved，精确零须输入结构或认证证书。PSD零空间U0先求 D0θ=y0（D0=U0ᵀD），不一致超过误差界invalid_model；求可行θp+Zη，再在正谱子空间白化GLS。exact约束可决定F并给V=0。伪逆只在已明确参数子空间中使用，不把 C+ 当总体精度。

大系统首选 sparse QR；内存不能承受时LSMR，停止同时满足缩放残差 ||Dᵀr||∞≤abs_res+rel_res||D||1||r||∞ 和已要求的输出泛函残差界，且迭代数未超。默认 rel_res=1e−10、abs_res=1e−12（输入白化无量纲空间），它们不能代替科学 tolerance；未给有限下游误差证书仅conditional。仅covariance action时KKT+MINRES，内层求解的残差/作用误差纳外层证书。预条件器不得加ridge改变目标，可以只作迭代加速。没有验证大系统nullspace能力时返回not_identifiable/geometry_solve_unresolved，不用LSMR收敛自动证明唯一性。

非线性PSF/photometry：每步按SYSTEM求GN/真实score导数，Armijo参数1e−4，回溯≤32次，先检查当前驻点，小步/梯度与输出误差满足则converged；非驻点更新必须likelihood下降；默认step=1e−10、gradient=1e−8、outer≤100。非驻点且目标不下降/overflow回溯失败nonconverged，不裁参数到任意物理值。covariance/influence只条件于报告的线性化模型，未知模型偏差不由Hessian变已知。

### BLUE、PSD和非负QP

SPD Γ：QR/Cholesky解 Γq=1，检查q/denominator误差；1ᵀq下界>0才给t=q/(1ᵀq)。PSD：先检查是否存在Γ零空间向量v且1ᵀv!=0；存在则有零方差归一解，取最小范数归一解并保存精确约束。否则在range(Γ)解q，若1在range外的分量未定则精度/秩未决；禁止用epsilon地板。权的数值区间由统计式3d传播，σ或分母太弱返回geometry_solve_unresolved。

非负QP用active-set：从equal可行解开始，在当前自由集解equality-constrained PSD QP；候选有负权时沿当前到候选线段走到最先碰0者，按frame ID打破平手并加入active；可行候选后计算KKT梯度，若active有负reduced gradient则放开最小者，反之最优。每步至多K个线性约束变化，max_iterations与work均限制；退化循环时按frame ID Bland规则。精确PSD多解取最小2范数最优权。停止验证sum=1、t≥0、stationarity和complementarity的区间残差，目标方差/权泛函误差必须在预算；max_iterations后nonconverged。Γ未知时显式QP/BLUE unavailable，automatic才equal并列原因。不同均值mixed结果的方差可算，但不是共同量最佳无偏估计。

LOO只计算一次所有候选q，从最大|q|超过Φ⁻¹(1−α/(2N))者拒一个，相等按frame ID；v_res必须有严格正下界，精确0且残差也0无需拒，0且残差被精确约束否定则模型不一致，不能除0。数值区间跨阈值是selection_unresolved：细化/升级，或显式固定实际分支conditional；默认不以任意中点决策认证成功。

## 全链误差记录和发布门

每个输出的ErrorRecord以不同字段保存：numerical_bound、stochastic_noise_ref、finite_model_bias、unknown_model_terms、linearization/selection_condition、requested/achieved functional。它们不相加成一张“总variance”。x/G/α的数值误差先输入同一R分子，再传P、权与Q；同数据影响及根的shared identity随compose保留。no_noise不表示numerical_bound未知；geometry certified不表示物理误差known。

预算优先级：先保证输入模型有效/原语有界→证明支持/可估性→请求的所有科学泛函→序列化量化。每次细化按贡献上界从大到小、再按source/leaf ID排序，至少留总容差1/8作累计舍入与输出量化；四种误差合计仍须满足最终绝对+相对floor标准。相对容差的有效目标为max(absolute_floor,absolute+relative*|nominal|)，零附近不得以未证明nominal放松停止；保守用区间中最小|true|检验relative目标。输出所有尚未达标请求，不能用多数叶通过代表run成功。

GeometryRevision使Ω/A共同源相关、全部目标叶、全sky分量、selection/weight及Export依赖失效；有新闭包之前旧闭包完整可消费，不混版本。峰值旧+新闭包预留不足时按可验证checkpoint/spill串行，不提前销毁仍有Lease的数组。科学方向的反馈不在模块DAG建循环：workflows迭代控制调用geometry与下游纯函数，所有版本依赖仍无环。

publish_product只有required quantities均达标、所有必需closed graph存在时产生complete=true；明确conditional信号可发布但capability/condition必须匹配用户接受的目标。资源耗尽的面积/diagnostics可写run目录，不能偷标正式complete产品。FP32失败→FP64；FP64量化仍超预算则publication nonconverged，不自动降低请求或无依据允许高精度FITS dtype（FITS signal首期仅float32/64）。高精度primitive及模型端点仍可持久化供后续计算。

## 驻点、舍入停滞和非线性失败

非线性每次迭代在线搜索前检查当前有限有效likelihood、可估空间的缩放score、参数/输出泛函误差；score与预测步均达请求阈值时允许零步收敛，likelihood不需再下降。无噪声初始化已经精确拟合的情形必须成功。仅对尚未满足停止条件的非零更新要求Armijo下降；步被舍入为零但score仍大是precision_exhausted/升级，不是收敛。非驻点且所有回溯失败才nonconverged。此规则替代前文对“目标不下降”不区分驻点的表述。

## 可计算的SVD/eigen误差证书

对数值SVD A≈UΣVᵀ，先向外求δU=||UᵀU−I||F、δV=||VᵀV−I||F及eR=||A−UΣVᵀ||F，δU/δV必须<1。极分解正交因子的距离界取eU=δU/(1+sqrt(1−δU))、eV同式，则奇异值绝对误差可取eR+eU||Σ||2 sqrt(1+δV)+eV||Σ||2，加输入矩阵区间的Frobenius半径与全部舍入界。所有范数、sqrt、乘加向外。δ≥1或界跨rank阈值则升级/未决，不省略正交缺陷。对称eigen A≈UΛUᵀ同理用δU及重建残差，给λ区间并区分显著负/零未决；精确零约束仍须结构来源。大系统无法给证书时只conditional数值秩，不升级为certified。
