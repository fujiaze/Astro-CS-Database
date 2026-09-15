# AstroCS — 天文 CCD 图像校准与标准化数据库

> 目标产品：`0.11.0-alpha.2`（根 `VERSION`，GOV-003 唯一源）。
> 检出基提交：`6affe3009985452f5bc0bdf654aa95a4b61b2d2e`（GOV-005 工作树检出基，
> 2026-09-02 时点历史值；当前 HEAD 以 `git rev-parse HEAD` 为准）。
> 负责人设计入口：`docs/owner/PROJECT_SPEC.md`（冻结宪章之下唯一目标态总规范）以及
> `docs/design/PHASE1_DETAILED_DESIGN.md`、`PHASE2_DETAILED_DESIGN.md`、
> `PHASE3_DETAILED_DESIGN.md`；统一科学定义见 `docs/science/UNIFIED_SCIENCE_MODEL.md`。
> 项目采用的科学与格式参考文献统一留档于 `docs/references/SCIENTIFIC_REFERENCES.md`，
> 具体 SCI/设计 claim 仍须给出论文节/式和项目推导差异。现状/发布口径见
> `docs/owner/RELEASE_STATUS.md` 与 `docs/KNOWN_LIMITATIONS.md`。
> 根 `ASTROCS_PROJECT_CONSTITUTION.md` 是 FROZEN 唯一最高约束；
> `AstroCS_ENGINEERING_CONSTRAINTS.md` 已降级为历史参照。


## 目标态设计与科学参考

- [最高设计细节规范](docs/owner/PROJECT_SPEC.md)：项目必须做到什么。
- [Phase1 详细设计](docs/design/PHASE1_DETAILED_DESIGN.md)：单帧观测模型、PSF/噪声/信息产品。
- [Phase2 详细设计](docs/design/PHASE2_DETAILED_DESIGN.md)：UPM、排异、扩展源与点源最优合并。
- [Phase3 详细设计](docs/design/PHASE3_DETAILED_DESIGN.md)：HiPS→WCS FITS、采样和不确定度传播。
- [统一科学定义](docs/science/UNIFIED_SCIENCE_MODEL.md)：SNR、inverse variance、PSF information 与跨阶段合同。
- [PSF Signal Weight](docs/science/PSF_SIGNAL_WEIGHT.md)：严格 W_info 与 PixInsight-style `psfsw_robust` 正式可选集成双轨。当前实施入口为 `工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/`（并行子代理 DAG）。
- [科学与格式参考文献档案](docs/references/SCIENTIFIC_REFERENCES.md)：Horne/Naylor、Zackay & Ofek、PixInsight PSFSW、Drizzle、HEALPix/HiPS、FITS WCS、全局相对定标与排异；并纳入既有测光专项 [B1]–[B90] [tracked 原文快照](docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md)及核验规则。

README 不复制公式；上述文件是设计与引用入口。

## 是什么

AstroCS 是天文 CCD 图像校准与标准化数据库系统。产品模型（约束 §A）为三个隔离
命令，不是固定顺序流水线：

- **Phase1**：单帧 light + masters/catalog/config → 单帧标准化 IVOA HiPS + manifest
  （校准/定标/星点/WCS/测光/噪声/SNR/Drizzle/投影）。
- **Phase2**：任意一组合同兼容 HiPS → 马赛克 HiPS + UPM/rejection/integration
  provenance；支持扩展源 GLS、点源 information/Q-W 及 PixInsight-style `psfsw_robust` 三种显式集成模式。
- **Phase3**：任一合同兼容 HiPS（不要求来自 Phase2）→ 平面 FITS +
  WCS/coverage/validity/provenance。

阶段间只通过原子发布、哈希与 provenance 完整的磁盘产品/manifest 交换
（DATA-002）。正式平台为 Windows x64（用户只面对 `astrocs.exe`，运行时/I/O/
科学模块/CPU provider 以 DLL 交付）；Linux amd64 仅作控制/静态分析/轻量编译/
小合成节点（约束 §B）。ACR 为 DORMANT（生产构建默认排除，§C）。

## 当前状态（如实，详见 docs/owner/）

- Alpha 架构收敛进行中：工程约束/文档边界/版本单源/ABI v1/数据产物/Runtime 图/
  Windows 工具链 preset/DLL schema/FITS 流接口已冻结入 main（合同面 PASS）。
- Windows DLL 化发布安装树、MSVC 编译/测试/32R/真实数据最终验收未完成
  （NOT_VERIFIED）；Phase3 SIN/ZEA/CAR/AIT、`healpix_interp4`、流式 FITS 接入
  未实现（NOT_VERIFIED）。当前状态 **NOT_READY_FOR_RELEASE**。
- 遗留 `astrocs run --phases 1,2,3` 进程内连跑与约束 §A.4 冲突，未删除
  （FAIL，W4 范围）；约束 §F.1 每节点唯一模块 operation 未达成（W3/W4）。
  以上均如实登记于 REVIEW/docs/owner，不冒充已实现。

## 仓库布局（模块索引权威：docs/architecture/MODULE_MAP.md、docs/modules/）

```text
cli/                    astrocs CLI（phase1/2/3 run、verify、doctor 等；唯一可执行源）
lib/core lib/io         运行时/类型化运行图/模块注册表；IO-001 流式 FITS 适配
lib/common              sha256 / HEALPix core
lib/phase{1,2,3}_session 三阶段会话实现（p1/p2/p3_session）
lib/<module>/           科学模块树（astro_image_io/calibration/dynamic_psf/
                        star_detector/plate_solve/photometric_calib/snr_estimator/
                        gaia_xpsd_client/healpix_db/phase2/orchestrator/acr）
modules/services/io     IO-001 流式 FITS 模块服务（头 + 自测）
include/astrocs         公共头（contracts/abi/core/io）
contracts/              类型化产物/data-schema/module_dll_contract schema
docs/                   文档体系（science/algorithms/architecture/standards/
                        modules/contracts/owner/governance/...）
runtime/                运行图/artifact_store/typed_dag 等（RT-001）
engineering/control/archive/  历史控制包归档（GOV-002）
```

## 快速入口

- 构建入口：根 `CMakeLists.txt`（唯一 `project(astrocs)`，BLD-002）；
  Windows preset `win-msvc-17.14.39-x64` / Linux `linux-control`（CMakePresets.json）。
- 命令面：`astrocs phase1/2/3 run --config <path>`（docs/api/CLI_PROTOCOL_V1.md）。
- 文档：`docs/README-DOCS.md`（L0–L5 分层）、`docs/DEVELOPER_GUIDE.md`、
  `docs/RELEASE_STATUS.md`、`docs/KNOWN_LIMITATIONS.md`。
- 记忆：根 `memory.md`（唯一项目记忆：稳定目标/当前 SHA/版本/模块索引/开放问题）。
- 变更历史：`CHANGELOG.md`（含当前 alpha 节；历史轮次节仅供追溯）。

## 历史说明

V1–V19/V18R2/V19R2/V19R8/V19R6R2-W1 等历史轮次（含旧 11 子仓结构、旧 F 盘路径、
旧 16 线程约定、Python 调 DLL 时代）不是当前状态：历史只存在于 CHANGELOG.md
历史节与 `docs/archive/**`、`engineering/control/archive/**`
（ARCHIVED_NON_NORMATIVE），详见根 memory.md §4 与
`docs/archive/history/README.md`。
