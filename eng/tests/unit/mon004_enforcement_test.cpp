// eng/tests/unit/mon004_enforcement_test.cpp — 资源判据「只记录、不裁决」+「重计算运行」
// 工作量下限单元测试（FIX-208 更新：一般性资源超限门已取消）。
//
// 覆盖(全部为纯头文件判定, 不需要采样线程):
//   1) 冻结阈值未被本改动触碰(90/85/60 与 §18.2 一致; 工作量下限=10 核·秒);
//   2) 工作量下限口径: 显式 work_core_seconds 与 avg_equivalent_cores×window 回算,
//      低于下限/恰好下限/未提供 active 窗三态;
//   3) 记录/裁决分离(**唯一**处置 = RecordOnly): 默认与 strict 参数下、Ok 与非 Ok 判定
//      一律 RecordOnly（§9.74 裁决 10 + ASTROCS_DESIGN §3.5/§6.3「内存/CPU/线程不设门；
//      exit 10 = 磁盘写满/写盘失败」）—— 正例(违规→RecordOnly)与阴性对照(Ok→RecordOnly)
//      同时锁死, 排除「恒真/恒假」判定。Enforced 枚举值保留仅为兼容引用, 无生产路径。
//   4) 判定本体未被削弱: 旧的 LowAvgCores 失败案例仍失败, 旧的 Ok 案例仍通过,
//      diag 名称不变（判据仍如实记录, 只是不再改变退出码）。
//
// 治理说明: 本条只锁「记录/裁决分离」的机械语义, 不改变任何 §18.2 冻结阈值。
#include "exit_codes.h"
#include "resource_gate.h"

#include <cstdint>
#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

int main() {
    using namespace astrocs;

    // 1) 冻结阈值/退出码常量未被本改动触碰(§18.2 85%/60%; RESOURCE=10)。
    CHECK(kCpuP50MinPercent == 90.0);
    CHECK(kCpuMeanMinPercent == 85.0);
    CHECK(kMon001QueueUtilMinPercent == 60.0);
    CHECK(kMon001QueueWindowSeconds == 10.0);
    CHECK(kMon003MinCoreSeconds == 10.0);
    CHECK(RESOURCE == 10);

    // 2) 工作量下限(线程秒 = 等效核·秒)口径。
    GateConfig g;
    g.avg_equivalent_cores = 2.0;
    g.active_window_seconds = 12.0;
    g.wall_seconds = 12.0;
    CHECK(work_core_seconds_of(g) == 24.0);
    CHECK(gate_workload_above_floor(g));
    g.work_core_seconds = 9.999;                 // 低于下限
    CHECK(!gate_workload_above_floor(g));
    g.work_core_seconds = 10.0;                  // 恰好在下限(含)
    CHECK(gate_workload_above_floor(g));
    g.work_core_seconds = -1.0;                  // 未提供 → 回算
    g.avg_equivalent_cores = 0.5;
    g.active_window_seconds = 6.0;
    CHECK(work_core_seconds_of(g) == 3.0);
    CHECK(!gate_workload_above_floor(g));        // 极小冒烟 run: 只记录不裁决
    g.avg_equivalent_cores = 1.0;
    g.active_window_seconds = -1.0;              // 未提供 active 窗 → 回退整段 wall
    g.wall_seconds = 20.0;
    CHECK(work_core_seconds_of(g) == 20.0);
    CHECK(gate_workload_above_floor(g));

    // 3) 记录/裁决分离(gate_enforcement) —— §9.74 裁决 10: 恒 RecordOnly。
    //   默认 mode: Ok / 非 Ok 都不阻塞 —— 资源判据不再决定退出码。
    CHECK(gate_enforcement(false, GateDiag::Ok) == GateEnforcement::RecordOnly);
    CHECK(gate_enforcement(false, GateDiag::LowAvgCores) == GateEnforcement::RecordOnly);
    CHECK(gate_enforcement(false, GateDiag::MemoryGrowth) == GateEnforcement::RecordOnly);
    CHECK(gate_enforcement(false, GateDiag::AllocGrowthUnbounded) == GateEnforcement::RecordOnly);
    //   strict 旗标(已退役的复现开关): 即使带 strict + 违规也不得 Enforced（rc=10 路径已删）。
    CHECK(gate_enforcement(true, GateDiag::Ok) == GateEnforcement::RecordOnly);
    CHECK(gate_enforcement(true, GateDiag::LowAvgCores) == GateEnforcement::RecordOnly);
    CHECK(gate_enforcement(true, GateDiag::CpuMeanLow) == GateEnforcement::RecordOnly);
    CHECK(gate_enforcement(true, GateDiag::MemoryGrowth) == GateEnforcement::RecordOnly);
    CHECK(gate_enforcement(true, GateDiag::AllocGrowthUnbounded) == GateEnforcement::RecordOnly);
    CHECK(std::string(gate_enforcement_name(GateEnforcement::RecordOnly)) == "record_only");
    CHECK(std::string(gate_enforcement_name(GateEnforcement::Enforced)) == "enforced");

    // 4) 判定本体未被削弱: 旧失败案例仍失败, 旧通过案例仍通过。
    GateConfig c;
    c.kind = ResKind::Compute;
    c.available_cpus = 4;
    c.selected_workers = 4;
    c.max_active_threads = 4;
    c.avg_equivalent_cores = 1.5;                // < 0.85*4 = 3.4 → 旧失败案例
    c.wall_seconds = 6.0;
    c.has_stage_annotation = true;
    c.cpu_percent = 80.0;
    c.iowait_percent = 1.0;
    c.mem_bandwidth_percent = 90.0;
    CHECK(evaluate_gate(c) == GateDiag::LowAvgCores);
    c.avg_equivalent_cores = 3.6;                // >= 3.4 → 旧通过案例
    CHECK(evaluate_gate(c) == GateDiag::Ok);
    CHECK(std::string(gate_diag_name(GateDiag::LowAvgCores)) == "low_avg_cores");

    if (failures != 0) {
        std::fprintf(stderr, "mon004_enforcement: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("mon004_enforcement: OK\n");
    return 0;
}
