# ARCHITECTURE_OVERVIEW — L0 治理评审层（架构）

> 文档 ID：DOC-REVIEW-ARCHITECTURE-001
> 状态：ACTIVE_INFORMATIVE（L0 治理评审汇总层）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003 唯一源）
> 权威：`AstroCS_ENGINEERING_CONSTRAINTS.md` §B/C/D/F、`docs/contracts/ARCH-001.md`、
> `docs/architecture/`、`include/astrocs/abi/`、`contracts/config/module_dll_contract.schema.json`、
> L0 汇总权威 `docs/owner/ARCHITECTURE_OVERVIEW.md`（GOV-004）。

## 1. 平台与发布形态（约束 §B）

- 正式开发/客户端/发布平台 = Windows x64（Win10 22H2 下限 / Win11 主验证）；
  Linux amd64 仅控制/静态分析/轻量编译/小合成（§B.3），不得反向塑造架构。
- Windows 用户只面对 `astrocs.exe`；runtime/io/科学模块/CPU provider 以 DLL 交付（§B.4）。
- 现状：合同面已冻结（`CMakePresets.json` BLD-001、唯一根 CMake BLD-002、
  C ABI v1 `include/astrocs/abi/*.h`、DLL 边界 schema ARC-001）；
  **Linux 技术预览安装面已 INSTALLED**（`cmake/install_layout.cmake` 五科学模块 +
  `packaging/astrocs.product.json` units=10 + 安全 loader 实测 64/64）；
  Windows 安装树与 MSVC/32R/真实数据验收未完成（NOT_VERIFIED，不宣称 Windows 已交付）。

## 2. ACR 状态（约束 §C）

- ACR = DORMANT：保留源码与隔离测试（`lib/acr/`），生产构建默认排除
  （根 `CMakeLists.txt` option `ASTROCS_ENABLE_ACR` 默认 OFF，不 add_subdirectory(lib/acr)；
  preset 强制 OFF），不加载、不路由、不发布。
- 当前唯一生产计算后端是**纯 CPU**（§C.2）；CPU 自适应/线程/block/ISA 参数由
  `schemas/cpu_profile.schema.json`（profile 契约）与 `--cpu-profile` /
  `config show-effective --cpu-profile` 链承载（CLI-004 域），
  线程/ISA/block 逐内核 benchmark 选择，禁止硬编码（约束 §D.3）。

## 3. Runtime 与资源（RT-001 / 约束 §D）

- 唯一生产 Runtime：`astrocs_core`（`include/astrocs/core/runtime.h`、
  `lib/core/src/runtime.cpp`），load_pipeline→run→cancel→inspect。
- 类型化运行图：`runtime/pipeline/typed_dag.py` + `typed_dag.schema.json` +
  `module_ports.registry.json`（跨 Phase edge 拒绝）。
- 资源纪律：重计算禁止单线程、自动资源监控；MON-003/004 资源门禁
  （`cli/resource_gate.h`）已接入 phaseN run 生产路径，失败 exit 10；
  模块不得私建永久线程池/硬编码核数（合同）。

## 4. 依赖方向（ARCH-001 §3）

冻结方向：`cli → runtime → registry → modules → cpu_backend`；`io → data_contracts`；
禁反向依赖与 `file(GLOB)`。DLL 边界/C ABI 所有权规则见
`docs/standards/{C_ABI_STANDARD,API_STANDARD,CONCURRENCY_STANDARD,ERROR_HANDLING_STANDARD}.md`
与 `docs/api/COMMON_ABI_V1.md`。

## 5. 状态汇总

```text
Windows 优先合同面:      CONTRACT_READY（preset + 唯一根 CMake + ABI v1 + DLL schema）
Linux 技术预览安装面:    INSTALLED（5 科学模块 + noop / 10 units / loader 64/64 实测）
Windows 发布执行面:       NOT_VERIFIED（Windows 安装树/MSVC/32R/真实数据）
ACR:                    DORMANT（生产排除；纯 CPU 后端）
唯一 Runtime/typed DAG:  IMPLEMENTED（合同 + 源码 + 实测）
资源门禁生产接线:         IMPLEMENTED（RT-001 冻结阈值 + MON-004 phaseN run 接线）
遗留 run --phases 连跑:   已删除 IMPLEMENTED（CLI-002；实测 exit 2）
§F.1 每节点唯一 operation: IMPLEMENTED（P1 8 / P2 7 / P3 5 节点；ctest 实测）
```

---
authoring_task: DOC-L0
authoring_layer: docs/review (L0 governance review)
base_product_version: 0.11.0-alpha.2
