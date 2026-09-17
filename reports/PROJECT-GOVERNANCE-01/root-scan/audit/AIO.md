# AIO 轴审计报告 — I/O 所有权、manifest 与原子产品（ROOT-004 扩展轴）

- **轴名**：AIO（io 所有权分散 / manifest 生成点与字段 / 原子发布 / config 分离 / 三命令串接 / exit 码）
- **开工基线**（父指令新政策：三者相等即可开工）：`git rev-parse HEAD main origin/main` 实测三口径一致 = `900916fb0dfe93e21bd36c09908a6cc679de5256`（branch=main）。
- **收工基线**：审计期间并行线持续推进提交，收工实测 HEAD=main=`180c8a0ad9755e513670c7a1feb8fa215a87e1d7`（新增 f776da71/23acb453/9d9b2bb5/180c8a0a 等），origin/main=`b4afc135848d8b401ee36278effcb06699542fec`（本地领先，属已知并行执行线情况，不停工）。**证据抖动核对**：`git diff --stat 900916fb..HEAD` 对本轴全部证据文件（cli/**、lib/astro_image_io/**、lib/core/src/**、lib/orchestrator/cpp/**、lib/hips/**、lib/io/**、runtime/io/**、CMakeLists.txt、contracts/data/**、include/astrocs/**）零命中；11 条发现的锚点行全部回归复跑一致——**受影响条目：无**。工作树含并行线未提交改动（开工 158 条），本轴证据一律以当前工作树实况为准。
- **方法**：只读审计。grep/find（排除巨目录 {run,build,out,artifacts,evidence,reports,graph,worktrees,Testing,logs,third_party,设计大纲,GaiaDR3,GaiaDR3SP}）+ read 重定位行号 + python 解析 idmap.csv 对照旧清单；所有证据命令本轮真跑，逐字输出 ≤3 行。
- **摘要**（≤10 行）：
  1. I/O 所有权仍多边界并存：`astrocs_io` SHARED 仅含 runtime/io/fits_core.c（CMakeLists.txt:146），HiPS 读写在 astrocs_hips STATIC、FITS 面在 astrocs_aio STATIC（另 vendored cfitsio），phase3 直调 cfitsio 32 处，healpix_db/adapter/phase2_int 各有裸 FITS 解析——唯一 AIO 不成立（承 GAP-013）。
  2. **P0**：生产 HiPS 写出为直写：tile 先 `std::remove` 再 `fits_create_file` 落最终路径，manifest.json fopen("wb") 就地写，整文件 0 rename/fsync；而注释自称"AIO-002 原子发布原语内建于 aio_hips 落盘路径"与事实相反（覆写中断=新旧同灭）。
  3. 原子发布原语 ≥10 处独立实现（含 Python 侧），lib/hips/src/aio_publish.cpp 自称"唯一实现"失实；p1_atomic_publish 兜底"先删后 rename"与 hiss_stream_writer 同仓纪律互斥。
  4. run 记录七件套（DESIGN §9:422）未按名录产出：run-plan.json / run-trace.jsonl / artifact-manifest.json / run-summary.json 无生产者；started_utc/finished_utc 同刻取"now"（计划值冒充实际值）。
  5. manifest 方言 ≥5 套并行；DATA-001 artifact_manifest 合同+验证器在 C++ 生产面零接线（artifact_manifest/storage_uri 于 lib+cli C++ 命中 0）。run_manifest 的 config_sha256 冻结与 resume 哈希比对已核实合规（正面项）。
  6. 三节点间 typed 产物与 resource 记录就地覆写（p2_write_text/p2_write_bin、p3_props/p3_wcs、resource_recorder），下游直读半写 JSON 风险。
  7. config/ 仍缺失（承 GAP-006）：滤镜库实体落在 lib/photometric_calib/data/、"Baader *"名映射硬编码于 orchestrator；默认参数嵌在 Stage1Config 结构体；暗场-亮场容差 exposure_tol=0.5s 硬编码，与已裁决 5s（GAP_AUDIT §6 R-003）不一致，且无 defaults.json 落点。
  8. exit 码：orchestrator 第二套词表直接 return 进程退出码（7=CONFIG/8=FILE_IO/10=CANCELLED 与 §6.3 冻结表语义冲突，自称一致的 error_code_registry.csv 不存在）；§6.3 声明的唯一源路径 include/astrocs/exit_codes.h 不存在（实际在 cli/，11 值与表一致）。
  9. output_dir 缺省 "." 的 CWD 隐式写出机制仍活体（cli 11 处 + adapters 19 处），是根目录 62 个 astrocs_run_*.json（GAP-021/022）的成因残留。
  10. 三命令隐式串接：**未复现**（phase run 各自单阶段 {1}/{2}/{3}；orchestrator 仅 stage1；run_stage2 零调用方）——不立条；DATASUM 读侧不复算（读校验失败拒条款缺执行者）单列一条。
- **发现计数**：**P0=1 ｜ P1=8 ｜ P2=2**（共 11 条）

## 发现表

### AIO-1
| 字段 | 内容 |
|---|---|
| 定位 | CMakeLists.txt:146（astrocs_io SHARED=fits_core.c）· CMakeLists.txt:316-361（astrocs_aio/astrocs_hips STATIC）· lib/phase3_session/p3_output.cpp::(fits_* 直调 32 处) · lib/healpix_db/healpix_drizzle/fits_reader.cpp · lib/core/src/module_adapters.cpp::(SIMPLE/BITPIX 手解) · runtime/io/hips_core.c(956 行)+hips_output_store.py(688 行) |
| 违反条款 | ASTROCS_DESIGN §9:420「aio 是唯一 FITS/HiPS/manifest 读写边界；禁止各自复制 reader/writer」；§7.2「AIO 唯一 I/O」；docs/plugins/infrastructure/17_aio.md §1「唯一 I/O 边界」 |
| 当前证据 | 命令：`grep -c "fits_open_file\|fits_create_file\|fits_read_pix\|fits_write_pix\|fits_write_key" lib/phase3_session/p3_output.cpp`；输出：`32`。命令：`grep -rn "add_library(astrocs_io SHARED" CMakeLists.txt`；输出：`146:add_library(astrocs_io SHARED runtime/io/fits_core.c)` |
| 严重度 | P1 |
| 影响 | 读校验/原子性/缓存纪律无法在一个边界统一执行，产品面完整性按模块各自为政。 |
| 整改建议 | 以 aio_abi_v1（lib/astro_image_io/src/aio_abi.cpp，已在生产 target）为唯一出口：p3_output.cpp 与 module_adapters 的裸 fits 调用改为 AIO API 调用；healpix_db/hips_core.c 标注消费边界并入注册表。 |
| 建议文件域 | lib/astro_image_io、lib/phase3_session、lib/core/src、CMakeLists.txt |
| 验收门 | `grep -rc "fits_open_file\|fits_create_file" lib/phase3_session lib/healpix_db --include="*.cpp" | grep -v ":0" | wc -l` → 期望 0 |
| GAP/任务 | 与 GAP-013 同源不删条；归属 AIO-001 |
| 旧清单同源 | M2a-E-4（P1，归属失实）、GAP-003（目录形态） |

### AIO-2
| 字段 | 内容 |
|---|---|
| 定位 | lib/astro_image_io/src/hips/aio_hips_writer.cpp:231-234、:1468-1469 · lib/core/src/module_adapters.cpp:4600-4603（失实注释）· lib/hips/src/aio_publish.cpp:1（"唯一实现"自称） |
| 违反条款 | ASTROCS_DESIGN §9:421「所有产品：临时文件/目录+校验+fsync+原子 rename 提交；失败/取消不得留下可被误认为正式产品的半成品」；§6.3:339 取消不得留完整假象；17_aio.md §4 写路径条款 |
| 当前证据 | 命令：`grep -c "rename\|fsync\|staging" lib/astro_image_io/src/hips/aio_hips_writer.cpp`；输出：`0`。源码 :231-232 逐字：`// CFITSIO fits_create_file 拒绝覆盖已存在文件; 显式先删 (HiPS 输出允许 overwrite)` + `std::remove(path_n.c_str());` |
| 严重度 | P0 |
| 影响 | 覆写/中断窗口产出可被 Phase2 直接消费的残损 tile（且读侧不复核，见 AIO-9），科学产品完整性无闸门。 |
| 整改建议 | write_fits_image/finalize 改走既有 aio_publish_stage_* 原语（staging→fsync→rename promote），删除 remove-then-create；manifest.json 最后原子 rename 作完成标记。 |
| 建议文件域 | lib/astro_image_io/src/hips |
| 验收门 | `grep -c "std::remove(path_n" lib/astro_image_io/src/hips/aio_hips_writer.cpp` → 期望 0；`grep -c "aio_publish_stage" lib/astro_image_io/src/hips/aio_hips_writer.cpp` → 期望 ≥1 |
| GAP/任务 | 与 GAP-012/GAP-013 相关；归属 AIO-001、P2-001 |
| 旧清单同源 | M2b-C-01（P0/OPEN【部分修复】"HiPS 生产写出仍为直写"）同源不删条 |

### AIO-3
| 字段 | 内容 |
|---|---|
| 定位 | lib/core/src/module_adapters.cpp:3348-3353（p2_write_text）、:3368-3375（p2_write_bin）、:5427-5431（p3_props.json）、:5477-5481（p3_wcs.json）· cli/resource_recorder.h:257,281,316 |
| 违反条款 | ASTROCS_DESIGN §9:421（原子 rename 提交覆盖"所有产品"）；17_aio.md §4「先数据后 manifest、flush/close/fsync、原子 rename」 |
| 当前证据 | 命令：`grep -c "std::ofstream f(std::filesystem::u8path(path)" lib/core/src/module_adapters.cpp`；输出：`4`。:3349 逐字：`std::ofstream f(std::filesystem::u8path(path), std::ios::binary);`（直写最终路径，无 tmp） |
| 严重度 | P1 |
| 影响 | 节点间以 output_dir 文件约定交换（:5448 直读 p3_props.json），中断留下半写 JSON 即被下游当真实输入消费。 |
| 整改建议 | p2_write_text/p2_write_bin/p3 props/wcs 全部改走同文件已有 p1_atomic_publish（staging+rename）；resource_recorder 三文件同改。 |
| 建议文件域 | lib/core/src、cli |
| 验收门 | `grep -c "ofstream f(std::filesystem::u8path(path)" lib/core/src/module_adapters.cpp` → 期望 0 |
| GAP/任务 | 无对应 GAP（新立）；归属 AIO-001、P2-001、P3-002 |
| 旧清单同源 | W3-R2-009 家族（原子性纪律不一致）同源 |

### AIO-4
| 字段 | 内容 |
|---|---|
| 定位 | cli/commands.cpp:410-475（单文件承担 §9 七件职责）、:435-436（started/finished 同刻）· 全仓无 run-plan.json/artifact-manifest.json/run-trace.jsonl/run-summary.json 生产者 |
| 违反条款 | ASTROCS_DESIGN §9:422「每次运行至少生成：run-plan.json、run-graph.json、run-trace.jsonl、resource-timeseries.csv、resource-summary.json、artifact-manifest.json、run-summary.json；plan 是预期，trace 是实际观测，禁止把计划值伪装成实际值」 |
| 当前证据 | 命令：`grep -rn "run-plan\.json" lib cli --include="*.cpp" --include="*.h" | wc -l`；输出：`0`。:435-436 逐字：`{"started_utc", astrocs::iso8601_utc_now()}, {"finished_utc", astrocs::iso8601_utc_now()}`（同一构造点两次取 now） |
| 严重度 | P1 |
| 影响 | 运行记录与 §9 名录不对账，事后无法区分预期/实际，资源时间序列命名漂移（resource_samples.csv≠resource-timeseries.csv）。 |
| 整改建议 | CLI 收尾按名录补 run-plan/run-trace/artifact-manifest/run-summary（可先落空壳→逐步实装，但文件名与语义冻结）；started_utc 从 JsonlEmitter run 起点取。 |
| 建议文件域 | cli、runtime/artifact_store |
| 验收门 | `grep -rl "run-plan.json" cli lib --include="*.cpp" | wc -l` → 期望 ≥1 |
| GAP/任务 | 无 GAP（GAP-016/OBS-001 域外未覆盖此面）；归属 OBS-001、CLI-003 |
| 旧清单同源 | M5b-C-06（同秒时间戳残余形态）部分同源不删条 |

### AIO-5
| 字段 | 内容 |
|---|---|
| 定位 | cli/commands.cpp:417(kind=astrocs_run_manifest) · lib/core/src/module_adapters.cpp(节点 man) · aio_hips_writer.cpp:1483-1506(手写 fprintf 拼 JSON) · lib/astro_image_io/v6/src/v6_hips_manifest.cpp(第五方言，add_subdirectory 在编译) · runtime/io/hips_output_store.py:48 · runtime/artifact_store/production_store.py:6-21(DATA-001 形态) |
| 违反条款 | ASTROCS_DESIGN §9:423（manifest 至少记录字段，同一口径）；§7.1「contracts/ 合同 schema 唯一事实源」；17_aio.md §3（参考 contracts/schemas） |
| 当前证据 | 命令：`grep -rn "artifact_manifest\|storage_uri" lib cli --include="*.cpp" --include="*.h" --include="*.c" | wc -l`；输出：`0`（DATA-001 manifest 合同在 C++ 生产面零消费者，仅 python 侧 validator/store） |
| 严重度 | P1 |
| 影响 | 同一"manifest"词承载 5+ 方言，跨阶段哈希交换（§1.2:62 磁盘产品+manifest+哈希）无可机检统一 schema。 |
| 整改建议 | 以 contracts/data/artifact_manifest.schema.json 为准把 C++ 出口收敛到 DATA-001 形态（先加导出适配层），弃手写 fprintf JSON；run_manifest 保留但声明为七件套之一。 |
| 建议文件域 | lib/astro_image_io、cli、contracts/data |
| 验收门 | `grep -rl "artifact_manifest" lib --include="*.cpp" | wc -l` → 期望 ≥1 |
| GAP/任务 | 与 GAP-007 同源不删条；归属 DATA-001、AIO-001 |
| 旧清单同源 | M2a-C-14（schema 与校验器口径分叉）同源 |

### AIO-6
| 字段 | 内容 |
|---|---|
| 定位 | config/（不存在）· lib/photometric_calib/data/response_curves/filters.json（滤镜库实体错位，tracked）· lib/orchestrator/cpp/src/orchestrator.cpp:1340-1357(map_filter_name 硬编码"Baader *")、:953-955(exposure_tol=0.5/temp_tol=1.0)、:1069-1078(暗场-亮场容差判定用该硬编码) · lib/orchestrator/cpp/include/json_config.h:34-71(默认参数内嵌 Stage1Config：psf.tolerance=1e-6、pixfrac=0.8、max_stars=2000…) |
| 违反条款 | ASTROCS_DESIGN §3.3:135-144「容差与默认参数放程序根目录 config/（filters.json+defaults.json），从 defaults.json 读取；滤镜型号必须与 config/filters.json 匹配，未知滤镜→error」；§2:83 三类配置分离 |
| 当前证据 | 命令：`ls -d config`；输出：`ls: 无法访问 'config': 没有那个文件目录`。命令：`git ls-files | grep -iE "filters?\.json$"`；输出：`lib/photometric_calib/data/response_curves/filters.json`。:953 逐字：`double exposure_tol = 0.5;` |
| 严重度 | P1 |
| 影响 | 默认值/容差改一处需重编译，预检三级判定（§3.5）无权威基准落点；另有 exposure_tol=0.5s 与负责人已裁决 5s（GAP_AUDIT §6 R-003）相差一个量级的待裁决偏差。 |
| 整改建议 | 建 config/{filters.json,defaults.json}（CFG-001 既定范围），把 Stage1Config 内嵌默认与 exposure/temp 容差迁入并改读取；map_filter_name 的硬编码映射并入 filters.json 别名表。 |
| 建议文件域 | config/（新）、lib/orchestrator、lib/photometric_calib |
| 验收门 | `test -f config/defaults.json && test -f config/filters.json; echo rc=$?` → 期望 rc=0 |
| GAP/任务 | 与 GAP-006 同源不删条；归属 CFG-001（容差数值待裁决归 SCI-RES-01 R-003/R-004） |
| 旧清单同源 | M1a-A-005（容差族口径断裂）相关 |

### AIO-7
| 字段 | 内容 |
|---|---|
| 定位 | lib/orchestrator/cpp/include/orchestrator.h:113-135(AstroCsExitCode 词表) · lib/orchestrator/cpp/src/main.cpp:383-391(return exit_code 直接作进程退出码) |
| 违反条款 | ASTROCS_DESIGN §6.3:323-337 退出码冻结表（4=科学验证失败、7=I/O 失败、8=输出完整性失败、9=取消或超时、10=资源门禁失败）——第二词表 1/2/4/5/6/7/8/10 全部语义错位（7=CONFIG_ERROR、8=FILE_IO、10=CANCELLED 冲突；1=GENERIC 表外） |
| 当前证据 | 命令：`grep -n "CONFIG_ERROR      = 7\|FILE_IO_ERROR = 8\|CANCELLED         = 10" lib/orchestrator/cpp/include/orchestrator.h`；输出：`120:    constexpr int CONFIG_ERROR      = 7;` / `121:    constexpr int FILE_IO_ERROR     = 8;` / `122:    constexpr int CANCELLED         = 10;`。注释自称一致的 engineering/contracts/error_code_registry.csv 实测不存在（engineering/ 空目录，git ls-files=0） |
| 严重度 | P1 |
| 影响 | 同一进程退出码在两个入口映射两种语义，机器消费方（CI/CLI 合同）拿到假信号。 |
| 整改建议 | orchestrator main 收尾把内部码映射到 cli/exit_codes.h 的 11 码表（内部细分只留 JSONL error.numeric_code）；删除对不存在 registry 文件的引用注释。 |
| 建议文件域 | lib/orchestrator/cpp |
| 验收门 | `grep -c "return exit_code;" lib/orchestrator/cpp/src/main.cpp` → 期望 0（改经映射函数） |
| GAP/任务 | 无 GAP（GAP-005 记命令树、未记码表冲突）；归属 CLI-003 |
| 旧清单同源 | — |

### AIO-8
| 字段 | 内容 |
|---|---|
| 定位 | cli/commands.cpp::value("output_dir", std::string(".")) ×11 · lib/core/src/module_adapters.cpp 同型 ×19（如 :2854、:4611） |
| 违反条款 | ASTROCS_DESIGN §6.3:340「运行产物只落配置 output_dir，不得以进程 CWD 作隐式缺省写出」 |
| 当前证据 | 命令：`grep -c 'value("output_dir", std::string("."))' cli/commands.cpp; grep -c 'value("output_dir", std::string("."))' lib/core/src/module_adapters.cpp`；输出：`11` / `19` |
| 严重度 | P1 |
| 影响 | 缺 output_dir 的配置仍会把产品/manifest 写进 CWD（根目录曾累积 62 个 astrocs_run_*.json 的机制未拆）。 |
| 整改建议 | 该 30 处缺省改为 fail-closed（缺失→exit 2 配置错误），预检页列为 error 项。 |
| 建议文件域 | cli、lib/core/src |
| 验收门 | `grep -c 'std::string(".")' cli/commands.cpp lib/core/src/module_adapters.cpp | awk -F: '{s+=$2} END {print s}'` → 期望 0（在 output_dir 取值上下文内） |
| GAP/任务 | 与 GAP-021/GAP-022 同源不删条（本条是根因机制面）；归属 CLI-003、ROOT-002 口径 |
| 旧清单同源 | GAP-022 根产物散落 |

### AIO-9
| 字段 | 内容 |
|---|---|
| 定位 | lib/astro_image_io/src/hips/aio_hips_reader.cpp:408-450(read_tile_t 仅查存在/尺寸/BITPIX)、:484-518(aio_hips_read_tile_datasum 仅取头键值) |
| 违反条款 | 17_aio.md §4「读：格式校验、哈希校验」·§7「哈希/格式不匹配 → 拒绝读取」；DESIGN §9:420 唯一边界读纪律 |
| 当前证据 | 命令：`grep -rn "aio_hips_read_tile_datasum" lib cli tests runtime tools 2>/dev/null | grep -v "aio_hips_reader" | wc -l`；输出：`0`（DATASUM 校验 API 全仓零消费者） |
| 严重度 | P1 |
| 影响 | 位级损坏/覆写残损 tile 在读侧不被拒（cli verify --run-manifest 的事后哈希链只能兜住被登记文件）。 |
| 整改建议 | read_tile_t 读毕重算 DATASUM 与头值比对，不符→-7 拒读（映射 exit 3）；补正/负例单测。 |
| 建议文件域 | lib/astro_image_io/src/hips |
| 验收门 | `grep -c "datasum" lib/astro_image_io/src/hips/aio_hips_reader.cpp` → 期望 ≥3（读路径出现比对逻辑） |
| GAP/任务 | 无 GAP；归属 AIO-001 |
| 旧清单同源 | — |

### AIO-10
| 字段 | 内容 |
|---|---|
| 定位 | ASTROCS_DESIGN §6.3:323 声明唯一源 include/astrocs/exit_codes.h（该文件不存在）· 实际唯一枚举在 cli/exit_codes.h:5-21 |
| 违反条款 | ASTROCS_DESIGN §6.3「退出码（唯一源 include/astrocs/exit_codes.h）」 |
| 当前证据 | 命令：`test -f include/astrocs/exit_codes.h; echo rc=$?`；输出：`rc=1`。cli/exit_codes.h:3 自称「本文件是 11 个退出码在仓库内的唯一定义处」；11 值实测与 §6.3 表一致 |
| 严重度 | P2 |
| 影响 | 数值未漂移但权威路径失配，lib 层跨 DLL 复用无公共头可 include，只能各自重抄（AIO-7 的诱因之一）。 |
| 整改建议 | 把枚举迁到 include/astrocs/exit_codes.h，cli/exit_codes.h 改转发 include（或 DESIGN 修订由负责人批准——agent 无权改文档绕过）。 |
| 建议文件域 | include/astrocs、cli |
| 验收门 | `test -f include/astrocs/exit_codes.h && grep -c "include" cli/exit_codes.h | head -1` → 期望文件存在 |
| GAP/任务 | 无 GAP；归属 CLI-003 |
| 旧清单同源 | — |

### AIO-11
| 字段 | 内容 |
|---|---|
| 定位 | ≥10 处独立原子发布实现：lib/io/src/io_adapter.cpp(astrocs::io) · module_adapters.cpp:1183(p1_atomic_publish) 与 :6216(write_run_context) · lib/hips/src/aio_publish.cpp · aio_hips_writer→无 · hiss_stream_writer.cpp:174-197 · lib/backend_host/profile_store.cpp:110 · cli/commands.cpp:446 · runtime/io/hips_output_store.py:251 · runtime/artifact_store/production_store.py:229 · lib/orchestrator/cpp/src/checkpoint.cpp |
| 违反条款 | 17_aio.md §6「唯一允许触碰磁盘产品的模块」；DESIGN §9:420；§1.3 非目标「为未发生的假设故障堆叠 fallback/兼容层」（重复实现即债） |
| 当前证据 | 命令：`grep -rlnE "atomic_rename\|p1_atomic_publish\|atomic_publish\|p1_atomic" lib cli runtime --include="*.cpp" --include="*.h" --include="*.py" | grep -v test | wc -l`；输出：≥10。aio_publish.cpp:1 逐字：`aio_publish.cpp - HiPS 原子发布原语 v1 唯一实现 (AIO-002)`；其唯一消费者是 lib/hips/src/module_entry.cpp:862，生产 HiPS 路径（AIO-2）不在其内 |
| 严重度 | P2 |
| 影响 | "唯一实现"自称与树内事实矛盾；Windows 兜底「先删后 rename」（module_adapters.cpp:1189-1196）与 hiss_stream_writer.cpp:14「不先删除旧文件再 rename（避免竞态窗口）」同仓互斥纪律。 |
| 整改建议 | 原子发布收敛到 aio_publish_stage_* 单原语（含 Windows MoveFileEx 覆盖语义），其余实现标记 deprecated 并逐步替换；两文档一题处先统一口径。 |
| 建议文件域 | lib/hips、lib/io、lib/core/src、lib/astro_image_io |
| 验收门 | `grep -rlE "atomic_rename\|atomic_publish(" lib cli runtime --include="*.cpp" --include="*.py" | grep -v test | wc -l` → 期望收敛 ≤3 |
| GAP/任务 | 与 GAP-013 相关；归属 AIO-001 |
| 旧清单同源 | W3-R2-001（aio_upm 先删后 rename）、W3-R2-009（p1_atomic_publish 兜底与注释矛盾）同源不删条 |

## 核查过但未立条（如实记录）

- **三命令隐式串接**：CLI phase1/2/3 run 各自单次派发，run manifest phases 均为单元素 `{1}`/`{2}`/`{3}`（cli/commands.cpp:1855/1264/1497）；orchestrator main 仅执行 stage1（main.cpp:345），run_stage2 无生产调用方（orchestrator.cpp:5465 注释「legacy Stage2 removed in V17」）。未发现"一次运行三阶段"入口 → **未复现**，不立条。orchestrator/astrocs-stage2 属第二入口问题，归 GAP-005/CLI-001 域。
- **run_manifest 冻结配置哈希**：config_path+config_sha256 在写（commands.cpp:428-429），resume 时哈希不符→incomplete+拒跑（commands.cpp:1934、:1367）→ **合规**（正面项）。
- **libjpeg**：全仓生产码零命中 → 无此违例。
- **fopen 卫生**：lib/star_detector(3)、photometric_calib(2) 等 fopen 为读入数据/调试 dump，未见直接写共享产品路径，不单列。
- **14 对象 schema**：contracts/schemas/unified 14 对象+port_contract 齐备（实测 15 文件）；frame_snr.schema.json 明文"与 depth_m5 不得互填"——而 module_adapters.cpp:2826-2832 在 p1_snr.json 用 `frame_snr` 字段承载 5σ 深度，属 GAP-009 域（同源不删条，归 P1-002），本轴不重复立条。

## 复跑口径

所有证据命令见上表；完整命令与输出流水见 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/AIO.log`。开工 HEAD=`900916fb`（三口径一致）；收工 HEAD=main=`180c8a0a`（origin/main=`b4afc135`，并行线推进所致）；轴内证据文件区间零 diff、锚点全部复跑一致，受影响条目=无。
