# noop — conformance 可加载性模块 (BLD-003 SKELETON)

> 状态：`SKELETON`（宿主骨架 G2，非完整模块实现）。本目录由 BLD-003 建立
> 独立 SHARED target 与安装/加载骨架；语义（host callback 正/负探针、
> artifact/executor/cancel/metrics 契约测试）由 ABI-005
> `noop/echo conformance module` 填充。**不得把本 SKELETON 当作 ABI-005
> 已完成的 conformance 实现。**

## 1. 标识与状态

| 项 | 值 |
|---|---|
| 模块 ID | `astrocs.conformance.noop` |
| DLL target | `astrocs_noop`（Windows `astrocs_noop.dll`；Linux `libastrocs_noop.so`） |
| module/ABI/doc revision | module 0.11.0-alpha.1 / ABI v1 / BLD-003 |
| owner | SA-BLD-02（target/布局）；语义 owner SA-ABI-03（ABI-005） |
| 状态 | `SKELETON` — 可加载性骨架；非完整实现 |

## 2. 负责范围 / 不负责

负责：
- 提供唯一导出入口 `astrocs_module_query_v1`（12 §1：module DLL 不得导出
  其他符号）的独立 SHARED target，供安全 loader（ABI-003）动态加载；
- query 握手：`host_abi` 失配 → `ACS_ERR_ABI_MISMATCH`；`out_api` =
  模块静态表；
- `describe` 返回静态 descriptor（module_id/version/build/api 引用）；
- 验证删除/替换本 DLL 后宿主无静态 fallback（BLD-003 验收失败路径）。

不负责（明确非本 SKELETON）：
- 任何 host callback 正/负语义、artifact read/write、host allocator/logger/
  metrics/cancel/executor 契约测试 → ABI-005；
- 科学公式/算法（本模块无科学含义，`SCI-NONE`/`ALG-NONE`）；
- loader/registry 实现（ABI-003/004）；CLI `modules verify` 命令（CLI-001）。

## 3. 输入与输出

无输入/输出 ports（conformance 模块；不接入 Phase DAG，仅宿主骨架加载探针）。

## 4. 合同链接

- API：`API-ABI-001`（include/astrocs/abi/module_api_v1.h，ABI-001 冻结）；
- 模块元数据合同：`contracts/config/module_dll_contract.schema.json`
  （ARC-001 冻结）与 module.yaml 字段规范（11_MODULE_SOURCE_TEST_STANDARD.md §4）；
- 安装树：`packaging/install-tree.contract.json`（BLD-003）。
- SCI/ALG/DATA：无（`SCI-NONE`/`ALG-NONE`/`DATA-NONE`）。

## 5. 实现事实

| 源 | 内容 |
|---|---|
| `src/noop_module.c` | 唯一导出 `astrocs_module_query_v1` + 静态 vtable |
| `include/astrocs/noop/types.h` | 静态标识常量（三方一致） |

导出符号（机器核对见 ABI-006）：仅 `astrocs_module_query_v1`。
当前 vtable 语义：`describe`=OK；`validate_config/plan/create/execute/inspect`
=`ACS_ERR_UNSUPPORTED`（SKELETON）；`request_cancel`=OK（空操作）；
`destroy`=空操作。

## 6. 并发与资源

SKELETON 无实例状态、无线程、无分配。ABI-005 填充后按 12 §2 生命周期与
host executor/lease 语义补齐（本文件不预先声明伪并发模型）。

## 7. 验证

- `cmake --build <build> --target astrocs_noop` → 产物
  `libastrocs_noop.so`（Linux 技术预览）；
- `nm -D` 导出表仅 `astrocs_module_query_v1`；
- 安装后删除 `modules/astrocs_noop.*` → `packaging/verify_install_tree.py`
  非零退出并明确报缺模块（BLD-003 失败路径演示）；
- 无宿主静态副本：CLI/宿主二进制符号扫描不含 `astrocs_module_query_v1`
  定义（仅动态引用/经 loader）。

## 8. 已知限制 / 未实现项

- `SKELETON`：execute/plan/create 等全部返回 `ACS_ERR_UNSUPPORTED`；
- Windows `astrocs_noop.dll` 构建/加载留 WIN-* 系列（本任务 Linux_LIGHT）；
- loader 动态加载本模块的集成测试属 ABI-003/005；本目录不含 loader。
