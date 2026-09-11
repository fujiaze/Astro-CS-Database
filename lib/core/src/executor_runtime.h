// lib/core/src/executor_runtime.h — RT-001 Runtime 唯一 work-unit executor 注册点
//
// 归属: lib/core（Runtime 适配层）内部头, 不进安装面 include/（include/ 不在
// RT-001 写入白名单）。RT-004 冻结合同头 astrocs/core/executor.h 语义不变:
// 全进程唯一共享 CPU heavy executor（per Runtime/预算源一个）；本注册点是它
// 在生产执行路径的接入面（此前 executor.cpp 未编入任何生产 target, 唯一池
// 为死代码 —— 接入缺口即 RT-001 任务目标）。
//
// 语义:
//   - 按 ThreadBudget 实例绑定（同一预算对象 → 同一池; 进程内每个预算源恰好
//     一个池 = "一个进程只有一个资源调度器和线程预算源" 宪章 §10.4）。
//   - 池 worker 数 = 预算上限; 每个 work-unit 任务执行前经 acquire(1,1,NONBLOCK)
//     恰租 1 个预算槽（Σ active ≤ budget 全局不超卖, 与模块线程租约同一预算源）。
//   - owner=Runtime: 池随注册表驻留进程生命周期（强引用）; 预算对象消亡后由
//     后续调用惰性回收（析构 join 全部 worker, 无 detach/UAF）。
//   - budget 为空/无效 → 返回 nullptr: 调用方必须串行降级（不伪造执行授权）,
//     禁止回退到调用点自建线程池。
//
// 编译归属注记（RT-001 白名单约束）: executor.cpp 的实现经 module_adapters.cpp
// 顶部 #include 编入 astrocs_module_adapters（根 CMakeLists.txt 不在 RT-001
// 写入白名单, 无法直接把 executor.cpp 加进生产 target —— finding F-RT-001-04
// 登记: 整改归位 = 根 CMakeLists 为 astrocs_module_adapters 显式追加
// lib/core/src/executor.cpp 并删除该 #include）。因此任何把 executor.cpp 与
// astrocs_module_adapters 同时编入一个二进制的 target 都会重定义 —— 新测试
// target 一律只链 astrocs_module_adapters, 不得单编 executor.cpp。
#pragma once

#include "astrocs/core/executor.h"

#include <memory>

namespace astrocs::core::rt {

// Runtime 唯一共享 CPU heavy executor（按预算实例绑定）。
// 返回的 shared_ptr 与注册表共享所有权; 调用方仅持有句柄, 不得 delete。
std::shared_ptr<CpuHeavyExecutor> shared_work_executor(
    const std::shared_ptr<ThreadBudget>& budget);

}  // namespace astrocs::core::rt
