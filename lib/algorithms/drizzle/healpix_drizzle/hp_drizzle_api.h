#ifndef HP_DRIZZLE_API_H
#define HP_DRIZZLE_API_H

// 三态导出 (dllexport / 静态链 / dllimport):
//   事实: 本仓同名生产源被编进**两个** target ——
//     (1) 根 CMakeLists.txt:745 acsd_drizzle  = **STATIC** (全仓生产消费者链它)
//     (2) lib/algorithms/drizzle/CMakeLists.txt acsd_p1_drizzle = SHARED 模块 DLL
//         (ABI-006 导出面净化: 只导出 acsd_module_query_v1, legacy 六符号经
//          version-script / def 降 local)
//   HP_DRIZZLE_EXPORTS 只由**定义侧** TU 定义 (hp_drizzle_api.cpp /
//   hp_drizzle_hips_api.cpp), 模块 CMakeLists 侧禁止补定义 ⇒ 消费者 include
//   本头必落 else 分支拿 __declspec(dllimport) ⇒ 引用 __imp_hp_drizzle_run。
//   而 STATIC 库里不存在 __imp_ 间接层 (那是 import lib / DLL 才有的 thunk)
//   ⇒ Windows 下 LNK2019。修法即第三态 HP_DRIZZLE_STATIC (静态链不加 declspec)。
//   顺序要紧: EXPORTS 必须**先**判, 否则 SHARED target 的定义侧会掉进
//   STATIC 支路, 把 DLL 的导出属性丢掉。
#ifdef _WIN32
#  ifdef HP_DRIZZLE_EXPORTS
#    define HP_DRIZZLE_API __declspec(dllexport)
#  elif defined(HP_DRIZZLE_STATIC)
#    define HP_DRIZZLE_API          /* 静态链接: 无 __imp_ 间接层, 不加 declspec */
#  else
#    define HP_DRIZZLE_API __declspec(dllimport)
#  endif
#else
#  define HP_DRIZZLE_API __attribute__((visibility("default")))
#endif

#include <cstdint>
#include "aio_pipeline.h"   // PipelineFrame 定义

#ifdef __cplusplus
extern "C" {
#endif

// Drizzle 结果
typedef struct {
    int64_t n_healpix_pixels;   // 有效 HEALPix 像素数
    int64_t n_source_pixels;    // 源图像像素数
    int     nside;
    int     nested;             // 1=NESTED, 0=RING
    double  pixfrac;
    double  elapsed_sec;
    char    error_msg[512];     // 错误信息
    // ── 样本级掩膜 / DATA-002 §2a（rule_id NAN-SAMPLE-MASK-COVERAGE-NAN）──
    // 样本级掩膜强制计数（禁静默剔除）。**只增不改**：新字段一律追加在尾部，
    // 既有字段偏移不变（已编译的 DLL/ctypes 消费者不受影响）。
    // 语义: 不合格样本 = ¬isfinite(x_j)（值非有限即不合格）；
    //       被剔除样本从 F_p（分子）、D_p（分母）、Var_p（方差）三项一并剔除
    //       并重新归一；仅 D_p=0 时输出 signal=NaN ∧ support≤0。
    // 方差可用性 = 独立通道，**不参与**合格性判定：V_j 有限且 V_j ≤ 0 属
    //       「有覆盖但无方差信息」⇒ 信号与几何权重照常计入 F_p/D_p（保信号、
    //       保覆盖），仅不计入 Var_p；产品面按
    //       `docs/science/unified/DATA_SEMANTICS.md`「方差与逆方差的三态编码」一节 表达为
    //       variance=0 ∧ ivar=0（显式不可用），**禁止**用常数/地板/哨兵值顶替。
    //       V_j 非有限 = 方差面损坏（同节「损坏」行）⇒ 按不合格样本剔除并
    //       计入下方方差计数。
    // n_rejected_nonfinite = value + variance + nonpositive_weight（可加、互斥）。
    int64_t n_rejected_nonfinite;            // 合计（按原因分类见下三字段）
    int64_t n_rejected_nonfinite_value;      // 值非有限 (NaN/Inf)
    int64_t n_rejected_nonfinite_variance;   // 方差面非有限 (NaN/Inf)
    int64_t n_rejected_nonpositive_weight;   // 权重非有限或 ≤0
} HpDrizzleResult;

// 执行 Drizzle: FITS → .hiss
// fits_path: 输入 FITS 文件路径 (UTF-8)
// output_path: 输出 .hiss 文件路径 (UTF-8, 若以 .ahpx 结尾会自动改为 .hiss)
// nside: HEALPix nside，2 的幂；传 0 走 auto nside (compute_auto_nside)。
//        本函数不设默认 nside —— 数值默认的唯一来源是配置 (drizzle.nside /
//        nside_mode)，调用方不得依赖本层的隐含值。
// nested: 1=NESTED，0=RING。RING 被引擎拒绝 (DRIZZLE.md §4)，此参数仅为
//        ABI 兼容保留，恒应传 1。
// pixfrac: drop 收缩因子，有效域 (0,1]（DRIZZLE.md §4）；<=0 或 >1 显式
//        NO_DATA 拒绝，不夹逼。数值默认的唯一来源是
//        eng/packaging/config/defaults.json 的 drizzle.pixfrac，本层不设默认。
// snr_path: 可选 SNR FITS 文件路径 (nullptr 则不用)
// weight_path: 可选权重 FITS 文件路径 (nullptr 则不用)
// result: 输出结果
// 返回: 0=成功；非 0 = 失败，**正负号无语义**（同一失败面同时存在正值 1 与
//        负值 −1..−14 的返回，且同一负值在不同处表示不同原因）。失败原因
//        只能读 result->error_msg。这是已登记缺陷（正本：detail/registry/
//        acsd.phase1.drizzle.md「错误」节「无集中枚举 —— 登记缺陷」），
//        在错误码集中枚举落地前，调用方**不得**对返回码做分支，只可判 !=0。
HP_DRIZZLE_API int hp_drizzle_fits_to_ahpx(
    const char* fits_path,
    const char* output_path,
    int nside,
    int nested,
    double pixfrac,
    const char* snr_path,
    const char* weight_path,
    HpDrizzleResult* result
);

// Drizzle 阶段: 从 PipelineFrame 命名块直通调用 DrizzleEngine (不经临时 FITS 文件)
// frame: 输入帧 (需含 "data" 块 [H,W] float32 + "header" KV 块含 WCS/SIP 字段)
// nside: HEALPix nside
// nested: 1=NESTED, 0=RING
// pixfrac: 像素收缩因子 (0.0~1.0)
// output_path: 输出 .hiss 文件路径 (nullptr 则不写文件, 仅返回统计; 若以 .ahpx 结尾会自动改为 .hiss)
// result: 输出结果统计与错误信息
// precision_mode: 精度模式 (0=FP32, 1=FP64; -1=未指定: header KV "PRECISION" 优先, 无 KV 缺省 FP64/宪章 §5.3, 未知值显式拒绝)
// 返回: 0=成功, 非0=失败
HP_DRIZZLE_API int hp_drizzle_run(PipelineFrame* frame,
                                   int nside, int nested, double pixfrac,
                                   const char* output_path,
                                   HpDrizzleResult* result,
                                   int precision_mode);

// Phase1 Final Closure: Drizzle -> AIO HiPS 直写 (无 HISS 中转)
// hips_dir: HiPS 产品集根目录; legacy_hiss_path: 可选 legacy .hiss (validation 用)
HP_DRIZZLE_API int hp_drizzle_run_hips(PipelineFrame* frame,
                                       int nside, int nested, double pixfrac,
                                       const char* hips_dir,
                                       const char* legacy_hiss_path,
                                       HpDrizzleResult* result,
                                       int precision_mode);

// Phase1 生产末端正式 C ABI: Drizzle -> 标准 HiPS 直写 (无容器中转)。
// 产物与旧 writer 节点 (读中间容器后写 HiPS) 逐字节等价:
// signal+support 子产品恒写 (support 按 uint8 面积比量化); 输入帧含
// "variance" 块时按 DATA-P1-HIPS §12.1/§12.2 追加 variance/ivar 子产品,
// 不含时保持两产品面不变; 不写 snr, 不写 drizzle provenance;
// product_begin 元数据与旧 writer 一致。
// hips_dir: HiPS 产品集根目录 (signal/ support/ Moc.fits metadata.fits properties)
// filter_passband: obs_filter 透传 (可 NULL 或空串)
// 返回: 0=成功, 非 0=失败 (result->error_msg 给出原因)
HP_DRIZZLE_API int hp_drizzle_run_phase1_hips(PipelineFrame* frame,
                                              int nside, int nested, double pixfrac,
                                              const char* hips_dir,
                                              const char* filter_passband,
                                              HpDrizzleResult* result,
                                              int precision_mode);

// P1-DRZ-ASYNC-01: Phase1 写盘异步化（跨帧流水, 帧内仍单线程且仍按
// parent_ipix 升序）。形态：
//   compute（parse/SNR/drizzle, 帧 worker 线程）
//     → tiles 随任务 move 进 shared_ptr, 投递进程级写池
//     → 帧 worker 立即返回（可原子认领下一帧）
//     → p1_parallel_for join 后按帧下标升序 wait 回收（复现同步判定次序）。
// begin: 执行计算并把 HiPS 直写投递写池；返回时写盘在飞, *out_job 非空
//   （写池宽度 0/未启用时退化为同步路径, *out_job = nullptr）。
// wait: 等待写盘完成并合并 rc/error_msg（返回写盘 rc）。
// disk_full: 写线程内的磁盘满归因（thread_local FailureEpoch 的跨线程回传）。
// free: 释放 job。
// 语义保全：同一 write_hips_phase1、同一 tile 升序、同一 aio 原子落盘 ⇒
// 成功路径逐位不变；p1_stack.json 由调用方在 wait 成功后落盘（写盘失败时
// 同样不产出，与同步路径一致）；失败判定按帧序。
// 线程安全：写池宽度由 P1-DRZ-ASYNC-01 的内存门控决定（见调用方）；
// 写线程内新建 FailureEpoch 取快照（不得复用帧体线程的 epoch：base_ 取自
// 构造线程的 tl_fail_seq() 会假阳性）；g_hips_error 在写线程内读取回传。
typedef struct HpDrizzleJob HpDrizzleJob;
// P1-DRZ-ASYNC-01: 写池宽度的生产侧查询（供调用方保守预检；返回 0 = 未启用）。
// 定义在 hp_drizzle_hips_api.cpp（DrzWritePool 实例侧）；同步路径不调用。
HP_DRIZZLE_API int hp_drizzle_write_pool_width(void);
HP_DRIZZLE_API int hp_drizzle_write_pool_ensure(int width);
HP_DRIZZLE_API int hp_drizzle_run_phase1_hips_begin(PipelineFrame* frame,
                                                    int nside, int nested, double pixfrac,
                                                    const char* hips_dir,
                                                    const char* filter_passband,
                                                    HpDrizzleJob** out_job,
                                                    HpDrizzleResult* result,
                                                    int precision_mode);
HP_DRIZZLE_API int hp_drizzle_job_wait(HpDrizzleJob* job, HpDrizzleResult* result);
HP_DRIZZLE_API int hp_drizzle_job_disk_full(const HpDrizzleJob* job);
HP_DRIZZLE_API void hp_drizzle_job_free(HpDrizzleJob* job);

// ============================================================================
// P17-NSIDE: 自动 NSIDE 决策 (采样率等价 drizzle 1x-2x) 的正式 C ABI。
//
// 语义 (负责人裁定): nside 缺省 (0/空/未给出) => 自动; 依据帧内 WCS/SIP 的
// 最细局部输入像素尺度 finest (自适应四叉树 + 3D 切向量 Jacobian), 取最小
// 2 次幂 NSIDE 使 HEALPix 特征尺度 hp_res = 211076.3/nside <= finest
// => 线性过采样倍率 finest/hp_res ∈ [1, 2); nside 钳位 [16, 2^22]。
//
// frame: 输入帧 (需含 "data" 块 [H,W] 与 "header" KV WCS/SIP; 与 hp_drizzle_run
//        同源解析, 保证决策输入 == drizzle 输入)
// result: 决策依据输出 (nside/finest/hp_res/oversample/clamped)
// 返回: 0=成功; 非 0=失败 (result->error_msg 给出原因)
// ============================================================================
typedef struct {
    int     nside;            // 推荐 NSIDE (2 的幂); 失败时 0
    double  finest_arcsec;    // 最细局部输入像素尺度 (角秒/像素)
    double  hp_res_arcsec;    // 所选 NSIDE 的 HEALPix 特征尺度 (角秒)
    double  oversample;       // hp_res/finest (引擎口径; 0.5~1 = 1~2 倍过采样)
    int     clamped;          // 1 = nside 被 [16, 2^22] 钳位 (决策不自由)
    char    error_msg[512];   // 失败原因
} HpAutoNsideResult;

HP_DRIZZLE_API int hp_drizzle_compute_auto_nside(PipelineFrame* frame,
                                                 HpAutoNsideResult* result);

// ============================================================================
// 反向 Drizzle (Sphere -> Plane, 球面面积语义) — 签字修正 REV-101 正式 C ABI
// ============================================================================

// 反向 Drizzle 输入 (扁平 WCS/SIP)
typedef struct {
    int32_t  nside;              // HEALPix NSIDE (2 的幂, 1..2^22)
    int32_t  nested;             // 必须 1 (NESTED)
    int32_t  target_width;       // 输出平面宽 (>0)
    int32_t  target_height;      // 输出平面高 (>0)
    double   pixfrac;            // source leaf 球面收缩 (0, 1]
    int32_t  output_fp64;        // 1=FP64 输出, 0=FP32 输出
    // WCS/SIP
    double   crval[2];           // 度
    double   crpix[2];           // 1-based
    double   cd[4];              // [cd1_1, cd1_2, cd2_1, cd2_2]
    int32_t  sip_order;          // 0..5 (K 值; hp_drizzle_api.cpp 的取值域校验即此范围)
    int32_t  sip_ap_order;       // 0..5
    double   sip_a[36];          // (order+1)^2 系数, order=5 时恰好用满 36
    double   sip_b[36];
    double   sip_ap[36];
    double   sip_bp[36];
    // source leaf 数据
    const uint64_t* leaf_ipix;       // NESTED ipix
    int64_t  n_leaf;
    const float*   leaf_signal_f32;  // 与 leaf_signal_f64 二选一 (非空即使用)
    const double*  leaf_signal_f64;
    const double*  leaf_support;     // [0,1], 可选 (NULL=全 1.0)
    int32_t  no_data_as_zero;        // 1=无覆盖输出 0, 0=NaN
} HpReverseDrizzleInput;

// 反向 Drizzle 结果/统计
typedef struct {
    int64_t n_source_leaf;
    int64_t n_target_pixel_touched;
    int64_t n_candidates;
    int64_t n_overlaps;
    double  total_signal_in;
    double  total_signal_out;
    double  total_covered_area_in;
    double  total_covered_area_out;
    int64_t n_invalid_ipix;
    int64_t n_nonfinite;
    int64_t n_skipped_outside;
    char    error_msg[512];
} HpReverseDrizzleResult;

// 执行反向 Drizzle。
// in: 输入 (严格校验, 非法返回非 0)
// signal_out / coverage_out: 输出缓冲区, 尺寸 width*height;
// output_fp64=1 时按 double 数组, 否则按 float 数组。
// result: 统计与错误信息。
// 返回 0=成功, 非 0=失败。
HP_DRIZZLE_API int hp_drizzle_reverse_run(
    const HpReverseDrizzleInput* in,
    void* signal_out,
    void* coverage_out,
    HpReverseDrizzleResult* result);

// 反向 Drizzle 能力/版本 (capability bit):
// 0x01 球面面积权重 | 0x02 FP32 数据面 | 0x04 FP64 数据面 |
// 0x08 support 语义 | 0x10 partial support (均匀假设) | 0x20 严格输入校验
HP_DRIZZLE_API uint32_t hp_drizzle_reverse_capability(void);
HP_DRIZZLE_API const char* hp_drizzle_reverse_version(void);

#ifdef __cplusplus
}
#endif

#endif // HP_DRIZZLE_API_H
