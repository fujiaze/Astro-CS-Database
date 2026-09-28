# Astro Celestial Sphere Database（ACSD）架构

> 上游：ASTROCS_DESIGN.md §8（软件架构）

## 1 总览与单一入口

ACSD 是天文 CCD/CMOS 图像校准与标准化数据库系统，发布物为**每平台恰好一个用户入口**：`acsd` CLI（Windows 交付名 `acsd.exe`，Linux 为 `acsd`；amd64）。该唯一入口按命令**拉起对应阶段的调度器**：一次 CLI 调用只驱动一个阶段，normalize / mosaic / export 三个阶段各自实例化自己的调度器与内存管线，互不共享内存、会话或运行时状态。

- **唯一生产入口**：`acsd`（Windows 交付名 `acsd.exe`；单一 target，产品清单 unit `PLATFORM-CLI`）。**不新增第二入口或第二套命令树**；Phase1 编排、Phase2 阶段工具与 HiPS 浏览器均属非入口工具面，其能力分别由 CLI 内嵌 pipeline driver、`astrocs-stage2` 工具与浏览器源码承载（工具分类与生产可达性登记面 = `PRODUCTION_EXECUTION_INVENTORY.csv`）。命令树与机器可读接口 = `docs/api/CLI_PROTOCOL_V1.md`。
- ACR 不接入：生产为纯 CPU 自适应 backend；`acr_route` 仅存配置守卫，非 cpu 值显式拒或回退 cpu（ACR 隔离条款见 `docs/contracts/ARCH-001.md` §6、最高设计 §1.4）。

## 2 分层与组件图

```text
acsd CLI (唯一入口; parser/JSONL/exit/cancel/crash boundary — docs/api/CLI_PROTOCOL_V1.md)
  └── 阶段调度器分发（一次 CLI 调用只驱动一个阶段；串行控制面）
        ├── Phase1（normalize）: astro_image_io → calibration → cosmetic/validity → background/noise
        │           → platesolve(WCS) → star_detection → psf → photometry → noise_snr → drizzle → 产品验证 → HiPS
        ├── Phase2（mosaic）: coverage → sampler → UPM → rejection → integration → HiPS 产品
        ├── Phase3（export）: HiPS reader → order/采样 → 反向映射 → resample → coverage → FITS 原子写
        └── CPU 后端能力层（docs/architecture/CPU_BACKEND_ARCH.md: C ABI loader
              + per-kernel dispatcher + profile/fallback；供三个阶段按 profile 选路）
      └── astro_image_io（target astrocs_aio；FITS/XISF/HiPS/aio_upm 唯一 I/O 层，原子写契约）
      └── common（target astrocs_common；healpix_core 唯一 NESTED 实现、crypto/sha256 唯一 frame_id 实现）
```

算法模块在 `lib/algorithms/` 下并联放置（最高设计 §8.4）；CPU 后端是三个阶段共用的能力层，不是第四个执行阶段。

## 3 Phase 数据流

- **Phase1**（单帧→标准化）：FITS/XISF 亮场+母版 → 校准(CAL) → 坏点/有效性 → 背景/噪声 → 天文定位(WCS/platesolve) → 星表引导检测 → PSF → 测光定标（同一步内施加归一化） → 噪声/SNR(ivarr) → 球面重采样(drizzle) → 产品验证 → HiPS 入库（节点序唯一正本 = 最高设计 §4.2：解算在前、检测与 PSF 建模在后）；帧身份=`frame_id`（truncated-64 SHA-256；正本 = `docs/contracts/DATA_SEMANTICS.md` §5，术语见 `docs/GLOSSARY.md`）。测光定标口径正本 = `docs/science/PHOTOMETRY.md`，噪声模型正本 = `docs/science/NOISE_MODEL.md`。
- **Phase2**（多帧→统一产品）：coverage → control 采样(UPM) → 加性校正场 → 逐像素候选栈 → 排异(rejection) → 加权积分(integration) → signal/support → HiPS。
- **Phase3**（HiPS→FITS）：图像 HiPS(单通道/ICRS/NESTED/float) → SCI-P3 alpha 范围校验 → 反向映射+采样+coverage → TAN FITS+provenance。
- 三 Phase 经 manifest/数据文件衔接（**产品落块级 `output_dir`；`run/` 只放开发与 CI 过程产物**，见 §4），不共享进程外状态。

## 4 配置 / manifest / artifact 生命周期

- **配置**：科学 config（用户，schema 校验）与 CPU profile（benchmark 产物，逐内核）**分离**（配置分离条款 = `docs/contracts/CONFIG_CONTRACT.md`）；profile 缺失 → baseline 后端 + 动态 worker（保守合法）。
- **manifest**：每次 run 生成 run manifest（版本/输入 hash/参数/软件版本/manifest hash），输出原子落盘（tmp+rename，见 `IO_AND_ATOMICITY.md`）；Phase3 额外写 provenance 到 FITS HISTORY。
- **artifact**：**产品只落块级 `output_dir`**（最高设计 §10）；`run/` 只放开发/CI 过程产物与过程日志，与运行日志不互替；失败/取消的 artifact 不落盘（帧/行带/整文件原子单元，见 `PRODUCTION_EXECUTION_INVENTORY.csv` `thread_model` 列）；verify 能力由 **doctor 的机器旗标 `--run-manifest`** 承载（唯一命令树无独立 verify 命令，`verify*` 别名按未知命令返回 rc=2；旗标落位见 `lib/infrastructure/cli/command_tree.h`，复算 hash 判定 stale）。

## 5 错误 / 取消 / 恢复

- **错误**：唯一模型 `ERROR_MODEL.md`（错误码 + diagnostics JSON 事件，退出码唯一源 = `lib/infrastructure/cli/exit_codes.h`，表见最高设计 §7.2）；输入不明确返回确定错误而非猜测（显式拒绝清单 = `docs/science/algorithms/PHASE3_RESAMPLE.md` §9a）。
- **取消**：JSONL cancel 事件 → 取消点粒度 = 内核定义（帧/行带/迭代/整模型/整文件，逐内核冻结于 `docs/science/algorithms/` 各篇「SIMD 安全与取消点」节）；取消即无产物（原子单元不落盘）。
- **恢复**：无断点续算（alpha 范围）；重跑 = 新 run 目录 + 新 manifest；崩溃边界由 CLI crash boundary 捕获（exit code + JSONL 事件，见 `docs/api/CLI_PROTOCOL_V1.md`）。

## 6 线程与执行

- 全局 thread budget、两轴分配与执行架构见 `THREAD_BUDGET_ARCH.md` 与 `THREADING_MODEL.md`；每 kernel 预算来源 = `PERFORMANCE_MODEL.md` 的规范常数 + benchmark profile；无硬编码线程数（模块边界合同 = `docs/contracts/ARCH-001.md`；工程硬约束 = `AGENTS.md` §6）。
- 执行语义存量证据：`production_call_paths_stage1.csv` / `production_call_paths_stage2.csv`（symbol 级）与 `PRODUCTION_EXECUTION_INVENTORY.csv`（由 `eng/tools/arch/build_production_execution_inventory.py` 从当前提交导出）。ACR 的口径**按表分列**（两表当前不一致，属登记缺口）：`production_call_paths_stage2.csv` 的 `acr_routing` 行为 `DORMANT`（「生产构建/加载/路由/benchmark/发布不含 ACR/CUDA」）；而 `PRODUCTION_EXECUTION_INVENTORY.csv` 中 `lib/infrastructure/acr/**` 的 `production_reachable` 列有 45 行为 `yes`（该列口径 = 符号可达，不等价于「生产接线」）。本条不把任一表外推为「ACR 不可达」的全局断言。

## 7 不变量（机器可验）

1. 唯一生产入口 = `acsd`；文档内「正式运行入口」表述唯一（机器门覆盖 `eng/tests/cli/` 与 `docs/api/CLI_PROTOCOL_V1.md` 的命令树一致性）。
2. `lib/` 唯一源码目录；**产品落块级 `output_dir`，`run/` 只放开发与 CI 过程产物**（最高设计 §10）；`testdata/` 只读。
3. I/O 唯一入口 `astrocs_aio`；`healpix_core` 与 `crypto/sha256` 单源（target `astrocs_common`）。
4. 科学语义唯一实现，oracle/reference 并存、不重复 active path。
5. 唯一 CLI 入口在进程内（in-process）按命令拉起对应阶段的调度器：一次调用只驱动一个阶段，三个阶段各自实例化调度器与内存管线，无跨阶段进程边界。

## 8 关联文档

`MODULE_MAP.md`、`DATA_FLOW.md`、`PIPELINE.md`、`ERROR_MODEL.md`、`THREADING_MODEL.md`、`THREAD_BUDGET_ARCH.md`、`OWNERSHIP_AND_LIFETIME.md`、`IO_AND_ATOMICITY.md`、`PERFORMANCE_MODEL.md`、`CPU_BACKEND_ARCH.md`。

架构条款以最高设计 §8 为准，科学/算法条款以 `docs/science/` 与 `docs/science/algorithms/` 为准；本文件与上游冲突时以上游为准并按 `AGENTS.md` §8 处置。
