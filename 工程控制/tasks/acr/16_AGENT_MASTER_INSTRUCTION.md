# Agent总执行指令

1. 只使用现有 `feature/astrocompute-runtime`，不新建仓库或版本分支。
2. 首先读取 `00_READ_FIRST.md` 和 `21_COMMIT_F_CORRECTION_PLAN.md`。
3. Commit F仅视为中间基础，不得宣称Cost-aware Mixed或95%闭环完成。
4. 保留CPU采样、MemoryBudget接口和尾段缩块实验，但按计划修正命名和接线。
5. 优先修复path guard、单一HEAD、coverage和actual执行报告。
6. 接通 `CostEstimator → Shared Pending Pool → Backend`，不能只生成推荐字符串。
7. 设备每次claim依据固定画像、当前队列、驻留、容量和利用率提交许可。
8. 实现动态guided，禁止固定70/30两段作为最终方案。
9. 资源控制必须具有真实采样、决策和执行动作；所有CPU线程仍可参与。
10. MemoryBudget配置从RuntimeConfig注入，每种动作都必须有真实行为。
11. 真实GPU不可用时相关测试标SKIPPED，但最终不得合并main。
12. ASan/UBSan未实际开启时不得使用sanitizer通过措辞。
13. 禁止修改任何AstroCS现有算法、OpenMP、Pipeline或正常CLI。
14. 所有外部命令、构建和测试必须设置明确超时。
15. 每阶段原子提交；失败不得伪装PASS，继续交付完整证据。
16. 只有CHECKLIST全部通过后，才允许 `--no-ff` 合并main备用。
