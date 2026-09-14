# DESIGN · check_numeric_constants（S-4① 设计稿，非落地脚本）

- 轴：V12 延伸（父代理 session-9db01ebd 批准"只写设计稿，不改 tools/ 与生产面"）。
- 本文件性质：规格书。本层不创建可执行脚本、不改 ci/checks.json、不动 findings/ 与 账本/。
- 时点与漂移声明：本稿两次采样间隔约 1.5 小时，HEAD 从 521095b 前进到 d2493d55，gaia_client.c 的 #define 段整体下移约 14 行（QUERY_CACHE_CAPACITY 由 :113 → :127）。⇒ 本稿一律按**符号**定位，禁以行号为锚（也是 §3/§4 的设计约束来源之一）。
- 数值口径：python3 复算，FP64，时点 2026-09-15；引用值见 V12.md §1 常数权威表 18 行。

## 1. 判据形式化定义

1.1 登记面（唯一语义源，需 S-1 先建 docs/algorithms/NUMERIC_CONSTANTS.md），行式（markdown 表，一值一行）：
    | id | symbol | value | digits | display | unit | domain | derivation | cmp | boundary | kind | disp |
  id 形如 NC-MAD-TO-SIGMA / NC-TRIM-TO-SIGMA / NC-MOFFAT4-FWHM / NC-GATE-CPU-MEAN / NC-GAIA-CAP-PER-FILE；kind ∈ {science, gate, format, synthetic}；disp 为 DISP-* 号或 -。
1.2 "同语义常数族"的机器定义 = **同一 id 的全部宿主**；不做数值猜测聚类。族的判定分三层：
    - 显式族：宿主字面量出现在某 id 的 sites 声明范围内（由 1.4 的召回正则给出候选，再由 id 绑定确认）。
    - 影子族（本门主要产出）：被任一召回正则命中，但**无任何 id 绑定** ⇒ 判 NO_AUTHORITY（该常数无权威承载点）。
    - 共源族：宿主是 oracle/参考实现且与生产取同一字面 ⇒ 判 CO_SOURCE（不能当"正向钉死"采信）。
1.3 两侧数据源：
    - 登记侧：NUMERIC_CONSTANTS.md 表格行（value/digits/display/unit/domain/derivation）。
    - 生产侧：include/astrocs/sci_constants.h 的具名 constexpr（S-1 目标态）；**过渡态**允许"权威候选清单"（本仓现有具名点，按符号列死）：lib/phase1/noise/noise_model.h::kMadToSigma、lib/snr_estimator/cpp/src/noise_model.cpp::kTrimMeanToSigma 与 kLn10、snr_science.cpp::kTrimMeanToSigma/kMoffat4FwhmFactor/kLn10/kPi、lib/dynamic_psf/src/dpsf_psf.cpp::MOFFAT4_FWHM_FACTOR、lib/photometric_calib/cpp/src/star_matcher.cpp::_MAD_SCALE、cli/resource_gate.h::k*（:96-111/:184）、cli/memory_report.h::k*（:47-57）、lib/gaia_xpsd_client/src/gaia_client.c::WL_COUNT/MAX_STARS_RESULT/QUERY_CACHE_CAPACITY/QUERY_CACHE_MAX_BYTES/BLOCK_CACHE_*/AE_CAP_BOUNDARY_DEG/AE_C45_FACTOR/AE_GLOBAL_FACTOR、lib/plate_solve/cpp/ipv/include/ipv_types.h::m_lim_*。
1.4 两级判定（关键）：先把字面量**归一为数值再比**，字符串位数只作展示层判据。
    - VALUE_MISMATCH（红）：|v_host − v_reg| / |v_reg| > 声明容差（默认 0 ulp；FP32 站点按 §4.2 折算后比）。
    - PRECISION_PRESENTATION（黄，不算门红）：数值 0 ulp 相同但 digits 列与宿主位数不符。
      真实例：kLn10 在 noise_model.cpp:34 写 30 位、snr_science.cpp:33 写 19 位，归一后同为 2.302585092994046（0 ulp）⇒ 只黄不红；
      而 1.4826 与 1.482602218505602 相对差 -1.496359e-06 ⇒ 红。**位数不同不等于值不同，门必须能区分这两件事。**
1.5 为什么必须逐值比对、禁止子串在场判据（引 V12-N-02 为反例）：
    - 恒真面：'1.4826022185' 是 '1.482602218505602' 的前缀子串 ⇒ "文档含该串"永不为假 ⇒ 门永不红（在册例：tools/docs_machine_consistency.py:139-144 的 snr_constants）。
    - 反向锁死面：'0.7316728' 不是 '0.7316727929211932' 的子串（实测 False）⇒ 一旦按 S-1 把 docs/science/PSF.md 订正到全精度，旧判据反而变红 ⇒ **子串判据会把截断串固化为过门必要条件**，即"把旧错误口径当权威钉住"。
    - 结论：判据必须是 (id → 归一数值 → 逐宿主集合比对)，输出宿主清单；任何 in/正则"是否出现"都不得作为通过条件。derivation 列写可执行表达式（如 1/NormalDist().inv_cdf(0.75)、2*sqrt(2*(2**0.25-1))、pi/(2*sqrt(2))），门自行复算并允许 0 ulp；不中即红（本轴 N-04 型：在册 16 位 trimmed 值对解析值 rel -4.132439e-07，第 7 位起就错，属"值错"而非"截断"）。

## 2. 族扫描正则（召回层；命中即要求 id 绑定，未绑定=NO_AUTHORITY）

口径：Python raw 字符串；作用于 .c/.h/.cpp/.hpp/.py 与 docs/**.md；排除 archive/、third_party/、docs/archive/、*.log、evidence/*.csv。
下表"族"列即建议 id。正则只做候选召回，判定靠 1.4/1.5。

| 族（建议 id） | 召回正则 | 已知宿主（符号） | 当前值一致性 |
|---|---|---|---|
| NC-MAD-TO-SIGMA | 1[.]4826[0-9]* | noise_model.h::kMadToSigma；snr noise_model.cpp:61；sdet_api.cpp:227；sdet_image.cpp:495/524；photometer.cpp::sky_sigma；star_detector.cpp:51；baseline_provider.cpp:320/335；baseline_kernels_impl.inc:129/137；frame_qc_grid.py | 红（15 位 与 4 位 rel -1.496e-06 并存，FP32 -1.514e-06） |
| NC-MAD-INV | 0[.]6745[0-9]* | 0[.]6744[0-9]* | star_matcher.cpp::_MAD_SCALE；photometric_calib.h 四处注释；p1phot_oracle.hpp:68 | 红（1/0.6745 rel -1.519615e-05；Φ⁻¹(3/4)=0.6744897501960817） |
| NC-TRIM-TO-SIGMA | 0[.]731672[0-9]* | 0[.]731673(?![0-9]) | noise_model.cpp::kTrimMeanToSigma；snr_science.cpp::kTrimMeanToSigma；snr_estimator.cpp:74/99；PSF.md；STAR_PSF_ALGORITHMS.md:56-57 | 红（16 位对解析 0.731673095280613 rel -4.13e-07；同文三套位数） |
| NC-MOFFAT4-FWHM | 1[.]23031[0-9]* | 1[.]230307[0-9]* | snr_science.cpp::kMoffat4FwhmFactor；dpsf_psf.cpp::MOFFAT4_FWHM_FACTOR；snr_estimator.cpp:63/66/97 | 红（对解析 1.2303076525901024 rel +1.907986e-06；且同值兼作 fwhm 兜底） |
| NC-LN10 | 2[.]302585[0-9]* | snr noise_model.cpp:34；snr_science.cpp:33 | 黄（位数不同，归一 0 ulp） |
| NC-PI / NC-DEG2RAD / NC-AE-C45 | 3[.]14159265[0-9]* | 1[.]5707963267948966 | 1[.]1107207345395915 | 180[.]0 / M_PI | gaia_client.c::DEG2RAD/RAD2DEG/AE_GLOBAL_FACTOR/AE_C45_FACTOR；snr_science.cpp::kPi | 绿（π/(2√2) 与 π/2 逐位一致，本轴唯一全绿族） |
| NC-MAG-DECODE | [ * ]0[.]001 | -[ * ]1[.]5 | +[ * ]24(?![0-9.]) | gaia_client.c 五处 mag_raw*0.001-1.5（G/BP/RP） | 红（三常数内联 5 次，无具名点；口径仅 docs/algorithms/GAIA_QUERY.md 公式行） |
| NC-DEX-to-MAG | 2[.]5 \* sigma | 2[.]5 \* | snr noise_model.cpp:286/295；PHOTOMETRY 侧 | 绿（定义常数 2.5，但无 id 绑定 ⇒ NO_AUTHORITY 类） |
| NC-MAG-LIM-EXPOSURE | \b6\.0\b.*log10 | 1\.5 \* std::log10 | 2\.0 \* std::log10 | \b13\.0\b | -4\.0 | ipv_select.cpp::compute_initial_mag_cut 与 :408-409 内联式；:394 m0_hi；ipv_types.h::m_lim_m0_offset | 红（两份实现差 4.000 mag；13.0 无承载） |
| NC-MAG-ITER | 0\.2885 | m_lim_(safety|max_iter|zero_step|alpha_(prior|min|max)) | -6\.0.*6\.0 | ipv_types.h 默认 与 ipv_select.cpp:395-400/492 兜底 | 红（默认↔兜底双写无锁；±6.0 全无承载） |
| NC-GATE-CPU-* | 85\.0 | 0\.85 | 60\.0 | 0\.60 | 90\.0 | 0\.70 | \b10\.0\b | kWorkerP50Min | cli/resource_gate.h:96-111/184；tools/monitoring/run_monitored.py:451-456 | 红（跨语言手抄；90.0/0.70 无冻结文本；10.0 开闭相反） |
| NC-GATE-MEM-* | 32\.0 | 32ull \* 1024 | kAlloc[A-Za-z]* | 0\.5 | cli/memory_report.h:47-57；cli/resource_gate.h:164 | 红（同数字三义、比较号 >/>= 不一致、覆盖通道只作用一条） |
| NC-GAIA-CAP-PER-FILE | \b200000\b | MAX_STARS_RESULT | m_lim_gaia_cap_per_file | 64LL \* 200000LL | gaia_client.c；ipv_types.h:244；module_entry.c::kQueryCacheCap；GAIA_QUERY.md:147/222；PUBLIC_API.md:93 | 红（1 具名(.c 私有)+3 复制；派生乘积两 TU 各算） |
| NC-XPSD-WLGRID | \b343\b | WL_COUNT | \b336\b | \b1020\b | spectrumStep= | wl_step | gaia_client.c::WL_COUNT/STAR_STRIDE_SP/parse；fixture_gen；xpsd_spectrum_count_bounds_test；gaia_cat_test；DR3SP_SCHEMA_AUDIT.md:29；spectrum_integrator.cpp::wl_step | 红（真库 step=2 与 fixture step=1 且被 CHECK 钉死） |
| NC-BBOX-MARGIN / NC-MAG-PRUNE-MARGIN / NC-COS-GUARD | \* 1\.2 \b | radius_deg \* 1\.2 | \+ 0\.25 | cos_dec < 0\.01 | 1e-15 | gaia_client.c:923/922/2054/897 | 红（裕量无适用域登记；无大半径回退守卫） |
| NC-UPM-GRID / NC-PATH-BUF | \bgrid\b.*= *8 | != *8 | \b512\b | \+ *511 | ACS_FIO_PATH_MAX | upm.h/upm.cpp:245/414、sampler.cpp:1449；gaia_client.c 两处 +511 | 红（4 处字面量 0 具名；512-1 手写两处而 ACS_FIO_PATH_MAX 在册不用） |
| NC-CACHE-SIZES | \b8192\b | QUERY_CACHE_CAPACITY | \* 64LL | 4ULL \* 1024 \* 1024 \* 1024 | 60(?!\d) \*\* /TTL | gaia_client.c::BLOCK_CACHE_CAPACITY/QUERY_CACHE_CAPACITY/TTL/MAX_MEMORY/MEMORY_PRESSURE_THRESHOLD | 黄（4GB 同值两义：块预算 与 内存压力阈值） |
| NC-EPS-FLOORS | 1e-12 | 1e-9 | 1e-6(?![0-9]) | 1e-3(?![0-9]) | 1e-15 | noise_model.cpp:353/411（floor 与回退各写）；ipv_types.h:229 | 黄 |
| NC-NTARGET | n_target_cap | std::max\(50 | \b60\b 附近注释"统一为 60" | ipv_select.cpp:317-318；ipv_types.h::img_n_target；PLATESOLVE.md §5:115 | 红（[50,60] 与两套文本口径互斥） |

不扫为常数的数字（避免误报）：数组下标/位移/位宽（&3、>>5、*8 sizeof）、纯结构常数（row[9]、40 字节头）、时间戳与 seed（SplitMix64 常量、0x5EED…）、日志格式串中的数字、evidence CSV/日志。

## 3. 退出码与 profile / waivable 建议

- 退出码：0=全绿；1=VALUE_MISMATCH 或 复算与 derivation 不符（红）；2=NO_AUTHORITY（影子族，红）；3=BAD_REGISTRATION（登记面自身不可解析 / 同 id 多值 / unit 缺失，fail-fast 红）；4=OBSERVE_ONLY（§5 阶段 0 的退出，红项非零但以 4 结束，CI 视作绿并落证据）。共源(CO_SOURCE)与展示精度(PRECISION)只进报告字段，不占退出码。
- 检查项建议：id CON-NUMERIC-CONSTANTS；command ["python3","-B","tools/quality/check_numeric_constants.py","--check"]；profile 三档同 AGENTS-GOV（core/docs/full 都跑，本门不依赖构建产物）；timeout 120s；waivable=**false**。
  理由：本门是 S-1 的唯一执法面，一旦可豁免即回到"权威点缺位 + 无据 VERIFIED"；观察期一律用退出码 4 表达，不用 waiver（宪章纪律 R-05/R-13：严禁用 waiver 掩盖红灯）。
- 为什么必须进 ci/checks.json（而不是像 docs_machine_consistency.py 那样未接线）：
  ① 该工具实测不在 checks.json 的 126 项内（本轴 json.load 复核），于是 ACTIVE 文档里"PASS 9/9"成为无据证据（在册 M6b-G-003），门等于不存在；
  ② 它无 --check 只读模式、无条件覆盖写 reports/...（在册 ci/INVENTORY_REPORT.md:73 记其"未入 registry"的原因）⇒ 未接线 + 有副作用 互相锁死，永远进不了 CI；
  ③ 本门设计为默认零副作用（不写文件，除非显式 --out 且路径限 run/），且自证三件套：--selftest（注入 §5 坏例集，必须逐条红）、打印 自身输入 SHA + 命令 + UTC 时间戳、机器可读 jsonl 证据。这样"本版实测 PASS"才第一次可核对。

## 4. 误报面与抑制策略

- 合法截断展示：只允许登记面的 display 列表达，且必须满足 (|display − value|/|value| ≤ 该行声明容差) 且 同一 id 在单个文档里最多一种 display。命中即绿；未在 display 列声明而文档写截断串 ⇒ 红。**不给 waiver 口子**（豁免会掩盖红灯；截断展示属"事实"，不是"例外"）。同时删掉任何"文档必须含某截断串"型判据（N-02）。
- FP32 后缀 f：宿主归一为 (value, precision) 二元组；precision=f32 时比较 float32(host) 对 float32(value)（允许 1 ulp），并额外要求 digits ≤ 7（float32 十进制有效位），否则报 PRECISION"伪精度"。真实两例：1.4826f=1.4825999736785889 对 15 位值 rel -1.514e-06（红）；1.482602218505602f=1.4826022386550903 对真值 rel +1.36e-08（绿）。
- 注释中的举例值：默认**扫全部文本区但分级** —— 代码区命中计入判定；注释/markdown 命中只与同 id 的 value·display 比对，不符记 DOC_DRIFT（进报告不占退出码），避免"注释里写 0.7316728 而代码 16 位"这类既有事实变成红噪声。诊断码名与实现值不符（N-08(b) UtilizationP75Low 对 0.70）由登记面 name_semantics 列单列检查，属另一条判据。
- 共源抑制：oracle/参考实现取与生产同一字面 ⇒ 报 CO_SOURCE 并要求该 id 的 derivation 为可执行表达式，由门复算取代"oracle 钉值"（本轴实例：p1phot_oracle.hpp:68 用 0.6745、noise_model_science_test.cpp:363 用 16 位串、p1psf_oracle.hpp 用 1.230310）。
- 合成/fixture 值：允许偏离真库但须以 kind=synthetic 登记（如 fixture spectrumStep=1、测试星等窗 10.0,20.0、test_mag_iter 的 cap_value=200000.0），并要求其 id 与真库 id 同行注明 reference_id；未登记的红。
- 禁止行号锚：抑制表按 文件+符号 定位（本轴实测同文件 1.5 小时内漂移 14 行）。

## 5. 接入顺序（先只报不改）

- 阶段 0（只报）：S-1 先落登记面与 sci_constants.h；门以 --observe-only（退出码 4）跑一轮，输出"族 × 宿主"红/绿矩阵与 jsonl 证据，交前台入 findings（本层不写 findings/）。按 V12.md §1 十八行预判：红 ≥13 族（权威点数=0 的 6 行 + ≥2 独立复制的 7 行 + 值错 3 行），绿 1 族（π 族），黄 3 族（kLn10、4GB 两义、eps 族）。
- 阶段 1（清红）：按族补权威点与登记面，禁止逐文件改位数（A-12 型局部修复是本轮缺陷的成因）。每族收口条件：NO_AUTHORITY 归零、VALUE_MISMATCH 归零。
- 阶段 2（升门）：进 ci/checks.json，waivable=false，非豁免门，三 profile；同时把本门的红项与 40_OWNER_DECISIONS.md 的 A-12/A-39/A-42/A-43 逐条对齐（凡"待裁"的位数/取义争议，S-1 接受后自动判）。
- 阶段 3（拆旧门）：删除或改写 docs_machine_consistency.py 的 snr_constants 判据（改为调用本门子命令），消除 N-02 的"永不红 + 反向锁死"两面。
- 每阶段验收：--selftest 坏例集必须三类各红 —— ①位数不一致型（把 1.4826 注进带 15 位 id 的文件）；②倒数式型（0.6745 与 1.4826… 同族并存）；③伪精度型（16 位值对 derivation 复算差 4.13e-07）。任一不红即门自身失效（红退出码 5=SELFTEST_FAILED）。

## 6. 与 S-4②③ 的复用关系

- 同一解析器：一个 extractor 输出统一记录 (id,name,value_raw,digits,precision,unit,domain,site(file,symbol,kind),decl|use|comment)。C/C++ 走 constexpr/#define/字面量三类抽取；Python 走 ast 顶层赋值；markdown 走表格行解析。三条门只消费这份记录集的不同 filter。
- 同一登记面：NUMERIC_CONSTANTS.md 一张表三用 —— S-4① 取 value/digits/derivation；S-4② 取 disp/锚点列（校验注释与文档里出现的 docs/**、contracts/**、DISP-* 是否存在，直接灭 N-06 与 M6b-E-002 面）；S-4③ 取 kind=gate 行的 cmp/boundary/effective_if 列，比对 C++ 与 Python 两侧的判据集合与开闭（灭 N-08(a)(c)、N-09）。
- 共用坏例集与共用退出码语义；共用 --observe-only 接入节奏（同 §5 阶段 0→2）。
- 不复用的部分：S-4② 不需要值归一（纯存在性），S-4③ 不需要 derivation 复算；但三者共享"行号禁用、按符号定位"与"零副作用 + 自证三件套"两条纪律。
