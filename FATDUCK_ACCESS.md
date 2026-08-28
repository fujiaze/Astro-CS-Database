# Fatduck Windows 验证节点接入说明（vm-bj → Fatduck）

## 跳板与身份
- **主机**：Fatduck（Windows），`fujia@100.104.10.71`，域名身份 `fatduck\fujia`。
- 不支持 Tailscale SSH 服务端，已用标准 `sshd` 回落（`sshd Automatic / 0.0.0.0:22 /
  DefaultShell=pwsh 7`；Tailscale Automatic；开机无需登录即自启）。
- 专用密钥已就绪：`vm-bj:/root/.ssh/id_ed25519`；公钥已写入
  `Fatduck:C:\ProgramData\ssh\administrators_authorized_keys`。换密钥时把公钥追加到同一文件
  （需 Fatduck 管理员权限，权限 `Administrators:(F) + SYSTEM:(F)`）。
- 首次一次性配好的免密链路已就绪，vm-bj root 可直接免密进 Fatduck pwsh 7。

## 常用命令（vm-bj 上执行）

```bash
# 单条命令
ssh -i /root/.ssh/id_ed25519 fujia@100.104.10.71 "pwsh -NoProfile -c 'your command here'"

# 交互式 pwsh
ssh -i /root/.ssh/id_ed25519 -t fujia@100.104.10.71 pwsh

# 传文件
scp -i /root/.ssh/id_ed25519 local_file fujia@100.104.10.71:C:/Users/fujia/

# 验证
ssh -i /root/.ssh/id_ed25519 fujia@100.104.10.71 "pwsh -NoProfile -c 'hostname; whoami; Get-Location'"
# 已验证：Fatduck / fatduck\fujia / 100.104.10.71
```

## 在线时间窗（北京时间）
- 每日 **07:00（最早 06:30）～ 23:30** 在线的确定性高；其余时段可能离线（睡眠/关机）。
- 需在 Fatduck 上执行的任务（Windows 编译 / MSVC 测试 / GUI 验证）必须安排在该时间窗内。

## 离线处理策略
- **Fatduck 离线不中止目标**：若目标需要 Fatduck，但机器不可达，一律计时等待该机器恢复
  （在时间窗内重试），而非放弃、降级或伪造结果。
- 等待期间可继续 Linux 侧不依赖 Fatduck 的工作（AGENTS.md：Windows 不可用时记录等待，
  不阻塞 Linux 开发）。
- 恢复后重新发起原任务；所有重试/等待记录进 logs。

## 使用范围
- Windows 编译（MSVC）、MSVC 测试、GUI 验证。
- 32R 全量运行的预定宿主之一（默认配置在本机 Linux 实测 >49min CPU 不可行，见审计
  §14 性能实测；Fatduck 为该类运行指定节点）。