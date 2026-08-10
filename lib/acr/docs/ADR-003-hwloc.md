# ADR-003: hwloc 2.11.2 拓扑/NUMA/PCI 探测

| 项目 | 值 |
| --- | --- |
| 状态 | Accepted |
| 日期 | 2026-08-02 |
| 依赖锁 | `lib/acr/docs/dependency-lock.json` |
| 版本 | hwloc hwloc-2.11.2（commit `hwloc-2.11.2`） |
| 许可证 | BSD-3-Clause |
| 平台 | Windows / Linux |
| 形态 | 编译型库 |

## 状态

Accepted。hwloc 作为 ACR 唯一的硬件拓扑、NUMA 与 PCI 局部性探测来源。所有亲和性建议、NUMA 距离、GPU PCI 映射均以 hwloc 报告为准。

## 背景

ACR 需要为标定与路由提供准确的硬件拓扑：package / core / PU / cache 层级、NUMA 节点距离、GPU 与 CPU 之间的 PCI 局部性。这些信息决定了 kernel 的亲和性绑定、内存分配策略（NUMA 本地）与 GPU 路由选择。`std::thread::hardware_concurrency` 只给 PU 数量，无 NUMA、无 cache 层级；手写 cpuid 不可移植且无法获取 NUMA/PCI 信息。

hwloc 跨 Windows/Linux 一致，但 OS 暴露的信息存在差异（例如 Windows 上 NUMA 距离可能不全），且 GPU OS device 依赖可选组件（如 CUDA backend）。hwloc 只报告拓扑，**不报告带宽**——带宽需 ACR 标定阶段实测。

## 决策

1. 引入 hwloc 2.11.2 作为 ACR 拓扑/NUMA/PCI 探测的唯一来源。
2. 枚举层级：package → core → PU，附带 cache 层级（L1/L2/L3）与 NUMA 节点。
3. GPU PCI 局部性：通过 hwloc 的 PCI bridge 与 OS device 子树映射 GPU 到 NUMA 节点。
4. 亲和性建议由 hwloc `cpubind` API 提供，ACR 不自行计算掩码。
5. 拓扑报告序列化为 JSON，作为设备指纹的一部分。

## 理由

- hwloc 是 OpenMPI/PMIx 等社区的事实标准，跨平台一致性高。
- BSD-3 许可证宽松，无传染风险。
- 提供从 package 到 PU 的完整层级，含 cache 共享关系，决定 kernel 分块策略。
- NUMA 距离直接决定内存放置策略，避免跨 socket 访问。

## 集成边界

- **职责内**：枚举 package/core/PU/cache/NUMA、GPU PCI 局部性、亲和性建议、topology report 序列化。
- **职责外**：带宽测量（由 ACR 标定阶段实测）、CPU 利用率监测（由 ACR 监测层读 OS 计数器）、GPU 内部拓扑（SM 数等，由 CUDA/HIP runtime API 提供）。
- **降级边界**：当 hwloc 报告信息缺失（OS 限制、可选组件未装）时，ACR **降级而非拒绝**：缺失字段以 `null` 写入指纹，baseline 路由仍可用，不阻塞构建或运行。
- **API 边界**：`hwloc_topology_t` 等 hwloc 类型不得出现在公共 API 签名中。
- **构建边界**：hwloc 缺失时 ACR 不允许构建失败（参见 ADR-009 的降级策略）；但本 ADR 假定 hwloc 始终可用，缺失降级路径在 ADR-009 与构建脚本中处理。

## 替代方案

1. **`std::thread::hardware_concurrency`**：
   - 未采用：仅返回 PU 数量，无 NUMA、无 cache 层级、无 PCI。
2. **手写 cpuid 解析**：
   - 未采用：不可移植（ARM/RISC-V 无 cpuid）、无法获取 NUMA 距离、维护成本极高。
3. **`lscpu` / `lspci` 命令解析**：
   - 未采用：Linux 专属，Windows 不可用；输出格式不稳定，解析脆弱。
4. **直接读 `/sys/devices/system/cpu/`**：
   - 未采用：Linux 专属，无 PCI 局部性，无 Windows 支持。

## 未采用原因

所有替代方案在跨平台、信息完整性、可维护性上均不及 hwloc；命令解析与 `/sys` 方案在 Windows 上完全不可用。

## 验收实验

| 实验 | 目标 | 通过条件 |
| --- | --- | --- |
| 拓扑序列化 | hwloc → JSON | 含 package/core/PU/cache/NUMA，schema 校验通过 |
| NUMA 本地/远端 | 跨 NUMA 访问延迟 | 本地访问比远端快 ≥1.5x（实测，非 hwloc 报告） |
| PCI 映射 | GPU ↔ NUMA 节点 | 每个 GPU 报告归属 NUMA，与 `nvidia-smi topo -m` 一致 |
| 无 hwloc 降级 | 模拟 hwloc 缺失 | baseline 路由仍可运行，指纹含 `null` 字段，不崩溃 |

实验日志写入 `run/logs/acr/hwloc/<YYYYMMDD>/`。
