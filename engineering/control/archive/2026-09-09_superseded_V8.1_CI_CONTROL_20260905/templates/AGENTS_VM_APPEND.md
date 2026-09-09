# VM/CI 执行摘要

使用 `git rev-parse --show-toplevel` 定位并原地使用现有 `main` 工作区；禁止迁移、额外克隆、分支和worktree。前台只分发/验收/提交，固定子Agent执行；只读可并行，tracked写入串行。每任务机器验收后原子commit/push，由GitHub Linux/Windows CI判定。合成数据自动运行；Fatduck只运行已校验二进制和本地真实数据，原始数据禁止上传。heavy计算必须动态多线程并记录CPU/线程/内存/IO；ACR关闭。
