# ARCH-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ARCH-001 行(符号级清点七类原语, 验收=PRODUCTION_EXECUTION_INVENTORY 100% 构建目标+AIO/sampler/UPM 风险定位到 symbol); 现有 docs/architecture/{execution_inventory,api_inventory,production_call_paths_stage1/2}.csv; 全仓 grep 检索。

## 动作
1. 全仓符号检索(排除 archive//third_party/): add_executable/pragma omp/std::async|thread/std::mutex|lock_guard|critical/std::queue/acr_*|p2_acr/aio_*|fits_*writer 七类。
2. 新建生成器 tools/arch/build_production_execution_inventory.py(可重跑幂等): 9 列 CSV(category/symbol/location/classification/production_reachable/phase/thread_model/evidence/risk_note), 217 行= exe_target 46(全部 tool/test, production exe=0 待 CLI-001)+openmp_kernel 25 文件(逐文件 pragma 数+V5 禁硬编码注记)+thread_creation 62( watchdog/monitor 豁免注记)+lock 46 文件+queue 1+acr_boundary 24(全部配置守卫: p2_acr_block_eligible/stage2_common.cpp:391-393 acr_route 只支持 auto/cpu, 无 ACR 计算调用)+io_writer 13 文件。
3. 验收映射: 构建目标 100%(46/46 add_executable); 历史 AIO/sampler/UPM 风险定位=execution_inventory.csv stage 级 call_chain 列(symbol 级)+本清单 217 行 symbol/file:line; 死代码=archive/ 排除并机器断言。
4. 机器门 tests/arch/test_inventory.py 5 用例: 七类全覆盖/archive 与 third_party 零出现/ACR 行仅配置守卫+风险注记/production exe=0(单 exe 策略, CLI-001 建立)/生成器幂等(逐字节)。

## 验证
- 生成器跑通 217 行; 全量回归 unittest **26/26 OK**(新增 5)。

## 产物
docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv; tools/arch/build_production_execution_inventory.py; tests/arch/; 本日志。

## PASS 判定
七类原语 100% 覆盖+分类标注(生产/测试/工具/死代码排除)+ACR=纯配置边界(24 处无计算调用)+AIO/sampler/UPM stage 级 call_chain 锚定 symbol。ARCH-001 = PASS。
