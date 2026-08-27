# 09 连续执行覆盖指令 (Continuous Execution Mandate)

来源: 审核人直接指令 (2026-08-27)。本指令覆盖 / 放宽 00_READ_FIRST.md 规则 #12、#32 中
『每个 Checkpoint 等待外部审核、Agent 不得自行跨关』的等待要求, 但**不改变**以下约束:
  仅 main、禁止建分支、单任务原子 commit、测试失败不 commit/push、禁止 force-push/reset、
  状态词限定五值、BLOCKED 需外部阻塞证据、禁跑超过门禁的科学运行、命令带 timeout/日志。

## 指令内容 (审核人逐字)

1. CP0 已 PASS。
2. 从 **CON-001** 开始连续推进至 **G7**(执行覆盖):
   - 连续推进 CON-001..003(G1) → CON-004..010(G2) → SCI/ALG/ORA(G3) → ARCH/API/DOC/CHK(G4)
     → BLD/TST(G5) → WIN/ACR(G6) → RUN/SEAM/HIPS(G7)。
3. 各 Checkpoint 机器验证通过后**自动继续**, 不再等待外部审核。
   - 机器验证 = Control/审核包校验 (validate_control / validate_audit_package / package_audit) + 该 Gate 的数值/构建/测试门禁。
4. 完成 Linux 全部验证后**自动转 Fatduck** 执行 Windows 验证、ACR(CPU/GPU/Mixed) 与 32R(A/B/C/D) 验证。
5. **仅在 G8 提交一次预发布审核包**; G1..G7 的 Checkpoint 审核包只作中间机器验证产物, 不最终对外交付。

## 记录
- 该指令由审核人录入, 属对执行顺序/豁免的外部授权; Agent 无权限自行修改任务/阈值/状态词。
- 合法执行容器: 指令 @ 记录于本控制包; 上一条对版本号/Checkpoint 的等待要求, 在此豁免范围内仅针对中间 Checkpoint 放开自动继续。

## 边界 (不因本指令放开)
- 仍不得在未过机器门禁时提交/推送; 测试失败仍记 FAIL 并停止;
- 仍不得为历史锚建分支或在锚上提交; 仅允许 git archive 导出;
- 32R 前必须通过 G2 并行门禁 (max_threads>=2 / 平均CPU>=150% / 1T-2T >=1.5 / 串行段<1s且<1%);
- 数据/大产物不入包; 仅 LARGE_ARTIFACT_MANIFEST 记录引用;
- 大包先白名单打包 (package_audit.py), 禁止先大包后裁剪。
