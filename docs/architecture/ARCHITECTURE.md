# AstroCS Architecture (V5 单一 CLI 冻结版)

> ID: ARCH-ARCH-001  状态: FROZEN (V5 ARCH-002, 2026-08-28)  上游: GOV-001/ARCH-001  下游: API-001..005/ARCH-003..005/CLI-001

## 1 总览与单一入口

AstroCS 是天文 CCD 图像校准-标准化-重投影系统，发布物为**每平台恰好一个用户入口**：`astrocs` CLI（Windows/Linux amd64），Phase1/2/3 由该 CLI 进程内调用，无第二用户入口。

- **唯一生产 exe**：`astrocs`（单一 target，Windows/Linux 同名规则见 CLI-001 安装策略）。
- **被取代的旧入口（迁移冻结）**：`orchestrator.exe`（Phase1 编排 exe）、`astrocs-stage2`（Phase2 CLI）、`healpix_browser_qt`/`browser_cli`（浏览器工具）——**不再构建为发布目标**，其能力迁移如下：orchestrator 的 DllLoader/stage 编排→CLI 内嵌 pipeline driver（API-003/004 直接函数调用，无进程边界）；astrocs-stage2 的参数面→CLI 命令树（API-002 schema v1）；browser→保留源码为 tool 分类（不进发布安装规则，PRODUCTION_EXECUTION_INVENTORY exe_target=tool）。
- ACR 不接入（V5）：生产为纯 CPU 自适应 backend；`acr_route` 仅存配置守卫（ARCH-001 清单 24 处，`!=cpu/auto` 显式拒）。

## 2 分层与组件图

```text
astrocs CLI (唯一入口; parser/JSONL/exit/cancel/crash boundary — API-002)
  └── pipeline driver (Phase1/2/3 in-process; 串行控制面)
        ├── Phase1: astro_image_io → calibration → star_detector → dynamic_psf
        │           → ipv(plate solve) → photometric_calib → snr_estimator → healpix_drizzle → HiPS
        ├── Phase2: coverage → sampler → UPM → rejection → integration → HiPS 产品
        ├── Phase3: HiPS reader → order/采样(ALG-P3-003) → 反向映射(ALG-P3-002)
        │           → resample → coverage → FITS 原子写(ALG-P3-004)
        └── cpu backend (ARCH-003: C ABI loader + per-kernel dispatcher + profile/fallback)
      └── astro_image_io (FITS/XISF/HiPS/aio_upm, 唯一 I/O 层, 原子写契约)
      └── common (healpix_core 唯一 NESTED 实现; crypto/sha256 唯一 frame_id 实现)
```

## 3 Phase 数据流

- **Phase1**（单帧→标准化）：FITS/XISF 亮场+母版 → 校准(CAL) → 检测/PSF → 天文定位(WCS) → 测光定标 → 噪声模型(ivarr) → HiPS 入库；帧身份=`frame_id`(SHA-256 截断 64)。
- **Phase2**（多帧→统一产品）：coverage → control 采样(UPM) → 加性校正场 → 逐像素候选栈 → 排异(rejection) → 加权积分(integration) → signal/support → HiPS。
- **Phase3**（HiPS→FITS）：图像 HiPS(单通道/ICRS/NESTED/float) → SCI-P3 alpha 范围校验 → 反向映射+采样+coverage → TAN FITS+provenance。
- 三 Phase 经 manifest/数据文件衔接（运行目录 run/，见 §4），不共享进程外状态。

## 4 配置/manifest/artifact 生命周期

- **配置**：科学 config（用户，schema 校验）与 CPU profile（benchmark 产物，逐内核）**分离**（CLI-003）；profile 缺失→baseline 后端+动态 worker（保守合法）。
- **manifest**：每次 run 生成 run manifest（版本/输入 hash/参数/软件版本/manifest hash），输出原子落盘（tmp+rename，IO_AND_ATOMICITY.md）；Phase3 额外写 provenance 到 FITS HISTORY（ALG-P3-004）。
- **artifact**：run/ 唯一运行输出目录；失败/取消的 artifact 不落盘（帧/行带/整文件原子单元，见 ARCH-001 清单 thread_model 列）；verify 子命令复算 hash 判定 stale。

## 5 错误/取消/恢复

- **错误**：唯一模型 ERROR_MODEL.md（错误码+diagnostics JSON 事件）；输入不明确返回确定错误而非猜测（SCI-P3 §8/§9a 显式拒绝清单）。
- **取消**：JSONL cancel 事件→取消点粒度=内核定义（帧/行带/迭代/整模型/整文件，ALG 文档 5c 节已逐内核冻结）；取消即无产物（原子单元不落盘）。
- **恢复**：无断点续算（alpha 范围）；重跑=新 run 目录+新 manifest；崩溃边界由 CLI crash boundary 捕获（exit code+JSONL 事件，API-002 冻结）。

## 6 线程与执行

- 全局 thread budget 与串行 I/O/异步 pipeline/backpressure 见 ARCH-004（冻结前置）；每 kernel 预算来源=PERFORMANCE_MODEL + benchmark profile；无硬编码线程数（ARCH-001 门+AGENTS 硬约束）。
- 历史路径的执行语义存量证据：`production_call_paths_stage1/2.csv`（symbol 级）+`PRODUCTION_EXECUTION_INVENTORY.csv`（217 行）；stage2 迁移后其 ACR Dispatcher 接线标记为不可达（ACR 不接入）。

## 7 不变量（机器可验）

1. 唯一生产入口=astrocs；文档内"正式运行入口"表述唯一（测试断言）。
2. lib/ 唯一源码目录；run/ 唯一运行输出；testdata/ 只读。
3. I/O 唯一入口 astro_image_io；healpix_core/sha256 单源（B4-01）。
4. 科学语义唯一实现，oracle/reference 并存不重复 active path。
5. Phase1/2/3 全部 in-process 由 CLI 调用（无 orchestrator 进程边界）。

## 8 关联

- 任务: ARCH-002(本文件)/ARCH-003(backend ABI)/ARCH-004(thread budget)/ARCH-005(Phase3 模块)/CLI-001(单一 target)/API-001..005
- 文档: MODULE_MAP.md/DATA_FLOW.md/PIPELINE.md/ERROR_MODEL.md/THREADING_MODEL.md/OWNERSHIP_AND_LIFETIME.md/IO_AND_ATOMICITY.md/PERFORMANCE_MODEL.md
- 迁移遗留: 旧 orchestrator/stage2 文档描述保留于 git 历史；本文件为 V5 唯一权威。
