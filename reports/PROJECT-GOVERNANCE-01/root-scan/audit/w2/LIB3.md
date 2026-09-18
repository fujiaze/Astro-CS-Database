# W2-LIB3 文件级审计报告（第二波·文件域穷尽扫描）

- 分片：W2-LIB3（lib/phase3_proj｜lib/phase3_rsmp｜lib/phase3_fits｜lib/phase3_session｜lib/hips｜lib/hips_browser｜lib/astro_sphere｜runtime/ ＋ 父增补 lib/acr/**、lib/io/**）
- 开工 SHA：180c8a0a→（按规程 2 分钟复测）HEAD=main=23f42ffa2272496a5f373ca57ffdcb5d74a132b9；origin/main=b4afc135 持续滞后（并行线高频提交；按「HEAD=main 相等即开工」开工并注明）。**枚举与取证基线固定在 23f42ffa**（审计期间工作区被并行线持续改写：lib/acr→lib/infrastructure/acr、lib/io→lib/infrastructure/aio/io、healpix_db→algorithms/drizzle 等 R100 迁移落地/暂存中）。
- 收工 SHA：b60d2e868e0e7c363eb9e5c0a34dc96d15bf4fa5（HEAD=main 相等；origin/main=d4729169，全程落后于本地——并行线只 push 到 origin 的窗口滞后，不阻塞）
- 口径：SHARD_BRIEF §1 四态精神＋任务书 P0/P1/P2 严重度；判定基于 23f42ffa 树内容（git show / 基线快照），工作区脏改动（M/A/D）影响判定处均注明。
- **域变迁（重要）**：任务书 8 前缀中 lib/phase3_proj / lib/phase3_rsmp / lib/phase3_fits / lib/hips 在基线 23f42ffa 已 0 tracked 文件——23acb453(R100)、59981aa9(R100) 分别整体改名至 lib/algorithms/projection(7) / lib/algorithms/resample(12) / lib/algorithms/fits_output(3) / lib/algorithms/drizzle/hips(10)；lib/hips_browser、lib/astro_sphere 从未以该名 tracked（实体=lib/healpix_db/healpix_browser_qt(48) 与 lib/healpix_db/healpix_drizzle/astro_sphere_sink.{cpp,h}(2)，已纳入覆盖清单）。科学核查点（八投影 registry、CRVAL2）在后继路径取证，发现表「建议文件域」列注明。
- 简报口径修正：任务书所指对齐目标 docs/science/PHASE3_PROJECTION 在树内不存在（docs/science 仅有 PHASE3_HIPS_TO_FITS.md）；投影对齐实按 docs/plugins/algorithms_phase3/14_projection.md ＋ docs/algorithms/PHASE3_PROJ_IMPL.md 执行（见 W2-LIB3-3/7）。
- NSIDE=2^(K+9) 读写两侧一致：**通过**——读侧 lib/phase3_session/p3_resample.cpp:183-184（leaf_order=K+9）、runtime/io/hips_core.c:245（nside=1ULL<<(K+9)）；写侧 hips 模块要求 nside 为 2 的幂且 ≥512、leaf_order==log2(nside)（lib/algorithms/drizzle/hips/src/module_entry.cpp:341-347/443-447）。但 order_sel 不驱动读取层级 的缺陷另立 P0（W2-LIB3-1）。
- browser 不进产品面：**通过**——基线根 CMake 0 browser/Qt 引用；packaging/astrocs.product.json、packaging/install-tree.contract.json、cmake/astrocs.product.windows.json.in 均 0 命中；ci/ctest_baseline.json 仅登记其测试目标（隔离测试面，合法）。残留为 §7.1 落位差（W2-LIB3-13，迁移进行中）。
- CRVAL2 读写一致：会话 TAN 面（p3_wcs.cpp:166-176/186-204 使用 d0）与 v6 zenithal（p3_proj_v6.cpp:71-96）一致；**legacy registry CAR/AIT 违例**→ W2-LIB3-2。

---

## 一、发现表

（14 条：P0×2＝W2-LIB3-1/2；P1×4＝3/4/5/6；P2×8＝7..14；「｜」为字段分隔）
| | ID | 定位 path:line | 违反的最新权威条款 | 当前证据（命令＋本轮真跑输出≤3行） | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门（单命令） | 同源标注 |
|---|---|---|---|---|---|---|---|---|---|
|  W2-LIB3-1 | lib/phase3_session/p3_session.cpp:200(算 order_sel)+171/255(sampler 按 properties 阶打开)；p3_resample.cpp:183-184/280 | docs/science/PHASE3_HIPS_TO_FITS.md §9a-5:61（ipix=ang2pix_NESTED(nside=2^(order_sel+9))、leaf_order=order_sel+9）＋§9a-11（provenance 必写 order_sel）；ASTROCS_DESIGN §5.2 | 命令：git show 23f42ffa:lib/phase3_session/p3_resample.cpp ｜ sed -n '183,184p'；输出：const int tile_order = s->order; / const uint32_t leaf_order = static_cast<uint32_t>(tile_order) + 9;（读取层=s->order=hips_order，非 order_sel） | P0 | 采样层级与 ORDERSEL 头卡/manifest 宣称的 order_sel 不一致：ORDERSEL 系统性失真、SCI §9a-5 公式未实现（粗输出比例尺时仍按最细叶层取点） | 以 order_sel 驱动 p3_sampler/read_leaf 的叶级层级（或修订 SCI 并在 provenance 记实际读取阶）；读/记录两侧一致后回归 | lib/phase3_session（＋后继 IR lib/infrastructure/scheduler 同型面） | 基线复跑该 sed 门：tile_order 来源改为 order_sel 且新增 dec/scale 组合回归绿 | 第一波 M6a-C-001（P0 OPEN）本轮在基线树复现仍成立  |
|  W2-LIB3-2 | lib/algorithms/projection/p3_projection.cpp:198-204/216（CAR）、243-255（AIT）（=原域 lib/phase3_proj） | ASTROCS_DESIGN §5.3（每种投影声明 CRPIX/CRVAL/CD/CTYPE）＋附录 B（FITS WCS Paper I/II；标准偏离仍是偏差）；FITS WCS Paper II §2.2（fiducial (α0,δ0) 与投影像原点配对） | 命令：git show 23f42ffa:lib/algorithms/projection/p3_projection.cpp ｜ sed -n '216p;243p'；输出：const double theta = dec_deg * kRad; / *dec_deg = theta * kDeg;（θ=δ 恒等、CRVAL2 不进映射；对照 v6 p3_proj_v6.cpp:71-96 已用 d0） | P0 | legacy registry 的 CAR/AIT 正反变换在 CRVAL2≠0 时给出错误天球↔像素映射；registry 标 IMPLEMENTED 且与 v6、会话 TAN 三处口径互斥 | CAR/AIT 按 Paper II 引入 (φ0,θ0) 旋转（与 v6 同源公式）或废 legacy 面；独立往返 Oracle 增加 CRVAL2 敏感用例 | lib/algorithms/projection（原 lib/phase3_proj 后继） | tests/unit p3_projection CAR/AIT oracle 增 dec0≠0 用例并通过 | 第一波 M1a-A-003/M1a-B-002（P0/P1 OPEN）复现仍成立；同属 P3-001  |
|  W2-LIB3-3 | lib/algorithms/projection/p3_projection.cpp:266（kRegistry[4]）＋p3_proj_v6.cpp:226-232（亦四行）；对照 docs/plugins/algorithms_phase3/14_projection.md:5/22/51 | ASTROCS_DESIGN §5.3:264「首批冻结 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA」 | 命令：git show 23f42ffa:lib/algorithms/projection/p3_projection.cpp ｜ sed -n '266p'；输出：const P3ProjectionSpec kRegistry[4] = {（仅 TAN/SIN/CAR/AIT）；基线 git grep STG/MOL/CEA/ZEA 于 lib/ tests/ 实现命中=0（仅 cfitsio 第三方与文档） | P1 | registry 实数=4≠冻结名单 8；14_projection.md:51「八投影全覆盖测试」在缺实现面上被声明；docs/science/PHASE3_PROJECTION（任务书对齐目标）不存在 | 补 STG/MOL/CEA/ZEA 注册＋独立往返 Oracle＋全八投影测试；或按负责人裁决流程收缩冻结名单并同步 §5.3/插件文档 | lib/algorithms/projection ＋ docs/plugins/algorithms_phase3 | 八投影往返 ctest 门存在且绿（或 §5.3 有负责人裁决记录） | 第一波 M6b-L16-18/RC8 状态面多头同型；GAP 对照未读（工程控制/** 本任务禁触），标 TASK_LIST P3-001  |
|  W2-LIB3-4 | lib/phase3_session/p3_wcs.cpp:62（仅TAN）；p3_session.cpp:97＋parse_request 无 mode 键；p3_v6_export.cpp（三模式+流式面，仅测试构建 tests/integration/v6_p3/CMakeLists.txt:36） | ASTROCS_DESIGN §5.3:267（输出模式显式 surface_brightness/point_source_flux/visualization；缺所需信息→拒绝或明确 unavailable）＋§5.2（流程含流式 FITS 输出） | 命令：git grep -c output_mode 23f42ffa -- lib/core/src/module_adapters.cpp；输出：0；git grep -l p3_v6_export 23f42ffa 仅命中 tests/integration/v6_p3/**（生产 IR 0 引用）；会话不拒绝未知字段（grep -c unknown p3_session.cpp=0） | P1 | 生产导出面无模式合同：新配置契约（contracts/schemas/phase_config_export.schema.json:113 必填 output_mode、:149-157 投影 enum 八值）与生产实现互斥；模式请求被静默吞（非"拒绝或明确 unavailable"）；§5.2 流式面（aio::FitsStreamWriter）未接产品 | IR/会话接 p3_v6_export（流式+三模式）或补模式门；请求层显式拒未知/未接线模式；schema↔实现一致性机器门 | lib/phase3_session ＋ lib/infrastructure/scheduler（后继 IR） | git grep -n output_mode 于生产源命中≥1 且模式缺失路径返回 UNSUPPORTED 的门测绿 | TASK_LIST P3-002（反向重采样/模式/流式 FITS）；与 W2-LIB3-3 同域  |
|  W2-LIB3-5 | runtime/io/fits_core.c:176（fio_file_flush=fflush 而已）＋1463-1483（flush→close→自校验→rename，全程无 fsync） | ASTROCS_DESIGN §9:421「临时文件/目录 + 校验 + fsync + 原子 rename 提交」 | 命令：git show 23f42ffa:runtime/io/fits_core.c ｜ grep -c fsync；输出：0（astrocs_io 属 install 组件：cmake/install_layout.cmake:41/59；对照 p3_output.cpp R10-C 已修成 flush→close→fsync→rename、hips_output_store.py os.fsync+_fsync_dir 在位） | P1 | IO-001 平台 DLL 的原子发布在断电/崩溃窗口可留下"已 rename 但未落盘"的假完整产品；与同仓其它发布路径行为不一致 | close 前加平台 fsync（Linux fsync / Windows _commit·FlushFileBuffers），rename 后按需目录 sync；补崩溃注入负例 | runtime/io ＋ modules/services/io ｜ grep -c "fsync｜FlushFileBuffers" runtime/io/fits_core.c ≥2 且新增负例门绿 | 新（第一波未见同条）；R10-C 同类缺陷的第二落点  |
|  W2-LIB3-6 | 基线 CMakeLists.txt:389-400（生产 target astrocs_phase2 源含 lib/phase2/src/cuda_bridge_stub.cpp、include dir 直指 lib/acr/backends/cuda/bridge；该库链入唯一 exe :629-633）；消费头 lib/acr/backends/cuda/bridge/cuda_bridge_api.hpp；另 lib/phase2/CMakeLists.txt:52-57（BUILD_TESTS=ON 时编译 ../acr/api/kernel_registry.cpp、../acr/scheduler/device_executor.cpp、cuda_bridge_loader.cpp(Windows)） | ASTROCS_DESIGN §8:410「生产构建/加载/路由/benchmark/发布均不含 ACR/CUDA」＋§1.3（ACR 仅保留源码 DORMANT）＋§7.1 | 命令：git show 23f42ffa:CMakeLists.txt ｜ sed -n '397p;400p'；输出：lib/phase2/src/cuda_bridge_stub.cpp) / ${CMAKE_CURRENT_SOURCE_DIR}/lib/acr/backends/cuda/bridge；cuda_bridge_stub.cpp:2 #include "cuda_bridge_api.hpp" 实证 | P1 | 生产构建图确实触达 lib/acr/** 树（include 路径+头消费）；"生产不含 ACR"的机器断言检不到本接线——ACK-ACR-005 自述"legacy cuda_bridge_stub.cpp 已排除出产品源白名单"=守卫自我豁免；默认测试构建还把 ACR 源编译入图 | 桥接 API 头移中立合同位（include/astrocs/ 或 contracts/），stub/loader 只依赖中立头；生产 include 路径零 lib/acr/**；lib/phase2 兼容目标去 ../acr 源 | CMakeLists(根)＋lib/phase2＋lib/acr/backends/cuda/bridge | git grep -n "lib/acr｜infrastructure/acr" -- CMakeLists.txt cli/CMakeLists.txt lib/phase2/CMakeLists.txt 命中=0 | 新；归属 DESIGN §8/CPU-001 线；GAP 对照未读（禁触工程控制）  |
|  W2-LIB3-7 | 批量 21 处/16 文件（同型归并；覆盖清单逐文件标注）：runtime 10 文件头注引 tasks/0[23]_*.md；runtime/pipeline/trace_replay.py:5（lib/core/src/trace.cpp）；lib/phase3_session/p3_v6_export.h:21-22（lib/phase3_proj/、lib/phase3_rsmp/）；lib/hips_p2/memory.md（lib/hips/src/…）；lib/acr/README.md、lib/acr/CMakeLists.txt、lib/acr/ci/ACR_DORMANT_GUARD.md（引 AstroCS_ENGINEERING_CONSTRAINTS.md §C.1——已不在树且非链上权威）；lib/hips_p2/README+module.yaml（引 11_MODULE_SOURCE_TEST_STANDARD.md、MODULE_MIGRATION_MATRIX.csv——均不在树）；lib/algorithms/fits_output/README.md:48/97（引 SCI-P3 §96，该文件 143 行无 §96） | ENGINEERING_SPEC §8「删除/重命名无悬空引用」；ASTROCS_DESIGN §0（权威链：链外文档不作冻结依据） | 命令：git ls-files tasks ｜ wc -l；输出：0；且 TraceStore::export_jsonl 实际位于 lib/core/src/context.cpp:387（trace.cpp 不存在）；git show 23f42ffa:docs/science/PHASE3_HIPS_TO_FITS.md ｜ grep -c "§96"；输出：0 | P2 | "冻结语义/权威规格/落位依据"锚点指向不存在文件→契约可追溯性断裂；约束实质由 DESIGN §8 承接（改引即可，不新增行为偏差） | 批量刷新指针：tasks/*.md→docs/interfaces/** 与 docs/contracts/RT-00x.md；trace.cpp→context.cpp；phase3_proj/rsmp/hips→后继路径；旧约束文档引用改 DESIGN §8/§1.3/§7.1；把"源码注释内路径引用"纳入某现有门 | runtime/** ＋ lib/phase3_session ＋ lib/hips_p2 ＋ lib/acr ＋ lib/algorithms/fits_output（文档面归 DOC-001/MOD-002） | 迁移后 git grep -l "tasks/0｜lib/phase3_proj/｜lib/hips/src｜src/trace.cpp｜§96" -- runtime lib/phase3_session lib/hips_p2 lib/acr lib/algorithms/fits_output 输出为空 | 第一波 M1a-C-008（§96 引用）同型；TASK_LIST MOD-002（32a5f5f3 立「迁移后引用刷新」）同源  |
|  W2-LIB3-8 | lib/phase3_session/p3_wcs.cpp:215-224（CTYPE1=/CTYPE2= 等号第7列；CUNIT/CRPIX/CD 系第8列）；同型 lib/algorithms/projection/p3_projection.cpp:380 | IAU FITS 4.0 §4.4.1（关键字占列1-8、等号第9列）；ENGINEERING_SPEC §8（无第9列断言=不能红） | 命令：git show 23f42ffa:lib/phase3_session/p3_wcs.cpp ｜ sed -n '215p'；输出：    add("CTYPE1= '" + ctype1 + "'");（对照真卡片正确实现：runtime/io/fits_core.c:206 raw[8]='=' 即第9列；p3_output 走 cfitsio fits_write_key） | P2 | 关键字文本面产出不合规卡片；本轮复核消费者=测试 printf/IR 诊断 JSON 字段 fits_keywords（lib/infrastructure/scheduler/src/module_adapters.cpp:5476），产品头卡不经此文本→较第一波 P1 降级并注明；若未来直写头卡即恢复 P1 | 文本生成改标准列位（关键字左对齐 8 列+第 9 列 =）或删函数改用统一卡片构造器；补列位断言 | lib/phase3_session ＋ lib/algorithms/projection | tests/unit 增断言：p3_wcs_fits_keywords 每卡第 9 列为 = （现必红） | 第一波 M1a-B-004（P1 OPEN）域内再验证；严重度按消费者分析调整并注明  |
|  W2-LIB3-9 | lib/phase3_session/p3_output.cpp:374（写路径无条件置1）vs p3_output.h:33（"1=coverage 头/数据一致"） | ENGINEERING_SPEC §8（声明与实现一致）；ASTROCS_DESIGN §12:510（不得以文档声明/无条件 PASS 冒充验收） | 命令：git show 23f42ffa:lib/phase3_session/p3_output.cpp ｜ sed -n '374p'；输出：        result->coverage_ok = 1;（真值判定仅在 verify 通道 :538；write 内调用 verify_ex 后只回收 reopen_ok，coverage_ok 不回填） | P2 | 新消费者按头注语义取 write 结果 coverage_ok 得无条件 PASS（判别力=0）；基线复现仍在 | write 路径以 v.coverage_ok 回填（或删写路径该字段并订正头注）；补"coverage 不一致→write 结果必红"单测 | lib/phase3_session | p3_output 单测：构造 coverage 头/数据不一致 → write 结果 coverage_ok==0（现恒 1 必红） | 第一波 M6a-B-002/I-DOC_HYGIENE（同字段两通道两套语义）同源，OPEN 复现  |
|  W2-LIB3-10 | lib/io/src/io_adapter.cpp:126-137（commit：校验→filesystem::rename，无 fsync）＋基线 CMakeLists.txt:106-108 astrocs_io_adapter 零链接消费者 | ASTROCS_DESIGN §9:421（校验+fsync+原子 rename）；ENGINEERING_SPEC §4（模块应有真实消费/测试位） | 命令：git show 23f42ffa:CMakeLists.txt ｜ grep -n astrocs_io_adapter；输出仅 :101/:106/:107/:108（无任何 target 链入）；收工时点该文件已 staged 删除、同名拷贝入住 lib/infrastructure/aio/io/src/io_adapter.cpp（diff=identical） | P2 | 孤儿 STATIC target＋与 W2-LIB3-5 同族的无 fsync 事务提交；同内容拷贝已随迁移带病入住 infrastructure 面 | 在后继 lib/infrastructure/aio/io/src/io_adapter.cpp 收口 fsync；删除孤儿 target 或真实接线并入门禁 | lib/infrastructure/aio/io（后继）＋CMakeLists(根) | 后继文件 commit 路径出现 fsync 且 astrocs_io_adapter 有消费者或已除名 | 与 W2-LIB3-5 同族（R10-C 类）；moved-wip 注记  |
|  W2-LIB3-11 | lib/phase3_session/README.md:11 vs lib/phase3_session/p3_session.cpp:112（硬编码 20000）＋:384（缺 output_dir 时默认写输入目录父级） | README 自述「最大尺寸来自配置合同, 不硬编码 20000 (P3-002)」；ASTROCS_DESIGN §6.3:340「运行产物只落配置 output_dir」 | 命令：git show 23f42ffa:lib/phase3_session/p3_session.cpp ｜ sed -n '112p;384p'；输出：if (wpx < 1 ｜｜ wpx > 20000 ｜｜ hpx < 1 ｜｜ hpx > 20000) { / const std::string out_path = std::string(hips_dir) + "/../output_phase3.fits"; | P2 | 尺寸上限双源：以 ASTROCS_P3_MAX_SIDE 覆盖 p3_wcs 后会话仍以 20000 先行判定（单源失真）；未给 output_dir 时产品写进输入 HiPS 同级目录（产物落位合同旁路） | 上限收敛到编译/配置单源常量；output_dir 必填（缺失显式拒绝而非回退） | lib/phase3_session ＋ cli 预检面 | 单测：无 output_dir 请求被拒；ASTROCS_P3_MAX_SIDE=1024 构建后会话拒 1025（现必红） | 第一波 P3-002「配置合同上限」议题同源  |
|  W2-LIB3-12 | runtime/io/hips_core.c:217（界 ACS_HIPS_ORDER_MAX=29：modules/services/io/include/astrocs/io/hips_input_v1.h:31）vs lib/phase3_session/hips_properties.h:24（kMaxOrder=20）；且 hips_core.c 不入任何构建 target（基线 CMakeLists.txt:146 astrocs_io 仅 fits_core.c；install_layout:15-16 注释称 astrocs_io=FITS/HiPS 宿主） | ASTROCS_DESIGN §9（aio 唯一 HiPS 读写边界——两读面界值互斥）＋§7.3（模块必备 CMake target/单一 entrypoint） | 命令：grep -n ACS_HIPS_ORDER_MAX modules/services/io/include/astrocs/io/hips_input_v1.h；输出：#define ACS_HIPS_ORDER_MAX 29；git show 23f42ffa:lib/phase3_session/hips_properties.h ｜ grep -n kMaxOrder；输出：static constexpr int kMaxOrder = 20; | P2 | 同一 HiPS 读界两实现 20 vs 29 不一致（order 21..29 一个面合法一个面拒）；hips_core.c 仅 tests/io ad-hoc gcc 编译（test_hips_input_contract.py:36）→产品 target 与注释宣称能力面不符（骨架有"不伪装完成"自述，界值不一致仍属实质） | order 上限收敛单源常量；hips_core.c 入 astrocs_io target 或注释/契约订正为 contract-only 面 | runtime/io ＋ lib/infrastructure/aio ＋ modules/services/io | 两读面引用同一 ORDER_MAX 常量且 astrocs_io 源列表含 hips_core.c（或两处注释订正） | 新；INT-001（构建图/清单一致）同源标注  |
|  W2-LIB3-13 | lib/healpix_db/healpix_browser_qt/（基线 48 文件落位）＋packaging/launch/start_browser.ps1:23（硬编码 lib\healpix_db\healpix_browser_qt 路径） | ASTROCS_DESIGN §7.1:380（hips_browser 归属 lib/infrastructure/hips_browser）；§1.3/§10.1「不进产品 manifest」——该项验证通过（页眉证据） | 命令：git show 23f42ffa:CMakeLists.txt ｜ grep -ci "browser｜qt"；输出：0；grep -icE "browser｜qt" packaging/astrocs.product.json；输出：0 | P2 | 产品面零渗透（实证合规）；唯落位偏离 §7.1——迁往 lib/infrastructure/hips_browser 的 48 文件为并行线未提交 WIP（moved-wip）；launcher/ctest_baseline 旧路径在迁移落地瞬间将悬空 | 迁移落地同批刷新 packaging/launch 与 ci/ctest_baseline.json 路径；保留"不入产品面"负例门 | lib/infrastructure/hips_browser（迁移中）＋packaging/launch | 迁移后 git grep -l healpix_browser_qt -- packaging/ ci/ctest_baseline.json 无旧路径残留 | TASK_LIST ARCH-001/MOD-002 同源；本条不计产品面违规  |
|  W2-LIB3-14 | lib/acr/ci/check_acr_dormant.py（ACK-ACR-001..007 静态守卫）——未登记且当前红 | ENGINEERING_SPEC §8（每项检查有执行落点、能红能绿）；ASTROCS_DESIGN §8/§12（发布前提"ACR 生产不可达"） | 命令：timeout 120 python3 lib/acr/ci/check_acr_dormant.py --repo . ；输出：ACR_DORMANT_FAIL - ACK-ACR-004: ACR/CUDA reference in cmake/install_layout.cmake: [ # missing_unit_file → 退出 5(ACR BACKEND)… ]；EXIT=1（命中为注释文本误报）；ci/checks.json 38 项中 acr 相关 0；.github/workflows grep acr=0 | P2 | "ACR 隔离"的唯一静态机器守卫不在任何 CI 注册表/工作流执行，且现态即红（红→更无人接线）；实质约束目前靠人肉维持 | 排除 # 注释行修误报；把该守卫登记进 ci/checks.json（P2 门禁）或并入 CHK-STATIC；与 W2-LIB3-6 一并收口 | lib/acr/ci（后继 lib/infrastructure/acr/ci）＋ci/checks.json ｜ python3 lib/acr/ci/check_acr_dormant.py --repo . 返回 0 且 ci/checks.json grep 到其注册条目 | 新；另记本轮实测 CHK-DANGLING、ENG-CONSTRAINTS 两注册门在工作树 FAIL（失败样本在 docs/**、含并行线未提交改动，移交 CI-001/DOC 线干净树复验）  |

---

## 二、覆盖清单（基线 23f42ffa 的 tracked 文件逐条，317 行；每行 <相对路径>	<VERDICT>）

口径：原任务书 8 前缀∩基线 tracked ＝ lib/phase3_session(13)＋runtime(32)＝45；父增补 lib/acr(218)＋lib/io(1)；旧域后继纳入：lib/hips→lib/hips_p2(3,字符串前缀命中；实体 hips 模块 10 文件已 R100 迁移至 lib/algorithms/drizzle/hips=他域)、lib/hips_browser→lib/healpix_db/healpix_browser_qt(48,browser 实体)、lib/astro_sphere→healpix_drizzle/astro_sphere_sink(2,唯一命中)。lib/phase3_proj/rsmp/fits 前缀基线 0 tracked（迁移至 lib/algorithms/*，后继面由对应线覆盖；本域发现 W2-LIB3-2/3/4/7 已在表中给出后继定位）。

lib/acr/.gitignore	NA:dormant-isolated
lib/acr/CMakeLists.txt	FINDING:W2-LIB3-7
lib/acr/README.md	FINDING:W2-LIB3-7
lib/acr/api/CMakeLists.txt	NA:dormant-isolated
lib/acr/api/event.cpp	NA:dormant-isolated
lib/acr/api/kernel_registry.cpp	NA:dormant-isolated
lib/acr/backends/classic/CMakeLists.txt	NA:dormant-isolated
lib/acr/backends/classic/classic_kernels.cpp	NA:dormant-isolated
lib/acr/backends/classic/classic_kernels.hpp	NA:dormant-isolated
lib/acr/backends/cpu/CMakeLists.txt	NA:dormant-isolated
lib/acr/backends/cpu/isa/avx.cpp	NA:dormant-isolated
lib/acr/backends/cpu/isa/avx2.cpp	NA:dormant-isolated
lib/acr/backends/cpu/isa/avx512.cpp	NA:dormant-isolated
lib/acr/backends/cpu/isa/dispatch.cpp	NA:dormant-isolated
lib/acr/backends/cpu/isa/isa_kernels.hpp	NA:dormant-isolated
lib/acr/backends/cpu/isa/scalar.cpp	NA:dormant-isolated
lib/acr/backends/cpu/isa/sse.cpp	NA:dormant-isolated
lib/acr/backends/cuda/CMakeLists.txt	NA:dormant-isolated
lib/acr/backends/cuda/bridge/acr_cuda_bridge.h	NA:dormant-isolated
lib/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp	NA:dormant-isolated
lib/acr/backends/cuda/bridge/acr_cuda_bridge_kernels.cu	NA:dormant-isolated
lib/acr/backends/cuda/bridge/cuda_bridge_api.hpp	FINDING:W2-LIB3-6
lib/acr/backends/cuda/cuda_backend.cu	NA:dormant-isolated
lib/acr/backends/cuda/cuda_backend.hpp	NA:dormant-isolated
lib/acr/backends/cuda/cuda_bridge_loader.cpp	NA:dormant-isolated
lib/acr/backends/cuda/cuda_buffer.cpp	NA:dormant-isolated
lib/acr/backends/cuda/cuda_buffer.hpp	NA:dormant-isolated
lib/acr/backends/cuda/cuda_executor.cpp	NA:dormant-isolated
lib/acr/backends/cuda/cuda_executor.hpp	NA:dormant-isolated
lib/acr/ci/ACR_DORMANT_GUARD.md	FINDING:W2-LIB3-7
lib/acr/ci/check_acr_dormant.py	FINDING:W2-LIB3-14
lib/acr/ci/path_guard.ps1	NA:dormant-isolated
lib/acr/ci/sha256_utf8.py	NA:dormant-isolated
lib/acr/core/CMakeLists.txt	NA:dormant-isolated
lib/acr/core/runtime.cpp	NA:dormant-isolated
lib/acr/core/task_descriptor.cpp	NA:dormant-isolated
lib/acr/core/task_descriptor.hpp	NA:dormant-isolated
lib/acr/cost/CMakeLists.txt	NA:dormant-isolated
lib/acr/cost/cost_estimator.cpp	NA:dormant-isolated
lib/acr/cost/cost_estimator.hpp	NA:dormant-isolated
lib/acr/diagnostics/CMakeLists.txt	NA:dormant-isolated
lib/acr/diagnostics/hardware_report.cpp	NA:dormant-isolated
lib/acr/docs/ADR-001-alpaka.md	NA:dormant-isolated
lib/acr/docs/ADR-002-oneTBB.md	NA:dormant-isolated
lib/acr/docs/ADR-003-hwloc.md	NA:dormant-isolated
lib/acr/docs/ADR-004-cpu_features.md	NA:dormant-isolated
lib/acr/docs/ADR-005-google-benchmark.md	NA:dormant-isolated
lib/acr/docs/ADR-006-googletest.md	NA:dormant-isolated
lib/acr/docs/ADR-007-starpu-optional.md	NA:dormant-isolated
lib/acr/docs/ADR-008-cmake-fetchcontent.md	NA:dormant-isolated
lib/acr/docs/ADR-009-cpu-only-build-gate.md	NA:dormant-isolated
lib/acr/docs/ADR-010-delete-per-kernel-routing.md	NA:dormant-isolated
lib/acr/docs/audit-report.md	NA:dormant-isolated
lib/acr/docs/dependency-lock.json	NA:dormant-isolated
lib/acr/docs/forbidden-paths.md	NA:dormant-isolated
lib/acr/examples/CMakeLists.txt	NA:dormant-isolated
lib/acr/examples/cuda_axpy.cu	NA:dormant-isolated
lib/acr/examples/legacy_chunk_adapter.cpp	NA:dormant-isolated
lib/acr/examples/minimal_parallel_for.cpp	NA:dormant-isolated
lib/acr/examples/weighted_integration/CMakeLists.txt	NA:dormant-isolated
lib/acr/examples/weighted_integration/route_profile_calibration.cpp	NA:dormant-isolated
lib/acr/examples/weighted_integration/route_profile_calibration.hpp	NA:dormant-isolated
lib/acr/examples/weighted_integration/weighted_integration_benchmark.cpp	NA:dormant-isolated
lib/acr/examples/weighted_integration/weighted_integration_common.hpp	NA:dormant-isolated
lib/acr/examples/weighted_integration/weighted_integration_kernels.cpp	NA:dormant-isolated
lib/acr/examples/weighted_integration/weighted_integration_kernels.hpp	NA:dormant-isolated
lib/acr/include/astro/compute/acr.hpp	NA:dormant-isolated
lib/acr/include/astro/compute/hardware_profile.hpp	NA:dormant-isolated
lib/acr/include/astro/compute/kernel_registry.hpp	NA:dormant-isolated
lib/acr/include/astro/compute/runtime_internal.h	NA:dormant-isolated
lib/acr/include/astro/compute/task_traits.hpp	NA:dormant-isolated
lib/acr/include/astro/compute/topology.hpp	NA:dormant-isolated
lib/acr/memory.md	NA:dormant-isolated
lib/acr/profile/CMakeLists.txt	NA:dormant-isolated
lib/acr/profile/profile_reader.cpp	NA:dormant-isolated
lib/acr/profile/profile_reader.hpp	NA:dormant-isolated
lib/acr/qualification/CMakeLists.txt	NA:dormant-isolated
lib/acr/qualification/benchmark_driver.cpp	NA:dormant-isolated
lib/acr/qualification/benchmark_driver.hpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/arithmetic_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/atomic_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/benchmark_common.hpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/benchmark_main.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/branch_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/convolution_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/irregular_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/numa_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/overhead_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/reduction_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/stream_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/thread_curve_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/benchmarks/transfer_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/focused/acr_benchmark_focused_main.cpp	NA:dormant-isolated
lib/acr/qualification/focused/focused_benchmark.cpp	NA:dormant-isolated
lib/acr/qualification/focused/focused_benchmark.hpp	NA:dormant-isolated
lib/acr/qualification/focused/focused_operations.cpp	NA:dormant-isolated
lib/acr/qualification/focused/focused_operations.hpp	NA:dormant-isolated
lib/acr/qualification/focused/operation_profile.cpp	NA:dormant-isolated
lib/acr/qualification/focused/operation_profile.hpp	NA:dormant-isolated
lib/acr/qualification/profile_generator.cpp	NA:dormant-isolated
lib/acr/qualification/profile_generator.hpp	NA:dormant-isolated
lib/acr/qualification/profile_schema.hpp	NA:dormant-isolated
lib/acr/routing/CMakeLists.txt	NA:dormant-isolated
lib/acr/routing/benchmark_route_estimator.cpp	NA:dormant-isolated
lib/acr/routing/benchmark_route_estimator.hpp	NA:dormant-isolated
lib/acr/routing/route_profile_v2.cpp	NA:dormant-isolated
lib/acr/routing/route_profile_v2.hpp	NA:dormant-isolated
lib/acr/scheduler/CMakeLists.txt	NA:dormant-isolated
lib/acr/scheduler/current_state.cpp	NA:dormant-isolated
lib/acr/scheduler/current_state.hpp	NA:dormant-isolated
lib/acr/scheduler/device_executor.cpp	NA:dormant-isolated
lib/acr/scheduler/device_executor.hpp	NA:dormant-isolated
lib/acr/scheduler/dispatcher.cpp	NA:dormant-isolated
lib/acr/scheduler/dispatcher.hpp	NA:dormant-isolated
lib/acr/scheduler/fallback.cpp	NA:dormant-isolated
lib/acr/scheduler/fallback.hpp	NA:dormant-isolated
lib/acr/scheduler/mixed_route_planner.cpp	NA:dormant-isolated
lib/acr/scheduler/mixed_route_planner.hpp	NA:dormant-isolated
lib/acr/scheduler/mixed_runner.cpp	NA:dormant-isolated
lib/acr/scheduler/mixed_runner.hpp	NA:dormant-isolated
lib/acr/scheduler/partitioner.cpp	NA:dormant-isolated
lib/acr/scheduler/partitioner.hpp	NA:dormant-isolated
lib/acr/scheduler/queue_aware.cpp	NA:dormant-isolated
lib/acr/scheduler/queue_aware.hpp	NA:dormant-isolated
lib/acr/scheduler/reduction_merger.cpp	NA:dormant-isolated
lib/acr/scheduler/reduction_merger.hpp	NA:dormant-isolated
lib/acr/scheduler/residency_manager.cpp	NA:dormant-isolated
lib/acr/scheduler/residency_manager.hpp	NA:dormant-isolated
lib/acr/scheduler/shared_work_pool.cpp	NA:dormant-isolated
lib/acr/scheduler/shared_work_pool.hpp	NA:dormant-isolated
lib/acr/schemas/compute_config.example.yaml	NA:dormant-isolated
lib/acr/schemas/evidence_manifest.schema.json	NA:dormant-isolated
lib/acr/schemas/hardware_profile.example.json	NA:dormant-isolated
lib/acr/schemas/hardware_profile.schema.json	NA:dormant-isolated
lib/acr/schemas/task_descriptor.schema.json	NA:dormant-isolated
lib/acr/tests/CMakeLists.txt	NA:dormant-isolated
lib/acr/tests/classic/CMakeLists.txt	NA:dormant-isolated
lib/acr/tests/classic/classic_common.hpp	NA:dormant-isolated
lib/acr/tests/classic/classic_main.cpp	NA:dormant-isolated
lib/acr/tests/classic/cpu_partition_coverage.cpp	NA:dormant-isolated
lib/acr/tests/classic/e01_memory.cpp	NA:dormant-isolated
lib/acr/tests/classic/e02_axpy.cpp	NA:dormant-isolated
lib/acr/tests/classic/e03_dot.cpp	NA:dormant-isolated
lib/acr/tests/classic/e04_transpose.cpp	NA:dormant-isolated
lib/acr/tests/classic/e05_convolution.cpp	NA:dormant-isolated
lib/acr/tests/classic/e06_resample.cpp	NA:dormant-isolated
lib/acr/tests/classic/e07_histogram.cpp	NA:dormant-isolated
lib/acr/tests/classic/e08_scan.cpp	NA:dormant-isolated
lib/acr/tests/classic/e09_gather_scatter.cpp	NA:dormant-isolated
lib/acr/tests/classic/e10_mandelbrot.cpp	NA:dormant-isolated
lib/acr/tests/classic/e11_gemm.cpp	NA:dormant-isolated
lib/acr/tests/classic/e12_fft.cpp	NA:dormant-isolated
lib/acr/tests/classic/e15_failure.cpp	NA:dormant-isolated
lib/acr/tests/classic/e16_concurrency.cpp	NA:dormant-isolated
lib/acr/tests/classic/e17_model_fit.cpp	NA:dormant-isolated
lib/acr/tests/classic/e18_workpool.cpp	NA:dormant-isolated
lib/acr/tests/classic/e20_fault_fallback.cpp	NA:dormant-isolated
lib/acr/tests/classic/e21_persistence_concurrency.cpp	NA:dormant-isolated
lib/acr/tests/fault/CMakeLists.txt	NA:dormant-isolated
lib/acr/tests/fault/exit_safe.hpp	NA:dormant-isolated
lib/acr/tests/fault/fault_injection.cpp	NA:dormant-isolated
lib/acr/tests/fault/lifecycle_smoke.cpp	NA:dormant-isolated
lib/acr/tests/fault/persistence.cpp	NA:dormant-isolated
lib/acr/tests/fault/sanitizer_actual.cpp	NA:dormant-isolated
lib/acr/tests/integration/CMakeLists.txt	NA:dormant-isolated
lib/acr/tests/integration/test_weighted_integration.cpp	NA:dormant-isolated
lib/acr/tests/sanitizer/msvc_asan_main.cpp	NA:dormant-isolated
lib/acr/tests/sanitizer/run_acr_sanitizers.ps1	NA:dormant-isolated
lib/acr/tests/unit/CMakeLists.txt	NA:dormant-isolated
lib/acr/tests/unit/test_api.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_api_traits.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_buffer.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_cost.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_cpu_profile.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_cuda.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_cuda_bridge.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_cuda_bridge_chunk.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_device_executor.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_dispatcher_bdr.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_focused_mixed.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_focused_operation.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_hardware_profile.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_invocation_dispatch.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_kernel_registry.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_mixed_route.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_profile_holdout.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_qualification.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_residency.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_resource_control.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_route_calibration.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_route_estimator.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_scheduler.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_task_descriptor.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_topology.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_utilization.cpp	NA:dormant-isolated
lib/acr/tests/unit/test_work_pool.cpp	NA:dormant-isolated
lib/acr/tools/acr_benchmark/CMakeLists.txt	NA:dormant-isolated
lib/acr/tools/acr_benchmark/main.cpp	NA:dormant-isolated
lib/acr/tools/acr_classic_runner/CMakeLists.txt	NA:dormant-isolated
lib/acr/tools/acr_classic_runner/main.cpp	NA:dormant-isolated
lib/acr/tools/acr_report/CMakeLists.txt	NA:dormant-isolated
lib/acr/tools/acr_report/main.cpp	NA:dormant-isolated
lib/acr/tools/acr_status/CMakeLists.txt	NA:dormant-isolated
lib/acr/tools/acr_status/main.cpp	NA:dormant-isolated
lib/acr/topology/CMakeLists.txt	NA:dormant-isolated
lib/acr/topology/cpu_features.cpp	NA:dormant-isolated
lib/acr/topology/hwloc_topo.cpp	NA:dormant-isolated
lib/acr/utilization/CMakeLists.txt	NA:dormant-isolated
lib/acr/utilization/actual_tracker.cpp	NA:dormant-isolated
lib/acr/utilization/actual_tracker.hpp	NA:dormant-isolated
lib/acr/utilization/config_hot_read.cpp	NA:dormant-isolated
lib/acr/utilization/config_hot_read.hpp	NA:dormant-isolated
lib/acr/utilization/memory_budget.cpp	NA:dormant-isolated
lib/acr/utilization/memory_budget.hpp	NA:dormant-isolated
lib/acr/utilization/staging_ledger.cpp	NA:dormant-isolated
lib/acr/utilization/staging_ledger.hpp	NA:dormant-isolated
lib/acr/utilization/system_metrics.cpp	NA:dormant-isolated
lib/acr/utilization/system_metrics.hpp	NA:dormant-isolated
lib/healpix_db/healpix_browser_qt/CMakeLists.txt	FINDING:W2-LIB3-13
lib/healpix_db/healpix_browser_qt/Makefile	OK
lib/healpix_db/healpix_browser_qt/README.md	OK
lib/healpix_db/healpix_browser_qt/app/browser_cli.cpp	OK
lib/healpix_db/healpix_browser_qt/app/main.cpp	OK
lib/healpix_db/healpix_browser_qt/app/main_window.cpp	OK
lib/healpix_db/healpix_browser_qt/app/main_window.h	OK
lib/healpix_db/healpix_browser_qt/app/stf_bar.cpp	OK
lib/healpix_db/healpix_browser_qt/app/stf_bar.h	OK
lib/healpix_db/healpix_browser_qt/app/stf_panel.cpp	OK
lib/healpix_db/healpix_browser_qt/app/stf_panel.h	OK
lib/healpix_db/healpix_browser_qt/core/browser_backend.cpp	OK
lib/healpix_db/healpix_browser_qt/core/browser_backend.h	OK
lib/healpix_db/healpix_browser_qt/core/gl_renderer.cpp	OK
lib/healpix_db/healpix_browser_qt/core/gl_renderer.h	OK
lib/healpix_db/healpix_browser_qt/core/healpix_math.cpp	OK
lib/healpix_db/healpix_browser_qt/core/healpix_math.h	OK
lib/healpix_db/healpix_browser_qt/core/hips_browser_backend.cpp	OK
lib/healpix_db/healpix_browser_qt/core/hips_browser_backend.h	OK
lib/healpix_db/healpix_browser_qt/core/hips_sky_view.cpp	OK
lib/healpix_db/healpix_browser_qt/core/hips_sky_view.h	OK
lib/healpix_db/healpix_browser_qt/core/logger.h	OK
lib/healpix_db/healpix_browser_qt/core/stf_engine.cpp	OK
lib/healpix_db/healpix_browser_qt/core/stf_engine.h	OK
lib/healpix_db/healpix_browser_qt/deploy.ps1	OK
lib/healpix_db/healpix_browser_qt/include/healpix_browser_core.h	OK
lib/healpix_db/healpix_browser_qt/memory.md	OK
lib/healpix_db/healpix_browser_qt/run_healpix.bat	OK
lib/healpix_db/healpix_browser_qt/tests/debug_healpix.cpp	OK
lib/healpix_db/healpix_browser_qt/tests/debug_trace.cpp	OK
lib/healpix_db/healpix_browser_qt/tests/gen_hips_browser_test.py	OK
lib/healpix_db/healpix_browser_qt/tests/gen_tiny_hiss.cpp	OK
lib/healpix_db/healpix_browser_qt/tests/test_browser_backend.cpp	OK
lib/healpix_db/healpix_browser_qt/tests/test_browser_dual_dtype.cpp	OK
lib/healpix_db/healpix_browser_qt/tests/test_geometry_truth.cpp	OK
lib/healpix_db/healpix_browser_qt/tests/test_healpix_math.cpp	OK
lib/healpix_db/healpix_browser_qt/tests/test_hips_browser_backend.cpp	OK
lib/healpix_db/healpix_browser_qt/tests/test_stf_engine.cpp	OK
lib/healpix_db/healpix_browser_qt/tests/trace_browser_lineage.cpp	OK
lib/healpix_db/healpix_browser_qt/tools/gen_geometry_truth.py	OK
lib/healpix_db/healpix_browser_qt/tools/gen_ref_source.py	OK
lib/healpix_db/healpix_browser_qt/tools/hips_tile_oracle.py	OK
lib/healpix_db/healpix_browser_qt/widgets/abstract_view.cpp	OK
lib/healpix_db/healpix_browser_qt/widgets/abstract_view.h	OK
lib/healpix_db/healpix_browser_qt/widgets/hips_view.cpp	OK
lib/healpix_db/healpix_browser_qt/widgets/hips_view.h	OK
lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp	OK
lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h	OK
lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp	OK
lib/healpix_db/healpix_drizzle/astro_sphere_sink.h	OK
lib/hips_p2/README.md	OK
lib/hips_p2/memory.md	FINDING:W2-LIB3-7
lib/hips_p2/module.yaml	OK
lib/io/src/io_adapter.cpp	FINDING:W2-LIB3-10
lib/phase3_session/README.md	FINDING:W2-LIB3-11
lib/phase3_session/hips_properties.cpp	OK
lib/phase3_session/hips_properties.h	FINDING:W2-LIB3-12
lib/phase3_session/p3_output.cpp	FINDING:W2-LIB3-9
lib/phase3_session/p3_output.h	FINDING:W2-LIB3-9
lib/phase3_session/p3_resample.cpp	FINDING:W2-LIB3-1
lib/phase3_session/p3_resample.h	OK
lib/phase3_session/p3_session.cpp	FINDING:W2-LIB3-1
lib/phase3_session/p3_session.h	OK
lib/phase3_session/p3_v6_export.cpp	FINDING:W2-LIB3-4
lib/phase3_session/p3_v6_export.h	FINDING:W2-LIB3-7
lib/phase3_session/p3_wcs.cpp	FINDING:W2-LIB3-8
lib/phase3_session/p3_wcs.h	OK
runtime/artifact_store/artifact_abi_v1.c	OK
runtime/artifact_store/artifact_manifest_validator.py	OK
runtime/artifact_store/phase_product_exchange_validator.py	OK
runtime/artifact_store/production_store.py	FINDING:W2-LIB3-7
runtime/artifact_store/provenance.py	FINDING:W2-LIB3-7
runtime/core/phase_lifecycle.README.md	FINDING:W2-LIB3-7
runtime/core/phase_lifecycle.py	FINDING:W2-LIB3-7
runtime/io/fits_core.c	FINDING:W2-LIB3-5
runtime/io/fits_verify.py	OK
runtime/io/hips_core.c	FINDING:W2-LIB3-12
runtime/io/hips_output_store.py	FINDING:W2-LIB3-7
runtime/logging/README.md	OK
runtime/logging/log_event.py	OK
runtime/logging/log_event_v1.schema.json	FINDING:W2-LIB3-7
runtime/module_loader/README.md	OK
runtime/module_loader/secure_loader.c	OK
runtime/module_loader/secure_loader.h	FINDING:W2-LIB3-7
runtime/monitoring/__init__.py	OK
runtime/monitoring/linux_procfs.py	OK
runtime/monitoring/monitor.py	FINDING:W2-LIB3-7
runtime/monitoring/runner.py	OK
runtime/monitoring/trace_feed.py	OK
runtime/monitoring/windows_pdh_etw.py	OK
runtime/pipeline/fixtures/phase2_typed_dag.json	OK
runtime/pipeline/module_ports.registry.json	OK
runtime/pipeline/trace_replay.py	FINDING:W2-LIB3-7
runtime/pipeline/typed_dag.py	FINDING:W2-LIB3-7
runtime/pipeline/typed_dag.schema.json	OK
runtime/pipeline/typed_dag_contract.h	OK
runtime/registry/module_registry.c	OK
runtime/registry/module_registry.h	FINDING:W2-LIB3-7
runtime/v6_budget.py	OK

### 附：域内未跟踪/被忽略构建残留（UNTRACKED-IGNORED，不计入 tracked 统计）
runtime/io/libhips_core_test.so	NA:binary-ignored-build-artifact
runtime/io/libfits_core_test.so	NA:binary-ignored-build-artifact
runtime/io/__pycache__/hips_output_store.cpython-313.pyc	NA:generated
runtime/io/__pycache__/fits_verify.cpython-313.pyc	NA:generated
runtime/logging/__pycache__/log_event.cpython-313.pyc	NA:generated
runtime/pipeline/__pycache__/trace_replay.cpython-313.pyc	NA:generated
runtime/pipeline/__pycache__/typed_dag.cpython-313.pyc	NA:generated
runtime/core/__pycache__/phase_lifecycle.cpython-313.pyc	NA:generated
runtime/monitoring/__pycache__/__init__.cpython-313.pyc	NA:generated
runtime/monitoring/__pycache__/runner.cpython-313.pyc	NA:generated
runtime/monitoring/__pycache__/linux_procfs.cpython-313.pyc	NA:generated
runtime/monitoring/__pycache__/monitor.cpython-313.pyc	NA:generated
runtime/monitoring/__pycache__/trace_feed.cpython-313.pyc	NA:generated
runtime/monitoring/__pycache__/windows_pdh_etw.cpython-313.pyc	NA:generated
runtime/artifact_store/__pycache__/phase_product_exchange_validator.cpython-313.pyc	NA:generated
runtime/artifact_store/__pycache__/provenance.cpython-313.pyc	NA:generated
runtime/artifact_store/__pycache__/artifact_manifest_validator.cpython-313.pyc	NA:generated
runtime/artifact_store/__pycache__/production_store.cpython-313.pyc	NA:generated

### 统计
files_total: 317（tracked@23f42ffa；另有 18 个 gitignore 构建残留 .so×2/__pycache__ .pyc×16 列于附行，不计）
verdict_counts: OK=74 / FINDING=30（14 个 ID 中的 12 个有覆盖行落点：1,4,5,6,7,8,9,10,11,12,13,14）/ NA:dormant-isolated=213
枚举命令逐字：
  cd "/workspace/Astro CS Database" && timeout 30 git ls-files lib/phase3_proj lib/phase3_rsmp lib/phase3_fits lib/phase3_session lib/hips lib/hips_browser lib/astro_sphere runtime lib/acr lib/io
  cd "/workspace/Astro CS Database" && timeout 30 git ls-tree -r 23f42ffa --name-only | grep -E "^lib/healpix_db/healpix_browser_qt/|^lib/healpix_db/healpix_drizzle/astro_sphere_sink|^lib/hips_p2/"
  cd "/workspace/Astro CS Database" && timeout 30 git status --porcelain --untracked-files=all -- lib/acr lib/io lib/phase3_session runtime ＆ find runtime -type f 与 git ls-files 求差（18 项均为 .gitignore 命中：*.so / __pycache__/）
