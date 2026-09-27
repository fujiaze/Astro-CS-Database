# 检查-S27 · lib/algorithms noise_snr＋psf＋shared 对抗性静态复查

> 检查人：独立静态检查子代理（只读）｜基准：当前工作树（未提交状态，零 git 写）
> 产出：本文件（本轮唯一写入）；不构建、不测试、不改任何源文件。
> 先读 `独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表：上轮已订正验证通过项不重报；科学口径以 `分歧台账.md` D 系终裁（D-01…D-11）与 A-* 裁决为准，不翻案。
> 判面四选一：①科学性（对数 D 系终裁；常数/公式/单位/量纲；判据非退化）②行文逻辑 ③跨文档冲突（权威链五方兼容）④幻觉与锚（file:line 实开核对；「文档说有、代码没接」）。
> 纪律：涉及科学公式／默认容差／冻结定义者只红级上呈、不给越权改法；第三方代码只查对接。

## 0. 结论汇总

| 严重级 | 条数 | 编号 |
|---|---|---|
| 红（必须改） | 3 | S27-红-1 ~ 红-3 |
| 黄（应改） | 21 | S27-黄-1 ~ 黄-21 |
| 绿（可不改，登记） | 10 | S27-绿-1 ~ 绿-10 |
| 前轮已报未修（不计数） | 1 | §7 |
| shared 域已登记项（实读现状，不计数） | 9 | §6 REG-1 ~ REG-9 |

三行摘要：① 红级 3 件——noise_model.cpp:1057 注释仍写 D-04 已否决的 1.144 旧口径（终裁 1.152/复合 1.44，交接 §10 指定的代码修复单候选）；D-05「idw_power 默认 1.0＋配置化」终裁未落地（snr_estimator 三入口写死 2.0、eng/packaging 无 idw_power 键、07_noise_snr.md:195「按配置解析」为死键，生产链 snr_model→SnrEvaluator 实际生效 2.0）；psf README:179-180＋module.yaml:19-20 把 Makefile 中**已按 ISA-001 移除**的 -march=native 登记为现状构建（AGENTS §6 违禁旗标两说）。② 黄级 21 件主体是锚面系统性漂移：NOISE_ESTIMATION §13.1（声明 938 行实 1482、CMake 三锚含不存在的 astrocs_p1_session）、NOISE_MODEL 四锚、noise_snr README（475 vs 1482、wrapper_phase1/noise_model.cpp 已不存在、测试路径 eng/tests/* 不存在）、PSF.md/STAR_PSF_ALGORITHMS（dpsf 锚 9/14 失效）、psf README/module.yaml r2 后 60+ 锚、双 module.yaml descriptor 锚、defaults.json source_ref 8/9 行漂且无门覆盖；oracle 复现链断裂 3 处（snr_oracle/conf1_oracle 随 run/ 回收、healpix 双参照脚本与 fullsky 生成器缺位）。③ 非锚内容性黄条：DISP-NOISE-001「无锁」与 PERF-P1 加锁现状反向、README psf 端口列序/invalid-NaN 两说＋漏 wcs 必输入口、flux 括注 α² 差 2×（生产数值无错）、拟合窗 3.7172σ 与 fitRadius=8px 无接线、dpsf_image 1.4826 float 后缀、shared Makefile 三处自述矛盾、dpsf 日志写死源码树（283 MB 残留未入库）；判据非退化三域全过（p1noise 4 相位注入、p1psf 双向注入＋prodpath 必红预设、shared 四测试负例齐、oracle 独立实现）。

---

## 1. 红级记录

### S27-红-1 ｜红｜面①（科学性/常数口径对 D-04 终裁，交接 §10 指定遗留）
- **文件:行**：`lib/algorithms/noise_snr/cpp/src/noise_model.cpp:1057`
- **问题**：天空预算判据注释仍写旧口径 `SE(σ̂)/σ ≈ 1.144/√N_sky (MAD 路径) ≤ 1.5% ⇒ N_sky ≥ 9216`——D-04 已终裁 `c(n=64)` 取 **1.152**、1.144 支为算术漂移；现行正本口径为复合常数 **1.44 = 1.152×1.2533**（9216 = (1.44/0.015)²）。注释中 1.144 自身推导 (1.144/0.015)² ≈ 5816.6 与同一句结论 9216 不自洽，属新旧两说并存于同一注释行。
- **证据**：
  - 分歧台账 D-04（`独立审计/实验重做/总编对账/分歧台账.md:42-51`）：「1.152 vs 1.144…**终裁取 1.152**」；摘要表 :218「值已终裁 1.152」。
  - 文档侧已换轨：`docs/science/NOISE_MODEL.md:99`「单 patch 相对误差 c ≈ **1.152**/√N，中位数效率 1.25 ⇒ SE ≈ 1.44/√N_sky」；`:301` 订正注「D-04 原 1.144/√N_sky 用了被终裁否定的 1.144 支系数…c(n=64) 终裁 1.152，复合口径 1.44」。
  - 代码侧同文件其他两处已用新口径：`noise_model.cpp:310`（fill_impl 权重注释）与 `:1328`（fill_impl 内）均为「SE(σ̂)/σ ≈ 1.44/√N」；全责任域 grep `1.144` 仅 `:1057` 一处残留。
  - 上轮已点名未修：`检查-科学性.md:58`（G-2）与 `检查-跨文档冲突.md:76` 均登记该处「注释随下次代码整改轮更新」，至今现状未变（PASS 表只验证文档侧，未含代码注释）。
- **建议改法**：登记为代码修复单候选——注释订正为 1.152 口径或按台账「1.44 = 1.152×1.2533 复合口径」表述（数值 9216 不受影响）；本切片只读不改。
- **所属面**：①（兼②）

### S27-红-2 ｜红｜面①＋④（D-05 终裁未落地＋配置死键；只登记上呈，不给越权改法）
- **文件:行**：`lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:593`、`:730`、`:833`（三处 `out_model->idw_power = 2.0;`，分别位于 snr_extract_model / _v2 / _v3 初始化）；`lib/algorithms/noise_snr/cpp/include/snr_estimator.h:588`（注释「IDW 幂次 (默认 2.0)」）；对照 `docs/plugins/algorithms_phase1/07_noise_snr.md:195`。
- **问题**：分歧台账 D-05 终裁「**默认 idw_power = 1.0**（生产含噪）；idw_power/K 配置化，每次重建在日志输出实测 p*；影响文档：defaults/配置（idw_power 2.0→1.0，负责人已批）」。现状：① 生产提取入口写死 2.0、无任何配置读取；② `eng/packaging/config/` 全目录 grep `idw_power` **零命中**（配置键不存在）；③ 已按终裁改写的 07 文档声称「其 `idw_power` 默认 **1.0**…均按配置解析」——该配置解析链在源码中不存在，属「文档说有、代码没接」的死键。
- **证据**：
  - 台账 D-05 全段（`分歧台账.md:53-63`）；07_noise_snr.md:195 已带「订正: D-05」注记写默认 1.0。
  - 代码链：snr_extract_model 写死 2.0 → orchestrator 落盘 snr_model 块三标量（`orchestrator.cpp:4609/:4622`，日志输出见 `:4508`）→ `hp_drizzle_api.cpp:793` 读回 idw_power → `gradient::SnrEvaluator::buildF64(..., idw_power)`（`lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h:46`「idw_power - IDW 幂次 (默认 2.0)」、`:109` `double idw_power_ = 2.0;`）做 KD-tree IDW 逐像素 SNR 重建（`docs/contracts/DATA_SEMANTICS.md:376` 登记该消费）——即 **2.0 是生产链实际生效值**。
  - 配置面 grep：`grep -rn idw_power eng/`（含 packaging/config、ci、tools、contracts）全部零命中；`grep -rn idw_power docs/` 仅 07:195（说默认 1.0 按配置解析）与 DATA_SEMANTICS:376（字段登记）。
  - 测试 spy 同步 2.0：`orchestrator/tests/gate/gate_module_snr_extract_spy.cpp:41`。
  - **反方核验**：P4 生产默认算子为样条（`lib/algorithms/integration/v6/src/weight_chain.cpp:338` operator_id 词表无 IDW 档，与 D-10/07 §4.5 一致），IDW 是备选口径——但 drizzle 侧 SnrEvaluator IDW 是 snr_model 块的**现行登记消费方式**（DATA_SEMANTICS:376），并非已退役死路径；且 07:195 的「按配置解析」无论走哪条链均无接线。故两说中任一解释下，「文档默认 1.0＋配置化」与「代码默认 2.0＋无配置键」至少一处冲突成立。
- **建议改法**：只登记上呈（涉及默认值/冻结裁决落地面）：请负责人确认 D-05 的「idw_power 2.0→1.0」是否覆盖 snr_model 块→SnrEvaluator 链；若覆盖，按变更流程排期改代码默认＋建配置键（本切片不给具体改法）；若不覆盖，07:195 的「默认 1.0 按配置解析」需明确其适用算子面。
- **所属面**：①（兼③④）

### S27-红-3 ｜红｜面③（psf 域 PSF-1：两份模块登记把已移除的 -march=native 登记为现状）
- **文件:行**：`lib/algorithms/psf/README.md:179-180`、`lib/algorithms/psf/module.yaml:19-20` vs `lib/algorithms/psf/Makefile:1`、`:5`
- **问题**：两份模块级「现状构建」登记声称 Makefile 含 `-march=native`，与 Makefile 自身 ISA-001 条款直接冲突——**把 AGENTS §6 硬禁令（不硬编码线程/ISA）所禁止、且已移除的旗标登记为生产现状**（同一量两说，且方向是「文档说有违禁项、代码已合规」，属跨文档冲突红级）。
- **证据**：
  - README:179-180 原文「（Makefile:3-5，g++ -shared -fopenmp -O2 **-march=native** -std=c++17）」；module.yaml:19-20 原文「现状构建= lib/algorithms/psf/Makefile:3-5（g++ -shared -fopenmp -O2 **-march=native** → dynamic_psf.dll）」。
  - Makefile 实开：`:1`「**ISA-001（TRUTHFUL-CONCLUSION-01）：本文件不得自带宿主 ISA 旗标（原 -march=native 已移除）**」；`:5` `CXXFLAGS = -O2 -Wall -std=c++17 -fopenmp`；全文件 grep march 仅 :1 的「已移除」语境。
  - README:158「-march=native 编译期向量化属构建配置」同源默认该旗标仍在。
  - **反方核验**：Makefile:10 `TARGET=dynamic_psf.dll`、:17 链接行真实，产物名与 -shared/-fopenmp/-lm 两文档所述为真——失实的仅 march 与引侧行号，非整段捏造。
- **建议改法**：README:179-180 与 module.yaml:19-20 删 `-march=native`、补 ISA-001 依据行（登记上呈，本切片不代改）。
- **所属面**：③（兼②）

---

## 2. 黄级记录

### S27-黄-1 ｜黄｜面④（NOISE_ESTIMATION.md §13 锚面系统性漂移）
- **文件:行**：`docs/science/algorithms/NOISE_ESTIMATION.md:129-138`（行数与 CMake 接线声明）、`:140-154`（§13.1 逐符号表全表）、`:161`、`:184-186`、`:196-204`（DISP 表锚）、`:213`、`:215-216`、`:30`、`:144-153` 等。
- **问题**：§13.1 自称「由源码逐符号核对后追加」「现行唯一生产实现逐符号锚」，但锚面已整体漂移：声明 `noise_model.cpp` **938 行**（实 **1482 行**）、`snr_estimator.h` **624 行**（实 **647 行**）；§13.1 表内约 10 组行号锚全部指向旧版行号（漂 300–550 行），逐条实开不符。
- **证据（逐条实开，节选）**：
  - `:144` `snr_noise_model_v1_default_config | noise_model.cpp:696-719` → 实位 **:1243-1264**；:696-719 现为 `SkyBudget/sky_budget` 函数。
  - `:142` `noise_model_impl :347-621` → 实位 **:783-1168**；`:143` 门面 `:746-767` → 实 **:1293-1314**；`:146` `fill_impl :776-866` → 实 **:1323-1408**（fill 门面 :868-883 → 实 **:1412-1426**）；`:147` free `:903-917` → 实 **:1447-1461**；`:148` scale_law `:919-926` → 实 **:1463-1470**；`:149` gain_variance `:928-937` → 实 **:1472-1480**；`:150` phot_cal `:630-657` → 实 **:1177-1200**、psf_fit `:658-695` → 实 **:1205-1238**；`:153` 返回码 `:483-486,566-578` 现为掩膜/统计区。
  - `:30` `noise_model.cpp:1-938 … variance_floor=1e-12（:709）` → 文件 1482 行、default_config 内 floor 字面量在 :1256。
  - `:137-138` CMake 三锚全错：`CMakeLists.txt:767-770`（实为 photometry 源列表段，astrocs_phase1_noise 实位 **:807-810**）；「经 `astrocs_p1_session` PUBLIC 闭包链入主程序（CMakeLists.txt:876）」——**全仓 CMakeLists 中无 astrocs_p1_session 目标**、:876 实为 sdet 注释，实际链入是 astrocs_phase1_noise 直接列于 **:921/:1032** 目标链接表；`eng/tests/unit/CMakeLists.txt:619-623`（p1_noise_test 实注册于 **:722-728**，:619-623 实为 p2hips 段）。
  - `:145` robust_median/collect_patch_sky 锚 `:100-110,113-120,131-171`——前两组相符（文件前部未变），`:131-171` 覆盖 collect_patch_sky（实 :133-176，尾部差 5 行）。
  - `:161` `snr_estimator.h:138、default_config :629`：头锚需核（实 :158 附近 default_config 声明），:629 旧值。
  - `:196` DISP-NOISE-001 锚 `noise_model.cpp:33,372,903`：:33 ✓、:372（注册）→ 实 **:808**、:903（free）→ 实 **:1449**，2/3 失效。
  - `:213` MAD 冻结条款锚 `noise_model.cpp:279` → :279 现为 SCI-VAR-ADAPT 注释，MAD 常数实位 **:119**（:112 注释/:119 字面量）。
  - `:216` 声称 defaults source_ref 指向 NOISE_MODEL :46/:48/:37/:39/:21 → defaults.json 实值为 :54/:56/:52/:47/:23（三方互不对齐，另见黄-6）。
  - **反方核验**：`:145` 前两组锚、`:215` kLn10 锚 `noise_model.cpp:92`（实 :92 ✓）、`:144` 括注内的默认值数值（8×8/10/6/5.0/64/2/1/1e-12/k=0.1/r_min 1.5/0.75/8/9216）与 default_config 实值逐项相符——**数值口径全对，仅行号/行数失真**；`:153` 引用的 rc 语义与代码一致。
- **建议改法**：按现行源码重锚 §13.1 表与行数声明（或声明「行号为 VERSION X 快照」）；CMake 接线段改述实际目标与行号。属订正性重锚，不动任何数值。
- **所属面**：④

### S27-黄-2 ｜黄｜面④（NOISE_MODEL.md 源码锚漂移，上轮仅修 :301 一处）
- **文件:行**：`docs/science/NOISE_MODEL.md:31`、`:55`、`:84`、`:368`
- **问题**：FROZEN 正本的实现锚多处失效（上轮 Y-3e 只订正了 :301 行内锚）。
- **证据（实开对照）**：
  - `:31` `g_model_floor | noise_model.cpp:33,48,55,86,903`——前四个 ✓（:33/:48/:55/:86 实存同内容），**:903 失效**（现为自校准栅栏注释；free 实位 :1447-1461）。
  - `:55` `build 侧 noise_model.cpp:369-371、fill 侧 :818`——:369-371 现为 null_space_basis 注释（build 拒绝实位 **:805-806**）、:818 现为掩膜注释（fill 拒绝实位 **:1362**）。
  - `:84` `与 noise_model.cpp:1-938 一致（fill_impl :776-866；snr_noise_model_v1_fill :868-883）`——实 1482 行、fill_impl :1323-1408、fill :1412-1426。
  - `:368` 导出链 `noise_model.cpp:703-704`→NOISE_ESTIMATION:143→defaults 三段——:703-704 现为 sky_budget 循环行（r0=10/scale=6 实位 :1245-1250 区）。
  - **反方核验**：`:81` `noise_model_science_test.cpp:238-272`（Poisson 交叉验证块）实开相符 ✓；`:26/:62/:95-97` 等公式行不含行号锚；数值口径（9216、1.44、k=0.1、r_min、floor 1e-12）全部与代码相符。
- **建议改法**：四处重锚（内容性订正注格式按仓库惯例）；数值与公式不动。
- **所属面**：④

### S27-黄-3 ｜黄｜面②＋④（noise_snr/README.md 行数声明与构建面/符号锚大面积失实）
- **文件:行**：`lib/algorithms/noise_snr/README.md:6-14`、`:27`、`:88-99`、`:106`、`:116`、`:146-152`、`:241-244`
- **问题**：README 自称「由源码逐符号核对后新建…以现行实现为准」，但：
  1. **行数声明失实**：`:6` noise_model.cpp「**475 行**（2026-09-16 复测）」实 **1482 行**；`:8` snr_estimator.h「**526 行**（复测）」实 **647 行**。
  2. **构建面陈述与实况矛盾**：`:10`「根 CMake 主图只收 `wrapper_phase1/noise_model.cpp` + snr_frame_science.cpp + snr_science.cpp」——**wrapper_phase1/noise_model.cpp 已不存在**（目录下仅 README/snr_frame_science.*），根 CMakeLists.txt:808 实收 `cpp/src/noise_model.cpp`；`:12-13`「因根 CMakeLists.txt:236 的 add_subdirectory(lib/algorithms/noise_snr) 被注释」——该注释行实位 **:320**，且同段 `:9-11` 说「主图只收三文件」与 `:12` 说「不在根 CMake 主图」两句并存、字面互相矛盾（实际是源码已以 astrocs_phase1_noise STATIC 编入并链入主程序 :921/:1032，仅 Makefile/dll 通道未入 CMake）。
  3. **七导出符号行号锚全失效**（`:88-92`）：头锚（:143-149/:151-157/:115/:172-174/:177/:183-184/:188-190）与实现锚（:348-356/:357-365/:333-345/:463-470/:472-486/:488-495/:497-505）均为旧版行号——实位头 :214/:223/:158/:252…、实现 :1293 起各门面。
  4. **返回码锚**（:96-99 的 :238/:266/:225-232/:348-365/:249-253）、**default_config 锚**（:106 的 :371-384 → 实 :1243-1264）、**钳位锚**（:116 的 :166-167 → 实 :813-817）、**注册表锚**（:146-152 的 :32/build :126/fill :402-405/free :433 → 实 :33/:808/:1362/:1449）全部失效。
  5. **测试路径不存在**：`:241` `eng/tests/p1noise/p1snr_science_test.cpp`——`eng/tests/p1noise` 目录不存在，实位 `lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp`。
  6. **oracle 引用已消失**：`:243-244` 「独立 NumPy oracle：run/perf-fix/P5-snr/harness/snr_oracle.py + snr_oracle_probe.cpp」——全仓 find 零命中（见黄-5）。
- **证据**：上述行号均来自本轮实开（read/grep/sed/wc/ls 逐一核对）；上轮三份检查报告 grep `noise_snr/README` 零命中（未点名过）。
- **建议改法**：README 头部与 §5/§6 锚整批按现行源码重锚、行数声明改实测或删去行数；构建面段改述「astrocs_phase1_noise STATIC 已编入根 CMake（:807-810，链入 :921/:1032）＋Makefile/dll 旧通道并存」。
- **所属面**：②（自相矛盾句）＋④

### S27-黄-4 ｜黄｜面③（DISP-NOISE-001「无锁」登记与代码 PERF-P1 加锁现状反向）
- **文件:行**：文档侧 `docs/science/algorithms/NOISE_ESTIMATION.md:76`（§5c「锁自由设计」）、`:196`（DISP-NOISE-001「多线程并发 build/free 对该 map 无锁竞争」）、`:191`「均为现行实现事实」；`lib/algorithms/noise_snr/module.yaml:22-23`（「唯一共享可变状态 g_model_floor 进程级无锁 unordered_map」）、`:108-109`（notes 001「并发无锁」）；`README.md:146-152` 同段。代码侧 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:38-45`。
- **问题**：noise_model.cpp 已由 PERF-P1（RELEASE-05）给注册表加互斥锁（:45 `static std::mutex g_model_registry_mutex;`，注释明确「并发写 std::unordered_map 是未定义行为…故全部访问统一经本互斥量」，:48-90 全部访问函数带 `lock_guard`）。三份文档仍把「无锁竞争」登记为**现行缺陷事实**——文档与代码反向漂移；DISP 表头「均为现行实现事实」失真。ABA 子面（按裸指针键控）仍与代码一致，仅「无锁」部分过时。
- **证据**：代码 :45/:49/:56/:63/:70/:81/:87 实开；文档三处行号见上；**反方核验**：module.yaml:21 说 threading_model=host_executor_lease 是「迁移目标合同值」、:22「现状单线程顺序（无 omp pragma）」仍准确（noise_model_impl 内确无 omp），故过时的只是「无锁」三处，不是整个线程段落。
- **建议改法**：NOISE_ESTIMATION :76/:196、module.yaml :23/:109、README 注册表段补一行「PERF-P1 已加进程级互斥（noise_model.cpp:38-90），无锁登记降为历史」；或按「登记不改码」纪律出 DISP-NOISE-001 修订注。
- **所属面**：③（兼④）

### S27-黄-5 ｜黄｜面④（oracle 齐备：独立 NumPy oracle 随 run/ 回收消失，复算链断裂）
- **文件:行**：`lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp:7`、`:171-173`；`lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp:10-12`；`lib/algorithms/noise_snr/README.md:243-244`
- **问题**：测试与文档声称的独立复算 oracle——`run/perf-fix/P5-snr/harness/snr_oracle.py`、`run/RELEASE-02/conform-fix-a/harness/conf1_oracle.py`、`snr_oracle_probe.cpp`——在当前工作树**全部不存在**（`find . -name snr_oracle.py/conf1_oracle.py` 零命中；`.gitignore:17` `run/*`，run/ 已被轮次回收策略清理）。测试内嵌锚值（kOracle*）仍可跑，但「同一公式由 NumPy oracle 独立复算」的证据链在仓库内不可复现。
- **证据**：`ls run/perf-fix/P5-snr/harness/ ...` → 无此文件；`.gitignore:17`。**反方核验**：noise_model 面 oracle 齐备——`tests/p1noise/noise_model_numpy_oracle.py`（610 行，入库）、`p1noise_oracle.hpp`（505 行，入库）均在；p1snr 测试锚值内嵌且判据可红（negative/oracle/determinism 组实开），故是「参考脚本缺位」而非「测试无 oracle」。
- **建议改法**：把 snr_oracle.py/conf1_oracle.py 收编入库（如 tests/p1noise/ 或实验单元），或在注释改述「oracle 已内嵌为锚值，原脚本见 run/…（临时面，不保证存续）」。
- **所属面**：④

### S27-黄-6 ｜黄｜面④（defaults.json→NOISE_MODEL.md source_ref 行锚 8/9 漂移且无门覆盖）
- **文件:行**：`eng/packaging/config/defaults.json` 各 `noise.*.source_ref.line`；对照 `docs/science/NOISE_MODEL.md` 现行内容；`docs/science/algorithms/NOISE_ESTIMATION.md:216`
- **问题**：defaults 的 source_ref 设计是「默认值引用落在科学文档实际陈述行」（NOISE_ESTIMATION:216、ENGINEERING_SPEC §3），但实测 9 个 NOISE_MODEL 行锚中 **8 个已指到非陈述行**（NOISE_MODEL 历轮插入订正注后未同步）：
  - variance_floor→:23（现为符号表 `x` 行；variance_floor 行实为 :30）
  - spatial_field_enabled→:47（现为「## 3a 坐标 frame」标题；陈述实为 :54）
  - patch_grid→:54（现为平面场启用条件；8×8 陈述实为 :61）
  - clip_sigma/max_clip_rounds→:56（现为 gain 行；5σ 陈述实为 :63）
  - mask_k_sigma→:84（现为「与 noise_model.cpp:1-938 一致」行；k=0.1 实为 :95）
  - mask_r_min_px/mask_fwhm_floor_scale→:85（实为 :96）
  - source_mask_radius_px/mask_radius_scale→:86（现为 §5a 标题；rmax 实为 :97）
  - mask_budget_*→:87（空行；预算行实为 :98）
  - **仅 min_patch_samples/saturation_level→:52 命中**（该行同时含 min_samples 64 与饱和域陈述 ✓）。
- **证据**：本报告 §2 附录「source_ref 实开对照」由本轮逐行 read 比对得出；`grep -rn source_ref eng/ci` → 仅 `check_registration_anchors.py`（其 R1/R2 只扫 **eng/contracts/**、且只验「路径存在」不验 line），`defaults.json` 不在该门覆盖内 ⇒ 行漂移无机器告警。
  - **反方核验**：NOISE_ESTIMATION:216 自己登记的行号（:46/:48/:37/:39/:21）与 defaults 实值（:54/:56/:52/:47/:23）也互不相同——三方（本节登记 / defaults 实值 / NOISE_MODEL 现行）无一对齐；但所有键的**路径与键语义**均真实存在，非死键。
- **建议改法**：defaults.json 9 行 source_ref.line 按 NOISE_MODEL 现行陈述行重钉；NOISE_ESTIMATION:216 的行号清单同步（或改为按小节引用避免再漂）。
- **所属面**：④

### S27-黄-7 ｜黄｜面②（连④，shared 域 SHX-1）
- **文件:行**：`lib/algorithms/shared/Makefile:4`、`:15`、`:18-19`、`:24-25`、`:47-49`
- **问题**：模块 Makefile 三处自述与实况/自身矛盾：① :4/:15 称「lib/common 是 header-only 公共类型库…加 -I../common/include」——`lib/common` 与 `lib/infrastructure/common` 目录均不存在（本目录实为 lib/algorithms/shared，旧路径仅存于 artifacts/evidence/v19r7-quality/file_audit_before.json:525-534），照抄必失败；docs 侧同款残留已由 v19r7 DRZ-08 登记，本 Makefile 未见登记。② :4-5「header-only…不产出 .dll/.so」与 :24-25/:44-45 自己声明并编译 HEALPIX_SRC=healpix/healpix_core.cpp 内部两说（实况 CMakeLists.txt:464-466 将其编入 astrocs_common STATIC）。③ :18-19 POSIX 工具链（g++/-std=c++17、./$(TEST_BIN)）与 :47-49 clean 目标 Windows 语法（`del /f /q … 2>nul`）平台两说——AGENTS §3 Linux 节点下 make clean 必失败。
- **证据**：文件行实读＋`ls lib/common` 失败＋CMakeLists.txt:464-466 实开。**反方核验**：:15 有「按模块位置调整」对冲语，但「lib/common」目录名已不存在；权威构建为根 CMake/Ninja，Makefile 属辅助通道。
- **建议改法**：订正 :4/:15 路径与 header-only 表述、clean 改 rm -f 或注明平台限定；不改任何数值/容差。
- **所属面**：②（连④）

### S27-黄-8 ｜黄｜面④（shared 域 SHX-2，「文档说有」专项）
- **文件:行**：`lib/algorithms/shared/healpix/healpix_core.cpp:340-341`（兼 `tests/test_healpix_neighbors.cpp:15`）
- **问题**：注释以现在完成时声称「由纯 python 双参照 oracle（astrometry get_neighbours 逐槽算法＋Gorski 2005 钉值）全像素交叉验证」——该 python 双参照脚本全仓不存在：grep「get_neighbours」仅 5 命中（本注释 :340/:390、test:8、独立审计/证据/通读-CR-63.md:556/:615），glob `**/*neighbours*`／`**/*oracle*healpix*` 空；test:15 的「astrometry test_healpix.c 钉值 18 例」源文件亦未随仓。CR-63:556 自证「T7b 只等值集合、未对拍逐槽顺序⇒槽序无外部锚」。
- **证据**：见上 grep/glob。**反方核验**：M6a-F-001 逐字登记的是 `test_healpix_oracle.cpp:4` 的 healpix_fullsky_oracle.py 缺失、M2b-F-01 登记其未挂 ctest——均不覆盖本处 neighbors 段的双参照声明；CR-63-25 对 :338-341 的处置是删「移植自官方 C++」字样（许可证面），非验证可复现面 ⇒ 本位点属同族新位点。
- **建议改法**：与 M6a-F-001 修复批次同批——脚本入仓，或把注释改写为「一次性验证记录（归档＋日期/SHA 指针）」，禁以在位交叉验证宣称。
- **所属面**：④

### S27-黄-9 ｜黄（增量子点，主体已由 CR-12-23 登记）｜面①（对照面，shared 域 SHX-3）
- **文件:行**：`lib/algorithms/drizzle/healpix_drizzle/healpix_core.h:106`（shim `pixelToFine` 内 `int n = 1 << (2 * shift);`）
- **问题**：shift = log2(nside_fine/m_nside)，shift ≥ 16 时 2·shift ≥ 32，对 int 左移 ≥32 位为 UB——即便按 CR-12-23 建议「沿用现值」也不安全，须先消 UB。这是已登记项（静默退化语义＋零消费者＋建议删除/改抛）之外未被点名的维度。
- **证据**：h:100-110 原文；对照权威 `shared/healpix/healpix_core.h:90-101` 对 bits≥64 显式抛 overflow_error、:56 注「R9-B: 禁止 UB」。**反方核验**：grep 复证 pixelToCoarse|pixelToFine 全仓仅两处定义、零构建消费（archive/legacy/stack_engine.cpp:242 属零构建归档），今日不可达——故仅补 UB 维度供合并处置，不重复 CR-12-23 语义结论。
- **建议改法**：并入 CR-12-23 处置（删除两方法或与权威同判据抛异常）时一并消 UB；只登记上呈。
- **所属面**：①（兼④）

### S27-黄-10 ｜黄｜面④（psf 域：PSF.md 实现锚批量漂移，dpsf 锚 9/14 失效＋sdet 锚 1）
- **文件:行**：`docs/science/PSF.md:16`、`:17`、`:23`、`:106`、`:113`、`:114`、`:115`、`:116`、`:117`、`:93`
- **问题**：PSF.md 的 dpsf/sdet 实现锚多组失效（dpsf_psf.cpp 现 1146 行，锚面按旧版钉）：
  - `:16` `I(r) 模型 | dpsf_psf.cpp:13-18` → 实为 `#include` 行；
  - `:17` `Q 二次型 | :66-95` → 实为 gauss 消元（`x[i] = aug[i*(n+1)+n]…`）；
  - `:23` `解析通量 | :368` → :368 实为 `double sx0 = 0.15 * rw;`，flux 实位 **:429**；
  - `:106` `θ 消歧 | :352-363` → :352 实为 `nf=filtered.size()`，θ 四候选消歧实位 **:411-423**；
  - `:113` 证据格 `dpsf_fit:446 empty rect` → 实际日志行 **dpsf_psf.cpp:500**（:499-504 段）；
  - `:114` `A<=0 WARN | dpsf_psf.cpp:310` → :310 实为 `return DPSF_FIT_INVALID_PARAMS`（采样全非有限返回处），A<=0 WARN 实位 **:364**；
  - `:115` `WARN Invalid fit params | :333` → :333 实为 trimmed 中位数计算，日志行实位 **:387**；
  - `:116` `WARN FWHM exceeds rect | :343` → :343 实为 `threshold = 2.0×1.4826…×mad_lh`，日志行实位 **:397**；
  - `:117` `LM 不收敛 | :186` → :186 现为 `}`（lm_solve 内部），lm 调用实位 **:374-380**；
  - `:93` `s_factor | sdet_api.cpp:2071-2074` → 该处实为对称性质量检查注释，s_factor 定义实位 **:1812/:2567**（值 3.7172 本身无误）。
- **证据**：本切片用脚本批量抽出 PSF.md 全部 `dpsf_psf.cpp:N` 声称行号并逐行显示实际源码行（各条实录见上）＋psf 子代理逐格实开补 3 锚（:106/:113/:114）＋grep s_factor 复核。**反方核验**：6 组锚命中——`:51`（:499-504 rw/rh 检查 ✓）、`:70`（:24-25 ✓）、`:76`（:25 MOFFAT4_FWHM_FACTOR ✓）、`:80`（:393-394 ✓）、`:122`（:120-208 lm_solve ✓）、`:135`（:248-249 lo/hi ✓）；失效锚指向的语义在实现中均存在（行号漂 54–61 行），数值/公式面无冲突；同文件 :113 的错误码 DPSF_ERR_PARAM 缺失问题属前轮 S3B-黄5 已报未修（见 §7）。
- **建议改法**：10 处按现行 dpsf_psf.cpp/sdet_api.cpp 重锚（数值不动）。
- **所属面**：④

### S27-黄-11 ｜黄｜面④（psf 域：STAR_PSF_ALGORITHMS.md §11 冻结附录同款漂移）
- **文件:行**：`docs/science/algorithms/STAR_PSF_ALGORITHMS.md:150`（行数声明）、`:158-174`（§11.1 表）、`:176`、`:186-189`、`:199-203`
- **问题**：§11 自称「冻结附录（SRC-PSF-001 源码实测）」，实测基准声明 `dpsf_psf.cpp` **934 行**（实 **1146 行**）；表内多组锚按旧版行号：`moffat4 残差 :72-101`（现为 gauss 消元区）、`compute_trimmed_mad :190-220`（实位 **:222** 起）、`LM 调用 :320-321`（实位 **:374-380**）、`F3 flux :374-375`（实位 **:429**）、`FWHM :339-340`（实位 **:393-394**）。
- **证据**：wc -l 与 sed/grep 实开（与黄-10 同批核验数据）。**反方核验**：`lm_solve :104-188` 段位含实位 :120 起 ✓、`:24-25`（MOFFAT4 常数）✓、错误码域 dynamic_psf.h:33-36 与 §11.2 表一致、DISP-PSF-001..006 内容描述与代码现状一致（含 DISP-PSF-003 死参数 :716-719）——语义/数值面无冲突，仅行号失真。
- **建议改法**：§11 按现行源码重锚＋行数声明改实测；不改任何冻结语义。
- **所属面**：④

### S27-黄-12 ｜黄｜面②（psf 域：dpsf_log 写死源码树日志路径）
- **文件:行**：`lib/algorithms/psf/src/dpsf_log.cpp:32-45`（`make_dirs("lib/algorithms/psf/logs")`＋`append_open("lib/algorithms/psf/logs/dynamic_psf.log")`，_WIN32 分支 `lib\\dynamic_psf\\logs`）
- **问题**：日志路径写死为相对 cwd 的**源码树目录**（非 output_dir、非 run/），与 AGENTS §6「一切输出落 output_dir 或 run/」相悖；工作树已累积 `lib/algorithms/psf/logs/` **283 MB** 运行日志（另 `psf/lib/dynamic_psf/logs/` 12 KB 为历史分支产物）。
- **证据**：dpsf_log.cpp:30-46 实读；`du -sh` = 283M。**反方核验**：`lib/algorithms/psf/.gitignore` 含 `logs/`、`git ls-files` 零命中 ⇒ **未入库**，不构成入库污染；盘面风险真实（AGENTS §3 盘满伪装随机故障），但无 CI 门指向该路径。
- **建议改法**：登记——日志目录可配置、默认落 run/ 或 output_dir；现状至少定期清理 283 MB 残留。本切片不改。
- **所属面**：②

### S27-黄-13 ｜黄｜面④（psf/noise_snr 两模块 module.yaml 与子图 CMake 注释锚漂移）
- **文件:行**：`lib/algorithms/noise_snr/module.yaml:27/:116`、`lib/algorithms/psf/module.yaml:31-32`、`lib/algorithms/noise_snr/CMakeLists.txt:25/:29`
- **问题**：
  - noise module.yaml:27/:116 声称 descriptor 在 `module_adapters.cpp:489-503` → `p1_noise_snr_descriptor` 实位 **:956**（:489-503 现为无关代码）；
  - psf module.yaml:31-32 声称 `module_adapters.cpp:430-448 p1_star_psf_descriptor` → 实位 **:868**（:430-448 现为错误码字符串映射）；
  - noise_snr/CMakeLists.txt:25「根 :240 add_subdirectory…」→ 注释行实位 **:320**；`:29`「已源文件直接编入根目标 astrocs_phase1_noise（根 CMakeLists.txt:630-633）」→ 实位 **:807-810**（该行是 2026-09-20 订正注，当时正确、现漂移）。
- **证据**：grep -n 定位两 descriptor 定义行＋sed 实开两 module.yaml/CMakeLists 注释原文。**反方核验**：descriptor 注册本身在册（module_adapters.cpp:15465/:15468）✓；两 module.yaml 的「entrypoint=MISSING/dll 未入根图」状态语义与 F-CI-002-01 摘出裁决自洽（非错报）——失真的仅是 file:line 锚；psf 无子图 CMakeLists.txt（只有根 CMake 无 psf 源——psf 源以其他目标接入，另核）。
- **建议改法**：三个文件的行号锚按现行源码重钉；descriptor 内容字段（module_id/sci_id 等）不动。
- **所属面**：④

### S27-黄-14 ｜黄｜面④（psf 域 PSF-2：README/module.yaml/源码头注释行号锚 r2 后整体漂移）
- **文件:行**：`lib/algorithms/psf/README.md:78-104/:86-92/:110/:121-138/:146-147/:182-190`、`module.yaml:26-27/:128-155`、`src/dpsf_psf.cpp:1-5`、`src/dpsf_psf.h:4-12`
- **问题**：两份模块登记自称「行号锚全量复测刷新」（README:4-5、module.yaml:2-3 r2 抬头），但其后的多批改动（P2 性能批 2026-09-14、B2-A2、W1 收缩）使源码行号锚再次整体漂移而未刷新。实测对照（声称→实开，60+ 条节选）：dpsf_fit :459→**dpsf_psf.cpp:481**；空 rect :477-482→**:499-504**；batch :534→**:556**；batch_d :694→**:716**；f64 :937→**:990**；moffat4_fit_tmpl :238→**:260**；lm_solve :105→**:120**；trim lo/hi :226-227→**:248-249**；残差 :73→**:84**；LM 容差 :352-353→**:374-375**；θ 候选 :389/:391-401→**:411/:413-423**；flux :406-407→**:429**；OpenMP 4 处 :593,718,842,986→**:615,740,873,1047**；dynamic_psf.h 头行 :107/:142/:181→**:122/:160/:200**；外部锚 dll_loader :39,53→**:57/:71**、orchestrator 必需 stage :2071-2075→**:2082-2088**、descriptor :430-448→**module_adapters.cpp:868-893**、CMake 接入 :984→**eng/tests/unit/CMakeLists.txt:1293**；源码头注释自锚同漂（dpsf_psf.cpp:3-5、dpsf_psf.h:4-12 所引行号 vs 实开）。
- **证据**：子代理逐段实读 dpsf_psf.cpp 全文 1-1146 六段＋对照行 grep。**反方核验**：逐条确认符号与语义全部真实存在（非幻觉）；`:25/:26`（MOFFAT4_FWHM_FACTOR/NPARAMS）与 dynamic_psf.h:17-31/:33-36 等常量锚仍命中——属「批后漂移」而非整文件错位；N4b 锚 :717 行号仍正确。
- **建议改法**：按现树做一次全量行号复测刷新并登记刷新日期（上呈）。
- **所属面**：④

### S27-黄-15 ｜黄｜面③＋②（psf 域 PSF-3：README 端口表把批 API 缓冲布局错标为 psf 交付块，invalid 格两说）
- **文件:行**：`lib/algorithms/psf/README.md:46`（§2 输出 ports 表 psf 行）、`module.yaml:151-155`（「非 OK 星 9 字段全 NaN」）
- **问题**：psf 输出行列序写成 API 缓冲布局 `[0]=B…[7]=fwhm_x [8]=fwhm_y`、invalid=「拟合失败星 9 字段全 NaN」，与三处冲突：①实际落块 orchestrator.cpp:2386-2402（fn_add_block psf 实读）= **status,B,flux,cx,cy,fwhm,A,mad,eccentricity**，[7]=mad 即 residual_scale；②PSF.md:24「residual_scale = PSF 块第 8 列」（与落块一致，PSF.md 无错）；③README 自身 :131-136 B2-A2 段「失败星不占参数行（不再留 NaN 洞）」、dynamic_psf.h:95-96「compact 写入前 n_valid 行、其余行不承诺值」、DATA_SEMANTICS:764-765「禁留 NaN 占位行」。
- **证据**：落块三行与三处文档原文实读。**反方核验**：README 所列列序确实对应 dpsf_fit_batch_f32/f64 的 out_psf_params（dpsf_psf.cpp:925-933/:1093-1101）——非捏造，而是把批 API 缓冲布局错标成 psf 端口交付块且未按 STAR_PSF:52-55 消歧纪律先声明布局；生产通道 dpsf_fit_batch_d 失败星=memset 0＋status（:501、README:122 自述），非 NaN。
- **建议改法**：§2 行改注「批 API out_psf_params 布局（非落块）」或改写为落块布局；invalid 格与 §6 B2-A2 统一（上呈）。
- **所属面**：③（兼②）

### S27-黄-16 ｜黄｜面④＋③（psf 域 PSF-4：端口表锚失效且漏列必输入端口 wcs）
- **文件:行**：`lib/algorithms/psf/README.md:40-46`、`module.yaml:50-52`
- **问题**：端口表引锚「registry descriptor ports（module_adapters.cpp:437-441）」失效（实 p1_star_psf_descriptor=`module_adapters.cpp:868-893`、ports=:875-886）；且漏列**必输入**端口 `wcs`（module_adapters.cpp:883 `{"wcs","DATA-P1-WCS",true,…ICRS}` required=true）——README 只列 cleaned/sources/psf 三口，module.yaml input_ports 仅 [cleaned, sources]。
- **证据**：descriptor 行实读。**反方核验**：README 已列三行的方向/必选/单位与 descriptor 逐字相符（cleaned 必 ADU :876、sources 可 :884、psf 可 DIMENSIONLESS PIXEL :885）——缺的是 wcs 整行；module.yaml:35 有「由 P1-PSF-INT 对齐」待办，但 README:40 声称表格即 descriptor 现状。
- **建议改法**：两文件补 wcs 输入口并刷新锚（上呈）。
- **所属面**：④（兼③）

### S27-黄-17 ｜黄｜面④（psf 域 PSF-5：测试路径与接入锚失实）
- **文件:行**：`lib/algorithms/psf/README.md:144/:186-187`、`module.yaml:143`
- **问题**：README:186「共址测试面 eng/tests/p1psf/」、:144「eng/tests/p1psf/p1psf_tests_core.cpp:717」、module.yaml:143「负例锚 eng/tests/p1psf negative 组 N4b/N4c」——`eng/tests/p1psf` 目录**不存在**，测试实位 `lib/algorithms/psf/tests/p1psf/`（git ls-files 12 文件全 tracked）；CMake 接入实位 `eng/tests/unit/CMakeLists.txt:1293`（README 声称 :984）。
- **证据**：ls/grep/实读。**反方核验**：:717 内容正确（N4b 实位 p1psf_tests_core.cpp:717-765，w/h∈{0,-1,INT_MIN}×5 ✓）、N4c 存在（:772-792 ✓）、ctest 七名与 README:187-188 逐名相符（CMakeLists:52-78 ✓）——错的仅目录路径与 :984 两处事实。
- **建议改法**：路径改现树共址并刷新锚（上呈）。
- **所属面**：④

### S27-黄-18 ｜黄｜面①（psf 域 PSF-6：README:103 flux 括注在 α 参数化下差 2×）
- **文件:行**：`lib/algorithms/psf/README.md:103`
- **问题**：flux 通式括注「（β=4 Moffat 解析积分 **∫=2πAα²/(β−1)**）」在 α=√2σ 参数化下比正确值大 2 倍：按 PSF.md:66 α=√2σ ⇒ 2πAα²/3 = 4πAσ²/3，而正确值为 2πAσ²/3（PSF.md:178「各向同性极限 πα²/3·A=2πAσ²/3 自洽」、代码注释 dpsf_psf.cpp:428 逐字「2*pi*A*sx*sy/(beta-1)」）。
- **证据**：三处原文实读＋子代理独立复算（PSF.md:85-88 二重积分 det M=1/(4sx²sy²)、∫(1+Q)^{-4}=π/(3√detM) ⇒ 2πAsxsy/3 ✓）。**反方核验**：**最终数值 flux=2πA·sx·sy/3 三方一致（代码 :429 生产无错）**，仅 README 括注把 sx·sy 误写成 α²（仅当 α²:=sx·sy 时相等，与 README:64/PSF.md:66 的 α=√2σ 定义矛盾）。
- **建议改法**：括注改 πAα²/(β−1) 或径引 sxsy 式（登记上呈；公式正文不动）。
- **所属面**：①（同量口径，兼②）

### S27-黄-19 ｜黄｜面①（适用域）＋④（psf 域 PSF-7：拟合窗「典型值」与实现接线不符）
- **文件:行**：`docs/science/PSF.md:90-96`
- **问题**：「典型拟合窗 r_win=3.7172σ（sdet_api.cpp 的 s_factor）」把 PSF 拟合窗等同于星检测盒常数，但实现里 PSF 拟合窗=**配置 px**（json_config.h:49 `int fit_radius=8`、orchestrator.cpp:2289/2297 `params.fitRadius=fit_radius` 实读，dpsf 固定半径裁窗 dpsf_psf.cpp:492-495），无按 σ 的接线；sdet 的 s_factor 只进检测盒（sdet_api.cpp:1812/:2060-2061）。由该等同导出的「发布 flux 偏高 0.20%（f_out=2.02e-3）」的适用域与固定 fitRadius=8px 下逐星 r_win/σ 关系未声明。
- **证据**：配置链与 s_factor 消费实读。**反方核验**：算术自洽（3.7172/√2=2.6285=r_win/α、(1+2.629²)^{-3}=2.02e-3 复算 ✓，r_win<1.41σ⇒f_out>3.1% 方向正确）；文档用词「典型」并给出 r_win/α 依赖式，属量级估计非冻结门；该句的 sdet 锚失效并入黄-10。
- **建议改法**：注明「典型」取值来源是检测盒量级而非实现接线，或按 fitRadius=8px/典型 σ 折算（登记上呈，不改冻结公式）。
- **所属面**：①（适用域）＋④

### S27-黄-20 ｜黄｜面①（常数写法，psf 域 PSF-8）
- **文件:行**：`lib/algorithms/psf/src/dpsf_image.cpp:258`
- **问题**：MAD 常数写成 float 字面量 `return mad * 1.482602218505602f;`——与 NOISE_MODEL.md:312「唯一权威写法 1.482602218505602（double 字面量；11 位简写只能在约等于语境）」冲突，也与同模块 dpsf_psf.cpp:343 的 double 写法两说；f 后缀使常数求值截为 float32（约 7 位有效）。
- **证据**：两处字面量实读＋NOISE_MODEL:312 原文。**反方核验**：①robust_mad/robust_median 全仓无调用方（grep 除定义零命中，PSF 拟合路径用 compute_trimmed_mad＋dpsf_psf.cpp:343 double 常数）——**生产数值零影响**；②函数签名返回 float、入参 float，即便去 f 结果仍截断 float32——属写法口径非行为缺陷；③16 位数字串逐位正确。**注**：1e-6 量级冻结条款（A-P1-08 家族）对 float 求值相对差 ~1.9e-8 仍在 1e-6 内，故为写法两说而非违规定量。
- **建议改法**：按权威写法去 f 后缀或注明 float32 工具域（登记上呈，不代改冻结常数）。
- **所属面**：①

### S27-黄-21 ｜黄｜面④（psf 域 PSF-9：TST-PSF-INV-*/FAIL-* 两族 ID 未登记却声称见 TRACEABILITY）
- **文件:行**：`docs/science/PSF.md:173`
- **问题**：「TST-PSF-INV-* 三门、TST-PSF-FAIL-* 参数校验（新增/映射见 docs/TRACEABILITY.csv）」——TRACEABILITY.csv 全文只有 TST-PSF-001（:65 映射 eng/tests/backend/test_psf_moffat_oracle.py），两族 ID **零登记**（grep docs/eng/lib 仅 PSF.md:173、STAR_PSF:145 与测试注释自引）。
- **证据**：CSV 全文 grep。**反方核验**：功能在位——三不变量与失败注入真实现（p1psf_tests_core properties/negative 组、selfcheck 恒 FAIL/恒 PASS 双向自检 :63-72 实读，注入必败验证非恒真门），TRACEABILITY:65 也如实注记 TST-PSF-001——缺的只是「映射见 TRACEABILITY」这一登记事实。
- **建议改法**：补两族 ID 登记或改 PSF.md:173 指向（上呈）。
- **所属面**：④
---

## 3. 绿级记录（可不改，登记）

### S27-绿-1 ｜绿｜面④
- `lib/algorithms/noise_snr/include/astrocs/v6/information_weight.h:58` 注释声明拒绝词含 `"dimension_mismatch"`，`information_weight.cpp` 全文件无此字符串（实际拒绝词集合见实现；维度由调用方契约保证）——注释多列一个未实现的拒绝词。合同行为不受影响。

### S27-绿-2 ｜绿｜面④
- `lib/algorithms/noise_snr/cpp/src/snr_science.cpp:55-56`：`kTrimMeanToSigma` 定义处注释锚「noise_model.cpp:37 同源常数」——该常数现行位于 `noise_model.cpp:95`（:37 现为 namespace 内注释行）；且本 TU 内 `kTrimMeanToSigma` 定义后零引用（未标 [[maybe_unused]]）。同文件 `:45-46` 说明 kLn10 唯一定义点策略已执行 ✓。

### S27-绿-3 ｜绿｜面②
- `lib/algorithms/noise_snr/src/astrocs_p1_noise.def:1`、`module_exports.map:1` 注释头仍写旧路径 `lib/snr_estimator/src/…`（实位 lib/algorithms/noise_snr/src/）；`.map:9` 引用 `tests/unit/p1_noise/adapter_test.cpp`（实位 eng/tests/unit/p1_noise/adapter_test.cpp，文件存在 ✓）。历史路径注释，导出白名单内容本身与 ABI-006 一致（唯一导出 astrocs_module_query_v1）。

### S27-绿-4 ｜绿｜面②
- `lib/algorithms/drizzle/healpix_drizzle/healpix_core.h`（shared 域交叉观察）：shim 构造函数注释「非法 nside 不抛异常…与 common 行为对齐」，而权威实现 `shared/healpix/healpix_core.h:22-23` 对非法 nside「抛 std::invalid_argument」——shim 构造容忍、后续委托调用时仍会抛，「与 common 对齐」措辞不准确（行为差异在构造时机，drizzle 入口已校验 nside，实害≈0）。

### S27-绿-5 ｜绿｜面④（shared 域 SHX-4）
- `lib/algorithms/shared/dirent_win.h:2`：自述「仅供 MSVC…足够 hips_properties.cpp 的目录遍历」——全仓零 `#include "dirent_win.h"`，且 `lib/algorithms/coverage/hips_properties.cpp:9` 自述不再 include dirent。本体已由 AUD-401/AUD-404 判零引用（保留并说明/可删项），此处仅补「注释声称的用途本身已失效、未随零引用登记一并订正」的注记面；另 AUD-404 称其「vendored 上游文件」与文件自述（ACSD 自写 shim）不符，处置时宜一并纠正定性。

### S27-绿-6 ｜绿｜面④（shared 域 SHX-5）
- `lib/algorithms/shared/healpix/tests/test_hips_tile_mapping.cpp:4-5`：证据锚指向 `run/temp/v5_oracle`——不存在且按 AGENTS §7 不入库；但外部 Oracle 生成脚本本体实存（`lib/infrastructure/aio/tests/v5_maptile_oracle.py`、`hips_mapping_oracle.py:6-8`），另有执行面 `sanitize_wsl_v5.sh:80-86`，故非「验证不存在」而是锚指瞬态产物。建议注释锚改指脚本级。

### S27-绿-7 ｜绿｜面④（psf 域：04_psf.md 死 schema 引用，已由 MODULE_MAP 登记）
- `docs/plugins/algorithms_phase1/04_psf.md:21`「参考：eng/contracts/schemas/psf_output.schema.json」——该文件全仓不存在，unified/ 16 个对象 schema 中亦无 psf 对象（examples/v6/psf.example.json 存在但无对应 unified schema）。该死链已由 `docs/modules/MODULE_MAP.yaml:785` 显式登记（BLD-401：「模块输出 schema 未落地（plugin 文档声明的 legacy 名；现行 canonical=unified/**）」），故只登记不计黄；建议 04_psf.md:21 补同款失效注记。

### S27-绿-8 ｜绿｜面②（psf 域：04_psf.md 空间变化职责表述与块状现状的口径宽严）
- `docs/plugins/algorithms_phase1/04_psf.md:7/:19/:26` 以「估计空间变化的 PSF 模型…空间变化模型及协方差」为职责表述，而 `docs/science/PSF.md:189` 明言「ACSD 现状为块状共享 7 参数 Moffat4，不做空间变异多项式基（§1 非目标）」、实现无协方差输出（DISP-PSF-005 已登记 covariance 缺口）。04:26 的「若通过均匀性门可降级为帧级；否则保留空间模型/控制点」可读作 drizzle 侧按 psf_block 消费的分块语义，不构成硬冲突——登记为表述宽严差，建议 04 措辞与 PSF.md 非目标句对齐（块状/无协方差）。04:35-37 三配置键（psf_spatial_order/psf_uniformity_gate/fit_residual_gate）在 eng/packaging/config 无登记键（defaults 仅 psf.default_model/moffat_beta），属「文档表列但全局配置无此键」——键是否应落 defaults 由 P1-PSF-INT 确认，本切片只登记。

### S27-绿-9 ｜绿｜面①（psf 域 PSF-10：nan_sort 状态门缺第四码）
- `lib/algorithms/psf/test/test_dpsf_nan_sort.cpp:78-79`（同型 :104-110、:120-126）：Case2/4/5 的 status_legal={OK, INVALID_PARAMS, NO_CONVERGENCE} 不含合同第四码 `DPSF_FIT_ITERATION_LIMIT=3`（dynamic_psf.h:36、README §6:124）——迭代触顶的合法结果会被判 FAIL（过严、潜在假红）；且 rc≠OK 时该 Case 无后续实质断言（弱门）。**反方核验**：非恒真——Case1 严格要求 OK、Case3 全 NaN 严格要求 INVALID_PARAMS，有区分度；同套件 selfcheck 有故障注入必败自检（p1psf_tests_selfcheck.cpp:63-73）。建议合法码集补 3（登记）。

### S27-绿-10 ｜绿｜面②（psf 域 PSF-11：fixtures 字母表述）
- `lib/algorithms/psf/tests/p1psf/p1psf_fixtures.hpp:1-15`：头注写「FIX-PSF-A..F」，实际仅 A–E＋FIX-PSF-PERF（:12-15 自列即无 F、:216 fix_psf_perf）。六类 fixture 功能齐备，仅字母表述多一档。

（另：psf 运行日志未入库核查——`lib/algorithms/psf/.gitignore:3` `logs/` 覆盖，`git check-ignore -v` rc=0、`git ls-files` 零命中、status 干净，已并入黄-12 反方核验，不单列。）

---

## 4. 开放问题登记（不给裁决）

- （无新增；红-2 中 D-05 适用域问题已按「上呈确认」处理。）

---

## 5. 已查无问题面（逐面说明）

### 5.1 noise_snr 域

- **①科学性（无问题）**：
  - 1.4826 常数：责任域全部出现处（`noise_model.cpp:119/:219`、`module_entry.cpp:23`、`wrapper README:15`）均为全精度 `1.482602218505602`，无 4 位简写；`snr_science.cpp:58` 与 NOISE_MODEL.md 同式同精度（A-P1-08 责任域外）。
  - 1.253（=√(π/2)）口径：`snr_science.cpp:48/:67/:76/:118`、`snr_frame_science.cpp:87`、README:211/:231、NOISE_ESTIMATION:211/:213 全链一致（比值语境与实现一致，不报）。
  - k_gauss/k_moffat 表（`snr_science.cpp:45-46`）与 NOISE_MODEL.md:323 逐值一致；10-90% 残差常数 `0.7316727929211932` 在 `snr_science.cpp:50`、`snr_estimator.cpp:63`、PSF.md:188 三处逐字符一致。
  - `information_weight.cpp` 数学逐式对 `docs/science/PSF_SIGNAL_WEIGHT.md §2`：`W=a²PᵀC⁻¹P`（finish_from_x :237-238）、`Q=a·dᵀx`（combine :279）、`Var=1/ΣW`（:288）、白噪声 `a²/(σ²·A_NEA)`、`A_NEA=1/ΣP²`（:231-232）全部相符；对角近似偏差比 variance_ratio=`c̃ᵀC c̃/(1/W_true)`（:200-212）方向正确；白噪声 gate 双条件（对角＋sigma_declared，:106-110）与 FZ-COND-WHITENOISE 逐字对应；`kQwRelTol=1e-9` 与 FZ-AP1-GLS-QW-RTOL 冻结值一致。
  - `snr_frame_science.cpp`：帧深度公式（F5=5σ_F、m5=ZP−2.5log10F5、无 ZP→NaN）与 NOISE_MODEL §C.3.1 一致；`:87` 的 2.5× 系数已核（A-P2-03 终裁非 0.4，不重报）；8/16-bit 头缺省 σ 计算逐式同配方。
  - `snr_estimator.cpp` 常数面与正本一致；detect 阈值（10×、2.5、5.0）两分块逐句同源且注明照抄字段文档。
- **②行文逻辑（无问题）**：`noise_model.cpp`、`snr_science.cpp`、`information_weight.cpp`、`p1noise` 测试内部注释自洽（fill_impl 双处已用 1.44 新口径）；`memory.md` 零 UNRESOLVED 残留；饱和/口径门 docstring 判据段与代码判据一一对应。
- **③跨文档冲突（数值面无冲突）**：NOISE_MODEL.md ↔ NOISE_ESTIMATION.md ↔ defaults.json 的科学数值（floor 1e-12、预算 9216、k=0.1、r_min 1.5、fwhm_floor 0.75、min_samples 64、clip 5.0/rounds 2）实开逐项一致；冲突仅存在于行号/状态类（黄-1/2/4/6）。
- **④判据与接线（非退化）**：
  - `p1noise_tests_selfcheck.cpp`：4 相位故障注入必败验证（不 fail 报 FAIL-SILENT rc 2）、n7 用 near_ls 对 ls_oracle 独立成立、adaptive 支恒真检测（:145-151 仅 id==3 计数）——能红能绿。
  - `p1noise_tests_core.cpp`：真判据在命名完成标记（P1NOISE_CHECK(true)）之前，非恒真门；`p1snr_science_test.cpp` 有 oracle 组（1e-12 对内嵌 NumPy 锚值）、negative 组（flux≤0/sigma_sky≤0/NaN→status==1、读噪双计 MC z>3σ 反例 :394）、determinism bitwise（:476）。
  - `check_snr_caliber.py`：正例×2＋负例×5 self-test；`check_saturation_wiring.py` W1-W4 全可失败；`p1noise_abi_layout_check.py` 三条判据含突变自检（宏字段交换后必须红）。
  - `p1noise_saturation_test.cpp` G1a–G1n 负例齐（非数值/NaN/负/空串⇒0 且 DISABLED_NO_METADATA 显式降级）＋ on/off 污染对照两侧都断言。
  - ABI/导出面：`astrocs_p1_noise.def`＋`module_exports.map` 唯一导出 astrocs_module_query_v1；`eng/tests/unit/p1_noise/adapter_test.cpp` 导出探针（legacy 七符号 NULL 断言 :381）、ABI mismatch 负例（:418-422）、direct↔adapter 整 JSON bitwise 对照（:323-324）。
  - CMake 接线实存：astrocs_phase1_noise STATIC（根 CMakeLists.txt:807-810）＋ p1_noise_test（eng/tests/unit/CMakeLists.txt:722-728）＋ adapter 测试编译通道（adapter_entry_impl.cpp:6 include module_entry.cpp）；astrocs_p1_noise(SHARED) 按 F-CI-002-01 裁决摘出根图，与 module.yaml「entrypoint=MISSING/dll 尚未存在」自洽。
  - `module_entry.cpp`：三操作 vtable、fail-closed（SNR_FLOOR_UNBOUND -10 / rc=-9）、失败路径零写入、floor 绑定拒绝语义与 noise_model.cpp 一致；注释自标 scientific_change=false 且常数与正本一致。

### 5.2 psf 域

- **①科学性（无新问题）**：
  - FWHM 因子：独立复算 2√2·√(2^{1/4}−1)=1.2303076525901024，实现 1.230310 相对差 +1.90799e-6，与 PSF.md:74-77「+1.91e-6」一致；dpsf_psf.cpp:20-25 注释链（α=√2σ、0.87·√2 带 ≈）与 PSF.md:66/STAR_PSF F2 同源；轴向 FWHM :393-394 ✓。
  - 二次型代数：dpsf_psf.cpp:94-106（p1/p2/p3/Q 含 2·p2）与 PSF.md:60-63、STAR_PSF F1 逐字同式；独立推导旋转主轴等价（交叉项 = sinθcosθ(1/sx²−1/sy²) ✓、det M=1/(4sx²sy²) ✓）。
  - flux 量纲链：PSF.md:30-35（B=ADU/pixel、flux=ADU）与代码 :429 一致；∫(1+Q)^{-4}=π/(3√detM) 独立复算 ⇒ 2πAsxsy/3 ✓；截断式 0.20%/3.1% 量级复算 ✓（窗口来源问题另立黄-19）。
  - trimmed-mean 常数：独立复算 2(φ(Φ⁻¹(0.55))−φ(Φ⁻¹(0.95)))/0.8=0.731673094，与 PSF.md:126 的 0.7316730952806134 相对差 2.1e-9、与实现 0.7316727929211932 相对差 4.11e-7 ≈ 文档声明 4.13e-7 ✓；lo=int(0.1m)/hi=int(0.9m) 实读 ✓；Gaussian 专属不可互换已随文（PSF.md:129-133/:188）。
  - θ 值域/周期性（PSF.md:36-43）、单位表（:28-35）、e=√(1−(smin/smax)²)（:431-432）✓；DPSFFitResult/Params 12 字段+4 码与 dynamic_psf.h:17-42 逐字一致。
  - 常数精度点名项：1.4826 在 dpsf_psf.cpp:343 为 16 位 double ✓（dpsf_image.cpp float 后缀另立黄-20）；范围内无 2.3548 高斯因子混入、无 0.7316728 七位简写（grep 命中仅在 logs 运行产物，未采证）。
  - 判据非退化：p1psf selfcheck baseline＋fault-injection 双向（:63-73，注入后仍 PASS 即报「恒 PASS 占位」）；negative 组 N1-N4c 期待精确码；oracle 真值独立实现（p1psf_oracle.hpp:34-66 旋转主轴投影 vs 生产 p1/p2/p3 无共享）；p1psf_prodpath_check T1「未修复必红、T2 对照」预设非绿（不预置已绿假设）；**未发现恒真门**。
- **②行文逻辑（仅黄-12/15 及绿-8/10）**：README §1/§3/§6/§12 状态口径自洽（CONTRACT_READY 未 IMPLEMENTED、exports 与 W1 收缩一致、DISP-PSF-003 死参数三处一致、B2-A2 段内自洽）；module.yaml notes 与 README §6/§12 同源；grep UNRESOLVED 范围内仅 PSF_SIGNAL_WEIGHT:150「已定案（原 UNRESOLVED）」为收口登记，**未发现以 UNRESOLVED 当结论**。
- **③跨文档冲突（仅黄-3/15/16 相关面）**：1.230310 在 PSF.md/STAR_PSF/README/module.yaml/fixtures(kFwhmFactor)/代码 六处同值、无 2.354820 混用；0.7316727929211932 四处同精度；residual_scale 语义以 PSF.md:24＋DATA_SEMANTICS §15.2 为正（README §2 是错的一方，黄-15）；q_psf 只作诊断不进权重三方一致（PSF.md:10/:153、PSF_SIGNAL_WEIGHT:14-15、UNIFIED_MODEL:58），psf_information.h:13「不接线 session、不含权重反推」与 PSF_SIGNAL_WEIGHT §5/§12 一致；**分歧台账全文 grep "PSF|psf" 0 命中 ⇒ D-01…D-11 不约束本域，无翻案对象**。
- **④幻觉与锚/接线（例外仅黄-10/14/16/17/21＋绿-7）**：实开范围——README 全文 1-247、module.yaml 1-174、Makefile、.gitignore、dpsf_psf.cpp 全文 1-1146 分六段、dpsf_psf.h、dynamic_psf.h 1-215、dpsf_log/image/psf_information 全文、test_dpsf_nan_sort 1-136、p1psf fixtures/oracle/selfcheck/perf/core 分段、PSF.md 1-211、PSF_SIGNAL_WEIGHT 1-162；对照面 orchestrator.cpp 2075-2465＋dpsf 行、dll_loader、module_adapters 425-450/868-893/15465-15477、DATA_SEMANTICS 711-805、NOISE_MODEL 310-316、sdet_api 2055-2080、eng/tests/unit CMakeLists 1285-1293、json_config.h:49、TRACEABILITY.csv:65、git ls-files/check-ignore/status。「文档说有、代码没接」扫描：PSF.md §13 实现锚（dpsf_fit/batch、lm_solve、MOFFAT4_FWHM_FACTOR、compute_trimmed_mad、noise_model.cpp:95、dynamic_psf.h）**全存在** ✓；README §10 消费函数与 orchestrator 三入口存在（仅行号漂）✓；README §12 kPrecisePsfEnabled（module_adapters.cpp:4297）、p1_op_star_psf_precise（:4302）、precise_json 钩子（:15420＋module_adapters.h:36）、测试 p1001:2416 全存在 ✓；psf_information.h 三 FZ 条款与 PSF_SIGNAL_WEIGHT §2 及 v6_clause_registry#weight_vocabulary（本切片实开验证存在、PSFSW 命中 221）同源 ✓；例外即黄-15/16/17/21。

### 5.3 shared 域

- **①科学性（无新问题）**：`shared/healpix/healpix_core.cpp:26-30`（kPi/kTwoPi/kHalfPi/kTwoThird/kRoot3）、`:77-149`（xyz_to_hp 极冠 z=±2/3、赤道 zunits 归一）、`:155-226`（hp_to_xyz，:213-214 z=(1-vv)(1+vv) 手工核为 sinθ 恒等式）、`:304-312`（角距 acos+clamp）、`:327-335`（4π/(12n²) 分辨率，与 STANDARDS_REGISTRY.md:137 同式、与 test_healpix_oracle.cpp:92 sqrt(π/3)/nside 数学恒等）、`:426-434`/`:438-508`（query_disc zone 判定，由 test_healpix_neighbors 暴力真值锁）；`crypto/sha256.cpp:10-66`（轮常数/压缩，抽核 FIPS 首值 0x428a2f98/0x6a09e667）；`astro_scalar.h:44-61`、`precision_context.h:25-57`。D-01/D-09 涉及常数不在 shared（grep CIRCUMRADIUS|1.0415|0.1043885 零命中），无可冲突面。
- **②行文逻辑（仅黄-7）**：16 个范围文件全部通读（healpix_core.h/cpp、THIRD_PARTY_NOTICE、sha256.h/cpp、dirent_win.h、astro_scalar.h、precision_context.h、Makefile、.gitignore、四份测试、两份 python），无 UNRESOLVED/订正注两说混入。
- **③跨文档冲突（无新增）**：shared 无 README/module.yaml，口径面=头注释 vs 实现 vs STANDARDS_REGISTRY vs THIRD_PARTY_NOTICE，冲突全落已登记项 REG-1/REG-4；test_healpix_oracle:92 vs healpix_core.cpp:330 vs STANDARDS_REGISTRY:137 三方恒等；test_hips_tile_mapping:36 vs healpix_core.h:46 vs cpp:286-292 三方逐字一致；psf 专属口径（1.230310/2.354820/0.7316728/1.4826）在 shared 零命中。
- **④判据与锚（专项结论）**：
  - 双实现对照：drizzle 侧已是纯 shim（.cpp 16 行全转发），π 为同一 double，npix/分辨率/邻居表/4π 只 shared 一份，**无数值漂移**；`healpix_core.h:9`「禁止第二套 ang2pix/pix2ang」实测未被违反（实验/healpix-polar 的 include 均解析回 shared，无本地副本）；shim EXIT 注记的「引用清点」经代做：drizzle 内 16+ 消费点（spherical_overlap.h:32、drizzle_engine.cpp:4、reverse_drizzle.cpp:17、v6_spherical_overlap.h:19＋11 tests），WHY-KEPT 属实。
  - 判据非退化：test_precision（static_assert×6＋反向断言「另一分支不得被触碰」:89/:103/:123/:136）；test_healpix_neighbors（18 钉值＋槽序双钉＋全像素对称＋query_disc 暴力真值 16×4×5＋非法 nside 负例含「合法不抛」正向对照＋移位溢出负例，g_fail 非零返 1）；test_healpix_oracle（lines==0‖bad_parse‖mismatch‖bad_roundtrip→FAIL，空输入不放行）；test_hips_tile_mapping（262144 全量双向 roundtrip＋10 锚＋10 万随机固定 seed 20260809，四计数器门）；gen_spatial_fuzz.py（seed 固定、均匀立体角、asin(2/3) 对抗锚与实现 kTwoThird 分支对齐）；snr_hips_spatial_oracle.py（return 0 需 rows>0 && wrong==0 && dup==0 && actual==expected 四条件）。
  - SHA-256 对接：FIPS 180-4 向量在 eng/tests/unit/aio_abi_tests.cpp:47-48 与 v6_aio_test.cpp:193-196，链路 aio_file_io.h:240-254 → astrocs::crypto::Sha256（:245）实读成立；padding :88-98 核对无 56/64 边界错。
  - 假/失效锚 5 处已列（黄-8、绿-5、绿-6；healpix_fullsky_oracle.py 归已登记 REG-4）。

---

## 6. shared 域已登记项状态（实读确认现状，不重复计数）

- **REG-1 许可/来源四面互斥（红级族）**：healpix_core.h:5-8「独立实现」 vs healpix_core.cpp:4-8「迁移自 astrometry.net healpix.c」/ :338-341「移植自官方 HEALPix C++ GPL-2+ 参考」 vs THIRD_PARTY_NOTICE.md:19「未迁移…邻居查询」＋:22「未复制任何 GPL 代码」——已由 FIX_LEDGER M2b-G-04(P1)、CR-63-25、M8a-I-004、OWNER_DECISIONS A-31(P0) 登记；**现状未闭合（四处文本原样在）**；许可证面按 AGENTS §10 属负责人裁决，只登记不改。
- **REG-2 nside 校验三立场**（npix 完全不校验 cpp:333-335、neighbors(0) 返回空 cpp:376、pixel_resolution(0)=0 cpp:328 vs ang2pix/pix2ang/query_disc 抛异常 :234/:251/:441）＝CR-63-24 已登；测试负例面 test_healpix_neighbors.cpp:200-205 仅测 ang2pix_nest(0)、未测 neighbors(0)（同族补充事实）。
- **REG-3 shift 静默夹紧** cpp:275/:280（shift≥32→31）＝M2b-H-02 已登；增量注记：fits_index_to_nested_local :300 与 nested_local_to_fits_index :292 为同族另两处钳位，枚举未含，处置可顺带。
- **REG-4 oracle 可复现性簇**：healpix_fullsky_oracle.py 缺失＝M6a-F-001；oracle 测试未挂 ctest＋容差口径差（test:95 的 1.2×hp_res+1e-9 vs STANDARDS_REGISTRY.md:139「≤1e-12 deg」）＝M2b-F-01(P0)；STANDARDS_REGISTRY:140「order≤29 显式拒绝」vs 代码无上界＝M2b-B-07(P0)。均不再计数。
- **REG-5 sha256 声明面**：sha256.h:2/:14-17「单一实现」与 4 份实现/8+ 模块并存＝CR-63-15；v6_atomic_publish.cpp:190-202 自持 sha256_file_hex 双通道＝CR-63-17（S2）。
- **REG-6 drizzle shim**：构造器空 if 假守卫（h:38-40）＝CR-12 已登（S3）；pixelToCoarse/Fine 静默退化＝CR-12-23 已登（S3；UB 增量见黄-9）。
- **REG-7 孤儿测试**：test_precision.cpp、test_hips_tile_mapping.cpp 列 AUD-401 orphan 清单已登；执行面补充：test_healpix_neighbors 已挂 ctest（eng/tests/unit/CMakeLists.txt:740-742）、tile_mapping 另有 sanitize_wsl_v5.sh:80-86 手动面、test_healpix_oracle 与 test_precision 仅 shared/Makefile 手动目标。
- **REG-8 D-01 关联**：spherical_overlap.cpp:30-31/:39 的 1.043827/1.044 注释属 drizzle 切片，已由 `检查-S25-alg-drizzle-core.md:231` 与 CR-36 复算件覆盖；DRIZZLE.md:108 红-3 已 PASS（1.0415+≥20.0%），不翻案。shared 侧无外接半径/sup 常数可与 D-01 冲突（grep 证实零命中）。
- **REG-9 第三份 healpix_core.h**：`lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/`——archive/legacy 零构建，AUD-401 R5/:191 已登。

---

## 7. 前轮已报、本轮确认仍未修（不重复计数）

- `docs/science/PSF.md:113` 错误码 `DPSF_ERR_PARAM` 全仓不存在——`独立审计/排查/全仓-01/检查-S3-scienceB.md:154-164`（S3B-黄5）已报；与同文件 :51 的 `DPSF_FIT_INVALID_PARAMS` 两说仍在。
- `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:1057` 的 1.144 注释——上轮 `检查-科学性.md:58`（G-2）与 `检查-跨文档冲突.md:76` 已点名「随下次代码整改轮更新」，PASS 表未含代码注释面；本切片按交接 §10 指定登记为代码修复单候选，**已计入红-1**（编号归本报告，此处注明上轮渊源）。
- 交接指明的三项专项核对结论：①1.144 注释现状已核并登记（红-1）；②snr_frame_science.cpp:87 的 2.5× 与 1.4826 冻结值——责任域全精度一致、A-P2-03 不重报（§5.1 ①面）；③shared vs drizzle healpix 双实现——已 shim 化、无数值漂移（§5.3 ④面），dpsf 判据非退化（§5.2 ①面）。
