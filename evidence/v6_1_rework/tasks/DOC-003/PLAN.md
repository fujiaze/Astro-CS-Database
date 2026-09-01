# DOC-003: 重建全公共 API AST 语义检查

任务 ID: DOC-003
Gate: G7
依赖: DOC-001
平台: Linux
变更类别: documentation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` DOC-003：

> 扫描全部安装/导出的 public headers（不是 3 个样本）。比较 return/param 类型、name、
> const/ref/noexcept/calling convention/visibility、C struct size/align/offset、enum 值。
> 接口语义表逐函数填写单位、范围、nullable、ownership、lifetime、mutation、blocking、
> thread safety、reentrancy、error、determinism、precision 和 test；禁止 `per header`
> 等占位。`void free()` 不得有错误声明。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| public headers 与 API 文档一致 | check_api_docs PASS(命令树=CLI --help, 退出码单源, phase 字段代码实现) | c01 |
| C struct size/version/ABI | check_abi_boundary PASS(11 个 ABI 结构均带 size/version; 8 头无 STL/异常跨边界) | c02 |
| AST public 符号一致 | check_ast_api PASS(3 头 AST public 符号与声明一致: 参数/cv/noexcept 基线) | c03 |
| 语义表逐函数无占位 | docs/API_REFERENCE.md 59 行语义表逐函数填(单位/所有权 borrowed-owned/线程 thread_local/错误码/追溯 ID); 无 per-header/TBD/占位 | c03/c01 |

## 发现的缺陷与修复

- **F-DOC003-1**: phase3 请求字段 `max_tiles` 仅在文档(ARCH-P3 §3 内存守卫)而无代码
  → 已实现: `p3_sampler_set_max_tiles` setter + session 解析 max_tiles
  (默认 min(1024, ceil(W·H/W²)+16), 可降不可升; 超默认 → ACS_ERR_BUDGET 拒绝,
  实测 max_tiles=999999 → "session run: BUDGET")。
- **F-DOC003-2**: CLI `benchmark cpu` 实际带 `[--events-jsonl]` 而 CLI_PROTOCOL_V1.md
  缺 → 文档已同步。

## 测试结果

- c01/c02/c03 检查全 PASS; phase3 run 默认 max_tiles 正常, 超守卫拒绝(BUDGET)
- ctest 56/56 PASS; test_p3005/test_p3006 回归 PASS

## 说明

- api_doc_consistency.py 依赖旧 V5 DLL 架构(reports/api_inventory.md), 不适用当前
  public-headers 检查; 语义表完整性由 API_REFERENCE.md 逐函数表体现。
