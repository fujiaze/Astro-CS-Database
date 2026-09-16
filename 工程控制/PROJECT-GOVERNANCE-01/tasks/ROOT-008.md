# 任务：ROOT-008 旧世代根目录归位（有用的迁走、没用的退役）

状态：NOT_STARTED
层：L1　依赖：ARCH-001（已完成）、INT-001（同一构建图域，需串行）
文件域互斥组：S1-A（根级旧世代目录树 + 根 `CMakeLists.txt` 的路径串）

## 背景（负责人指示）

> 「如果有用就把内容迁移到属于他的地方，而不是全都堆在根目录。」

ARCH-001 只处理了 `lib/**`；根下仍有 **5 个旧世代目录树**留在原位，职责与 `ASTROCS_DESIGN §7.1` 的目标结构重叠或无处安放。本任务把它们**归位**（`git mv` + 构建图/include 修正），**零行为变更**。

## 归位映射（逐条执行；每条都要给「依据 + 迁移后等价性证据」）

| 源 | 目标 | 依据 | 备注 |
|---|---|---|---|
| `runtime/artifact_store/**` | `lib/infrastructure/aio/runtime/artifact_store/**` | §7.1 aio = 原子产品边界 | 与 AIO-001 的 `aio_atomic_file.h` 职责相邻 |
| `runtime/io/**`、`runtime/core/**` | `lib/infrastructure/aio/io/**`、`lib/infrastructure/scheduler/core/**` | §7.1 aio / scheduler | 若某文件其实是 logging/monitoring 职责⇒改投 observability |
| `runtime/logging/**`、`runtime/monitoring/**` | `lib/infrastructure/observability/**` | §7.1 observability = 观测事件/资源门 | |
| `runtime/module_loader/**`、`runtime/registry/**` | `lib/infrastructure/pipeline/module_loader/**` | §7.1 pipeline | 若与 `lib/infrastructure/cli/**` 的会话装配更近，报告并给证据 |
| `runtime/pipeline/**` | `lib/infrastructure/pipeline/**` | §7.1 pipeline | 注意与既有同名文件冲突：**冲突即停，报告不覆盖** |
| `cli/**` | `lib/infrastructure/cli/**`（与 CLI-001 的 9 文件**统一**） | §7.1 cli；§7.3「单一 entrypoint」 | **核心裁决点**：最终唯一 entrypoint 是哪个 `main()`？给出结论 + 理由；被取代者按 §7.3 退役（不删测试，登记能力去向） |
| `providers/cpu/**` | `lib/infrastructure/benchmark/cpu/**` | §7.1 benchmark 拥有 CPU profile/ISA 选择（§6.4） | 若构建图证明它属 `shared` 内核 ⇒ 停下报告，不擅改 |
| `modules/conformance/**` | `tests/conformance/**` | 它们是**模块加载器的一致性夹具**（含 `module.yaml` + `tests/unit`），非产品模块 | `noop` 必须**显式**说明是负例夹具还是占位骨架：前者保留并接入 CI 负例，后者**删除** |
| `scripts/package_audit.py`、`scripts/validate_audit.py` | **退役（删除）** | 与已退役的审核包线重复（`tools/pack_audit_package.py`/`assemble_audit.py`/`make_capsule.py` 已 exit 2） | 恢复走 git 历史；删除前在 `SUMMARY.md` 登记能力去向 |
| `third_party/nlohmann/**` | **不动** | §7 固定目录，位置正确 | |
| `include/**` | **不动** | §7 固定目录 | |

## 迁移纪律（硬）

1. **一律 `git mv`**（保留历史）；**禁止**新建文件再删旧文件；
2. 迁移后**必须**修正所有引用：根 `CMakeLists.txt`（当前引用 `runtime/` 4 处、`providers/` 3 处、`cli/` 11 处、`modules/` 7 处、`third_party/` 6 处、`include/` 2 处）、各 `CMakeLists.txt`、`#include` 相对路径、Python/shell 的相对 repo-root 深度；
3. **相对路径语义漂移**是本类迁移的头号事故（ARCH-001 教训）：`${CMAKE_CURRENT_SOURCE_DIR}(/..)+`、兄弟引用、脚本 repo-root 深度，逐类建检查器；
4. **零行为变更**：源文件字节等同（`git status` 显示 R/RM）；
5. 冲突即停：目标位置已有同名文件时**不得覆盖**，停下报告；
6. **不得**改科学语义、`docs/science/**`、`docs/algorithms/**` 锚、`ci/**`；
7. 零 git 写（前台提交）。

## 验收门（前台独立复跑）

- [ ] `cmake -S . -B build -G Ninja` rc=0 且 `ninja -C build -k 0` **0 FAILED**；
- [ ] `ctest --test-dir build` **不低于迁移前基线**（迁移前 413/414，唯一红 `core_pipeline` = 既有 GAP-036）；
- [ ] 旧路径在**构建图**中零命中（`CMakeLists.txt` + `cmake/`；`run/` 内副本不计）；
- [ ] 逐条归位映射的**执行表**（源→目标→依据→引用修正处数→证据），**计数一致**；
- [ ] 未迁移项逐条写明理由；
- [ ] 与 INT-001 的交界（唯一 entrypoint、`lib/infrastructure/cli/**` 接线、include 注册）写成**交接清单**。

## 交付物

1. `git mv` 与引用修正改动（留工作区）；2. 执行表 + 未迁移清单；3. 构建/测试证据；4. 与 INT-001 的交接清单；5. 自证摘要。