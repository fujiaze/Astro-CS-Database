# 模块 acsd.phase1.session

> 上游：docs/ACSD_DESIGN.md §8.5（模块与 ABI）
> 数据正本：docs/detail/registry/acsd.phase1.session.md（DATA-P1-SESSION 端口表，本页输入输出端口表）
> API 正本：docs/engineering/api/PUBLIC_API.md「Phase1 装配会话 C API」节
> （API-P1-SESSION）、docs/engineering/api/PUBLIC_API.md「分阶段 API 面」（API-P1-001，FROZEN）
> C ABI 与 host services 四通道：docs/engineering/api/abi/ABI.md
> I/O：docs/detail/infrastructure/17_aio.md（IO-002 canonical deleter）

Phase1 装配底座（`p1_session` 函数族）登记页。权威签名头
`lib/phase1_session/p1_session.h`，实现 `lib/phase1_session/p1_session.cpp`。
模块合同落位 `lib/phase1_session/README.md` + `module.yaml`。

**模块词汇与 registry 关系**：`acsd.phase1.session` 是 assembly 层的模块名；
现行 registry descriptor 工厂表中**无此 module_id** —— 八个 Phase1 descriptor
工厂经 `P1Api` 委托本模块的五函数（工厂注册面 =
lib/infrastructure/scheduler/src/module_adapters.cpp）。层级 = assembly（编排），
不设独立 DLL；构建 = 静态库 `acsd_phase1_session`（根 CMakeLists）。

## 职责与明确非职责

职责：Phase1 装配 / 编排 —— canonical 四段 `io_read → calibrate → cosmetic →
io_write` 的进程内会话（create / validate / run / inspect / destroy 生命周期）；
config 键集校验、取消传播、线程预算注入、manifest 产出。科学实现全部委托既有
冻结 C API（`ac_calibrate_frame` / `ac_correct_frame`，lib/algorithms/calibration）。

外部输入 = config JSON 键集（本页输入输出端口表）/ host services 四通道
（common_abi_v1.h）/ FITS·XISF 读帧 / FITS 写帧。

不做：拥有任何算法（校准 / 星点 / PSF / WCS / 测光 / SNR / drizzle / HiPS 公式
均不在本层，见各 SCI/ALG 冻结合同）；描述 Phase2 / Phase3。

**已知差距（如实登记）**：API-P1-001 冻结 7-stage 序列与现行四段（CAL+COS）
不重合 —— 现行实现不覆盖检测 / PSF / platesolve / 测光 / SNR / Drizzle / HiPS
段；补齐属迁移目标（未落地，登记面 = lib/phase1_session/README.md 的
「装配顺序与数据流（p1_session_run 实测）」段与其「已知限制（登记不改码）」段）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `config` | `DATA-P1-SESSION` | 必 | `UnitId::DIMENSIONLESS` | —— |
| `calibrated` | `DATA-P1-CAL` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

数据面 = config JSON 键集 / host services / manifest / 校准 artifact
（DATA-P1-SESSION，本页输入输出端口表）。像素输出为 float32 ADU `[h,w]`，
仅 FITS 落盘。

## 公共 header、核心 symbol 与生命周期

五导出 C API + `acsd::phase1::last_error`；合同 = API-P1-SESSION
（PUBLIC_API.md「Phase1 装配会话 C API」节）；签名权威 =
lib/phase1_session/p1_session.h。生命周期 create→validate→run→inspect→destroy。

manifest 状态机 = created → complete / failed。

### 源文件

`lib/phase1_session/p1_session.cpp`（实现）与 `lib/phase1_session/p1_session.h`
（权威签名头）。

## Registry descriptor 与配置 schema

本模块不设独立 registry descriptor，配置面 = config JSON 键集
（本页输入输出端口表），由本层校验。

## Execution class、并行轴、ThreadBudget lease、确定性

threadsafe:no（handle 级单线程）; reentrant:yes。OpenMP worker 数 =
host `budget.max_workers` 注入（禁硬编码），经 `ac_set_num_threads` 下传。B 线
registry 通道经 SessionModule ThreadLease 租借（module_adapters.cpp 的适配器
租借面）。

## 内存/cache/I-O/所有权

所有权 = handle 归创建者（唯一 create/destroy 对）；inspect 输出由 host 分配、
调用方 host 释放；`AIOImageData` 经 canonical deleter `aio_free_image_data`
（IO-002）。

## 错误、日志、指标、取消和 checkpoint

错误码 = `ACS_ERR_*` 全集（PARAM / ABI_MISMATCH / NOMEM / IO / CANCELLED /
INTERNAL …），触发锚见 docs/engineering/api/PUBLIC_API.md 的「Phase1 装配会话 C
API（API-P1-SESSION）」章内「返回码」。失败短路返回，**不留伪完整
产物**；manifest 记 `status=failed` + `error` / `error_kind`。错误码与退出码
唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。

取消传播：本层负责把宿主取消通道下传到各段。

## 独立 synthetic 验证命令与容差

测试标识 = `TEST-P1-SESSION-001`，覆盖面为 facade 语义 / canonical 四节点 / 委托；
生命周期登记覆盖 API-003。载体不在本仓可复算路径上。

## 已知限制

- API-P1-001 冻结 7-stage 序列与现行四段实现的差距（登记面 =
  lib/phase1_session/README.md 的「装配顺序与数据流（p1_session_run 实测）」段）；
- 本模块无独立 registry descriptor，module_id 只在 assembly 层成立；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
