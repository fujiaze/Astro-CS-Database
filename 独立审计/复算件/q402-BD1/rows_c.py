# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, r"独立审计/复算件/q402-BD1")
from emit import emit

R = [
# ── 26 max_recip ─────────────────────────────────────────────────────────
["max_recip", "eng/tests/unit/p1001_real_nodes_test.cpp:3241", "0.0",
 "无量纲（|v·w−1| 的上确界，variance×ivar 互反性偏差）", "逐像素 max 归约", "不适用（累加器初值）", "非负",
 "无（结构性）", "无", "不适用（结构性常数——累加器初值）",
 "IVAR-002(a) 互反性判据",
 "本行 0.0 无科学语义（max 归约初值，:3246 逐像素取 max）。**但本用例真正的判据容差 5e-3（:3261 CHECK_MSG(max_recip < 5e-3)）机械层完全没有登记**——它值得登记：互反性 |var·ivar−1| 在 float32 表示下量级约 1e-7，5e-3 比之高约 4 个数量级 ⇒ 判别力弱（同文件 :3756-3759 对 1e-5 有逐项推导，此处 5e-3 无任何依据登记）。另 :3063 有同名第二处累加器（另一用例）。出现处数 tot=7（全在 eng/tests）"],

# ── 27 r_scaled ──────────────────────────────────────────────────────────
["r_scaled", "eng/tests/unit/p1001_real_nodes_test.cpp:3749", "−1.0（JSON 缺键哨兵）；被测值来自 mask_radius_p50",
 "像素", "逐帧掩膜半径 p50（α 标度面上）", "判据 rel≤1e-5（:3763，推导注释 :3756-3759）", "≥0 为有效；<0 = 未取到",
 "无（哨兵）；被测判据有推导注释",
 "半径硬上界 60 px 的唯一源链：docs/science/NOISE_MODEL.md:97/:368（rmax=max(1,r0)·max(1,scale)=60 px，MASK-002 导出链）→ docs/algorithms/NOISE_ESTIMATION.md:144/167 → eng/packaging/config/defaults.json:109-111（source_ref 三段登记）",
 "不适用（结构性常数——读取兜底哨兵）",
 "W3(b) 标度不变性（ADU 面 vs 光标面半径须不变）",
 "**立案（哨兵方向使判据可静默通过）**：:3749 取 -1.0 兜底后，:3752 的断言是 `CHECK(r_scaled < 60.0)`——缺键时 -1.0 < 60.0 **恒真**，该 red-side 判据在\"字段根本不存在\"时不红。同簇的 :3763 因要求 `r_scaled > 0.0` 而能红，所以整用例不致全盲，但**第一条门是空门**。同类形态：expect/observed 用负哨兵 + 只判上界。出现处数 tot=9（eng 9）"],

# ── 28 frac_scaled ───────────────────────────────────────────────────────
["frac_scaled", "eng/tests/unit/p1001_real_nodes_test.cpp:3750", "−1.0（JSON 缺键哨兵）",
 "无量纲（掩膜像素占比）", "逐帧 mask_frac", "缺（本队列内未见对它的判据）", "[0,1]；-1=未取到",
 "无（结构性）", "无", "不适用（结构性常数——读取兜底哨兵）",
 "同 W3(b) 的标度不变性对照量",
 "与 :3749 同形态（.value(key, -1.0) 兜底）；该符号在本用例里只被读、未见被断言（若确无断言则为死读，属\"看起来通过\"的空槽）。出现处数 tot=5（eng 5）"],

# ── 29 gain_e_per_adu ────────────────────────────────────────────────────
["gain_e_per_adu", "eng/tests/unit/p1001_real_nodes_test.cpp:91", "1.5",
 "e⁻/ADU", "StarField 夹具逐帧标量", "不适用（激励值）", ">0",
 "无（激励值）；单位与用途有 A/C 锚",
 "docs/science/NOISE_MODEL.md:32/:44（gain: e-/ADU）；本文件 :94-97 引 §9.41 负责人裁决要求\"真实物理噪声过程（电子域 Poisson + 读出 Gaussian + 增益量化到 ADU）\"，1.5/5.0 只是使该过程可算的任意正数",
 "不适用（结构性常数——测试激励）",
 "合成帧噪声生成（Poisson 在 e⁻ 域、量化回 ADU）",
 "取值链自洽（同一夹具 read_noise_e=5.0，:92），且**注释把\"为什么必须有噪声\"登记为裁决**，是本队列里少见的\"激励值带规范依据\"的正例。出现处数 tot=261（lib 42/eng 13/docs 8/实验 195）——跨面同名，非本常数副本数"],

# ── 30 scale_deg ─────────────────────────────────────────────────────────
["scale_deg", "eng/tests/unit/p1_hips_writer_test.cpp:153", "表达式 (180/π)·√(π/3)/512（机械层记作 180.0;3.0;512.0）",
 "度", "HiPS 叶像素角尺度（nside=512）", "%.6f 双侧字符串一致（同 oracle O4）", "nside>0",
 "A（一手标准）＋ 可导出",
 "IVOA REC-HIPS-1.0 §4.4.1（hips_pixel_scale 单位=度，见 :151-152 注释与 lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1827）；解析式=HEALPix 等面积像素立体角 ⇒ scale=(180/π)√(π/3)/nside",
 "②可由关系导出（代码即按式算，属正例）",
 "产品键 hips_pixel_scale 的期望值复算",
 "**证伪机械结论**：本行不是三个常数（180/3/512 是公式里的 π 与 nside），是**一个导出式的成分**。出现处数（模式 scale_deg）tot=211（lib 95/eng 77/docs 17/实验 22）——同名跨 WCS 比例、图幅尺度等多义，不可当副本数读"],

# ── 31 hips_pixel_scale ──────────────────────────────────────────────────
["hips_pixel_scale", "eng/tests/unit/p1_hips_writer_test.cpp:158", "字符串字面量 412.258369，出现在 **== npos**（必须不存在）的断言里",
 "角秒（该字面量的原口径，即被否证的错值）", "HiPS properties 键", "n/a（负例断言）", "n/a",
 "无（该值在此处是\"必须消失的旧缺陷指纹\"）",
 "412.258369 的正当出处＝nside=512 叶像素**角秒**尺度：3600·(180/π)·√(π/3)/512，登记于 lib/infrastructure/aio/include/aio_hips.h:93-96（用于 SCI-DRZ-001 的 1–2× 过采样帧尺度上界）与 docs/algorithms/HIPS_WRITER.md:222",
 "不适用（结构性常数——负例锚），并立案\"同数值双单位口径\"",
 "M2b-B-03 回归锁定（产品键单位）",
 "**证伪机械结论**：登记为\"现行值 412.258369\"是误读——该行是 `CHECK(props.find( 字符串 hips_pixel_scale=412.258369 ) == npos)`，即旧角秒口径缺陷的**反例指纹**；现行产品值为 0.114516 度。同族两面夹击已具备（lib/infrastructure/aio/tests/p1hips/p1hips_tests_oracle.cpp:584-598 正例复算＋负例 o4_pixel_scale_neg）。**顺带扫出的一处新问题（非本行）**：lib/infrastructure/hips_browser/healpix_browser_qt/tools/gen_geometry_truth.py:177 写死 `hips_pixel_scale=58.6`，而 58.6323°=nside=1 基面像元尺度（本机按 IVOA 式复算，命令见 md），同文件 :168 的 hips_order 却是 leaf_order=order+SHIFT（SHIFT=9，:35 ⇒ nside≥512 ⇒ 应约 0.114516°）⇒ 该真值文件的尺度与声明阶数差 ≥512×，且无单位注释；生产写出路径（aio_hips_writer.cpp:1890）不受影响"],

# ── 32 FX_SIGMA ──────────────────────────────────────────────────────────
["FX_SIGMA", "eng/tests/unit/p1_noise/adapter_test.cpp:261", "5.0",
 "ADU（噪声标准差，与 FX_BG=100.0 同域）", "逐像素高斯噪声夹具 σ", "不适用（激励值）", ">0",
 "无（测试激励值）", "无",
 "不适用（结构性常数——测试激励）",
 "noise 适配器 f32/f64 影子对拍的输入面",
 "同族：FX_H=96/FX_W=128（:260）为结构性维度；σ=5 与 docs/science/NOISE_MODEL.md:345 源污染 oracle 的\"空背景 N(0,5²)\"同值，说明该族夹具**对齐了科学文档的 oracle 标定条件**（可作跨面同值佐证，非硬约束）。出现处数 tot=2（全在 eng/tests）"],

# ── 33 gain_variance ─────────────────────────────────────────────────────
["gain_variance", "eng/tests/unit/p1_noise/adapter_test.cpp:710", "506.25",
 "ADU²（方差）", "诊断式输出（逐帧标量）", "bitwise/==（精确相等）", "≥0",
 "A（标准 CCD 噪声方程）＋ ②可导出",
 "公式锚：docs/science/NOISE_MODEL.md:32（§5c 加权方差面输入）与实现 lib/algorithms/noise_snr/cpp/src/noise_model.cpp（snr_noise_gain_variance = s/g + rn²/g²），同文件 :700 注释亦标 SNR-005",
 "②可由关系导出（本行正是按式复算的期望值）",
 "D10 gain_variance 数值语义抽查",
 "506.25 = 1000.0/2.0 + 5.0²/2.0² = 500 + 6.25（输入见 :682 的 cfg_gv），且在二进制浮点下**精确可表示** ⇒ 用 `==` 而非容差是成立的。这是本队列里\"钉值有完整导出链\"的最佳正例，可作为 D4 台账的模板。出现处数 tot=85（lib 39/eng 23/docs 17/实验 5）"],

# ── 34 sigma_true ────────────────────────────────────────────────────────
["sigma_true", "eng/tests/unit/p1_stars_test.cpp:209", "10.0",
 "ADU（背景噪声真值）", "合成图像逐像素（bg_true=1000.0 同域）", "判据：σ 相对偏差 ≤2%（case1 实测 +0.27%）、纯噪声面 ≤2%（实测 −0.54%）",
 ">0", "B（本仓可复现实验，seed 固定 :215）",
 "无文献锚，也无需——真值由夹具自设（10.0 是 chosen truth）；但**判据阈值 2%/6% 的登记在 :204-206 指向 run/CLEAN-401/third_sigma/**（run/ 不入库 ⇒ 证据不可复核）",
 "③需实验标定（已标定，但标定结果在仓库之外）",
 "第三 σ 估计器（裁剪后 RMS = StarCatalog::noise_sigma）回归锁定",
 "出现处数 tot=421（实验/** 388 为主）——同名跨多实验单元，不是本常数副本。登记可复核性缺口：该用例引用 run/ 下的实测件为正/负例依据，仓库内无 results 落档 ⇒ 按标准 02 §1「B 级须结果落档」不达标"],

# ── 35 sigma ─────────────────────────────────────────────────────────────
["sigma", "eng/tests/unit/p1_stars_test.cpp:227", "无（.6f 是 printf 格式串片段）",
 "n/a", "n/a", "n/a", "n/a",
 "无（机械抽取假阳性）", "无", "不适用（非数值常数——格式说明符）",
 "n/a",
 "证伪机械结论：:227 是一行 fprintf 诊断输出（格式串为 sigma=%.6f rel=%.6f），扫描器把格式说明符里的 6f / .6f 当成了数值，符号名 sigma 也取自该格式串。同类假阳性另见 BD-02 的 ZP_k 行与 BD-01 的 dark/gain/scale/limit/snr 行（值取自表达式或同行他项）。建议机械层屏蔽字符串字面量与格式说明符"],

# ── 36 kZPsyn ────────────────────────────────────────────────────────────
["kZPsyn", "eng/tests/unit/p1snr/p1snr_fref_baseline_test.cpp:115", "-14.269",
 "星等（mag，零点）", "逐帧零点 ZP_syn（Gaia XPSD 拟合同域）", "缺", "任意实数；本用例只用到其差值",
 "无（注释称取自 L4 t2_m1_red 拟合，但**该拟合值不在跟踪集内**）",
 "注释 :112（\"取自 L4 t2_m1_red 的 Gaia XPSD 拟合量级\"）；同族 kK1=6.272203e-17/kK2=5.685037e-17（:113-114）同源，另见 eng/tests/unit/p1001_real_nodes_test.cpp:4649 以\"旧断言：帧间 k 散度 0.107 mag（6.272203e-17 vs 5.685037e-17）\"引用同一对数值",
 "④待确认",
 "合成零点/恒等式回归 N2",
 "**按\"抄观测值\"追查后判定为低风险但须登记**：全仓 `git grep -F 14.269` 只有本行这一条真命中（另一条是无关 CSV 数字），即唯一数值源是**口头注释**而非落档件；`t2_m1_red` 只出现在 eng/tests/validation/release02/phot_verify/debug*.py 的帧名里，无 -14.269 结果件。缓解：本用例是自证恒等式——:171 把同一个 kZPsyn 写进合成输入，:269-270 再断言乘积等于它，故绝对值取错不会漏判真实缺陷（也不会证真）；文件头 :23 已自称\"恒等式\"。⇒ 不构成 142 s 式\"阈值抄观测值\"，属\"观测抄录值无锚且无落档\"。出现处数 tot=3（eng 3）"],

# ── 37 zpk ───────────────────────────────────────────────────────────────
["zpk", "eng/tests/unit/p1snr/p1snr_fref_baseline_test.cpp:264", "0.0（JSON 缺键兜底）",
 "星等（mag）", "读取产品字段 frame_zero_point_mag", "判据 |zpk−expect_zp|<1e-9（:270）", "有限实数",
 "无（结构性兜底）", "无", "不适用（结构性常数——读取兜底哨兵）",
 "N2 恒等式断言的观测侧",
 "本行兜底 0.0 与 :270 的 1e-9 绝对容差配套 ⇒ 缺键时 zpk=0.0 ≠ expect_zp（expect_zp≈-14.269+...）必红，**方向正确**（与 r_scaled 的负哨兵+上界恒真形成对照）。出现处数（\\bzpk\\b）tot=3（eng 2/实验 1）"],
]
emit(R)
