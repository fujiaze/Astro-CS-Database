# V4 任务规格（98 原子任务）

> 每个 Task 恰好一个原子 commit 并立即 push main；同时生成 review capsule（07 规范）。
> 依赖列指示前置 task_id；测试通过后方可置 PASS。状态词与门禁详见 00/05。

## C0

### C0-001 — V4 control package authoring and self-validation

- 依赖：（无）
- 规格：编写 00_READ_FIRST/01 工作流/02 账本(98 行)/03 规格/04 检查点/05 门禁/06-10 支撑文档与 scripts 校验器；validate_control.py 必须 CONTROL_PASS。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C0-002 — repo identity and origin/main freeze

- 依赖：C0-001
- 规格：git fetch 后冻结 origin/main SHA 为 V4 唯一起点并记入台账/日志；不得假设任何历史 SHA。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C0-003 — toolchain and host inventory

- 依赖：C0-002
- 规格：Linux(gcc/版本/核数/内存)与 Fatduck(MinGW/线程/在线窗)工具链盘点落档，作为动态线程与门禁阈值输入。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C0-004 — V3 evidence carry-over registration

- 依赖：C0-002
- 规格：登记 REAUDIT_V3_REVIEWPACK_20260828T1126Z 为继承证据：TSan race 债务、层缺口、串行架构、stage1 版本漂移、BLD-002/003。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C0-005 — baseline ledger initialization check

- 依赖：C0-001
- 规格：98 行全 NOT_STARTED 核验、依赖闭合、原子 commit+push 策略生效确认。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

## C1

### C1-001 — machine checker: build_graph + config_contracts

- 依赖：C0-005
- 规格：在当前 main 运行 build_graph 与 config_contracts 检查器，0 FAIL。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C1-002 — machine checker: api_contracts + traceability

- 依赖：C0-005
- 规格：api_contracts 与 traceability 检查器 0 FAIL（含 SCI→ALG→API→SRC→TEST 链）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C1-003 — machine checker: doc_symbols + comments

- 依赖：C0-005
- 规格：doc_symbols 与 comments 检查器 0 FAIL。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C1-004 — machine checker: execution_contracts + science_units

- 依赖：C0-005
- 规格：execution_contracts 与 science_units 检查器 0 FAIL。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C1-005 — machine checker: forbidden_patterns + test_contracts

- 依赖：C0-005
- 规格：forbidden_patterns 与 test_contracts 检查器 0 FAIL。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C1-006 — hardcode scan: cores/workers/blocksize/AVX

- 依赖：C1-001
- 规格：全库扫描硬编码核心数/worker 数/block 尺寸/全局 AVX 编译参数并登记清单（C5 治理输入）；omp_get_num_threads 例外登记。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C1-007 — race-debt registry from TSan evidence

- 依赖：C0-004
- 规格：将 V3 TSan 5634 条警告归类为 race 债务清单（upm.cpp:532/577/625 等），逐项映射到 C5 修复任务。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C1-008 — static analysis checkpoint snapshot

- 依赖：C1-006
- 规格：C1 全部证据（检查器输出+扫描清单+race 债务）打包为 C1 检查点快照。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

## C2

### C2-001 — Phase3 SCI: reprojection scientific definition

- 依赖：C1-008
- 规格：Phase3=HiPS 球面分块到平面 WCS FITS 重投影的科学定义：输入(HiPS tile 层级)、输出(平面 WCS FITS 层级)、单位、量纲、精度、像素语义。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C2-002 — Phase3 SCI: tile-to-plane coverage semantics

- 依赖：C2-001
- 规格：球面 tile 覆盖到平面网格的覆盖/空洞语义定义（含 output 生成范围与投影边界）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C2-003 — Phase3 SCI: resampling kernel and flux semantics

- 依赖：C2-001
- 规格：重采样核与通量语义（最近邻/双线性的科学取值、强度守恒要求与适用条件）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C2-004 — Phase3 SCI: NaN/out-of-footprint semantics

- 依赖：C2-001
- 规格：NaN/足迹外/覆盖不足的输出语义与 validity 表示。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C2-005 — Phase3 SCI: precision contract fp32/fp64

- 依赖：C2-001
- 规格：Phase3 精度合同（fp32 主、fp64 对照阈值）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C2-006 — Phase1 SCI re-freeze on current main

- 依赖：C1-008
- 规格：Phase1 科学定义在当前 main 复核冻结（无变更则显式记录 no-change）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C2-007 — Phase2 SCI re-freeze on current main

- 依赖：C1-008
- 规格：Phase2 科学定义在当前 main 复核冻结（含 weight_mode=ivar 现行语义）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C2-008 — SCI checkpoint snapshot

- 依赖：C2-005
- 规格：C2 全部 SCI 文档快照+评审要点。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

## C3

### C3-001 — Phase3 ALG: HEALPix nested tile geometry derivation

- 依赖：C2-008
- 规格：HEALPix NESTED tile 几何推导：order/nside/tile 内像素到天球角（RA,Dec）的精确映射公式。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C3-002 — Phase3 ALG: target WCS grid derivation

- 依赖：C3-001
- 规格：目标平面 WCS 网格推导：CRVAL/CRPIX/CD 矩阵与输出尺寸的确定算法（含 preset 视场）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C3-003 — Phase3 ALG: sky-to-plane pixel mapping derivation

- 依赖：C3-002
- 规格：输出像素(x,y)→WCS→(RA,Dec)→HEALPix 输入像素的反演映射与采样位置算法。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C3-004 — Phase3 ALG: interpolation and coverage algorithm

- 依赖：C3-003
- 规格：采样插值与覆盖判定算法（与 C2-003 语义一致；含并发安全的数据访问划分）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C3-005 — Phase3 ALG: boundary and NaN algorithm

- 依赖：C3-004
- 规格：边界/NaN/覆盖不足的算法处理（与 C2-004 一致）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C3-006 — backend selection algorithm (profile→kernel map)

- 依赖：C3-005
- 规格：backend 选择算法：存在有效 profile 时选择最优 kernel 实现映射，否则 fallback baseline；禁止硬件探测猜测。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C3-007 — dynamic threading algorithm from CPU affinity

- 依赖：C3-006
- 规格：动态线程算法：从 sched_getaffinity/系统 affinity 推导线程数与分块；禁止硬编码核心数/worker/block。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C3-008 — per-kernel benchmark algorithm

- 依赖：C3-007
- 规格：逐内核 benchmark 算法：每个 kernel 的计时/统计/重复策略与 profile 文件格式（含生成与消费规则）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C3-009 — resource monitor and gate algorithms

- 依赖：C3-008
- 规格：资源监控算法（CPU%/内存/I/O/线程/阶段时间序列采样）与资源门禁判定算法（利用率阈值、异常内存增长、race、报告缺失→不 PASS）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C3-010 — ALG checkpoint snapshot

- 依赖：C3-009
- 规格：C3 算法文档快照+自洽性核对（与 SCI/ARCH 交叉引用）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

## C4

### C4-001 — single astrocs CLI contract (Win/Linux)

- 依赖：C3-010
- 规格：单一 astrocs CLI 合同：统一入口/子命令/参数/退出码/日志，Windows 与 Linux 行为一致；退役 orchestrator.exe 双入口。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-002 — in-process invocation architecture

- 依赖：C4-001
- 规格：Phase1/2/3 进程内调用架构：同一进程内函数级接线（退役 DLL 加载路径），进程边界与隔离策略。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-003 — DLL-era retirement migration plan

- 依赖：C4-002
- 规格：DLL 时代组件退役迁移方案（保留源码、移除运行时加载依赖、兼容层映射表）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-004 — amd64 CPU backend interface

- 依赖：C4-001
- 规格：amd64 私有 CPU backend 接口：kernel 抽象（注册/签名/归约）、backend 与算法实现解耦。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-005 — backend registry and selection API

- 依赖：C4-004
- 规格：backend 注册/枚举/选择 API（与 C3-006 选择算法一致；无 profile→baseline）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-006 — profile data contract

- 依赖：C4-004
- 规格：profile 数据合同：逐内核条目（kernel/维度/耗时统计/环境），文件格式与校验。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-007 — resource monitor API

- 依赖：C4-001
- 规格：资源监控器 API：采样启停、CPU/内存/I/O/线程/阶段序列、输出契约。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-008 — resource gate API

- 依赖：C4-007
- 规格：资源门禁 API：判定输入/阈值/结论(含低利用率、异常内存增长、race、缺失报告→不 PASS)。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-009 — Phase3 public API

- 依赖：C4-002
- 规格：Phase3 公共 API：重投影入口/配置结构/产物句柄，与 Phase1/2 风格一致。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-010 — Phase3 data contracts (FITS/WCS)

- 依赖：C4-009
- 规格：Phase3 数据合同：FITS header 关键字、WCS 表示、层布局与单位。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-011 — error and exit-code contract

- 依赖：C4-001
- 规格：统一错误与退出码合同（CONFIG_ERROR 等语义沿用并扩展 Phase3）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C4-012 — ARCH/API checkpoint snapshot

- 依赖：C4-008
- 规格：C4 合同文档快照+机器可核对要点（供 C6 合同测试）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

## C5

### C5-001 — astrocs CLI implementation

- 依赖：C4-012
- 规格：实现单一 astrocs CLI（Win/Linux 同源），子命令/退出码/日志落实现。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-002 — in-process Phase1 wiring

- 依赖：C5-001
- 规格：Phase1 进程内接线（去除对 DLL 运行时加载的依赖路径）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-003 — in-process Phase2 wiring

- 依赖：C5-001
- 规格：Phase2 进程内接线。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-004 — in-process Phase3 skeleton

- 依赖：C5-001
- 规格：Phase3 进程内骨架（无 stub 语义占位——仅调用结构，科学语义随后续任务实现）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-005 — CPU backend skeleton + baseline backend

- 依赖：C4-005
- 规格：amd64 CPU backend 骨架与 baseline backend（无 profile 时的默认实现）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-006 — dynamic threading per affinity

- 依赖：C5-005
- 规格：按 affinity 动态线程化实现（替换硬编码；含 Phase1/2 现有 OMP 段接入）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-007 — hardcode cleanup: cores/workers/blocksize

- 依赖：C1-006
- 规格：按 C1-006 清单清除硬编码核心数/worker/block（逐项提交核销）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-008 — AVX/global arch flag governance

- 依赖：C1-006
- 规格：全局 AVX/架构编译参数治理：改为可移植默认+可选函数级多版本；禁止全局强开。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-009 — Phase3 tile geometry core

- 依赖：C5-004
- 规格：Phase3 核心：HEALPix NESTED tile→天球几何（按 C3-001）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-010 — Phase3 WCS grid builder

- 依赖：C5-009
- 规格：目标 WCS 网格构建（按 C3-002）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-011 — Phase3 sky-to-plane sampler

- 依赖：C5-010
- 规格：球面→平面采样实现（按 C3-003/004，走 backend 抽象）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-012 — Phase3 boundary/NaN handling

- 依赖：C5-011
- 规格：边界与 NaN/覆盖不足实现（按 C3-005）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-013 — Phase3 FITS/WCS writer

- 依赖：C4-010
- 规格：平面 WCS FITS 写出（header 关键字与布局按 C4-010）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-014 — resource monitor implementation

- 依赖：C4-007
- 规格：资源监控器实现（采样器+阶段标注+序列落盘）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-015 — resource gate implementation

- 依赖：C4-008
- 规格：资源门禁实现（阈值判定+结论输出，绑定重计算入口）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-016 — per-kernel benchmark implementation

- 依赖：C3-008
- 规格：逐内核 benchmark 实现与 profile 生成/读取。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-017 — upm.cpp race fixes (532/577/625)

- 依赖：C1-007
- 规格：按 C1-007 债务清单修复 upm.cpp OMP 段数据竞争（532/577/625 优先）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-018 — race regression suite (TSan clean)

- 依赖：C5-017
- 规格：race 回归：TSan 全门禁重跑 0 新增 race（对 C6-013 的输入）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-019 — serial-segment audit on recompute paths

- 依赖：C5-006
- 规格：重计算路径串行段复核：>1s 串行段清零或列入 FAIL（规则 9）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-020 — alpha packaging script

- 依赖：C5-013
- 规格：alpha 包打包脚本（bin/资源/SHA256/manifest）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-021 — Windows build adaptation for CLI/backend

- 依赖：C5-020
- 规格：Windows(MinGW) 对 CLI/backend/Phase3 的构建适配（toolchain 扩展）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C5-022 — CODE checkpoint snapshot

- 依赖：C5-021
- 规格：C5 实现清单与 diff 摘要快照。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

## C6

### C6-001 — Phase3 independent synthetic Oracle

- 依赖：C2-008
- 规格：独立合成 Oracle：解析解/恒等映射合成数据（小网格精确期望值），独立于实现推导。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-002 — Oracle vs implementation conformance

- 依赖：C6-001
- 规格：Oracle 与实现对拍：逐像素阈值判定（数值门禁 05）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-003 — CLI contract tests

- 依赖：C5-001
- 规格：CLI 合同测试（子命令/退出码/跨平台一致）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-004 — in-process invocation tests

- 依赖：C5-004
- 规格：Phase1/2/3 进程内调用测试。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-005 — backend dispatch tests

- 依赖：C5-005
- 规格：backend 注册/选择/fallback（无 profile→baseline）测试。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-006 — dynamic threading tests

- 依赖：C5-006
- 规格：动态线程测试（affinity 模拟 1/2/N 核，行为与日志证明）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-007 — per-kernel benchmark tests

- 依赖：C5-016
- 规格：benchmark/profile 测试（生成/读取/缺 profile→baseline 路径）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-008 — resource monitor accuracy tests

- 依赖：C5-014
- 规格：资源监控精度测试（已知负载下 CPU%/内存序列可信）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-009 — resource gate trigger tests

- 依赖：C5-015
- 规格：资源门禁触发测试（低利用率/内存异常/race/缺失报告各→不 PASS）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-010 — Phase3 end-to-end synthetic test

- 依赖：C5-013
- 规格：Phase3 端到端合成测试（合成 HiPS→平面 WCS FITS→Oracle 对拍）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-011 — Phase1/2/3 full-chain synthetic test

- 依赖：C6-004
- 规格：全链合成测试（Phase1→2→3 进程内一次运行）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-012 — existing suites regression on current main

- 依赖：C6-002
- 规格：既有全部测试套件在当前 main 回归（phase2 5 + orchestrator 4，FAIL=0 除登记的平台预期）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-013 — TSan full-gate clean verification

- 依赖：C5-018
- 规格：TSan 全门禁 0 新增 race 验证（用例集与 V3 相同）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C6-014 — test checkpoint snapshot

- 依赖：C6-011
- 规格：C6 全部测试证据快照。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

## C7

### C7-001 — full target build (build.sh extended)

- 依赖：C6-014
- 规格：build.sh 扩展至全部新目标（CLI/backend/Phase3/监控/门禁），0 错 0 警。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C7-002 — alpha package build

- 依赖：C7-001
- 规格：alpha 包打包（bin+资源+manifest+SHA256）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C7-003 — alpha SHA256 and manifest registration

- 依赖：C7-002
- 规格：alpha SHA/manifest 登记入台账模板。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C7-004 — Linux full verification summary

- 依赖：C7-001
- 规格：Linux 全验证汇总（静态/科学/合成/构建/测试+资源证据）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C7-005 — review capsule generation (latest full files)

- 依赖：C7-003
- 规格：生成 review capsule（最新完整文件集，按 07 规范）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C7-006 — C7 checkpoint package

- 依赖：C7-005
- 规格：C7 检查点包（04 清单逐项核验）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

## C8

### C8-001 — Windows official build (toolchain)

- 依赖：C7-006
- 规格：Fatduck 在线后 Windows 正式构建（toolchain 扩展目标），exit 0。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C8-002 — Windows full test suite

- 依赖：C8-001
- 规格：Windows 全测试套件（C6 全部测试在 Windows 跑，FAIL=0）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C8-003 — Windows full per-kernel benchmark

- 依赖：C8-001
- 规格：Windows full benchmark（逐内核 profile 生成+资源监控全开）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C8-004 — Windows synthetic verification

- 依赖：C8-002
- 规格：Windows 合成验证（Phase1/2/3 全链+Oracle 对拍）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C8-005 — Windows small real-data verification (2R)

- 依赖：C8-004
- 规格：Windows 小真实数据验证（每板块 2 帧 R，A/B/C/D 语义不重复——仅当前候选）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C8-006 — Windows resource gate adjudication

- 依赖：C8-003
- 规格：Windows 资源门禁判定（低利用率/异常内存/race→不 PASS；串行段>1s→FAIL）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C8-007 — Windows artifact hash registration

- 依赖：C8-006
- 规格：Windows 产物 hash/manifest 登记。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C8-008 — C8 checkpoint package

- 依赖：C8-007
- 规格：C8 检查点包。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

## C9

### C9-001 — Galaxy Center 32R (current candidate, once)

- 依赖：C8-008
- 规格：当前候选唯一一次银心 32R（资源监控全开、串行段>1s 即 FAIL；禁止历史重跑——失败仅允许修复明确原因后单次重跑该项）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C9-002 — 32R resource gate adjudication

- 依赖：C9-001
- 规格：32R 资源门禁判定与数值核验（对接缝/层/竞态的证据要求）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C9-003 — release review package assembly

- 依赖：C9-002
- 规格：发布审核包组装（07 规范+全部检查点包+capsule）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C9-004 — final state freeze and ledger closure

- 依赖：C9-003
- 规格：终态冻结：账本闭合、未完成项如实 FAIL/BLOCKED、禁改。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

### C9-005 — emit AWAITING_EXTERNAL_RELEASE_REVIEW

- 依赖：C9-004
- 规格：输出 AWAITING_EXTERNAL_RELEASE_REVIEW（唯一允许的终态；不得自行宣称正式发布）。
- 完成判据：测试/验证通过 → 原子 commit → 立即 push → review capsule 更新。

