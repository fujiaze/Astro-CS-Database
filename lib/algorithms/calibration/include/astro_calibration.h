#ifndef ASTRO_CALIBRATION_H
#define ASTRO_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/* AC_API 可被构建方预定义覆盖: astrocs_p1_calibration DLL target (P1-CAL-IMPL)
 * 以 -DAC_API= 将 12 个 legacy 符号本地化, 导出面仅 astrocs_module_query_v1
 * (ABI-006; 对齐 GAIA_EXPORT= 先例)。未定义时保持原语义, 既有构建零变化。 */
#ifndef AC_API
#ifdef _WIN32
#define AC_API __declspec(dllexport)
#else
#define AC_API __attribute__((visibility("default")))
#endif
#endif

/* ========== 常量 ========== */
#define AC_COMBINE_MEAN   0
#define AC_COMBINE_MEDIAN 1

#define AC_METHOD_MEDIAN  0
#define AC_METHOD_BILINEAR 1

#define AC_OK             0
#define AC_ERR_PARAM     -1
#define AC_ERR_MEMORY    -2
#define AC_ERR_INTERNAL  -3

/* 坏列（linear defect）路径状态位（按位或；0 = 无降级）。
 * 与 ac_correct_frame 的坏点路径完全独立：不改动其判据、阈值与语义。 */
#define AC_COLSTAT_OK                 0
#define AC_COLSTAT_SCALE_DEGENERATE   1   /* MAD(dev)==0 => 检测不可用，不判任何列 */
#define AC_COLSTAT_FRAME_TOO_SMALL    2   /* 列数不足 => 无列可判 */
#define AC_COLSTAT_EDGE_ONE_SIDED     4   /* 坏列段贴边，仅单侧锚点 => 复制该锚列 */
#define AC_COLSTAT_NO_ANCHOR          8   /* 坏列段无任何好列锚点 => 不修复，保留原值 */
#define AC_COLSTAT_EDGE_EXCLUDED      16  /* 适用域声明：首尾 k 列无双侧邻域，未参与判定
                                           * （单侧基准对横向梯度系统性有偏 => 必然假阳性；
                                           *  该位恒置，非故障。实际排除列数见 manifest 的
                                           *  bad_column_edge_columns_excluded） */
#define AC_COLSTAT_WIDE_DEFECT        32  /* 检出宽度 > max_seg_len 的缺陷段：**未修复**，
                                           * 仅在 col_mask 里以值 2 标记（依据
                                           * docs/contracts/DATA_SEMANTICS.md §10.4
                                           * 产物表：值 2 = 仅标记未修；只修单列）。
                                           * 该位只声明"存在被跳过的宽段"，不静默。 */

/* col_mask 的取值语义（不是布尔！） */
#define AC_COLSTAT_MASK_CLEAN  0   /* 未检出 */
#define AC_COLSTAT_MASK_REPAIRED 1 /* 单列缺陷，已按隔壁列插值修复 */
#define AC_COLSTAT_MASK_WIDE   2   /* 宽缺陷段（> max_seg_len），仅标记、未修复 */

/* ========== 主帧生成 ========== */

/* 生成 Master Bias：sigma-clip + median/mean 合并
 * stack: [n_frames * height * width] float32 行优先
 * out: [height * width] float32
 * combine: AC_COMBINE_MEAN 或 AC_COMBINE_MEDIAN
 */
AC_API int ac_generate_master_bias(
    const float* stack, int n_frames, int width, int height,
    float* out,
    float sigma_low, float sigma_high, int max_iterations,
    int combine);

/* 生成 Master Dark：sigma-clip + median/mean 合并（不减Bias，Dark已含Bias）
 * 同 ac_generate_master_bias
 */
AC_API int ac_generate_master_dark(
    const float* stack, int n_frames, int width, int height,
    float* out,
    float sigma_low, float sigma_high, int max_iterations,
    int combine);

/* 生成 Master Flat：减Bias + 逐帧归一化 + sigma-clip + mean + 再归一化
 * flat_stack: [n_frames * height * width]
 * master_bias: [height * width] 或 NULL
 * out: [height * width]
 */
AC_API int ac_generate_master_flat(
    const float* flat_stack, int n_frames, int width, int height,
    const float* master_bias,
    float* out,
    float sigma_low, float sigma_high, int max_iterations);

/* ========== 图像校准 ========== */

/* 校准单帧 Light
 * 无暗场优化: (Light - Dark) / Flat
 * 有暗场优化: (Light - Bias - K*(Dark - Bias)) / Flat
 *
 * light: [height * width]
 * master_dark: [height * width] 或 NULL
 * master_flat: [height * width] 或 NULL
 * master_bias: [height * width] 或 NULL
 * out: [height * width]
 * dark_optimization: 0=关闭, 1=开启
 * dark_scale_factor: K初始值（如 Light曝光/Dark曝光）
 * actual_k: 输出实际使用的K值（可选，NULL则不输出）
 */
AC_API int ac_calibrate_frame(
    const float* light, int width, int height,
    const float* master_dark, const float* master_flat, const float* master_bias,
    float* out,
    int dark_optimization, float dark_scale_factor,
    float* actual_k);

/* ========== 坏点修复 ========== */

/* 校正单帧图像的坏点
 * 检测方法：Dark全局统计检测热像素 + Bias全局统计检测冷像素
 *
 * data: [height * width] 校准后Light
 * master_dark: [height * width] 或 NULL
 * master_bias: [height * width] 或 NULL
 * out: [height * width]
 * hot_sigma: Dark热像素检测sigma倍数
 * cold_sigma: Bias冷像素检测sigma倍数
 * method: AC_METHOD_MEDIAN 或 AC_METHOD_BILINEAR
 * max_structure_size: 连通区域大小阈值（>=此值视为星点）
 * out_hot: 输出热像素数（可选）
 * out_cold: 输出冷像素数（可选）
 */
AC_API int ac_correct_frame(
    const float* data, int width, int height,
    const float* master_dark, const float* master_bias,
    float* out,
    float hot_sigma, float cold_sigma,
    int method, int max_structure_size,
    int* out_hot, int* out_cold);

/* ========== 坏列（linear defect, 单列）检测与修复 ==========
 *
 * 【与 ac_correct_frame 的关系】完全独立的新路径。ac_correct_frame 的检测
 * 判据、阈值、插值语义与产物（out/out_hot/out_cold）在本合同下**零改动**；
 * 本函数不调用它，它也不调用本函数。两者可同时施加（先坏列、后坏点），
 * 互不覆盖：坏点按像素判定，坏列按列判定。
 *
 * data: 检测与修复源帧 [height*width] float32（生产 = cal 节点产物）
 * out:  修复帧 [height*width] float32（调用方分配；非坏列逐像素恒等）
 * column_sigma: 判据阈值（帧内 MAD 倍数，无量纲）；<=0 = 显式禁用本路径
 * neighbor_k:   横向邻域半径（每侧列数）；<=0 归一到 3
 * col_mask:     输出 [width] unsigned char，1 = 该列被判为坏列（可 NULL）
 * out_n_cols:   输出检出坏列数（可 NULL）
 * out_px_repaired: 输出被插值覆盖的像素数 = Σ(坏列段长度)*height（可 NULL）
 * out_sigma_col:输出帧内稳健尺度 1.482602218505602*MAD(dev) [ADU]（可 NULL，供审计）
 * out_status:   输出状态位（AC_COLSTAT_* 的按位或；可 NULL）
 *
 * 检测（列统计量的跳变配对分段）：colstat[x]=median_y data；
 *   d[x]=colstat[x]-colstat[x-1]；sigma_col=1.482602218505602*MAD(d)；
 *   显著跳变 J={x: |d[x]-median(d)| >= column_sigma*sigma_col}；
 *   边界 {0}∪J∪{width} 把列轴切成电平段；段长 <= 2*neighbor_k-1 且非全宽
 *   ⇒ 判为坏列段。判据建在一阶差分上，对缓变结构、段内互相印证、坏列污染
 *   邻居基准三种失败模式同时免疫；且与修复算子（段外锚点线性插值）同构。
 *   sigma_col==0 时显式降级（AC_COLSTAT_SCALE_DEGENERATE），不判任何列。
 * 修复：隔壁列插值。单列缺陷 = 左右两邻算术平均；连续坏列段 = 段两端最近
 *   好列之间的线性插值。插值一律取自输入 data（不级联）=> 闭式解析预期。
 *   段贴边（仅单侧锚点）=> 复制该锚列并置位；无锚点 => 保留原值并置位。
 * 返回：AC_OK；data/out 空指针或 width/height 非正 => AC_ERR_PARAM（不写 out）。
 */
AC_API int ac_correct_columns(
    const float* data, int width, int height,
    float* out,
    float column_sigma, int neighbor_k, int max_seg_len,
    unsigned char* col_mask,
    int* out_n_cols, int* out_px_repaired,
    float* out_sigma_col, int* out_status);

/* ========== 坏列定位：dark / bias 母版路径 + 三路径仲裁 ==========
 *
 * 【为什么要有母版路径】整列缺陷的**物理来源**是探测器本身，而 dark/bias 母版
 * 是这一缺陷最直接的观测面：母版上没有天体结构，坏列表现为该列电平的系统性
 * 偏离，判据比科学帧自身统计干净得多（成熟实现同向：Siril 的缺陷图从 dark
 * 生成；IRAF ccdmask 的列向中值窗默认 7；见 run/LINDEF-IMPL-01/LIT-methods.md）。
 *
 * 【标度无关性】三条路径各自**帧内自校准**（判据尺度取本帧 MAD），且判据只
 * 依赖列统计量的**横向相对**关系 ⇒ 对检测源的正标度变换严格不变（与
 * detect_hot_pixels 的同类不变性同性质，ALG-COS-001 §1）。因此母版与科学帧
 * 标度不一致**不影响**坏列检出集合，无需标度声明即可安全并用。
 *
 * 判据与 ac_correct_columns 完全同构（跳变配对分段），阈值语义同：
 *   column_sigma <= 0 显式禁用；neighbor_k <= 0 归一到 3；段长上限 2k-1。
 */
AC_API int ac_detect_bad_columns_from_master(
    const float* master, int width, int height,
    float column_sigma, int neighbor_k, int max_seg_len,
    unsigned char* col_mask,
    int* out_n_cols, float* out_sigma_col, int* out_status);

/* 三路径仲裁来源位（out_source_mask[x] 的按位或） */
#define AC_COLSTAT_SRC_SCIENCE 1u   /* 该列在科学帧自身被判坏列 */
#define AC_COLSTAT_SRC_DARK    2u   /* 该列在 master_dark 被判坏列 */
#define AC_COLSTAT_SRC_BIAS    4u   /* 该列在 master_bias 被判坏列 */
/* 列级置信度（由来源组合导出，供下游筛选；不改判据本身） */
#define AC_COLSTAT_CONF_HIGH   3u   /* dark 与 bias 一致判坏（物理来源双重印证） */
#define AC_COLSTAT_CONF_MEDIUM 2u   /* 仅一个物理来源判坏，或科学帧+任一母版 */
#define AC_COLSTAT_CONF_LOW    1u   /* 仅科学帧自身判坏（生产上母版不可得时的主路径） */

/* 三路径仲裁 + 修复（坏列的完整入口）。
 * science 必需；master_dark / master_bias 可为 NULL（该路径不参与）。
 * 仲裁规则 = **并集**并逐列记录来源（不用交集：生产两条路径都不接线母版，
 * 科学帧路径必须能独立工作；交集会漏掉只在一处可见的缺陷）。每列的来源与
 * 置信度写入 out_source_mask / out_conf_mask，供下游筛选与审计——不静默丢弃
 * 任何一路的证据。若某列仅由母版判出而科学帧未判，仍修复（该缺陷经校准会
 * 进入科学帧，只是单帧统计未必显形），置信度标记为 MEDIUM。
 */
AC_API int ac_correct_columns_ex(
    const float* data, const float* master_dark, const float* master_bias,
    int width, int height, float* out,
    float column_sigma, int neighbor_k, int max_seg_len,
    unsigned char* col_mask,
    unsigned char* out_source_mask,
    unsigned char* out_conf_mask,
    int* out_n_cols, int* out_n_science, int* out_n_dark, int* out_n_bias,
    int* out_px_repaired, float* out_sigma_col, int* out_status);

/* ========== 坏列：母版专用阈值 + 已修/仅标记分账（LINDEF-CLOSE-01）==========
 *
 * 【为什么母版要单独一个阈值】三条路径**各自帧内自校准**（尺度取本帧 MAD），
 * 但"同一个 column_sigma 数值"在三条路径上**不是同一个松紧度**：母版的 d 分布
 * 重尾（真实 T3 master dark/bias 实测 |d-med| 的 p99/p50 ≈ 23，高斯分布该比值
 * 为 2.6），同一 5σ 在母版上放进的假跳变比科学帧多一个量级 ⇒ 母版路径的
 * **仅标记**（宽缺陷，掩膜值 2）列数虚高（真实数据实测帧 1 达 924 列）。
 * 因此母版路径用**独立的** master_column_sigma；其取值按母版自身的稳健尺度
 * 标定（见 run/LINDEF-CLOSE-01/REPORT.md §4 的标定曲线与落点）。
 *   母版阈值必须 >= 科学帧阈值：更严，不得更松（放松判据 = 回退）。
 *
 * 【宽标记的声明区间与守卫 master_wide_frac_max / science_wide_frac_max】
 * **"仅标记"（宽缺陷，掩膜值 2）这个动作本身必须有可声明的上界**——没有上界就
 * 等于一个没有推导的常数。两条路径各有一个上界参数（占帧宽的比例；<=0 = 不设界），
 * 某路超过上界 ⇒ **该路整类宽标记丢弃**并把丢弃列数分别写入
 * out_master_wide_suppressed / out_science_wide_suppressed（不静默、**不影响已修判定**）。
 * 依据（实测）：两类路径的宽标记都不是稳定物理结构 ——
 *   母版：同一 master bias 在 k=5/10/30/40/60/80 上标 350 / 596 / 1383 / **0** / 1419 / **0** 列；
 *   科学帧：真实 T3 第 2 帧标 588 列（同一帧第 1 帧 0 列），同为"就近反号配对"
 *   在重尾 d 上的配对事故（相距很远的两个显著跳变被配成一段）。
 * 为什么只丢"标记"不动"判据"：已修（掩膜值 1）是逐列的可验证判决（单列段 + 解析插值），
 * 宽标记只是"这一片可疑"的提示且**从不参与修复**；给它设上界不放松任何判据。
 *
 * 【已修与仅标记必须分账】并集掩膜只说明"这一列被判为缺陷"，不说明"改没改
 * 像素值"。out_n_cols = 已修（掩膜值 1）；out_n_marked_only = 仅标记（值 2）；
 * 二者不得互相顶替（下游按掩膜筛选权重时，只有前者真的换了值）。
 *
 * 其余语义与 ac_correct_columns_ex **逐字相同**（同判据、同仲裁、同修复算子）；
 * master_column_sigma <= 0 时归一到 column_sigma、两个 *_wide_frac_max <= 0 时
 * 不设界 ⇒ 与旧入口逐位一致。
 */
AC_API int ac_correct_columns_ex2(
    const float* data, const float* master_dark, const float* master_bias,
    int width, int height, float* out,
    float column_sigma, float master_column_sigma, float master_wide_frac_max,
    float science_wide_frac_max,
    int neighbor_k, int max_seg_len,
    unsigned char* col_mask,
    unsigned char* out_source_mask,
    unsigned char* out_conf_mask,
    int* out_n_cols, int* out_n_marked_only,
    int* out_n_science, int* out_n_dark, int* out_n_bias,
    int* out_n_science_marked, int* out_n_dark_marked, int* out_n_bias_marked,
    int* out_master_wide_suppressed, int* out_science_wide_suppressed,
    int* out_px_repaired, float* out_sigma_col,
    float* out_sigma_col_dark, float* out_sigma_col_bias,
    int* out_status);

/* ========== 修复像素的方差面：var = (Σᵢwᵢ²·var)·κ（LINDEF-CLOSE-01）==========
 *
 * 【本层为什么产方差**因子**而不是方差本身】DATA-P1-COS §10.3 冻结
 * "variance/ivar 不在本层（由 snr_estimator 独立估计）"。本函数不越这条线：
 * 它只把"修复算子的权重平方和 Σwᵢ²"与"是否被修复"这两个**本层已知的事实**
 * 施加到调用方给的方差面上，不引入任何新的噪声模型。
 *
 * 【处方（冻结式）】
 *     var_out[p] = (Σᵢ wᵢ² · var_in[p]) · κ       p 属于被修复列
 *     var_out[p] = var_in[p]                      p 未被修复（干净 or 仅标记）
 * 权重即修复算子自己的插值权重（与 repair_bad_columns 同式）：
 *     段 [a,b] 两侧锚点 L=a-1、R=b+1：w_L=(R-x)/(R-L)、w_R=(x-L)/(R-L)；
 *     单列段（本层默认 max_seg_len=1）⇒ w_L=w_R=1/2 ⇒ Σw² = 1/2；
 *     贴边单侧复制 ⇒ w=1 ⇒ Σw² = 1。
 *
 * 【三个量必须并列写清，不得只留一个（依据 docs/contracts/DATA_SEMANTICS.md
 *   §10.5「列状缺陷的方差处置」：Σw²、κ 与实测口径三者并列）】
 *   (Q1) **Σw² = 1/2**（传播式）：单列段两侧锚点各 1/2 ⇒ 权重平方和 1/2。
 *        这是"把修复算子自己的插值权重施加到方差面"（与 LSST meas_algorithms
 *        w.2026.39 src/Interp.cc:2108-2110 用同一个 do_defects 插值 variance 面同源）。
 *   (Q2) **κ = 2**（本层取值，配置键 bad_column_variance_kappa 默认 2.0）：
 *        由"独立信息不重复计数"推导 —— 修复值 = 同帧两个**已被计入数据**的邻居的
 *        确定性函数，若给它 1/(Σw²·var) 的 ivar 就是**重复计数**邻居的光子；
 *        故其 ivar 不得高于一次独立测量 ⇒ κ >= 1/Σw² = 2。取最小可行值 2。
 *        语义域：**该像素携带多少独立信息**（逆方差加权 / 稠密 SNR 重建用）。
 *   (Q3) **Var(pred − truth) = 0.5502·σ²**（实测，蒙特卡洛真值已知；配套实测
 *        留一干净列 σ_r/(√1.5·σ_pix) = 0.9998）。量级与 Σw²·σ² = 0.5σ² 一致
 *        （偏差项 bias² = 1.5e-4·σ² 可忽略）。
 *        **适用域 = 独立噪声下"估计量误差"**（插值值作为该位置真值的估计有多准）——
 *        它确实**小于** σ²，因为两邻平均降低了噪声。
 * 【Q2 与 Q3 冲突时的取舍（依据 DATA_SEMANTICS.md §10.5 的 SNR 偏高条款）】
 * 二者语义域不同：Q3 说"估计得准"，Q2 说
 * "携带的独立信息不大于一次测量"。**κ 偏小 ⇒ 修复像素 SNR 系统性偏高**
 * （稠密 SNR 重建时把插值像素当成比干净像素更可靠，正是本项点名的危害）；
 * **κ 偏大只是保守**。预发布阶段取**保守且可推导**的一侧 ⇒ 默认 κ = 2.0。
 * 保留 Q3 的实测不删：它是"若下游语义确实是估计量误差、且能正确处理与邻居的
 * 相关性"时把 κ 取 1 的依据。κ 由调用方给出，掩膜 0/1/2 已落盘，下游可按自己的
 * 语义选（kappa=2 / kappa=1 / 直接按掩膜剔除）。
 *
 * col_mask: [width]，取值语义同产物掩膜：1 = 已修（inflate），2/0 = 原样拷贝。
 * out_w2_mean: 输出被修复列的平均 Σw²（可 NULL，供审计）；
 * out_px_inflated: 输出被乘过 κ 的**列数**（可 NULL）。
 * 返回：AC_OK；var_in/var_out/col_mask 空指针、width/height 非正、kappa 非有限
 * 或 <=0 ⇒ AC_ERR_PARAM（不写 var_out）。
 * 非有限输入按同态处理（NaN→NaN、Inf→Inf），不做静默替换。
 */
AC_API int ac_column_variance_inflate(
    const float* var_in, int width, int height,
    const unsigned char* col_mask,
    float kappa,
    float* var_out,
    float* out_w2_mean, int* out_px_inflated);

/* ========== 双精度 ABI (FP64) ==========
 *
 * 双精度 ABI 改造 (R10): FP64 模式下全链路使用 double, 不降级到 float32。
 *   - ac_calibrate_frame_f64: 像素级算术 (light-bias-K*(dark-bias))/flat 在 double 上运行,
 *     不降级 (精度关键路径)。
 *   - ac_generate_master_bias_f64 / dark_f64 / flat_f64 / ac_correct_frame_f64:
 *     统计/mask 操作, 内部将 double 输入转 float 调用 f32 实现, 输出转回 double。
 *     (这些函数用于 master 帧预生成与坏点修复, orchestrator 的 run_stage_calibrate
 *      不调用它们, 因此不影响 FP64 全链路精度。)
 * 向后兼容: 原有 float32 API 保留不变。
 */

AC_API int ac_generate_master_bias_f64(
    const double* stack, int n_frames, int width, int height,
    double* out,
    double sigma_low, double sigma_high, int max_iterations,
    int combine);

AC_API int ac_generate_master_dark_f64(
    const double* stack, int n_frames, int width, int height,
    double* out,
    double sigma_low, double sigma_high, int max_iterations,
    int combine);

AC_API int ac_generate_master_flat_f64(
    const double* flat_stack, int n_frames, int width, int height,
    const double* master_bias,
    double* out,
    double sigma_low, double sigma_high, int max_iterations);

AC_API int ac_calibrate_frame_f64(
    const double* light, int width, int height,
    const double* master_dark, const double* master_flat, const double* master_bias,
    double* out,
    int dark_optimization, double dark_scale_factor,
    double* actual_k);

AC_API int ac_correct_frame_f64(
    const double* data, int width, int height,
    const double* master_dark, const double* master_bias,
    double* out,
    double hot_sigma, double cold_sigma,
    int method, int max_structure_size,
    int* out_hot, int* out_cold);

/* ac_correct_columns 的 double 变体：与 ac_correct_frame_f64 同款降级语义
 * （double 输入转 float32 执行、输出回转 double；统计/mask/插值全程 f32）。 */
AC_API int ac_correct_columns_f64(
    const double* data, int width, int height,
    double* out,
    double column_sigma, int neighbor_k, int max_seg_len,
    unsigned char* col_mask,
    int* out_n_cols, int* out_px_repaired,
    double* out_sigma_col, int* out_status);

/* ========== 工具 ========== */

/* 设置OpenMP线程数 */
AC_API void ac_set_num_threads(int n);

/* 获取版本号 */
AC_API const char* ac_version();

#ifdef __cplusplus
}
#endif

/* ========== C++ 接口（最优 Dark 系数估计）========== */
/* 纯 C++ 接口，使用 hiss::Stage1Diagnostics 结构化诊断，仅供 C++ 调用方使用。
 * 对应实现见 src/dark_optimizer.cpp。
 */
#ifdef __cplusplus
#include "hiss_format.h"  // hiss::Stage1Diagnostics (02_FROZEN §2.3)
namespace ac {
/* 最优 Dark 系数估计
 * 模型: L - B = c + k*(D - B)
 * 算法: 背景提取(sigma-clip) + 8x8 分区抽样 + 鲁棒线性回归(MAD 离群抑制, 5 轮迭代)
 * 失败处理: 输出结构化诊断后自动回退曝光时间比例 k_init，
 *           设置 diagnostics.fell_back=1, fallback_from="OPTIMAL", fallback_to="EXPOSURE_RATIO"
 * 返回: 最优 k 值；失败时返回 k_init
 */
float optimize_dark_k(const float* light, const float* bias, const float* dark,
                      const float* flat, int w, int h, float k_init,
                      hiss::Stage1Diagnostics& diagnostics);
}
#endif

#endif /* ASTRO_CALIBRATION_H */
