# 分层扫描代理派发计划（RQS-PLAN-001）

三级：18 个叶子扫描代理（L01–L18，各写一份 `_cache/` 档案）→ 6 个合并验证代理（M1–M6，逐条读原文复核并定稿到
`findings/<类别>/pN/`）→ 前台归纳（`INDEX.md`、`SUMMARY.md`，并亲自复核全部 P0）。

## 第一层：叶子扫描（slice = 一条「SCI 文档 ↔ ALG 文档 ↔ 实现 ↔ 测试」全链）

| 码 | 域 | 主要审计对象 |
|---|---|---|
| L01 | Phase1 天测与 plate solve | docs/science/ASTROMETRY.md；docs/algorithms/PLATESOLVE.md；lib/plate_solve；tests/unit/p1wcs；tools/astrometry_oracle；tests/oracle/gaia_oracle.py |
| L02 | Phase3 投影/WCS/重采样/FITS 导出 | docs/science/PHASE3_HIPS_TO_FITS.md；docs/algorithms/PHASE3_{PROJ_IMPL,RESAMPLE,RSMP_IMPL,FITS_IMPL}.md；lib/phase3_{proj,rsmp,fits}；lib/phase3_session/p3_wcs.*；tests/unit/p3_*.cpp；tests/backend/test_p3_*.py |
| L03 | HiPS 产品与 HEALPix 映射 | docs/algorithms/{HEALPIX_MAPPING,HIPS_WRITER}.md；docs/interfaces/io/IO_002*；lib/hips；lib/hips_p2；lib/healpix_db；lib/common/healpix；lib/astro_image_io；tests/io；tests/backend/test_hips_properties.py |
| L04 | Drizzle 球面重建与不确定度 | docs/science/{DRIZZLE,UNCERTAINTY_AND_COVARIANCE}.md；docs/algorithms/DRIZZLE_GEOMETRY.md；lib/drizzle；tests/backend/test_drizzle_*.py；tests/unit/drizzle_*.cpp；tests/cli/test_p1003_drizzle_path.py |
| L05 | 校准与 cosmetique | docs/science/CALIBRATION.md；docs/algorithms/{CALIBRATION_ALGORITHMS,COSMETIC_ALGORITHMS}.md；lib/calibration；lib/cosmetic；tests/backend/test_calibration_oracle.py；tests/unit/{p1_calibration,cosmetic_*,master_flat_median}* |
| L06 | 星点检测与 PSF | docs/science/{STAR_DETECTION,PSF}.md；docs/algorithms/STAR_{DETECTION_ALGORITHMS,PSF_ALGORITHMS}.md；lib/star_detector；lib/dynamic_psf；tests/unit/p1_stars_test.cpp；tests/backend/test_wcs_psf_oracle.py |
| L07 | 测光、噪声与 SNR | docs/science/{PHOTOMETRY,NOISE_MODEL,CONTROL_WEIGHT_SNR}.md；docs/algorithms/{PHOTOMETRIC_FIT,NOISE_ESTIMATION}.md；lib/photometric_calib；lib/snr_estimator；tests/unit/{p1_noise*,p1_wcs_phot_test.cpp}；tests/backend/test_noise_model_oracle.py |
| L08 | Phase2 UPM / 控制采样 / 覆盖 | docs/science/PHASE2_UPM.md；docs/algorithms/{PHASE2_UPM_IMPL,UPM_SOLVER,PHASE2_SAMPLER,PHASE2_COVERAGE}.md；lib/phase2_upm；lib/phase2_samp；lib/phase2；tests/unit/p2_upm_synthetic_test.cpp；tests/api/test_upm_*.py；tests/backend/test_p200{1,2}_*.py |
| L09 | Phase2 排异 / 积分 / 写出 / 会话 | docs/science/{REJECTION,INTEGRATION,SCIENCE_SCOPE}.md；docs/algorithms/{REJECTION_ALGORITHMS,PHASE2_REJECTION,INTEGRATION_ALGORITHMS,PHASE2_INTEGRATION,PHASE2_MOSAIC_WRITE,PHASE2_SESSION}.md；lib/phase2_{rej,int,session}；tests/unit/p2_{rejection,output_semantics,seam_gate}_test.cpp；tests/api/test_reject_*.py；tests/backend/test_p200{3,4,5}_*.py |
| L10 | Gaia 星表与数据/产物语义合同 | docs/algorithms/GAIA_QUERY.md；lib/gaia_xpsd_client；docs/contracts/{DATA_SEMANTICS,DATA_ARTIFACTS}.md；docs/interfaces/data/*；contracts/data/*；tests/unit/gaia_*；tests/{artifact,contracts} |
| L11 | 运行时、线程预算、CPU 后端、ACR | docs/architecture/{THREADING_MODEL,THREAD_BUDGET_ARCH,EXECUTION_MODEL,PERFORMANCE_MODEL,CPU_BACKEND_ARCH,ISA_VARIANTS,ISA_BIT_MANIP_VARIANTS,CACHE_POLICY,ASYNC_IO_CONTRACT,DATA_FLOW}.md；runtime/；lib/backend_host；providers/cpu；lib/orchestrator；lib/acr；docs/{science,algorithms}/ACR_EQUIVALENCE.md；tools/arch/check_thread_budget.py；tests/{arch,unit}/…线程与利用率门禁测试 |
| L12 | CLI、ABI、模块注册与构建面 | docs/standards/{C_ABI_STANDARD,API_STANDARD,ERROR_HANDLING_STANDARD,LOGGING_DIAGNOSTICS_STANDARD}.md；docs/architecture/{ERROR_MODEL,MODULE_MAP,DEPENDENCY_RULES,BUILD_GRAPH,PHASE3_MODULE_ARCH,abi/ABI_003*}.md；docs/contracts/{API-001,ARCH-001,RT-001,PUBLIC_API}.md；include/astrocs/{abi,common_abi_v1.h}；cli/；contracts/{schemas,config}；CMakeLists.txt；cmake/；lib/*/module.yaml；docs/modules/**；modules/；tests/{abi,api,cli} |
| L13 | 注释广度扫描（Phase1 侧 + 公共） | lib/{calibration,cosmetic,star_detector,dynamic_psf,photometric_calib,snr_estimator,drizzle,plate_solve,common,core}；include/astrocs/{core,contracts,io} |
| L14 | 注释广度扫描（Phase2/3 + 运行时） | lib/phase2*；lib/phase3*；lib/hips*；lib/{healpix_db,gaia_xpsd_client,astro_image_io,backend_host,acr,orchestrator}；runtime/；cli/；providers/；modules/ |
| L15 | 国际标准横向合规 | docs/standards/**（STANDARDS_REGISTRY 六域 + 14 个 STANDARD.md + checks/）对照 docs/science、docs/algorithms 与实现；可用 web 检索核对标准原文 |
| L16 | 追溯链与机器检查有效性 | docs/traceability/**；docs/algorithms/anchors/**；docs/validation/SCIENCE_FREEZE.md；docs/quality/**；contracts/schemas/traceability_matrix.schema.json；tests/{traceability,sciencelint,glossary,quality}；tools/{traceability,doccheck,quality}（读源码判断检查器是否真能发现其声称覆盖的问题） |
| L17 | 治理与发布门禁 | ASTROCS_PROJECT_CONSTITUTION.md；AGENTS.md；AstroCS_ENGINEERING_CONSTRAINTS.md；README/HANDOVER/REVIEW/CHANGELOG/DEPENDENCIES/VERSION；docs/owner/**；docs/governance/**；docs/audit/**；docs/backlog/**；docs/review/**；ci/{checks.json,known_failures.json,impact_map.json}；.github/workflows/*；tools/check_agents_gov.py 与 tools/check_* 治理类检查器 |
| L18 | 文档体系完整性与交叉引用 | docs/** 全量文本层：引用与链接是否指向存在的文件/章节；同名分叉文档；活动文档中的陈旧版本号与历史状态；术语与单位声明一致性；memory.md 与文档冲突 |

## 第二层：合并验证

| 码 | 合并范围 |
|---|---|
| M1 | L01 + L02 + L15（天测 / 投影 / 国际标准合规） |
| M2 | L03 + L04 + L10（HiPS / HEALPix / Drizzle / 数据合同） |
| M3 | L05 + L06 + L07（校准 / 测光 / 星点 PSF / 噪声） |
| M4 | L08 + L09（Phase2 全链） |
| M5 | L11 + L12 + L17（架构 / ABI / 治理门禁） |
| M6 | L13 + L14 + L16 + L18（注释横向 / 追溯横向 / 文档横向） |

## 第三层：前台

去重跨域重复项、亲自复核每条 P0 的证据、写 `INDEX.md` 与 `SUMMARY.md`（根因聚类、系统性结论、待负责人裁定项）。
