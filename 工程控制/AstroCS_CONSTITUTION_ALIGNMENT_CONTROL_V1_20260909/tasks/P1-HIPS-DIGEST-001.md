# P1-HIPS-DIGEST-001｜p1hips tree_digest 多 HDU 归一化修复（负责人裁决 3，选择 A）

## 背景（owner 裁决 2026-09-09）
64c1e988 的 CHECKSUM/DATASUM 时间戳归一化只扫第一个 END 卡；Moc.fits 等多 HDU 产品第二头区的 CHECKSUM/DATASUM 卡未被覆盖，秒级时间戳仍入哈希 → asan 树 `p1hips_properties nt=4` 3/3 稳定假败（主树快速同秒通过）。确定性测试假失败会污染 sanitizer/deep CI 证据，须独立小提交闭环。

## 目标
`lib/astro_image_io/tests/p1hips/p1hips_tests_units.cpp` 的 `fits_header_end`/归一化逻辑：**正确遍历全部 FITS HDU**（多 HDU 逐头区扫描），不得只扫第一个 END。

## 写入白名单
- `lib/astro_image_io/tests/p1hips/`（仅测试域；生产源零改动）

## 硬性要求
1. 只归一化明确允许变化的 CHECKSUM、DATASUM 及其时间相关注释；**科学 payload 必须逐字节敏感**。
2. 验收覆盖：
   - 单 HDU 与多 HDU fixture；
   - 主树与 ASan 树；
   - 强制跨秒（sleep 2 间隔）；
   - **连续至少 10 轮**全绿；
   - 修改科学 payload 后 digest 必须变化（敏感性负向验证）；
   - 修改白名单时间字段后科学 payload digest 必须保持一致（稳定性验证）。
3. 单独提交，不与 P2、P3 或 P1 节点化混合。
4. 生产源零改动；其他域同款问题维持登记不越界。

## 纪律
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone。
- SubAgent 不 git add/commit/push，不修改 TASK_LEDGER。
- 所有外部命令带 timeout 存日志；未运行不得写 PASS。

## 验收
- write_scope 零越界；上述验收矩阵全绿（10 轮 × 主树/asan × 单/多 HDU × 敏感/稳定双向）。
- 前台复跑后才可 PASS；一个任务一个原子 commit 并 push main。

## 返回证据
schema 要求 scope/acceptance/provenance + artifacts（10 轮日志、asan 树日志、敏感/稳定双向验证输出）；不得伪造 PASS。
