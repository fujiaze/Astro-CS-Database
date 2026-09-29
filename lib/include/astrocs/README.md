# astrocs

ACSD 公共头文件域：定义跨动态库边界的版本化 C ABI 合同与核心 C++ 接口声明。

## 职责边界

- 放：公共 C ABI 头（结构前两字段恒为 `struct_size` + `abi_version` 握手）、模块与宿主生命周期接口、Artifact 与 AIO 的 ABI 合同、core 层公共类型声明。
- 不放：任何实现代码（实现在 `lib/infrastructure/` 与 `lib/algorithms/`）、schema 机器事实源（在 `eng/contracts/`）、第三方依赖（在 `lib/third_party/`）。

## 内容

- `common_abi_v1.h` —— 公共 C ABI v1 单一头文件，C11 与 C++17 双可编译，禁 STL/异常/RTTI 跨边界。
- `abi/` —— 模块 ABI 面：`module_api_v1.h`（唯一导出入口与生命周期时序）、`host_api_v1.h`、`artifact_api_v1.h`、`lifecycle_v1.h`、`status_codes.h`。
- `contracts/artifact_abi_v1.h` —— 跨进程 Artifact 句柄与 manifest 查询 C ABI，句柄不暴露文件系统路径。
- `core/` —— 核心 C++ 合同头 24 个：`module.h`、`scheduler.h`、`pipeline.h`、`artifact.h`、`runtime.h`、`block_flow.h` 等。
- `io/` —— AIO 域接口：`aio_abi_v1.h`（唯一版本化 C ABI，状态码与 `common_abi_v1.h` 对齐）与 `io_adapter.h`。

## 上游

- 本目录不是检查器，未注册于 `eng/ci/checks.json` 的命令面；`lib/include/**` 作为改动影响面登记在 `CHK-BUILD-LINUX`、`CHK-STATIC` 等检查项的 `changed_paths` 中。
- 检查项条目见 `docs/engineering/01_CHECKS.md`，门禁分级见 `docs/engineering/03_GATES.md`。
