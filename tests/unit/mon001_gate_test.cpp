// tests/unit/mon001_gate_test.cpp — MON-001 (V7 04_CPU_RESOURCE_TASKS) 资源阈值判定单测
// 覆盖: 监控缺失直接 FAIL / 70% 样本 U>=0.75 / 队列有工作连续 10s U<0.50 /
//       utilization_value normalized 口径(分母=min(selected,available), 无硬编码核数)/
//       哨兵自洽(-1=未采样非合法值, 跳过对应判定, 不构成低利用率证据;
//       批次 P p2007 先例)/双层判定与 evaluate_gate 互补(wall<5s 豁免不豁免本判定)。
// 矛盾说明(mini workload 结构失配, 裁决项): 资源门 0.80*min(selected,available) 对
//       mini 任务不可达——本测试侧不做 abs-floor 放宽, 只验证未采样哨兵按纪律跳过;
//       abs-floor 方案由负责人裁决(见 TASK_RESULT findings)。
#include "resource_gate.h"

#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 构造一个"其余判据全部通过"的 compute GateConfig 基线, 单测只改 MON-001 字段。
static astrocs::GateConfig base_config() {
    astrocs::GateConfig g;
    g.kind = astrocs::ResKind::Compute;
    g.available_cpus = 4;
    g.selected_workers = 4;
    g.max_active_threads = 4;
    g.avg_equivalent_cores = 3.8;
    g.wall_seconds = 30.0;
    g.has_stage_annotation = true;
    g.cpu_percent = 95.0;
    g.workers_p50 = 4.0;
    g.cpu_p50_percent = 95.0;
    g.cpu_mean_percent = 90.0;
    return g;
}

int main() {
    // 1) 正常通过: 监控在跑, 40 个有效样本, 80% 样本 U>=0.75, 队列无饥饿。
    {
        astrocs::GateConfig g = base_config();
        g.monitor_present = true;
        g.util_samples_measured = 40.0;
        g.util_samples_pass_frac = 0.80;
        g.queue_low_run_seconds = 0.0;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::Ok);
    }
    // 2) 监控缺失(monitor_present=false) → FAIL, 即便采样统计"看起来"达标
    //    (伪造达标统计不能弥补监控缺失; 规格: 监控缺失直接 FAIL)。
    {
        astrocs::GateConfig g = base_config();
        g.monitor_present = false;
        g.util_samples_measured = 40.0;
        g.util_samples_pass_frac = 0.95;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::MonitoringMissing);
    }
    // 3) 谎报监控在跑但零有效样本 → FAIL(无资源证据即 FAIL)。
    {
        astrocs::GateConfig g = base_config();
        g.monitor_present = true;
        g.util_samples_measured = 0.0;
        g.util_samples_pass_frac = 0.95;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::MonitoringMissing);
    }
    // 4) 哨兵自洽(批次 P p2007 先例): -1=未采样非合法值。全部 MON-001 统计
    //    未提供时按"监控无有效证据"FAIL, 不按数值 0/负利用率误判其它诊断。
    {
        astrocs::GateConfig g = base_config();
        g.monitor_present = false;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::MonitoringMissing);
    }
    // 5) 哨兵不构成低利用率证据: 样本数有效但 pass_frac/queue 未观测(-1) →
    //    对应判定跳过 → Ok(禁止把哨兵当 0 参与 70%/10s 判定)。
    {
        astrocs::GateConfig g = base_config();
        g.monitor_present = true;
        g.util_samples_measured = 20.0;
        g.util_samples_pass_frac = -1.0;
        g.queue_low_run_seconds = -1.0;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::Ok);
    }
    // 6) 70% 样本门: 达标占比 0.50 < 0.70 → UtilizationP75Low。
    {
        astrocs::GateConfig g = base_config();
        g.monitor_present = true;
        g.util_samples_measured = 40.0;
        g.util_samples_pass_frac = 0.50;
        g.queue_low_run_seconds = 0.0;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::UtilizationP75Low);
    }
    // 7) 70% 样本门边界: 恰好 0.70 → 通过; 0.6999 → FAIL。
    {
        astrocs::GateConfig g = base_config();
        g.monitor_present = true;
        g.util_samples_measured = 40.0;
        g.util_samples_pass_frac = 0.70;
        g.queue_low_run_seconds = 0.0;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::Ok);
        g.util_samples_pass_frac = 0.6999;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::UtilizationP75Low);
    }
    // 8) 队列饥饿门: 队列有工作连续 12s(>=10s) 低利用 → QueueStarvedCpu;
    //    9.9s(<10s) 不触发; 哨兵 -1(未观测) 不触发。
    {
        astrocs::GateConfig g = base_config();
        g.monitor_present = true;
        g.util_samples_measured = 40.0;
        g.util_samples_pass_frac = 0.9;
        g.queue_low_run_seconds = 12.0;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::QueueStarvedCpu);
        g.queue_low_run_seconds = 9.9;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::Ok);
        g.queue_low_run_seconds = -1.0;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::Ok);
    }
    // 9) 判定优先级: 70% 门先于队列门报告(首个失败即返回, 可诊断)。
    {
        astrocs::GateConfig g = base_config();
        g.monitor_present = true;
        g.util_samples_measured = 40.0;
        g.util_samples_pass_frac = 0.30;
        g.queue_low_run_seconds = 15.0;
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::UtilizationP75Low);
    }
    // 10) utilization_value 口径: 分母=min(selected_workers, available_cpus);
    //     100% CPU(全部可用核) 均摊到 4 worker → U=0.25; 320% → 0.80;
    //     selected=2 时 100% → 0.5。无硬编码核数参与。
    {
        astrocs::GateConfig g = base_config();   // min(4,4)=4
        CHECK(astrocs::utilization_value(g, 100.0) == 0.25);
        CHECK(astrocs::utilization_value(g, 320.0) == 0.80);
        g.selected_workers = 2;                  // min(2,4)=2
        CHECK(astrocs::utilization_value(g, 100.0) == 0.50);
        g.selected_workers = 8;                  // min(8,4)=4 → available 封顶
        CHECK(astrocs::utilization_value(g, 200.0) == 0.50);
    }
    // 11) 分母退化保护: available_cpus=0 → U=0(除零安全)。
    {
        astrocs::GateConfig g = base_config();
        g.available_cpus = 0;
        CHECK(astrocs::utilization_value(g, 100.0) == 0.0);
    }
    // 12) 双层互补: evaluate_gate 短任务统计豁免(wall<5s) 不豁免 MON-001 逐样本
    //     判定 —— 短 heavy 片段仍受 70% 样本门约束(V7: 不能切成 <5s 片段规避门禁;
    //     累计 wall 由 heavy_wall_seconds_total 承载, 哨兵=未汇总不阻塞判定)。
    {
        astrocs::GateConfig g = base_config();
        g.wall_seconds = 4.0;
        g.monitor_present = true;
        g.util_samples_measured = 8.0;
        g.util_samples_pass_frac = 0.10;
        g.queue_low_run_seconds = 0.0;
        g.heavy_wall_seconds_total = 24.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::Ok);          // 统计判据豁免
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::UtilizationP75Low); // 仍 FAIL
    }
    // 13) 平均 U 门(LowAvgCores) 与逐样本门互认: 平均 0.8 但 70% 样本门不过 →
    //     双层均须判 FAIL(evaluate_gate 由 LowAvgCores 承担平均门; 此处验证
    //     evaluate_mon001 独立按样本占比拒绝)。
    {
        astrocs::GateConfig g = base_config();
        g.avg_equivalent_cores = 3.4;   // 0.85*4 >= 0.80*4 → evaluate_gate 过
        g.monitor_present = true;
        g.util_samples_measured = 50.0;
        g.util_samples_pass_frac = 0.40;
        g.queue_low_run_seconds = 0.0;
        CHECK(astrocs::evaluate_gate(g) == astrocs::GateDiag::Ok);
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::UtilizationP75Low);
    }
    // 14) 诊断枚举名与消息: 新诊断名落 enum→string 映射; 哨兵场景消息如实标注
    //     unsampled 而非伪造数值。
    {
        astrocs::GateConfig g = base_config();
        CHECK(std::string(astrocs::gate_diag_name(astrocs::GateDiag::MonitoringMissing)) ==
              "monitoring_missing");
        CHECK(std::string(astrocs::gate_diag_name(astrocs::GateDiag::UtilizationP75Low)) ==
              "utilization_p75_low");
        CHECK(std::string(astrocs::gate_diag_name(astrocs::GateDiag::QueueStarvedCpu)) ==
              "queue_starved_cpu");
        const std::string m1 = astrocs::diag_message(astrocs::GateDiag::UtilizationP75Low, g);
        CHECK(m1.find("unsampled") != std::string::npos);
        g.util_samples_pass_frac = 0.30;
        const std::string m2 = astrocs::diag_message(astrocs::GateDiag::UtilizationP75Low, g);
        CHECK(m2.find("0.300000") != std::string::npos);
        CHECK(m2.find("0.700000") != std::string::npos);
        const std::string m3 = astrocs::diag_message(astrocs::GateDiag::QueueStarvedCpu, g);
        CHECK(m3.find("10.000000") != std::string::npos);
    }
    // 15) mini workload 矛盾(裁决项)的哨兵自洽: mini 任务(批次小, U 低于
    //     0.80*min(selected,available)) 未采样时按哨兵纪律跳过 70%/10s 判定,
    //     不在测试侧引入 abs-floor 放宽 —— 阈值本体维持 V7 规格, 放宽只能由
    //     负责人裁决落在生产面。
    {
        astrocs::GateConfig g = base_config();
        g.selected_workers = 2;
        g.max_active_threads = 2;
        g.avg_equivalent_cores = 0.2;   // mini workload: 远低于 0.80*2
        g.wall_seconds = 3.0;           // 短任务
        g.monitor_present = true;
        g.util_samples_measured = -1.0; // 未采样
        g.util_samples_pass_frac = -1.0;
        g.queue_low_run_seconds = -1.0;
        // MON-001 判定面: 未采样→监控证据不足路径 (monitoring_effective=false →
        // MonitoringMissing; 不产生 UtilizationP75Low/QueueStarvedCpu 误判)。
        CHECK(astrocs::evaluate_mon001(g) == astrocs::GateDiag::MonitoringMissing);
        // 哨兵纪律: 未采样字段对 gate 消费方跳过判定(不作为低利用率证据),
        // 由 p2007 联合门测试的 verdict 自洽分支覆盖端到端语义。
        CHECK(!astrocs::mon001_sampled(g.util_samples_measured));
        CHECK(!astrocs::mon001_sampled(g.util_samples_pass_frac));
        CHECK(!astrocs::mon001_sampled(g.queue_low_run_seconds));
    }
    // 16) monitoring_effective 直测: 三要素(监控在跑/样本数有效/样本数>0)缺一不可。
    {
        astrocs::GateConfig g = base_config();
        g.util_samples_measured = 10.0;
        CHECK(!astrocs::monitoring_effective(g));           // 缺 monitor_present
        g.monitor_present = true;
        CHECK(astrocs::monitoring_effective(g));
        g.util_samples_measured = -1.0;
        CHECK(!astrocs::monitoring_effective(g));           // 哨兵=未提供
        g.util_samples_measured = 0.0;
        CHECK(!astrocs::monitoring_effective(g));           // 零样本
    }
    // 17) GateDiag 追加不改既有枚举序(first10s_diag 以 int 持久化, 依赖既有值稳定)。
    {
        CHECK(static_cast<int>(astrocs::GateDiag::IoWaitHigh) <
              static_cast<int>(astrocs::GateDiag::MonitoringMissing));
        CHECK(static_cast<int>(astrocs::GateDiag::MonitoringMissing) <
              static_cast<int>(astrocs::GateDiag::UtilizationP75Low));
        CHECK(static_cast<int>(astrocs::GateDiag::UtilizationP75Low) <
              static_cast<int>(astrocs::GateDiag::QueueStarvedCpu));
    }

    if (failures != 0) {
        std::fprintf(stderr, "mon001_gate_test: %d check(s) FAILED\n", failures);
        return 1;
    }
    std::fprintf(stdout, "mon001_gate_test: all checks passed\n");
    return 0;
}
