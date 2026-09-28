#ifndef ASTROCS_SDET_TEST_PROBE_H
#define ASTROCS_SDET_TEST_PROBE_H
// O11 deblending 判据的测试观察面（仅以 -DSDET_TESTING 编译的目标含此面;
// 生产编译不含, 行为与 ABI 零影响）。
//
// 用途: Oracle 以文献式「fsum > delta_c * 父组分检出阈上总流量」在测试内
// 独立复算, 与实现记录的逐枝判定 sat1 逐条比对。旧基准（枝自身 bflow）
// 下 bflow != total_flux, 比对即判红 —— 「原版红、补丁绿」的可判定度量。
#include <vector>

struct SdetDeblendProbe {
    double t;           // 层阈值 t(i) = thr*(Smax/thr)^(i/30)
    double fsum;        // 判据分子: 枝在当前层阈值之上的积分流量
    double bflow;       // 对照读数: 枝在检出阈值之上的积分流量（旧权重基准）
    double total_flux;  // delta_c 权重基准: 父组分在检出阈值之上的总流量
    int nbrs;           // 该层枝数
    int sat1;           // 实现判定: (fsum > SDET_DELTA_C * total_flux) ? 1 : 0
};

extern std::vector<SdetDeblendProbe>* sdet_test_deblend_probe;

// 每组分（按扫描序）经 O11 分裂后的叶数: 判据基准的**行为级**读数
// （同一帧在「枝自身流量」基准下会多分裂出叶, 见 Oracle 用例）
extern std::vector<int>* sdet_test_deblend_leaves;

#endif  // ASTROCS_SDET_TEST_PROBE_H
