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
