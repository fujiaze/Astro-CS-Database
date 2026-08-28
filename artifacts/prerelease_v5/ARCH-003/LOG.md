# ARCH-003 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ARCH-003 行(backend C ABI/loader/per-kernel dispatcher/profile-fallback/信任边界/错误回退; 验收=覆盖 05 全条目+C++ ABI 与任意插件路径为禁止项); 控制包 05(80 行 7 节); ARCH-002。

## 动作
1. 新建 docs/architecture/CPU_BACKEND_ARCH.md: 05 §1-§7 逐节落成架构(设计结论/编译隔离/六查+信任边界/C ABI v1 全字段/kernel 七类注册粒度/失败回退 4 条/发布检查)+§8 任务映射表(05 条目→ABI/ISA/BENCH 任务落点)。
2. 关键冻结: 六查全过才执行(CPUID/OSXSAVE/XGETBV/affinity 可用 CPU/required_features/sha256+ABI); 禁 PATH/LD_LIBRARY_PATH 注入; 异常禁跨边界; backend 禁私有线程池(host thread budget); 禁全局"AVX2 模式"(逐 kernel 注册, science_contract_id/algorithm_id 挂链); 计算中失败禁静默换 backend 混合结果。
3. 机器门 tests/arch/test_backend_arch.py 5 用例: 05 七节覆盖/C++ ABI 禁词/插件注入禁词/kernel 粒度七类+contract id/回退规则。

## 验证
- 全量回归 unittest **36/36 OK**(新增 5)。

## 产物
docs/architecture/CPU_BACKEND_ARCH.md; tests/arch/test_backend_arch.py; 本日志。

## PASS 判定
05 全条目覆盖(7 节+映射表); C++ ABI/任意插件路径禁止项以机器门固化; kernel 粒度与 SCI→ALG 链挂接; 失败回退与信任边界冻结。ARCH-003 = PASS。
