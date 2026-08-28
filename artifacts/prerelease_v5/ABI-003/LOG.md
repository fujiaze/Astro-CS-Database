# ABI-003 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ABI-003 行(实现最低 amd64 baseline 的所有生产 kernel;每 kernel 外层按有效 affinity 多线程;验收=baseline opcode scanner+>=2 CPU compute synthetic 观察 >=2 active threads+Oracle PASS); 05 §2/§5/§6; baseline_kernels.h(ACS_KOP_* 缓冲合同, 本任务冻结)。

## 动作
1. lib/backend_host/baseline_kernels.h: v1 通用缓冲参数合同 acs_baseline_params_v1(head/op/w/h/k/aux/四入二出 span/workers_used)+12 op 标签(逐 op 公式注释)。
2. lib/backend_host/baseline_kernels_impl.inc: 12 生产 kernel 全实现——calibration(像素变换 (light−bias−k·dark)·gain)/noise(栈式 median+MAD·1.4826)/psf(高斯批量)/drizzle overlap(双线性覆盖权重)/accumulate(固定下标序加权和)/normalize(w>eps)/UPM SpMV(CSR 固定列序)+residual+weight-update(floor 钳制)/rejection(MADσ>k 计数)/integration(固定序加权均值)/HiPS bulk(双线性重采样, 边缘 clamp)。
3. 并行外层 run_banded: 连续行带切分+host budget 全或无租借(Σ≤max_workers, 失败减半, 1=串行兜底); per-call std::thread(无持久池, 调用后 join); 输出元素间独立=无跨线程归约→确定性。workers_used 写回+getter。kernel 表 12 条目 fn→分发器(参数校验/取消点=调用边界/UNSPECIFIED op→UNSUPPORTED)。检查器登记: baseline_backend.cpp 线程创建=budget 驱动(ARCH-004 REGISTERED)。
4. 验收门 1 baseline opcode scanner: tools/check_baseline_opcodes.py(objdump 反汇编禁 v-前缀 VEX 助记符/ymm/zmm 寄存器; SSE2 基线允许)。
5. 验收门 2/3: tests/backend/kernel_oracle_main.cpp(12 op 固定 LCG 合成数据, budget=1 与 4 双跑)+tests/backend/test_abi_kernels.py 5 测试: 多线程观测(budget=4→workers_used≥2, budget=1→1)/确定性双跑逐位相等/Oracle(Python 独立参考实现全 12 op 逐值比对, 容差 2e-4 相对; median/spmv/计数=精确)/opcode 扫描 PASS/预算耗尽串行兜底。
6. 同步: ABI-001 selftest 断言更新(kernel null params→PARAM, 因 baseline 实现后不再是 UNSUPPORTED stub)。

## 验证
- 全量回归 unittest **121/121 OK**(新增 5; 全套 70s)。
- 三验收门: opcode 扫描 PASS(无 VEX/ymm/zmm)/≥2 CPU 观测 workers_used≥2 且 budget=1 串行/Oracle 12 op 独立参考全 PASS+确定性 DET OK。

## 限制与遗留
- Fatduck MSVC 交叉验证: 节点持续离线(ssh 超时), ABI-001/002/003 Windows 侧编译+运行列入 WIN/FAT 域复验(源码严格 C++17, 已有 cl 兼容先例)。
- baseline kernel 为"合成缓冲合同"实现(ACS_KOP_* 泛型缓冲); V4 模块真实科学语义经 API-003..005 会话层调用现有模块接线(CODE-P* 域), 本任务交付 backend 边界内核+并行外层+三验收门。
- SPMV 列索引经 f32 承载(v1 合成合同), 真实 UPM 走 phase2 模块(API-004)。

## 产物
lib/backend_host/{baseline_kernels.h,baseline_kernels_impl.inc}; tests/backend/{kernel_oracle_main.cpp,test_abi_kernels.py}; tools/check_baseline_opcodes.py; 本日志。

## PASS 判定
baseline 全 12 生产 kernel 实现+affinity/budget 驱动并行外层; 三验收门(opcode scanner/≥2 线程观测/Oracle PASS+确定性)全过。ABI-003 = PASS。
