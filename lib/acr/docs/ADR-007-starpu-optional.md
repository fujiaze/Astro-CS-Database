# ADR-007: StarPU 评估 only，不强制引入

| 项目 | 值 |
| --- | --- |
| 状态 | Evaluated (Not Adopted for v1) |
| 日期 | 2026-08-02 |
| 依赖锁 | `lib/acr/docs/dependency-lock.json` |
| 版本 | StarPU `not_fetched`（commit `none`，PoC only） |
| 许可证 | LGPL-2.1 |
| 平台 | Linux（Windows 支持有限） |
| 形态 | 编译型库 |

## 状态

Evaluated, Not Adopted for v1。StarPU 仅作为评估对象，不作为 ACR v1 的硬依赖。第一版即使不采用也记录本 ADR 说明未采用原因；若未来采用，只能作为可选 adapter，公共 API 不绑 StarPU。

## 背景

StarPU 是经典的动态任务运行时，核心能力是**动态性能模型**（codelet 性能预测）与**动态调度**（运行时根据历史性能选择执行设备）。ACR 的控制包已冻结设计范式为"**离线标定 + 固定路由 + 运行时工作保持**"：

- 离线标定：性能数据在 Qualification 阶段一次性测量产出。
- 固定路由：路由表基于标定结果离线生成，运行时不重新决策。
- 运行时工作保持：运行时只按路由表执行，不做动态调度。

这一范式与 StarPU 的动态调度哲学存在根本冲突。但 StarPU 的 worker mask、scheduler 抽象在工程上有参考价值，需评估其能否作为承载固定路由的可选 adapter。

此外 StarPU 的 Windows 支持有限（主要为 Linux），而 ACR 必须支持 Windows（用户主开发机为 Windows + MSYS2）。LGPL-2.1 许可证对动态链接无传染风险，但静态链接需谨慎。

## 决策

1. StarPU 在 ACR v1 中**不强制引入**，作为评估 only。
2. 要求产出独立 PoC 报告与 ADR，评估：
   - StarPU worker mask / scheduler 能否承载 ACR 固定路由（而非动态调度）
   - Windows / Linux / CUDA / HIP 部署成本（尤其 Windows 可行性）
   - 公共 API 不绑 StarPU 的隔离成本
3. **公共 API 不绑 StarPU**：即使未来采用，StarPU 仅作为内部 adapter，公共接口签名无 StarPU 类型。
4. 第一版（v1）即使不采用 StarPU，也必须记录本 ADR 与未采用原因。
5. 若未来采用，只能作为**可选 adapter**，通过 `ACR_BUILD_STARPU_ADAPTER=ON` 启用，默认 OFF。

## 理由

- ACR 范式已冻结为静态路由，StarPU 的动态调度优势在 ACR 场景下无法发挥。
- Windows 支持有限，与 ACR 跨平台要求存在张力。
- 强制引入会绑死用户于 LGPL-2.1 库，增加部署复杂度。
- 但 StarPU 的 worker mask / scheduler 抽象有工程参考价值，完全否定为时过早，需 PoC 评估。
- 保留可选 adapter 路径，为未来范式演进留余地。

## 集成边界

- **v1 集成边界**：不集成。`dependency-lock.json` 中 StarPU `not_fetched`，构建不拉取。
- **未来可选 adapter 边界（如采用）**：
  - 仅在 `ACR_BUILD_STARPU_ADAPTER=ON` 时编译，默认 OFF。
  - StarPU 类型不得出现在公共 API 签名。
  - CPU-only 构建不依赖 StarPU。
  - adapter 必须能被替换为 ACR 自有的静态路由执行器，保证可移除性。
- **PoC 边界**：PoC 代码不进入主线，独立分支或独立仓库，结论写入 PoC 报告。

## 替代方案

1. **不引入 StarPU（默认选择）**：
   - 采用：ACR v1 默认走此路径，固定路由由 ACR 自有执行器承载。
   - 理由：与冻结范式一致，无新依赖，跨平台无障碍。
2. **强制引入 StarPU 作为运行时**：
   - 未采用：与冻结范式冲突；Windows 支持有限；绑死 LGPL 依赖。
3. **引入 StarPU 作为默认 adapter**：
   - 未采用：增加部署复杂度，v1 价值不明确；待 PoC 结论后再议。

## 未采用原因（v1）

- ACR 范式已冻结为静态路由，StarPU 动态调度优势无法发挥。
- Windows 支持有限，与跨平台要求存在张力。
- 强制引入增加部署复杂度，v1 收益不明确。
- PoC 尚未完成，无法证明 worker mask / scheduler 能优雅承载固定路由。

## 验收实验

由于 StarPU 为评估 only，验收实验针对 **PoC 报告**而非主线代码：

| 实验 | 目标 | 通过条件 |
| --- | --- | --- |
| PoC 报告产出 | 评估 StarPU 承载固定路由可行性 | 报告含 worker mask / scheduler 适配方案、Windows 部署成本、API 隔离成本 |
| 不强制 | v1 默认构建不依赖 StarPU | `ACR_BUILD_STARPU_ADAPTER=OFF`（默认）时构建无 StarPU 引用 |
| CPU-only 不依赖 | CPU-only 构建无 StarPU | 无 StarPU 头文件引用，无链接 |
| 公共 API 不绑 | 公共头文件无 StarPU 类型 | 即使 adapter 启用，公共签名无 StarPU 类型 |

PoC 报告与实验日志写入 `run/logs/acr/starpu_poc/<YYYYMMDD>/`。
