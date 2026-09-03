# lib/acr/ci — ACR-001 休眠构建守卫机器测试 (SA-ACR-13)

## 目的

ACR-001（04_CPU_RESOURCE_TASKS.md）机器验收：**默认/Release/Win
install/product manifest 不构建、不链接、不加载 ACR/CUDA；显式
`ASTROCS_ENABLE_ACR=ON` 只能生成隔离实验 target，不得被 release preset
接收**（冻结约束 §C.1：生产构建/加载/路由/benchmark/发布包均不含 ACR/CUDA）。

## 校验器

`check_acr_dormant.py` — 纯 stdlib 静态校验（Linux 控制节点可跑，不触发
configure/构建）：

| ID | 断言 | 检查 |
|---|---|---|
| ACK-ACR-001 | 产品 CMake 图不引入 ACR | 根 CMakeLists.txt 无 `add_subdirectory(lib/acr)` |
| ACK-ACR-002 | ACR 树只能 standalone | lib/acr/CMakeLists.txt 顶部 guard：非 standalone → FATAL_ERROR |
| ACK-ACR-003 | release preset 不接受 ON | `ASTROCS_ENABLE_ACR` option 默认 OFF；win-msvc-17.14.39-x64 / linux-control preset 冻结 OFF（合并隐藏祖先后） |
| ACK-ACR-004 | install/manifest 零 ACR | install_layout.cmake / install-tree.contract.json / astrocs.product.json 无 ACR/CUDA 条目 |
| ACK-ACR-005 | 生产源码零 ACR 引用 | cli/ core/ phase1*/ phase2_session/ phase3_session/ backend_host/ runtime/ modules/ include/astrocs 不 include ACR 头/符号（legacy cuda_bridge_stub.cpp 已排除出产品源白名单） |
| ACK-ACR-006 | 注册表拒绝 ACR | lib/core/src/module.cpp 拒绝 `astrocs.acr.*` 模块注册 |
| ACK-ACR-007 | product manifest 无 ACR unit | units 中无 acr/cuda rel_path |

运行：

```bash
python3 lib/acr/ci/check_acr_dormant.py --repo .            # exit 0 = PASS
python3 lib/acr/ci/check_acr_dormant.py --repo . --selftest # 7 负测全捕获
```

## 机器级验证（真实 configure，证据在返回包 logs/）

```bash
# 默认（Release）根 configure → target graph 无 ACR target
cmake -S . -B build/acr001-default -DCMAKE_BUILD_TYPE=Release
# 显式 ON 根 configure → 仍无 ACR target（隔离实验 target 只能独立构建）
cmake -S . -B build/acr001-on -DCMAKE_BUILD_TYPE=Release -DASTROCS_ENABLE_ACR=ON
# 显式 ON 隔离实验 target（lib/acr 自身为 -S 根，guard PASS）
cmake -S lib/acr -B build/acr001-exp -DCMAKE_BUILD_TYPE=Release -DASTROCS_ENABLE_ACR=ON
# 负测：产品根 add_subdirectory(lib/acr) → FATAL（lib/acr CMakeLists guard）
```

target graph 用 `cmake --build <dir> --target help` 或
`cmake --graphviz` 提取并 grep（`acr` 零命中）。

## 范围

- 本目录属 SA-ACR-13 写域（`lib/acr/ci/**`）。
- 不修改生产 CMake/源码（SA-BLD-02/相应 owner 写域）；守卫落位 ACR 树自身 +
  本校验器。ACR 源码/接口/隔离测试保留原样（LEG-004 dormant）。
