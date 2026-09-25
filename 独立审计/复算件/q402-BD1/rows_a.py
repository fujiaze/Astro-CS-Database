# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, r"独立审计/复算件/q402-BD1")
from emit import emit

R = [
# ── 1 sigma_floor ─────────────────────────────────────────────────────────
["sigma_floor", "eng/tests/backend/syn008_seam_main.cpp:105", "0.02（本行）；同符号生产侧 1e-3",
 "ADU（σ 标度，非 ADU²）",
 "与所消费 uncertainty 同标度（帧面标度；photo_scaled_adu 时按 α 换算）",
 "缺（文档只给数值与分工，未给精度/收敛要求）",
 ">0；≤0 时 upm.cpp:292 静默钳回 1e-3",
 "C（合同/算法文档登记 1e-3；本行 0.02 无任何登记）",
 "唯一数值源：docs/contracts/DATA_SEMANTICS.md:2086（冻结 1e-3）＋ lib/algorithms/coverage/include/astro/phase2/upm.h:83、src/upm.cpp:273/:292；测试侧 0.02 = 无",
 "④待确认（并案：多侧取值不一致）",
 "UPM 观测降权路径 sigma_eff=max(|unc|,sigma_floor)",
 "三侧并列：1e-3（docs＋upm.h:83＋stage2_common.h:60＋5 个 coverage/*.json＋stage2_common.cpp:169 兜底）/ 0.02（syn008_seam_main.cpp:105）/ 1e8（eng/tests/validation/release02/fix_p2a_seam_oracle/p2a_oracle.cpp:134）。0.02 不是无效占位——同夹具 kNoiseRms=0.05（:25），故地板与噪声同阶，Huber z 被真实压制 ⇒ 该测试测的是\"抬高地板后的降权\"而非冻结口径。出现处数见 md 计数命令（tot=165：lib 63/eng 6/docs 25/实验 71）。另：同符号单位两说（DATA_SEMANTICS:2086「σ（ADU）」vs docs/algorithms/PHASE2_SESSION.md:142「面亮度 ADU·sr⁻¹」）"],

# ── 2 snr ────────────────────────────────────────────────────────────────
["snr", "eng/tests/backend/syn008_seam_main.cpp:96", "100.0",
 "无量纲（观测信噪比）", "逐控制观测（per-control），无归一化", "不适用（激励值）",
 "权重式要求 snr≥0；snr²/(1+snr²) 单调",
 "无（测试激励值，非科学量）",
 "无（作为激励值无需出处；权重式见 docs/algorithms/PHASE2_UPM_IMPL.md:166）",
 "不适用（结构性常数——测试激励，不进科学语义）",
 "P2ControlObservation.snr → raw_w 权重因子",
 "机械层写 \"1.0;100.0\" 是把同行 `o.support = 1.0` 并了进来（:96 一行三赋值），本符号实际只有一个值 100.0。取 100 的意图可由式核验：snr²/(1+snr²)=0.99990，即\"不降权\"。出现处数（模式 `\\.snr = `）tot=94：lib 67/eng 20/docs 1/实验 6；字面 100 同形命中 tot=18"],

# ── 3 max_rel ────────────────────────────────────────────────────────────
["max_rel", "eng/tests/cpu/avx2/provider_avx2_oracle_main.cpp:262", "0.0f",
 "无量纲（相对偏差）", "逐元素 max 归约", "不适用", "非负上确界累加器",
 "无（结构性）",
 "无",
 "不适用（结构性常数）",
 "AVX2 vs baseline 逐元素最大相对差统计",
 "进科学语义的判据：0 是 max 归约的初值（:263-265 取 max），不是阈值；本文件真正的容差另有其值。模式 `max_rel` 全仓 tot=284 命中，绝大多数为同名异址累加器（机械层按名字而非按址聚合的固有偏差），本判读只覆盖 :262 一处"],

# ── 4 read_noise_e ───────────────────────────────────────────────────────
["read_noise_e", "eng/tests/integration/v6_p1/v6_p1_integrate_test.cpp:101", "5.0",
 "e⁻", "探测器元数据（逐帧标量）", "不适用（激励值）", "≥0；与 gain 同域进方差式",
 "无（测试激励值）",
 "单位与语义有 A/C 锚：docs/science/NOISE_MODEL.md:44（read_noise_e: e-）、§5c 方差式；数值 5.0 本身无出处",
 "不适用（结构性常数——测试激励）",
 "DetectorMetadata.read_noise_e",
 "五件事对**符号**齐备（单位/域见 SCI），缺的只是本行的数值来源，而测试激励不需要来源。但注意生产事实：DISP-NOISE-003（docs/algorithms/NOISE_ESTIMATION.md:198）登记 SnrNoiseModelConfig 的 gain_e_per_adu/read_noise_e/use_gain_model 三字段在 noise_model_impl 中**零读取** ⇒ 该 fixture 不进入生产权重面语义；只在诊断式 snr_noise_gain_variance 路径生效。出现处数 tot=353（lib 38/eng 19/docs 15/实验 276）"],

# ── 5 dark ───────────────────────────────────────────────────────────────
["dark", "eng/tests/integration/v6_p1/v6_p1_integrate_test.cpp:145", "100.0",
 "ADU", "逐像素（CalPixelInput）", "不适用（激励值）", "has_dark=true 时参与扣减",
 "无（测试激励值）", "无",
 "不适用（结构性常数——测试激励）",
 "Phase-1 校准像素输入",
 "机械层把符号记作 `dark` 过于宽泛：它是 px.dark 字段赋值，不是具名常数。模式 `\\.dark = ` 全仓 tot=10（eng 9/实验 1），本行值 100 与同行 px.r=1000、flat=1.0、bias_light=50 构成\"整数量级便于手算\"的夹具设计，无科学含义"],

# ── 6 bias_dark ──────────────────────────────────────────────────────────
["bias_dark", "eng/tests/integration/v6_p1/v6_p1_integrate_test.cpp:151", "50.0",
 "ADU", "逐像素", "不适用（激励值，但与 bias_light 相等是判据前提）",
 "has_bias_dark=true 时参与系数折叠",
 "无（测试激励值）", "无",
 "不适用（结构性常数——测试激励）",
 "OI-02 系数折叠精确形式（同文件 :133 注释）",
 "值本身任意，**但 50.0 必须等于同行 bias_light=50.0**（:149/:151 同值）才能触发\"共享 master → 系数折叠\"路径；这层\"相等约束\"是判据的一部分却未被任何文档登记，靠字面量巧合维持——若日后有人单独调 bias_light，折叠用例静默退化为非折叠用例而断言仍绿。同类同值站点见 v6_p2_integrate_test.cpp:120。出现处数（bias_dark）tot=33（lib 17/eng 16）"],

# ── 7 closure_rel_tol ────────────────────────────────────────────────────
["closure_rel_tol", "eng/tests/integration/v6_p1/v6_p1_integrate_test.cpp:606", "0.0（本行，负例探针）；生产默认 1e-6",
 "无量纲（相对闭合残差阈值）", "逐输出像素几何闭合判据（|closure_rel|>tol 即具名失败）",
 "缺（未见任何精度推导）", "判据取绝对值，>0 才有意义",
 "无（生产默认 1e-6 亦无档；本行 0.0 有行内注释说明是故意取零制造必败）",
 "语义锚：lib/algorithms/drizzle/healpix_drizzle/v6_spherical_overlap.h:73-75（定义判据形式）；数值 1e-6 唯一源＝代码默认 lib/algorithms/integration/v6_phase1/include/astrocs/v6/phase1_product.h:131，**docs 侧零登记**（closure_rel_tol 在 docs/** 命中 0）",
 "④待确认（生产默认 1e-6 无量纲依据；本行 0.0 本身按负例探针计结构性）",
 "drizzle 几何闭合门（通量守恒 Σ_p F_p = Σ_j x_j，docs/science/DRIZZLE.md:97 称之为\"严格不变量\"）",
 "值得立案：科学文档把这个量称为**严格不变量**（精确守恒），实现却用一个 1e-6 的相对容差放行——\"严格\"与\"1e-6\"之间缺一条 FP64 累加误差包络推导（对比：G-P1-WCS-BRIDGE 的 1e-8 px 就有逐项推导与适用域下限）。生产侧取值：v6_p1:187 与 v6_p2:149 用 1e-6（与代码默认同值），无第三方文档校准。出现处数 tot=14（lib 11/eng 3/docs 0）"],

# ── 8 gain ───────────────────────────────────────────────────────────────
["gain", "eng/tests/integration/v6_p1/v6_p1_integrate_test.cpp:99", "2.0",
 "e⁻/ADU", "探测器元数据（逐帧标量）", "不适用（激励值）", ">0（≤0 时诊断方差式返回 0）",
 "无（测试激励值）",
 "单位锚 docs/science/NOISE_MODEL.md:32/§44；数值 2.0 无出处（也不需要）",
 "不适用（结构性常数——测试激励）",
 "DetectorMetadata.gain（has_gain=true）",
 "机械层符号名 `gain` 过泛：本行是 md.gain 字段赋值。模式 `\\.gain = ` tot=16（lib 7/eng 8/实验 1）。同夹具的 read_noise=5.0/gain=2.0 组合使 snr_noise_gain_variance 可手算（见 BD-02 的 gain_variance 行）"],

# ── 9 photometric_scale_a ────────────────────────────────────────────────
["photometric_scale_a", "eng/tests/integration/v6_p3/p3_v6_export_e2e_test.cpp:168", "1.5",
 "无量纲（线性标度因子 α）", "x′=α·x ⇒ Var′=α²Var、ivar′=ivar/α²（逐帧）",
 "不适用（激励值）", ">0 才算 present；0/负 → 缺 a → REJECT",
 "无（测试激励值）",
 "语义锚：docs/science/PHOTOMETRY.md:13（标度类别 photo_scaled_adu 与 α 换算式，引 docs/standards/NUMERIC_STANDARD.md）；合同侧 fail-closed 锚：eng/contracts/schemas/product_family_field_constraints.schema.json:3999-4005（x-astrocs.fail_closed=\"缺光度尺度 a -> REJECT\"）＋ lib/algorithms/resample/p3_rsmp_failclosed.cpp:153 门 G-P3-PSF-05",
 "不适用（结构性常数——测试激励）",
 "Phase-3 导出输入的 α；生产默认 0.0（lib/phase3_session/p3_v6_export.h:139）⇒ 不声明即拒",
 "本行 1.5 是任意标度激励，且该键的 fail-closed 面完好（同文件 :491 m_psf_no_a 置 0.0 验必拒）。登记断链一处：**键名 photometric_scale_a 在 docs/** 零命中**，科学文档只用符号 α，schema 只用 a_ref ⇒ 按键名检索不到口径。出现处数 tot=9（lib 4/eng 5/docs 0）"],

# ── 10 scale ─────────────────────────────────────────────────────────────
["scale", "eng/tests/system/v6_runtime/v6_runtime_determinism_test.cpp:56", "表达式 (i%17)+1.0（机械层只记 1.0）",
 "无量纲（幅度分层倍数）", "合成向量逐元素", "不适用", "取值域 1..17",
 "无（结构性）", "无",
 "不适用（结构性常数——合成数据生成器）",
 "确定性回归的输入幅度打散",
 "机械层把表达式常量项单独提成\"现行值 1.0\"，丢了真正的结构项 `i % 17`（模数 17 与索引打散配对，1..17 共 17 档）。两者都无科学语义：这是为\"同序不同量级\"设计的合成激励，不进任何公式。出现处数（模式 `i % 17`）tot=2（lib 1/eng 1）"],

# ── 11 max_pt ────────────────────────────────────────────────────────────
["max_pt", "eng/tests/system/v6_runtime/v6_runtime_resource_record_test.cpp:106", "0.0",
 "无量纲（每线程 CPU 占用百分比的累加上确界）", "逐样本 max", "不适用", "非负",
 "无（结构性）", "无",
 "不适用（结构性常数——累加器初值）",
 "资源记录回归（per_thread_cpu_max_pct 等 summary 键）",
 "0.0 是 max 归约初值（同文件 :106 同行 max_pt/sum_pt）。真正的阈值常量在生产侧：eng/contracts/resource_gate_v1.json（经 CMakeLists.txt:135-154 生成 resource_gate_thresholds_generated.h），本行不承载它。出现处数 tot=8（lib 5/eng 3）"],

# ── 12 iterativeClipSigma ────────────────────────────────────────────────
["iterativeClipSigma", "eng/tests/unit/ipv_platform_binding_test.cpp:65", "5.0f（本行）；登记默认 9.0f",
 "σ 倍数（无量纲）", "逐候选结构图/迭代裁剪",
 "缺（无精度或收敛要求条目）", "名义 >0；实际生产零读取",
 "C（文档登记默认 9.0，无文献/实验锚）＋ 本行 5.0 无登记",
 "值 9.0 唯一源＝代码 lib/algorithms/star_detection/src/sdet_api.cpp:996 与生产调用 lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:1569；文档镜像 lib/algorithms/star_detection/README.md:119、docs/contracts/PUBLIC_API.md:833。值 5.0 无出处",
 "④待确认（并案：多侧默认不一致）",
 "SDetParams 结构图迭代裁剪（当前实现不消费）",
 "**三套取值并存**：9.0f（sdet_api.cpp:996 默认、orchestrator.cpp:1569、sdet_fp64_test.cpp:54、sdet_saturation_cursor_test.cpp:114、p1star_tests_core.cpp:552、实验/absolute-snr/code/b7_recon_driver.cpp:191）vs 5.0f（**生产入口两处** lib/infrastructure/scheduler/src/module_adapters.cpp:3959 与 :4846、p1star_guided_test.cpp:72、本行）。且 sdet src 内对该字段**只赋值不读取**（git grep 全仓 24 命中中无一处读取）⇒ 现网无行为差异，但 docs/contracts/PUBLIC_API.md:833 把\"生产实参\"记为 orchestrator 一侧，未记 scheduler 的 5.0f。已登记缺陷：artifacts/evidence/audit-2026-01/FIX_LEDGER.csv:209（M3b-C-05，P1，OPEN）\"SDetParams 五字段中四个全仓零读取而文档称有消费面；检测阈三套并存\"。出现处数 tot=23（lib 17/eng 2/docs 3/实验 1）"],

# ── 13 fwhmClipSigma ─────────────────────────────────────────────────────
["fwhmClipSigma", "eng/tests/unit/ipv_platform_binding_test.cpp:70", "3.0f",
 "σ 倍数（无量纲）", "逐星 FWHM 估计裁剪", "缺", "名义 >0；仅 debug 入口消费",
 "C（默认 3.0 只有文档/代码镜像，无文献或实验锚）",
 "lib/algorithms/star_detection/src/sdet_api.cpp:963-975（默认 3.0f）＋ README.md:119-123 ＋ docs/contracts/PUBLIC_API.md:833；消费面限定见 docs/algorithms/STAR_DETECTION_ALGORITHMS.md:252（fwhmClipSigma 仅 debug 入口消费，impl 阶段8 已移除全局）",
 "④待确认（与登记默认一致，但语义上为死参数）",
 "SDetParams.fwhmClipSigma",
 "本行值与登记默认相等（3.0f），故**不立案多侧不一致**；但 README:121-123 明确\"生产消费面只有 maxStars/maxAxisRatio 完整消费，fwhmClipSigma 仅 debug 入口\" ⇒ 该测试设置它不产生判据意义（假绿风险，属断言强度而非取值）。值 3.0 属\"惯例 σ 倍数\"，无实验标定与文献锚：与同族 iterativeClipSigma 9.0、noise 侧 cosmic_clip_sigma 5.0、background_clip_sigma 3.0 并存四套裁剪倍数，各自的适用域未在一份文档内对比登记。出现处数 tot=32（lib 25/eng 2/docs 4/实验 1）"],
]
emit(R)
