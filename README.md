# AstroCS — 天文 CCD 图像校准与标准化数据库

> **权威链（唯一）**：`ASTROCS_DESIGN.md`（最高设计）→ `AGENTS.md` → `ENGINEERING_SPEC.md` →
> `CONTROL_PACK_SPEC.md` → `docs/ci/` → `docs/plugins/`（23 篇）；另立 `docs/science/`（公式权威）、
> `docs/algorithms/`（推导权威）、`docs/design/UNIFIED_MODEL.md`（数据对象与三类配置分离）。
> 与其他文档冲突时以 `ASTROCS_DESIGN.md` 为准（§0）；已删除的旧根治理文件（旧宪章、旧工程约束、旧 `CHANGELOG.md`/`REVIEW.md`/`HANDOVER.md`）、
> `设计大纲/`、`evidence/**`
> 已由 ROOT-007、CLEAN-402 及后续根清洁删除，历史仅存在于 git 历史。
> 目标产品版本为根 `VERSION` = `0.11.0-alpha.2`——**仅内部开发助记符**，不进入程序/代码/产物；
> Alpha 前程序内不存在任何版本信息（ASTROCS_DESIGN §12、ENGINEERING_SPEC §7）。
> 现状与发布口径：`docs/owner/RELEASE_STATUS.md`、`docs/KNOWN_LIMITATIONS.md`。
> 本轮治理控制包：`工程控制/PROJECT-GOVERNANCE-01/`（任务与验收状态以该包 `TASK_LIST.md`/`ACCEPTANCE.md` 为准）。
>
> **V6 产品族冻结合同（现行合同面）**：Phase2 生产~~权重模式~~ =（已按 §9.73 A44 作废：该概念不存在；权重是阶段二按该天球像素对应帧集合现场算出的派生量）
> `{point_information, surface_gls, psfsw_robust}`；文档基线 = `{equal, pixel_ivar}`；
> `psf_snr_power` = **DEFERRED 且生产拒绝**。语义权威 = `docs/contracts/DATA_SEMANTICS.md` §31
> （96 条款：FROZEN 39 / PENDING_OWNER_SIGNOFF 49 / OPEN 8，逐条登记见 §31.10）；
> 机器登记表 = `eng/contracts/data/v6_clause_registry_v1.json`。
> 当前**未发布**（NOT_READY_FOR_RELEASE）。

## 目标态设计与科学参考

- [最高设计](ASTROCS_DESIGN.md)：AstroCS 是什么、做到什么、CLI/架构/验收（最高权威）。
- [工程规范](ENGINEERING_SPEC.md) / [机干活手册](AGENTS.md) / [控制包规范](CONTROL_PACK_SPEC.md)。
- [数据对象与配置分离](docs/design/UNIFIED_MODEL.md)：signal/variance/ivar/support/coverage/… 各处一义。
- [Phase1 详细设计](docs/design/PHASE1_DETAILED_DESIGN.md)：单帧观测模型、PSF/噪声/信息产品。
- [Phase2 详细设计](docs/design/PHASE2_DETAILED_DESIGN.md)：UPM、排异、扩展源与点源最优合并。
- [Phase3 详细设计](docs/design/PHASE3_DETAILED_DESIGN.md)：HiPS→WCS FITS、采样和不确定度传播。
- [统一科学定义](docs/science/UNIFIED_SCIENCE_MODEL.md)：SNR、inverse variance、PSF information 与跨阶段合同。
- [PSF Signal Weight](docs/science/PSF_SIGNAL_WEIGHT.md)：严格 W_info 与 PixInsight-style `psfsw_robust` 双轨。
- [插件文档](docs/plugins/00_INDEX.md)：23 篇模块规范（每篇 8 节模板）。
- [科学与格式参考文献档案](docs/references/SCIENTIFIC_REFERENCES.md)：Horne/Naylor、Zackay & Ofek、PixInsight PSFSW、Drizzle、HEALPix/HiPS、FITS WCS、全局相对定标与排异。

README 不复制公式；上述文件是设计与引用入口。

## 是什么

AstroCS 是天文 CCD/CMOS 图像校准与标准化数据库系统。产品形态是**三个平级独立命令**，
不是一个固定顺序的流水线（ASTROCS_DESIGN §1.2）：

- **`normalize`（内部指代 Phase1）**：JSON 配置 + 元数据 → 标准化单帧 HiPS（帧级 SNR 入文件头，
  可选稀疏帧内 SNR 层）+ 结构化 JSON。数据块 = 一组 light + 对应校准帧 + 滤镜。
- **`mosaic`（内部指代 Phase2）**：一组合同兼容 HiPS + JSON 配置 → 马赛克 HiPS +
  UPM/排异/集成 provenance + 结构化 JSON；三种显式生产口径 `point_information`（已按 §9.73 A44 作废：该概念不存在；权重是阶段二按该天球像素对应帧集合现场算出的派生量）
  （`Q=aPᵀC⁻¹d`、`W_info=a²PᵀC⁻¹P`）、`surface_gls`（扩展源 GLS）、`psfsw_robust`
  （PixInsight-style，**无量纲/组内 median=1、不是 ivar/Fisher**）；文档基线模式
  `equal`/`pixel_ivar` 仅作对照；`psf_snr_power` 保持 DEFERRED 且生产拒绝。
- **`export`（内部指代 Phase3）**：任一合同兼容 HiPS（**不要求来自 mosaic**）+ JSON 配置 →
  平面 WCS FITS（已叠加，无需再带权重）。

阶段间只通过**磁盘产品 + manifest + 哈希**交换（DATA-002）；三个命令各自独立启动、独立恢复、
独立验收，**禁止**隐式串接为一次运行。正式平台为 Windows x64（交付 `astrocs.exe` 唯一入口 +
各 `.dll`）与 Linux amd64（`astrocs`），纯 CPU 生产；ACR 保留源码但 DORMANT
（生产构建默认排除，ASTROCS_DESIGN §8/§10.1）。

## 当前状态（如实；状态词只用 ASTROCS_DESIGN §12.5 词表）

§12.5 词表：`CONTRACT_READY` / `IMPLEMENTED` / `INSTALLED` / `VERIFIED`，负向
`NOT_IMPLEMENTED` / `NOT_VERIFIED` / `DEFERRED` / `DORMANT` / `FAIL`。
**合成测试或历史可用节点不等于真实数据/Windows VERIFIED**（§12.5 末条）。

- 逐模块/逐阶段状态与证据锚：`docs/owner/RELEASE_STATUS.md`、`docs/modules/MODULE_MAP.yaml`。
- **发布结论 = `NOT_READY_FOR_RELEASE`**：`VERIFIED` 要求正式平台（Windows x64）+ 真实数据
  验收通过，当前**未达成**（`NOT_VERIFIED`）；Agent 至多声明 `READY_FOR_OWNER_REVIEW`，
  最终发布决定只属项目负责人（§12）。
- 51 条待决条款/开放项（49 `PENDING_OWNER_SIGNOFF` + 8 条款级 `OPEN`）保持 fail-closed；
  `psf_snr_power` 保持 `DEFERRED`。
- 用户命令面 = 唯一命令树 `normalize/mosaic/export + help/--version/doctor/benchmark`
  （CLI-001，ASTROCS_DESIGN §7.1）；`phase1|2|3`、`run --phases`、`validate/plan/inspect`
  均不在命令树内（实测 rc=2），`phase` 仅为内部指代）。
- ACR = `DORMANT`；HiPS Browser = 未来可视化组件，`NOT_IMPLEMENTED` 且不进产品 manifest（§1.3）。

## 仓库布局（模块索引权威：docs/architecture/MODULE_MAP.md、docs/modules/；ASTROCS_DESIGN §7.1）

```text
lib/algorithms/      科学算法唯一家（并联放置；phase 为内部指代）
                     calibration cosmetic star_detection psf platesolve photometry
                     noise_snr drizzle coverage sampling upm rejection integration
                     projection resample fits_output shared/
lib/infrastructure/  基建：cli/{normalize,mosaic,export} + scheduler pipeline aio
                     benchmark observability gaia_xpsd_client acr hips_browser
lib/phase{1,2,3}_session  三阶段会话编排（引用算法模块，不重复实现）
lib/include/astrocs/     公共头（ABI/core/io；版本化 C ABI）
eng/contracts/           合同 schema（唯一事实源：schemas/data/config）
eng/cmake/              平台相关构建与安装布局
eng/packaging/config/             程序根全局配置（filters.json / defaults.json）
docs/               文档体系（science/algorithms/plugins/ci/design/architecture/...）
eng/tests/              测试（与模块共址可复用）；testdata/ 真实数据与合成数据
工程控制/            控制包（一个控制包一个子目录）
run/                临时产物/日志（gitignore，不入库）
```

根目录固定条目见 `ENGINEERING_SPEC.md` §7；新根目录条目必须先登记并经负责人确认。

## 快速入口

- 构建：`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` → `ninja -C build`
  （唯一根 CMake）；测试 `ctest --test-dir build --output-on-failure`；
  机器一致性检查见 `docs/ci/CI_SPEC.md`。
- 命令面：`normalize|mosaic|export --json <config.json>`（`--template` 生成模板，`--help` 字段说明）；
  `help` / `--version` / `doctor` / `benchmark`（docs/api/CLI_PROTOCOL_V1.md）。
- 文档路由：`docs/standards/DOCUMENTATION_STANDARD.md`（文档体系分层）、`docs/DOCUMENT_INDEX.yaml`（机器索引：
  活动/归档边界 + `eng/tools/doccheck/check_doc_index.py` 校验）。
- 记忆：根 `memory.md`（稳定目标/模块索引/开放问题）。
- 变更历史：无独立 CHANGELOG（已删除）；历史只存在于 git 历史
  （`ARCHIVED_NON_NORMATIVE`）与 `git log`。

## 历史说明

V1–V19/V18R2/V19R2/V19R8/V19R6R2-W1 等历史轮次（含旧 11 子仓结构、旧 F 盘路径、
旧 16 线程约定、Python 调 DLL 时代）**不是当前状态**：历史只存在于 git 历史
（`ARCHIVED_NON_NORMATIVE`）与 `docs/**/v6/**`（V6 产品族冻结/设计档案）。
已删除的旧根治理文件（`ASTROCS_PROJECT_CONSTITUTION.md`、`AstroCS_ENGINEERING_CONSTRAINTS.md`、
旧 `CHANGELOG.md`/`REVIEW.md`/`HANDOVER.md`）、`设计大纲/`、`evidence/**`、旧 `工程控制/AstroCS_*` 控制包（均已删除）
及其旧历史控制包归档树**不得再被当作现状引用**。详见根 `memory.md` 与 git 历史。
