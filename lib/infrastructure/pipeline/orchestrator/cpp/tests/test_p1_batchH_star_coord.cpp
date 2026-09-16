// ============================================================================
// test_p1_batchH_star_coord.cpp - Bug 狩猎 R8-A 修复验证:
//   star_measurements 坐标契约 0.5px 系统错位 (同帧混合星点方向性错位)
// 仲裁口径 (DATA-P1-STAR §17.2 / DATA-P1-WCS §18.1 冻结合同):
//   - sdet star_det 坐标 = "像素中心=索引+0.5" 连续系 (§17.2 权威);
//   - DPSF 拟合中心与输入 det_x 同系 (dpsf_psf.cpp 样本 dx=索引-cx、
//     回移 cx+x0, 无 0.5 注入) → 亦为 "像素中心=索引+0.5" 系;
//   - star_measurements 权威块 = 统一契约 index-is-center (值=连续系-0.5);
//   - ipv detections 输入 = IPV 接口契约 (像素中心=索引+0.5) (§18.1)。
// 修复内容:
//   1) orchestrator 写端 dpsf 分支: cx/cy 经 astro_coord_to_unified(-0.5)
//      写入 star_measurements (与 fallback 分支同系);
//   2) 读端 PLATESOLVE: 统一契约 +0.5 (astro_coord_from_unified) 桥接,
//      fallback :1914 直送 (sdet 系 == IPV 接口契约, 不再重复注释矛盾);
//   3) detections 构造提取纯函数 build_platesolve_detections, 去重比较
//      双方处于同一坐标系。
// 运行: ./tests/test_p1_batchH_star_coord.exe (rc=0 即全部通过)
// ============================================================================

#include <iostream>
#include <cmath>
#include <vector>

#include "orchestrator.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, msg) do { \
    ++g_checks; \
    if (cond) { std::cout << "  [PASS] " << (msg) << "\n"; } \
    else { ++g_failures; std::cout << "  [FAIL] " << (msg) << "\n"; } \
} while (0)

static bool feq(double a, double b, double tol = 1e-12) {
    return std::fabs(a - b) <= tol;
}

// ----------------------------------------------------------------------------
// 场景 1: 同帧混合星点 (两来源) 进入 ipv 的坐标全部同系 (核心数值断言)
// 布局 (真实恒星中心, "像素中心=索引+0.5" 系):
//   P0: (100.75, 200.75)  PSF 成功行 (star_measurements 统一契约 = 100.25,200.75)
//   P1: (300.75, 400.75)  PSF 失败行 (status=1, 过滤, n_filtered++)
//   P2: (500.75, 600.75)  PSF 成功行 (统一契约 = 500.25,600.75)
//   F0: (500.75, 600.75)  star_det fallback 与 P2 同一颗 (应被去重)
//       修复前: P2 直送 500.25 (错), F0=500.75, 距离恰 0.5 → 不去重,
//       双星以 0.5px 错位同时进入 ipv (方向性系统错位实证)
//   F1: (700.75, 800.75)  独立 fallback 星 (保留)
// ----------------------------------------------------------------------------
static void test_mixed_frame_same_frame(void) {
    std::cout << "[场景 1] 同帧混合星点坐标契约 (PSF + fallback 两来源)\n";

    const int kSmCols = 15;
    std::vector<double> sm(3 * kSmCols, 0.0);
    // 列: 0 star_id, 1 x, 2 y, 3 flux, 4 flux_unc, 5 bkg, 6 status, 7 fwhm,
    //     8 A, 9 B, 10 mad, 11 ecc, 12 mag, 13 sat, 14 has_sat
    auto set_sm = [&](int row, double x, double y, double status) {
        double* r = sm.data() + static_cast<size_t>(row) * kSmCols;
        r[0] = 1000.0 + row; r[1] = x; r[2] = y;
        r[3] = 50000.0; r[4] = 1.0; r[5] = 100.0; r[6] = status;
        r[7] = 2.0; r[8] = 100.0; r[9] = 100.0; r[10] = 0.5; r[11] = 0.1;
        r[12] = 15.0; r[13] = 0.0; r[14] = 0.0;
    };
    set_sm(0, 100.25, 200.25, 0.0);  // PSF 成功 (统一契约; 连续中心 100.75,200.75)
    set_sm(1, 300.25, 400.25, 1.0);  // PSF 失败 → 过滤
    set_sm(2, 500.25, 600.25, 0.0);  // PSF 成功 (连续中心 500.75,600.75)

    // star_det [N,6] = x,y,flux,mag,sat,has_sat (sdet "像素中心=索引+0.5" 系)
    std::vector<double> det = {
        500.75, 600.75, 40000.0, 15.5, 0.0, 0.0,   // F0: 与 P2 同一颗 → 去重
        700.75, 800.75, 30000.0, 16.0, 0.0, 0.0,   // F1: 独立星 → 保留
    };

    std::vector<double> astro_det;
    Orchestrator::PlatesolveDetStats st = Orchestrator::build_platesolve_detections(
        sm.data(), 3, kSmCols, det.data(), 2, 1024, 1024, astro_det);

    CHECK(st.n_psf == 2, "n_psf=2 (status=0 两行)");
    CHECK(st.n_filtered == 1, "n_filtered=1 (status=1 失败行)");
    CHECK(st.n_fallback == 1, "n_fallback=1 (F0 与 P2 同一颗被去重; 修复前错位实证为 2)");
    CHECK(astro_det.size() == 3 * 6, "detections 3 星 [n,6]");

    // P0: 统一契约 100.25,200.25 → IPV 接口契约 100.75,200.75 (单一桥 +0.5)
    CHECK(feq(astro_det[0], 100.75) && feq(astro_det[1], 200.75),
          "P0 输出 (100.75,200.75) = 统一契约 +0.5 (修复前为 100.25 错位)");

    // P2: 统一契约 500.25,600.25 → 桥接 500.75,600.75
    CHECK(feq(astro_det[6], 500.75) && feq(astro_det[7], 600.75),
          "P2 输出 (500.75,600.75) = 统一契约 +0.5");

    // F1: sdet 连续系直送 (已为 IPV 接口契约, :1914 fallback 无二次变换)
    CHECK(feq(astro_det[12], 700.75) && feq(astro_det[13], 800.75),
          "F1 fallback 直送 (700.75,800.75) 不加 0.5 (sdet 系 == IPV 接口契约)");

    // 同系总断言: 同帧混合后, P0/P2 与 F1 同处一个连续坐标系 —
    // 所有输出坐标满足「值 = 真实像素中心 (索引+0.5 系)」:
    //   PSF 星: 输出 == star_measurements 统一契约值 + 0.5;
    //   fallback 星: 输出 == star_det 原值。
    // 修复前 P0/P2 输出统一契约值 (索引系), 与 F1 (中心系) 同帧混入即
    // 含方向性 0.5px 错位; 本断言在两种实现下数值上互斥。
    bool all_same_frame = feq(astro_det[0], 100.75) &&
                          feq(astro_det[1], 200.75) &&
                          feq(astro_det[6], 500.75) &&
                          feq(astro_det[7], 600.75) &&
                          feq(astro_det[12], 700.75) &&
                          feq(astro_det[13], 800.75);
    CHECK(all_same_frame, "全部 detections 同处 '像素中心=索引+0.5' 坐标系 (同帧混合无方向性错位)");
}

// ----------------------------------------------------------------------------
// 场景 2: 坐标契约桥单一来源语义 (写端 -0.5 / 读端 +0.5 / 往返恒等)
// ----------------------------------------------------------------------------
static void test_coord_bridge(void) {
    std::cout << "[场景 2] 坐标契约桥 (astro_coord_to_unified / from_unified)\n";
    // sdet/dpsf 连续系值 (像素中心=索引+0.5): 整数像素索引 i 的中心 = i+0.5
    CHECK(feq(Orchestrator::astro_coord_to_unified(100.75), 100.25),
          "写端 to_unified(100.75)=100.25 (索引即中心统一契约)");
    CHECK(feq(Orchestrator::astro_coord_from_unified(100.25), 100.75),
          "读端 from_unified(100.25)=100.75 (IPV 接口契约)");
    // 往返恒等 (含边界: 索引 0 中心 0.5 → 统一契约 0.0)
    CHECK(feq(Orchestrator::astro_coord_from_unified(
              Orchestrator::astro_coord_to_unified(0.5)), 0.5),
          "往返恒等: from(to(0.5))=0.5 (左缘像素中心)");
    CHECK(feq(Orchestrator::astro_coord_from_unified(
              Orchestrator::astro_coord_to_unified(172.691)), 172.691),
          "往返恒等: 非整值坐标 (trace 实测星点 172.691)");
}

// ----------------------------------------------------------------------------
// 场景 3: 去重阈值 0.5px 边界复核 (统一后坐标差恰 0.5px 时安全性)
// 统一后: PSF 桥接值与 sdet fallback 直送值同系, 同一颗星两来源观测差
// = 检测-拟合中心距 (<<1px) < 0.5 阈值 → 正常去重; 不同星最小间距 >> 0.5。
// 阈值语义为严格小于 (<0.5): 恰 0.5 不去重 (与修复前语义一致)。
// ----------------------------------------------------------------------------
static void test_dedup_threshold_boundary(void) {
    std::cout << "[场景 3] 去重阈值 0.5px 边界\n";
    const int kSmCols = 15;
    std::vector<double> sm(kSmCols, 0.0);
    sm[0] = 1.0; sm[1] = 500.25; sm[2] = 600.25; sm[6] = 0.0; sm[7] = 2.0;

    auto run = [&](double fx, double fy) {
        std::vector<double> det = {fx, fy, 30000.0, 16.0, 0.0, 0.0};
        std::vector<double> astro;
        Orchestrator::PlatesolveDetStats st =
            Orchestrator::build_platesolve_detections(
                sm.data(), 1, kSmCols, det.data(), 1, 1024, 1024, astro);
        return st.n_fallback;
    };
    // P2 桥接后 = (500.75, 600.75); 距离 0.499 < 0.5 → 去重
    CHECK(run(500.75 + 0.499, 600.75) == 0, "距 0.499px < 0.5 → 同一颗去重");    // 距离恰 0.5 → 不去重 (严格 <0.5 语义保持)
    CHECK(run(501.25, 600.75) == 1, "距恰 0.5px → 不去重 (阈值严格 <0.5)");
    // 距离 0.5001 > 0.5 → 保留
    CHECK(run(500.75 + 0.5001, 600.75) == 1, "距 0.5001px > 0.5 → 保留");
}

// ----------------------------------------------------------------------------
// 场景 4: 过滤语义不变性 (status/边缘/饱和/FWHM 仅计数不剔除 PSF 星)
// ----------------------------------------------------------------------------
static void test_filter_semantics_unchanged(void) {
    std::cout << "[场景 4] PSF 星过滤语义不变性\n";
    const int kSmCols = 15;
    std::vector<double> sm(2 * kSmCols, 0.0);
    auto set_row = [&](int row, double x, double y, double status,
                       double sat, double fwhm) {
        double* r = sm.data() + static_cast<size_t>(row) * kSmCols;
        r[0] = 2000.0 + row; r[1] = x; r[2] = y; r[6] = status;
        r[7] = fwhm; r[13] = sat; r[14] = sat;
    };
    // 星 0: 边缘 (统一契约 x=4.75 → 连续 5.25 <5+0.5? 边界用 x<5 判统一值)
    // 边缘判定与修复前一致: 对统一契约值 x=4.75 (<5) 计 edge, 仅计数不剔除
    set_row(0, 4.75, 200.75, 0.0, 0.0, 2.0);
    // 星 1: 饱和 + FWHM 越界, 仅计数
    set_row(1, 500.25, 600.75, 0.0, 1.0, 25.0);
    std::vector<double> astro;
    Orchestrator::PlatesolveDetStats st = Orchestrator::build_platesolve_detections(
        sm.data(), 2, kSmCols, nullptr, 0, 1024, 1024, astro);
    CHECK(st.n_psf == 2, "sat/fwhm/edge PSF 星仍保留输入 (仅计数, 语义不变)");
    CHECK(st.n_filtered == 2, "n_filtered=2 (edge+sat/fwhm 计数)");
    CHECK(astro.size() == 2 * 6, "两星均进入 detections");
    // 边缘星坐标仍桥接 (统一契约 4.75 → 5.25)
    CHECK(feq(astro[0], 5.25), "边缘 PSF 星仍经 +0.5 桥接 (4.75→5.25)");

    // NULL fallback 块与非法列数 fallback 块安全跳过
    std::vector<double> astro2;
    Orchestrator::PlatesolveDetStats st2 = Orchestrator::build_platesolve_detections(
        sm.data(), 2, kSmCols, nullptr, 0, 1024, 1024, astro2);
    CHECK(st2.n_fallback == 0 && astro2.size() == 2 * 6,
          "star_det 缺失 (nullptr) 时仅 PSF 路径生效");
}

int main(void) {
    std::cout << "== R8-A star_measurements 坐标契约修复验证 (test_p1_batchH_star_coord) ==\n";
    test_mixed_frame_same_frame();
    test_coord_bridge();
    test_dedup_threshold_boundary();
    test_filter_semantics_unchanged();
    std::cout << "== 完成: " << g_checks << " 项检查, " << g_failures << " 项失败 ==\n";
    return g_failures == 0 ? 0 : 1;
}
