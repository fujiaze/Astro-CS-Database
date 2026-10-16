# 光度模块详细设计

本页落实[光度模型](../../../science/photometry/PHOTOMETRY.md)、[积分与求解算法](../../../science/algorithms/normalize/PHOTOMETRIC_FIT.md)、[Normalize管线](../../../engineering/architecture/NORMALIZE_PIPELINE.md)和[数据合同](../../../engineering/contracts/NORMALIZE_DATA.md)。这里只描述接口与现代码分支，科学估计器以算法正文为准。源码依据为main的217e56d99所含版本；未构建或执行本模块。

## 入口、依赖与调用关系

生产scheduler的p1_op_photometry装配FramePhotFitRequest，调用acsd::photometry::fit_frame_photometry。该函数创建GaiaClient、解析具名filter/QE、取得谱网格、调用C++质量位感知v2路径，返回k_photo、诊断、逐星记录和可选空间场。节点只在fit_ok成立后调用校准库apply_photometry施加α=k_photo m；后续noise/Drizzle消费实际应用对象及身份。

库级C兼容入口直接输出试乘像素；frame装配内部也申请这份缓冲但不把它返回或覆盖输入。最终产品应用在节点的独立输出块完成，不能把二者叠乘。相同数组是否真的已应用，依据产品application身份和实际调用解释。

头文件photometric_calib.h声明7个C函数：2个simple、2个with_gaia、2个with_gaia_v2及1个投影函数。pc_api_qf.h另声明C++链接期质量位入口，不是第8个C导出。公开头目前含cstdint和未typedef的struct PhotometricDiag，不能据extern C便宣称可由任意C编译器直接包含；版本/struct_size也未嵌入这些结构，跨二进制须固定布局和工具链，不能自行追加字段。

## 完整公开函数签名

以下为现头的完整顺序；PC_API宏和extern C链接包装依公开头。所有数组连续，尺寸见下一节。

```cpp
PC_API int pc_calibrate_simple(
    const float* pixels, int width, int height,
    const double* gaia_ra, const double* gaia_dec,
    const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    const double* qe_wl, const double* qe_trans, int qe_count,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    float* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual,
    PhotometricDiag* out_diag);

PC_API int pc_calibrate_simple_with_gaia(
    void* gaia_client_handle,
    double ra_center, double dec_center, double radius_deg,
    double mag_min, double mag_max,
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count,
    const float* pixels, int width, int height,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    float* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual,
    PhotometricDiag* out_diag);

PC_API int pc_calibrate_simple_f64(
    const double* pixels, int width, int height,
    const double* gaia_ra, const double* gaia_dec,
    const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    const double* qe_wl, const double* qe_trans, int qe_count,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    double* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual,
    PhotometricDiag* out_diag);

PC_API int pc_calibrate_simple_with_gaia_f64(
    void* gaia_client_handle,
    double ra_center, double dec_center, double radius_deg,
    double mag_min, double mag_max,
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count,
    const double* pixels, int width, int height,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    double* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual,
    PhotometricDiag* out_diag);

PC_API int pc_calibrate_simple_with_gaia_v2(
    void* gaia_client_handle,
    double ra_center, double dec_center, double radius_deg,
    double mag_min, double mag_max,
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count,
    const float* pixels, int width, int height,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    const int64_t* psf_star_ids, PcMatchRecord* out_records,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    float* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual,
    PhotometricDiag* out_diag);

PC_API int pc_calibrate_simple_with_gaia_f64_v2(
    void* gaia_client_handle,
    double ra_center, double dec_center, double radius_deg,
    double mag_min, double mag_max,
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count,
    const double* pixels, int width, int height,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    const int64_t* psf_star_ids, PcMatchRecord* out_records,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    double* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual,
    PhotometricDiag* out_diag);

PC_API int pc_project_sky_to_pixel(
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order, const double* sip_a, const double* sip_b,
    const double* ra, const double* dec, int n,
    double* out_x, double* out_y);
```

## 参数、坐标和所有权

| 参数组 | 形状、单位、有效性及语义 |
|---|---|
| pixels/out_pixels,width,height | row-major [H*W]；f32为float、f64为double；输入借用只读，输出调用者分配。W/H正；乘积在当前int循环内须可表示且实际缓冲足够，库未全面检查溢出；负值/有限零作为线性信号保留 |
| gaia_ra/dec/mag/fsyn,n_gaia | simple专用，同序[n_gaia]；ICRS degree、G mag和已按声明约定合成的F_syn。正有限通量才可取对数；函数不重算F_syn |
| gaia_client_handle | with_gaia借用已创建GaiaClient；本函数不销毁；调用期间调用方保持有效，不得并发销毁 |
| ra_center,dec_center,radius_deg | with_gaia查询中心/锥半径，degree；frame路径取CRVAL和下述FOV估算，不是逐角完整足迹 |
| mag_min,mag_max | 查询G范围；mag_min实际消费，with_gaia的mag_max签名保留但被{12,13,14,15,16}覆盖；simple已给星表无此组 |
| filter_wl,filter_trans,filter_count | with_gaia曲线[N_T]，nm和无量纲效率，目标物理域[0,1]；至少2个唯一有效节点才有完整插值。底层不等于生产曲线身份校验 |
| qe_wl,qe_trans,qe_count | [N_Q]，nm和无量纲QE；with_gaia兼容入口可空则Q=1；simple不消费这三个参数。生产frame入口必须具名QE，不允许静默缺省 |
| spectrum_wl,spectrum_count | with_gaia的完整均匀SED采样网格[N_λ]，nm；须与Gaia返回谱stride/起点/步长一致。当前只对长度不一致告警，详见异常分支；343/336/2nm只是已观察构型 |
| psf_cx/cy,psf_flux,psf_status,n_psf | 同序[n_psf]；位置0基像元中心，单位px；通量为本帧PSF解析总通量，非检测盒和/峰幅；status==0进入匹配。PSF自身中心原点到此约定须由唯一adapter转换 |
| psf_star_ids | v2同序[n_psf]稳定source ID，可空则回传0；frame当前传nullptr，故其records的star_id需节点按PSF行接回，不能当已经贯通 |
| out_records | v2调用者分配PcMatchRecord[n_psf]，可空；按原PSF行输出，包括未匹配/拒绝/无效，不压成n_matched长度；调用方应预置f_instr/gaia_mag为NaN，现入口部分分支不填二者 |
| crval1/2,crpix1/2 | ICRS degree、FITS一基参考像元；WcsTransform对外像元为0基。切勿把CRPIX先减1后又在转换器减1 |
| cd11/12/21/22 | degree/px的2×2线性矩阵；须有限且非奇异。TAN与完整SIP参与投影，不以线性WCS代替场边预测 |
| sip_order,A/B/AP/BP | 阶0–5；系数每表36个double、i*6+j。A/B前向，AP/BP逆；可空，A/B不齐时hasSip不成立。阶越界在校准入口捕获为−4，投影函数为−2；不要静默截阶 |
| out_n_matched,out_scale_factor | 必需指针；清洗后的内点数和10^(−location)。单位为[F_syn]/[F_instr]；0返回时仍可能只是NO_DATA占位scale=1 |
| out_sigma_residual,out_diag | 可空；前者dex残差散度，后者分阶段诊断。sigma=0可表示不可估计，不能由此给误差为零 |
| ra,dec,n,out_x,out_y | project专用，输入/输出[n]，degree→0基px；数组必需，即使n=0仍检查指针；n≥0。每点投影非有限应由消费者识别，函数0返回不保证所有点可计算 |

全部输入仅调用期借用，函数不保存span；输出由调用者释放。Gaia查询产生的spec_stars/spectra_buf在正常/已知退化路径由库free；C++异常屏障转错误码但这些是裸指针，异常发生后不能从catch推出全部资源已释放。frame管理自己创建的GaiaClient及临时vector，不把Gaia返回内存转交消费者。生产大图归Runtime；当前frame额外O(WH)试乘缓冲是实现开销。

## 结果布局和逐星身份

```cpp
struct PhotometricDiag {
    int spectrum_rows_total, valid_fsyn;
    int gaia_projected_in_frame;
    int psf_total, psf_valid;
    int spatial_candidates, unique_matches;
    int rejected_ambiguous, rejected_distance, rejected_quality;
    int fit_used, robust_iterations;
    double scale_factor, sigma_residual;
    double r_median, r_p90, r_max;
    double match_distance_median, match_distance_p90, match_distance_max;
};
struct PcMatchRecord {
    int64_t star_id, dr3sp_id;
    double reference_flux, residual;
    int32_t status, reject_reason;
    double f_instr, gaia_mag;
};
```

上述逐字段顺序和类型与头一致；ABI偏移还取决于目标对齐，不手填未经验证的跨编译器sizeof。

spectrum_rows_total是最后成功查询行数，valid_fsyn只数正有限积分；gaia_projected_in_frame数[0,W)×[0,H)；psf_total/valid是输入与status==0行数。spatial_candidates为正向严格半径命中，unique_matches为互最近邻，rejected_ambiguous为前两者差；rejected_distance为距离未命中。rejected_quality混合质量位、非法通量、mag预筛及IRLS拒绝，分支口径不同，不能拿Σ所有rejected+fit_used和任意单分母构造恒等守恒。

fit_used为最后内点，robust_iterations数实际IRLS（S=0为0，50表示触顶可能性）；scale_factor/sigma_residual在正常拟合分支填，与顶层对应。r_median/p90/max统计**有符号原始log比**，不是|r−location|；匹配距离统计来自粗匹配，不保证与最终内点同族。simple不产生谱总数；退化时diag可能保留全零，不能解释为所有步骤真实运行且计数为0。

reference_flux=F_syn，residual=log10(F_instr/F_syn)是**未减location**的dex值；f_instr为PSF域通量，gaia_mag为G。status：0 unmatched、1 matched+used、2 matched+rejected、3 PSF-invalid。reject_reason：0 used、1 mag、2 IRLS、3 invalid-flux、4 no-spatial、5 unmatched-other、6 PSF-invalid；质量位路径还可写**7质量拒绝**，公开头注释未列全，消费者须保留未知枚举值。

当前质量拒绝的reason=7可被后面的invalid回填改为3，不能凭现导出精确恢复全部饱和拒绝。无匹配/PSF失败/无谱时，f_instr与gaia_mag不总被本函数写入，frame已NaN预置；一般C调用者也需初始化，错误返回时整组out不保证事务清空。

dr3sp_id不是Gaia原source_id：ra/dec分别round(degree×10000)后按make_dr3sp_id中的uint64混合生成。位置量化、碰撞和目录版本须保留，不能据哈希等于全球唯一星身份。主关联仍按frame/source/PSF原行维护。

## 生产C++装配对象

以下结构为进程内C++对象（std::string/vector允许在同一构建内使用），不跨C ABI。完整字段及默认值如下；SpatialGainField由spatial_gain.h定义，空间求解单独消费，不从标量函数暗中得到。

```cpp
struct FramePhotFitRequest {
  const double* pixels = nullptr;
  int width = 0;
  int height = 0;
  const double* psf_cx = nullptr;
  const double* psf_cy = nullptr;
  const double* psf_flux = nullptr;
  const int* psf_status = nullptr;
  const uint32_t* psf_quality = nullptr;  
  int n_psf = 0;
  double crval1 = 0.0, crval2 = 0.0, crpix1 = 0.0, crpix2 = 0.0;
  double cd11 = 0.0, cd12 = 0.0, cd21 = 0.0, cd22 = 0.0;
  int sip_order = 0;
  const double* sip_a = nullptr;
  const double* sip_b = nullptr;
  const double* sip_ap = nullptr;
  const double* sip_bp = nullptr;
  std::string gaia_data_dir;   
  std::string filter_name;
  std::string declared_filter_passband;
  std::string qe_name;
  double mag_min = 6.0;
  double mag_max = 16.0;
  int spatial_gain_order = 0;
  int spatial_gain_min_stars_order1 = 50;
  int spatial_gain_min_stars_order2 = 200;
  int spatial_gain_coverage_block_grid = 3;
  int spatial_gain_coverage_min_stars_per_block = 5;
  int spatial_gain_coverage_min_blocks = 6;
  double spatial_gain_coverage_min_bbox_frac = 0.5;
};

enum class FitFailureScope : uint8_t {
  kNone = 0,
  kFrame = 1,
  kEnvironment = 2,
};

struct FramePhotFitResult {
  std::vector<PcMatchRecord> stars;
  int rc = -1;                  
  double k_photo = 1.0;         
  FitFailureScope failure_scope = FitFailureScope::kNone;
  bool fit_ok = false;
  int n_matched = 0;            
  double sigma_residual_dex = 0.0;
  int n_gaia = 0;               
  int psf_valid = 0;            
  int robust_iterations = 0;
  std::string degraded_reason;  
  std::string error;
  bool synthetic_colour_valid = false;
  double synthetic_colour_median_mag = 0.0;   
  int synthetic_colour_n_stars = 0;           
  double synthetic_colour_scatter_mag = 0.0;  
  std::string filter_key;
  std::string filter_curve_name;
  std::string filter_curve_source;
  int filter_n_points = 0;
  double filter_wl_min_nm = 0.0;
  double filter_wl_max_nm = 0.0;
  double filter_val_min = 0.0;
  double filter_val_max = 0.0;
  std::string qe_key;            
  std::string qe_curve_name;     
  std::string qe_curve_source;   
  int qe_n_points = 0;
  double qe_wl_min_nm = 0.0;
  double qe_wl_max_nm = 0.0;
  double qe_val_min = 0.0;
  double qe_val_max = 0.0;
  SpatialGainField spatial;
  bool spatial_frame_fail = false;
};

inline constexpr int kMinFitStars = 3;
FramePhotFitResult fit_frame_photometry(const FramePhotFitRequest& req);
```

request借用像素/PSF/WCS数组到同步调用返回，字符串自持；result按值返回并拥有stars、字符串和空间场。filter_name来自fit.filter，否则block.filter_passband；declared_filter_passband非空时必须字节一致。运行期扫描<exe>/config/response_curves，filter须非Q且channel非空，QE须channel==Q；所有身份核对规则见CONFIG。输入缺曲线标为环境失败，不借C兼容入口的Q=1默认伪造定标。未知响应的合法信号能力由上层单独处理。

查询半径r=√|det(CD)|·√(W²+H²)/2·1.2（degree）；仅r≤0或r≥30时再clamp(r,1,10)，并非每次都限制[1,10]，也不证明含SIP/非等尺度完整足迹。frame从Gaia元数据构造wl_start+i·wl_step，要求get_spectrum_params返回1、count和step正，然后传给v2。

fit_ok是施加判据：C入口rc==0、n_matched≥3、scale正有限才真。C返回非零→环境失败；非物理scale→rc−5/frame；内点不足→rc−6/frame，k_photo恢复占位1而不施加。frame本身无PSF数组是装配环境错误，数组齐但n_psf≤0是本帧失败；配置/曲线库/星表环境错误中止该运行，本帧无数据不阻塞其它独立帧。spatial_frame_fail须另检查，不能因标量fit_ok忽略空间数值失败。

synthetic_colour_*由另一次锥查询计算median(G+2.5log10F_syn)，至少3个正有限F_syn且G有效才valid；scatter为1.482602218505602·MAD。该查询实际消费req.mag_max，与标量拟合的自适应阶梯可能不同星族。它是天区样本颜色项，不是本帧观测零点，也不能拿它定义固定参考SED。空间场仅从records.status==1的(x,y,r)装配，沿请求的样本/覆盖预算拟合；order0不应用空间修正。

## 实际错误、退化和精度差异

| 入口/条件 | 当前返回及输出 |
|---|---|
| simple必需图像/标量输出指针空 | −1；除diag初始化外不保证输出写入 |
| simple W/H≤0 | −2 |
| simple无Gaia数组或无PSF数组/数量 | 返回0、图像恒等复制、scale1、n0、sigma0，**不是头旧注释的−3** |
| with_gaia必需指针/尺寸/filter/spectrum参数无效 | −1 |
| with_gaia handle空 | −2 |
| with_gaia任一次谱查询rc非0 | −3，释放已返回Gaia缓冲；不以最后成功批继续 |
| with_gaia无PSF、查询最终无谱、缓存为空 | 返回0、恒等复制和占位1；v2记录有的分支填部分、有的未填，不称成功标定 |
| 校准C入口C++异常（含非法SIP阶） | −4；错误out不保证清空，异常后裸查询缓冲释放仍需核 |
| project参数指针空/n<0；SIP阶越界；内部异常 | 分别−1/−2/−3，成功0；逐点非有限不自动变整批错误 |

with_gaia保持半径，依12/13/14/15/16查询，达到**返回总谱行数≥2000**或末档停止；该计数不是有效F_syn或最终拟合星数，也没有实现“最多10000”。长度不一致时取DB count作谱stride，缓存仍按传入spectrum_count；底层compute函数仅要求byte数不少于缓存点数，不能据告警文字声称已按DB完整网格正确重采样。

现行v2共用模板校正为out_pixels[i]=static_cast<float>(pixels[i]*scale)，即使T=double也经float舍入；旧non-v2 f64保持double乘法。frame当前丢弃这份试乘图，最终apply_photometry另行处理，故该差异不能直接外推为所有生产产品已失FP64；独立调用v2 f64的消费者受影响，见E69。scale指数溢出/下溢需正有限校验；frame已补该校验，底层C不能据0返回承诺。

当前只有同步调用，无plan/execute/cancel/inspect及IRLS内取消点；线程由宿主OpenMP预算控制，积分按星dynamic64（只整数计数归约），试乘像素static，IRLS串行固定样本序。frame每次自建client/缓存，不自建线程池；共享GaiaClient并发行为未由此API保证。诊断stderr/旧日志的实际输出与统一日志目标需按实施差异处理。

## 非生产孔径路径

acsd::phase1::Photometer(aperture_radius_px=4,sky_annulus_inner=6,sky_annulus_outer=10)的measure(const float* image,int w,int h,double cx,double cy)返回Result<PhotometryResult>。结果含double flux/flux_error/background/snr、bool valid和string failure_reason；输入借用，结果自持。坐标按整数像元中心比较；它不是生产PSF解析通量或F_syn求解器。

收集inner≤distance≤outer的像元，上中位数为bkg；在distance²≤radius²内Σ(image−bkg)。环尺度取上中位数绝对差×1.482602218505602，误差暂用sqrt(max(flux,0)+n_ap sigma_sky²)，SNR=flux/error（error≤0给0）。此误差默认计数与Poisson单位相容，未含gain、背景估计误差、相关/共享噪声和孔径修正，不能当完整科学误差。

坏指针/尺寸返回DATA错误；中心越界、空环/空孔径返回外层ok但valid=false和原因，消费者必须检查二层状态。当前循环已缩到外接框，**旧每源两次全图O(WH)缺陷已修复**；当前约O(r_outer²+n_sky log n_sky)，内存O(n_sky)。NaN中心/像元及非法半径未全面拒绝；不把valid=true提升为经过所有物理验证。

## 连贯使用与验证范围

生产调用先取得同帧PSF解析通量及完整WCS，按真实配置构造FramePhotFitRequest，调用fit_frame_photometry；检查fit_ok及spatial_frame_fail后，才在新的Runtime图像块应用k_photo和m。失败依failure_scope上报；禁止在NO_DATA占位1时标photometry_applied。请求像素保持原样供正式应用与条件噪声；逐星records按PSF行接source ID，response/application身份随正式对象传递。直接C调用者自行分配所有out、NaN初始化可缺测字段、检查返回与n/状态；Crc0或非空数组都不足以声明绝对定标完成。

科学公式和独立真值构型见算法页。这里的签名比对、调用静态可达、归属和分支阅读不表示已编译、运行真实FITS或独立验证；当前数值差异集中E69，保持生产源未改。
