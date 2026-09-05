# AstroCS 项目交接文档（HANDOVER）

> 更新：2026-09-02（GOV-005 现状收敛；历史轮次交接内容移入 CHANGELOG 历史节与
> `docs/archive/history/`，不再在本文件冒充当前状态）。
> 分支：main ｜ 本交接基线 HEAD（2026-09-02 交接时点，历史值；当前 HEAD 以
> `git rev-parse HEAD` 为准）：`6affe3009985452f5bc0bdf654aa95a4b61b2d2e`
> （`docs(owner): GOV-004 建立负责人审查入口`；后续集成由前台在 main 串行推进）。
> 权威：根 `AstroCS_ENGINEERING_CONSTRAINTS.md`（冻结约束）、`REVIEW.md` +
> `docs/owner/`（L0 负责人入口）、`docs/` 分层文档体系；本文件只做交接定位，
> 不复制权威内容。

## 1. 项目总览

AstroCS 是天文 CCD 图像校准与标准化数据库系统：Phase1 把单帧 FITS 经校准/
定标/星点/WCS/测光/噪声/SNR/Drizzle 建成标准化 IVOA HiPS（signal/variance/
ivar/snr/support + manifest）；Phase2 把一组合同兼容 HiPS 经 UPM 联合光度模型
（背景稳健采样、Huber、ivar 科学权重）叠加为马赛克 HiPS；Phase3 把任一合同
兼容 HiPS 投影为平面 FITS（WCS/coverage/validity/provenance）。三 Phase 是
隔离命令，跨 Phase 仅磁盘产品交换（DATA-002）。

- 主仓库: https://github.com/fujiaze/Astro-CS-Database
- 产品版本唯一源：根 `VERSION` = `0.11.0-alpha.2`（GOV-003）；
  生成串 `0.11.0-alpha.2+g<commit12>`（docs/governance/VERSION_NAMESPACES.md）。
- 权威文档：`docs/`（L0–L5）+ `AstroCS_ENGINEERING_CONSTRAINTS.md`；
  唯一产品构建入口：根 `CMakeLists.txt`（BLD-002）。
- 正式平台 Windows x64；Linux amd64 为控制/静态分析/轻量编译节点（约束 §B）。

## 2. 生产入口（当前基线）

```text
构建   : 根 CMakeLists.txt（Windows preset win-msvc-17.14.39-x64 / Linux linux-control）
命令   : astrocs phase1 run --config <path>
          astrocs phase2 run --config <path>
          astrocs phase3 run --config <path>
          astrocs verify / doctor / benchmark cpu（docs/api/CLI_PROTOCOL_V1.md）
遗留   : astrocs run --phases 1,2,3 仍在 cli 中（与约束 §A.4 冲突，W4 待删，见 §6）
```

> 旧轮次入口（`lib/orchestrator/cpp/orchestrator.exe`、`astrocs-stage2.exe`、
> `healpix_browser_qt.exe`、`toolchain.ps1 run` 等）是历史时代产物（旧 V 轮次），
> 当前基线已迁至唯一 `astrocs` CLI + 根 CMake；历史细节见 CHANGELOG 历史节与
> `docs/archive/history/`，不冒充当前入口。

## 3. 当前状态（2026-09-02，GOV-005 基线；权威口径见 docs/owner/RELEASE_STATUS.md）

- 合同面（工程约束/文档边界/版本单源/ABI v1/数据产物/三阶段交换合同/Runtime 图/
  Windows 工具链 preset/DLL 边界 schema/结构化日志/FITS 流式接口/L0 入口）已
  冻结入 main（PASS，静态可核；各集成验收由前台在各自集成提交执行）。
- 执行面未完成（NOT_VERIFIED，如实）：Windows DLL 化发布安装树（astrocs.exe +
  runtime/io/科学模块/provider DLL）未产出/验证；MSVC 编译/测试与真实数据
  （BASS/32R/接缝）最终验收未跑（Fatduck 侧）；Phase3 SIN/ZEA/CAR/AIT 投影、
  `healpix_interp4`、流式 FITS 输出接入不在当前基线。
- 发布结论：NOT_READY_FOR_RELEASE（未到 READY_FOR_OWNER_REVIEW；发布裁定权
  只在项目负责人，约束 §H）。

## 4. 模块地图（权威：docs/architecture/MODULE_MAP.md、docs/modules/*.md）

```text
产品骨架   cli/（astrocs 可执行源）；lib/core（runtime/类型化运行图/模块注册表）；
           lib/io（IO-001）；lib/common（sha256/HEALPix）；modules/services/io
阶段会话   lib/phase1_session / lib/phase2_session / lib/phase3_session
科学模块   lib/astro_image_io、calibration、dynamic_psf、star_detector、
           plate_solve、photometric_calib、snr_estimator、gaia_xpsd_client、
           healpix_db（healpix_drizzle + healpix_browser_qt）、phase2、
           orchestrator、acr（dormant）
公共接口   include/astrocs/（contracts/abi/core/io）；contracts/（data-schema）
运行图     runtime/（typed_dag、artifact_store；RT-001）
```

## 5. 数据与外部依赖

```text
testdata/    测试数据（只读）
BASS DR3/    BASS DR3 备用数据集索引（真实数据验收待 Windows/Fatduck 阶段）
lib/astro_image_io/third_party/cfitsio   vendored CFITSIO（显式源清单，BLD-001）
```

> 真实数据（GaiaDR3/GaiaDR3SP/BASS）大体积文件本地化且 gitignored，不进入仓库
> 与发布包；32R/真实数据最终验收按约束 §E.5 只在最终候选提交 Windows 上执行一次。

## 6. 冻结与待办（如实）

- 已冻结：工程约束（GOV-001）、文档边界/归档（GOV-002）、产品版本单源
  （GOV-003）、L0 负责人入口（GOV-004）、ABI v1 / DATA-001/002 / RT-001 /
  LOG-001 / ARC-001 / BLD-001/002 / IO-001 合同面。
- 待办（按控制包任务图 Wave）：W3 模块 DLL 迁移（§F.1 每节点唯一 operation）；
  W4 删除遗留 `run --phases 1,2,3` 与伪/旧路由；W5 Linux 验证；W6 Windows 正式
  验证（DLL 发布树 + MSVC + 32R/真实数据）；W7 文档收敛；W8 独立终审。
- 开放问题清单：根 memory.md §5（与 REVIEW/docs/owner 同口径）。

## 7. 历史与归档

- 历史轮次（V1–V19/V18R2/V19R2/V19R8/V19R6R2-W1、旧 11 子仓结构、旧 F 盘路径、
  旧 16 线程约定、Python 调 DLL 时代）只存在于 CHANGELOG.md 历史节与
  `docs/archive/**`、`engineering/control/archive/**`（ARCHIVED_NON_NORMATIVE）。
- 历史操作记忆本体：`docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md`。
- 归档不删除、不冒充现状；追溯用途见 `docs/archive/history/README.md`。
