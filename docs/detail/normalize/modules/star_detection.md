# 星检测模块详细设计

本模块连接[检测科学](../../../science/detection/STAR_DETECTION.md)、[数值方法](../../../science/algorithms/normalize/STAR_DETECTION_ALGORITHMS.md)与[Normalize数据合同](../../../engineering/contracts/NORMALIZE_DATA.md)。lib/algorithms/star_detection/提供图像盲检与预测位置拟合；当前科学源表经完整WCS后的Gaia引导产生，IPV候选属于WCS内部，两者不互换。

## 公开入口与内存

公开头include/star_detector.h声明sdet_create、sdet_destroy、sdet_detect_ex、sdet_detect_ex_f64、sdet_detect_guided_ex_f64、sdet_free_detect_ex。实现src/sdet_api.cpp，平滑/统计在src/sdet_image.cpp，LM在src/nls_lm.*；源码行数不作为接口身份。

| 入口 | 输入/输出 |
|---|---|
| create/destroy | SDetParams或null创建opaque handle，匹配destroy释放 |
| detect_ex | uint16行主序图像、宽高、输出地址与extras请求 |
| detect_ex_f64 | double图像，其余输出布局相同 |
| detect_guided_ex_f64 | double图像、pred_x/pred_y、n_pred，另给可空SDetGuidedStats |
| free_detect_ex | 回收x/y、flux、saturated、mag、has_saturated及extras，extra_count须配对 |

图像与预测数组由调用者持有，至少覆盖同步调用；输出由库分配并用专属free一次释放，不能自由混用宿主delete或另一DLL分配器。x/y为double，flux/mag/extras是float，标志为int；f64入口不意味着全部输出列FP64。六个核心输出数组加extras表和count，不存在含糊“十数组都同一精度”的保证。

rc=0含零输出，参数/分配失败返回−1；零源是否影响下游由消费者判断，不能等同完整科学模型成功。拟合失败和质量拒绝不作为有效行输出；错误域、CLI退出码与库rc不同。当前句柄没有共享调用同步保证，按worker独立handle管理，退出前等待在途调用，模块不能私建长期线程池。

## 坐标与单位

科学合同内部整数为像元中心。当前sdet头/拟合使用index+0.5中心，外部预测/输出的转换须在单一adapter核对；不能把二者都标“0-based”而忽略半像元差。当前p1_op_star_psf_impl直接把gx/gy赋给StarSource并以lround取峰，后续完整WCS、PSF和Catalogue关联须共同验证。这里只登记静态路径，不声称已复现半像元偏差或更改原点。

flux的实际赋值为拟合振幅A，mag由方形窗扣背景积分得到；消费者需知道其真实估计器和单位，不能由字段名直接当Moffat总通量。saturated/has_saturated当前按项目平顶/幅度政策产生，不证明物理满阱；输入校准尺度不由65535常数重新归一。extras按请求名解释，高斯宽度不当Moffat宽度使用。

## 当前参数消费

SDetParams现有13字段，默认成员初始化与sdet_create(null)通过消费者的零哨兵规则配对。使用值初始化再设字段，不能用旧9字段表判断ABI大小。

| 字段 | 当前用途 |
|---|---|
| maxStars | 最终输出上限，正值截断 |
| maxAxisRatio | 大于0启用轴比检查 |
| fitRadius | 盲检额外形状量测窗半径，≤0默认6、实际最大12；不是LM候选盒半径 |
| psfFwhmLoRatio/HiRatio | 盲检相对帧FWHM门，0默认0.5/2.5，负值关闭相应门 |
| maxPeakFraction | 盲检峰/正通量比例，0默认0.35，负值关闭 |
| minQuarterMaxPixels | 盲检四分之一峰像素数，0默认4，负值关闭 |
| structureLayers/hotPixelFilterRadius/iterativeClipSigma/iterativeMaxRounds/medianFilterDetail/fwhmClipSigma | 保留字段与既有赋值/日志，未据这些名字承诺有效算法调节；使用前核对当前真实读点 |

额外形状门在sdet_apply_psf_shape_gate，仅盲检路径调用；引导拟合不接受由这些字段带来的盲检召回保证。固定阈值和未启用的CLEAN/DAOFIND/Kron提案不是本轮实现授权，旧替换任务/冻结流程已移入来源记录。

## 引导生产者与消费者

module_adapters.cpp::p1_op_star_psf_impl读取cleaned平面，取得本帧WCS/SIP并经p1_guided_predict查询Gaia和逆投影，调用sdet_detect_guided_ex_f64。数组输出转成StarCatalog，继续PSF拟合及sources工作载荷，供photometry、noise和Drizzle使用。原orchestrator和独立盲检wrapper是不同调用面，存在不等于当前CLI命中。

当前g_count=0分支在adapter直接返回DATA错误并拒绝盲检回退；科学目标允许已有合法signal伴随源/精度不可用，两者需要单独核对实际失败传播，不能由文档宣称该能力已完整实现。该分支不授权将盲检候选改成science_sources。

SDetGuidedStats有n_predicted、n_dropped、n_fit_failed、n_rejected、n_fit_ok、n_output；fit_ok在去重/截断前，output在其后，不把所有计数直接相加当总预测数。引导定义域不代表无错误关联或虚假拟合，仍需独立真源和匹配负例。

## 数值与验证边界

统计定义归科学正本，平滑与最小二乘的数学推导归算法页；以下逐算子规格说明其真实离散操作、参数、分支及交接。跨worker重复核对排序与实际数值，不以候选独立推定全链等价。有效缓存、开发探针、科学工作文件各自分工，不使用开发快照取数。

核验包含真实公开入口、错误/空源、输出释放、全部列类型、原点/畸变、源ID关联、参数真实消费、额外形状门与引导路径隔离。正负例独立生成，召回表只在相应校准域解释。本轮已读公开头、主要调用与关键实现段，未构建或运行检测程序，不把旧论文核验与性能数字当新执行证据。

原文和历史观测保存在[来源档案](../../../../实验/documentation-refactor/docs/DETECTION_SOURCE.txt)；实现所需的有效规格在本页及算法页给出。科学/数学待核事项集中登记，历史召回表不作为另一份普适阈值定义。

## 完整调用签名与参数域

下列声明与公开头一致；`SDET_EXPORT` 的平台可见性修饰省略。当前头有 C++ 默认成员初值，虽使用 `extern "C"` 导出，不能据此称该头可由纯 C 编译器直接包含。`SDetParams` 没有 struct_size/abi_version，增字段后调用方与 DLL 必须使用配套布局。

```cpp
StarDetectorHandle sdet_create(const SDetParams* params);
void sdet_destroy(StarDetectorHandle handle);
int sdet_detect_ex(StarDetectorHandle handle,
    const uint16_t* image, int width, int height,
    double** out_x, double** out_y, float** out_flux,
    int** out_saturated, float** out_mag, int** out_has_saturated,
    int* out_count, const char** extra_names, int extra_count,
    float*** out_extras);
int sdet_detect_ex_f64(StarDetectorHandle handle,
    const double* image, int width, int height,
    double** out_x, double** out_y, float** out_flux,
    int** out_saturated, float** out_mag, int** out_has_saturated,
    int* out_count, const char** extra_names, int extra_count,
    float*** out_extras);
int sdet_detect_guided_ex_f64(StarDetectorHandle handle,
    const double* image, int width, int height,
    const double* pred_x, const double* pred_y, int n_pred,
    double** out_x, double** out_y, float** out_flux,
    int** out_saturated, float** out_mag, int** out_has_saturated,
    int* out_count, const char** extra_names, int extra_count,
    float*** out_extras, SDetGuidedStats* out_stats);
void sdet_free_detect_ex(double* x, double* y, float* flux,
    int* saturated, float* mag, int* has_saturated,
    float** extras, int extra_count);
```

image 是连续 H×W 行主序单通道缓冲，`image[y*W+x]`，无 stride 参数。width/height 为正 int，乘积与中间 int 索引必须可表示；超大图不能仅靠 size_t 分配成功认定安全。uint16入口先复制为 float，double入口的图像/平滑为 double，LM 内部双精度；最终 float 列仍会舍入。调用不修改输入，可空 params 表示内置默认；自定义配置先用 `SDetParams p{};` 再设字段。

必填输出地址是 out_x/out_y/out_flux/out_saturated/out_mag/out_has_saturated/out_count。extra_count>0 时还要求 extra_names 和 out_extras；每个请求名对应 `extras[k][0..count)` 一列，顺序与请求一致。负 extra_count 不构成正式合法请求，调用方须拒绝。预测数 n_pred≥0；n_pred>0 才要求两个预测数组，n_pred=0 可用空预测指针。

| SDetParams字段（按ABI顺序） | 类型 | 类型默认 / create(null) | 实际消费与单位 |
|---|---|---|---|
| structureLayers | int | 5 / 5 | 现主链无行为读点，不承诺多尺度层数 |
| hotPixelFilterRadius | int | 1 / 1 | 现主链无行为读点，不承诺热像元滤波 |
| iterativeClipSigma | float | 9 / 9 | 不控制 O1 的固定5σ |
| iterativeMaxRounds | int | 5 / 5 | 不控制 O1 的固定3轮 |
| medianFilterDetail | int | 1 / 1 | 现主链无行为读点 |
| maxStars | int | 2000 / 2000 | >0时最终截断；≤0不截断，不是关闭检测 |
| fitRadius | int | 0 / 6 | O13b量测窗半径px，≤0用6，最大12；不控制O9/LM盒 |
| fwhmClipSigma | float | 3 / 3 | 当前日志读点，非有效FWHM迭代配置 |
| maxAxisRatio | float | 2 / 2 | >0启用σ大/σ小上限；≤0关闭该附加门，O13圆度门仍在 |
| psfFwhmLoRatio | float | 0 / 0.5 | O13b下限；0用0.5，<0关闭 |
| psfFwhmHiRatio | float | 0 / 2.5 | O13b上限；0用2.5，<0关闭 |
| maxPeakFraction | float | 0 / 0.35 | O13b峰/正通量比上限；0用0.35，<0关闭 |
| minQuarterMaxPixels | int | 0 / 4 | O13b四分之一峰最少像元数；0用4，<0关闭 |

非有限配置值没有完整公开校验，消费者不得依赖 NaN 自动关闭门。需要严格配置失败时在 adapter 检查，不能把当前无校验当推荐用法。

## 数据布局、内部记录与输出列

内部 SdetCand 保存 ix/iy（峰索引）、x/y（初始中心）、mag_est（实际meanhigh，降序候选键）、Sr/Sc、Ar/Ac、四方向距离、R、半开盒 `[rx0,rx1)×[ry0,ry1)`、sat及sat_r。O7读数用于盒和门，不能作为最终PSF参数。SdetLeaf含排序像素索引表及成分峰；像素集合与峰集的并集参与O8扫描。

拟合结果含 B/A、cx/cy、sx/sy、theta、fwhm_x/y、残差MAD与状态。StarRecord将几何中心保持double，其余连续输出降为float；O15只重排整行，所有列索引严格对齐。API不返回原预测索引或 Gaia ID，排序后不能凭输出第 i 行绑定输入第 i 个预测；adapter必须按明确匹配规则恢复来源，并检查歧义。

| 输出 | 布局/单位 | 实际定义 |
|---|---|---|
| x/y | double[count]，px | LM中心，当前sdet边界坐标为index+0.5 |
| flux | float[count]，图像信号单位 | Gaussian振幅A，既非孔径和也非解析总通量 |
| saturated / has_saturated | int[count] | 目前两列同为 `A>dynrange`，不是O4b标志直接透传 |
| mag | float[count]，mag数值 | O14方窗和的−2.5log10；非正和为NaN，无外部零点 |
| fwhm_x / fwhm_y | extra float[count]，px | 2√(2ln2)·sx/sy |
| sx / sy | extra float[count]，px | 当前直接Gaussian σ轴宽 |
| theta | extra float[count]，degree | 与轴交换配对归一到(−90,90] |
| background / amplitude | extra float[count]，信号单位 | 拟合B/A；amplitude与flux同估计器 |
| r | extra float[count]，px | O4a合格岛等效半径；非岛/非相应候选为0 |
| cand_R | extra float[count]，px | O9候选盒半径，非拟合σ |

extras只接受上表九个精确小写名称；未知名或空名称目前生成全0列，不返回“字段不存在”。因此不得用0列当作有效量测，正式消费者先验证请求名。一个星有NaN mag仍可携带有效拟合参数；“拒绝拟合不输出行”不等于每个输出浮点数均有限。

## 算子与执行次序

O编号沿用基线以便逐项对照，编号不是执行顺序。主链可直接按下列步骤实现与核对：

```text
validate pointers, dimensions, prediction count, extras request
prepare: optional finite-median fill; O2 smoothing; O1 bg/noise/max
O4a: label saturation islands
blind:
  O3: threshold + 8-connected components
  O11a: split components; O8: 11x11 maxima in leaves ∪ tree peaks
  each peak: O4b → O5 or O6 → O7 → O9 → O10 → O11b
  stable sort candidates by meanhigh descending; do not truncate
guided:
  each prediction: boundary/finite → O4b → prediction or O6 → O7 → O9 → O10
both: O12 fit at fixed candidate slots → O13 → O14 → record
blind only: O13b point-source shape checks
O15: post-fit dedup → stable mag order, NaN last → maxStars
allocate output columns transactionally → publish pointers/count
```

### O1 背景与噪声统计

bg为全图有限样本中位数。行内差分 d=I[y,x]−I[y,x−1]仅收集有限值，逐行至多3轮：median(d)、σ=1.482602218505602MAD(d)，保留闭区间 median±5σ。少于8样本或σ≤0停止裁剪；最终少于8不产行尺度。对剩余差分用总体分母N求标准差，跨行中位后乘1/√2；全部行无效返回0。

行差分消去常量项，缓变梯度仍有残留；Var(d)=2σ²−2Cov(邻项)的推导和裁剪/相关偏差见算法页。零返回可能是常量图或无可用行，不能单独解释为测得无噪声。`iterativeClipSigma`与`iterativeMaxRounds`不控制本算子。

### O2 递归平滑及缺测处理

主链固定σ_s=2，调用YvV族双向递推，完整系数见算法页。每行先递推、每列再递推；正向三个历史值都取该线首值，反向都取正向末值。σ_s≤0.5或W/H<4复制直通。当前主递推行列串行，不能把另一卷积函数的OpenMP当本函数已并行。

任意非有限像元触发工作副本，以全图有限中位数填充再平滑/统计/拟合；原输入不改。该填充值没有独立观测权，现接口也没有回传有效性掩膜，故缺测邻域量测需谨慎。全无有限值时阈值非有限，返回成功空表；这不是有效源模型。对大图还有int计数域限制。

### O3 检测阈值与连通域

thr=bg+5σ_raw，在平滑图上严格S>thr形成二值图，以8邻接BFS建组分。队列每组清空，像元索引升序排序。原图噪声与平滑图阈值的关系见算法页，5不是平滑噪声的5倍。不在此复制另一张召回表。

组分阈底流量 F_parent=Σ_comp(S−thr)。负/零thr与负背景仍可能存在；O11指数分层使用Smax/thr及pow，要求其数学域可用，当前没有全面检查负底数非整数幂。此域限制须给后续数值审查，不将有限负图像整体判非法。

### O4 饱和岛与候选平顶判定

令 D=min(max(I),65535.0f)−bg，t_sat=bg+0.7D。O4a对原工作图 I>t_sat 做4邻接岛标号，拒面积≤1或bbox长短比>3的岛；标签0未访问、−1不合格、正数为岛序+1。r=√(area/π)。岛另算的亮度重心/边缘盒中心当前无下游中心消费者，只有label和r被使用；权重和非正的异常域仍需核验。

O4b对每峰原图3×3除中心，收集 I>thr 的邻居。少于3个：盲检丢候选，引导计fit_failed。设其均值m_hi、最小m_min，中心p0；is_sat ⇔ m_hi−bg≥0.7D 且 p0−m_min≤0.1D。此标志控制初值和门豁免，输出饱和列另按A>D赋值，二者不能混同。

65535只是当前数值上界，库不把浮点图归一到该范围；不同光度标度可使D≤0或失去物理意义。改变物理饱和模型属于后续科学工作。

### O5 非饱和中心初值

沿x轴取d_L=S[x]−S[x−1]、d_R=S[x+1]−S[x]，偏移δ=−0.5−d_L/(d_R−d_L)，中心x0=x+0.5+δ；y同构。abs(分母)<1e−20时δ=−0.5。它是局部一阶差分过零插值，非对称/拥挤/平台不保证无偏；最终中心由O12给出。引导路径使用预测中心，跳过O5。

### O6 饱和中心行走

从峰沿同一行/列四向走到原图不再>t_sat，边缘索引xl/xr/yu/yd给中心 ((xl+xr)/2+0.5,(yu+yd)/2+0.5)。不是寻找“最大未饱和3×3邻域重心”，也不直接使用O4a的岛中心。遇帧边停止，之后盒按O9收缩。饱和候选仍进入去顶采样的LM，不把平台中心单独当最终科学质心。

### O7 二阶差分零交叉读数

每正交方向逐像元计算d2=S[p−d]−2S[p]+S[p+d]，找前次<0到当前≥0的第一次跨越；不作零交叉内插。饱和候选先跳过原图≥t_sat的平台。每段最多4096步，越界/未找到返回distance=1、raw=0。距离是峰到交叉的正像素距离，raw=S_cross−bg。

Sr=(d_left+d_right)/2、Sc=(d_up+d_down)/2；Ar=√e·(raw_left+raw_right)/2，Ac同构。四单侧距离保存为正量。高斯一维拐点在σ、信号为A/√e仅说明该读数的理想解释；它不是曲率反演，也不直接给O12宽度初值。

### O8 局部极大

扫描域x∈[5,W−5)、y∈[5,H−5)，11×11全图邻域。邻值更大则拒；相等时更上或同排更左的像元胜。每叶扫描其像元及树成分峰，防止分配后自然峰不在该叶表内。叶数不等于源数，一叶可以有多个峰。边缘5px不产生盲检峰，引导路径的边界策略另见O16。

### O9 盒半径与采样边界

R=clamp(ceil(√(2ln1000)·max(Sr,Sc)),5,200)，√(2ln1000)用解析双精度值。圆高斯99.9%能量半径仅是动机，不证明任意椭圆/方窗/截边也恰含99.9%。盒中心索引lround(x0−0.5)、lround(y0−0.5)，半开边界收缩到图内，空盒丢弃。

LM取盒内有限像元；t_sat>0时排除值≥t_sat者，即使某候选O4b未被标饱和也可能去顶。拟合样本≤8返回INVALID_PARAMS。背景初始化另外使用全部有限盒像元（包括被去顶的点）：排序取下半 `(N+1)/2`，中位bg0、MAD尺度，保留3σ内再取中位；σ=0直接用bg0。

### O10 对称性与振幅读数门

非饱和候选比较 max(|Ar|,|Ac|)/min、max(d_left,d_right)/min和y方向比值，任一>2拒；分母<1e−20置1e30。另有 max(|Ar|,|Ac|)<5σ_raw 拒。饱和候选跳过整门。它与DAOFIND的零积分核roundness/sharpness不同，不能凭类似名称替换。

### O11 组分分裂与候选去重

O11a先于O8。组分像元少于3或无峰高于thr，直接返回一叶。分层t_i=thr(Smax/thr)^(i/30)，实际内部i=1..29，按8邻接重提组分。高层向低层处理，枝流量Σ_branch(S−t_i)>0.005F_parent，且同层至少两枝满足时允许新独立成分；已有单成分吸收未分配像元。F_parent是根组分检出阈底以上和，不能换为枝自身分母。

多成分未分配像元以成分峰为μ，以正权二阶轴矩加0.25px²求sx²/sy²，按score=−½[(x−μx)²/sx²+(y−μy)²/sy²]最大分配；未含协方差交叉项、振幅因子和高斯归一系数，平局保登记先者。根层剩余像元也作一次分配，无存活成分才返回原组分单叶。这是当前项目实现，不能声称与作者完整随机重分配逐项一致。

O11b在O10后、候选亮度排序前，mr=max(1,floor(0.2R))。与已留候选比较曼哈顿距离≤mr则丢当前；非饱和用峰ix/iy，饱和用floor(x0−0.5)。这是先到先留，不是SNR最优。树与此门当前并存；depth常量8/形参在分裂实现中未消费，不是有效递归上限。

### O12 椭圆高斯与LM数值过程

当前直接参数p=(cx,cy,σx,σy,θ,A,B)，残差/解析Jacobian和信赖域方程完整见算法页。初值A0取候选中心±1原图最大值减bg0，沿中心行列搜首次≤A0/2的左右边界，以间距/2.354820045给σ并钳[0.3,100]；θ0=0、A初值max(A0,1)、中心取O5/O6或预测值。缺交叉边界用盒边。

每次solve普通20、中心被采样阈值判饱和时60轮；xtol/ftol/gtol=1e−3，xtol_abs=1e−6。失败时按固定序重试：七参数σx×1.05/σy×0.97；再冻结θ=0的六参数；再σx=σy的五参数。六/五参数须Success且σ>0.2122。已有七参数解超maxAxisRatio时还可能五参数重拟合。重试选用较低维模型是现有实现行为，不保证真实椭圆源的可观测量不受影响。

最终要求Success、所有七参数有限、A/σx/σy>0；角度周期折算并在±90°时配对交换轴。失败状态合并为SDET_FIT_NO_CONVERGENCE，不输出伪成功行。当前残差MAD在最后一次轴比五参数重拟合之前计算，可能不对应最终参数；这是真实静态风险，不能把质量统计泛称完全一致，见问题记录E67。

### O13 拟合排异及O13b盲检形状

内部门按下列顺序返回原因，当前公开API只丢行而不逐行导出这些码。

| SfError | 触发 |
|---|---|
| SF_FWHM_NEG=1 | 任一FWHM不满足>0（NaN亦如此；正无穷需前置有限检查） |
| SF_FWHM_TOO_SMALL=2 | 任一轴≤0.5px |
| SF_ROUNDNESS_BELOW_CRIT=3 | min(FWHM)/max(FWHM)<0.5 |
| SF_RMSE_TOO_LARGE=4 | 非O4b饱和且A>0，1.482602218505602·MAD(res)/A>0.2；名称不是实际RMSE |
| SF_FWHM_TOO_LARGE=5 | s=max(Sr,Sc)>2，任一FWHM>s·2.354820045·[1+0.5ln(s/2)] |

maxAxisRatio>0的σ比门与上述五门分别执行。O13b只在盲检、O14装配后且O15前执行：f0=候选平均轴FWHM的中位数，接受相对f0的上下界；局部盒取拟合背景B，peak=max(I−B,0)、F_pos=Σmax(I−B,0)、pf=peak/F_pos，严格计 I−B>peak/4 的像元数。实际用盒内最大值，不一定是中心像元。峰与正和非正时量读数为0；相应最少像元门仍可拒。配置和默认见前表。

### O14 方形孔径量

以候选中心和R，在图内方窗累加F_box=Σ(I−B_fit)，不是以最终拟合中心重新选窗，不是Kron或无限高斯积分。F_box>0才mag=−2.5log10(F_box)，否则NaN。窗口截断、未掩邻源、背景拟合误差和缺测填充影响此量；它只用于当前排序/粗量测，不附完整方差。

### O15 行整理与事务输出

正常−正常距离²≤1保先者；饱和−正常距离²<4保饱和；饱和−饱和距离²<4保r大者，平局保先者。然后mag升序stable_sort，NaN排末且保原序，maxStars>0才截断。不能称mag序与SNR序普遍等价。

核心六列及extras逐列malloc。全部分配成功后才发布指针；输出分配失败释放全部、全部指针NULL、count=−1、rc=−1；空表count=0、rc=0。参数在输出清零前就可能失败，调用者应先初始化所有输出地址，不能假定任意失败都改写出参。内部vector/std::sort仍可能抛异常，外层没有全面异常屏障；“禁止异常跨ABI”是工程要求，不是已验证事实。

### O16 引导域、统计与来源

有限预测位置先要求min(px,W−1−px,py,H−1−py)≥2，再以lround(px−0.5)、lround(py−0.5)取像元；该边缘判式仍含索引中心与边界坐标混用风险。未饱和中心直接用预测位置，依次O4b/O7/O9/O10/O12/O13/O14/O15，不经过O3/O5/O8/O11与O13b。预测存在不保证通过原图3×3亮邻居前置条件。

正常逐位置循环完成时，n_dropped+n_fit_failed+n_rejected+n_fit_ok=n_predicted；n_output≤n_fit_ok，去重/截断损失不另加到前四类。n_pred=0、全非有限图或参数提前返回时不由该恒等式证明处理完整；以rc、实际执行域和count共同解释。循环结果按预测索引写回后串行装配；输出随后排序，来源索引没有原样导出。

## 生命周期、并发与资源

create复制参数，handle由匹配destroy释放；destroy(null)安全，结果不依赖handle继续存活，但须在所属库卸载前free。调用期间借用图像、预测与请求名称；没有异步访问承诺，不在运行中destroy/改参数。结果六列与extras必须由同DLL的free_detect_ex成组释放，extra_count使用原请求数，不能对各列另行delete/free后再整体释放。

O1行统计static、uint16转换static、O12和guided位置dynamic使用OpenMP；O2主递推本身串行。API没有worker或cancel参数，生产adapter/宿主负责预算，函数内无逐星取消保证。当前LM传workspace=null并分配局部vector，不能声称已实现每worker复用全部scratch。不同handle可分开工作；共享日志/运行库环境仍需核验，不声明无条件跨DLL线程安全。

若N=WH、C候选、m_i盒样本、p≤7、L=29，准备和标号O(N)，O8上界O(121N)，拟合约ΣO(K_i(m_i p²+p³))，两次简单去重最坏O(C²)。分裂树的排序/二分查找、枝包含检查和逐像元逐成分分配不能无条件简化成O(N)；辅助assigned按绝对最大像元索引分配，单组分也可达O(N)。内存包括原副本（若缺测）、平滑图、标号/岛/树、每并发拟合工作区与全部列，需计峰值而非只算输出星表。

## 连贯使用例与独立核验

设独立生成的64×64图像，库坐标中心(31.8,30.2)，I=B+Aexp(−r²/(2σ²))+ε，B=100、A=1000、σ=2、噪声σ_n=1。它说明输入→O1噪声→O2/O3候选→O12模型→六列接口的关系；不是已运行结果。理论无限Gaussian总和约2πAσ²，而out_flux应解释为A，O14方窗和是第三个量。

```cpp
SDetParams p{};
auto h = sdet_create(&p);  // 实际调用方先检查h
double *x=nullptr, *y=nullptr;
float *a=nullptr, *mag=nullptr, **extras=nullptr;
int *sat=nullptr, *hs=nullptr, n=0;
const char* names[] = {"sx", "sy", "fwhm_x", "background"};
double px[] = {31.8}, py[] = {30.2}; // 当前库的index+0.5坐标
SDetGuidedStats stats{};
int rc = sdet_detect_guided_ex_f64(h, image.data(), 64, 64,
    px, py, 1, &x, &y, &a, &sat, &mag, &hs, &n,
    names, 4, &extras, &stats);
// rc==0再解释n/stats；绑定Gaia ID须另核对输出位置，不能靠行号。
sdet_free_detect_ex(x,y,a,sat,mag,hs,extras,4);
sdet_destroy(h);
```

测试路径在eng目录和实际构建生成的测试清单中查证，不据源码注释中的旧`tests/p1star`路径宣布存在或已执行。必要独立构型包括：解析Gaussian及Jacobian有限差分、θ圆星退化、异方差/相关噪声、双星流量比和鞍点、负背景分层数学域、全NaN/局部缺测、边界、饱和标志差异、OOM事务与参数提前失败、列对齐和身份、并发及全部默认参数消费。每个构型给真值、样本分母与匹配准则；本轮只读实现，未运行检测/LM测试。

## 方法来源与未启用设计

原文CLEAN、DAOFIND门和Kron孔径仍有研究价值，保留为具名候选方法，不能由旧“本次替换”措辞自动启用。CLEAN以邻源模型外推并检查剩余检测信号；DAOFIND的roundness/sharpness使用其零积分核域；Kron以一阶径向矩r1=ΣrI/ΣI定义自适应孔径。这些改变估计器或样本选择，需另立明确规格和独立验证。当前Gaussian A是峰幅，代入以总通量F/(2πσ²)写的邻源式前须先转换，不混用符号。

一手论文、参考代码版本和适用差异统一见[算法文献](../../../science/algorithms/normalize/STAR_DETECTION_ALGORITHMS.md#参考资料)。O1、O4–O7、O9–O10、O13、O15–O16的具体规则是项目构造；引用一般文献不把所有常数归给原作者。阈值化、去混叠、数值求解与科学PSF积分各有职责，不以同族谱系证明实现完全等价。
