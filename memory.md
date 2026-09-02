# AstroCS 项目记忆（GOV-005 收敛）

> 本文件是仓库**唯一项目记忆**，自 GOV-005（SA-GOV-01，2026-09-02）起只保留：
> 稳定目标、当前 SHA/版本、模块索引、开放问题。
> 历史开发过程记录（V1–V19/V18R2/V19R2/V19R8/V19R6R2-W1 轮次、11 子仓合并、
> 旧 F 盘路径、旧线程数、Python 调 DLL 等失效现状）已整体归档至
> `docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md`
> （ARCHIVED_NON_NORMATIVE，仅追溯用，不代表当前状态）。
> 科学/算法/架构/发布权威一律以 `docs/`（science|algorithms|architecture|
> standards|modules|contracts）、`AstroCS_ENGINEERING_CONSTRAINTS.md` 与
> `REVIEW.md` + `docs/owner/`（L0）为准；本文件不复制长文、不承载权威判定。

## 1. 稳定目标（不随轮次变化）

- 天文 CCD 图像校准与标准化数据库：Phase1 单帧 light + masters/catalog/config →
  单帧标准化 IVOA HiPS + manifest；Phase2 一组合同兼容 HiPS → 马赛克 HiPS +
  UPM/rejection/integration provenance；Phase3 任一合同兼容 HiPS → 平面 FITS +
  WCS/coverage/validity/provenance（约束 §A）。
- 三 Phase 是**三个隔离产品命令**，非固定顺序流水线；跨 Phase 仅磁盘产品/manifest
  交换；禁止同进程 `--phases 1,2,3`（约束 §A.4，遗留未删见 §5）。
- 正式开发/客户端/发布平台 = **Windows x64**（Win10 22H2 下限 / Win11 主验证）；
  Windows 用户只面对 `astrocs.exe`，运行时/I/O/科学模块/CPU provider 以 DLL 交付；
  Linux amd64 仅控制/静态分析/轻量编译/小合成（约束 §B）。
- ACR = DORMANT：保留源码与隔离测试；生产构建/加载/路由/benchmark/发布不含
  ACR/CUDA；当前唯一生产计算后端是纯 CPU（约束 §C）。
- 科学定义 = 算法 = 接口 = 代码 = 测试；科学公式与默认容差不得改动（约束 §E）。
- 仅 `main` 形成正式历史；SubAgent 不直接 commit/push；最终发布裁定权只属项目
  负责人（约束 §G/H）。

## 2. 当前 SHA / 版本（GOV-005 基线，2026-09-02）

- 基线提交（本任务检出）：`6affe3009985452f5bc0bdf654aa95a4b61b2d2e`
  （`docs(owner): GOV-004 建立负责人审查入口`）
- 产品版本唯一源：根 `VERSION` = `0.11.0-alpha.1`（GOV-003；生成串
  `0.11.0-alpha.1+g<commit12>`，见 `docs/governance/VERSION_NAMESPACES.md`）。
- 文档索引基线：`docs/DOCUMENT_INDEX.yaml`（GOV-002，`base_product_version`
  已由 GOV-005 收敛为 `0.11.0-alpha.1`）。
- 权威入口：`REVIEW.md` + `docs/owner/` 5 份 L0 文档（GOV-004）。
- 主线最近提交链（GOV 系列）：GOV-001 冻结约束 → GOV-002 归档 → GOV-003 版本
  单源 → GOV-004 L0 骨架 → GOV-005（本任务，文档收敛）。

## 3. 模块索引（权威：docs/architecture/MODULE_MAP.md、docs/modules/、module.yaml）

- **产品骨架**（唯一根 CMake，BLD-002）：`astrocs` 可执行 = cli/ + 显式静态库图
  （astrocs_contracts / astrocs_core / astrocs_io / astrocs_common /
  astrocs_cfitsio 等）；`lib/core`（runtime/pipeline/module registry）、
  `lib/io`（IO-001 io_adapter）、`lib/common`（sha256/healpix）。
- **阶段会话**：`lib/phase1_session`（p1_session）、`lib/phase2_session`
  （p2_session）、`lib/phase3_session`（p3_session + wcs/resample/output/
  hips_properties）。
- **科学模块**（legacy lib/ 树，模块文档见 docs/modules/*.md）：
  astro_image_io / calibration / dynamic_psf / star_detector / plate_solve /
  photometric_calib / snr_estimator / gaia_xpsd_client / healpix_db
  （healpix_drizzle + healpix_browser_qt）/ phase2 / orchestrator / acr。
- **模块服务**：`modules/services/io`（fits_stream_v1 头 + 自测）。
- **CLI 命令**（docs/api/CLI_PROTOCOL_V1.md）：`astrocs phase1/2/3 run
  --config …`、`verify`、`benchmark cpu`、`doctor` 等；遗留
  `run --phases 1,2,3` 见 §5。
- 每模块 README.md + module.yaml + 公共头 + CMake + 共址测试是模块规范（约束
  §F.5）；docs/modules/registry/ 为机器生成模块 README（GENERATED）。

## 4. 已归档/不再活跃（history，勿当现状）

- V1–V19/V18R2/V19R2/V19R8/V19R6R2-W1 等历史轮次：只进 CHANGELOG.md 历史节与
  `docs/archive/**`、`engineering/control/archive/**`（ARCHIVED_NON_NORMATIVE）。
- 旧 11 子仓合并记录、F 盘本地路径、16 线程硬编码、Python 脚本调 DLL 时代：
  全部为失效现状，已随 GOV-005 迁 history（见 §1 归档位置）。
- 开发过程操作日志本体：`docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md`。

## 5. 开放问题（如实，不冒充已解决）

1. **`astrocs run --phases 1,2,3` 进程内连跑遗留未删** —— 冲突约束 §A.4（FAIL，
   W4 删除伪/旧路由范围；REVIEW/PIPELINE_OVERVIEW 同记）。
2. **约束 §F.1 每 DAG 节点唯一真实模块 operation 未达成** ——
   `lib/core/src/module_adapters.cpp` 各子模块 factory 当前委托同一 phaseN
   session（W3 模块化 + W4 范围）。
3. **Windows 发布执行面未完成**：DLL 化发布安装树（astrocs.exe + runtime/io/
   科学模块/provider DLL）未产出/验证；MSVC 编译/测试/32R/真实数据最终验收未跑
   （Fatduck 侧，NOT_VERIFIED）。
4. **Phase3 扩展未实现**：SIN/ZEA/CAR/AIT 投影（当前仅 TAN）、`healpix_interp4`
   （当前 nearest/bilinear）、流式 FITS 输出接入（IO-001 接口在位未接入 p3
   writer）。
5. **版本收敛遗留（他人路径，非 GOV-005 写域）**：docs/VERSIONING.md 的旧基线
   版本号字样、CMake `project(... VERSION …)` 字面量、tests/tools 硬编码旧版本
   断言 —— 已由 check_version_namespaces.py `out_of_scope` 登记，待前台/QA
   协调（GOV-005 不改他人写域；旧号段见该检查器输出与 CHANGELOG 历史节）。
6. **运行环境事实**：当前控制节点为 Linux amd64（vm-bj Debian 13），正式验证
   依赖 Windows/Fatduck 节点恢复；本提交后如需在 main 上继续，前台需 fetch 并
   验证三 SHA 一致（约束 §G.2）。
