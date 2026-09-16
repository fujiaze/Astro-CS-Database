# 插件文档：benchmark（CPU profile）

## 1. 职责与边界

- **职责**：`benchmark` 按 kernel 测量数值误差、吞吐、线程扩展、内存带宽、block/worker，生成机器绑定的 `cpu_profile`。
- **不是**：不做科学公式；profile 只影响并行/ISA，**不得进入科学配置**；不因代码优化改变科学结果。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §6.2（benchmark）、§8（CPU 后端与资源）
- `contracts/schemas/cpu_profile.schema.json`

## 3. 输入/输出数据合同

- **输入**：机器特征（CPU、ISA、OS、版本）、kernel 清单、配置。
- **输出**：`cpu_profile`（ISA/workers/block、provider 选择、哈希绑定机器）。
- **缓存位置：程序安装目录**；`benchmark` 直接输出 profile 到安装目录（自动生成/更新），后续运行时自动读取。
- 参考：`contracts/schemas/cpu_profile.schema.json`。

## 4. 算法与公式要点

- 测量：数值误差（相对 baseline）、吞吐、线程扩展、内存带宽、block/worker 组合；
- 选择：稳定统计（中位数/稳健汇总），**不用一次最快值**；
- profile 绑定 CPU 特征/OS/版本/provider 哈希；
- 无 profile → 保守运行（baseline ISA + 保守并行）并提示，**不阻塞**；有 profile → 按 profile 运行。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `output` | 安装目录（固定） | —— | profile 输出路径（程序直接写入） |
| `kernels` | 全部 | —— | 测量 kernel 选择 |
| `repeats` | —— | —— | 重复测量次数 |

## 6. 接口/ABI

- entrypoint：`benchmark`（直接运行，程序输出到安装目录）；
- runtime 读取安装目录 profile 决定线程/ISA。

## 7. 错误与边界

- 不支持 ISA → 记录，不崩溃；
- profile 与机器不匹配（哈希不符）→ 拒绝使用，走保守运行；
- 测量期间不干扰科学产物。

## 8. 测试与 Oracle

- profile 生成可复现（同机同版本稳定）；
- profile 哈希绑定机器验证；
- 无 profile 保守运行验证（不阻塞）；
- 不同 provider 数值等价测试。
