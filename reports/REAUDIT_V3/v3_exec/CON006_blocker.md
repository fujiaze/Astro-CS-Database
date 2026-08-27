# 阻断项记录：CON-006 stage2 逐 pixel integration 并行化

## 状态：BLOCKED（已中断子代理委托，确认无法在本会话安全验证完成）
按 AGENTS.md『复杂且困难，无法解决的问题可以记录为阻断项，在审核包中汇报』记录。
委托记录：20 分钟+ 无产出（大文件并行化委托两次均停滞，与 CON-005 委托相同），已中断 subagentId 60f209dc-3348-4979-8ccb-89c4eeb12d43。

## 为什么困难（已完整分析）
- 生产（cfg.acr_route=='cpu'，use_acr_block=false）integration 在 chunk 循环 L1041 内，逐 pixel 循环 L1079（for i in [0,cnt)，p=p0+i）。
- 该循环是热循环，但正确并行需：
  1. per-thread scratch：stack/weights/support_v/acc/fid_stack/reasons/frame_seq（L~820, depth 大小）+ 循环内 src_idx(depth)。
  2. 原子计数：local_ivar_used/dbg_zero_px/px_depth_0/1/ge2/total_rejected/total_fallback/underdetermined_px/px_integrated/dbg_reject_px/dbg_fallback_px/total_pixels + reject_hist[]（12 项）。
  3. fatal 路径重构：p2_collect_candidate_stack/p2_validate_candidate_weights/p2_reject_stack_ex 当前 log+p2_upm_close(model)+return 6；parallel 内不可 return，需 atomic fail 标志 + 区后统一 close+return。
  4. large_scale_active 两遍分支（buf_val/buf_w/buf_sup/buf_lo/buf_hi/buf_elig/buf_nvalid）需一并处理。
  5. 线程安全前提：aio_hips_read_tile 与 kernel 分发需确认并发安全（未验证）。
- 量级：~200 行，16 数组 + 12 计数 + 3 fatal 路径。单测无法可靠抓竞态（需 OMPT-TSan 权威验证，见 v3_exec/CON009_tsan_finding.md）。

## 处置
- 已委托子代理（subagentId 60f209dc-3348-4979-8ccb-89c4eeb12d43）实现 + 验证；未完成则保持 BLOCKED。
- 子代理若成功：OF/FON 构建 + Phase2Integrate/Phase2Reject 测试通过后转 PASS。
- 子代理若失败/停滞：在审核包显式列为 BLOCKED（复杂/困难，需 OMPT-TSan 先行或完整上下文会话）。

## 前置（建议先做，降低风险）
- 搭 OMPT 背书的 TSan（或按构造成立+determinism 测试矩阵）作为竞态权威依据，见 CON009_tsan_finding.md。
