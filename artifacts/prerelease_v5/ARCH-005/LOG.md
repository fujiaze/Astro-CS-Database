# ARCH-005 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ARCH-005 行(HiPS reader/WCS/resampler/FITS writer 模块与数据结构/tile cache/跨 tile 访问/并发与内存上界; 验收=架构逐 claim 追到 ALG-007, 科学选择不藏 cache/loader); PHASE3_RESAMPLE.md(ALG-P3-001..004); THREAD_BUDGET_ARCH。

## 动作
1. 新建 docs/architecture/PHASE3_MODULE_ARCH.md: 四单元单向流(Reader→TileCache→Resampler→FitsWriter)逐单元数据结构与 ALG 权威公式映射(架构不重复定义科学公式); 跨 tile 访问(NESTED 邻域由 Resampler 计算, cache 只取放+missing 不伪造); 并发与内存上界冻结(M ≤ W·H·(4|8)+max_tiles·W²·(4|8)+常数, max_tiles 默认公式+MEM_BUDGET 显式错误码); 行带 worker pool+tile cache 互斥加载+单写者; 取消=FitsWriter 不发生; 错误回退(reader 启动前拒/运行中安全中止/落盘失败清理); §5 逐 claim 追溯表(4×ALG-P3+THREAD_BUDGET+容差)。
2. 机器门 tests/arch/test_phase3_module_arch.py 6 用例: 四单元/cache 无科学决策/内存上界+MEM_BUDGET/并发随预算+取消原子性/追溯表 4 锚/禁静默降级。

## 验证
- 全量回归 unittest **47/47 OK**(新增 6)。

## 产物
docs/architecture/PHASE3_MODULE_ARCH.md; tests/arch/test_phase3_module_arch.py; 本日志。

## PASS 判定
四模块+数据结构/跨 tile 访问/并发与内存上界齐备且逐 claim 锚定 ALG-007; 科学选择位置显式(ALG 公式, 非.cache/loader); 上界与资源门联动。ARCH-005 = PASS → ARCH 段(001..005)闭合。
