# 注释纪律

上游：最高设计的模块与 ABI 一章[1]；代码条款见 `CODE.md`。

生产源码注释的内容面、清理面、长度面与审计面。注释解释**为什么**；代码解释**做什么**。

## 原则

Comment 解释 WHY / SCIENCE / INVARIANT / OWNERSHIP / THREAD-SAFETY /
NON-OBVIOUS PERFORMANCE；Code 解释 WHAT；历史沿革进 ADR 与 git 提交消息。

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

production code 注释的清理面 = 轮次标识（如 `V<数字>`、`R<数字>`）、`MICROFIX`、控制包、审计轮次、
骨架版本、第??号计划、"本次修复"、"历史原因如下"、"以后 Task 再做"。
允许 whitelist：API protocol version、FITS/HiPS formal version、
scientific model version。

- **管辖文件集**：本节的管辖对象是**生产源码文件内的注释**，即 `lib/`、`eng/` 下的 `.c`/`.cpp`/`.h`/`.hpp`。
  文档与台账的轮次标识**不在**本节管辖内，它们按 `CODE.md` 的机器契约保留面按字面保留；
  两个口径的边界与各自主管文件集见 `CODE.md` 的显示名与机器契约保留面一节[2]。

## 叙述性注释的清理面

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

每个 production 文件记录 comment_hygiene = PASS/FAIL，由注释卫生检查项产出并随该检查项的运行产物落盘。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md`，最高设计，第 8.5 节（模块与 ABI），上位来源。
[2] 内部文档 `docs/engineering/standards/CODE.md`，代码标准，显示名与机器契约保留面一节定轮次标识的管辖边界。