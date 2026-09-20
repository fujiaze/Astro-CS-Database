# RELEASE-03 最简工作包提示词

```text
工作目录 /workspace/Astro CS Database。执行控制包 RELEASE-03。

第一步（不可跳过）：读 AGENTS.md §1.1 铁律 + CONTROL_PACK_SPEC.md，
再读 工程控制/RELEASE-03/00_README.md 与 TASK_LIST.md，然后按 tasks/*.md 执行。

铁律：任何事之前先回权威链确认规范在哪、怎么规定的。规范已存在就照做；
不存在就先补规范再动手。交付前必须能回答「规范依据是哪份文档哪条」。

执行方式：
- A 类（DOC-201..204 / FIX-201..208）= 已敲定，直接订正详细层文档与代码；
- C 类（EXP-201..206）= 待定科学问题，做实验定案，不预设结论；
- 按 §5.1 拆分派 SubAgent（文件域互斥），SubAgent 零 git 写，前台独立复跑后统一提交。

硬约束：
- TMPDIR=/var/tmp/astrocs；所有命令带 timeout 并存日志到 run/RELEASE-03/logs/；
- 版本号不碰（代码 0.11.0 保持）；发布决定权归负责人；
- 论文/实验材料不入库（run/* 已 gitignore，只留本地痕迹）；
- 不得用 waiver 掩盖红灯、不得删检查、不得 force push/amend、不得在 main 外开分支；
- 实验必须三种数据面（纯合成 / 哈勃真实信号+科学噪声梯度 / testdata 真实），
  判据事前冻结且非退化，≥5 轮独立复核。

遇到必须上呈的六类（发布判定 / 版本号 / 顶层合同结构性变更 / 许可证存疑 /
删 testdata 或交付物 / 穷尽证据仍无法收敛的科学争议）才停下来问我，其余自决。
```

## 一句话版（更短）

```text
执行控制包 RELEASE-03（工作目录 /workspace/Astro CS Database）。
先读 AGENTS.md §1.1 与 CONTROL_PACK_SPEC.md，再读 工程控制/RELEASE-03/00_README.md 和 TASK_LIST.md，按 tasks/*.md 干。
已敲定的（DOC-201..204、FIX-201..208）直接订正；待定的（EXP-201..206）做实验定案，不预设结论。
TMPDIR=/var/tmp/astrocs；版本号不碰；run/* 不入库；SubAgent 零 git 写，前台复跑后提交。
只有六类事（发布判定/版本号/顶层合同结构性变更/许可证/删交付物/科学争议收敛不了）才问我。
```
