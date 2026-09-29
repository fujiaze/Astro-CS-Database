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
---

# 接入信息（2026-09-30 复验更新）——以本节为准，上文旧路径已过期

## 链路（已复验通过）
- **登录用户是 fujia**（域名身份 fatduck\fujia），**不是 fujiaze**；命令：ssh -i <key> fujia@100.104.10.71
- 主机 100.104.10.71（tailnet 名 fatduck）；复验返回 hostname=Fatduck、whoami=fujia。
- **本节点（dsh, uid 1001）可用私钥**：/home/dsh/.ssh/id_ed25519_fatduck
  （注释 vm-bj-to-fatduck，指纹 SHA256:f68iOS2+lsuIkHDsDx4n5ASspUv154laQ8EGd3JD4jE）；
  另有 root 侧一把密钥，指纹 SHA256:OZ3FIOoZGVpWyhqHU6WUFQP6EgAJGBjtEa4EznSDpig。
  **只以 -i 路径引用，不读取、不打印、不复制私钥内容。**
- 网络前提：tailnet 在线且 22 端口开放；离线时按上文离线处理策略等待，不降级、不伪造。

## 三个必踩的坑（都已在别处踩过，照此避开）
1. **用户名**：fujia（曾误用 fujiaze ⇒ publickey 拒绝，看起来像密钥问题，其实是用户名错）。
2. **Windows OpenSSH 管理员组陷阱（最隐蔽）**：目标机 sshd_config 末尾有
   Match Group administrators → AuthorizedKeysFile __PROGRAMDATA__/ssh/administrators_authorized_keys，
   **管理员组成员的家目录 authorized_keys 被完全忽略**，只认那个管理员文件。
   故公钥必须追加到 C:/ProgramData/ssh/administrators_authorized_keys（需管理员权限，权限为 Administrators(F)+SYSTEM(F)）。
3. **Windows 侧 sudo 默认禁用**：普通 PowerShell 读写该管理员文件会 Access Denied；
   改用 Start-Process -Verb RunAs 弹 UAC 提权（可用 Base64 编码传命令）。

## 远端 shell 与路径（重要，决定命令怎么写）
- **远端默认 shell 是 MINGW64 Git Bash**（uname -s = MINGW64_NT-…），**不是 cmd 也不是 pwsh**。
  **优先用 Git Bash 语法写远端命令**；不要按 cmd 写（dir /b、if exist 之类会失败），
  也不要假设 %COMSPEC% 会被展开。
- F 盘在 Git Bash 下是 /f/；**带空格的路径用引号或通配**（例如 cd /f/Astro*/Astro*Normalization*Database）。
- 远端输出可能是 GBK，中文报错会乱码；必要时先 chcp 65001 或改看英文/ASCII 输出。

## 工程落位（与上文记载不同，以本节为准）
- Windows 侧工程实际位于 F:/Astro dev/Astro CS Normalization Database
  （Git Bash 写法 /f/Astro dev/Astro CS Normalization Database）——**不是** F:/Astro CS Dev/…。
- 同盘另有独立检出 F:/Astro dev/acsd-win。
- 复验时该检出在 main 分支、停在旧提交 ⇒ **跑腿前必须先同步到当前主线**，否则验的是旧树。

## 复验命令（可直接跑）
ssh -i /home/dsh/.ssh/id_ed25519_fatduck -o BatchMode=yes fujia@100.104.10.71 'hostname; whoami'
ssh -i /home/dsh/.ssh/id_ed25519_fatduck -o BatchMode=yes fujia@100.104.10.71 'cd /f/Astro*/Astro*Normalization*Database && git rev-parse --short HEAD'

## 纪律
- **不以「主机可达」推断「腿已跑通」**；未实跑前该腿一律记未验证；
- 不得用密码交互或降级手段替代凭据；不得把该腿写成已通过或已豁免。