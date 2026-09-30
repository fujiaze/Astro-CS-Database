// ============================================================================
// wcs_degrade_policy.h — WCS/板解的**逐帧降级契约**（唯一权威实现，头文件即正本）
// ----------------------------------------------------------------------------
// 上位依据（本轮负责人裁决，登记号待前台写入权威链）：
//   「盲检测的用途是供 IPV 求解器解图像坐标。有假星、真星不足的情况，
//     足够强大的求解器依然要解析出坐标，因此这类情况不应该阻塞运行。」
//   ⇒ 契约四条：
//     (1) 绝不中止：解算质量类失败只判该帧，不中止节点/整次运行；
//     (2) 机读可查：逐帧落 status + 稳定错误码 + 证据（残差/配对数/源数/假星占比代理量）；
//     (3) 先尽力再降级：降级前必须走完稳健化阶梯（换策略重试），不许一步放弃；
//     (4) 下游能正确接收：未解出/降级的帧不带 wcs 对象，下游据此 fail-closed 于**该帧**。
// 边界（必须保持硬失败，**不降级**）：输入完整性类（文件不可读、校验和不符、schema 违约、
//   星表目录缺失/不完整、句柄创建失败、解算器 ABI 不匹配）。
//
// 复用既有机制（不自造并行体系）：
//   · 逐帧判决形状 = FAILSEM-01 的 P1FrameVerdict（module_adapters.cpp:5380-5388），
//     字段名与 error_report 口径（LOG 合同 §5）逐字沿用；
//   · 稳定错误码表 = LOG_AND_ERROR_CONTRACT §5 的 `^[A-Z][A-Z0-9_]{0,63}$`，
//     邻域已有 PHOT_WCS_UNUSABLE / PHOT_FRAME_FAILED 等码，本头只增 WCS_* 族；
//   · 降级事实落位 = manifest 的 frame_status/frame_errors/failed_frames（与测光节点同源）。
//
// 冻结面（本头**不含**任何容差/公式/默认星数；容差仍由 ALG-WCS-001 与 DISP-WCS-001 拥有）：
//   · 稳健化阶梯只放宽**采集广度**（检测星数上限、Gaia 锥半径因子、极限星等迭代次数），
//     **不动任何验收门**：rms_px>0.5 / n_pairs<12 / 尺度比[0.8,1.25] / 票数≥3 /
//     iter_trans order 3→2→1 回退，全部保持原样；
//   · 阶梯默认第 0 级 = 生产现行参数（零行为变化），只有失败后才进入更宽的第 1 级。
// ============================================================================
#ifndef ACSD_CORE_WCS_DEGRADE_POLICY_H
#define ACSD_CORE_WCS_DEGRADE_POLICY_H

#include <cmath>
#include <string>
#include <vector>

namespace acsd {
namespace core {

// ── 稳健化阶梯：一级 = 一次求解尝试的采集广度。数值全部是**采集面**参数，
//    没有任何一项是验收容差。level 0 必须恒等于生产现行取值（零行为变化）。
struct WcsSolveStrategy {
  std::string name;          // 稳定标识 ^[a-z][a-z0-9_]{0,63}$（进产物 attempts[].strategy）
  int    max_stars;          // sdet 检测星数上限（采集面）
  double query_radius_factor;// IpvParams.gaia_query_radius_factor（Gaia 锥半径 = FOV对角 × 该因子）
  int    mag_lim_max_iter;   // IpvParams.m_lim_max_iter（极限星等割线迭代最大查询次数）
};

// 生产默认阶梯：第 0 级 = 现行值（sp.maxStars=2000 module_adapters.cpp:4896；
// gaia_query_radius_factor=0.55 / m_lim_max_iter=4 = ipv_types.h:251,277 的默认值）。
// 第 1 级 = 只在第 0 级**失败后**启用：检测星数上限与 Gaia 锥半径放宽，验收门不变。
inline std::vector<WcsSolveStrategy> WcsDefaultSolveLadder() {
  return {
      {"ipv_default",       2000, 0.55, 4},
      {"ipv_wide_acquisition", 6000, 0.90, 6},
  };
}

// ── 一次求解尝试的留痕（每级一行，失败原因保留求解器自报原文）
struct WcsSolveAttempt {
  std::string strategy;
  int    rc            = -1;   // C ABI 返回值（1=成功）
  bool   success       = false;
  int    sip_order     = 0;
  int    trans_order   = 0;
  int    n_pairs       = 0;
  double rms_px        = 0.0;
  std::string error;           // 求解器自报原文（verbatim，不改写）
};

// ── 证据：残差 / 配对数 / 源数 / 假星占比代理量。全部来自 IpvWcsResult 的**结构化字段**，
//    不做任何 error_msg 字符串解析。
struct WcsSolveEvidence {
  int    n_detected = 0;       // 源数：检测星数
  int    n_catalog  = 0;       // 星表星数
  int    n_pairs    = 0;       // 配对数
  int    n_inliers  = 0;       // 最优内点数
  int    trans_order = 0;      // TRANS 阶数（失败=0）
  int    sip_order  = 0;       // SIP 阶数
  double rms_px     = 0.0;     // 残差（px）
  double rms_arcsec = 0.0;     // 残差（角秒）
  bool   rms_finite = false;   // 残差是否有限（false 本身也是证据）
  // 假星占比**代理量** = 1 − 配对数/检测星数（= 未被星表解释的检测占比）。
  // 声明：这不是地面真值的假星率，只是「未被配上的检测」占比；n_detected==0 时为 false。
  bool   fake_fraction_valid = false;
  double fake_fraction = 0.0;
};

// ── 逐帧判决（形状对齐 FAILSEM-01 P1FrameVerdict, module_adapters.cpp:5380-5388）
// status 沿用既有词表 "ok" | "fail"（下游 drizzle :7711 / photometry :5551 已在按
// status=="fail" 选输入面 ⇒ 复用即得「下游正确接收」，不需要改任何下游判据）。
// wcs_status 是本节点特有的**坐标态**，与 status 正交：solved=已解出；unsolved=未解出。
// **刻意不写 degraded_reason**：docs/detail/LOG_AND_ERROR_SYSTEM.md:209 与
// docs/detail/PHASE1_DETAILED_DESIGN.md:73 明文「帧级失败不是降级, 不写
// degraded_reason」；本节点对未解出帧**从不**编造替代 WCS（改变科学语义的降级
// 不是降级 ⇒ docs/ACSD_DESIGN.md:563-564），故它是帧级失败而非降级。
// 「不阻塞运行」是**节点行为**（continue，不 return fail），由 status 之外的
// 节点返回值承载, 不靠 degraded_reason 冒充。
struct WcsFrameVerdict {
  std::string status;          // "ok" | "fail"（FAILSEM-01 既有词表）
  std::string wcs_status;      // "solved" | "unsolved"
  std::string error_domain;    // ErrorDomain 名（fail 时 = "DATA"）
  std::string error_status;    // 稳定错误码 WCS_*（fail 时非空）
  std::string error;           // 诊断（求解器自报原文 + 证据摘要）
  bool     abort_run   = false;// 恒 false —— 见下方契约断言
  WcsSolveEvidence evidence;
  std::vector<WcsSolveAttempt> attempts;
};

// ── 原因码分类：只用结构化证据分类，**不解析 error_msg 文本** ──────────────
//   求解器在 F-9 闸门拒绝解时 success=0 但 n_pairs/rms_px 已填（ipv_wcs.cpp:754-769），
//   而更早的失败点（选星/三角投票/iter_trans/重投影）用零初始化的 fail_result 返回
//   ⇒ n_pairs>0 ⟺ 「有候选解但被质量门拒」；n_pairs==0 ⟺ 「根本没走到出解」。
//   这是**可判别**的差异，也是登记在案的证据边界：早期失败点不填 n_detected。
inline std::string WcsClassifyFailure(const WcsSolveEvidence& ev) {
  if (ev.n_pairs <= 0) {
    if (ev.n_detected <= 0) return "WCS_NO_DETECTED_SOURCES";
    return "WCS_NO_SOLUTION";
  }
  if (!ev.rms_finite) return "WCS_RESIDUAL_NOT_FINITE";
  if (ev.rms_px > 0.5) return "WCS_RESIDUAL_EXCEEDS_GATE";
  if (ev.n_pairs < 12) return "WCS_PAIRS_BELOW_MIN";
  return "WCS_NO_SOLUTION";
}

// ── 证据装配：由 IpvWcsResult 的结构化字段直接搬运 ─────────────────────────
inline WcsSolveEvidence WcsMakeEvidence(int n_detected, int n_catalog, int n_pairs,
                                        int n_inliers, int trans_order, int sip_order,
                                        double rms_px, double rms_arcsec) {
  WcsSolveEvidence e;
  e.n_detected  = n_detected;
  e.n_catalog   = n_catalog;
  e.n_pairs     = n_pairs;
  e.n_inliers   = n_inliers;
  e.trans_order = trans_order;
  e.sip_order   = sip_order;
  e.rms_px      = rms_px;
  e.rms_arcsec  = rms_arcsec;
  e.rms_finite  = std::isfinite(rms_px) && std::isfinite(rms_arcsec);
  if (n_detected > 0) {
    e.fake_fraction_valid = true;
    e.fake_fraction = 1.0 - static_cast<double>(n_pairs) / static_cast<double>(n_detected);
    if (e.fake_fraction < 0.0) e.fake_fraction = 0.0;
    if (e.fake_fraction > 1.0) e.fake_fraction = 1.0;
  }
  return e;
}

// ── 判词合成：成功 → ok；阶梯耗尽仍失败 → unsolved（**绝不 abort**）──────────
// force_abort_only_for_negative_control：验收负例专用开关。生产**恒 false**。
//   置 true 时本函数退回「一步放弃即中止」的旧行为，用来证明判据能红
//   （见 eng/tests/unit/wcsdegrade/ 的负例自检）。
inline WcsFrameVerdict WcsMakeVerdict(bool solved, const WcsSolveEvidence& ev,
                                      const std::vector<WcsSolveAttempt>& attempts,
                                      const std::string& solver_error,
                                      bool force_abort_only_for_negative_control = false) {
  WcsFrameVerdict v;
  v.evidence = ev;
  v.attempts = attempts;
  if (solved) {
    v.status     = "ok";
    v.wcs_status = "solved";
    return v;
  }
  // 未解出 = **帧级失败**（不是降级）：不写 degraded_reason
  // （LOG_AND_ERROR_SYSTEM.md:209 / PHASE1_DETAILED_DESIGN.md:73）。
  v.status       = force_abort_only_for_negative_control ? "abort" : "fail";
  v.wcs_status   = "unsolved";
  v.error_domain = "DATA";
  v.error_status = WcsClassifyFailure(ev);
  v.abort_run    = force_abort_only_for_negative_control;
  v.error        = solver_error.empty() ? std::string("solver returned failure")
                                        : solver_error;
  v.error += " (robustness ladder exhausted: ";
  v.error += std::to_string(attempts.size());
  v.error += " attempt(s), n_pairs=" + std::to_string(ev.n_pairs) +
             ", rms_px=" + std::to_string(ev.rms_px) +
             ", n_detected=" + std::to_string(ev.n_detected) + ")";
  return v;
}

}  // namespace core
}  // namespace acsd

#endif  // ACSD_CORE_WCS_DEGRADE_POLICY_H
