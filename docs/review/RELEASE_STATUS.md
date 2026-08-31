# RELEASE_STATUS.md — AstroCS 发布状态（L0 负责人层）

> 目标版本: 0.10.0-alpha.2  更新: 2026-08-31 (V6 重构 Linux 侧完成)

## 当前状态
- 阶段: **ALPHA 重构 Linux 侧完成, 待负责人最终审核**。
- 已过门: G0/G1/G2/G3/G4/G5/G6/G7/G8/G9/G11(Linux 侧) — 证据在 `evidence/refactor/gates/G*/CHECKLIST.md`。
- QA-001..005 质量门 5/5 PASS; LNX-001..005 平台任务 5/5 PASS。
- 88 任务: 81 PASS + 6 WAITING_WINDOWS + 1 REVIEW_PENDING。

## 未完成/等待项 (owner review 范围; 未过项一律标 NOT VERIFIED/FAIL, 无模糊完成措辞)
| 项 | 状态 | 说明 |
|---|---|---|
| G10 Windows (WIN-001..006) | WAITING_WINDOWS | Fatduck 离线 (DNS 失败); 恢复后执行 MSVC/benchmark/真实帧 |
| REL-004 视觉验收 | REVIEW_PENDING | 6 视图已备 (dist/visual_views/); 需负责人核对 |
| 最终发布决定 | AWAITING_EXTERNAL_RELEASE_REVIEW | Agent 无权宣布发布 |

## 已验证 (PASS)
- 科学: SCI 合同 → 算法 → 接口 → 代码 → 测试 六层追溯 66 claims。
- 质量: 生产零警告抑制; 无 GLOB/硬编码; ASan 7 组合成无内存错误; 重复实现=0。
- 平台: GCC Release + Clang Debug 构建; 2 核 resource; 合成 7 组; 发布布局单入口 + SBOM。

## 环境限制 (登记)
- TSan/ASan 数值验证: GCC14 曾拒编 cfitsio → 已解除 (显式清单+omp 宏保护), ASan 可跑; LSan 受 ptrace 限制。
- Windows: Fatduck 离线登记, 不阻塞 Linux。
