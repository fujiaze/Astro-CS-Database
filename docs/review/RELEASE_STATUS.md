# RELEASE_STATUS — L0 治理评审层（发布状态）

> 文档 ID：DOC-REVIEW-RELEASE-001
> 状态：ACTIVE_INFORMATIVE（L0 治理评审汇总层）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003 唯一源；生成串
> `0.11.0-alpha.2+g<commit12>`，见 `docs/governance/VERSION_NAMESPACES.md`）
> 发布裁定权只属项目负责人（约束 §H）；L0 汇总权威 `docs/owner/RELEASE_STATUS.md`。

## 1. 一句话结论

```text
Alpha 收敛进行中：合同面（工程约束/版本单源/文档索引/ABI v1/数据产物/Runtime 图/
工具链 preset/DLL schema/FITS 流接口/CLI 协议）已冻结入 main；CLI-002 已完成
三入口隔离（run --phases 删除）；MON-004 资源门禁已接入 phaseN run 生产路径；
Windows 发布执行面与真实数据/32R 验收未完成 → NOT_READY_FOR_RELEASE。
```

## 2. 版本口径

- 产品版本唯一源：根 `VERSION` = `0.11.0-alpha.2`（V81-ADOPT-006 统一活动版本源）。
- 机器门禁：`ci/check_version.py`（活动文档 alpha 字面量必须 == 根 VERSION；
  豁免仅机器修订关系字段）、`tools/doccheck/check_version_namespaces.py`
  （product/module/ABI/data-schema/doc-revision/history 命名空间）。
- CLI 版本输出由生成链注入（`cli/version_generated.h.in` @ASTROCS_VERSION_STRING@），
  禁止手抄字面量。

## 3. 冻结面（PASS，当前提交静态可核）

| 面 | 依据 |
|---|---|
| 工程约束/文档索引/版本单源 | 根约束 + `docs/DOCUMENT_INDEX.yaml` + `VERSION`（GOV-001/002/003） |
| CLI 协议合同 v1 | `docs/api/CLI_PROTOCOL_V1.md`（API-CLI-001，与 `cli/parser.cpp` kRules 对齐） |
| 三入口隔离 | CLI-002：phase1/2/3 run 唯一 run 入口；run/graph 已删（负测 exit 2） |
| 资源门禁接线 | MON-004：`cli/resource_gate.h` evaluate_gate 进入 phaseN run（exit 10） |
| C ABI v1 / DLL schema | `include/astrocs/abi/*.h` + `contracts/config/module_dll_contract.schema.json` |
| 数据产物/交换合同 | DATA-001/002（schema/registry/validator/矩阵） |
| Runtime 类型化运行图 | RT-001（typed_dag/registry/负测） |
| Windows 工具链 preset | BLD-001（合同面）+ 唯一根 CMake（BLD-002） |
| FITS 流式接口 | IO-001（接口+实现+契约测试） |

## 4. 未完成 / 未验证面（NOT_VERIFIED，如实）

| 面 | 说明 |
|---|---|
| Windows DLL 化发布安装树 | 未产出/验证（目标 astrocs.exe + runtime/io/模块/provider DLL） |
| MSVC 编译 + 测试 / 32R / 真实数据 | Windows/Fatduck 侧执行，FINAL_REAL_DATA_VALIDATION=PENDING |
| Phase3 扩展 | SIN/ZEA/CAR/AIT、healpix_interp4、流式 FITS 接入未实现 |
| §F.1 每节点唯一 operation | factory 仍委托同一 phaseN session（W3/W4 域） |
| 各执行验收当前提交复跑 | 未复跑，不冒充（历史存档只作追溯） |

## 5. 发布 Gate 口径

- 当前未达 `READY_FOR_OWNER_REVIEW`；如实标注 **NOT_READY_FOR_RELEASE**。
- 发布 Gate 与证据链口径见 `docs/owner/RELEASE_STATUS.md` 与控制包 20_RELEASE_GATES 域。

---
authoring_task: DOC-L0
authoring_layer: docs/review (L0 governance review)
base_product_version: 0.11.0-alpha.2
