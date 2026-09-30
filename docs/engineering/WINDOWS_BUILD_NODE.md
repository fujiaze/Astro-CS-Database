# Windows 验证节点接入

> 上游：`docs/ACSD_DESIGN.md` §11（双平台发行）、§12.4（验证层级与四层验收）。
> 地位：Windows 侧构建与复验节点的接入实操的唯一正本。节点承担 Windows x64 正式工具链的编译、测试与真实数据复验（最高设计 §11、§12.4）；Linux 节点承担开发、静态分析、轻量编译与合成实验，两节点分工见最高设计 §11。

## 1 节点身份与凭据引用

| 项 | 值 |
|---|---|
| 主机 | Fatduck（Windows） |
| 地址 | `100.104.10.71`；tailnet 名 `fatduck`；`hostname` 返回 `Fatduck` |
| 登录用户 | `fujia`；域名身份 `fatduck\fujia` |
| 本节点私钥路径 | `$HOME/.ssh/id_ed25519_fatduck`（本节点用户 `dsh`；即 `/home/dsh/.ssh/id_ed25519_fatduck`），注释 `vm-bj-to-fatduck` |
| 本节点私钥公钥指纹 | `SHA256:f68iOS2+lsuIkHDsDx4n5ASspUv154laQ8EGd3JD4jE` |
| 管理侧私钥 | 另有一把由管理侧持有，公钥指纹 `SHA256:OZ3FIOoZGVpWyhqHU6WUFQP6EgAJGBjtEa4EznSDpig` |
| 网络前提 | tailnet 在线且目标机 22 端口开放 |

凭据纪律：

- 私钥**只以 `-i` 路径引用**，不读取、不打印、不复制私钥内容；文档与日志只登记路径与公钥指纹；
- 不得用密码交互或任何降级手段替代凭据；
- 公钥指纹用于确认所用密钥身份，不作授权凭据。

## 2 接入步骤与复验命令

远程命令按 §3 的远端 shell 规则书写。两条复验命令可直接运行，用于确认身份与工程落位：

```bash
# 复验 1：身份（hostname 与 whoami）
ssh -i /home/dsh/.ssh/id_ed25519_fatduck -o BatchMode=yes fujia@100.104.10.71 'hostname; whoami'

# 复验 2：Windows 侧工程落位的当前提交
ssh -i /home/dsh/.ssh/id_ed25519_fatduck -o BatchMode=yes fujia@100.104.10.71 'cd /f/Astro*/Astro*Normalization*Database && git rev-parse --short HEAD'
```

- 复验 1 的期望输出为 `Fatduck` 与 `fujia`；
- 复验 2 的期望结果是可读出的提交短 SHA；读不出说明落位路径已变，按 §4 更新后再验；
- `BatchMode=yes` 保证免交互：凭据不成立时立即失败，不挂起等输入。

## 3 远端 shell 与路径规则

- 远端**默认 shell 是 MINGW64 Git Bash**（`uname -s` 返回 `MINGW64_NT-…`），不是 cmd，也不是 pwsh；
- 远端命令一律按 Git Bash 语法书写：cmd 的 `dir /b`、`if exist` 之类写法失败，不假设 `%COMSPEC%` 被展开；
- F 盘在 Git Bash 下是 `/f/`；
- 含空格的路径用引号或通配消解，例如 `cd /f/Astro*/Astro*Normalization*Database`；
- 远端输出可能为 GBK，中文报错会乱码：需要读中文诊断时先 `chcp 65001`，否则改看英文或 ASCII 输出。

## 4 工程落位与同步纪律

| 项 | 路径 |
|---|---|
| Windows 侧主检出 | `F:/Astro dev/Astro CS Normalization Database`（Git Bash：`/f/Astro dev/Astro CS Normalization Database`） |
| 同盘独立检出 | `F:/Astro dev/acsd-win` |

- 复验命令用通配匹配主检出，不依赖目录名的完整字面；
- 独立检出会停留在较早的提交上：**跑腿前必须先同步到当前主线**，否则验证的是与当前权威链不一致的旧树；
- 同步后先跑 §2 复验 2 读出提交短 SHA 并与本地主线比对，再执行编译与测试。

## 5 写入公钥与远程提权的三个约束

1. **用户名**：`fujia`。用户名拼错会被目标机 sshd 按 publickey 拒绝，现象与密钥失效相同；排障先核对用户名，再核对密钥。
2. **Windows OpenSSH 管理员组陷阱（最隐蔽）**：目标机 `sshd_config` 末尾有 `Match Group administrators` 段，把 `AuthorizedKeysFile` 指向 `__PROGRAMDATA__/ssh/administrators_authorized_keys`。管理员组成员的家目录 `authorized_keys` 被完全忽略，只认该管理员文件。公钥必须追加到 `C:/ProgramData/ssh/administrators_authorized_keys`，文件权限为 `Administrators:(F) + SYSTEM:(F)`；换密钥时把新公钥追加到同一文件。
3. **Windows 侧提权**：sudo 默认禁用，普通 PowerShell 读写该管理员文件返回 Access Denied。改用 `Start-Process -Verb RunAs` 触发 UAC 提权执行，长命令可用 Base64 编码传入。

## 6 可用时间窗与离线策略

- 在线时间窗（北京时间）：每日 **07:00 最早 06:30 至 23:30** 在线确定性高；其余时段机器可能因睡眠或关机而离线；
- 需要在 Windows 节点执行的任务（Windows 编译、MSVC 测试、真实数据复验）安排在该时间窗内；
- **节点离线不中止目标**：目标需要该节点而机器不可达时，计时等待机器恢复并在时间窗内重试，不放弃、不降级、不伪造结果；
- 等待期间继续 Linux 侧不依赖该节点的工作，不因此阻塞 Linux 侧开发；
- 恢复后重新发起原任务；全部重试与等待记录进运行日志（落点按最高设计 §10 的块级 `output_dir` 规则）。

## 7 使用范围与跑腿纪律

使用范围：

- Windows 编译（MSVC 工具链，冻结取值见 `TOOLCHAIN_AGENT_HOST.md`）；
- MSVC 测试；
- 大规模全量运行指定在该节点执行——同规格负载在默认配置的 Linux 控制节点上耗时不可接受；
- 真实数据复验（L4 视觉验收的 Windows 侧，见最高设计 §12.4）。

跑腿纪律：

- **不以「主机可达」推断「腿已跑通」**：未实际执行前该腿一律记未验证；
- 不得把该腿写成已通过或已豁免；
- 主机可达、复验命令有输出，都只是前置条件；该腿是否通过只由实际执行的断言与退出码决定（最高设计 §12.4：机器门全绿 + 负责人目检）。

## 8 关联

- 双平台发行与工具链分工：`docs/ACSD_DESIGN.md` §11；
- Windows 工具链冻结取值与 preset 边界：`docs/engineering/TOOLCHAIN_AGENT_HOST.md`；
- 验证层级与四层验收：`docs/ACSD_DESIGN.md` §12.4、`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md`；
- 构建与安装：`docs/engineering/BUILD_GRAPH.md`、`docs/engineering/RELEASE_STATUS.md`。
