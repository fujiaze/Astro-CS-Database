# AIO 轴审计报告 — I/O 所有权、manifest 与原子产品（ROOT-004 扩展轴）

- **轴名**：AIO（io 所有权分散 / manifest 生成点 / 原子发布违例 / config 分离 / 三命令串接）
- **声明基线**：HEAD=main=origin/main=`2c328348304d033aecfa81faf79d1c6cd802b30a`
- **实测基线**（命令 `timeout 30 git rev-parse HEAD main origin/main`）：
  - HEAD=`4fc3e898e3394d6e64e353ef5dad8c99716e8c88`
  - main=`4fc3e898e3394d6e64e353ef5dad8c99716e8c88`
  - origin/main=`4511712b345a2548aad6230ad61cc705b44ef7bd`
- **方法**：仅执行开工第一步基线核对；按任务规则「不一致立即停止并回报」，**未开展任何树内审计**。

## 摘要（≤10 行）

1. 基线三方不一致：期望 `2c32834` 三口径同 SHA；实测 HEAD=main=`4fc3e89`，origin/main=`4511712`（main 领先 origin/main 两个未推送提交：`e5fba37`、`4fc3e89`）。
2. `2c32834` 是 HEAD 的祖先（`git merge-base --is-ancestor` 通过）；HEAD 比基线多 6 个提交，origin/main 比基线多 4 个提交（含 `4511712b` 删除 设计大纲/ 344 文件、`c44adc08` 立 ROOT-006/GAP-028 凭据入仓）。
3. 工作树脏：`git status --porcelain` 计 158 条（含 `docs/contracts/DATA_ARTIFACTS.md`、`docs/contracts/INDEX.yaml`、`工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md` 被修改，及未跟踪 `artifacts/AstroCS_AUDIT_REVIEWPACK_20260909T133841Z.zip`）。
4. 审计对象「当前树」已偏离基线且带未提交改动，AIO 轴任何发现都会挂在不稳定树上，按硬规则停止。
5. 发现计数：**P0=0｜P1=0｜P2=0**（未审，非无问题）。

## 发现表

（空——基线不符，本轴本轮未执行审计；基线校正/追认后需重跑本轴。）

## 停止依据（逐字关键输出另见 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/AIO.log`）

```
$ timeout 30 git rev-parse HEAD main origin/main
4fc3e898e3394d6e64e353ef5dad8c99716e8c88
4fc3e898e3394d6e64e353ef5dad8c99716e8c88
4511712b345a2548aad6230ad61cc705b44ef7bd
```
