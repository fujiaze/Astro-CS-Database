# 检查-S34-infra-acr-tests-browser-aio（全仓-01）

- **检查对象**：lib/infrastructure/acr/{qualification,tests,tools,examples} 全量；lib/infrastructure/hips_browser/healpix_browser_qt/ 全量；lib/infrastructure/aio/ 自研部分（排除 third_party/cfitsio 等第三方，仅接口面）；互查文档 docs/plugins/infrastructure/17_aio.md、20_benchmark.md、docs/plugins/infrastructure/23_hips_browser.md、docs/architecture/ASYNC_IO_CONTRACT.md、IO_AND_ATOMICITY.md、docs/modules/healpix_browser_qt.md。
- **检查面**：①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚（文档说有、代码没接 / 锚失真 / 引文）。
- **纪律**：纯静态检查（零构建/零测试/零 git 写）；仅写本报告一个文件。
- **判据基线（先读）**：独立审计/实验重做/总编对账/检查-修复验证.md PASS 表（红8/黄16，不重开已 PASS 项）；分歧台账.md D-01…D-11 终裁（未重开）；兄弟报告 S7/S10/S11/S12/S23 与 独立审计/01_文档/文档现状审计台账(.md/.csv)、独立审计/证据/AUD-101-DB-* 在册条目按"已登记不重报"处理，清单见文末【在册规避清单】。
- **计数**：红 5 · 黄 9 · 绿 3（共 17 条）。

---

## 一、红（必须改）

### S34-红-01 ｜ ① ｜ lib/infrastructure/acr/tests/classic/e15_failure.cpp:82、e20_fault_fallback.cpp:40、e20_fault_fallback.cpp:76 ｜ 三个"故障"实验 case 级判据恒真（恒真门）

**问题描述**：任务书明示"e15_failure、e20_fault_fallback、fault_injection 应能红能绿，恒真门即红"。三处 case 的判定值 bool ok = true 硬编码且函数体无任何使其为 false 的路径，"分配失败/内存耗尽"场景从未被注入。

**证据（对照来源 + 反方核验）**：
- e15_failure.cpp:68-87 run_allocation_failure：:82 bool ok = true; // 不崩溃即 ok；函数体 Buffer<float> b(256*1024*1024)（= 1 GiB 正常分配，:70 注释自认"验证正常路径"），:76-78 catch(...) 仅打印后 ok 仍为 true。
- e20_fault_fallback.cpp:29-45 run_oom_simulation：:40 bool ok = true；:33 分配 64 MiB（16*1024*1024 floats，注释"尝试分配 16M floats"）——非 OOM，:34-36 catch 不改变 ok。
- e20_fault_fallback.cpp:67-81 run_allocation_failure：:76 bool ok = true；函数体 = 100 轮 1024-int 正常分配循环（:71-74），无失败注入。
- 判据链：classic_common.hpp:281 make_result 以 r.correct = correct（=ok）定判，e15:156 / e20:188 等 EXPECT_TRUE(r.correct) ⇒ 这三个 case 永远 PASS、JSON status 永远 "PASS"。
- **反方核验**：进程级崩溃可由 ctest 外部判红（外部红路径存在），但不豁免 case 级判据——同目录 e15:44-61 run_kernel_exception 有真实 ev.status()==KernelFailed 断言、e16:57-74 mmap 有 munmap 失败→ok=false 循环，证明仓内存在非退化范式，本三处属判据缺失而非范式不同。

**建议改法**：按"负例注入能红能绿"补真实失败路径（分配器失败钩子/超限探测失败，或改为可判红的错误传播断言）；注入机制落地前不得以 ok = true 作为 PASS 依据。属 Phase H 实验规格问题，具体方案以规格/负责人口径为准（只登记不越权定案）。

---

### S34-红-02 ｜ ④（兼①） ｜ lib/infrastructure/acr/tools/acr_classic_runner/main.cpp:102 对 main.cpp:276-285、:322-325、:121-144 ｜ 退出码自述合同在两条路径上假绿

**问题描述**：--help 声明"退出码: 0=全部 PASS, 1=存在 FAIL/SKIPPED, 2=参数错误"（:102），两条路径违反该合同并静默返回 0。

**证据（对照来源 + 反方核验）**：
1. **实验异常→exit 0**：:276-285 捕获 std::exception 后只写 exp_summaries（es.failed=1），不向 all_cases 投放任何 case；:182-188 的 summary.failed、build_report 全部计数与 :322-325 的退出码循环只遍历 all_cases ⇒ 实验整体崩溃仍 summary.failed=0 且 exit 0（experiments[] 里虽有 failed=1，但退出码与 summary 均绿——自相矛盾）。
2. **未知/已删 ID→exit 0**：:121-144 把 -e token 规整后直接入 set，不与 kExperiments 表校验；:268-269 过滤后 0 实验运行 ⇒ -e E99（或已删的 E14/E19）→ 0 case → return 0，违反 :102"参数错误=2"。该工具在包装清单内（eng/tests/cli/test_cli_single_install.py:10 列 acr-classic-runner 为安装产物 exe），用户面可达。
- **反方核验**：异常路径的红信号在 experiments[].failed 可查（非完全无痕）；--list 可列合法 ID；eng/ci/checks.json 未把该 runner 注册为 CI 门（grep 0 命中）⇒ 影响限于该工具自身的资格运行语义，不牵连机器门。

**建议改法**：退出码判定改为 all_cases ∪ exp_summaries(failed>0)；-e 选定后校验每个 token 命中 kExperiments，未命中按参数错误 return 2（与 :102 自述一致）。

---

### S34-红-03 ｜ ④（兼②） ｜ lib/infrastructure/aio/src/aio_upm.cpp:101-108（对 aio_atomic_file.h:18-19 冻结禁令、hiss_stream_writer.cpp:174） ｜ Windows 分支"先删目标再 rename"＋失败后新旧双删＋注记与冻结禁令正面冲突（另附 fsync 缺口）

**问题描述**：UPM 模型文件写回的原子 promote 分支违反本模块自己声明的冻结禁令；其上方注记还断言了并不成立的可恢复性。

**证据（对照来源 + 反方核验）**：
- aio_upm.cpp:101-102：POSIX 路径 rename(tmp, path) 覆盖，合规。
- aio_upm.cpp:104-106：Windows 路径 if (std::remove(path)==0 && std::rename(tmp,path)==0) return 0; —— **先删目标再 rename**，正是 aio_atomic_file.h:18-19 冻结禁令（"**禁止先删目标再 rename** (hiss_stream_writer.cpp:174 冻结禁令: 删除后 rename 前崩溃 → 文件丢失)"）与 hiss_stream_writer.cpp:174 所禁；aio_atomic_file.h:17 规定 Windows 正范式 = MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)（hiss_stream_writer.cpp:177-187 已按此实现）。
- **失败路径双删**：:104 条件为假时落入 :107 std::remove(tmp); :108 return 1 —— 若 std::remove(path) 已成功（旧模型已删）而 rename 失败（Windows rename 常因占用失败），旧模型与临时文件双双被删：数据全丢。而 :84 注记"temp 仍完整，失败可恢复"、:103 注记"ENG-IO-001 单文件原子语义保持"——注记与实际行为矛盾（②内部矛盾）。
- **平台前提**：docs/ASTROCS_DESIGN §11 "Windows 官方工具链为 MSVC"、AGENTS §2 正式平台 Windows x64 ⇒ Windows 分支属正式平台路径，非边角。
- **生产调用链**：lib/algorithms/coverage/src/upm.cpp:1501-1503 调 aio_upm_write_sparse 落模型（生产可达；域外调用方仅作可达性证据）。
- **fsync 缺口（同条附注）**：aio_upm.cpp:86-111 全程仅 ofstream flush()+close()＋rename，无 fsync/_commit（grep "fsync" 于 aio_upm.cpp = 0），对照 docs/plugins/infrastructure/17_aio.md:26"所有产品：临时文件/目录 + 校验 + fsync + 原子 rename 提交"与 docs/ASTROCS_DESIGN §10 :713。**反方核验**：IO_AND_ATOMICITY.md:12 对 UPM 的登记口径是"temp write → validate → atomic promote"（无 fsync 字样），且台账 DB-10-补已判 UPM temp+rename 挂账闭合——UPM 是否落入"所有产品 fsync"范围存在口径分歧，故 fsync 部分不单独定红、随口径裁决处理；**红立足于先删后 rename＋双删＋注记不实**。
- **反方核验（主罪）**：POSIX 正式路径无此问题（Linux rename 覆盖）；本条仅在 Windows 触发，但恰是官方平台之一。

**建议改法**：Windows 分支改用既有 aio_atomic 原语（write_file_atomic / MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)，aio_atomic_file.h:102/:153 已提供），删除或订正 :84/:103 两处与事实不符的注记；fsync 范围按上呈口径裁决（登记不越权）。

---

### S34-红-04 ｜ ④（兼③） ｜ docs/plugins/infrastructure/17_aio.md:7、:9、:25 ｜ 归档形态 <name>.hips.zst 读侧"形态解析层"与两处"落点"声明，代码零实现（台账只登记了写出键，读侧声明无人登记）

**问题描述**：17_aio 以现在时声称 aio 具备归档读取能力与两处形态事实落点；实际仓内既无归档读取实现、也无任一落点写出，机器台账明确把该能力登记为"阶段 2 计划"——文档与代码、台账三方冲突。

**证据（对照来源 + 反方核验）**：
- 文档主张：:7"**形态解析**（裸 HiPS / zstd 归档包）也在此层完成"；:9"裸 <name>.hips/ 直接按路径读；**归档 <name>.hips.zst 按产品级索引的定位表 pread 单帧解压交付瓦片**……调用方（Phase2/Phase3）不感知形态"；:25"（name）.hips.index.json 与运行完成清单 manifest.json 的 storage 段是形态事实的唯一落点……**落点 = 上述两处**"。
- 代码核验（分开 grep，规避 alternation 假阴性）：
  - grep 'zst' lib/*.cpp/*.h（非第三方）→ 命中仅 cli/session_commands.h:214（注释：由命名规则派生索引路径）与 ahpx 的 zstdLevel（容器参数，非 .hips.zst）；归档写出/读取实现 0 命中；
  - grep index_path / index_sha256 / archive_sha256 于 lib/ → 0 命中；"storage" 作为形态段仅在 scheduler/src/artifact.cpp:114/:181 出现，但那是 DataArtifactDescriptor 的 storage（位置串），不是 hips_storage_form.schema.json 词表（storage_form/index_path/index_sha256/archive_sha256/archive_bytes/tree_hash）——词表字段在 lib 写出侧 0 命中；
  - storage_form 仅 cli/parser.cpp:283-288、cli/session_commands.h:168-176 两处（CLI 输入键识别），aio/orchestrator/writer 0 命中。
- 台账对照：eng/ci/ledgers/dead_config_keys.json:157-161 dead_config_key:storage_form 明确登记"**生产写出侧零读取**……两形态、逐成员 zstd 帧、**产品级索引写出**属……**阶段 2**；exit_condition = manifest.storage.form_source 记 default"——即该能力在册状态为未实现。
- **反方核验**：① docs/ASTROCS_DESIGN §10 :715 同样现在时写"Phase1 默认（archive）"（上位同向，不构成豁免，反而说明这是 L1↔代码的正面差距）；② PRODUCT_STORAGE_FORM.md §7 :151 与 HIPS_STORAGE_FORM_CONTRACT.md 也未标实现状态（同族）；③ 检查器 eng/tools/hipsform/check_hips_storage_form.py 与门 CHK-HIPS-STORAGE-FORM 只跑 --self-test（docs/ci/01_CHECKS.md:137），不验产品，不能当实现存在证据；④ 台账只登记**写出键**，:9 的**读侧能力声明**与 :25 的两处落点声明不在任何在册条目内。
- 影响：下游据 17_aio:9 会认为归档产品可被 aio 读取；实际任何 .hips.zst 产物当前无人可读（写出侧亦无），且 Phase 实际全部为裸形态。

**建议改法**：17_aio:7/:9/:25 改为带状态的条款（"归档读侧 = 阶段 2 计划，见 dead_config_keys:storage_form"）或按变更流程先登记实现缺口；两处"落点"在实现前不得写成现状。上位 §10"Phase1 默认 archive"与代码差距属上呈事项（登记，不越权裁决）。

---

### S34-红-05 ｜ ④（兼③） ｜ lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1938-1957、:2004 ｜ metadata.fits 与 SNR TSV 瓦片直写正式路径——R13 真正的残余非原子面，未被任何登记覆盖

**问题描述**：tile 本体已走 write_fits_atomic（temp→校验→fsync→打洞→原子 rename→父目录 fsync，:539/:580/:624），但同一发布流程里 metadata.fits 与 SNR 目录表瓦片仍直接写正式路径，违反 docs/ASTROCS_DESIGN §10 :713"**所有产品**（含 HiPS tile）走：临时区 → 校验 → fsync → 算哈希 → 原子改名发布"与 17_aio.md:26。这两处是 R13 真正未闭合的残余，却无人登记。

**证据（对照来源 + 反方核验）**：
- aio_hips_writer.cpp:1934-1957：std::string mp = dir + "/metadata.fits"; std::remove(mp); fits_create_file(&mfptr, mp.c_str(), &status) —— 对正式路径先删再建、写在正式目录，无 temp、无校验、无 fsync；metadata.fits 为 NAXIS=0 头文件，无 DATASUM/CHECKSUM。
- aio_hips_writer.cpp:2004：FILE* f = std::fopen(p.c_str(), "wb");（SNR catalogue 瓦片直写正式路径；同文件其余 fopen :2490/:2515/:2650 为读）。文件头 M9-G-6/AIO-001 注释（:668）曾自述同类问题"原实现 fopen 失败即静默 return……且直写正式路径"——该反模式在本文件已有先例修复记录，这两处是残余。
- 上位对照：docs/ASTROCS_DESIGN §10 :713；17_aio.md:26；PRODUCT_STORAGE_FORM.md:153（裸形态写出 = "临时区 → 校验 → 哈希 → 原子改名 → 完成清单"）。
- **反方核验**：IO_AND_ATOMICITY.md:13-19 R13 把"tile 非原子"记为未闭合，但其锚 :330/:404 现指原子包装内部的 tmp remove（失效，S10-红已报登记面）；S10 建议改法把残余指认为"raw/MOC helper"——实测 write_fits_image_raw / write_moc_fits_raw 仅被 write_fits_atomic 内部调用（:643/:660，全 lib 无外部调用者），不是残余；真正的两处直写面在全部兄弟报告 grep "metadata.fits" = 0 命中 = 未在册。缓解项：完成清单 gating（A2 fail-closed）使半成品不被消费——但 R13 对 tile 的判词（正式目录只应出现完整产品）对这两文件同样成立。

**建议改法**：两处纳入既有 aio_atomic 原语（temp→校验→promote）；R13 行把缺口对象从（失效锚指的）tile 换成这两处真实直写面后再行闭合。与 S10 的登记面订正互为补充、不重复。

---

## 二、黄（建议改）

### S34-黄-06 ｜ ① ｜ fault/e15/e20 五处"场景名/注释 vs 实际未注入"与子断言恒真

**证据**：
1. tests/fault/fault_injection.cpp:3、:26、:29-38：场景 1"取消**正在执行的** kernel"——:33 ev.wait() 后才 :34 cancel()，kernel 早已完成；:35 EXPECT_TRUE(ev.cancelled()) 实为 set-then-read 自证（event.cpp:41 置位 → :52 读）。:36-37 断言 cnt==1000 / status==Ok 能红，故整测非恒真，但主张的场景未被执行。
2. fault_injection.cpp:52、:55-74：注释/测试名"设备失败 → CPU 回退"，:60 cfg.devices = {{"cpu",0,0,50.0,true}} 只有 cpu 设备、无任何失败注入——实为 CPU 正常分发测试，FallbackPolicy 真实失败路径零覆盖。
3. tests/classic/e15_failure.cpp:137、:121-150 run_cancel_dispatch：注释自认"完成校验即退出（不实际派发队列任务）"，名 cancel 实无 cancel 动作。
4. e20_fault_fallback.cpp:153-184 run_mixed_fallback_to_cpu：注释"设备失败 → CPU 回退"，:159 同样只有 cpu、:177 只断言 all_done && failed==0 && sum==100——happy path 冒充回退。
5. e20_fault_fallback.cpp:126-151 run_fallback_no_replay：:128-131 专建 bitmap 标记 3 个已完成块，但 ok（:143-145）只断言 decision.strategy / target_backend / skip_already_done——其中 skip_already_done 在 scheduler/fallback.cpp:20 硬编码恒为 true（grep 实测），而 decision.pending_chunks（bitmap 的真正作用点，fallback.cpp:21）从未被断言 ⇒"已完成块不重复"无判据覆盖。

**反方核验**：五处所在文件整体均存在能红断言（fault 的异常传播/失败计数、e15 的 kernel_exception、e20 的 launch_failure），故不判红；恒真门已由 S34-红-01 单列。
**建议改法**：场景注释/命名与注入事实对齐；no_replay 补 pending_chunks==7 类断言；mixed_fallback 构造不可用设备（enabled=false 或故障注入）或改名"正常分发"。
**所属面**：①（判据非退化与名实）。

---

### S34-黄-07 ｜ ① ｜ e16_failure.cpp:76/:99/:119、e21_persistence_concurrency.cpp:149/:213/:186-200 ｜ "不崩溃即 ok"族与"称 SKIPPED 实为 PASS"

**证据**：e16 三处（mmap / callback_shm / detached_thread）与 e21 的 racy_task_queue:149、run_no_leak:213 均 bool ok = true（注释"不崩溃即…"），case 级判据恒真（红路径仅进程崩溃/ASan 进程级退出）；e21:192-194 注释"sanitizer 未开启时**仅标记 SKIPPED**"，但 :197-198 status = ok ? "PASS" : "FAIL" ⇒ 未开启也产出 status="PASS"（注释与行为矛盾；runner 按 acr_classic_runner/main.cpp:292 计入 passed）。
**反方核验**：e07/e08/e09/e04 中同样形如 ok=true 的判定均处于含真实 mismatch/除零守卫的循环内（合法范式），本族无任何失败值来源；sanitizer 条件性本身是登记过的设计（ACR_BUILD_SANITIZER 默认 OFF），问题仅在 PASS/SKIPPED 标签。
**建议改法**：:198 未开启分支产出 "SKIPPED"；"不崩溃"族至少把关键不变量（mmap 内容回读、队列完成数）纳入 ok。
**所属面**：①。

---

### S34-黄-08 ｜ ① ｜ healpix_browser_qt/tests/test_geometry_truth.cpp:226-227 / :231-232 / :167-170 ｜ 三条子判据恒真或 any-of 弱化

**证据**：:226-227 cache_bounded ≡ sky.target_order() >= 0 —— hips_sky_view.cpp:152-159 把 order clamp 在 [0, leaf]，恒真；且未触及文件头 :7 主张的"LRU cache 有界 + eviction"（cap=8 在 hips_sky_view.cpp:200，缓存是否越界从未被检查）。:231-232 raster_nonempty ≡ rgba.size()==640*480 —— gl_renderer.cpp:289 在任何分支前无条件 rgba.assign(w_*h_*)，恒真。:167-170 seam 判据 5 个采样点任一 e<0.03 即 seam_ok=true（any-of-5，非全过）。
**反方核验**：同测试其余子判据非退化：cache_evictions>0（:220，cap8 vs order0 12 tiles 必触发）、cache_hits>0（:222，12 次 rasterize 必回压）、:215-217 wrap 与正北正南方向、:166 ZERO_REGION 全图过——故不判红。
**建议改法**：cache_bounded 改断言 cache_.size() <= cap（暴露只读查询）或以 evictions ≥ 12−8 界定；raster_nonempty 改查均值/非均匀性；seam 改全点通过。
**所属面**：①。

---

### S34-黄-09 ｜ ④ ｜ docs/modules/healpix_browser_qt.md:27、:43 ｜ "后台 tile I/O"无实现、Tests 节与实况不符

**证据**：:27 "Qt 主线程 + **后台 tile I/O**；GL 单线程"——grep 'std::thread|std::async|QThread|QtConcurrent|future' 于 healpix_browser_qt 全目录 = 0 命中（分开跑确认）；tile 读与 rasterize 在调用线程同步执行（OpenMP 并行 ≠ 后台 I/O 线程）。:43 Tests 节只写"STF engine 单测；视觉验收"，实际 tests/ 5 个测试源 + CMake 注册 5 个 target（geometry_truth / healpix_math / browser_backend / hips_browser_backend / stf_engine）。
**反方核验（同文档正核项）**：:39 STF "0.5%/99.5% 分位" ✓ 实现于 core/stf_engine.cpp:124-130；:35 Data contract 读侧走 aio ✓（browser_backend.cpp:9 include aio_healpix_io.h、hips_browser_backend.cpp:15 include aio_hips_reader.h、CMake :47/:60 链接 astro_image_io）；"GL 单线程" ✓。
**建议改法**：:27 删"后台"或改"调用线程 + OpenMP 并行采样"；:43 补测试清单。
**所属面**：④。

---

### S34-黄-10 ｜ ① ｜ healpix_browser_qt/core/gl_renderer.cpp:1436-1444 ｜ "相邻像素刚好拼接无重叠/无缝隙"几何论证不成立（独立复算）

**证据**：:1437 h = sqrt(M_PI/6.0)/nside*1.02、:1440 注释称"相邻像素**刚好拼接**，无重叠/无缝隙"、:1439 称四角为 n=0 邻域的"上下左右"（十字方向）。独立复算（python astropy_healpix，nside=4/8、ring 像素 0）：同一环东西向邻接像素边界全跨度 = 22.52°/11.26°，南北向 = 19.48°/9.61°（各向异性 ~1.15×）；等面积菱形半对角 h×nside = 10.77°（含 1.02 裕量），2h = 21.14°/10.57° ⇒ 东西向留 ~6% 像元宽的缝、南北向重叠 ~8–9%——"刚好拼接"两头不成立（菱形面积对：复算 223 deg² vs 理论 219 deg²，面积等、镶嵌不等）。
对照 D-01 口径：hp_res = sqrt(pi/3)/nside（DRIZZLE.md:25），D-01 裁定 sup 中心→角点 = 1.0415·hp_res（N=64 穷举）= 15.27°@nside4；本处把"十字方向半对角"（10.77°）当像元角点——对象不同（角点应在对角方向；复算对角半径 ≈14.9°@nside4，与 1.0415·hp_res 一致）。注释的"无重叠无缝隙"与 D-01 外接半径语义互斥。
**反方核验**：memory.md:51-54 记录 1.25→1.02 修的是 drizzle **数据面** 64% 覆盖造成的缝（像素画布/UV 问题），与本条**渲染几何镶嵌**的缝是两个问题——该历史记录不能为 :1440 注释背书；三角形朝向与环错位可能部分遮盖南北重叠，但东西向同环缝隙无静态遮盖机制。渲染为显示型降级（23_hips_browser:25"显示型降级产物，不产生 Phase1/2/3 正式数据"），不涉测量，故不判红。
**建议改法**：订正 :1439-1440 注释为"等面积近似 + 2% 裕量；各向异性残差（东西向缝约像元宽 6%）以视觉验收为准"，删绝对化"刚好拼接"；若需闭合残差属渲染参数变更，另行立项（不越权改公式/参数）。
**所属面**：①。

---

### S34-黄-11 ｜ ④ ｜ tests/classic/CMakeLists.txt:2-3/:18/:66、tools/acr_classic_runner/main.cpp:2、tests/classic/classic_common.hpp:2 ｜ "E01-E21 / 21 files / run_e14 占位"三处口径与实况（19 项）不符，规范锚悬空

**证据**：CMakeLists.txt:2 "run_e14 接口保留为空占位"、:18 "run_e13/**run_e14** 函数名保留（…classic_main.cpp 依赖序号调度）"——grep run_e14 全仓 = 0 命中（无声明、无定义、无引用，"保留占位"无实体）；:66 message "21 files" vs 实际 19 源（:2-3 自述 E14/E19 已删）；runner main.cpp:2 "运行 E01-E21 全部经典实验" vs kExperiments 19 项（E01-E13,E15-E18,E20,E21）；classic_common.hpp:2 规范锚 " 09_PHASE_H_CLASSIC_EXPERIMENTS_SPEC.md" 全仓不存在（find 0 命中；工程控制/ 仅 RELEASE-05）。
**反方核验**：注册面本身一致——19 源 ↔ 19 add_executable、unit 27 文件 ↔ 27 add_executable、fault 4 / integration 1 全注册（gtest_discover_tests 齐），故只是文案与锚问题。
**建议改法**：三处口径统一为 19 项并删 run_e14 残句；规范锚改指现存权威或登记 09_PHASE_H 规格去向。
**所属面**：④。

---

### S34-黄-12 ｜ ④ ｜ lib/infrastructure/aio/src/hiss_stream_writer.cpp:173（被 aio_atomic_file.h:18 转引） ｜ 冻结禁令的权威出处 00_COMMON_CONTRACTS 全仓不存在

**证据**：:173 "依据 **00_COMMON_CONTRACTS §4.6**：正式目录只允许出现完整产品……不能先删除旧文件再 rename（删除后 rename 前崩溃 → 文件丢失）"——find（iname *common*contract*，排除 .git/run/build）= 0 命中；grep COMMON_CONTRACTS 于 docs/、工程控制/、独立审计/排查 = 0。禁令本体在代码注释中自持（aio_atomic_file.h:18-19），但其援引的上位条款文件断链。
**反方核验**：禁令实质与 S34-红-03 的执行依据不依赖该文件存在（两处注释互相引用）；但权威出处断链妨碍追溯（AGENTS §1.1 要求能答"依据哪份文档哪一条"）。
**建议改法**：引用改指现存上位条款（docs/ASTROCS_DESIGN §10 或现行契约）或登记 00_COMMON_CONTRACTS 的去向/合并记录。
**所属面**：④。

---

### S34-黄-13 ｜ ②（兼④） ｜ lib/infrastructure/hips_browser/PENDING.md:2-5、healpix_browser_qt/README.md:3/:17 ｜ 目录状态文件与 README 对自身目录的描述失实

**证据**：PENDING.md:2 "**本目录为空，未含任何实现；不代表模块已实现或可用**"、:4 "迁移清单登记为 **PENDING**"——实况：目录含完整 healpix_browser_qt/（core/widgets/app/tests/tools/include 六层、50+ 文件），且 eng/cmake/ARCH-001-migration-manifest.md:22 行 12 同一迁移项状态 = **DONE**（同一对象两份状态互斥）。README:3 "旧架构已归档至 ../archive/"、:17 "archive/single_frame_view.*"——目录树实测两路径均不存在。
**反方核验**：grep PENDING.md 于 docs//工程控制 = 0 引用（无工具消费，不判红）；MODULE_MAP.yaml:825-828 已登记 hips_browser 缺 README/module.yaml 等缺口（在册，不重报）。
**建议改法**：PENDING.md 按 ARCH-001 DONE 现实改写或删除；README 归档指针改指现存落点或删。
**所属面**：②。

---

### S34-黄-14 ｜ ④ ｜ 域外 ｜ docs/contracts/PUBLIC_API.md:1769-1776、docs/modules/registry/astrocs.phase2.upm-fit.md:118/:126、lib/algorithms/upm/README.md:65-67 ｜ p2_upm_save/open 行锚家族整体失锚

**证据**：三处声称 p2_upm_save = upm.cpp:940-1006（内部 aio_upm_write_sparse :941/:943-945/:1003-1005/:1027-1028 等）、p2_upm_open = upm.cpp:1008（upm.h:108/:109）——实测 lib/algorithms/coverage/src/upm.cpp：p2_upm_save = :1383、p2_upm_open = :1506、p2_upm_close = :2218，声明在 upm.h:134-135；aio_upm.cpp 全文 586 行，:941/:1005/:1028/:1234 全部超界。三处一致地错且无历史基线标注。
**反方核验**：上述现行行号经 grep p2_upm_save / p2_upm_open 逐一命中；本域（acr/aio/browser）文档无引用这些锚者——按任务规则标「域外」登记，交对应片处理。
**建议改法**：三处重锚至现行行号或改符号引用。
**所属面**：④。

---

## 三、绿（可选）

### S34-绿-15 ｜ ④ ｜ lib/infrastructure/aio/src/aio_cfitsio_mutex.h:9 ｜ cfitsio 引文语义正确、行锚微漂

引文 "cfileio.c:1544: if (mode == 0) return(*status);" —— 实际 third_party/cfitsio/cfileio.c:1550-1551（两行）同语义，:1540-1548 为说明注释；fitscore.c:766 FFLOCK ✓。语义逐字相符（READONLY 不复用句柄 → 线程安全前提成立），仅行号偏 6 行。建议顺手更锚。

### S34-绿-16 ｜ ④ ｜ 代码侧 docs/ASTROCS_DESIGN §9→§10 条款号残余 4 处（同族已裁 DB-04，仅登记代码面残余）

文档侧 §9→§10 家族已由台账 DB-04 登记（IO_AND_ATOMICITY 6 处 + ARCHITECTURE 2 处 + ASYNC 1 处，判"虚构引文本期例外"），本条不重开；代码注释面未列入该条：aio_atomic_file.h:8、aio_pipeline.h:222/:239、aio_hips_writer.cpp:669/:2364 仍写 "docs/ASTROCS_DESIGN §9"，而同文件 aio_hips_writer.cpp:443 与 test_hips_atomic_publish.cpp:10 已用 §10——同一文件两种条款号并存。建议随 DB-04 订正时连带改（仅注释，不构成新冲突）。

### S34-绿-17 ｜ ④ ｜ healpix_browser_qt/CMakeLists.txt:45/:111（死 include 路径）＋ acr/tools/acr_benchmark/main.cpp 默认输出落 CWD

CMakeLists:45/:111 target_include_directories 引 ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/scheduler —— 该目录不存在（目录树实测），无害死路径；acr_benchmark/main.cpp 默认 --output hardware-profile.json 落 CWD（对照 AGENTS §6"输出落 output_dir 或 run/"），属开发工具默认值且可用 --output 覆盖。与 20_benchmark.md 的"安装目录 cpu_profile.json"主张**无关**（后者由 lib/infrastructure/cli/commands.cpp:2343-2397 正确接线：安装目录解析 + :2386 写 install_dir + "/cpu_profile.json" ✓ 已核）。各改一行即可。

---

## 四、已查无问题面（按①②③④，含抽样方式）

### ①科学性（判据非退化、常数/公式/单位/域）
- **对照 D-系终裁未重开**：test_geometry_truth 全文无 HP_CIRCUMRADIUS_FACTOR / 1.25 / 外接半径类常量（无 D-01 冲突面）；truth 模型生成器 gen_geometry_truth.py 的网格折半（frac<0.015|>0.985 → −0.30）、marker +0.30（0.55° 内）、ZERO_REGION(178,182,−1.5,1.5) 与测试容差（seam 0.03、ev<0.001 等）逐项对拍一致——抽验 test_geometry_truth.cpp:88-218 全部 16 个子判据与生成器公式。gl_renderer 的 h 推导问题已单列黄-10（与 D-01 对照后登记，未重开 D-01 终裁）。
- **acr 判据非退化抽验**：fault 4 文件全扫——fault_injection 4 TEST（异常传播 :44-49、失败计数 :80-100 均真实断言）、lifecycle_smoke EXPECT 17 处（move 语义/线程计数/事件就绪）、persistence 8 处（含 profile_state Missing/Corrupt 双向）、sanitizer_actual 6 处 EXPECT_DEATH（构造"ASan 未捕获则正常退出 → DEATH 断言必败"的非退化回路）；integration test_weighted_integration 容差断言（max_abs ≤ 2e-5、rel_l2 ≤ 2e-6）与 GPU/CPU 分布双向断言（:199-200 / :228-231）非退化；unit 套件抽 6 文件（engine/dispatcher/executor/fallback/partitioner/api）均为行为断言。恒真点已全部列入红-01 / 黄-06 / 黄-07，无漏抽。
- **科学常数与单位**：stf_engine 0.5%/99.5% 分位 ✓；healpix_math（pix2ang/ang2pix/query_disc/ud_grade）由 test_healpix_math 与 geometry_truth 双路覆盖；本域未触碰权重/variance/ivar/SNR 定义面（无 SCI/ALG 冻结定义涉及）。

### ②行文逻辑（论证链、内部一致、订正注记、UNRESOLVED 混入）
- 本域六份文档（17_aio、20_benchmark、23_hips_browser、ASYNC_IO_CONTRACT、IO_AND_ATOMICITY、healpix_browser_qt）通读：无嵌套"注：修正了原判断"式残留，无 UNRESOLVED 段落混入结论节；检查-修复验证.md PASS 表涉本域的项未重开。
- IO_AND_ATOMICITY 的行文矛盾类问题（R13 锚失效、协议 3 步 vs 5 步、:32 表结构漂移）已由 S10 / 台账 DB-04 在册，本片核对后未重复报（见【在册规避清单】）。
- 20_benchmark 的"相对/绝对路径重复陈述"（:33/:45、:59/:67 同文两节）属重复但同义，未达矛盾阈值，不计。
- PENDING.md 与 README 的自述失实单列黄-13（②面唯一在报项）。

### ③跨文档冲突（同一数量跨文档不一致、五方权威链）
- **20_benchmark ↔ 实现**：§3/§6 "cpu_profile.json 落安装目录" ↔ lib/infrastructure/cli/commands.cpp:2343-2397 逐行吻合（从可执行路径解析 install_dir、:2386 落盘）；§8 测试存在性 ✓（eng/tests/unit/cpu003_profile_v2_test、cpu007_profile_store_test、cli_bench_verdict_test、cpu_profile_test、eng/tests/backend/test_cpu_profile.py 均在）。
- **17_aio §4/§8 ↔ PRODUCT_STORAGE_FORM ↔ 代码**：打洞链"临时文件 → 内容写出 → 校验 → fsync → 打洞+读回复算 → 原子 rename → 父目录 fsync"三方一致（aio_hips_writer.cpp:539/:580/:624、aio_sparse_punch.h 四函数在位；S9 已抽锚通过）；tile 非原子的登记面问题属 S10（未重开），其代码侧残余由红-05 承接。
- **ASYNC_IO_CONTRACT ↔ aio 线程/句柄模型**：§3 "READONLY 可安全共享" ↔ aio_cfitsio_mutex.h:10-11 进程级互斥（比合同更严，兼容不冲突）；fopen 句柄进程级共享 ↔ reader 全部 READONLY（aio_fits.cpp:532、aio_hips_reader.cpp:173/:222/:262/:324/:624），无写者持读句柄；write_fits_image / write_moc_fits_raw 仅被 write_fits_atomic 内部调用（:643/:660，全 lib 无外部调用者）。
- **healpix_browser_qt 文档 ↔ 实现**：Data contract / STF 分位 / GL 单线程 / astro_image_io 链接全部核实（黄-09 已列仅两处失实）；23_hips_browser:19 的 hips_product.schema.json 悬空引用为在册（台账 DB-04:461、MODULE_MAP.yaml:828、AUD-101-DB-04），未重报。
- **口径冲突扫描**：本域内 hp_res 倍数/阶数/深度等量跨文档同值（DRIZZLE 1.2658、SUBPIXEL 1.4142、层级 12/9 仅作本片阈值语境且一致）；acr_benchmark 的 hardware-profile.json（ACR 内部指纹）与 cpu_profile.json（产品 profile）是两个不同产物，无冲突。

### ④幻觉与锚（文档说有/代码没接、锚失真、引文核验）
- **注册面正核**：unit 27 文件 ↔ 27 add_executable（逐一 set 对拍）；classic 19 ↔ 19；fault 4 ↔ 4（含 ACR_BUILD_SANITIZER 条件与 gtest_discover_tests）；integration 1 ↔ 1；browser 5 test ↔ CMake BUILD_TESTS 5 target；aio/tests/CMakeLists.txt:40 注册 test_hips_atomic_publish 且经 eng/tests/unit/CMakeLists.txt:598 add_subdirectory 可达（首轮局部 grep 空为假阴性，已用全仓 grep 复核）；tests/__pycache__/*.pyc 未入库（git ls-files 空）。
- **引文/锚正核抽样**：DRIZZLE.md:25 hp_res ✓；ARCH-001-migration-manifest.md:22-25 行 12-15（hips_browser/aio/io/orchestrator）DONE ✓；BUILD_GRAPH.md:97-102 browser 测试计数与 CMake 一致 ✓（standalone project() 由 :97-98 登记为工具面，非缺陷）；ASYNC_IO_CONTRACT §8 的测试存在性 ✓（async_io_test.cpp 10 TEST、eng/tests/cli/test_parallel_queue.py 6 用例；测试名差异已由 S10 报）；17_aio:38 列的各写出器实现 ✓（aio_fq_writer 等真实存在）。
- **文献面**：本域六份文档全文扫描无 DOI / arXiv / 外部文献引用，无可核幻觉。
- **死键扫描（本域）**：storage_form 读取面 = 0（已入红-04 作为对照证据）；EXECUTE_INLINE 经 verify.cpp 注册为"演示期工具"且有完整登记链（PASS 表红7 范围），未重开；acsd_v6_dry 死键属 orchestrator 片（域外）。
- **构建图**：healpix_browser_qt 顶层 project() 不在根 CMake（BUILD_GRAPH:97-98 已登记工具面）；根 CMake BUILD_TESTS 默认 OFF 下 5 测试不进默认图（与 :99 说明一致），非缺陷。

---

## 五、在册规避清单（本报告刻意不重报的条目及出处）

| 事项 | 在册出处 | 本报告处理 |
|---|---|---|
| IO_AND_ATOMICITY R13 行"未闭合"与 :330/:404 锚失效 | S10-红（检查-S10-architecture.md:47-54）＋台账 DB-04 | 不重报；只承接代码侧残余（红-05） |
| IO_AND_ATOMICITY:12 aio_upm.cpp:60-97 锚漂移 | S10（:144）＋ DB-04 | 不重报 |
| IO_AND_ATOMICITY:33 aio_pipeline.h:205-247 锚漂移 | S10（:145）＋ DB-04 | 不重报 |
| §9→§10 条款号族（文档 9 处）、"虚构引文本期例外"、协议 3 步 vs 5 步 | 台账 DB-04（docs 端）；S12:145 声明不重计 | 文档端不重报；代码端残余 = 绿-16 |
| ASYNC §8 测试名 CancelWakesBothBlockedSidesNoDeadlock | S10（:180） | 不重报 |
| io_workers 池 vs §9 "一个专用线程"分歧 | DB-04 登记为上呈项 | 不重报、不裁决 |
| 17_aio:37-39 config 表；23_hips_browser:7/:33-34 color_map/stretch/时态 | S7-黄-03/04 | 不重报 |
| 23_hips_browser:19 hips_product.schema.json 悬空 | 台账 DB-04:461、MODULE_MAP:828、AUD-101-DB-04 | 不重报 |
| lod_max_order runtime_policy 未登记 | DOC-SCI-001 类 C-1 | 不重报 |
| test_browser_dual_dtype 未入 CMake（Makefile:41 有） | WIRING-AUDIT-01 / AUD-401 | 不重报 |
| aio_upm temp+rename 挂账"已闭合"（IO_STANDARD 端订正） | 台账 DB-10-补 | 不重报；红-03 只报先删后 rename/双删/注记这一未登记新面 |
| EXECUTE_INLINE 演示期工具登记、三门 FAIL 重报禁令、k_shape 纸面红 | 检查-修复验证.md PASS 表 | 不重报 |
| stage2.cpp / sampler.cpp 指错落点（ASYNC §9"待接入点"） | 台账 DB-04 | 不重报 |
| check-coverage-alignment 无门接线 | DB-04（S7 引） | 不重报 |

---

*报告完：红 5 / 黄 9 / 绿 3；红黄均附文件:行与反方核验；科学公式/默认容差/冻结定义未越权改判（红-01 注入方案、黄-10 渲染参数、红-03 fsync 口径均只登记不裁决）。*

