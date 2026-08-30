# P2-001: Phase2 old Stage2 步骤映射表

状态: **PASS** — HEAD=`fb4d023`
规则: 完整 old Stage2 每步骤映射; **ACR call 标为禁止迁移依赖** (P2-001)。

## 映射表 (old Stage2 步骤/模块 → 目标模块 → contracts)

| old Stage2 步骤/符号 | 目标模块 | 输入 → 输出 | SCI | ALG | 状态 |
|---|---|---|---|---|---|
| `stage2_common.cpp` / `p2_stage2_parse_config` (config 解析) | `astrocs.phase2.config` | config → exec config | SCI-UPM-001 | ALG-UPM-CFG-001 | ✅ 已映射 |
| `coverage.cpp` (覆盖/控制点) | `astrocs.phase2.coverage` | frame signal → coverage map | SCI-UPM-001 | ALG-UPM-COV-001 | ✅ 已映射 |
| `sampler.cpp` (采样/weight) | `astrocs.phase2.sampler` | coverage+ivar → samples | SCI-UPM-001 | ALG-UPM-SAM-001 | ✅ 已映射 |
| `upm.cpp` (UPM 拟合) | `astrocs.phase2.upm` | samples → control plane | SCI-UPM-001 | ALG-UPM-001 | ✅ 已映射 |
| `rejection.cpp` (坏帧拒斥) | `astrocs.phase2.rejection` | frames → accepted set | SCI-REJ-001 | ALG-REJ-001 | ✅ 已映射 |
| `integrate.cpp` (加权积分) | `astrocs.phase2.integrate` | accepted frames → stack | SCI-INT-001 | ALG-INT-001 | ✅ 已映射 |
| `block.cpp` (block 调度) | `astrocs.phase2.block` | chunk → per-block work | SCI-UPM-001 | ALG-UPM-BLK-001 | ✅ 已映射 |
| `async_io.cpp` (异步 I/O) | `astrocs.phase2.io` | tile reads | SCI-SCOPE-001 | ALG-P2-IO-001 | ✅ 已映射 |
| **`acr_kernels.cpp` (ACR 合成注册)** | **禁止迁移依赖** | — | — | — | ⛔ **ACR call 禁止迁移** (LEG-004 处理) |
| `cuda_bridge_stub.cpp` (CUDA 桥 stub) | 禁止迁移依赖 | — | — | — | ⛔ stub 仅编译占位 (LEG-004) |
| `p2_acr_block_eligible` (ACR block 判定) | 禁止迁移依赖 | — | — | — | ⛔ ACR 路径不进生产 (LEG-004) |

## 禁止迁移依赖声明
- `acr_kernels.cpp` 注册 `astro::compute::phase2` 内核 (ACR GPU 路径) — **禁止作为迁移依赖**;
  生产仅纯 CPU backend (AGENTS.md); ACR 树保持 dormant (ASTROCS_ENABLE_ACR=OFF)。
- `cuda_bridge_stub` 仅编译占位 (链接完整), 不含任何生产逻辑。
- 生产 heavy 配置禁止选择 workers=1 (P2-002; Runtime lease 至少 2)。

## 验证
- 每个映射目标模块存在于 lib/phase2/src/ (config/coverage/sampler/upm/rejection/integrate/block/io)
- ACR 相关文件全部标注禁止迁移 (3 项)
- 生产配置不引用 ACR (tools/check_link_scan.py acr_symbols=0)
