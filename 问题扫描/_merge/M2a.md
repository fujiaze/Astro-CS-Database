# M2a 合并验证记录 — 球面 Drizzle 重建与不确定度 + Gaia 星表客户端与数据/产物语义合同

> 代理：M2a（第二层合并验证）· 域：L04（Drizzle / 不确定度 / 单位与文档语义）+ L10（Gaia 星表客户端 / 数据与产物语义合同 / FITS 核心）
> 纪律：纯只读（read / grep / glob）+ run_code 纯数学复算；禁 shell / 构建 / 测试 / git；只写 问题扫描/findings/** 与 问题扫描/_merge/M2a.md
> 复核基线：**全部条目按当下树重新定位**（不信叶子行号，位置改写 path::符号）；叶子引用的 commit 哈希（30b5516d / 09ee5363 / babe752d / 790e0315 / 9a3b5a7d / f7fa3160 等）一律不作事实使用，涉改之处写「代码现状 vs 文档记载失实」。
> 状态：**已定稿**。45 条叶子条目全部处置完毕，定稿 46 条 finding（3 条为本轮新增、1 条拆分、3 条合并），分落 9 个类别码目录下的 M2a_L04_L10.md（见 §F 产出清单）。

## A. 逐条处置表（四状态分表，45 条叶子条目每条只出现一次）

### A.1 仍成立（40 条 = L04 17 + L10 23）

| 来源 | 摘要 | 定稿去向（ID · 类别 · 优先级） | 当下树复核要点 |
|---|---|---|---|
| L04-002 | pixfrac 归一化与常数场不变量互斥 | M2a-A-1 · A_SCI_DEF · P0 | 推导自证（§B.1）；代码按 SCI §5 逐字落地，缺陷在冻结合同 |
| L04-003 | variance≤0 整像素静默丢弃 | M2a-H-2 · H_NUMERIC · P0 | 现文 :1733-1736 continue + :1739 之后才计数；三处文本互斥逐条核对 |
| L04-004 | SCI §11 Oracle 测试未注册 + 自证 | M2a-F-1 · F_TEST_GAP · P0 | 注册面仅 p1drz 六门；9003 例与 4563(≈51%) 本代理独立复算一致 |
| L04-006 | 悬空文件/函数/文档锚（api.cpp、finalize_tile、02_FROZEN…） | M2a-E-1 · E_TRACE_BREAK · P1（与 L04-007/L10-011 合并） | glob/grep 逐项 0 命中复现；第 7 子项剔除见 §C |
| L04-007 | descriptor 引用不存在的 SCI/ALG/TEST ID | M2a-E-1（同条实例 5） | module_adapters.cpp:730-734 vs registry 页自述直接矛盾 |
| L04-008 | lib/drizzle 状态声明与建成事实相反 | M2a-C-4 · C_DOC_CODE_GAP · P1（类别改判 I→C） | README:9/:21、module.yaml:21/:24/:34 vs CMakeLists:184 + lib/drizzle/CMakeLists:60 |
| L04-009 | pixfrac 定义域在接口层被扩为 [0,1] | M2a-B-3 · B_STD_MISMATCH · P1（范围收窄） | DISP-DRZ-003 已登记双轨 ⇒ 本条只定稿迁移面固化 + validate/plan 放行 + 头注释 :37/:57 vs :87 自相矛盾 |
| L04-010 | 面积/方差单位链 px²↔sr + tile 无 BUNIT | M2a-A-2 · A_SCI_DEF · P1 | writer 全文 grep BUNIT → NO HIT；两合同单位互斥（不与 L10-001 合并） |
| L04-011 | pixfrac 默认值 0.8 vs 1.0 分裂 + 杜撰锚 | M2a-C-6 · C_DOC_CODE_GAP · P1 | PUBLIC_API API-DRZ-001 全节无默认值条款（grep 实证） |
| L04-012 | 极区常数注释 211034.6 等 | M2a-D-1 · D_COMMENT · P2（范围收窄） | 真值 ≈211076.3（本代理复算）；:1598 注释 1.25 vs :1621 代码 1.15 |
| L04-013 | 累加器字段数/位宽注释互斥 | M2a-D-2 · D_COMMENT · P2 | 头 :62-63 三字段/12B vs :64-70 四字段；module_entry.cpp:551 「6 个 f64」 |
| L04-014 | support>1 静默钳 + 层级反乘 | M2a-H-3 · H_NUMERIC · P2 | writer:526 钳制（合同已规定）→ 定稿残余为 :596-597 反乘破坏闭合且无计数 |
| L04-015 | test_drizzle_oracle 名不副实 | M2a-F-5 · F_TEST_GAP · P2 | 现文头部自述 SYN-004 平面原语（OVERLAP/NORMALIZE/ACCUM） |
| L04-016 | 相关性数值无断言 | M2a-F-6 · F_TEST_GAP · P2 | :246-283 仅 printf corr_mean/corr_max；UNCERTAINTY 给数值 |
| L04-017 | SCI-DRZ 符号表混入 Gaia 极区常数 | M2a-I-1 · I_DOC_HYGIENE · P2 | DRIZZLE.md:23 收录 C/C45；模块全域 grep Lipschitz → 0 命中 |
| L04-018 | 注释堆积任务号/审计流水 | M2a-D-3 · D_COMMENT · P2 | 宪章 §12.2:429 逐字为禁令；P1-DRZ-NONFINITE / ORACLE_HARDENING / B2-A8 等簇实证 |
| L04-019 | variance FLOAT64→FLOAT32 静默窄化 | M2a-H-4 · H_NUMERIC · P2 | api.cpp:984-1008 区段实存；引擎侧仅 const float* 通道 |
| L10-001 | 星表位置历元 J2016.0 vs J2000 | M2a-B-1 · B_STD_MISMATCH · P0（**无条件成立**，见 §E.1） | 恒零天测列 + 无传播接口实证补齐；L15-004 已钉外部原文 |
| L10-002 | out_match_idx 被截断为 matched_count | M2a-C-1 · C_DOC_CODE_GAP · P0 | 合同原文逐字核对；无任何部分未命中用例 ⇒ 不降级 |
| L10-003 | Gaia README/module.yaml 现状陈述过期 | M2a-C-5 · C_DOC_CODE_GAP · P1（与 L10-004 合并） | 无 CMake target/无测试/无 plan-cancel 三说与 CMakeLists:15、ctest 七项、源码符号相反 |
| L10-004 | ALG §3.1/§4/§5 三条迁移缺口陈述过期 | M2a-C-5（同条实例） | collect_plan_stats:237、set_cancel_checkpoint:85、set_worker_lease:83 均在树内 |
| L10-005 | 合同行锚系统性漂移 +120~170 | M2a-E-2 · E_TRACE_BREAK · P1 | unproject 文档 622-646 / 实测 :750；QueryCacheEntry 文档 112-134 / 实测 :159-179；缓存版本宏 文档 75 / 实测 :121；README:148-149 自述基线行数 2255 vs 现文件 2441 |
| L10-006 | 块缓存 4GB 全局陈述 vs 每文件实现 | M2a-C-7 · C_DOC_CODE_GAP · P1 | :115 常量、:434 per-bc 判据、:212 内嵌 XPSDFileInternal、:30 MAX_FILES=32 |
| L10-008 | XPSD 字段撒谎族零覆盖 | M2a-C-9 · C_DOC_CODE_GAP · P1（**按前台 R-2 重写定档**） | 现状是静默半加载而非崩溃；header_len 读出即弃（全文件仅两处出现） |
| L10-009 | fixture 全单叶树，深树零覆盖 | M2a-F-2 · F_TEST_GAP · P1 | 两 writer 均 nodeCount=1；:1415/:1555/:1692 递归分支不可达 |
| L10-010 | §6/§8 章节错指 + TEST 未回登 | M2a-E-3 · E_TRACE_BREAK · P1 | §6 实为 precision；module.yaml test_ids 只有设计件，ctest 七项在册 |
| L10-011 | DATA-001 合同文档全仓不存在 | M2a-E-1（同条实例 6） | docs/interfaces/data 仅 002/003/004；registry doc_ref 指向不存在的它 |
| L10-012 | calibrated_frame type_id 词法死锁 | M2a-C-10 · C_DOC_CODE_GAP · P1（P0→P1，见 §C） | 正则两侧实测：三段 vs 四段；**无真实产品发射该 id** ⇒ 按「一经使用必拒」定档 |
| L10-013 | 标准注册表检查器未登记 CI | M2a-G-1 · G_GOV_GATE · P1 | 注册表 §5 自陈 + ci/checks.json grep → NO HIT（自陈≠修复） |
| L10-014 | fits_core CHECKSUM 占位卡恒写 + verify 豁免落点倒置 + D.fits §6 CONFORMANT 失真 | M2a-B-4 · B_STD_MISMATCH · P1（类别改判，见 §C.2-5） | runtime/io/fits_core.c:1146-1156/:1261-1271/:1448-1463/:1648-1651 四处一手复读；IO_001:139 与注册表:223 同时不兼容；测试默认 checksum=0 |
| L10-016 | §30 合同 PENDING 与已实现 writer 矛盾 | M2a-C-11 · C_DOC_CODE_GAP · P1 | aio_hips.h:46-47 NREJ/NUSED；writer:943-957 int32 通道 + ASTROCS_* 五键 |
| L10-018 | by_coords 匹配判据全未文档化 | M2a-A-3 · A_SCI_DEF · P1 | ALG「不做匹配求解」vs op 名「匹配（最近邻）」；:2266 严格小于=平手取先见 |
| L10-019 | 合同负测映射引用不存在的用例名 | M2a-E-5 · E_TRACE_BREAK · P2（证据范围收窄） | 三项中两项在指定文件无同名函数；第三项名存在于另一文件（见 §C 剔除） |
| L10-020 | determinism 与行序不稳定并存 | M2a-I-2 · I_DOC_HYGIENE · P2 | module.yaml:57 vs ALG:167 逐字 |
| L10-021 | DATA ID 闭包检查正则半覆盖 | M2a-G-2 · G_GOV_GATE · P2 | DATA_RE 要求 -NNN 后缀；无后缀形态（DATA-P1-CAL 等）不被采集；合同仍写「全部登记、无重复」 |
| L10-022 | 注释/断言簇（算法名、sincos、KI-1、恒真断言） | M2a-D-4 · D_COMMENT · P2 | 五项逐一 grep：无 85° 分支、无 sincos、KI-1 已改 double、:912 恒真、tail_keys 死数组 |
| L10-023 | 归档约束件作现行权威 | M2a-I-3 · I_DOC_HYGIENE · P2 | DATA-003 约束来源行 + AGENTS.md/宪章 §1.1 分层 |
| L10-024 | get_spectrum_params 缺 NULL 防护 | M2a-C-13 · C_DOC_CODE_GAP · P2 | :2425-2427 直接解引用；同族 5 符号均有判空 |
| L10-025 | module_version 与产品 VERSION 同串 | M2a-G-3 · G_GOV_GATE · P2（置信度中） | 三处字面量同值 vs 宪章 §16.1:616「不与产品版本混用」 |
| L10-026 | exchange schema 与 validator 口径分叉 | M2a-C-14 · C_DOC_CODE_GAP · P2 | storage_uri 词法、int64 上限、type_id 两处字面分叉、min_planes 仅执行形态 |

> 覆盖核对：**45 条叶子条目 = A.1 的 40 条 + A.3 的 5 条**（A.3 每条的已修复半边对应 A.2 的 5 项事实）；A.4 的 13 项是两档案各自 §6 待复核清单的逐条转录（L04 六项 + L10 七项），不在 45 条之内、不定稿为 finding。定稿 finding **46 条** = 45（A.1 40 + A.3 5）+ 1（M2a-C-2 由 L04-001 拆出）+ 3（本轮新增 M2a-B-2、M2a-F-3、M2a-F-4）− 3（合并净减：L04-006+L04-007+L10-011→M2a-E-1 少 2，L10-003+L10-004→M2a-C-5 少 1）。优先级分布：P0 6 / P1 24 / P2 16。

### A.2 已被修复（并发提交期间，**不入 findings/**，5 项事实）

| # | 原判据（叶子口径） | 当下树事实（一手复读） | 是否有回归保护 | 后续处置 |
|---|---|---|---|---|
| F-1 | L04-001 半边：「缺省 precision_mode 会静默降 FP32」 | module_adapters.cpp:2240-2247 现对缺失/非整数 precision_mode 显式 DATA 拒绝（B2-A12「no silent default」） | **有**：tests/cli/test_phase1_inprocess.py:212-224、tests/unit/p1001_real_nodes_test.cpp:1024-1044 断言拒绝路径 | 不再立条；残余事实转 A.3 的 P-1 |
| F-2 | L04-005 半边：「值像素 NaN/Inf 被主循环静默 continue（等效掩膜）」 | drizzle_engine.cpp:1713-1719 自述旧 isfinite+continue 已删除，NaN 经 F_p 直接传播 | **有**：p1drz/p1drz_tests_core.cpp:423-443（注册于 p1drz/CMakeLists.txt:48-72）断言 NaN 面引擎层成功 + 无有限伪输出 | 文档/登记侧失实另立 M2a-C-3 |
| F-3 | L10-007 半边：「block_cache 无同步数据竞争（UAF）」 | gaia_client.c:300-306 自述已改为 per-file BcLock + 调用方线程局部 scratch（:307-312 锁原语实存） | **无在线保护**：唯一验证件 lib/gaia_xpsd_client/test/test_gaia_race.c 不被任何 CMake target 编译、不进 ctest/CI ⇒ 另立 M2a-F-3 | 残余事实转 A.3 的 P-3 |
| F-4 | L10-017 半边：「GaiaStar.parallax/pmra/pmdec 输出未初始化（UB）」 | gaia_client.c:1829/:1888 改用 calloc 并注释「未初始化字段显式置 0」 | **无**：tests/** grep parallax → 0 命中，恒零语义与列下发均无在册断言 ⇒ 另立 M2a-F-3 实例② | 残余事实转 A.3 的 P-5 |
| F-5 | L10-015 半边：「DATA_ARTIFACTS.md:150-152 把 HiPS tile 读路径归属到 modules/services/io（astrocs_io.dll）」 | 该文件当下 103 行，grep hips_input_v1 / astrocs_io.dll / modules/services/io → **NO HIT**；IO_002:43-44 归属为 runtime/io/hips_core.c（正确树） | 文档面已不在现文 ⇒ 无从谈保护 | 该锚剔除（§C 之 5）；残余事实转 A.3 的 P-4 |

### A.3 部分修复（定稿标题一律改写为残余事实，5 条）

| 来源 | 已修复半边（见 A.2） | 定稿的残余事实 → 去向 |
|---|---|---|
| P-1 L04-001 | precision_mode 缺失已显式拒绝（F-1） | 累加域仍由输入块 dtype 决定、元数据仍记请求值 → **M2a-H-1（P0）+ M2a-C-2（P1，本轮拆出）** |
| P-2 L04-005 | 值侧掩膜已删且有注册回归（F-2） | 六处合同/注册仍把已不存在的行为作当前事实登记、DISP-DRZ-004 仍 TRACKED → **M2a-C-3（P1）** |
| P-3 L10-007 | 缓存锁已落地（F-3） | ALG 仍写「单写者」+ 返回指针生命周期无合同 + lease/cancel 是进程级 static → **M2a-C-8（P1）+ M2a-F-3（P1）** |
| P-4 L10-015 | 合同归属段已不在现文（F-5） | 头文件 :3 与 IO_002:44 仍声明经 astrocs_io.dll 对外，而该 target 只编 fits_core.c、hips_core.c 不在任何构建目标内、无生产消费点 → **M2a-E-4（P1）** |
| P-5 L10-017 | UB 已消除（F-4） | 合同/README 仍写「未初始化」+ 恒零列以 published 行 schema 下发且 0 落合法值域 → **M2a-C-12（P1）** |

### A.4 无法判定 / 移交（13 项 = L04 §6 六项 + L10 §6 七项；均**不计入 45 条**、不定稿为 finding）

| 来源 | 事项 | 本轮状态 | 缺什么权限/证据、或转给谁 |
|---|---|---|---|
| L04 §6-1 | adaptive 大像素路径 build_drop_geometry_into 传 nullptr 与 spherical_overlap 注释「生产路径必须提供 double 顶点」自述冲突（FP32+≥60″ 时法向来自 float） | **无法判定** | 需构建 + 差分运行；本代理禁构建/测试 |
| L04 §6-2 | 零漏选裕度：解析界 1.127 / 实测 1.14 / 代码 1.15 / 注释 1.25 四值并置，且 SCI §6 把 1.532（全对角）与 1.25（半径）并列 | **部分转定稿**：注释与代码互斥已入 M2a-D-1；「SCI §6 表述是否会被误读为剪枝不足」仍待裁决 | 数学 Owner 裁决 + 数值实验 |
| L04 §6-3 | reports/evidence/SYN004_verification.md 与 LNX003_verification.md 声称的覆盖面 | **无法判定**（属并发免报名单，未核内容） | 转 M5/前台以当前 SHA 重跑口径裁决；本域 M2a-F-5 只定稿测试实体本身 |
| L04 §6-4 | legacy drizzle()/drizzle_f64()/writeHis 与 tiled 生产链是否仍在 CLI/orchestrator 路由可达（DISP 清单未覆盖其 NaN/variance 差异） | **无法判定** | 需运行期调用图；若可达须另立双实现语义条 |
| L04 §6-5 | DRIZZLE.md:160 指向不存在的「§15 SYN-004 数据与不变量表」、§9a:147 指向不存在的「§4a」 | **转定稿**：作为 M2a-E-1 的实例之一（同一根因：引用未定义对象） | 已入 E-1；机器侧漏判转 M6/L16 |
| L04 §6-6 | engine.h:25 uncalibrated_adu_allowed/BUNIT 路径与 SCI §13「不做测光归一化」的责任边界 | **移交** | 转 M3a/L07（测光/噪声域交叉确认）；本域 M2a-A-2 只定稿单位声明与 tile BUNIT 缺失 |
| L10 §6-1 | 上游 XPSD 建库工具链导出时是否已传播到 J2000 | **不再需要**：本域以「模块无天测列、无传播接口」证明「无法传播」，L10-001 无条件定稿（§E.1）；历元真值一侧由 L15-004 以 ESA 原文钉死 | 已闭（前台 L15 补证） |
| L10 §6-2 | 外部工具（astropy/fitsverify）对发布文件中 CHECKSUM=全零占位的确切处置 | **无法判定（且不再阻塞）** | 本域 M2a-B-4 只定稿仓内代码/登记事实；外部处置转 L15/L16 标准原文面 |
| L10 §6-3 | DISP-GAIA-001 的实测锚（历史 commit + memory.md V18R3「18 查询 683,297 星一致」） | **不作事实使用** | 本会话禁 git/evidence；已按「代码现状 vs 文档记载」改写（§C.1-4）；锚存在性由前台按 §14.5 复核 |
| L10 §6-4 | DATA-HIPS-SIGNAL-001「invalid: NaN/support=0」与 §4a F-UNC-001 双值消歧进度 | **移交** | 属 Phase1 域任务；转 M3a/M4 与 L03 去重 |
| L10 §6-5 | DATA-002 §3「frame_hips_manifest.json（hash 64 hex）」所指产物名与 AIO 实际落盘名是否一致 | **无法判定** | AIO writer 命名核对属 L03 域，转 M4/L03 |
| L10 §6-6 | module.yaml resource_class: io_bound 与迁移桥自述「heavy 工作=查询」+ OpenMP 并行解压是否矛盾 | **无法判定** | §10.5 heavy 门适用性属负责人口径 |
| L10 §6-7 | tests/io/test_hips_input_contract.py 与 make_hips_fixture.py 逐行细节 | **部分覆盖**：本域 M2a-E-4 只用到「reader 不在构建目标内、无生产消费点」两点，其余逐行细节未核 | 转 L03/M2b |

### A.5 行号漂移处置（单列）

本轮纪律：**行号漂移不作拒绝理由**。所有条目一律先按符号/关键语句在当下树重新定位，能定位即按新位置定稿（`path::符号`），只有「对象在树内不存在」才构成事实变化。逐类结果：

| 漂移类型 | 处理方式 | 本轮实例（叶子原锚 → 当下树锚） |
|---|---|---|
| 纯区间漂移（内容仍成立） | 按新位置定稿，条目内改记符号锚并在表 A.1 注明「实测」 | L10-005 全部行锚（unproject 622-646→:750；polar_plane_intersects 661-734→:830；QueryCacheEntry 112-134→:159-179；GAIA_CACHE_VERSION 75→:121；gaia_client.h 头注 427-472→:554；fixture 2255 行自述 vs 现文件 2441 行）；L04-012 的 engine 注释簇（613→615-619/675-677）；L04-019 的 variance 窄化段（984-1008 区段仍在，行内偏移）；L04-003 的 continue 段（叶子 :1718-1732 → 实测 :1733-1736）；L04-011（:1768 → :2234）；L04-006 的 CMake 356-366 → :408；L10-014 的 :228-236/:1649-1655 → :1146-1156/:1648-1651；L10-006 的 :429 → :434；L10-008 的 :1030-1031/:1094-1101 → :1036-1037/:1100-1107；L10-002 的 :2312-2315 → :2293-2326 区段；L10-024 的 :2390 → :2425-2427；L04-016 的 236-237 → :246-283 |
| 锚指向的对象不存在 | 不作「位置错所以不成立」处理，而是**升格为追溯断链**单独定稿 | L04-006（api.cpp / finalize_tile / 02_FROZEN）、L10-011（DATA-001 合同文档）、L10-010（§6 错指）、L10-019（用例名不存在/在别处）→ 全部落 M2a-E-1 / E-3 / E-5 |
| 锚所在文档被并发改版，原句已不在现文 | 剔除该子证据（记 §C.1），条目按剩余一手证据重锚 | L10-015 的 `DATA_ARTIFACTS.md:150-152`（文件现 103 行、grep NO HIT）→ 重锚为 hips_input_v1.h:3 + IO_002:43-44 + CMakeLists.txt:146 + 全仓符号检索（M2a-E-4） |
| 行号未漂移但代码语义已变 | 按四状态重新定档，不改判为「叶子误报」 | L04-001（precision 缺失已显式拒绝 → A.2 F-1 / A.3 P-1）、L04-005（掩膜已删 → A.2 F-2）、L10-007（已加锁 → A.2 F-3）、L10-017（calloc 置 0 → A.2 F-4） |
| 叶子位置列写成 `path:数字` | 定稿时统一改写为 `path::符号`，并在条目内保留「实测行号」仅为本轮定位凭据 | 全部 46 条定稿条目均已按此改写 |

> 附带核对：本域未使用前台作废的 `aio_hips_writer.cpp:143-144 / PATH_MAX / <=4096` 锚（见 §E.3 第 4 点），故无因该锚作废而需改判的条目。
## B. 复算与推导（六项必查逐条）

### B.1 L04-002：pixfrac 归一化推导（结论：**成立，P0**）

合同三条原文（当下树逐字）：§5 权重 `w_jp = a_jp / A_drop,j`、`F_p = Σ_j x_j·w_jp`、`D_p = Σ_j a_jp`、`S_p = F_p/D_p`；§7 前提「常数面亮度 B0 的 drop 满足 `x_j = B0·A_drop,j` ⇒ `S_p = B0`」与「每像素常量 ADU=C ⇒ `S_p = C/A_drop ≠ C`（正确的面亮度语义）」；§11 门「常数场 C 的 `S_p=C` 全像素 `max_abs==0`」。

1. drop 的构造：`spherical_overlap.cpp:799` 与 `:910` 均为 `T half = T(0.5) * pixfrac;` —— 源像素角点在**两个线性维**各乘 pixfrac，故 `A_drop,j = pixfrac² · A_pixel_j`。
2. 把 §7 自己给的前提代入 §5：`F_p = Σ_j (B0·A_pixel_j)·a_jp/(pixfrac²·A_pixel_j) = (B0/pixfrac²)·Σ_j a_jp = (B0/pixfrac²)·D_p` ⇒ **`S_p = B0/pixfrac²`**。`A_pixel_j` 在每一项内逐元约掉，因此该因子与网格、nside、覆盖完整度、单/多帧、面积单位（px² 或 sr）全部无关，只由 pixfrac 决定。
3. `pixfrac=1` 时 `S_p=B0` 恰好通过 ⇒ 这就是缺陷在 `pf=1.0` 配置与全部在册测试下隐形的原因（见 M2a-F-4）；`pixfrac=0.8`（生产默认，`json_config.h:67`、`configs/stage1.template.json:41`）⇒ 1.5625×；`pixfrac=0.5` ⇒ 4×。
4. 落点核对（非文档转述）：引擎 `drizzle_engine.cpp:1508` `Scalar weight = overlap_area / drop_area;`、`:1530` `acc.sumFlux += Scalar(pixelValue * weight);`、`:1531` `acc.sumArea += Scalar(overlap_area);`；输出 `aio_hips_writer.cpp:524` `sig = flux / area;` ⇒ 输出即 `Σ(x·a/A_drop)/Σa`，与 §5 逐字一致 ⇒ **代码合规、合同互斥**：§5+§7:82 的推导出 1/B0 因子，与 §7:84「⇒ S_p=B0」和 §11:117 同符号声明直接矛盾，三者不可能同时为真。
5. 结论与置信度：P0 定稿（M2a-A-1）；置信度高（纯代数 + 四处代码一手复读）。修复须由 Owner 裁决归一分母（`D_p` 或 `D_p/pixfrac²`），并同步 §7/§11/§2-§3 单位表（M2a-A-2）。

### B.2 L04-001：FP32 累加链（结论：**累加域确实不由 precision_mode 决定**）

逐跳链（当下树）：
`json_config.h:67`（DrizzleConfig）→ `orchestrator.cpp` 装配 → `module_adapters.cpp:2240-2247` precision_mode 必须显式给出（0/1，否则 DATA 拒绝）→ `:2234` pixfrac → **`:2303` `aio_frame_add_block(frame, "data", AIO_BLOCK_FLOAT32, im.px(), ...)`（恒 FLOAT32，与该值无关）** → `:2382` `hp_drizzle_run(frame, nside, nested, pixfrac, hiss_path, &res, precision_mode)` → `hp_drizzle_api.cpp:465-472` 由块 dtype 置 `data_is_f64`、`:528-536` `img.use_f64` → `:1022-1031` `img.use_f64 ? drizzleTiled<double> : drizzleTiled<float>` → `:2346-2347` 帧头 PRECISION 写**请求值** → `drizzle_engine.cpp:2022-2023` `hmeta.precision_mode/hmeta.signal_dtype = config.precision_mode`（也是请求值）、`:2099` `static_cast<double>(tile.pixels[local].sumFlux)` 事后加宽、`:2133-2140` 按 precision_mode 选 `add_tile_f64`。

结论：核心 IR 通道无论请求何精度都是 binary32 逐元素累加，FP64 只体现在「事后加宽落盘 + 元数据声明」；只有 f64 manifest 的 DLL 通道（`lib/drizzle/src/module_entry.cpp` + `hp_drizzle_api.cpp` 的 FLOAT64 块）能拿到真 FP64 ⇒ 通道级不对称。叶子原案「缺省静默降 FP32」半边**已被并发提交修掉**（A.2 F-1，且有测试钉住），故定稿改为残余事实（M2a-H-1 P0 + M2a-C-2 P1），拆出的元数据半边单独成条。

### B.3 其余四项必查

- **L04-003（variance≤0）**：现文 `drizzle_engine.cpp:1733-1736` `if (varianceValue <= 0.0f) continue;`（注释还写「合法数据边界, 非掩膜」），`:1739-1740` 的 `nSourcePixels++`/计数在其后 ⇒ 丢像素不计数；`:1534` 另一处 `if (varianceValue > 0.0f)` 只跳方差项；`drizzle_engine.h:265` 写「0 = 未知, 跳过传播」；DATA §11.1:254 与 ALG §5:119 写「非有限或 ≤0 → 跳过该像素」。⇒ **实现只在 ≤0 半边符合 DATA 文本，非有限半边与两份合同相反，且与头注释、SCI §4/§8 全部互斥**；SCI 从未授权用方差值决定像素有效性 ⇒ M2a-H-2 定 P0（§4.1/§7.3）。
- **L04-004（Oracle 未注册 + 自证）**：注册面 grep 实证——`tests/p1drz/CMakeLists.txt:48-72` 六个 add_test 是唯一在线 drizzle 门；`tests/**/CMakeLists.txt` 对 candidate_oracle/variance_propagation/drizzle_freeze/drizzle_nonfinite/control_median_mc 全部 0 命中；`ci/checks.json` 无 drizzle 项；`TargetGeomCache` 与 `healpy` 在 tests/** 0 命中（§11 两门在树内无实体）。**9003 例复算**：矩阵 111 位置 × 5 尺度 × 4 pixfrac × 4 nside = 8880，+ 高 nside 附加 48 + 4194304 面 75 = **9003**（与文档一致）；其中 nside∈{64,128} 分支 111×5×4×2 = 4440，加 48、75 ⇒ **4563/9003 ≈ 50.7%** 用生产 `query_candidate_pixels` 超集 ∩ 生产 `compute_overlap_area_g>0` 充当真集合；余下 4440 例的穷举真值集同样调用生产 `compute_overlap_area_g`（:55-78 两个 oracle 函数）⇒ **全 9003 例都非「不调用生产实现的独立 Oracle」**（§13.1 违），且 `:147` 注释自认 64/128 只是「保守参考」。定稿 M2a-F-1（P0），`related: L02-001, L07-006, L08-004`（按协调指令）。
- **L10-001 + F00-06**：见 §E.1（含前台要求的恒零补证与「注册表判定式与自身条款面不一致」单立条 M2a-B-2）。
- **L10-002（match_idx 截断）**：合同原文逐字 `| out_match_idx | int32 | 坐标序 | −1 = 该坐标未匹配 |`（DATA §8.2:113）；底层 `gaia_client.c:2293-2326` 按 n_coords 分配、命中写行号、未命中写 −1、`*out_count = matched_count`；载荷 `module_entry.c:732` `b64_encoded_len((uint64_t)count * sizeof(int))` 且 :760-763 schema 只声明 match_idx:"i32"、不回显 n_coords ⇒ 部分未命中时坐标↔行映射**不可恢复**（不是显示问题）。降级前置：注册测试是否全命中——`gaia_adapter_test.c:592-607` NC=8 由 find_star 预筛并断言 `dn == NC`，`gaia_integration_test.c` 单坐标单命中 ⇒ **无部分未命中用例 ⇒ 不降级，定 P0**（M2a-C-1）。

## C. 剔除与降级显式清单（无静默丢弃）

### C.1 剔除（4 项证据/子项，不进入定稿）

| # | 被剔除内容 | 理由 |
|---|---|---|
| 1 | L04-006 第 7 子项「reverse 通道返回码域两侧写 1..6 vs 1..7」 | 两侧文件 grep 码域字面 0 命中，无法按现文成立 ⇒ 移入 §A.4-8（需运行期返回码比对） |
| 2 | L10-015 的 `DATA_ARTIFACTS.md:150-152` 锚 | 文件当下 103 行且相关陈述不存在（多锚点 grep NO HIT）⇒ 该锚作废，条目按头文件/IO_002/构建面重锚（见 §E.3 之 5） |
| 3 | L10-019 的「test_missing_hash_rejected 不存在」半边 | 该名**存在**于 tests/pipeline/test_phase_lifecycle.py:168，只是不在合同所指文件 ⇒ 证据收窄为「指针指错文件」，另两项维持（M2a-E-5） |
| 4 | 全部叶子引用的 commit 哈希（30b5516d / 09ee5363 / babe752d / 790e0315 / 9a3b5a7d / f7fa3160）与「某提交已修/未修」叙事 | 本会话禁 git，不作事实使用 ⇒ 一律改写为「代码现状 vs 文档记载失实」 |

### C.2 降级 / 改判（9 项，均给出理由）

| # | 来源 | 原档 | 定稿 | 理由 |
|---|---|---|---|---|
| 1 | L10-012 | P0 | **P1**（M2a-C-10） | 前台/协调要求核实「是否真的拒绝真实产品」：正则两侧实测互斥成立，但全仓**无产品发射该 type_id**（仅 registry + DATA-002 声明沿用）⇒ 按「一经使用必拒」而非「已在产线被拒」定档 |
| 2 | L04-009 | P1 · B_STD_MISMATCH | P1 · B_STD_MISMATCH（**范围收窄**） | 双轨本身已由 DISP-DRZ-003 登记，不得当新问题重报；残余只保留迁移面词表固化 + validate/plan 放行 + 头注释自相矛盾 |
| 3 | L04-012 | P2 · D_COMMENT | P2 · D_COMMENT（**范围收窄**） | 面积算法措辞偏差已登记 DISP-DRZ-002 ⇒ 只保留注释常数（211034.6 vs 复算 211076.3）与 1.25/1.15 系数互斥 |
| 4 | L04-008 | I_DOC_HYGIENE | **C_DOC_CODE_GAP**（M2a-C-4） | 实质是文档陈述与构建事实矛盾（非排版卫生）；按协调令单独立条，命名模式入移交清单 |
| 5 | L10-014 | C_DOC_CODE_GAP | **B_STD_MISMATCH**（M2a-B-4） | 前台指令：D.fits §6 声称 CONFORMANT/偏差无为「声称符合实际不符」；主类别改，并与 L15-006/L15-007 并档（related 已写），不再等标准原文 |
| 6 | L10-015 | C_DOC_CODE_GAP | **E_TRACE_BREAK**（M2a-E-4） | 前台指令要求把「合同文档把读路径归属写错树」单立为 E；重锚后事实链为 ABI 头/IO 文档 ↔ 构建 target ↔ 无消费点，属发布归属断链 |
| 7 | L10-008 | C_DOC_CODE_GAP · 可用性 | 同类别 · **科学完整性**（M2a-C-9） | 前台改判令 R-2：在册 mode_negative 把损坏/半加载钉成期望，故不是「崩溃/不可用」而是「吞错后返回成功码」（宪章 §11）；「零覆盖」下修为基础损坏**有**在册回归，真正零覆盖的是字段撒谎族 |
| 8 | L10-017 | C_DOC_CODE_GAP · UB | C_DOC_CODE_GAP · **恒零列下发 + 文本失实**（M2a-C-12） | UB 半边已由 calloc 修复（A.2 F-4）⇒ 定稿改为残余事实，另立 M2a-F-3 实例③（无断言） |
| 9 | L04-001 | 单一 P0 | **拆两条**：M2a-H-1（P0 数值）+ M2a-C-2（P1 元数据失真） | 数值域选择与 manifest 如实记录是两个独立处置面（§5.3 vs §4.3），修法与 Owner 不同 |

## D. 移交清单（跨域项）

| 去向 | 事项 | 本域已提供 |
|---|---|---|
| 前台 / 负责人 | **「状态声明无单一事实源」模式**（README / module.yaml / registry / 注册表四处对同一模块给出互斥当前事实；本轮两处实例：lib/drizzle 说「没建」、lib/gaia_xpsd_client 说「未迁移」，方向相反但同病；F00-04 为第三实例，**不自行合并**） | M2a-C-4、M2a-C-5 已列全部逐字证据 |
| M6（追溯矩阵域） | 追溯矩阵双头 / 锚点治理：本域已把「引用对象不存在」全部登记在 M2a-E-1，矩阵本身的双头事实**由 M6 主落，本域不重复定稿**（related: F00-01 已写） | E-1 实例清单 1-7 + §A.4-8 |
| M7（横向标准/注册表域） | **「注册表判定式与自身条款面不一致」可复用模式**：判定行丢掉条款面的某一维仍给 CONFORMANT。本域实例 D.catalog:186 vs :193（M2a-B-2）；D.fits 章节错挂由 L15-007 主落；建议把该规则做成检查器断言 | M2a-B-2 全文 + M2a-G-1 的门禁缺失 |
| M5（CI / 配置治理域） | ① `check_standards_registry.py` 未登记 `ci/checks.json`（M2a-G-1）；② pixfrac 双默认值（0.8 vs 1.0）需单一事实源与默认值登记（M2a-C-6 已移交后半）；③ 注册表 §5 自陈项的实际承接任务 | M2a-G-1、M2a-C-6、M2a-C-10 的机器门建议 |
| M2b / L08（lib-drizzle 副本与后端域） | DISP-DRZ-008 的 lib/drizzle 副本双份实现、`TargetGeomCache` 等价门在树内无实体、`snr_evaluator` 的 IDW 遮蔽问题与本域 M2a-H-2 的「用不确定度面决定有效性」同族 | M2a-F-1（§11 两门无实体）、M2a-H-2 |
| M3a / M4（Phase2 / Phase3 消费方） | ① Gaia 恒零天测列（parallax/pmra/pmdec/source_id）若被 Phase2 用作天测约束即错（M2a-C-12）；② variance 平面单位与 signal 单位维度不符会污染 ivar 组合（M2a-A-2）；③ HiPS support 钳制对 Phase3 导出闭合的影响（M2a-H-3）；④ SCI §11 的 1/pixfrac² 因子对候选栈加权的影响（M2a-A-1） | 四条均已给出消费口径 |
| M9 / L24（健壮性与交付安全域） | XPSD 解析面的 4 条按前台指令由 L24 主落，本域**不重复定稿**：spectrumCount（L24-002）、block_offset/data_position（L24-005）、树构建 malloc break 吞错（L24-006）、json_get_f64_array 零推进挂死（L24-007）。本域只保留 L10 独有事实并在 M2a-C-9 的 related 指过去；建议同一 create 路径统一建立只读区间模型 | M2a-C-9（含 related 指向） |
| L15（FITS 标准域） | D.fits §6 行的判定与 DATASUM/CHECKSUM 章节错挂：本域 M2a-B-4 只定稿代码事实与登记失真，标准原文条款号归属由 L15-006/L15-007 主落 | related: L15-006, L15-007 已写 |
| 运行/数据域（本代理无权限） | §A.4 中真正**无法判定**的 6 项（需构建 / ctest / 真实 GaiaDR3 目录 / 运行期返回码 / TSAN 构建）：L04 §6-1、§6-3、§6-4 与 L10 §6-2、§6-5、§6-6 | 已逐项给出所需权限与判据；另 §A.4 中 4 项属移交他域、3 项已闭或转定稿 |

## E. 前台指令执行记录

### E.1 L10-001 补证（历元 P0 是否无条件）

前台要求回答「XPSD 是否真的不下发 parallax/pmra/pmdec」。**答：是，且比「不下发」更强**：
1. cone 结果结构里的天测列是 calloc 出来的恒零值 —— `gaia_client.c:1829`（缓存命中路径）与 `:1888` 均为 `calloc(...) /* CAT-GAIA-IMPL: 未初始化字段显式置 0 */`；
2. 按坐标匹配的结果结构 `GaiaSpectrumStar`（`gaia_client.h:57-66`）**连这三个字段都不存在**；
3. 记录解码路径（`gaia_client.c:1378-1412`）只读位置量化（2 µas/LSB）、G 星等（+20）、以及 +26 处的 `dra`（10 µas/LSB 的 **RA 位置细分量**，不是自行）；
4. 全文件 grep `epoch|历元|J2016|propagat` → 0 命中，无任何历元处理；
5. 而 `module_entry.c:751-753` 仍把 parallax/pmra/pmdec/source_id 作为发布列写进行 schema。
⇒ 本模块既无天测数据也无传播接口，「无法传播」成立 ⇒ **L10-001 的 P0 不再依赖上游建库是否已传播，无条件成立**（M2a-B-1）。按前台指令另立 **M2a-B-2「注册表判定式与自身条款面不一致」**（D.catalog:186 条款面含 J2016.0，:193 判定式丢掉历元维仍给 CONFORMANT/偏差无），与 L15-007（D.fits 章节错挂）同型，命名入移交清单交 M7 横向核。

### E.2 CHECKSUM 条（L10-014）按指令定档

不重做取证、不等 L15：类别改 B_STD_MISMATCH、`related: L15-006, L15-007` 已写入 M2a-B-4；本代理一手复核的代码事实是 `runtime/io/fits_core.c` 的「无条件写 `CHECKSUM="0000000000000000"` 占位卡（:1146-1156）+ 恒预留两槽（:1261-1271）+ 仅 `write_checksum` 才回填（:1448-1463）+ verify 把全零豁免设在**计算值**侧（:1648-1651）」，与 `IO_001:139`「CHECKSUM 卡由调用方选项决定（默认关闭）」及注册表 `:223` 的 CONFORMANT 同时不兼容；测试默认 `checksum=0` 且只有一个 `checksum=1` 用例（tests/io/test_fits_stream_contract.py:162/:283-295）。

### E.3 路径订正核对（前台指令第 1 项）

- **`modules/services/io/src/fits_core.c` 假锚**：glob `modules/services/io/**` → 只有 `include/astrocs/io/{fits_stream_v1.h, hips_input_v1.h}` 与 `tests/{fits_core_selftest.c, hips_core_selftest.c}`，**无 src/**；真源为 `runtime/io/fits_core.c`（且 `CMakeLists.txt:146 add_library(astrocs_io SHARED runtime/io/fits_core.c)` 佐证）。**核对结果：L10 全部 io 锚未使用该假锚**（M2a-B-4/M2a-E-4 用的都是 runtime/io 与 modules/services/io/{include,tests} 下真实存在文件）⇒ 本域无需订正，仅在此记录核对。
- **`DATA_ARTIFACTS.md:150-152` 锚**：该文件当下 103 行，相关陈述不存在（多锚点 grep NO HIT）⇒ 已在 §C.1-2 剔除并按现树重锚为「头文件角色行 + IO_002 归属表 + astrocs_io target 源列表 + 全仓符号检索无消费点」四点（M2a-E-4）。
- **另立 E_TRACE_BREAK**：前台建议的「合同文档把读路径归属写错树」已由 M2a-E-4 承载（未再拆第 47 条，避免与残余事实条重复）。
- **F00-07b 原锚作废（前台改判令 R-1）**：本域**所有已写定稿文件均未引用** `aio_hips_writer.cpp:143-144 / PATH_MAX / <=4096`（grep 实证：本代理产出树内相关命中为零，仅 `_cache/` 前台档案内保留撤回记录）⇒ 无需订正。本域 M2a-A-2/M2a-H-3 引用的 writer 行段（:524-526、:551-553、:596-597、:943-957）与路径缓冲无关。原子大小 512 的截断站点归 L24-011（`runtime/io/fits_core.c:1077/1078/1097-1099`），本域不重复定稿。

### E.4 前台三条改判令（L24 反证 + 前台自纠）执行记录

| 令 | 要求 | 执行情况 |
|---|---|---|
| R-1 | 作废 `aio_hips_writer.cpp:143-144 / PATH_MAX / <=4096` 锚；正确站点为 `runtime/io/fits_core.c:1077/1078/1097-1099`（ACS_FIO_PATH_MAX=512、target 与 tmp 同宽、`snprintf("%s.tmp.%ld.%lu")` 截断致原地清空正式产品），已定稿 L24-011 | 本域全部定稿文件 grep `PATH_MAX` / `4096` / `143-144` / `tmp.` → **0 命中**（只在 `问题扫描/_cache/` 的前台撤回记录里出现），故**无需订正**；本域引用的 writer 行段（:524-526 / :551-553 / :596-597 / :943-957）均属归一、单位与 provenance 面，与路径缓冲无关。已在 §E.3 第 4 点与 M2a-A-2 / M2a-H-3 内注明该站点归 L24-011、本域不重复定稿 |
| R-2 | L10-008 改判：①「create 必败/崩溃」与现状相反，须改为「损坏或半加载被静默当成功，plan/create/cone_search 全链返回成功码」；②「零覆盖」下修为基础损坏**有**在册回归，真正零覆盖的是字段撒谎族；③严重性从可用性移到科学完整性（宪章 §11 禁吞错后继续生成看似成功的产品）——属加重，但定性必换 | **已全部执行**：M2a-C-9 整条重写（标题 / 问题说明三点定档 / 影响 / 建议处置①-⑤），逐字引用 `tests/unit/gaia_cat_test.c:660-663`、`:686-689`、`:692-699`（含测试自述「应静默跳过」与「只要部分树可加载即接受」）与 ALG §5 negative 行「create 失败或 0 结果」；补两处一手实证：`header_len` 在 gaia_client.c 仅 :1036/:1037 两命中（读出即弃），fixture 生成器只写正确值（rootPosition/nodeCount/header_len 无撒谎注入，grep tests/unit 实证）⇒ 字段撒谎族确为零覆盖。定性=科学完整性，优先级维持 P1 并写明升 P0 的触发条件 |
| R-3 | 与 L24 并档、不得重复定稿：spectrumCount（L24-002）、block_offset/data_position（L24-005）、树构建 malloc break 吞错（L24-006）、json_get_f64_array 零推进挂死（L24-007）由 L24 主落 | **已执行**：本域星表侧只落 L10 独有事实（header_len 弃用、rootPosition/nodeCount 不设界、child 索引不校验、半加载被在册测试钉成期望）；M2a-C-9 的问题说明第 (3) 段与建议 ⑤ 显式声明不重复定稿，related 指向 L24-002 / 005 / 006 / 007；移交清单 D 表新增一行给 M9/L24，并建议在同一 create 路径一次做足只读区间模型 |

## F. 产出清单（本轮写入：18 个文件 / 46 条定稿，粒度=类别×优先级一文件）

| 文件（问题扫描/findings/…） | 优先级 | 条数 | 内含 ID |
|---|---|---|---|
| A_SCI_DEF/p0/M2a_L04_L10.md | 0 | 1 | M2a-A-1 |
| A_SCI_DEF/p1/M2a_L04_L10.md | 1 | 2 | M2a-A-2、M2a-A-3 |
| B_STD_MISMATCH/p0/M2a_L04_L10.md | 0 | 1 | M2a-B-1 |
| B_STD_MISMATCH/p1/M2a_L04_L10.md | 1 | 3 | M2a-B-2、M2a-B-3、M2a-B-4 |
| C_DOC_CODE_GAP/p0/M2a_L04_L10.md | 0 | 1 | M2a-C-1 |
| C_DOC_CODE_GAP/p1/M2a_L04_L10.md | 1 | 11 | M2a-C-2、M2a-C-3、M2a-C-4、M2a-C-5、M2a-C-6、M2a-C-7、M2a-C-8、M2a-C-9、M2a-C-10、M2a-C-11、M2a-C-12 |
| C_DOC_CODE_GAP/p2/M2a_L04_L10.md | 2 | 2 | M2a-C-13、M2a-C-14 |
| D_COMMENT/p2/M2a_L04_L10.md | 2 | 4 | M2a-D-1、M2a-D-2、M2a-D-3、M2a-D-4 |
| E_TRACE_BREAK/p1/M2a_L04_L10.md | 1 | 4 | M2a-E-1、M2a-E-2、M2a-E-3、M2a-E-4 |
| E_TRACE_BREAK/p2/M2a_L04_L10.md | 2 | 1 | M2a-E-5 |
| F_TEST_GAP/p0/M2a_L04_L10.md | 0 | 1 | M2a-F-1 |
| F_TEST_GAP/p1/M2a_L04_L10.md | 1 | 3 | M2a-F-2、M2a-F-3、M2a-F-4 |
| F_TEST_GAP/p2/M2a_L04_L10.md | 2 | 2 | M2a-F-5、M2a-F-6 |
| G_GOV_GATE/p1/M2a_L04_L10.md | 1 | 1 | M2a-G-1 |
| G_GOV_GATE/p2/M2a_L04_L10.md | 2 | 2 | M2a-G-2、M2a-G-3 |
| H_NUMERIC/p0/M2a_L04_L10.md | 0 | 2 | M2a-H-1、M2a-H-2 |
| H_NUMERIC/p2/M2a_L04_L10.md | 2 | 2 | M2a-H-3、M2a-H-4 |
| I_DOC_HYGIENE/p2/M2a_L04_L10.md | 2 | 3 | M2a-I-1、M2a-I-2、M2a-I-3 |

**合计 46 条：P0 6 / P1 24 / P2 16。** 未创建任何单条一文件；类别目录内的 `README.md` 占位文件全部未触碰（112 个非本代理文件保持原状）；无占位/待填残留；跨条引用 0 悬空（M2a-C-15 → 已改指 M2a-C-14）。
