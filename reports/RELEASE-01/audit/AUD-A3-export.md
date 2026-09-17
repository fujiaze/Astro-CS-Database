# AUD-001-A3-export

分片：export（Phase3）3 个模块（projection / resample / fits_output）+ CLI 契约横向面的设计-实现差异审计。

只读声明：未修改任何仓库文件；唯一写入本报告。报告外仅执行只读命令（`ls`/`grep`/`sed`/`wc`）、单测目标 `ctest -R`、`./build/astrocs --help/--template`、以及控制包允许的 `python3 ci/run_checks.py --check <ID>` 与 `python3 tools/quality/check_module_map.py`（按 ci/checks.json 注册定义写 `run/ci/**`，run/ 为 gitignore 目录）。未执行任何 git 写操作。

审计基线：以当前工作区文件为准（禁 git 写，未记录 HEAD）。对照面：`ASTROCS_DESIGN.md`（最高权威）、`ENGINEERING_SPEC.md`、`docs/plugins/algorithms_phase3/14..16`、`docs/plugins/infrastructure/18_cli.md`、`docs/science/PHASE3_HIPS_TO_FITS.md`、`docs/science/UNCERTAINTY_AND_COVARIANCE.md`、`docs/algorithms/PHASE3_{PROJ_IMPL,RESAMPLE,RSMP_IMPL,FITS_IMPL}.md`、`docs/algorithms/v6/phase3/**`、`docs/design/PHASE3_DETAILED_DESIGN.md`、`docs/contracts/**`、`contracts/**`、`docs/modules/MODULE_MAP.yaml`。

实现落位事实（先 `ls`/`wc -l` 确认）：
- projection：`lib/algorithms/projection/` 含 p3_wcs.{h,cpp}（66/232 行）、p3_proj_v6.{h,cpp}（219/732 行，registry v3）、p3_projection.{h,cpp}（v1，RETIRED）、tests/p3wcs/。CMake target `astrocs_p3_projection_wcs` 仅编 p3_wcs.cpp；**p3_proj_v6.cpp 无生产 target**（`lib/algorithms/projection/CMakeLists.txt:13`）。
- resample：`lib/algorithms/resample/` 含 p3_resample.{h,cpp}（150/519 行，叶级重采样）+ p3_rsmp.{h}+`p3_rsmp_*.cpp` 6 个（V6 三模式传播）。CMake target `astrocs_p3_rsmp`（`CMakeLists.txt:20-28`）。
- fits_output：`lib/algorithms/fits_output/` 含 p3_output.{h,cpp}（99/560 行）。CMake target `astrocs_p3_fits_output`。
- CLI：`lib/infrastructure/cli/`（session_commands.h / subcommand.h / parser.cpp / commands.cpp）；会话实现 `lib/phase3_session/p3_session.cpp`（441 行）与 V6 接线层 `p3_v6_export.cpp`（801 行，仅测试目标编译）。

## 1. 覆盖清单

| 模块 | 审计结论（含"无差距"也要写） | 差距号 |
|---|---|---|
| projection | 职责面基本在位：registry v6 冻结集合 = DESIGN §5.3 八投影（`p3_proj_v6.cpp:348-349`），已实现 TAN/SIN/CAR/AIT 四行（`:272-281`，`registry_table` 返回 count=4 `:365`），`registry_selfcheck()` 强制表内 code ∈ 冻结集（`:383-402`）；TAN 逐式路径 p3_wcs.cpp 生产在用。**差距**：CLI `export --help` 文案称"当前唯一实现 TAN"与 registry 4/8 事实冲突；registry v3 无生产构建 target/入口；`projection_registry.schema.json` 的导出字段（domain/reference_point/crval2_in_mapping/lonpole_default/implemented）在 C++ `Spec` 中不存在且无任何导出器；module.yaml/README 的构建归属与 descriptor 为占位陈旧。 | PROJ-01/03/04/05、X-01/02/05 |
| resample | 叶级重采样（nearest / 四象限最近中心 bilinear、跨 tile、coverage/NaN 语义、order 选择）实现完整且 p3_output/会话链闭合；V6 传播（`C_y=R C_x Rᵀ`、Q/W 重算、三模式门、kernel registry）实现完整且测试通过（`ctest -R v6_p3_rsmp_core` Passed）。**差距**：生产默认核 `bilinear` 对应 V6 registry 的非生产默认核 `bilinear_4quad`，而 V6 冻结的生产科学默认核是 `bilinear_area_overlap_exact`（未接生产）；V6 模块无任何生产消费方（仅测试直编）；生产方差传播仅对角 `Σc_k²u_k`，文档要求的 `correlation_output`/`C_y` 相关核不产出；ALG-P3-RSMP-IMPL 多处行数/缓存/并发描述陈旧；共址测试缺 CMake 挂载。 | RSMP-01..07、X-01/02/03 |
| fits_output | 原子写协议完整且与文档一致：tmp→fits_flush_file→close→fsync(fd)→rename（`p3_output.cpp:316-359`），失败/取消 unlink 不留半成品（`:239-243`、`:320-327`）；PRIMARY + COVERAGE +（有 uncertainty 时）VARIANCE/IVAR，逐 HDU CFITSIO 标准 DATASUM/CHECKSUM（`:245-308`）；WCS 关键字 CTYPE/CUNIT/CRPIX/CRVAL/CD + BSCALE/BZERO(TDOUBLE)+BUNIT（`:181-217`）；重开 verify 逐项回读 WCS/数据/checksum（`:392-541`）。**差距**：文档要求的 SUPPORT/REJECTION/POINT_INFORMATION/W/PSF 表/correlation 描述扩展 HDU 全缺；输出模式（surface_brightness/point_source_flux/visualization）在生产会话未实现；DATA_SEMANTICS §27 与 ALG-P3-FITS §12 有陈旧陈述；共址测试缺；module_id 冲突。 | FITS-01..07、X-01/02/03 |
| CLI 契约（横向） | 命令树与 DESIGN §6.2 一致（`./build/astrocs help`）；`--template`/`--help`/`-o`/`--json`/退出码 2/3/9 行为符合；`-y` 跳过确认、error 阻断（`-y` 不能越）、`-force` 越可强制项（`subcommand.h:187-226`）符合 §3.5。**差距**：预检三级缺"橘色 optimize"（从不产生）；预检不检测"文件找不到"（§3.5 要求红）；`--version` 携带版本信息且非 0.1alpha（§12）；正式 phase_config schema/模板与 CLI 实际接受格式互斥；help 未列 `-y`/`-force`。 | CLI-01..05、X-01 |

## 2. 差距明细

| # | 模块 | 差距类型 | 级别 | 文档条款引用 | 现状复现路径（文件:行 / 命令+输出） | 影响面 | 建议归属 |
|---|---|---|---|---|---|---|---|
| CLI-03 | CLI | 违规 | P0 | `ASTROCS_DESIGN.md:533`（§12：Alpha 前代码/产物不含任何版本信息；可发布时 `--version` 写 `0.1alpha`）；`ACCEPTANCE_SPEC.md:158,183` | `cd build && ./astrocs --version` → `astrocs 0.11.0-alpha.2+ge88a0d893ac18c59e77b4da0d73e96fe6151cb2e`；`cat VERSION` → `0.11.0-alpha.2`；`lib/algorithms/{projection,resample,fits_output}/module.yaml` module_version=0.11.0-alpha.2；`ci/checks.json` VERSION-CONSISTENCY `--expected 0.11.0-alpha.2`；`module_adapters.cpp:662,681,701` descriptor version="1.0.0" | 版本纪律违反最高设计 §12 与验收规范；发布产物携带非 `0.1alpha` 版本串。**与 CI 期望互斥，登记 UNRESOLVED（U1）** | 负责人裁决 + DOC-002/VERSION 任务 |
| CLI-04 | CLI | 违规/UNRESOLVED | P0 | `ENGINEERING_SPEC.md:43`（§4 第 7 项：端口引用有效 DATA 合同，contracts/schemas 唯一事实源）；`config/config_registry.json:29`（"phase_config 行以 contracts/schemas/phase_config_*.schema.json 为唯一事实源"）；`ASTROCS_DESIGN.md:326`（§6.3 模板可直接运行） | 正式 schema `contracts/schemas/phase_config_export.schema.json` 要求 `{phase_name, config:{output_dir,precision,output_mode,wcs:{projection,center_deg,s_out_deg,width_px,height_px}}, inputs:[{product}]}`，模板 `config/templates/export.phase_config.json` 同构；但 CLI 实际模板（`./build/astrocs export --template`）为 `{schema_version, source:{hips_dir}, output_dir, center:{ra_deg,dec_deg}, width_px, height_px, scale_deg_per_px}`。`./build/astrocs export --json config/templates/export.phase_config.json -y` → `[error] output_dir ...（收到 缺失）` `[error] source 缺失` rc=2。parser 白名单 `parser.cpp:271-305` 不含 phase_name/config，且 flat 会话键 `:299-302` 为 source/center/scale_deg_per_px 等 | 官方 schema+模板不可运行；CFG-001 门只自证 schema↔模板一致，未验证 CLI 可达；违反"唯一事实源"。**谁权威登记 UNRESOLVED（U2）** | DOC-002 + CFG/SCHEMA 任务 + 负责人裁决 |
| X-01 | 横向（3 模块） | 违规 | P0 | `ENGINEERING_SPEC.md:33-43`（§4 七件套）、`:127`（检查器覆盖：模块 manifest/注册表/构建 target/产品清单一致） | `python3 ci/run_checks.py --check CHK-MODULE-MANIFEST` → `verdict=FAIL entries=1 steps=8 pass=7 fail=1`；`python3 tools/quality/check_module_map.py` 对三模块全 FAIL：projection(`header_missing_abi_version`/`missing_cmake_target`/`missing_implementation`/`module_yaml_value_mismatch`/`product_unit_missing`)、resample(+dangling_schema_link/missing_co_located_tests)、fits_output(+dangling_schema_link/missing_co_located_tests)。详见 `run/ci/module-map/module_map.json` modules[projection/resample/fits_output].findings | P0 机器门红（waivable=false，fast/linux-main/windows-main）；三模块不可独立加载/不可调度；发布前必须处置 | BLD-001 + P3-PROJ-IMPL / P3-RSMP-IMPL / P3-FITS-IMPL |
| PROJ-01 | projection | 违规 | P1 | `ASTROCS_DESIGN.md:269`（§5.3 内置多种投影，首批冻结八种）；`docs/plugins/algorithms_phase3/14_projection.md:25`（"已实现 TAN/SIN/CAR/AIT（4/8）"）；`docs/algorithms/PHASE3_PROJ_IMPL.md:405-408`（v3 已实现 4/8）；`docs/science/PHASE3_HIPS_TO_FITS.md:132-136` | `cd build && ./astrocs export --help` → `projection (optional) — 可选投影（当前唯一实现 TAN；缺省 TAN）`（源：`lib/infrastructure/cli/session_commands.h:150`）；而 registry v3 实有 4 行：`p3_proj_v6.cpp:272-281` kRegistry[4]，冻结集 8 行 `:348-349`，count=4 `:365`。**判定：plugin 14 §4 正确（registry 层 4/8 已实现），CLI help 文案错误**——应表述为"alpha 会话仅接受 TAN"（`p3_session.cpp:96-97`），而非"唯一实现" | 用户可见 CLI 合同与实现事实冲突；误导用户以为仅存在 TAN 实现 | CLI/DOC 修复（文案） |
| RSMP-01 | resample | 违规 | P1 | `docs/algorithms/v6/phase3/ALG-P3-001_KERNEL_REGISTRY.md:24-25`（`bilinear_4quad` production default=false；`bilinear_area_overlap_exact` production default=true）；`docs/plugins/algorithms_phase3/15_resample.md:29,35` | 生产默认 `sampler="bilinear"`（`p3_session.cpp:116`），分派到 `p3_sample_bilinear_ex`（`:291`）= 四象限最近中心 = V6 `bilinear_4quad`；`p3_rsmp_kernel_registry.cpp:198` `bilinear_4quad.production_science_default=false`、`:226` `bilinear_area_overlap_exact.production_science_default=true`，且后者无生产消费方 | 生产科学采样核与 V6 冻结的生产默认核不一致；"精确面积重叠"核缺失 | P3-RSMP-INT + SCI/ALG 裁决 |
| RSMP-02 | resample | 无主/缺口 | P1 | `docs/algorithms/v6/phase3/ALG-P3-001_SPEC.md`（三模式 y=Rx/f=Sd/pi=Sp、C_y、Q/W 重算）；`docs/design/PHASE3_DETAILED_DESIGN.md:39-47` | `grep -rn "p3_rsmp" lib/ --include=*.cpp --include=*.h` 仅 `p3_v6_export.h:54` 与 CMake；`p3_v6_export.cpp` 仅被 `tests/integration/v6_p3/CMakeLists.txt:35-36` 编译。生产 export 链（p3_session.cpp / module_adapters p3_op_resample:5973）只调 p3_resample | V6 covariance/QW/modes/PSF 传播在生产不可达；两套重采样实现并存 | P3-RSMP-INT + 负责人裁决（是否废弃 legacy） |
| RSMP-03 | resample | 缺口/漂移 | P1 | `docs/plugins/algorithms_phase3/15_resample.md:27-29,36`（`C_y=RC_xRᵀ`；仅输出对角 variance 必须给相关核；`correlation_output` 默认 true）；`docs/science/UNCERTAINTY_AND_COVARIANCE.md:82-99`；`docs/plugins/algorithms_phase3/16_fits_output.md:21` | 生产 `p3_uncertainty_propagate`（`p3_resample.cpp:489` 起）实现对角 `var_out=Σc_k²u_k`，无相关核；`grep -rn "correlation_output" lib/ docs/` 仅命中 `15_resample.md:36`（实现/配置零命中）；FITS 输出无 correlation 描述 HDU。完整 `C_y` 在 `p3_rsmp_covariance.cpp:22-48`，但未接生产 | 输出 variance 系统性低估（文档 MC 估计 ρ≈0.19 时方差低估约 36%）；相关核不可得 | P3-RSMP-INT + SCI 裁决 |
| RSMP-04 | resample | 过时 | P1 | `docs/algorithms/PHASE3_RSMP_IMPL.md:3-14`（p3_resample.cpp 239 行）、`:24-28`（不实现 variance 输入）、`:215-230`（每 sampler 独立 FIFO cache）、`:271-279`（DISP-P3RSMP-002） | 实测 `wc -l lib/algorithms/resample/p3_resample.cpp` = 519；`p3_resample.h:43-56` 明示 P30 共享有界缓存 + `p3_sampler_attach_cache`；`module.yaml` known_defects 记 DISP-P3RSMP-002 已被 P30 取代；variance/ivar 已由 `p3_uncertainty_open` 消费（`p3_resample.h:93-146`） | 实现级合同文档与生产源/模块清单三处不一致 | DOC-002 |
| RSMP-05 | resample | 违规 | P1 | `ENGINEERING_SPEC.md:42`（§4 第 6 项：共址可复用测试） | `python3 tools/quality/check_module_map.py` → resample `missing_co_located_tests: lib/algorithms/resample/tests`；`ls lib/algorithms/resample/tests` 仅 `p3rsmp/p3_resample_probe_main.cpp`，无 `tests/CMakeLists.txt`、未被 add_subdirectory 注册 | 模块共址测试面缺失/未挂载 | P3-RSMP-TEST |
| FITS-01 | fits_output | 缺口 | P1 | `docs/plugins/algorithms_phase3/16_fits_output.md:20`（扩展 HDU：VARIANCE/IVAR、COVERAGE、VALIDITY、SUPPORT、REJECTION、POINT_INFORMATION/W、PSF 表/图、correlation 描述）；`docs/design/PHASE3_DETAILED_DESIGN.md:52-53` | `p3_output.cpp` 仅写 PRIMARY signal + COVERAGE（`:256-276`）+ 有 uncertainty 时 VARIANCE/IVAR（`:281-308`）；`grep -rn "SUPPORT\|REJECTION\|POINT_INFORMATION" lib/algorithms/fits_output/` 零命中 | 声明的科学层缺 HDU；16 §3"所有 HDU shape/WCS 对齐"不完整 | P3-FITS-IMPL |
| FITS-02 | fits_output | 缺口 | P1 | `ASTROCS_DESIGN.md:272`（§5.3 输出模式显式 surface_brightness/point_source_flux/visualization；缺所需信息→拒绝或 unavailable）；`16_fits_output.md:41`（`mode` 配置项） | `p3_session.cpp` parse_request（`:73-132`）不读取 `mode`/`output_mode`；`parser.cpp:304` 把 `mode` 列入会话键但无人消费；`grep -rn "output_mode" lib/` 零命中。三模式仅存在于未接生产的 `p3_v6_export.cpp`/`p3_rsmp_propagation.cpp` | 用户传 `mode` 被静默忽略，export 恒为 surface_brightness；违反"输出模式显式/缺信息拒绝" | P3-INTEGRATE-001 / CLI |
| FITS-03 | fits_output | 过时 | P1 | `docs/contracts/DATA_SEMANTICS.md:2121`（"VARIANCE/IVAR ... 实现归 P3-001，现状无此 HDU"）、`:2133`（"DATASUM u32 经 TINT 写入"）、`:2136`（"sha256/fdatasum 纯函数"）；`docs/algorithms/PHASE3_FITS_IMPL.md:306-312`（T5/T6"现状未覆盖"） | VARIANCE/IVAR 已实现：`p3_output.h:79-95`、`p3_output.cpp:281-308`、`p3_session.cpp:392-397`；DATASUM/CHECKSUM 已改 CFITSIO `fits_write_chksum`（`p3_output.cpp:62-70,245-254`）；取消/哈希注入测试已存在 `tests/backend/test_p3_output.py:187-191,300`（335 行） | 合同/实现级文档与生产源不一致，误导后续整改 | DOC-002 |
| FITS-04 | fits_output | 违规 | P1 | `ENGINEERING_SPEC.md:42`（§4 第 6 项） | `check_module_map.py` → fits_output `missing_co_located_tests: lib/algorithms/fits_output/tests`；`ls lib/algorithms/fits_output` 无 tests 目录（测试在 `tests/unit/p3_output_test.cpp`、`tests/backend/test_p3_output.py`） | 共址测试面缺失 | P3-FITS-TEST |
| FITS-06 | fits_output | UNRESOLVED | P1 | `ENGINEERING_SPEC.md:38`（module.yaml ID 一致）；`docs/modules/MODULE_MAP.yaml:500-501`（期望 module_id=astrocs.p3.fits_output，aliases 含 fits_writer） | `check_module_map.py` → `module.yaml module_id='astrocs.p3.fits_writer' 与映射表期望 'astrocs.p3.fits_output' 不一致`；module.yaml/README/PHASE3_FITS_IMPL 均主张 fits_writer 为矩阵权威值 | 模块 ID 双口径；产品清单匹配失败 | 负责人裁决（U3）+ DOC-002 |
| CLI-01 | CLI | 缺口 | P1 | `ASTROCS_DESIGN.md:186-189`（§3.5 三级：绿 correct/橘 optimize/红 error）；`docs/plugins/infrastructure/18_cli.md:22,55` | `subcommand.h:126-146 precheck_config` 只产生 `correct`/`error`；`calibration_checks`（`:108-120`）也只 correct/error；`grep -rn "optimize" lib/infrastructure/cli/` 仅注释与 render 分支（`:188,202`），无产生点；`grep -rn "\[optimize\]" tests/` 零命中 | 橘色"可优化"提示（暗场-亮场超容差→暗场优化）永不出现；§3.5 语义缺失且无测试 | CLI 修复 + TST |
| CLI-02 | CLI | 缺口 | P1 | `ASTROCS_DESIGN.md:189`（§3.5 红色 error：滤镜不匹配/文件找不到）；`18_cli.md:22` | `config_structure_errors`（`subcommand.h:64-104`）只判键存在/类型/非空，无任何 `filesystem::exists`；`validate_config_full` 仅在非 flat（V1）分支检查输入存在（`parser.cpp:344-349`），flat 会话分支不检查。故 export 的 `source.hips_dir` 不存在时预检通过，运行期才失败 | "文件找不到"不红、不阻断，退出码与阶段不符；用户无法在预检页看到 | CLI 修复 |
| X-02 | 横向（3 模块） | 违规 | P1 | `ENGINEERING_SPEC.md:39`（§4 第 3 项：公开头版本化 C ABI，带 struct_size/abi_version） | `check_module_map.py` 对三模块均报 `header_missing_abi_version`；`p3_wcs.h`、`p3_resample.h`、`p3_output.h`、`p3_rsmp.h` 均无 abi_version/struct_size（`grep -n "abi_version\|struct_size" ` 零命中） | 跨 DLL C ABI 版本化合同缺失 | P3-*-IMPL / ABI-005 |
| X-03 | 横向（resample/fits_output） | 缺口 | P1 | `ENGINEERING_SPEC.md:43`（§4 第 7 项：端口引用有效 DATA 合同/schema 唯一事实源）；`15_resample.md:18`；`16_fits_output.md:25` | `ls contracts/schemas/export_product.schema.json contracts/schemas/fits_product.schema.json` → 均"没有那个文件或目录"；`check_module_map.py` 报 resample `dangling_schema_link: contracts/schemas/export_product.schema.json`、fits_output `.../fits_product.schema.json` | 插件文档引用的产品 schema 不存在；合同链断链 | DOC-002 + SCHEMA 任务 |
| PROJ-03 | projection | 无主/缺口 | P2 | `docs/plugins/algorithms_phase3/14_projection.md:18-19`（`projection_registry.schema.json`（registry v3，机器可校验导出 schema）） | schema 存在（`contracts/schemas/projection_registry.schema.json:1-70`）且要求每个 projection 含 `domain/reference_point/crval2_in_mapping/lonpole_default/implemented`（`:38-65`）；但 `p3_proj_v6.h:96-106 Spec` 无这些字段，`grep -rn "crval2_in_mapping\|reference_point\|lonpole_default" lib/ tests/ tools/` 零命中，无任何 JSON 导出器 | schema 无生产者；"机器可校验导出"不成立 | P3-PROJ-INT |
| PROJ-04 | projection | 过时 | P2 | `ENGINEERING_SPEC.md:19`（注释禁堆历史/复述）；模块元数据自洽 | `lib/algorithms/projection/module.yaml` 称"现状构建=astrocs_phase3_session 静态库成员"，实际已是独立 target `astrocs_p3_projection_wcs`（`lib/algorithms/projection/CMakeLists.txt:20-21`）；README 同类陈旧 | 模块清单/追溯面失真 | DOC-002 |
| PROJ-05 | projection | 漂移 | P2 | `docs/modules/MODULE_MAP.yaml:439`（target: astrocs_p3_projection） | `check_module_map.py` → `missing_cmake_target: ... 无 add_library/add_executable(astrocs_p3_projection)`；实际 `astrocs_p3_projection_wcs`（CMakeLists.txt:20）；另 `p3_proj_v6.cpp` 无生产 target | target 名与映射表/产品清单不一致 | BLD-001 |
| RSMP-06 | resample | 漂移 | P2 | `docs/modules/MODULE_MAP.yaml:468`（target: astrocs_p3_resample） | `check_module_map.py` → `missing_cmake_target: ... 无 add_library/add_executable(astrocs_p3_resample)`；实际 `astrocs_p3_rsmp`（`lib/algorithms/resample/CMakeLists.txt:20`） | target 名不一致 | BLD-001 |
| RSMP-07 | resample | 过时 | P2 | `ENGINEERING_SPEC.md:19` | `lib/algorithms/resample/README.md:6-11` 称"生产源实际位于 lib/phase3_session/ ... astrocs_phase3_session 静态库成员"，实际已迁 `lib/algorithms/resample/` 并归 `astrocs_p3_rsmp`（`CMakeLists.txt:10-28`）；`p3_resample.h:1`、`p3_resample.cpp:1` 文件头仍写 `lib/phase3_session/p3_resample.*`；module.yaml 同类陈述陈旧 | 路径/构建归属失真 | DOC-002 |
| FITS-05 | fits_output | 缺口 | P2 | `16_fits_output.md:51`（>2 GiB、长 UTF-8 路径、Windows CFITSIO 必须正确处理）、`:60`（>2 GiB 与长路径测试） | `grep -rln "2 GiB\|2GiB\|long path\|长路径\|2147483648" tests/` 零命中；`ls tests/**/p3_output*` 无对应用例 | 大文件/长路径/Windows 路径边界未验证 | P3-FITS-TEST |
| FITS-07 | fits_output | 缺口 | P2 | `16_fits_output.md:25` | 同 X-03（`contracts/schemas/fits_product.schema.json` 不存在） | schema 断链 | DOC-002 |
| CLI-05 | CLI | 缺口 | P2 | `18_cli.md:32-39`（`-y`、`-force` 登记在 CLI 字段表）；`ASTROCS_DESIGN.md:320` | `./build/astrocs export --help` 仅列配置字段，无 `-y`/`-force` 说明；`subcommand.h:250-255 print_help` 只输出 `config_field_help`；根 `help` 亦无 | 用户不可从 help 得知确认/强制语义 | CLI/DOC |
| X-05 | 横向（3 模块） | 过时/无主 | P2 | `ENGINEERING_SPEC.md:113`（Alpha 前代码/产物不含版本信息）；`ASTROCS_DESIGN.md:534` | `module_adapters.cpp:661-662,680-681,700-701` descriptor `module_id=astrocs.phase3.{wcs,resample2,writer}`、`version="1.0.0"`，与 module.yaml（`astrocs.p3.*`、0.11.0-alpha.2）不一致；docs 自称"占位词汇待 P3-*-INT 对齐"但未闭环 | 注册表/模块清单/版本三处不一致 | P3-*-INT + DOC-002 |

## 3. UNRESOLVED

1. **U1 版本纪律（对应 CLI-03）**：`ASTROCS_DESIGN.md:533` + `ACCEPTANCE_SPEC.md:158,183` 要求 Alpha 前无版本信息、首个可发布版本 `0.1alpha`；但 `VERSION`、CLI `--version`、三模块 `module.yaml`、`module_adapters` descriptor 与 CI 注册项 `VERSION-CONSISTENCY --expected 0.11.0-alpha.2` 全用 `0.11.0-alpha.2`。两套口径直接互斥，本审计不自行裁决是"设计/验收文档过时"还是"实现/CI 违规"。
2. **U2 export phase_config 格式（对应 CLI-04）**：`contracts/schemas/phase_config_export.schema.json` + `config/templates/export.phase_config.json`（`{phase_name,config.wcs,inputs}`）与 CLI 实际模板/会话格式（`source.hips_dir,center,scale_deg_per_px`，`docs/api/PHASE3_API_V1.md:20-29`）互斥，且 `config/config_registry.json` 声明 schema 为"唯一事实源"。CFG-001 测试只验证 schema↔模板，不验证 CLI 可达。谁权威待裁决。
3. **U3 fits_output 模块 ID（对应 FITS-06）**：`docs/modules/MODULE_MAP.yaml:500` 期望 `astrocs.p3.fits_output`（`fits_writer` 仅为 alias）；`lib/algorithms/fits_output/module.yaml`、README、`PHASE3_FITS_IMPL.md` 均主张 `astrocs.p3.fits_writer` 为 MODULE_MIGRATION_MATRIX 权威值。两处权威链内文档冲突。
4. **U4 采样核命名/语义（对应 RSMP-01）**：插件 `15_resample.md:35`（`sampler ∈ nearest|bilinear`）与 V6 `ALG-P3-001_KERNEL_REGISTRY.md:24-25`（`nearest|bilinear_4quad|bilinear_area_overlap_exact|bicubic|lanczos`，且生产默认=area_overlap_exact）两套核词表并存；生产 CLI 用前者，V6 冻结件用后者。哪层为生产权威未在活动文档中闭合。

## 4. 复核命令清单

```bash
# 只读：命令树 / 模板 / 帮助 / 版本 / 预检阻断
cd build
./astrocs help
./astrocs export --help
./astrocs export --template
./astrocs --version
./astrocs export -y            # rc=2：缺 --json
./astrocs export --force       # rc=2：unknown flag '--force'（合同用 -force）
./astrocs export --json /nonexistent.json    # rc=3 config not found
# 正式 schema 模板被 CLI 拒绝（CLI-04 证据）
./astrocs export --json ../config/templates/export.phase_config.json -y   # [error] source 缺失, rc=2

# registry 冻结集与实现数（PROJ-01）
sed -n '270,281p;348,365p' ../lib/algorithms/projection/p3_proj_v6.cpp
grep -n "当前唯一实现 TAN" ../lib/infrastructure/cli/session_commands.h

# 模块七件套机器门（X-01/X-02/X-03/RSMP-05/FITS-04/PROJ-05/RSMP-06/FITS-06）
python3 ci/run_checks.py --check CHK-MODULE-MANIFEST
python3 tools/quality/check_module_map.py --json-out run/ci/module-map/module_map.json
python3 - <<'PY'
import json
d=json.load(open("run/ci/module-map/module_map.json"))
for m in d["modules"]:
    if any(x in m["target_dir"] for x in ("projection","resample","fits_output")):
        print(m["id"], m["status"], [f["code"] for f in m["findings"]])
PY
ls contracts/schemas/export_product.schema.json contracts/schemas/fits_product.schema.json   # 均不存在

# 预检三级 / optimize 缺失（CLI-01）
grep -n "optimize" lib/infrastructure/cli/session_commands.h lib/infrastructure/cli/subcommand.h
grep -rn "\[optimize\]" tests/    # 零命中

# 预检不检文件存在（CLI-02）
grep -n "exists" lib/infrastructure/cli/subcommand.h    # 零命中

# V6 重采样未接生产 / 生产默认核（RSMP-01/02/03）
grep -rn "p3_rsmp" lib/ --include=*.cpp --include=*.h
grep -n "production_science_default" lib/algorithms/resample/p3_rsmp_kernel_registry.cpp
grep -rn "correlation_output" lib/ docs/

# 单测目标（已实测 Passed）
ctest --test-dir build -R '^(v6_p3_proj_units|p3_output|v6_p3_rsmp_core)$' --output-on-failure

# 版本纪律（CLI-03/U1）
grep -n "0.1alpha" ASTROCS_DESIGN.md ACCEPTANCE_SPEC.md
python3 ci/run_checks.py --check VERSION-CONSISTENCY
```
