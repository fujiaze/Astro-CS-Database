# 构建图

上游：最高设计的软件架构与双平台发行两章 [1]；模块登记见 `../architecture/MODULE_MAP.md` [2]。

生产构建图的传递闭包、非生产闭包面、子项目自有构建文件、编译定义与链接面。

## 生产构建图（生产入口的传递闭包）

生产入口取自规范点名实现登记表的 production_entry。下表 = 该入口沿
target_link_libraries 的传递闭包内的全部 target；唯一事实源 = 根 CMakeLists.txt
沿未注释 add_subdirectory 递归。src_fingerprint = 该 target 源集（排序后逐行连接）的 sha256 前 12 位，4-4-4 分组书写。

<!-- BUILD-GRAPH-TABLE:BEGIN -->
| target | kind | cmakelists | sources | src_fingerprint |
|---|---|---|---|---|
| acsd | add_executable | CMakeLists.txt | 5 | 8a28-c03e-0a23 |
| acsd_aio | add_library | CMakeLists.txt | 8 | 00cb-06ac-ca2e |
| acsd_calibration | add_library | CMakeLists.txt | 5 | df12-9401-fe58 |
| acsd_cfitsio | add_library | CMakeLists.txt | 60 | ff4f-c594-adf6 |
| acsd_cli_runtime | add_library | CMakeLists.txt | 1 | 302c-75f0-f0b7 |
| acsd_cli_subcommands | add_library | lib/infrastructure/cli/CMakeLists.txt | 0 | e3b0-c442-98fc |
| acsd_common | add_library | CMakeLists.txt | 2 | 0999-866f-1db8 |
| acsd_contracts | add_library | CMakeLists.txt | 0 | e3b0-c442-98fc |
| acsd_core | add_library | CMakeLists.txt | 20 | e1c4-8466-ab9c |

| target | kind | cmakelists | sources | src_fingerprint |
|---|---|---|---|---|
| acsd_cpu | add_library | CMakeLists.txt | 12 | 2652-49d2-aaef |
| acsd_drizzle | add_library | CMakeLists.txt | 10 | 55ab-3624-4493 |
| acsd_gaia_zlib_include | add_library | lib/infrastructure/gaia_xpsd_client/CMakeLists.txt | 0 | e3b0-c442-98fc |
| acsd_hips | add_library | CMakeLists.txt | 12 | 3f23-587c-c49f |
| acsd_hips_properties | add_library | CMakeLists.txt | 1 | e540-501d-bac6 |
| acsd_identifiability | add_library | CMakeLists.txt | 1 | 8c32-1b51-d415 |
| acsd_module_adapters | add_library | CMakeLists.txt | 3 | 1dab-024e-54ba |
| acsd_p1_dpsf | add_library | CMakeLists.txt | 3 | bd4b-5760-44b7 |
| acsd_p1_ipv | add_library | CMakeLists.txt | 14 | bf86-f77b-497a |

| target | kind | cmakelists | sources | src_fingerprint |
|---|---|---|---|---|
| acsd_p1_sdet | add_library | CMakeLists.txt | 4 | 70f1-4ec7-61a7 |
| acsd_p3_fits_output | add_library | lib/algorithms/fits_output/CMakeLists.txt | 1 | de8b-c0b3-0a99 |
| acsd_p3_projection_wcs | add_library | lib/algorithms/projection/CMakeLists.txt | 1 | 81c4-3738-7e10 |
| acsd_p3_rsmp | add_library | lib/algorithms/resample/CMakeLists.txt | 7 | 74a1-a82a-a146 |
| acsd_phase1_noise | add_library | CMakeLists.txt | 3 | 380c-35c1-6401 |
| acsd_phase1_phot | add_library | CMakeLists.txt | 1 | 0752-1556-f4aa |
| acsd_phase1_photcal | add_library | CMakeLists.txt | 7 | 8e2a-a074-caf3 |
| acsd_phase1_session | add_library | CMakeLists.txt | 1 | a498-df53-69af |
| acsd_phase1_stars | add_library | CMakeLists.txt | 1 | d71e-d61c-4a2d |

| target | kind | cmakelists | sources | src_fingerprint |
|---|---|---|---|---|
| acsd_phase1_wcs | add_library | CMakeLists.txt | 1 | eab3-b424-92fe |
| acsd_phase2 | add_library | CMakeLists.txt | 8 | f368-2182-1625 |
| acsd_phase2_session | add_library | CMakeLists.txt | 1 | 8229-66f7-9313 |
| acsd_phase3_session | add_library | CMakeLists.txt | 1 | fdfa-26f7-e91a |
| acsd_platform_math | add_library | CMakeLists.txt | 0 | e3b0-c442-98fc |
| acsd_platform_zlib | add_library | CMakeLists.txt | 0 | e3b0-c442-98fc |
| acsd_probes | add_library | CMakeLists.txt | 1 | 3f51-ed08-c746 |
<!-- BUILD-GRAPH-TABLE:END -->

## 非生产闭包面（交付件与工具面）

下表 target 真实存在于根构建图，且不在生产入口的链接闭包内：安装树交付件（运行期按 ABI
加载）与迁移冻结的工具面。判据 C4：任一行进入生产闭包即判红。

<!-- BUILD-GRAPH-NONPROD:BEGIN -->
| target | 理由 |
|---|---|
| acsd_runtime | 安装树根的平台 SHARED（交付件，运行期加载） |
| acsd_io | 安装树根的平台 SHARED（交付件，运行期加载） |
| acsd_noop | 安装到 modules/ 的一致性模块（交付件） |
| acsd_catalog_gaia | 安装到 modules/ 的星表服务模块（交付件） |
| acsd_p1_drizzle | 安装到 modules/ 的 Phase1 模块（交付件） |
| acsd_p1_calibration | 安装到 modules/ 的 Phase1 模块（交付件） |
| acsd_p1_cosmetic | 安装到 modules/ 的 Phase1 模块（交付件） |
| acsd_p1_hips_writer | 安装到 modules/ 的 Phase1 模块（交付件） |
| acsd_cpu_baseline | 安装到 providers/ 的 baseline provider（交付件） |

| target | 理由 |
|---|---|
| acsd-stage2 | Phase2 工具面（ARCHITECTURE 「生产构建图」一节 迁移冻结：非入口） |
| orchestrator_legacy_cli | Phase1 编排工具面（ARCHITECTURE 「生产构建图」一节 迁移冻结：非入口） |
| calibrated_pair_diag | 标定对诊断工具（非入口） |
| rejection_cli | 排异诊断工具（非入口） |
<!-- BUILD-GRAPH-NONPROD:END -->

## 非根图目标（子项目自有 CMakeLists）

下表 target 不在根构建图内：其 CMakeLists 未被根 CMakeLists 的 add_subdirectory 纳入。
判据 C5：任一行出现在根构建图内即判红。

<!-- BUILD-GRAPH-NONROOT:BEGIN -->
| target | cmakelists | 理由 |
|---|---|---|
| healpix_browser_qt | lib/infrastructure/hips_browser/healpix_browser_qt/CMakeLists.txt | 浏览器工具（ARCHITECTURE 「生产构建图」一节 迁移冻结：工具面） |
| browser_cli | lib/infrastructure/hips_browser/healpix_browser_qt/CMakeLists.txt | 浏览器工具（ARCHITECTURE 「生产构建图」一节 迁移冻结：工具面） |
<!-- BUILD-GRAPH-NONROOT:END -->

## 编译定义与链接面

- OpenMP：lib/algorithms/coverage/CMakeLists.txt 的 option(P2_ENABLE_OPENMP ... OFF) 默认关；
 取 ON 且找到 OpenMP 时才 target_compile_definitions(... P2_ENABLE_OPENMP=1) 并链接
 OpenMP::OpenMP_CXX —— 编译定义与链接同源，见该文件。
- ACR/CUDA：ACR 状态 DORMANT，其源集、编译定义、链接行与安装单元都在生产面之外
 （最高设计的软件架构一章）[1]；生产安装面的 target 集合见 eng/cmake/install_layout.cmake。

## 复算

 python3 eng/tools/arch/gen_build_graph_doc.py # 从构建图导出机器块

复算后的核对 = 把本文三张机器块与生成器输出逐行比对：目标集与生产闭包的双向差集、
`kind`、定义文件、源集数量与 `src_fingerprint` 全部一致。任一行对不上即判红 ——
判据的输入是构建图本身，文档转述不构成证据。

**复算链当前不可复跑，须由代码侧订正后重导**。三条阻断，每条都可复现：

| 阻断 | 复现命令 | 读数 |
|---|---|---|
| 生成器落点常量指向不存在的路径 | `grep -n 'DOC_REL = ' eng/tools/arch/gen_build_graph_doc.py` | 常量值 `docs/engineering/BUILD_GRAPH.md`；该路径在仓内不存在（本文真实落点是 `docs/engineering/build/BUILD_GRAPH.md`） |
| 生成器自身 fail-closed 拦截 | `python3 eng/tools/arch/gen_build_graph_doc.py --out /tmp/bg.md` | 退出码 1，stderr `NONPROD 登记项不在根构建图: acsd-stage2` |
| 非生产登记项含三个非根图目标 | 同上，逐项核对 `acsd-stage2` / `calibrated_pair_diag` / `rejection_cli` | 三者由 `lib/algorithms/coverage/CMakeLists.txt` 声明，但该目录未被根 `CMakeLists.txt` 的 `add_subdirectory` 纳入 ⇒ 三者不在根构建图，本节抬头的「真实存在于根构建图」断言对这三行不成立 |

因此本文三张机器块目前由本车道手工重导，内容与生成器的渲染函数逐字一致，但**生成器跑不出这个结果**。
上表三条由代码侧订正、生成器能以 0 退出后，须由生成器就地重导一次并以该次输出为准。
生产构建图的值域可独立复算：生产入口 `acsd`，传递闭包 34 个 target；本文表内 34 行，
逐行的 `sources` 与 `src_fingerprint` 均由 `eng/tools/arch/cmake_graph.py` 的
`parse_cmake_graph` / `production_entry` / `production_closure` / `source_fingerprint` 直接产出。

## 关联

- 安装树：eng/cmake/install_layout.cmake + eng/packaging/install-tree.contract.json；
- 工具链与构建档位：`BUILD_NODES.md`；
- 模块登记与点名实现的落点映射：`../architecture/MODULE_MAP.md` [2]；
- 规范点名实现的生产可达性：规范点名实现必须落在生产入口沿 `target_link_libraries` 的传递闭包内。封闭性核对 = 以根 CMakeLists.txt 的实际闭包为准，逐个点名实现反查其所属 target 是否在该闭包内；查不到即判红。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md`，最高设计的软件架构与双平台发行两章。
[2] 内部文档 `docs/engineering/architecture/MODULE_MAP.md`，模块登记与落点映射。
