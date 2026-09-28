# 全仓整理 sweep-01 · 问题台账快照

- 快照时间：2026-09-28；来源：run/全仓-01/ledger-raw.json（gitignore 面的运行台账，此为随仓存档副本）
- 规模：P-001..P-210 共 228 条（P-001..P-018 为早期 schema 用 p 键）
- 状态字段：confirmed = 已过对抗复验；pending_verify = 检测兵产出待复验；豁免/误报/归并见 verdict 字段
- 消费方式：fixer 按批次取 confirmed 条目修复；复验兵逐条裁决后回写；豁免面以独立审计登记为准
