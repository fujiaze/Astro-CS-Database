# RC1 复核档案 · recheck_round1 verify[0::8]（62 条）

- 时点：HEAD `a3a343a4`（工作树另含未入库 WIP：`lib/snr_estimator/{src,include,CMakeLists.txt}`、`lib/phase1/tests/`、`tests/unit/p1_noise/`、`lib/plate_solve/cpp/ipv/test/ipv_dead_params_lock.py`、脏改 `docs/TRACEABILITY.csv` 等——涉及时单独注明）
- 方法：一律按 文件::符号 重新定位（glob/grep/read + 只读 python3 统计 + git --no-optional-locks 只读），复算原缺陷机制是否消失；缺失判定走三级复核（①工作树 ②HEAD git ls-files/show ③改名/移动检索）
- 分片：verify[0::8]（i=0），共 62 条；不触碰他人区间
- 四态：STILL（缺陷仍在，给新锚）/ FIXED（给修在哪+是否修全）/ MOVED（位置变缺陷原样）/ CANNOT_STATIC（需运行期）
- 本档只输出建议标记，不回写账本（账本由前台统一处理）

## 结论总表（收工时回填）

| # | ID | 四态 | 新锚（文件::符号） | 一句话判据 |
|---|----|------|--------------------|------------|

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