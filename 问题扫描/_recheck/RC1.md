# RC1 复核档案 · recheck_round1 verify[0::8]（62 条）

- 时点：HEAD `a3a343a4`（工作树另含未入库 WIP：`lib/snr_estimator/{src,include,CMakeLists.txt}`、`lib/phase1/tests/`、`tests/unit/p1_noise/`、`lib/plate_solve/cpp/ipv/test/ipv_dead_params_lock.py`、脏改 `docs/TRACEABILITY.csv` 等——涉及时单独注明）
- 方法：一律按 文件::符号 重新定位（glob/grep/read + 只读 python3 统计 + git --no-optional-locks 只读），复算原缺陷机制是否消失；缺失判定走三级复核（①工作树 ②HEAD git ls-files/show ③改名/移动检索）
- 分片：verify[0::8]（i=0），共 62 条；不触碰他人区间
- 四态：STILL（缺陷仍在，给新锚）/ FIXED（给修在哪+是否修全）/ MOVED（位置变缺陷原样）/ CANNOT_STATIC（需运行期）
- 本档只输出建议标记，不回写账本（账本由前台统一处理）

## 结论总表（收工时回填）

| # | ID | 四态 | 新锚（文件::符号） | 一句话判据 |
|---|----|------|--------------------|------------|
| 1 | FD-F-001 | STILL | tests/unit/cpu_provider_test.cpp::check_shared :62-78；注册 CMakeLists :218/:223 | 机制未变——fopen 后端源文件读 4096B 前缀、substring 查 include 文件名，仍测文本不测 kKernels 表内容/行为； |
| 2 | L28c-E-002 | STILL | lib/photometric_calib/cpp/src/spectrum_integrator.h::文件头 :3（契约锚行）与 :7（参考行），报告锚 :3/:7 无漂移 | ① :7 参考: lib/photometric_calib/spectrum_integrator/python/synthetic_photome |
| 3 | M1a-A-005 | STILL | PLATESOLVE.md :83「尺度容差 0.002 (各向异性 0.2%)」与 :187「CD 元素相对误差 ≤2%（§9 尺度容差 0.002 同源）」行号零漂移；ipv_types | 0.2%↔2% 十倍挪用与「同源」错误声明原样；尺度窗四套并存（s_min/s_max=0.90/1.10 ±10% 注释、ransac 头注 [0. |
| 4 | M1a-C-003 | STILL | 0-based 喂点环在 module_adapters.cpp::p1_op_wcs 显式分支 :2148-2151 与求解分支 :2384-2387（报告锚 :1785-1787 → 漂 | ① 环仍把 0-based 数组下标喂进 1-based 契约（wcs_tan.h:11 与参考解头注 :342「像素输入为 FITS 1-based |
| 5 | M1a-D-003 | STILL | 废止注释现 :23-28（p3_resample.h::p3_resample_check_mode 头注）；PHASE3_RSMP_IMPL.md :24「非目标: 不实现 varianc | 四类实例全数健在；唯一减项：p3_wcs.h :43-44「默认实参、既有调用零改动」现与代码一致（:47 确有默认参数），该子例不再独立成立，但整条 |
| 6 | M1a-G-001 | STILL | 宪章 §4.3 义务行现 :125（原样）；FITS 键面 p3_output.cpp :185-186 原样无 RADESYS/EQUINOX；p3_resampled.json 键集 : | §4.3「坐标 frame + 像素/采样语义」两义务中——FITS 产品头两键全缺、p3_resampled/inspect 两面无 frame、* |
| 7 | M2a-A-2 | STILL | DRIZZLE.md :27「px²（球面立体角等价）」原样；:48-51 累加式原样；DATA_SEMANTICS.md :262 sumArea=**sr**、:263 variance | px²↔sr 混用（SCI 说等价、DATA 说 sr、writer 无单位键）、variance 公式维度不符、HiPS tile 无 BUNIT  |
| 8 | M2a-C-11 | STILL | pending_aio_channels 仍 :81-94 零漂移（writer_int32_tile.gap 写「flags 位 32/64 未定义」、properties_key_cha | 合同 JSON 与已落地能力直接矛盾原样；lib/astro_image_io/memory.md:290 仍写 pending；建议的 pendin |
| 9 | M2a-C-6 | STILL | json_config.h:67=0.8 vs module_adapters:2968/module_entry:355=1.0；杜撰锚 API-DRZ-001 无默认条款 |
| 10 | M2a-E-1 | STILL | DRIZZLE_GEOMETRY :113 仍引 api.cpp:191（无此文件）；DATA_SEMANTICS :245 仍引 api.cpp:486-503；:261 与 DRIZZL | ≥9 类引用对象零承载，机制原样；追溯门仍拦不住（M6b-E-001 同族）。 |
| 11 | M2a-G-1 | STILL | ci/checks.json 仍无该项；docs/standards/STANDARDS_REGISTRY.md §5(:292-329) 与 :8 仍自陈「机器检查: check_stan | 「检查器存在但未登记进 ci/checks.json」原样成立；D.catalog/D.fits 失真 CONFORMANT 仍无门可拦。 |
| 12 | M2a-I-2 | STILL | module.yaml :57 determinism: fixed_reduction_order；GAIA_QUERY.md :167 第 5 条「输出行序依赖目录枚举顺序（跨平台不稳定 | module.yaml 声明与 ALG 自述的枚举序依赖仍互斥；未加排序、未改声明、module.yaml 与 ALG 均未订正。 |
| 13 | M2b-B-05 | STILL | aio_hips_writer.cpp::finalize_snr_product VOTABLE 头 fprintf 常量串现 :1249-1251，xmlns="http:// www. | URI 内空格未去 → 产出仍非合规 VOTable、根命名空间解析必败；建议的可解析性测试/XML 断言未加（无任何 VOTABLE 解析测试面） |
| 14 | M2b-F-01 | STILL | D.healpix COMPLIANCE: CONFORMANT :127、DEVIATION 无 :129、§5.3 行「往返 ≤1e-12 deg；astropy-healpix 百万点 | 百万点独立 oracle 在仓内仍不可复现（数据缺位+生成器缺位+未注册）；现存判据口径（1.2×像素≈1e-3 deg 级）与声明 1e-12 de |
| 15 | M3-A-002 | STILL | SCI-CW §4 代码块现 :65-71，:66 仍标「像素级 SNR 权重（stage2 排异/积分，weight_mode=2）」、:71 仍写 weights[s] = suppor | 三腿全存——①CW 把 support×snr² 归给 weight_mode=2 而实现 mode2=ivar；②SCI-UPM:54 与 SCI- |
| 16 | M3-C-004 | STILL | CALIBRATION_PROCESS.md 仍无文件级历史/非规范标记（§2 :52-62 K 初值 t_light/t_dark + 「残差最小化（背景区域 MAD 最小）搜索最优 K」 | 文件自身无标记 + 三处叙述与冻结 SCI/现行实现互斥原样；README:147 的「历史流程参考」不构成文件级豁免。 |
| 17 | M3-C-012 | STILL | 三处形参静默丢弃现 :181/:566/:827（报告复核时点 :172/:544/:800 → +9/+22/+27 漂移）；自适应锥 {12,13,14,15,16} :254 与 n_ | 静默丢弃机制与无出处常数锥原样；DISP 登记在报告时即存在（POS 引用它），非修复证据；头文件合同面未订正。 |
| 18 | M3-I-001 | STILL | COSMETIC_ALGORITHMS.md :9 仍写「落码由 P1-COS-IMPL 执行，尚未存在生产符号」；lib/cosmetic/README.md :9-10「当前本目录仅合同 | 「已入库被消费的代码被写成未编译/无符号/无源码」整族现状断言仍过期；无一处订正。 |
| 19 | M3b-A-05 | STILL | 候选集四元组原样（dpsf :411；报告复核时点消歧 :389-401 → 现 :405-42x，+~16 漂移）；PSF.md「旋转简并不变量」现 :65（原 §7:63-65 区间内） | {θ,π/2−θ,π/2+θ,π−θ} 非 Moffat4 二次型对称群（90−θ 是镜面反射，未同步交换 sx/sy 时 M(θ)≠M(90−θ)） |
| 20 | M3b-D-01 | STILL | 残句「// FWHM 上限完全」现 sdet_api.cpp:230；修改流水「修复: RMSE 系数从 mad*3.0 改为 mad*1.4826」:207；dynamic_psf.h : | 注释卫生簇实例逐条仍在（断句残句×3、修改流水×1）；公共头单位契约欠账（star_detector.h :17-67 面）未见补写。 |
| 21 | M3b-I-01 | STILL | ① **不可达守卫实例已修**：SDET-ANGLE-001（P11）以 normalize_angle_deg_bounded 替换 while 循环+死 10000 守卫（sdet_ap | 整条为多实例簇——θ 守卫一站已 FIXED（SDET-ANGLE-001，含逐位一致回归语义），其余实例（行自引/字段数/常数截断）原样 ⇒ 整条判 |
| 22 | M4-C-06 | STILL | SCI 判据行 :34/:61/:87 零漂移；实现 gather 资格层现 :1183-1207 对非有限 value ++invalid_finite; ok=false **逐样本静默 | SCI「INVALID_INPUT hard fail」vs 实现剔除/不检查三腿原样；oracle 对比脚本 rejection_oracle_co |
| 23 | M4-F-05 | STILL | 判据段现 :217-219 if (!(k_corr >= 0.98 && k_corr <= 2.0)) 原样（复核时点 :217-219 零漂移）；头注释 :16「UPMW-005 断言 | 「MC 硬门」仍为 [0.98,2.0] 宽松区间（k_corr 漂移 1.0→1.99 皆绿），头注自述与代码相反，且不锚定冻结值；另 contro |
| 24 | M5a-C-004 | STILL | run_provider_avx2_checks.py :10（dlopen baseline.so+avx2.so 互比）、:15（对照容差 ≤2e-4）、:54 TOL = 2e-4、: | 「与 Python 参考比对」仍只是援引性声明——核验链是 avx2↔baseline 两生产 provider 互比，Python 侧无比对生产交付 |
| 25 | M5a-G-006 | STILL | execution_options.h :1「全局执行预算唯一来源」+ :16/:24 hardware_concurrency 自取（零漂移）；calibrator.cpp 裸 omp 五 | 第二预算源两机制全存——①execution_options 默认路径直接硬件自取（不做 affinity/cgroup 折算），与 ARCH:8 义 |
| 26 | M5b-C-04 | STILL | 合同面现 lifecycle_v1.h :45/:47（execute 互斥）、:71（double destroy→ACS_ERR_STATE 类违例）、:154（destroy 再调=检 | ①double-destroy「忽略」而非合同要求的报错（destroy 签名返回 void，错误根本不可表达）原样；②检测读已释放内存原样——第二次 |
| 27 | M5b-E-05 | STILL | 宪章零字母节（两向 grep 复核）；字母节冒充宪章现集中于 docs/owner/RELEASE_STATUS.md:11「宪章 §1.2/§H」、SCIENCE_OVERVIEW.md: | 治理引用层机制原样——ARCHIVED 文件字母条款号写作「宪章」且宪章无该节。子面订正：AGENTS.md 映射表现全为数字条款且与 check_a |
| 28 | M5b-G-07 | STILL | 宪章 §12.1 L0 清单现 :410-421，docs/owner/PHASE_OVERVIEW.md 在 :416（点名字面零漂移）；全仓该文件不存在；ci/checks.json D | 门验错目录（docs/review ≠ 宪章指定 docs/owner）+ 点名文件缺失，两腿均原样；checks.json 在 changed 清单 |
| 29 | M5b-G-15 | STILL | 承诺行现 CLI_PROTOCOL_V1.md :47「机器化一致性检查器合同(04 §6,API-002 建立 tools/check_cli_protocol.py)」——该脚本两向复核 | §6 承诺的检查器仍不存在，现役门仍是「源文件里有没有字面量」级判断，drizzle 类转发无规则表覆盖性核验。 |
| 30 | M5b-I-06 | STILL | 三份 audit 表仍 ACTIVE_INFORMATIVE（:238-245），notes 现加「V19R2 …历史快照；权威分类见本 DOCUMENT_INDEX.yaml」；T012. | 登记状态未改（仍 ACTIVE_INFORMATIVE 而非 ARCHIVED/GENERATED）+ 内容与树不符原样；notes 的历史快照自述是 |
| 31 | M6a-D-002 | STILL | calibration 文件头现 :14-15 仍写「acquire 失败 → 单线程降级 (drizzle 先例, 非致命)」，同文件 :930-931「acquire 失败/execut | 同一 host_lease 合同在四个适配器上仍是三种互斥语义（降级非致命/硬拒致命/硬占坑禁借口），且「CAL 先例」被两族各取所需地互指；无一处注 |
| 32 | M6a-D-010 | STILL | ac_calibrate_frame 注释块 :69-78 有形状无单位/生命周期/线程安全（:79 定义）；photometry_apply.h 返回码表 :27-31 仍缺实现 :51  | 被点名的三实例（返回码表缺 -5、ac_calibrate_frame 无单位、star_detector.h 零单位契约）逐条复算仍在 ⇒ STIL |
| 33 | M6a-I-001 | STILL | PUBLIC_API.md :1069 与 :2100、DATA_SEMANTICS.md :856 与 :2095 均仍写「coverage.cpp 239 行」「p3_resample. | 「实测 N 行」现状断言整簇滞后原样；phase3_proj 两现状陈述被代码推翻的实例亦原样。 |
| 34 | M6b-E-001 | STILL | SPEC 权威声明 :15-16「JSON（权威，机器真相）」零漂移；路由面全数仍指旧 CSV：README-DOCS :13「**唯一矩阵** docs/TRACEABILITY.csv」 | 双头无门比对机制原样。恒绿面更新：TRACEABILITY 门现跑 tools/quality/check_traceability.py（check |
| 35 | M6b-G-002 | STILL | 四份并存（路径同报告）；三套状态词并立：owner 版 :14-18「状态词阶梯（唯一口径）」+ :31-32 明言取代旧 PASS 口径，但 docs/RELEASE_STATUS.md: | 发布/完成状态无单一事实源 + 门禁自相矛盾两腿全存。 |
| 36 | M7-A-001 | STILL | SCI-UPM :46/:47 两式零漂移（三因子积式 vs per-control 归一两行并存）；§3 量纲表 :29（control_ivar ADU⁻² 行）；§7 :79 k_co | 所有报告锚逐字复现，无任何文本/实现/裁决动作落盘。 |
| 37 | M7-A-124 | STILL | GLOSSARY variance 行现 :10（仍单值式 variance_p=Σ v_j·w_jp²/D_p²、仍写「无覆盖像素=0」）、ivar 行现 :11（仍「variance=0 | GLOSSARY 未随 DATA-UNC-001 双值消解同步——「=0」与「→NaN」直接互斥、公式未换 var_num_sum/D² 形 ⇒ 原样 |
| 38 | M7-A-134 | STILL | ALG-CAL F4.1 冷像素行现 :149 cold = (bias < med − cold_sigma·σ)，med/σ 上一行仍在 dark 域定义（:147-148）——复用符号 | 两篇 ALG 对同一实现的冷分支判据仍互斥（按 ALG-CAL 字面冷阈基于 dark 电平，冷像素永不被检出），处置建议「以代码符号回写一篇」未执行 |
| 39 | M7-E-202 | STILL | docs/science/REJECTION.md :32 自引 docs/science/REJECTION.md:16，而 :16 现为 weights[i] 行、n=nominal 定 | 绝对行号自引在插入后指错行的机制原样；DOC-LINE-ANCHORS 门不覆盖 SCI 文档内部自引（无新增防护证据）。 |
| 40 | M7-I-203 | STILL | §24.4 现 :1656 起，「UNIT=ADU/tile 口径透传」在 :1659（报告复核时点 :1592 → **+67 漂移**，§30 扩写所致）；coverage 段所引 P2 | 给全整数的 coverage 结果挂「ADU/tile」单位口径的文字错误原样，且仍自称「唯一权威=DATA-COV-001 §19」透传。 |
| 41 | M8-F-003 | FIXED | tests/cli/test_cli_single_install.py::TestCliSingleInstall.setUpClass :35-39 —— 缺前置产物由 SkipTest |  |
| 42 | M8-F-011 | STILL | tests/realdata 实测 __init__.py+test_index_v12.py+test_match_plan.py 三文件（报告锚「目录实测 3 文件」零漂移）；ci/ch | 36 用例仍不在任何阻断 profile；引用面仍只有 tools/realdata/README/REVIEW（:1 标题+命令面原样）。形态侧：f |
| 43 | M8a-C-002 | STILL | cosmetic README :9-10「仅合同文件、无源码」/:19「DLL…尚未存在」vs 实存 astrocs_p1_cosmetic SHARED（lib/cosmetic/CMa | README↔CMake↔install↔product↔registry 五源对「源码有无/DLL 建没建/entrypoint 接没接」仍互斥；无 |
| 44 | M8a-E-004 | STILL | cosmetic README :7-8 签名头无解析前缀「include/astro_calibration.h」按 lib/cosmetic/include/ 解析落空（真身在 lib/ | 报告点名五站（现状段/测试引用/构建锚/两条 ARCH 行）全部仍红。 |
| 45 | M8a-G-008 | STILL | README-DOCS.md :8 把「L2 算法规范 docs/algorithms/*.md」定为 L2，而宪章 §12.2（现 :423-426）明定 L1 含 SCI/**ALG** | 两腿（改写 L2 定义 + 仓外 Wiki 顶格权威链）均原样；无机器门比对 README-DOCS 与宪章分层（AGENTS-GOV 只管 AGEN |
| 46 | M9-A-1 | STILL | 面积路径自证注释现 spherical_overlap.cpp:159「双路径切换阈值 60″ 无数学依据…Girard 失效」（原样，其路径已统一 fan 剖分）；引擎站 DrizzleR | 「修复未固化」三要件全中——判据未收口（引擎仍按无数学依据的 60″ 选路）、文档未登记去向、DISP 偏差未挂；文件虽在 changed 清单，站点 |
| 47 | M9-D-2 | STILL | 双口径并存原样（radius_deg ×4 与 match_radius_arcsec ×1，内部 :2448 radius_deg = match_radius_arcsec/3600.0 | 四站逐一复现，注释回写义务（§12.3-3）未执行。 |
| 48 | M9-G-6 | STILL | write_properties 仍 void（:335），if (!f) return; 吞失败、无 tmp+rename/fsync；aio_hips_finalize（:1361）有返 | HiPS properties（metadata 唯一载体）写失败静默 + 非原子发布两机制原样；正对照（HISS .partial/atomic_r |
| 49 | V1-N-03 | STILL | 头文件 :463「全局尺度与归一化基准均置 1.0」与 :466-467 字段注「snr_phot/median_snr = median(SNR_F)」同文件两值（报告 :463/:465 | 接口文本自斥 + 实现按路径二义原样；无门比对头注释语义与实现赋值。 |
| 50 | V3-N-03 | STILL | upm.cpp :245 if (cfg.grid != 8) return 3（报告 :245 零漂移）与 :1065 open 校验（报告 :1061-1065 区间内）；**缺键即 8 | 「写侧必写、读侧必校验、不等即 rc=3」三件中读侧对缺键走默认而非拒 ⇒ 跨版本静默错位镜像原样；报告自注「潜在（非当前）」亦原样。 |
| 51 | V7-N-01 | STILL | IR 侧静默兜底原样：p1_num !is_number→dflt（:1134-1137）使 hot/cold 缺键/错型→5.0、1e999→inf 直穿 ac_correct_frame | 两通道值域几乎不相交 + IR 全静默 + stage 无生效参数判别位 + IR 分叉零覆盖，四腿全数复算成立。 |
| 52 | V7-N-09 | STILL |  | 抽查 4/9 负判项（precision_mode 双拒、HISS signal_dtype 恒写、nside 三态记账、drizzle FP 归约唯 |
| 53 | V9-N-01 | STILL | 三行冻结科学门仍缺位（净删 4 行未回滚，两向复核 0 命中）；现行 CON-TRACEABILITY 跑 tools/quality/contracts/check_traceabilit | 机制三件套（净删无检测 / 门按子串 / 无 ID 承载对账）原样；工单「回滚三行」未执行。 |
| 54 | V9-N-10 | STILL | run.py 现 :901 elif returncode == SKIP_EXIT_CODE and not timed_out: → verdict = V_SKIP_WAIVABLE（ | 机制②新通道原样：77 语义对 waivable=false 门同样生效，且计入 skipped_waivable 计数。 |
| 55 | V10-N-03 | STILL | xf->item_size = item_sz[0] ? atoi(item_sz) : 0 现 :1269（报告 :1213 → +56 漂移，.c 在 changed 清单）；byte_ | 报告「界内性成立无越界」的限定仍真；损坏静默分支留。gaia_unshuffle_test.c 边级锁未见新增。 |
| 56 | V11-N-02 | STILL | 真源 AstroSphereTileView 现 aio_hips.h :69-81（8 字段含 const void* var_num_sum，sizeof=56）；五镜像 hips_di | 无一份镜像可作参照物原样；「当前无越界路径」限定仍真（调用点未把 variance 产品需求路由给这些镜像），但 Python 侧 variance/ |
| 57 | V11-N-10 | STILL |  | 五镜像至今**无任何门采集**（不在 UT discover 面、不在 ctest 注册、无 ctypes 布局对账工具）⇒ 「镜像同步↔门采集」10 |
| 58 | V12-N-08 | STILL | C++ 常数八位 :96/:97(90.0)/:98/:108/:109(0.70)/:110/:111/:184 全部零漂移；Python :451-455 六常数值集合不同（无 90.0 | 三腿（两套判据集合不同 / 边界开闭相反 / 非冻结值当门）+ 正例（可复算反例 wall=12s 均值86% p50=88% frac0.65：C+ |
| 59 | V12-N-16 | STILL |  | ln10 两处独立硬编码腿成立（且两版位数不一致，正是「派生优于复制」未推广的实例）；**sqrt(10) 两站与正例 MAG_PER_LN10 在工 |
| 60 | V13-N-07 | STILL | schema 对 evidence_id 仅 ^EVID-…$ 格式正则（:54），无承载要求；SPEC/standards 仍无「编号先登记后使用」「EVID 须对应证据件」条文 ⇒ 新造 | 制度缺口 + 自毁两腿原样；C-18 编号对账门仍未出现。 |
| 61 | V18-N-01 | STILL | 三种 cond 语义并存复算：①「不判即翻」——p1cal 宏 :50-58 注入分支排 !(cond) 之前（短路，cond 不求值），10 套同族宏文件在位（p1cal/p1drz/p1 | 零一致性门未出现（无任何 CI 项比对 12 套宏语义）；C-21 判据②在现状仍不成立。 |
| 62 | V19-N-01 | STILL | CI-REG-002 校验器 ADD_TEST_NAME_RE = re.compile("add_test\s*\(\s*NAME\s+...")（:65）——目标集只从 CMake 文本 | 三条事实（40 计数、正则闭包、C4 反向）逐字复现，无新增运行期枚举（ctest -N）接入证据。 |

## 逐条复核记录

<!-- 逐条追加 -->
### FD-F-001 · **STILL**
- 命令：`grep -n 'check_shared\|s.find(' tests/unit/cpu_provider_test.cpp`；`grep -n cpu_provider tests/unit/CMakeLists.txt`；`git log --oneline -- tests/unit/cpu_provider_test.cpp`（仅 1 次提交 4560aaf9，创建后未再改）
- 输出/新锚：`tests/unit/cpu_provider_test.cpp::check_shared` :62-78，两条文本断言仍在 :72-73（`CHECK(s.find("baseline_kernels_impl.inc")…)`/`CHECK(s.find("backend_table.inc")…)`）；注册面 `tests/unit/CMakeLists.txt` :218/:223 原样
- 判据：机制未变——fopen 后端源文件读 4096B 前缀、substring 查 include 文件名，仍测文本不测 kKernels 表内容/行为；建议的行为级等表测试（逐列比对）未在任何测试面出现（全仓 grep 'kKernels' 测试侧零命中）。行号：报告时未记行号，无漂移问题。

### L28c-E-002 · **STILL**
- 命令：`grep -n '' lib/photometric_calib/cpp/src/spectrum_integrator.h | sed -n '1,10p'`；`git ls-files | grep -i synthetic_photometry`（0 命中）；`ls lib/photometric_calib/spectrum_integrator`（不存在）；`grep -c 'ALG-PHOTOMETRIC-FIT' docs/algorithms/PHOTOMETRIC_FIT.md`（=0）
- 新锚：`lib/photometric_calib/cpp/src/spectrum_integrator.h::文件头` :3（契约锚行）与 :7（参考行），报告锚 :3/:7 无漂移
- 判据：① :7 `参考: lib/photometric_calib/spectrum_integrator/python/synthetic_photometry.py` 三级复核均不存在（工作树/HEAD/改名检索）→ 悬空依旧；② :3 引 `ALG-PHOTOMETRIC-FIT-*`：PHOTOMETRIC_FIT.md 实际 ID 族为 `ALG-PHOT-001/002`，通配锚零承载；③ :3 引 `lib/photometric_calib/docs/algorithm.md §3`：文件存在（v1.0 起入库），但 :7-13 挂 **ARCHIVED_NON_NORMATIVE 横幅「不得作为权威；数值多处失实」**（含 :115 步长 0.1nm 自标失实）——作为数值口径锚该面已被废止，机制仍成立。注：B4-17（1a21b52d）只加了 PHOTOMETRIC_FIT 主锚，未订正 :7 与通配形。

### M1a-A-005 · **STILL**（且单位名实不符面有增无减）
- 命令：`grep -n '0.002\|2%' docs/algorithms/PLATESOLVE.md`；`grep -n 's_min\|good_rms' lib/plate_solve/cpp/ipv/include/ipv_types.h`；`grep -n 'px\|arcsec' lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp`；`sed -n '454,457p' lib/plate_solve/cpp/ipv/src/ipv_solver.cpp`
- 新锚：PLATESOLVE.md :83「尺度容差 0.002 (各向异性 0.2%)」与 :187「CD 元素相对误差 ≤2%（§9 尺度容差 0.002 同源）」行号零漂移；`ipv_types.h::IPVSolverParams` 字段现 :213-216（报告锚 :201 → **+14 漂移**，因 StarSelection 于 base→head 间加 P14-N-10 字段）；`ipv_ransac.cpp::文件头` :18「默认 [0.95, 1.05]」原样；`ipv_solver.cpp` :455-456 `triangle_match(…, 0.002, selection.s0)` 原样
- 判据：0.2%↔2% 十倍挪用与「同源」错误声明原样；尺度窗四套并存（s_min/s_max=0.90/1.10 ±10% 注释、ransac 头注 [0.95,1.05]、solver 注释 ±20%、triangle 实参 0.002）全部健在；SCI 侧（ASTROMETRY.md）grep 零命中 → 生效阈值无 SCI 出处仍成立。新证：`good_rms_threshold` types.h:214 注「(角秒)」而消费点 ransac.cpp:485-488 注「现在是像素单位」+日志 "px"（:340 τ 同名弧秒字段注「默认 3 px」）——3600× 族实例仍在，处置建议的改名 `*_px` 未做。

### M1a-C-003 · **STILL**（P0；base→head 间 module_adapters.cpp 大改但本站逻辑未动）
- 命令：`git diff 521095b8..a3a343a4 -- lib/core/src/module_adapters.cpp | grep emplace_back`（p1_op_wcs 区零命中）；`sed -n '2146,2151p;2384,2387p;347,352p' lib/core/src/module_adapters.cpp`；`grep -n crpix lib/phase1/wcs/wcs_tan.h`
- 新锚：0-based 喂点环在 `module_adapters.cpp::p1_op_wcs` 显式分支 :2148-2151 与求解分支 :2384-2387（报告锚 :1785-1787 → 漂移 +361/+363，行号重写必要）；`p1_tan_forward_reference` :347-352 `u = x - crpix1` 与 `wcs_tan.cpp::WcsTan::pix2sky` :9-10 `dx = x - crpix1` **同原点式**；wcs_tan.h :2 合同行、:11「参考像素 (1-based)」原样
- 判据：① 环仍把 0-based 数组下标喂进 1-based 契约（wcs_tan.h:11 与参考解头注 :342「像素输入为 FITS 1-based」名实分裂）；② 交叉门仍与生产式共用同一原点平移 ⇒ 对 ±1 原点错置零判别力（注释自辩的独立性只针对 deg/rad 成对单位错，不针对原点）；③ 发布面 samples 键集仍为 x/y/ra/dec/…（:2183-2186、:2406-2409），无像素原点声明（全文件 grep pixel_origin = 0）。建议的必败负向用例与 xp=x+1 独立形式均未出现。

### M1a-D-003 · **STILL**（P2，多实例簇逐个坐实）
- 命令：`sed -n '23,28p' lib/phase3_session/p3_resample.h`；`grep -n '非目标' docs/algorithms/PHASE3_RSMP_IMPL.md`；`grep -n 'STD-F1' lib/phase3_session/p3_wcs.cpp`；`grep -n 'B2-\|R10-\|AUD-' lib/phase3_session/p3_output.cpp`
- 新锚：废止注释现 :23-28（p3_resample.h::p3_resample_check_mode 头注）；PHASE3_RSMP_IMPL.md :24「非目标: 不实现 variance/weight/ivar/support 输入」原样（合同未订正——代码 :26 已把 variance/ivar 转 `p3_uncertainty_open`(:84) 消费面，文档仍捆绑"不实现"）；p3_wcs.cpp STD-F1 流水注释现 :30-39；审计流水注释簇 p3_output.cpp 现 :58/:85/:141/:177/:203-205/:241/:261/:295/:307/:363/:399-401/:418/:546（报告锚 :73-84 漂移，簇未清）
- 判据：四类实例全数健在；唯一减项：p3_wcs.h :43-44「默认实参、既有调用零改动」现与代码一致（:47 确有默认参数），该子例不再独立成立，但整条缺陷为"多文件簇"，主例仍红。

### M1a-G-001 · **STILL**（范围须收窄：frame 一键已在 p3_writer.json 存在，采样语义仍零承载）
- 命令：`grep -c RADESYS lib/phase3_session/p3_output.cpp`（=0）；`sed -n '5941,5952p;6052,6091p' lib/core/src/module_adapters.cpp`；`grep -rn 'sampling_semantics\|pixel_semantics\|PIXORIGIN' lib/ docs/contracts/ include/`（非 md 面 0 命中）；`grep -n frame lib/phase3_session/p3_session.cpp`（:98-102 仅请求校验）
- 新锚：宪章 §4.3 义务行现 :125（原样）；FITS 键面 p3_output.cpp :185-186 原样无 RADESYS/EQUINOX；p3_resampled.json 键集 :5941-5952 无 frame；inspect result 键集 p3_session.cpp :400-414 无 frame；**反证须登记**：p3_writer.json :6069 `{"coordinate_frame","icrs"}` 与 run manifest :6091 已有 frame 键，且 `git show 521095b8:...` 计 6 处 → 该键在报告基线前即存在，非本轮修复
- 判据：§4.3「坐标 frame + 像素/采样语义」两义务中——FITS 产品头两键全缺、p3_resampled/inspect 两面无 frame、**像素/采样语义全仓产品/清单/inspect 零承载**（只有代码内注释 "FITS=+1"）⇒ 产品面不可判 frame/采样仍成立；建议 ①RADESYS ②sampling_semantics 双键 ③DATA §27 登记均未做。

### M2a-A-2 · **STILL**
- 命令：`grep -n 'px²|ADU|sr' docs/science/DRIZZLE.md`；`sed -n '262,263p' docs/contracts/DATA_SEMANTICS.md`；`grep -c BUNIT lib/astro_image_io/src/hips/aio_hips_writer.cpp`(=0)；`git diff 521095b8..a3a343a4 -- docs/contracts/DATA_SEMANTICS.md` §11.2/12.2 零变更
- 新锚：DRIZZLE.md :27「px²（球面立体角等价）」原样；:48-51 累加式原样；DATA_SEMANTICS.md :262 sumArea=**sr**、:263 variance=ADU²（与 §5 variance_p=sumVarNum/D_p²、D 为 px² 维度不符）原样；`aio_hips_writer.cpp::write_tile_core` cards 面 BUNIT 仍 0；descriptor :608-609 hips=ADU(PIXEL)/tile=SURFACE_BRIGHTNESS(HEALPIX) 原样
- 判据：px²↔sr 混用（SCI 说等价、DATA 说 sr、writer 无单位键）、variance 公式维度不符、HiPS tile 无 BUNIT 三腿全存；未引入换算因子。

### M2a-C-11 · **STILL**
- 命令：`sed -n '81,94p' contracts/data/phase2_uncertainty_rejection_provenance_v1.json`；`grep -n AIO_HIPS_PRODUCT_NREJ lib/astro_image_io/include/aio_hips.h`；`grep -c ASTROCS_ lib/astro_image_io/src/hips/aio_hips_writer.cpp`(=26)
- 新锚：pending_aio_channels 仍 :81-94 零漂移（writer_int32_tile.gap 写「flags 位 32/64 未定义」、properties_key_channel.gap 写「ASTROCS_* 五键无法写」）；而 `aio_hips.h` :46-47 NREJ=32/NUSED=64 已定义、`aio_hips_writer.cpp` :274 起 int32 BITPIX=32 诊断平面已落、:175-181 五键通道已在
- 判据：合同 JSON 与已落地能力直接矛盾原样；`lib/astro_image_io/memory.md:290` 仍写 pending；建议的 pending 块订正/删除未做。

### M2a-C-6 · **STILL**（一站点灭失，须记）
- 命令：`grep -n pixfrac lib/orchestrator/cpp/include/json_config.h configs/stage1.template.json`(后者无此文件)；`grep -n 'p1_num(dj, "pixfrac"' lib/core/src/module_adapters.cpp`；`grep -n 'API-DRZ-001 默认' lib/drizzle/src/module_entry.cpp`；`sed -n '207,240p' docs/contracts/PUBLIC_API.md`
- 新锚：json_config.h :67 `pixfrac = 0.8; // 生产默认 0.8` 原样；`module_adapters.cpp::p1_op_drizzle` 缺键默认 1.0 现 :2968（报告锚 :2234 → +734 漂移）；`lib/drizzle/src/module_entry.cpp` :355 `if (!found) pixfrac = 1.0; /* API-DRZ-001 默认 1.0 */` 原样；PUBLIC_API.md API-DRZ-001 全节无任何默认值条款 → 杜撰锚原样
- 站点灭失注记：报告引用的 `configs/stage1.template.json:41` 在工作树与 HEAD 均无（`git show a3a343a4:configs` 路径不存在；`git log -- configs/` 零提交，从未入库）。默认值分裂主链经其余站点仍成立，判 STILL 不判 MOVED。

### M2a-E-1 · **STILL**（引用对象仍不存在，部分锚新失准）
- 命令：`git ls-files | grep '/api.cpp$'`(0)；`sed -n '113p' docs/algorithms/DRIZZLE_GEOMETRY.md`；`sed -n '245p;261p' docs/contracts/DATA_SEMANTICS.md`；`grep -c finalize_tile lib/astro_image_io/src/hips/aio_hips_writer.cpp`(=0)；`sed -n '47p' lib/healpix_db/healpix_drizzle/drizzle_engine.h`；`sed -n '1712p' lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`
- 新锚/复算：DRIZZLE_GEOMETRY :113 仍引 `api.cpp:191`（无此文件）；DATA_SEMANTICS :245 仍引 `api.cpp:486-503`；:261 与 DRIZZLE.md :131 仍引 `finalize_tile`（writer 0 命中）；drizzle_engine.h :47-48 仍注「02_FROZEN §8/§10」（该文档仅存在于 `设计大纲/_evidence/.../02_FROZEN_STAGE1_HISS_SPEC.md` 历史取证包，非规范路径）；DATA_SEMANTICS :245 引 drizzle_engine.cpp:1712 → 现 :1712 为 compute_tile_nside 行（0765064c 流水线改造致再漂，DISP-DRZ-004 所指累加器语义不在该行）；module_adapters :832 `d.alg_id="ALG-005"` 仍无对应合同（registry 页 upstream 用 ALG-DRZ-001）
- 判据：≥9 类引用对象零承载，机制原样；追溯门仍拦不住（M6b-E-001 同族）。

### M2a-G-1 · **STILL**（较报告更差）
- 命令：`grep -in 'standards|registry' ci/checks.json`（仅 WORKFLOW-REGISTRY-BINDING，属 ci/validate_workflow_binding.py，与注册表无关）；`grep -rn check_standards_registry .github/ ci/`（零命中）
- 新锚：`ci/checks.json` 仍无该项；`docs/standards/STANDARDS_REGISTRY.md` §5(:292-329) 与 :8 仍自陈「机器检查: check_standards_registry.py（exit 0=PASS）」；**报告时 build.yml 尚有 standards-registry 步骤，现全 workflow 零引用** → 检查器完全脱门
- 判据：「检查器存在但未登记进 ci/checks.json」原样成立；D.catalog/D.fits 失真 CONFORMANT 仍无门可拦。

### M2a-I-2 · **STILL**
- 命令：`sed -n '57p' lib/gaia_xpsd_client/module.yaml`；`sed -n '167p' docs/algorithms/GAIA_QUERY.md`；`grep -n 'qsort' lib/gaia_xpsd_client/src/gaia_client.c`（无）；`grep -n 'FindFirstFileA\|readdir' gaia_client.c`（:1972/:1993）
- 新锚：module.yaml :57 `determinism: fixed_reduction_order`；GAIA_QUERY.md :167 第 5 条「输出行序依赖目录枚举顺序（跨平台不稳定）」——两行号零漂移；gaia_client.c 虽在 base→head 改动清单内，枚举面无任何排序引入
- 判据：module.yaml 声明与 ALG 自述的枚举序依赖仍互斥；未加排序、未改声明、module.yaml 与 ALG 均未订正。
### M2b-B-05 · **STILL**
- 命令：`grep -n 'http:// ' lib/astro_image_io/src/hips/aio_hips_writer.cpp`（3 行 4 处）；`grep -rn metadata.xml tests/ | grep -i vot`（零命中）
- 新锚：`aio_hips_writer.cpp::finalize_snr_product` VOTABLE 头 fprintf 常量串现 :1249-1251，`xmlns="http:// www.ivoa.net/xml/VOTable/v1.3"` 等 **4 处字面空格原样**（报告锚未给行号，无漂移问题）
- 判据：URI 内空格未去 → 产出仍非合规 VOTable、根命名空间解析必败；建议的可解析性测试/XML 断言未加（无任何 VOTABLE 解析测试面）

### M2b-F-01 · **STILL**（P0 判据全数复算成立）
- 命令：`sed -n '121,145p' docs/standards/STANDARDS_REGISTRY.md`；`grep -n astropy lib/common/healpix/healpix_core.h`；`ls lib/common/healpix/tests/`；`grep -n 'test_healpix_oracle|test_hips_tile_mapping' tests/unit/CMakeLists.txt`（0 命中）；oracle 测试体内 sed :88-96
- 新锚：D.healpix COMPLIANCE: CONFORMANT :127、DEVIATION 无 :129、§5.3 行「往返 ≤1e-12 deg；astropy-healpix 百万点 oracle 对拍」:135、偏差表「（无）」:139-143 全原样；healpix_core.h :6 外部 Oracle 声明原样；tests/ 目录五文件无 oracle.jsonl 冻结样本、无 astropy-healpix 生成器（gen_spatial_fuzz.py 为模糊器非 oracle 生成器）；ctest 仍只注册 `r9b_healpix_neighbors`（tests/unit/CMakeLists.txt :612-614），**test_healpix_oracle 与 test_hips_tile_mapping 均未注册**；`test_healpix_oracle.cpp` :88 极点 continue 跳过、:95 `d > 1.2 * hp_res_deg + 1e-9` 容差原样
- 判据：百万点独立 oracle 在仓内仍不可复现（数据缺位+生成器缺位+未注册）；现存判据口径（1.2×像素≈1e-3 deg 级）与声明 1e-12 deg 差约 10 个数量级；极区专项仍零覆盖。CONFORMANT+无偏差判定仍失去支撑。
### M3-A-002 · **STILL**（P0；SCI↔实现互斥未收敛，双 SCI 互斥未裁决）
- 命令：`sed -n '63,71p' docs/science/CONTROL_WEIGHT_SNR.md`；`sed -n '44,55p' docs/science/PHASE2_UPM.md`；`grep -n 'weights\[s\] = support_v\[s\] \* snr_v' lib/phase2/tools/stage2.cpp`；`sed -n '372,385p' lib/phase2/src/stage2_common.cpp`；`git diff --stat 521095b8..a3a343a4 -- lib/phase2/`（空，代码未动）
- 新锚：SCI-CW §4 代码块现 :65-71，:66 仍标「像素级 SNR 权重（stage2 排异/积分，weight_mode=2）」、:71 仍写 `weights[s] = support[s] × snr_v²`（报告复核时点 :38/:42 → 因 P5-SNR 订正注释插入而 +27 漂移）；符号表 w_snr 现 :40（原 :27）；实现 mode=2 走 ivar（`weights[s] = iv` :1112/:1379），support×snr² 在 **mode=0** 两站 :1141/:1414（报告锚 :1136/:1396 → +5/+18）；stage2_common 解析 :373-385 无漂移（默认 ivar :378、错误串 :384）；SCI-UPM 禁式现 :54（「禁 production 乘 star SNR / support^p」）
- 判据：三腿全存——①CW 把 support×snr² 归给 weight_mode=2 而实现 mode2=ivar；②SCI-UPM:54 与 SCI-CW:66/71 互斥（负责人裁决 A-02 仍未落文本）；③P5-SNR 订正只重命名语义（quality_weight），未订正 mode 归属。另 UPM:47 `normalized = raw/Σraw·geom` 与 CW 域式仍两套并存。

### M3-C-004 · **STILL**
- 命令：`grep -n '历史|ARCHIVED|非规范' lib/calibration/CALIBRATION_PROCESS.md`（0 命中）；`sed -n '52,62p;66,79p' CALIBRATION_PROCESS.md`；`grep -n 'OLS|回归' lib/calibration/src/dark_optimizer.cpp`（:14/:219-220）；`sed -n '99,102p' docs/science/CALIBRATION.md`
- 新锚：CALIBRATION_PROCESS.md 仍无文件级历史/非规范标记（§2 :52-62 K 初值 t_light/t_dark + 「残差最小化（背景区域 MAD 最小）搜索最优 K」原样；§三坏点检测 :71-79「5×5 中值滤波+残差 MAD、max_neighbor_candidates=0」原样）；实现是鲁棒线性回归估 k（optimize_dark_k :97 起）；SCI §10 禁止项现 :99-100「将 K 改为优化搜索值而非 t_light/t_dark 比值」；README 侧「历史流程参考」注记仍只在 README:147
- 判据：文件自身无标记 + 三处叙述与冻结 SCI/现行实现互斥原样；README:147 的「历史流程参考」不构成文件级豁免。

### M3-C-012 · **STILL**
- 命令：`grep -n 'double /\*mag_max\*/' lib/photometric_calib/cpp/src/pc_api.cpp`（:181/:566/:827）；`grep -n 'mag_max_arr' pc_api.cpp`（:254/:636/第三站同族）；`sed -n '133p' lib/photometric_calib/cpp/include/photometric_calib.h`；`sed -n '147,148p;168,169p' docs/algorithms/PHOTOMETRIC_FIT.md`
- 新锚：三处形参静默丢弃现 :181/:566/:827（报告复核时点 :172/:544/:800 → +9/+22/+27 漂移）；自适应锥 {12,13,14,15,16} :254 与 `n_gaia >= 2000` :278 原样；公共头 :133 参数说明仍写「mag_min, mag_max - 星等范围」无失效警示；DISP-PHOT-005 登记仍在 ALG :168-169
- 判据：静默丢弃机制与无出处常数锥原样；DISP 登记在报告时即存在（POS 引用它），非修复证据；头文件合同面未订正。

### M3-I-001 · **STILL**
- 命令：`sed -n '9,10p;19p;23p' lib/cosmetic/README.md`；`grep -n '尚未存在生产符号' docs/algorithms/COSMETIC_ALGORITHMS.md`（:9）；`grep -n 'add_library(astrocs_p1_cosmetic SHARED' lib/cosmetic/CMakeLists.txt`（:50）；`grep -n 'add_subdirectory(lib/cosmetic)' CMakeLists.txt`（:199）
- 新锚：COSMETIC_ALGORITHMS.md :9 仍写「落码由 P1-COS-IMPL 执行，尚未存在生产符号」；lib/cosmetic/README.md :9-10「当前本目录仅合同文件、无源码」、:19「DLL 目标…尚未存在」、:23「不声明 IMPLEMENTED」原样；而 module_entry.cpp/CMakeLists.txt/tests/p1cos 均已入库、:50 SHARED 目标存在、根 :199 挂接；CALG §1 :20-23 仍称唯一构建清单为根 CMakeLists.txt:321-333 STATIC，同文档 :5-8 又指模块化——自相矛盾原样
- 判据：「已入库被消费的代码被写成未编译/无符号/无源码」整族现状断言仍过期；无一处订正。
### M3b-A-05 · **STILL**
- 命令：`grep -n 'thetas\[4\]' lib/dynamic_psf/src/dpsf_psf.cpp`（:411）；`sed -n '65p' docs/science/PSF.md`；`sed -n '76p;175,177p' docs/algorithms/STAR_PSF_ALGORITHMS.md`；`sed -n '67,68p' lib/dynamic_psf/tests/p1psf/p1psf_oracle.hpp`；`sed -n '104,113p' p1psf_tests_core.cpp`
- 新锚：候选集四元组原样（dpsf :411；报告复核时点消歧 :389-401 → 现 :405-42x，+~16 漂移）；PSF.md「旋转简并不变量」现 :65（原 §7:63-65 区间内）；ALG §3 :76 零漂移、§11.2 :175-177；oracle.hpp :67-69「四元组生成逐元素相同的 M」原样；U1 M 判据现 :104-113
- 判据：{θ,π/2−θ,π/2+θ,π−θ} 非 Moffat4 二次型对称群（90−θ 是镜面反射，未同步交换 sx/sy 时 M(θ)≠M(90−θ)），而 SCI 不变量声明与 oracle「逐元素相同」注记均未订正；恒真不可证伪原样。

### M3b-D-01 · **STILL**
- 命令：`grep -n 'FWHM 上限完全\|修复: RMSE 系数' lib/star_detector/src/sdet_api.cpp`（:230/:207）；`grep -n 'v1.1 新增,' lib/dynamic_psf/include/dynamic_psf.h`（:67）；`sed -n '55,57p' dynamic_psf.h`
- 新锚：残句「// FWHM 上限完全」现 sdet_api.cpp:230；修改流水「修复: RMSE 系数从 mad*3.0 改为 mad*1.4826」:207；dynamic_psf.h :55「(, PREC-105)」与 :67「(v1.1 新增,)」残句原样；:109-114 B2-A2 注释簇在位
- 判据：注释卫生簇实例逐条仍在（断句残句×3、修改流水×1）；公共头单位契约欠账（star_detector.h :17-67 面）未见补写。

### M3b-I-01 · **STILL**（一实例已修，须记 SDET-ANGLE-001）
- 命令：`sed -n '415,431p' lib/star_detector/src/sdet_api.cpp`；`grep -n '3.7172' sdet_api.cpp`（:1823）；`sed -n '114p' lib/dynamic_psf/README.md`；`sed -n '91p;135p' docs/science/NOISE_MODEL.md`
- 新锚：① **不可达守卫实例已修**：SDET-ANGLE-001（P11）以 normalize_angle_deg_bounded 替换 while 循环+死 10000 守卫（sdet_api.cpp :415-431 非有限 fail-closed）——修在我报站点，注释亦点名原缺陷；② s_factor 注释「=3.7172 (line 275)」现 :1823（原 :1787，+36 漂移）绝对行自引仍在，ALG STAR_DETECTION_ALGORITHMS.md:52 复读「(:1678)」同族自引未清；③ README:114「DPSFFitResult **12 字段**」而 struct 实为 **13 成员**（status+12，dynamic_psf.h:17-31）→ 字段数文字错原样；④ NOISE_MODEL.md :91 仍写 15 位常数与 4 位常数差「<1e-12」（实算 5.602e-12，SUMMARY 簇 5 在册）+ :135 双常数并存
- 判据：整条为多实例簇——θ 守卫一站已 FIXED（SDET-ANGLE-001，含逐位一致回归语义），其余实例（行自引/字段数/常数截断）原样 ⇒ 整条判 STILL，账本可加注「θ 守卫子项已由 P11 修复」。

### M4-C-06 · **STILL**
- 命令：`grep -n 'INVALID_.*hard fail\|非有限' docs/science/REJECTION.md`（:34/:61/:78/:87）；`sed -n '1183,1207p' lib/phase2/src/rejection.cpp`；`grep -n 'has_nonfinite' rejection.cpp`（:1873-1890/:1971）
- 新锚：SCI 判据行 :34/:61/:87 零漂移；实现 gather 资格层现 :1183-1207 对非有限 value `++invalid_finite; ok=false` **逐样本静默剔除**、support 非有限落 `>support_threshold` 假分支即 invalid_support、weights **无有限性校验**（:1211 直拷）；p2_reject_stack_ex 入口现 :1684（原 :1710）；RCR weights 消费现 :1801；compat has_nonfinite 通道现 :1873-1890+:1971（`(void)has_nonfinite;`）
- 判据：SCI「INVALID_INPUT hard fail」vs 实现剔除/不检查三腿原样；oracle 对比脚本 rejection_oracle_compare.py :250-262 仍仅在 compat 通道断言 NaN→status=3（其注释自辩「生产路径由 eligibility 层过滤」恰证实与 SCI 语义背离）。

### M4-F-05 · **STILL**
- 命令：`sed -n '16p;215,221p' lib/healpix_db/healpix_drizzle/tests/control_median_mc_test.cpp`；`sed -n '142p' docs/science/PHASE2_UPM.md`；`sed -n '98p' docs/algorithms/UPM_SOLVER.md`
- 新锚：判据段现 :217-219 `if (!(k_corr >= 0.98 && k_corr <= 2.0))` 原样（复核时点 :217-219 零漂移）；头注释 :16「UPMW-005 断言 |k_corr_frozen − k_corr_empirical| 在容差内」原样——**与判据段矛盾未消**（实现是无锚宽区间）；UPM.md :142/UPM_SOLVER.md:98 仍引用「MC 实测 1.3883」为锚，测试判据不含 1.3883/1.4
- 判据：「MC 硬门」仍为 [0.98,2.0] 宽松区间（k_corr 漂移 1.0→1.99 皆绿），头注自述与代码相反，且不锚定冻结值；另 control_median_mc 未在 p1drz/根 tests 的 CMakeLists 注册（构建孤儿同族，L22-005 域）。
### M5a-C-004 · **STILL**
- 命令：`grep -n 'dlopen\|TOL =\|2e-4' tests/cpu/avx2/run_provider_avx2_checks.py`；`sed -n '3p;11p' tests/cpu/avx2/provider_avx2_oracle_main.cpp`；`sed -n '24p;112,118p' docs/architecture/cpu/CPU_003_AVX2_PROVIDER.md`；`sed -n '73p' docs/architecture/ISA_VARIANTS.md`
- 新锚：run_provider_avx2_checks.py :10（dlopen baseline.so+avx2.so 互比）、:15（对照容差 ≤2e-4）、:54 `TOL = 2e-4`、:170-172 判定——四点与报告锚（:10-16/:54/:170-172）零漂移；oracle_main :3 仍自述「dlopen 两个独立 provider .so」、:11「baseline 对照容差」；CPU_003 :24/:112-118 容差式与「与 CPU-002 同规」原样；ISA_VARIANTS.md :73「variant Oracle …（共享源）… 与 Python 参考比对」原样
- 判据：「与 Python 参考比对」仍只是援引性声明——核验链是 avx2↔baseline 两生产 provider 互比，Python 侧无比对生产交付物的独立参考数值面（NUMERIC_STANDARD.md:17 等价性测试条款原样）；独立性=继承 CPU-002 的历史一致声明，未做误差合成。文件在 changed 清单内但上述站点逐行未动。

### M5a-G-006 · **STILL**（裸 omp 面还在扩大）
- 命令：`grep -rn '#pragma omp parallel' lib/ --include=*.cpp --include=*.h | wc -l`（=**106**，报告时 99）；`sed -n '1p;14,16p' lib/phase2/include/astro/phase2/execution_options.h`；`grep -n '#pragma omp' lib/calibration/src/calibrator.cpp`；`sed -n '8p' docs/architecture/THREAD_BUDGET_ARCH.md`
- 新锚：execution_options.h :1「全局执行预算唯一来源」+ :16/:24 hardware_concurrency 自取（零漂移）；calibrator.cpp 裸 omp 五站 :89/:119/:128/:162/:171（零漂移）；THREAD_BUDGET_ARCH.md:8 义务「available_cpus = affinity ∩ cgroup ∩ Job Object」；宪章「一个进程只有一个资源调度器和线程预算源」现 :367
- 判据：第二预算源两机制全存——①execution_options 默认路径直接硬件自取（不做 affinity/cgroup 折算），与 ARCH:8 义务矛盾；②lib/ 生产 omp 隐式组队 99→106 处增长，无一处接 Runtime 预算；worker_advisor/cpu_features 的正确折算实现仍在位未被 run 路径消费（CONCURRENCY_STANDARD:5-6/:19 条款未变）。

### M5b-C-04 · **STILL**（涉未入库 WIP 文件，时点须注明）
- 命令：`sed -n '45,47p;71p;154p' include/astrocs/abi/lifecycle_v1.h`；`sed -n '1454,1464p' lib/calibration/src/module_entry.cpp`；`grep -n 'state == ACS_LC_STATE_DESTROYED' lib/*/src/module_entry.*`
- 新锚：合同面现 lifecycle_v1.h :45/:47（execute 互斥）、:71（double destroy→ACS_ERR_STATE 类违例）、:154（destroy 再调=检测）；实现面 cal_destroy 现 :1454-1464：`if (inst->state == ACS_LC_STATE_DESTROYED) return; /* double → 忽略 */`（:1458）后 `allocator->free(...)`；同族 cosmetic :1224、drizzle :682、hips :766、gaia :1021、snr :894；`state` 裸 int 字段（:612）与 `volatile int32_t cancel_req`（:613）仍非原子
- 判据：①double-destroy「忽略」而非合同要求的报错（destroy 签名返回 void，错误根本不可表达）原样；②检测读已释放内存原样——第二次调用先读 `inst->state`（内存已被第一次 free 归还）再判定；③execute 间互斥无实现（无锁/state CAS 检查站点未增）。**时点注**：`lib/snr_estimator/src/`、`lib/cosmetic/src/module_entry.cpp` 部分为工作树未入库文件（HEAD a3a343a4 不含 lib/snr_estimator/src/**），本判据基于工作树文本。

### M5b-E-05 · **STILL**（AGENTS 子面已修，主体原样）
- 命令：`grep -rn '宪章 §' docs/ REVIEW.md | grep -E '§[A-K]'`（RELEASE_STATUS:11、SCIENCE_OVERVIEW:79、MODULE_MAP:33 三处「宪章 §H/§F.1」）；`grep -cE '§[A-K]\.' ASTROCS_PROJECT_CONSTITUTION.md`（=0）；`REVIEW.md :133`
- 新锚：宪章零字母节（两向 grep 复核）；字母节冒充宪章现集中于 docs/owner/RELEASE_STATUS.md:11「宪章 §1.2/§H」、SCIENCE_OVERVIEW.md:79「宪章 §F.1」、docs/architecture/MODULE_MAP.md:33、REVIEW.md:133/:82/:104（§F.1 裸引）；SCIENCE_OVERVIEW 仍双前缀混用（:40「约束 §C.1」、:92「约束 §E.5」）
- 判据：治理引用层机制原样——ARCHIVED 文件字母条款号写作「宪章」且宪章无该节。子面订正：AGENTS.md 映射表现全为数字条款且与 check_agents_gov.py::REQUIRED(:6-18) 10/10 对齐（b40c8a49 GOV-AGENTS-001 转 PASS，早于报告时即如此）⇒ 报告 position 中 AGENTS :14/:16/:28 的「双向不一致」不立，建议账本把该子句从本条剥离；RELEASE_STANDARD.md 现仅 12 行（:19 站点灭失），TRACEABILITY_SPEC 字母引用已清。

### M5b-G-07 · **STILL**
- 命令：`find . -name 'PHASE_OVERVIEW*'`（0）；`sed -n '410,421p' ASTROCS_PROJECT_CONSTITUTION.md`；`sed -n '1,4p;27,29p' tools/check_l0_docs.py`；`grep -n '"DOC-L0"' ci/checks.json`（:370）
- 新锚：宪章 §12.1 L0 清单现 :410-421，`docs/owner/PHASE_OVERVIEW.md` 在 :416（点名字面零漂移）；全仓该文件不存在；ci/checks.json DOC-L0 现 :370（报告 :369-389 区间内）；`tools/check_l0_docs.py` 规则行 :4 与检查体 :27-29 仍校验 **docs/review/**{SCIENCE,PIPELINE,ARCHITECTURE,RELEASE_STATUS,CHANGE_REVIEW}_OVERVIEW.md
- 判据：门验错目录（docs/review ≠ 宪章指定 docs/owner）+ 点名文件缺失，两腿均原样；checks.json 在 changed 清单内但 DOC-L0 站点未动。

### M5b-G-15 · **STILL**
- 命令：`ls tools/check_cli_protocol.py`（不存在）；`sed -n '47p' docs/api/CLI_PROTOCOL_V1.md`；`sed -n '20,33p' tools/check_cli_command_layer.py`；`grep -n 'joined == "drizzle"' cli/commands.cpp`（:2514）
- 新锚：承诺行现 CLI_PROTOCOL_V1.md :47「机器化一致性检查器合同(04 §6,API-002 建立 tools/check_cli_protocol.py)」——该脚本两向复核（工作树+`git ls-files tools/`）不存在；字符串存在性判据原样（:20/:22 `not in PARSER`、:29-33 token in 判断）；commands.cpp drizzle 转发现 :2514（报告 :2451 → +63）；parser.cpp kRules :36、check_api_docs.py EXIT_NAMES :27、exit_codes.h COMPUTE :13 零漂移
- 判据：§6 承诺的检查器仍不存在，现役门仍是「源文件里有没有字面量」级判断，drizzle 类转发无规则表覆盖性核验。

### M5b-I-06 · **STILL**（有缓解注记，状态未改）
- 命令：`sed -n '238,245p' docs/DOCUMENT_INDEX.yaml`；`sed -n '3p' docs/audit/risk_verification_T012.csv`；`sed -n '7p;64p' docs/audit/doc_classification.csv`；`grep -n -A1 'docs/modules/orchestrator.md' docs/DOCUMENT_INDEX.yaml`
- 新锚：三份 audit 表仍 ACTIVE_INFORMATIVE（:238-245），notes 现加「V19R2 …历史快照；权威分类见本 DOCUMENT_INDEX.yaml」；T012.csv:3 仍引 `tools/stage2.cpp:720-724 weight_mode==2 禁 ACR` 而现行 :718-726 是 large_scale_rejection 装配段（锚失准，ACR-IVAR 现注释在 stage2_common.cpp:391）；doc_classification.csv:64 称 orchestrator.md「AUTHORITATIVE」而 INDEX :343-344 记 ACTIVE_INFORMATIVE；:7 RELEASE_STATUS 行称「CANDIDATE」与现行 owner/review 多头状态（M6b-G-002）不合
- 判据：登记状态未改（仍 ACTIVE_INFORMATIVE 而非 ARCHIVED/GENERATED）+ 内容与树不符原样；notes 的历史快照自述是缓解不构成交付（宪章 §12.3 一致性义务仍缺位）。
### M6a-D-002 · **STILL**
- 命令：`sed -n '14,15p;929,953p' lib/calibration/src/module_entry.cpp`；`sed -n '920,924p' lib/drizzle/src/module_entry.cpp`；`sed -n '17,19p' lib/cosmetic/src/module_entry.cpp`；`sed -n '30,32p' lib/snr_estimator/src/module_entry.cpp`
- 新锚：calibration 文件头现 :14-15 仍写「acquire 失败 → 单线程降级 (drizzle 先例, 非致命)」，同文件 :930-931「acquire 失败/executor 缺失均为硬 BUDGET, 不取 drizzle 降级单线程路径」+ :938/:951 `ACS_ERR_BUDGET` 硬拒 —— 头注与实现相反原样；drizzle :922 注释仍「单线程降级执行 (BUDGET 缺额非致命; workers=0)」；cosmetic :19「禁单线程降级 (cpu_heavy; CAL 先例同款」；snr :30-32「P1-CAL/COS 同款…禁降级」
- 判据：同一 host_lease 合同在四个适配器上仍是三种互斥语义（降级非致命/硬拒致命/硬占坑禁借口），且「CAL 先例」被两族各取所需地互指；无一处注释订正。

### M6a-D-010 · **STILL**
- 命令：`grep -c 'ADU' lib/calibration/include/astro_calibration.h`（=0）；`sed -n '69,79p' astro_calibration.h`；`grep -n '返回' lib/calibration/src/photometry_apply.h`（:27-31 仅列 -1..-4）；`grep -n 'return -5' lib/calibration/src/photometry_apply.cpp`（:51）；`grep -c 'ADU\|单位' lib/star_detector/include/star_detector.h`（=0）
- 新锚：ac_calibrate_frame 注释块 :69-78 有形状无单位/生命周期/线程安全（:79 定义）；photometry_apply.h 返回码表 :27-31 仍缺实现 :51 的 `-5`；star_detector.h :17-67 单位零载句原样；20 样本抽查的 55%/15% 缺失率是历史口径判读（SUMMARY 簇 4 补已裁定与 L28b 判据不同源，不撤）
- 判据：被点名的三实例（返回码表缺 -5、ac_calibrate_frame 无单位、star_detector.h 零单位契约）逐条复算仍在 ⇒ STILL；抽样统计面不依赖行号。

### M6a-I-001 · **STILL**（7 文件滞后簇无一订正）
- 命令：`wc -l lib/phase2/src/coverage.cpp lib/phase2/include/astro/phase2/coverage.h`（**280/64**）；`wc -l lib/phase3_session/p3_resample.h/.cpp/p3_session.cpp`（**116/366/433**）、p3_wcs.h/.cpp（**63/230**）；`grep -rn '239 行\|58 行\|165 行\|343 行' docs/contracts/PUBLIC_API.md docs/contracts/DATA_SEMANTICS.md docs/algorithms/PHASE3_*.md lib/phase2/README.md`
- 新锚：PUBLIC_API.md :1069 与 :2100、DATA_SEMANTICS.md :856 与 :2095 均仍写「coverage.cpp 239 行」「p3_resample.cpp（239 行）」；PHASE3_RSMP_IMPL.md :13-14/:54-56 仍写 58/239/343（实测 116/366/433）；PHASE3_PROJ_IMPL.md :11-12/:46 仍写 50/165「实测 2026-09-11」（实测 63/230）；lib/phase2/README.md :10-11「239 行实测/59 行」（280/64）、:34/:69/:94/:96-97 的 coverage.cpp 行锚区间（:59-140/:144/:154-157/:20-45 等）在 +41 行增长后未回写
- 判据：「实测 N 行」现状断言整簇滞后原样；phase3_proj 两现状陈述被代码推翻的实例亦原样。

### M6b-E-001 · **STILL**（P0；恒零门换文件不换行为）
- 命令：`sed -n '13,18p' docs/traceability/TRACEABILITY_SPEC.md`；`sed -n '13p' docs/README-DOCS.md`；`sed -n '38p' docs/DEVELOPER_GUIDE.md`；`grep -n TRACEABILITY.csv docs/standards/API_STANDARD.md docs/DOCUMENT_INDEX.yaml ci/checks.json`；比对脚本：python3 读两表按 science_id 交并
- 新锚：SPEC 权威声明 :15-16「JSON（权威，机器真相）」零漂移；路由面全数仍指旧 CSV：README-DOCS :13「**唯一矩阵** docs/TRACEABILITY.csv」、DEVELOPER_GUIDE :38、API_STANDARD :14、DOCUMENT_INDEX :68-69（且把旧 CSV 记 ACTIVE_INFORMATIVE、MATRIX.json :78-79 并存无互斥裁决）、ci/checks.json :55/:81 changed_paths；两表数据量：CSV 63 行 / JSON modules 30 行（报告 :67/:30 → CSV 被删至 63 与 V9-N-01 相关）；可交比 ID 现 **6 个**（曾 8），TEST 证据完全一致 **0/6**（例 SCI-DRZ-001 CSV=TEST-DRZ-CAND-001@cpp 测试 vs JSON=TEST-DRZ-DESIGN-001@docs 设计）
- 判据：双头无门比对机制原样。恒绿面更新：TRACEABILITY 门现跑 `tools/quality/check_traceability.py`（checks.json :45），其 broken 统计照打、**:159 `return 0` 无条件**（行号恰与原报告锚 :159 相同，属两版文件同线巧合，机制未变）；另一 TRACEABILITY-CODE 门跑 tools/check_traceability.py（现能 rc=1）但判决对象是 `artifacts/prerelease_v5/tables/TRACEABILITY.csv`（v5 归档表，非两矩阵）⇒ 校验对象≠交付对象新证。

### M6b-G-002 · **STILL**（P0；多头与状态词互锁原样）
- 命令：`ls docs/RELEASE_STATUS.md docs/review/RELEASE_STATUS.md docs/owner/RELEASE_STATUS.md docs/archive/review/RELEASE_STATUS.md`（4/4 存在）；`sed -n '6p' docs/RELEASE_STATUS.md`；`sed -n '29p' docs/review/RELEASE_STATUS.md`；`sed -n '12p' docs/archive/review/RELEASE_STATUS.md`；`sed -n '42,43p;208p;229p' tools/quality/validate_task_ledger.py`；`grep -n 状态机 AGENTS.md`（:28）
- 新锚：四份并存（路径同报告）；三套状态词并立：owner 版 :14-18「状态词阶梯（唯一口径）」+ :31-32 明言取代旧 PASS 口径，但 docs/RELEASE_STATUS.md:6 仍 `PRE_RELEASE_ENGINEERING_FOUNDATION=PASS`、docs/review/RELEASE_STATUS.md:29 仍「## 3. 冻结面（PASS…）」（:29 零漂移）、archive :12 含 REVIEW_PENDING；互锁原样：validate_task_ledger.py:208/:229 把 REVIEW_PENDING 作 illegal_state 负例，AGENTS-GOV 的 REQUIRED["状态机"] 却必须含 REVIEW_PENDING 字串 → AGENTS.md:28 被迫保留「被自家校验器判非法的状态行」（新注记只是缓解措辞，义务冲突未解）；DOCUMENT_INDEX :68-69 RELEASE_STATUS=ACTIVE_INFORMATIVE 与 TRACEABILITY.csv 并存的路由歧义亦原样
- 判据：发布/完成状态无单一事实源 + 门禁自相矛盾两腿全存。
### M7-A-001 · **STILL**（P0；负责人裁决 A-02 未落，三处「唯一」未收敛）
- 命令：`sed -n '46,47p;29p;79p' docs/science/PHASE2_UPM.md`；`sed -n '15,17p' docs/algorithms/UPM_SOLVER.md`；`grep -n '科学权重唯一\|唯一科学权重源\|唯一来源=control_ivar' docs/contracts/DATA_SEMANTICS.md`；`sed -n '12p' docs/GLOSSARY.md`；`sed -n '1337,1344p' lib/phase2/src/upm.cpp`；`sed -n '1031p' lib/phase2/src/sampler.cpp`
- 新锚：SCI-UPM :46/:47 两式零漂移（三因子积式 vs per-control 归一两行并存）；§3 量纲表 :29（control_ivar ADU⁻² 行）；§7 :79 k_corr 缩放不变量；UPM_SOLVER :15/:17 F1/F2「w_norm = w / Σw · geom per-control」；DATA_SEMANTICS 「唯一」三式现 :918/:1046（w_UPM 冻结式）与 :1521/:1766（control_ivar 唯一源）——两两组对立；GLOSSARY :12 pixel_weight「=ivar」却标「无量纲」零漂移；实现 upm.cpp :1337 sums[control_id]→:1344 out_norm=raw/s·rel、sampler.cpp :1031 control_id=cell 索引——Σ 域=单 control 内跨帧，与 :46 全栈归一歧义并存
- 判据：所有报告锚逐字复现，无任何文本/实现/裁决动作落盘。

### M7-A-124 · **STILL**
- 命令：`sed -n '10,11p' docs/GLOSSARY.md | cut -c1-150`；`grep -n 'NaN\|var_num_sum' docs/contracts/DATA_SEMANTICS.md | sed -n '262,346p' 区间站点核对`
- 新锚：GLOSSARY variance 行现 :10（仍单值式 `variance_p=Σ v_j·w_jp²/D_p²`、仍写「无覆盖像素=0」）、ivar 行现 :11（仍「variance=0/缺失 → ivar=0」）；DATA 侧 §30 双值口径已冻结：:262/:280/:316/:325/:346 一致为 var_num_sum/covered_area² 且无覆盖/无效 → **NaN**
- 判据：GLOSSARY 未随 DATA-UNC-001 双值消解同步——「=0」与「→NaN」直接互斥、公式未换 var_num_sum/D² 形 ⇒ 原样。

### M7-A-134 · **STILL**
- 命令：`grep -n -A2 'F4.1' docs/algorithms/CALIBRATION_ALGORITHMS.md`（现 :147-149）；`sed -n '45,49p' docs/algorithms/COSMETIC_ALGORITHMS.md`；`sed -n '143,149p' lib/calibration/src/cosmetic_corrector.cpp`
- 新锚：ALG-CAL F4.1 冷像素行现 :149 `cold = (bias < med − cold_sigma·σ)`，med/σ 上一行仍在 dark 域定义（:147-148）——复用符号未声明 bias 重算（报告锚 :134-136 → +13 漂移，3fd9e5e2 锚订正批次未触及此句）；ALG-COS :45-49 用 med_d/σ_d、med_b/σ_b 双下标；代码事实：detect_cold_pixels :143-145 在 **bias 帧**上算 med/mad ⇒ ALG-CAL 字面判据一侧必错
- 判据：两篇 ALG 对同一实现的冷分支判据仍互斥（按 ALG-CAL 字面冷阈基于 dark 电平，冷像素永不被检出），处置建议「以代码符号回写一篇」未执行。

### M7-E-202 · **STILL**（主实例原样，同族站点行号已非自引形）
- 命令：`grep -n 'REJECTION.md:1[0-9]' docs/science/REJECTION.md`（:32 一处）；`sed -n '16p;18p' docs/science/REJECTION.md`
- 新锚：docs/science/REJECTION.md :32 自引 `docs/science/REJECTION.md:16`，而 :16 现为 `weights[i]` 行、`n`=nominal 定义实际在 :18——错位 2 行未修；报告点名的 :46/:80/:130 行内自引同族站点现无 `(:N)` 形（行号再漂/形制变更），主实例仍红
- 判据：绝对行号自引在插入后指错行的机制原样；DOC-LINE-ANCHORS 门不覆盖 SCI 文档内部自引（无新增防护证据）。

### M7-I-203 · **STILL**
- 命令：`grep -n 'ADU/tile' docs/contracts/DATA_SEMANTICS.md`；`sed -n '1656,1663p' docs/contracts/DATA_SEMANTICS.md`
- 新锚：§24.4 现 :1656 起，「UNIT=ADU/tile 口径透传」在 :1659（报告复核时点 :1592 → **+67 漂移**，§30 扩写所致）；coverage 段所引 P2CoverageResult 全整数字段（union_cells/order 计数，:897 亦登记 uint64 域），无任何 ADU 量纲通道
- 判据：给全整数的 coverage 结果挂「ADU/tile」单位口径的文字错误原样，且仍自称「唯一权威=DATA-COV-001 §19」透传。
### M8-F-003 · **FIXED**（基本修全，留一处同类残余 + 锁名订正建议）
- 修在哪：`tests/cli/test_cli_single_install.py::TestCliSingleInstall.setUpClass` :35-39 —— 缺前置产物由 SkipTest 改 `raise AssertionError`（注释直引 M8-F-003「该门恒 SKIP = 未执行」）；四处 `self.skipTest("install 失败")` 全改 `self.fail`（:64/:81/:90/:100）；`ci/steps/linux_build_root_graph.sh` :25-33 补 install 载荷目标（:31 cmake --build … --target astrocs_runtime astrocs_io … astrocs_p1_hips_writer）+:33 复制 libastrocs_runtime.so；采集面 ci/checks.json::UT-CLI profiles=[linux-main] waivable=False。台账记 adaeb531。
- 是否修全：原报「前置产物从不被产出 + 失败降级 skip」两机制均复算消失（产物面/判据面逐站点核对 4/4）。**残余（同类他站）**：:16 类级 `@unittest.skipUnless(shutil.which("cmake") and shutil.which("g++"))` 仍可整类静默 SKIP（与簇 1「0 用例记 PASS」同型，属他条域）。**建议标记**：账本 regression_test 记的 `ci_missing_prereq_fails` 全仓零命中，真名 :58-:100 `test_01_install_succeeds`..`test_05_install_tree_only_bin_and_astroc_data`（verified_state 已记，维持）。

### M8-F-011 · **STILL**
- 命令：`ls tests/realdata/`；python3 读 ci/checks.json 全量 grep 'realdata'（0 项）；UT-* discover 目录清单（17 个，无 realdata）；`grep 'class\|def test_' tests/realdata/*.py`（15+21=36 用例，pytest 类）
- 新锚：tests/realdata 实测 __init__.py+test_index_v12.py+test_match_plan.py 三文件（报告锚「目录实测 3 文件」零漂移）；`ci/checks.json::UT-*` 无 tests/realdata；DEEP-COV-PY profiles=[linux-deep]、waivable=true（:?同报告）；ci_coverage_runner.py grep 'realdata|discover' 零命中 ⇒ 连「可豁免面」都未显式含 realdata（仅靠 changed_paths=tests/** 触发覆盖统计）
- 判据：36 用例仍不在任何阻断 profile；引用面仍只有 tools/realdata/README/REVIEW（:1 标题+命令面原样）。形态侧：fixture 读 testdata/index.json 磁盘对账（:15-27），不接真实 Gaia 运行面——「真实数据验证」名不副实原样。

### M8a-C-002 · **STILL**（五源互斥逐站复现）
- 命令：`sed -n '7,10p;19p;23p' lib/cosmetic/README.md`；`grep -n 'SHARED' lib/cosmetic/CMakeLists.txt`（:50）；`sed -n '24,28p' lib/hips/README.md`；`grep -n 'SHARED' lib/hips/CMakeLists.txt`（:43）；`grep -n 'p1_hips_writer\|p1_cosmetic' cmake/install_layout.cmake packaging/astrocs.product.json`
- 新锚：cosmetic README :9-10「仅合同文件、无源码」/:19「DLL…尚未存在」vs 实存 `astrocs_p1_cosmetic SHARED`（lib/cosmetic/CMakeLists.txt:50）、install_layout.cmake:105 交付、product.json:15 status=IMPLEMENTED；hips README:24「全仓库无 astrocs_p1_hips_writer 目标」vs lib/hips/CMakeLists.txt:43 同名 SHARED 目标 + install_layout:105 + product.json:16 IMPLEMENTED；calibration README :159-163「entrypoint=MISSING」与 module.yaml 面仍未同步
- 判据：README↔CMake↔install↔product↔registry 五源对「源码有无/DLL 建没建/entrypoint 接没接」仍互斥；无一站订正。

### M8a-E-004 · **STILL**
- 命令：`sed -n '7,8p' lib/cosmetic/README.md`；`sed -n '171,174p' lib/hips/README.md`；`grep -n 'add_library(astrocs_aio\|astrocs_hips\|astrocs_calibration' CMakeLists.txt`（:322/:351/:373）；`ls docs/architecture/cpu/ARCH_CONTRACTS.md`（不存在）；`ls tests/hiss_write_probe.cpp tests/bench_write.cpp`（均不存在）
- 新锚：cosmetic README :7-8 签名头无解析前缀「`include/astro_calibration.h`」按 lib/cosmetic/include/ 解析落空（真身在 lib/calibration/include/）；其 CMake 行锚 :321-333 落在 astrocs_aio 块（astrocs_calibration 实为 :373）——邻近目标错锚原样；hips README :172 引 tests/hiss_write_probe.cpp、:173 bench_write.cpp 均灭失（p1_hips_writer_test.cpp 存在）；phase3_proj README:38 与 phase3_rsmp README:30 的 ARCH 行仍指 docs/architecture/cpu/ARCH_CONTRACTS.md（不存在）
- 判据：报告点名五站（现状段/测试引用/构建锚/两条 ARCH 行）全部仍红。

### M8a-G-008 · **STILL**
- 命令：`sed -n '8p;16,20p' docs/README-DOCS.md`；`sed -n '423,429p' ASTROCS_PROJECT_CONSTITUTION.md`；`sed -n '3p' docs/standards/DOCUMENTATION_STANDARD.md`
- 新锚：README-DOCS.md :8 把「L2 算法规范 docs/algorithms/*.md」定为 L2，而宪章 §12.2（现 :423-426）明定 L1 含 SCI/**ALG**/DATA/ARCH/API、L2=模块 README/manifest/函数合同；:16「权威链：Wiki(核心约束) → …」+:20「矛盾以 Wiki 为准」——把仓外 AstroCS.wiki（.gitignore 用户资料区，禁改禁删）置于权威链首位，宪章 §1.1（:17-28 权威分层：宪章最高）与 §1.2 被架空；DOCUMENTATION_STANDARD.md:3 让渡行原样（:3 零漂移）
- 判据：两腿（改写 L2 定义 + 仓外 Wiki 顶格权威链）均原样；无机器门比对 README-DOCS 与宪章分层（AGENTS-GOV 只管 AGENTS.md）。
### M9-A-1 · **STILL**（面积站注释修复未收口到引擎站，偏差仍零登记）
- 命令：`grep -rn 'THRESH_60ARCSEC\|cos_thresh_60\|use_adaptive' lib/healpix_db/healpix_drizzle/*.cpp`；`sed -n '159p' spherical_overlap.cpp`；`grep -n '60"\|60 角秒\|双路径' docs/algorithms/DRIZZLE_GEOMETRY.md`（0）；`grep -o 'DISP-DRZ-00[0-9]' DRIZZLE_GEOMETRY.md | sort -u`（001..008，无 60″ 项）
- 新锚：面积路径自证注释现 spherical_overlap.cpp:159「双路径切换阈值 60″ 无数学依据…Girard 失效」（原样，其路径已统一 fan 剖分）；引擎站 `DrizzleRunContext::cos_thresh_60` 现 drizzle_engine.cpp:244（报告 :243 → +1），`THRESH_60ARCSEC` 现 :1770-1774（报告 :1652 → **+118 漂移**，0765064c 合并流水线改造），`use_adaptive` 判据 `d <= rctx.cos_thresh_60` 现 :1385-1396 活性路径；DRIZZLE_GEOMETRY.md 对 60″ 阈值去向**仍零记述**；偏差表 DISP-DRZ-001..008 无该阈值项
- 判据：「修复未固化」三要件全中——判据未收口（引擎仍按无数学依据的 60″ 选路）、文档未登记去向、DISP 偏差未挂；文件虽在 changed 清单，站点仅行号漂移，机制复算未变。

### M9-D-2 · **STILL**
- 命令：`grep -n 'radius_deg\|match_radius_arcsec' lib/gaia_xpsd_client/src/gaia_client.h`（:88/:94/:105/:126 + :116）；`sed -n '46,55p' gaia_client.h`；`sed -n '1602,1606p' gaia_client.c`；`sed -n '17,18p' lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/DR3SP_SCHEMA_AUDIT.md`；`sed -n '75,80p' docs/algorithms/GAIA_QUERY.md`
- 新锚：双口径并存原样（radius_deg ×4 与 match_radius_arcsec ×1，内部 :2448 `radius_deg = match_radius_arcsec/3600.0` 换算无合同声明）；GaiaStar :46-55 九裸字段（ra/dec/magG/magBP/magRP/parallax/pmra/pmdec/source_id）零单位注释原样；解码块 inv_scale/inv_dra 现 gaia_client.c:1604-1605（.h/.c 均在 changed 清单，该块仍无 µas/LSB 单位回写——文档侧 GAIA_QUERY :75-80 已核出 10 µas/LSB 并证伪历史 7.2，代码注释未同步）；审计表「比例 1/(3600·1000·500)」列名仍以「比例」承载 deg/LSB 倒数，:17-18 原样
- 判据：四站逐一复现，注释回写义务（§12.3-3）未执行。

### M9-G-6 · **STILL**
- 命令：`sed -n '335,342p' lib/astro_image_io/src/hips/aio_hips_writer.cpp`；`grep -n 'write_properties(' aio_hips_writer.cpp`（:1023/:1242 两处直写正式路径）；`grep -n 'atomic_replace' lib/astro_image_io/src/hiss_stream_writer.cpp`（:178 正对照在位）；`sed -n '313p' docs/algorithms/HIPS_WRITER.md`
- 新锚：write_properties 仍 `void`（:335），`if (!f) return;` 吞失败、无 tmp+rename/fsync；aio_hips_finalize（:1361）有返回码但该写段不可传播；DISP-HIPS-004 登记文本（HIPS_WRITER.md:313，STANDARDS_REGISTRY :89/:109）仍自陈「properties/manifest 直写…writer 未采用 atomic_replace」——报告时即存在，非修复证据；IO_003 合同 :34 与 HIPS_WRITER :26 的「承接」边界未改
- 判据：HiPS properties（metadata 唯一载体）写失败静默 + 非原子发布两机制原样；正对照（HISS .partial/atomic_replace）仍未被采用。
### V1-N-03 · **STILL**
- 命令：`sed -n '462,467p' lib/snr_estimator/cpp/include/snr_estimator.h`；`grep -n 'snr_phot = 1.0' lib/snr_estimator/cpp/src/snr_estimator.cpp`（:146/:318）；`sed -n '638,641p' snr_estimator.cpp`
- 新锚：头文件 :463「全局尺度与归一化基准均置 1.0」与 :466-467 字段注「snr_phot/median_snr = median(SNR_F)」同文件两值（报告 :463/:465-466 → +1~2 微漂）；实现 extract 路径 :638-641 双置 median，snr_estimate/snr_estimate_f64 路径 :146/:318 置 1.0 ⇒ 同名两值逐站坐实
- 判据：接口文本自斥 + 实现按路径二义原样；无门比对头注释语义与实现赋值。

### V3-N-03 · **STILL**
- 命令：`sed -n '245p;1065p' lib/phase2/src/upm.cpp`；`grep -n 'control_grid_per_tile' lib/core/src/module_adapters.cpp lib/phase2/src/stage2_common.cpp`
- 新锚：upm.cpp :245 `if (cfg.grid != 8) return 3`（报告 :245 零漂移）与 :1065 open 校验（报告 :1061-1065 区间内）；**缺键即 8 的读侧**在 module_adapters :3738 `uc.grid = smp_doc.value("control_grid_per_tile", 8)`（写侧 :3644 必写该键；旧版工件无键 ⇒ 默认 8 恰好通过 !=8 门）
- 判据：「写侧必写、读侧必校验、不等即 rc=3」三件中读侧对缺键走默认而非拒 ⇒ 跨版本静默错位镜像原样；报告自注「潜在（非当前）」亦原样。

### V7-N-01 · **STILL**
- 命令：`sed -n '1125,1141p' lib/core/src/module_adapters.cpp`（p1_flag/p1_num/p1_int）；`sed -n '1523,1536p' module_adapters.cpp`；`sed -n '4857p' module_adapters.cpp`；`grep -n 'method\|hot_sigma' lib/cosmetic/src/module_entry.cpp`（:487-489 必填、词表 PARAM/103）；`grep -rn '1e999' tests/unit/calibration_integration_test.cpp`（B3 只测 DLL 侧）
- 新锚：IR 侧静默兜底原样：p1_num `!is_number→dflt`（:1134-1137）使 hot/cold 缺键/错型→5.0、`1e999→inf` 直穿 ac_correct_frame；p1_flag 字符串落 dflt=true ⇒ `enabled:"false"` 仍按真值执行（:1523）；method 任意值→MEDIAN（:1534-1535）；validate 仍只查 is_object（:4812→现 :4857，+45）；mss `p1_int(...,4)` :1536（0→全结构清除链未变，cosmetic_corrector.cpp:242/247 门未加严）；DLL 侧严格度未降（module_entry.cpp:487-489/词表 103）
- 判据：两通道值域几乎不相交 + IR 全静默 + stage 无生效参数判别位 + IR 分叉零覆盖，四腿全数复算成立。

### V7-N-09 · **STILL（负清单仍有效——本条非缺陷，是「不得重报」登记）**
- 命令：`grep -n 'signal_dtype' lib/astro_image_io/src/hiss_common.cpp`（:197 恒写）；`grep -n 'precision_mode' lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp`（:968-974 0/1 收口）；`ls lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_taskset_invariance.sh`（在）；`grep -n 'nside' module_adapters.cpp` 三态记账站点
- 判据：抽查 4/9 负判项（precision_mode 双拒、HISS signal_dtype 恒写、nside 三态记账、drizzle FP 归约唯一站点已带锁）在现行树全部仍为 fail-closed/已修形态 ⇒ 负清单内容未被推翻。建议账本处理：该条四态本不适用（非缺陷），建议标 NOT_A_DEFECT 并保留「免重报」语义，勿计入缺陷统计。

### V9-N-01 · **STILL**（未回滚、亦未凑词——门仍红态由 known-failures 基线承担）
- 命令：`git log --oneline -1 c3452d48 -- docs/TRACEABILITY.csv`（在历史）；`grep -c 'SCI-PSF-001\|SCI-REJ-001\|SCI-ACR-EQUIV-001' docs/TRACEABILITY.csv`（工作树=0）；`git show HEAD:docs/TRACEABILITY.csv | grep -c ...`（HEAD=0）；CON-TRACEABILITY 门定义 ci/checks.json :981-1002
- 新锚：三行冻结科学门仍缺位（净删 4 行未回滚，两向复核 0 命中）；现行 CON-TRACEABILITY 跑 `tools/quality/contracts/check_traceability.py`，判据仍是「读 docs/TRACEABILITY.csv + 关键词/唯一性」子串面（ID 集对账无载体）；ci/known_failures.json 未见为 CON-TRACEABILITY 开条目（基线机制在场，但「删除即永久红、修门最省力=塞词」的组合未解）
- 判据：机制三件套（净删无检测 / 门按子串 / 无 ID 承载对账）原样；工单「回滚三行」未执行。

### V9-N-10 · **STILL**
- 命令：`grep -n 'SKIP_EXIT_CODE\|V_SKIP_WAIVABLE' ci/run.py`（:92/:82）；`sed -n '900,908p' ci/run.py`；`grep -n 'UT-CPU-AVX512' ci/checks.json`（:1863）
- 新锚：run.py 现 :901 `elif returncode == SKIP_EXIT_CODE and not timed_out:` → `verdict = V_SKIP_WAIVABLE`（=字符串 "SKIPPED(waivable)" :82），**不读该门 waivable 字段**（报告锚 :898 → +3）；UT-CPU-AVX512 仍登记 `waivable=false`（:1863）⇒ 非豁免门仍可 exit 77 隐身；1036-1066 区总判定措辞未改
- 判据：机制②新通道原样：77 语义对 waivable=false 门同样生效，且计入 skipped_waivable 计数。

### V10-N-03 · **STILL**（同函数其它 atoi 已收口，itemSize 独漏）
- 命令：`sed -n '1266,1269p' lib/gaia_xpsd_client/src/gaia_client.c`；`sed -n '1069,1078p;1119,1120p' gaia_client.c`；`grep -n 'parse_bounded_int' gaia_client.c`（定义 :1163，仅 :1284 一处消费）
- 新锚：`xf->item_size = item_sz[0] ? atoi(item_sz) : 0` 现 :1269（报告 :1213 → +56 漂移，.c 在 changed 清单）；byte_unshuffle :1069-1071 `item_size<=1 → return 0`；消费点 :1119/:1139 `if (use_byte_shuffle && item_size>1)` 跳过——三 fail-open（缺省跳置换 / n==0 跳 / 不整除半置换）全 rc=0 原样；magnitudeRange/spectrumCount 已改 parse_bounded_int+警告/整文件拒（:1228/:1284 区）恰证「同段两种严格度」仍未闭合
- 判据：报告「界内性成立无越界」的限定仍真；损坏静默分支留。gaia_unshuffle_test.c 边级锁未见新增。

### V11-N-02 · **STILL**
- 命令：python3 解析五镜像 `_fields_`（全部 7 字段、无 var_num_sum）；`sed -n '69,81p' lib/astro_image_io/include/aio_hips.h`（C 侧 8 字段）
- 新锚：真源 AstroSphereTileView 现 aio_hips.h :69-81（8 字段含 `const void* var_num_sum`，sizeof=56）；五镜像 `hips_direct_smoke.py`/`v5_snr_precision_roundtrip.py`/`v5_maptile_oracle.py`/`hips_mapping_oracle.py`（lib/astro_image_io/tests/）与 `gen_hips_browser_test.py`（lib/healpix_db/healpix_browser_qt/tests/）**全部仍 7 字段/48B，零同步**（报告锚行号未记，无漂移问题）
- 判据：无一份镜像可作参照物原样；「当前无越界路径」限定仍真（调用点未把 variance 产品需求路由给这些镜像），但 Python 侧 variance/ivar 无处可写仍成立。

### V11-N-10 · **STILL**
- 命令：`grep -rn 'hips_direct_smoke|v5_maptile_oracle|hips_mapping_oracle|gen_hips_browser' ci/ .github/ tests/unit/CMakeLists.txt --include=* 2>/dev/null`（全 0 命中）；`grep -rn 'AstroSphereTileView' tools/ ci/`（0）
- 判据：五镜像至今**无任何门采集**（不在 UT discover 面、不在 ctest 注册、无 ctypes 布局对账工具）⇒ 「镜像同步↔门采集」100% 对应关系未被打破：零采集 ⇒ 必漂（V11-N-02 即实例）。机制/统计结论原样。

### V12-N-08 · **STILL**（报告锚全数零漂移）
- 命令：`sed -n '96,98p;108,111p;184,187p;82p;342p' cli/resource_gate.h`；`sed -n '451,456p' tools/monitoring/run_monitored.py`；`sed -n '373,375p' ASTROCS_PROJECT_CONSTITUTION.md`；`grep -c '90%' ASTROCS_PROJECT_CONSTITUTION.md`（=0）；`sed -n '179,180p' tests/unit/mon001_gate_test.cpp`；`sed -n '110p' ci/ci_repair_round.py`
- 新锚：C++ 常数八位 :96/:97(90.0)/:98/:108/:109(0.70)/:110/:111/:184 全部零漂移；Python :451-455 六常数值集合不同（无 90.0/0.70/p50 项）；(c) 边界开闭：run_monitored :452「严格 >10s」vs resource_gate :185-187 `>= kMon002MinWindowSeconds`——**恰 10.000s 两实现结论相反**仍成立；(b) 名值不符：`UtilizationP75Low`→"utilization_p75_low"（:82）实际判据「≥70% 样本 ≥0.85」（:342 区 `util_samples_pass_frac < kMon001UtilSampleFrac`），被 mon001_gate_test :179-180 钉成合同字面量、ci_repair_round.py:110 用作归因串；宪章冻结文本 :374-375 仍只四条，90%/0.70 零出处
- 判据：三腿（两套判据集合不同 / 边界开闭相反 / 非冻结值当门）+ 正例（可复算反例 wall=12s 均值86% p50=88% frac0.65：C++ FAIL、Python PASS）均可从现行文本重推 ⇒ STILL。

### V12-N-16 · **STILL（范围须缩：仅 ln10 腿可证）**
- 命令：`grep -rn 'kLn10' lib/ --include=*.cpp`（**两处**：snr_estimator/cpp/src/snr_science.cpp:33、noise_model.cpp:34，独立字面量 2.302585092994045684/04568401799…精度还不同）；`grep -rn '3\.16227766\|sqrt(10' lib tools cli tests scripts`（0）；`git grep 'MAG_PER_LN10' 521095b8`（0）
- 判据：ln10 两处独立硬编码腿成立（且两版位数不一致，正是「派生优于复制」未推广的实例）；**sqrt(10) 两站与正例 MAG_PER_LN10 在工作树/HEAD/base 三向复核全零命中**——该腿宿主不可定位（疑指向未入库影子稿），建议账本把本条描述收窄为「kLn10 两处复制（snr_science/noise_model）」，否则半条无法复算。

### V13-N-07 · **STILL**
- 命令：`sed -n '54p' contracts/schemas/traceability_matrix.schema.json`；`grep -rn '编号生命周期\|先登记后使用' docs/standards/ docs/traceability/`（0）；`grep -c SUPERSEDED docs/traceability/TRACEABILITY_MATRIX.json`（=0）；CSV :11-12 注记两行仍在
- 新锚：schema 对 evidence_id 仅 `^EVID-…$` 格式正则（:54），无承载要求；SPEC/standards 仍无「编号先登记后使用」「EVID 须对应证据件」条文 ⇒ 新造未登记制度上无可违之规；SUPERSEDED 自毁细节复算成立：声明宿主在派生 CSV（TRACEABILITY_MATRIX.csv:11/:12 notes），JSON 权威侧零该字样，`gen_traceability_csv.py`（零 SUPERSEDED 字面量）重跑即蒸发
- 判据：制度缺口 + 自毁两腿原样；C-18 编号对账门仍未出现。

### V18-N-01 · **STILL**
- 命令：`sed -n '48,58p' lib/calibration/tests/p1cal/p1cal_test_main.hpp`；`sed -n '55,56p;70,72p' lib/star_detector/tests/p1star/p1star_test_main.hpp`；`grep -rn 'fault_reported' lib/*/tests/*/ --include=*.hpp | head`
- 新锚：三种 cond 语义并存复算：①「不判即翻」——p1cal 宏 :50-58 注入分支排 `!(cond)` 之前（短路，cond 不求值），10 套同族宏文件在位（p1cal/p1drz/p1psf/…）；②「判后翻」——p1star :55-56 `if (fault_hit(name)) ok = !ok`（宏 :70/:72 原样）；③正对照 P2H_CHECK（p2hips 使用面在）；归因失真机制原样：宏首支 `if (cs.fault_reported && faultname) ++failures`（p1cal:50-52 注释自述「后续 CHECK 全部计为失败」），p1drz/p1psf 无此支 ⇒ 同名注入不同家族 failures 不等
- 判据：零一致性门未出现（无任何 CI 项比对 12 套宏语义）；C-21 判据②在现状仍不成立。

### V19-N-01 · **STILL**（P0）
- 命令：`grep -rn 'gtest_discover_tests' --include=CMakeLists.txt . | grep -v run/build | wc -l`（=**40**，与报告计数逐字相符）；`sed -n '65,66p;178p' tools/quality/check_ctest_registration.py`
- 新锚：CI-REG-002 校验器 `ADD_TEST_NAME_RE = re.compile("add_test\s*\(\s*NAME\s+...")`（:65）——目标集只从 CMake 文本的 add_test(NAME) 铸造；40 处 gtest_discover_tests 不产 add_test 字面量 ⇒ 双零覆盖（checks.json 面与 ctest_baseline 冻结面）原样；C4（:178「ctest_targets 模式匹配不到任何现存目标→FAIL(陈旧注册)」）使「为 discover 目标补声明」反被判陈旧 ⇒ 唯一涉及它们的门被自身规则锁死
- 判据：三条事实（40 计数、正则闭包、C4 反向）逐字复现，无新增运行期枚举（ctest -N）接入证据。

## 统计与订正建议（供前台统一回写；本档不落账本）

- **总数 62 = STILL 61 / FIXED 1 / MOVED 0 / CANNOT_STATIC 0**（FIXED 唯一一条：M8-F-003，修于 adaeb531，本复核复算其机制两腿均消失；残余：类级 skipUnless(cmake/g++) 静默面属他条域）。
- **建议账本订正标记**：
  1. V7-N-09：非缺陷（判否负清单），建议状态 NOT_A_DEFECT，保留免重报语义，勿计缺陷数。
  2. V12-N-16：建议描述收窄为「kLn10 两处独立复制（snr_science.cpp:33 / noise_model.cpp:34）」；sqrt(10) 两站与正例 MAG_PER_LN10 三向复核（工作树/HEAD/base）零命中，宿主不可定位。
  3. M5b-E-05：AGENTS.md 映射表子面（:14/:16/:28 双向不一致）不立——base 时已 10/10 对齐（b40c8a49）；主面（宪章 §H/§F.1 字母节冒充）保留 STILL；RELEASE_STANDARD.md:19 站点灭失（文件现 12 行）。
  4. M2a-C-6：configs/stage1.template.json 站点灭失注记（从未入库，疑报告时引用未跟踪文件）；主链仍 STILL。
  5. M3b-I-01：加「θ 守卫子项已由 SDET-ANGLE-001（P11）修复」注；簇整体仍 STILL。
  6. M8-F-003：regression_test 锁名 ci_missing_prereq_fails → 真名 test_01_install_succeeds..test_05_install_tree_only_bin_and_astroc_data（tests/cli/test_cli_single_install.py :58-:100）。
- **行号漂移热点**（报告锚→新锚）：M1a-A-005 ipv_types.h +14；M1a-C-003 module_adapters +361~363；M2a-C-6 p1_op_drizzle +734；M3-A-002 stage2.cpp +5/+18；M9-A-1 drizzle_engine.cpp +118；V10-N-03 gaia_client.c +56；M7-I-203 DATA_SEMANTICS +67；V9-N-10 run.py +3；M5b-I-06/M4 系列多数零漂移（纯文档锚稳定）。
- **时点纪律**：HEAD a3a343a4 + 工作树；涉未入库 WIP 的站点（lib/snr_estimator/src/**、lib/phase1/tests/**、tests/unit/p1_noise/**、docs/TRACEABILITY.csv 脏改）已在对应条目注明。
