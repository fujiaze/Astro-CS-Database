# 当前任务：P08-001 CLI Core v1 发布包

读取 `tasks/P08-001.md` 并执行。生成自包含运行包、版本清单、hash、默认配置与验证命令。

## 上一任务完成情况

- P07-002 长批次与故障稳定性: DONE (VERDICT: PASS)
  - 证据: evidence/P07-002/
  - 13/13 测试用例 PASS (Stage1 批量 6 帧 + Stage2 重复 3 次 + 取消重跑 2 项 + 故障注入 + 资源泄漏)
  - Stage1 批量 6/6 帧 PASS: C001(3.6GB/19s) C003(35.5GB/86s) C004(793MB/17s) C005(791MB/17s) C006(796MB/17s) C007(32.6GB/68s)
  - Stage2 重复 3/3 确定性 PASS: HCSD SHA-256 = 2A9BD12E... 与 P07-001/P00-003 baseline 字节级一致
  - 取消后重跑 PASS: 进程正常退出 (STATUS_CONTROL_C_EXIT), 重跑成功, 无残留进程/partial 输出
  - 故障注入 PASS: stage2 输入 HISS 删除后 exit_code=1 优雅退出 (非崩溃)
  - 资源泄漏检查 PASS: 残留进程=0, 临时文件=0, 系统可用 46.68GB (71% healthy), stage2 重复峰值差异 65.59MB (stable)
  - 性能异常: 2 项均非回归 (C003 wall +10.8% 长批次负载波动; stage2 wall +14.5% 长批次后冷启动)
  - 残留: 无新增 (南天内存需求 32-35GB/HISS 非字节级可重现 P07-001 已记录)

## P08-001 依赖

- P07-002 (DONE, 长批次与故障稳定性)
- P04-003 (DONE, capabilities 与 inspect 命令)

## 执行步骤

1. 发布包不得依赖用户安装 Python/PowerShell
2. 从干净目录验证 capabilities、smoke、inspect
3. 生成版本与 SHA-256 清单
4. 独立复核以 VERDICT: PASS 结束

完成独立复核后, 更新状态并进入依赖满足的下一任务。
