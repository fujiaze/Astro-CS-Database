# IPV天测模块详细设计

本页把[天测科学](../../../science/detection/ASTROMETRY.md)、[选星与匹配算法](../../../science/algorithms/normalize/PLATESOLVE.md)落实为真实函数、数据和调用。上层工程依据为[Normalize架构](../../../engineering/architecture/NORMALIZE_PIPELINE.md)、[数据合同](../../../engineering/contracts/NORMALIZE_DATA.md)及[阶段接口](../contracts.md)。

## 调用关系与范围

当前CLI经run_phase1_block→Runtime→P1NodeModule→p1_op_wcs，使用校准像元调用ipv_solve_from_memory_with_callback_d。IPV内部检测solver_detections、读本地Gaia、选星/匹配、拟合并导出TAN/SIP；WCS后科学源另经Gaia引导检测，不能把回调表当science_sources。

公开头是`lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h`，C入口在ipv_entry.cpp，核心在ipv_select/triangle/itertrans/solver/wcs.cpp。文件入口、外部候选入口和内存回调入口分别可用，不因共享solver名字就等同当前CLI路径。WcsTan与photometry的WcsTransform是消费者/桥接实现，非IPV输出模型的另一份定义。

## 完整公开声明

下列省略IPV_API平台链接修饰。Windows DLL定义IPV_EXPORTS、静态消费者定义IPV_STATIC；二者都无时是dllimport。使用匹配的头/库，不能凭名字猜产物一定为DLL。

```cpp
void* ipv_solve_create(void);
void ipv_solve_destroy(void* solver);
void ipv_set_gaia_handle(void* solver, intptr_t handle);
void ipv_set_detector_handle(void* solver, intptr_t handle);
void ipv_set_detector_handle_local_override(void* handle);
void ipv_clear_detector_handle_local_override(void);
void ipv_get_default_params(IpvParams* params);
int ipv_get_last_inlier_count(void* solver);
int ipv_get_last_inliers(void* solver, double* out_buffer, int max_count);

typedef void (*IpvDetectionCallback)(
    const double* detections, int n_detections, void* user_data);

int ipv_solve(void* solver, const char* image_path,
    double ra0, double dec0, double focal_length_mm, double pixel_size_um,
    const IpvParams* params, IpvWcsResult* result);
int ipv_solve_from_memory(void* solver, const float* pixels,
    int width, int height, double ra0, double dec0,
    double focal_length_mm, double pixel_size_um,
    const IpvParams* params, IpvWcsResult* result);
int ipv_solve_from_detections_v1(void* solver, const double* detections,
    int n_detections, int image_width, int image_height,
    double ra0, double dec0, double focal_length_mm, double pixel_size_um,
    const IpvParams* params, IpvWcsResult* result);
int ipv_solve_from_memory_with_callback(void* solver, const float* pixels,
    int width, int height, double ra0, double dec0,
    double focal_length_mm, double pixel_size_um, const IpvParams* params,
    IpvDetectionCallback callback, void* user_data, IpvWcsResult* result);
int ipv_solve_from_memory_with_callback_d(void* solver, const double* pixels,
    int width, int height, double ra0, double dec0,
    double focal_length_mm, double pixel_size_um, const IpvParams* params,
    IpvDetectionCallback callback, void* user_data, IpvWcsResult* result);
```

solver非零由create产生，用destroy配对。pixels是连续H×W行主序float/double，width/height正且索引可表示。文件路径为UTF-8。ra0/dec0单位degree、focal_length_mm单位mm、pixel_size_um单位μm，后二者正有限；hint不是求解真值。params可空使用默认；非空必须先以default函数填全ABI和数值字段。result由调用者分配。

## 参数结构与真实生效范围

IpvParams首部为uint32_t struct_size、abi_version，当前版本宏IPV_PARAMS_ABI_VERSION=2。非空参数按sizeof/版本核对，不匹配返回失败与原因；C/Python镜像须逐字段核对offset和alignment。默认函数需要一整份当前结构体缓冲，它不是“先查大小再只填旧版”的协商函数。

| ABI顺序字段 | 类型 | 数值语义与生产读点 |
|---|---|---|
| polygon_sides | int | 历史多边形参数；当前三角主链不消费 |
| n_pivot | int | 当前主链不消费 |
| sigma_d_arcsec | double | 名称为arcsec；当前主链不消费 |
| vote_threshold | int | 主要用于日志；solve_post_select实际接受门为max_vote≥3 |
| ransac_max_iter | int | 当前主链不走RANSAC配置 |
| ransac_inlier_threshold_arcsec | double | 当前主链不消费 |
| s_min / s_max | double各一项 | 旧尺度域；当前三角尺度实际由s0±20%生成 |
| img_n_target | int | f64回调生产路径选星前覆盖60 |
| gaia_density_ratio | double | 目标密度与实际U密度的无量纲比 |
| gaia_query_radius_factor | double | FOV对角线角度乘此值为锥半径 |
| m_lim_alpha_prior | double | dlog10(N)/dmag初值0.2885 |
| m_lim_alpha_min / max | double各一项 | 斜率更新开区间，默认1e−3/100 |
| m_lim_safety | double | 查询目标数量安全倍数，默认3 |
| m_lim_m0_exposure_s | double | 迭代初值名义曝光180s，不是已读取每帧EXPTIME |
| m_lim_m0_offset | double | 星等初值偏移−4mag |
| m_lim_clamp_lo / hi | double各一项 | 允许迭代星等范围，默认6/22 |
| m_lim_zero_step | double | 空结果向更暗步长3mag |
| m_lim_gaia_cap_per_file | double | 触顶检测阈200000条；当前比较总返回数的保守策略 |
| m_lim_max_iter | int | 最大查询次数4 |
| density_tolerance | double | 目标数量相对容差0.1 |
| log_dir | char[256] | 空串不写IPV文件日志；CLI当前清空 |

除首部外，全部字段由ipv_get_default_params装配。当前p1_op_wcs并未给这些求解参数建立完整用户配置注入，只取默认再清log_dir；存在字段不等于用户JSON可调。以上先验默认、强制常量与真正数值消费分开，不能把无效字段变更当成功调参。稳健精化的RobustRefineParams当前以默认对象传入，详细数值见算法页，不冒充用户配置。

## 候选与回调布局

外部候选和同步回调都用double[N×6]行主序：x、y、flux、mag、saturated、has_saturated。flags在该矩阵中也用double数值，不能按int指针读列。x/y采用sdet的index+0.5坐标；以科学公共整数中心输入时必须由唯一adapter加0.5，不能在IPV和调用者都转换。flux目前是Gaussian A，mag来自方窗；天测按mag选星，flux不充当独立总通量标尺。

回调在图像检测后、选星前同步执行，detections由solver拥有，仅回调期间有效。需要持久化时在回调内复制N×6与来源。user_data借用；回调不销毁solver/检测器、不抛出跨ABI异常，也不在同solver重入solve。callback=null表示不导出，不关闭内部检测。

## WCS结果布局与有效性

IpvWcsResult字段按下表顺序排列。它无struct_size/abi_version，使用者应以配套库与镜像探针验证，不假设参数版本2同时保护结果布局。

| 字段 | 类型/形状 | 语义 |
|---|---|---|
| cd | double[4] | cd11,cd12,cd21,cd22；degree/pixel |
| crval | double[2] | ICRS RA/Dec degree，历元状态另记 |
| crpix | double[2] | FITS一基，当前生成w/2+0.5,h/2+0.5 |
| sip_order | int | 前向阶0表示无SIP |
| sip_a / sip_b | double[36]各一项 | idx=i*6+j；i为x幂、j为y幂，有效三角域i+j≤order |
| sip_ap_order | int | 公开逆多项式阶0表示无逆多项式 |
| sip_ap / sip_bp | double[36]各一项 | 同stride6，逆自变量为畸变像元偏移 |
| rms_px / rms_arcsec | double各一项 | 当前拟合内点残差摘要，非独立留出精度 |
| n_pairs / success | int各一项 | 最终匹配数、0失败/1成功 |
| n_detected / n_catalog | int各一项 | 预留计数字段，当前to_c_result均置0；不能据此称实际无检测/目录 |
| trans_order / best_inliers | int各一项 | 实际拟合阶；best_inliers当前复制最终n_pairs，非独立全局搜索最优统计 |
| ctype1 / ctype2 | char[16]各一项 | RA---TAN[-SIP]、DEC--TAN[-SIP] |
| error_msg | char[256] | 失败原因UTF-8，NUL结尾 |

内部APx/BPx[100]、stride10、7阶是独立扩展载体，没有导出到当前IpvWcsResult；不能由内部81×81拟合存在推断C消费者拿到7阶。AP/BP是近似初值，一步直加精度与完整迭代逆不同。外部FITS可有非居中CRPIX，不能把当前生成策略当通用输入约束。

## 求解、失败与缓存

solve类返回1成功、0失败，消费必须同时核对ret和result.success。失败不得继续解释旧缓冲为本帧解；默认值、失败后的数字以及非空错误串不充当可用WCS。低星数、查询不可用/空扫描/截断、投票不足/预算、各阶拟合失败、重投影失败、奇异或非有限线性项、不可接受残差/尺度均有不同诊断。

当前extract对非线性TRANS的线性项无效、非有限或abs(det)<1e−15明确失败；不是旧“仅warn跳过SIP后成功”。AP/BP网格奇异仍只警告并留ap_order=0，不能把它泛称全部SIP失败均已严检。后置接受政策目前为等效尺度/s0∈[0.8,1.25]、rms_px≤0.5、n_pairs≥12；没有按det符号一概拒镜像。它们是实际项目阈值，不能证明所有通过解均物理正确。

ipv_entry将异常转失败并用utf8_safe_copy写错误。规则是容量−1以内、NUL结尾、完整码点截断、非法编码用ASCII问号代替；控制台渲染与缓冲字节是不同对象。跨层仍要核对实际路径是否完整保留原因为上游所用，不把内部日志行当公开错误字段已覆盖。

inlier缓存为solver最近成功解的配对，在新一次solve开始/失败时应失效。get_last_inlier_count返回非负行数，0为无缓存；get_last_inliers写最多max_count行，空缓冲/非正容量等返回负值。每行9个double，调用者分配9×max_count：

| 列 | 定义 |
|---|---|
| 0/1 | det_x/det_y，图像中心为原点、Y-up，px |
| 2/3 | Gaia RA/Dec，degree |
| 4/5 | TRANS预测位置/s0，px |
| 6/7 | det−pred，px |
| 8 | hypot(res_x,res_y)，px |

该布局不等于回调原图坐标，不能直接写成FITS位置。只在成功解上读取；缓存配对便于核对拟合，不替代独立留出源。查询与下一次solve不能并发改变同solver缓存。

## 资源与线程

solver由调用者独占销毁；设置的Gaia/检测器是借用，须长于所有在途solve，不能先释放DLL/句柄。set_detector_handle还影响进程级单例；帧并发使用线程本地override，设置、求解和clear必须在同线程，所有退出路径清除覆盖。它不会使全局setter自动线程安全，同一solver仍串行使用。

三角生成/投票和星表投影有OpenMP，投票用线程局部整数矩阵定序合并；其余浮点路径还受枚举、退化/平局与环境影响。宿主决定帧数和内部线程预算，不得将无锁或整数票数确定性泛化为全链任意worker逐位一致。当前公开C签名没有取消token，长求解不能据上层token宣称可即时中断。

## 从图像到产品的使用例

独立前向星场以已知TAN+SIP生成像元，给略有偏差的ra0/dec0和光学尺度。内部盲检→非饱和U→本地Gaia→三角票→TRANS→SIP→结果；然后将输出按标准写入独立WCSLIB/Astropy，比较未参与拟合的星及图像角点。

```cpp
IpvParams params{};
ipv_get_default_params(&params);
params.log_dir[0]='\0';
IpvWcsResult result{};
void* solver=ipv_solve_create();
// 先核对solver、Gaia和detector句柄，以下在同一线程完成。
ipv_set_gaia_handle(solver, reinterpret_cast<intptr_t>(gaia));
ipv_set_detector_handle_local_override(detector);
int ok=ipv_solve_from_memory_with_callback_d(solver, pixels.data(), w,h,
    ra_hint,dec_hint,focal_mm,pixel_um,&params,nullptr,nullptr,&result);
ipv_clear_detector_handle_local_override();
if (ok==1 && result.success==1) {
    int n=ipv_get_last_inlier_count(solver);
    std::vector<double> pairs(static_cast<size_t>(n)*9);
    if (n>0) ipv_get_last_inliers(solver,pairs.data(),n);
    // 独立留出评价另行做；CRPIX已为FITS一基。
}
ipv_solve_destroy(solver);
```

例子省略RA/Dec/输入指针域校验和宿主RAII包装，未声称实际运行。负例分别注入少星、同亮度平局、NaN mag、星表截断、错单位、重复原点变换、退化/非线性、无逆SIP及真实缺头输入。解析TAN/WCS独立参考、原始论文和历史误差读数的适用界见算法页；本页不将源码注释的旧测试目录或历史通过数当当前测试执行。
