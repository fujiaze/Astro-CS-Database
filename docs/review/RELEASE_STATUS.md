# RELEASE_STATUS.md — AstroCS 发布状态（L0 负责人层）

> 目标版本: 0.10.0-alpha.1  更新: V6 重构进行中

## 当前状态
- 阶段: **ALPHA 重构执行中** (G0..G11 任务 DAG)。
- 已过门: G0/G1/G2/G3/G4/G5/G6/G7 (证据在 `evidence/refactor/gates/G*/CHECKLIST.md`)。
- 当前: G8 (文档与质量机器门 DOC-002..005)。
- HEAD: 与 origin/main 同步 (三 SHA 一致)。

## 发布判据 (未过硬门不得称完成)
1. G8 文档与质量门全过 (DOC-002..005)。
2. G9 Linux / G10 Windows 平台门全过。
3. G11 发布任务 (REL-001..004) 全过。
4. 最终: 负责人 HiPS 视觉审核 → ALPHA_RELEASE_APPROVED。
5. Agent 无权宣布发布; 全部通过后输出 `AWAITING_EXTERNAL_RELEASE_REVIEW`。

## 已知环境限制 (移交)
- GCC14 拒编 cfitsio C 代码 (TSan/ASan 数值验证受限) → 移交 Fatduck/MSVC (G3 PASS* 登记)。
- Windows/Fatduck 离线不阻塞 Linux 任务 (标 WAITING_WINDOWS 继续)。
