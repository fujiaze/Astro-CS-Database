# Build Graph

> 上游：ASTROCS_DESIGN.md §8（软件架构）；ARCHITECTURE.md §1（总览与单一入口）、§7 不变量 1。

> **本文的构建图机器块由生成器从根 CMake 构建图导出**：
> 命令 = python3 eng/tools/arch/gen_build_graph_doc.py（根 CMakeLists 或子目录源集变动后重跑）。
> 门 = CON-BUILD-GRAPH（python3 eng/tools/quality/contracts/check_build_graph.py）：
> 逐行比对目标集（与生产闭包的双向差集）、类型、定义文件与源集摘要；判据的输入是构建图本身。
> 判别力自证 = 同一条命令加 --self-test。

## 1 生产构建图（生产入口的传递闭包）

生产入口取自 eng/ci/spec_named_impls.json 的 production_entry。下表 = 该入口沿
target_link_libraries 的传递闭包内的全部 target；唯一事实源 = 根 CMakeLists.txt
沿未注释 add_subdirectory 递归。src_fingerprint = 该 target 源集（排序后逐行连接）的 sha256 前 12 位，4-4-4 分组书写。

<!-- BUILD-GRAPH-TABLE:BEGIN -->
| target | kind | cmakelists | sources | src_fingerprint |
|---|---|---|---|---|
| acsd | add_executable | CMakeLists.txt | 5 | 8a28-c03e-0a23 |
| astrocs_aio | add_library | CMakeLists.txt | 8 | 00cb-06ac-ca2e |
| astrocs_calibration | add_library | CMakeLists.txt | 5 | df12-9401-fe58 |
| astrocs_cfitsio | add_library | CMakeLists.txt | 0 | e3b0-c442-98fc |
| astrocs_cli_runtime | add_library | CMakeLists.txt | 1 | 302c-75f0-f0b7 |
| astrocs_cli_subcommands | add_library | lib/infrastructure/cli/CMakeLists.txt | 0 | e3b0-c442-98fc |
| astrocs_common | add_library | CMakeLists.txt | 2 | 0999-866f-1db8 |
| astrocs_contracts | add_library | CMakeLists.txt | 0 | e3b0-c442-98fc |
| astrocs_core | add_library | CMakeLists.txt | 19 | a537-24d4-cfa9 |

| target | kind | cmakelists | sources | src_fingerprint |
|---|---|---|---|---|
| astrocs_cpu | add_library | CMakeLists.txt | 12 | 2652-49d2-aaef |
| astrocs_drizzle | add_library | CMakeLists.txt | 10 | 55ab-3624-4493 |
| astrocs_hips | add_library | CMakeLists.txt | 11 | c417-1ccb-f83e |
| astrocs_hips_properties | add_library | CMakeLists.txt | 1 | e540-501d-bac6 |
| astrocs_identifiability | add_library | CMakeLists.txt | 1 | 8c32-1b51-d415 |
| astrocs_module_adapters | add_library | CMakeLists.txt | 3 | 5f34-5c8a-c1c5 |
| astrocs_p1_dpsf | add_library | CMakeLists.txt | 3 | bd4b-5760-44b7 |
| astrocs_p1_ipv | add_library | CMakeLists.txt | 14 | bf86-f77b-497a |
| astrocs_p1_sdet | add_library | CMakeLists.txt | 6 | 64f6-d6c1-554b |

| target | kind | cmakelists | sources | src_fingerprint |
|---|---|---|---|---|
| astrocs_p3_fits_output | add_library | lib/algorithms/fits_output/CMakeLists.txt | 1 | de8b-c0b3-0a99 |
| astrocs_p3_projection_wcs | add_library | lib/algorithms/projection/CMakeLists.txt | 1 | 81c4-3738-7e10 |
| astrocs_p3_rsmp | add_library | lib/algorithms/resample/CMakeLists.txt | 7 | 74a1-a82a-a146 |
| astrocs_phase1_noise | add_library | CMakeLists.txt | 3 | 380c-35c1-6401 |
| astrocs_phase1_phot | add_library | CMakeLists.txt | 1 | 0752-1556-f4aa |
| astrocs_phase1_photcal | add_library | CMakeLists.txt | 7 | 8e2a-a074-caf3 |
| astrocs_phase1_session | add_library | CMakeLists.txt | 1 | a498-df53-69af |
| astrocs_phase1_stars | add_library | CMakeLists.txt | 1 | d71e-d61c-4a2d |
| astrocs_phase1_wcs | add_library | CMakeLists.txt | 1 | eab3-b424-92fe |

| target | kind | cmakelists | sources | src_fingerprint |
|---|---|---|---|---|
| astrocs_phase2 | add_library | CMakeLists.txt | 9 | 1f29-dde6-2cba |
| astrocs_phase2_session | add_library | CMakeLists.txt | 1 | 8229-66f7-9313 |
| astrocs_phase3_session | add_library | CMakeLists.txt | 1 | fdfa-26f7-e91a |
| astrocs_probes | add_library | CMakeLists.txt | 1 | 3f51-ed08-c746 |
<!-- BUILD-GRAPH-TABLE:END -->

## 2 非生产闭包面（交付件与工具面）

下表 target 真实存在于根构建图，且不在生产入口的链接闭包内：安装树交付件（运行期按 ABI
加载）与迁移冻结的工具面。判据 C4：任一行进入生产闭包即判红。

<!-- BUILD-GRAPH-NONPROD:BEGIN -->
| target | 理由 |
|---|---|
| acsd_runtime | 安装树根的平台 SHARED（交付件，运行期加载） |
| acsd_io | 安装树根的平台 SHARED（交付件，运行期加载） |
| astrocs_noop | 安装到 modules/ 的一致性模块（交付件） |
| astrocs_catalog_gaia | 安装到 modules/ 的星表服务模块（交付件） |
| astrocs_p1_drizzle | 安装到 modules/ 的 Phase1 模块（交付件） |
| astrocs_p1_calibration | 安装到 modules/ 的 Phase1 模块（交付件） |
| astrocs_p1_cosmetic | 安装到 modules/ 的 Phase1 模块（交付件） |
| astrocs_p1_hips_writer | 安装到 modules/ 的 Phase1 模块（交付件） |
| astrocs_cpu_baseline | 安装到 providers/ 的 baseline provider（交付件） |

| target | 理由 |
|---|---|
| astrocs-stage2 | Phase2 工具面（ARCHITECTURE §1 迁移冻结：非入口） |
| orchestrator_legacy_cli | Phase1 编排工具面（ARCHITECTURE §1 迁移冻结：非入口） |
| calibrated_pair_diag | 标定对诊断工具（非入口） |
| rejection_cli | 排异诊断工具（非入口） |
| m42_criterion_probe | 判据探针工具（非入口） |
<!-- BUILD-GRAPH-NONPROD:END -->

## 3 非根图目标（子项目自有 CMakeLists）

下表 target 不在根构建图内：其 CMakeLists 未被根 CMakeLists 的 add_subdirectory 纳入。
判据 C5：任一行出现在根构建图内即判红。

<!-- BUILD-GRAPH-NONROOT:BEGIN -->
| target | cmakelists | 理由 |
|---|---|---|
| healpix_browser_qt | lib/infrastructure/hips_browser/healpix_browser_qt/CMakeLists.txt | 浏览器工具（ARCHITECTURE §1 迁移冻结：工具面） |
| browser_cli | lib/infrastructure/hips_browser/healpix_browser_qt/CMakeLists.txt | 浏览器工具（ARCHITECTURE §1 迁移冻结：工具面） |
| acr-benchmark | lib/infrastructure/acr/tools/acr_benchmark/CMakeLists.txt | ACR DORMANT（最高设计 §8：不进生产构建） |
| acr_test_api | lib/infrastructure/acr/tests/unit/CMakeLists.txt | ACR DORMANT（最高设计 §8：不进生产构建） |
<!-- BUILD-GRAPH-NONROOT:END -->

## 4 编译定义与链接面

- OpenMP：lib/algorithms/coverage/CMakeLists.txt 的 option(P2_ENABLE_OPENMP ... OFF) 默认关；
  取 ON 且找到 OpenMP 时才 target_compile_definitions(... P2_ENABLE_OPENMP=1) 并链接
  OpenMP::OpenMP_CXX —— 编译定义与链接同源，见该文件。
- ACR/CUDA：ACR 状态 DORMANT，其源集、编译定义、链接行与安装单元都在生产面之外
  （最高设计 §8）；生产安装面的 target 集合见 eng/cmake/install_layout.cmake。

## 5 复算

    python3 eng/tools/arch/gen_build_graph_doc.py            # 从构建图导出机器块
    python3 eng/tools/quality/contracts/check_build_graph.py # 门（PASS / FAIL）
    python3 eng/tools/quality/contracts/check_build_graph.py --self-test

## 6 关联

- 安装树：eng/cmake/install_layout.cmake + eng/packaging/install-tree.contract.json；
- 执行面登记：docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv；
- 规范点名实现的生产可达性：eng/ci/check_spec_named_impl.py。
