// ============================================================================
// star_coord_contract.h - star_det / star_measurements 像素坐标契约唯一事实源
//
// 三套坐标系 (DATA-P1-STAR §17.2 / DATA-P1-WCS §18.1):
//
//   (1) 连续系 = "IPV 接口契约": 像素中心 = 索引 + 0.5
//       - sdet 检测输出: 拟合残差 dx = x_pixel + 0.5 - cx
//         (sdet_api.cpp:127-131, "samples[k].dx 已含 +0.5"), 故 sdet 的 cx
//         即真实像素中心坐标 (0-based 连续);
//       - ipv detections 输入 (§18.1), fallback 支路直送不再变换。
//
//   (2) 统一契约 = "index-is-center": 值 = 连续坐标 - 0.5
//       - star_measurements 权威块 (§17.2 列 [1]/[2] = x/y)。
//
//   (3) dpsf 拟合中心输出 = **已是统一契约 (index-is-center)**
//       - dpsf 采样 sp.dx = (double)x - cx (dpsf_psf.cpp:295), 即像素"索引"
//         就是模型坐标, 拟合中心回移 img_cx = cx + x0 (dpsf_psf.cpp:437)
//         仍在该系; **无 +0.5 注入**。
//       - 因此同一颗星 sdet 输出比 dpsf 输出恒大 0.5 px。实测 (R-3 §2.9,
//         run/PROJECT-GOVERNANCE-01/R-3/probe/probe_convention, SNR=50/100/300):
//         sdet - truthFITS median 0.0100..0.0202 px;
//         dpsf - truthFITS 恒 -0.4993..-0.5000 px; 三种初值同解。
//
// 生产桥 (orchestrator.cpp run_stage_psf 写端 / run_stage_platesolve 读端)
// 必须且只能经本文件三函数取坐标。历史缺陷: 写端对 PSF 支路**再**施加
// -0.5 (把已是统一契约的 dpsf 输出当成 +0.5 系) ⇒ PSF 支路较 fallback 支路
// 系统性偏低恰 0.5000 px, 而 ipv 去重阈值是严格 < 0.5 px ⇒ 同一颗星双份
// 进入 ipv (R-3 §2.9 Part D 实测; 缺陷已由 SCI-FIX-PSF 第 1 项修复)。
//
// 机器门: G-P1-CENTROID-1 = lib/algorithms/psf/tests/p1psf/p1psf_centroid_gate.cpp
// (链接 sdet + dpsf 真实生产源, ctest p1psf_centroid_gate /
//  p1psf_centroid_gate_neg), 判据与证据见
//  docs/science/GATES_AND_TOLERANCES.md G-P1-CENTROID-1 行。
// ============================================================================
#pragma once

namespace astrocs {
namespace p1 {
namespace coord {

// sdet 检测输出 (像素中心=索引+0.5) -> star_measurements 统一契约 (index-is-center)
inline double star_measurement_from_sdet(double sdet_center) {
    return sdet_center - 0.5;
}

// dpsf 拟合中心 (已是 index-is-center) -> star_measurements 统一契约: 恒等。
// 恒等不是冗余: 它是本契约的机器可断言锚点, 显式禁止对该支路再施加 -0.5
// (该重复减法就是 R-3 实测的 0.5000 px 双份星缺陷)。
inline double star_measurement_from_dpsf(double dpsf_center) {
    return dpsf_center;
}

// star_measurements 统一契约 (index-is-center) -> ipv detections 接口契约
// (像素中心=索引+0.5)
inline double ipv_detection_from_star_measurement(double unified) {
    return unified + 0.5;
}

}  // namespace coord
}  // namespace p1
}  // namespace astrocs
