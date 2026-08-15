# AstroCS Comment Standard

## 原则

Comment 解释 WHY / SCIENCE / INVARIANT / OWNERSHIP / THREAD-SAFETY /
NON-OBVIOUS PERFORMANCE；Code 解释 WHAT；历史进 CHANGELOG/ADR/git。

## 必须注释

1. 科学公式：变量/单位 + SCI/ALG ID（完整推导放 science/algorithm docs）。
2. 非显然 invariant。
3. conservative geometry：明示 false positive allowed / false negative forbidden。
4. ownership/lifetime。
5. thread-safety（shared/thread-local/reduction）。
6. unusual numeric constant 来源。
7. 非显然性能决策。
8. workaround 必须链接 issue/ADR。

## 必须删除/迁移

production code 中禁止出现：V[0-9]+、R[0-9]+、MICROFIX、控制包、审计轮次、
骨架版本、第??号计划、"本次修复"、"历史原因如下"、"以后 Task 再做"。
允许 whitelist：API protocol version、FITS/HiPS formal version、
scientific model version。

## 禁止废话注释

删除 "// 初始化变量"、"// 遍历数组"、"// 写文件"、"// 返回成功" 等叙述性注释。

## 长度

- 普通非显然逻辑：1–4 行；数学 derivation 源码只留结论 + ID；
- 超过 8–12 行历史/推导注释优先迁文档。

## 科学代码推荐写法

```cpp
// SCI-DRZ-004 / ALG-DRZ-OVERLAP-002:
// Conservative reject; false positives are allowed, false negatives are not.
```

## 审计

每个 production 文件记录 comment_hygiene = PASS/FAIL（见
reports/v19r2/file_audit_inventory.csv）。
